// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/features/features.h"

#import "base/i18n/language_tag.h"
#import "base/i18n/tag_converters.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "base/time/time.h"
#import "components/application_locale_storage/application_locale_storage.h"
#import "ios/chrome/app/background_task/features.h"
#import "ios/chrome/browser/intelligence/actor/tools/utils/actor_tool_utils.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_variations_service.h"
#import "ios/chrome/test/testing_application_context.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

class ActorFeaturesTest : public PlatformTest {
 protected:
  base::test::TaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
};

TEST_F(ActorFeaturesTest, IsActorEnabledDefault) {
  EXPECT_FALSE(IsActorEnabled());
}

TEST_F(ActorFeaturesTest, IsToolDisabled_Default) {
  base::test::ScopedFeatureList scoped_feature_list;

  scoped_feature_list.InitAndEnableFeature(kActorTools);
  // Arbitrarily select kClick to test that tools are enabled by default.
  EXPECT_FALSE(IsToolDisabled(optimization_guide::proto::Action::kClick));
}

TEST_F(ActorFeaturesTest, IsToolDisabled_ToolDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;

  base::FieldTrialParams params;
  params["DisabledTools"] = "ClickTool,TypeTool";
  scoped_feature_list.InitAndEnableFeatureWithParameters(kActorTools, params);

  EXPECT_TRUE(IsToolDisabled(optimization_guide::proto::Action::kClick));
  EXPECT_TRUE(IsToolDisabled(optimization_guide::proto::Action::kType));
  // ScrollTool is NOT in the disabled list, so it should be enabled.
  EXPECT_FALSE(IsToolDisabled(optimization_guide::proto::Action::kScroll));
}

TEST_F(ActorFeaturesTest, IsToolDisabled_ToolNotSet) {
  base::test::ScopedFeatureList scoped_feature_list;

  scoped_feature_list.InitAndEnableFeature(kActorTools);
  EXPECT_TRUE(
      IsToolDisabled(optimization_guide::proto::Action::ACTION_NOT_SET));
}

// Tests that the tab observation PageContext timeout honors the feature
// parameter override.
TEST_F(ActorFeaturesTest, TabObservationPageContextTimeout_ParamOverride) {
  base::test::ScopedFeatureList scoped_feature_list;

  base::FieldTrialParams params;
  params["ActorTabObservationPageContextTimeout"] = "7s";
  scoped_feature_list.InitAndEnableFeatureWithParameters(kGeminiActor, params);
  EXPECT_EQ(base::Seconds(7), GetActorTabObservationPageContextTimeout());
}

// Tests that `ActorDisablesInputDuringActuation()` returns false when
// `kActorTools` is disabled.
TEST_F(ActorFeaturesTest, ActorDisablesInputDuringActuation_ActorDisabled) {
  EXPECT_FALSE(ActorDisablesInputDuringActuation());
}

// Tests that `ActorDisablesInputDuringActuation()` returns true by default when
// `kActorTools` is enabled.
TEST_F(ActorFeaturesTest, ActorDisablesInputDuringActuation_Default) {
  base::test::ScopedFeatureList scoped_feature_list;

  scoped_feature_list.InitAndEnableFeature(kActorTools);
  EXPECT_TRUE(ActorDisablesInputDuringActuation());
}

// Tests that `ActorDisablesInputDuringActuation()` returns false when
// `kActorToolsDisableInputDuringActuationParam` is set to "false".
TEST_F(ActorFeaturesTest, ActorDisablesInputDuringActuation_DisabledViaParam) {
  base::test::ScopedFeatureList scoped_feature_list;

  scoped_feature_list.InitAndEnableFeatureWithParameters(
      kActorTools, {{kActorToolsDisableInputDuringActuationParam, "false"}});
  EXPECT_FALSE(ActorDisablesInputDuringActuation());
}

TEST_F(ActorFeaturesTest, IsPageActionMenuEnabled_Default) {
  EXPECT_FALSE(IsPageActionMenuEnabled());
}

TEST_F(ActorFeaturesTest, IsPageActionMenuEnabled_FeatureFlagEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kPageActionMenu);
  EXPECT_TRUE(IsPageActionMenuEnabled());
}

