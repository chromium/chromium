// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/model/gemini_browser_agent.h"

#import "base/run_loop.h"
#import "base/strings/utf_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/run_until.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "components/favicon/core/favicon_service.h"
#import "components/favicon/ios/web_favicon_driver.h"
#import "components/feature_engagement/public/event_constants.h"
#import "components/feature_engagement/test/mock_tracker.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/optimization_guide/proto/features/common_quality_data.pb.h"
#import "components/signin/public/base/consent_level.h"
#import "components/signin/public/identity_manager/primary_account_change_event.h"
#import "ios/chrome/browser/favicon/model/favicon_service_factory.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/fullscreen_controller.h"
#import "ios/chrome/browser/intelligence/bwg/metrics/gemini_metrics.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_configuration.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_page_context.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_tab_helper.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/intelligence/proto_wrappers/page_context_extractor_java_script_feature.h"
#import "ios/chrome/browser/intelligence/proto_wrappers/page_context_wrapper.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service_factory.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_manager_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/fullscreen_commands.h"
#import "ios/chrome/browser/shared/public/commands/gemini_commands.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/public/snackbar/snackbar_message.h"
#import "ios/chrome/browser/snapshots/model/fake_snapshot_generator_delegate.h"
#import "ios/chrome/browser/snapshots/model/snapshot_source_tab_helper.h"
#import "ios/chrome/browser/snapshots/model/snapshot_tab_helper.h"
#import "ios/chrome/browser/web/model/web_view_proxy/web_view_proxy_tab_helper.h"
#import "ios/chrome/common/app_group/app_group_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/find_in_page/find_in_page_java_script_feature.h"
#import "ios/web/js_messaging/java_script_feature_manager.h"
#import "ios/web/public/js_messaging/java_script_feature_util.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_client.h"
#import "ios/web/public/test/fakes/fake_web_frame.h"
#import "ios/web/public/test/fakes/fake_web_frames_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/js_test_util.h"
#import "ios/web/public/test/scoped_testing_web_client.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "ui/base/l10n/l10n_util.h"

namespace {
std::unique_ptr<KeyedService> BuildFeatureEngagementMockTracker(
    ProfileIOS* profile) {
  return std::make_unique<feature_engagement::test::MockTracker>();
}
}  // namespace

// Test fixture for GeminiBrowserAgent.
class GeminiBrowserAgentTest : public PlatformTest {
 protected:
  GeminiBrowserAgentTest()
      : web_client_(std::make_unique<web::FakeWebClient>()),
        task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    feature_list_.InitWithFeatures(
        {kPageActionMenu, kPageContextExtractorRefactored}, {});
    static_cast<web::FakeWebClient*>(web_client_.Get())
        ->SetJavaScriptFeatures(
            {web::FindInPageJavaScriptFeature::GetInstance(),
             PageContextExtractorJavaScriptFeature::GetInstance()});
    TestProfileIOS::Builder profile_builder;
    profile_builder.AddTestingFactory(
        OptimizationGuideServiceFactory::GetInstance(),
        OptimizationGuideServiceFactory::GetDefaultFactory());
    profile_builder.AddTestingFactory(
        feature_engagement::TrackerFactory::GetInstance(),
        base::BindRepeating(&BuildFeatureEngagementMockTracker));
    profile_ =
        profile_manager_.AddProfileWithBuilder(std::move(profile_builder));
    mock_tracker_ = static_cast<feature_engagement::test::MockTracker*>(
        feature_engagement::TrackerFactory::GetForProfile(profile_));
    web::JavaScriptFeatureManager::FromBrowserState(profile_)
        ->ConfigureFeatures(
            {web::FindInPageJavaScriptFeature::GetInstance(),
             PageContextExtractorJavaScriptFeature::GetInstance()});
    SceneState* scene_state = [[SceneState alloc] initWithAppState:nil];
    browser_ = std::make_unique<TestBrowser>(profile_, scene_state);
    GeminiBrowserAgent::CreateForBrowser(browser_.get());
    gemini_browser_agent_ = GeminiBrowserAgent::FromBrowser(browser_.get());

    optimization_guide_service_ =
        OptimizationGuideServiceFactory::GetForProfile(profile_);

