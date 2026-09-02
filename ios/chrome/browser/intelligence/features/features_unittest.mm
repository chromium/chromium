// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/features/features.h"

#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "components/application_locale_storage/application_locale_storage.h"
#import "ios/chrome/app/background_mode_buildflags.h"
#import "ios/chrome/browser/intelligence/actor/tools/utils/actor_tool_utils.h"
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
  std::string original_locale = locale_storage->Get();
  locale_storage->Set("zh_TW");

  EXPECT_TRUE(IsPageActionMenuEnabled());

  // Restore locale.
  locale_storage->Set(original_locale);
}

TEST_F(ActorFeaturesTest, IsPageActionMenuEnabled_DisabledLocale) {
  IOSChromeScopedTestingVariationsService scoped_variations_service;
  scoped_variations_service.Get()->OverrideStoredPermanentCountry("us");

  ApplicationLocaleStorage* locale_storage =
      TestingApplicationContext::GetGlobal()->GetApplicationLocaleStorage();
  std::string original_locale = locale_storage->Get();
  locale_storage->Set("xx-XX");

  EXPECT_FALSE(IsPageActionMenuEnabled());

  // Restore locale.
  locale_storage->Set(original_locale);
}

TEST_F(ActorFeaturesTest, IsZeroStateSuggestionsEnabled_Default) {
  EXPECT_FALSE(IsZeroStateSuggestionsEnabled());
}

// US + en-US users should have the feature enabled even without the
// kZeroStateSuggestions feature flag.
TEST_F(ActorFeaturesTest,
       IsZeroStateSuggestionsEnabled_US_enUS_WithoutFeatureFlag) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kPageActionMenu);

  IOSChromeScopedTestingVariationsService scoped_variations_service;
  scoped_variations_service.Get()->OverrideStoredPermanentCountry("US");

  ApplicationLocaleStorage* locale_storage =
      TestingApplicationContext::GetGlobal()->GetApplicationLocaleStorage();
  std::string original_locale = locale_storage->Get();
  locale_storage->Set("en-US");

  EXPECT_TRUE(IsZeroStateSuggestionsEnabled());

  // Restore locale.
  locale_storage->Set(original_locale);
}

// US + en-US users should have the feature enabled with the
// kZeroStateSuggestions feature flag.
TEST_F(ActorFeaturesTest,
       IsZeroStateSuggestionsEnabled_US_enUS_WithFeatureFlag) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({kPageActionMenu, kZeroStateSuggestions},
                                       {});

  IOSChromeScopedTestingVariationsService scoped_variations_service;
  scoped_variations_service.Get()->OverrideStoredPermanentCountry("us");

  ApplicationLocaleStorage* locale_storage =
      TestingApplicationContext::GetGlobal()->GetApplicationLocaleStorage();
  std::string original_locale = locale_storage->Get();
  locale_storage->Set("en-US");

  EXPECT_TRUE(IsZeroStateSuggestionsEnabled());

  // Restore locale.
  locale_storage->Set(original_locale);
}

// Other countries (e.g., CA) should NOT have the feature enabled without the
// kZeroStateSuggestions feature flag.
TEST_F(ActorFeaturesTest,
       IsZeroStateSuggestionsEnabled_CA_enUS_WithoutFeatureFlag) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kPageActionMenu);

  IOSChromeScopedTestingVariationsService scoped_variations_service;
  scoped_variations_service.Get()->OverrideStoredPermanentCountry("ca");

  ApplicationLocaleStorage* locale_storage =
      TestingApplicationContext::GetGlobal()->GetApplicationLocaleStorage();
  std::string original_locale = locale_storage->Get();
  locale_storage->Set("en-US");

  EXPECT_FALSE(IsZeroStateSuggestionsEnabled());

  // Restore locale.
  locale_storage->Set(original_locale);
}

// Other countries (e.g., CA) should have the feature enabled with the
// kZeroStateSuggestions feature flag.
TEST_F(ActorFeaturesTest,
       IsZeroStateSuggestionsEnabled_CA_enUS_WithFeatureFlag) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({kPageActionMenu, kZeroStateSuggestions},
                                       {});

  IOSChromeScopedTestingVariationsService scoped_variations_service;
  scoped_variations_service.Get()->OverrideStoredPermanentCountry("ca");

  ApplicationLocaleStorage* locale_storage =
      TestingApplicationContext::GetGlobal()->GetApplicationLocaleStorage();
  std::string original_locale = locale_storage->Get();
  locale_storage->Set("en-US");

  EXPECT_TRUE(IsZeroStateSuggestionsEnabled());

  // Restore locale.
  locale_storage->Set(original_locale);
}

