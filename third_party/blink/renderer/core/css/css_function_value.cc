// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_function_value.h"

#include "third_party/blink/renderer/core/css/css_property_name.h"
#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

String CSSFunctionValue::CustomCSSText() const {
  StringBuilder result;
  result.Append(GetCSSValueNameAs<StringView>(value_id_));
  result.Append('(');
  result.Append(CSSValueList::CustomCSSText());
  result.Append(')');
  return result.ReleaseString();
}

const CSSValue*
CSSFunctionValue::CopyRandomValueWithPropertyNameAndValueIndexIfNeeded(
    const CSSPropertyName& property_name,
    wtf_size_t property_value_index) const {
  CSSFunctionValue* new_function_value = MakeGarbageCollected<CSSFunctionValue>(
      value_id_, static_cast<ValueListSeparator>(value_list_separator_));
  for (wtf_size_t i = 0; i < CSSValueList::length(); i++) {
    new_function_value->Append(
        *CSSValueList::Item(i)
             .CopyRandomValueWithPropertyNameAndValueIndexIfNeeded(
                 property_name, property_value_index + i));
  }
  return new_function_value;
}

}  // namespace blink
