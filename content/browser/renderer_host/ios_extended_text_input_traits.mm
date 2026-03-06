// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/ios_extended_text_input_traits.h"

#include "ui/base/ime/mojom/text_input_state.mojom.h"
#include "ui/base/ime/text_input_flags.h"

namespace {

UITextAutocapitalizationType AutocapitalizationTypeForState(
    const ui::mojom::TextInputState& state) {
  if (state.flags & ui::TEXT_INPUT_FLAG_AUTOCAPITALIZE_NONE) {
    return UITextAutocapitalizationTypeNone;
  }
  if (state.flags & ui::TEXT_INPUT_FLAG_AUTOCAPITALIZE_CHARACTERS) {
    return UITextAutocapitalizationTypeAllCharacters;
  }
  if (state.flags & ui::TEXT_INPUT_FLAG_AUTOCAPITALIZE_WORDS) {
    return UITextAutocapitalizationTypeWords;
  }
  if (state.flags & ui::TEXT_INPUT_FLAG_AUTOCAPITALIZE_SENTENCES) {
    return UITextAutocapitalizationTypeSentences;
  }
  switch (state.type) {
    case ui::TextInputType::TEXT_INPUT_TYPE_PASSWORD:
    case ui::TextInputType::TEXT_INPUT_TYPE_EMAIL:
    case ui::TextInputType::TEXT_INPUT_TYPE_NUMBER:
    case ui::TextInputType::TEXT_INPUT_TYPE_TELEPHONE:
    case ui::TextInputType::TEXT_INPUT_TYPE_URL:
      return UITextAutocapitalizationTypeNone;
    default:
      return UITextAutocapitalizationTypeSentences;
  }
}
UITextAutocorrectionType AutocorrectionTypeForState(
    const ui::mojom::TextInputState& state) {
  if (state.flags & ui::TEXT_INPUT_FLAG_AUTOCORRECT_OFF) {
    return UITextAutocorrectionTypeNo;
  }
  if (state.flags & ui::TEXT_INPUT_FLAG_AUTOCORRECT_ON) {
    return UITextAutocorrectionTypeYes;
  }
  switch (state.type) {
    case ui::TextInputType::TEXT_INPUT_TYPE_PASSWORD:
    case ui::TextInputType::TEXT_INPUT_TYPE_EMAIL:
    case ui::TextInputType::TEXT_INPUT_TYPE_NUMBER:
    case ui::TextInputType::TEXT_INPUT_TYPE_TELEPHONE:
    case ui::TextInputType::TEXT_INPUT_TYPE_URL:
      return UITextAutocorrectionTypeNo;
    default:
      return UITextAutocorrectionTypeDefault;
  }
}

UITextSpellCheckingType SpellCheckingTypeForState(
    const ui::mojom::TextInputState& state) {
  if (state.flags & ui::TEXT_INPUT_FLAG_SPELLCHECK_OFF) {
    return UITextSpellCheckingTypeNo;
  }
  if (state.flags & ui::TEXT_INPUT_FLAG_SPELLCHECK_ON) {
    return UITextSpellCheckingTypeYes;
  }
  switch (state.type) {
    case ui::TextInputType::TEXT_INPUT_TYPE_PASSWORD:
    case ui::TextInputType::TEXT_INPUT_TYPE_EMAIL:
    case ui::TextInputType::TEXT_INPUT_TYPE_NUMBER:
    case ui::TextInputType::TEXT_INPUT_TYPE_TELEPHONE:
    case ui::TextInputType::TEXT_INPUT_TYPE_URL:
      return UITextSpellCheckingTypeNo;
    default:
      return UITextSpellCheckingTypeDefault;
  }
}

UITextSmartQuotesType SmartQuotesTypeForSpellCheckingType(
    UITextSpellCheckingType spellCheckingType) {
  switch (spellCheckingType) {
    case UITextSpellCheckingTypeNo:
      return UITextSmartQuotesTypeNo;
    case UITextSpellCheckingTypeYes:
      return UITextSmartQuotesTypeYes;
    case UITextSpellCheckingTypeDefault:
      return UITextSmartQuotesTypeDefault;
  }
}

UITextSmartDashesType SmartDashesTypeForSpellCheckingType(
    UITextSpellCheckingType spellCheckingType) {
  switch (spellCheckingType) {
    case UITextSpellCheckingTypeNo:
      return UITextSmartDashesTypeNo;
    case UITextSpellCheckingTypeYes:
      return UITextSmartDashesTypeYes;
    case UITextSpellCheckingTypeDefault:
      return UITextSmartDashesTypeDefault;
  }
}

UIKeyboardType KeyboardTypeForState(const ui::mojom::TextInputState& state) {
  switch (state.mode) {
    case ui::TextInputMode::TEXT_INPUT_MODE_EMAIL:
      return UIKeyboardTypeEmailAddress;
    case ui::TextInputMode::TEXT_INPUT_MODE_URL:
      return UIKeyboardTypeURL;
    case ui::TextInputMode::TEXT_INPUT_MODE_TEL:
      return UIKeyboardTypePhonePad;
    case ui::TextInputMode::TEXT_INPUT_MODE_NUMERIC:
      return UIKeyboardTypeNumberPad;
    case ui::TextInputMode::TEXT_INPUT_MODE_DECIMAL:
      return UIKeyboardTypeDecimalPad;
    case ui::TextInputMode::TEXT_INPUT_MODE_SEARCH:
      return UIKeyboardTypeWebSearch;
    case ui::TextInputMode::TEXT_INPUT_MODE_NONE:
    case ui::TextInputMode::TEXT_INPUT_MODE_DEFAULT:
    case ui::TextInputMode::TEXT_INPUT_MODE_TEXT:
      break;
  }

  switch (state.type) {
    case ui::TextInputType::TEXT_INPUT_TYPE_SEARCH:
      return UIKeyboardTypeWebSearch;
    case ui::TextInputType::TEXT_INPUT_TYPE_EMAIL:
      return UIKeyboardTypeEmailAddress;
    case ui::TextInputType::TEXT_INPUT_TYPE_NUMBER:
      return UIKeyboardTypeNumberPad;
    case ui::TextInputType::TEXT_INPUT_TYPE_TELEPHONE:
      return UIKeyboardTypePhonePad;
    case ui::TextInputType::TEXT_INPUT_TYPE_URL:
      return UIKeyboardTypeURL;
    default:
      return UIKeyboardTypeDefault;
  }
}

UIReturnKeyType ReturnKeyTypeForState(const ui::mojom::TextInputState& state) {
  switch (state.action) {
    case ui::TextInputAction::kDone:
      return UIReturnKeyDone;
    case ui::TextInputAction::kGo:
      return UIReturnKeyGo;
    case ui::TextInputAction::kNext:
      return UIReturnKeyNext;
    case ui::TextInputAction::kSearch:
      return UIReturnKeySearch;
    case ui::TextInputAction::kSend:
      return UIReturnKeySend;
    case ui::TextInputAction::kDefault:
    case ui::TextInputAction::kEnter:
    case ui::TextInputAction::kPrevious:
      return UIReturnKeyDefault;
  }
}

BOOL SingleLineDocumentForState(const ui::mojom::TextInputState& state) {
  switch (state.type) {
    case ui::TextInputType::TEXT_INPUT_TYPE_NONE:
    case ui::TextInputType::TEXT_INPUT_TYPE_TEXT_AREA:
    case ui::TextInputType::TEXT_INPUT_TYPE_CONTENT_EDITABLE:
      return NO;
    default:
      return YES;
  }
}

BOOL TypingAdaptationEnabledForState(const ui::mojom::TextInputState& state) {
  if (state.type == ui::TextInputType::TEXT_INPUT_TYPE_PASSWORD) {
    return NO;
  }
  return !(state.flags & ui::TEXT_INPUT_FLAG_HAS_BEEN_PASSWORD);
}

}  // namespace

