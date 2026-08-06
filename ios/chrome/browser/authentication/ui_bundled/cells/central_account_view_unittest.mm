// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/ui_bundled/cells/central_account_view.h"

#import <CoreGraphics/CoreGraphics.h>
#import <UIKit/UIKit.h>

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/policy/model/management_state.h"
#import "ios/chrome/browser/signin/model/constants.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/signin_util.h"
#import "ios/chrome/common/ui/util/image_util.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/signin/signin_resources_api.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/l10n/l10n_util_mac.h"

using CentralAccountViewTest = PlatformTest;

// Tests that the UIImageView and UILabels are set properly in the view.
TEST_F(CentralAccountViewTest, ImageViewAndTextLabels) {
  UIImage* image = ios::provider::GetSigninDefaultAvatar();
  image = ResizeImage(image,
                      GetSizeForIdentityAvatarSize(IdentityAvatarSize::Large),
                      ProjectionMode::kAspectFit);
  NSString* mainText = @"Main text";
  NSString* detailText = @"Detail text";

  CentralAccountView* accountView =
      [[CentralAccountView alloc] initWithFrame:CGRectMake(0, 0, 100, 100)
                                    avatarImage:image
                                showsAITierRing:NO
                                 aiTierFullName:nil
                                           name:mainText
                                          email:detailText
                          managementDescription:nil
                                useLargeMargins:YES];

  EXPECT_NSEQ(accountView.avatarImage, image);
  EXPECT_NSEQ(accountView.name, mainText);
  EXPECT_NSEQ(accountView.email, detailText);
  EXPECT_EQ(accountView.managed, false);
  EXPECT_NSEQ([accountView managementDescription], nil);
}

// Tests that the UIImageView and UILabels are set properly in the view if the
// account name is not provided.
TEST_F(CentralAccountViewTest, ImageViewAndTextLabelsWithoutGivenName) {
  UIImage* image = ios::provider::GetSigninDefaultAvatar();
  image = ResizeImage(image,
                      GetSizeForIdentityAvatarSize(IdentityAvatarSize::Large),
                      ProjectionMode::kAspectFit);
  NSString* mainText = @"Main text";

  CentralAccountView* accountView =
      [[CentralAccountView alloc] initWithFrame:CGRectMake(0, 0, 100, 100)
                                    avatarImage:image
                                showsAITierRing:NO
                                 aiTierFullName:nil
                                           name:nil
                                          email:mainText
                          managementDescription:nil
                                useLargeMargins:YES];

  EXPECT_NSEQ(accountView.avatarImage, image);
  EXPECT_NSEQ(accountView.name, mainText);
  EXPECT_NSEQ(accountView.email, nil);
  EXPECT_EQ(accountView.managed, false);
  EXPECT_NSEQ([accountView managementDescription], nil);
}

// Tests that the UIImageView and UILabels are set properly in the view if the
// machine policy domain is provided.
TEST_F(CentralAccountViewTest,
       ImageViewAndTextLabelsWithManagementDescription) {
  UIImage* image = ios::provider::GetSigninDefaultAvatar();
  image = ResizeImage(image,
                      GetSizeForIdentityAvatarSize(IdentityAvatarSize::Large),
                      ProjectionMode::kAspectFit);
  NSString* mainText = @"Main text";
  NSString* detailText = @"Detail text";
  NSString* managementDescription = @"A management label";

  CentralAccountView* accountView =
      [[CentralAccountView alloc] initWithFrame:CGRectMake(0, 0, 100, 100)
                                    avatarImage:image
                                showsAITierRing:NO
                                 aiTierFullName:nil
                                           name:mainText
                                          email:detailText
                          managementDescription:managementDescription
                                useLargeMargins:YES];

  EXPECT_NSEQ(accountView.avatarImage, image);
  EXPECT_NSEQ(accountView.name, mainText);
  EXPECT_NSEQ(accountView.email, detailText);
  EXPECT_EQ(accountView.managed, true);
  EXPECT_NSEQ([accountView managementDescription], managementDescription);
}

