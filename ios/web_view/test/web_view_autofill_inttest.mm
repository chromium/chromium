// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <ChromeWebView/ChromeWebView.h>
#import <Foundation/Foundation.h>

#include "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#include "components/variations/variations_ids_provider.h"
#import "ios/web_view/public/cwv_navigation_delegate.h"
#import "ios/web_view/test/web_view_inttest_base.h"
#import "ios/web_view/test/web_view_test_util.h"
#import "net/base/apple/url_conversions.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest_mac.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#include "url/gurl.h"

using base::test::ios::kWaitForActionTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;
using base::test::ios::kWaitForPageLoadTimeout;

// A stub object that observes the |webViewDidFinishNavigation| event of
// CWVNavigationDelegate. CWVNavigationDelegate is also used as navigation
// policy decider, so OCMProtocolMock doesn't work here because it implements
// all protocol methods which will return NO and block the navigation.
@interface CWVNavigationPageLoadObserver : NSObject <CWVNavigationDelegate>

// Whether |webViewDidFinishNavigation| has been called. Initiated as NO.
@property(nonatomic, assign, readonly) BOOL pageLoaded;

- (void)webViewDidFinishNavigation:(CWVWebView*)webView;

@end

@implementation CWVNavigationPageLoadObserver

- (void)webViewDidFinishNavigation:(CWVWebView*)webView {
  _pageLoaded = YES;
}

@end

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
  WebViewAutofillTest()
      : autofill_controller_delegate_(
            OCMProtocolMock(@protocol(CWVAutofillControllerDelegate))) {
    data_source_ =
        OCMStrictProtocolMock(@protocol(CWVSyncControllerDataSource));
    OCMStub([data_source_ allKnownIdentities]).andReturn(@[]);
    CWVSyncController.dataSource = data_source_;
    autofill_controller_ = web_view_.autofillController;
    autofill_controller_.delegate = autofill_controller_delegate_;
  }

  void TearDown() override {
    [(id)data_source_ verify];
    [(id)autofill_controller_delegate_ verify];
  }

  // Loads a test page with a single form and waits until Autofill has parsed
  // that form.
  [[nodiscard]] bool LoadTestPage() {
    std::string html = base::SysNSStringToUTF8(kTestFormHtml);
    GURL url = GetUrlForPageWithHtmlBody(html);
    [[autofill_controller_delegate_ expect]
        autofillController:autofill_controller_
              didFindForms:[OCMArg any]
                   frameID:[OCMArg any]];
    if (!test::LoadUrl(web_view_, net::NSURLWithGURL(url))) {
      return false;
    }
    bool frame_appeared =
        WaitUntilConditionOrTimeout(kWaitForActionTimeout, ^bool {
          return !!GetMainFrameId();
        });
    if (!frame_appeared) {
      return false;
    }
    [autofill_controller_delegate_
        verifyWithDelay:kWaitForActionTimeout.InSecondsF()];
    return true;
  }

  [[nodiscard]] bool SubmitForm() {
    NSString* submit_script =
        [NSString stringWithFormat:@"document.getElementById('%@').click();",
                                   kTestSubmitID];
    NSError* error = nil;
    test::EvaluateJavaScript(web_view_, submit_script, &error);
    return !error;
  }

  [[nodiscard]] bool SetFormFieldValue(NSString* field_id,
                                       NSString* field_value) {
    NSString* set_value_script = [NSString
        stringWithFormat:@"document.getElementById('%@').value = '%@';",
                         field_id, field_value];
    NSError* error = nil;
    test::EvaluateJavaScript(web_view_, set_value_script, &error);
    return !error;
  }

  NSArray<CWVAutofillSuggestion*>* FetchSuggestions(NSString* main_frame_id) {
    __block bool suggestions_fetched = false;
    __block NSArray<CWVAutofillSuggestion*>* fetched_suggestions = nil;
    [autofill_controller_
        fetchSuggestionsForFormWithName:kTestFormName
                        fieldIdentifier:kTestAddressFieldID
                              fieldType:kTestFieldType
                                frameID:main_frame_id
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
    NSString* main_frame_id_script = @"__gCrWeb.message.getFrameId();";
    return test::EvaluateJavaScript(web_view_, main_frame_id_script);
  }

  bool WaitUntilPageLoaded() {
    CWVNavigationPageLoadObserver* observer =
        [[CWVNavigationPageLoadObserver alloc] init];
    web_view_.navigationDelegate = observer;
    bool result = WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
      return observer.pageLoaded;
    });
    web_view_.navigationDelegate = nil;
    return result;
  }

  CWVAutofillController* autofill_controller_;
  id autofill_controller_delegate_ = nil;
  id<CWVNavigationDelegate> navigation_delegate_ = nil;
  UIView* dummy_super_view_ = nil;
  id<CWVSyncControllerDataSource> data_source_;
};