@implementation IOSExtendedTextInputTraits

- (instancetype)init {
  if (!(self = [super init])) {
    return nil;
  }
  self.typingAdaptationEnabled = YES;
  self.selectionHandleColor = [UIColor blueColor];
  return self;
}

- (BOOL)updateFromTextInputState:(const ui::mojom::TextInputState&)state {
  UITextAutocapitalizationType autocapitalizationType =
      AutocapitalizationTypeForState(state);
  UITextAutocorrectionType autocorrectionType =
      AutocorrectionTypeForState(state);
  UITextSpellCheckingType spellCheckingType = SpellCheckingTypeForState(state);
  UITextSmartQuotesType smartQuotesType =
      SmartQuotesTypeForSpellCheckingType(spellCheckingType);
  UITextSmartDashesType smartDashesType =
      SmartDashesTypeForSpellCheckingType(spellCheckingType);
  UIKeyboardType keyboardType = KeyboardTypeForState(state);
  UIReturnKeyType returnKeyType = ReturnKeyTypeForState(state);
  BOOL secureTextEntry =
      state.type == ui::TextInputType::TEXT_INPUT_TYPE_PASSWORD;
  BOOL singleLineDocument = SingleLineDocumentForState(state);
  BOOL typingAdaptationEnabled = TypingAdaptationEnabledForState(state);

  BOOL changed = self.autocapitalizationType != autocapitalizationType ||
                 self.autocorrectionType != autocorrectionType ||
                 self.spellCheckingType != spellCheckingType ||
                 self.smartQuotesType != smartQuotesType ||
                 self.smartDashesType != smartDashesType ||
                 self.keyboardType != keyboardType ||
                 self.returnKeyType != returnKeyType ||
                 self.secureTextEntry != secureTextEntry ||
                 self.singleLineDocument != singleLineDocument ||
                 self.typingAdaptationEnabled != typingAdaptationEnabled;

  self.autocapitalizationType = autocapitalizationType;
  self.autocorrectionType = autocorrectionType;
  self.spellCheckingType = spellCheckingType;
  self.smartQuotesType = smartQuotesType;
  self.smartDashesType = smartDashesType;
  self.keyboardType = keyboardType;
  self.returnKeyType = returnKeyType;
  self.secureTextEntry = secureTextEntry;
  self.singleLineDocument = singleLineDocument;
  self.typingAdaptationEnabled = typingAdaptationEnabled;
  return changed;
}

@end
