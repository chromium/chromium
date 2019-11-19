// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <XCTest/XCTest.h>

#include <memory>

#include "components/strings/grit/components_strings.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#include "ios/web/public/test/http_server/error_page_response_provider.h"
#import "ios/web/public/test/http_server/http_server.h"
#include "ios/web/public/test/http_server/http_server_util.h"
#include "ios/web/public/test/http_server/response_provider.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Assert the activity service is visible by checking the "copy" button.
void AssertActivityServiceVisible() {
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonWithAccessibilityLabel(
                                   @"Copy")]
      assertWithMatcher:grey_interactable()];
}

// Assert the activity service is not visible by checking the "copy" button.
void AssertActivityServiceNotVisible() {
  [[EarlGrey
      selectElementWithMatcher:
          grey_allOf(chrome_test_util::ButtonWithAccessibilityLabel(@"Copy"),
                     grey_interactable(), nil)] assertWithMatcher:grey_nil()];
}

// Returns a button with a print label.
id<GREYMatcher> PrintButton() {
  return chrome_test_util::ButtonWithAccessibilityLabel(@"Print");
}

// Returns a button with a copy label.
id<GREYMatcher> CopyButton() {
  return chrome_test_util::ButtonWithAccessibilityLabel(@"Copy");
}

// Returns the collection view for the activity services menu. Since this is a
// system widget, it does not have an a11y id.  Instead, search for a
// UICollectionView that is the superview of the "Copy" menu item.  There are
// two nested UICollectionViews in the activity services menu, so choose the
// innermost one.
id<GREYMatcher> ShareMenuCollectionView() {
  return grey_allOf(
      grey_kindOfClass([UICollectionView class]), grey_descendant(CopyButton()),
      // Looking for a nested UICollectionView.
      grey_descendant(grey_kindOfClass([UICollectionView class])), nil);
}

}  // namespace

// Earl grey integration tests for Activity Service Controller.
@interface ActivityServiceControllerTestCase : ChromeTestCase
@end

@implementation ActivityServiceControllerTestCase

// TODO(crbug.com/747622): re-enable this test on once earl grey can interact
// with the share menu.
// TODO(crbug.com/864597): Reenable this test. This test should be rewritten
// to use Offline Version.
- (void)DISABLED_testActivityServiceControllerCantPrintUnprintablePages {
  std::unique_ptr<web::DataResponseProvider> provider(
      new ErrorPageResponseProvider());
  web::test::SetUpHttpServer(std::move(provider));

  // Open a page with an error.
  [ChromeEarlGrey loadURL:ErrorPageResponseProvider::GetDnsFailureUrl()];

  // Verify that you can share, but that the Print action is not available.
  [ChromeEarlGreyUI openShareMenu];
  AssertActivityServiceVisible();

  // To verify that the Print action is missing, scroll through the entire
  // collection view using grey_scrollInDirection(), then make sure the
  // operation failed with kGREYInteractionElementNotFoundErrorCode.
  NSError* error;
  [[[EarlGrey selectElementWithMatcher:PrintButton()]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionRight, 100)
      onElementWithMatcher:ShareMenuCollectionView()]
      assertWithMatcher:grey_notNil()
                  error:&error];

  GREYAssert([error.domain isEqual:kGREYInteractionErrorDomain] &&
                 error.code == kGREYInteractionElementNotFoundErrorCode,
             @"Print action was unexpectedly present");
}

- (void)testActivityServiceControllerIsDisabled {
  // TODO(crbug.com/996541) Starting in Xcode 11 beta 6, the share button does
  // not appear (even with a delay) flakily.
  if (@available(iOS 13, *))
    EARL_GREY_TEST_DISABLED(@"Test disabled on iOS13.");

  // Open an un-shareable page.
  GURL kURL("chrome://version");
  [ChromeEarlGrey loadURL:kURL];
  // Verify that the share button is disabled.
  id<GREYMatcher> share_button = chrome_test_util::ShareButton();
  [[EarlGrey selectElementWithMatcher:share_button]
      assertWithMatcher:grey_accessibilityTrait(
                            UIAccessibilityTraitNotEnabled)];
}

// TODO(crbug.com/747622): re-enable this test once earl grey can interact
// with the share menu.
- (void)DISABLED_testOpenActivityServiceControllerAndCopy {
  // Set up mock http server.
  std::map<GURL, std::string> responses;
  GURL url = web::test::HttpServer::MakeUrl("http://potato");
  responses[url] = "tomato";
  web::test::SetUpSimpleHttpServer(responses);

  // Open page and open the share menu.
  [ChromeEarlGrey loadURL:url];
  [ChromeEarlGreyUI openShareMenu];

  // Verify that the share menu is up and contains a Copy action.
  AssertActivityServiceVisible();
  [[EarlGrey selectElementWithMatcher:CopyButton()]
      assertWithMatcher:grey_interactable()];

  // Start the Copy action and verify that the share menu gets dismissed.
  [[EarlGrey selectElementWithMatcher:CopyButton()] performAction:grey_tap()];
  AssertActivityServiceNotVisible();
}

@end
