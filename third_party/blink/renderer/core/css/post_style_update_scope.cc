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

namespace blink {

PostStyleUpdateScope* PostStyleUpdateScope::current_ = nullptr;

PostStyleUpdateScope::AnimationData*
PostStyleUpdateScope::CurrentAnimationData() {
  return current_ ? &current_->animation_data_ : nullptr;
}

PostStyleUpdateScope::PseudoData* PostStyleUpdateScope::CurrentPseudoData() {
  return current_ ? current_->GetPseudoData() : nullptr;
}

PostStyleUpdateScope::PostStyleUpdateScope(Document& document)
    : document_(document) {
  if (!current_) {
    current_ = this;
  }
}

PostStyleUpdateScope::~PostStyleUpdateScope() {
  if (current_ == this) {
    current_ = nullptr;
  }
  DCHECK(animation_data_.elements_with_pending_updates_.empty())
      << "Missing Apply (animations)";
  DCHECK(pseudo_data_.pending_backdrops_.empty())
      << "Missing Apply (::backdrop)";
}

bool PostStyleUpdateScope::Apply() {
  if (ApplyPseudo()) {
    return true;
  }
  ApplyAnimations();
  document_.RemoveFinishedTopLayerElements();
  return false;
}

bool PostStyleUpdateScope::ApplyPseudo() {
  nullify_pseudo_data_ = true;

  if (pseudo_data_.pending_backdrops_.empty()) {
    return false;
  }

  HeapVector<Member<Element>> pending_backdrops;
  std::swap(pending_backdrops, pseudo_data_.pending_backdrops_);

  for (Member<Element>& element : pending_backdrops) {
    element->ApplyPendingBackdropPseudoElementUpdate();
  }

  return true;
}

void PostStyleUpdateScope::ApplyAnimations() {
  StyleEngine::InApplyAnimationUpdateScope in_apply_animation_update_scope(
      document_.GetStyleEngine());

  HeapHashSet<Member<Element>> pending;
  std::swap(pending, animation_data_.elements_with_pending_updates_);

  for (auto& element : pending) {
    ElementAnimations* element_animations = element->GetElementAnimations();
    if (!element_animations) {
      continue;
    }
    element_animations->CssAnimations().MaybeApplyPendingUpdate(element.Get());
  }

  // NOTE(crbug.com/446159591): With AnimationTrigger enabled, we see renderer
  // hang reports. This hang should be fixed before enabling AnimationTrigger.
  // NOTE: We avoid performing the trigger attachments if we know we still need
  // to run layout because the trigger names (and scopes) are made visible by
  // propagating them through the fragment tree which happens during layout.
  // Otherwise, we run the risk of performing attachments based on obsolete
  // trigger names and scopes.
  if (RuntimeEnabledFeatures::AnimationTriggerEnabled() &&
      !document_.View()->NeedsLayout()) {
    // TODO(crbug.com/469286970): This is too late a point at which to figure
    // out which animations are attached to which triggers as a trigger might
    // already be in a tripped state and should be playing an animation whose
    // effect should be reflected in getComputedstyle. This results in
    // producing a frame with a flash-of-no-animation where the animation
    // should be playing. We should move some of this logic to the style
    // calculation code so that when we apply pending updates above we account
    // for animations that should already be triggered.
    document_.GetDocumentAnimations().UpdateAnimationTriggerAttachments();
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
  old_styles_.insert(&element,
                     ComputedStyle::NullifyEnsured(element.GetComputedStyle()));
}

const ComputedStyle* PostStyleUpdateScope::AnimationData::GetOldStyle(
    const Element& element) const {
  auto iter = old_styles_.find(&element);
  if (iter == old_styles_.end()) {
    return ComputedStyle::NullifyEnsured(element.GetComputedStyle());
  }
  return iter->value.Get();
}

void PostStyleUpdateScope::PseudoData::AddPendingBackdrop(
    Element& originating_element) {
  pending_backdrops_.push_back(&originating_element);
}

const ComputedStyle* PostStyleUpdateScope::GetOldStyle(const Element& element) {
  if (PostStyleUpdateScope::AnimationData* data =
          PostStyleUpdateScope::CurrentAnimationData()) {
    return data->GetOldStyle(element);
  }
  return ComputedStyle::NullifyEnsured(element.GetComputedStyle());
}

}  // namespace blink
