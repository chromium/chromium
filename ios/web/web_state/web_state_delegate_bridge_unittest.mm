// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/web_state_delegate_bridge.h"

#import <Foundation/Foundation.h>

#include <memory>

#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#import "ios/web/public/test/crw_fake_web_state_delegate.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#import "ios/web/public/ui/context_menu_params.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#include "ui/base/page_transition_types.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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
  web::TestWebState test_web_state_;
};

// Tests |webState:createNewWebStateForURL:openerURL:initiatedByUser:|
// forwarding.
TEST_F(WebStateDelegateBridgeTest, CreateNewWebState) {
  ASSERT_FALSE([delegate_ webState]);
  ASSERT_FALSE([delegate_ webStateCreationRequested]);

  EXPECT_FALSE(
      bridge_->CreateNewWebState(&test_web_state_, GURL(), GURL(), true));

  EXPECT_EQ(&test_web_state_, [delegate_ webState]);
  ASSERT_TRUE([delegate_ webStateCreationRequested]);
}

// Tests |closeWebState:| forwarding.
TEST_F(WebStateDelegateBridgeTest, CloseWebState) {
  ASSERT_FALSE([delegate_ webState]);
  ASSERT_FALSE([delegate_ webStateClosingRequested]);

  bridge_->CloseWebState(&test_web_state_);

  EXPECT_EQ(&test_web_state_, [delegate_ webState]);
  ASSERT_TRUE([delegate_ webStateClosingRequested]);
}

