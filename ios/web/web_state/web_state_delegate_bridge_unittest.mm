// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/web_state_delegate_bridge.h"

#import <Foundation/Foundation.h>

#import <memory>

#import "base/functional/bind.h"
#import "base/functional/callback_helpers.h"
#import "base/strings/utf_string_conversions.h"
#import "ios/web/public/test/crw_fake_web_state_delegate.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/ui/context_menu_params.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "ui/base/page_transition_types.h"

// Class which conforms to CRWWebStateDelegate protocol, but does not implement
// any optional methods.
@interface TestEmptyWebStateDelegate : NSObject<CRWWebStateDelegate>
@end

@implementation TestEmptyWebStateDelegate
@end

namespace web {

// Test fixture to test WebStateDelegateBridge class.
class WebStateDelegateBridgeTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();

    delegate_ = [[CRWFakeWebStateDelegate alloc] init];
    empty_delegate_ = [[TestEmptyWebStateDelegate alloc] init];

    bridge_.reset(new WebStateDelegateBridge(delegate_));
    empty_delegate_bridge_.reset(new WebStateDelegateBridge(empty_delegate_));
  }

  void TearDown() override {
    PlatformTest::TearDown();
  }

  CRWFakeWebStateDelegate* delegate_;
  id empty_delegate_;
  std::unique_ptr<WebStateDelegateBridge> bridge_;
  std::unique_ptr<WebStateDelegateBridge> empty_delegate_bridge_;
  web::FakeWebState fake_web_state_;
};

// Tests `webState:createNewWebStateForURL:openerURL:initiatedByUser:`
// forwarding.
TEST_F(WebStateDelegateBridgeTest, CreateNewWebState) {
  ASSERT_FALSE([delegate_ webState]);
  ASSERT_FALSE([delegate_ webStateCreationRequested]);

  EXPECT_FALSE(
      bridge_->CreateNewWebState(&fake_web_state_, GURL(), GURL(), true));

  EXPECT_EQ(&fake_web_state_, [delegate_ webState]);
  ASSERT_TRUE([delegate_ webStateCreationRequested]);
}

// Tests `closeWebState:` forwarding.
TEST_F(WebStateDelegateBridgeTest, CloseWebState) {
  ASSERT_FALSE([delegate_ webState]);
  ASSERT_FALSE([delegate_ webStateClosingRequested]);

  bridge_->CloseWebState(&fake_web_state_);

  EXPECT_EQ(&fake_web_state_, [delegate_ webState]);
  ASSERT_TRUE([delegate_ webStateClosingRequested]);
}

// Tests `webState:openURLWithParams:` forwarding.
TEST_F(WebStateDelegateBridgeTest, OpenURLFromWebState) {
  ASSERT_FALSE([delegate_ webState]);
  ASSERT_FALSE([delegate_ openURLParams]);

  web::WebState::OpenURLParams params(
      GURL("https://chromium.test/"), GURL("https://virtual.chromium.test/"),
      web::Referrer(GURL("https://chromium2.test/"), ReferrerPolicyNever),
      WindowOpenDisposition::NEW_WINDOW, ui::PAGE_TRANSITION_FORM_SUBMIT, true);
  EXPECT_EQ(&fake_web_state_,
            bridge_->OpenURLFromWebState(&fake_web_state_, params));

  EXPECT_EQ(&fake_web_state_, [delegate_ webState]);
  const web::WebState::OpenURLParams* result_params = [delegate_ openURLParams];
  ASSERT_TRUE(result_params);
  EXPECT_EQ(params.url, result_params->url);
  EXPECT_EQ(params.virtual_url, result_params->virtual_url);
  EXPECT_EQ(params.referrer.url, result_params->referrer.url);
  EXPECT_EQ(params.referrer.policy, result_params->referrer.policy);
  EXPECT_EQ(params.disposition, result_params->disposition);
  EXPECT_EQ(static_cast<int>(params.transition),
            static_cast<int>(result_params->transition));
  EXPECT_EQ(params.is_renderer_initiated, result_params->is_renderer_initiated);
}

