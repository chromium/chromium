// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/view_transition/view_transition_content_element.h"

#include "third_party/blink/renderer/core/layout/layout_view_transition_content.h"

namespace blink {

ViewTransitionContentElement::ViewTransitionContentElement(
    Element* parent,
    PseudoId pseudo_id,
    const AtomicString& view_transition_name,
    viz::ViewTransitionElementResourceId resource_id,
    bool is_live_content_element,
    const ViewTransitionStyleTracker* style_tracker)
    : ViewTransitionPseudoElementBase(parent,
                                      pseudo_id,
                                      view_transition_name,
                                      style_tracker),
      resource_id_(resource_id),
      is_live_content_element_(is_live_content_element) {
  DCHECK(resource_id_.IsValid());
}

ViewTransitionContentElement::~ViewTransitionContentElement() = default;

void ViewTransitionContentElement::SetIntrinsicSize(
    const gfx::RectF& captured_rect,
    const gfx::RectF& border_box_rect) {
  captured_rect_ = captured_rect;
  border_box_rect_ = border_box_rect;
  if (auto* layout_object = GetLayoutObject()) {
    static_cast<LayoutViewTransitionContent*>(layout_object)
        ->OnIntrinsicSizeUpdated(captured_rect_, border_box_rect_);
  }
}

LayoutObject* ViewTransitionContentElement::CreateLayoutObject(
    const ComputedStyle&) {
  return MakeGarbageCollected<LayoutViewTransitionContent>(this);
}

}  // namespace blink