TEST_F(ActorFeaturesTest, IsPageActionMenuEnabled_KillSwitchEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({kGeminiKillSwitch, kPageActionMenu},
                                       {});
  EXPECT_FALSE(IsPageActionMenuEnabled());
}

TEST_F(ActorFeaturesTest, IsPageActionMenuEnabled_EnabledLocale) {
  IOSChromeScopedTestingVariationsService scoped_variations_service;
  scoped_variations_service.Get()->OverrideStoredPermanentCountry("ca");

  ApplicationLocaleStorage* locale_storage =
      TestingApplicationContext::GetGlobal()->GetApplicationLocaleStorage();
  base::i18n::LanguageTag original_locale = locale_storage->GetTag();
  locale_storage->SetTag(base::i18n::GetKnownLanguageTag("zh-TW"));

  EXPECT_TRUE(IsPageActionMenuEnabled());

  // Restore locale.
  locale_storage->SetTag(original_locale);
}

TEST_F(ActorFeaturesTest, IsPageActionMenuEnabled_DisabledLocale) {
  IOSChromeScopedTestingVariationsService scoped_variations_service;
  scoped_variations_service.Get()->OverrideStoredPermanentCountry("us");

  ApplicationLocaleStorage* locale_storage =
      TestingApplicationContext::GetGlobal()->GetApplicationLocaleStorage();
  base::i18n::LanguageTag original_locale = locale_storage->GetTag();
  locale_storage->SetTag(base::i18n::GetLanguageTagFromString("xx-XX").value());

  EXPECT_FALSE(IsPageActionMenuEnabled());

  // Restore locale.
  locale_storage->SetTag(original_locale);
}

// Test that zero-state suggestions are disabled by default when PageActionMenu
// is disabled.
TEST_F(ActorFeaturesTest, IsZeroStateSuggestionsEnabled_Default) {
  EXPECT_FALSE(IsZeroStateSuggestionsEnabled());
}

// Test that zero-state suggestions are enabled by default when PageActionMenu
// is enabled.
TEST_F(ActorFeaturesTest, IsZeroStateSuggestionsEnabled_WithPageActionMenu) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kPageActionMenu);
  EXPECT_TRUE(IsZeroStateSuggestionsEnabled());
}

// Test that zero-state suggestions are disabled when the feature flag is
// explicitly disabled.
TEST_F(ActorFeaturesTest, IsZeroStateSuggestionsEnabled_FeatureFlagDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({kPageActionMenu},
                                       {kZeroStateSuggestions});
  EXPECT_FALSE(IsZeroStateSuggestionsEnabled());
}

TEST_F(ActorFeaturesTest, IsGeminiLuminousEnabled_DefaultWithoutPAM) {
  EXPECT_FALSE(IsGeminiLuminousEnabled());
}

TEST_F(ActorFeaturesTest, IsGeminiLuminousEnabled_WithPageActionMenu) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kPageActionMenu);
  EXPECT_TRUE(IsGeminiLuminousEnabled());
}

TEST_F(ActorFeaturesTest, IsGeminiLuminousEnabled_FeatureFlagDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({kPageActionMenu}, {kGeminiLuminous});
  EXPECT_FALSE(IsGeminiLuminousEnabled());
}

