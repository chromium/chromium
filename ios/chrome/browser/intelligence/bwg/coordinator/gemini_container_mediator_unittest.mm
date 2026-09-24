// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/coordinator/gemini_container_mediator.h"

#import <Foundation/Foundation.h>

#import <optional>

#import "base/functional/callback_helpers.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "components/autofill/core/common/autofill_debug_features.h"
#import "components/autofill/core/common/autofill_features.h"
#import "components/feature_engagement/public/feature_constants.h"
#import "components/feature_engagement/test/mock_tracker.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/assistant/coordinator/assistant_container_commands.h"
#import "ios/chrome/browser/assistant/ui/assistant_container_detent.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/intelligence/bwg/coordinator/gemini_container_mediator_event_handler.h"
#import "ios/chrome/browser/intelligence/bwg/metrics/gemini_metrics.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_configuration.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_page_context.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_shared_tabs_delegate.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_tab_helper.h"
#import "ios/chrome/browser/intelligence/bwg/ui/gemini_container_consumer.h"
#import "ios/chrome/browser/intelligence/bwg/utils/gemini_constants.h"
#import "ios/chrome/browser/intelligence/bwg/utils/gemini_prefs.h"
#import "ios/chrome/browser/intelligence/bwg/utils/gemini_test_utils.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/intelligence/proto_wrappers/page_context_wrapper.h"
#import "ios/chrome/browser/intelligence/zero_state_suggestions/zero_state_suggestions_service.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service_factory.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/gemini_commands.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/signin/model/identity_test_environment_browser_state_adaptor.h"
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
#import "third_party/ocmock/gtest_support.h"
#import "url/gurl.h"

@interface GeminiContainerMediator (Testing)
- (void)cancelPageContextGeneration;
- (void)setActuationActive:(BOOL)actuationActive;
@end

// Fake PageContextWrapper for testing page context generation.
@interface MediatorFakePageContextWrapper : PageContextWrapper
@property(nonatomic, assign) BOOL populateCalled;
@end

@implementation MediatorFakePageContextWrapper {
  base::OnceCallback<void(PageContextWrapperCallbackResponse)>
      _completionCallback;
}

- (instancetype)initWithWebState:(web::WebState*)webState
                          config:(PageContextWrapperConfig)config
              completionCallback:
                  (base::OnceCallback<void(PageContextWrapperCallbackResponse)>)
                      completionCallback {
  self = [super initWithWebState:webState
                          config:config
              completionCallback:base::DoNothing()];
  if (self) {
    _completionCallback = std::move(completionCallback);
  }
  return self;
}

- (instancetype)initWithWebState:(web::WebState*)webState
              completionCallback:
                  (base::OnceCallback<void(PageContextWrapperCallbackResponse)>)
                      completionCallback {
  return [self initWithWebState:webState
                         config:PageContextWrapperConfigBuilder().Build()
             completionCallback:std::move(completionCallback)];
}

- (void)populatePageContextFieldsAsync {
  self.populateCalled = YES;
  if (_completionCallback) {
    std::move(_completionCallback)
        .Run(base::ok(
            std::make_unique<optimization_guide::proto::PageContext>()));
  }
}
@end

// Fake consumer for testing zero state updates.
@interface FakeGeminiContainerConsumer : NSObject <GeminiContainerConsumer>
@property(nonatomic, assign, getter=isZeroState) BOOL zeroState;
@property(nonatomic, assign) NSInteger zeroStateChangeCount;
@property(nonatomic, assign) BOOL dismissKeyboardCalled;
@property(nonatomic, assign) BOOL worklogCompact;
@property(nonatomic, assign, getter=isActuationActive) BOOL actuationActive;
@end

@implementation FakeGeminiContainerConsumer
- (void)updateZeroStateVisibility:(BOOL)visible {
  _zeroState = visible;
  _zeroStateChangeCount++;
}

- (void)dismissKeyboard {
  _dismissKeyboardCalled = YES;
}

- (void)setWorklogCompact:(BOOL)compact {
  _worklogCompact = compact;
}

- (void)setActuationActive:(BOOL)active {
  _actuationActive = active;
}
@end

