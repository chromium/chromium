// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/account_switching_promo/model/account_switching_promo_prefs.h"

#import <string>

#import "base/check.h"
#import "base/check_op.h"
#import "base/containers/flat_set.h"
#import "base/not_fatal_until.h"
#import "base/values.h"
#import "components/prefs/pref_registry_simple.h"
#import "components/prefs/pref_service.h"
#import "components/prefs/scoped_user_pref_update.h"
#import "components/signin/public/base/signin_switches.h"
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
//         // Number of times the user tapped "Not now".
//         "explicit_dismissal_count": 1,
//         // Set when the user switched from `target` to `primary`.
//         "suppressed_by_reverse_switch": true
//       }
//     }
//   }
// }
constexpr char kPairsDictKey[] = "pairs";
constexpr char kPairExplicitDismissalCountKey[] = "explicit_dismissal_count";
constexpr char kPairSuppressedByReverseSwitchKey[] =
    "suppressed_by_reverse_switch";

// Checks that `profile_prefs` is the `PrefService` of the personal profile.
void CheckIsPersonalProfilePrefs(const PrefService& profile_prefs) {
  ProfileManagerIOS* profile_manager =
      GetApplicationContext()->GetProfileManager();
  CHECK(profile_manager);
  ProfileIOS* personal_profile = profile_manager->GetProfileWithName(
      profile_manager->GetProfileAttributesStorage()->GetPersonalProfileName());
  CHECK(personal_profile);
  CHECK_EQ(&profile_prefs, personal_profile->GetPrefs());
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

  if (pair_dict->FindBool(kPairSuppressedByReverseSwitchKey).value_or(false)) {
    // The user previously switched away from `target_gaia_id` to
    // `primary_gaia_id`; do not suggest switching back while both accounts
    // remain on the device.
    return true;
  }

  // Maximum number of explicit dismissals for a specific `(primary, target)`
  // pair before suppressing that pair (until one of the accounts is removed
  // from the device).
  static constexpr int kMaxPairExplicitDismissals = 2;
  return pair_dict->FindInt(kPairExplicitDismissalCountKey).value_or(0) >=
         kMaxPairExplicitDismissals;
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
  CHECK(!previous_primary_gaia_id.empty(), base::NotFatalUntil::M170);
  CHECK(!new_primary_gaia_id.empty(), base::NotFatalUntil::M170);
  CHECK_NE(previous_primary_gaia_id, new_primary_gaia_id,
           base::NotFatalUntil::M170);
  if (previous_primary_gaia_id.empty() || new_primary_gaia_id.empty() ||
      previous_primary_gaia_id == new_primary_gaia_id) {
    return;
  }

  ScopedDictPrefUpdate update(&profile_prefs, kAccountSwitchingPromoStatePref);
  // User switched from `previous_primary_gaia_id` (Account B) to
  // `new_primary_gaia_id` (Account A): suppress reverse pair (A -> B) until one
  // of the accounts is removed from the device.
  EnsurePairDict(*update, new_primary_gaia_id, previous_primary_gaia_id)
      .Set(kPairSuppressedByReverseSwitchKey, true);
}

void DeleteAccountSwitchingPromoStateForRemovedAccounts(
    PrefService& profile_prefs,
    const base::flat_set<GaiaId>& active_gaia_ids) {
  CheckIsPersonalProfilePrefs(profile_prefs);

  ScopedDictPrefUpdate update(&profile_prefs, kAccountSwitchingPromoStatePref);
  base::DictValue* pairs_dict = update->FindDict(kPairsDictKey);
  if (!pairs_dict) {
    // No pairs has been registered yet.
    return;
  }
  for (auto [_, primary_value] : *pairs_dict) {
    if (base::DictValue* primary_dict = primary_value.GetIfDict()) {
      primary_dict->EraseIf([&active_gaia_ids](const auto& entry) {
        return !active_gaia_ids.contains(GaiaId(entry.first));
      });
    }
  }
  pairs_dict->EraseIf([&active_gaia_ids](const auto& entry) {
    const base::DictValue* primary_dict = entry.second.GetIfDict();
    return !active_gaia_ids.contains(GaiaId(entry.first)) || !primary_dict ||
           primary_dict->empty();
  });
}

void RecordAccountSwitchingPromoExplicitlyDismissed(
    PrefService& profile_prefs,
    const GaiaId& primary_gaia_id,
    const GaiaId& target_gaia_id) {
  CheckIsPersonalProfilePrefs(profile_prefs);
  CHECK(!primary_gaia_id.empty(), base::NotFatalUntil::M170);
  CHECK(!target_gaia_id.empty(), base::NotFatalUntil::M170);
  CHECK_NE(primary_gaia_id, target_gaia_id, base::NotFatalUntil::M170);
  if (primary_gaia_id.empty() || target_gaia_id.empty() ||
      primary_gaia_id == target_gaia_id) {
    return;
  }

  ScopedDictPrefUpdate update(&profile_prefs, kAccountSwitchingPromoStatePref);
  base::DictValue& pair_dict =
      EnsurePairDict(*update, primary_gaia_id, target_gaia_id);
  int pair_dismissals =
      pair_dict.FindInt(kPairExplicitDismissalCountKey).value_or(0);
  pair_dict.Set(kPairExplicitDismissalCountKey, pair_dismissals + 1);
}

}  // namespace signin
