// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTHENTICATION_ACCOUNT_SWITCHING_PROMO_MODEL_ACCOUNT_SWITCHING_PROMO_PREFS_H_
#define IOS_CHROME_BROWSER_AUTHENTICATION_ACCOUNT_SWITCHING_PROMO_MODEL_ACCOUNT_SWITCHING_PROMO_PREFS_H_

#include "google_apis/gaia/gaia_id.h"

class PrefRegistrySimple;
class PrefService;

namespace signin {

// Dictionary pref storing per-account-pair suppression state for the iOS smart
// account switching startup promo. Global cooldowns and caps are enforced via
// Feature Engagement Tracker rules. Note: Although registered as a profile
// preference (to co-locate with profile prefs such as
// `prefs::kAccountPreviewSwitchingPreference`), this feature is only enabled
// for personal (non-managed) accounts, which all share the single personal
// profile on iOS. Consequently, this state is in practice app-wide across all
// eligible accounts on the device. All helper functions in this file must only
// be called with the personal profile's `PrefService` (enforced by a `CHECK`).
inline constexpr char kAccountSwitchingPromoStatePref[] =
    "ios.account_switching_promo.state";

// Registers profile preferences used by the account switching promo.
void RegisterAccountSwitchingPromoProfilePrefs(PrefRegistrySimple* registry);

// Returns true if the account switching promo is allowed to be shown for
// (`primary_gaia_id`, `target_gaia_id`) according to per-account-pair rules
// (reverse-switch suppression). Global cooldowns and caps are enforced by
// Feature Engagement Tracker.
bool CanShowAccountSwitchingPromo(const PrefService& profile_prefs,
                                  const GaiaId& primary_gaia_id,
                                  const GaiaId& target_gaia_id);

// Suppresses the reverse pair
// `(new_primary_gaia_id, previous_primary_gaia_id)` when the user switches from
// `previous_primary_gaia_id` to `new_primary_gaia_id`.
void SuppressAccountSwitchingReversePair(PrefService& profile_prefs,
                                         const GaiaId& previous_primary_gaia_id,
                                         const GaiaId& new_primary_gaia_id);

}  // namespace signin

#endif  // IOS_CHROME_BROWSER_AUTHENTICATION_ACCOUNT_SWITCHING_PROMO_MODEL_ACCOUNT_SWITCHING_PROMO_PREFS_H_