namespace ios::provider {
std::optional<gemini::EntryPoint> GetLastUpdatePromptActionEntryPoint();
NSString* GetLastUpdatePromptActionPrompt();
BOOL GetLastUpdatePromptActionShouldAutoSubmit();
int GetUpdateActivePageContextCallCount();
}  // namespace ios::provider

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
    builder.AddTestingFactory(
        IdentityManagerFactory::GetInstance(),
        base::BindRepeating(IdentityTestEnvironmentBrowserStateAdaptor::
                                BuildIdentityManagerForTests));
    profile_ = std::move(builder).Build();

    gemini::test::SetUpEligibleAccount(profile_.get());

    browser_ = std::make_unique<TestBrowser>(profile_.get());

    CommandDispatcher* dispatcher = browser_->GetCommandDispatcher();
    mock_settings_handler_ = OCMProtocolMock(@protocol(SettingsCommands));
    [dispatcher startDispatchingToTarget:mock_settings_handler_
                             forProtocol:@protocol(SettingsCommands)];
    mock_gemini_handler_ = OCMProtocolMock(@protocol(GeminiCommands));
    [dispatcher startDispatchingToTarget:mock_gemini_handler_
                             forProtocol:@protocol(GeminiCommands)];
    mock_container_handler_ =
        OCMProtocolMock(@protocol(AssistantContainerCommands));

    startup_state_ = [[GeminiStartupState alloc]
        initWithEntryPoint:gemini::EntryPoint::Promo];

    mediator_ = [[GeminiContainerMediator alloc] initWithBrowser:browser_.get()
                                                    actorService:nullptr
                                                    eventHandler:&delegate_];
    mediator_.containerHandler = mock_container_handler_;
    mediator_.geminiHandler = mock_gemini_handler_;
  }

  void TearDown() override {
    [mediator_ disconnect];
    PlatformTest::TearDown();
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

  web::WebTaskEnvironment task_environment_{
      web::WebTaskEnvironment::TimeSource::MOCK_TIME};
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  GeminiStartupState* startup_state_;
  FakeGeminiContainerMediatorEventHandler delegate_;
  GeminiContainerMediator* mediator_;
  id mock_settings_handler_;
  id mock_gemini_handler_;
  id mock_container_handler_;
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
  EXPECT_FALSE(config.shouldAutoSubmit);
}

// Tests that createGeminiConfigurationForActiveWebState sets shouldAutoSubmit
// on configuration when requested by startup state.
TEST_F(GeminiContainerMediatorTest, TestCreateConfigurationWithAutoSubmit) {
  AppendActiveWebState();

  startup_state_.shouldAutoSubmit = YES;
  GeminiConfiguration* config =
      [mediator_ createGeminiConfigurationForActiveWebState:startup_state_
                                         baseViewController:nil];
  EXPECT_NE(nil, config);
  EXPECT_TRUE(config.shouldAutoSubmit);
}

// Tests that suggestion chips are hidden when creating configuration for
// AtMemorySearch.
TEST_F(GeminiContainerMediatorTest, TestCreateConfigurationForAtMemorySearch) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{autofill::features::kAutofillAtMemory,
                            autofill::features::debug::
                                kAtMemorySkipEnablementChecks},
      /*disabled_features=*/{});

  AppendActiveWebState();

  GeminiStartupState* at_memory_startup_state = [[GeminiStartupState alloc]
      initWithEntryPoint:gemini::EntryPoint::AtMemorySearch];

  GeminiConfiguration* config = [mediator_
      createGeminiConfigurationForActiveWebState:at_memory_startup_state
                              baseViewController:nil];
  EXPECT_FALSE(config.shouldShowSuggestionChips);
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

