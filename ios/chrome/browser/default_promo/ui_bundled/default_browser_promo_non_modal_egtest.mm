// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/functional/bind.h"
#import "base/ios/ios_util.h"
#import "base/test/ios/wait_util.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/ui/infobars/banners/infobar_banner_constants.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "net/test/embedded_test_server/http_request.h"
#import "net/test/embedded_test_server/http_response.h"
#import "ui/base/l10n/l10n_util_mac.h"

using base::test::ios::WaitUntilConditionOrTimeout;
using base::test::ios::kWaitForDownloadTimeout;

namespace {

// Wait for 2 seconds longer than the default promo show time, in case it's
// slightly delayed.
constexpr base::TimeDelta kShowPromoWebpageLoadWaitTime = base::Seconds(5);

// Returns a matcher for the non modal promo title.
id<GREYMatcher> NonModalTitleMatcher() {
  NSString* a11yLabelText =
      l10n_util::GetNSString(IDS_IOS_DEFAULT_BROWSER_NON_MODAL_TITLE);
  return grey_accessibilityLabel(a11yLabelText);
}

}  // namespace

// Tests Non Modal Default Promo.
@interface NonModalEGTest : ChromeTestCase
@end

@implementation NonModalEGTest

- (void)setUp {
  [super setUp];
  GREYAssertTrue(self.testServer->Start(), @"Server did not start.");
  [ChromeEarlGrey clearDefaultBrowserPromoData];
}

- (void)tearDown {
  [super tearDown];
  [ChromeEarlGrey clearDefaultBrowserPromoData];
}

// Test that a non modal default modal promo appears when it is triggered by
// using the share menu.
- (void)testNonModalAppears {
  const GURL destinationUrl = self.testServer->GetURL("/destination.html");
  [ChromeEarlGrey loadURL:destinationUrl];

  [ChromeEarlGreyUI openShareMenu];

  // Verify that the share menu is up and contains a Copy action.
  [ChromeEarlGrey verifyActivitySheetVisible];
  // Start the Copy action and verify that the share menu gets dismissed.
  [ChromeEarlGrey tapButtonInActivitySheetWithID:@"Copy"];
  [ChromeEarlGrey verifyActivitySheetNotVisible];

  // Wait until the promo appears.
  NSString* description = @"Wait for the promo to appear.";
  ConditionBlock condition = ^{
    NSError* error = nil;
    [[EarlGrey selectElementWithMatcher:NonModalTitleMatcher()]
        assertWithMatcher:grey_sufficientlyVisible()
                    error:&error];
    return (error == nil);
  };
  GREYAssert(
      WaitUntilConditionOrTimeout(kShowPromoWebpageLoadWaitTime, condition),
      description);
}

@end
