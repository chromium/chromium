// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>

#include "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "components/autofill/ios/browser/js_suggestion_manager.h"
#import "ios/chrome/browser/autofill/form_input_accessory_view_handler.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/app/tab_test_util.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/web/public/test/earl_grey/web_view_actions.h"
#import "ios/web/public/test/earl_grey/web_view_matchers.h"
#include "ios/web/public/test/element_selector.h"
#import "ios/web/public/test/http_server/http_server.h"
#include "ios/web/public/test/http_server/http_server_util.h"
#import "ios/web/public/web_state/js/crw_js_injection_receiver.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using web::test::ElementSelector;

namespace {

const char kFormElementId1[] = "username";
const char kFormElementId2[] = "otherstuff";

// If an element is focused in the webview, returns its ID. Returns an empty
// NSString otherwise.
NSString* GetFocusedElementId() {
  NSString* js = @"(function() {"
                  "  return document.activeElement.id;"
                  "})();";
  NSError* error = nil;
  NSString* result = chrome_test_util::ExecuteJavaScript(js, &error);
  GREYAssertTrue(!error, @"Unexpected error when executing JavaScript.");
  return result;
}

// Verifies that |elementId| is the selected element in the web page.
void AssertElementIsFocused(const std::string& element_id) {
  NSString* description =
      [NSString stringWithFormat:@"Timeout waiting for the focused element in "
                                 @"the webview to be \"%@\"",
                                 base::SysUTF8ToNSString(element_id.c_str())];
  ConditionBlock condition = ^{
    return base::SysNSStringToUTF8(GetFocusedElementId()) == element_id;
  };
  GREYAssert(base::test::ios::WaitUntilConditionOrTimeout(10, condition),
             description);
}

}  // namespace

// Tests autofill's keyboard and keyboard's accessories handling.
@interface FormInputTestCase : ChromeTestCase
@end

@implementation FormInputTestCase

- (void)tearDown {
  // |testFindDefaultFormAssistControls| disables synchronization.
  // This makes sure it is enabled if that test has failed and did not enable it
  // back.
  [[GREYConfiguration sharedInstance]
          setValue:@YES
      forConfigKey:kGREYConfigKeySynchronizationEnabled];
  [super tearDown];
}

// Tests finding the correct "next" and "previous" form assist controls in the
// iOS built-in form assist view.
- (void)testFindDefaultFormAssistControls {
  // This test is not relevant on iPads:
  // the previous and next buttons are not shown in our keyboard input
  // accessory. Instead, they appear in the native keyboard's shortcut bar (to
  // the left and right of the QuickType suggestions).
  if (IsIPadIdiom()) {
    EARL_GREY_TEST_SKIPPED(@"Skipped for iPad (no hidden toolbar in tablet)");
  }

  web::test::SetUpFileBasedHttpServer();
  GURL URL = web::test::HttpServer::MakeUrl(
      "http://ios/testing/data/http_server_files/multi_field_form.html");
  [ChromeEarlGrey loadURL:URL];

  [ChromeEarlGrey waitForWebViewContainingText:"hello!"];

  // Opening the keyboard from a webview blocks EarlGrey's synchronization.
  [[GREYConfiguration sharedInstance]
          setValue:@NO
      forConfigKey:kGREYConfigKeySynchronizationEnabled];

  // Brings up the keyboard by tapping on one of the form's field.
  [[EarlGrey
      selectElementWithMatcher:web::WebViewInWebState(
                                   chrome_test_util::GetCurrentWebState())]
      performAction:web::WebViewTapElement(
                        chrome_test_util::GetCurrentWebState(),
                        ElementSelector::ElementSelectorId(kFormElementId1))];

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
  NSString* description = @"Wait for the keyboard's \"Next\" button to appear.";
  ConditionBlock condition = ^{
    NSError* error = nil;
    [[EarlGrey selectElementWithMatcher:nextButtonMatcher]
        assertWithMatcher:grey_notNil()
                    error:&error];
    return (error == nil);
  };
  GREYAssert(base::test::ios::WaitUntilConditionOrTimeout(
                 base::test::ios::kWaitForUIElementTimeout, condition),
             description);
  base::test::ios::SpinRunLoopWithMinDelay(base::TimeDelta::FromSeconds(1));

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

  [[GREYConfiguration sharedInstance]
          setValue:@YES
      forConfigKey:kGREYConfigKeySynchronizationEnabled];
}

// Tests that trying to programmatically dismiss the keyboard when it isn't
// visible doesn't crash the browser.
- (void)testCloseKeyboardWhenNotVisible {
  FormInputAccessoryViewHandler* accessoryViewDelegate =
      [[FormInputAccessoryViewHandler alloc] init];
  GREYAssertNotNil(accessoryViewDelegate,
                   @"The Accessory View Delegate should not be non nil.");
  [accessoryViewDelegate closeKeyboardWithoutButtonPress];
  CRWJSInjectionReceiver* injectionReceiver =
      chrome_test_util::GetCurrentWebState()->GetJSInjectionReceiver();
  accessoryViewDelegate.JSSuggestionManager =
      base::mac::ObjCCastStrict<JsSuggestionManager>(
          [injectionReceiver instanceOfClass:[JsSuggestionManager class]]);
  [accessoryViewDelegate closeKeyboardWithoutButtonPress];
}

@end
