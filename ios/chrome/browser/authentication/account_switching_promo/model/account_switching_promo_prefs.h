// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTHENTICATION_ACCOUNT_SWITCHING_PROMO_MODEL_ACCOUNT_SWITCHING_PROMO_PREFS_H_
#define IOS_CHROME_BROWSER_AUTHENTICATION_ACCOUNT_SWITCHING_PROMO_MODEL_ACCOUNT_SWITCHING_PROMO_PREFS_H_

#include "base/containers/flat_set.h"
#include "google_apis/gaia/gaia_id.h"

class PrefRegistrySimple;
class PrefService;

namespace signin {

// Dictionary pref storing per-account-pair suppression and 24-hour retention
// state for the iOS smart account switching startup promo. Global cooldowns and
// caps are enforced via Feature Engagement Tracker rules. Note: Although
// registered as a profile preference (to co-locate with profile prefs such as
// `prefs::kAccountPreviewSwitchingPreference`), this feature is only enabled
// for personal (non-managed) accounts, which all share the single personal
// profile on iOS. Consequently, this state is in practice app-wide across all
// eligible accounts on the device. All helper functions in this file must only
// be called with the personal profile's `PrefService` (enforced by a `CHECK`).
inline constexpr char kAccountSwitchingPromoStatePref[] =
    "ios.account_switching_promo.state";

// Registers profile preferences used by the account switching promo.
void RegisterAccountSwitchingPromoProfilePrefs(PrefRegistrySimple* registry);

// Returns whether the account switching promo may be shown for the
// (`primary_gaia_id`, `target_gaia_id`) pair. Applies the per-account-pair
// rules (reverse-switch suppression and a cap of 2 explicit dismissals). Global
// cooldowns and caps are enforced by Feature Engagement Tracker.
// `profile_prefs` must be the personal profile's `PrefService`.
bool CanShowAccountSwitchingPromo(const PrefService& profile_prefs,
                                  const GaiaId& primary_gaia_id,
                                  const GaiaId& target_gaia_id);

// Suppresses the reverse pair
// `(new_primary_gaia_id, previous_primary_gaia_id)` when the user switches from
// `previous_primary_gaia_id` to `new_primary_gaia_id` (until one of the
// accounts is removed from the device). `profile_prefs` must be the personal
// profile's `PrefService`.
void SuppressAccountSwitchingReversePair(PrefService& profile_prefs,
                                         const GaiaId& previous_primary_gaia_id,
                                         const GaiaId& new_primary_gaia_id);

// Deletes any stored per-pair or retention state referencing accounts absent
// from `active_gaia_ids`. `active_gaia_ids` is expected to be the complete set
// of accounts currently on the device (as opposed to the accounts of a single
// profile), since the pref lives on the personal profile but is effectively
// app-wide. `profile_prefs` must be the personal profile's `PrefService`.
void DeleteAccountSwitchingPromoStateForRemovedAccounts(
    PrefService& profile_prefs,
    const base::flat_set<GaiaId>& active_gaia_ids);

// Increments the explicit dismissal ("Not now") count of the
// (`primary_gaia_id`, `target_gaia_id`) pair in `profile_prefs`.
// `profile_prefs` must be the personal profile's `PrefService`.
// `primary_gaia_id` and `target_gaia_id` must be non-empty and distinct.
void RecordAccountSwitchingPromoExplicitlyDismissed(
    PrefService& profile_prefs,
    const GaiaId& primary_gaia_id,
    const GaiaId& target_gaia_id);

// Must be called after an account switch initiated from the account switching
// promo to `switched_target_gaia_id` has succeeded. Records the acceptance
// timestamp and target account to evaluate 24-hour retention later. Reverse
// pair suppression is recorded separately by `AuthenticationFlow`, which
// performs the switch. `profile_prefs` must be the personal profile's
// `PrefService`.
void RecordAccountSwitchingPromoAccepted(PrefService& profile_prefs,
                                         const GaiaId& switched_target_gaia_id);

// Evaluates 24-hour retention for an accepted switch, and records the
// `Signin.AccountSwitchingPromo.SwitchedAwayWithin24Hours` metric if a terminal
// state is reached. Should be called on app startup, scene activation, and on
// primary account changes. If 24 hours have elapsed without a switch being
// recorded during the window, logs `false` (assuming the user remained on the
// target account for the full window prior to any subsequent switch).
// Otherwise, if the user switched away before 24 hours elapsed, logs `true`. If
// the system clock moved backwards before `last_accepted_switch_time`, drops
// the pending state without recording a metric. If the 24-hour window has not
// elapsed yet and the user is still on the target account, or if no switch is
// pending, does nothing. If a metric is recorded, clears the accepted switch
// state. `profile_prefs` must be the personal profile's `PrefService`.
// `current_primary_gaia_id` is the currently authenticated primary `GaiaId` (or
// an empty `GaiaId` if signed out).
void MaybeRecordAccountSwitchingRetention(
    PrefService& profile_prefs,
    const GaiaId& current_primary_gaia_id);

}  // namespace signin

#endif  // IOS_CHROME_BROWSER_AUTHENTICATION_ACCOUNT_SWITCHING_PROMO_MODEL_ACCOUNT_SWITCHING_PROMO_PREFS_H_
