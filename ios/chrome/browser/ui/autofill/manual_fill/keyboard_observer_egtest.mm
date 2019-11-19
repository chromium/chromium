// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "ios/chrome/browser/ui/util/keyboard_observer_helper.h"
#import "ios/chrome/browser/ui/util/keyboard_observer_helper_app_interface.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/chrome/test/scoped_eg_synchronization_disabler.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#if defined(CHROME_EARL_GREY_2)
// TODO(crbug.com/1015113): The EG2 macro is breaking indexing for some reason
// without the trailing semicolon.  For now, disable the extra semi warning
// so Xcode indexing works for the egtest.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wc++98-compat-extra-semi"
GREY_STUB_CLASS_IN_APP_MAIN_QUEUE(KeyboardObserverHelperAppInterface);
#pragma clang diagnostic pop
#endif  // defined(CHROME_EARL_GREY_2)

using base::TimeDelta;
using base::test::ios::SpinRunLoopWithMinDelay;
using base::test::ios::WaitUntilConditionOrTimeout;
using chrome_test_util::TapWebElementWithId;
using chrome_test_util::WebViewMatcher;

namespace {

const std::string kFormElementID1 = "username";
const std::string kFormElementID2 = "otherstuff";
const std::string kFormElementSubmit = "submit";

// If an element is focused in the webview, returns its ID. Returns an empty
// NSString otherwise.
NSString* GetFocusedElementID() {
  NSString* javaScript = @"(function() {"
                          "  return document.activeElement.id;"
                          "})();";
  return [ChromeEarlGrey executeJavaScript:javaScript];
}

// Verifies that |elementId| is the selected element in the web page.
void AssertElementIsFocused(const std::string& element_id) {
  NSString* description =
      [NSString stringWithFormat:
                    @"Timeout waiting for the focused element in "
                    @"the webview to be \"%@\"",
                    base::SysUTF8ToNSString(element_id.c_str())];
  ConditionBlock condition = ^{
    return base::SysNSStringToUTF8(GetFocusedElementID()) == element_id;
  };
  GREYAssert(WaitUntilConditionOrTimeout(10, condition), description);
}

// Helper to tap a web element.
void TapOnWebElementWithID(const std::string& elementID) {
  [[EarlGrey selectElementWithMatcher:WebViewMatcher()]
      performAction:TapWebElementWithId(elementID)];
}

}  // namespace

// Tests Mannual Fallback keyboard observer for form handling.
@interface KeyboardObserverTestCase : ChromeTestCase

// Observer to be tested.
@property(nonatomic, strong) KeyboardObserverHelper* keyboardObserverHelper;

@end

@implementation KeyboardObserverTestCase

- (void)setUp {
  [super setUp];
  self.keyboardObserverHelper =
      [KeyboardObserverHelperAppInterface appSharedInstance];

  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  const GURL URL = self.testServer->GetURL("/multi_field_form.html");
  [ChromeEarlGrey loadURL:URL];
  [ChromeEarlGrey waitForWebStateContainingText:"hello!"];
}

- (void)tearDown {
  self.keyboardObserverHelper = nil;
  [super tearDown];
}

// Tests that when the keyboard actually dismiss the right callback is done.
- (void)testKeyboardHideState {
  // Opening the keyboard from a webview blocks EarlGrey's synchronization.
  ScopedSynchronizationDisabler disabler;

  // Brings up the keyboard by tapping on one of the form's field.
  TapOnWebElementWithID(kFormElementID1);
  SpinRunLoopWithMinDelay(TimeDelta::FromSeconds(1));

  // Verifies that the taped element is focused.
  AssertElementIsFocused(kFormElementID1);

  // Verify the visible state.
  KeyboardObserverHelper* observer = self.keyboardObserverHelper;
  GREYAssertTrue(observer.keyboardState.isVisible,
                 @"Keyboard should be visible.");

  // Tap the "Submit" button, and let the run loop spin.
  TapOnWebElementWithID(kFormElementSubmit);
  SpinRunLoopWithMinDelay(TimeDelta::FromSeconds(1));

  // Verify the state changed.
  GREYAssertFalse(observer.keyboardState.isVisible,
                  @"Keyboard shouldn't be visible.");
}

@end
