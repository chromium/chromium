// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_RESOLVER_SELECTOR_FILTER_PARENT_SCOPE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_RESOLVER_SELECTOR_FILTER_PARENT_SCOPE_H_

#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/css/selector_filter.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"

namespace blink {

// Maintains the parent element stack (and bloom filter) inside RecalcStyle.
// SelectorFilterParentScope for the parent element is added to the stack before
// recalculating style for its children. The bloom filter is populated lazily by
// PushParentIfNeeded().
class CORE_EXPORT SelectorFilterParentScope {
  STACK_ALLOCATED();

 public:
  explicit SelectorFilterParentScope(Element& parent)
      : SelectorFilterParentScope(&parent, ScopeType::kParent) {
    DCHECK(previous_);
    DCHECK(previous_->scope_type_ == ScopeType::kRoot ||
           (previous_->parent_ &&
            &previous_->parent_->GetDocument() == &parent.GetDocument()));
  }
  ~SelectorFilterParentScope();

  static void EnsureParentStackIsPushed();

 protected:
  enum class ScopeType { kParent, kRoot };
  SelectorFilterParentScope(Element* parent, ScopeType scope);

 private:
  void PushParentIfNeeded();
  void PushAncestors(Element&);
  void PopAncestors(Element&);

  Element* parent_;
  bool pushed_ = false;
  ScopeType scope_type_;
  SelectorFilterParentScope* previous_;
  StyleResolver* resolver_;

  static SelectorFilterParentScope* current_scope_;
};

// When starting the style recalc, we push an object of this class onto the
// stack to establish a root scope for the SelectorFilter for a document to
// make the style recalc re-entrant. If we do a style recalc for a document
// inside a style recalc for another document (which can happen when
// synchronously loading an svg generated content image), the previous_ pointer
// for the root scope of the inner recalc will point to the current scope of the
// outer one, but the root scope will isolate the inner from trying to push any
// parent stacks in the outer document.
class CORE_EXPORT SelectorFilterRootScope final
    : private SelectorFilterParentScope {
  STACK_ALLOCATED();

 public:
  // |parent| is nullptr when the documentElement() is the style recalc root.
  explicit SelectorFilterRootScope(Element* parent)
      : SelectorFilterParentScope(parent, ScopeType::kRoot) {}
};

inline SelectorFilterParentScope::SelectorFilterParentScope(
    Element* parent,
    ScopeType scope_type)
    : parent_(parent),
      scope_type_(scope_type),
      previous_(current_scope_),
      resolver_(nullptr) {
  DCHECK(scope_type != ScopeType::kRoot || !parent || !previous_ ||
         !previous_->parent_ ||
         &parent_->GetDocument() != &previous_->parent_->GetDocument());
  if (parent) {
    DCHECK(parent->GetDocument().InStyleRecalc());
    resolver_ = &parent->GetDocument().GetStyleResolver();
  }
  current_scope_ = this;
}

inline SelectorFilterParentScope::~SelectorFilterParentScope() {
  current_scope_ = previous_;
  if (!pushed_)
    return;
  DCHECK(resolver_);
  DCHECK(parent_);
  resolver_->GetSelectorFilter().PopParent(*parent_);
  if (scope_type_ == ScopeType::kRoot)
    PopAncestors(*parent_);
}

inline void SelectorFilterParentScope::EnsureParentStackIsPushed() {
  if (current_scope_)
    current_scope_->PushParentIfNeeded();
}

inline void SelectorFilterParentScope::PushParentIfNeeded() {
  if (pushed_)
    return;
  if (!parent_) {
    DCHECK(scope_type_ == ScopeType::kRoot);
    return;
  }
  if (scope_type_ == ScopeType::kRoot) {
    PushAncestors(*parent_);
  } else {
    DCHECK(previous_);
    previous_->PushParentIfNeeded();
  }
  resolver_->GetSelectorFilter().PushParent(*parent_);
  pushed_ = true;
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_RESOLVER_SELECTOR_FILTER_PARENT_SCOPE_H_
