// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/autofill_egtest_util.h"

#import "base/test/ios/wait_util.h"
#import "base/time/time.h"
#import "base/values.h"
#import "ios/chrome/browser/autofill/ui_bundled/autofill_app_interface.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/web/public/test/element_selector.h"

using base::test::ios::kWaitForActionTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

namespace autofill {

void FocusWebElementWithId(const char* element_id) {
  // Wait for the form to be cached by autofill first: the keyboard accessory
  // only offers suggestions for parsed forms, so focusing earlier could leave
  // the suggestion view empty.
  GREYAssertTrue([AutofillAppInterface waitForFormToBeCachedInMainFrame],
                 @"Forms were not parsed and cached.");
  NSString* scroll_script = [NSString
      stringWithFormat:@"document.getElementById('%s').scrollIntoView({block: "
                       @"'center'});",
                       element_id];

  NSString* focused_script = [NSString
      stringWithFormat:@"document.activeElement?.id === '%s'", element_id];
  ElementSelector* selector =
      [ElementSelector selectorWithElementID:element_id];
  auto is_element_focused = ^bool {
    base::Value is_focused = [ChromeEarlGrey evaluateJavaScript:focused_script];
    return is_focused.is_bool() && is_focused.GetBool();
  };
  GREYCondition* focused = [GREYCondition
      conditionWithName:@"Focus web element"
                  block:^BOOL {
                    if (is_element_focused()) {
                      return YES;
                    }
                    [ChromeEarlGrey
                        evaluateJavaScriptForSideEffect:scroll_script];
                    NSError* error = nil;
                    [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                                            WebViewMatcher()]
                        performAction:chrome_test_util::TapWebElementUnverified(
                                          selector)
                                error:&error];
                    if (error != nil) {
                      return NO;
                    }
                    // On pages without a viewport meta tag (such as
                    // `profile_form.html`), `WKWebView` delays single-tap
                    // recognition by ~350ms to disambiguate double-tap-to-zoom.
                    // Wait for the tap gesture and WebContent focus IPC to
                    // complete before retrying so a second tap is not injected
                    // while the web view is scrolling for the rising keyboard.
                    return WaitUntilConditionOrTimeout(base::Seconds(1),
                                                       is_element_focused);
                  }];
  GREYAssertTrue([focused waitWithTimeout:kWaitForActionTimeout.InSecondsF()],
                 @"Failed to focus web element with ID %s", element_id);
}

}  // namespace autofill
