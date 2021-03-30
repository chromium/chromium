// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/style/list_style_type_data.h"

#include "third_party/blink/renderer/core/css/counter_style.h"
#include "third_party/blink/renderer/core/css/css_value_id_mappings.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/tree_scope.h"
#include "third_party/blink/renderer/core/style/content_data.h"
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

EListStyleType CounterStyleNameToDeprecatedEnum(const AtomicString& name) {
  DEFINE_STATIC_LOCAL(PredefinedCounterStyleNameMap,
                      predefined_counter_style_name_map,
                      (BuildPredefinedCounterStyleNameMap()));
  auto iterator = predefined_counter_style_name_map.find(name);
  if (iterator != predefined_counter_style_name_map.end())
    return iterator->value;
  DCHECK(RuntimeEnabledFeatures::CSSAtRuleCounterStyleEnabled());
  return EListStyleType::kDecimal;
}

}  // namespace

void ListStyleTypeData::Trace(Visitor* visitor) const {
  visitor->Trace(tree_scope_);
  visitor->Trace(counter_style_);
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
  if (IsString())
    return EListStyleType::kString;
  return CounterStyleNameToDeprecatedEnum(GetCounterStyleName());
}

// TODO(crbug.com/687225): We temporarily put this function here to share the
// common logic. Clean it up when @counter-style is shipped.
EListStyleType CounterContentData::ToDeprecatedListStyleTypeEnum() const {
  if (ListStyle() == "none")
    return EListStyleType::kNone;
  return CounterStyleNameToDeprecatedEnum(ListStyle());
}

bool ListStyleTypeData::IsCounterStyleReferenceValid(Document& document) const {
  DCHECK(RuntimeEnabledFeatures::CSSAtRuleCounterStyleEnabled());

  if (!IsCounterStyle()) {
    DCHECK(!counter_style_);
    return true;
  }

  if (!counter_style_ || counter_style_->IsDirty())
    return false;

  // Even if the referenced counter style is clean, it may still be stale if new
  // counter styles have been inserted, in which case the same (scope, name) now
  // refers to a different counter style. So we make an extra lookup to verify.
  return counter_style_ ==
         &document.GetStyleEngine().FindCounterStyleAcrossScopes(
             GetCounterStyleName(), GetTreeScope());
}

const CounterStyle& ListStyleTypeData::GetCounterStyle(
    Document& document) const {
  DCHECK(RuntimeEnabledFeatures::CSSAtRuleCounterStyleEnabled());
  DCHECK(IsCounterStyle());
  if (!IsCounterStyleReferenceValid(document)) {
    counter_style_ = document.GetStyleEngine().FindCounterStyleAcrossScopes(
        GetCounterStyleName(), GetTreeScope());
  }
  return *counter_style_;
}

}  // namespace blink
