// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/ui_bundled/cells/central_account_view.h"

#import <CoreGraphics/CoreGraphics.h>
#import <UIKit/UIKit.h>

#import "base/apple/foundation_util.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/authentication/ui_bundled/cells/ai_subscription_chip_constants.h"
#import "ios/chrome/browser/authentication/ui_bundled/cells/signin_promo_view_constants.h"
#import "ios/chrome/browser/policy/model/management_state.h"
#import "ios/chrome/browser/signin/model/constants.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/signin_util.h"
#import "ios/chrome/common/ui/util/image_util.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/intelligence/signin/signin_ai_logo.h"
#import "ios/public/provider/chrome/browser/signin/signin_resources_api.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// Recursively searches `view` and its subviews for a view with the given
// `accessibility_id`. Returns nil if not found.
UIView* FindSubviewWithAccessibilityIdentifier(UIView* view,
                                               NSString* accessibility_id) {
  if ([view.accessibilityIdentifier isEqualToString:accessibility_id]) {
    return view;
  }
  for (UIView* subview in view.subviews) {
    if (UIView* found =
            FindSubviewWithAccessibilityIdentifier(subview, accessibility_id)) {
      return found;
    }
  }
  return nil;
}

// Returns true if `view` or any of its recursive subviews has the given
// `accessibility_id`.
bool HasSubviewWithAccessibilityIdentifier(UIView* view,
                                           NSString* accessibility_id) {
  return FindSubviewWithAccessibilityIdentifier(view, accessibility_id) != nil;
}

// Returns the avatar image displayed in `account_view`.
UIImage* GetAvatarImage(CentralAccountView* account_view) {
  UIImageView* avatar_image_view = base::apple::ObjCCastStrict<UIImageView>(
      FindSubviewWithAccessibilityIdentifier(
          account_view, kIdentityAvatarImageAccessibilityIdentifier));
  return avatar_image_view.image;
}

// Returns the title text displayed in `account_view`.
NSString* GetTitle(CentralAccountView* account_view) {
  UILabel* title_label = base::apple::ObjCCastStrict<UILabel>(
      FindSubviewWithAccessibilityIdentifier(
          account_view, kCentralAccountViewTitleAccessibilityIdentifier));
  return title_label.text;
}

// Returns the subtitle text displayed in `account_view`.
NSString* GetSubtitle(CentralAccountView* account_view) {
  UILabel* subtitle_label = base::apple::ObjCCastStrict<UILabel>(
      FindSubviewWithAccessibilityIdentifier(
          account_view, kCentralAccountViewSubtitleAccessibilityIdentifier));
  return subtitle_label.text;
}

// Returns the management description displayed in `account_view`, or nil if
// the view is not managed.
NSString* GetManagementDescription(CentralAccountView* account_view) {
  UILabel* management_label = base::apple::ObjCCastStrict<UILabel>(
      FindSubviewWithAccessibilityIdentifier(
          account_view,
          kCentralAccountViewManagementDescriptionAccessibilityIdentifier));
  return management_label.text;
}

