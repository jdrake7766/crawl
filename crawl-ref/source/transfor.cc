/*
 *  File:       transfor.cc
 *  Summary:    Misc function related to player transformations.
 *  Written by: Linley Henzell
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 *
 *  Change History (most recent first):
 *
 * <2>  5/26/99  JDJ  transform() and untransform() set you.wield_change so
 *                    the weapon line updates.
 * <1> -/--/--   LRH  Created
 */

#include "AppHdr.h"
#include "transfor.h"

#include <stdio.h>
#include <string.h>

#include "externs.h"

#include "delay.h"
#include "it_use2.h"
#include "itemprop.h"
#include "items.h"
#include "misc.h"
#include "output.h"
#include "player.h"
#include "skills2.h"
#include "stuff.h"
#include "traps.h"

void drop_everything(void);
void extra_hp(int amount_extra);

bool remove_equipment(std::set<equipment_type> removed)
{
    if ( removed.find(EQ_WEAPON) != removed.end() &&
         you.equip[EQ_WEAPON] != -1)
    {
        unwield_item();
        canned_msg(MSG_EMPTY_HANDED);
    }

    // Remove items in order (std::set is a sorted container)
    std::set<equipment_type>::const_iterator iter;
    for ( iter = removed.begin(); iter != removed.end(); ++iter )
    {
        const equipment_type e = *iter;
        if ( e == EQ_WEAPON || you.equip[e] == -1 )
            continue;

        mprf("%s falls away.",
             you.inv[you.equip[e]].name(DESC_CAP_YOUR).c_str());

        unwear_armour( you.equip[e] );
        you.equip[e] = -1;
    }

    return true;
}                               // end remove_equipment()

bool remove_one_equip(equipment_type eq)
{
    std::set<equipment_type> r;
    r.insert(eq);
    return remove_equipment(r);
}

// Returns true if any piece of equipment that has to be removed is cursed.
// Useful for keeping low level transformations from being too useful.
static bool check_for_cursed_equipment(const std::set<equipment_type> &remove)
{
    std::set<equipment_type>::const_iterator iter;
    for (iter = remove.begin(); iter != remove.end(); ++iter )
    {
        equipment_type e = *iter;
        if ( you.equip[e] == -1 )
            continue;

        if (item_cursed( you.inv[ you.equip[e] ] ))
        {
            mpr( "Your cursed equipment won't allow you to complete the "
                 "transformation." );

            return (true);
        }
    }

    return (false);
}                               // end check_for_cursed_equipment()

// FIXME: Switch to 4.1 transforms handling.
size_type transform_size(int psize)
{
    return you.transform_size(psize);
}

size_type player::transform_size(int psize) const
{
    const int transform = attribute[ATTR_TRANSFORMATION];
    switch (transform)
    {
    case TRAN_SPIDER:
    case TRAN_BAT:
        return SIZE_TINY;
    case TRAN_ICE_BEAST:
        return SIZE_LARGE;
    case TRAN_DRAGON:
    case TRAN_SERPENT_OF_HELL:
        return SIZE_HUGE;
    case TRAN_AIR:
        return SIZE_MEDIUM;
    default:
        return SIZE_CHARACTER;
    }
}

