// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/autofill/ios/browser/js_autofill_manager.h"

#import <Foundation/Foundation.h>

#import "base/test/ios/wait_util.h"
#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_features.h"
#import "components/autofill/ios/browser/js_autofill_manager.h"
#include "ios/chrome/browser/web/chrome_web_client.h"
#import "ios/chrome/browser/web/chrome_web_test.h"
#import "ios/web/public/deprecated/crw_js_injection_receiver.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/test/js_test_util.h"
#import "ios/web/public/web_state.h"
#import "testing/gtest_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

NSString* const kUnownedUntitledFormHtml =
    @"<INPUT type='text' id='firstname'/>"
     "<INPUT type='text' id='lastname'/>"
     "<INPUT type='hidden' id='imhidden'/>"
     "<INPUT type='text' id='notempty' value='Hi'/>"
     "<INPUT type='text' autocomplete='off' id='noautocomplete'/>"
     "<INPUT type='text' disabled='disabled' id='notenabled'/>"
     "<INPUT type='text' readonly id='readonly'/>"
     "<INPUT type='text' style='visibility: hidden'"
     "       id='invisible'/>"
     "<INPUT type='text' style='display: none' id='displaynone'/>"
     "<INPUT type='month' id='month'/>"
     "<INPUT type='month' id='month-nonempty' value='2011-12'/>"
     "<SELECT id='select'>"
     "  <OPTION></OPTION>"
     "  <OPTION value='CA'>California</OPTION>"
     "  <OPTION value='TX'>Texas</OPTION>"
     "</SELECT>"
     "<SELECT id='select-nonempty'>"
     "  <OPTION value='CA' selected>California</OPTION>"
     "  <OPTION value='TX'>Texas</OPTION>"
     "</SELECT>"
     "<SELECT id='select-unchanged'>"
     "  <OPTION value='CA' selected>California</OPTION>"
     "  <OPTION value='TX'>Texas</OPTION>"
     "</SELECT>"
     "<SELECT id='select-displaynone' style='display:none'>"
     "  <OPTION value='CA' selected>California</OPTION>"
     "  <OPTION value='TX'>Texas</OPTION>"
     "</SELECT>"
     "<TEXTAREA id='textarea'></TEXTAREA>"
     "<TEXTAREA id='textarea-nonempty'>Go&#10;away!</TEXTAREA>"
     "<INPUT type='submit' name='reply-send' value='Send'/>";

NSNumber* GetDefaultMaxLength() {
  return @524288;
}

// Text fixture to test JsAutofillManager.
class JsAutofillManagerTest : public ChromeWebTest {
 protected:
  JsAutofillManagerTest()
      : ChromeWebTest(std::make_unique<ChromeWebClient>()) {}

  // Loads the given HTML and initializes the Autofill JS scripts.
  void LoadHtml(NSString* html) {
    ChromeWebTest::LoadHtml(html);
    manager_ = [[JsAutofillManager alloc]
        initWithReceiver:web_state()->GetJSInjectionReceiver()];
  }

  web::WebFrame* main_web_frame() {
    return web_state()->GetWebFramesManager()->GetMainWebFrame();
  }

  // Testable autofill manager.
  JsAutofillManager* manager_;
};

// Tests that |hasBeenInjected| returns YES after |inject| call.
TEST_F(JsAutofillManagerTest, InitAndInject) {
  LoadHtml(@"<html></html>");
  EXPECT_NSEQ(@"object", ExecuteJavaScript(@"typeof __gCrWeb.autofill"));
}

