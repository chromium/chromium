// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/css_toggle_map.h"

#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/element_rare_data_field.h"
#include "third_party/blink/renderer/core/style/toggle_root.h"
#include "third_party/blink/renderer/core/style/toggle_root_list.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

CSSToggleMap::CSSToggleMap(Element* owner_element)
    : owner_element_(owner_element) {}

void CSSToggleMap::Trace(Visitor* visitor) const {
  visitor->Trace(owner_element_);
  visitor->Trace(toggles_);

  ScriptWrappable::Trace(visitor);
  ElementRareDataField::Trace(visitor);
}

void CSSToggleMap::CreateToggles(const ToggleRootList* toggle_roots) {
  const auto& roots = toggle_roots->Roots();
  DCHECK(!roots.empty());
  DCHECK(OwnerElement());

  auto& toggles = Toggles();
  for (const ToggleRoot& root : roots) {
    // We want to leave the table unmodified if the key is already present, as
    // described in https://tabatkins.github.io/css-toggle/#toggle-creation
    // and https://tabatkins.github.io/css-toggle/#toggles .  This is exactly
    // what HashMap::insert() does.
    auto insert_result = toggles.insert(root.Name(), nullptr);
    if (insert_result.is_new_entry) {
      CSSToggle* toggle = MakeGarbageCollected<CSSToggle>(root, *this);
      insert_result.stored_value->value = toggle;
      toggle->SetNeedsStyleRecalc(OwnerElement(),
                                  CSSToggle::PostRecalcAt::kLater);
    }
  }
}

CSSToggleMap* CSSToggleMap::set(const AtomicString& key,
                                CSSToggle* value,
                                ExceptionState& exception_state) {
  // The specification describes the name as stored by the toggle map;
  // however, it's convenient in our implementation to store it on the toggle
  // instead (by inheriting from ToggleRoot).  And since a toggle can only be
  // in one map at once, it's not distinguishable by the API user.

  if (EqualIgnoringASCIICase(key, "none")) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kSyntaxError,
        "The key \"" + key + "\" is not allowed.");
    return this;
  }

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
                               const String& key,
                               CSSToggle*& value,
                               ExceptionState&) {
  auto iterator = toggles_.find(AtomicString(key));
  if (iterator == toggles_.end())
    return false;

  value = iterator->value;
  return true;
}

CSSToggleMapMaplike::IterationSource* CSSToggleMap::CreateIterationSource(
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

bool CSSToggleMap::IterationSource::FetchNextItem(ScriptState*,
                                                  String& key,
                                                  CSSToggle*& value,
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
