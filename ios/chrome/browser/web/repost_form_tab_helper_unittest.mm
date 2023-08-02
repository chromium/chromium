// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/repost_form_tab_helper.h"

#import <UIKit/UIKit.h>

#import "base/functional/bind.h"
#import "ios/chrome/browser/web/repost_form_tab_helper_delegate.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

// A configurable TabHelper delegate for testing.
@interface RepostFormTabHelperTestDelegate
    : NSObject<RepostFormTabHelperDelegate>

// YES if repost form dialog is currently presented.
@property(nonatomic, readonly, getter=isPresentingDialog) BOOL presentingDialog;

// Location where the dialog was presented last time.
@property(nonatomic, assign) CGPoint location;

// Tab helper which delegates to this class.
@property(nonatomic, assign) RepostFormTabHelper* tabHelper;

// Calls `repostFormTabHelper:presentRepostFromDialogAtPoint:completionHandler:`
// completion handler.
- (void)allowRepost:(BOOL)shouldContinue;

@end

@implementation RepostFormTabHelperTestDelegate {
  void (^_completionHandler)(BOOL);
}

@synthesize presentingDialog = _presentingDialog;
@synthesize location = _location;
@synthesize tabHelper = _tabHelper;

- (void)allowRepost:(BOOL)allow {
  _completionHandler(allow);
  _presentingDialog = NO;
}

- (void)repostFormTabHelper:(RepostFormTabHelper*)helper
    presentRepostFormDialogForWebState:(web::WebState*)webState
                         dialogAtPoint:(CGPoint)location
                     completionHandler:(void (^)(BOOL))completionHandler {
  EXPECT_EQ(_tabHelper, helper);
  EXPECT_FALSE(_presentingDialog);
  _presentingDialog = YES;
  _location = location;
  _completionHandler = [completionHandler copy];
}

- (void)repostFormTabHelperDismissRepostFormDialog:
    (RepostFormTabHelper*)helper {
  EXPECT_EQ(_tabHelper, helper);
  EXPECT_TRUE(_presentingDialog);
  _presentingDialog = NO;
  _completionHandler = nil;
}

@end

namespace {
// Test location passed to RepostFormTabHelper.
const CGFloat kDialogHLocation = 10;
const CGFloat kDialogVLocation = 20;

// No-op callback for RepostFormTabHelper.
void IgnoreBool(bool) {}
}  // namespace

// Test fixture for RepostFormTabHelper class.
class RepostFormTabHelperTest : public PlatformTest {
 protected:
  RepostFormTabHelperTest()
      : web_state_(std::make_unique<web::FakeWebState>()),
        delegate_([[RepostFormTabHelperTestDelegate alloc] init]),
        location_(CGPointMake(kDialogHLocation, kDialogVLocation)) {
    RepostFormTabHelper::CreateForWebState(web_state_.get());
    RepostFormTabHelper::FromWebState(web_state_.get())->SetDelegate(delegate_);
    delegate_.tabHelper = tab_helper();
  }

  RepostFormTabHelper* tab_helper() {
    return RepostFormTabHelper::FromWebState(web_state_.get());
  }

 protected:
  std::unique_ptr<web::FakeWebState> web_state_;
  RepostFormTabHelperTestDelegate* delegate_;
  CGPoint location_;
};

// Tests presentation location.
TEST_F(RepostFormTabHelperTest, Location) {
  EXPECT_FALSE(CGPointEqualToPoint(delegate_.location, location_));
  tab_helper()->PresentDialog(location_, base::BindOnce(&IgnoreBool));
  EXPECT_TRUE(CGPointEqualToPoint(delegate_.location, location_));
}

// Tests cancelling repost.
TEST_F(RepostFormTabHelperTest, CancelRepost) {
  ASSERT_FALSE(delegate_.presentingDialog);
  __block bool callback_called = false;
  tab_helper()->PresentDialog(location_, base::BindOnce(^(bool repost) {
                                EXPECT_FALSE(repost);
                                callback_called = true;
                              }));
  EXPECT_TRUE(delegate_.presentingDialog);

  ASSERT_FALSE(callback_called);
  [delegate_ allowRepost:NO];
  EXPECT_TRUE(callback_called);
}

// Tests allowing repost.
TEST_F(RepostFormTabHelperTest, AllowRepost) {
  ASSERT_FALSE(delegate_.presentingDialog);
  __block bool callback_called = false;
  tab_helper()->PresentDialog(location_, base::BindOnce(^(bool repost) {
                                EXPECT_TRUE(repost);
                                callback_called = true;
                              }));
  EXPECT_TRUE(delegate_.presentingDialog);

  ASSERT_FALSE(callback_called);
  [delegate_ allowRepost:YES];
  EXPECT_TRUE(callback_called);
}

// Tests that dialog is dismissed when WebState is destroyed.
TEST_F(RepostFormTabHelperTest, DismissingOnWebStateDestruction) {
  ASSERT_FALSE(delegate_.presentingDialog);
  tab_helper()->PresentDialog(location_, base::BindOnce(&IgnoreBool));
  EXPECT_TRUE(delegate_.presentingDialog);
  web_state_.reset();
  EXPECT_FALSE(delegate_.presentingDialog);
}

// Tests that dialog is dismissed after provisional navigation has started.
TEST_F(RepostFormTabHelperTest, DismissingOnNavigationStart) {
  ASSERT_FALSE(delegate_.presentingDialog);
  tab_helper()->PresentDialog(location_, base::BindOnce(&IgnoreBool));
  EXPECT_TRUE(delegate_.presentingDialog);
  web_state_->OnNavigationStarted(nullptr /* navigation_context */);
  EXPECT_FALSE(delegate_.presentingDialog);
}
