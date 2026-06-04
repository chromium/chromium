// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/cobrowse/coordinator/assistant_aim_mediator.h"

#import "base/memory/raw_ptr.h"
#import "base/test/scoped_feature_list.h"
#import "components/contextual_tasks/public/features.h"
#import "ios/chrome/browser/assistant/coordinator/assistant_container_commands.h"
#import "ios/chrome/browser/assistant/ui/assistant_container_detent.h"
#import "ios/chrome/browser/cobrowse/model/aim_cobrowse_java_script_feature.h"
#import "ios/chrome/browser/cobrowse/model/assistant_aim_tab_helper.h"
#import "ios/chrome/browser/cobrowse/ui/assistant_aim_ui_constants.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/public/commands/scene_commands.h"
#import "ios/chrome/browser/url_loading/model/fake_url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_notifier_browser_agent.h"
#import "ios/chrome/browser/web/model/chrome_web_client.h"
#import "ios/web/public/navigation/web_state_policy_decider_bridge.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/fakes/fake_web_frame.h"
#import "ios/web/public/test/fakes/fake_web_frames_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/js_test_util.h"
#import "ios/web/public/test/scoped_testing_web_client.h"
#import "ios/web/public/test/web_task_environment.h"
#import "ios/web/public/web_state_delegate_bridge.h"
#import "net/base/apple/url_conversions.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/lens_server_proto/aim_communication.pb.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "ui/base/page_transition_types.h"
#import "url/gurl.h"

class AssistantAIMMediatorTest : public PlatformTest {
 protected:
  AssistantAIMMediatorTest()
      : web_client_(std::make_unique<ChromeWebClient>()) {
    scoped_feature_list_.InitAndEnableFeature(
        contextual_tasks::kContextualTasks);
    TestProfileIOS::Builder builder;
    profile_ = std::move(builder).Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());
    UrlLoadingNotifierBrowserAgent::CreateForBrowser(browser_.get());
    FakeUrlLoadingBrowserAgent::InjectForBrowser(browser_.get());
    url_loader_ = FakeUrlLoadingBrowserAgent::FromUrlLoadingBrowserAgent(
        UrlLoadingBrowserAgent::FromBrowser(browser_.get()));
  }

  void SetUp() override {
    PlatformTest::SetUp();
    auto fake_web_state = std::make_unique<web::FakeWebState>();
    fake_web_state_ = fake_web_state.get();
    fake_web_state->SetBrowserState(profile_.get());
    auto manager = std::make_unique<web::FakeWebFramesManager>();
    fake_web_state->SetWebFramesManager(web::ContentWorld::kPageContentWorld,
                                        std::move(manager));
    AssistantAimTabHelper::CreateForWebState(fake_web_state.get());
    web::test::OverrideJavaScriptFeatures(
        profile_.get(), {AimCobrowseJavaScriptFeature::GetInstance()});

    mock_container_handler_ =
        OCMProtocolMock(@protocol(AssistantContainerCommands));
    mock_scene_handler_ = OCMProtocolMock(@protocol(SceneCommands));

    mediator_ =
        [[AssistantAIMMediator alloc] initWithWebState:std::move(fake_web_state)
                                               context:nullptr
                                      containerHandler:mock_container_handler_
                                contextualTasksService:nullptr
                                             URLLoader:url_loader_];
    mediator_.sceneHandler = mock_scene_handler_;

    mock_delegate_ = OCMProtocolMock(@protocol(AssistantAIMMediatorDelegate));
    mediator_.delegate = mock_delegate_;
  }

  void TearDown() override {
    fake_web_state_ = nullptr;
    [mediator_ disconnect];
    mediator_ = nil;
    PlatformTest::TearDown();
  }

  web::WebTaskEnvironment task_environment_{
      web::WebTaskEnvironment::TimeSource::MOCK_TIME};
  web::ScopedTestingWebClient web_client_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  raw_ptr<FakeUrlLoadingBrowserAgent> url_loader_;
  raw_ptr<web::FakeWebState> fake_web_state_ = nullptr;
  AssistantAIMMediator* mediator_;
  id mock_delegate_;
  id mock_container_handler_;
  id mock_scene_handler_;
};

