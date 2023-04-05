// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/scroll_timeline_attachment.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"

namespace blink {

ScrollTimelineAttachment::ScrollTimelineAttachment(ReferenceType reference_type,
                                                   Element* reference_element,
                                                   ScrollAxis axis)
    : reference_type_(reference_type),
      reference_element_(reference_element),
      axis_(axis) {}

Element* ScrollTimelineAttachment::ComputeSource() const {
  if (reference_type_ == ReferenceType::kNearestAncestor &&
      reference_element_) {
    reference_element_->GetDocument().UpdateStyleAndLayout(
        DocumentUpdateReason::kJavaScript);
  }
  return ComputeSourceNoLayout();
}

Element* ScrollTimelineAttachment::ComputeSourceNoLayout() const {
  if (reference_type_ == ReferenceType::kSource) {
    return reference_element_.Get();
  }
  DCHECK_EQ(ReferenceType::kNearestAncestor, reference_type_);

  if (!reference_element_) {
    return nullptr;
  }

  LayoutBox* layout_box = reference_element_->GetLayoutBox();
  if (!layout_box) {
    return nullptr;
  }

  const LayoutBox* scroll_container = layout_box->ContainingScrollContainer();
  if (!scroll_container) {
    return reference_element_->GetDocument().ScrollingElementNoLayout();
  }

  Node* node = scroll_container->GetNode();
  if (node->IsElementNode()) {
    return DynamicTo<Element>(node);
  }
  if (node->IsDocumentNode()) {
    return DynamicTo<Document>(node)->ScrollingElementNoLayout();
  }

  NOTREACHED();
  return nullptr;
}

void ScrollTimelineAttachment::Trace(Visitor* visitor) const {
  visitor->Trace(reference_element_);
}

}  // namespace blink
