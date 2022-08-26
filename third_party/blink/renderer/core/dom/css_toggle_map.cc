// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/css_toggle_map.h"

#include "third_party/blink/renderer/core/dom/element.h"

namespace blink {

CSSToggleMap::CSSToggleMap(Element* owner_element)
    : owner_element_(owner_element) {}

void CSSToggleMap::Trace(Visitor* visitor) const {
  visitor->Trace(owner_element_);
  visitor->Trace(toggles_);

  ScriptWrappable::Trace(visitor);
}

CSSToggleMap* CSSToggleMap::set(const AtomicString& key, CSSToggle* value) {
  // The specification describes the name as stored by the toggle map;
  // however, it's convenient in our implementation to store it on the toggle
  // instead (by inheriting from ToggleRoot).  And since a toggle can only be
  // in one map at once, it's not distinguishable by the API user.

  CSSToggleMap* old_map = value->OwnerToggleMap();
  if (old_map == this && key == value->Name())
    return this;

  if (old_map) {
    value->SetNeedsStyleRecalc(old_map->OwnerElement(),
                               CSSToggle::PostRecalcAt::kNow);
    old_map->Toggles().erase(value->Name());
  }
  value->ChangeOwner(*this, key);
  toggles_.insert(key, value);

  if (old_map != this) {
    value->SetNeedsStyleRecalc(OwnerElement(), CSSToggle::PostRecalcAt::kNow);
  }

  return this;
}

void CSSToggleMap::clearForBinding(ScriptState*, ExceptionState&) {
  for (auto& [name, toggle] : toggles_) {
    toggle->SetNeedsStyleRecalc(OwnerElement(), CSSToggle::PostRecalcAt::kNow);
  }
  toggles_.clear();
}

bool CSSToggleMap::deleteForBinding(ScriptState*,
                                    const AtomicString& key,
                                    ExceptionState&) {
  auto toggles_iterator = toggles_.find(key);
  if (toggles_iterator == toggles_.end())
    return false;

  toggles_iterator->value->SetNeedsStyleRecalc(OwnerElement(),
                                               CSSToggle::PostRecalcAt::kNow);
  toggles_.erase(toggles_iterator);

  return true;
}

bool CSSToggleMap::GetMapEntry(ScriptState*,
                               const AtomicString& key,
                               Member<CSSToggle>& value,
                               ExceptionState&) {
  auto iterator = toggles_.find(key);
  if (iterator == toggles_.end())
    return false;

  value = iterator->value;
  return true;
}

CSSToggleMapMaplike::IterationSource* CSSToggleMap::StartIteration(
    ScriptState*,
    ExceptionState&) {
  return MakeGarbageCollected<IterationSource>(*this);
}

CSSToggleMap::IterationSource::IterationSource(const CSSToggleMap& toggle_map) {
  // TODO(https://crbug.com/1354597): Could this be simplified?
  toggles_snapshot_.ReserveInitialCapacity(toggle_map.size());
  for (const auto& [name, toggle] : toggle_map.toggles_) {
    // TODO(dbaron): Should this copy-construct the toggle?  (Highlight does!)
    toggles_snapshot_.push_back(toggle.Get());
  }
}

bool CSSToggleMap::IterationSource::Next(ScriptState*,
                                         AtomicString& key,
                                         Member<CSSToggle>& value,
                                         ExceptionState&) {
  if (index_ >= toggles_snapshot_.size())
    return false;
  value = toggles_snapshot_[index_++];
  key = value->Name();
  return true;
}

void CSSToggleMap::IterationSource::Trace(blink::Visitor* visitor) const {
  visitor->Trace(toggles_snapshot_);
  CSSToggleMapMaplike::IterationSource::Trace(visitor);
}

}  // namespace blink