// Tests that prepareLoadForQueryText correctly aborts if the message is empty.
TEST_F(AssistantAIMMediatorTest, AbortsWhenMessageEmpty) {
  // Empty message (size 0).
  lens::ClientToAimMessage message;

  [[mock_delegate_ reject] assistantAIMMediatorDidLoadQuery:[OCMArg any]];

  [mediator_ prepareLoadWithClientToAimMessage:message];

  [mock_delegate_ verify];
}

// Tests that prepareLoadForQueryText correctly aborts if the WebState is null.
TEST_F(AssistantAIMMediatorTest, AbortsWhenWebStateNull) {
  lens::ClientToAimMessage message;
  // Non-empty message.
  message.mutable_submit_query()->mutable_payload()->set_query_text("test");

  fake_web_state_ = nullptr;
  [mediator_ disconnect];

  [[mock_delegate_ reject] assistantAIMMediatorDidLoadQuery:[OCMArg any]];

  [mediator_ prepareLoadWithClientToAimMessage:message];

  [mock_delegate_ verify];
}

// Tests that prepareLoadForQueryText successfully loads the query and notifies
// the delegate when valid parameters are provided.
TEST_F(AssistantAIMMediatorTest, LoadsSuccessfully) {
  lens::ClientToAimMessage message;
  // Non-empty message.
  message.mutable_submit_query()->mutable_payload()->set_query_text("test");

  [[mock_delegate_ expect] assistantAIMMediatorDidLoadQuery:mediator_];

  [mediator_ prepareLoadWithClientToAimMessage:message];

  [mock_delegate_ verify];
}

// Tests that the navigation policy decider allows standard Google AIM search
// result pages to navigate in-place.
TEST_F(AssistantAIMMediatorTest, AllowsGoogleAIMSearchURL) {
  id<CRWWebStatePolicyDecider> policy_decider =
      static_cast<id<CRWWebStatePolicyDecider>>(mediator_);

  GURL google_aim_url("https://www.google.com/search?udm=50&q=test");
  __block web::WebStatePolicyDecider::PolicyDecision allowed_decision =
      web::WebStatePolicyDecider::PolicyDecision::Cancel();
  [policy_decider
      shouldAllowRequest:[NSURLRequest
                             requestWithURL:net::NSURLWithGURL(google_aim_url)]
             requestInfo:web::WebStatePolicyDecider::RequestInfo(
                             ui::PageTransition::PAGE_TRANSITION_LINK,
                             /*target_frame_is_main=*/true,
                             /*target_frame_is_cross_origin=*/false,
                             /*target_window_is_cross_origin=*/false,
                             /*is_user_initiated=*/true,
                             /*user_tapped_recently=*/true)
         decisionHandler:^(
             web::WebStatePolicyDecider::PolicyDecision decision) {
           allowed_decision = decision;
         }];
  EXPECT_TRUE(allowed_decision.ShouldAllowNavigation());
  EXPECT_EQ(url_loader_->load_new_tab_call_count, 0);
  EXPECT_EQ(url_loader_->load_current_tab_call_count, 0);
}

// Tests that the navigation policy decider intercepts and cancels third-party
// links, dispatching them to open in a new tab.
TEST_F(AssistantAIMMediatorTest,
       InterceptsSameWindowThirdPartyURLAndOpensInNewTab) {
  id<CRWWebStatePolicyDecider> policy_decider =
      static_cast<id<CRWWebStatePolicyDecider>>(mediator_);

  GURL third_party_url("https://attacker.com");
  __block web::WebStatePolicyDecider::PolicyDecision blocked_decision =
      web::WebStatePolicyDecider::PolicyDecision::Allow();

  [policy_decider
      shouldAllowRequest:[NSURLRequest
                             requestWithURL:net::NSURLWithGURL(third_party_url)]
             requestInfo:web::WebStatePolicyDecider::RequestInfo(
                             ui::PageTransition::PAGE_TRANSITION_LINK,
                             /*target_frame_is_main=*/true,
                             /*target_frame_is_cross_origin=*/true,
                             /*target_window_is_cross_origin=*/false,
                             /*is_user_initiated=*/true,
                             /*user_tapped_recently=*/true)
         decisionHandler:^(
             web::WebStatePolicyDecider::PolicyDecision decision) {
           blocked_decision = decision;
         }];
  EXPECT_TRUE(blocked_decision.ShouldCancelNavigation());
  EXPECT_EQ(url_loader_->load_new_tab_call_count, 1);
  EXPECT_EQ(url_loader_->last_params.web_params.url, third_party_url);
}

