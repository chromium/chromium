// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/apple/foundation_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "ios/chrome/browser/autofill/ui_bundled/autofill_app_interface.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/chrome/test/scoped_eg_synchronization_disabler.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/web/public/test/element_selector.h"
#import "ios/web/public/web_state.h"
#import "net/test/embedded_test_server/embedded_test_server.h"

using base::test::ios::kWaitForUIElementTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

using chrome_test_util::TapWebElementWithId;
using chrome_test_util::WebViewMatcher;

namespace {

const char kFormElementId1[] = "username";
const char kFormElementId2[] = "otherstuff";

// If an element is focused in the webview, returns its ID. Returns an empty
// NSString otherwise.
NSString* GetFocusedElementId() {
  NSString* js = @"(function() {"
                  "  return document.activeElement.id;"
                  "})();";
  base::Value result = [ChromeEarlGrey evaluateJavaScript:js];
  return result.is_string() ? base::SysUTF8ToNSString(result.GetString()) : @"";
}

// Verifies that `elementId` is the selected element in the web page.
void AssertElementIsFocused(const std::string& element_id) {
  NSString* description =
      [NSString stringWithFormat:@"Timeout waiting for the focused element in "
                                 @"the webview to be \"%@\"",
                                 base::SysUTF8ToNSString(element_id.c_str())];
  ConditionBlock condition = ^{
    return base::SysNSStringToUTF8(GetFocusedElementId()) == element_id;
  };
  GREYAssert(WaitUntilConditionOrTimeout(base::Seconds(10), condition),
             description);
}

}  // namespace

// Tests autofill's keyboard and keyboard's accessories handling.
@interface FormInputTestCase : ChromeTestCase
@end

@implementation FormInputTestCase

// Tests finding the correct "next" and "previous" form assist controls in the
// iOS built-in form assist view.
- (void)testFindDefaultFormAssistControls {
  // This test is not relevant on iPads:
  // the previous and next buttons are not shown in our keyboard input
  // accessory. Instead, they appear in the native keyboard's shortcut bar (to
  // the left and right of the QuickType suggestions). Those buttons are also
  // not shown when the Keyboard Accessory Upgrade feature is enabled.
  if ([ChromeEarlGrey isIPadIdiom] ||
      [AutofillAppInterface isKeyboardAccessoryUpgradeEnabled]) {
    EARL_GREY_TEST_SKIPPED(
        @"Skipped for iPad (no hidden toolbar in tablet) or when the Keyboard "
        @"Accessory Upgrade feature is enabled.");
  }

  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  GURL URL = self.testServer->GetURL("/multi_field_form.html");
  [ChromeEarlGrey loadURL:URL];

  [ChromeEarlGrey waitForWebStateContainingText:"hello!"];

  // Opening the keyboard from a webview blocks EarlGrey's synchronization.
  {
    ScopedSynchronizationDisabler disabler;

    // Brings up the keyboard by tapping on one of the form's field.
    [[EarlGrey selectElementWithMatcher:WebViewMatcher()]
        performAction:TapWebElementWithId(kFormElementId1)];

    id<GREYMatcher> nextButtonMatcher =
        chrome_test_util::ButtonWithAccessibilityLabelId(
            IDS_IOS_AUTOFILL_ACCNAME_NEXT_FIELD);
    id<GREYMatcher> previousButtonMatcher =
        chrome_test_util::ButtonWithAccessibilityLabelId(
            IDS_IOS_AUTOFILL_ACCNAME_PREVIOUS_FIELD);
    id<GREYMatcher> closeButtonMatcher =
        chrome_test_util::ButtonWithAccessibilityLabelId(
            IDS_IOS_AUTOFILL_ACCNAME_HIDE_KEYBOARD);

    // Wait until the keyboard's "Next" button appeared.
    NSString* description =
        @"Wait for the keyboard's \"Next\" button to appear.";
    ConditionBlock condition = ^{
      NSError* error = nil;
      [[EarlGrey selectElementWithMatcher:nextButtonMatcher]
          assertWithMatcher:grey_notNil()
                      error:&error];
      return (error == nil);
    };
    GREYAssert(WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, condition),
               description);
    base::test::ios::SpinRunLoopWithMinDelay(base::Seconds(1));

    // Verifies that the taped element is focused.
    AssertElementIsFocused(kFormElementId1);

    // Tap the "Next" button.
    [[EarlGrey selectElementWithMatcher:nextButtonMatcher]
        performAction:grey_tap()];
    AssertElementIsFocused(kFormElementId2);

    // Tap the "Previous" button.
    [[EarlGrey selectElementWithMatcher:previousButtonMatcher]
        performAction:grey_tap()];
    AssertElementIsFocused(kFormElementId1);

    // Tap the "Close" button.
    [[EarlGrey selectElementWithMatcher:closeButtonMatcher]
        performAction:grey_tap()];
  }
}

@end
