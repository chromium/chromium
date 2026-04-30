// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/features/features.h"

#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "components/application_locale_storage/application_locale_storage.h"
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