// Tests that CWVAutofillControllerDelegate receives callbacks.
TEST_F(WebViewAutofillTest, TestDelegateCallbacks) {
  ASSERT_TRUE(variations::VariationsIdsProvider::GetInstance());
  ASSERT_TRUE(test_server_->Start());
  ASSERT_TRUE(LoadTestPage());
  ASSERT_TRUE(SetFormFieldValue(kTestAddressFieldID, kTestAddressFieldValue));

  [[autofill_controller_delegate_ expect]
                 autofillController:autofill_controller_
      didFocusOnFieldWithIdentifier:kTestAddressFieldID
                          fieldType:kTestFieldType
                           formName:kTestFormName
                            frameID:[OCMArg any]
                              value:kTestAddressFieldValue
                      userInitiated:YES];
  NSString* focus_script =
      [NSString stringWithFormat:@"document.getElementById('%@').focus();",
                                 kTestAddressFieldID];
  NSError* focus_error = nil;
  test::EvaluateJavaScript(web_view_, focus_script, &focus_error);
  ASSERT_FALSE(focus_error);
  [autofill_controller_delegate_
      verifyWithDelay:kWaitForActionTimeout.InSecondsF()];

  [[autofill_controller_delegate_ expect]
                autofillController:autofill_controller_
      didBlurOnFieldWithIdentifier:kTestAddressFieldID
                         fieldType:kTestFieldType
                          formName:kTestFormName
                           frameID:[OCMArg any]
                             value:kTestAddressFieldValue
                     userInitiated:NO];
  NSString* blur_script =
      [NSString stringWithFormat:
                    @"var event = new Event('blur', {bubbles:true});"
                     "document.getElementById('%@').dispatchEvent(event);",
                    kTestAddressFieldID];
  NSError* blur_error = nil;
  test::EvaluateJavaScript(web_view_, blur_script, &blur_error);
  ASSERT_FALSE(blur_error);
  [autofill_controller_delegate_
      verifyWithDelay:kWaitForActionTimeout.InSecondsF()];

  [[autofill_controller_delegate_ expect]
                 autofillController:autofill_controller_
      didInputInFieldWithIdentifier:kTestAddressFieldID
                          fieldType:kTestFieldType
                           formName:kTestFormName
                            frameID:[OCMArg any]
                              value:kTestAddressFieldValue
                      userInitiated:NO];
  // The 'input' event listener defined in form.js is only called during the
  // bubbling phase.
  NSString* input_script =
      [NSString stringWithFormat:
                    @"var event = new Event('input', {'bubbles': true});"
                     "document.getElementById('%@').dispatchEvent(event);",
                    kTestAddressFieldID];
  NSError* input_error = nil;
  test::EvaluateJavaScript(web_view_, input_script, &input_error);
  ASSERT_FALSE(input_error);
  [autofill_controller_delegate_
      verifyWithDelay:kWaitForActionTimeout.InSecondsF()];

  // TODO(crbug.com/40911875): `userInitiated` flipped from `NO` in iOS 16.1 to
  // `YES` in 16.4, so we cannot reliably verify it until the bug is fixed.
  [[[autofill_controller_delegate_ expect] ignoringNonObjectArgs]
         autofillController:autofill_controller_
      didSubmitFormWithName:kTestFormName
                    frameID:[OCMArg any]
              userInitiated:[OCMArg any]];
  // The 'submit' event listener defined in form.js is only called during the
  // bubbling phase.
  NSString* submit_script =
      [NSString stringWithFormat:
                    @"var event = new Event('submit', {'bubbles': true});"
                     "document.getElementById('%@').dispatchEvent(event);",
                    kTestFormID];
  NSError* submit_error = nil;
  test::EvaluateJavaScript(web_view_, submit_script, &submit_error);
  ASSERT_FALSE(submit_error);
  [autofill_controller_delegate_
      verifyWithDelay:kWaitForActionTimeout.InSecondsF()];
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

  // Stub the confirm save callback to save the new profile right away.
  void (^invocation_handler)(NSInvocation*) = ^(NSInvocation* invocation) {
    void (^decision_handler)(CWVAutofillProfileUserDecision);
    [invocation getArgument:&decision_handler atIndex:5];
    decision_handler(CWVAutofillProfileUserDecisionAccepted);
  };
  [[[autofill_controller_delegate_ stub] andDo:invocation_handler]
                    autofillController:autofill_controller_
      confirmSaveForNewAutofillProfile:[OCMArg any]
                            oldProfile:[OCMArg any]
                       decisionHandler:[OCMArg any]];
  ASSERT_TRUE(SubmitForm());

  // Wait for about:blank to be loaded after <form> submitted.
  ASSERT_TRUE(WaitUntilPageLoaded());

  ASSERT_TRUE(LoadTestPage());

  __block NSString* main_frame_id = nil;

  // The input element needs to be focused before suggestions can be fetched.
  [[autofill_controller_delegate_ expect]
                 autofillController:autofill_controller_
      didFocusOnFieldWithIdentifier:kTestAddressFieldID
                          fieldType:kTestFieldType
                           formName:kTestFormName
                            frameID:[OCMArg checkWithBlock:^BOOL(id frameId) {
                              main_frame_id = frameId;
                              return frameId != nil;
                            }]
                              value:[OCMArg any]
                      userInitiated:YES];
  NSString* focus_script =
      [NSString stringWithFormat:@"document.getElementById('%@').focus()",
                                 kTestAddressFieldID];
  NSError* focus_error = nil;
  test::EvaluateJavaScript(web_view_, focus_script, &focus_error);
  ASSERT_TRUE(!focus_error);
  [autofill_controller_delegate_
      verifyWithDelay:kWaitForActionTimeout.InSecondsF()];

  NSArray<CWVAutofillSuggestion*>* fetched_suggestions =
      FetchSuggestions(main_frame_id);
  ASSERT_EQ(1U, fetched_suggestions.count);
  CWVAutofillSuggestion* fetched_suggestion = fetched_suggestions.firstObject;
  EXPECT_NSEQ(kTestAddressFieldValue, fetched_suggestion.value);
  EXPECT_NSEQ(kTestFormName, fetched_suggestion.formName);
  EXPECT_NSEQ(main_frame_id, fetched_suggestion.frameID);

  [autofill_controller_ acceptSuggestion:fetched_suggestion
                                 atIndex:0
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
  ASSERT_FALSE(filled_error);
  [autofill_controller_ clearFormWithName:kTestFormName
                          fieldIdentifier:kTestAddressFieldID
                                  frameID:main_frame_id
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
  EXPECT_FALSE(cleared_error);
}

}  // namespace ios_web_view