// Tests that the UIImageView and UILabels are set properly in the view if the
// account given name is missing.
TEST_F(CentralAccountViewTest, ImageViewAndTextLabelsWithMissingGivenName) {
  UIImage* image = ios::provider::GetSigninDefaultAvatar();
  image = ResizeImage(image,
                      GetSizeForIdentityAvatarSize(IdentityAvatarSize::Large),
                      ProjectionMode::kAspectFit);

  FakeSystemIdentity* identity =
      [FakeSystemIdentity fakeIdentityWithMissingGivenName];

  CentralAccountView* accountView =
      [[CentralAccountView alloc] initWithFrame:CGRectMake(0, 0, 100, 100)
                                    avatarImage:image
                                showsAITierRing:NO
                                 aiTierFullName:nil
                                           name:identity.userFullName
                                          email:identity.userEmail
                          managementDescription:nil
                                useLargeMargins:YES];

  EXPECT_NSEQ(accountView.avatarImage, image);
  EXPECT_NSEQ(accountView.name, identity.userFullName);
  EXPECT_NSEQ(accountView.email, identity.userEmail);
  EXPECT_EQ(accountView.managed, false);
}

// Tests that the UIImageView and UILabels are set properly in the view if both
// names are missing.
TEST_F(CentralAccountViewTest, ImageViewAndTextLabelsWithMissingNames) {
  UIImage* image = ios::provider::GetSigninDefaultAvatar();
  image = ResizeImage(image,
                      GetSizeForIdentityAvatarSize(IdentityAvatarSize::Large),
                      ProjectionMode::kAspectFit);

  FakeSystemIdentity* identity =
      [FakeSystemIdentity fakeIdentityWithMissingNames];

  CentralAccountView* accountView =
      [[CentralAccountView alloc] initWithFrame:CGRectMake(0, 0, 100, 100)
                                    avatarImage:image
                                showsAITierRing:NO
                                 aiTierFullName:nil
                                           name:identity.userFullName
                                          email:identity.userEmail
                          managementDescription:nil
                                useLargeMargins:YES];

  EXPECT_NSEQ(accountView.avatarImage, image);
  EXPECT_NSEQ(accountView.name, identity.userEmail);
  EXPECT_NSEQ(accountView.email, nil);
  EXPECT_EQ(accountView.managed, false);
}

// Tests that the UIImageView and UILabels are set properly in the view if the
// AI tier ring is shown.
TEST_F(CentralAccountViewTest, ImageViewAndTextLabelsWithAITierRing) {
  UIImage* image = ios::provider::GetSigninDefaultAvatar();
  image = ResizeImage(image,
                      GetSizeForIdentityAvatarSize(IdentityAvatarSize::Large),
                      ProjectionMode::kAspectFit);
  NSString* mainText = @"Main text";
  NSString* detailText = @"Detail text";

  CentralAccountView* accountView =
      [[CentralAccountView alloc] initWithFrame:CGRectMake(0, 0, 100, 100)
                                    avatarImage:image
                                showsAITierRing:YES
                                 aiTierFullName:nil
                                           name:mainText
                                          email:detailText
                          managementDescription:nil
                                useLargeMargins:YES];

  UIView* avatarView = accountView.avatarView;
  EXPECT_TRUE([avatarView isDescendantOfView:accountView]);
  EXPECT_EQ(avatarView.subviews.count, 2u);

  BOOL foundAvatarImage = NO;
  BOOL foundPremiumRing = NO;
  for (UIView* subview in avatarView.subviews) {
    if ([subview isKindOfClass:[UIImageView class]]) {
      UIImageView* imageView = (UIImageView*)subview;
      if (imageView.image == image) {
        foundAvatarImage = YES;
      } else if ([imageView.accessibilityIdentifier
                     isEqualToString:
                         kPremiumAvatarRingAccessibilityIdentifier]) {
        foundPremiumRing = YES;
      }
    }
  }
  EXPECT_TRUE(foundAvatarImage);
  EXPECT_TRUE(foundPremiumRing);

  EXPECT_NSEQ(accountView.avatarImage, image);
  EXPECT_NSEQ(accountView.name, mainText);
  EXPECT_NSEQ(accountView.email, detailText);
  EXPECT_EQ(accountView.managed, false);
}

