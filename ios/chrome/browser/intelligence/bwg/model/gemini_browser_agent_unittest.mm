// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/model/gemini_browser_agent.h"

#import "base/run_loop.h"
#import "base/strings/utf_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "base/test/run_until.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "components/favicon/core/favicon_service.h"
#import "components/favicon/ios/web_favicon_driver.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/optimization_guide/proto/features/common_quality_data.pb.h"
#import "components/signin/public/base/consent_level.h"
#import "components/signin/public/identity_manager/primary_account_change_event.h"
#import "ios/chrome/browser/favicon/model/favicon_service_factory.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_configuration.h"
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
#import "ios/chrome/browser/shared/public/commands/bwg_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/chrome/browser/snapshots/model/fake_snapshot_generator_delegate.h"
#import "ios/chrome/browser/snapshots/model/snapshot_source_tab_helper.h"
#import "ios/chrome/browser/snapshots/model/snapshot_tab_helper.h"
#import "ios/chrome/browser/web/model/web_view_proxy/web_view_proxy_tab_helper.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/find_in_page/find_in_page_java_script_feature.h"
#import "ios/web/js_messaging/java_script_feature_manager.h"
#import "ios/web/public/js_messaging/java_script_feature_util.h"
#import "ios/web/public/test/fakes/fake_web_client.h"
#import "ios/web/public/test/fakes/fake_web_frame.h"
#import "ios/web/public/test/fakes/fake_web_frames_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/js_test_util.h"
#import "ios/web/public/test/scoped_testing_web_client.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

// Test fixture for GeminiBrowserAgent.
class GeminiBrowserAgentTest : public PlatformTest {
 protected:
  GeminiBrowserAgentTest()
      : web_client_(std::make_unique<web::FakeWebClient>()),
        task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    feature_list_.InitWithFeatures(
        {kPageActionMenu, kPageContextExtractorRefactored, kGeminiCopresence},
        {});
    static_cast<web::FakeWebClient*>(web_client_.Get())
        ->SetJavaScriptFeatures(
            {web::FindInPageJavaScriptFeature::GetInstance(),
             PageContextExtractorJavaScriptFeature::GetInstance()});
    TestProfileIOS::Builder profile_builder;
    profile_builder.AddTestingFactory(
        OptimizationGuideServiceFactory::GetInstance(),
        OptimizationGuideServiceFactory::GetDefaultFactory());
    profile_ =
        profile_manager_.AddProfileWithBuilder(std::move(profile_builder));
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
    mock_bwg_handler_ = OCMProtocolMock(@protocol(BWGCommands));
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:mock_bwg_handler_
                     forProtocol:@protocol(BWGCommands)];

    std::unique_ptr<web::FakeWebState> web_state =
        std::make_unique<web::FakeWebState>();
    web_state_ = web_state.get();
    web_state->SetBrowserState(profile_);
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
    mock_bwg_handler_ = nullptr;
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

  // Clear `active_hiding_sources_`.
  void ClearActiveHidingSources() {
    gemini_browser_agent_->active_hiding_sources_.clear();
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

  // Triggers `UpdateGeminiPageContext()` in the browser agent.
  void UpdateGeminiPageContext() {
    gemini_browser_agent_->UpdateGeminiPageContext();
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
  id mock_bwg_handler_;
  FakeSnapshotGeneratorDelegate* fake_snapshot_delegate_;
};

// A test observer for GeminiBrowserAgent.
class TestGeminiObserver : public GeminiBrowserAgent::Observer {
 public:
  void OnFloatyInvokedChanged(bool is_invoked) override {
    is_invoked_ = is_invoked;
    call_count_++;
  }
  bool is_invoked_ = false;
  int call_count_ = 0;
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

  gemini_browser_agent_->StartGeminiFlow(
      base_view_controller, [[GeminiStartupState alloc]
                                initWithEntryPoint:gemini::EntryPoint::Promo]);

  // Wait for the delegate method to be called.
  ASSERT_TRUE(
      base::test::RunUntil([delegate_called]() { return *delegate_called; }));

  [mock_delegate verify];
}

// Tests that switching active web states handles observations correctly.
TEST_F(GeminiBrowserAgentTest, TestActiveWebStateChanged) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kGeminiCopresence);

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

// Tests that UpdateGeminiPageContext triggers page context generation.
TEST_F(GeminiBrowserAgentTest, TestUpdateGeminiPageContext) {
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

  UpdateGeminiPageContext();

  // Wait for the delegate method to be called.
  ASSERT_TRUE(
      base::test::RunUntil([delegate_called]() { return *delegate_called; }));

  [mock_delegate verify];
}

