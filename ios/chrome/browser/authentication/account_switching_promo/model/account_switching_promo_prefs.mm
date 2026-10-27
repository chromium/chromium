// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/account_switching_promo/model/account_switching_promo_prefs.h"

#import <string>

#import "base/check_op.h"
#import "base/values.h"
#import "components/prefs/pref_registry_simple.h"
#import "components/prefs/pref_service.h"
#import "components/prefs/scoped_user_pref_update.h"
#import "google_apis/gaia/gaia_id.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_attributes_storage_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_manager_ios.h"

namespace signin {

namespace {

// Schema of the profile dictionary preference `kAccountSwitchingPromoStatePref`
// (stored on the personal profile only, see its declaration). Global cooldowns
// and caps are enforced by Feature Engagement Tracker rules in
// `ios_promo_feature_configuration.cc`, so this preference stores only
// per-account-pair state:
//
// {
//   // Per-account-pair state.
//   "pairs": {
//     "<primary_gaia_id>": {
//       "<target_gaia_id>": {
//         // Set when the user switched from `target` to `primary`.
//         "suppressed_by_reverse_switch": true
//       }
//     }
//   }
// }
constexpr char kPairsDictKey[] = "pairs";
constexpr char kPairSuppressedByReverseSwitchKey[] =
    "suppressed_by_reverse_switch";

// Verifies that `profile_prefs` is the `PrefService` of the personal profile
// whenever a `ProfileManagerIOS` is active.
void CheckIsPersonalProfilePrefs(const PrefService& profile_prefs) {
  if (ProfileManagerIOS* profile_manager =
          GetApplicationContext()->GetProfileManager()) {
    ProfileIOS* personal_profile = profile_manager->GetProfileWithName(
        profile_manager->GetProfileAttributesStorage()
            ->GetPersonalProfileName());
    CHECK(personal_profile);
    CHECK_EQ(&profile_prefs, personal_profile->GetPrefs());
  }
}

// Ensures and returns the mutable per-pair dictionary for
// `(primary_gaia_id, target_gaia_id)`.
base::DictValue& EnsurePairDict(base::DictValue& state_dict,
                                const GaiaId& primary_gaia_id,
                                const GaiaId& target_gaia_id) {
  return *state_dict.EnsureDict(kPairsDictKey)
              ->EnsureDict(primary_gaia_id.ToString())
              ->EnsureDict(target_gaia_id.ToString());
}

// Returns true if per-account-pair rules suppress showing the promo for
// `(primary_gaia_id, target_gaia_id)`.
bool IsSuppressedByPairRules(const base::DictValue* pairs_dict,
                             const GaiaId& primary_gaia_id,
                             const GaiaId& target_gaia_id) {
  if (!pairs_dict) {
    // No per-pair state has been recorded yet.
    return false;
  }

  const base::DictValue* primary_dict =
      pairs_dict->FindDict(primary_gaia_id.ToString());
  if (!primary_dict) {
    // No pair state has been recorded for `primary_gaia_id`.
    return false;
  }

  const base::DictValue* pair_dict =
      primary_dict->FindDict(target_gaia_id.ToString());
  if (!pair_dict) {
    // No state has been recorded for this specific pair.
    return false;
  }

  // Set when the user previously switched away from `target_gaia_id` to
  // `primary_gaia_id`; never suggest switching back.
  return pair_dict->FindBool(kPairSuppressedByReverseSwitchKey).value_or(false);
}

}  // namespace

void RegisterAccountSwitchingPromoProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(kAccountSwitchingPromoStatePref);
}

bool CanShowAccountSwitchingPromo(const PrefService& profile_prefs,
                                  const GaiaId& primary_gaia_id,
                                  const GaiaId& target_gaia_id) {
  CheckIsPersonalProfilePrefs(profile_prefs);
  const base::DictValue& state_dict =
      profile_prefs.GetDict(kAccountSwitchingPromoStatePref);
  return !IsSuppressedByPairRules(state_dict.FindDict(kPairsDictKey),
                                  primary_gaia_id, target_gaia_id);
}

void SuppressAccountSwitchingReversePair(PrefService& profile_prefs,
                                         const GaiaId& previous_primary_gaia_id,
                                         const GaiaId& new_primary_gaia_id) {
  CheckIsPersonalProfilePrefs(profile_prefs);
  CHECK(!previous_primary_gaia_id.empty());
  CHECK(!new_primary_gaia_id.empty());
  CHECK_NE(previous_primary_gaia_id, new_primary_gaia_id);

  ScopedDictPrefUpdate update(&profile_prefs, kAccountSwitchingPromoStatePref);
  // User switched from `previous_primary_gaia_id` (Account B) to
  // `new_primary_gaia_id` (Account A): permanently suppress reverse pair
  // (A -> B).
  EnsurePairDict(*update, new_primary_gaia_id, previous_primary_gaia_id)
      .Set(kPairSuppressedByReverseSwitchKey, true);
}

}  // namespace signin
