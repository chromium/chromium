// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/atmemory/ui/at_memory_search_item.h"

#import <algorithm>
#import <optional>

#import "base/apple/foundation_util.h"
#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#import "components/autofill/core/browser/integrators/at_memory/memory_data_type.h"
#import "components/autofill/core/browser/integrators/at_memory/memory_data_type_util.h"
#import "components/autofill/core/browser/integrators/at_memory/memory_search_result.h"
#import "ios/chrome/browser/autofill/atmemory/public/at_memory_constants.h"
#import "ios/chrome/browser/autofill/autofill_ai/public/autofill_ai_ui_util.h"

namespace {

bool IsMemorySearchResultAutofillSourced(
    const autofill::MemorySearchResult& result) {
  return std::ranges::contains(result.sources,
                               autofill::MemoryEntrySourceType::kAutofill,
                               &autofill::MemoryEntrySource::type);
}

// TODO(crbug.com/545708069) Update GetAtMemorySearchItemIcon to provide the
// icon for non-AI entities.
UIImage* GetAtMemorySearchItemIcon(autofill::MemoryDataType entity_type,
                                   bool is_personal_context) {
  std::optional<autofill::AttributeType> attribute_type =
      autofill::ToAttributeType(entity_type);
  if (!attribute_type) {
    return nil;
  }
  return autofill::DefaultIconForAutofillAiEntityType(
      attribute_type->entity_type().name(), is_personal_context, kIconPointSize,
      /*tint_color=*/nil);
}

}  // namespace

@implementation AtMemorySearchItem

- (instancetype)initWithMemorySearchResult:
                    (const autofill::MemorySearchResult&)result
                                     index:(NSInteger)index {
  self = [super init];
  if (self) {
    const bool isPersonalContext = !IsMemorySearchResultAutofillSourced(result);
    _title = base::SysUTF16ToNSString(result.value);
    _subtitle = base::SysUTF16ToNSString(result.type_name);
    _icon = GetAtMemorySearchItemIcon(result.type, isPersonalContext);
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