    mock_settings_handler_ = OCMProtocolMock(@protocol(SettingsCommands));
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:mock_settings_handler_
                     forProtocol:@protocol(SettingsCommands)];
    mock_gemini_handler_ = OCMProtocolMock(@protocol(GeminiCommands));
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:mock_gemini_handler_
                     forProtocol:@protocol(GeminiCommands)];

    std::unique_ptr<web::FakeWebState> web_state =
        std::make_unique<web::FakeWebState>();
    web_state_ = web_state.get();
    web_state->SetBrowserState(profile_);
    web_state->SetCurrentURL(GURL("chrome://newtab/"));
    web_state->SetNavigationManager(
        std::make_unique<web::FakeNavigationManager>());
    GeminiTabHelper::CreateForWebState(web_state.get());
    WebViewProxyTabHelper::CreateForWebState(web_state.get());
    gemini_tab_helper_ = GeminiTabHelper::FromWebState(web_state.get());

    SnapshotTabHelper::CreateForWebState(web_state.get());
    SnapshotSourceTabHelper::CreateForWebState(web_state.get());
    fake_snapshot_delegate_ = [[FakeSnapshotGeneratorDelegate alloc] init];
    fake_snapshot_delegate_.view = [[UIView alloc] init];
    web_state->SetView(fake_snapshot_delegate_.view);
    web_state->SetCanTakeSnapshot(true);
    SnapshotTabHelper::FromWebState(web_state.get())
        ->SetDelegate(fake_snapshot_delegate_);

    favicon::WebFaviconDriver::CreateForWebState(
        web_state.get(), ios::FaviconServiceFactory::GetForProfile(
                             profile_, ServiceAccessType::IMPLICIT_ACCESS));

    auto web_frames_manager = std::make_unique<web::FakeWebFramesManager>();
    auto main_frame =
        web::FakeWebFrame::CreateMainWebFrame(GURL("https://example.com"));
    fake_main_frame_ = main_frame.get();
    main_frame->set_browser_state(profile_);
    web_frames_manager->AddWebFrame(std::move(main_frame));
    web_state->SetWebFramesManager(web::ContentWorld::kIsolatedWorld,
                                   std::move(web_frames_manager));

    // Also set for kPageContentWorld as PageContextExtractor might use it
    // depending on flags
    auto page_content_frames_manager =
        std::make_unique<web::FakeWebFramesManager>();
    auto main_frame_page_content =
        web::FakeWebFrame::CreateMainWebFrame(GURL("https://example.com"));
    main_frame_page_content->set_browser_state(profile_);
    page_content_frames_manager->AddWebFrame(
        std::move(main_frame_page_content));
    web_state->SetWebFramesManager(web::ContentWorld::kPageContentWorld,
                                   std::move(page_content_frames_manager));

    browser_->GetWebStateList()->InsertWebState(
        std::move(web_state),
        WebStateList::InsertionParams::Automatic().Activate(true));
  }

  void TearDown() override {
    fake_main_frame_ = nullptr;
    web_state_ = nullptr;
    profile_ = nullptr;
    gemini_browser_agent_ = nullptr;
    gemini_tab_helper_ = nullptr;
    optimization_guide_service_ = nullptr;
    mock_settings_handler_ = nullptr;
    mock_gemini_handler_ = nullptr;
    fake_snapshot_delegate_ = nullptr;
    browser_.reset();
    profile_manager_.PrepareForDestruction();
  }

  // Getter for `is_floaty_invoked_`.
  bool IsFloatyInvoked() { return gemini_browser_agent_->is_floaty_invoked_; }

  // Getter for `is_floaty_temporarily_hidden_`.
  bool IsFloatyTemporarilyHidden() {
    return gemini_browser_agent_->is_floaty_temporarily_hidden_;
  }

  // Getter for `last_shown_view_state_`.
  ios::provider::GeminiViewState GetLastShownViewState() {
    return gemini_browser_agent_->last_shown_view_state_;
  }

  // Returns true if the conversation ID preference is empty.
  bool IsConversationIdPrefCleared() {
    return profile_->GetPrefs()
        ->GetString(prefs::kGeminiConversationId)
        .empty();
  }

  // Setter for `is_floaty_invoked_`.
  void SetIsFloatyInvoked(bool is_invoked) {
    gemini_browser_agent_->is_floaty_invoked_ = is_invoked;
  }

  // Setter for `is_floaty_temporarily_hidden_`.
  void SetIsFloatyTemporarilyHidden(bool is_hidden) {
    gemini_browser_agent_->is_floaty_temporarily_hidden_ = is_hidden;
  }

  // Setter for `floaty_hidden_timestamp_`.
  // Wrapper for `InvokeFloaty`.
  void InvokeFloaty(GeminiConfiguration* config) {
    gemini_browser_agent_->InvokeFloaty(config);
  }
  void SetFloatyHiddenTimestamp(base::TimeTicks timestamp) {
    gemini_browser_agent_->floaty_hidden_timestamp_ = timestamp;
  }

  // Triggers `RequestPageContextGeneration()` in the browser agent.
  void RequestPageContextGeneration() {
    gemini_browser_agent_->RequestPageContextGeneration();
  }

  // Wrapper for `CreateGeminiConfiguration`.
  GeminiConfiguration* CreateGeminiConfiguration(
      UIViewController* base_view_controller,
      GeminiStartupState* startup_state,
      web::WebState* web_state,
      GeminiPageContext* page_context) {
    return gemini_browser_agent_->CreateGeminiConfiguration(
        base_view_controller, startup_state, web_state, page_context);
  }

  // Setter for `processing_status_`.
  void SetProcessingStatus(ios::provider::GeminiClientMode mode) {
    gemini_browser_agent_->processing_status_ = mode;
  }

  // Getter for `processing_status_`.
  ios::provider::GeminiClientMode GetProcessingStatus() {
    return gemini_browser_agent_->processing_status_;
  }

  // Getter for raw `attached_tabs_` member.
  std::map<web::WebStateID, GeminiPageContext*> GetRawAttachedTabs() {
    return gemini_browser_agent_->attached_tabs_;
  }

  // Setter for raw `attached_tabs_` member.
  void SetRawAttachedTab(web::WebStateID id, GeminiPageContext* context) {
    gemini_browser_agent_->attached_tabs_[id] = context;
  }

  // Wrapper for `DetachTabWithID`.
  void DetachTabWithID(NSString* tab_id) {
    gemini_browser_agent_->DetachTabWithID(tab_id);
  }

  base::test::ScopedFeatureList feature_list_;
  web::ScopedTestingWebClient web_client_;
  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  std::unique_ptr<TestBrowser> browser_;
  TestProfileManagerIOS profile_manager_;
  raw_ptr<TestProfileIOS> profile_;
  raw_ptr<GeminiBrowserAgent> gemini_browser_agent_;
  raw_ptr<GeminiTabHelper> gemini_tab_helper_;
  raw_ptr<OptimizationGuideService> optimization_guide_service_;
  raw_ptr<web::FakeWebState> web_state_;
  raw_ptr<web::FakeWebFrame> fake_main_frame_;
  id mock_settings_handler_;
  id mock_gemini_handler_;
  FakeSnapshotGeneratorDelegate* fake_snapshot_delegate_;
  raw_ptr<feature_engagement::test::MockTracker> mock_tracker_;
};

// A test observer for GeminiBrowserAgent.
class TestGeminiObserver : public GeminiBrowserAgent::Observer {
 public:
  void OnFloatyInvokedChanged(bool is_invoked) override {
    is_invoked_ = is_invoked;
    call_count_++;
  }
  void OnGeminiAvailabilityChanged(bool available) override {
    available_ = available;
    availability_call_count_++;
  }
  bool is_invoked_ = false;
  int call_count_ = 0;
  bool available_ = false;
  int availability_call_count_ = 0;
};

// Tests that the GeminiBrowserAgent can be instantiated.
TEST_F(GeminiBrowserAgentTest, TestGeminiBrowserAgentInstantiation) {
  EXPECT_NE(nullptr, gemini_browser_agent_);
}

// Tests that observers are notified when the floaty invocation state changes.
TEST_F(GeminiBrowserAgentTest, TestObserverNotification) {
  TestGeminiObserver observer;
  gemini_browser_agent_->AddObserver(&observer);

  // Set invoked.
  SetIsFloatyInvoked(false);
  InvokeFloaty([[GeminiConfiguration alloc] init]);
  EXPECT_TRUE(observer.is_invoked_);
  EXPECT_EQ(1, observer.call_count_);

  // Dismiss.
  gemini_browser_agent_->DismissFloaty();
  EXPECT_FALSE(observer.is_invoked_);
  EXPECT_EQ(2, observer.call_count_);

  gemini_browser_agent_->RemoveObserver(&observer);
}

// Tests the presentation of the BWG overlay and state of tab helper side
// effects.
TEST_F(GeminiBrowserAgentTest, TestGeminiBrowserAgentStartGeminiFlow) {
  UIViewController* base_view_controller = [[UIViewController alloc] init];

  // Set a valid URL.
  web_state_->SetCurrentURL(GURL("https://example.com"));
  web_state_->SetContentsMimeType("text/html");

  // Add a fake JS result for page context extraction.
  base::DictValue result;
  result.Set("currentNodeInnerText", "Example Text");
  fake_main_frame_->AddJsResultForFunctionCall(
      std::make_unique<base::Value>(std::move(result)).release(),
      "pageContextExtractor.extractPageContext");

  // Simulate FRE completion.
  profile_->GetPrefs()->SetBoolean(prefs::kIOSBwgConsent, true);

  // Create a protocol mock to intercept the delegate call.
  id mock_delegate = OCMProtocolMock(@protocol(SnapshotGeneratorDelegate));

  // Set the mock as the delegate.
  SnapshotTabHelper::FromWebState(web_state_)->SetDelegate(mock_delegate);

  // Expect the snapshot delegate to be notified. Use a flag to wait for the
  // async call. Use shared_ptr to safely share state between ObjC block and C++
  // lambda.
  auto delegate_called = std::make_shared<bool>(false);
  [[[mock_delegate expect] andDo:^(NSInvocation*) {
    *delegate_called = true;
  }] willUpdateSnapshotWithWebStateInfo:[OCMArg any]];

  // Stub the canTakeSnapshot method to return YES.
  OCMStub([mock_delegate canTakeSnapshotWithWebStateInfo:[OCMArg any]])
      .andReturn(YES);

  // Ensure the WebState is visible so PageContextWrapper attempts a snapshot.
  web_state_->WasShown();

  base::HistogramTester histogram_tester;

  gemini_browser_agent_->StartGeminiFlow(
      base_view_controller, [[GeminiStartupState alloc]
                                initWithEntryPoint:gemini::EntryPoint::Promo]);

  // Wait for the delegate method to be called.
  ASSERT_TRUE(
      base::test::RunUntil([delegate_called]() { return *delegate_called; }));

  [mock_delegate verify];

  histogram_tester.ExpectUniqueSample(
      kGeminiInvocationPageTypeHistogram,
      IOSGeminiInvocationPageType::kExtractableWebPage, 1);
  EXPECT_EQ(gemini_browser_agent_->GetEntryPoint(), gemini::EntryPoint::Promo);
}

