// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/atmemory/utils/atmemory_ui_util.h"

#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/browser/integrators/at_memory/memory_data_type.h"
#import "components/autofill/core/browser/integrators/at_memory/memory_data_type_util.h"
#import "components/autofill/core/browser/integrators/at_memory/memory_search_result.h"

NSString* GetAtMemoryGranularFillTitle(
    const autofill::MemorySearchResult& result) {
  if (std::optional<autofill::AttributeType> attribute_type =
          autofill::ToAttributeType(result.type)) {
    return base::SysUTF16ToNSString(
        attribute_type->entity_type().GetNameForI18n());
  }
  std::u16string type_name =
      result.type == autofill::MemoryDataType::kUnknown
          ? result.type_name
          : autofill::GetMemoryDataTypeNameForI18n(result.type);
  return base::SysUTF16ToNSString(type_name);
}
