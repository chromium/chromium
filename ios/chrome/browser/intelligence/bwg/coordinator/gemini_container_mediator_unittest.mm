// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/coordinator/gemini_container_mediator.h"

#import <Foundation/Foundation.h>

#import "base/test/scoped_feature_list.h"
#import "components/feature_engagement/public/feature_constants.h"
#import "components/feature_engagement/test/mock_tracker.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_configuration.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_tab_helper.h"
#import "ios/chrome/browser/intelligence/bwg/utils/gemini_constants.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service_factory.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/gemini_commands.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/public/provider/chrome/browser/bwg/bwg_gateway_protocol.h"
#import "ios/public/provider/chrome/browser/bwg/gemini_api.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "url/gurl.h"

class GeminiContainerMediatorTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        OptimizationGuideServiceFactory::GetInstance(),
        OptimizationGuideServiceFactory::GetDefaultFactory());
    builder.AddTestingFactory(
        feature_engagement::TrackerFactory::GetInstance(),
        base::BindOnce(&GeminiContainerMediatorTest::CreateMockTracker));
    profile_ = std::move(builder).Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());

    CommandDispatcher* dispatcher = browser_->GetCommandDispatcher();
    mock_settings_handler_ = OCMProtocolMock(@protocol(SettingsCommands));
    [dispatcher startDispatchingToTarget:mock_settings_handler_
                             forProtocol:@protocol(SettingsCommands)];
    mock_gemini_handler_ = OCMProtocolMock(@protocol(GeminiCommands));
    [dispatcher startDispatchingToTarget:mock_gemini_handler_
                             forProtocol:@protocol(GeminiCommands)];

    startup_state_ = [[GeminiStartupState alloc]
        initWithEntryPoint:gemini::EntryPoint::Promo];

    mediator_ = [[GeminiContainerMediator alloc] initWithBrowser:browser_.get()
                                                          target:nullptr];
  }

  static std::unique_ptr<KeyedService> CreateMockTracker(ProfileIOS* context) {
    return std::make_unique<feature_engagement::test::MockTracker>();
  }

  web::FakeWebState* AppendActiveWebState() {
    auto web_state = std::make_unique<web::FakeWebState>();
    web::FakeWebState* web_state_ptr = web_state.get();
    web_state->SetBrowserState(profile_.get());
    web_state->SetCurrentURL(GURL("chrome://newtab/"));
    web_state->SetNavigationManager(
        std::make_unique<web::FakeNavigationManager>());
    GeminiTabHelper::CreateForWebState(web_state.get());
    browser_->GetWebStateList()->InsertWebState(
        std::move(web_state),
        WebStateList::InsertionParams::Automatic().Activate(true));
    return web_state_ptr;
  }

  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  GeminiStartupState* startup_state_;
  GeminiContainerMediator* mediator_;
  id mock_settings_handler_;
  id mock_gemini_handler_;
};

// Tests that createGeminiConfigurationForActiveWebState returns nil when no
// active web state exists.
TEST_F(GeminiContainerMediatorTest, TestCreateConfigurationNoActiveWebState) {
  EXPECT_EQ(nil,
            [mediator_ createGeminiConfigurationForActiveWebState:startup_state_
                                               baseViewController:nil]);
}

// Tests that createGeminiConfigurationForActiveWebState returns a valid
// configuration when an active web state is present.
TEST_F(GeminiContainerMediatorTest, TestCreateConfigurationActiveWebState) {
  AppendActiveWebState();

  GeminiConfiguration* config =
      [mediator_ createGeminiConfigurationForActiveWebState:startup_state_
                                         baseViewController:nil];
  EXPECT_NE(nil, config);
  EXPECT_EQ(mediator_.gateway, config.gateway);
}