TEST_F(ActorFeaturesTest, GeminiFREExperimentVariants) {
  // Enabled by default with Lightweight Convenience.
  EXPECT_TRUE(IsGeminiFREExperimentEnabled());
  EXPECT_FALSE(IsGeminiVisualRichFREEnabled());
  EXPECT_TRUE(IsGeminiLightweightFREEnabled());
  EXPECT_EQ(GetGeminiLightweightFREVariant(),
            GeminiLightweightFREVariant::kConvenience);

  // Explicitly disabled.
  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitAndDisableFeature(kGeminiFREExperiment);
    EXPECT_FALSE(IsGeminiFREExperimentEnabled());
    EXPECT_FALSE(IsGeminiVisualRichFREEnabled());
    EXPECT_FALSE(IsGeminiLightweightFREEnabled());
  }

  // Enabled with no parameters defaults to Lightweight Convenience.
  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitAndEnableFeature(kGeminiFREExperiment);
    EXPECT_TRUE(IsGeminiFREExperimentEnabled());
    EXPECT_FALSE(IsGeminiVisualRichFREEnabled());
    EXPECT_TRUE(IsGeminiLightweightFREEnabled());
    EXPECT_EQ(GetGeminiLightweightFREVariant(),
              GeminiLightweightFREVariant::kConvenience);
  }

  // Enabled with explicit "visual-rich" parameter.
  {
    base::test::ScopedFeatureList scoped_feature_list;
    base::FieldTrialParams params;
    params[kGeminiFREExperimentParam] = kGeminiFREExperimentParamVisualRich;
    scoped_feature_list.InitAndEnableFeatureWithParameters(kGeminiFREExperiment,
                                                           params);
    EXPECT_TRUE(IsGeminiFREExperimentEnabled());
    EXPECT_TRUE(IsGeminiVisualRichFREEnabled());
    EXPECT_FALSE(IsGeminiLightweightFREEnabled());
  }

  // Enabled with "lightweight-convenience" parameter.
  {
    base::test::ScopedFeatureList scoped_feature_list;
    base::FieldTrialParams params;
    params[kGeminiFREExperimentParam] =
        kGeminiFREExperimentParamLightweightConvenience;
    scoped_feature_list.InitAndEnableFeatureWithParameters(kGeminiFREExperiment,
                                                           params);
    EXPECT_TRUE(IsGeminiFREExperimentEnabled());
    EXPECT_FALSE(IsGeminiVisualRichFREEnabled());
    EXPECT_TRUE(IsGeminiLightweightFREEnabled());
    EXPECT_EQ(GetGeminiLightweightFREVariant(),
              GeminiLightweightFREVariant::kConvenience);
  }

  // Enabled with "lightweight-page-sharing" parameter.
  {
    base::test::ScopedFeatureList scoped_feature_list;
    base::FieldTrialParams params;
    params[kGeminiFREExperimentParam] =
        kGeminiFREExperimentParamLightweightPageSharing;
    scoped_feature_list.InitAndEnableFeatureWithParameters(kGeminiFREExperiment,
                                                           params);
    EXPECT_TRUE(IsGeminiFREExperimentEnabled());
    EXPECT_FALSE(IsGeminiVisualRichFREEnabled());
    EXPECT_TRUE(IsGeminiLightweightFREEnabled());
    EXPECT_EQ(GetGeminiLightweightFREVariant(),
              GeminiLightweightFREVariant::kPageSharing);
  }

  // Enabled with "lightweight-diverse" parameter.
  {
    base::test::ScopedFeatureList scoped_feature_list;
    base::FieldTrialParams params;
    params[kGeminiFREExperimentParam] =
        kGeminiFREExperimentParamLightweightDiverse;
    scoped_feature_list.InitAndEnableFeatureWithParameters(kGeminiFREExperiment,
                                                           params);
    EXPECT_TRUE(IsGeminiFREExperimentEnabled());
    EXPECT_FALSE(IsGeminiVisualRichFREEnabled());
    EXPECT_TRUE(IsGeminiLightweightFREEnabled());
    EXPECT_EQ(GetGeminiLightweightFREVariant(),
              GeminiLightweightFREVariant::kDiverse);
  }
}

TEST_F(ActorFeaturesTest, IsGeminiExperimentalGuidedOnboardingEnabled) {
  // Disabled by default.
  EXPECT_FALSE(IsGeminiExperimentalGuidedOnboardingEnabled());
  EXPECT_FALSE(ShouldForceGeminiExperimentalGuidedOnboarding());

  // Disabled without PageActionMenu dependency.
  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitAndEnableFeature(
        kGeminiExperimentalGuidedOnboarding);
    EXPECT_FALSE(IsGeminiExperimentalGuidedOnboardingEnabled());
    EXPECT_FALSE(ShouldForceGeminiExperimentalGuidedOnboarding());
  }

  // Enabled when PageActionMenu and feature are enabled.
  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitWithFeatures(
        {kPageActionMenu, kGeminiExperimentalGuidedOnboarding}, {});
    EXPECT_TRUE(IsGeminiExperimentalGuidedOnboardingEnabled());
    EXPECT_FALSE(ShouldForceGeminiExperimentalGuidedOnboarding());
  }

  // Forced when force param is enabled with PageActionMenu.
  {
    base::test::ScopedFeatureList scoped_feature_list;
    base::FieldTrialParams params;
    params[kGeminiExperimentalGuidedOnboardingForceParam] = "true";
    scoped_feature_list.InitWithFeaturesAndParameters(
        {{kPageActionMenu, {}}, {kGeminiExperimentalGuidedOnboarding, params}},
        {});
    EXPECT_TRUE(IsGeminiExperimentalGuidedOnboardingEnabled());
    EXPECT_TRUE(ShouldForceGeminiExperimentalGuidedOnboarding());
  }
}

