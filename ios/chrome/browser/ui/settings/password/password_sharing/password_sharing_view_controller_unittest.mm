// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_sharing/password_sharing_view_controller.h"

#import "ios/chrome/browser/ui/settings/password/password_sharing/password_sharing_view_controller_presentation_delegate.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

class PasswordSharingViewControllerTest : public PlatformTest {};

TEST_F(PasswordSharingViewControllerTest, CancelButtonCallsDelegateDismiss) {
  PasswordSharingViewController* view_controller =
      [[PasswordSharingViewController alloc] init];
  id delegate = OCMStrictProtocolMock(
      @protocol(PasswordSharingViewControllerPresentationDelegate));
  view_controller.delegate = delegate;
  [view_controller loadViewIfNeeded];

  UIBarButtonItem* cancelButton =
      view_controller.navigationItem.leftBarButtonItem;
  ASSERT_TRUE(cancelButton);

  OCMExpect([delegate sharingSpinnerViewWasDismissed:view_controller]);
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Warc-performSelector-leaks"
  [cancelButton.target performSelector:cancelButton.action
                            withObject:cancelButton];
#pragma clang diagnostic pop
  EXPECT_OCMOCK_VERIFY(delegate);
}
