// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/post_style_update_scope.h"

#include "third_party/blink/renderer/core/animation/css/css_animations.h"
#include "third_party/blink/renderer/core/animation/document_animations.h"
#include "third_party/blink/renderer/core/animation/element_animations.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"

namespace blink {

PostStyleUpdateScope* PostStyleUpdateScope::current_ = nullptr;

PostStyleUpdateScope::AnimationData*
PostStyleUpdateScope::CurrentAnimationData() {
  return current_ ? &current_->animation_data_ : nullptr;
}

PostStyleUpdateScope::PostStyleUpdateScope(Document& document)
    : document_(document) {
  if (!current_)
    current_ = this;
}

PostStyleUpdateScope::~PostStyleUpdateScope() {
  if (current_ == this) {
    Apply();
    document_.ClearFocusedElementIfNeeded();
    current_ = nullptr;
  }
}

void PostStyleUpdateScope::Apply() {
  StyleEngine::InApplyAnimationUpdateScope in_apply_animation_update_scope(
      document_.GetStyleEngine());

  HeapHashSet<Member<Element>> pending;
  std::swap(pending, animation_data_.elements_with_pending_updates_);

  for (auto& element : pending) {
    ElementAnimations* element_animations = element->GetElementAnimations();
    if (!element_animations)
      continue;
    element_animations->CssAnimations().MaybeApplyPendingUpdate(element.Get());
  }

  DCHECK(animation_data_.elements_with_pending_updates_.empty())
      << "MaybeApplyPendingUpdate must not set further pending updates";
}

void PostStyleUpdateScope::AnimationData::SetPendingUpdate(
    Element& element,
    const CSSAnimationUpdate& update) {
  element.EnsureElementAnimations().CssAnimations().SetPendingUpdate(update);
  elements_with_pending_updates_.insert(&element);
}

void PostStyleUpdateScope::AnimationData::StoreOldStyleIfNeeded(
    Element& element) {
  old_styles_.insert(
      &element, scoped_refptr<const ComputedStyle>(element.GetComputedStyle()));
}

const ComputedStyle* PostStyleUpdateScope::AnimationData::GetOldStyle(
    Element& element) const {
  auto iter = old_styles_.find(&element);
  if (iter == old_styles_.end())
    return element.GetComputedStyle();
  return iter->value.get();
}

}  // namespace blink
