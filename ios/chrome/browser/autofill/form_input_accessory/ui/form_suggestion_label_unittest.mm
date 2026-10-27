// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/form_input_accessory/ui/form_suggestion_label.h"

#import <UIKit/UIKit.h>

#import "components/autofill/ios/browser/form_suggestion.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "ui/base/l10n/l10n_util.h"

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
// user interaction disabled and accessibility properties configured.
TEST_F(FormSuggestionLabelTest,
       FetchingAmbientDataAccessibilityAndInteraction) {
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
  EXPECT_NSEQ(l10n_util::GetNSStringF(IDS_IOS_AUTOFILL_ACCNAME_SUGGESTION,
                                      u"Test", u""),
              loadingLabel.accessibilityLabel);
  EXPECT_NSEQ(l10n_util::GetNSStringF(IDS_IOS_AUTOFILL_SUGGESTION_INDEX_VALUE,
                                      u"1", u"1"),
              loadingLabel.accessibilityValue);

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

// Test that the accessibility hint is set when context menu is enabled for
// suggestions supporting context menu.
TEST_F(FormSuggestionLabelTest, AccessibilityHintWhenContextMenuEnabled) {
  FormSuggestion* suggestion =
      CreateFormSuggestion(SuggestionType::kAddressEntry);
  FormSuggestionLabel* label =
      [[FormSuggestionLabel alloc] initWithSuggestion:suggestion
                                                index:0
                                  numberOfSuggestions:1
                                accessoryTrailingView:nil
                                 isContextMenuEnabled:YES
                                            isCompact:YES
                                             delegate:nil];
  EXPECT_NSEQ(
      l10n_util::GetNSString(IDS_IOS_TOOLBAR_ACCESSIBILITY_HINT_NEW_TAB),
      label.accessibilityHint);
}

// Test that the accessibility hint is nil when context menu is disabled.
TEST_F(FormSuggestionLabelTest, AccessibilityHintWhenContextMenuDisabled) {
  FormSuggestion* suggestion =
      CreateFormSuggestion(SuggestionType::kAddressEntry);
  FormSuggestionLabel* label =
      [[FormSuggestionLabel alloc] initWithSuggestion:suggestion
                                                index:0
                                  numberOfSuggestions:1
                                accessoryTrailingView:nil
                                 isContextMenuEnabled:NO
                                            isCompact:YES
                                             delegate:nil];
  EXPECT_NSEQ(nil, label.accessibilityHint);
}

// Test that the accessibility hint is nil when the suggestion does not support
// context menu, even if context menu is enabled.
TEST_F(FormSuggestionLabelTest,
       AccessibilityHintWhenSuggestionDoesNotSupportContextMenu) {
  FormSuggestion* suggestion =
      CreateFormSuggestion(SuggestionType::kAutocompleteEntry);
  FormSuggestionLabel* label =
      [[FormSuggestionLabel alloc] initWithSuggestion:suggestion
                                                index:0
                                  numberOfSuggestions:1
                                accessoryTrailingView:nil
                                 isContextMenuEnabled:YES
                                            isCompact:YES
                                             delegate:nil];
  EXPECT_NSEQ(nil, label.accessibilityHint);
}

// Test that a personal context Autofill AI suggestion with a non-empty
// description includes "Suggested by Gemini" in its accessibility label.
TEST_F(FormSuggestionLabelTest,
       PersonalContextSuggestionAccessibilityLabelWithDescription) {
  FormSuggestion* suggestion =
      CreateFormSuggestion(SuggestionType::kFillAutofillAi);
  id delegate = OCMProtocolMock(@protocol(FormSuggestionLabelDelegate));
  OCMStub([delegate displayDescriptionForSuggestion:suggestion])
      .andReturn(@"Passport · John Doe");
  OCMStub([delegate isPersonalContextSuggestion:suggestion]).andReturn(YES);

  FormSuggestionLabel* label =
      [[FormSuggestionLabel alloc] initWithSuggestion:suggestion
                                                index:0
                                  numberOfSuggestions:1
                                accessoryTrailingView:nil
                                 isContextMenuEnabled:YES
                                            isCompact:YES
                                             delegate:delegate];

  std::u16string baseLabel = l10n_util::GetStringFUTF16(
      IDS_IOS_AUTOFILL_ACCNAME_SUGGESTION, u"Test", u"Passport · John Doe");
  NSString* expectedLabel = l10n_util::GetNSStringF(
      IDS_IOS_AUTOFILL_ACCNAME_SUGGESTION, baseLabel,
      l10n_util::GetStringUTF16(IDS_AUTOFILL_AI_SUGGESTED_BY_GEMINI));
  EXPECT_NSEQ(expectedLabel, label.accessibilityLabel);
}

// Test that a personal context Autofill AI suggestion without a description
// includes "Suggested by Gemini" in its accessibility label without a duplicate
// separator.
TEST_F(FormSuggestionLabelTest,
       PersonalContextSuggestionAccessibilityLabelWithoutDescription) {
  FormSuggestion* suggestion =
      CreateFormSuggestion(SuggestionType::kFillAutofillAi);
  NSString* expectedLabel = l10n_util::GetNSStringF(
      IDS_IOS_AUTOFILL_ACCNAME_SUGGESTION, u"Test",
      l10n_util::GetStringUTF16(IDS_AUTOFILL_AI_SUGGESTED_BY_GEMINI));

  id emptyDescriptionDelegate =
      OCMProtocolMock(@protocol(FormSuggestionLabelDelegate));
  OCMStub([emptyDescriptionDelegate displayDescriptionForSuggestion:suggestion])
      .andReturn(@"");
  OCMStub([emptyDescriptionDelegate isPersonalContextSuggestion:suggestion])
      .andReturn(YES);

  FormSuggestionLabel* emptyDescriptionLabel =
      [[FormSuggestionLabel alloc] initWithSuggestion:suggestion
                                                index:0
                                  numberOfSuggestions:1
                                accessoryTrailingView:nil
                                 isContextMenuEnabled:YES
                                            isCompact:YES
                                             delegate:emptyDescriptionDelegate];
  EXPECT_NSEQ(expectedLabel, emptyDescriptionLabel.accessibilityLabel);

  id nilDescriptionDelegate =
      OCMProtocolMock(@protocol(FormSuggestionLabelDelegate));
  OCMStub([nilDescriptionDelegate isPersonalContextSuggestion:suggestion])
      .andReturn(YES);

  FormSuggestionLabel* nilDescriptionLabel =
      [[FormSuggestionLabel alloc] initWithSuggestion:suggestion
                                                index:0
                                  numberOfSuggestions:1
                                accessoryTrailingView:nil
                                 isContextMenuEnabled:YES
                                            isCompact:YES
                                             delegate:nilDescriptionDelegate];
  EXPECT_NSEQ(expectedLabel, nilDescriptionLabel.accessibilityLabel);
}

// Test that a non-personal-context Autofill AI suggestion does not include
// "Suggested by Gemini" in its accessibility label.
TEST_F(FormSuggestionLabelTest,
       NonPersonalContextAutofillAiSuggestionAccessibilityLabel) {
  FormSuggestion* suggestion =
      CreateFormSuggestion(SuggestionType::kFillAutofillAi);
  id delegate = OCMProtocolMock(@protocol(FormSuggestionLabelDelegate));
  OCMStub([delegate displayDescriptionForSuggestion:suggestion])
      .andReturn(@"Passport · John Doe");
  OCMStub([delegate isPersonalContextSuggestion:suggestion]).andReturn(NO);

  FormSuggestionLabel* label =
      [[FormSuggestionLabel alloc] initWithSuggestion:suggestion
                                                index:0
                                  numberOfSuggestions:1
                                accessoryTrailingView:nil
                                 isContextMenuEnabled:YES
                                            isCompact:YES
                                             delegate:delegate];

  NSString* expectedLabel = l10n_util::GetNSStringF(
      IDS_IOS_AUTOFILL_ACCNAME_SUGGESTION, u"Test", u"Passport · John Doe");
  EXPECT_NSEQ(expectedLabel, label.accessibilityLabel);
}
