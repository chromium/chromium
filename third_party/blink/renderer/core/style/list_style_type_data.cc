// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/style/list_style_type_data.h"

#include "third_party/blink/renderer/core/css/css_value_id_mappings.h"
#include "third_party/blink/renderer/core/dom/tree_scope.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"

namespace blink {

namespace {

using PredefinedCounterStyleNameMap = HashMap<AtomicString, EListStyleType>;

PredefinedCounterStyleNameMap BuildPredefinedCounterStyleNameMap() {
  PredefinedCounterStyleNameMap map;
  for (unsigned i = 0; i < static_cast<unsigned>(EListStyleType::kString);
       ++i) {
    EListStyleType list_style_type = static_cast<EListStyleType>(i);
    CSSValueID css_value_id = PlatformEnumToCSSValueID(list_style_type);
    AtomicString value_name(getValueName(css_value_id));
    map.Set(value_name, list_style_type);
  }
  return map;
}

}  // namespace

void ListStyleTypeData::Trace(Visitor* visitor) const {
  visitor->Trace(tree_scope_);
}

// static
ListStyleTypeData* ListStyleTypeData::CreateString(const AtomicString& value) {
  return MakeGarbageCollected<ListStyleTypeData>(Type::kString, value, nullptr);
}

// static
ListStyleTypeData* ListStyleTypeData::CreateCounterStyle(
    const AtomicString& name,
    const TreeScope* tree_scope) {
  return MakeGarbageCollected<ListStyleTypeData>(Type::kCounterStyle, name,
                                                 tree_scope);
}

EListStyleType ListStyleTypeData::ToDeprecatedListStyleTypeEnum() const {
  DEFINE_STATIC_LOCAL(PredefinedCounterStyleNameMap,
                      predefined_counter_style_name_map,
                      (BuildPredefinedCounterStyleNameMap()));
  if (IsString())
    return EListStyleType::kString;
  auto iterator = predefined_counter_style_name_map.find(GetCounterStyleName());
  if (iterator != predefined_counter_style_name_map.end())
    return iterator->value;
  DCHECK(RuntimeEnabledFeatures::CSSAtRuleCounterStyleEnabled());
  return EListStyleType::kDecimal;
}

}  // namespace blink
