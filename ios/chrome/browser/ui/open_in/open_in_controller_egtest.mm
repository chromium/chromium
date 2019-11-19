// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>

#include "base/ios/ios_util.h"
#include "base/strings/sys_string_conversions.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Path which leads to a PDF file.
const char kPDFPath[] = "/testpage.pdf";

id<GREYMatcher> ShareMenuDismissButton() {
  if (@available(iOS 13, *)) {
    return chrome_test_util::CloseButton();
  } else {
    return chrome_test_util::CancelButton();
  }
}

}  // namespace

// Tests Open in Feature for PDF files.
@interface OpenInManagerTestCase : ChromeTestCase
@end

@implementation OpenInManagerTestCase

- (void)setUp {
  [super setUp];
  GREYAssertTrue(self.testServer->Start(), @"Server did not start.");
}

// Tests that open in button appears when opening a PDF, and that tapping on it
// will open the activity view.
- (void)testOpenIn {
  // TODO(crbug.com/983135): The share menu displays in a popover on iPad, which
  // causes this test to fail.
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_DISABLED(@"Disabled on iPad");
  }

  // TODO(crbug.com/982845): A bug is causing the "Open in" toolbar to disappear
  // after any VC is presented fullscreen over the BVC.  The iOS 13 share menu
  // is presented fullscreen, but only when compiling with the iOS 12 SDK.
  // Disable this test in this configuration until the underlying bug is fixed.
#if !defined(__IPHONE_13_0) || (__IPHONE_OS_VERSION_MAX_ALLOWED < __IPHONE_13_0)
  if (base::ios::IsRunningOnIOS13OrLater()) {
    EARL_GREY_TEST_DISABLED(
        @"Disabled on iOS 13 when compiled with the iOS 12 SDK.");
  }
#endif

  [ChromeEarlGrey loadURL:self.testServer->GetURL(kPDFPath)];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::OpenInButton()]
      performAction:grey_tap()];
  // Check and tap on the Cancel button that appears below the activity menu.
  // Other actions buttons can't be checked from EarlGrey.
  [[[EarlGrey selectElementWithMatcher:ShareMenuDismissButton()]
      assertWithMatcher:grey_interactable()] performAction:grey_tap()];

  // Check that after dismissing the activity view, tapping on the web view will
  // bring the open in bar again.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::OpenInButton()]
      assertWithMatcher:grey_interactable()];
}

@end
