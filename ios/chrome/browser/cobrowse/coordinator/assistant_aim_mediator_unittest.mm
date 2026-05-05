// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/cobrowse/coordinator/assistant_aim_mediator.h"

#import "base/memory/raw_ptr.h"
#import "base/test/scoped_feature_list.h"
#import "components/contextual_tasks/public/features.h"
#import "ios/chrome/browser/cobrowse/model/aim_cobrowse_java_script_feature.h"
#import "ios/chrome/browser/cobrowse/model/cobrowse_context.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/url_loading/model/fake_url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_notifier_browser_agent.h"
#import "ios/chrome/browser/web/model/chrome_web_client.h"
#import "ios/web/public/navigation/web_state_policy_decider_bridge.h"
#import "ios/web/public/test/fakes/fake_web_frames_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/scoped_testing_web_client.h"
#import "ios/web/public/test/web_task_environment.h"
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
    fake_web_state->SetBrowserState(profile_.get());
    auto manager = std::make_unique<web::FakeWebFramesManager>();
    fake_web_state->SetWebFramesManager(web::ContentWorld::kIsolatedWorld,
                                        std::move(manager));

    mediator_ =
        [[AssistantAIMMediator alloc] initWithWebState:std::move(fake_web_state)
                                               context:nullptr
                                      containerHandler:nil
                                contextualTasksService:nullptr
                                             URLLoader:url_loader_];

    mock_delegate_ = OCMProtocolMock(@protocol(AssistantAIMMediatorDelegate));
    mediator_.delegate = mock_delegate_;
  }

  void TearDown() override {
    [mediator_ disconnect];
    mediator_ = nil;
    PlatformTest::TearDown();
  }

  web::WebTaskEnvironment task_environment_;
  web::ScopedTestingWebClient web_client_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  raw_ptr<FakeUrlLoadingBrowserAgent> url_loader_;
  AssistantAIMMediator* mediator_;
  id mock_delegate_;
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
// links, dispatching them to open in the current underlying tab.
TEST_F(AssistantAIMMediatorTest, InterceptsThirdPartyURLAndOpensInCurrentTab) {
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
  EXPECT_EQ(url_loader_->load_current_tab_call_count, 1);
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
