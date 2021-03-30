// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/svg/layout_ng_svg_text.h"

#include "third_party/blink/renderer/core/layout/svg/layout_svg_inline_text.h"
#include "third_party/blink/renderer/core/layout/svg/svg_layout_support.h"
#include "third_party/blink/renderer/core/svg/svg_text_element.h"

namespace blink {

LayoutNGSVGText::LayoutNGSVGText(Element* element)
    : LayoutNGBlockFlowMixin<LayoutSVGBlock>(element),
      needs_text_metrics_update_(true) {
  DCHECK(IsA<SVGTextElement>(element));
}

const char* LayoutNGSVGText::GetName() const {
  NOT_DESTROYED();
  return "LayoutNGSVGText";
}

bool LayoutNGSVGText::IsOfType(LayoutObjectType type) const {
  NOT_DESTROYED();
  return type == kLayoutObjectNGSVGText ||
         LayoutNGBlockFlowMixin<LayoutSVGBlock>::IsOfType(type);
}

bool LayoutNGSVGText::CreatesNewFormattingContext() const {
  NOT_DESTROYED();
  return true;
}

bool LayoutNGSVGText::IsChildAllowed(LayoutObject* child,
                                     const ComputedStyle&) const {
  NOT_DESTROYED();
  return child->IsSVGInline() ||
         (child->IsText() && SVGLayoutSupport::IsLayoutableTextNode(child));
}

void LayoutNGSVGText::AddChild(LayoutObject* child,
                               LayoutObject* before_child) {
  NOT_DESTROYED();
  LayoutSVGBlock::AddChild(child, before_child);
  SubtreeStructureChanged(layout_invalidation_reason::kChildChanged);
}

void LayoutNGSVGText::RemoveChild(LayoutObject* child) {
  NOT_DESTROYED();
  SubtreeStructureChanged(layout_invalidation_reason::kChildChanged);
  LayoutSVGBlock::RemoveChild(child);
}

void LayoutNGSVGText::SubtreeStructureChanged(
    LayoutInvalidationReasonForTracing) {
  NOT_DESTROYED();
  if (BeingDestroyed() || !EverHadLayout())
    return;
  if (DocumentBeingDestroyed())
    return;

  SetNeedsTextMetricsUpdate();
}

void LayoutNGSVGText::UpdateFont() {
  for (LayoutObject* descendant = FirstChild(); descendant;
       descendant = descendant->NextInPreOrder(this)) {
    if (auto* text = DynamicTo<LayoutSVGInlineText>(descendant))
      text->UpdateScaledFont();
  }
}

void LayoutNGSVGText::UpdateBlockLayout(bool relayout_children) {
  NOT_DESTROYED();

  // If the root layout size changed (eg. window size changes), or the screen
  // scale factor has changed, then recompute the on-screen font size. Since
  // the computation of layout attributes uses the text metrics, we need to
  // update them before updating the layout attributes.
  if (needs_text_metrics_update_) {
    // Recompute the transform before updating font and corresponding
    // metrics. At this point our bounding box may be incorrect, so
    // any box relative transforms will be incorrect. Since the scaled
    // font size only needs the scaling components to be correct, this
    // should be fine. We update the transform again after computing
    // the bounding box below, and after that we clear the
    // |needs_transform_update_| flag.
    if (needs_transform_update_) {
      local_transform_ =
          GetElement()->CalculateTransform(SVGElement::kIncludeMotionTransform);
    }

    UpdateFont();
    needs_text_metrics_update_ = false;
  }

  UpdateNGBlockLayout();

  // TODO(crbug.com/1179585): Pass |bounds_changed|, and check the return value
  // of the function.
  UpdateTransformAfterLayout(true);
}

}  // namespace blink