// Tests that shouldShowSuggestionChipsForEntryPoint returns false for
// AtMemorySearch.
TEST_F(GeminiContainerMediatorTest,
       TestShouldShowSuggestionChipsForEntryPointAtMemorySearch) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{autofill::features::kAutofillAtMemory,
                            autofill::features::debug::
                                kAtMemorySkipEnablementChecks},
      /*disabled_features=*/{});

  AppendActiveWebState();

  EXPECT_FALSE([mediator_ shouldShowSuggestionChipsForEntryPoint:
                              gemini::EntryPoint::AtMemorySearch]);
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
// status changes in live mode.
TEST_F(GeminiContainerMediatorTest, TestDidUpdateProcessingStatusInLiveMode) {
  [mediator_ didSwitchToMode:ios::provider::GeminiViewMode::kLive];
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

// Tests that the mediator handles a null delegate gracefully without crashing.
TEST_F(GeminiContainerMediatorTest, TestNullDelegate) {
  GeminiContainerMediator* null_delegate_mediator =
      [[GeminiContainerMediator alloc] initWithBrowser:browser_.get()
                                          actorService:nullptr
                                          eventHandler:nullptr];

  // Verify that calling delegate methods does not crash when delegate is null.
  [null_delegate_mediator
      didSwitchToViewState:ios::provider::GeminiViewState::kExpanded];

  SUCCEED();
}

// Tests that the mediator stops forwarding events after disconnect.
TEST_F(GeminiContainerMediatorTest, TestDisconnectDelegate) {
  [mediator_ disconnect];

  [mediator_ didSwitchToViewState:ios::provider::GeminiViewState::kExpanded];
  EXPECT_FALSE(delegate_.last_view_state_changed_.has_value());
  EXPECT_FALSE(delegate_.last_shown_view_state_.has_value());

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

// Tests that connect configures initial UI state, notifies
// containerHandler, and requests active page context generation.
TEST_F(GeminiContainerMediatorTest, TestConnectTriggersInitialUIState) {
  @autoreleasepool {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitWithFeatures(
        {kAssistantContainer, kIOSGeminiBottomSheetMigration}, {});

    FakeGeminiContainerConsumer* consumer =
        [[FakeGeminiContainerConsumer alloc] init];
    mediator_.consumer = consumer;

    OCMExpect([mock_container_handler_
        animateAssistantContainerToDetent:AssistantContainerDetent::kMedium]);
    OCMExpect([mock_container_handler_ setAssistantContainerGrabberHidden:NO
                                                                 animated:YES]);

    id mediator_mock = OCMPartialMock(mediator_);
    OCMExpect([mediator_mock requestActivePageContextGeneration]);

    [mediator_mock connect];

    EXPECT_TRUE(consumer.isZeroState);
    EXPECT_EQ(1, consumer.zeroStateChangeCount);
    EXPECT_TRUE(consumer.dismissKeyboardCalled);
    EXPECT_OCMOCK_VERIFY(mock_container_handler_);
    EXPECT_OCMOCK_VERIFY(mediator_mock);
    [mediator_mock stopMocking];
  }
}

// Tests that blockQuerySubmissionWhileLoading and
// showPageLoadingSnackbarOnOpeningInvocation are YES when
// kAppSwitcherAISummarization is enabled and entry point is
// AppSwitcherAISummarization.
TEST_F(GeminiContainerMediatorTest,
       TestLoadingConfigurationEnabledForAppSwitcher) {
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
  EXPECT_TRUE(config.blockQuerySubmissionWhileLoading);
  EXPECT_TRUE(config.showPageLoadingSnackbarOnOpeningInvocation);
  histogram_tester.ExpectUniqueSample(
      kBlockQuerySubmissionWhileLoadingHistogram, true, 1);
  histogram_tester.ExpectUniqueSample(
      kShowPageLoadingSnackbarOnOpeningInvocationHistogram, true, 1);
}

// Tests that blockQuerySubmissionWhileLoading and
// showPageLoadingSnackbarOnOpeningInvocation are NO when
// kAppSwitcherAISummarization is enabled but entry point is not
// AppSwitcherAISummarization.
TEST_F(GeminiContainerMediatorTest,
       TestLoadingConfigurationDisabledForOtherEntryPoints) {
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
  EXPECT_FALSE(config.blockQuerySubmissionWhileLoading);
  EXPECT_FALSE(config.showPageLoadingSnackbarOnOpeningInvocation);
  histogram_tester.ExpectUniqueSample(
      kBlockQuerySubmissionWhileLoadingHistogram, false, 1);
  histogram_tester.ExpectUniqueSample(
      kShowPageLoadingSnackbarOnOpeningInvocationHistogram, false, 1);
}

// Tests that shouldBlockQuerySubmissionWhileLoadingForEntryPoint returns true
// for AppSwitcherAISummarization when feature is enabled and false otherwise.
TEST_F(GeminiContainerMediatorTest,
       TestShouldBlockQuerySubmissionWhileLoadingForEntryPoint) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {kAppSwitcherAISummarization, kPageActionMenu}, {});

  EXPECT_TRUE([mediator_ shouldBlockQuerySubmissionWhileLoadingForEntryPoint:
                             gemini::EntryPoint::AppSwitcherAISummarization]);
  EXPECT_FALSE([mediator_ shouldBlockQuerySubmissionWhileLoadingForEntryPoint:
                              gemini::EntryPoint::Promo]);
}

// Tests that shouldShowPageLoadingSnackbarOnOpeningInvocationForEntryPoint
// returns true for AppSwitcherAISummarization when feature is enabled and false
// otherwise.
TEST_F(GeminiContainerMediatorTest,
       TestShouldShowPageLoadingSnackbarOnOpeningInvocationForEntryPoint) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {kAppSwitcherAISummarization, kPageActionMenu}, {});

  EXPECT_TRUE(
      [mediator_ shouldShowPageLoadingSnackbarOnOpeningInvocationForEntryPoint:
                     gemini::EntryPoint::AppSwitcherAISummarization]);
  EXPECT_FALSE(
      [mediator_ shouldShowPageLoadingSnackbarOnOpeningInvocationForEntryPoint:
                     gemini::EntryPoint::Promo]);
}

// Tests that changing detent to minimized when container is in zero state and
// Chrome Next IA is enabled dismisses the Gemini flow, while changing detent
// otherwise does not.
TEST_F(GeminiContainerMediatorTest,
       TestDidChangeDetentDismissesInZeroStateChromeNextIa) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {kAssistantContainer, kIOSGeminiBottomSheetMigration, kChromeNextIa}, {});

  FakeGeminiContainerConsumer* consumer =
      [[FakeGeminiContainerConsumer alloc] init];
  mediator_.consumer = consumer;
  [mediator_ connect];

  OCMExpect([mock_gemini_handler_ dismissGeminiFlowWithCompletion:nil]);
  [mediator_ assistantContainer:nil
                didChangeDetent:AssistantContainerDetent::kMinimized];
  EXPECT_OCMOCK_VERIFY(mock_gemini_handler_);

  // When there is an active conversation, changing detent to minimized should
  // not dismiss.
  [mediator_ didUpdateProcessingStatus:ios::provider::GeminiClientMode::
                                           kPreviousConversationLoading
                             sessionID:@"session"
                        conversationID:@"conv"];
  [[mock_gemini_handler_ reject] dismissGeminiFlowWithCompletion:nil];
  [mediator_ assistantContainer:nil
                didChangeDetent:AssistantContainerDetent::kMinimized];
  EXPECT_OCMOCK_VERIFY(mock_gemini_handler_);
}