TEST_F(GeminiBrowserAgentTest, TestPageContextGenerationTimeout) {
  base::test::ScopedFeatureList scoped_feature_list;
  // Simulate FRE completion.
  profile_->GetPrefs()->SetBoolean(prefs::kIOSBwgConsent, true);

  UIViewController* base_view_controller = [[UIViewController alloc] init];
  web_state_->SetCurrentURL(GURL("https://example.com"));
  web_state_->SetContentsMimeType("text/html");
  web_state_->SetLoading(true);

  // Add a fake JS result for page context extraction.
  base::DictValue result;
  result.Set("currentNodeInnerText", "Example Text");
  fake_main_frame_->AddJsResultForFunctionCall(
      new base::Value(std::move(result)),
      "pageContextExtractor.extractPageContext");

  // Create a protocol mock to intercept the delegate call.
  id mock_delegate = OCMProtocolMock(@protocol(SnapshotGeneratorDelegate));
  SnapshotTabHelper::FromWebState(web_state_)->SetDelegate(mock_delegate);
  // Stub the canTakeSnapshot method to return YES.
  OCMStub([mock_delegate canTakeSnapshotWithWebStateInfo:[OCMArg any]])
      .andReturn(YES);

  gemini_browser_agent_->StartGeminiFlow(
      base_view_controller, [[GeminiStartupState alloc]
                                initWithEntryPoint:gemini::EntryPoint::Promo]);

  // At this point, the page is loading and we are waiting for context.
  // The timer should be running. Verify that JS has NOT been called yet.
  EXPECT_EQ(0ul, fake_main_frame_->GetJavaScriptCallHistory().size());

  // Fast forward by the timeout duration (3 seconds) + epsilon.
  task_environment_.FastForwardBy(base::Seconds(3) + base::Milliseconds(100));

  // Verify that the page context extraction was forced (JS called).
  // We check if "extractPageContext" was called.
  const auto& call_history = fake_main_frame_->GetJavaScriptCallHistory();
  ASSERT_GT(call_history.size(), 0ul);
  bool found_context_extraction = false;
  for (const auto& call : call_history) {
    if (base::UTF16ToUTF8(call).find("extractPageContext") !=
        std::string::npos) {
      found_context_extraction = true;
      break;
    }
  }
  EXPECT_TRUE(found_context_extraction);
  fake_main_frame_->ClearJavaScriptCallHistory();

  // Now simulate the page finishing loading.
  web_state_->SetLoading(false);
  web_state_->OnPageLoaded(web::PageLoadCompletionStatus::SUCCESS);

  // Verify that the page context extraction was triggered AGAIN (JS called).
  const auto& new_call_history = fake_main_frame_->GetJavaScriptCallHistory();
  ASSERT_GT(new_call_history.size(), 0ul);
  bool found_context_extraction_again = false;
  for (const auto& call : new_call_history) {
    if (base::UTF16ToUTF8(call).find("extractPageContext") !=
        std::string::npos) {
      found_context_extraction_again = true;
      break;
    }
  }
  EXPECT_TRUE(found_context_extraction_again);
  task_environment_.RunUntilIdle();  // IN-TEST
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

// Tests that the floaty remains hidden if the keyboard dismisses but a view
// controller is still presenting.
TEST_F(GeminiBrowserAgentTest,
       TestFloatyRemainsHiddenWhenKeyboardDismissedIfViewPresent) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      kGeminiCopresence, {{kGeminiCopresenceTrackSources, "true"}});
  SetIsFloatyInvoked(true);
  gemini_browser_agent_->HideFloatyIfInvoked(
      /*animated=*/true, /*source=*/gemini::FloatyUpdateSource::ViewTransition);
  gemini_browser_agent_->HideFloatyIfInvoked(
      /*animated=*/true, /*source=*/gemini::FloatyUpdateSource::Keyboard);
  gemini_browser_agent_->SetLastShownViewState(
      ios::provider::GeminiViewState::kExpanded);

  // Emulate a user typing for some time.
  SetFloatyHiddenTimestamp(base::TimeTicks::Now() - base::Seconds(5));

  // Emulate keyboard dismissing.
  gemini_browser_agent_->ShowFloatyIfInvoked(
      /*animated=*/true, /*source=*/gemini::FloatyUpdateSource::Keyboard);

  // The floaty should still be considered temporarily hidden.
  EXPECT_TRUE(IsFloatyTemporarilyHidden());

  // Emulate view controller dismissing.
  gemini_browser_agent_->ShowFloatyIfInvoked(
      /*animated=*/true, /*source=*/gemini::FloatyUpdateSource::ViewTransition);

  // The floaty should now be shown.
  EXPECT_FALSE(IsFloatyTemporarilyHidden());
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
  ClearActiveHidingSources();
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

  id mock_second_handler = OCMProtocolMock(@protocol(BWGCommands));
  [second_browser->GetCommandDispatcher()
      startDispatchingToTarget:mock_second_handler
                   forProtocol:@protocol(BWGCommands)];

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
  ClearActiveHidingSources();

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
// current mode is live.
TEST_F(GeminiBrowserAgentTest, TestSwitchToTextModeOnBackgroundingIfLive) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({kGeminiLive}, {});

  SetIsFloatyInvoked(true);

  // Set the current mode to Live.
  ios::provider::SwitchToMode(ios::provider::GeminiViewMode::kLive,
                              /*animated=*/false);
  EXPECT_EQ(ios::provider::GetCurrentMode(),
            ios::provider::GeminiViewMode::kLive);

  // Simulate app backgrounding via SceneState activation level.
  browser_->GetSceneState().activationLevel = SceneActivationLevelBackground;

  // Verify it switched to Floaty (text mode).
  EXPECT_EQ(ios::provider::GetCurrentMode(),
            ios::provider::GeminiViewMode::kFloaty);
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

  // Simulate app backgrounding via SceneState activation level.
  browser_->GetSceneState().activationLevel = SceneActivationLevelBackground;

  // Verify it remained Floaty (text mode).
  EXPECT_EQ(ios::provider::GetCurrentMode(),
            ios::provider::GeminiViewMode::kFloaty);
}