// Tests that switching active web states handles observations correctly.
TEST_F(GeminiBrowserAgentTest, TestActiveWebStateChanged) {
  // Create a new browser to ensure the GeminiBrowserAgent is initialized with
  // the feature flag enabled.

  std::unique_ptr<TestBrowser> scoped_browser =
      std::make_unique<TestBrowser>(profile_);
  GeminiBrowserAgent::CreateForBrowser(scoped_browser.get());
  GeminiBrowserAgent* agent =
      GeminiBrowserAgent::FromBrowser(scoped_browser.get());

  std::unique_ptr<web::FakeWebState> web_state1 =
      std::make_unique<web::FakeWebState>();
  web_state1->SetBrowserState(profile_);
  GeminiTabHelper::CreateForWebState(web_state1.get());
  WebViewProxyTabHelper::CreateForWebState(web_state1.get());
  GeminiTabHelper* helper1 = GeminiTabHelper::FromWebState(web_state1.get());

  scoped_browser->GetWebStateList()->InsertWebState(
      std::move(web_state1),
      WebStateList::InsertionParams::Automatic().Activate(true));

  // Verify that the agent is observing the first tab helper.
  EXPECT_TRUE(helper1->HasObserver(agent));

  std::unique_ptr<web::FakeWebState> web_state2 =
      std::make_unique<web::FakeWebState>();
  web_state2->SetBrowserState(profile_);
  GeminiTabHelper::CreateForWebState(web_state2.get());
  WebViewProxyTabHelper::CreateForWebState(web_state2.get());
  GeminiTabHelper* helper2 = GeminiTabHelper::FromWebState(web_state2.get());

  // Switch to new web state.
  scoped_browser->GetWebStateList()->InsertWebState(
      std::move(web_state2),
      WebStateList::InsertionParams::Automatic().Activate(true));

  // Verify that the agent stopped observing the first tab helper and started
  // observing the second one.
  EXPECT_FALSE(helper1->HasObserver(agent));
  EXPECT_TRUE(helper2->HasObserver(agent));
}

// Tests that RequestPageContextGeneration triggers page context generation.
TEST_F(GeminiBrowserAgentTest, TestRequestPageContextGeneration) {
  // Set a valid URL.
  web_state_->SetCurrentURL(GURL("https://example.com"));
  web_state_->SetContentsMimeType("text/html");

  // Add a fake JS result for page context extraction.
  base::DictValue result;
  result.Set("currentNodeInnerText", "Example Text");
  fake_main_frame_->AddJsResultForFunctionCall(
      std::make_unique<base::Value>(std::move(result)).release(),
      "pageContextExtractor.extractPageContext");

  // Create a protocol mock to intercept the delegate call.
  id mock_delegate = OCMProtocolMock(@protocol(SnapshotGeneratorDelegate));

  // Set the mock as the delegate.
  SnapshotTabHelper::FromWebState(web_state_)->SetDelegate(mock_delegate);

  // Expect the snapshot delegate to be notified.
  auto delegate_called = std::make_shared<bool>(false);
  [[[mock_delegate expect] andDo:^(NSInvocation*) {
    *delegate_called = true;
  }] willUpdateSnapshotWithWebStateInfo:[OCMArg any]];

  // Stub the canTakeSnapshot method to return YES.
  OCMStub([mock_delegate canTakeSnapshotWithWebStateInfo:[OCMArg any]])
      .andReturn(YES);

  // Ensure the WebState is visible so PageContextWrapper attempts a snapshot.
  web_state_->WasShown();

  RequestPageContextGeneration();

  // Wait for the delegate method to be called.
  ASSERT_TRUE(
      base::test::RunUntil([delegate_called]() { return *delegate_called; }));

  [mock_delegate verify];
}

// Tests hiding the floaty.
TEST_F(GeminiBrowserAgentTest, TestHideFloatyIfInvoked) {
  SetIsFloatyInvoked(true);
  gemini_browser_agent_->HideFloatyIfInvoked(
      /*animated=*/true, /*source=*/gemini::FloatyUpdateSource::ViewTransition);
  // This test is not connected to the provider APIs, so we mock setting the
  // `last_shown_view_state_`.
  gemini_browser_agent_->SetLastShownViewState(
      ios::provider::GeminiViewState::kExpanded);

  EXPECT_TRUE(IsFloatyTemporarilyHidden());
  EXPECT_EQ(ios::provider::GeminiViewState::kExpanded, GetLastShownViewState());
}

// Tests if a floaty is shown if a user dismisses a view controller and the
// dismissed view controller is not due to a transition to a new view
// controller.
TEST_F(GeminiBrowserAgentTest, TestShowFloatyIfInvoked) {
  SetIsFloatyInvoked(true);
  gemini_browser_agent_->HideFloatyIfInvoked(
      /*animated=*/true, /*source=*/gemini::FloatyUpdateSource::ViewTransition);
  gemini_browser_agent_->SetLastShownViewState(
      ios::provider::GeminiViewState::kExpanded);

  // Set the hidden timestamp to be long enough in the past. Simulates a user
  // staying on a view controller for more than the transition time.
  SetFloatyHiddenTimestamp(base::TimeTicks::Now() - base::Seconds(5));

  gemini_browser_agent_->ShowFloatyIfInvoked(
      /*animated=*/true, /*source=*/gemini::FloatyUpdateSource::ViewTransition);

  EXPECT_FALSE(IsFloatyTemporarilyHidden());
  EXPECT_EQ(ios::provider::GeminiViewState::kExpanded, GetLastShownViewState());
}

// Tests if a floaty is shown regardless of transition time when the source is
// from a web navigation
TEST_F(GeminiBrowserAgentTest, TestShowFloatyIfInvokedWithWebNavigation) {
  SetIsFloatyInvoked(true);
  gemini_browser_agent_->HideFloatyIfInvoked(
      /*animated=*/true, /*source=*/gemini::FloatyUpdateSource::ViewTransition);
  gemini_browser_agent_->SetLastShownViewState(
      ios::provider::GeminiViewState::kExpanded);

  gemini_browser_agent_->ShowFloatyIfInvoked(
      /*animated=*/true, /*source=*/gemini::FloatyUpdateSource::WebNavigation);

  EXPECT_FALSE(IsFloatyTemporarilyHidden());
  EXPECT_EQ(ios::provider::GeminiViewState::kExpanded, GetLastShownViewState());
}

// Tests if a floaty is shown during a simulated view controller transition.
TEST_F(GeminiBrowserAgentTest,
       TestShowFloatyIfInvokedDuringViewControllerTransition) {
  SetIsFloatyInvoked(true);

  // Emulates a new view controller being presented.
  gemini_browser_agent_->HideFloatyIfInvoked(
      /*animated=*/true, /*source=*/gemini::FloatyUpdateSource::ViewTransition);
  // This test is not connected to the provider APIs, so we mock setting the
  // `last_shown_view_state_`.
  gemini_browser_agent_->SetLastShownViewState(
      ios::provider::GeminiViewState::kExpanded);

  // Emulates the old view controller being dismissed.
  gemini_browser_agent_->ShowFloatyIfInvoked(
      /*animated=*/true, /*source=*/gemini::FloatyUpdateSource::ViewTransition);

  // The floaty should still be considered temporarily hidden.
  EXPECT_TRUE(IsFloatyTemporarilyHidden());

  // The last shown view state will be kUnknown, as GetCurrentGeminiViewState()
  // is not mocked.
  EXPECT_EQ(ios::provider::GeminiViewState::kExpanded, GetLastShownViewState());
}

