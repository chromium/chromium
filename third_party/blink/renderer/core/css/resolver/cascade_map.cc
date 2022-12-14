// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/resolver/cascade_map.h"
#include "third_party/blink/renderer/core/css/properties/css_property.h"

namespace blink {

static_assert(
    std::is_trivially_destructible<CascadeMap::CascadePriorityList>::value,
    "Destructor is never called on CascadePriorityList objects created here");

static_assert(
    !VectorTraits<CascadeMap::CascadePriorityList::Node>::kNeedsDestruction,
    "Backing vector should not need destruction");

namespace {}  // namespace

CascadePriority CascadeMap::At(const CSSPropertyName& name) const {
  if (const CascadePriority* find_result = Find(name))
    return *find_result;
  return CascadePriority();
}

const CascadePriority* CascadeMap::Find(const CSSPropertyName& name) const {
  if (name.IsCustomProperty()) {
    auto iter = custom_properties_.find(name);
    if (iter != custom_properties_.end())
      return &iter->value.Top(backing_vector_);
    return nullptr;
  }
  size_t index = static_cast<size_t>(name.Id());
  DCHECK_LT(index, static_cast<size_t>(kNumCSSProperties));
  return native_properties_.Bits().Has(name.Id())
             ? &native_properties_.Buffer()[index].Top(backing_vector_)
             : nullptr;
}

CascadePriority* CascadeMap::Find(const CSSPropertyName& name) {
  const CascadeMap* const_this = this;
  return const_cast<CascadePriority*>(const_this->Find(name));
}

const CascadePriority* CascadeMap::Find(const CSSPropertyName& name,
                                        CascadeOrigin origin) const {
  auto find_origin = [this](const CascadeMap::CascadePriorityList& list,
                            CascadeOrigin origin) -> const CascadePriority* {
    for (auto iter = list.Begin(backing_vector_);
         iter != list.End(backing_vector_); ++iter) {
      if (origin >= iter->GetOrigin())
        return &(*iter);
    }
    return nullptr;
  };

  if (name.IsCustomProperty()) {
    DCHECK(custom_properties_.Contains(name));
    return find_origin(custom_properties_.find(name)->value, origin);
  }

  DCHECK(native_properties_.Bits().Has(name.Id()));
  size_t index = static_cast<size_t>(name.Id());
  DCHECK_LT(index, static_cast<size_t>(kNumCSSProperties));
  return find_origin(native_properties_.Buffer()[index], origin);
}

CascadePriority& CascadeMap::Top(CascadePriorityList& list) {
  return list.Top(backing_vector_);
}

const CascadePriority* CascadeMap::FindRevertLayer(const CSSPropertyName& name,
                                                   uint64_t revert_from) const {
  auto find_revert_layer = [this](
                               const CascadeMap::CascadePriorityList& list,
                               uint64_t revert_from) -> const CascadePriority* {
    for (auto iter = list.Begin(backing_vector_);
         iter != list.End(backing_vector_); ++iter) {
      if (iter->ForLayerComparison() < revert_from)
        return &(*iter);
    }
    return nullptr;
  };

  if (name.IsCustomProperty()) {
    DCHECK(custom_properties_.Contains(name));
    return find_revert_layer(custom_properties_.find(name)->value, revert_from);
  }

  DCHECK(native_properties_.Bits().Has(name.Id()));
  size_t index = static_cast<size_t>(name.Id());
  DCHECK_LT(index, static_cast<size_t>(kNumCSSProperties));
  return find_revert_layer(native_properties_.Buffer()[index], revert_from);
}

void CascadeMap::Add(const CSSPropertyName& name, CascadePriority priority) {
  CascadePriorityList* list;
  if (name.IsCustomProperty()) {
    auto result = custom_properties_.insert(name, CascadePriorityList());
    list = &result.stored_value->value;
    if (list->IsEmpty()) {
      list->Push(backing_vector_, priority);
      return;
    }
  } else {
    DCHECK(!CSSProperty::Get(name.Id()).IsSurrogate());

    CSSPropertyID id = name.Id();
    size_t index = static_cast<size_t>(id);
    DCHECK_LT(index, static_cast<size_t>(kNumCSSProperties));

    // Set bit in high_priority_, if appropriate.
    static_assert(static_cast<int>(kLastHighPriorityCSSProperty) < 64,
                  "CascadeMap supports at most 63 high-priority properties");
    if (IsHighPriority(id))
      high_priority_ |= (1ull << index);
    has_important_ |= priority.IsImportant();

    list = &native_properties_.Buffer()[index];
    if (!native_properties_.Bits().Has(id)) {
      native_properties_.Bits().Set(id);
      new (list) CascadeMap::CascadePriorityList(backing_vector_, priority);
      return;
    }
  }

  CascadePriority& top = list->Top(backing_vector_);
  DCHECK(priority.ForLayerComparison() >= top.ForLayerComparison());
  if (top >= priority) {
    if (priority.IsInlineStyle()) {
      inline_style_lost_ = true;
    }
    return;
  }
  if (top.IsInlineStyle()) {
    // Something with a higher priority overrides something from the
    // inline style, so we need to set the flag. But note that
    // we _could_ have this layer be negated by “revert”; if so,
    // this value will be a false positive. But since we only
    // use it to disable an optimization (incremental inline
    // style computation), false positives are fine.
    inline_style_lost_ = true;
  }
  if (top.ForLayerComparison() < priority.ForLayerComparison())
    list->Push(backing_vector_, priority);
  else
    top = priority;
}

void CascadeMap::Reset() {
  inline_style_lost_ = false;
  high_priority_ = 0;
  has_important_ = false;
  native_properties_.Bits().Reset();
  custom_properties_.clear();
  backing_vector_.clear();
}

}  // namespace blink
