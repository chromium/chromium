// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <PassKit/PassKit.h>

#import <memory>

#import "base/functional/bind.h"
#import "base/test/ios/wait_util.h"
#import "ios/chrome/browser/download/model/download_test_util.h"
#import "ios/chrome/browser/download/model/mime_type_util.h"
#import "ios/chrome/browser/ui/infobars/banners/infobar_banner_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/chrome/test/scoped_eg_synchronization_disabler.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "net/test/embedded_test_server/http_request.h"
#import "net/test/embedded_test_server/http_response.h"
#import "ui/base/l10n/l10n_util_mac.h"

using base::test::ios::WaitUntilConditionOrTimeout;
using base::test::ios::kWaitForDownloadTimeout;

namespace {

// Returns matcher for PassKit error infobar.
id<GREYMatcher> PassKitErrorInfobarLabels() {
  return grey_allOf(
      grey_accessibilityID(kInfobarBannerLabelsStackViewIdentifier),
      grey_accessibilityLabel(
          l10n_util::GetNSString(IDS_IOS_GENERIC_PASSKIT_ERROR)),
      nil);
}

// PassKit landing page and download request handler.
std::unique_ptr<net::test_server::HttpResponse> GetResponse(
    const net::test_server::HttpRequest& request) {
  auto result = std::make_unique<net::test_server::BasicHttpResponse>();
  result->set_code(net::HTTP_OK);

  if (request.GetURL().path() == "/single") {
    result->set_content("<a id='bad' href='/single-bad'>Bad</a>"
                        "<a id='good' href='/single-good'>Good</a>");
  } else if (request.GetURL().path() == "/bundle") {
    result->set_content("<a id='bad' href='/bundle-bad'>Bad</a>"
                        "<a id='good' href='/bundle-good'>Good</a>"
                        "<a id='semi' href='/bundle-semi'>Semi</a>");
  } else if (request.GetURL().path() == "/single-bad") {
    result->AddCustomHeader("Content-Type", kPkPassMimeType);
    result->set_content("corrupted");
  } else if (request.GetURL().path() == "/single-good") {
    result->AddCustomHeader("Content-Type", kPkPassMimeType);
    result->set_content(testing::GetTestFileContents(testing::kPkPassFilePath));
  } else if (request.GetURL().path() == "/bundle-bad") {
    result->AddCustomHeader("Content-Type", kPkBundledPassMimeType);
    // Returning a valid pass unzipped is invalid for a bundled pass.
    result->set_content(testing::GetTestFileContents(testing::kPkPassFilePath));
  } else if (request.GetURL().path() == "/bundle-good") {
    result->AddCustomHeader("Content-Type", kPkBundledPassMimeType);
    result->set_content(
        testing::GetTestFileContents(testing::kBundledPkPassFilePath));
  } else if (request.GetURL().path() == "/bundle-semi") {
    result->AddCustomHeader("Content-Type", kPkBundledPassMimeType);
    result->set_content(
        testing::GetTestFileContents(testing::kSemiValidBundledPkPassFilePath));
  }

  return result;
}

}  // namespace

// Tests PassKit file download.
@interface PassKitEGTest : ChromeTestCase
@end

@implementation PassKitEGTest {
}

- (void)setUp {
  [super setUp];

  self.testServer->RegisterRequestHandler(base::BindRepeating(&GetResponse));
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
}

// Tests that Chrome presents PassKit error infobar if pkpass file cannot be
// parsed.
- (void)testPassKitParsingError {
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/single")];
  [ChromeEarlGrey waitForWebStateContainingText:"Bad"];
  [ChromeEarlGrey tapWebStateElementWithID:@"bad"];

  bool infobarShown = WaitUntilConditionOrTimeout(kWaitForDownloadTimeout, ^{
    NSError* error = nil;
    [[EarlGrey selectElementWithMatcher:PassKitErrorInfobarLabels()]
        assertWithMatcher:grey_notNil()
                    error:&error];
    return (error == nil);
  });
  GREYAssert(infobarShown, @"PassKit error infobar was not shown");
}

