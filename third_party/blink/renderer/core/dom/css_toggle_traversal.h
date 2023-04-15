// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_CSS_TOGGLE_TRAVERSAL_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_CSS_TOGGLE_TRAVERSAL_H_

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/core/dom/css_toggle.h"
#include "third_party/blink/renderer/core/dom/css_toggle_map.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/style/toggle_group.h"
#include "third_party/blink/renderer/core/style/toggle_group_list.h"

namespace blink {

struct CSSToggleGroupScopeIteratorTraits {
  static absl::optional<ToggleScope> Lookup(Element* element,
                                            const AtomicString& name) {
    // TODO(https://crbug.com/1250716): What if style is null?  See
    // https://github.com/tabatkins/css-toggle/issues/24 .
    if (const ComputedStyle* style = element->GetComputedStyle()) {
      if (const ToggleGroupList* toggle_groups = style->ToggleGroup()) {
        for (const ToggleGroup& group : toggle_groups->Groups()) {
          // TODO(https://github.com/tabatkins/css-toggle/issues/25):
          // Consider multiple occurrences of the same name.
          if (group.Name() == name) {
            return group.Scope();
          }
        }
      }
    }
    return absl::nullopt;
  }
};

struct CSSToggleScopeIteratorTraits {
  static absl::optional<ToggleScope> Lookup(Element* element,
                                            const AtomicString& name) {
    if (CSSToggleMap* toggle_map = element->GetToggleMap()) {
      const auto& toggles = toggle_map->Toggles();
      auto iter = toggles.find(name);
      if (iter != toggles.end()) {
        return iter->value->Scope();
      }
    }
    return absl::nullopt;
  }
};

// An iterator over the elements that are in the scope of a named toggle
// or toggle group.  (This excludes elements that are not in the scope
// because they are in the scope of a closer toggle or toggle group with
// the same name.)
template <class IteratorTraits>
class CSSTogglesScopeIteratorBase {
  STACK_ALLOCATED();

 private:
  Element* current_;
  Element* stay_within_;
  AtomicString name_;

 public:
  CSSTogglesScopeIteratorBase(Element* current,
                              Element* stay_within,
                              const AtomicString& name)
      : current_(current), stay_within_(stay_within), name_(name) {}

  void operator++() {
    Element* e = ElementTraversal::Next(*current_, stay_within_);
    while (e) {
      // Skip descendants in a different group.
      absl::optional<ToggleScope> element_scope =
          IteratorTraits::Lookup(e, name_);
      if (!element_scope) {
        break;
      }
      switch (*element_scope) {
        case ToggleScope::kWide:
          if (e->parentElement()) {
            e = ElementTraversal::NextSkippingChildren(*e->parentElement(),
                                                       stay_within_);
          } else {
            e = nullptr;
          }
          break;
        case ToggleScope::kNarrow:
          e = ElementTraversal::NextSkippingChildren(*e, stay_within_);
          break;
      }
    }
    current_ = e;
  }

  Element* operator*() const { return current_; }

  bool operator==(const CSSTogglesScopeIteratorBase& other) const {
    DCHECK(stay_within_ == other.stay_within_);
    DCHECK(name_ == other.name_);
    return current_ == other.current_;
  }

  bool operator!=(const CSSTogglesScopeIteratorBase& other) const {
    return !(*this == other);
  }
};

// The elements that are in the scope of a named toggle or toggle group.
// (This excludes elements that are not in the scope because they are in
// the scope of a closer toggle or toggle group with the same name.)
template <class IteratorTraits>
class CSSTogglesScopeRangeBase {
  STACK_ALLOCATED();

 private:
  Element* establishing_element_;
  Element* stay_within_;
  AtomicString name_;

 public:
  typedef CSSTogglesScopeIteratorBase<IteratorTraits> Iterator;

  CSSTogglesScopeRangeBase(Element* establishing_element,
                           const AtomicString& name,
                           ToggleScope scope)
      : establishing_element_(establishing_element), name_(name) {
    switch (scope) {
      case ToggleScope::kNarrow:
        stay_within_ = establishing_element;
        break;
      case ToggleScope::kWide:
        stay_within_ = establishing_element->parentElement();
        break;
    }
  }

  Iterator begin() {
    return Iterator(establishing_element_, stay_within_, name_);
  }
  Iterator end() { return Iterator(nullptr, stay_within_, name_); }
};

typedef CSSTogglesScopeRangeBase<CSSToggleGroupScopeIteratorTraits>
    CSSToggleGroupScopeRange;
typedef CSSTogglesScopeRangeBase<CSSToggleScopeIteratorTraits>
    CSSToggleScopeRange;

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_CSS_TOGGLE_TRAVERSAL_H_
