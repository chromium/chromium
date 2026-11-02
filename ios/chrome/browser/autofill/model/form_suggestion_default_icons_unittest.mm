// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/model/form_suggestion_default_icons.h"

#import <UIKit/UIKit.h>

#import "components/autofill/core/browser/suggestions/suggestion_type.h"
#import "components/autofill/ios/browser/form_suggestion.h"
#import "components/autofill/ios/browser/form_suggestion_provider.h"
#import "components/autofill/ios/form_util/form_activity_params.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

namespace {

using FormSuggestionDefaultIconsTest = PlatformTest;

// Tests that `CopyFormSuggestionsWithDefaultIcons` attaches a default icon when
// `suggestion.icon` is nil and preserves `params`, `provider`, `metadata`,
// `featureForIPH`, and `suggestionIconType`.
TEST_F(FormSuggestionDefaultIconsTest,
       AttachesDefaultIconAndPreservesParamsAndProvider) {
  FormSuggestionMetadata metadata;
  metadata.popup_suggestion_index = 2;

  FormSuggestion* suggestion =
      [FormSuggestion suggestionWithValue:@"Undo"
                               minorValue:nil
                       displayDescription:nil
                                     icon:nil
                                     type:autofill::SuggestionType::kUndo
                                  payload:autofill::Suggestion::Payload()
              fieldByFieldFillingTypeUsed:autofill::FieldType::EMPTY_TYPE
                           requiresReauth:NO
               acceptanceA11yAnnouncement:nil
                                 metadata:metadata];
  suggestion.featureForIPH =
      SuggestionFeatureForIPH::kHomeAndWorkAddressSuggestion;
  suggestion.suggestionIconType = SuggestionIconType::kUndoAutofill;

  autofill::FormActivityParams params;
  params.form_name = "test_form";
  params.field_identifier = "test_field";
  id<FormSuggestionProvider> mock_provider =
      OCMProtocolMock(@protocol(FormSuggestionProvider));
  suggestion = [FormSuggestion copy:suggestion
                       andSetParams:params
                           provider:mock_provider];

  NSArray<FormSuggestion*>* result = CopyFormSuggestionsWithDefaultIcons(
      @[ suggestion ], /*web_state=*/nullptr);
  ASSERT_EQ(1u, result.count);
  FormSuggestion* copied = result[0];
  EXPECT_NE(nil, copied.icon);
  ASSERT_TRUE(copied.params.has_value());
  EXPECT_EQ("test_form", copied.params->form_name);
  EXPECT_EQ("test_field", copied.params->field_identifier);
  EXPECT_EQ(mock_provider, copied.provider);
  EXPECT_EQ(std::optional<size_t>(2), copied.metadata.popup_suggestion_index);
  EXPECT_EQ(SuggestionFeatureForIPH::kHomeAndWorkAddressSuggestion,
            copied.featureForIPH);
  EXPECT_EQ(SuggestionIconType::kUndoAutofill, copied.suggestionIconType);
}

// Tests that `CopyFormSuggestionsWithDefaultIcons` preserves a pre-existing
// icon on the suggestion and does not overwrite it with a default icon or
// perform an unnecessary copy.
TEST_F(FormSuggestionDefaultIconsTest, PreservesExistingIcon) {
  UIImage* custom_icon = [[UIImage alloc] init];
  FormSuggestion* suggestion =
      [FormSuggestion suggestionWithValue:@"Custom Suggestion"
                               minorValue:nil
                       displayDescription:nil
                                     icon:custom_icon
                                     type:autofill::SuggestionType::kUndo
                                  payload:autofill::Suggestion::Payload()
              fieldByFieldFillingTypeUsed:autofill::FieldType::EMPTY_TYPE
                           requiresReauth:NO
               acceptanceA11yAnnouncement:nil
                                 metadata:FormSuggestionMetadata()];

  NSArray<FormSuggestion*>* result = CopyFormSuggestionsWithDefaultIcons(
      @[ suggestion ], /*web_state=*/nullptr);
  ASSERT_EQ(1u, result.count);
  EXPECT_EQ(suggestion, result[0]);
  EXPECT_EQ(custom_icon, result[0].icon);
}

// Tests that `CopyFormSuggestionsWithDefaultIcons` leaves `icon` as nil when
// the suggestion type does not define a default icon.
TEST_F(FormSuggestionDefaultIconsTest, LeavesIconNilWhenNoDefaultIconExists) {
  FormSuggestion* suggestion = [FormSuggestion
              suggestionWithValue:@"Autocomplete"
                       minorValue:nil
               displayDescription:nil
                             icon:nil
                             type:autofill::SuggestionType::kAutocompleteEntry
                          payload:autofill::Suggestion::Payload()
      fieldByFieldFillingTypeUsed:autofill::FieldType::EMPTY_TYPE
                   requiresReauth:NO
       acceptanceA11yAnnouncement:nil
                         metadata:FormSuggestionMetadata()];

  NSArray<FormSuggestion*>* result = CopyFormSuggestionsWithDefaultIcons(
      @[ suggestion ], /*web_state=*/nullptr);
  ASSERT_EQ(1u, result.count);
  EXPECT_EQ(suggestion, result[0]);
  EXPECT_EQ(nil, result[0].icon);
}

}  // namespace
