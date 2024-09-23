// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_header_view_controller.h"

#import "base/strings/sys_string_conversions.h"
#import "base/test/scoped_feature_list.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_feature.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_header_view.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_header_view_controller+Testing.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/ui/content_suggestions/ntp_home_constant.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_variations_service.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "ui/base/test/ios/ui_image_test_utils.h"

using ui::test::uiimage_utils::UIImageWithSizeAndSolidColor;

// Tests for the header view controller of the NTP.
class NewTabPageHeaderViewControllerUnitTest : public PlatformTest {
 public:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures({kHomeCustomization}, {});

    view_controller_ = [[NewTabPageHeaderViewController alloc]
        initWithUseNewBadgeForLensButton:YES
         useNewBadgeForCustomizationMenu:YES];
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  NewTabPageHeaderViewController* view_controller_;
};

// Tests the header view when the user is signed out.
TEST_F(NewTabPageHeaderViewControllerUnitTest, TestSignedOut) {
  [view_controller_ loadViewIfNeeded];

  EXPECT_NE(nil, view_controller_.identityDiscButton);
  EXPECT_NE(nil, view_controller_.headerView.customizationMenuButton);

  // Checks that the identity disc's label is correctly set when
  // `setSignedOutAccountImage` is called, which is triggered by the mediator
  // after checking sign-in status.
  [view_controller_ setSignedOutAccountImage];
  EXPECT_NSEQ(view_controller_.identityDiscButton.accessibilityLabel,
              l10n_util::GetNSString(
                  IDS_IOS_IDENTITY_DISC_SIGNED_OUT_ACCESSIBILITY_LABEL));
  EXPECT_NE(nil, view_controller_.identityDiscImage);
}

// Tests the header view when the user is signed in.
TEST_F(NewTabPageHeaderViewControllerUnitTest, TestSignedIn) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(kIdentityDiscAccountMenu);
  [view_controller_ loadViewIfNeeded];

  EXPECT_NE(nil, view_controller_.identityDiscButton);
  EXPECT_NE(nil, view_controller_.headerView.customizationMenuButton);

  // Checks that the identity disc's label is correctly set when
  // `updateAccountImage:name:email:` is called, which is triggered by the
  // mediator after checking sign-in status.
  UIImage* someIdentityImage = UIImageWithSizeAndSolidColor(
      CGSizeMake(ntp_home::kIdentityAvatarDimension,
                 ntp_home::kIdentityAvatarDimension),
      [UIColor redColor]);
  [view_controller_ updateAccountImage:someIdentityImage
                                  name:@"Some name"
                                 email:@"someemail"];
  EXPECT_NSEQ(view_controller_.identityDiscButton.accessibilityLabel,
              l10n_util::GetNSStringF(IDS_IOS_IDENTITY_DISC_WITH_NAME_AND_EMAIL,
                                      base::SysNSStringToUTF16(@"Some name"),
                                      base::SysNSStringToUTF16(@"someemail")));
  EXPECT_NE(nil, view_controller_.identityDiscImage);
}

// Tests the header view when the user is signed in.
TEST_F(NewTabPageHeaderViewControllerUnitTest, TestSignedIn_AccountMenu) {
  base::test::ScopedFeatureList scoped_feature_list{kIdentityDiscAccountMenu};

  [view_controller_ loadViewIfNeeded];

  EXPECT_NE(nil, view_controller_.identityDiscButton);
  EXPECT_NE(nil, view_controller_.headerView.customizationMenuButton);

  // Checks that the identity disc's label is correctly set when
  // `updateAccountImage:name:email:` is called, which is triggered by the
  // mediator after checking sign-in status.
  UIImage* someIdentityImage = UIImageWithSizeAndSolidColor(
      CGSizeMake(ntp_home::kIdentityAvatarDimension,
                 ntp_home::kIdentityAvatarDimension),
      [UIColor redColor]);
  [view_controller_ updateAccountImage:someIdentityImage
                                  name:@"Some name"
                                 email:@"someemail"];
  EXPECT_NSEQ(view_controller_.identityDiscButton.accessibilityLabel,
              l10n_util::GetNSStringF(
                  IDS_IOS_IDENTITY_DISC_WITH_NAME_AND_EMAIL_OPEN_ACCOUNT_MENU,
                  base::SysNSStringToUTF16(@"Some name"),
                  base::SysNSStringToUTF16(@"someemail")));
  EXPECT_NE(nil, view_controller_.identityDiscImage);
}