bool transform(int pow, transformation_type which_trans)
{
    if (you.species == SP_MERFOLK && player_is_swimming()
        && which_trans != TRAN_DRAGON)
    {
        // This might by overkill, but it's okay because obviously
        // whatever magical ability that let's them walk on land is
        // removed when they're in water (in this case, their natural
        // form is completely over-riding any other... goes well with
        // the forced transform when entering water)... but merfolk can
        // transform into dragons, because dragons fly. -- bwr
        mpr("You cannot transform out of your normal form while in water.");
        return (false);
    }

    // This must occur before the untransform() and the is_undead check.
    if (you.attribute[ATTR_TRANSFORMATION] == which_trans)
    {
        if (you.duration[DUR_TRANSFORMATION] < 100)
        {
            mpr( "You extend your transformation's duration." );
            you.duration[DUR_TRANSFORMATION] += random2(pow);

            if (you.duration[DUR_TRANSFORMATION] > 100)
                you.duration[DUR_TRANSFORMATION] = 100;

            return (true);
        }
        else
        {
            mpr( "You cannot extend your transformation any further!" );
            return (false);
        }
    }

    if (you.attribute[ATTR_TRANSFORMATION] != TRAN_NONE)
        untransform();

    if (you.is_undead && (you.species != SP_VAMPIRE || which_trans != TRAN_BAT))
    {
        mpr("Your unliving flesh cannot be transformed in this way.");
        return (false);
    }

    //jmf: silently discard this enchantment
    you.duration[DUR_STONESKIN] = 0;

    // We drop everything except jewellery by default
    equipment_type default_rem[] = {
        EQ_WEAPON, EQ_CLOAK, EQ_HELMET, EQ_GLOVES, EQ_BOOTS,
        EQ_SHIELD, EQ_BODY_ARMOUR
    };

    std::set<equipment_type> rem_stuff(default_rem,
                                       default_rem + ARRAYSIZE(default_rem));

    you.redraw_evasion = true;
    you.redraw_armour_class = true;
    you.wield_change = true;

    /* Remember, it can still fail in the switch below... */
    switch (which_trans)
    {
    case TRAN_SPIDER:           // also AC +3, ev +3, fast_run
        // spiders CAN wear soft helmets
        if ( you.equip[EQ_HELMET] == -1
             || !is_hard_helmet(you.inv[you.equip[EQ_HELMET]]))
        {
             rem_stuff.erase(EQ_HELMET);
        }
        
        if (check_for_cursed_equipment( rem_stuff ))
            return (false);

        mpr("You turn into a venomous arachnid creature.");
        remove_equipment(rem_stuff);

        you.attribute[ATTR_TRANSFORMATION] = TRAN_SPIDER;
        you.duration[DUR_TRANSFORMATION] = 10 + random2(pow) + random2(pow);

        if (you.duration[DUR_TRANSFORMATION] > 60)
            you.duration[DUR_TRANSFORMATION] = 60;

        modify_stat( STAT_DEXTERITY, 5, true,
                     "gaining the spider transformation");

        you.symbol = 's';
        you.colour = BROWN;
        return (true);

    case TRAN_BAT:
        // bats CAN wear soft helmets
        if ( you.equip[EQ_HELMET] == -1
             || !is_hard_helmet(you.inv[you.equip[EQ_HELMET]]))
        {
             rem_stuff.erase(EQ_HELMET);
        }
        
        // high ev, low ac, high speed
        if (check_for_cursed_equipment( rem_stuff ))
            return false;
    
        mprf("You turn into a %sbat.",
             you.species == SP_VAMPIRE ? "vampire " : "");

        remove_equipment( rem_stuff );

        you.attribute[ATTR_TRANSFORMATION] = TRAN_BAT;
        you.duration[DUR_TRANSFORMATION] = 20 + random2(pow) + random2(pow);

        if (you.duration[DUR_TRANSFORMATION] > 100)
            you.duration[DUR_TRANSFORMATION] = 100;

       modify_stat( STAT_DEXTERITY, 5, true,
                    "gaining the bat transformation");
       modify_stat( STAT_STRENGTH, -5, true,
                    "gaining the bat transformation" );
   
       you.symbol = 'b';
       you.colour = (you.species == SP_VAMPIRE ? DARKGREY : LIGHTGREY);
       return (true);
        
    case TRAN_ICE_BEAST:  // also AC +3, cold +3, fire -1, pois +1 
        rem_stuff.erase(EQ_CLOAK);
        // ice beasts CAN wear soft helmets
        if ( you.equip[EQ_HELMET] == -1
             || !is_hard_helmet(you.inv[you.equip[EQ_HELMET]]))
        {
             rem_stuff.erase(EQ_HELMET);
        }

        if (check_for_cursed_equipment( rem_stuff ))
            return false;
            
        mpr( "You turn into a creature of crystalline ice." );

        remove_equipment( rem_stuff );

        you.attribute[ATTR_TRANSFORMATION] = TRAN_ICE_BEAST;
        you.duration[DUR_TRANSFORMATION] = 30 + random2(pow) + random2(pow);

        if (you.duration[ DUR_TRANSFORMATION ] > 100)
            you.duration[ DUR_TRANSFORMATION ] = 100;

        extra_hp(12);   // must occur after attribute set

        if (you.duration[DUR_ICY_ARMOUR])
            mpr( "Your new body merges with your icy armour." );

        you.symbol = 'I';
        you.colour = WHITE;
        return (true);

    case TRAN_BLADE_HANDS:
        rem_stuff.erase(EQ_CLOAK);
        rem_stuff.erase(EQ_HELMET);
        rem_stuff.erase(EQ_BOOTS);
        rem_stuff.erase(EQ_BODY_ARMOUR);

        if (check_for_cursed_equipment( rem_stuff ))
            return (false);

        mpr("Your hands turn into razor-sharp scythe blades.");
        remove_equipment( rem_stuff );

        you.attribute[ATTR_TRANSFORMATION] = TRAN_BLADE_HANDS;
        you.duration[DUR_TRANSFORMATION] = 10 + random2(pow);

        if (you.duration[ DUR_TRANSFORMATION ] > 100)
            you.duration[ DUR_TRANSFORMATION ] = 100;
        return (true);

    case TRAN_STATUE: // also AC +20, ev -5, elec +1, pois +1, neg +1, slow
        rem_stuff.erase(EQ_WEAPON); // can still hold a weapon
        rem_stuff.erase(EQ_CLOAK);
        rem_stuff.erase(EQ_HELMET);

        if (check_for_cursed_equipment( rem_stuff ))
            return false;

        if (you.species == SP_GNOME && coinflip())
            mpr( "Look, a garden gnome.  How cute!" );
        else if (player_genus(GENPC_DWARVEN) && one_chance_in(10))
            mpr( "You inwardly fear your resemblance to a lawn ornament." );
        else
            mpr( "You turn into a living statue of rough stone." );

        // too stiff to make use of shields, gloves, or armour -- bwr
        remove_equipment( rem_stuff );

        you.attribute[ATTR_TRANSFORMATION] = TRAN_STATUE;
        you.duration[DUR_TRANSFORMATION] = 20 + random2(pow) + random2(pow);

        if (you.duration[ DUR_TRANSFORMATION ] > 100)
            you.duration[ DUR_TRANSFORMATION ] = 100;

        modify_stat( STAT_DEXTERITY, -2, true,
                     "gaining the statue transformation" );
        modify_stat( STAT_STRENGTH, 2, true,
                     "gaining the statue transformation");
        extra_hp(15);   // must occur after attribute set

        if (you.duration[DUR_STONEMAIL] || you.duration[DUR_STONESKIN])
            mpr( "Your new body merges with your stone armour." );

        you.symbol = '8';
        you.colour = LIGHTGREY;
        return (true);

    case TRAN_DRAGON:  // also AC +10, ev -3, cold -1, fire +2, pois +1, flight
        if (check_for_cursed_equipment( rem_stuff ))
            return false;

        if (you.species == SP_MERFOLK && player_is_swimming())
            mpr("You fly out of the water as you turn into "
                "a fearsome dragon!");
        else
            mpr("You turn into a fearsome dragon!");

        remove_equipment(rem_stuff);

        you.attribute[ATTR_TRANSFORMATION] = TRAN_DRAGON;
        you.duration[DUR_TRANSFORMATION] = 20 + random2(pow) + random2(pow);

        if (you.duration[ DUR_TRANSFORMATION ] > 100)
            you.duration[ DUR_TRANSFORMATION ] = 100;

        modify_stat( STAT_STRENGTH, 10, true,
                     "gaining the dragon transformation" );
        extra_hp(16);   // must occur after attribute set

        you.symbol = 'D';
        you.colour = GREEN;
        
        if (you.attribute[ATTR_HELD])
        {
            mpr("The net rips apart!");
            you.attribute[ATTR_HELD] = 0;
            int net = get_trapping_net(you.x_pos, you.y_pos);
            if (net != NON_ITEM)
                destroy_item(net);
        }
        return (true);

    case TRAN_LICH:
        // also AC +3, cold +1, neg +3, pois +1, is_undead, res magic +50,
        // spec_death +1, and drain attack (if empty-handed)
        if (you.duration[DUR_DEATHS_DOOR])
        {
            mpr( "The transformation conflicts with an enchantment "
                 "already in effect." );

            return (false);
        }

        mpr("Your body is suffused with negative energy!");

        // undead cannot regenerate -- bwr
        if (you.duration[DUR_REGENERATION])
        {
            mpr( "You stop regenerating.", MSGCH_DURATION );
            you.duration[DUR_REGENERATION] = 0;
        }

        // silently removed since undead automatically resist poison -- bwr
        you.duration[DUR_RESIST_POISON] = 0;

        /* no remove_equip */
        you.attribute[ATTR_TRANSFORMATION] = TRAN_LICH;
        you.duration[DUR_TRANSFORMATION] = 20 + random2(pow) + random2(pow);

        if (you.duration[ DUR_TRANSFORMATION ] > 100)
            you.duration[ DUR_TRANSFORMATION ] = 100;

        modify_stat( STAT_STRENGTH, 3, true,
                     "gaining the lich transformation" );
        you.symbol = 'L';
        you.colour = LIGHTGREY;
        you.is_undead = US_UNDEAD;
        you.hunger_state = HS_SATIATED;  // no hunger effects while transformed
        set_redraw_status( REDRAW_HUNGER );
        return (true);

    case TRAN_AIR:
        rem_stuff.insert(EQ_LEFT_RING);
        rem_stuff.insert(EQ_RIGHT_RING);
        rem_stuff.insert(EQ_AMULET);
        
        if (check_for_cursed_equipment( rem_stuff ))
            return false;

        // also AC 20, ev +20, regen/2, no hunger, fire -2, cold -2, air +2,
        // pois +1, spec_earth -1
        mpr( "You feel diffuse..." );

        remove_equipment(rem_stuff);

        drop_everything();

        you.attribute[ATTR_TRANSFORMATION] = TRAN_AIR;
        you.duration[DUR_TRANSFORMATION] = 35 + random2(pow) + random2(pow);

        if (you.duration[ DUR_TRANSFORMATION ] > 150)
            you.duration[ DUR_TRANSFORMATION ] = 150;

        modify_stat( STAT_DEXTERITY, 8, true,
                     "gaining the air transformation" );
        you.symbol = '#';
        you.colour = DARKGREY;
        
        if (you.attribute[ATTR_HELD])
        {
            mpr("You drift through the net!");
            you.attribute[ATTR_HELD] = 0;
            int net = get_trapping_net(you.x_pos, you.y_pos);
            if (net != NON_ITEM)
                remove_item_stationary(mitm[net]);
        }
        return (true);

    case TRAN_SERPENT_OF_HELL:
        if (check_for_cursed_equipment( rem_stuff ))
            return false;

        // also AC +10, ev -5, fire +2, pois +1, life +2, slow
        mpr( "You transform into a huge demonic serpent!" );

        remove_equipment(rem_stuff);

        you.attribute[ATTR_TRANSFORMATION] = TRAN_SERPENT_OF_HELL;
        you.duration[DUR_TRANSFORMATION] = 20 + random2(pow) + random2(pow);

        if (you.duration[ DUR_TRANSFORMATION ] > 120)
            you.duration[ DUR_TRANSFORMATION ] = 120;

        modify_stat( STAT_STRENGTH, 13, true,
                     "gaining the Serpent of Hell transformation");
        extra_hp(17);   // must occur after attribute set

        you.symbol = 'S';
        you.colour = RED;
        return (true);
    case TRAN_NONE:
    case NUM_TRANSFORMATIONS:
        break;
    }

    return (false);
}                               // end transform()