// Tests that changing detent to minimized when container is in zero state but
// Chrome Next IA is disabled does not dismiss the Gemini flow.
TEST_F(GeminiContainerMediatorTest, TestDidChangeDetentNextIaDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {kAssistantContainer, kIOSGeminiBottomSheetMigration}, {kChromeNextIa});

  FakeGeminiContainerConsumer* consumer =
      [[FakeGeminiContainerConsumer alloc] init];
  mediator_.consumer = consumer;
  [mediator_ connect];

  [[mock_gemini_handler_ reject] dismissGeminiFlowWithCompletion:nil];
  [mediator_ assistantContainer:nil
                didChangeDetent:AssistantContainerDetent::kMinimized];
  EXPECT_OCMOCK_VERIFY(mock_gemini_handler_);
}

// Tests that accessibility escape request dismisses the Gemini flow.
TEST_F(GeminiContainerMediatorTest, TestAssistantContainerDidRequestDismissal) {
  OCMExpect([mock_gemini_handler_ dismissGeminiFlowWithCompletion:nil]);
  [mediator_ assistantContainerDidRequestDismissal:nil];
  EXPECT_OCMOCK_VERIFY(mock_gemini_handler_);
}

// Tests that when detent size is updated during a transition out of zero state
// (e.g., switching to Live view mode) and the container delegate notifies of
// the detent change, the Gemini flow is not dismissed.
TEST_F(GeminiContainerMediatorTest,
       TestDidChangeDetentWhenSwitchingOutOfZeroStateDoesNotDismiss) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {kAssistantContainer, kIOSGeminiBottomSheetMigration, kChromeNextIa}, {});

  FakeGeminiContainerConsumer* consumer =
      [[FakeGeminiContainerConsumer alloc] init];
  mediator_.consumer = consumer;
  [mediator_ connect];

  OCMStub([mock_container_handler_ animateAssistantContainerToDetent:
                                       AssistantContainerDetent::kMinimized])
      .andDo(^(NSInvocation* invocation) {
        [mediator_ assistantContainer:nil
                      didChangeDetent:AssistantContainerDetent::kMinimized];
      });

  __block BOOL dismissed = NO;
  OCMStub([mock_gemini_handler_ dismissGeminiFlowWithCompletion:nil])
      .andDo(^(NSInvocation* invocation) {
        dismissed = YES;
      });

  [mediator_ didSwitchToMode:ios::provider::GeminiViewMode::kLive];
  EXPECT_FALSE(dismissed);
}

// Tests that container detent change updates consumer's worklog compact state.
TEST_F(GeminiContainerMediatorTest, TestDidChangeDetentUpdatesWorklogCompact) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {kAssistantContainer, kIOSGeminiBottomSheetMigration}, {});

  FakeGeminiContainerConsumer* consumer =
      [[FakeGeminiContainerConsumer alloc] init];
  mediator_.consumer = consumer;

  [mediator_ assistantContainer:nil
                didChangeDetent:AssistantContainerDetent::kMinimized];
  EXPECT_TRUE(consumer.worklogCompact);

  [mediator_ assistantContainer:nil
                didChangeDetent:AssistantContainerDetent::kMedium];
  EXPECT_FALSE(consumer.worklogCompact);
}

// Tests that didSelectSuggestion calls UpdatePromptAction with the entry point
// from startupState, the suggestion's query, and shouldAutoSubmit set to YES.
TEST_F(GeminiContainerMediatorTest,
       TestDidSelectSuggestionUpdatesPromptAction) {
  ios::provider::ResetGemini();
  mediator_.startupState = startup_state_;

  ZeroStateSuggestion* suggestion = [[ZeroStateSuggestion alloc] init];
  suggestion.query = @"What is this page about?";

  [mediator_ geminiZeroStateViewController:nil didSelectSuggestion:suggestion];

  EXPECT_THAT(ios::provider::GetLastUpdatePromptActionEntryPoint(),
              testing::Optional(gemini::EntryPoint::Promo));
  EXPECT_NSEQ(@"What is this page about?",
              ios::provider::GetLastUpdatePromptActionPrompt());
  EXPECT_TRUE(ios::provider::GetLastUpdatePromptActionShouldAutoSubmit());
}

