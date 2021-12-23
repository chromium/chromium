// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/first_run/fre_field_trial.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/variations/variations_associated_data.h"
#include "components/version_info/version_info.h"
#include "ios/chrome/browser/first_run/first_run.h"
#include "ios/chrome/browser/ui/first_run/ios_first_run_field_trials.h"
#include "ios/chrome/browser/ui/ui_feature_flags.h"
#include "ios/chrome/common/channel_info.h"

const char kFREDefaultPromoTestingDefaultDelayParam[] =
    "variant_default_delay_enabled";
const char kFREDefaultPromoTestingOnlyParam[] = "variant_fre_only_enabled";
const char kFREDefaultPromoTestingShortDelayParam[] =
    "variant_short_delay_enabled";
const char kFREUIIdentitySwitcherPositionParam[] =
    "signin_sync_screen_identity_position";
const char kFREUIStringsSetParam[] = "signin_sync_screen_strings_set";
const char kFRESecondUITrialName[] = "EnableFREUIModuleIOSV2";

// Group names for the second trial of the FRE UI.
const char kIdentitySwitcherInTopAndOldStringsSetGroup[] =
    "IdentitySwitcherInTopAndOldStringsSet";
const char kIdentitySwitcherInTopAndNewStringsSetGroup[] =
    "IdentitySwitcherInTopAndNewStringsSet";
const char kIdentitySwitcherInBottomAndOldStringsSetGroup[] =
    "IdentitySwitcherInBottomAndOldStringsSet";
const char kIdentitySwitcherInBottomAndNewStringsSetGroup[] =
    "IdentitySwitcherInBottomAndNewStringsSet";

// Feature param and options for the identity switcher position.
constexpr base::FeatureParam<SigninSyncScreenUIIdentitySwitcherPosition>::Option
    kIdentitySwitcherPositionOptions[] = {
        {SigninSyncScreenUIIdentitySwitcherPosition::kTop, "top"},
        {SigninSyncScreenUIIdentitySwitcherPosition::kBottom, "bottom"}};

constexpr base::FeatureParam<SigninSyncScreenUIIdentitySwitcherPosition>
    kIdentitySwitcherPositionParam{
        &kEnableFREUIModuleIOS, kFREUIIdentitySwitcherPositionParam,
        SigninSyncScreenUIIdentitySwitcherPosition::kTop,
        &kIdentitySwitcherPositionOptions};

// Feature param and options for the sign-in & sync screen strings set.
constexpr base::FeatureParam<SigninSyncScreenUIStringSet>::Option
    kStringSetOptions[] = {{SigninSyncScreenUIStringSet::kOld, "old"},
                           {SigninSyncScreenUIStringSet::kNew, "new"}};

constexpr base::FeatureParam<SigninSyncScreenUIStringSet> kStringSetParam{
    &kEnableFREUIModuleIOS, kFREUIStringsSetParam,
    SigninSyncScreenUIStringSet::kOld, &kStringSetOptions};

namespace {
// String local state preference with the name of the assigned trial group.
// Empty if no group has been assigned yet.
const char kTrialGroupPrefName[] = "fre_refactoring.trial_group";

// Group names for the default browser promo trial.
const char kFREDefaultBrowserAndSmallDelayBeforeOtherPromosGroup[] =
    "FREDefaultBrowserAndSmallDelayBeforeOtherPromos";
const char kFREDefaultBrowserAndDefaultDelayBeforeOtherPromosGroup[] =
    "FREDefaultBrowserAndDefaultDelayBeforeOtherPromos";
const char kDefaultBrowserPromoAtFirstRunOnlyGroup[] =
    "DefaultBrowserPromoAtFirstRunOnly";
// Group names for the FRE redesign permissions trial.
const char kDefaultGroup[] = "Default";
// Experiment IDs defined for the above field trial groups.
const variations::VariationID kDefaultTrialID = 3330131;
const variations::VariationID kDefaultBrowserPromoAtFirstRunOnlyID = 3342136;
const variations::VariationID
    kFREDefaultBrowserAndDefaultDelayBeforeOtherPromosID = 3342137;
const variations::VariationID
    kFREDefaultBrowserAndSmallDelayBeforeOtherPromosID = 3342138;
// Group name for the FRE disabled group.
const char kDisabledGroup[] = "Disabled";
// Experiment IDs defined for the second trial of the FRE UI.
const variations::VariationID kDisabledTrialID = 3344682;
const variations::VariationID kIdentitySwitcherInTopAndOldStringsSetID =
    3344678;
const variations::VariationID kIdentitySwitcherInTopAndNewStringsSetID =
    3344679;
const variations::VariationID kIdentitySwitcherInBottomAndOldStringsSetID =
    3344680;
const variations::VariationID kIdentitySwitcherInBottomAndNewStringsSetID =
    3344681;

// Default local state pref value.
const int kDefaultPrefValue = -1;

// Sets the parameters value of the position and the strings set for a specific
// group for the FRE second experiment.
void AssociateFieldTrialParamsForFRESecondTrialGroup(
    const std::string& group_name,
    const std::string& position,
    const std::string& stringsSet) {
  base::FieldTrialParams params;
  params[kFREUIIdentitySwitcherPositionParam] = position;
  params[kFREUIStringsSetParam] = stringsSet;
  DCHECK(base::AssociateFieldTrialParams(kEnableFREUIModuleIOS.name, group_name,
                                         params));
}
}  // namespace

