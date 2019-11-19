// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>
#import <XCTest/XCTest.h>

#import "base/strings/sys_string_conversions.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#include "ios/web/public/test/element_selector.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Directory containing the |kLogoPagePath| and |kLogoPageImageSourcePath|
// resources.
const char kServerFilesDir[] = "ios/testing/data/http_server_files/";

// Id of the "Start Logging" button.
NSString* const kStartLoggingButtonId = @"start-logging";
// Id of the "Stop Logging" button.
NSString* const kStopLoggingButtonId = @"stop-logging";

// Test page continaing buttons to test console logging.
const char kConsolePage[] = "/console_with_iframe.html";
// Id of the console page button to log a debug message.
NSString* const kDebugMessageButtonId = @"debug";
// Id of the console page button to log an error.
NSString* const kErrorMessageButtonId = @"error";
// Id of the console page button to log an info message.
NSString* const kInfoMessageButtonId = @"info";
// Id of the console page button to log a message.
NSString* const kLogMessageButtonId = @"log";
// Id of the console page button to log a warning.
NSString* const kWarningMessageButtonId = @"warn";

// Label for debug console messages.
const char kDebugMessageLabel[] = "DEBUG";
// Label for error console messages.
const char kErrorMessageLabel[] = "ERROR";
// Label for info console messages.
const char kInfoMessageLabel[] = "INFO";
// Label for log console messages.
const char kLogMessageLabel[] = "LOG";
// Label for warning console messages.
const char kWarningMessageLabel[] = "WARN";

// Text of the message emitted from the |kDebugMessageButtonId| button on
// |kConsolePage|.
const char kDebugMessageText[] = "This is a debug message.";
// Text of the message emitted from the |kErrorMessageButtonId| button on
// |kConsolePage|.
const char kErrorMessageText[] = "This is an error message.";
// Text of the message emitted from the |kInfoMessageButtonId| button on
// |kConsolePage|.
const char kInfoMessageText[] = "This is an informative message.";
// Text of the message emitted from the |kLogMessageButtonId| button on
// |kConsolePage|.
const char kLogMessageText[] = "This log is very round.";
// Text of the message emitted from the |kWarningMessageButtonId| button on
// |kConsolePage|.
const char kWarningMessageText[] = "This is a warning message.";

// Text of the message emitted from the |kDebugMessageButtonId| button within
// the iframe on |kConsolePage|.
const char kIFrameDebugMessageText[] = "This is an iframe debug message.";
// Text of the message emitted from the |kErrorMessageButtonId| button within
// the iframe on |kConsolePage|.
const char kIFrameErrorMessageText[] = "This is an iframe error message.";
// Text of the message emitted from the |kInfoMessageButtonId| button within the
// iframe on |kConsolePage|.
const char kIFrameInfoMessageText[] = "This is an iframe informative message.";
// Text of the message emitted from the |kLogMessageButtonId| button within the
// iframe on |kConsolePage|.
const char kIFrameLogMessageText[] = "This iframe log is very round.";
// Text of the message emitted from the |kWarningMessageButtonId| button within
// the iframe on |kConsolePage|.
const char kIFrameWarningMessageText[] = "This is an iframe warning message.";

ElementSelector* StartLoggingButton() {
  return [ElementSelector
      selectorWithElementID:base::SysNSStringToUTF8(kStartLoggingButtonId)];
}

}  // namespace

// Test case for chrome://inspect WebUI page.
@interface InspectUITestCase : ChromeTestCase
@end

@implementation InspectUITestCase

- (void)setUp {
  [super setUp];
  self.testServer->ServeFilesFromSourceDirectory(
      base::FilePath(kServerFilesDir));
  GREYAssertTrue(self.testServer->Start(), @"Server did not start.");
}

