// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/ui_bundled/cells/central_account_view.h"

#import <CoreGraphics/CoreGraphics.h>
#import <UIKit/UIKit.h>

#import "base/strings/sys_string_conversions.h"
#import "base/test/scoped_feature_list.h"
#import "components/subscription_eligibility/ai_subscription_tier_names.h"
#import "ios/chrome/browser/authentication/ui_bundled/cells/ai_subscription_chip_constants.h"
#import "ios/chrome/browser/authentication/ui_bundled/cells/signin_promo_view_constants.h"
#import "ios/chrome/browser/signin/model/constants.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/signin_util.h"
#import "ios/chrome/common/ui/util/image_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/app/uikit_test_util.h"
#import "ios/public/provider/chrome/browser/signin/signin_resources_api.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// Returns the avatar image displayed in `account_view`.
UIImage* GetAvatarImage(CentralAccountView* account_view) {
  UIImageView* avatar_image_view = chrome_test_util::FindViewById<UIImageView>(
      account_view, kIdentityAvatarImageAccessibilityIdentifier);
  return avatar_image_view.image;
}

// Returns the title text displayed in `account_view`.
NSString* GetTitle(CentralAccountView* account_view) {
  UILabel* title_label = chrome_test_util::FindViewById<UILabel>(
      account_view, kCentralAccountViewTitleAccessibilityIdentifier);
  return title_label.text;
}

// Returns the subtitle text displayed in `account_view`.
NSString* GetSubtitle(CentralAccountView* account_view) {
  UILabel* subtitle_label = chrome_test_util::FindViewById<UILabel>(
      account_view, kCentralAccountViewSubtitleAccessibilityIdentifier);
  return subtitle_label.text;
}

// Returns the management description displayed in `account_view`, or nil if
// the view is not managed.
NSString* GetManagementDescription(CentralAccountView* account_view) {
  UILabel* management_label = chrome_test_util::FindViewById<UILabel>(
      account_view,
      kCentralAccountViewManagementDescriptionAccessibilityIdentifier);
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

  EXPECT_NE(chrome_test_util::FindViewById(
                accountView, kPremiumAvatarRingAccessibilityIdentifier),
            nil);

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

  EXPECT_NE(chrome_test_util::FindViewById(accountView, kAISubscriptionChipId),
            nil);
}

// Tests accessibility labels when AI tier is present.
TEST_F(CentralAccountViewTest, AccessibilityLabelsWithAITier) {
  UIImage* image = ios::provider::GetSigninDefaultAvatar();
  NSString* name = @"Jessica";
  NSString* email = @"jessica@gmail.com";
  NSString* managementDescription = @"Managed by Google";
  const std::u16string aiTierFullName =
      subscription_eligibility::GetAiSubscriptionTierDisplayInfo(1)
          .full_membership_label;
  ASSERT_FALSE(aiTierFullName.empty());

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
            aiTierFullName)));
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
            aiTierFullName)));
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
            aiTierFullName)));
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
            aiTierFullName)));
    EXPECT_NSEQ(accountView.accessibilityLabel, expectedLabel);
  }
}

// Test fixture enabling `kAiSubscriptionTierOverrides` with `overrides`.
class CentralAccountViewAiTierOverridesTest : public PlatformTest {
 protected:
  void EnableOverrides(const std::string& overrides) {
    feature_list_.InitAndEnableFeatureWithParameters(
        subscription_eligibility::kAiSubscriptionTierOverrides,
        {{subscription_eligibility::kAiTierOverrides.name, overrides}});
    subscription_eligibility::ResetAiTierOverridesForTesting();
  }

  void TearDown() override {
    feature_list_.Reset();
    subscription_eligibility::ResetAiTierOverridesForTesting();
    PlatformTest::TearDown();
  }

  CentralAccountView* CreateView(NSInteger tier) {
    return [[CentralAccountView alloc]
                initWithFrame:CGRectMake(0, 0, 100, 100)
                  avatarImage:ios::provider::GetSigninDefaultAvatar()
              displayedAiTier:tier
                         name:nil
                        email:@"jessica@gmail.com"
        managementDescription:nil
              useLargeMargins:YES];
  }

  base::test::ScopedFeatureList feature_list_;
};

// Tests that an excluded tier shows no ring, no chip and no AI tier
// accessibility label.
TEST_F(CentralAccountViewAiTierOverridesTest, ExcludedTier) {
  EnableOverrides(R"({"excluded_tiers": [1]})");
  CentralAccountView* accountView = CreateView(1);

  EXPECT_EQ(chrome_test_util::FindViewById(
                accountView, kPremiumAvatarRingAccessibilityIdentifier),
            nil);
  EXPECT_EQ(chrome_test_util::FindViewById(accountView, kAISubscriptionChipId),
            nil);
  EXPECT_NSEQ(accountView.accessibilityLabel, @"jessica@gmail.com");
}

// Tests that the overridden names of a tier are used.
TEST_F(CentralAccountViewAiTierOverridesTest, OverriddenNames) {
  EnableOverrides(
      R"({"tier_names": {"4": {"short_name": "Max", "full_name": "AI Max"}}})");
  CentralAccountView* accountView = CreateView(4);

  EXPECT_NE(chrome_test_util::FindViewById(
                accountView, kPremiumAvatarRingAccessibilityIdentifier),
            nil);
  EXPECT_NE(chrome_test_util::FindViewById(accountView, kAISubscriptionChipId),
            nil);
  NSString* expectedLabel = l10n_util::GetNSStringF(
      IDS_IOS_ACCOUNT_VIEW_ACCESSIBILITY_LABEL_AI_TIER, u"jessica@gmail.com",
      l10n_util::GetStringFUTF16(
          IDS_IOS_ACCOUNT_VIEW_ACCESSIBILITY_LABEL_MEMBERSHIPS_AI_TIER,
          u"AI Max"));
  EXPECT_NSEQ(accountView.accessibilityLabel, expectedLabel);
}

// Tests that a tier with empty overridden names shows the ring without a chip,
// and no AI tier accessibility label.
TEST_F(CentralAccountViewAiTierOverridesTest, EmptyNames) {
  EnableOverrides(
      R"({"tier_names": {"1": {"short_name": "", "full_name": ""}}})");
  CentralAccountView* accountView = CreateView(1);

  EXPECT_NE(chrome_test_util::FindViewById(
                accountView, kPremiumAvatarRingAccessibilityIdentifier),
            nil);
  EXPECT_EQ(chrome_test_util::FindViewById(accountView, kAISubscriptionChipId),
            nil);
  EXPECT_NSEQ(accountView.accessibilityLabel, @"jessica@gmail.com");
}