namespace fre_field_trial {

bool IsInFirstRunDefaultBrowserAndSmallDelayBeforeOtherPromosGroup() {
  if (base::FeatureList::IsEnabled(kEnableFREDefaultBrowserScreenTesting)) {
    return base::GetFieldTrialParamByFeatureAsBool(
        kEnableFREDefaultBrowserScreenTesting,
        kFREDefaultPromoTestingShortDelayParam, false);
  }
  return base::FeatureList::IsEnabled(kEnableFREUIModuleIOS) &&
         base::FieldTrialList::FindFullName(kEnableFREUIModuleIOS.name) ==
             kFREDefaultBrowserAndSmallDelayBeforeOtherPromosGroup;
}

bool IsInFirstRunDefaultBrowserAndDefaultDelayBeforeOtherPromosGroup() {
  if (base::FeatureList::IsEnabled(kEnableFREDefaultBrowserScreenTesting)) {
    return base::GetFieldTrialParamByFeatureAsBool(
        kEnableFREDefaultBrowserScreenTesting,
        kFREDefaultPromoTestingDefaultDelayParam, false);
  }
  return base::FeatureList::IsEnabled(kEnableFREUIModuleIOS) &&
         base::FieldTrialList::FindFullName(kEnableFREUIModuleIOS.name) ==
             kFREDefaultBrowserAndDefaultDelayBeforeOtherPromosGroup;
}

bool IsInDefaultBrowserPromoAtFirstRunOnlyGroup() {
  if (base::FeatureList::IsEnabled(kEnableFREDefaultBrowserScreenTesting)) {
    return base::GetFieldTrialParamByFeatureAsBool(
        kEnableFREDefaultBrowserScreenTesting, kFREDefaultPromoTestingOnlyParam,
        false);
  }
  return base::FeatureList::IsEnabled(kEnableFREUIModuleIOS) &&
         base::FieldTrialList::FindFullName(kEnableFREUIModuleIOS.name) ==
             kDefaultBrowserPromoAtFirstRunOnlyGroup;
}

bool IsFREDefaultBrowserScreenEnabled() {
  return IsInFirstRunDefaultBrowserAndSmallDelayBeforeOtherPromosGroup() ||
         IsInFirstRunDefaultBrowserAndDefaultDelayBeforeOtherPromosGroup() ||
         IsInDefaultBrowserPromoAtFirstRunOnlyGroup();
}

SigninSyncScreenUIIdentitySwitcherPosition
GetSigninSyncScreenUIIdentitySwitcherPosition() {
  // Default: TOP position.
  return kIdentitySwitcherPositionParam.Get();
}

SigninSyncScreenUIStringSet GetSigninSyncScreenUIStringSet() {
  // Default: OLD strings set.
  return kStringSetParam.Get();
}

// Creates a trial for the first run (when there is no variations seed) if
// necessary and enables the feature based on the randomly selected trial group.
// Returns the group number.
int CreateFirstRunTrial(
    base::FieldTrial::EntropyProvider const& low_entropy_provider,
    base::FeatureList* feature_list) {
  // New FRE enabled/disabled.
  int new_fre_default_percent = 0;

  // FRE's default browser screen experiment
  int new_fre_with_default_screen_and_short_cooldown_percent = 0;
  int new_fre_with_default_screen_and_default_cooldown_percent = 0;
  int new_fre_with_default_screen_only_percent = 0;

  switch (GetChannel()) {
    case version_info::Channel::UNKNOWN:
    case version_info::Channel::CANARY:
    case version_info::Channel::DEV:
    case version_info::Channel::BETA:
      new_fre_with_default_screen_and_short_cooldown_percent = 0;
      new_fre_with_default_screen_and_default_cooldown_percent = 0;
      new_fre_with_default_screen_only_percent = 0;
      new_fre_default_percent = 100;
      break;
    case version_info::Channel::STABLE:
      new_fre_with_default_screen_and_short_cooldown_percent = 0;
      new_fre_with_default_screen_and_default_cooldown_percent = 0;
      new_fre_with_default_screen_only_percent = 0;
      new_fre_default_percent = 100;
      break;
  }

  // Set up the trial and groups.
  FirstRunFieldTrialConfig config(kEnableFREUIModuleIOS.name);
  // Default browser promo experiment groups
  config.AddGroup(kFREDefaultBrowserAndSmallDelayBeforeOtherPromosGroup,
                  kFREDefaultBrowserAndSmallDelayBeforeOtherPromosID,
                  new_fre_with_default_screen_and_short_cooldown_percent);
  config.AddGroup(kFREDefaultBrowserAndDefaultDelayBeforeOtherPromosGroup,
                  kFREDefaultBrowserAndDefaultDelayBeforeOtherPromosID,
                  new_fre_with_default_screen_and_default_cooldown_percent);
  config.AddGroup(kDefaultBrowserPromoAtFirstRunOnlyGroup,
                  kDefaultBrowserPromoAtFirstRunOnlyID,
                  new_fre_with_default_screen_only_percent);

  // New FRE groups
  config.AddGroup(kDefaultGroup, kDefaultTrialID, new_fre_default_percent);

  DCHECK_EQ(100, config.GetTotalProbability());

  scoped_refptr<base::FieldTrial> trial =
      config.CreateOneTimeRandomizedTrial(kDefaultGroup, low_entropy_provider);

  // Finalize the group choice and activates the trial - similar to a variation
  // config that's marked with |starts_active| true. This is required for
  // studies that register variation ids, so they don't reveal extra information
  // beyond the low-entropy source.
  int group = trial->group();
  const std::string& group_name = trial->group_name();
  if (group_name == kFREDefaultBrowserAndSmallDelayBeforeOtherPromosGroup ||
      group_name == kFREDefaultBrowserAndDefaultDelayBeforeOtherPromosGroup ||
      group_name == kDefaultBrowserPromoAtFirstRunOnlyGroup) {
    feature_list->RegisterFieldTrialOverride(
        kEnableFREUIModuleIOS.name, base::FeatureList::OVERRIDE_ENABLE_FEATURE,
        trial.get());
  }
  return group;
}

// Creates the trial config, initialize the trial and returns the ID of the
// trial group. There are 6 groups:
// - Control (Default)
// - Disabled
// - Top position + Old strings set
// - Top position + New strings set
// - Bottom position + Old strings set
// - Bottom position + New strings set
int CreateFirstRunSecondTrial(
    const base::FieldTrial::EntropyProvider& low_entropy_provider,
    base::FeatureList* feature_list) {
  // Experiment groups
  int new_fre_default_percent = 0;
  int new_fre_disabled_percent = 0;
  int new_fre_with_top_position_old_strings_set_percent = 0;
  int new_fre_with_top_position_new_strings_set_percent = 0;
  int new_fre_with_bottom_position_old_strings_set_percent = 0;
  int new_fre_with_bottom_position_new_strings_set_percent = 0;

  switch (GetChannel()) {
    case version_info::Channel::UNKNOWN:
    case version_info::Channel::CANARY:
    case version_info::Channel::DEV:
    case version_info::Channel::BETA:
      new_fre_with_top_position_old_strings_set_percent = 20;
      new_fre_with_top_position_new_strings_set_percent = 20;
      new_fre_with_bottom_position_old_strings_set_percent = 20;
      new_fre_with_bottom_position_new_strings_set_percent = 20;
      new_fre_disabled_percent = 20;
      new_fre_default_percent = 0;
      break;
    case version_info::Channel::STABLE:
      new_fre_with_top_position_old_strings_set_percent = 0;
      new_fre_with_top_position_new_strings_set_percent = 0;
      new_fre_with_bottom_position_old_strings_set_percent = 0;
      new_fre_with_bottom_position_new_strings_set_percent = 0;
      new_fre_disabled_percent = 0;
      new_fre_default_percent = 100;
      break;
  }

  // Set up the trial and groups.
  FirstRunFieldTrialConfig config(kFRESecondUITrialName);

  config.AddGroup(kIdentitySwitcherInTopAndOldStringsSetGroup,
                  kIdentitySwitcherInTopAndOldStringsSetID,
                  new_fre_with_top_position_old_strings_set_percent);

  config.AddGroup(kIdentitySwitcherInTopAndNewStringsSetGroup,
                  kIdentitySwitcherInTopAndNewStringsSetID,
                  new_fre_with_top_position_new_strings_set_percent);

  config.AddGroup(kIdentitySwitcherInBottomAndOldStringsSetGroup,
                  kIdentitySwitcherInBottomAndOldStringsSetID,
                  new_fre_with_bottom_position_old_strings_set_percent);

  config.AddGroup(kIdentitySwitcherInBottomAndNewStringsSetGroup,
                  kIdentitySwitcherInBottomAndNewStringsSetID,
                  new_fre_with_bottom_position_new_strings_set_percent);

  config.AddGroup(kDisabledGroup, kDisabledTrialID, new_fre_disabled_percent);
  config.AddGroup(kDefaultGroup, kDefaultTrialID, new_fre_default_percent);

  DCHECK_EQ(100, config.GetTotalProbability());

  // Associate field trial params to each group.
  AssociateFieldTrialParamsForFRESecondTrialGroup(
      kIdentitySwitcherInTopAndOldStringsSetGroup, "top", "old");
  AssociateFieldTrialParamsForFRESecondTrialGroup(
      kIdentitySwitcherInTopAndNewStringsSetGroup, "top", "new");
  AssociateFieldTrialParamsForFRESecondTrialGroup(
      kIdentitySwitcherInBottomAndOldStringsSetGroup, "bottom", "old");
  AssociateFieldTrialParamsForFRESecondTrialGroup(
      kIdentitySwitcherInBottomAndNewStringsSetGroup, "bottom", "new");

  scoped_refptr<base::FieldTrial> trial =
      config.CreateOneTimeRandomizedTrial(kDefaultGroup, low_entropy_provider);

  // Finalize the group choice and activates the trial - similar to a variation
  // config that's marked with |starts_active| true. This is required for
  // studies that register variation ids, so they don't reveal extra information
  // beyond the low-entropy source.
  const std::string& group_name = trial->group_name();
  if (group_name == kIdentitySwitcherInTopAndOldStringsSetGroup ||
      group_name == kIdentitySwitcherInTopAndNewStringsSetGroup ||
      group_name == kIdentitySwitcherInBottomAndOldStringsSetGroup ||
      group_name == kIdentitySwitcherInBottomAndNewStringsSetGroup) {
    feature_list->RegisterFieldTrialOverride(
        kEnableFREUIModuleIOS.name, base::FeatureList::OVERRIDE_ENABLE_FEATURE,
        trial.get());
  } else if (group_name == kDisabledGroup) {
    feature_list->RegisterFieldTrialOverride(
        kEnableFREUIModuleIOS.name, base::FeatureList::OVERRIDE_DISABLE_FEATURE,
        trial.get());
  }

  return trial->group();
}

void RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(kTrialGroupPrefName, kDefaultPrefValue);
}

