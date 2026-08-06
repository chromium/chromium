// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/coordinator/gemini_container_mediator.h"

#import <Foundation/Foundation.h>

#import <optional>

#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "components/feature_engagement/public/feature_constants.h"
#import "components/feature_engagement/test/mock_tracker.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/intelligence/bwg/coordinator/gemini_container_mediator_event_handler.h"
#import "ios/chrome/browser/intelligence/bwg/metrics/gemini_metrics.h"
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

namespace {

// A test spy for tracking delegate callbacks from GeminiContainerMediator.
class FakeGeminiContainerMediatorEventHandler
    : public GeminiContainerMediatorEventHandler {
 public:
  void OnViewStateChanged(ios::provider::GeminiViewState view_state) override {
    last_view_state_changed_ = view_state;
  }
  void OnProcessingStatusChanged(
      ios::provider::GeminiClientMode processing_status,
      ios::provider::GeminiDormantReason dormant_reason) override {
    last_processing_status_changed_ = processing_status;
    last_dormant_reason_changed_ = dormant_reason;
  }
  void SetLastShownViewState(
      ios::provider::GeminiViewState view_state) override {
    last_shown_view_state_ = view_state;
  }
  void CollapseFloatyIfInvoked() override { collapse_floaty_called_ = true; }
  void OnLiveButtonTapped() override { live_button_tapped_called_ = true; }
  void OnGeminiLiveUserDidBargeIn() override { barge_in_called_ = true; }
  void OnGeminiLiveUserDidPressStopButton() override {
    stop_button_pressed_called_ = true;
  }
  void OnModeChanged(ios::provider::GeminiViewMode mode) override {
    last_mode_changed_ = mode;
  }
  void OnGeminiUIDidAppear() override { ui_did_appear_called_ = true; }

  std::optional<ios::provider::GeminiViewState> last_view_state_changed_;
  std::optional<ios::provider::GeminiClientMode>
      last_processing_status_changed_;
  std::optional<ios::provider::GeminiDormantReason>
      last_dormant_reason_changed_;
  std::optional<ios::provider::GeminiViewState> last_shown_view_state_;
  bool collapse_floaty_called_ = false;
  bool live_button_tapped_called_ = false;
  bool barge_in_called_ = false;
  bool stop_button_pressed_called_ = false;
  std::optional<ios::provider::GeminiViewMode> last_mode_changed_;
  bool ui_did_appear_called_ = false;
};

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
                                                    eventHandler:&delegate_];
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
  FakeGeminiContainerMediatorEventHandler delegate_;
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

// Tests that the mediator correctly notifies the delegate when the view state
// switches to expanded.
TEST_F(GeminiContainerMediatorTest, TestDidSwitchToViewStateExpanded) {
  [mediator_ didSwitchToViewState:ios::provider::GeminiViewState::kExpanded];
  EXPECT_THAT(delegate_.last_view_state_changed_,
              testing::Optional(ios::provider::GeminiViewState::kExpanded));
  EXPECT_THAT(delegate_.last_shown_view_state_,
              testing::Optional(ios::provider::GeminiViewState::kExpanded));
}

// Tests that the mediator correctly updates the last shown view state when
// switching to collapsed.
TEST_F(GeminiContainerMediatorTest, TestDidSwitchToViewStateCollapsed) {
  [mediator_ didSwitchToViewState:ios::provider::GeminiViewState::kCollapsed];
  EXPECT_THAT(delegate_.last_view_state_changed_,
              testing::Optional(ios::provider::GeminiViewState::kCollapsed));
  EXPECT_THAT(delegate_.last_shown_view_state_,
              testing::Optional(ios::provider::GeminiViewState::kCollapsed));
}

// Tests that the mediator correctly notifies the delegate when processing
// status changes.
TEST_F(GeminiContainerMediatorTest, TestDidUpdateProcessingStatus) {
  [mediator_
      didUpdateProcessingStatus:ios::provider::GeminiClientMode::kListening
                      sessionID:@"session_id"
                 conversationID:@"conversation_id"];
  EXPECT_THAT(delegate_.last_processing_status_changed_,
              testing::Optional(ios::provider::GeminiClientMode::kListening));
  EXPECT_THAT(delegate_.last_dormant_reason_changed_,
              testing::Optional(ios::provider::GeminiDormantReason::kUnknown));
}

// Tests that the mediator correctly notifies the delegate when processing
// status changes with a dormant reason.
TEST_F(GeminiContainerMediatorTest,
       TestDidUpdateProcessingStatusWithDormantReason) {
  [mediator_
      didUpdateProcessingStatus:ios::provider::GeminiClientMode::kDormant
                  dormantReason:ios::provider::GeminiDormantReason::kUserStop
                      sessionID:@"session_id"
                 conversationID:@"conversation_id"];
  EXPECT_THAT(delegate_.last_processing_status_changed_,
              testing::Optional(ios::provider::GeminiClientMode::kDormant));
  EXPECT_THAT(delegate_.last_dormant_reason_changed_,
              testing::Optional(ios::provider::GeminiDormantReason::kUserStop));
}

// Tests that the mediator requests collapsing the floaty when requested to
// switch to collapsed state.
TEST_F(GeminiContainerMediatorTest, TestSwitchToViewStateCollapsed) {
  [mediator_ switchToViewState:ios::provider::GeminiViewState::kCollapsed];
  EXPECT_TRUE(delegate_.collapse_floaty_called_);
}

