// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/layout_multi_column_spanner_placeholder.h"

#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/layout/layout_multi_column_flow_thread.h"

namespace blink {

static void CopyMarginProperties(
    ComputedStyleBuilder& placeholder_style_builder,
    const ComputedStyle& spanner_style) {
  // We really only need the block direction margins, but there are no setters
  // for that in ComputedStyle. Just copy all margin sides. The inline ones
  // don't matter anyway.
  placeholder_style_builder.SetMarginLeft(spanner_style.MarginLeft());
  placeholder_style_builder.SetMarginRight(spanner_style.MarginRight());
  placeholder_style_builder.SetMarginTop(spanner_style.MarginTop());
  placeholder_style_builder.SetMarginBottom(spanner_style.MarginBottom());
}

LayoutMultiColumnSpannerPlaceholder*
LayoutMultiColumnSpannerPlaceholder::CreateAnonymous(
    const ComputedStyle& parent_style,
    LayoutBox& layout_object_in_flow_thread) {
  LayoutMultiColumnSpannerPlaceholder* new_spanner =
      MakeGarbageCollected<LayoutMultiColumnSpannerPlaceholder>(
          &layout_object_in_flow_thread);
  Document& document = layout_object_in_flow_thread.GetDocument();
  new_spanner->SetDocumentForAnonymous(&document);
  new_spanner->UpdateProperties(parent_style);
  return new_spanner;
}

LayoutMultiColumnSpannerPlaceholder::LayoutMultiColumnSpannerPlaceholder(
    LayoutBox* layout_object_in_flow_thread)
    : LayoutBox(nullptr),
      layout_object_in_flow_thread_(layout_object_in_flow_thread) {}

void LayoutMultiColumnSpannerPlaceholder::Trace(Visitor* visitor) const {
  visitor->Trace(layout_object_in_flow_thread_);
  LayoutBox::Trace(visitor);
}

void LayoutMultiColumnSpannerPlaceholder::
    LayoutObjectInFlowThreadStyleDidChange(const ComputedStyle* old_style) {
  NOT_DESTROYED();
  LayoutBox* object_in_flow_thread = layout_object_in_flow_thread_;
  if (FlowThread()->RemoveSpannerPlaceholderIfNoLongerValid(
          object_in_flow_thread)) {
    // No longer a valid spanner, due to style changes. |this| is now dead.
    if (object_in_flow_thread->StyleRef().HasOutOfFlowPosition() &&
        !old_style->HasOutOfFlowPosition()) {
      // We went from being a spanner to being out-of-flow positioned. When an
      // object becomes out-of-flow positioned, we need to lay out its parent,
      // since that's where the now-out-of-flow object gets added to the right
      // containing block for out-of-flow positioned objects. Since neither a
      // spanner nor an out-of-flow object is guaranteed to have this parent in
      // its containing block chain, we need to mark it here, or we risk that
      // the object isn't laid out.
      object_in_flow_thread->Parent()->SetNeedsLayout(
          layout_invalidation_reason::kColumnsChanged);
    }
    return;
  }
  UpdateProperties(Parent()->StyleRef());
}

void LayoutMultiColumnSpannerPlaceholder::UpdateProperties(
    const ComputedStyle& parent_style) {
  NOT_DESTROYED();
  ComputedStyleBuilder new_style_builder =
      GetDocument().GetStyleResolver().CreateAnonymousStyleBuilderWithDisplay(
          parent_style, EDisplay::kBlock);
  CopyMarginProperties(new_style_builder,
                       layout_object_in_flow_thread_->StyleRef());
  SetStyle(new_style_builder.TakeStyle());
}

void LayoutMultiColumnSpannerPlaceholder::InsertedIntoTree() {
  NOT_DESTROYED();
  LayoutBox::InsertedIntoTree();
  // The object may previously have been laid out as a non-spanner, but since
  // it's a spanner now, it needs to be relaid out.
  layout_object_in_flow_thread_->SetNeedsLayoutAndIntrinsicWidthsRecalc(
      layout_invalidation_reason::kColumnsChanged);
}

void LayoutMultiColumnSpannerPlaceholder::WillBeRemovedFromTree() {
  NOT_DESTROYED();
  if (layout_object_in_flow_thread_) {
    LayoutBox* ex_spanner = layout_object_in_flow_thread_;
    layout_object_in_flow_thread_->ClearSpannerPlaceholder();
    // Even if the placeholder is going away, the object in the flow thread
    // might live on. Since it's not a spanner anymore, it needs to be relaid
    // out.
    ex_spanner->SetNeedsLayoutAndIntrinsicWidthsRecalc(
        layout_invalidation_reason::kColumnsChanged);
  }
  LayoutBox::WillBeRemovedFromTree();
}

LayoutPoint LayoutMultiColumnSpannerPlaceholder::LocationInternal() const {
  NOT_DESTROYED();
  return layout_object_in_flow_thread_->LocationInternal();
}

PhysicalSize LayoutMultiColumnSpannerPlaceholder::Size() const {
  NOT_DESTROYED();
  return layout_object_in_flow_thread_->Size();
}

}  // namespace blink