// Tests accessibility labels when AI tier is present.
TEST_F(CentralAccountViewTest, AccessibilityLabelsWithAITier) {
  UIImage* image = ios::provider::GetSigninDefaultAvatar();
  NSString* name = @"Jessica";
  NSString* email = @"jessica@gmail.com";
  NSString* managementDescription = @"Managed by Google";
  NSString* aiTierFullName = @"Premium";

  // Case 1: name, email, managed, AI tier.
  {
    CentralAccountView* accountView =
        [[CentralAccountView alloc] initWithFrame:CGRectMake(0, 0, 100, 100)
                                      avatarImage:image
                                  showsAITierRing:YES
                                   aiTierFullName:aiTierFullName
                                             name:name
                                            email:email
                            managementDescription:managementDescription
                                  useLargeMargins:YES];
    NSString* expectedLabel = l10n_util::GetNSStringF(
        IDS_IOS_ACCOUNT_VIEW_ACCESSIBILITY_LABEL_NAME_MANAGED_STATUS_AI_TIER,
        base::SysNSStringToUTF16(name), base::SysNSStringToUTF16(email),
        base::SysNSStringToUTF16(managementDescription),
        base::SysNSStringToUTF16(l10n_util::GetNSStringF(
            IDS_IOS_ACCOUNT_VIEW_ACCESSIBILITY_LABEL_MEMBERSHIPS_AI_TIER,
            base::SysNSStringToUTF16(aiTierFullName))));
    EXPECT_NSEQ(accountView.accessibilityLabel, expectedLabel);
  }

  // Case 2: name, email, not managed, AI tier.
  {
    CentralAccountView* accountView =
        [[CentralAccountView alloc] initWithFrame:CGRectMake(0, 0, 100, 100)
                                      avatarImage:image
                                  showsAITierRing:YES
                                   aiTierFullName:aiTierFullName
                                             name:name
                                            email:email
                            managementDescription:nil
                                  useLargeMargins:YES];
    NSString* expectedLabel = l10n_util::GetNSStringF(
        IDS_IOS_ACCOUNT_VIEW_ACCESSIBILITY_LABEL_NAME_AI_TIER,
        base::SysNSStringToUTF16(name), base::SysNSStringToUTF16(email),
        base::SysNSStringToUTF16(l10n_util::GetNSStringF(
            IDS_IOS_ACCOUNT_VIEW_ACCESSIBILITY_LABEL_MEMBERSHIPS_AI_TIER,
            base::SysNSStringToUTF16(aiTierFullName))));
    EXPECT_NSEQ(accountView.accessibilityLabel, expectedLabel);
  }

  // Case 3: no name, email, managed, AI tier.
  {
    CentralAccountView* accountView =
        [[CentralAccountView alloc] initWithFrame:CGRectMake(0, 0, 100, 100)
                                      avatarImage:image
                                  showsAITierRing:YES
                                   aiTierFullName:aiTierFullName
                                             name:nil
                                            email:email
                            managementDescription:managementDescription
                                  useLargeMargins:YES];
    NSString* expectedLabel = l10n_util::GetNSStringF(
        IDS_IOS_ACCOUNT_VIEW_ACCESSIBILITY_LABEL_MANAGED_STATUS_AI_TIER,
        base::SysNSStringToUTF16(email),
        base::SysNSStringToUTF16(managementDescription),
        base::SysNSStringToUTF16(l10n_util::GetNSStringF(
            IDS_IOS_ACCOUNT_VIEW_ACCESSIBILITY_LABEL_MEMBERSHIPS_AI_TIER,
            base::SysNSStringToUTF16(aiTierFullName))));
    EXPECT_NSEQ(accountView.accessibilityLabel, expectedLabel);
  }

  // Case 4: no name, email, not managed, AI tier.
  {
    CentralAccountView* accountView =
        [[CentralAccountView alloc] initWithFrame:CGRectMake(0, 0, 100, 100)
                                      avatarImage:image
                                  showsAITierRing:YES
                                   aiTierFullName:aiTierFullName
                                             name:nil
                                            email:email
                            managementDescription:nil
                                  useLargeMargins:YES];
    NSString* expectedLabel = l10n_util::GetNSStringF(
        IDS_IOS_ACCOUNT_VIEW_ACCESSIBILITY_LABEL_AI_TIER,
        base::SysNSStringToUTF16(email),
        base::SysNSStringToUTF16(l10n_util::GetNSStringF(
            IDS_IOS_ACCOUNT_VIEW_ACCESSIBILITY_LABEL_MEMBERSHIPS_AI_TIER,
            base::SysNSStringToUTF16(aiTierFullName))));
    EXPECT_NSEQ(accountView.accessibilityLabel, expectedLabel);
  }
}
