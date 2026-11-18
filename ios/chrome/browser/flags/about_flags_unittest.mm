// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/flags/about_flags.h"

#import <string>

#import "base/base_switches.h"
#import "base/command_line.h"
#import "base/feature_list.h"
#import "base/test/scoped_command_line.h"
#import "base/test/scoped_feature_list.h"
#import "components/prefs/testing_pref_service.h"
#import "components/webui/flags/feature_entry.h"
#import "components/webui/flags/flags_state.h"
#import "components/webui/flags/flags_test_helpers.h"
#import "components/webui/flags/flags_ui_switches.h"
#import "components/webui/flags/pref_service_flags_storage.h"
#import "testing/platform_test.h"

namespace {

// Returns an iOS about:flags entry that has variation parameters, or nullptr if
// there is none.
const flags_ui::FeatureEntry* FindFeatureEntryWithVariations() {
  for (const flags_ui::FeatureEntry& entry : testing::GetFeatureEntries()) {
    if (entry.type == flags_ui::FeatureEntry::FEATURE_WITH_PARAMS_VALUE &&
        (entry.supported_platforms & flags_ui::kOsIos) &&
        !entry.GetVariations().empty()) {
      return &entry;
    }
  }
  return nullptr;
}

}  // namespace

using AboutFlagsTest = PlatformTest;

// Makes sure that every flag has an owner and an expiry entry in
// flag-metadata.json.
TEST_F(AboutFlagsTest, EveryFlagHasMetadata) {
  flags_ui::testing::EnsureEveryFlagHasMetadata(testing::GetFeatureEntries());
}

// Ensures that all flags marked as never expiring in flag-metadata.json is
// listed in flag-never-expire-list.json.
TEST_F(AboutFlagsTest, OnlyPermittedFlagsNeverExpire) {
  flags_ui::testing::EnsureOnlyPermittedFlagsNeverExpire();
}

// Ensures that every flag has an owner.
TEST_F(AboutFlagsTest, EveryFlagHasNonEmptyOwners) {
  flags_ui::testing::EnsureEveryFlagHasNonEmptyOwners();
}

// Ensures that owners conform to rules in flag-metadata.json.
TEST_F(AboutFlagsTest, OwnersLookValid) {
  flags_ui::testing::EnsureOwnersLookValid();
}

// Ensures that every flag in `flag-never-expire-list.json` has a matching entry
// in `flag-metadata.json`.
TEST_F(AboutFlagsTest, NeverExpireFlagsExist) {
  flags_ui::testing::EnsureNeverExpireFlagsExist();
}

// Ensures that flags are listed in alphabetical order in flag-metadata.json and
// flag-never-expire-list.json.
TEST_F(AboutFlagsTest, FlagsListedInAlphabeticalOrder) {
  flags_ui::testing::EnsureFlagsAreListedInAlphabeticalOrder();
}

// Tests that ConvertFlagsToSwitches adds sentinels and enabled flag switches to
// the command line.
TEST_F(AboutFlagsTest, ConvertFlagsToSwitches) {
  TestingPrefServiceSimple prefs;
  flags_ui::PrefServiceFlagsStorage::RegisterPrefs(prefs.registry());
  flags_ui::PrefServiceFlagsStorage flags_storage(&prefs);

  // Initially, sentinels are added, but no feature switches.
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  ConvertFlagsToSwitches(&flags_storage, &command_line);
  EXPECT_TRUE(command_line.HasSwitch(switches::kFlagSwitchesBegin));
  EXPECT_TRUE(command_line.HasSwitch(switches::kFlagSwitchesEnd));
  EXPECT_FALSE(command_line.HasSwitch(switches::kEnableFeatures));

  // Enable a feature flag.
  const std::string kFlagName = "enable-autofill-credit-card-upload@1";
  SetFeatureEntryEnabled(&flags_storage, kFlagName, /*enable=*/true);

  base::CommandLine enabled_command_line(base::CommandLine::NO_PROGRAM);
  ConvertFlagsToSwitches(&flags_storage, &enabled_command_line);
  EXPECT_TRUE(enabled_command_line.HasSwitch(switches::kFlagSwitchesBegin));
  EXPECT_TRUE(enabled_command_line.HasSwitch(switches::kFlagSwitchesEnd));
  EXPECT_TRUE(enabled_command_line.HasSwitch(switches::kEnableFeatures));
  EXPECT_EQ(enabled_command_line.GetSwitchValueASCII(switches::kEnableFeatures),
            "AutofillUpstream");

  // Reset flags and verify feature switch is no longer present.
  ResetAllFlags(&flags_storage);
  base::CommandLine reset_command_line(base::CommandLine::NO_PROGRAM);
  ConvertFlagsToSwitches(&flags_storage, &reset_command_line);
  EXPECT_FALSE(reset_command_line.HasSwitch(switches::kEnableFeatures));
}

// Tests that ConvertFlagsToSwitches() and
// RegisterAllFeatureVariationParameters() skip about:flags when the command
// line has --no-experiments.
TEST_F(AboutFlagsTest, NoExperimentsSkipsFlags) {
  const flags_ui::FeatureEntry* entry = FindFeatureEntryWithVariations();
  ASSERT_TRUE(entry);
  const std::string feature_name = entry->feature.feature->name;

  TestingPrefServiceSimple prefs;
  flags_ui::PrefServiceFlagsStorage::RegisterPrefs(prefs.registry());
  flags_ui::PrefServiceFlagsStorage flags_storage(&prefs);
  // The options are Default, Enabled, one option per variation, then Disabled.
  // Option 2 selects the first variation.
  SetFeatureEntryEnabled(&flags_storage, entry->NameForOption(2),
                         /*enable=*/true);

  // Isolate the field trials that RegisterAllFeatureVariationParameters()
  // creates.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithEmptyFeatureAndFieldTrialLists();
  base::test::ScopedCommandLine scoped_command_line;

  // Without --no-experiments, both functions apply the flag.
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  ConvertFlagsToSwitches(&flags_storage, &command_line);
  EXPECT_TRUE(command_line.HasSwitch(switches::kFlagSwitchesBegin));
  EXPECT_TRUE(command_line.HasSwitch(switches::kEnableFeatures));
  base::FeatureList feature_list;
  RegisterAllFeatureVariationParameters(&flags_storage, &feature_list);
  EXPECT_TRUE(feature_list.IsFeatureOverridden(feature_name));

  // With --no-experiments, neither function applies the flag.
  scoped_command_line.GetProcessCommandLine()->AppendSwitch(
      switches::kNoExperiments);
  base::CommandLine no_experiments_command_line(base::CommandLine::NO_PROGRAM);
  no_experiments_command_line.AppendSwitch(switches::kNoExperiments);
  ConvertFlagsToSwitches(&flags_storage, &no_experiments_command_line);
  EXPECT_FALSE(
      no_experiments_command_line.HasSwitch(switches::kFlagSwitchesBegin));
  EXPECT_FALSE(
      no_experiments_command_line.HasSwitch(switches::kEnableFeatures));
  base::FeatureList no_experiments_feature_list;
  EXPECT_TRUE(RegisterAllFeatureVariationParameters(
                  &flags_storage, &no_experiments_feature_list)
                  .empty());
  EXPECT_FALSE(no_experiments_feature_list.IsFeatureOverridden(feature_name));
}
