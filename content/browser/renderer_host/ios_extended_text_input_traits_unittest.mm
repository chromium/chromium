// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/ios_extended_text_input_traits.h"

#include "testing/platform_test.h"
#include "ui/base/ime/mojom/text_input_state.mojom.h"
#include "ui/base/ime/text_input_flags.h"

namespace content {
namespace {

ui::mojom::TextInputStatePtr MakeState(
    ui::TextInputType type = ui::TextInputType::TEXT_INPUT_TYPE_TEXT,
    ui::TextInputMode mode = ui::TextInputMode::TEXT_INPUT_MODE_DEFAULT,
    ui::TextInputAction action = ui::TextInputAction::kDefault,
    uint32_t flags = ui::TEXT_INPUT_FLAG_NONE) {
  auto state = ui::mojom::TextInputState::New();
  state->type = type;
  state->mode = mode;
  state->action = action;
  state->flags = flags;
  return state;
}

}  // namespace

class IOSExtendedTextInputTraitsTest : public PlatformTest {
 public:
  void SetUp() override { traits_ = [[IOSExtendedTextInputTraits alloc] init]; }

 protected:
  IOSExtendedTextInputTraits* traits_;
};

TEST_F(IOSExtendedTextInputTraitsTest, AutocapitalizationTypeMapping) {
  EXPECT_TRUE([traits_
      updateFromTextInputState:*MakeState(
                                   ui::TextInputType::TEXT_INPUT_TYPE_TEXT,
                                   ui::TextInputMode::TEXT_INPUT_MODE_DEFAULT,
                                   ui::TextInputAction::kDefault,
                                   ui::TEXT_INPUT_FLAG_AUTOCAPITALIZE_WORDS)]);
  EXPECT_EQ(UITextAutocapitalizationTypeWords, traits_.autocapitalizationType);

  EXPECT_TRUE([traits_
      updateFromTextInputState:*MakeState(
                                   ui::TextInputType::TEXT_INPUT_TYPE_EMAIL)]);
  EXPECT_EQ(UITextAutocapitalizationTypeNone, traits_.autocapitalizationType);
}

TEST_F(IOSExtendedTextInputTraitsTest, AutocorrectionTypeMapping) {
  EXPECT_TRUE([traits_
      updateFromTextInputState:*MakeState(
                                   ui::TextInputType::TEXT_INPUT_TYPE_TEXT,
                                   ui::TextInputMode::TEXT_INPUT_MODE_DEFAULT,
                                   ui::TextInputAction::kDefault,
                                   ui::TEXT_INPUT_FLAG_AUTOCORRECT_ON)]);
  EXPECT_EQ(UITextAutocorrectionTypeYes, traits_.autocorrectionType);

  EXPECT_TRUE([traits_
      updateFromTextInputState:*MakeState(
                                   ui::TextInputType::TEXT_INPUT_TYPE_URL)]);
  EXPECT_EQ(UITextAutocorrectionTypeNo, traits_.autocorrectionType);
}

TEST_F(IOSExtendedTextInputTraitsTest,
       SpellcheckOnEnablesSmartQuotesAndSmartDashes) {
  auto state = MakeState(ui::TextInputType::TEXT_INPUT_TYPE_TEXT,
                         ui::TextInputMode::TEXT_INPUT_MODE_DEFAULT,
                         ui::TextInputAction::kDefault,
                         ui::TEXT_INPUT_FLAG_SPELLCHECK_ON);

  EXPECT_TRUE([traits_ updateFromTextInputState:*state]);
  EXPECT_EQ(UITextSpellCheckingTypeYes, traits_.spellCheckingType);
  EXPECT_EQ(UITextSmartQuotesTypeYes, traits_.smartQuotesType);
  EXPECT_EQ(UITextSmartDashesTypeYes, traits_.smartDashesType);
}

TEST_F(IOSExtendedTextInputTraitsTest,
       SpellcheckOffDisablesSmartQuotesAndSmartDashes) {
  EXPECT_TRUE([traits_
      updateFromTextInputState:*MakeState(
                                   ui::TextInputType::TEXT_INPUT_TYPE_TEXT,
                                   ui::TextInputMode::TEXT_INPUT_MODE_DEFAULT,
                                   ui::TextInputAction::kDefault,
                                   ui::TEXT_INPUT_FLAG_SPELLCHECK_ON)]);

  EXPECT_TRUE([traits_
      updateFromTextInputState:*MakeState(
                                   ui::TextInputType::TEXT_INPUT_TYPE_TEXT,
                                   ui::TextInputMode::TEXT_INPUT_MODE_DEFAULT,
                                   ui::TextInputAction::kDefault,
                                   ui::TEXT_INPUT_FLAG_SPELLCHECK_OFF)]);
  EXPECT_EQ(UITextSpellCheckingTypeNo, traits_.spellCheckingType);
  EXPECT_EQ(UITextSmartQuotesTypeNo, traits_.smartQuotesType);
  EXPECT_EQ(UITextSmartDashesTypeNo, traits_.smartDashesType);
}

TEST_F(IOSExtendedTextInputTraitsTest,
       DefaultSpellcheckUsesDefaultSmartQuotesAndDashes) {
  auto state = MakeState(ui::TextInputType::TEXT_INPUT_TYPE_TEXT);

  EXPECT_TRUE([traits_ updateFromTextInputState:*state]);
  EXPECT_EQ(UITextSpellCheckingTypeDefault, traits_.spellCheckingType);
  EXPECT_EQ(UITextSmartQuotesTypeDefault, traits_.smartQuotesType);
  EXPECT_EQ(UITextSmartDashesTypeDefault, traits_.smartDashesType);
}

TEST_F(IOSExtendedTextInputTraitsTest, KeyboardTypePrefersModeOverType) {
  auto modeWinsState = MakeState(ui::TextInputType::TEXT_INPUT_TYPE_URL,
                                 ui::TextInputMode::TEXT_INPUT_MODE_EMAIL);

  EXPECT_TRUE([traits_ updateFromTextInputState:*modeWinsState]);
  EXPECT_EQ(UIKeyboardTypeEmailAddress, traits_.keyboardType);
}

TEST_F(IOSExtendedTextInputTraitsTest, KeyboardTypeFallsBackToType) {
  auto typeFallbackState =
      MakeState(ui::TextInputType::TEXT_INPUT_TYPE_URL,
                ui::TextInputMode::TEXT_INPUT_MODE_DEFAULT);

  EXPECT_TRUE([traits_ updateFromTextInputState:*typeFallbackState]);
  EXPECT_EQ(UIKeyboardTypeURL, traits_.keyboardType);
}

TEST_F(IOSExtendedTextInputTraitsTest, ReturnKeyTypeMapping) {
  EXPECT_TRUE([traits_
      updateFromTextInputState:*MakeState(
                                   ui::TextInputType::TEXT_INPUT_TYPE_TEXT,
                                   ui::TextInputMode::TEXT_INPUT_MODE_DEFAULT,
                                   ui::TextInputAction::kDone)]);
  EXPECT_EQ(UIReturnKeyDone, traits_.returnKeyType);

  EXPECT_TRUE([traits_
      updateFromTextInputState:*MakeState(
                                   ui::TextInputType::TEXT_INPUT_TYPE_TEXT,
                                   ui::TextInputMode::TEXT_INPUT_MODE_DEFAULT,
                                   ui::TextInputAction::kSearch)]);
  EXPECT_EQ(UIReturnKeySearch, traits_.returnKeyType);
}

TEST_F(IOSExtendedTextInputTraitsTest, SecureTextEntryMapping) {
  EXPECT_TRUE(
      [traits_ updateFromTextInputState:
                   *MakeState(ui::TextInputType::TEXT_INPUT_TYPE_PASSWORD)]);
  EXPECT_TRUE(traits_.secureTextEntry);

  EXPECT_TRUE([traits_
      updateFromTextInputState:*MakeState(
                                   ui::TextInputType::TEXT_INPUT_TYPE_TEXT)]);
  EXPECT_FALSE(traits_.secureTextEntry);
}

TEST_F(IOSExtendedTextInputTraitsTest, SingleLineDocumentMapping) {
  EXPECT_TRUE(
      [traits_ updateFromTextInputState:
                   *MakeState(ui::TextInputType::TEXT_INPUT_TYPE_TEXT_AREA)]);
  EXPECT_FALSE(traits_.singleLineDocument);

  EXPECT_TRUE([traits_
      updateFromTextInputState:*MakeState(
                                   ui::TextInputType::TEXT_INPUT_TYPE_TEXT)]);
  EXPECT_TRUE(traits_.singleLineDocument);

  EXPECT_TRUE([traits_
      updateFromTextInputState:
          *MakeState(ui::TextInputType::TEXT_INPUT_TYPE_CONTENT_EDITABLE)]);
  EXPECT_FALSE(traits_.singleLineDocument);
}

TEST_F(IOSExtendedTextInputTraitsTest, TypingAdaptationEnabledMapping) {
  EXPECT_TRUE(
      [traits_ updateFromTextInputState:
                   *MakeState(ui::TextInputType::TEXT_INPUT_TYPE_PASSWORD)]);
  EXPECT_FALSE(traits_.typingAdaptationEnabled);

  EXPECT_TRUE([traits_
      updateFromTextInputState:*MakeState(
                                   ui::TextInputType::TEXT_INPUT_TYPE_TEXT,
                                   ui::TextInputMode::TEXT_INPUT_MODE_DEFAULT,
                                   ui::TextInputAction::kDefault,
                                   ui::TEXT_INPUT_FLAG_HAS_BEEN_PASSWORD)]);
  EXPECT_FALSE(traits_.typingAdaptationEnabled);

  EXPECT_TRUE([traits_
      updateFromTextInputState:*MakeState(
                                   ui::TextInputType::TEXT_INPUT_TYPE_TEXT)]);
  EXPECT_TRUE(traits_.typingAdaptationEnabled);
}

TEST_F(IOSExtendedTextInputTraitsTest, UpdateIsIdempotentForSameState) {
  auto state = MakeState(
      ui::TextInputType::TEXT_INPUT_TYPE_URL,
      ui::TextInputMode::TEXT_INPUT_MODE_EMAIL, ui::TextInputAction::kSearch,
      ui::TEXT_INPUT_FLAG_SPELLCHECK_ON | ui::TEXT_INPUT_FLAG_AUTOCORRECT_ON |
          ui::TEXT_INPUT_FLAG_AUTOCAPITALIZE_WORDS);

  EXPECT_TRUE([traits_ updateFromTextInputState:*state]);
  EXPECT_FALSE([traits_ updateFromTextInputState:*state]);
}

}  // namespace content
