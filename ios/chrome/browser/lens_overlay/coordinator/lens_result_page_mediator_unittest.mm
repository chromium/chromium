// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/lens_overlay/coordinator/lens_result_page_mediator.h"

#import "base/memory/raw_ptr.h"
#import "base/test/scoped_feature_list.h"
#import "components/variations/scoped_variations_ids_provider.h"
#import "components/variations/variations_ids_provider.h"
#import "ios/chrome/browser/lens_overlay/ui/lens_result_page_consumer.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_authentication_service_delegate.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/navigation/web_state_policy_decider.h"
#import "ios/web/public/navigation/web_state_policy_decider_bridge.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_frames_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/fakes/fake_web_state_delegate.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

@interface LensResultPageMediator (Testing)
- (std::unique_ptr<web::WebState>)detachWebState;
- (void)attachWebState:(std::unique_ptr<web::WebState>)webState;
@end

namespace {

class LensResultPageMediatorTest : public PlatformTest {
 public:
  LensResultPageMediatorTest() {
    // AuthenticationService in required in AttachTabHelpers.
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetDefaultFactory());
    profile_ = std::move(builder).Build();
    AuthenticationServiceFactory::CreateAndInitializeForProfile(
        profile_.get(), std::make_unique<FakeAuthenticationServiceDelegate>());

    web::WebState::CreateParams params(profile_.get());
    mediator_ = [[LensResultPageMediator alloc]
         initWithWebStateParams:params
        browserWebStateDelegate:&browser_web_state_delegate_
                   webStateList:nil
                    isIncognito:NO];

    mock_consumer_ =
        [OCMockObject niceMockForProtocol:@protocol(LensResultPageConsumer)];
    mock_application_handler_ =
        [OCMockObject mockForProtocol:@protocol(ApplicationCommands)];

    mediator_.consumer = mock_consumer_;
    mediator_.applicationHandler = mock_application_handler_;
  }

  ~LensResultPageMediatorTest() override {
    [mediator_ disconnect];
    PlatformTest::TearDown();
  }

  // Tests whether the navigation is allowed by the policy provider.
  [[nodiscard]] bool TestShouldAllowRequest(NSString* url_string,
                                            bool target_frame_is_main) {
    NSURL* url = [NSURL URLWithString:url_string];
    const web::WebStatePolicyDecider::RequestInfo request_info(
        ui::PageTransition::PAGE_TRANSITION_LINK, target_frame_is_main,
        /*target_frame_is_cross_origin=*/false,
        /*target_window_is_cross_origin=*/false,
        /*is_user_initiated=*/true,
        /*user_tapped_recently=*/false);
    __block bool callback_called = false;
    __block web::WebStatePolicyDecider::PolicyDecision policy_decision =
        web::WebStatePolicyDecider::PolicyDecision::Allow();
    auto callback = ^(web::WebStatePolicyDecider::PolicyDecision decision) {
      policy_decision = decision;
      callback_called = true;
    };

    if ([mediator_ conformsToProtocol:@protocol(CRWWebStatePolicyDecider)]) {
      id<CRWWebStatePolicyDecider> policy_decider =
          static_cast<id<CRWWebStatePolicyDecider>>(mediator_);
      [policy_decider shouldAllowRequest:[NSURLRequest requestWithURL:url]
                             requestInfo:request_info
                         decisionHandler:std::move(callback)];
    }
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(callback_called);
    return policy_decision.ShouldAllowNavigation();
  }

  // Replaces the web state from LensResultPageMediator with a fake one.
  void AttachFakeWebState() {
    auto web_state = std::make_unique<web::FakeWebState>();
    web_state->SetBrowserState(profile_.get());
    web_state->SetIsRealized(true);
    web_state->SetWebFramesManager(
        web::ContentWorld::kAllContentWorlds,
        std::make_unique<web::FakeWebFramesManager>());
    web_state->SetWebFramesManager(
        web::ContentWorld::kPageContentWorld,
        std::make_unique<web::FakeWebFramesManager>());
    web_state->SetWebFramesManager(
        web::ContentWorld::kIsolatedWorld,
        std::make_unique<web::FakeWebFramesManager>());
    web_state->SetNavigationManager(
        std::make_unique<web::FakeNavigationManager>());
    fake_web_state_ = web_state.get();
    [mediator_ detachWebState];
    [mediator_ attachWebState:std::move(web_state)];
  }