// Tests that didSelectSuggestion does not call UpdatePromptAction when
// startupState is nil.
TEST_F(GeminiContainerMediatorTest, TestDidSelectSuggestionNoStartupState) {
  ios::provider::ResetGemini();
  mediator_.startupState = nil;

  ZeroStateSuggestion* suggestion = [[ZeroStateSuggestion alloc] init];
  suggestion.query = @"What is this page about?";

  [mediator_ geminiZeroStateViewController:nil didSelectSuggestion:suggestion];

  EXPECT_EQ(std::nullopt, ios::provider::GetLastUpdatePromptActionEntryPoint());
  EXPECT_EQ(nil, ios::provider::GetLastUpdatePromptActionPrompt());
  EXPECT_FALSE(ios::provider::GetLastUpdatePromptActionShouldAutoSubmit());
}

// Tests that didSelectSuggestion does not call UpdatePromptAction when the
// suggestion query is empty.
TEST_F(GeminiContainerMediatorTest, TestDidSelectSuggestionEmptyQuery) {
  ios::provider::ResetGemini();
  mediator_.startupState = startup_state_;

  ZeroStateSuggestion* suggestion = [[ZeroStateSuggestion alloc] init];
  suggestion.query = @"";

  [mediator_ geminiZeroStateViewController:nil didSelectSuggestion:suggestion];

  EXPECT_EQ(std::nullopt, ios::provider::GetLastUpdatePromptActionEntryPoint());
  EXPECT_EQ(nil, ios::provider::GetLastUpdatePromptActionPrompt());
  EXPECT_FALSE(ios::provider::GetLastUpdatePromptActionShouldAutoSubmit());
}

// Tests that propagatePageContext queries sharedTabsDelegate and
// updates the active attached tab context.
TEST_F(GeminiContainerMediatorTest,
       TestPropagatePageContextQueriesSharedTabsDelegate) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {kGeminiMultiTabContext, kPageActionMenu}, {});

  web::FakeWebState* web_state =
      static_cast<web::FakeWebState*>(AppendActiveWebState());
  web_state->WasShown();
  web_state->SetCurrentURL(GURL("https://example.com"));
  web_state->SetContentsMimeType("text/html");

  id mock_shared_tabs_delegate =
      OCMProtocolMock(@protocol(GeminiSharedTabsDelegate));
  GeminiPageContext* shared_context = [[GeminiPageContext alloc] init];
  OCMStub([mock_shared_tabs_delegate inactiveSharedTabs]).andReturn(@[
    shared_context
  ]);
  GeminiPageContext* active_context = [[GeminiPageContext alloc] init];
  OCMExpect([mock_shared_tabs_delegate
      saveActivePageContextToSharedTabs:active_context]);
  mediator_.sharedTabsDelegate = mock_shared_tabs_delegate;

  [mediator_ propagatePageContext:active_context];

  EXPECT_EQ(ios::provider::GeminiPageContextAttachmentState::kAttached,
            active_context.geminiPageContextAttachmentState);
  EXPECT_NE(ios::provider::GeminiPageContextComputationState::kBlocked,
            active_context.geminiPageContextComputationState);
  EXPECT_OCMOCK_VERIFY(mock_shared_tabs_delegate);
}

// Tests that requestActivePageContextGeneration triggers page context
// generation on the active tab helper.
TEST_F(GeminiContainerMediatorTest, TestRequestActivePageContextGeneration) {
  web::FakeWebState* web_state =
      static_cast<web::FakeWebState*>(AppendActiveWebState());
  web_state->WasShown();
  web_state->SetCurrentURL(GURL("https://example.com"));
  web_state->SetContentsMimeType("text/html");

  id mock_wrapper_class = OCMClassMock([PageContextWrapper class]);
  MediatorFakePageContextWrapper* fake_wrapper =
      [[MediatorFakePageContextWrapper alloc]
            initWithWebState:web_state
          completionCallback:base::DoNothing()];
  OCMStub([mock_wrapper_class alloc]).andReturn(fake_wrapper);

  [mediator_ requestActivePageContextGeneration];

  EXPECT_TRUE(fake_wrapper.populateCalled);
}

// Tests that onFloatyDismiss calls cancelPageContextGeneration.
TEST_F(GeminiContainerMediatorTest,
       TestOnFloatyDismissCallsCancelPageContextGeneration) {
  @autoreleasepool {
    id mediator_mock = OCMPartialMock(mediator_);
    OCMExpect([mediator_mock cancelPageContextGeneration]);

    [mediator_mock onFloatyDismiss];

    EXPECT_OCMOCK_VERIFY(mediator_mock);
    [mediator_mock stopMocking];
  }
}

