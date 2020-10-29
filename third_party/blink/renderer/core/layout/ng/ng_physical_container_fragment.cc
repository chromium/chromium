// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_physical_container_fragment.h"

#include "third_party/blink/renderer/core/layout/geometry/writing_mode_converter.h"
#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/core/layout/layout_inline.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_cursor.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_physical_line_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_ruby_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_container_fragment_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_overflow_calculator.h"
#include "third_party/blink/renderer/core/layout/ng/ng_outline_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_relative_utils.h"
#include "third_party/blink/renderer/platform/geometry/layout_rect.h"
#include "third_party/blink/renderer/platform/wtf/size_assertions.h"

namespace blink {

namespace {

struct SameSizeAsNGPhysicalContainerFragment : NGPhysicalFragment {
  wtf_size_t size;
  void* break_token;
  std::unique_ptr<Vector<NGPhysicalOutOfFlowPositionedNode>>
      oof_positioned_descendants_;
  void* pointer;
};

ASSERT_SIZE(NGPhysicalContainerFragment, SameSizeAsNGPhysicalContainerFragment);

}  // namespace

NGPhysicalContainerFragment::NGPhysicalContainerFragment(
    NGContainerFragmentBuilder* builder,
    WritingMode block_or_line_writing_mode,
    NGLink* buffer,
    NGFragmentType type,
    unsigned sub_type)
    : NGPhysicalFragment(builder, type, sub_type),
      num_children_(builder->children_.size()),
      break_token_(std::move(builder->break_token_)),
      oof_positioned_descendants_(
          builder->oof_positioned_descendants_.IsEmpty()
              ? nullptr
              : new Vector<NGPhysicalOutOfFlowPositionedNode>()),
      buffer_(buffer) {
  has_floating_descendants_for_paint_ =
      builder->has_floating_descendants_for_paint_;
  has_adjoining_object_descendants_ =
      builder->has_adjoining_object_descendants_;
  depends_on_percentage_block_size_ = DependsOnPercentageBlockSize(*builder);

  PhysicalSize size = Size();
  if (oof_positioned_descendants_) {
    oof_positioned_descendants_->ReserveCapacity(
        builder->oof_positioned_descendants_.size());
    const WritingModeConverter converter(
        {builder->Style().GetWritingMode(), builder->Direction()}, size);
    for (const auto& descendant : builder->oof_positioned_descendants_) {
      oof_positioned_descendants_->emplace_back(
          descendant.node,
          descendant.static_position.ConvertToPhysical(converter),
          descendant.inline_container);
    }
  }

  // Because flexible arrays need to be the last member in a class, we need to
  // have the buffer passed as a constructor argument and have the actual
  // storage be part of the subclass.
  const WritingModeConverter converter(
      {block_or_line_writing_mode, builder->Direction()}, size);
  wtf_size_t i = 0;
  for (auto& child : builder->children_) {
    buffer[i].offset =
        converter.ToPhysical(child.offset, child.fragment->Size());
    // Call the move constructor to move without |AddRef|. Fragments in
    // |builder| are not used after |this| was constructed.
    static_assert(
        sizeof(buffer[0].fragment) ==
            sizeof(scoped_refptr<const NGPhysicalFragment>),
        "scoped_refptr must be the size of a pointer for this to work");
    new (&buffer[i].fragment)
        scoped_refptr<const NGPhysicalFragment>(std::move(child.fragment));
    DCHECK(!child.fragment);  // Ensure it was moved.
    ++i;
  }
}

NGPhysicalContainerFragment::NGPhysicalContainerFragment(
    const NGPhysicalContainerFragment& other,
    bool recalculate_layout_overflow,
    NGLink* buffer)
    : NGPhysicalFragment(other),
      num_children_(other.num_children_),
      break_token_(other.break_token_),
      oof_positioned_descendants_(
          other.oof_positioned_descendants_
              ? new Vector<NGPhysicalOutOfFlowPositionedNode>(
                    *other.oof_positioned_descendants_)
              : nullptr),
      buffer_(buffer) {
  // To ensure the fragment tree is consistent, use the post-layout fragment.
  for (wtf_size_t i = 0; i < num_children_; ++i) {
    buffer[i].offset = other.buffer_[i].offset;
    scoped_refptr<const NGPhysicalFragment> post_layout =
        other.buffer_[i]->PostLayout();
    // While making the fragment tree consistent, we need to also clone any
    // fragmentainer fragments, as they don't nessecerily have their result
    // stored on the layout-object tree.
    if (post_layout->IsFragmentainerBox()) {
      const auto& box_fragment = To<NGPhysicalBoxFragment>(*post_layout);

      base::Optional<PhysicalRect> layout_overflow;
      if (recalculate_layout_overflow) {
        layout_overflow =
            NGLayoutOverflowCalculator::RecalculateLayoutOverflowForFragment(
                box_fragment);
      }

      post_layout = NGPhysicalBoxFragment::CloneWithPostLayoutFragments(
          box_fragment, layout_overflow);
    }
    new (&buffer[i].fragment)
        scoped_refptr<const NGPhysicalFragment>(std::move(post_layout));
  }
}

NGPhysicalContainerFragment::~NGPhysicalContainerFragment() = default;

// additional_offset must be offset from the containing_block.
void NGPhysicalContainerFragment::AddOutlineRectsForNormalChildren(
    Vector<PhysicalRect>* outline_rects,
    const PhysicalOffset& additional_offset,
    NGOutlineType outline_type,
    const LayoutBoxModelObject* containing_block) const {
  if (const auto* box = DynamicTo<NGPhysicalBoxFragment>(this)) {
    DCHECK_EQ(box->PostLayout(), box);
    if (const NGFragmentItems* items = box->Items()) {
      for (NGInlineCursor cursor(*box, *items); cursor; cursor.MoveToNext()) {
        DCHECK(cursor.Current().Item());
        const NGFragmentItem& item = *cursor.Current().Item();
        if (UNLIKELY(item.IsLayoutObjectDestroyedOrMoved()))
          continue;
        if (item.Type() == NGFragmentItem::kLine) {
          AddOutlineRectsForDescendant(
              {item.LineBoxFragment(), item.OffsetInContainerBlock()},
              outline_rects, additional_offset, outline_type, containing_block);
          continue;
        }
        if (item.IsText()) {
          if (outline_type == NGOutlineType::kDontIncludeBlockVisualOverflow)
            continue;
          outline_rects->push_back(
              PhysicalRect(additional_offset + item.OffsetInContainerBlock(),
                           item.Size().ToLayoutSize()));
          continue;
        }
        if (item.Type() == NGFragmentItem::kBox) {
          if (const NGPhysicalBoxFragment* child_box =
                  item.PostLayoutBoxFragment()) {
            DCHECK(!child_box->IsOutOfFlowPositioned());
            AddOutlineRectsForDescendant(
                {child_box, item.OffsetInContainerBlock()}, outline_rects,
                additional_offset, outline_type, containing_block);
          }
          continue;
        }
      }
      // Don't add |Children()|. If |this| has |NGFragmentItems|, children are
      // either line box, which we already handled in items, or OOF, which we
      // should ignore.
      DCHECK(std::all_of(PostLayoutChildren().begin(),
                         PostLayoutChildren().end(), [](const NGLink& child) {
                           return child->IsLineBox() ||
                                  child->IsOutOfFlowPositioned();
                         }));
      return;
    }
  }

  for (const auto& child : PostLayoutChildren()) {
    // Outlines of out-of-flow positioned descendants are handled in
    // NGPhysicalBoxFragment::AddSelfOutlineRects().
    if (child->IsOutOfFlowPositioned())
      continue;

    // Outline of an element continuation or anonymous block continuation is
    // added when we iterate the continuation chain.
    // See NGPhysicalBoxFragment::AddSelfOutlineRects().
    if (!child->IsLineBox()) {
      const LayoutObject* child_layout_object = child->GetLayoutObject();
      if (auto* child_layout_block_flow =
              DynamicTo<LayoutBlockFlow>(child_layout_object)) {
        if (child_layout_object->IsElementContinuation() ||
            child_layout_block_flow->IsAnonymousBlockContinuation())
          continue;
      }
    }
    AddOutlineRectsForDescendant(child, outline_rects, additional_offset,
                                 outline_type, containing_block);
  }
}

void NGPhysicalContainerFragment::AddScrollableOverflowForInlineChild(
    const NGPhysicalBoxFragment& container,
    const ComputedStyle& container_style,
    const NGFragmentItem& line,
    bool has_hanging,
    const NGInlineCursor& cursor,
    TextHeightType height_type,
    PhysicalRect* overflow) const {
  DCHECK(RuntimeEnabledFeatures::LayoutNGFragmentItemEnabled());
  DCHECK(IsLineBox() || IsInlineBox());
  DCHECK(cursor.Current().Item() &&
         (cursor.Current().Item()->BoxFragment() == this ||
          cursor.Current().Item()->LineBoxFragment() == this));
  const WritingMode container_writing_mode = container_style.GetWritingMode();
  for (NGInlineCursor descendants = cursor.CursorForDescendants();
       descendants;) {
    const NGFragmentItem* item = descendants.CurrentItem();
    DCHECK(item);
    if (UNLIKELY(item->IsLayoutObjectDestroyedOrMoved())) {
      NOTREACHED();
      descendants.MoveToNextSkippingChildren();
      continue;
    }
    if (item->IsText()) {
      PhysicalRect child_scroll_overflow = item->RectInContainerBlock();
      if (height_type == TextHeightType::kEmHeight) {
        child_scroll_overflow = AdjustTextRectForEmHeight(
            child_scroll_overflow, item->Style(), item->TextShapeResult(),
            container_writing_mode);
      }
      if (UNLIKELY(has_hanging)) {
        AdjustScrollableOverflowForHanging(line.RectInContainerBlock(),
                                           container_writing_mode,
                                           &child_scroll_overflow);
      }
      overflow->Unite(child_scroll_overflow);
      descendants.MoveToNextSkippingChildren();
      continue;
    }

    if (const NGPhysicalBoxFragment* child_box =
            item->PostLayoutBoxFragment()) {
      PhysicalRect child_scroll_overflow;
      if (height_type == TextHeightType::kNormalHeight ||
          (child_box->BoxType() != kInlineBox && !IsRubyBox()))
        child_scroll_overflow = item->RectInContainerBlock();
      if (child_box->IsInlineBox()) {
        child_box->AddScrollableOverflowForInlineChild(
            container, container_style, line, has_hanging, descendants,
            height_type, &child_scroll_overflow);
        child_box->AdjustScrollableOverflowForPropagation(
            container, height_type, &child_scroll_overflow);
        if (UNLIKELY(has_hanging)) {
          AdjustScrollableOverflowForHanging(line.RectInContainerBlock(),
                                             container_writing_mode,
                                             &child_scroll_overflow);
        }
      } else {
        child_scroll_overflow =
            child_box->ScrollableOverflowForPropagation(container, height_type);
        child_scroll_overflow.offset += item->OffsetInContainerBlock();
      }
      overflow->Unite(child_scroll_overflow);
      descendants.MoveToNextSkippingChildren();
      continue;
    }

    // Add all children of a culled inline box; i.e., an inline box without
    // margin/border/padding etc.
    DCHECK_EQ(item->Type(), NGFragmentItem::kBox);
    descendants.MoveToNext();
  }
}

// Chop the hanging part from scrollable overflow. Children overflow in inline
// direction should hang, which should not cause scroll.
// TODO(kojii): Should move to text fragment to make this more accurate.
void NGPhysicalContainerFragment::AdjustScrollableOverflowForHanging(
    const PhysicalRect& rect,
    const WritingMode container_writing_mode,
    PhysicalRect* overflow) {
  if (IsHorizontalWritingMode(container_writing_mode)) {
    if (overflow->offset.left < rect.offset.left)
      overflow->offset.left = rect.offset.left;
    if (overflow->Right() > rect.Right())
      overflow->ShiftRightEdgeTo(rect.Right());
  } else {
    if (overflow->offset.top < rect.offset.top)
      overflow->offset.top = rect.offset.top;
    if (overflow->Bottom() > rect.Bottom())
      overflow->ShiftBottomEdgeTo(rect.Bottom());
  }
}

// additional_offset must be offset from the containing_block because
// LocalToAncestorRect returns rects wrt containing_block.
void NGPhysicalContainerFragment::AddOutlineRectsForDescendant(
    const NGLink& descendant,
    Vector<PhysicalRect>* outline_rects,
    const PhysicalOffset& additional_offset,
    NGOutlineType outline_type,
    const LayoutBoxModelObject* containing_block) const {
  DCHECK(!descendant->IsLayoutObjectDestroyedOrMoved());
  if (descendant->IsText() || descendant->IsListMarker())
    return;

  if (const auto* descendant_box =
          DynamicTo<NGPhysicalBoxFragment>(descendant.get())) {
    DCHECK_EQ(descendant_box->PostLayout(), descendant_box);
    const LayoutObject* descendant_layout_object =
        descendant_box->GetLayoutObject();

    // TODO(layoutng): Explain this check. I assume we need it because layers
    // may have transforms and so we have to go through LocalToAncestorRects?
    if (descendant_box->HasLayer()) {
      DCHECK(descendant_layout_object);
      Vector<PhysicalRect> layer_outline_rects;
      descendant_box->AddSelfOutlineRects(PhysicalOffset(), outline_type,
                                          &layer_outline_rects);

      // Don't pass additional_offset because LocalToAncestorRects will itself
      // apply it.
      descendant_layout_object->LocalToAncestorRects(
          layer_outline_rects, containing_block, PhysicalOffset(),
          PhysicalOffset());
      outline_rects->AppendVector(layer_outline_rects);
      return;
    }

    if (!descendant_box->IsInlineBox()) {
      descendant_box->AddSelfOutlineRects(
          additional_offset + descendant.Offset(), outline_type, outline_rects);
      return;
    }

    DCHECK(descendant_layout_object);
    DCHECK(descendant_layout_object->IsLayoutInline());
    const LayoutInline* descendant_layout_inline =
        ToLayoutInline(descendant_layout_object);
    // As an optimization, an ancestor has added rects for its line boxes
    // covering descendants' line boxes, so descendants don't need to add line
    // boxes again. For example, if the parent is a LayoutBlock, it adds rects
    // for its line box which cover the line boxes of this LayoutInline. So
    // the LayoutInline needs to add rects for children and continuations
    // only.
    if (NGOutlineUtils::ShouldPaintOutline(*descendant_box)) {
      // We don't pass additional_offset here because the function requires
      // additional_offset to be the offset from the containing block.
      descendant_layout_inline->AddOutlineRectsForChildrenAndContinuations(
          *outline_rects, PhysicalOffset(), outline_type);
    }
    return;
  }

  if (const auto* descendant_line_box =
          DynamicTo<NGPhysicalLineBoxFragment>(descendant.get())) {
    descendant_line_box->AddOutlineRectsForNormalChildren(
        outline_rects, additional_offset + descendant.Offset(), outline_type,
        containing_block);

    if (!descendant_line_box->Size().IsEmpty()) {
      outline_rects->emplace_back(additional_offset + descendant.Offset(),
                                  descendant_line_box->Size().ToLayoutSize());
    } else if (descendant_line_box->Children().empty()) {
      // Special-case for when the first continuation does not generate
      // fragments. NGInlineLayoutAlgorithm suppresses box fragments when the
      // line is "empty". When there is a continuation from the LayoutInline,
      // the suppression makes such continuation not reachable. Check the
      // continuation from LayoutInline in such case.
      DCHECK(GetLayoutObject());
      if (LayoutInline* first_layout_inline =
              ToLayoutInlineOrNull(GetLayoutObject()->SlowFirstChild())) {
        if (!first_layout_inline->IsElementContinuation()) {
          first_layout_inline->AddOutlineRectsForChildrenAndContinuations(
              *outline_rects, additional_offset, outline_type);
        }
      }
    }
  }
}

bool NGPhysicalContainerFragment::DependsOnPercentageBlockSize(
    const NGContainerFragmentBuilder& builder) {
  NGLayoutInputNode node = builder.node_;

  if (!node || node.IsInline())
    return builder.has_descendant_that_depends_on_percentage_block_size_;

  // For the below if-stmt we only want to consider legacy *containers* as
  // potentially having %-dependent children - i.e. an image doesn't have any
  // children.
  bool is_legacy_container_with_percent_height_descendants =
      builder.is_legacy_layout_root_ && !node.IsReplaced() &&
      node.GetLayoutBox()->MaybeHasPercentHeightDescendant();

  // NOTE: If an element is OOF positioned, and has top/bottom constraints
  // which are percentage based, this function will return false.
  //
  // This is fine as the top/bottom constraints are computed *before* layout,
  // and the result is set as a fixed-block-size constraint. (And the caching
  // logic will never check the result of this function).
  //
  // The result of this function still may be used for an OOF positioned
  // element if it has a percentage block-size however, but this will return
  // the correct result from below.

  // There are two conditions where we need to know about an (arbitrary)
  // descendant which depends on a %-block-size.
  //  - In quirks mode, the arbitrary descendant may depend the percentage
  //    resolution block-size given (to this node), and need to relayout if
  //    this size changes.
  //  - A flex-item may have its "definiteness" change, (e.g. if itself is a
  //    flex item which is being stretched). This definiteness change will
  //    affect any %-block-size children.
  //
  // NOTE(ikilpatrick): For the flex-item case this is potentially too general.
  // We only need to know about if this flex-item has a %-block-size child if
  // the "definiteness" changes, not if the percentage resolution size changes.
  if ((builder.has_descendant_that_depends_on_percentage_block_size_ ||
       is_legacy_container_with_percent_height_descendants) &&
      (node.UseParentPercentageResolutionBlockSizeForChildren() ||
       node.IsFlexItem()))
    return true;

  const ComputedStyle& style = builder.Style();
  if (style.LogicalHeight().IsPercentOrCalc() ||
      style.LogicalMinHeight().IsPercentOrCalc() ||
      style.LogicalMaxHeight().IsPercentOrCalc())
    return true;

  return false;
}

}  // namespace blink