// Tests forms extraction method
// (fetchFormsWithRequirements:minimumRequiredFieldsCount:completionHandler:).
TEST_F(JsAutofillManagerTest, ExtractForms) {
  LoadHtml(@"<html><body><form name='testform' method='post'>"
            "<div id='div1'>Last Name</div>"
            "<div id='div2'>Email Address</div>"
            "<input type='text' id='firstname' name='firstname'/"
            "    aria-label='First Name'>"
            "<input type='text' id='lastname' name='lastname'"
            "    aria-labelledby='div1'/>"
            "<input type='email' id='email' name='email'"
            "    aria-describedby='div2'/>"
            "</form>"
            "</body></html>");

  NSDictionary* expected = @{
    @"name" : @"testform",
    @"fields" : @[
      @{
        @"aria_description" : @"",
        @"aria_label" : @"First Name",
        @"name" : @"firstname",
        @"name_attribute" : @"firstname",
        @"id_attribute" : @"firstname",
        @"identifier" : @"firstname",
        @"form_control_type" : @"text",
        @"max_length" : GetDefaultMaxLength(),
        @"should_autocomplete" : @true,
        @"is_checkable" : @false,
        @"is_focusable" : @true,
        @"value" : @"",
        @"label" : @"First Name"
      },
      @{
        @"aria_description" : @"",
        @"aria_label" : @"Last Name",
        @"name" : @"lastname",
        @"name_attribute" : @"lastname",
        @"id_attribute" : @"lastname",
        @"identifier" : @"lastname",
        @"form_control_type" : @"text",
        @"max_length" : GetDefaultMaxLength(),
        @"should_autocomplete" : @true,
        @"is_checkable" : @false,
        @"is_focusable" : @true,
        @"value" : @"",
        @"label" : @""
      },
      @{
        @"aria_description" : @"Email Address",
        @"aria_label" : @"",
        @"name" : @"email",
        @"name_attribute" : @"email",
        @"id_attribute" : @"email",
        @"identifier" : @"email",
        @"form_control_type" : @"email",
        @"max_length" : GetDefaultMaxLength(),
        @"should_autocomplete" : @true,
        @"is_checkable" : @false,
        @"is_focusable" : @true,
        @"value" : @"",
        @"label" : @""
      }
    ]
  };

  __block BOOL block_was_called = NO;
  __block NSString* result;
  [manager_ fetchFormsWithMinimumRequiredFieldsCount:
                autofill::MinRequiredFieldsForHeuristics()
                                             inFrame:main_web_frame()
                                   completionHandler:^(NSString* actualResult) {
                                     block_was_called = YES;
                                     result = [actualResult copy];
                                   }];
  base::test::ios::WaitUntilCondition(^bool() {
    return block_was_called;
  });

  NSArray* resultArray = [NSJSONSerialization
      JSONObjectWithData:[result dataUsingEncoding:NSUTF8StringEncoding]
                 options:0
                   error:nil];
  EXPECT_NSNE(nil, resultArray);

  NSDictionary* form = [resultArray firstObject];
  [expected enumerateKeysAndObjectsUsingBlock:^(id key, id obj, BOOL* stop) {
    EXPECT_NSEQ(form[key], obj);
  }];
}

// Tests forms extraction method
// (fetchFormsWithRequirements:minimumRequiredFieldsCount:completionHandler:).
TEST_F(JsAutofillManagerTest, ExtractForms2) {
  LoadHtml(@"<html><body><form name='testform' method='post'>"
            "<input type='text' id='firstname' name='firstname'/"
            "    aria-label='First Name'>"
            "<input type='text' id='lastname' name='lastname'"
            "    aria-labelledby='div1'/>"
            "<input type='email' id='email' name='email'"
            "    aria-describedby='div2'/>"
            "</form>"
            "<div id='div1'>Last Name</div>"
            "<div id='div2'>Email Address</div>"
            "</body></html>");

  NSDictionary* expected = @{
    @"name" : @"testform",
    @"fields" : @[
      @{
        @"aria_description" : @"",
        @"aria_label" : @"First Name",
        @"name" : @"firstname",
        @"name_attribute" : @"firstname",
        @"id_attribute" : @"firstname",
        @"identifier" : @"firstname",
        @"form_control_type" : @"text",
        @"max_length" : GetDefaultMaxLength(),
        @"should_autocomplete" : @true,
        @"is_checkable" : @false,
        @"is_focusable" : @true,
        @"value" : @"",
        @"label" : @"First Name"
      },
      @{
        @"aria_description" : @"",
        @"aria_label" : @"Last Name",
        @"name" : @"lastname",
        @"name_attribute" : @"lastname",
        @"id_attribute" : @"lastname",
        @"identifier" : @"lastname",
        @"form_control_type" : @"text",
        @"max_length" : GetDefaultMaxLength(),
        @"should_autocomplete" : @true,
        @"is_checkable" : @false,
        @"is_focusable" : @true,
        @"value" : @"",
        @"label" : @""
      },
      @{
        @"aria_description" : @"Email Address",
        @"aria_label" : @"",
        @"name" : @"email",
        @"name_attribute" : @"email",
        @"id_attribute" : @"email",
        @"identifier" : @"email",
        @"form_control_type" : @"email",
        @"max_length" : GetDefaultMaxLength(),
        @"should_autocomplete" : @true,
        @"is_checkable" : @false,
        @"is_focusable" : @true,
        @"value" : @"",
        @"label" : @""
      }
    ]
  };

  __block BOOL block_was_called = NO;
  __block NSString* result;
  [manager_ fetchFormsWithMinimumRequiredFieldsCount:
                autofill::MinRequiredFieldsForHeuristics()
                                             inFrame:main_web_frame()
                                   completionHandler:^(NSString* actualResult) {
                                     block_was_called = YES;
                                     result = [actualResult copy];
                                   }];
  base::test::ios::WaitUntilCondition(^bool() {
    return block_was_called;
  });

  NSArray* resultArray = [NSJSONSerialization
      JSONObjectWithData:[result dataUsingEncoding:NSUTF8StringEncoding]
                 options:0
                   error:nil];
  EXPECT_NSNE(nil, resultArray);

  NSDictionary* form = [resultArray firstObject];
  [expected enumerateKeysAndObjectsUsingBlock:^(id key, id obj, BOOL* stop) {
    EXPECT_NSEQ(form[key], obj);
  }];
}