// Tests |webState:openURLWithParams:| forwarding.
TEST_F(WebStateDelegateBridgeTest, OpenURLFromWebState) {
  ASSERT_FALSE([delegate_ webState]);
  ASSERT_FALSE([delegate_ openURLParams]);

  web::WebState::OpenURLParams params(
      GURL("https://chromium.test/"), GURL("https://virtual.chromium.test/"),
      web::Referrer(GURL("https://chromium2.test/"), ReferrerPolicyNever),
      WindowOpenDisposition::NEW_WINDOW, ui::PAGE_TRANSITION_FORM_SUBMIT, true);
  EXPECT_EQ(&test_web_state_,
            bridge_->OpenURLFromWebState(&test_web_state_, params));

  EXPECT_EQ(&test_web_state_, [delegate_ webState]);
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

// Tests |HandleContextMenu| forwarding.
TEST_F(WebStateDelegateBridgeTest, HandleContextMenu) {
  EXPECT_EQ(nil, [delegate_ contextMenuParams]);
  web::ContextMenuParams context_menu_params;
  context_menu_params.menu_title = [@"Menu title" copy];
  context_menu_params.link_url = GURL("http://www.url.com");
  context_menu_params.src_url = GURL("http://www.url.com/image.jpeg");
  context_menu_params.referrer_policy = web::ReferrerPolicyOrigin;
  context_menu_params.view = [[UIView alloc] init];
  context_menu_params.location = CGPointMake(5.0, 5.0);
  bridge_->HandleContextMenu(nullptr, context_menu_params);
  web::ContextMenuParams* result_params = [delegate_ contextMenuParams];
  EXPECT_NE(nullptr, result_params);
  EXPECT_EQ(context_menu_params.menu_title, result_params->menu_title);
  EXPECT_EQ(context_menu_params.link_url, result_params->link_url);
  EXPECT_EQ(context_menu_params.src_url, result_params->src_url);
  EXPECT_EQ(context_menu_params.referrer_policy,
            result_params->referrer_policy);
  EXPECT_EQ(context_menu_params.view, result_params->view);
  EXPECT_EQ(context_menu_params.location.x, result_params->location.x);
  EXPECT_EQ(context_menu_params.location.y, result_params->location.y);
}

// Tests |ShowRepostFormWarningDialog| forwarding.
TEST_F(WebStateDelegateBridgeTest, ShowRepostFormWarningDialog) {
  EXPECT_FALSE([delegate_ repostFormWarningRequested]);
  EXPECT_FALSE([delegate_ webState]);
  base::Callback<void(bool)> callback;
  bridge_->ShowRepostFormWarningDialog(&test_web_state_, callback);
  EXPECT_TRUE([delegate_ repostFormWarningRequested]);
  EXPECT_EQ(&test_web_state_, [delegate_ webState]);
}

// Tests |ShowRepostFormWarningDialog| forwarding to delegate which does not
// implement |webState:runRepostFormDialogWithCompletionHandler:| method.
TEST_F(WebStateDelegateBridgeTest, ShowRepostFormWarningWithNoDelegateMethod) {
  __block bool callback_called = false;
  empty_delegate_bridge_->ShowRepostFormWarningDialog(
      nullptr, base::BindOnce(^(bool should_repost) {
        EXPECT_TRUE(should_repost);
        callback_called = true;
      }));
  EXPECT_TRUE(callback_called);
}

// Tests |GetJavaScriptDialogPresenter| forwarding.
TEST_F(WebStateDelegateBridgeTest, GetJavaScriptDialogPresenter) {
  EXPECT_FALSE([delegate_ javaScriptDialogPresenterRequested]);
  bridge_->GetJavaScriptDialogPresenter(nullptr);
  EXPECT_TRUE([delegate_ javaScriptDialogPresenterRequested]);
}

// Tests |OnAuthRequired| forwarding.
TEST_F(WebStateDelegateBridgeTest, OnAuthRequired) {
  EXPECT_FALSE([delegate_ authenticationRequested]);
  EXPECT_FALSE([delegate_ webState]);
  NSURLProtectionSpace* protection_space = [[NSURLProtectionSpace alloc] init];
  NSURLCredential* credential = [[NSURLCredential alloc] init];
  WebStateDelegate::AuthCallback callback;
  bridge_->OnAuthRequired(&test_web_state_, protection_space, credential,
                          callback);
  EXPECT_TRUE([delegate_ authenticationRequested]);
  EXPECT_EQ(&test_web_state_, [delegate_ webState]);
}

// Tests |ShouldPreviewLink| forwarding.
TEST_F(WebStateDelegateBridgeTest, ShouldPreviewLinkWithURL) {
  GURL link_url("http://link.test/");
  EXPECT_FALSE(delegate_.webState);

  delegate_.shouldPreviewLinkWithURLReturnValue = YES;
  EXPECT_TRUE(bridge_->ShouldPreviewLink(&test_web_state_, link_url));
  EXPECT_EQ(&test_web_state_, delegate_.webState);
  EXPECT_EQ(link_url, delegate_.linkURL);

  delegate_.shouldPreviewLinkWithURLReturnValue = NO;
  EXPECT_FALSE(bridge_->ShouldPreviewLink(&test_web_state_, link_url));
  EXPECT_EQ(&test_web_state_, delegate_.webState);
  EXPECT_EQ(link_url, delegate_.linkURL);
}

// Tests |GetPreviewingViewController| forwarding.
TEST_F(WebStateDelegateBridgeTest, GetPreviewingViewController) {
  GURL link_url("http://link.test/");
  UIViewController* previewing_view_controller =
      OCMClassMock([UIViewController class]);

  EXPECT_FALSE(delegate_.webState);
  delegate_.previewingViewControllerForLinkWithURLReturnValue =
      previewing_view_controller;
  EXPECT_EQ(previewing_view_controller,
            bridge_->GetPreviewingViewController(&test_web_state_, link_url));
  EXPECT_EQ(&test_web_state_, delegate_.webState);
  EXPECT_EQ(link_url, delegate_.linkURL);
}

// Tests |CommitPreviewingViewController| forwarding.
TEST_F(WebStateDelegateBridgeTest, CommitPreviewingViewController) {
  UIViewController* previewing_view_controller =
      OCMClassMock([UIViewController class]);

  EXPECT_FALSE(delegate_.webState);
  EXPECT_FALSE(delegate_.previewingViewController);
  bridge_->CommitPreviewingViewController(&test_web_state_,
                                          previewing_view_controller);
  EXPECT_TRUE(delegate_.commitPreviewingViewControllerRequested);
  EXPECT_EQ(&test_web_state_, delegate_.webState);
  EXPECT_EQ(previewing_view_controller, delegate_.previewingViewController);
}

}  // namespace web
