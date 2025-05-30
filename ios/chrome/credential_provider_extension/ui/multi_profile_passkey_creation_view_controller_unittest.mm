// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/credential_provider_extension/ui/multi_profile_passkey_creation_view_controller.h"

#import <AuthenticationServices/AuthenticationServices.h>

#import "base/test/task_environment.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/credential_provider_extension/passkey_request_details+Testing.h"
#import "ios/chrome/credential_provider_extension/ui/feature_flags.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

class MultiProfilePasskeyCreationViewControllerTest : public PlatformTest {
 public:
  // Creates a PasskeyWelcomeScreenViewController for the provided purpose.
  MultiProfilePasskeyCreationViewController* CreateController() {
    PasskeyRequestDetails* details =
        [[PasskeyRequestDetails alloc] initWithURL:@"example.com"
                                          username:@"username"
                               excludedCredentials:nil];
    UIView* navigationView = [[UIView alloc] init];
    return [[MultiProfilePasskeyCreationViewController alloc]
                initWithDetails:details
                           gaia:nil
                      userEmail:@"peter.parker@gmail.com"
                        favicon:nil
        navigationItemTitleView:navigationView
                       delegate:nil];
  }
};

// Tests that the view's content is as expected.
TEST_F(MultiProfilePasskeyCreationViewControllerTest,
       TestDefaultCreationParameters) {
  MultiProfilePasskeyCreationViewController* controller = CreateController();
  [controller viewDidLoad];

  EXPECT_TRUE(controller.navigationItem.titleView);
  EXPECT_NSEQ(controller.bannerName, @"passkey_generic_banner");
  EXPECT_EQ(controller.bannerSize, BannerImageSizeType::kExtraShort);
  EXPECT_NSEQ(controller.titleText,
              @"IDS_IOS_CREDENTIAL_PROVIDER_PASSKEYS_CREATE");
  EXPECT_FALSE(controller.subtitleText);
  EXPECT_EQ(controller.specificContentView.subviews.count, 2u);
  EXPECT_NSEQ(controller.primaryActionString,
              @"IDS_IOS_CREDENTIAL_PROVIDER_EXTENSION_CREATE");
  EXPECT_NSEQ(controller.secondaryActionString,
              @"IDS_IOS_CREDENTIAL_PROVIDER_EXTENSION_CANCEL");
  EXPECT_NSEQ(controller.view.backgroundColor,
              [UIColor colorNamed:kPrimaryBackgroundColor]);
}