// Tests that the floaty is not dismissed when `DismissFloaty` is called to
// clean up properties but a user has not interacted with floaty UI to properly
// dismiss it.
TEST_F(GeminiBrowserAgentTest, TestDismissFloatyWhenTemporarilyHidden) {
  SetIsFloatyInvoked(true);
  gemini_browser_agent_->HideFloatyIfInvoked(
      /*animated=*/true, /*source=*/gemini::FloatyUpdateSource::ViewTransition);

  gemini_browser_agent_->DismissFloaty();

  // is_floaty_invoked_ should still be true.
  EXPECT_TRUE(IsFloatyInvoked());
}

// Tests that the floaty is properly dismissed when the floaty is shown. With
// Copresence, a user can only dismiss the floaty while interacting with the
// floaty i.e. when the floaty is shown.
TEST_F(GeminiBrowserAgentTest, TestDismissFloatyWhenFloatyIsShown) {
  SetIsFloatyInvoked(true);
  gemini_browser_agent_->DismissFloaty();

  EXPECT_FALSE(IsFloatyInvoked());
}

// Without mocking the provider, we cannot assert the UI state. This test
// ensures the method doesn't crash.
TEST_F(GeminiBrowserAgentTest, TestCollapseFloatyIfInvoked) {
  SetIsFloatyInvoked(true);
  gemini_browser_agent_->CollapseFloatyIfInvoked();
}

// Tests that DismissGeminiFromOtherWindows dismisses Gemini in other browsers.
TEST_F(GeminiBrowserAgentTest, TestDismissGeminiFromOtherWindows) {
  TestProfileIOS::Builder second_profile_builder;
  second_profile_builder.SetName("profile2");
  TestProfileIOS* second_profile =
      profile_manager_.AddProfileWithBuilder(std::move(second_profile_builder));

  // Emulate opening a new window.
  std::unique_ptr<TestBrowser> second_browser =
      std::make_unique<TestBrowser>(second_profile);
  BrowserList* browser_list = BrowserListFactory::GetForProfile(second_profile);
  browser_list->AddBrowser(second_browser.get());

  id mock_second_handler = OCMProtocolMock(@protocol(GeminiCommands));
  [second_browser->GetCommandDispatcher()
      startDispatchingToTarget:mock_second_handler
                   forProtocol:@protocol(GeminiCommands)];

  [[mock_second_handler expect]
      dismissGeminiFlowWithCompletion:[OCMArg checkWithBlock:^BOOL(
                                                  ProceduralBlock block) {
        if (block) {
          block();
        }
        return YES;
      }]];

  base::RunLoop run_loop;
  gemini_browser_agent_->DismissGeminiFromOtherWindows(run_loop.QuitClosure());
  run_loop.Run();
  [mock_second_handler verify];
  browser_list->RemoveBrowser(second_browser.get());
}

// Tests that the floaty is dismissed when the primary account changes.
TEST_F(GeminiBrowserAgentTest, TestDismissedOnPrimaryAccountChanged) {
  SetIsFloatyInvoked(true);

  signin::PrimaryAccountChangeEvent::State previous_state;
  CoreAccountInfo account_info;
  account_info.account_id = CoreAccountId::FromGaiaId(GaiaId("gaia_id"));
  account_info.gaia = GaiaId("gaia_id");
  account_info.email = "test@test.com";
  signin::PrimaryAccountChangeEvent::State current_state(
      account_info, signin::ConsentLevel::kSignin);

  signin::PrimaryAccountChangeEvent event(
      previous_state, current_state, signin_metrics::AccessPoint::kSettings);

  gemini_browser_agent_->OnPrimaryAccountChanged(event);

  EXPECT_FALSE(IsFloatyInvoked());
  EXPECT_TRUE(IsConversationIdPrefCleared());
}

// Tests that the floaty is dismissed even if it is temporarily hidden.
TEST_F(GeminiBrowserAgentTest, TestForceDismissedWhenTemporarilyHidden) {
  SetIsFloatyInvoked(true);
  SetIsFloatyTemporarilyHidden(true);

  signin::PrimaryAccountChangeEvent::State previous_state;
  CoreAccountInfo account_info;
  account_info.account_id = CoreAccountId::FromGaiaId(GaiaId("gaia_id"));
  account_info.gaia = GaiaId("gaia_id");
  account_info.email = "test@test.com";
  signin::PrimaryAccountChangeEvent::State current_state(
      account_info, signin::ConsentLevel::kSignin);

  signin::PrimaryAccountChangeEvent event(
      previous_state, current_state, signin_metrics::AccessPoint::kSettings);

  gemini_browser_agent_->OnPrimaryAccountChanged(event);

  EXPECT_FALSE(IsFloatyInvoked());
  EXPECT_FALSE(IsFloatyTemporarilyHidden());
  EXPECT_TRUE(IsConversationIdPrefCleared());
}

// Tests that when the floaty is expanded/focused while temporarily hidden,
// it becomes visible again, resetting the temporary hidden state.
TEST_F(GeminiBrowserAgentTest,
       TestFloatyVisibleWhenExpandedWhileTemporarilyHidden) {
  SetIsFloatyInvoked(true);
  SetIsFloatyTemporarilyHidden(true);

  EXPECT_TRUE(IsFloatyTemporarilyHidden());

  // Simulate view state changing to expanded.
  gemini_browser_agent_->OnViewStateChanged(
      ios::provider::GeminiViewState::kExpanded);

  EXPECT_FALSE(IsFloatyTemporarilyHidden());
}

// Tests that the view mode switches to text/floaty mode on backgrounding if the
// current mode is live and the page is eligible.
TEST_F(GeminiBrowserAgentTest, TestSwitchToTextModeOnBackgroundingIfLive) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({kGeminiLive}, {});

  SetIsFloatyInvoked(true);
  web_state_->SetCurrentURL(GURL("https://example.com"));
  web_state_->WasShown();

  // Set the current mode to Live.
  ios::provider::SwitchToMode(ios::provider::GeminiViewMode::kLive,
                              /*animated=*/false);
  EXPECT_EQ(ios::provider::GetCurrentMode(),
            ios::provider::GeminiViewMode::kLive);

  // Simulate app backgrounding via SceneState activation level callback.
  gemini_browser_agent_->OnSceneActivationLevelChanged(
      SceneActivationLevelBackground);

  // Verify it switched to Floaty (text mode).
  EXPECT_EQ(ios::provider::GetCurrentMode(),
            ios::provider::GeminiViewMode::kFloaty);
  EXPECT_TRUE(IsFloatyInvoked());
}

// Tests that the floaty is dismissed on backgrounding if the current mode is
// live but the page is ineligible.
TEST_F(GeminiBrowserAgentTest, TestDismissOnBackgroundingIfLiveAndIneligible) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({kGeminiLive}, {});

  SetIsFloatyInvoked(true);
  web_state_->SetCurrentURL(GURL("chrome://newtab/"));

  // Set the current mode to Live.
  ios::provider::SwitchToMode(ios::provider::GeminiViewMode::kLive,
                              /*animated=*/false);
  EXPECT_EQ(ios::provider::GetCurrentMode(),
            ios::provider::GeminiViewMode::kLive);

  // Simulate app backgrounding via SceneState activation level callback.
  gemini_browser_agent_->OnSceneActivationLevelChanged(
      SceneActivationLevelBackground);

  // Verify it became dismissed instead of switching to Floaty.
  EXPECT_FALSE(IsFloatyInvoked());
}

