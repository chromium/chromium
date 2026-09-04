// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <memory>

#import "base/functional/bind.h"
#import "base/test/ios/wait_util.h"
#import "ios/chrome/browser/download/model/download_test_util.h"
#import "ios/chrome/browser/shared/model/utils/mime_type_util.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/web/public/test/element_selector.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "net/test/embedded_test_server/http_request.h"
#import "net/test/embedded_test_server/http_response.h"

using base::test::ios::kWaitForDownloadTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

namespace {

// vCard landing page and download request handler.
std::unique_ptr<net::test_server::HttpResponse> GetResponse(
    const net::test_server::HttpRequest& request) {
  auto result = std::make_unique<net::test_server::BasicHttpResponse>();
  result->set_code(net::HTTP_OK);

  if (request.GetURL().GetPath() == "/") {
    result->set_content("<a id='vcard' href='/vcard.vcf'>vCard</a>");
    return result;
  }

  if (request.GetURL().GetPath() == "/vcard.vcf") {
    result->set_code(net::HTTP_OK);
    result->AddCustomHeader("Content-Type", kVcardMimeType);
    result->set_content(testing::GetTestFileContents(testing::kVcardFilePath));
    return result;
  }

  return nullptr;
}

// Waits for the contact view Done button to be visible.
void WaitForVcardDoneButton() {
  ConditionBlock condition = ^{
    NSError* error = nil;
    [[EarlGrey
        selectElementWithMatcher:chrome_test_util::NavigationBarDoneButton()]
        assertWithMatcher:grey_sufficientlyVisible()
                    error:&error];
    return (BOOL)(error == nil);
  };
  GREYAssert(WaitUntilConditionOrTimeout(kWaitForDownloadTimeout, condition),
             @"vCard Done button was not visible.");
}

}  // namespace

// Tests downloading and presenting vCard files.
@interface VcardEGTest : ChromeTestCase
@end

@implementation VcardEGTest

- (void)setUp {
  [super setUp];

  self.testServer->RegisterRequestHandler(base::BindRepeating(&GetResponse));
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
}

// Tests that vCard contact view is shown for successfully downloaded vCard
// file.
- (void)testDownloadVcard {
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/")];
  [ChromeEarlGrey waitForWebStateContainingText:"vCard"];
  [ChromeEarlGrey tapWebStateElementWithID:@"vcard"];

  // Verify contact view Done button is presented.
  WaitForVcardDoneButton();
}

// Tests that vCard contact view is dismissed when web state navigation occurs.
- (void)testDismissVcardOnNavigation {
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/")];
  [ChromeEarlGrey waitForWebStateContainingText:"vCard"];
  [ChromeEarlGrey tapWebStateElementWithID:@"vcard"];

  // Verify contact view Done button is presented.
  WaitForVcardDoneButton();

  // Navigate away in the same tab.
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/")];

  // Verify contact view is dismissed.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::NavigationBarDoneButton()]
      assertWithMatcher:grey_nil()];
}

// Tests that a vCard opened in a new background tab defers its presentation
// until that tab is selected.
- (void)testDeferredVcardPresentationOnTabSwitch {
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/")];
  [ChromeEarlGrey waitForWebStateContainingText:"vCard"];

  // Open context menu on download link and select "Open in New Tab".
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::LongPressElementForContextMenu(
                        [ElementSelector selectorWithElementID:"vcard"],
                        /*menu_should_appear=*/true)];

  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::OpenLinkInNewTabButton()]
      performAction:grey_tap()];

  // Wait until the background tab is opened.
  [ChromeEarlGrey waitForMainTabCount:2];

  // Verify that the vCard UI is not shown on the foreground tab.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::NavigationBarDoneButton()]
      assertWithMatcher:grey_nil()];

  // Switch to the background tab.
  [ChromeEarlGrey selectTabAtIndex:1U];

  // Verify that the deferred vCard contact view is presented once shown.
  WaitForVcardDoneButton();
}

@end
