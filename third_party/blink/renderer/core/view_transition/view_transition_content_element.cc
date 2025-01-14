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
    bool is_generated_name,
    const ViewTransitionStyleTracker* style_tracker)
    : ViewTransitionPseudoElementBase(parent,
                                      pseudo_id,
                                      view_transition_name,
                                      is_generated_name,
                                      style_tracker),
      resource_id_(resource_id),
      is_live_content_element_(is_live_content_element) {
  DCHECK(resource_id_.IsValid());
}

ViewTransitionContentElement::~ViewTransitionContentElement() = default;

void ViewTransitionContentElement::SetIntrinsicSize(
    const gfx::RectF& captured_rect,
    const gfx::RectF& reference_rect_in_enclosing_layer_space,
    bool propagate_max_extent_rect) {
  captured_rect_ = captured_rect;
  reference_rect_in_enclosing_layer_space_ =
      reference_rect_in_enclosing_layer_space;
  propagate_max_extent_rect_ = propagate_max_extent_rect;
  if (auto* layout_object = GetLayoutObject()) {
    static_cast<LayoutViewTransitionContent*>(layout_object)
        ->OnIntrinsicSizeUpdated(captured_rect_,
                                 reference_rect_in_enclosing_layer_space_,
                                 propagate_max_extent_rect_);
  }
}

LayoutObject* ViewTransitionContentElement::CreateLayoutObject(
    const ComputedStyle&) {
  return MakeGarbageCollected<LayoutViewTransitionContent>(this);
}

}  // namespace blink
