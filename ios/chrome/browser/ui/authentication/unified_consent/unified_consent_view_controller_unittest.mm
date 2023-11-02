// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/unified_consent/unified_consent_view_controller.h"

#import <memory>

#import "base/mac/foundation_util.h"
#import "ios/chrome/common/string_util.h"
#import "testing/platform_test.h"
#import "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

NSString* kTestName = @"test";
NSString* kTestEmail = @"test@gmail.com";

class UnifiedConsentViewControllerTest : public PlatformTest {
 public:
  void SetUp() override {
    view_controller_ = [[UnifiedConsentViewController alloc]
        initWithPostRestoreSigninPromo:NO];
    [view_controller_ view];
  }

  void TearDown() override { view_controller_ = nil; }

 protected:
  // Returns whether the link attribute is available in the Sync Settings view.
  BOOL ContainsSyncSettingsLinkAttributes() {
    UITextView* syncSettingsView = GetSyncSettingsView(view_controller().view);
    __block BOOL containsLink = NO;
    [syncSettingsView.attributedText
        enumerateAttribute:NSLinkAttributeName
                   inRange:NSMakeRange(0,
                                       syncSettingsView.attributedText.length)
                   options:
                       NSAttributedStringEnumerationLongestEffectiveRangeNotRequired
                usingBlock:^(id value, NSRange range, BOOL* stop) {
                  if (value != nil) {
                    containsLink = YES;
                  }
                }];

    return containsLink;
  }

  UnifiedConsentViewController* view_controller() { return view_controller_; }

 private:
  // Returns the first UITextView in the unified consent UI hierarchy.
  UITextView* GetSyncSettingsView(UIView* view) {
    UIScrollView* scroll_view =
        base::mac::ObjCCast<UIScrollView>([[view subviews] firstObject]);
    UIView* container =
        base::mac::ObjCCast<UIView>([[scroll_view subviews] firstObject]);

    for (UIView* subview in [container subviews]) {
      if ([subview isKindOfClass:[UITextView class]]) {
        return base::mac::ObjCCast<UITextView>(subview);
      }
    }
    return nil;
  }

  UnifiedConsentViewController* view_controller_;
};

// Tests that the advanced Settings link in the unified consent view is not
// present when there are no identities on the device.
TEST_F(UnifiedConsentViewControllerTest, NoIdentitiesOnDevice) {
  [view_controller() hideIdentityButtonControl];

  EXPECT_FALSE(ContainsSyncSettingsLinkAttributes());
}

// Tests that the advanced Settings link in the unified consent view is
// present after the user adds an identity to the device.
TEST_F(UnifiedConsentViewControllerTest, NoIdentitiesOnDeviceUserAddsOne) {
  [view_controller() hideIdentityButtonControl];
  [view_controller() updateIdentityButtonControlWithUserFullName:kTestName
                                                           email:kTestEmail];

  EXPECT_TRUE(ContainsSyncSettingsLinkAttributes());
}

// Tests that the advanced Settings link in the unified consent view is
// present where there is an identity on the device.
TEST_F(UnifiedConsentViewControllerTest, IdentitiesOnDevice) {
  [view_controller() updateIdentityButtonControlWithUserFullName:kTestName
                                                           email:kTestEmail];

  EXPECT_TRUE(ContainsSyncSettingsLinkAttributes());
}

// Tests that the advanced Settings link in the unified consent view is not
// present after the user removes an identity from the device.
TEST_F(UnifiedConsentViewControllerTest, IdentitiesOnDeviceUserRemovesAll) {
  [view_controller() updateIdentityButtonControlWithUserFullName:kTestName
                                                           email:kTestEmail];
  [view_controller() hideIdentityButtonControl];

  EXPECT_FALSE(ContainsSyncSettingsLinkAttributes());
}

}  // namespace