// Tests that the navigation policy decider intercepts and cancels
// target="_blank" third-party links, dispatching them to open in a new tab.
TEST_F(AssistantAIMMediatorTest, InterceptsThirdPartyURLAndOpensInNewTab) {
  id<CRWWebStatePolicyDecider> policy_decider =
      static_cast<id<CRWWebStatePolicyDecider>>(mediator_);

  GURL third_party_url("https://attacker.com");
  __block web::WebStatePolicyDecider::PolicyDecision blocked_decision =
      web::WebStatePolicyDecider::PolicyDecision::Allow();

  [policy_decider
      shouldAllowRequest:[NSURLRequest
                             requestWithURL:net::NSURLWithGURL(third_party_url)]
             requestInfo:web::WebStatePolicyDecider::RequestInfo(
                             ui::PageTransition::PAGE_TRANSITION_LINK,
                             /*target_frame_is_main=*/true,
                             /*target_frame_is_cross_origin=*/true,
                             /*target_window_is_cross_origin=*/true,
                             /*is_user_initiated=*/true,
                             /*user_tapped_recently=*/true)
         decisionHandler:^(
             web::WebStatePolicyDecider::PolicyDecision decision) {
           blocked_decision = decision;
         }];
  EXPECT_TRUE(blocked_decision.ShouldCancelNavigation());
  EXPECT_EQ(url_loader_->load_new_tab_call_count, 1);
  EXPECT_EQ(url_loader_->last_params.web_params.url, third_party_url);
}

// Tests that the navigation policy decider allows authorized Google
// redirections.
TEST_F(AssistantAIMMediatorTest, AllowsAuthorizedGoogleRedirection) {
  id<CRWWebStatePolicyDecider> policy_decider =
      static_cast<id<CRWWebStatePolicyDecider>>(mediator_);

  GURL google_redirect_url("https://www.google.com/search?q=test");
  __block web::WebStatePolicyDecider::PolicyDecision allowed_decision =
      web::WebStatePolicyDecider::PolicyDecision::Cancel();

  [policy_decider
      shouldAllowRequest:[NSURLRequest requestWithURL:net::NSURLWithGURL(
                                                          google_redirect_url)]
             requestInfo:
                 web::WebStatePolicyDecider::RequestInfo(
                     ui::PageTransition::PAGE_TRANSITION_CLIENT_REDIRECT,
                     /*target_frame_is_main=*/true,
                     /*target_frame_is_cross_origin=*/false,
                     /*target_window_is_cross_origin=*/false,
                     /*is_user_initiated=*/true,
                     /*user_tapped_recently=*/true)
         decisionHandler:^(
             web::WebStatePolicyDecider::PolicyDecision decision) {
           allowed_decision = decision;
         }];
  EXPECT_TRUE(allowed_decision.ShouldAllowNavigation());
  EXPECT_EQ(url_loader_->load_new_tab_call_count, 0);
  EXPECT_EQ(url_loader_->load_current_tab_call_count, 0);
}

// Tests that the navigation policy decider intercepts and cancels unauthorized
// Google redirections (e.g. to AMP viewer or arbitrary paths).
TEST_F(AssistantAIMMediatorTest, CancelsUnauthorizedGoogleRedirection) {
  id<CRWWebStatePolicyDecider> policy_decider =
      static_cast<id<CRWWebStatePolicyDecider>>(mediator_);

  GURL unauthorized_google_url("https://www.google.com/amp/s/attacker.com");
  __block web::WebStatePolicyDecider::PolicyDecision blocked_decision =
      web::WebStatePolicyDecider::PolicyDecision::Allow();

  [policy_decider
      shouldAllowRequest:[NSURLRequest
                             requestWithURL:net::NSURLWithGURL(
                                                unauthorized_google_url)]
             requestInfo:
                 web::WebStatePolicyDecider::RequestInfo(
                     ui::PageTransition::PAGE_TRANSITION_CLIENT_REDIRECT,
                     /*target_frame_is_main=*/true,
                     /*target_frame_is_cross_origin=*/false,
                     /*target_window_is_cross_origin=*/false,
                     /*is_user_initiated=*/true,
                     /*user_tapped_recently=*/true)
         decisionHandler:^(
             web::WebStatePolicyDecider::PolicyDecision decision) {
           blocked_decision = decision;
         }];
  EXPECT_TRUE(blocked_decision.ShouldCancelNavigation());
  EXPECT_EQ(url_loader_->load_new_tab_call_count, 1);
  EXPECT_EQ(url_loader_->last_params.web_params.url, unauthorized_google_url);
}