  // Returns the fake navigation manager from the fake web state.
  web::FakeNavigationManager* GetFakeNavigationManager() {
    return static_cast<web::FakeNavigationManager*>(
        fake_web_state_->GetNavigationManager());
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  variations::ScopedVariationsIdsProvider scoped_variations_ids_provider_{
      variations::VariationsIdsProvider::Mode::kUseSignedInState};

  LensResultPageMediator* mediator_;
  std::unique_ptr<TestProfileIOS> profile_;
  web::FakeWebStateDelegate browser_web_state_delegate_;
  OCMockObject<LensResultPageConsumer>* mock_consumer_;
  OCMockObject<ApplicationCommands>* mock_application_handler_;

  // Call `AttachFakeWebState()` to use `fake_web_state_`.
  raw_ptr<web::FakeWebState> fake_web_state_;
};

// Tests that the mediator starts a navigation when loadResultsURL is called.
TEST_F(LensResultPageMediatorTest, ShouldStartNavigationWhenLoadingResultsURL) {
  ASSERT_EQ(variations::VariationsIdsProvider::ForceIdsResult::SUCCESS,
            variations::VariationsIdsProvider::GetInstance()->ForceVariationIds(
                /*variation_ids=*/{"100"}, /*command_line_variation_ids=*/""));
  AttachFakeWebState();
  GURL result_url = GURL("https://www.google.com");

  // Expect that the light mode query param is added to the URL.
  [mediator_ setIsDarkMode:NO];
  [mediator_ loadResultsURL:result_url];
  GURL light_mode_url = GURL("https://www.google.com?cs=0");
  EXPECT_TRUE(GetFakeNavigationManager()->LoadURLWithParamsWasCalled());
  std::optional<web::NavigationManager::WebLoadParams> load_params =
      GetFakeNavigationManager()->GetLastLoadURLWithParams();
  ASSERT_TRUE(load_params.has_value());
  EXPECT_EQ(load_params->url, light_mode_url);
  // Expect that the client data header is added to the request.
  ASSERT_TRUE([load_params->extra_headers objectForKey:@"X-Client-Data"]);

  // Expect that the dark mode query param is added to the URL.
  [mediator_ setIsDarkMode:YES];
  [mediator_ loadResultsURL:result_url];
  GURL dark_mode_url = GURL("https://www.google.com?cs=1");
  EXPECT_TRUE(GetFakeNavigationManager()->LoadURLWithParamsWasCalled());
  load_params = GetFakeNavigationManager()->GetLastLoadURLWithParams();
  ASSERT_TRUE(load_params.has_value());
  EXPECT_EQ(load_params->url, dark_mode_url);
  // Expect that the client data header is added to the request.
  ASSERT_TRUE([load_params->extra_headers objectForKey:@"X-Client-Data"]);
}

// Tests that web navigation to google is allowed.
TEST_F(LensResultPageMediatorTest, ShouldAllowGoogleNavigation) {
  EXPECT_TRUE(TestShouldAllowRequest(@"https://www.google.com",
                                     /*target_frame_is_main=*/true));
}

// Tests that other navigation is not allowed but opens a new tab.
TEST_F(LensResultPageMediatorTest, ShouldOpenOtherNavigationInNewTab) {
  OCMExpect([mock_application_handler_ openURLInNewTab:[OCMArg any]]);
  EXPECT_FALSE(TestShouldAllowRequest(@"https://www.chromium.com",
                                      /*target_frame_is_main=*/true));
  EXPECT_OCMOCK_VERIFY(mock_application_handler_);
}

// Tests that any navigation that's not on main frame is allowed.
TEST_F(LensResultPageMediatorTest, ShouldAllowAnyNavigationNotInMainFrame) {
  EXPECT_TRUE(TestShouldAllowRequest(@"https://www.chromium.com",
                                     /*target_frame_is_main=*/false));
  EXPECT_TRUE(TestShouldAllowRequest(@"https://www.google.com",
                                     /*target_frame_is_main=*/false));
}

}  // namespace