// Tests that kIPHiOSGeminiLiveIPHFeature and kIPHiOSGeminiLiveNewBadgeFeature
// are successfully triggered when creating configuration and dismissed when
// disconnect is called.
TEST_F(GeminiContainerMediatorTest, TestGeminiLiveIPHAndNewBadgeFET) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({kGeminiLive, kPageActionMenu}, {});

  auto* mock_tracker = static_cast<feature_engagement::test::MockTracker*>(
      feature_engagement::TrackerFactory::GetForProfile(profile_.get()));

  EXPECT_CALL(*mock_tracker,
              ShouldTriggerHelpUI(testing::Ref(
                  feature_engagement::kIPHiOSGeminiLiveIPHFeature)))
      .WillOnce(testing::Return(true));
  EXPECT_CALL(*mock_tracker,
              ShouldTriggerHelpUI(testing::Ref(
                  feature_engagement::kIPHiOSGeminiLiveNewBadgeFeature)))
      .WillOnce(testing::Return(true));

  AppendActiveWebState();

  GeminiConfiguration* config =
      [mediator_ createGeminiConfigurationForActiveWebState:startup_state_
                                         baseViewController:nil];
  EXPECT_TRUE(config.shouldShowGeminiLiveIPH);
  EXPECT_TRUE(config.shouldShowGeminiLiveNewBadge);

  EXPECT_CALL(
      *mock_tracker,
      Dismissed(testing::Ref(feature_engagement::kIPHiOSGeminiLiveIPHFeature)))
      .Times(1);
  EXPECT_CALL(*mock_tracker,
              Dismissed(testing::Ref(
                  feature_engagement::kIPHiOSGeminiLiveNewBadgeFeature)))
      .Times(1);

  [mediator_ disconnect];
}

// Tests that IPH features are not shown when kGeminiLive feature is disabled.
TEST_F(GeminiContainerMediatorTest,
       TestGeminiLiveIPHAndNewBadgeFETNotTriggered) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({}, {kGeminiLive});

  auto* mock_tracker = static_cast<feature_engagement::test::MockTracker*>(
      feature_engagement::TrackerFactory::GetForProfile(profile_.get()));

  EXPECT_CALL(*mock_tracker,
              ShouldTriggerHelpUI(testing::Ref(
                  feature_engagement::kIPHiOSGeminiLiveIPHFeature)))
      .Times(0);
  EXPECT_CALL(*mock_tracker,
              ShouldTriggerHelpUI(testing::Ref(
                  feature_engagement::kIPHiOSGeminiLiveNewBadgeFeature)))
      .Times(0);

  AppendActiveWebState();

  GeminiConfiguration* config =
      [mediator_ createGeminiConfigurationForActiveWebState:startup_state_
                                         baseViewController:nil];
  EXPECT_FALSE(config.shouldShowGeminiLiveIPH);
  EXPECT_FALSE(config.shouldShowGeminiLiveNewBadge);

  EXPECT_CALL(
      *mock_tracker,
      Dismissed(testing::Ref(feature_engagement::kIPHiOSGeminiLiveIPHFeature)))
      .Times(0);
  EXPECT_CALL(*mock_tracker,
              Dismissed(testing::Ref(
                  feature_engagement::kIPHiOSGeminiLiveNewBadgeFeature)))
      .Times(0);

  [mediator_ disconnect];
}

// Tests that suggestion chips are hidden when coming from
// AppSwitcherAISummarization.
TEST_F(GeminiContainerMediatorTest,
       TestShouldShowSuggestionChipsForAppSwitcherAISummarization) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {kAppSwitcherAISummarization, kPageActionMenu}, {});

  AppendActiveWebState();

  GeminiStartupState* app_switcher_startup_state = [[GeminiStartupState alloc]
      initWithEntryPoint:gemini::EntryPoint::AppSwitcherAISummarization];

  GeminiConfiguration* config = [mediator_
      createGeminiConfigurationForActiveWebState:app_switcher_startup_state
                              baseViewController:nil];
  EXPECT_FALSE(config.shouldShowSuggestionChips);
}

// Tests that shouldShowSuggestionChipsForEntryPoint returns false for
// AppSwitcherAISummarization.
TEST_F(GeminiContainerMediatorTest,
       TestShouldShowSuggestionChipsForEntryPointAppSwitcher) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {kAppSwitcherAISummarization, kPageActionMenu}, {});

  AppendActiveWebState();

  EXPECT_FALSE([mediator_
      shouldShowSuggestionChipsForEntryPoint:gemini::EntryPoint::
                                                  AppSwitcherAISummarization]);
  EXPECT_TRUE([mediator_
      shouldShowSuggestionChipsForEntryPoint:gemini::EntryPoint::Promo]);
}