// Tests that the view mode does not change on backgrounding if it is not
// currently in live mode.
TEST_F(GeminiBrowserAgentTest, TestNoSwitchOnBackgroundingIfNotLive) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({kGeminiLive}, {});

  SetIsFloatyInvoked(true);

  // Set the current mode to Floaty (text mode).
  ios::provider::SwitchToMode(ios::provider::GeminiViewMode::kFloaty,
                              /*animated=*/false);
  EXPECT_EQ(ios::provider::GetCurrentMode(),
            ios::provider::GeminiViewMode::kFloaty);

  // Simulate app backgrounding via SceneState activation level callback.
  gemini_browser_agent_->OnSceneActivationLevelChanged(
      SceneActivationLevelBackground);

  // Verify it remained Floaty (text mode).
  EXPECT_EQ(ios::provider::GetCurrentMode(),
            ios::provider::GeminiViewMode::kFloaty);
}

// Tests that OnGeminiAvailabilityChanged is called when Gemini availability
// changes.
TEST_F(GeminiBrowserAgentTest, TestOnGeminiAvailabilityChanged) {
  TestGeminiObserver observer;
  gemini_browser_agent_->AddObserver(&observer);

  // Add active WebState with GeminiTabHelper.
  auto web_state = std::make_unique<web::FakeWebState>();
  web_state->SetBrowserState(profile_);
  GeminiTabHelper::CreateForWebState(web_state.get());
  WebViewProxyTabHelper::CreateForWebState(web_state.get());
  web_state->SetCurrentURL(GURL("https://example.com"));
  web_state->SetContentsMimeType("text/html");
  web_state->WasShown();

  // Insert and activate the web state to trigger active web state change.
  browser_->GetWebStateList()->InsertWebState(
      std::move(web_state),
      WebStateList::InsertionParams::Automatic().Activate(true));

  // The observer should have been notified of availability.
  EXPECT_TRUE(observer.available_);
  EXPECT_GE(observer.availability_call_count_, 1);

  // Navigate to an ineligible site.
  web_state_ = static_cast<web::FakeWebState*>(
      browser_->GetWebStateList()->GetActiveWebState());
  web_state_->SetCurrentURL(GURL("chrome://newtab/"));

  // Manually trigger page context updated to simulate navigation finishing.
  gemini_browser_agent_->OnPageContextUpdated(web_state_);

  EXPECT_FALSE(observer.available_);

  gemini_browser_agent_->RemoveObserver(&observer);
}

// Tests that kNoWebState is recorded when StartGeminiFlow is invoked with no
// active WebState.
TEST_F(GeminiBrowserAgentTest, TestStartGeminiFlowNoActiveWebState) {
  UIViewController* base_view_controller = [[UIViewController alloc] init];

  // Initialize browser agent on a browser with no active WebStates.
  std::unique_ptr<TestBrowser> empty_browser =
      std::make_unique<TestBrowser>(profile_);
  id mock_gemini_handler = OCMProtocolMock(@protocol(GeminiCommands));
  [empty_browser->GetCommandDispatcher()
      startDispatchingToTarget:mock_gemini_handler
                   forProtocol:@protocol(GeminiCommands)];
  GeminiBrowserAgent::CreateForBrowser(empty_browser.get());
  GeminiBrowserAgent* empty_agent =
      GeminiBrowserAgent::FromBrowser(empty_browser.get());

  base::HistogramTester histogram_tester;
  empty_agent->StartGeminiFlow(
      base_view_controller, [[GeminiStartupState alloc]
                                initWithEntryPoint:gemini::EntryPoint::Promo]);

  histogram_tester.ExpectUniqueSample(kGeminiInvocationPageTypeHistogram,
                                      IOSGeminiInvocationPageType::kNoWebState,
                                      1);
}

// Tests that OnLiveButtonTapped triggers the feature engagement event.
TEST_F(GeminiBrowserAgentTest, TestOnLiveButtonTappedTriggersEvent) {
  EXPECT_CALL(
      *mock_tracker_,
      NotifyEvent(testing::Eq(feature_engagement::events::kIOSGeminiLiveUsed)));

  gemini_browser_agent_->OnLiveButtonTapped();
}

// Tests that kIPHiOSGeminiLiveIPHFeature and kIPHiOSGeminiLiveNewBadgeFeature
// are successfully triggered when starting the flow and dismissed when the
// floaty is dismissed.
TEST_F(GeminiBrowserAgentTest, TestGeminiLiveIPHAndNewBadgeFET) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({kGeminiLive}, {});

  // Setup mock tracker expectations for triggering
  EXPECT_CALL(*mock_tracker_,
              ShouldTriggerHelpUI(testing::Ref(
                  feature_engagement::kIPHiOSGeminiLiveIPHFeature)))
      .WillOnce(testing::Return(true));
  EXPECT_CALL(*mock_tracker_,
              ShouldTriggerHelpUI(testing::Ref(
                  feature_engagement::kIPHiOSGeminiLiveNewBadgeFeature)))
      .WillOnce(testing::Return(true));

  UIViewController* base_view_controller = [[UIViewController alloc] init];
  GeminiStartupState* startup_state =
      [[GeminiStartupState alloc] initWithEntryPoint:gemini::EntryPoint::Promo];
  GeminiPageContext* page_context = [[GeminiPageContext alloc] init];

  // Call CreateGeminiConfiguration to trigger the features.
  GeminiConfiguration* config = CreateGeminiConfiguration(
      base_view_controller, startup_state, web_state_, page_context);

  EXPECT_TRUE(config.shouldShowGeminiLiveIPH);
  EXPECT_TRUE(config.shouldShowGeminiLiveNewBadge);

  // Setup mock tracker expectations for dismissal
  EXPECT_CALL(
      *mock_tracker_,
      Dismissed(testing::Ref(feature_engagement::kIPHiOSGeminiLiveIPHFeature)))
      .Times(1);
  EXPECT_CALL(*mock_tracker_,
              Dismissed(testing::Ref(
                  feature_engagement::kIPHiOSGeminiLiveNewBadgeFeature)))
      .Times(1);

  // Emulate the floaty being invoked so DismissFloaty actually runs fully.
  SetIsFloatyInvoked(true);

  gemini_browser_agent_->DismissFloaty();
}

