// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/form_input_accessory/ui/form_suggestion_label.h"

#import <UIKit/UIKit.h>

#import "components/autofill/ios/browser/form_suggestion.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

using autofill::Suggestion;
using autofill::SuggestionType;

namespace {

// Creates a `FormSuggestion` with the given `type`.
FormSuggestion* CreateFormSuggestion(SuggestionType type) {
  return [FormSuggestion suggestionWithValue:@"Test"
                          displayDescription:@""
                                        icon:nil
                                        type:type
                                     payload:Suggestion::Payload()
                              requiresReauth:NO];
}

}  // namespace

using FormSuggestionLabelTest = PlatformTest;

// Test that a loading suggestion (`SuggestionType::kFetchingAmbientData`) has
// user interaction disabled while a regular suggestion has it enabled.
TEST_F(FormSuggestionLabelTest, FetchingAmbientDataDisablesUserInteraction) {
  FormSuggestion* loadingSuggestion =
      CreateFormSuggestion(SuggestionType::kFetchingAmbientData);
  FormSuggestionLabel* loadingLabel =
      [[FormSuggestionLabel alloc] initWithSuggestion:loadingSuggestion
                                                index:0
                                  numberOfSuggestions:1
                                accessoryTrailingView:nil
                                 isContextMenuEnabled:NO
                                            isCompact:YES
                                             delegate:nil];
  EXPECT_FALSE(loadingLabel.userInteractionEnabled);

  FormSuggestion* regularSuggestion =
      CreateFormSuggestion(SuggestionType::kAutocompleteEntry);
  FormSuggestionLabel* regularLabel =
      [[FormSuggestionLabel alloc] initWithSuggestion:regularSuggestion
                                                index:0
                                  numberOfSuggestions:1
                                accessoryTrailingView:nil
                                 isContextMenuEnabled:NO
                                            isCompact:YES
                                             delegate:nil];
  EXPECT_TRUE(regularLabel.userInteractionEnabled);
}
