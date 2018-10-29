// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <ChromeWebView/ChromeWebView.h>
#import <Foundation/Foundation.h>

#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "ios/web_view/test/web_view_inttest_base.h"
#import "ios/web_view/test/web_view_test_util.h"
#import "net/base/mac/url_conversions.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest_mac.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::test::ios::kWaitForActionTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

namespace ios_web_view {

namespace {
NSString* const kTestFormName = @"FormName";
NSString* const kTestFormID = @"FormID";
NSString* const kTestNameFieldID = @"nameID";
NSString* const kTestAddressFieldID = @"addressID";
NSString* const kTestCityFieldID = @"cityID";
NSString* const kTestStateFieldID = @"stateID";
NSString* const kTestZipFieldID = @"zipID";
NSString* const kTestFieldType = @"text";
NSString* const kTestAddressFieldValue = @"123 Main Street";
NSString* const kTestCityFieldValue = @"Springfield";
NSString* const kTestNameFieldValue = @"Homer Simpson";
NSString* const kTestStateFieldValue = @"IL";
NSString* const kTestZipFieldValue = @"55123";
NSString* const kTestSubmitID = @"SubmitID";
NSString* const kTestFormHtml =
    [NSString stringWithFormat:
                  // Direct form to about:blank to avoid unnecessary navigation.
                  @"<form action='about:blank' name='%@' id='%@'>"
                   "Name <input type='text' name='name' id='%@'>"
                   "Address <input type='text' name='address' id='%@'>"
                   "City <input type='text' name='city' id='%@'>"
                   "State <input type='text' name='state' id='%@'>"
                   "Zip <input type='text' name='zip' id='%@'>"
                   "<input type='submit' id='%@'/>"
                   "</form>",
                  kTestFormName,
                  kTestFormID,
                  kTestNameFieldID,
                  kTestAddressFieldID,
                  kTestCityFieldID,
                  kTestStateFieldID,
                  kTestZipFieldID,
                  kTestSubmitID];
}  // namespace

// Tests autofill features in CWVWebViews.
class WebViewAutofillTest : public WebViewInttestBase {
 protected:
  WebViewAutofillTest() : autofill_controller_(web_view_.autofillController) {
    // Adds a dummy superview to |web_view_| so that WebState visiblity gets
    // updated.
    dummy_super_view_ = [[UIView alloc] init];
    [dummy_super_view_ addSubview:web_view_];
  }

  bool LoadTestPage() WARN_UNUSED_RESULT {
    std::string html = base::SysNSStringToUTF8(kTestFormHtml);
    main_frame_id_ = nil;
    GURL url = GetUrlForPageWithHtmlBody(html);
    return test::LoadUrl(web_view_, net::NSURLWithGURL(url));
  }

  bool SubmitForm() WARN_UNUSED_RESULT {
    NSString* submit_script =
        [NSString stringWithFormat:@"document.getElementById('%@').click();",
                                   kTestSubmitID];
    NSError* submit_error = nil;
    test::EvaluateJavaScript(web_view_, submit_script, &submit_error);
    return !submit_error;
  }

  bool SetFormFieldValue(NSString* field_id,
                         NSString* field_value) WARN_UNUSED_RESULT {
    NSString* set_value_script = [NSString
        stringWithFormat:@"document.getElementById('%@').value = '%@';",
                         field_id, field_value];
    NSError* set_value_error = nil;
    test::EvaluateJavaScript(web_view_, set_value_script, &set_value_error);
    return !set_value_error;
  }

  NSArray<CWVAutofillSuggestion*>* FetchSuggestions() {
    __block bool suggestions_fetched = false;
    __block NSArray<CWVAutofillSuggestion*>* fetched_suggestions = nil;
    [autofill_controller_
        fetchSuggestionsForFormWithName:kTestFormName
                        fieldIdentifier:kTestAddressFieldID
                              fieldType:kTestFieldType
                                frameID:GetMainFrameId()
                      completionHandler:^(
                          NSArray<CWVAutofillSuggestion*>* suggestions) {
                        fetched_suggestions = suggestions;
                        suggestions_fetched = true;
                      }];
    EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForActionTimeout, ^bool {
      return suggestions_fetched;
    }));
    return fetched_suggestions;
  }

  NSString* GetMainFrameId() {
    if (main_frame_id_) {
      return main_frame_id_;
    }
    NSString* main_frame_id_script = @"__gCrWeb.message.getFrameId();";
    NSError* main_frame_id_error = nil;
    main_frame_id_ = test::EvaluateJavaScript(web_view_, main_frame_id_script,
                                              &main_frame_id_error);
    return main_frame_id_;
  }

  CWVAutofillController* autofill_controller_;
  NSString* main_frame_id_ = nil;
  UIView* dummy_super_view_ = nil;
};