// Returns whether `account_view` displays a management description.
bool IsManaged(CentralAccountView* account_view) {
  return GetManagementDescription(account_view) != nil;
}

}  // namespace

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
                                displayedAiTier:0
                                           name:mainText
                                          email:detailText
                          managementDescription:nil
                                useLargeMargins:YES];

  EXPECT_NSEQ(GetAvatarImage(accountView), image);
  EXPECT_NSEQ(GetTitle(accountView), mainText);
  EXPECT_NSEQ(GetSubtitle(accountView), detailText);
  EXPECT_EQ(IsManaged(accountView), false);
  EXPECT_NSEQ(GetManagementDescription(accountView), nil);
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
                                displayedAiTier:0
                                           name:nil
                                          email:mainText
                          managementDescription:nil
                                useLargeMargins:YES];

  EXPECT_NSEQ(GetAvatarImage(accountView), image);
  EXPECT_NSEQ(GetTitle(accountView), mainText);
  EXPECT_NSEQ(GetSubtitle(accountView), nil);
  EXPECT_EQ(IsManaged(accountView), false);
  EXPECT_NSEQ(GetManagementDescription(accountView), nil);
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
                                displayedAiTier:0
                                           name:mainText
                                          email:detailText
                          managementDescription:managementDescription
                                useLargeMargins:YES];

  EXPECT_NSEQ(GetAvatarImage(accountView), image);
  EXPECT_NSEQ(GetTitle(accountView), mainText);
  EXPECT_NSEQ(GetSubtitle(accountView), detailText);
  EXPECT_EQ(IsManaged(accountView), true);
  EXPECT_NSEQ(GetManagementDescription(accountView), managementDescription);
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
                                displayedAiTier:0
                                           name:identity.userFullName
                                          email:identity.userEmail
                          managementDescription:nil
                                useLargeMargins:YES];

  EXPECT_NSEQ(GetAvatarImage(accountView), image);
  EXPECT_NSEQ(GetTitle(accountView), identity.userFullName);
  EXPECT_NSEQ(GetSubtitle(accountView), identity.userEmail);
  EXPECT_EQ(IsManaged(accountView), false);
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
                                displayedAiTier:0
                                           name:identity.userFullName
                                          email:identity.userEmail
                          managementDescription:nil
                                useLargeMargins:YES];

  EXPECT_NSEQ(GetAvatarImage(accountView), image);
  EXPECT_NSEQ(GetTitle(accountView), identity.userEmail);
  EXPECT_NSEQ(GetSubtitle(accountView), nil);
  EXPECT_EQ(IsManaged(accountView), false);
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
                                displayedAiTier:1
                                           name:mainText
                                          email:detailText
                          managementDescription:nil
                                useLargeMargins:YES];

  EXPECT_TRUE(HasSubviewWithAccessibilityIdentifier(
      accountView, kPremiumAvatarRingAccessibilityIdentifier));

  EXPECT_NSEQ(GetAvatarImage(accountView), image);
  EXPECT_NSEQ(GetTitle(accountView), mainText);
  EXPECT_NSEQ(GetSubtitle(accountView), detailText);
  EXPECT_EQ(IsManaged(accountView), false);
}

// Test that the AI subscription chip view is created and added when
// `aiTier` is positive.
TEST_F(CentralAccountViewTest, ImageViewAndTextLabelsWithAISubscriptionChip) {
  UIImage* image = ios::provider::GetSigninDefaultAvatar();
  image = ResizeImage(image,
                      GetSizeForIdentityAvatarSize(IdentityAvatarSize::Large),
                      ProjectionMode::kAspectFit);
  NSString* mainText = @"Main text";
  NSString* detailText = @"Detail text";

  CentralAccountView* accountView =
      [[CentralAccountView alloc] initWithFrame:CGRectMake(0, 0, 100, 100)
                                    avatarImage:image
                                displayedAiTier:1
                                           name:mainText
                                          email:detailText
                          managementDescription:nil
                                useLargeMargins:YES];

  EXPECT_TRUE(HasSubviewWithAccessibilityIdentifier(accountView,
                                                    kAISubscriptionChipId));
}

// Tests accessibility labels when AI tier is present.
TEST_F(CentralAccountViewTest, AccessibilityLabelsWithAITier) {
  UIImage* image = ios::provider::GetSigninDefaultAvatar();
  NSString* name = @"Jessica";
  NSString* email = @"jessica@gmail.com";
  NSString* managementDescription = @"Managed by Google";
  NSString* aiTierFullName = ios::provider::GetAITierFullName(1);

  // Case 1: name, email, managed, AI tier.
  {
    CentralAccountView* accountView =
        [[CentralAccountView alloc] initWithFrame:CGRectMake(0, 0, 100, 100)
                                      avatarImage:image
                                  displayedAiTier:1
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
                                  displayedAiTier:1
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
                                  displayedAiTier:1
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
                                  displayedAiTier:1
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