// Tests that OnProcessingStatusChanged updates processing_status_ and switches
// to text mode when dormant.
TEST_F(GeminiBrowserAgentTest, TestOnProcessingStatusChangedLiveDormant) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({kGeminiLive}, {});

  // Register mock targets to satisfy commands used by
  // ShowLiveSessionDormantSnackbar/PrepareFloatyToBeShown.
  id mock_snackbar_handler = OCMProtocolMock(@protocol(SnackbarCommands));
  [browser_->GetCommandDispatcher()
      startDispatchingToTarget:mock_snackbar_handler
                   forProtocol:@protocol(SnackbarCommands)];
  id mock_fullscreen_handler = OCMProtocolMock(@protocol(FullscreenCommands));
  [browser_->GetCommandDispatcher()
      startDispatchingToTarget:mock_fullscreen_handler
                   forProtocol:@protocol(FullscreenCommands)];

  SetIsFloatyInvoked(true);
  web_state_->SetCurrentURL(GURL("https://example.com"));
  web_state_->WasShown();

  // Put in Live mode.
  ios::provider::SwitchToMode(ios::provider::GeminiViewMode::kLive,
                              /*animated=*/false);
  EXPECT_EQ(ios::provider::GetCurrentMode(),
            ios::provider::GeminiViewMode::kLive);

  // Change status to kDormant.
  gemini_browser_agent_->OnProcessingStatusChanged(
      ios::provider::GeminiClientMode::kDormant,
      ios::provider::GeminiDormantReason::kUnknown);

  // Should switch back to Floaty (text mode) and update the internal status.
  EXPECT_EQ(ios::provider::GetCurrentMode(),
            ios::provider::GeminiViewMode::kFloaty);
  EXPECT_EQ(GetProcessingStatus(), ios::provider::GeminiClientMode::kDormant);
}

// Tests that OnProcessingStatusChanged handles low volume dormant reason
// with no snackbar when the dormant reasons feature is enabled.
TEST_F(GeminiBrowserAgentTest,
       TestOnProcessingStatusChangedLiveDormantLowVolume) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({kGeminiLive, kGeminiLiveDormantReasons},
                                       {});

  id mock_snackbar_handler = OCMProtocolMock(@protocol(SnackbarCommands));
  [browser_->GetCommandDispatcher()
      startDispatchingToTarget:mock_snackbar_handler
                   forProtocol:@protocol(SnackbarCommands)];
  id mock_fullscreen_handler = OCMProtocolMock(@protocol(FullscreenCommands));
  [browser_->GetCommandDispatcher()
      startDispatchingToTarget:mock_fullscreen_handler
                   forProtocol:@protocol(FullscreenCommands)];

  SetIsFloatyInvoked(true);
  web_state_->SetCurrentURL(GURL("https://example.com"));
  web_state_->WasShown();

  // Put in Live mode.
  ios::provider::SwitchToMode(ios::provider::GeminiViewMode::kLive,
                              /*animated=*/false);
  EXPECT_EQ(ios::provider::GetCurrentMode(),
            ios::provider::GeminiViewMode::kLive);

  // For low volume dormant reasons, we expect switch to Floaty, and NO snackbar
  // shown.
  OCMReject([mock_snackbar_handler showSnackbarMessage:[OCMArg any]
                                          bottomOffset:0.0])
      .ignoringNonObjectArgs();
  OCMReject([mock_snackbar_handler showSnackbarMessage:[OCMArg any]]);

  gemini_browser_agent_->OnProcessingStatusChanged(
      ios::provider::GeminiClientMode::kDormant,
      ios::provider::GeminiDormantReason::kLowVolumeInForeground);

  // Should switch back to Floaty (text mode) and update the internal status.
  EXPECT_EQ(ios::provider::GetCurrentMode(),
            ios::provider::GeminiViewMode::kFloaty);
  EXPECT_EQ(GetProcessingStatus(), ios::provider::GeminiClientMode::kDormant);

  EXPECT_OCMOCK_VERIFY(mock_snackbar_handler);
}

// Tests that OnProcessingStatusChanged handles inactivity timeout dormant
// reason with the correct continue session snackbar when the feature is
// enabled.
TEST_F(GeminiBrowserAgentTest,
       TestOnProcessingStatusChangedLiveDormantTimeout) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({kGeminiLive, kGeminiLiveDormantReasons},
                                       {});

  id mock_snackbar_handler = OCMProtocolMock(@protocol(SnackbarCommands));
  [browser_->GetCommandDispatcher()
      startDispatchingToTarget:mock_snackbar_handler
                   forProtocol:@protocol(SnackbarCommands)];
  id mock_fullscreen_handler = OCMProtocolMock(@protocol(FullscreenCommands));
  [browser_->GetCommandDispatcher()
      startDispatchingToTarget:mock_fullscreen_handler
                   forProtocol:@protocol(FullscreenCommands)];

  SetIsFloatyInvoked(true);
  web_state_->SetCurrentURL(GURL("https://example.com"));
  web_state_->WasShown();

  // Put in Live mode.
  ios::provider::SwitchToMode(ios::provider::GeminiViewMode::kLive,
                              /*animated=*/false);
  EXPECT_EQ(ios::provider::GetCurrentMode(),
            ios::provider::GeminiViewMode::kLive);

  // Verify the correct timeout snackbar is shown.
  OCMExpect([mock_snackbar_handler
                showSnackbarMessage:[OCMArg checkWithBlock:^BOOL(id obj) {
                  SnackbarMessage* message = (SnackbarMessage*)obj;
                  NSString* expected_title = l10n_util::GetNSString(
                      IDS_IOS_GEMINI_LIVE_CONTINUE_SESSION_SNACKBAR);
                  return [message.title isEqualToString:expected_title];
                }]
                       bottomOffset:0.0])
      .ignoringNonObjectArgs();

  gemini_browser_agent_->OnProcessingStatusChanged(
      ios::provider::GeminiClientMode::kDormant,
      ios::provider::GeminiDormantReason::kInactivityTimeout);

  EXPECT_EQ(ios::provider::GetCurrentMode(),
            ios::provider::GeminiViewMode::kFloaty);
  EXPECT_EQ(GetProcessingStatus(), ios::provider::GeminiClientMode::kDormant);

  EXPECT_OCMOCK_VERIFY(mock_snackbar_handler);
}

// Tests that OnProcessingStatusChanged handles server pause dormant reason
// with the correct server pause snackbar when the feature is enabled.
TEST_F(GeminiBrowserAgentTest,
       TestOnProcessingStatusChangedLiveDormantServerPause) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({kGeminiLive, kGeminiLiveDormantReasons},
                                       {});

  id mock_snackbar_handler = OCMProtocolMock(@protocol(SnackbarCommands));
  [browser_->GetCommandDispatcher()
      startDispatchingToTarget:mock_snackbar_handler
                   forProtocol:@protocol(SnackbarCommands)];
  id mock_fullscreen_handler = OCMProtocolMock(@protocol(FullscreenCommands));
  [browser_->GetCommandDispatcher()
      startDispatchingToTarget:mock_fullscreen_handler
                   forProtocol:@protocol(FullscreenCommands)];

  SetIsFloatyInvoked(true);
  web_state_->SetCurrentURL(GURL("https://example.com"));
  web_state_->WasShown();

  // Put in Live mode.
  ios::provider::SwitchToMode(ios::provider::GeminiViewMode::kLive,
                              /*animated=*/false);
  EXPECT_EQ(ios::provider::GetCurrentMode(),
            ios::provider::GeminiViewMode::kLive);

  // Verify the correct server pause snackbar is shown.
  OCMExpect([mock_snackbar_handler
                showSnackbarMessage:[OCMArg checkWithBlock:^BOOL(id obj) {
                  SnackbarMessage* message = (SnackbarMessage*)obj;
                  NSString* expected_title = l10n_util::GetNSString(
                      IDS_IOS_GEMINI_LIVE_SERVER_PAUSE_SNACKBAR);
                  return [message.title isEqualToString:expected_title];
                }]
                       bottomOffset:0.0])
      .ignoringNonObjectArgs();

  gemini_browser_agent_->OnProcessingStatusChanged(
      ios::provider::GeminiClientMode::kDormant,
      ios::provider::GeminiDormantReason::kServerPause);

  EXPECT_EQ(ios::provider::GetCurrentMode(),
            ios::provider::GeminiViewMode::kFloaty);
  EXPECT_EQ(GetProcessingStatus(), ios::provider::GeminiClientMode::kDormant);

  EXPECT_OCMOCK_VERIFY(mock_snackbar_handler);
}