// Tests `ShowRepostFormWarningDialog` forwarding.
TEST_F(WebStateDelegateBridgeTest, ShowRepostFormWarningDialog) {
  EXPECT_FALSE([delegate_ repostFormWarningRequested]);
  EXPECT_FALSE([delegate_ webState]);
  base::OnceCallback<void(bool)> callback;
  bridge_->ShowRepostFormWarningDialog(
      &fake_web_state_, web::FormWarningType::kRepost, std::move(callback));
  EXPECT_TRUE([delegate_ repostFormWarningRequested]);
  EXPECT_EQ(&fake_web_state_, [delegate_ webState]);
}

// Tests `ShowRepostFormWarningDialog` forwarding to delegate which does not
// implement `webState:runRepostFormDialogWithCompletionHandler:` method.
TEST_F(WebStateDelegateBridgeTest, ShowRepostFormWarningWithNoDelegateMethod) {
  __block bool callback_called = false;
  empty_delegate_bridge_->ShowRepostFormWarningDialog(
      nullptr, web::FormWarningType::kRepost,
      base::BindOnce(^(bool should_repost) {
        EXPECT_TRUE(should_repost);
        callback_called = true;
      }));
  EXPECT_TRUE(callback_called);
}

// Tests `GetJavaScriptDialogPresenter` forwarding.
TEST_F(WebStateDelegateBridgeTest, GetJavaScriptDialogPresenter) {
  EXPECT_FALSE([delegate_ javaScriptDialogPresenterRequested]);
  bridge_->GetJavaScriptDialogPresenter(nullptr);
  EXPECT_TRUE([delegate_ javaScriptDialogPresenterRequested]);
}

// Tests `HandlePermissionsDecisionRequest` forwarding.
TEST_F(WebStateDelegateBridgeTest, HandlePermissionsDecisionRequest) {
  __block bool callback_called = false;
  EXPECT_FALSE([delegate_ permissionsRequestHandled]);
  EXPECT_FALSE([delegate_ webState]);
  bridge_->HandlePermissionsDecisionRequest(
      &fake_web_state_, @[], ^(PermissionDecision decision) {
        EXPECT_EQ(decision, PermissionDecisionGrant);
        callback_called = true;
      });
  EXPECT_TRUE([delegate_ permissionsRequestHandled]);
  EXPECT_EQ(&fake_web_state_, [delegate_ webState]);
  EXPECT_TRUE(callback_called);
}

// Tests `HandlePermissionsDecisionRequest` forwarding to delegate which does
// not implement `webState:handlePermissions:decisionHandler:` method.
TEST_F(WebStateDelegateBridgeTest,
       HandlePermissionsDecisionRequestWithNoDelegateMethod) {
  __block bool callback_called = false;
  empty_delegate_bridge_->HandlePermissionsDecisionRequest(
      nullptr, @[], ^(PermissionDecision decision) {
        // Default decision `PermissionDecisionShowDefaultPrompt` will be used
        // when delegate doesn't implement
        // `webState:handlePermissions:decisionHandler:` method to handle the
        // permissions.
        EXPECT_EQ(decision, PermissionDecisionShowDefaultPrompt);
        callback_called = true;
      });
  EXPECT_TRUE(callback_called);
}

// Tests `OnAuthRequired` forwarding.
TEST_F(WebStateDelegateBridgeTest, OnAuthRequired) {
  EXPECT_FALSE([delegate_ authenticationRequested]);
  EXPECT_FALSE([delegate_ webState]);
  NSURLProtectionSpace* protection_space = [[NSURLProtectionSpace alloc] init];
  NSURLCredential* credential = [[NSURLCredential alloc] init];
  WebStateDelegate::AuthCallback callback = base::DoNothing();
  bridge_->OnAuthRequired(&fake_web_state_, protection_space, credential,
                          std::move(callback));
  EXPECT_TRUE([delegate_ authenticationRequested]);
  EXPECT_EQ(&fake_web_state_, [delegate_ webState]);
}

}  // namespace web
