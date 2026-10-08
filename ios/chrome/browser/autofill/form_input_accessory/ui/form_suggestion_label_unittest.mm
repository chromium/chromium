// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/form_input_accessory/ui/form_suggestion_label.h"

#import <UIKit/UIKit.h>

#import "components/autofill/ios/browser/form_suggestion.h"
#import "ios/chrome/grit/ios_strings.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
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