// Tests that onFloatyDismiss cancels ongoing page context generation if the
// page is loading.
TEST_F(GeminiContainerMediatorTest,
       TestOnFloatyDismissCancelsOngoingPageContextGeneration) {
  web::FakeWebState* web_state =
      static_cast<web::FakeWebState*>(AppendActiveWebState());
  web_state->WasShown();
  web_state->SetCurrentURL(GURL("https://example.com"));
  web_state->SetContentsMimeType("text/html");
  web_state->SetLoading(true);

  id mock_wrapper_class = OCMClassMock([PageContextWrapper class]);
  MediatorFakePageContextWrapper* fake_wrapper =
      [[MediatorFakePageContextWrapper alloc]
            initWithWebState:web_state
          completionCallback:base::DoNothing()];
  OCMStub([mock_wrapper_class alloc]).andReturn(fake_wrapper);

  [mediator_ requestActivePageContextGeneration];
  EXPECT_FALSE(fake_wrapper.populateCalled);

  [mediator_ onFloatyDismiss];

  GeminiTabHelper* tab_helper = GeminiTabHelper::FromWebState(web_state);
  tab_helper->PageLoaded(web_state, web::PageLoadCompletionStatus::SUCCESS);

  EXPECT_FALSE(fake_wrapper.populateCalled);
}

// Tests the actuation lifecycle: activating actuation, updating height, and
// deactivating actuation back to the expanded response.
TEST_F(GeminiContainerMediatorTest, TestActuationLifecycle) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {kAssistantContainer, kIOSGeminiBottomSheetMigration}, {});

  FakeGeminiContainerConsumer* consumer =
      [[FakeGeminiContainerConsumer alloc] init];
  mediator_.consumer = consumer;

  // Actuation begins: sheet minimizes with grabber shown.
  OCMExpect([mock_container_handler_
      animateAssistantContainerToDetent:AssistantContainerDetent::kMinimized]);
  OCMExpect([mock_container_handler_ setAssistantContainerGrabberHidden:NO
                                                               animated:YES]);
  [mediator_ setActuationActive:YES];
  EXPECT_OCMOCK_VERIFY(mock_container_handler_);
  EXPECT_TRUE(consumer.isActuationActive);

  // Worklog reports height: minimized detent height updates.
  OCMExpect(
      [mock_container_handler_ setAssistantContainerMinimizedDetentHeight:120]);
  OCMExpect([mock_container_handler_
      animateAssistantContainerToDetent:AssistantContainerDetent::kMinimized]);
  [mediator_ containerDidChangeActuationHeight:120];
  EXPECT_OCMOCK_VERIFY(mock_container_handler_);

  // Actuation ends: detent height resets and expands to response.
  [mediator_
      didUpdateProcessingStatus:ios::provider::GeminiClientMode::kResponding
                      sessionID:nil
                 conversationID:nil];
  OCMExpect(
      [mock_container_handler_ setAssistantContainerMinimizedDetentHeight:
                                   kAssistantContainerMinimizedDetentHeight]);
  OCMExpect([mock_container_handler_
      animateAssistantContainerToDetent:AssistantContainerDetent::kMedium]);
  OCMExpect([mock_container_handler_ setAssistantContainerGrabberHidden:NO
                                                               animated:YES]);
  [mediator_ setActuationActive:NO];
  EXPECT_OCMOCK_VERIFY(mock_container_handler_);
  EXPECT_FALSE(consumer.isActuationActive);
}

// Test that switching the active `WebState` updates the page context via
// `ios::provider::UpdateActivePageContext`, notifies `sharedTabsDelegate`, and
// transfers the `GeminiTabHelper` observer from the old `WebState` to the new
// active `WebState` only while floaty is invoked.
TEST_F(GeminiContainerMediatorTest,
       TestActiveWebStateChangedUpdatesPageContextAndObservers) {
  id mock_shared_tabs_delegate =
      OCMProtocolMock(@protocol(GeminiSharedTabsDelegate));
  mediator_.sharedTabsDelegate = mock_shared_tabs_delegate;

  // Insert and activate the first `WebState` before floaty is invoked.
  web::FakeWebState* first_web_state = AppendActiveWebState();
  first_web_state->SetTitle(u"Initial Title Before Invoke");
  EXPECT_EQ(0, ios::provider::GetUpdateActivePageContextCallCount());

  // Invoke floaty to attach observers.
  [mediator_ onFloatyInvoked];

  // Updating the active `WebState` while invoked should trigger a page context
  // update.
  first_web_state->SetTitle(u"First WebState Title While Invoked");
  EXPECT_EQ(1, ios::provider::GetUpdateActivePageContextCallCount());

  // Insert and activate a second `WebState`.
  auto second_web_state_owned = std::make_unique<web::FakeWebState>();
  web::FakeWebState* second_web_state = second_web_state_owned.get();
  second_web_state->SetBrowserState(profile_.get());
  second_web_state->SetCurrentURL(GURL("chrome://newtab/"));
  second_web_state->SetNavigationManager(
      std::make_unique<web::FakeNavigationManager>());
  GeminiTabHelper::CreateForWebState(second_web_state);

  OCMExpect([mock_shared_tabs_delegate
      updateSharedTabsForActiveWebState:second_web_state]);
  browser_->GetWebStateList()->InsertWebState(
      std::move(second_web_state_owned),
      WebStateList::InsertionParams::Automatic().Activate(true));
  EXPECT_OCMOCK_VERIFY(mock_shared_tabs_delegate);
  EXPECT_EQ(2, ios::provider::GetUpdateActivePageContextCallCount());

  // Updating the title on the inactive first `WebState` should not trigger
  // a page context update on the mediator.
  first_web_state->SetTitle(u"First WebState Updated Title");
  EXPECT_EQ(2, ios::provider::GetUpdateActivePageContextCallCount());

  // Updating the title on the active second `WebState` should trigger a page
  // context update on the mediator.
  second_web_state->SetTitle(u"Second WebState Updated Title");
  EXPECT_EQ(3, ios::provider::GetUpdateActivePageContextCallCount());

  // Dismissing floaty resets Gemini provider state, detaches observers, and
  // stops further updates.
  [mediator_ onFloatyDismiss];
  second_web_state->SetTitle(u"Title Update After Dismiss");
  browser_->GetWebStateList()->ActivateWebStateAt(0);
  EXPECT_EQ(0, ios::provider::GetUpdateActivePageContextCallCount());
}

