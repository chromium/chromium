// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/css/css_animation_update_scope.h"

#include "third_party/blink/renderer/core/animation/css/css_animations.h"
#include "third_party/blink/renderer/core/animation/document_animations.h"
#include "third_party/blink/renderer/core/animation/element_animations.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

CSSAnimationUpdateScope* CSSAnimationUpdateScope::current_ = nullptr;

CSSAnimationUpdateScope::Data* CSSAnimationUpdateScope::CurrentData() {
  if (!RuntimeEnabledFeatures::CSSDelayedAnimationUpdatesEnabled())
    return nullptr;
  return current_ ? &current_->data_ : nullptr;
}

CSSAnimationUpdateScope::CSSAnimationUpdateScope(Document& document)
    : document_(document) {
  if (!current_)
    current_ = this;
}

CSSAnimationUpdateScope::~CSSAnimationUpdateScope() {
  if (current_ == this) {
    if (RuntimeEnabledFeatures::CSSDelayedAnimationUpdatesEnabled())
      Apply();
    current_ = nullptr;
  }
}

void CSSAnimationUpdateScope::Apply() {
  StyleEngine::InApplyAnimationUpdateScope in_apply_animation_update_scope(
      document_.GetStyleEngine());

  HeapHashSet<Member<Element>> pending;
  std::swap(pending, data_.elements_with_pending_updates_);

  for (auto& element : pending) {
    ElementAnimations* element_animations = element->GetElementAnimations();
    if (!element_animations)
      continue;
    element_animations->CssAnimations().MaybeApplyPendingUpdate(element.Get());
  }

  DCHECK(data_.elements_with_pending_updates_.IsEmpty())
      << "MaybeApplyPendingUpdate must not set further pending updates";
}

void CSSAnimationUpdateScope::Data::SetPendingUpdate(
    Element& element,
    const CSSAnimationUpdate& update) {
  element.EnsureElementAnimations().CssAnimations().SetPendingUpdate(update);
  elements_with_pending_updates_.insert(&element);
}

void CSSAnimationUpdateScope::Data::StoreOldStyleIfNeeded(Element& element) {
  old_styles_.insert(
      &element, scoped_refptr<const ComputedStyle>(element.GetComputedStyle()));
}

const ComputedStyle* CSSAnimationUpdateScope::Data::GetOldStyle(
    Element& element) const {
  auto iter = old_styles_.find(&element);
  if (iter == old_styles_.end())
    return element.GetComputedStyle();
  return iter->value.get();
}

}  // namespace blink