// Tests Gemini Actor backgrounding when Gemini Actor and its prerequisites are
// enabled with default parameter values (which defaults to true).
TEST_F(ActorFeaturesTest, TestGeminiActorBackgroundingEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {kPageActionMenu, kActorTools, kGeminiActor,
       kEnableBackgroundContinuedProcessing},
      {});

  EXPECT_EQ(IsBackgroundContinuedProcessingEnabled(),
            IsGeminiActorBackgroundingEnabled());
}

// Tests that Gemini Actor backgrounding returns false when explicitly disabled
// via parameter.
TEST_F(ActorFeaturesTest, TestGeminiActorBackgroundingDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  base::FieldTrialParams params;
  params[kGeminiActorBackgroundingParam] = "false";
  scoped_feature_list.InitWithFeaturesAndParameters(
      {{kPageActionMenu, {}},
       {kActorTools, {}},
       {kEnableBackgroundContinuedProcessing, {}},
       {kGeminiActor, params}},
      {});
  EXPECT_FALSE(IsGeminiActorBackgroundingEnabled());
}

// Tests that Gemini Actor backgrounding returns false when
// `kEnableBackgroundContinuedProcessing` is disabled.
TEST_F(ActorFeaturesTest, TestGeminiActorBackgroundingFeatureDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {kPageActionMenu, kActorTools, kGeminiActor},
      {kEnableBackgroundContinuedProcessing});
  EXPECT_FALSE(IsGeminiActorBackgroundingEnabled());
}

// Tests that page stability is disabled by default when `kActorTools` is
// disabled.
TEST_F(ActorFeaturesTest, IsPageStabilityEnabled_Default) {
  EXPECT_FALSE(IsPageStabilityEnabled());
}

// Tests that page stability is enabled by default when `kActorTools` is
// enabled.
TEST_F(ActorFeaturesTest, IsPageStabilityEnabled_WithActorTools) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kActorTools);
  EXPECT_TRUE(IsPageStabilityEnabled());
}

// Tests that page stability can be disabled via feature parameter when
// `kActorTools` is enabled.
TEST_F(ActorFeaturesTest, IsPageStabilityEnabled_FeatureParamDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      kActorTools, {{kActorToolsPageStabilityParam, "false"}});
  EXPECT_FALSE(IsPageStabilityEnabled());
}

// Tests that `IsGeminiLiveEnabled()` is false by default when bottom sheet
// migration is enabled and `kEnableGeminiLiveInBottomSheet` is disabled.
TEST_F(ActorFeaturesTest,
       IsGeminiLiveEnabled_DisabledByDefaultInBottomSheetMigration) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {kPageActionMenu, kGeminiLive, kAssistantContainer,
       kIOSGeminiBottomSheetMigration},
      {});
  EXPECT_FALSE(IsGeminiLiveEnabled());
}

// Tests that `IsGeminiLiveEnabled()` relies on `kGeminiLive` when
// `kIOSGeminiBottomSheetMigration` is disabled.
TEST_F(ActorFeaturesTest,
       IsGeminiLiveEnabled_EnabledWithoutBottomSheetMigration) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {kPageActionMenu, kGeminiLive},
      {kIOSGeminiBottomSheetMigration, kEnableGeminiLiveInBottomSheet});
  EXPECT_TRUE(IsGeminiLiveEnabled());
}

// Tests that `IsGeminiLiveEnabled()` is false when
// `kIOSGeminiBottomSheetMigration` and `kGeminiLive` are disabled, even when
// `kEnableGeminiLiveInBottomSheet` is enabled.
TEST_F(ActorFeaturesTest,
       IsGeminiLiveEnabled_DisabledWithoutBottomSheetMigrationWhenLiveOff) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {kPageActionMenu, kEnableGeminiLiveInBottomSheet},
      {kIOSGeminiBottomSheetMigration, kGeminiLive});
  EXPECT_FALSE(IsGeminiLiveEnabled());
}

