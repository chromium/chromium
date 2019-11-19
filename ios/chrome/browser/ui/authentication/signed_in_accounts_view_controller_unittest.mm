// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signed_in_accounts_view_controller.h"

#include <memory>

#include "base/bind.h"
#include "base/strings/sys_string_conversions.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/metrics/previous_session_info.h"
#include "ios/chrome/browser/metrics/previous_session_info_private.h"
#include "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/authentication_service_fake.h"
#include "ios/chrome/test/block_cleanup_test.h"
#include "ios/public/provider/chrome/browser/signin/fake_chrome_identity_service.h"
#include "ios/web/public/test/web_task_environment.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

class SignedInAccountsViewControllerTest : public BlockCleanupTest {
 public:
  void SetUp() override {
    BlockCleanupTest::SetUp();

    TestChromeBrowserState::Builder builder;
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        base::BindRepeating(
            &AuthenticationServiceFake::CreateAuthenticationService));
    browser_state_ = builder.Build();
    auth_service_ = static_cast<AuthenticationServiceFake*>(
        AuthenticationServiceFactory::GetInstance()->GetForBrowserState(
            browser_state_.get()));
    ios::FakeChromeIdentityService* identity_service =
        ios::FakeChromeIdentityService::GetInstanceFromChromeProvider();
    identity_service->AddIdentities(
        @[ @"identity1", @"identity2", @"identity3" ]);
    auth_service_->SignIn(
        [identity_service->GetAllIdentitiesSortedForDisplay() objectAtIndex:0]);
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  AuthenticationServiceFake* auth_service_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
};

// Tests that the signed in accounts view shouldn't be presented when the
// accounts haven't changed.
TEST_F(SignedInAccountsViewControllerTest,
       ShouldBePresentedForBrowserStateNotNecessary) {
  EXPECT_FALSE([SignedInAccountsViewController
      shouldBePresentedForBrowserState:browser_state_.get()]);
}

// Tests that the signed in accounts view should be presented when the accounts
// have changed.
TEST_F(SignedInAccountsViewControllerTest,
       ShouldBePresentedForBrowserStateNecessary) {
  auth_service_->SetHaveAccountsChangedWhileInBackground(true);
  EXPECT_TRUE([SignedInAccountsViewController
      shouldBePresentedForBrowserState:browser_state_.get()]);
}

// Tests that the signed in accounts view shouldn't be presented on the first
// session after upgrade.
TEST_F(SignedInAccountsViewControllerTest,
       ShouldBePresentedForBrowserStateAfterUpgrade) {
  auth_service_->SetHaveAccountsChangedWhileInBackground(true);

  {
    [PreviousSessionInfo resetSharedInstanceForTesting];
    PreviousSessionInfo* prevSessionInfo = [PreviousSessionInfo sharedInstance];
    [prevSessionInfo setIsFirstSessionAfterUpgrade:YES];
    [prevSessionInfo setPreviousSessionVersion:nil];
    EXPECT_TRUE([SignedInAccountsViewController
        shouldBePresentedForBrowserState:browser_state_.get()]);
  }

  {
    [PreviousSessionInfo resetSharedInstanceForTesting];
    PreviousSessionInfo* prevSessionInfo = [PreviousSessionInfo sharedInstance];
    [prevSessionInfo setIsFirstSessionAfterUpgrade:YES];
    EXPECT_TRUE([SignedInAccountsViewController
        shouldBePresentedForBrowserState:browser_state_.get()]);
  }

  {
    [PreviousSessionInfo resetSharedInstanceForTesting];
    PreviousSessionInfo* prevSessionInfo = [PreviousSessionInfo sharedInstance];
    [prevSessionInfo setIsFirstSessionAfterUpgrade:YES];
    [prevSessionInfo setPreviousSessionVersion:@"77.0.1.0"];
    EXPECT_FALSE([SignedInAccountsViewController
        shouldBePresentedForBrowserState:browser_state_.get()]);
  }

  {
    [PreviousSessionInfo resetSharedInstanceForTesting];
    PreviousSessionInfo* prevSessionInfo = [PreviousSessionInfo sharedInstance];
    [prevSessionInfo setIsFirstSessionAfterUpgrade:YES];
    [prevSessionInfo setPreviousSessionVersion:@"78.0.1.0"];
    EXPECT_TRUE([SignedInAccountsViewController
        shouldBePresentedForBrowserState:browser_state_.get()]);
  }
}
