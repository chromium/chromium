// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/strings/stringprintf.h"
#import "base/strings/sys_string_conversions.h"
#import "base/time/time.h"
#import "components/content_settings/core/common/content_settings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/chrome/test/earl_grey/scoped_block_popups_pref.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "net/test/embedded_test_server/http_request.h"
#import "net/test/embedded_test_server/http_response.h"
#import "net/test/embedded_test_server/request_handler_util.h"

using chrome_test_util::OmniboxText;

namespace {
// Test link text and ids.
NSString* kNamedWindowLink = @"openWindowWithName";
NSString* kUnnamedWindowLink = @"openWindowNoName";

// Web view text that indicates window's closed state.
const char kWindow2NeverOpen[] = "window2.closed: never opened";
const char kWindow1Open[] = "window1.closed: false";
const char kWindow2Open[] = "window2.closed: false";
const char kWindow1Closed[] = "window1.closed: true";
const char kWindow2Closed[] = "window2.closed: true";

// URLs for testWindowOpenWriteAndReload.
const char kWriteReloadPath[] = "/writeReload.html";
const char kSlowPath[] = "/slow.html";
const char kSlowPathContent[] = "Slow Page";
constexpr base::TimeDelta kSlowPathDelay = base::Seconds(3);

// net::EmbeddedTestServer handler for kWriteReloadPath.
std::unique_ptr<net::test_server::HttpResponse> ReloadHandler(
    const net::test_server::HttpRequest& request) {
  auto http_response = std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_content_type("text/html");
  http_response->set_content(base::StringPrintf(
      "<html><body><script>function start(){var x = window.open('javascript"
      ":document.write(1)');setTimeout(function(){x.location='%s'}, "
      "500);};</script><input onclick='start()' id='button' value='button' "
      "type='button' /></body></html>",
      kSlowPath));
  return std::move(http_response);
}

// net::EmbeddedTestServer handler for kSlowPath.
std::unique_ptr<net::test_server::HttpResponse> SlowResponseHandler(
    const net::test_server::HttpRequest& request) {
  auto slow_http_response =
      std::make_unique<net::test_server::DelayedHttpResponse>(kSlowPathDelay);
  slow_http_response->set_content_type("text/html");
  slow_http_response->set_content(kSlowPathContent);
  return std::move(slow_http_response);
}

}  // namespace

// Test case for child windows opened by DOM.
@interface ChildWindowOpenByDOMTestCase : ChromeTestCase {
  std::unique_ptr<ScopedBlockPopupsPref> _blockPopupsPref;
}

@end

@implementation ChildWindowOpenByDOMTestCase

- (void)setUp {
  [super setUp];
  _blockPopupsPref =
      std::make_unique<ScopedBlockPopupsPref>(CONTENT_SETTING_ALLOW);
  self.testServer->RegisterDefaultHandler(base::BindRepeating(
      net::test_server::HandlePrefixedRequest, kWriteReloadPath,
      base::BindRepeating(&ReloadHandler)));
  self.testServer->RegisterDefaultHandler(
      base::BindRepeating(net::test_server::HandlePrefixedRequest, kSlowPath,
                          base::BindRepeating(&SlowResponseHandler)));
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");

  // Open the test page. There should only be one tab open.
  const char kChildWindowTestURL[] = "/window_proxy.html";
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kChildWindowTestURL)];
  [ChromeEarlGrey waitForWebStateContainingText:(base::SysNSStringToUTF8(
                                                    kNamedWindowLink))];
  [ChromeEarlGrey waitForMainTabCount:1];
}