bool transform_can_butcher_barehanded(transformation_type tt)
{
    return (tt == TRAN_BLADE_HANDS || tt == TRAN_DRAGON
            || tt == TRAN_SERPENT_OF_HELL);
}

void untransform(void)
{
    you.redraw_evasion = true;
    you.redraw_armour_class = true;
    you.wield_change = true;

    you.symbol = '@';
    you.colour = LIGHTGREY;

    // must be unset first or else infinite loops might result -- bwr
    const transformation_type old_form =
        static_cast<transformation_type>(you.attribute[ ATTR_TRANSFORMATION ]);
    
    you.attribute[ ATTR_TRANSFORMATION ] = TRAN_NONE;
    you.duration[ DUR_TRANSFORMATION ] = 0;

    int hp_downscale = 10;
    
    switch (old_form)
    {
    case TRAN_SPIDER:
        mpr("Your transformation has ended.", MSGCH_DURATION);
        modify_stat( STAT_DEXTERITY, -5, true,
                     "losing the spider transformation" );
        break;

    case TRAN_BAT:
        mpr("Your transformation has ended.", MSGCH_DURATION);
        modify_stat( STAT_DEXTERITY, -5, true,
                     "losing the bat transformation" );
        modify_stat( STAT_STRENGTH, 5, true,
                     "losing the bat transformation" );
        break;

    case TRAN_BLADE_HANDS:
        mpr( "Your hands revert to their normal proportions.", MSGCH_DURATION );
        you.wield_change = true;
        break;

    case TRAN_STATUE:
        mpr( "You revert to your normal fleshy form.", MSGCH_DURATION );
        modify_stat( STAT_DEXTERITY, 2, true,
                     "losing the statue transformation" );
        modify_stat( STAT_STRENGTH, -2, true,
                     "losing the statue transformation");

        // Note: if the core goes down, the combined effect soon disappears, 
        // but the reverse isn't true. -- bwr
        if (you.duration[DUR_STONEMAIL])
            you.duration[DUR_STONEMAIL] = 1;

        if (you.duration[DUR_STONESKIN])
            you.duration[DUR_STONESKIN] = 1;

        hp_downscale = 15;
        
        break;

    case TRAN_ICE_BEAST:
        mpr( "You warm up again.", MSGCH_DURATION );

        // Note: if the core goes down, the combined effect soon disappears, 
        // but the reverse isn't true. -- bwr
        if (you.duration[DUR_ICY_ARMOUR])
            you.duration[DUR_ICY_ARMOUR] = 1;

        hp_downscale = 12;
        
        break;

    case TRAN_DRAGON:
        mpr( "Your transformation has ended.", MSGCH_DURATION );
        modify_stat(STAT_STRENGTH, -10, true,
                    "losing the dragon transformation" );

        // re-check terrain now that be may no longer be flying.
        move_player_to_grid( you.x_pos, you.y_pos, false, true, true );

        hp_downscale = 16;
        
        break;

    case TRAN_LICH:
        mpr( "You feel yourself come back to life.", MSGCH_DURATION );
        modify_stat(STAT_STRENGTH, -3, true,
                    "losing the lich transformation" );
        you.is_undead = US_ALIVE;
        break;

    case TRAN_AIR:
        mpr( "Your body solidifies.", MSGCH_DURATION );
        modify_stat(STAT_DEXTERITY, -8, true,
                    "losing the air transformation");
        break;

    case TRAN_SERPENT_OF_HELL:
        mpr( "Your transformation has ended.", MSGCH_DURATION );
        modify_stat(STAT_STRENGTH, -13, true,
                    "losing the Serpent of Hell transformation");
        hp_downscale = 17;
        break;

    default:
        break;
    }

    if (transform_can_butcher_barehanded(old_form))
        stop_butcher_delay();
    
    // If nagas wear boots while transformed, they fall off again afterwards:
    // I don't believe this is currently possible, and if it is we
    // probably need something better to cover all possibilities.  -bwr

    // Removed barding check, no transformed creatures can wear barding
    // anyway.
    if (you.species == SP_NAGA || you.species == SP_CENTAUR)
        remove_one_equip(EQ_BOOTS);

    if (hp_downscale != 10 && you.hp != you.hp_max)
    {
        you.hp = you.hp * 10 / hp_downscale;
        if (you.hp < 1)
            you.hp = 1;
        else if (you.hp > you.hp_max)
            you.hp = you.hp_max;
    }
    calc_hp();    
}                               // end untransform()