// Tests that OnGeminiLiveUserDidBargeIn updates processing_status_ to
// kTranscribing.
TEST_F(GeminiBrowserAgentTest, TestOnGeminiLiveUserDidBargeIn) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({kGeminiLive}, {});

  SetIsFloatyInvoked(true);

  // Put in Live mode.
  ios::provider::SwitchToMode(ios::provider::GeminiViewMode::kLive,
                              /*animated=*/false);

  // Trigger barge-in.
  gemini_browser_agent_->OnGeminiLiveUserDidBargeIn();

  // Status should be set to kTranscribing.
  EXPECT_EQ(GetProcessingStatus(),
            ios::provider::GeminiClientMode::kTranscribing);
}

// Tests that fullscreen remains disabled while floaty is invoked, until
// floaty is dismissed.
TEST_F(GeminiBrowserAgentTest,
       TestFloatyKeepsFullscreenDisabledUntilDismissed) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {kChromeNextIa, kAppBarHideInFullscreen, kComposeboxIpad}, {});

  FullscreenController* controller =
      FullscreenController::FromBrowser(browser_.get());
  ASSERT_NE(controller, nullptr);
  EXPECT_TRUE(controller->IsEnabled());

  InvokeFloaty([[GeminiConfiguration alloc] init]);
  EXPECT_FALSE(controller->IsEnabled());

  // Fullscreen should remain disabled even when UI appears or state collapses.
  gemini_browser_agent_->OnGeminiUIDidAppear();
  EXPECT_FALSE(controller->IsEnabled());

  gemini_browser_agent_->OnViewStateChanged(
      ios::provider::GeminiViewState::kCollapsed);
  EXPECT_FALSE(controller->IsEnabled());

  // Fullscreen should be re-enabled once floaty is dismissed.
  gemini_browser_agent_->DismissFloaty();
  EXPECT_TRUE(controller->IsEnabled());

  // Fullscreen should be disabled once the state transitions to expanded again.
  gemini_browser_agent_->OnViewStateChanged(
      ios::provider::GeminiViewState::kExpanded);
  EXPECT_FALSE(controller->IsEnabled());

  // Fullscreen should be re-enabled once the state transitions to hidden.
  gemini_browser_agent_->OnViewStateChanged(
      ios::provider::GeminiViewState::kHidden);
  EXPECT_TRUE(controller->IsEnabled());
}

// Tests that when the floaty is un-minimized (expanded), we check if the
// selected tabs should be persisted based on whether the active tab is in the
// selection.
TEST_F(GeminiBrowserAgentTest, TestPersistSelectedTabsOnUnMinimize) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kGeminiMultiTabContext);
  ios::provider::ResetGemini();
  SetIsFloatyInvoked(true);

  web::WebStateID active_id = web_state_->GetUniqueIdentifier();

  std::unique_ptr<web::FakeWebState> other_web_state =
      std::make_unique<web::FakeWebState>();
  other_web_state->SetBrowserState(profile_);
  GeminiTabHelper::CreateForWebState(other_web_state.get());
  WebViewProxyTabHelper::CreateForWebState(other_web_state.get());
  web::WebStateID other_id = other_web_state->GetUniqueIdentifier();
  browser_->GetWebStateList()->InsertWebState(
      std::move(other_web_state),
      WebStateList::InsertionParams::Automatic().Activate(false));

  std::unique_ptr<web::FakeWebState> non_selected_web_state =
      std::make_unique<web::FakeWebState>();
  non_selected_web_state->SetBrowserState(profile_);
  GeminiTabHelper::CreateForWebState(non_selected_web_state.get());
  WebViewProxyTabHelper::CreateForWebState(non_selected_web_state.get());
  browser_->GetWebStateList()->InsertWebState(
      std::move(non_selected_web_state),
      WebStateList::InsertionParams::Automatic().Activate(false));

  // Set the initial selection: active tab and another tab.
  EXPECT_TRUE(active_id.valid());
  EXPECT_TRUE(other_id.valid());
  EXPECT_NE(active_id, other_id);

  GeminiPageContext* active_context = [[GeminiPageContext alloc] init];
  active_context.geminiPageContextAttachmentState =
      ios::provider::GeminiPageContextAttachmentState::kAttached;
  SetRawAttachedTab(active_id, active_context);

  gemini_browser_agent_->OnTabPickerSelectionChanged({active_id, other_id});
  EXPECT_EQ(GetRawAttachedTabs().size(), 2u);
  // GetSelectedWebStateIDs() may return size 1 in downstream unit tests if
  // GCRGemini provider is uninitialized/nil, so we assert on raw selected IDs.

  // Verify that WebViewProxyTabHelper is attached to the non-selected web
  // state.
  web::WebState* raw_non_selected_web_state =
      browser_->GetWebStateList()->GetWebStateAt(2);
  ASSERT_NE(raw_non_selected_web_state, nullptr);
  EXPECT_NE(WebViewProxyTabHelper::FromWebState(raw_non_selected_web_state),
            nullptr);

  // Simulate minimizing the floaty.
  gemini_browser_agent_->OnViewStateChanged(
      ios::provider::GeminiViewState::kCollapsed);
  gemini_browser_agent_->SetLastShownViewState(
      ios::provider::GeminiViewState::kCollapsed);

  // Switch the active tab to the non-selected tab (index 2).
  browser_->GetWebStateList()->ActivateWebStateAt(2);
  web::WebStateID new_active_id =
      raw_non_selected_web_state->GetUniqueIdentifier();

  // Verify that attached tabs now contains only the new active tab.
  auto raw_tabs = GetRawAttachedTabs();
  EXPECT_EQ(raw_tabs.size(), 1u);
  EXPECT_TRUE(raw_tabs.count(new_active_id));

  // Simulate un-minimizing the floaty.
  gemini_browser_agent_->OnViewStateChanged(
      ios::provider::GeminiViewState::kExpanded);
  gemini_browser_agent_->SetLastShownViewState(
      ios::provider::GeminiViewState::kExpanded);

  // Verify that attached tabs now remains only the new active tab.
  EXPECT_EQ(raw_tabs.size(), 1u);
  EXPECT_TRUE(raw_tabs.count(new_active_id));
}

// Tests that switching from live to floaty mode on an eligible page keeps the
// floaty.
TEST_F(GeminiBrowserAgentTest, TestSwitchFromLiveToChatEligible) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({kGeminiLive, kGeminiLiveDormantReasons},
                                       {});

  id mock_snackbar_handler = OCMProtocolMock(@protocol(SnackbarCommands));
  [browser_->GetCommandDispatcher()
      startDispatchingToTarget:mock_snackbar_handler
                   forProtocol:@protocol(SnackbarCommands)];
  id mock_fullscreen_handler = OCMProtocolMock(@protocol(FullscreenCommands));
  [browser_->GetCommandDispatcher()
      startDispatchingToTarget:mock_fullscreen_handler
                   forProtocol:@protocol(FullscreenCommands)];

  SetIsFloatyInvoked(true);
  web_state_->SetCurrentURL(GURL("https://example.com"));
  web_state_->WasShown();

  // Switch to Live mode.
  ios::provider::SwitchToMode(ios::provider::GeminiViewMode::kLive,
                              /*animated=*/false);

  // Expect snackbar with bottom offset.
  OCMExpect([mock_snackbar_handler
                showSnackbarMessage:[OCMArg checkWithBlock:^BOOL(id obj) {
                  SnackbarMessage* message = (SnackbarMessage*)obj;
                  NSString* expected_title = l10n_util::GetNSString(
                      IDS_IOS_GEMINI_LIVE_CONTINUE_SESSION_SNACKBAR);
                  return [message.title isEqualToString:expected_title];
                }]
                       bottomOffset:0.0])
      .ignoringNonObjectArgs();

  // Trigger switching to floaty (kDormant status).
  gemini_browser_agent_->OnProcessingStatusChanged(
      ios::provider::GeminiClientMode::kDormant,
      ios::provider::GeminiDormantReason::kInactivityTimeout);

  // The floaty should not be dismissed, meaning it is still invoked.
  EXPECT_TRUE(IsFloatyInvoked());
  EXPECT_OCMOCK_VERIFY(mock_snackbar_handler);
}