// Tests that multiple calls to window.open() with the same window name returns
// the same window object.
- (void)test2ChildWindowsWithName {
  // Open two windows with the same name.
  [ChromeEarlGrey tapWebStateElementWithID:kNamedWindowLink];
  [ChromeEarlGrey waitForMainTabCount:2];
  [ChromeEarlGrey selectTabAtIndex:0];

  [ChromeEarlGrey tapWebStateElementWithID:kNamedWindowLink];
  [ChromeEarlGrey waitForMainTabCount:2];
  [ChromeEarlGrey selectTabAtIndex:0];

  // Check that they're the same window.
  [ChromeEarlGrey tapWebStateElementWithID:@"compareNamedWindows"];
  const char kWindowsEqualText[] = "named windows equal: true";
  [ChromeEarlGrey waitForWebStateContainingText:kWindowsEqualText];
}

// Tests that multiple calls to window.open() with no window name passed in
// returns a unique window object each time.
- (void)test2ChildWindowsWithoutName {
  // Open two unnamed windows.
  [ChromeEarlGrey tapWebStateElementWithID:kUnnamedWindowLink];
  [ChromeEarlGrey waitForMainTabCount:2];
  [ChromeEarlGrey selectTabAtIndex:0];

  [ChromeEarlGrey tapWebStateElementWithID:kUnnamedWindowLink];
  [ChromeEarlGrey waitForMainTabCount:3];
  [ChromeEarlGrey selectTabAtIndex:0];

  // Check that they aren't the same window object.
  [ChromeEarlGrey tapWebStateElementWithID:@"compareUnnamedWindows"];
  std::string kWindowsEqualText = "unnamed windows equal: false";
  [ChromeEarlGrey waitForWebStateContainingText:kWindowsEqualText];
}

// Tests that calling window.open() with a name returns a different window
// object than a subsequent call to window.open() without a name.
- (void)testChildWindowsWithAndWithoutName {
  // Open a named window.
  [ChromeEarlGrey tapWebStateElementWithID:kNamedWindowLink];
  [ChromeEarlGrey waitForMainTabCount:2];
  [ChromeEarlGrey selectTabAtIndex:0];

  // Open an unnamed window.
  [ChromeEarlGrey tapWebStateElementWithID:kUnnamedWindowLink];
  [ChromeEarlGrey waitForMainTabCount:3];
  [ChromeEarlGrey selectTabAtIndex:0];

  // Check that they aren't the same window object.
  [ChromeEarlGrey tapWebStateElementWithID:@"compareNamedAndUnnamedWindows"];
  const char kWindowsEqualText[] = "named and unnamed equal: false";
  [ChromeEarlGrey waitForWebStateContainingText:kWindowsEqualText];
}

// Tests that window.closed is correctly set to true when the corresponding tab
// is closed. Verifies that calling window.open() multiple times with the same
// name returns the same window object, and thus closing the corresponding tab
// results in window.closed being set to true for all references to the window
// object for that tab.
- (void)testWindowClosedWithName {
  [ChromeEarlGrey tapWebStateElementWithID:@"openWindowWithName"];
  [ChromeEarlGrey waitForMainTabCount:2];
  [ChromeEarlGrey selectTabAtIndex:0];

  // Check that named window 1 is opened and named window 2 isn't.
  const char kCheckWindow1Link[] = "checkNamedWindow1Closed";
  [ChromeEarlGrey waitForWebStateContainingText:kCheckWindow1Link];
  [ChromeEarlGrey
      tapWebStateElementWithID:[NSString
                                   stringWithUTF8String:kCheckWindow1Link]];
  [ChromeEarlGrey waitForWebStateContainingText:kWindow1Open];
  NSString* kCheckWindow2Link = @"checkNamedWindow2Closed";
  [ChromeEarlGrey tapWebStateElementWithID:kCheckWindow2Link];
  [ChromeEarlGrey waitForWebStateContainingText:kWindow2NeverOpen];

  // Open another window with the same name. Check that named window 2 is now
  // opened.
  [ChromeEarlGrey tapWebStateElementWithID:@"openWindowWithName"];
  [ChromeEarlGrey waitForMainTabCount:2];
  [ChromeEarlGrey selectTabAtIndex:0];
  [ChromeEarlGrey tapWebStateElementWithID:kCheckWindow2Link];
  [ChromeEarlGrey waitForWebStateContainingText:kWindow2Open];

  // Close the opened window. Check that named window 1 and 2 are both closed.
  [ChromeEarlGrey closeTabAtIndex:1];
  [ChromeEarlGrey waitForMainTabCount:1];
  [ChromeEarlGrey
      tapWebStateElementWithID:[NSString
                                   stringWithUTF8String:kCheckWindow1Link]];
  [ChromeEarlGrey waitForWebStateContainingText:kWindow1Closed];
  [ChromeEarlGrey tapWebStateElementWithID:kCheckWindow2Link];
  [ChromeEarlGrey waitForWebStateContainingText:kWindow2Closed];
}

