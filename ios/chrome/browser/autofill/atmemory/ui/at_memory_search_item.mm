// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/atmemory/ui/at_memory_search_item.h"

#import <optional>
#import <variant>

#import "base/apple/foundation_util.h"
#import "base/strings/string_util.h"
#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#import "components/autofill/core/browser/integrators/at_memory/memory_data_type.h"
#import "components/autofill/core/browser/integrators/at_memory/memory_data_type_util.h"
#import "components/autofill/core/browser/suggestions/suggestion.h"
#import "ios/chrome/browser/autofill/atmemory/public/at_memory_constants.h"
#import "ios/chrome/browser/autofill/autofill_ai/public/autofill_ai_ui_util.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"

namespace {

using autofill::Suggestion;

// Returns the icon to display for `suggestion`, resolving Autofill AI entity
// icons from the payload and falling back to `suggestion.icon` for other
// types.
UIImage* GetAtMemorySearchItemIcon(const Suggestion& suggestion) {
  if (const auto* payload =
          std::get_if<Suggestion::AtMemoryPayload>(&suggestion.payload)) {
    if (std::optional<autofill::AttributeType> attribute_type =
            autofill::ToAttributeType(payload->memory_data_type)) {
      return autofill::DefaultIconForAutofillAiEntityType(
          attribute_type->entity_type().name(),
          payload->is_personal_context_sourced, kIconPointSize,
          /*tint_color=*/nil);
    }
  }

  Symbol symbol;
  switch (suggestion.icon) {
    case Suggestion::Icon::kLocation:
      symbol = SymbolLocation;
      break;
    case Suggestion::Icon::kLocationSpark:
      symbol = SymbolLocationSpark;
      break;
    case Suggestion::Icon::kCardGenericVector:
      symbol = SymbolCreditCard;
      break;
    case Suggestion::Icon::kCardGenericSpark:
      symbol = SymbolCreditCardSpark;
      break;
    default:
      symbol = SymbolTextSpark;
      break;
  }

  return SymbolWithPointSize(symbol, kIconPointSize);
}

// Returns the subtitle string for `suggestion`.
std::u16string GetSubtitleFromSuggestion(const Suggestion& suggestion) {
  std::vector<std::u16string> label_pieces;
  for (const auto& row : suggestion.labels) {
    for (const auto& text : row) {
      if (!text.value.empty()) {
        label_pieces.push_back(text.value);
      }
    }
  }
  if (!label_pieces.empty()) {
    return base::JoinString(label_pieces, u" ");
  }
  if (const auto* payload =
          std::get_if<Suggestion::AtMemoryPayload>(&suggestion.payload)) {
    return payload->type_name;
  }
  return u"";
}

}  // namespace

@implementation AtMemorySearchItem

- (instancetype)initWithSuggestion:(const Suggestion&)suggestion
                             index:(NSInteger)index {
  self = [super init];
  if (self) {
    _title = base::SysUTF16ToNSString(suggestion.main_text.value);
    _subtitle = base::SysUTF16ToNSString(GetSubtitleFromSuggestion(suggestion));
    _icon = GetAtMemorySearchItemIcon(suggestion);
    if (const auto* payload =
            std::get_if<Suggestion::AtMemoryPayload>(&suggestion.payload)) {
      _isPersonalContextSourced = payload->is_personal_context_sourced;
    }
    _index = index;
  }
  return self;
}

#pragma mark - NSObject

- (BOOL)isEqual:(id)object {
  if (self == object) {
    return YES;
  }
  if (![object isKindOfClass:[AtMemorySearchItem class]]) {
    return NO;
  }
  AtMemorySearchItem* other =
      base::apple::ObjCCastStrict<AtMemorySearchItem>(object);
  if (!self.title || !other.title || !self.subtitle || !other.subtitle) {
    return NO;
  }
  return [self.title isEqualToString:other.title] &&
         [self.subtitle isEqualToString:other.subtitle];
}

- (NSUInteger)hash {
  return [self.title hash] ^ [self.subtitle hash];
}

@end
