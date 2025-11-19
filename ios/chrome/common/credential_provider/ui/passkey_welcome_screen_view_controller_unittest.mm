// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/credential_provider/ui/passkey_welcome_screen_view_controller.h"

#import "base/test/task_environment.h"
#import "ios/chrome/common/credential_provider/ui/passkey_welcome_screen_strings.h"
#import "ios/chrome/common/ui/button_stack/button_stack_configuration.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

class PasskeyWelcomeScreenViewControllerTest : public PlatformTest {
 public:
  PasskeyWelcomeScreenStrings* strings = [[PasskeyWelcomeScreenStrings alloc]
        initWithTitle:@"title"
             subtitle:@"subtitle"
               footer:@"footer"
        primaryButton:@"primaryButton"
      secondaryButton:@"secondaryButton"
         instructions:@[ @"step1", @"step2" ]];

  // Creates a PasskeyWelcomeScreenViewController for the provided purpose.
  PasskeyWelcomeScreenViewController* CreateController(
      PasskeyWelcomeScreenPurpose purpose) {
    return [[PasskeyWelcomeScreenViewController alloc]
                 initForPurpose:purpose
        navigationItemTitleView:[[UIView alloc] init]
                       delegate:nil
            primaryButtonAction:nil
                        strings:strings];
  }

 private:
  base::test::TaskEnvironment task_environment_;
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
  EXPECT_NSEQ(controller.titleText, @"title");
  EXPECT_FALSE(controller.subtitleText);
  EXPECT_EQ(controller.specificContentView.subviews.count, 2u);
  EXPECT_NSEQ(controller.configuration.primaryActionString, @"primaryButton");
  EXPECT_NSEQ(controller.configuration.secondaryActionString,
              @"secondaryButton");
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
  EXPECT_NSEQ(controller.titleText, @"title");
  EXPECT_NSEQ(controller.subtitleText, @"subtitle");
  EXPECT_EQ(controller.specificContentView.subviews.count, 0u);
  EXPECT_NSEQ(controller.configuration.primaryActionString, @"primaryButton");
  EXPECT_NSEQ(controller.configuration.secondaryActionString,
              @"secondaryButton");
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
  EXPECT_NSEQ(controller.titleText, @"title");
  EXPECT_NSEQ(controller.subtitleText, @"subtitle");
  EXPECT_EQ(controller.specificContentView.subviews.count, 0u);
  EXPECT_NSEQ(controller.configuration.primaryActionString, @"primaryButton");
  EXPECT_NSEQ(controller.configuration.secondaryActionString,
              @"secondaryButton");
  EXPECT_NSEQ(controller.view.backgroundColor,
              [UIColor colorNamed:kPrimaryBackgroundColor]);
}
