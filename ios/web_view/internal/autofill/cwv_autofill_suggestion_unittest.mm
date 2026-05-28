// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>

#import "base/strings/sys_string_conversions.h"
#import "components/autofill/ios/browser/form_suggestion.h"
#import "ios/web_view/internal/autofill/cwv_autofill_suggestion_internal.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace ios_web_view {

using CWVAutofillSuggestionTest = PlatformTest;

// Tests CWVAutofillSuggestion initialization.
TEST_F(CWVAutofillSuggestionTest, Initialization) {
  NSString* formName = @"TestFormName";
  NSString* fieldIdentifier = @"TestFieldIdentifier";
  NSString* frameID = @"TestFrameID";
  autofill::FormRendererId formRendererID(12);
  autofill::FieldRendererId fieldRendererID(34);
  FormSuggestion* formSuggestion = [FormSuggestion
      suggestionWithValue:@"TestValue"
       displayDescription:@"TestDisplayDescription"
                     icon:nil
                     type:autofill::SuggestionType::kAddressEntry
                  payload:autofill::Suggestion::Payload()
           requiresReauth:NO];
  CWVAutofillSuggestion* suggestion =
      [[CWVAutofillSuggestion alloc] initWithFormSuggestion:formSuggestion
                                                   formName:formName
                                             formRendererID:formRendererID
                                            fieldIdentifier:fieldIdentifier
                                            fieldRendererID:fieldRendererID
                                                    frameID:frameID
                                       isPasswordSuggestion:NO];
  EXPECT_NSEQ(formName, suggestion.formName);
  EXPECT_EQ(formRendererID, suggestion.formRendererID);
  EXPECT_NSEQ(fieldIdentifier, suggestion.fieldIdentifier);
  EXPECT_EQ(fieldRendererID, suggestion.fieldRendererID);
  EXPECT_NSEQ(frameID, suggestion.frameID);
  EXPECT_NSEQ(formSuggestion.displayDescription, suggestion.displayDescription);
  EXPECT_NSEQ(formSuggestion.value, suggestion.value);
  EXPECT_EQ(formSuggestion, suggestion.formSuggestion);
  EXPECT_EQ(CWVSuggestionTypeAddressEntry, suggestion.suggestionType);
  EXPECT_FALSE([suggestion isPasswordSuggestion]);
}

// Tests CWVAutofillSuggestion initialization with GUID payload.
TEST_F(CWVAutofillSuggestionTest, GUIDPayload) {
  NSString* guid = @"TestGUID";
  FormSuggestion* formSuggestion = [FormSuggestion
      suggestionWithValue:@"TestValue"
       displayDescription:@"TestDisplayDescription"
                     icon:nil
                     type:autofill::SuggestionType::kAddressEntry
                  payload:autofill::Suggestion::Guid("TestGUID")
           requiresReauth:NO];
  CWVAutofillSuggestion* suggestion = [[CWVAutofillSuggestion alloc]
      initWithFormSuggestion:formSuggestion
                    formName:@"TestFormName"
              formRendererID:autofill::FormRendererId(12)
             fieldIdentifier:@"TestFieldIdentifier"
             fieldRendererID:autofill::FieldRendererId(34)
                     frameID:@"TestFrameID"
        isPasswordSuggestion:NO];
  EXPECT_NSEQ(guid, suggestion.GUID);
}

}  // namespace ios_web_view
