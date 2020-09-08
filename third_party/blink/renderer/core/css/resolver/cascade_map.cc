// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/resolver/cascade_map.h"
#include "third_party/blink/renderer/core/css/properties/css_property.h"

namespace blink {

static_assert(
    std::is_trivially_destructible<CascadePriority>::value,
    "~CascadePriority is never called on CascadePriority objects created here");

namespace {

inline void AddCustom(const CSSPropertyName& name,
                      CascadePriority priority,
                      CascadeMap::CustomMap& map) {
  auto result = map.insert(name, priority);
  if (result.is_new_entry || result.stored_value->value < priority)
    result.stored_value->value = priority;
}

inline void AddNative(CSSPropertyID id,
                      CascadePriority priority,
                      CascadeMap::NativeMap& map) {
  CascadePriority* p = map.Buffer() + static_cast<size_t>(id);
  if (!map.Bits().Has(id) || *p < priority) {
    map.Bits().Set(id);
    new (p) CascadePriority(priority);
  }
}

inline CascadePriority* FindCustom(const CSSPropertyName& name,
                                   CascadeMap::CustomMap& map) {
  auto iter = map.find(name);
  if (iter != map.end())
    return &iter->value;
  return nullptr;
}

inline CascadePriority* FindNative(const CSSPropertyName& name,
                                   CascadeMap::NativeMap& map) {
  size_t index = static_cast<size_t>(name.Id());
  DCHECK_LT(index, static_cast<size_t>(numCSSProperties));
  return map.Bits().Has(name.Id()) ? (map.Buffer() + index) : nullptr;
}

inline CascadePriority AtCustom(const CSSPropertyName& name,
                                const CascadeMap::CustomMap& map) {
  return map.at(name);
}

inline CascadePriority AtNative(const CSSPropertyName& name,
                                const CascadeMap::NativeMap& map) {
  size_t index = static_cast<size_t>(name.Id());
  DCHECK_LT(index, static_cast<size_t>(numCSSProperties));
  return map.Bits().Has(name.Id()) ? map.Buffer()[index] : CascadePriority();
}

}  // namespace

CascadePriority CascadeMap::At(const CSSPropertyName& name) const {
  if (name.IsCustomProperty())
    return AtCustom(name, custom_properties_);
  return AtNative(name, native_properties_);
}

CascadePriority CascadeMap::At(const CSSPropertyName& name,
                               CascadeOrigin origin) const {
  if (name.IsCustomProperty()) {
    if (origin <= CascadeOrigin::kUserAgent)
      return CascadePriority();
    if (origin <= CascadeOrigin::kUser)
      return AtCustom(name, custom_user_properties_);
    return AtCustom(name, custom_properties_);
  }

  if (origin <= CascadeOrigin::kUserAgent)
    return AtNative(name, native_ua_properties_);
  if (origin <= CascadeOrigin::kUser)
    return AtNative(name, native_user_properties_);
  return AtNative(name, native_properties_);
}

CascadePriority* CascadeMap::Find(const CSSPropertyName& name) {
  if (name.IsCustomProperty())
    return FindCustom(name, custom_properties_);
  return FindNative(name, native_properties_);
}

CascadePriority* CascadeMap::Find(const CSSPropertyName& name,
                                  CascadeOrigin origin) {
  if (name.IsCustomProperty()) {
    if (origin <= CascadeOrigin::kUserAgent)
      return nullptr;
    if (origin <= CascadeOrigin::kUser)
      return FindCustom(name, custom_user_properties_);
    return FindCustom(name, custom_properties_);
  }

  if (origin <= CascadeOrigin::kUserAgent)
    return FindNative(name, native_ua_properties_);
  if (origin <= CascadeOrigin::kUser)
    return FindNative(name, native_user_properties_);
  return FindNative(name, native_properties_);
}

void CascadeMap::Add(const CSSPropertyName& name, CascadePriority priority) {
  CascadeOrigin origin = priority.GetOrigin();

  if (name.IsCustomProperty()) {
    DCHECK_NE(CascadeOrigin::kUserAgent, origin);
    if (origin <= CascadeOrigin::kUser)
      AddCustom(name, priority, custom_user_properties_);
    AddCustom(name, priority, custom_properties_);
    return;
  }

  DCHECK(!CSSProperty::Get(name.Id()).IsSurrogate());

  CSSPropertyID id = name.Id();
  size_t index = static_cast<size_t>(id);
  DCHECK_LT(index, static_cast<size_t>(numCSSProperties));

  // Set bit in high_priority_, if appropriate.
  static_assert(static_cast<int>(kLastHighPriorityCSSProperty) < 64,
                "CascadeMap supports at most 63 high-priority properties");
  if (IsHighPriority(id))
    high_priority_ |= (1ull << index);
  has_important_ |= priority.IsImportant();

  if (origin <= CascadeOrigin::kUserAgent)
    AddNative(id, priority, native_ua_properties_);
  if (origin <= CascadeOrigin::kUser)
    AddNative(id, priority, native_user_properties_);
  AddNative(id, priority, native_properties_);
}

void CascadeMap::Reset() {
  high_priority_ = 0;
  has_important_ = false;
  native_properties_.Bits().Reset();
  native_ua_properties_.Bits().Reset();
  native_user_properties_.Bits().Reset();
  custom_properties_.clear();
  custom_user_properties_.clear();
}

}  // namespace blink