// Tests that CWVAutofillControllerDelegate receives callbacks.
TEST_F(WebViewAutofillTest, TestDelegateCallbacks) {
  ASSERT_TRUE(test_server_->Start());
  ASSERT_TRUE(LoadTestPage());
  ASSERT_TRUE(SetFormFieldValue(kTestAddressFieldID, kTestAddressFieldValue));

  id delegate = OCMProtocolMock(@protocol(CWVAutofillControllerDelegate));
  autofill_controller_.delegate = delegate;

  [[delegate expect] autofillController:autofill_controller_
          didFocusOnFieldWithIdentifier:kTestAddressFieldID
                              fieldType:kTestFieldType
                               formName:kTestFormName
                                frameID:[OCMArg any]
                                  value:kTestAddressFieldValue];
  NSString* focus_script =
      [NSString stringWithFormat:@"document.getElementById('%@').focus();",
                                 kTestAddressFieldID];
  NSError* focus_error = nil;
  test::EvaluateJavaScript(web_view_, focus_script, &focus_error);
  ASSERT_NSEQ(nil, focus_error);
  [delegate verifyWithDelay:kWaitForActionTimeout];

  [[delegate expect] autofillController:autofill_controller_
           didBlurOnFieldWithIdentifier:kTestAddressFieldID
                              fieldType:kTestFieldType
                               formName:kTestFormName
                                frameID:[OCMArg any]
                                  value:kTestAddressFieldValue];
  NSString* blur_script =
      [NSString stringWithFormat:
                    @"var event = new Event('blur', {bubbles:true});"
                     "document.getElementById('%@').dispatchEvent(event);",
                    kTestAddressFieldID];
  NSError* blur_error = nil;
  test::EvaluateJavaScript(web_view_, blur_script, &blur_error);
  ASSERT_NSEQ(nil, blur_error);
  [delegate verifyWithDelay:kWaitForActionTimeout];

  [[delegate expect] autofillController:autofill_controller_
          didInputInFieldWithIdentifier:kTestAddressFieldID
                              fieldType:kTestFieldType
                               formName:kTestFormName
                                frameID:[OCMArg any]
                                  value:kTestAddressFieldValue];
  // The 'input' event listener defined in form.js is only called during the
  // bubbling phase.
  NSString* input_script =
      [NSString stringWithFormat:
                    @"var event = new Event('input', {'bubbles': true});"
                     "document.getElementById('%@').dispatchEvent(event);",
                    kTestAddressFieldID];
  NSError* input_error = nil;
  test::EvaluateJavaScript(web_view_, input_script, &input_error);
  ASSERT_NSEQ(nil, input_error);
  [delegate verifyWithDelay:kWaitForActionTimeout];

  [[delegate expect] autofillController:autofill_controller_
                  didSubmitFormWithName:kTestFormName
                          userInitiated:NO
                            isMainFrame:YES];
  // The 'submit' event listener defined in form.js is only called during the
  // bubbling phase.
  NSString* submit_script =
      [NSString stringWithFormat:
                    @"var event = new Event('submit', {'bubbles': true});"
                     "document.getElementById('%@').dispatchEvent(event);",
                    kTestFormID];
  NSError* submit_error = nil;
  test::EvaluateJavaScript(web_view_, submit_script, &submit_error);
  ASSERT_NSEQ(nil, submit_error);
  [delegate verifyWithDelay:kWaitForActionTimeout];
}