// Tests that `kEnableGeminiLiveInBottomSheet` allows Gemini Live when
// `IsIOSGeminiBottomSheetMigrationEnabled()` and `kGeminiLive` are enabled.
TEST_F(ActorFeaturesTest,
       IsGeminiLiveEnabled_EnabledInBottomSheetWhenFlagEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {kPageActionMenu, kGeminiLive, kAssistantContainer,
       kIOSGeminiBottomSheetMigration, kEnableGeminiLiveInBottomSheet},
      {});
  EXPECT_TRUE(IsGeminiLiveEnabled());
}

// Tests that `kEnableGeminiLiveInBottomSheet` alone does not enable Gemini Live
// in bottom sheet migration when `kGeminiLive` is disabled.
TEST_F(ActorFeaturesTest,
       IsGeminiLiveEnabled_DisabledInBottomSheetWhenGeminiLiveOff) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {kPageActionMenu, kAssistantContainer, kIOSGeminiBottomSheetMigration,
       kEnableGeminiLiveInBottomSheet},
      {kGeminiLive});
  EXPECT_FALSE(IsGeminiLiveEnabled());
}

// Tests that `IsActorBackgroundedSceneRenderingModeEnabled()` is false by
// default, even when `kActorTools` is enabled.
TEST_F(ActorFeaturesTest, BackgroundedSceneRenderingModeDisabledByDefault) {
  EXPECT_FALSE(IsActorBackgroundedSceneRenderingModeEnabled());

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kActorTools);
  EXPECT_FALSE(IsActorBackgroundedSceneRenderingModeEnabled());
}

// Tests that `IsActorBackgroundedSceneRenderingModeEnabled()` returns false
// when `kActorBackgroundedSceneRenderingMode` is enabled without `kActorTools`.
TEST_F(ActorFeaturesTest, BackgroundedSceneRenderingModeRequiresActorTools) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({kActorBackgroundedSceneRenderingMode},
                                       {kActorTools});
  EXPECT_FALSE(IsActorBackgroundedSceneRenderingModeEnabled());
}

// Tests that `IsActorBackgroundedSceneRenderingModeEnabled()` returns true when
// `kActorTools` and `kActorBackgroundedSceneRenderingMode` are enabled, even
// when `kPageActionMenu`, `kGeminiActor`, and
// `kEnableBackgroundContinuedProcessing` are disabled.
TEST_F(ActorFeaturesTest,
       BackgroundedSceneRenderingModeIgnoresProviderSpecificFlags) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {kActorTools, kActorBackgroundedSceneRenderingMode},
      {kPageActionMenu, kGeminiActor, kEnableBackgroundContinuedProcessing});
  EXPECT_TRUE(IsActorBackgroundedSceneRenderingModeEnabled());
}

// Tests that the feature parameters of `kActorBackgroundedSceneRenderingMode`
// use their expected defaults and parse configured overrides when enabled.
TEST_F(ActorFeaturesTest, BackgroundedSceneRenderingModeParamsAreParsed) {
  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitWithFeatures(
        {kActorTools, kActorBackgroundedSceneRenderingMode}, {});

    EXPECT_TRUE(IsActorBackgroundedSceneBackgroundRenderingEnabled());
    EXPECT_TRUE(IsActorBackgroundedSceneCustomSnapshotEnabled());
    EXPECT_EQ(base::Seconds(90),
              GetActorBackgroundedSceneRenderingModeRefreshInterval());
    EXPECT_FALSE(IsActorBackgroundedSceneRenderingModeActiveInForeground());
    EXPECT_EQ("", GetActorBackgroundedSceneRenderingModeBlockedIOSVersions());
  }

  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitWithFeaturesAndParameters(
        {{kActorTools, {}},
         {kActorBackgroundedSceneRenderingMode,
          {{"background_rendering_enabled", "false"},
           {"custom_snapshot_enabled", "false"},
           {"refresh_interval", "30s"},
           {"active_in_foreground", "true"},
           {"blocked_ios_versions", "27.0.1,27.1"}}}},
        {});

    EXPECT_FALSE(IsActorBackgroundedSceneBackgroundRenderingEnabled());
    EXPECT_FALSE(IsActorBackgroundedSceneCustomSnapshotEnabled());
    EXPECT_EQ(base::Seconds(30),
              GetActorBackgroundedSceneRenderingModeRefreshInterval());
    EXPECT_TRUE(IsActorBackgroundedSceneRenderingModeActiveInForeground());
    EXPECT_EQ("27.0.1,27.1",
              GetActorBackgroundedSceneRenderingModeBlockedIOSVersions());
  }
}