// Tests that chrome://inspect allows the user to enable and disable logging.
- (void)testStartStopLogging {
  [ChromeEarlGrey loadURL:GURL(kChromeUIInspectURL)];

  [ChromeEarlGrey waitForWebStateContainingElement:StartLoggingButton()];

  [ChromeEarlGrey tapWebStateElementWithID:kStartLoggingButtonId];

  ElementSelector* stopLoggingButton = [ElementSelector
      selectorWithElementID:base::SysNSStringToUTF8(kStopLoggingButtonId)];
  [ChromeEarlGrey waitForWebStateContainingElement:stopLoggingButton];

  [ChromeEarlGrey tapWebStateElementWithID:kStopLoggingButtonId];

  [ChromeEarlGrey waitForWebStateContainingElement:StartLoggingButton()];
}

// Tests that log messages from a page's main frame are displayed.
- (void)testMainFrameLogging {
  [ChromeEarlGrey loadURL:GURL(kChromeUIInspectURL)];

  // Start logging.
  [ChromeEarlGrey waitForWebStateContainingElement:StartLoggingButton()];
  [ChromeEarlGrey tapWebStateElementWithID:kStartLoggingButtonId];

  // Open console test page.
  [ChromeEarlGrey openNewTab];
  const GURL consoleTestsURL = self.testServer->GetURL(kConsolePage);
  [ChromeEarlGrey loadURL:consoleTestsURL];
  std::string debugButtonID = base::SysNSStringToUTF8(kDebugMessageButtonId);
  [ChromeEarlGrey waitForWebStateContainingElement:
                      [ElementSelector selectorWithElementID:debugButtonID]];

  // Log messages.
  [ChromeEarlGrey tapWebStateElementWithID:kDebugMessageButtonId];
  [ChromeEarlGrey tapWebStateElementWithID:kErrorMessageButtonId];
  [ChromeEarlGrey tapWebStateElementWithID:kInfoMessageButtonId];
  [ChromeEarlGrey tapWebStateElementWithID:kLogMessageButtonId];
  [ChromeEarlGrey tapWebStateElementWithID:kWarningMessageButtonId];

  [ChromeEarlGrey selectTabAtIndex:0];
  // Validate messages and labels are displayed.
  [ChromeEarlGrey waitForWebStateContainingText:kDebugMessageLabel];
  [ChromeEarlGrey waitForWebStateContainingText:kDebugMessageText];
  [ChromeEarlGrey waitForWebStateContainingText:kErrorMessageLabel];
  [ChromeEarlGrey waitForWebStateContainingText:kErrorMessageText];
  [ChromeEarlGrey waitForWebStateContainingText:kInfoMessageLabel];
  [ChromeEarlGrey waitForWebStateContainingText:kInfoMessageText];
  [ChromeEarlGrey waitForWebStateContainingText:kLogMessageLabel];
  [ChromeEarlGrey waitForWebStateContainingText:kLogMessageText];
  [ChromeEarlGrey waitForWebStateContainingText:kWarningMessageLabel];
  [ChromeEarlGrey waitForWebStateContainingText:kWarningMessageText];
}