// XXX: This whole system is a mess as it still relies on special
// cases to handle a large number of things (see wear_armour()) -- bwr
bool can_equip( equipment_type use_which, bool ignore_temporary )
{

    // if more cases are added to this if must also change in
    // item_use for naga barding
    if (ignore_temporary || !player_is_shapechanged())
        /* or a transformation which doesn't change overall shape */
    {
        if (use_which == EQ_HELMET)
        {
            switch (you.species)
            {
            case SP_KENKU:
                return (false);
            default:
                break;
            }
        }
    }

    if (use_which == EQ_HELMET && you.mutation[MUT_HORNS])
        return (false);

    if (use_which == EQ_BOOTS && !player_has_feet())
        return (false);

    if (use_which == EQ_GLOVES && you.has_claws(false) >= 3)
        return (false);

    if (!ignore_temporary)
    {
        switch (you.attribute[ATTR_TRANSFORMATION])
        {
        case TRAN_NONE:
        case TRAN_LICH:
            return (true);

        case TRAN_BLADE_HANDS:
            return (use_which != EQ_WEAPON
                    && use_which != EQ_GLOVES
                    && use_which != EQ_SHIELD);

        case TRAN_STATUE:
            return (use_which == EQ_WEAPON 
                    || use_which == EQ_CLOAK 
                    || use_which == EQ_HELMET);

        case TRAN_ICE_BEAST:
            return (use_which == EQ_CLOAK);

        default:
            return (false);
        }
    }

    return (true);
}                               // end can_equip()