// Tests forms extraction method
// (fetchFormsWithRequirements:minimumRequiredFieldsCount:completionHandler:)
// when formless forms are restricted to checkout flows. No form is expected to
// be extracted here.
TEST_F(JsAutofillManagerTest, ExtractFormlessForms_RestrictToFormlessCheckout) {
  // Restrict formless forms to checkout flows.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      autofill::features::kAutofillRestrictUnownedFieldsToFormlessCheckout);

  LoadHtml(kUnownedUntitledFormHtml);

  __block BOOL block_was_called = NO;
  __block NSString* result;
  [manager_ fetchFormsWithMinimumRequiredFieldsCount:
                autofill::MinRequiredFieldsForHeuristics()
                                             inFrame:main_web_frame()
                                   completionHandler:^(NSString* actualResult) {
                                     block_was_called = YES;
                                     result = [actualResult copy];
                                   }];
  base::test::ios::WaitUntilCondition(^bool() {
    return block_was_called;
  });

  // Verify that the form is empty.
  NSArray* resultArray = [NSJSONSerialization
      JSONObjectWithData:[result dataUsingEncoding:NSUTF8StringEncoding]
                 options:0
                   error:nil];
  EXPECT_NSNE(nil, resultArray);
  EXPECT_EQ(0u, resultArray.count);
}

// Tests forms extraction method
// (fetchFormsWithRequirements:minimumRequiredFieldsCount:completionHandler:)
// when all formless forms are extracted. A formless form is expected to be
// extracted here.
TEST_F(JsAutofillManagerTest, ExtractFormlessForms_AllFormlessForms) {
  // Allow all formless forms to be extracted.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      autofill::features::kAutofillRestrictUnownedFieldsToFormlessCheckout);

  LoadHtml(kUnownedUntitledFormHtml);

  __block BOOL block_was_called = NO;
  __block NSString* result;
  [manager_ fetchFormsWithMinimumRequiredFieldsCount:
                autofill::MinRequiredFieldsForHeuristics()
                                             inFrame:main_web_frame()
                                   completionHandler:^(NSString* actualResult) {
                                     block_was_called = YES;
                                     result = [actualResult copy];
                                   }];
  base::test::ios::WaitUntilCondition(^bool() {
    return block_was_called;
  });

  // Verify that the form is non-empty.
  NSArray* resultArray = [NSJSONSerialization
      JSONObjectWithData:[result dataUsingEncoding:NSUTF8StringEncoding]
                 options:0
                   error:nil];
  EXPECT_NSNE(nil, resultArray);
  EXPECT_NE(0u, resultArray.count);
}

// Tests form filling (fillActiveFormField:completionHandler:) method.
TEST_F(JsAutofillManagerTest, FillActiveFormField) {
  LoadHtml(
      @"<html><body><form name='testform' method='post'>"
       "<input type='email' id='email' name='email'/>"
       "</form></body></html>");

  NSString* get_element_javascript = @"document.getElementsByName('email')[0]";
  NSString* focus_element_javascript =
      [NSString stringWithFormat:@"%@.focus()", get_element_javascript];
  ExecuteJavaScript(focus_element_javascript);
  auto data = std::make_unique<base::DictionaryValue>();
  data->SetString("name", "email");
  data->SetString("identifier", "email");
  data->SetString("value", "newemail@com");
  __block BOOL block_was_called = NO;
  [manager_
      fillActiveFormField:std::move(data)
                  inFrame:web_state()->GetWebFramesManager()->GetMainWebFrame()
        completionHandler:^{
          block_was_called = YES;
        }];
  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForActionTimeout, ^bool() {
        return block_was_called;
      }));
  NSString* element_value_javascript =
      [NSString stringWithFormat:@"%@.value", get_element_javascript];
  EXPECT_NSEQ(@"newemail@com", ExecuteJavaScript(element_value_javascript));
}