// Tests that `kActorBackgroundedSceneBackgroundRenderingEnabledParam` and
// `kActorBackgroundedSceneCustomSnapshotEnabledParam` can be disabled
// independently.
TEST_F(ActorFeaturesTest,
       BackgroundedSceneRenderingModeSwitchesAreIndependent) {
  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitWithFeaturesAndParameters(
        {{kActorTools, {}},
         {kActorBackgroundedSceneRenderingMode,
          {{kActorBackgroundedSceneBackgroundRenderingEnabledParam, "false"}}}},
        {});

    EXPECT_FALSE(IsActorBackgroundedSceneBackgroundRenderingEnabled());
    EXPECT_TRUE(IsActorBackgroundedSceneCustomSnapshotEnabled());
  }

  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitWithFeaturesAndParameters(
        {{kActorTools, {}},
         {kActorBackgroundedSceneRenderingMode,
          {{kActorBackgroundedSceneCustomSnapshotEnabledParam, "false"}}}},
        {});

    EXPECT_TRUE(IsActorBackgroundedSceneBackgroundRenderingEnabled());
    EXPECT_FALSE(IsActorBackgroundedSceneCustomSnapshotEnabled());
  }
}

// Tests that all parameter getters of `kActorBackgroundedSceneRenderingMode`
// return disabled/empty defaults when the feature is off or when `kActorTools`
// is disabled.
TEST_F(ActorFeaturesTest,
       BackgroundedSceneRenderingModeParamGettersDisabledWhenFeatureOff) {
  EXPECT_FALSE(IsActorBackgroundedSceneBackgroundRenderingEnabled());
  EXPECT_FALSE(IsActorBackgroundedSceneCustomSnapshotEnabled());
  EXPECT_EQ(base::TimeDelta(),
            GetActorBackgroundedSceneRenderingModeRefreshInterval());
  EXPECT_FALSE(IsActorBackgroundedSceneRenderingModeActiveInForeground());
  EXPECT_EQ("", GetActorBackgroundedSceneRenderingModeBlockedIOSVersions());

  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitWithFeaturesAndParameters(
        {{kActorBackgroundedSceneRenderingMode,
          {{"background_rendering_enabled", "true"},
           {"custom_snapshot_enabled", "true"},
           {"refresh_interval", "30s"},
           {"active_in_foreground", "true"},
           {"blocked_ios_versions", "27.0.1,27.1"}}}},
        {kActorTools});

    EXPECT_FALSE(IsActorBackgroundedSceneBackgroundRenderingEnabled());
    EXPECT_FALSE(IsActorBackgroundedSceneCustomSnapshotEnabled());
    EXPECT_EQ(base::TimeDelta(),
              GetActorBackgroundedSceneRenderingModeRefreshInterval());
    EXPECT_FALSE(IsActorBackgroundedSceneRenderingModeActiveInForeground());
    EXPECT_EQ("", GetActorBackgroundedSceneRenderingModeBlockedIOSVersions());
  }

  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitWithFeatures(
        {kActorTools}, {kActorBackgroundedSceneRenderingMode});

    EXPECT_FALSE(IsActorBackgroundedSceneBackgroundRenderingEnabled());
    EXPECT_FALSE(IsActorBackgroundedSceneCustomSnapshotEnabled());
    EXPECT_EQ(base::TimeDelta(),
              GetActorBackgroundedSceneRenderingModeRefreshInterval());
    EXPECT_FALSE(IsActorBackgroundedSceneRenderingModeActiveInForeground());
    EXPECT_EQ("", GetActorBackgroundedSceneRenderingModeBlockedIOSVersions());
  }
}