// Tests that log messages from an iframe are displayed.
- (void)testIframeLogging {
  [ChromeEarlGrey loadURL:GURL(kChromeUIInspectURL)];

  // Start logging.
  [ChromeEarlGrey waitForWebStateContainingElement:StartLoggingButton()];
  [ChromeEarlGrey tapWebStateElementWithID:kStartLoggingButtonId];

  // Open console test page.
  [ChromeEarlGrey openNewTab];
  const GURL consoleTestsURL = self.testServer->GetURL(kConsolePage);
  [ChromeEarlGrey loadURL:consoleTestsURL];

  std::string debugButtonID = base::SysNSStringToUTF8(kDebugMessageButtonId);
  [ChromeEarlGrey waitForWebStateContainingElement:
                      [ElementSelector selectorWithElementID:debugButtonID]];

  // Log messages.
  [ChromeEarlGrey tapWebStateElementInIFrameWithID:debugButtonID];

  std::string errorButtonID = base::SysNSStringToUTF8(kErrorMessageButtonId);
  [ChromeEarlGrey tapWebStateElementInIFrameWithID:errorButtonID];

  std::string infoButtonID = base::SysNSStringToUTF8(kInfoMessageButtonId);
  [ChromeEarlGrey tapWebStateElementInIFrameWithID:infoButtonID];

  std::string logButtonID = base::SysNSStringToUTF8(kLogMessageButtonId);
  [ChromeEarlGrey tapWebStateElementInIFrameWithID:logButtonID];

  std::string warnButtonID = base::SysNSStringToUTF8(kWarningMessageButtonId);
  [ChromeEarlGrey tapWebStateElementInIFrameWithID:warnButtonID];

  [ChromeEarlGrey selectTabAtIndex:0];
  // Validate messages and labels are displayed.
  [ChromeEarlGrey waitForWebStateContainingText:kDebugMessageLabel];
  [ChromeEarlGrey waitForWebStateContainingText:kIFrameDebugMessageText];
  [ChromeEarlGrey waitForWebStateContainingText:kErrorMessageLabel];
  [ChromeEarlGrey waitForWebStateContainingText:kIFrameErrorMessageText];
  [ChromeEarlGrey waitForWebStateContainingText:kInfoMessageLabel];
  [ChromeEarlGrey waitForWebStateContainingText:kIFrameInfoMessageText];
  [ChromeEarlGrey waitForWebStateContainingText:kLogMessageLabel];
  [ChromeEarlGrey waitForWebStateContainingText:kIFrameLogMessageText];
  [ChromeEarlGrey waitForWebStateContainingText:kWarningMessageLabel];
  [ChromeEarlGrey waitForWebStateContainingText:kIFrameWarningMessageText];
}

// Tests that log messages are correctly displayed from multiple tabs.
- (void)testLoggingFromMultipleTabs {
  [ChromeEarlGrey loadURL:GURL(kChromeUIInspectURL)];

  // Start logging.
  [ChromeEarlGrey waitForWebStateContainingElement:StartLoggingButton()];
  [ChromeEarlGrey tapWebStateElementWithID:kStartLoggingButtonId];

  // Open console test page.
  [ChromeEarlGrey openNewTab];
  const GURL consoleTestsURL = self.testServer->GetURL(kConsolePage);
  [ChromeEarlGrey loadURL:consoleTestsURL];
  std::string logButtonID = base::SysNSStringToUTF8(kLogMessageButtonId);
  [ChromeEarlGrey
      waitForWebStateContainingElement:[ElementSelector
                                           selectorWithElementID:logButtonID]];

  // Log a message and verify it is displayed.
  [ChromeEarlGrey tapWebStateElementWithID:kDebugMessageButtonId];
  [ChromeEarlGrey selectTabAtIndex:0];
  [ChromeEarlGrey waitForWebStateContainingText:kDebugMessageLabel];
  [ChromeEarlGrey waitForWebStateContainingText:kDebugMessageText];

  // Open console test page again.
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey loadURL:consoleTestsURL];
  [ChromeEarlGrey
      waitForWebStateContainingElement:[ElementSelector
                                           selectorWithElementID:logButtonID]];

  // Log another message and verify it is displayed.
  [ChromeEarlGrey tapWebStateElementWithID:kLogMessageButtonId];
  [ChromeEarlGrey selectTabAtIndex:0];
  [ChromeEarlGrey waitForWebStateContainingText:kLogMessageLabel];
  [ChromeEarlGrey waitForWebStateContainingText:kLogMessageText];

  // Ensure the log from the first tab still exists.
  [ChromeEarlGrey waitForWebStateContainingText:kDebugMessageLabel];
  [ChromeEarlGrey waitForWebStateContainingText:kDebugMessageText];
}

