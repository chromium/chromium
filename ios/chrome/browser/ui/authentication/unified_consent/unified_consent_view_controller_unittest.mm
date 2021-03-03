// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/unified_consent/unified_consent_view_controller.h"

#include <memory>

#include "ios/chrome/common/string_util.h"
#include "testing/platform_test.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

NSString* kTestName = @"test";
NSString* kTestEmail = @"test@gmail.com";

class UnifiedConsentViewControllerTest : public PlatformTest {
 public:
  void SetUp() override {
    view_controller_ = [[UnifiedConsentViewController alloc] init];
    [view_controller_ view];
  }

  void TearDown() override { view_controller_ = nil; }

 protected:
  // Returns all attributes associated with the Sync Settings view.
  NSDictionary* GetSyncSettingsViewAttributes() {
    UITextView* syncSettingsView = GetSyncSettingsView(view_controller().view);
    return [syncSettingsView.attributedText
        attributesAtIndex:GetSyncSettingsLinkLocation()
           effectiveRange:nullptr];
  }

  UnifiedConsentViewController* view_controller() { return view_controller_; }

 private:
  // Returns the first UITextView in the unified consent UI hierarchy.
  UITextView* GetSyncSettingsView(UIView* view) {
    UIScrollView* scroll_view = (UIScrollView*)[[view subviews] firstObject];
    UIView* container = (UIView*)[[scroll_view subviews] firstObject];

    for (UIView* subview in [container subviews]) {
      if ([subview isKindOfClass:[UITextView class]]) {
        return (UITextView*)subview;
      }
    }
    return nil;
  }

  // Returns the location of the "Open Settings" link.
  NSUInteger GetSyncSettingsLinkLocation() {
    NSRange range;
    ParseStringWithLink(
        l10n_util::GetNSString(view_controller_.openSettingsStringId), &range);
    return range.location;
  }

  UnifiedConsentViewController* view_controller_;
};

// Tests that the advanced Settings link in the unified consent view is not
// present when there are no identities on the device.
TEST_F(UnifiedConsentViewControllerTest, NoIdentitiesOnDevice) {
  [view_controller() hideIdentityPickerView];

  EXPECT_EQ(nil, GetSyncSettingsViewAttributes()[NSLinkAttributeName]);
}

// Tests that the advanced Settings link in the unified consent view is
// present after the user adds an identity to the device.
TEST_F(UnifiedConsentViewControllerTest, NoIdentitiesOnDeviceUserAddsOne) {
  [view_controller() hideIdentityPickerView];
  [view_controller() updateIdentityPickerViewWithUserFullName:kTestName
                                                        email:kTestEmail];

  EXPECT_NE(nil, GetSyncSettingsViewAttributes()[NSLinkAttributeName]);
}

// Tests that the advanced Settings link in the unified consent view is
// present where there is an identity on the device.
TEST_F(UnifiedConsentViewControllerTest, IdentitiesOnDevice) {
  [view_controller() updateIdentityPickerViewWithUserFullName:kTestName
                                                        email:kTestEmail];

  EXPECT_NE(nil, GetSyncSettingsViewAttributes()[NSLinkAttributeName]);
}

// Tests that the advanced Settings link in the unified consent view is not
// present after the user removes an identity from the device.
TEST_F(UnifiedConsentViewControllerTest, IdentitiesOnDeviceUserRemovesAll) {
  [view_controller() updateIdentityPickerViewWithUserFullName:kTestName
                                                        email:kTestEmail];
  [view_controller() hideIdentityPickerView];

  EXPECT_EQ(nil, GetSyncSettingsViewAttributes()[NSLinkAttributeName]);
}

}  // namespace
