// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/autofill/cwv_autofill_suggestion_internal.h"

#import <Foundation/Foundation.h>

#import "components/autofill/ios/browser/form_suggestion.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"

namespace ios_web_view {

using CWVAutofillSuggestionTest = PlatformTest;

// Tests CWVAutofillSuggestion initialization.
TEST_F(CWVAutofillSuggestionTest, Initialization) {
  NSString* formName = @"TestFormName";
  NSString* fieldIdentifier = @"TestFieldIdentifier";
  NSString* frameID = @"TestFrameID";
  FormSuggestion* formSuggestion = [FormSuggestion
      suggestionWithValue:@"TestValue"
       displayDescription:@"TestDisplayDescription"
                     icon:nil
                     type:autofill::SuggestionType::kAddressEntry
        backendIdentifier:nil
           requiresReauth:NO];
  CWVAutofillSuggestion* suggestion =
      [[CWVAutofillSuggestion alloc] initWithFormSuggestion:formSuggestion
                                                   formName:formName
                                            fieldIdentifier:fieldIdentifier
                                                    frameID:frameID
                                       isPasswordSuggestion:NO];
  EXPECT_NSEQ(formName, suggestion.formName);
  EXPECT_NSEQ(fieldIdentifier, suggestion.fieldIdentifier);
  EXPECT_NSEQ(frameID, suggestion.frameID);
  EXPECT_NSEQ(formSuggestion.displayDescription, suggestion.displayDescription);
  EXPECT_NSEQ(formSuggestion.value, suggestion.value);
  EXPECT_EQ(formSuggestion, suggestion.formSuggestion);
  EXPECT_EQ(CWVSuggestionTypeAddressEntry, suggestion.suggestionType);
  EXPECT_FALSE([suggestion isPasswordSuggestion]);
}

}  // namespace ios_web_view
