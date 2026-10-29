// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/form_input_accessory/ui/form_suggestion_label.h"

#import <UIKit/UIKit.h>

#import "base/apple/foundation_util.h"
#import "components/autofill/ios/browser/form_suggestion.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/autofill/model/form_suggestion_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "ui/base/l10n/l10n_util.h"

// Category exposing private `FormSuggestionLabel` selectors and protocol
// conformance for testing.
@interface FormSuggestionLabel (Testing) <UIContextMenuInteractionDelegate>
- (UIMenu*)contextMenu;
@end

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

// Returns the `UIContextMenuInteraction` attached to `view`, or nil if none.
UIContextMenuInteraction* FindContextMenuInteraction(UIView* view) {
  for (id<UIInteraction> interaction in view.interactions) {
    if (auto* context_menu_interaction =
            base::apple::ObjCCast<UIContextMenuInteraction>(interaction)) {
      return context_menu_interaction;
    }
  }
  return nil;
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

// Tests that an Ambient Autofill suggestion (`SuggestionType::kFillAutofillAi`)
// renders an icon and a two-line layout with the primary value on the first
// line and the display description on the second line in both compact and
// non-compact modes.
TEST_F(FormSuggestionLabelTest, AmbientAutofillTwoLineLayoutAndIcon) {
  NSString* const kTestSuggestionValue = @"John Doe";
  NSString* const kTestDisplayDescription = @"Passport · 12345";
  UIImage* icon = [[UIImage alloc] init];
  FormSuggestion* suggestion =
      [FormSuggestion suggestionWithValue:kTestSuggestionValue
                       displayDescription:kTestDisplayDescription
                                     icon:icon
                                     type:SuggestionType::kFillAutofillAi
                                  payload:Suggestion::Payload()
                           requiresReauth:NO];

  id delegate = OCMProtocolMock(@protocol(FormSuggestionLabelDelegate));
  OCMStub([delegate displayDescriptionForSuggestion:suggestion])
      .andReturn(kTestDisplayDescription);

  FormSuggestionLabel* label =
      [[FormSuggestionLabel alloc] initWithSuggestion:suggestion
                                                index:0
                                  numberOfSuggestions:1
                                accessoryTrailingView:nil
                                 isContextMenuEnabled:YES
                                            isCompact:YES
                                             delegate:delegate];

  ASSERT_EQ(label.subviews.count, 1u);
  UIStackView* compact_horizontal_stack =
      base::apple::ObjCCastStrict<UIStackView>(label.subviews[0]);
  ASSERT_EQ(compact_horizontal_stack.arrangedSubviews.count, 2u);
  UIImageView* compact_icon_view = base::apple::ObjCCastStrict<UIImageView>(
      compact_horizontal_stack.arrangedSubviews[0]);
  EXPECT_EQ(compact_icon_view.image, icon);

  UIStackView* vertical_text_stack = base::apple::ObjCCastStrict<UIStackView>(
      compact_horizontal_stack.arrangedSubviews[1]);
  ASSERT_EQ(vertical_text_stack.arrangedSubviews.count, 2u);
  UILabel* title_label = base::apple::ObjCCastStrict<UILabel>(
      vertical_text_stack.arrangedSubviews[0]);
  UILabel* subtitle_label = base::apple::ObjCCastStrict<UILabel>(
      vertical_text_stack.arrangedSubviews[1]);
  EXPECT_NSEQ(title_label.text, kTestSuggestionValue);
  EXPECT_NSEQ(subtitle_label.text, kTestDisplayDescription);

  // Switch to non-compact mode and verify the attributed two-line label.
  [label setIsCompact:NO];
  ASSERT_EQ(label.subviews.count, 1u);
  UIStackView* non_compact_horizontal_stack =
      base::apple::ObjCCastStrict<UIStackView>(label.subviews[0]);
  ASSERT_EQ(non_compact_horizontal_stack.arrangedSubviews.count, 2u);
  UIImageView* non_compact_icon_view = base::apple::ObjCCastStrict<UIImageView>(
      non_compact_horizontal_stack.arrangedSubviews[0]);
  EXPECT_EQ(non_compact_icon_view.image, icon);

  UILabel* attributed_label = base::apple::ObjCCastStrict<UILabel>(
      non_compact_horizontal_stack.arrangedSubviews[1]);
  EXPECT_EQ(attributed_label.numberOfLines, 2);
  NSString* expected_combined_text =
      [NSString stringWithFormat:@"%@\n%@", kTestSuggestionValue,
                                 kTestDisplayDescription];
  EXPECT_NSEQ(attributed_label.attributedText.string, expected_combined_text);
}

// Tests that a Personal Context (`SuggestionType::kFillAutofillAi`) suggestion
// chip builds a context menu with the "From Gemini and connected apps. Manage
// enhanced autofill in settings." header and the "Open settings", "View
// sources", and destructive "Remove" actions (without an "Edit" action), and
// that selecting each action invokes the corresponding delegate method.
TEST_F(FormSuggestionLabelTest, PersonalContextSuggestionContextMenu) {
  FormSuggestion* suggestion =
      CreateFormSuggestion(SuggestionType::kFillAutofillAi);

  id delegate = OCMProtocolMock(@protocol(FormSuggestionLabelDelegate));
  OCMStub([delegate displayDescriptionForSuggestion:suggestion]).andReturn(@"");
  OCMStub([delegate isPersonalContextSuggestion:suggestion]).andReturn(YES);
  OCMStub([delegate hasSourcesForSuggestion:suggestion]).andReturn(YES);
  OCMStub([delegate canSuppressPersonalContextSuggestion:suggestion])
      .andReturn(YES);

  FormSuggestionLabel* label =
      [[FormSuggestionLabel alloc] initWithSuggestion:suggestion
                                                index:0
                                  numberOfSuggestions:1
                                accessoryTrailingView:nil
                                 isContextMenuEnabled:YES
                                            isCompact:YES
                                             delegate:delegate];

  UIContextMenuInteraction* interaction = FindContextMenuInteraction(label);
  ASSERT_NE(interaction, nil);

  UIContextMenuConfiguration* config =
      [label contextMenuInteraction:interaction
          configurationForMenuAtLocation:CGPointZero];
  EXPECT_NE(config, nil);

  UIMenu* menu = [label contextMenu];
  ASSERT_NE(menu, nil);
  EXPECT_NSEQ(
      menu.title,
      l10n_util::GetNSString(
          IDS_IOS_AUTOFILL_AI_CONTEXT_MENU_PERSONAL_CONTEXT_DESCRIPTION));

  ASSERT_EQ(menu.children.count, 3u);

  UIAction* open_settings_action =
      base::apple::ObjCCastStrict<UIAction>(menu.children[0]);
  EXPECT_NSEQ(open_settings_action.accessibilityIdentifier,
              kFormSuggestionLabelOpenSettingsAccessibilityIdentifier);
  EXPECT_NSEQ(open_settings_action.title,
              l10n_util::GetNSString(IDS_IOS_CONTEXT_MENU_OPEN_SETTINGS));

  UIAction* view_sources_action =
      base::apple::ObjCCastStrict<UIAction>(menu.children[1]);
  EXPECT_NSEQ(view_sources_action.accessibilityIdentifier,
              kFormSuggestionLabelViewSourcesAccessibilityIdentifier);
  EXPECT_NSEQ(view_sources_action.title,
              l10n_util::GetNSString(IDS_IOS_AUTOFILL_AI_VIEW_SOURCES));

  UIAction* remove_action =
      base::apple::ObjCCastStrict<UIAction>(menu.children[2]);
  EXPECT_NSEQ(remove_action.accessibilityIdentifier,
              kFormSuggestionLabelRemoveAccessibilityIdentifier);
  EXPECT_NSEQ(remove_action.title,
              l10n_util::GetNSString(IDS_IOS_AUTOFILL_AI_REMOVE_ACTION));
  EXPECT_TRUE(remove_action.attributes & UIMenuElementAttributesDestructive);

  OCMExpect([delegate openSettingsForSuggestion:suggestion]);
  [open_settings_action performWithSender:nil target:nil];

  OCMExpect([delegate openSourcesForSuggestion:suggestion]);
  [view_sources_action performWithSender:nil target:nil];

  OCMExpect([delegate suppressPersonalContextSuggestion:suggestion]);
  [remove_action performWithSender:nil target:nil];

  EXPECT_OCMOCK_VERIFY(delegate);
}