// Other locales (e.g., fr-FR) in US should NOT have the feature enabled without
// the kZeroStateSuggestions feature flag.
TEST_F(ActorFeaturesTest,
       IsZeroStateSuggestionsEnabled_US_frFR_WithoutFeatureFlag) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kPageActionMenu);

  IOSChromeScopedTestingVariationsService scoped_variations_service;
  scoped_variations_service.Get()->OverrideStoredPermanentCountry("us");

  ApplicationLocaleStorage* locale_storage =
      TestingApplicationContext::GetGlobal()->GetApplicationLocaleStorage();
  std::string original_locale = locale_storage->Get();
  locale_storage->Set("fr-FR");

  EXPECT_FALSE(IsZeroStateSuggestionsEnabled());

  // Restore locale.
  locale_storage->Set(original_locale);
}

// Other locales (e.g., fr-FR) in US should have the feature enabled with the
// kZeroStateSuggestions feature flag.
TEST_F(ActorFeaturesTest,
       IsZeroStateSuggestionsEnabled_US_frFR_WithFeatureFlag) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({kPageActionMenu, kZeroStateSuggestions},
                                       {});

  IOSChromeScopedTestingVariationsService scoped_variations_service;
  scoped_variations_service.Get()->OverrideStoredPermanentCountry("us");

  ApplicationLocaleStorage* locale_storage =
      TestingApplicationContext::GetGlobal()->GetApplicationLocaleStorage();
  std::string original_locale = locale_storage->Get();
  locale_storage->Set("fr-FR");

  EXPECT_TRUE(IsZeroStateSuggestionsEnabled());

  // Restore locale.
  locale_storage->Set(original_locale);
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

TEST_F(ActorFeaturesTest,
       IsGeminiContextualSuggestionsCuesOnDeviceClassifierEnabled) {
  base::FieldTrialParams params;
  params[kGeminiContextualSuggestionsCuesOnDeviceClassifierParam] = "true";

  // Disabled without PageActionMenu dependency.
  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitAndEnableFeatureWithParameters(
        kGeminiContextualSuggestionsCues, params);
    EXPECT_FALSE(IsGeminiContextualSuggestionsCuesOnDeviceClassifierEnabled());
  }

  // Enabled when PageActionMenu and feature with param are enabled.
  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitWithFeaturesAndParameters(
        {{kPageActionMenu, {}}, {kGeminiContextualSuggestionsCues, params}},
        {});
    EXPECT_TRUE(IsGeminiContextualSuggestionsCuesOnDeviceClassifierEnabled());
  }
}

TEST_F(ActorFeaturesTest,
       IsGeminiContextualSuggestionsCuesAllowGpuExecutionEnabled) {
  base::FieldTrialParams params;
  params[kGeminiContextualSuggestionsCuesAllowGpuExecutionParam] = "true";

  // Disabled without PageActionMenu dependency.
  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitAndEnableFeatureWithParameters(
        kGeminiContextualSuggestionsCues, params);
    EXPECT_FALSE(IsGeminiContextualSuggestionsCuesAllowGpuExecutionEnabled());
  }

  // Enabled when PageActionMenu and feature with param are enabled.
  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitWithFeaturesAndParameters(
        {{kPageActionMenu, {}}, {kGeminiContextualSuggestionsCues, params}},
        {});
    EXPECT_TRUE(IsGeminiContextualSuggestionsCuesAllowGpuExecutionEnabled());
  }
}

TEST_F(ActorFeaturesTest, GeminiFREExperimentVariants) {
  // Disabled by default.
  EXPECT_FALSE(IsGeminiFREExperimentEnabled());
  EXPECT_FALSE(IsGeminiVisualRichFREEnabled());
  EXPECT_FALSE(IsGeminiLightweightFREEnabled());

  // Enabled with no parameters defaults to Visual Rich.
  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitAndEnableFeature(kGeminiFREExperiment);
    EXPECT_TRUE(IsGeminiFREExperimentEnabled());
    EXPECT_TRUE(IsGeminiVisualRichFREEnabled());
    EXPECT_FALSE(IsGeminiLightweightFREEnabled());
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
      {kPageActionMenu, kActorTools, kGeminiClientMigration, kGeminiActor}, {});

#if BUILDFLAG(IOS_BACKGROUND_CONTINUED_PROCESSING_ENABLED)
  // Backgrounding is enabled by default when the compile flag is set.
  EXPECT_TRUE(IsGeminiActorBackgroundingEnabled());
#else
  // Backgrounding is always disabled when the compile flag is not set.
  EXPECT_FALSE(IsGeminiActorBackgroundingEnabled());
#endif
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
       {kGeminiClientMigration, {}},
       {kGeminiActor, params}},
      {});
  EXPECT_FALSE(IsGeminiActorBackgroundingEnabled());
}