void Create(const base::FieldTrial::EntropyProvider& low_entropy_provider,
            base::FeatureList* feature_list,
            PrefService* local_state) {
  int trial_group = local_state->GetInteger(kTrialGroupPrefName);
  if (trial_group == kDefaultPrefValue && !FirstRun::IsChromeFirstRun()) {
    // Do not bucket existing users that have not been already been grouped. The
    // experiment wants to only add users who have not seen the First Run
    // Experience yet.
    return;
  }
  // Don't create the trial if it was already created for testing. This is only
  // expected when the browser is used for development purpose. The trial
  // created when the about flag is set will have the same name as the feature.
  // This condition is to avoid having multiple trials overriding the same
  // feature. A trial might have also been created with the commandline
  // arugments.
  if (!base::FieldTrialList::TrialExists(kFRESecondUITrialName) &&
      !base::FieldTrialList::TrialExists(kEnableFREUIModuleIOS.name)) {
    // Create trial and group user for the first time, or tag users again to
    // ensure the experiment can be used to filter UMA metrics.
    trial_group = CreateFirstRunSecondTrial(low_entropy_provider, feature_list);
  }
  // Persist the assigned group for subsequent runs.
  local_state->SetInteger(kTrialGroupPrefName, trial_group);
}

}  // namespace fre_field_trial
