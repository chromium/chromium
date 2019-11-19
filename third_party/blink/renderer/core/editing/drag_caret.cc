/*
 * Copyright (C) 2004, 2008, 2009, 2010 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
* CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/editing/drag_caret.h"

#include "third_party/blink/renderer/core/editing/caret_display_item_client.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/visible_position.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"

namespace blink {

DragCaret::DragCaret() : display_item_client_(new CaretDisplayItemClient()) {}

DragCaret::~DragCaret() = default;

void DragCaret::ClearPreviousVisualRect(const LayoutBlock& block) {
  display_item_client_->ClearPreviousVisualRect(block);
}

void DragCaret::LayoutBlockWillBeDestroyed(const LayoutBlock& block) {
  display_item_client_->LayoutBlockWillBeDestroyed(block);
}

void DragCaret::UpdateStyleAndLayoutIfNeeded() {
  display_item_client_->UpdateStyleAndLayoutIfNeeded(
      RootEditableElementOf(position_.GetPosition()) ? position_
                                                     : PositionWithAffinity());
}

void DragCaret::InvalidatePaint(const LayoutBlock& block,
                                const PaintInvalidatorContext& context) {
  display_item_client_->InvalidatePaint(block, context);
}

bool DragCaret::IsContentRichlyEditable() const {
  return IsRichlyEditablePosition(position_.GetPosition());
}

void DragCaret::SetCaretPosition(const PositionWithAffinity& position) {
  position_ = position;
  Document* document = nullptr;
  if (Node* node = position_.AnchorNode()) {
    document = &node->GetDocument();
    SetContext(document);
  }
}

void DragCaret::NodeChildrenWillBeRemoved(ContainerNode& container) {
  if (!HasCaret() || !container.InActiveDocument())
    return;
  Node* const anchor_node = position_.GetPosition().AnchorNode();
  if (!anchor_node || anchor_node == container)
    return;
  if (!container.IsShadowIncludingInclusiveAncestorOf(*anchor_node))
    return;
  Clear();
}

void DragCaret::NodeWillBeRemoved(Node& node) {
  if (!HasCaret() || !node.InActiveDocument())
    return;
  Node* const anchor_node = position_.GetPosition().AnchorNode();
  if (!anchor_node)
    return;
  if (!node.IsShadowIncludingInclusiveAncestorOf(*anchor_node))
    return;
  Clear();
}

void DragCaret::Trace(Visitor* visitor) {
  visitor->Trace(position_);
  SynchronousMutationObserver::Trace(visitor);
}

bool DragCaret::ShouldPaintCaret(const LayoutBlock& block) const {
  return display_item_client_->ShouldPaintCaret(block);
}

void DragCaret::PaintDragCaret(const LocalFrame* frame,
                               GraphicsContext& context,
                               const PhysicalOffset& paint_offset) const {
  if (position_.AnchorNode()->GetDocument().GetFrame() != frame)
    return;
  display_item_client_->PaintCaret(context, paint_offset,
                                   DisplayItem::kDragCaret);
}

}  // namespace blink