// Tests that the handshake timer is NOT started on a non-AimURL page.
TEST_F(AssistantAIMMediatorTest, HandshakeTimerNotStartedOnNonAimURL) {
  fake_web_state_->SetCurrentURL(GURL("https://www.google.com/"));

  auto main_frame = web::FakeWebFrame::CreateMainWebFrame(
      url::Origin::Create(GURL("https://www.google.com/")));
  auto* main_frame_ptr = main_frame.get();
  main_frame_ptr->set_browser_state(profile_.get());

  web::FakeWebFramesManager* frames_manager =
      static_cast<web::FakeWebFramesManager*>(
          fake_web_state_->GetWebFramesManager(
              web::ContentWorld::kPageContentWorld));
  frames_manager->AddWebFrame(std::move(main_frame));

  // Handshake ping should NOT be sent since it is not an AimURL.
  EXPECT_TRUE(main_frame_ptr->GetJavaScriptCallHistory().empty());
}

// Tests that the handshake timer IS started when the main frame becomes
// available on an AimURL page.
TEST_F(AssistantAIMMediatorTest, HandshakeTimerStartedOnAimURL) {
  fake_web_state_->SetCurrentURL(
      GURL("https://www.google.com/search?q=test&udm=50"));

  auto main_frame = web::FakeWebFrame::CreateMainWebFrame(
      url::Origin::Create(GURL("https://www.google.com/")));
  auto* main_frame_ptr = main_frame.get();
  main_frame_ptr->set_browser_state(profile_.get());

  web::FakeWebFramesManager* frames_manager =
      static_cast<web::FakeWebFramesManager*>(
          fake_web_state_->GetWebFramesManager(
              web::ContentWorld::kPageContentWorld));
  frames_manager->AddWebFrame(std::move(main_frame));

  // Handshake ping should be sent immediately.
  EXPECT_FALSE(main_frame_ptr->GetJavaScriptCallHistory().empty());
  std::u16string last_call = main_frame_ptr->GetLastJavaScriptCall();
  EXPECT_TRUE(
      last_call.find(
          u"__gCrWeb.callFunctionInGcrWeb('aimCobrowse', 'sendNativeToWeb',") !=
      std::u16string::npos);
}

// Tests that the handshake timer is NOT started when a non-main frame (child
// frame) becomes available on an AimURL page.
TEST_F(AssistantAIMMediatorTest, HandshakeTimerNotStartedOnNonMainFrame) {
  fake_web_state_->SetCurrentURL(
      GURL("https://www.google.com/search?q=test&udm=50"));

  auto child_frame = web::FakeWebFrame::CreateChildWebFrame(
      url::Origin::Create(GURL("https://www.google.com/")));
  auto* child_frame_ptr = child_frame.get();
  child_frame_ptr->set_browser_state(profile_.get());

  web::FakeWebFramesManager* frames_manager =
      static_cast<web::FakeWebFramesManager*>(
          fake_web_state_->GetWebFramesManager(
              web::ContentWorld::kPageContentWorld));
  frames_manager->AddWebFrame(std::move(child_frame));

  // Handshake ping should NOT be sent since this is a non-main frame.
  EXPECT_TRUE(child_frame_ptr->GetJavaScriptCallHistory().empty());
}