// Tests the generation of the name of the fields.
TEST_F(JsAutofillManagerTest, TestExtractedFieldsNames) {
  LoadHtml(
      @"<html><body><form name='testform' method='post'>"
       "<input type='text' name='field_with_name'/>"
       "<input type='text' id='field_with_id'/>"
       "<input type='text' id='field_id' name='field_name'/>"
       "<input type='text'/>"
       "</form></body></html>");
  NSArray* expected_names =
      @[ @"field_with_name", @"field_with_id", @"field_name", @"" ];

  __block BOOL block_was_called = NO;
  __block NSString* result;
  [manager_ fetchFormsWithMinimumRequiredFieldsCount:
                autofill::MinRequiredFieldsForHeuristics()
                                             inFrame:main_web_frame()
                                   completionHandler:^(NSString* actualResult) {
                                     block_was_called = YES;
                                     result = [actualResult copy];
                                   }];
  base::test::ios::WaitUntilCondition(^bool() {
    return block_was_called;
  });

  NSArray* resultArray = [NSJSONSerialization
      JSONObjectWithData:[result dataUsingEncoding:NSUTF8StringEncoding]
                 options:0
                   error:nil];
  EXPECT_NSNE(nil, resultArray);

  NSArray* fields = [resultArray firstObject][@"fields"];
  EXPECT_EQ([fields count], [expected_names count]);
  for (NSUInteger i = 0; i < [fields count]; i++) {
    EXPECT_NSEQ(fields[i][@"name"], expected_names[i]);
  }
}

// Tests the generation of the name of the fields.
TEST_F(JsAutofillManagerTest, TestExtractedFieldsIDs) {
  // Allow all formless forms to be extracted.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      autofill::features::kAutofillRestrictUnownedFieldsToFormlessCheckout);

  NSString* HTML =
      @"<html><body><form name='testform' method='post'>"
       // Field with name and id
       "<input type='text' id='field0_id' name='field0_name'/>"
       // Field with id
       "<input type='text' id='field1_id'/>"
       // Field without id but in form and with name
       "<input type='text' name='field2_name'/>"
       // Field without id but in form and without name
       "<input type='text'/>"
       "</form>"
       // Field with name and id
       "<input type='text' id='field4_id' name='field4_name'/>"
       // Field with id
       "<input type='text' id='field5_id'/>"
       // Field without id, not in form and with name. Will be identified
       // as 6th input field in document.
       "<input type='text' name='field6_name'/>"
       // Field without id, not in form and without name. Will be
       // identified as 7th input field in document.
       "<input type='text'/>"
       // Field without id, not in form and with name. Will be
       // identified as 1st select field in document.
       "<select name='field8_name'></select>"
       // Field without id, not in form and with name. Will be
       // identified as input 0 field in #div_id.
       "<div id='div_id'><input type='text' name='field9_name'/></div>"
       "</body></html>";
  LoadHtml(HTML);
  NSArray* owned_expected_ids =
      @[ @"field0_id", @"field1_id", @"field2_name", @"gChrome~field~3" ];
  NSArray* unowned_expected_ids = @[
    @"field4_id", @"field5_id", @"gChrome~field~~INPUT~6",
    @"gChrome~field~~INPUT~7", @"gChrome~field~~SELECT~0",
    @"gChrome~field~#div_id~INPUT~0"
  ];

  __block BOOL block_was_called = NO;
  __block NSString* result;
  [manager_ fetchFormsWithMinimumRequiredFieldsCount:
                autofill::MinRequiredFieldsForHeuristics()
                                             inFrame:main_web_frame()
                                   completionHandler:^(NSString* actualResult) {
                                     block_was_called = YES;
                                     result = [actualResult copy];
                                   }];
  base::test::ios::WaitUntilCondition(^bool() {
    return block_was_called;
  });

  NSArray* resultArray = [NSJSONSerialization
      JSONObjectWithData:[result dataUsingEncoding:NSUTF8StringEncoding]
                 options:0
                   error:nil];
  EXPECT_NSNE(nil, resultArray);

  NSArray* owned_fields = [resultArray objectAtIndex:0][@"fields"];
  EXPECT_EQ([owned_fields count], [owned_expected_ids count]);
  for (NSUInteger i = 0; i < [owned_fields count]; i++) {
    EXPECT_NSEQ(owned_fields[i][@"identifier"], owned_expected_ids[i]);
  }
  NSArray* unowned_fields = [resultArray objectAtIndex:1][@"fields"];
  EXPECT_EQ([unowned_fields count], [unowned_expected_ids count]);
  for (NSUInteger i = 0; i < [unowned_fields count]; i++) {
    EXPECT_NSEQ(unowned_fields[i][@"identifier"], unowned_expected_ids[i]);
  }
}

}  // namespace