// Test that `OnPageContextUpdated` skips updating page context when in Live
// mode and the processing status in `_stateManager` is `kTranscribing`, and
// resumes updating once `kTranscribing` ends.
TEST_F(GeminiContainerMediatorTest,
       TestPageContextUpdatedSkipsUpdateWhenTranscribing) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({kGeminiLive, kPageActionMenu}, {});

  web::FakeWebState* web_state = AppendActiveWebState();
  [mediator_ onFloatyInvoked];

  web_state->SetTitle(u"Initial Title While Invoked");
  EXPECT_EQ(1, ios::provider::GetUpdateActivePageContextCallCount());

  // Switch to Live mode and transition processing status to `kTranscribing`.
  // Transitioning to `kTranscribing` requests active page context generation.
  [mediator_ didSwitchToMode:ios::provider::GeminiViewMode::kLive];
  [mediator_
      didUpdateProcessingStatus:ios::provider::GeminiClientMode::kTranscribing
                      sessionID:@"session"
                 conversationID:@"conv"];
  EXPECT_EQ(2, ios::provider::GetUpdateActivePageContextCallCount());

  // Updating the page context while transcribing in Live mode should be
  // ignored.
  web_state->SetTitle(u"Ignored Title While Transcribing");
  EXPECT_EQ(2, ios::provider::GetUpdateActivePageContextCallCount());

  // Transitioning processing status out of `kTranscribing` allows updates
  // again.
  [mediator_
      didUpdateProcessingStatus:ios::provider::GeminiClientMode::kListening
                      sessionID:@"session"
                 conversationID:@"conv"];
  web_state->SetTitle(u"Updated Title After Listening");
  EXPECT_EQ(3, ios::provider::GetUpdateActivePageContextCallCount());
}

// Test that `didUpdateProcessingStatus` requests full page context generation
// (`GeneratePageContext`) on `kTranscribing` and updates partial page context
// (`GetPartialPageContext`) on `kResponding` only when in Gemini Live mode.
TEST_F(GeminiContainerMediatorTest,
       TestDidUpdateProcessingStatusUpdatesPageContextInLiveMode) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({kGeminiLive, kPageActionMenu}, {});

  web::FakeWebState* web_state = AppendActiveWebState();
  web_state->WasShown();
  web_state->SetCurrentURL(GURL("https://example.com"));
  web_state->SetContentsMimeType("text/html");

  id mock_wrapper_class = OCMClassMock([PageContextWrapper class]);
  MediatorFakePageContextWrapper* fake_wrapper =
      [[MediatorFakePageContextWrapper alloc]
            initWithWebState:web_state
          completionCallback:base::DoNothing()];
  OCMStub([mock_wrapper_class alloc]).andReturn(fake_wrapper);

  id mock_shared_tabs_delegate =
      OCMProtocolMock(@protocol(GeminiSharedTabsDelegate));
  mediator_.sharedTabsDelegate = mock_shared_tabs_delegate;

  // In non-Live mode (`kFloaty`), `kTranscribing` and `kResponding` should not
  // trigger full or partial page context updates.
  [mediator_ didSwitchToMode:ios::provider::GeminiViewMode::kFloaty];
  [mediator_
      didUpdateProcessingStatus:ios::provider::GeminiClientMode::kTranscribing
                      sessionID:@"session"
                 conversationID:@"conv"];
  [mediator_
      didUpdateProcessingStatus:ios::provider::GeminiClientMode::kResponding
                      sessionID:@"session"
                 conversationID:@"conv"];
  EXPECT_FALSE(fake_wrapper.populateCalled);
  EXPECT_EQ(0, ios::provider::GetUpdateActivePageContextCallCount());

  // Switch to Live mode.
  [mediator_ didSwitchToMode:ios::provider::GeminiViewMode::kLive];

  // Transitioning to `kTranscribing` in Live mode requests full page context
  // generation via `tabHelper->GeneratePageContext`, which completes and calls
  // `propagatePageContext` with `geminiPageContextComputationState ==
  // kSuccess`.
  OCMExpect([mock_shared_tabs_delegate
      saveActivePageContextToSharedTabs:[OCMArg checkWithBlock:^BOOL(id obj) {
        GeminiPageContext* context = static_cast<GeminiPageContext*>(obj);
        return context.geminiPageContextComputationState ==
                   ios::provider::GeminiPageContextComputationState::kSuccess &&
               context.uniquePageContext != nullptr;
      }]]);
  [mediator_
      didUpdateProcessingStatus:ios::provider::GeminiClientMode::kTranscribing
                      sessionID:@"session"
                 conversationID:@"conv"];
  EXPECT_TRUE(fake_wrapper.populateCalled);
  EXPECT_EQ(1, ios::provider::GetUpdateActivePageContextCallCount());
  EXPECT_OCMOCK_VERIFY(mock_shared_tabs_delegate);

  // Reset `populateCalled` to verify subsequent statuses do not trigger full
  // page context generation.
  fake_wrapper.populateCalled = NO;

  // Transitioning to `kThinking` does not update page context.
  [mediator_
      didUpdateProcessingStatus:ios::provider::GeminiClientMode::kThinking
                      sessionID:@"session"
                 conversationID:@"conv"];
  EXPECT_FALSE(fake_wrapper.populateCalled);
  EXPECT_EQ(1, ios::provider::GetUpdateActivePageContextCallCount());

  // Transitioning to `kResponding` in Live mode updates partial page context
  // via `tabHelper->GetPartialPageContext` (`geminiPageContextComputationState
  // == kPending`) without triggering `PageContextWrapper`.
  OCMExpect([mock_shared_tabs_delegate
      saveActivePageContextToSharedTabs:[OCMArg checkWithBlock:^BOOL(id obj) {
        GeminiPageContext* context = static_cast<GeminiPageContext*>(obj);
        return context.geminiPageContextComputationState ==
                   ios::provider::GeminiPageContextComputationState::kPending &&
               context.uniquePageContext != nullptr;
      }]]);
  [mediator_
      didUpdateProcessingStatus:ios::provider::GeminiClientMode::kResponding
                      sessionID:@"session"
                 conversationID:@"conv"];
  EXPECT_FALSE(fake_wrapper.populateCalled);
  EXPECT_EQ(2, ios::provider::GetUpdateActivePageContextCallCount());
  EXPECT_OCMOCK_VERIFY(mock_shared_tabs_delegate);

  [mock_wrapper_class stopMocking];
}

