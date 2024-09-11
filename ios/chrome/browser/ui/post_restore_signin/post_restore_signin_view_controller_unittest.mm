// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/post_restore_signin/post_restore_signin_view_controller.h"

#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "ui/base/device_form_factor.h"

namespace {
const char kFakePreRestoreAccountEmail[] = "person@example.org";
const char kFakePreRestoreAccountGivenName[] = "Given";
const char kFakePreRestoreAccountFullName[] = "Full Name";
}  // namespace

// Tests the PostRestoreSignInProvider.
class PostRestoreSignInViewControllerTest : public PlatformTest {
 public:
  explicit PostRestoreSignInViewControllerTest() {
    view_controller_ = [[PostRestoreSignInViewController alloc]
        initWithAccountInfo:FakeAccountInfo()];
  }

  void ClearUserName() {
    view_controller_ = [[PostRestoreSignInViewController alloc]
        initWithAccountInfo:FakeAccountInfoWithoutName()];
  }

  AccountInfo FakeAccountInfo() {
    AccountInfo accountInfo;
    accountInfo.email = std::string(kFakePreRestoreAccountEmail);
    accountInfo.given_name = std::string(kFakePreRestoreAccountGivenName);
    accountInfo.full_name = std::string(kFakePreRestoreAccountFullName);
    return accountInfo;
  }

  AccountInfo FakeAccountInfoWithoutName() {
    AccountInfo accountInfo;
    accountInfo.email = std::string(kFakePreRestoreAccountEmail);
    return accountInfo;
  }

  PostRestoreSignInViewController* view_controller_;
};

TEST_F(PostRestoreSignInViewControllerTest, uiStrings) {
  [view_controller_ loadView];
  EXPECT_NSEQ(@"Welcome back, Given", view_controller_.titleText);
  EXPECT_NSEQ(@"Continue as Given", view_controller_.primaryActionString);
  EXPECT_NSEQ(@"Don't sign in", view_controller_.secondaryActionString);

  NSString* expectedDisclaimer;
  if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET) {
    expectedDisclaimer = @"You were signed out as part of your iPad reset. Tap "
                         @"continue below to sign in.";
  } else {
    expectedDisclaimer = @"You were signed out as part of your iPhone reset. "
                         @"Tap continue below to sign in.";
  }
  EXPECT_NSEQ(view_controller_.disclaimerText, expectedDisclaimer);
}

TEST_F(PostRestoreSignInViewControllerTest, uiStringsWithoutName) {
  ClearUserName();
  [view_controller_ loadView];
  EXPECT_NSEQ(@"Welcome back", view_controller_.titleText);
  EXPECT_NSEQ(@"Continue", view_controller_.primaryActionString);
}