// Tests that CWVAutofillController can fetch, fill, and clear suggestions.
TEST_F(WebViewAutofillTest, TestSuggestionFetchFillClear) {
  ASSERT_TRUE(test_server_->Start());
  ASSERT_TRUE(LoadTestPage());
  ASSERT_TRUE(SetFormFieldValue(kTestNameFieldID, kTestNameFieldValue));
  ASSERT_TRUE(SetFormFieldValue(kTestAddressFieldID, kTestAddressFieldValue));
  ASSERT_TRUE(SetFormFieldValue(kTestStateFieldID, kTestStateFieldValue));
  ASSERT_TRUE(SetFormFieldValue(kTestCityFieldID, kTestCityFieldValue));
  ASSERT_TRUE(SetFormFieldValue(kTestZipFieldID, kTestZipFieldValue));
  ASSERT_TRUE(SubmitForm());
  ASSERT_TRUE(LoadTestPage());

  NSArray<CWVAutofillSuggestion*>* fetched_suggestions = FetchSuggestions();
  ASSERT_EQ(1U, fetched_suggestions.count);
  CWVAutofillSuggestion* fetched_suggestion = fetched_suggestions.firstObject;
  EXPECT_NSEQ(kTestAddressFieldValue, fetched_suggestion.value);
  EXPECT_NSEQ(kTestFormName, fetched_suggestion.formName);
  EXPECT_NSEQ(GetMainFrameId(), fetched_suggestion.frameID);

  // The input element needs to be focused before it can be filled or cleared.
  NSString* focus_script =
      [NSString stringWithFormat:@"document.getElementById('%@').focus()",
                                 kTestAddressFieldID];
  NSError* focus_error = nil;
  test::EvaluateJavaScript(web_view_, focus_script, &focus_error);
  ASSERT_NSEQ(nil, focus_error);

  [autofill_controller_ fillSuggestion:fetched_suggestion
                     completionHandler:nil];
  NSString* filled_script =
      [NSString stringWithFormat:@"document.getElementById('%@').value",
                                 kTestAddressFieldID];
  __block NSError* filled_error = nil;
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForActionTimeout, ^bool {
    NSString* filled_value =
        test::EvaluateJavaScript(web_view_, filled_script, &filled_error);
    // If there is an error, early return so the ASSERT catch the error.
    LOG(INFO) << base::SysNSStringToUTF8(filled_value);
    LOG(INFO) << base::SysNSStringToUTF8(fetched_suggestion.value);
    if (filled_error)
      return true;
    return [fetched_suggestion.value isEqualToString:filled_value];
  }));
  ASSERT_NSEQ(nil, filled_error);
  [autofill_controller_ clearFormWithName:kTestFormName
                          fieldIdentifier:kTestAddressFieldID
                                  frameID:GetMainFrameId()
                        completionHandler:nil];
  NSString* cleared_script =
      [NSString stringWithFormat:@"document.getElementById('%@').value",
                                 kTestAddressFieldID];
  __block NSError* cleared_error = nil;
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForActionTimeout, ^bool {
    NSString* current_value =
        test::EvaluateJavaScript(web_view_, cleared_script, &cleared_error);
    // If there is an error, early return so the ASSERT catch the error.
    if (cleared_error)
      return true;
    return [current_value isEqualToString:@""];
  }));
  ASSERT_NSEQ(nil, cleared_error);
}

// Tests that CWVAutofillController can remove a suggestion.
TEST_F(WebViewAutofillTest, DISABLED_TestSuggestionFetchRemoveFetch) {
  // TODO(crbug.com/898990): Needs to be reenabled.
  ASSERT_TRUE(test_server_->Start());
  ASSERT_TRUE(LoadTestPage());
  ASSERT_TRUE(SetFormFieldValue(kTestNameFieldID, kTestNameFieldValue));
  ASSERT_TRUE(SetFormFieldValue(kTestAddressFieldID, kTestAddressFieldValue));
  ASSERT_TRUE(SetFormFieldValue(kTestStateFieldID, kTestStateFieldValue));
  ASSERT_TRUE(SetFormFieldValue(kTestCityFieldID, kTestCityFieldValue));
  ASSERT_TRUE(SetFormFieldValue(kTestZipFieldID, kTestZipFieldValue));
  ASSERT_TRUE(SubmitForm());
  ASSERT_TRUE(LoadTestPage());

  NSArray* fetched_suggestions_after_creating = FetchSuggestions();
  ASSERT_EQ(1U, fetched_suggestions_after_creating.count);

  CWVAutofillSuggestion* suggestion_to_remove =
      fetched_suggestions_after_creating.firstObject;
  [autofill_controller_ removeSuggestion:suggestion_to_remove];

  NSArray* fetched_suggestions_after_removing = FetchSuggestions();
  ASSERT_EQ(0U, fetched_suggestions_after_removing.count);
}

}  // namespace ios_web_view