// Test that `assistantContainer:didChangeDetent:` requests full page context
// generation when the detent changes from `kMinimized` to `kMedium` or
// `kLarge`, but not for other detent transitions.
TEST_F(
    GeminiContainerMediatorTest,
    TestDidChangeDetentRequestsPageContextGenerationWhenExpandingFromMinimized) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {kAssistantContainer, kIOSGeminiBottomSheetMigration, kPageActionMenu},
      {});

  web::FakeWebState* web_state = AppendActiveWebState();
  web_state->WasShown();
  web_state->SetCurrentURL(GURL("https://example.com"));
  web_state->SetContentsMimeType("text/html");

  id mock_wrapper_class = OCMClassMock([PageContextWrapper class]);
  MediatorFakePageContextWrapper* fake_wrapper =
      [[MediatorFakePageContextWrapper alloc]
            initWithWebState:web_state
          completionCallback:base::DoNothing()];
  OCMStub([mock_wrapper_class alloc]).andReturn(fake_wrapper);

  FakeGeminiContainerConsumer* consumer =
      [[FakeGeminiContainerConsumer alloc] init];
  mediator_.consumer = consumer;
  [mediator_ connect];
  EXPECT_TRUE(fake_wrapper.populateCalled);
  fake_wrapper.populateCalled = NO;

  // Transitioning from `kMedium` to `kLarge` should not request context
  // generation.
  [mediator_ assistantContainer:nil
                didChangeDetent:AssistantContainerDetent::kLarge];
  EXPECT_FALSE(fake_wrapper.populateCalled);

  // Transitioning from `kLarge` to `kMinimized` should not request context
  // generation.
  [mediator_ assistantContainer:nil
                didChangeDetent:AssistantContainerDetent::kMinimized];
  EXPECT_FALSE(fake_wrapper.populateCalled);

  // Transitioning from `kMinimized` to `kMedium` should request full page
  // context generation.
  [mediator_ assistantContainer:nil
                didChangeDetent:AssistantContainerDetent::kMedium];
  EXPECT_TRUE(fake_wrapper.populateCalled);
  fake_wrapper.populateCalled = NO;

  // Transitioning back to `kMinimized` and then to `kLarge` should also request
  // full page context generation.
  [mediator_ assistantContainer:nil
                didChangeDetent:AssistantContainerDetent::kMinimized];
  EXPECT_FALSE(fake_wrapper.populateCalled);

  [mediator_ assistantContainer:nil
                didChangeDetent:AssistantContainerDetent::kLarge];
  EXPECT_TRUE(fake_wrapper.populateCalled);

  [mock_wrapper_class stopMocking];
}

}  // namespace