// Tests that the mediator does not request collapsing the floaty when requested
// to switch to expanded state.
TEST_F(GeminiContainerMediatorTest, TestSwitchToViewStateExpanded) {
  [mediator_ switchToViewState:ios::provider::GeminiViewState::kExpanded];
  EXPECT_FALSE(delegate_.collapse_floaty_called_);
}

// Tests that the mediator handles a null delegate gracefully without crashing.
TEST_F(GeminiContainerMediatorTest, TestNullDelegate) {
  GeminiContainerMediator* null_delegate_mediator =
      [[GeminiContainerMediator alloc] initWithBrowser:browser_.get()
                                          eventHandler:nullptr];

  // Verify that calling delegate methods does not crash when delegate is null.
  [null_delegate_mediator
      didSwitchToViewState:ios::provider::GeminiViewState::kExpanded];
  [null_delegate_mediator
      switchToViewState:ios::provider::GeminiViewState::kCollapsed];

  SUCCEED();
}

// Tests that the mediator stops forwarding events after disconnect.
TEST_F(GeminiContainerMediatorTest, TestDisconnectDelegate) {
  [mediator_ disconnect];

  [mediator_ didSwitchToViewState:ios::provider::GeminiViewState::kExpanded];
  EXPECT_FALSE(delegate_.last_view_state_changed_.has_value());
  EXPECT_FALSE(delegate_.last_shown_view_state_.has_value());

  [mediator_ switchToViewState:ios::provider::GeminiViewState::kCollapsed];
  EXPECT_FALSE(delegate_.collapse_floaty_called_);

  [mediator_ geminiLiveUserDidBargeIn];
  EXPECT_FALSE(delegate_.barge_in_called_);
}

// Tests that the mediator correctly notifies the delegate when the user barges
// in.
TEST_F(GeminiContainerMediatorTest, TestGeminiLiveUserDidBargeIn) {
  [mediator_ geminiLiveUserDidBargeIn];
  EXPECT_TRUE(delegate_.barge_in_called_);
}

// Tests that the mediator correctly forwards live button taps.
TEST_F(GeminiContainerMediatorTest, TestLiveButtonTapped) {
  [mediator_ geminiLiveUserDidTapLiveButton];
  EXPECT_TRUE(delegate_.live_button_tapped_called_);
}

// Tests that the mediator correctly forwards geminiUIDidAppear calls.
TEST_F(GeminiContainerMediatorTest, TestGeminiUIDidAppear) {
  [mediator_ geminiUIDidAppear];
  EXPECT_TRUE(delegate_.ui_did_appear_called_);
}

// Tests that the mediator correctly forwards didSwitchToMode calls.
TEST_F(GeminiContainerMediatorTest, TestDidSwitchToMode) {
  [mediator_ didSwitchToMode:ios::provider::GeminiViewMode::kLive];
  EXPECT_THAT(delegate_.last_mode_changed_,
              testing::Optional(ios::provider::GeminiViewMode::kLive));
}

// Tests that the mediator correctly forwards geminiLiveUserDidPressStopButton
// calls.
TEST_F(GeminiContainerMediatorTest, TestGeminiLiveUserDidPressStopButton) {
  [mediator_ geminiLiveUserDidPressStopButton];
  EXPECT_TRUE(delegate_.stop_button_pressed_called_);
}

// Tests that requireFullPageContext is YES when kAppSwitcherAISummarization is
// enabled and entry point is AppSwitcherAISummarization.
TEST_F(GeminiContainerMediatorTest,
       TestRequireFullPageContextEnabledForAppSwitcher) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {kAppSwitcherAISummarization, kPageActionMenu}, {});
  base::HistogramTester histogram_tester;

  AppendActiveWebState();

  GeminiStartupState* app_switcher_startup_state = [[GeminiStartupState alloc]
      initWithEntryPoint:gemini::EntryPoint::AppSwitcherAISummarization];

  GeminiConfiguration* config = [mediator_
      createGeminiConfigurationForActiveWebState:app_switcher_startup_state
                              baseViewController:nil];
  EXPECT_TRUE(config.requireFullPageContext);
  histogram_tester.ExpectUniqueSample(kRequireFullPageContextHistogram, true,
                                      1);
}

// Tests that requireFullPageContext is NO when kAppSwitcherAISummarization is
// enabled but entry point is not AppSwitcherAISummarization.
TEST_F(GeminiContainerMediatorTest,
       TestRequireFullPageContextDisabledForOtherEntryPoints) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {kAppSwitcherAISummarization, kPageActionMenu}, {});
  base::HistogramTester histogram_tester;

  AppendActiveWebState();

  GeminiStartupState* promo_startup_state =
      [[GeminiStartupState alloc] initWithEntryPoint:gemini::EntryPoint::Promo];

  GeminiConfiguration* config =
      [mediator_ createGeminiConfigurationForActiveWebState:promo_startup_state
                                         baseViewController:nil];
  EXPECT_FALSE(config.requireFullPageContext);
  histogram_tester.ExpectUniqueSample(kRequireFullPageContextHistogram, false,
                                      1);
}

}  // namespace