// Tests that switching from live to floaty mode on an ineligible page dismisses
// the floaty.
TEST_F(GeminiBrowserAgentTest, TestSwitchFromLiveToChatIneligible) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({kGeminiLive, kGeminiLiveDormantReasons},
                                       {});

  id mock_snackbar_handler = OCMProtocolMock(@protocol(SnackbarCommands));
  [browser_->GetCommandDispatcher()
      startDispatchingToTarget:mock_snackbar_handler
                   forProtocol:@protocol(SnackbarCommands)];

  SetIsFloatyInvoked(true);
  web_state_->SetCurrentURL(GURL("chrome://newtab/"));

  // Switch to Live mode.
  ios::provider::SwitchToMode(ios::provider::GeminiViewMode::kLive,
                              /*animated=*/false);

  // Expect snackbar.
  OCMExpect([mock_snackbar_handler
                showSnackbarMessage:[OCMArg checkWithBlock:^BOOL(id obj) {
                  SnackbarMessage* message = (SnackbarMessage*)obj;
                  NSString* expected_title = l10n_util::GetNSString(
                      IDS_IOS_GEMINI_LIVE_CONTINUE_SESSION_SNACKBAR);
                  return [message.title isEqualToString:expected_title];
                }]
                       bottomOffset:0.0])
      .ignoringNonObjectArgs();

  // Trigger switching to floaty (kDormant status).
  gemini_browser_agent_->OnProcessingStatusChanged(
      ios::provider::GeminiClientMode::kDormant,
      ios::provider::GeminiDormantReason::kInactivityTimeout);

  // The floaty should be dismissed, meaning it is no longer invoked.
  EXPECT_FALSE(IsFloatyInvoked());
  EXPECT_OCMOCK_VERIFY(mock_snackbar_handler);
}

// Tests that DetachTabWithID gracefully handles an invalid tab ID string.
TEST_F(GeminiBrowserAgentTest, TestDetachInvalidTabId) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({kGeminiMultiTabContext, kPageActionMenu}, {});

  web::WebStateID active_id = web_state_->GetUniqueIdentifier();

  GeminiPageContext* active_context = [[GeminiPageContext alloc] init];
  active_context.geminiPageContextAttachmentState =
      ios::provider::GeminiPageContextAttachmentState::kAttached;
  SetRawAttachedTab(active_id, active_context);

  gemini_browser_agent_->OnTabPickerSelectionChanged({active_id});
  size_t initial_size = GetRawAttachedTabs().size();

  DetachTabWithID(@"invalid_id");

  // The map size should be unchanged.
  EXPECT_EQ(initial_size, GetRawAttachedTabs().size());
}

// Tests that DetachTabWithID gracefully early-exits when there is no active
// web state.
TEST_F(GeminiBrowserAgentTest, TestDetachTabWithoutActiveWebState) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({kGeminiMultiTabContext, kPageActionMenu}, {});

  // Clear raw_ptrs to prevent DanglingPtr crashes during TearDown when the
  // WebState (and its associated frames/helpers) is destroyed.
  web_state_ = nullptr;
  fake_main_frame_ = nullptr;
  gemini_tab_helper_ = nullptr;

  // Close the active web state so that GetActiveWebState() returns nullptr.
  browser_->GetWebStateList()->CloseWebStateAt(
      0, WebStateList::ClosingReason::kDefault);
  ASSERT_EQ(nullptr, browser_->GetWebStateList()->GetActiveWebState());

  DetachTabWithID(@"123");

  // Should not crash.
  EXPECT_EQ(0u, GetRawAttachedTabs().size());
}

// Tests that DetachTabWithID updates the attachment state of the active tab
// without removing it.
TEST_F(GeminiBrowserAgentTest, TestDetachActiveTab) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({kGeminiMultiTabContext, kPageActionMenu}, {});

  web::WebStateID active_id = web_state_->GetUniqueIdentifier();

  GeminiPageContext* mock_context = [[GeminiPageContext alloc] init];
  mock_context.geminiPageContextAttachmentState =
      ios::provider::GeminiPageContextAttachmentState::kAttached;
  SetRawAttachedTab(active_id, mock_context);

  gemini_browser_agent_->OnTabPickerSelectionChanged({active_id});

  // Verify it starts as attached.
  auto tabs = GetRawAttachedTabs();
  ASSERT_EQ(1u, tabs.size());
  ASSERT_EQ(ios::provider::GeminiPageContextAttachmentState::kAttached,
            tabs[active_id].geminiPageContextAttachmentState);

  NSString* tab_id_str =
      [NSString stringWithFormat:@"%d", active_id.identifier()];
  DetachTabWithID(tab_id_str);

  // Verify it is still in the map but detached.
  tabs = GetRawAttachedTabs();
  EXPECT_EQ(1u, tabs.size());
  EXPECT_EQ(ios::provider::GeminiPageContextAttachmentState::kDetached,
            tabs[active_id].geminiPageContextAttachmentState);
}

// Tests that DetachTabWithID completely removes a shared tab from the cache.
TEST_F(GeminiBrowserAgentTest, TestDetachSharedTab) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({kGeminiMultiTabContext, kPageActionMenu}, {});

  web::WebStateID active_id = web_state_->GetUniqueIdentifier();

  std::unique_ptr<web::FakeWebState> other_web_state =
      std::make_unique<web::FakeWebState>();
  other_web_state->SetBrowserState(profile_);
  GeminiTabHelper::CreateForWebState(other_web_state.get());
  WebViewProxyTabHelper::CreateForWebState(other_web_state.get());
  web::WebStateID other_id = other_web_state->GetUniqueIdentifier();
  browser_->GetWebStateList()->InsertWebState(
      std::move(other_web_state),
      WebStateList::InsertionParams::Automatic().Activate(false));

  GeminiPageContext* active_context = [[GeminiPageContext alloc] init];
  active_context.geminiPageContextAttachmentState =
      ios::provider::GeminiPageContextAttachmentState::kAttached;
  SetRawAttachedTab(active_id, active_context);

  gemini_browser_agent_->OnTabPickerSelectionChanged({active_id, other_id});

  auto tabs = GetRawAttachedTabs();
  ASSERT_EQ(2u, tabs.size());

  NSString* other_tab_id_str =
      [NSString stringWithFormat:@"%d", other_id.identifier()];
  DetachTabWithID(other_tab_id_str);

  // Verify the shared tab is completely removed.
  tabs = GetRawAttachedTabs();
  EXPECT_EQ(1u, tabs.size());
  EXPECT_EQ(0u, tabs.count(other_id));
}