// Tests that messages are cleared after stopping logging.
- (void)testMessagesClearedOnStopLogging {
  [ChromeEarlGrey loadURL:GURL(kChromeUIInspectURL)];

  // Start logging.
  [ChromeEarlGrey waitForWebStateContainingElement:StartLoggingButton()];
  [ChromeEarlGrey tapWebStateElementWithID:kStartLoggingButtonId];

  // Open console test page.
  [ChromeEarlGrey openNewTab];
  const GURL consoleTestsURL = self.testServer->GetURL(kConsolePage);
  [ChromeEarlGrey loadURL:consoleTestsURL];
  std::string logButtonID = base::SysNSStringToUTF8(kLogMessageButtonId);
  [ChromeEarlGrey
      waitForWebStateContainingElement:[ElementSelector
                                           selectorWithElementID:logButtonID]];

  // Log a message and verify it is displayed.
  [ChromeEarlGrey tapWebStateElementWithID:kDebugMessageButtonId];
  [ChromeEarlGrey selectTabAtIndex:0];
  [ChromeEarlGrey waitForWebStateContainingText:kDebugMessageLabel];
  [ChromeEarlGrey waitForWebStateContainingText:kDebugMessageText];

  // Stop logging.
  [ChromeEarlGrey tapWebStateElementWithID:kStopLoggingButtonId];
  // Ensure message was cleared.
  [ChromeEarlGrey waitForWebStateNotContainingText:kDebugMessageLabel];
  [ChromeEarlGrey waitForWebStateNotContainingText:kDebugMessageText];
}

// Tests that messages are cleared after a page reload.
- (void)testMessagesClearedOnReload {
  [ChromeEarlGrey loadURL:GURL(kChromeUIInspectURL)];

  // Start logging.
  [ChromeEarlGrey waitForWebStateContainingElement:StartLoggingButton()];
  [ChromeEarlGrey tapWebStateElementWithID:kStartLoggingButtonId];

  // Open console test page.
  [ChromeEarlGrey openNewTab];
  const GURL consoleTestsURL = self.testServer->GetURL(kConsolePage);
  [ChromeEarlGrey loadURL:consoleTestsURL];
  std::string logButtonID = base::SysNSStringToUTF8(kLogMessageButtonId);
  [ChromeEarlGrey
      waitForWebStateContainingElement:[ElementSelector
                                           selectorWithElementID:logButtonID]];

  // Log a message and verify it is displayed.
  [ChromeEarlGrey tapWebStateElementWithID:kDebugMessageButtonId];
  [ChromeEarlGrey selectTabAtIndex:0];
  [ChromeEarlGrey waitForWebStateContainingText:kDebugMessageLabel];
  [ChromeEarlGrey waitForWebStateContainingText:kDebugMessageText];

  // Reload page.
  [ChromeEarlGrey reload];
  // Ensure message was cleared.
  [ChromeEarlGrey waitForWebStateNotContainingText:kDebugMessageLabel];
  [ChromeEarlGrey waitForWebStateNotContainingText:kDebugMessageText];
}

// Tests that messages are cleared for a tab which is closed.
- (void)testMessagesClearedOnTabClosure {
  [ChromeEarlGrey loadURL:GURL(kChromeUIInspectURL)];

  // Start logging.
  [ChromeEarlGrey waitForWebStateContainingElement:StartLoggingButton()];
  [ChromeEarlGrey tapWebStateElementWithID:kStartLoggingButtonId];

  // Open console test page.
  [ChromeEarlGrey openNewTab];
  const GURL consoleTestsURL = self.testServer->GetURL(kConsolePage);
  [ChromeEarlGrey loadURL:consoleTestsURL];
  std::string debugButtonID = base::SysNSStringToUTF8(kDebugMessageButtonId);
  [ChromeEarlGrey waitForWebStateContainingElement:
                      [ElementSelector selectorWithElementID:debugButtonID]];

  [ChromeEarlGrey tapWebStateElementWithID:kDebugMessageButtonId];
  [ChromeEarlGrey closeCurrentTab];

  // Validate message and label are not displayed.
  [ChromeEarlGrey waitForWebStateNotContainingText:kDebugMessageLabel];
  [ChromeEarlGrey waitForWebStateNotContainingText:kDebugMessageText];
}

@end