// raw comparison of an item, must use check_armour_shape for full version 
bool transform_can_equip_type( int eq_slot )
{
    // FIXME FIXME FIXME
    return (false);

    // const int form = you.attribute[ATTR_TRANSFORMATION];
    // return (!must_remove( Trans[form].rem_stuff, eq_slot ));
}

void extra_hp(int amount_extra) // must also set in calc_hp
{
    calc_hp();

    you.hp *= amount_extra;
    you.hp /= 10;

    deflate_hp(you.hp_max, false);
}                               // end extra_hp()

void drop_everything(void)
{
    int i = 0;

    if (inv_count() < 1)
        return;

    mpr( "You find yourself unable to carry your possessions!" );

    for (i = 0; i < ENDOFPACK; i++)
    {
        if (is_valid_item( you.inv[i] ))
        {
            copy_item_to_grid( you.inv[i], you.x_pos, you.y_pos );
            you.inv[i].quantity = 0;
        }
    }

    return;
}                               // end drop_everything()

// Used to mark transformations which override species/mutation intrinsics.
// If phys_scales is true then we're checking to see if the form keeps 
// the physical (AC/EV) properties from scales... the special intrinsic 
// features (resistances, etc) are lost in those forms however.
bool transform_changed_physiology( bool phys_scales )
{
    return (you.attribute[ATTR_TRANSFORMATION] != TRAN_NONE
            && you.attribute[ATTR_TRANSFORMATION] != TRAN_BLADE_HANDS
            && (!phys_scales 
                || (you.attribute[ATTR_TRANSFORMATION] != TRAN_LICH
                    && you.attribute[ATTR_TRANSFORMATION] != TRAN_STATUE)));
}