// Tests that navigating away from an AimURL stops the handshake timer.
TEST_F(AssistantAIMMediatorTest, HandshakeTimerStoppedOnNavigationToNonAimURL) {
  fake_web_state_->SetCurrentURL(
      GURL("https://www.google.com/search?q=test&udm=50"));

  auto main_frame = web::FakeWebFrame::CreateMainWebFrame(
      url::Origin::Create(GURL("https://www.google.com/")));
  auto* main_frame_ptr = main_frame.get();
  main_frame_ptr->set_browser_state(profile_.get());

  web::FakeWebFramesManager* frames_manager =
      static_cast<web::FakeWebFramesManager*>(
          fake_web_state_->GetWebFramesManager(
              web::ContentWorld::kPageContentWorld));
  frames_manager->AddWebFrame(std::move(main_frame));

  // Ping 1 sent immediately.
  ASSERT_EQ(main_frame_ptr->GetJavaScriptCallHistory().size(), 1u);

  // Fast forward 1 second. The repeating timer should fire once.
  task_environment_.FastForwardBy(base::Seconds(1));
  EXPECT_EQ(main_frame_ptr->GetJavaScriptCallHistory().size(), 2u);

  // Simulate navigating to a non-AimURL.
  fake_web_state_->SetCurrentURL(GURL("https://www.google.com/"));
  auto non_aim_main_frame = web::FakeWebFrame::CreateMainWebFrame(
      url::Origin::Create(GURL("https://www.google.com/")));
  auto* non_aim_main_frame_ptr = non_aim_main_frame.get();
  non_aim_main_frame_ptr->set_browser_state(profile_.get());

  frames_manager->RemoveWebFrame(main_frame_ptr->GetFrameId());
  frames_manager->AddWebFrame(std::move(non_aim_main_frame));

  // Fast forward another 5 seconds. No new pings should be sent.
  task_environment_.FastForwardBy(base::Seconds(5));
  EXPECT_TRUE(non_aim_main_frame_ptr->GetJavaScriptCallHistory().empty());
}

// Tests that navigating from one AimURL to another AimURL resets and restarts
// the handshake.
TEST_F(AssistantAIMMediatorTest,
       HandshakeTimerRestartsOnNavigationToAnotherAimURL) {
  fake_web_state_->SetCurrentURL(
      GURL("https://www.google.com/search?q=page1&udm=50"));

  auto main_frame = web::FakeWebFrame::CreateMainWebFrame(
      url::Origin::Create(GURL("https://www.google.com/")));
  auto* main_frame_ptr = main_frame.get();
  main_frame_ptr->set_browser_state(profile_.get());

  web::FakeWebFramesManager* frames_manager =
      static_cast<web::FakeWebFramesManager*>(
          fake_web_state_->GetWebFramesManager(
              web::ContentWorld::kPageContentWorld));
  frames_manager->AddWebFrame(std::move(main_frame));

  // Ping 1 sent immediately.
  ASSERT_EQ(main_frame_ptr->GetJavaScriptCallHistory().size(), 1u);

  // Simulate handshake response received on Page 1 (stops the timer).
  AssistantAimTabHelper* tab_helper =
      AssistantAimTabHelper::FromWebState(fake_web_state_);
  lens::AimToClientMessage handshake_response;
  handshake_response.mutable_handshake_response();
  tab_helper->OnMessageReceived(handshake_response);
  ASSERT_TRUE(tab_helper->IsHandshakeReceived());

  // Fast forward 5 seconds. No new pings should be sent because the timer was
  // stopped.
  task_environment_.FastForwardBy(base::Seconds(5));
  EXPECT_EQ(main_frame_ptr->GetJavaScriptCallHistory().size(), 1u);

  // Simulate navigating to AimURL 2.
  fake_web_state_->SetCurrentURL(
      GURL("https://www.google.com/search?q=page2&udm=50"));
  web::FakeNavigationContext context;
  context.SetIsSameDocument(false);
  tab_helper->DidStartNavigation(fake_web_state_, &context);

  // Verify handshake is now reset.
  EXPECT_FALSE(tab_helper->IsHandshakeReceived());

  // Register the new main frame for Page 2.
  auto next_main_frame = web::FakeWebFrame::CreateMainWebFrame(
      url::Origin::Create(GURL("https://www.google.com/")));
  auto* next_main_frame_ptr = next_main_frame.get();
  next_main_frame_ptr->set_browser_state(profile_.get());

  frames_manager->RemoveWebFrame(main_frame_ptr->GetFrameId());
  frames_manager->AddWebFrame(std::move(next_main_frame));

  // A new handshake ping should be sent immediately on Page 2!
  EXPECT_EQ(next_main_frame_ptr->GetJavaScriptCallHistory().size(), 1u);
  std::u16string last_call = next_main_frame_ptr->GetLastJavaScriptCall();
  EXPECT_TRUE(
      last_call.find(
          u"__gCrWeb.callFunctionInGcrWeb('aimCobrowse', 'sendNativeToWeb',") !=
      std::u16string::npos);
}

