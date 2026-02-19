// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/resolver/cascade_map.h"

#include "base/compiler_specific.h"
#include "third_party/blink/renderer/core/css/properties/css_property.h"

namespace blink {

static_assert(
    std::is_trivially_destructible<CascadeMap::CascadePriorityList>::value,
    "Destructor is never called on CascadePriorityList objects created here");

static_assert(
    !VectorTraits<CascadeMap::CascadePriorityList::Node>::kNeedsDestruction,
    "Backing vector should not need destruction");

CascadePriority CascadeMap::At(const CSSPropertyName& name) const {
  if (const CascadePriority* find_result = Find(name)) {
    return *find_result;
  }
  return CascadePriority();
}

const CascadePriority* CascadeMap::Find(const CSSPropertyName& name) const {
  if (name.IsCustomProperty()) {
    auto iter = custom_properties_.find(name.ToAtomicString());
    if (iter != custom_properties_.end()) {
      return &iter->value.Top(backing_vector_);
    }
    return nullptr;
  }
  size_t index = static_cast<size_t>(name.Id());
  DCHECK_LT(index, static_cast<size_t>(kNumCSSProperties));
  return native_properties_.Bits().Has(name.Id())
             ? UNSAFE_BUFFERS(&native_properties_.Buffer()[index])
                   .Top(backing_vector_)
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
      if (origin >= iter->GetOrigin()) {
        return &(*iter);
      }
    }
    return nullptr;
  };

  if (name.IsCustomProperty()) {
    DCHECK(custom_properties_.Contains(name.ToAtomicString()));
    return find_origin(custom_properties_.find(name.ToAtomicString())->value,
                       origin);
  }

  DCHECK(native_properties_.Bits().Has(name.Id()));
  size_t index = static_cast<size_t>(name.Id());
  DCHECK_LT(index, static_cast<size_t>(kNumCSSProperties));
  return find_origin(UNSAFE_BUFFERS(native_properties_.Buffer()[index]),
                     origin);
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
      if (iter->ForLayerComparison() < revert_from) {
        return &(*iter);
      }
    }
    return nullptr;
  };

  if (name.IsCustomProperty()) {
    DCHECK(custom_properties_.Contains(name.ToAtomicString()));
    return find_revert_layer(
        custom_properties_.find(name.ToAtomicString())->value, revert_from);
  }

  DCHECK(native_properties_.Bits().Has(name.Id()));
  size_t index = static_cast<size_t>(name.Id());
  DCHECK_LT(index, static_cast<size_t>(kNumCSSProperties));
  return find_revert_layer(UNSAFE_BUFFERS(native_properties_.Buffer()[index]),
                           revert_from);
}

const CascadePriority* CascadeMap::FindRevertRule(
    const CSSPropertyName& name,
    CascadePriority revert_from) const {
  auto find_revert_rule =
      [this](const CascadeMap::CascadePriorityList& list,
             CascadePriority revert_from) -> const CascadePriority* {
    for (auto iter = list.Begin(backing_vector_);
         iter != list.End(backing_vector_); ++iter) {
      if (*iter < revert_from &&
          iter->GetRuleIndex() != revert_from.GetRuleIndex()) {
        return &(*iter);
      }
    }
    return nullptr;
  };

  if (name.IsCustomProperty()) {
    DCHECK(custom_properties_.Contains(name.ToAtomicString()));
    return find_revert_rule(
        custom_properties_.find(name.ToAtomicString())->value, revert_from);
  }

  DCHECK(native_properties_.Bits().Has(name.Id()));
  size_t index = static_cast<size_t>(name.Id());
  DCHECK_LT(index, static_cast<size_t>(kNumCSSProperties));
  return find_revert_rule(UNSAFE_BUFFERS(native_properties_.Buffer()[index]),
                          revert_from);
}

void CascadeMap::Add(const AtomicString& custom_property_name,
                     CascadePriority priority) {
  auto result =
      custom_properties_.insert(custom_property_name, CascadePriorityList());
  CascadePriorityList* list = &result.stored_value->value;
  if (list->IsEmpty()) {
    list->Push(backing_vector_, priority);
    return;
  }
  Add(list, priority);
}

static CSSPropertyID UnvisitedID(CSSPropertyID id) {
  CSSPropertyID unvisited_id =
      CSSProperty::UnvisitedID(static_cast<unsigned>(id));
  return unvisited_id == CSSPropertyID::kInvalid ? id : unvisited_id;
}

void CascadeMap::Add(CSSPropertyID id, CascadePriority priority) {
  DCHECK_NE(id, CSSPropertyID::kInvalid);
  DCHECK_NE(id, CSSPropertyID::kVariable);
  DCHECK(!CSSProperty::Get(id).IsSurrogate());

  size_t index = static_cast<size_t>(static_cast<unsigned>(id));
  DCHECK_LT(index, static_cast<size_t>(kNumCSSProperties));

  if (priority.IsImportant()) {
    if (!important_set_) {
      important_set_.reset(new CSSBitset);
    }
    // Mark that our winning declaration for this property is going to be
    // an !important declaration; this declaration may not be the eventual
    // winner, but it can only lose to other !important declarations,
    // so we never need to unset the bit. (The winning declaration could
    // resolve to a revert to a non-!important declaration, but the bitmap
    // does not attempt to track resolved values, only declarations.)
    //
    // We use the unvisited ID because visited/unvisited colors are currently
    // interpolated together.
    // TODO(crbug.com/40680035): Interpolate visited colors separately.
    important_set_->Set(UnvisitedID(id));
  }

  CascadePriorityList* list =
      UNSAFE_BUFFERS(&native_properties_.Buffer()[index]);
  if (!native_properties_.Bits().Has(id)) {
    native_properties_.Bits().Set(id);
    new (list) CascadeMap::CascadePriorityList(backing_vector_, priority);
    return;
  }
  Add(list, priority);
}

void CascadeMap::Add(CascadePriorityList* list, CascadePriority priority) {
  CascadePriority& top = list->Top(backing_vector_);
  DCHECK(priority.ForLayerComparison() >= top.ForLayerComparison());
  if (top >= priority) {
    if (priority.IsInlineStyle()) {
      inline_style_lost_ = true;
    }
    list->InsertKeepingSorted(backing_vector_, priority);
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
  list->Push(backing_vector_, priority);
}

void CascadeMap::Reset() {
  inline_style_lost_ = false;
  native_properties_.Bits().Reset();
  custom_properties_.clear();
  backing_vector_.clear();
  important_set_.reset();
#if DCHECK_IS_ON()
  important_set_released_ = false;
#endif
}

void CascadeMap::ClearAppliedFlags() {
  for (CascadePriorityList::Node& node : backing_vector_) {
    node.priority = CascadePriority(node.priority, /*already_applied=*/false);
  }
}

}  // namespace blink
