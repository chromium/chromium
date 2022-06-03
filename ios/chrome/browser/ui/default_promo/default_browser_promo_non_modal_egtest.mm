// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/ios/ios_util.h"
#import "base/test/ios/wait_util.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/infobars/banners/infobar_banner_constants.h"
#include "ios/chrome/browser/ui/ui_feature_flags.h"
#include "ios/chrome/grit/ios_chromium_strings.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_app_interface.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::test::ios::WaitUntilConditionOrTimeout;
using base::test::ios::kWaitForDownloadTimeout;

namespace {

// Returns a matcher to "Link You Copied" row.
id<GREYMatcher> LinkYouCopiedMatcher() {
  NSString* a11yLabelText = l10n_util::GetNSString(IDS_LINK_FROM_CLIPBOARD);
  return grey_accessibilityLabel(a11yLabelText);
}

// Returns a matcher for the non modal promo title.
id<GREYMatcher> NonModalTitleMatcher() {
  NSString* a11yLabelText =
      l10n_util::GetNSString(IDS_IOS_DEFAULT_BROWSER_NON_MODAL_TITLE);
  return grey_accessibilityLabel(a11yLabelText);
}

// Returns a matcher for Omnibox in NTP.
id<GREYMatcher> FakeOmniboxMatcher() {
  NSString* a11yLabelText = l10n_util::GetNSString(IDS_OMNIBOX_EMPTY_HINT);
  return grey_allOf(grey_accessibilityLabel(a11yLabelText),
                    grey_kindOfClass([UIButton class]), nil);
}

}  // namespace

// Tests Non Modal Default Promo.
@interface NonModalEGTest : ChromeTestCase
@end

@implementation NonModalEGTest

- (void)setUp {
  [super setUp];
  [ChromeEarlGreyAppInterface clearDefaultBrowserPromoData];
}

- (void)tearDown {
  [super tearDown];
  [ChromeEarlGreyAppInterface clearDefaultBrowserPromoData];
  [ChromeEarlGreyAppInterface disableDefaultBrowserPromo];
}

// Test that a non modal default modal promo appears when it is triggered by
// pasting a copied link.
// TODO(crbug.com/1218866): Test is failing on devices.
#if TARGET_IPHONE_SIMULATOR
#define MAYBE_testNonModalAppears testNonModalAppears
#else
#define MAYBE_testNonModalAppears DISABLED_testNonModalAppears
#endif
- (void)MAYBE_testNonModalAppears {
  [ChromeEarlGreyAppInterface copyURLToPasteBoard];
  [[EarlGrey selectElementWithMatcher:FakeOmniboxMatcher()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:LinkYouCopiedMatcher()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:NonModalTitleMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

@end