// Tests that receiving a handshake response stores capabilities correctly in
// the mediator.
TEST_F(AssistantAIMMediatorTest, HandshakeCapabilitiesStored) {
  EXPECT_FALSE(mediator_.capabilities.has_value());

  AssistantAimTabHelper* tab_helper =
      AssistantAimTabHelper::FromWebState(fake_web_state_);
  lens::AimToClientMessage message;
  message.mutable_handshake_response()->add_capabilities(
      lens::FeatureCapability::DEFAULT);
  message.mutable_handshake_response()->add_capabilities(
      lens::FeatureCapability::OPEN_THREADS_VIEW);

  tab_helper->OnMessageReceived(message);

  // Capabilities should now be stored.
  ASSERT_TRUE(mediator_.capabilities.has_value());
  EXPECT_EQ(mediator_.capabilities->size(), 2u);
  EXPECT_TRUE([mediator_ supportsCapability:lens::FeatureCapability::DEFAULT]);
  EXPECT_TRUE([mediator_
      supportsCapability:lens::FeatureCapability::OPEN_THREADS_VIEW]);
  EXPECT_FALSE(
      [mediator_ supportsCapability:lens::FeatureCapability::LOCK_INPUT]);
}

// Tests that stored capabilities are cleared when navigating to a new page.
TEST_F(AssistantAIMMediatorTest, HandshakeCapabilitiesResetOnNavigation) {
  fake_web_state_->SetCurrentURL(
      GURL("https://www.google.com/search?q=test&udm=50"));

  AssistantAimTabHelper* tab_helper =
      AssistantAimTabHelper::FromWebState(fake_web_state_);
  lens::AimToClientMessage message;
  message.mutable_handshake_response()->add_capabilities(
      lens::FeatureCapability::DEFAULT);

  tab_helper->OnMessageReceived(message);
  ASSERT_TRUE(mediator_.capabilities.has_value());

  // Simulate navigating to a new AIM page.
  fake_web_state_->SetCurrentURL(
      GURL("https://www.google.com/search?q=test2&udm=50"));
  web::FakeNavigationContext context;
  context.SetIsSameDocument(false);
  tab_helper->DidStartNavigation(fake_web_state_, &context);

  // Simulate main frame becoming available.
  auto main_frame = web::FakeWebFrame::CreateMainWebFrame(
      url::Origin::Create(GURL("https://www.google.com/")));
  main_frame->set_browser_state(profile_.get());
  web::FakeWebFramesManager* frames_manager =
      static_cast<web::FakeWebFramesManager*>(
          fake_web_state_->GetWebFramesManager(
              web::ContentWorld::kPageContentWorld));
  frames_manager->AddWebFrame(std::move(main_frame));

  // Verify capabilities are reset to std::nullopt.
  EXPECT_FALSE(mediator_.capabilities.has_value());
}

// Tests that createNewWebStateForURL correctly dispatches the request to open
// in a new tab via the scene handler, and minimizes the assistant container.
TEST_F(AssistantAIMMediatorTest, OpensWebStateInNewTabViaDelegate) {
  GURL url("https://example.com");

  // Expect the scene handler to be called with a command to open the URL in a
  // new tab. We also verify that inIncognito is NO (as incognito is not
  // supported).
  OCMExpect([mock_scene_handler_
      openURLInNewTab:[OCMArg checkWithBlock:^BOOL(OpenNewTabCommand* command) {
        return command.URL == url && !command.inIncognito;
      }]]);

  // Expect the container to be animated to minimized detent.
  OCMExpect([mock_container_handler_
      animateAssistantContainerToDetent:AssistantContainerDetent::kMinimized
                               duration:kSheetDetentAnimationDuration
                                  curve:UIViewAnimationCurveEaseInOut]);

  id<CRWWebStateDelegate> delegate = (id<CRWWebStateDelegate>)mediator_;
  web::WebState* result = [delegate webState:fake_web_state_
                     createNewWebStateForURL:url
                                   openerURL:GURL()
                             initiatedByUser:YES];

  EXPECT_EQ(result, nullptr);
  [mock_scene_handler_ verify];
  [mock_container_handler_ verify];
}