// Tests that closing a tab will set window.closed to true for only
// corresponding window object and not for any other window objects.
- (void)testWindowClosedWithoutName {
  [ChromeEarlGrey tapWebStateElementWithID:@"openWindowNoName"];
  [ChromeEarlGrey waitForMainTabCount:2];
  [ChromeEarlGrey selectTabAtIndex:0];

  // Check that unnamed window 1 is opened and unnamed window 2 isn't.
  const char kCheckWindow1Link[] = "checkUnnamedWindow1Closed";
  [ChromeEarlGrey waitForWebStateContainingText:kCheckWindow1Link];
  [ChromeEarlGrey
      tapWebStateElementWithID:[NSString
                                   stringWithUTF8String:kCheckWindow1Link]];
  [ChromeEarlGrey waitForWebStateContainingText:kWindow1Open];
  NSString* kCheckWindow2Link = @"checkUnnamedWindow2Closed";
  [ChromeEarlGrey tapWebStateElementWithID:kCheckWindow2Link];
  [ChromeEarlGrey waitForWebStateContainingText:kWindow2NeverOpen];

  // Open another unnamed window. Check that unnamed window 2 is now opened.
  [ChromeEarlGrey tapWebStateElementWithID:@"openWindowNoName"];
  [ChromeEarlGrey waitForMainTabCount:3];
  [ChromeEarlGrey selectTabAtIndex:0];
  [ChromeEarlGrey tapWebStateElementWithID:kCheckWindow2Link];
  [ChromeEarlGrey waitForWebStateContainingText:kWindow2Open];

  // Close the first opened window. Check that unnamed window 1 is closed and
  // unnamed window 2 is still open.
  [ChromeEarlGrey closeTabAtIndex:1];
  [ChromeEarlGrey
      tapWebStateElementWithID:[NSString
                                   stringWithUTF8String:kCheckWindow1Link]];
  [ChromeEarlGrey waitForWebStateContainingText:kWindow1Closed];
  [ChromeEarlGrey tapWebStateElementWithID:kCheckWindow2Link];
  [ChromeEarlGrey waitForWebStateContainingText:kWindow2Open];

  // Close the second opened window. Check that unnamed window 2 is closed.
  [ChromeEarlGrey closeTabAtIndex:1];
  [ChromeEarlGrey tapWebStateElementWithID:kCheckWindow2Link];
  [ChromeEarlGrey waitForWebStateContainingText:kWindow2Closed];
}

// Tests that reloading a window.open with a document.write does not leave a
// dangling pending item. This is a regression test from crbug.com/1011898
- (void)testWindowOpenWriteAndReload {
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kWriteReloadPath)];
  [ChromeEarlGrey tapWebStateElementWithID:@"button"];
  [ChromeEarlGrey reload];
  [ChromeEarlGrey waitForWebStateContainingText:kSlowPathContent
                                        timeout:kSlowPathDelay * 2];

  GURL slowURL = self.testServer->GetURL(kSlowPath);
  [[EarlGrey selectElementWithMatcher:OmniboxText(slowURL.GetContent())]
      assertWithMatcher:grey_notNil()];
}

@end
