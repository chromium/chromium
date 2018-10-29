// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"

#include "third_party/blink/renderer/core/editing/position_with_affinity.h"
#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/core/layout/layout_inline.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/layout_object_inlines.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_fragment_traversal.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_item.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_physical_line_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_box_fragment_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_outline_utils.h"

namespace blink {

scoped_refptr<const NGPhysicalBoxFragment> NGPhysicalBoxFragment::Create(
    NGBoxFragmentBuilder* builder,
    WritingMode block_or_line_writing_mode) {
  // We store the children list inline in the fragment as a flexible
  // array. Therefore, we need to make sure to allocate enough space for
  // that array here, which requires a manual allocation + placement new.
  // The initialization of the array is done by NGPhysicalContainerFragment;
  // we pass the buffer as a constructor argument.
  void* data = ::WTF::Partitions::FastMalloc(
      sizeof(NGPhysicalBoxFragment) +
          builder->children_.size() * sizeof(NGLinkStorage),
      ::WTF::GetStringWithTypeName<NGPhysicalBoxFragment>());
  new (data) NGPhysicalBoxFragment(builder, block_or_line_writing_mode);
  return base::AdoptRef(static_cast<NGPhysicalBoxFragment*>(data));
}

NGPhysicalBoxFragment::NGPhysicalBoxFragment(
    NGBoxFragmentBuilder* builder,
    WritingMode block_or_line_writing_mode)
    : NGPhysicalContainerFragment(
          builder,
          block_or_line_writing_mode,
          children_,
          (builder->node_ && builder->node_.IsRenderedLegend())
              ? kFragmentRenderedLegend
              : kFragmentBox,
          builder->BoxType()),
      baselines_(std::move(builder->baselines_)),
      borders_(builder->borders_.ConvertToPhysical(builder->GetWritingMode(),
                                                   builder->Direction())),
      padding_(builder->padding_.ConvertToPhysical(builder->GetWritingMode(),
                                                   builder->Direction())) {
  DCHECK(builder->baselines_.IsEmpty());  // Ensure move semantics is used.
  is_fieldset_container_ = builder->is_fieldset_container_;
  is_old_layout_root_ = builder->is_old_layout_root_;
  border_edge_ = builder->border_edges_.ToPhysical(builder->GetWritingMode());
  children_inline_ =
      builder->layout_object_ && builder->layout_object_->ChildrenInline();
}

const NGBaseline* NGPhysicalBoxFragment::Baseline(
    const NGBaselineRequest& request) const {
  for (const auto& baseline : baselines_) {
    if (baseline.request == request)
      return &baseline;
  }
  return nullptr;
}

bool NGPhysicalBoxFragment::HasSelfPaintingLayer() const {
  const LayoutObject* layout_object = GetLayoutObject();
  DCHECK(layout_object);
  DCHECK(layout_object->IsBoxModelObject());
  return ToLayoutBoxModelObject(layout_object)->HasSelfPaintingLayer();
}

bool NGPhysicalBoxFragment::HasOverflowClip() const {
  const LayoutObject* layout_object = GetLayoutObject();
  DCHECK(layout_object);
  return layout_object->HasOverflowClip();
}

bool NGPhysicalBoxFragment::ShouldClipOverflow() const {
  const LayoutObject* layout_object = GetLayoutObject();
  DCHECK(layout_object);
  return layout_object->IsBox() &&
         ToLayoutBox(layout_object)->ShouldClipOverflow();
}

bool NGPhysicalBoxFragment::HasControlClip() const {
  const LayoutObject* layout_object = GetLayoutObject();
  DCHECK(layout_object);
  return layout_object->IsBox() && ToLayoutBox(layout_object)->HasControlClip();
}

LayoutRect NGPhysicalBoxFragment::OverflowClipRect(
    const LayoutPoint& location,
    OverlayScrollbarClipBehavior overlay_scrollbar_clip_behavior) const {
  DCHECK(GetLayoutObject() && GetLayoutObject()->IsBox());
  const LayoutBox* box = ToLayoutBox(GetLayoutObject());
  return box->OverflowClipRect(location, overlay_scrollbar_clip_behavior);
}

NGPhysicalOffsetRect NGPhysicalBoxFragment::ScrollableOverflow() const {
  DCHECK(GetLayoutObject());
  LayoutObject* layout_object = GetLayoutObject();
  if (layout_object->IsBox()) {
    if (HasOverflowClip())
      return NGPhysicalOffsetRect({}, Size());
    // Legacy is the source of truth for overflow
    return NGPhysicalOffsetRect(
        ToLayoutBox(layout_object)->LayoutOverflowRect());
  } else if (layout_object->IsLayoutInline()) {
    // Inline overflow is a union of child overflows.
    NGPhysicalOffsetRect overflow({}, Size());
    for (const auto& child_fragment : Children()) {
      NGPhysicalOffsetRect child_overflow =
          child_fragment->ScrollableOverflow();
      child_overflow.offset += child_fragment.Offset();
      overflow.Unite(child_overflow);
    }
    return overflow;
  } else {
    NOTREACHED();
  }
  return NGPhysicalOffsetRect({}, Size());
}

IntSize NGPhysicalBoxFragment::ScrolledContentOffset() const {
  DCHECK(GetLayoutObject() && GetLayoutObject()->IsBox());
  const LayoutBox* box = ToLayoutBox(GetLayoutObject());
  return box->ScrolledContentOffset();
}

LayoutSize NGPhysicalBoxFragment::ScrollSize() const {
  DCHECK(GetLayoutObject() && GetLayoutObject()->IsBox());
  const LayoutBox* box = ToLayoutBox(GetLayoutObject());
  return LayoutSize(box->ScrollWidth(), box->ScrollHeight());
}

NGPhysicalOffsetRect NGPhysicalBoxFragment::SelfInkOverflow() const {
  const ComputedStyle& style = Style();
  LayoutRect ink_overflow({}, Size().ToLayoutSize());

  DCHECK(GetLayoutObject());
  if (style.HasVisualOverflowingEffect()) {
    ink_overflow.Expand(style.BoxDecorationOutsets());
    if (NGOutlineUtils::HasPaintedOutline(style,
                                          GetLayoutObject()->GetNode()) &&
        !NGOutlineUtils::IsInlineOutlineNonpaintingFragment(*this)) {
      Vector<LayoutRect> outline_rects;
      // The result rects are in coordinates of this object's border box.
      AddSelfOutlineRects(
          &outline_rects, LayoutPoint(),
          GetLayoutObject()->OutlineRectsShouldIncludeBlockVisualOverflow());
      LayoutRect rect = UnionRectEvenIfEmpty(outline_rects);
      rect.Inflate(style.OutlineOutsetExtent());
      ink_overflow.Unite(rect);
    }
  }
  ink_overflow.Unite(descendant_outlines_.ToLayoutRect());
  return NGPhysicalOffsetRect(ink_overflow);
}

void NGPhysicalBoxFragment::AddSelfOutlineRects(
    Vector<LayoutRect>* outline_rects,
    const LayoutPoint& additional_offset,
    NGOutlineType outline_type) const {
  // TODO(kojii): Needs inline_element_continuation logic from
  // LayoutBlockFlow::AddOutlineRects?

  const LayoutObject* layout_object = GetLayoutObject();
  DCHECK(layout_object);
  if (layout_object->IsLayoutInline()) {
    Vector<LayoutRect> blockflow_outline_rects;
    ToLayoutInline(layout_object)
        ->AddOutlineRects(blockflow_outline_rects, LayoutPoint(), outline_type);
    // The rectangles returned are offset from the containing block. We need the
    // offset from this fragment. Additionally, the rectangles are offset
    // relatively to the block-start of the container (this matters when
    // writing-mode is vertical-rl). We want them to be purely physical. Apply
    // correction.
    if (blockflow_outline_rects.size() > 0) {
      const LayoutBlock* block_for_flipping = nullptr;
      LayoutPoint first_fragment_offset = blockflow_outline_rects[0].Location();
      if (UNLIKELY(layout_object->HasFlippedBlocksWritingMode())) {
        block_for_flipping = layout_object->ContainingBlock();
        first_fragment_offset.SetX(block_for_flipping->FlipForWritingMode(
            blockflow_outline_rects[0].MaxX()));
      }
      LayoutSize corrected_offset = additional_offset - first_fragment_offset;
      for (auto& outline : blockflow_outline_rects) {
        if (UNLIKELY(block_for_flipping))
          block_for_flipping->FlipForWritingMode(outline);
        outline.Move(corrected_offset);
        outline_rects->push_back(outline);
      }
    }
    return;
  }
  DCHECK(layout_object->IsBox());

  // For anonymous blocks, the children add outline rects.
  if (!layout_object->IsAnonymous()) {
    outline_rects->emplace_back(additional_offset, Size().ToLayoutSize());
  }

  if (outline_type == NGOutlineType::kIncludeBlockVisualOverflow &&
      !HasOverflowClip() && !HasControlClip()) {
    AddOutlineRectsForNormalChildren(outline_rects, additional_offset,
                                     outline_type);

    // TODO(kojii): LayoutBlock::AddOutlineRects handles positioned objects
    // here. Do we need it?
  }

  // TODO(kojii): Needs inline_element_continuation logic from
  // LayoutBlockFlow::AddOutlineRects?
}

NGPhysicalOffsetRect NGPhysicalBoxFragment::InkOverflow(bool apply_clip) const {
  if ((apply_clip && HasOverflowClip()) || Style().HasMask())
    return SelfInkOverflow();

  NGPhysicalOffsetRect ink_overflow = SelfInkOverflow();
  ink_overflow.Unite(ContentsInkOverflow());
  return ink_overflow;
}

NGPhysicalOffsetRect NGPhysicalBoxFragment::ContentsInkOverflow() const {
  if (LayoutBox* layout_box = ToLayoutBoxOrNull(GetLayoutObject())) {
    return NGPhysicalOffsetRect(layout_box->ContentsVisualOverflowRect());
  }
  return ComputeContentsInkOverflow();
}

NGPhysicalOffsetRect NGPhysicalBoxFragment::ComputeContentsInkOverflow() const {
  NGPhysicalOffsetRect overflow({}, Size());
  for (const auto& child : Children()) {
    child->PropagateContentsInkOverflow(&overflow, child.Offset());
  }
  return overflow;
}

UBiDiLevel NGPhysicalBoxFragment::BidiLevel() const {
  // TODO(xiaochengh): Make the implementation more efficient.
  DCHECK(IsInline());
  DCHECK(IsAtomicInline());
  const auto& inline_items = InlineItemsOfContainingBlock();
  const NGInlineItem* self_item =
      std::find_if(inline_items.begin(), inline_items.end(),
                   [this](const NGInlineItem& item) {
                     return GetLayoutObject() == item.GetLayoutObject();
                   });
  DCHECK(self_item);
  DCHECK_NE(self_item, inline_items.end());
  return self_item->BidiLevel();
}

}  // namespace blink
