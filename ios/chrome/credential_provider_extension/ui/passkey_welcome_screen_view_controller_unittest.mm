// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/credential_provider_extension/ui/passkey_welcome_screen_view_controller.h"

#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

class PasskeyWelcomeScreenViewControllerTest : public PlatformTest {
 public:
  // Creates a PasskeyWelcomeScreenViewController for the provided purpose.
  PasskeyWelcomeScreenViewController* CreateController(
      PasskeyWelcomeScreenPurpose purpose) {
    return [[PasskeyWelcomeScreenViewController alloc]
                 initForPurpose:purpose
        navigationItemTitleView:[[UIView alloc] init]
                      userEmail:@"peter.parker@gmail.com"
                       delegate:nil
            primaryButtonAction:nil];
  }
};

// Tests that the view's content for the `kEnroll` purpose is as expected.
TEST_F(PasskeyWelcomeScreenViewControllerTest,
       TestContentForEnrollmentPurpose) {
  PasskeyWelcomeScreenViewController* controller =
      CreateController(PasskeyWelcomeScreenPurpose::kEnroll);
  [controller viewDidLoad];

  EXPECT_TRUE(controller.navigationItem.titleView);
  EXPECT_NSEQ(controller.bannerName, @"passkey_generic_banner");
  EXPECT_EQ(controller.bannerSize, BannerImageSizeType::kExtraShort);
  EXPECT_NSEQ(controller.titleText,
              @"IDS_IOS_CREDENTIAL_PROVIDER_PASSKEY_ENROLLMENT_TITLE");
  EXPECT_FALSE(controller.subtitleText);
  EXPECT_EQ(controller.specificContentView.subviews.count, 2u);
  EXPECT_NSEQ(controller.primaryActionString,
              @"IDS_IOS_CREDENTIAL_PROVIDER_GET_STARTED_BUTTON");
  EXPECT_NSEQ(controller.secondaryActionString,
              @"IDS_IOS_CREDENTIAL_PROVIDER_NOT_NOW_BUTTON");
  EXPECT_NSEQ(controller.view.backgroundColor,
              [UIColor colorNamed:kPrimaryBackgroundColor]);
}

// Tests that the view's content for the `kFixDegradedRecoverability` purpose is
// as expected.
TEST_F(PasskeyWelcomeScreenViewControllerTest,
       TestContentForFixDegradedRecoverabilityPurpose) {
  PasskeyWelcomeScreenViewController* controller =
      CreateController(PasskeyWelcomeScreenPurpose::kFixDegradedRecoverability);
  [controller viewDidLoad];

  EXPECT_TRUE(controller.navigationItem.titleView);
  EXPECT_NSEQ(controller.bannerName, @"passkey_generic_banner");
  EXPECT_EQ(controller.bannerSize, BannerImageSizeType::kExtraShort);
  EXPECT_NSEQ(
      controller.titleText,
      @"IDS_IOS_CREDENTIAL_PROVIDER_PASSKEY_PARTIAL_BOOTSRAPPING_TITLE");
  EXPECT_NSEQ(
      controller.subtitleText,
      @"IDS_IOS_CREDENTIAL_PROVIDER_PASSKEY_PARTIAL_BOOTSRAPPING_SUBTITLE");
  EXPECT_EQ(controller.specificContentView.subviews.count, 0u);
  EXPECT_NSEQ(controller.primaryActionString,
              @"IDS_IOS_CREDENTIAL_PROVIDER_GET_STARTED_BUTTON");
  EXPECT_NSEQ(controller.secondaryActionString,
              @"IDS_IOS_CREDENTIAL_PROVIDER_NOT_NOW_BUTTON");
  EXPECT_NSEQ(controller.view.backgroundColor,
              [UIColor colorNamed:kPrimaryBackgroundColor]);
}

// Tests that the view's content for the `kReauthenticate` purpose is as
// expected.
TEST_F(PasskeyWelcomeScreenViewControllerTest,
       TestContentForkReauthenticationPurpose) {
  PasskeyWelcomeScreenViewController* controller =
      CreateController(PasskeyWelcomeScreenPurpose::kReauthenticate);
  [controller viewDidLoad];

  EXPECT_TRUE(controller.navigationItem.titleView);
  EXPECT_NSEQ(controller.bannerName, @"passkey_bootstrapping_banner");
  EXPECT_EQ(controller.bannerSize, BannerImageSizeType::kExtraShort);
  EXPECT_NSEQ(controller.titleText,
              @"IDS_IOS_CREDENTIAL_PROVIDER_PASSKEY_BOOTSRAPPING_TITLE");
  EXPECT_NSEQ(controller.subtitleText,
              @"IDS_IOS_CREDENTIAL_PROVIDER_PASSKEY_BOOTSRAPPING_SUBTITLE");
  EXPECT_EQ(controller.specificContentView.subviews.count, 0u);
  EXPECT_NSEQ(controller.primaryActionString,
              @"IDS_IOS_CREDENTIAL_PROVIDER_NEXT_BUTTON");
  EXPECT_NSEQ(controller.secondaryActionString,
              @"IDS_IOS_CREDENTIAL_PROVIDER_NOT_NOW_BUTTON");
  EXPECT_NSEQ(controller.view.backgroundColor,
              [UIColor colorNamed:kPrimaryBackgroundColor]);
}