// Tests that Chrome PassKit dialog is shown for sucessfully downloaded pkpass
// file.
- (void)testPassKitDownload {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Wallet app is not supported on iPads.");
  }

  [ChromeEarlGrey loadURL:self.testServer->GetURL("/single")];
  [ChromeEarlGrey waitForWebStateContainingText:"Good"];
  [ChromeEarlGrey tapWebStateElementWithID:@"good"];

  // PKAddPassesViewController UI is rendered out of host process so EarlGrey
  // matcher can not find PassKit Dialog UI.
  // EG2 test can use XCUIApplication API to check for PassKit dialog UI
  // presentation.
  XCUIApplication* app = [[XCUIApplication alloc] init];
  XCUIElement* title = nil;
  title = app.staticTexts[@"Toy Town"];
  GREYAssert(
      [title waitForExistenceWithTimeout:kWaitForDownloadTimeout.InSecondsF()],
      @"PassKit dialog UI was not presented");
}

// Tests that Chrome PassKit dialog is shown for sucessfully downloaded bundle
// pkpasses file.
- (void)testBundlePassKitDownload {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Wallet app is not supported on iPads.");
  }

  [ChromeEarlGrey loadURL:self.testServer->GetURL("/bundle")];
  [ChromeEarlGrey waitForWebStateContainingText:"Good"];
  [ChromeEarlGrey tapWebStateElementWithID:@"good"];

  {
    ScopedSynchronizationDisabler synchronizationDisabler;
    // PKAddPassesViewController UI is rendered out of host process so EarlGrey
    // matcher can not find PassKit Dialog UI.
    // EG2 test can use XCUIApplication API to check for PassKit dialog UI
    // presentation.
    XCUIApplication* app = [[XCUIApplication alloc] init];
    XCUIElement* title = nil;
    title = app.staticTexts[@"Toy Town"];
    GREYAssert(
        [title
            waitForExistenceWithTimeout:kWaitForDownloadTimeout.InSecondsF()],
        @"PassKit dialog UI was not presented");

    // It is flaky to swipe to show the other pass, so check that there is the
    // title saying there are 2 passes.
    title = app.staticTexts[@"2 Passes"];
    GREYAssert(
        [title
            waitForExistenceWithTimeout:kWaitForDownloadTimeout.InSecondsF()],
        @"There aren't two passes.");
  }
}

// Tests that Chrome PassKit dialog is shown for sucessfully downloaded
// semi-valid bundle pkpasses file (containing one good pkpass and a non-valid
// one).
- (void)testSemiValidBundlePassKitDownload {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Wallet app is not supported on iPads.");
  }

  [ChromeEarlGrey loadURL:self.testServer->GetURL("/bundle")];
  [ChromeEarlGrey waitForWebStateContainingText:"Good"];
  [ChromeEarlGrey tapWebStateElementWithID:@"semi"];

  {
    ScopedSynchronizationDisabler synchronizationDisabler;
    // PKAddPassesViewController UI is rendered out of host process so EarlGrey
    // matcher can not find PassKit Dialog UI.
    // EG2 test can use XCUIApplication API to check for PassKit dialog UI
    // presentation.
    XCUIApplication* app = [[XCUIApplication alloc] init];
    XCUIElement* title = nil;
    title = app.staticTexts[@"Toy Town"];
    GREYAssert(
        [title
            waitForExistenceWithTimeout:kWaitForDownloadTimeout.InSecondsF()],
        @"PassKit dialog UI was not presented");

    // Swipe left, nothing should happen.
    [title swipeLeft];
    GREYAssertTrue(title.hittable, @"PassKit dialog UI was not scrolled");
  }
}

// Tests that Chrome presents PassKit error infobar if bundle pkpass file cannot
// be parsed.
- (void)testInvalidBundlePassKitDownload {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Wallet app is not supported on iPads.");
  }

  [ChromeEarlGrey loadURL:self.testServer->GetURL("/bundle")];
  [ChromeEarlGrey waitForWebStateContainingText:"Good"];
  [ChromeEarlGrey tapWebStateElementWithID:@"bad"];

  bool infobarShown = WaitUntilConditionOrTimeout(kWaitForDownloadTimeout, ^{
    NSError* error = nil;
    [[EarlGrey selectElementWithMatcher:PassKitErrorInfobarLabels()]
        assertWithMatcher:grey_notNil()
                    error:&error];
    return (error == nil);
  });
  GREYAssert(infobarShown, @"PassKit error infobar was not shown");
}

@end
