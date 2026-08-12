// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/atmemory/utils/atmemory_ui_util.h"

#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/browser/integrators/at_memory/memory_data_type.h"
#import "components/autofill/core/browser/integrators/at_memory/memory_data_type_util.h"
#import "components/autofill/core/browser/integrators/at_memory/memory_search_result.h"
#import "ios/chrome/browser/autofill/atmemory/ui/at_memory_granular_fill_item.h"

namespace {

// Resolves the attribute name string, falling back to i18n data type name.
NSString* GetAttributeName(const std::u16string& type_name,
                           autofill::MemoryDataType type) {
  std::u16string name = type_name.empty()
                            ? autofill::GetMemoryDataTypeNameForI18n(type)
                            : type_name;
  return base::SysUTF16ToNSString(name);
}

}  // namespace

NSString* GetAtMemoryGranularFillTitle(
    const autofill::MemorySearchResult& result) {
  if (std::optional<autofill::AttributeType> attribute_type =
          autofill::ToAttributeType(result.type)) {
    return base::SysUTF16ToNSString(
        attribute_type->entity_type().GetNameForI18n());
  }
  return GetAttributeName(result.type_name, result.type);
}

NSArray<AtMemoryGranularFillItem*>* AtMemoryGranularFillItemsForSearchResult(
    const autofill::MemorySearchResult& result) {
  NSMutableArray<AtMemoryGranularFillItem*>* items =
      [[NSMutableArray alloc] init];

  if (!result.value.empty()) {
    [items
        addObject:[[AtMemoryGranularFillItem alloc]
                      initWithAttributeName:GetAttributeName(result.type_name,
                                                             result.type)
                             attributeValue:base::SysUTF16ToNSString(
                                                result.value)]];
  }

  for (const auto& metadata : result.metadata_list) {
    if (!metadata.value.empty()) {
      [items addObject:[[AtMemoryGranularFillItem alloc]
                           initWithAttributeName:GetAttributeName(
                                                     metadata.type_name,
                                                     metadata.type)
                                  attributeValue:base::SysUTF16ToNSString(
                                                     metadata.value)]];
    }
  }

  return [items copy];
}
