// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"

#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/position_with_affinity.h"
#include "third_party/blink/renderer/core/editing/text_affinity.h"
#include "third_party/blink/renderer/core/layout/geometry/writing_mode_converter.h"
#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/core/layout/layout_inline.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/layout_object_inlines.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_fragment_item.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_cursor.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_fragment_traversal.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_item.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_physical_line_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_box_fragment_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_outline_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_relative_utils.h"
#include "third_party/blink/renderer/platform/wtf/size_assertions.h"

namespace blink {

namespace {

struct SameSizeAsNGPhysicalBoxFragment : NGPhysicalContainerFragment {
  LayoutUnit baseline;
  LayoutUnit last_baseline;
  NGLink children[];
};

ASSERT_SIZE(NGPhysicalBoxFragment, SameSizeAsNGPhysicalBoxFragment);

bool HasControlClip(const NGPhysicalBoxFragment& self) {
  const LayoutBox* box = DynamicTo<LayoutBox>(self.GetLayoutObject());
  return box && box->HasControlClip();
}

inline bool IsHitTestCandidate(const NGPhysicalBoxFragment& fragment) {
  return fragment.Size().height &&
         fragment.Style().Visibility() == EVisibility::kVisible &&
         !fragment.IsFloatingOrOutOfFlowPositioned();
}

}  // namespace

// static
scoped_refptr<const NGPhysicalBoxFragment> NGPhysicalBoxFragment::Create(
    NGBoxFragmentBuilder* builder,
    WritingMode block_or_line_writing_mode) {
  const auto writing_direction = builder->GetWritingDirection();
  const NGPhysicalBoxStrut borders =
      builder->initial_fragment_geometry_->border.ConvertToPhysical(
          writing_direction);
  bool has_borders = !borders.IsZero();
  const NGPhysicalBoxStrut padding =
      builder->initial_fragment_geometry_->padding.ConvertToPhysical(
          writing_direction);
  bool has_padding = !padding.IsZero();

  const PhysicalSize physical_size =
      ToPhysicalSize(builder->Size(), builder->GetWritingMode());
  WritingModeConverter converter(writing_direction, physical_size);

  base::Optional<PhysicalRect> inflow_bounds;
  if (builder->inflow_bounds_)
    inflow_bounds = converter.ToPhysical(*builder->inflow_bounds_);

  PhysicalRect layout_overflow = {PhysicalOffset(), physical_size};
  if (builder->node_ && !builder->is_legacy_layout_root_) {
    const NGPhysicalBoxStrut scrollbar =
        builder->initial_fragment_geometry_->scrollbar.ConvertToPhysical(
            writing_direction);
    NGLayoutOverflowCalculator calculator(
        To<NGBlockNode>(builder->node_),
        /* is_css_box */ builder->box_type_ != NGBoxType::kColumnBox, borders,
        scrollbar, padding, physical_size, writing_direction);

    if (NGFragmentItemsBuilder* items_builder = builder->ItemsBuilder())
      calculator.AddItems(items_builder->Items(physical_size));

    for (auto& child : builder->children_) {
      const auto* box_fragment =
          DynamicTo<NGPhysicalBoxFragment>(*child.fragment);
      if (!box_fragment)
        continue;

      calculator.AddChild(*box_fragment, child.offset.ConvertToPhysical(
                                             writing_direction, physical_size,
                                             box_fragment->Size()));
    }

    layout_overflow = calculator.Result(inflow_bounds);
  }

  // For the purposes of object allocation we have layout-overflow if it
  // differs from the fragment size.
  bool has_layout_overflow = layout_overflow != PhysicalRect({}, physical_size);

  bool has_rare_data =
      builder->mathml_paint_info_ ||
      !builder->oof_positioned_fragmentainer_descendants_.IsEmpty() ||
      builder->table_grid_rect_ || builder->table_column_geometries_ ||
      builder->table_collapsed_borders_ ||
      builder->table_collapsed_borders_geometry_ ||
      builder->table_cell_column_index_;

  wtf_size_t num_fragment_items =
      builder->ItemsBuilder() ? builder->ItemsBuilder()->Size() : 0;
  size_t byte_size = ByteSize(num_fragment_items, builder->children_.size(),
                              has_layout_overflow, has_borders, has_padding,
                              inflow_bounds.has_value(), has_rare_data);

  // We store the children list inline in the fragment as a flexible
  // array. Therefore, we need to make sure to allocate enough space for
  // that array here, which requires a manual allocation + placement new.
  // The initialization of the array is done by NGPhysicalContainerFragment;
  // we pass the buffer as a constructor argument.
  void* data = ::WTF::Partitions::FastMalloc(
      byte_size, ::WTF::GetStringWithTypeName<NGPhysicalBoxFragment>());
  new (data) NGPhysicalBoxFragment(PassKey(), builder, has_layout_overflow,
                                   layout_overflow, has_borders, borders,
                                   has_padding, padding, inflow_bounds,
                                   has_rare_data, block_or_line_writing_mode);
  return base::AdoptRef(static_cast<NGPhysicalBoxFragment*>(data));
}

// static
scoped_refptr<const NGPhysicalBoxFragment>
NGPhysicalBoxFragment::CloneWithPostLayoutFragments(
    const NGPhysicalBoxFragment& other,
    const base::Optional<PhysicalRect> updated_layout_overflow) {
  PhysicalRect layout_overflow = other.LayoutOverflow();
  bool has_layout_overflow = other.has_layout_overflow_;

  if (updated_layout_overflow) {
    layout_overflow = *updated_layout_overflow;
    has_layout_overflow = layout_overflow != PhysicalRect({}, other.Size());
  }

  // The size of the new fragment shouldn't differ from the old one.
  wtf_size_t num_fragment_items = other.Items() ? other.Items()->Size() : 0;
  size_t byte_size =
      ByteSize(num_fragment_items, other.num_children_, has_layout_overflow,
               other.has_borders_, other.has_padding_, other.has_inflow_bounds_,
               other.has_rare_data_);

  void* data = ::WTF::Partitions::FastMalloc(
      byte_size, ::WTF::GetStringWithTypeName<NGPhysicalBoxFragment>());
  new (data) NGPhysicalBoxFragment(
      PassKey(), other, has_layout_overflow, layout_overflow,
      /* recalculate_layout_overflow */ updated_layout_overflow.has_value());
  return base::AdoptRef(static_cast<NGPhysicalBoxFragment*>(data));
}

// static
size_t NGPhysicalBoxFragment::ByteSize(wtf_size_t num_fragment_items,
                                       wtf_size_t num_children,
                                       bool has_layout_overflow,
                                       bool has_borders,
                                       bool has_padding,
                                       bool has_inflow_bounds,
                                       bool has_rare_data) {
  return sizeof(NGPhysicalBoxFragment) +
         NGFragmentItems::ByteSizeFor(num_fragment_items) +
         sizeof(NGLink) * num_children +
         (has_layout_overflow ? sizeof(PhysicalRect) : 0) +
         (has_borders ? sizeof(NGPhysicalBoxStrut) : 0) +
         (has_padding ? sizeof(NGPhysicalBoxStrut) : 0) +
         (has_inflow_bounds ? sizeof(PhysicalRect) : 0) +
         (has_rare_data ? sizeof(NGPhysicalBoxFragment::RareData) : 0);
}

NGPhysicalBoxFragment::NGPhysicalBoxFragment(
    PassKey key,
    NGBoxFragmentBuilder* builder,
    bool has_layout_overflow,
    const PhysicalRect& layout_overflow,
    bool has_borders,
    const NGPhysicalBoxStrut& borders,
    bool has_padding,
    const NGPhysicalBoxStrut& padding,
    const base::Optional<PhysicalRect>& inflow_bounds,
    bool has_rare_data,
    WritingMode block_or_line_writing_mode)
    : NGPhysicalContainerFragment(builder,
                                  block_or_line_writing_mode,
                                  children_,
                                  kFragmentBox,
                                  builder->BoxType()) {
  DCHECK(layout_object_);
  DCHECK(layout_object_->IsBoxModelObject());

  has_fragment_items_ = false;
  if (NGFragmentItemsBuilder* items_builder = builder->ItemsBuilder()) {
    // Omit |NGFragmentItems| if there were no items; e.g., display-lock.
    if (items_builder->Size()) {
      has_fragment_items_ = true;
      NGFragmentItems* items =
          const_cast<NGFragmentItems*>(ComputeItemsAddress());
      DCHECK_EQ(items_builder->GetWritingMode(), block_or_line_writing_mode);
      DCHECK_EQ(items_builder->Direction(), builder->Direction());
      items_builder->ToFragmentItems(Size(), items);
    }
  }

  has_layout_overflow_ = has_layout_overflow;
  if (has_layout_overflow_) {
    *const_cast<PhysicalRect*>(ComputeLayoutOverflowAddress()) =
        layout_overflow;
  }
  has_borders_ = has_borders;
  if (has_borders_)
    *const_cast<NGPhysicalBoxStrut*>(ComputeBordersAddress()) = borders;
  has_padding_ = has_padding;
  if (has_padding_)
    *const_cast<NGPhysicalBoxStrut*>(ComputePaddingAddress()) = padding;
  has_inflow_bounds_ = inflow_bounds.has_value();
  if (has_inflow_bounds_)
    *const_cast<PhysicalRect*>(ComputeInflowBoundsAddress()) = *inflow_bounds;
  has_rare_data_ = has_rare_data;
  if (has_rare_data_) {
    new (const_cast<RareData*>(ComputeRareDataAddress()))
        RareData(builder, Size());
  }

  is_first_for_node_ = builder->is_first_for_node_;
  may_have_descendant_above_block_start_ =
      builder->may_have_descendant_above_block_start_;
  is_fieldset_container_ = builder->is_fieldset_container_;
  is_table_ng_part_ = builder->is_table_ng_part_;
  is_legacy_layout_root_ = builder->is_legacy_layout_root_;
  is_painted_atomically_ =
      builder->space_ && builder->space_->IsPaintedAtomically();
  PhysicalBoxSides sides_to_include(builder->sides_to_include_,
                                    builder->GetWritingMode());
  include_border_top_ = sides_to_include.top;
  include_border_right_ = sides_to_include.right;
  include_border_bottom_ = sides_to_include.bottom;
  include_border_left_ = sides_to_include.left;
  is_inline_formatting_context_ = builder->is_inline_formatting_context_;
  is_math_fraction_ = builder->is_math_fraction_;
  is_math_operator_ = builder->is_math_operator_;

  // TODO(ikilpatrick): Investigate if new table-cells should always produce a
  // baseline.
  bool has_layout_containment = layout_object_->ShouldApplyLayoutContainment();
  if (builder->baseline_.has_value() && !has_layout_containment) {
    has_baseline_ = true;
    baseline_ = *builder->baseline_;
  } else {
    has_baseline_ = false;
    baseline_ = LayoutUnit::Min();
  }
  if (builder->last_baseline_.has_value() && !has_layout_containment) {
    has_last_baseline_ = true;
    last_baseline_ = *builder->last_baseline_;
  } else {
    has_last_baseline_ = false;
    last_baseline_ = LayoutUnit::Min();
  }

#if DCHECK_IS_ON()
  CheckIntegrity();
#endif
}

NGPhysicalBoxFragment::NGPhysicalBoxFragment(
    PassKey key,
    const NGPhysicalBoxFragment& other,
    bool has_layout_overflow,
    const PhysicalRect& layout_overflow,
    bool recalculate_layout_overflow)
    : NGPhysicalContainerFragment(other,
                                  recalculate_layout_overflow,
                                  children_),
      baseline_(other.baseline_),
      last_baseline_(other.last_baseline_) {
  if (has_fragment_items_) {
    NGFragmentItems* items =
        const_cast<NGFragmentItems*>(ComputeItemsAddress());
    new (items) NGFragmentItems(*other.ComputeItemsAddress());
  }
  has_layout_overflow_ = has_layout_overflow;
  if (has_layout_overflow_) {
    *const_cast<PhysicalRect*>(ComputeLayoutOverflowAddress()) =
        layout_overflow;
  }
  if (has_borders_) {
    *const_cast<NGPhysicalBoxStrut*>(ComputeBordersAddress()) =
        *other.ComputeBordersAddress();
  }
  if (has_padding_) {
    *const_cast<NGPhysicalBoxStrut*>(ComputePaddingAddress()) =
        *other.ComputePaddingAddress();
  }
  if (has_inflow_bounds_) {
    *const_cast<PhysicalRect*>(ComputeInflowBoundsAddress()) =
        *other.ComputeInflowBoundsAddress();
  }
  if (has_rare_data_) {
    new (const_cast<RareData*>(ComputeRareDataAddress()))
        RareData(*other.ComputeRareDataAddress());
  }
}

NGPhysicalBoxFragment::RareData::RareData(NGBoxFragmentBuilder* builder,
                                          PhysicalSize size)
    : mathml_paint_info(std::move(builder->mathml_paint_info_)) {
  oof_positioned_fragmentainer_descendants.ReserveCapacity(
      builder->oof_positioned_fragmentainer_descendants_.size());
  const WritingModeConverter converter(
      {builder->Style().GetWritingMode(), builder->Direction()}, size);
  for (const auto& descendant :
       builder->oof_positioned_fragmentainer_descendants_) {
    oof_positioned_fragmentainer_descendants.emplace_back(
        descendant.node,
        descendant.static_position.ConvertToPhysical(converter),
        descendant.inline_container,
        descendant.containing_block_offset.ConvertToPhysical(
            builder->Style().GetWritingDirection(), size,
            descendant.containing_block_fragment
                ? descendant.containing_block_fragment->Size()
                : PhysicalSize()),
        descendant.containing_block_fragment);
  }
  if (builder->table_grid_rect_)
    table_grid_rect = *builder->table_grid_rect_;
  if (builder->table_column_geometries_)
    table_column_geometries = *builder->table_column_geometries_;
  if (builder->table_collapsed_borders_)
    table_collapsed_borders = std::move(builder->table_collapsed_borders_);
  if (builder->table_collapsed_borders_geometry_) {
    table_collapsed_borders_geometry =
        std::move(builder->table_collapsed_borders_geometry_);
  }
  if (builder->table_cell_column_index_)
    table_cell_column_index = *builder->table_cell_column_index_;
}

NGPhysicalBoxFragment::RareData::RareData(const RareData& other)
    : oof_positioned_fragmentainer_descendants(
          other.oof_positioned_fragmentainer_descendants),
      mathml_paint_info(other.mathml_paint_info
                            ? new NGMathMLPaintInfo(*other.mathml_paint_info)
                            : nullptr),
      table_grid_rect(other.table_grid_rect),
      table_column_geometries(other.table_column_geometries),
      table_collapsed_borders(other.table_collapsed_borders),
      table_collapsed_borders_geometry(
          other.table_collapsed_borders_geometry
              ? new NGTableFragmentData::CollapsedBordersGeometry(
                    *other.table_collapsed_borders_geometry)
              : nullptr),
      table_cell_column_index(other.table_cell_column_index) {}

scoped_refptr<const NGLayoutResult>
NGPhysicalBoxFragment::CloneAsHiddenForPaint() const {
  const ComputedStyle& style = Style();
  NGBoxFragmentBuilder builder(GetMutableLayoutObject(), &style,
                               style.GetWritingDirection());
  builder.SetBoxType(BoxType());
  NGFragmentGeometry initial_fragment_geometry{
      Size().ConvertToLogical(style.GetWritingMode())};
  builder.SetInitialFragmentGeometry(initial_fragment_geometry);
  builder.SetIsHiddenForPaint(true);
  return builder.ToBoxFragment();
}

const NGPhysicalBoxFragment* NGPhysicalBoxFragment::PostLayout() const {
  const auto* layout_object = GetSelfOrContainerLayoutObject();
  if (UNLIKELY(!layout_object)) {
    NOTREACHED();
    return nullptr;
  }
  const auto* box = DynamicTo<LayoutBox>(layout_object);
  if (UNLIKELY(!box)) {
    DCHECK(IsInlineBox());
    return this;
  }
  if (UNLIKELY(IsColumnBox())) {
    // Column boxes should not be a relayout boundary.
    return this;
  }

  const wtf_size_t fragment_count = box->PhysicalFragmentCount();
  if (UNLIKELY(fragment_count == 0)) {
    // This should not happen, but DCHECK hits. crbug.com/1107204
    return nullptr;
  }
  if (fragment_count == 1) {
    const NGPhysicalBoxFragment* post_layout = box->GetPhysicalFragment(0);
    DCHECK(post_layout);
    if (UNLIKELY(post_layout != this)) {
      // This can happen at the relayout boundary crbug.com/829028
      // but DCHECKing |IsRelayoutBoundary()| hits. crbug.com/1107204
      return post_layout;
    }
  }
  // TODO(crbug.com/829028): Block fragmentation not supported yet.

  DCHECK(std::any_of(box->PhysicalFragments().begin(),
                     box->PhysicalFragments().end(),
                     [this](const NGPhysicalFragment& fragment) {
                       return this == &fragment;
                     }));
  return this;
}

PhysicalRect NGPhysicalBoxFragment::InkOverflow() const {
  if (const auto* owner_box = DynamicTo<LayoutBox>(GetLayoutObject()))
    return owner_box->PhysicalVisualOverflowRect();
  // TODO(kojii): (!IsCSSBox() || IsInlineBox()) is not supported yet. Implement
  // if needed.
  NOTREACHED();
  return LocalRect();
}

PhysicalRect NGPhysicalBoxFragment::ContentsInkOverflow() const {
  if (const auto* owner_box = DynamicTo<LayoutBox>(GetLayoutObject()))
    return owner_box->PhysicalContentsVisualOverflowRect();
  // TODO(kojii): (!IsCSSBox() || IsInlineBox()) is not supported yet. Implement
  // if needed.
  NOTREACHED();
  return LocalRect();
}

PhysicalRect NGPhysicalBoxFragment::OverflowClipRect(
    const PhysicalOffset& location,
    OverlayScrollbarClipBehavior overlay_scrollbar_clip_behavior) const {
  DCHECK(GetLayoutObject() && GetLayoutObject()->IsBox());
  const LayoutBox* box = To<LayoutBox>(GetLayoutObject());
  return box->OverflowClipRect(location, overlay_scrollbar_clip_behavior);
}

bool NGPhysicalBoxFragment::MayIntersect(
    const HitTestResult& result,
    const HitTestLocation& hit_test_location,
    const PhysicalOffset& accumulated_offset) const {
  if (const auto* box = DynamicTo<LayoutBox>(GetLayoutObject()))
    return box->MayIntersect(result, hit_test_location, accumulated_offset);
  // TODO(kojii): (!IsCSSBox() || IsInlineBox()) is not supported yet. Implement
  // if needed. For now, just return |true| not to do early return.
  return true;
}

PhysicalRect NGPhysicalBoxFragment::ScrollableOverflow(
    TextHeightType height_type) const {
  DCHECK(GetLayoutObject());
  DCHECK_EQ(PostLayout(), this);
  if (UNLIKELY(IsLayoutObjectDestroyedOrMoved())) {
    NOTREACHED();
    return PhysicalRect();
  }
  const LayoutObject* layout_object = GetLayoutObject();
  if (height_type == TextHeightType::kEmHeight && IsRubyBox()) {
    return ScrollableOverflowFromChildren(height_type);
  }
  if (layout_object->IsBox()) {
    if (HasNonVisibleOverflow())
      return PhysicalRect({}, Size());
    // Legacy is the source of truth for overflow
    return PhysicalRect(To<LayoutBox>(layout_object)->LayoutOverflowRect());
  } else if (layout_object->IsLayoutInline()) {
    // Inline overflow is a union of child overflows.
    PhysicalRect overflow;
    if (height_type == TextHeightType::kNormalHeight || BoxType() != kInlineBox)
      overflow = PhysicalRect({}, Size());
    for (const auto& child_fragment : PostLayoutChildren()) {
      PhysicalRect child_overflow =
          child_fragment->ScrollableOverflowForPropagation(*this, height_type);
      child_overflow.offset += child_fragment.Offset();
      overflow.Unite(child_overflow);
    }
    return overflow;
  } else {
    NOTREACHED();
  }
  return PhysicalRect({}, Size());
}

PhysicalRect NGPhysicalBoxFragment::ScrollableOverflowFromChildren(
    TextHeightType height_type) const {
  DCHECK_EQ(PostLayout(), this);
  const NGFragmentItems* items = Items();
  if (Children().empty() && !items)
    return PhysicalRect();

  // Internal struct to share logic between child fragments and child items.
  // - Inline children's overflow expands by padding end/after.
  // - Float / OOF overflow is added as is.
  // - Children not reachable by scroll overflow do not contribute to it.
  struct ComputeOverflowContext {
    ComputeOverflowContext(const NGPhysicalBoxFragment& container,
                           TextHeightType height_type)
        : container(container),
          style(container.Style()),
          writing_direction(style.GetWritingDirection()),
          border_inline_start(LayoutUnit(style.BorderStartWidth())),
          border_block_start(LayoutUnit(style.BorderBeforeWidth())),
          height_type(height_type) {
      DCHECK_EQ(&style, container.GetLayoutObject()->Style(
                            container.UsesFirstLineStyle()));

      // End and under padding are added to scroll overflow of inline children.
      // https://github.com/w3c/csswg-drafts/issues/129
      DCHECK_EQ(container.HasNonVisibleOverflow(),
                container.GetLayoutObject()->HasNonVisibleOverflow());
      if (container.HasNonVisibleOverflow()) {
        const auto* layout_object = To<LayoutBox>(container.GetLayoutObject());
        padding_strut = NGBoxStrut(LayoutUnit(), layout_object->PaddingEnd(),
                                   LayoutUnit(), layout_object->PaddingAfter())
                            .ConvertToPhysical(writing_direction);
      }
    }

    // Rectangles not reachable by scroll should not be added to overflow.
    bool IsRectReachableByScroll(const PhysicalRect& rect) {
      LogicalOffset rect_logical_end =
          rect.offset.ConvertToLogical(writing_direction, container.Size(),
                                       rect.size) +
          rect.size.ConvertToLogical(writing_direction.GetWritingMode());
      return rect_logical_end.inline_offset > border_inline_start ||
             rect_logical_end.block_offset > border_block_start;
    }

    void AddChild(const PhysicalRect& child_scrollable_overflow) {
      // Do not add overflow if fragment is not reachable by scrolling.
      if (height_type == kEmHeight ||
          IsRectReachableByScroll(child_scrollable_overflow))
        children_overflow.Unite(child_scrollable_overflow);
    }

    void AddFloatingOrOutOfFlowPositionedChild(
        const NGPhysicalFragment& child,
        const PhysicalOffset& child_offset) {
      DCHECK(child.IsFloatingOrOutOfFlowPositioned());
      PhysicalRect child_scrollable_overflow =
          child.ScrollableOverflowForPropagation(container, height_type);
      child_scrollable_overflow.offset += child_offset;
      AddChild(child_scrollable_overflow);
    }

    void AddLineBoxChild(const NGPhysicalLineBoxFragment& child,
                         const PhysicalOffset& child_offset) {
      if (padding_strut)
        AddLineBoxRect({child_offset, child.Size()});
      PhysicalRect child_scrollable_overflow =
          child.ScrollableOverflow(container, style, height_type);
      child_scrollable_overflow.offset += child_offset;
      AddChild(child_scrollable_overflow);
    }

    void AddLineBoxChild(const NGFragmentItem& child,
                         const NGInlineCursor& cursor) {
      DCHECK_EQ(&child, cursor.CurrentItem());
      DCHECK_EQ(child.Type(), NGFragmentItem::kLine);
      if (padding_strut)
        AddLineBoxRect(child.RectInContainerFragment());
      const NGPhysicalLineBoxFragment* line_box = child.LineBoxFragment();
      DCHECK(line_box);
      PhysicalRect child_scrollable_overflow =
          line_box->ScrollableOverflowForLine(container, style, child, cursor,
                                              height_type);
      AddChild(child_scrollable_overflow);
    }

    void AddLineBoxRect(const PhysicalRect& linebox_rect) {
      DCHECK(padding_strut);
      if (lineboxes_enclosing_rect)
        lineboxes_enclosing_rect->Unite(linebox_rect);
      else
        lineboxes_enclosing_rect = linebox_rect;
    }

    void AddPaddingToLineBoxChildren() {
      if (lineboxes_enclosing_rect) {
        DCHECK(padding_strut);
        lineboxes_enclosing_rect->Expand(*padding_strut);
        AddChild(*lineboxes_enclosing_rect);
      }
    }

    const NGPhysicalBoxFragment& container;
    const ComputedStyle& style;
    const WritingDirectionMode writing_direction;
    const LayoutUnit border_inline_start;
    const LayoutUnit border_block_start;
    base::Optional<NGPhysicalBoxStrut> padding_strut;
    base::Optional<PhysicalRect> lineboxes_enclosing_rect;
    PhysicalRect children_overflow;
    TextHeightType height_type;
  } context(*this, height_type);

  // Traverse child items.
  if (items) {
    for (NGInlineCursor cursor(*this, *items); cursor;
         cursor.MoveToNextSkippingChildren()) {
      const NGFragmentItem* item = cursor.CurrentItem();
      if (item->Type() == NGFragmentItem::kLine) {
        context.AddLineBoxChild(*item, cursor);
        continue;
      }

      if (const NGPhysicalBoxFragment* child_box =
              item->PostLayoutBoxFragment()) {
        if (child_box->IsFloatingOrOutOfFlowPositioned()) {
          context.AddFloatingOrOutOfFlowPositionedChild(
              *child_box, item->OffsetInContainerFragment());
        }
      }
    }
  }

  // Traverse child fragments.
  const bool add_inline_children = !items && IsInlineFormattingContext();
  // Only add overflow for fragments NG has not reflected into Legacy.
  // These fragments are:
  // - inline fragments,
  // - out of flow fragments whose css container is inline box.
  // TODO(layout-dev) Transforms also need to be applied to compute overflow
  // correctly. NG is not yet transform-aware. crbug.com/855965
  for (const auto& child : PostLayoutChildren()) {
    if (child->IsFloatingOrOutOfFlowPositioned()) {
      context.AddFloatingOrOutOfFlowPositionedChild(*child, child.Offset());
    } else if (add_inline_children && child->IsLineBox()) {
      context.AddLineBoxChild(To<NGPhysicalLineBoxFragment>(*child),
                              child.Offset());
    } else if (height_type == TextHeightType::kEmHeight && IsRubyRun()) {
      PhysicalRect r = child->ScrollableOverflow(*this, height_type);
      r.offset += child.offset;
      context.AddChild(r);
    }
  }

  context.AddPaddingToLineBoxChildren();

  return context.children_overflow;
}

LayoutSize NGPhysicalBoxFragment::PixelSnappedScrolledContentOffset() const {
  DCHECK(GetLayoutObject());
  const LayoutBox* box = To<LayoutBox>(GetLayoutObject());
  return box->PixelSnappedScrolledContentOffset();
}

PhysicalSize NGPhysicalBoxFragment::ScrollSize() const {
  DCHECK(GetLayoutObject());
  const LayoutBox* box = To<LayoutBox>(GetLayoutObject());
  return {box->ScrollWidth(), box->ScrollHeight()};
}

const NGPhysicalBoxFragment*
NGPhysicalBoxFragment::InlineContainerFragmentIfOutlineOwner() const {
  DCHECK(IsInlineBox());
  // In order to compute united outlines, collect all rectangles of inline
  // fragments for |LayoutInline| if |this| is the first inline fragment.
  // Otherwise return none.
  const LayoutObject* layout_object = GetLayoutObject();
  DCHECK(layout_object);
  DCHECK(layout_object->IsLayoutInline());
  NGInlineCursor cursor;
  cursor.MoveTo(*layout_object);
  DCHECK(cursor);
  if (cursor.Current().BoxFragment() == this)
    return &cursor.ContainerFragment();
  if (!cursor.IsBlockFragmented())
    return nullptr;

  // When |LayoutInline| is block fragmented, unite rectangles for each block
  // fragment. To do this, return |true| if |this| is the first inline fragment
  // of a block fragment.
  for (wtf_size_t previous_fragment_index = cursor.ContainerFragmentIndex();;) {
    cursor.MoveToNextForSameLayoutObject();
    DCHECK(cursor);
    const wtf_size_t fragment_index = cursor.ContainerFragmentIndex();
    if (cursor.Current().BoxFragment() == this) {
      if (fragment_index != previous_fragment_index)
        return &cursor.ContainerFragment();
      return nullptr;
    }
    previous_fragment_index = fragment_index;
  }
}

PhysicalRect NGPhysicalBoxFragment::ComputeSelfInkOverflow() const {
  DCHECK_EQ(PostLayout(), this);
  CheckCanUpdateInkOverflow();
  const ComputedStyle& style = Style();
  if (!style.HasVisualOverflowingEffect())
    return LocalRect();

  DCHECK(GetLayoutObject());
  PhysicalRect ink_overflow(LocalRect());
  ink_overflow.Expand(style.BoxDecorationOutsets());
  if (NGOutlineUtils::HasPaintedOutline(style, GetNode()) && IsOutlineOwner()) {
    Vector<PhysicalRect> outline_rects;
    // The result rects are in coordinates of this object's border box.
    AddSelfOutlineRects(PhysicalOffset(),
                        style.OutlineRectsShouldIncludeBlockVisualOverflow(),
                        &outline_rects);
    PhysicalRect rect = UnionRect(outline_rects);
    rect.Inflate(LayoutUnit(style.OutlineOutsetExtent()));
    ink_overflow.Unite(rect);
  }
  return ink_overflow;
}

void NGPhysicalBoxFragment::AddSelfOutlineRects(
    const PhysicalOffset& additional_offset,
    NGOutlineType outline_type,
    Vector<PhysicalRect>* outline_rects) const {
  AddOutlineRects(additional_offset, outline_type,
                  /* container_relative */ false, outline_rects);
}

void NGPhysicalBoxFragment::AddOutlineRects(
    const PhysicalOffset& additional_offset,
    NGOutlineType outline_type,
    Vector<PhysicalRect>* outline_rects) const {
  AddOutlineRects(additional_offset, outline_type,
                  /* container_relative */ true, outline_rects);
}

void NGPhysicalBoxFragment::AddOutlineRects(
    const PhysicalOffset& additional_offset,
    NGOutlineType outline_type,
    bool inline_container_relative,
    Vector<PhysicalRect>* outline_rects) const {
  DCHECK_EQ(PostLayout(), this);

  if (IsInlineBox()) {
    AddOutlineRectsForInlineBox(additional_offset, outline_type,
                                inline_container_relative, outline_rects);
    return;
  }
  DCHECK(IsOutlineOwner());

  // For anonymous blocks, the children add outline rects.
  if (!IsAnonymousBlock())
    outline_rects->emplace_back(additional_offset, Size().ToLayoutSize());

  if (outline_type == NGOutlineType::kIncludeBlockVisualOverflow &&
      !HasNonVisibleOverflow() && !HasControlClip(*this)) {
    // Tricky code ahead: we pass a 0,0 additional_offset to
    // AddOutlineRectsForNormalChildren, and add it in after the call.
    // This is necessary because AddOutlineRectsForNormalChildren expects
    // additional_offset to be an offset from containing_block.
    // Since containing_block is our layout object, offset must be 0,0.
    // https://crbug.com/968019
    Vector<PhysicalRect> children_rects;
    AddOutlineRectsForNormalChildren(
        &children_rects, PhysicalOffset(), outline_type,
        To<LayoutBoxModelObject>(GetLayoutObject()));
    if (!additional_offset.IsZero()) {
      for (auto& rect : children_rects)
        rect.offset += additional_offset;
    }
    outline_rects->AppendVector(children_rects);
    // LayoutBlock::AddOutlineRects also adds out of flow objects here.
    // In LayoutNG out of flow objects are not part of the outline.
  }
  // TODO(kojii): Needs inline_element_continuation logic from
  // LayoutBlockFlow::AddOutlineRects?
}

void NGPhysicalBoxFragment::AddOutlineRectsForInlineBox(
    PhysicalOffset additional_offset,
    NGOutlineType outline_type,
    bool container_relative,
    Vector<PhysicalRect>* rects) const {
  DCHECK_EQ(PostLayout(), this);
  DCHECK(IsInlineBox());

  const NGPhysicalBoxFragment* container =
      InlineContainerFragmentIfOutlineOwner();
  if (!container)
    return;

  // In order to compute united outlines, collect all rectangles of inline
  // fragments for |LayoutInline| if |this| is the first inline fragment.
  // Otherwise return none.
  //
  // When |LayoutInline| is block fragmented, unite rectangles for each block
  // fragment.
  DCHECK(GetLayoutObject());
  DCHECK(GetLayoutObject()->IsLayoutInline());
  const auto* layout_object = To<LayoutInline>(GetLayoutObject());
  const wtf_size_t initial_rects_size = rects->size();
  NGInlineCursor cursor(*container);
  cursor.MoveTo(*layout_object);
  DCHECK(cursor);
#if DCHECK_IS_ON()
  bool has_this_fragment = false;
#endif
  for (; cursor; cursor.MoveToNextForSameLayoutObject()) {
    const NGInlineCursorPosition& current = cursor.Current();
#if DCHECK_IS_ON()
    has_this_fragment = has_this_fragment || current.BoxFragment() == this;
#endif
    if (!current.Size().IsZero())
      rects->push_back(current.RectInContainerFragment());

    // Add descendants if any, in the container-relative coordinate.
    if (!current.HasChildren())
      continue;
    NGInlineCursor descendants = cursor.CursorForDescendants();
    AddOutlineRectsForCursor(rects, PhysicalOffset(), outline_type,
                             layout_object, &descendants);
  }
#if DCHECK_IS_ON()
  DCHECK(has_this_fragment);
#endif
  DCHECK_GE(rects->size(), initial_rects_size);
  if (rects->size() <= initial_rects_size)
    return;

  // Adjust the rectangles using |additional_offset| and |container_relative|.
  if (!container_relative)
    additional_offset -= (*rects)[initial_rects_size].offset;
  if (additional_offset.IsZero())
    return;
  for (PhysicalRect& rect :
       base::make_span(rects->begin() + initial_rects_size, rects->end()))
    rect.offset += additional_offset;
}

PositionWithAffinity NGPhysicalBoxFragment::PositionForPoint(
    PhysicalOffset point) const {
  if (layout_object_->IsBox() && !layout_object_->IsLayoutNGObject()) {
    // Layout engine boundary. Enter legacy PositionForPoint().
    return layout_object_->PositionForPoint(point);
  }

  // TODO(layout-dev): Handle situations where we're near (but not within)
  // atomic inlines here, rather than relying on it being taken care of by the
  // layout object. This is currently handled in LayoutBlock and
  // LayoutNGBlockFlowMixin - look for
  // PositionForPointIfOutsideAtomicInlineLevel().
  DCHECK(!IsAtomicInline() ||
         PhysicalRect(PhysicalOffset(), Size()).Contains(point));

  if (IsScrollContainer())
    point += PhysicalOffset(PixelSnappedScrolledContentOffset());

  if (const NGFragmentItems* items = Items()) {
    NGInlineCursor cursor(*this, *items);
    if (const PositionWithAffinity position =
            cursor.PositionForPointInInlineFormattingContext(point, *this))
      return AdjustForEditingBoundary(position);
    return layout_object_->CreatePositionWithAffinity(0);
  }

  if (!RuntimeEnabledFeatures::LayoutNGFullPositionForPointEnabled()) {
    DCHECK(layout_object_->ChildrenInline());
    return layout_object_->CreatePositionWithAffinity(0);
  }

  NGLink closest_child = {nullptr};
  LayoutUnit shortest_distance = LayoutUnit::Max();
  bool found_hit_test_candidate = false;
  const PhysicalSize pixel_size(LayoutUnit(1), LayoutUnit(1));
  PhysicalRect point_rect(point, pixel_size);

  // This is a general-purpose algorithm for finding the nearest child. There
  // may be cases where want to introduce specialized algorithms that e.g. takes
  // the progression direction into account (so that we can break earlier, or
  // even add special behavior). Children in block containers progress in the
  // block direction, for instance, while table cells progress in the inline
  // direction. Flex containers may progress in the inline direction, reverse
  // inline direction, block direction or reverse block direction. Multicol
  // containers progress both in the inline direction (columns) and block
  // direction (column rows and spanners).
  for (const NGLink& child : Children()) {
    const auto& box_fragment = To<NGPhysicalBoxFragment>(*child.fragment);
    bool is_hit_test_candidate = IsHitTestCandidate(box_fragment);
    if (!is_hit_test_candidate) {
      if (found_hit_test_candidate)
        continue;
      // We prefer valid hit-test candidates, but if there are no such children,
      // we'll lower our requirements somewhat. The exact reasoning behind the
      // details here is unknown, but it is something that evolved during
      // WebKit's early years.
      if (box_fragment.Style().Visibility() != EVisibility::kVisible ||
          (box_fragment.Children().empty() && !box_fragment.IsBlockFlow()))
        continue;
    }

    PhysicalRect child_rect(child.offset, child->Size());
    LayoutUnit horizontal_distance;
    if (child_rect.X() > point_rect.X())
      horizontal_distance = child_rect.X() - point_rect.X();
    else if (point_rect.Right() > child_rect.Right())
      horizontal_distance = point_rect.Right() - child_rect.Right();
    LayoutUnit vertical_distance;
    if (child_rect.Y() > point_rect.Y())
      vertical_distance = child_rect.Y() - point_rect.Y();
    else if (point_rect.Bottom() > child_rect.Bottom())
      vertical_distance = point_rect.Bottom() - child_rect.Bottom();

    if (!horizontal_distance && !vertical_distance) {
      // We actually hit a child. We're done.
      closest_child = child;
      break;
    }

    const LayoutUnit distance = horizontal_distance * horizontal_distance +
                                vertical_distance * vertical_distance;

    if (shortest_distance > distance ||
        (is_hit_test_candidate && !found_hit_test_candidate)) {
      // This child is either closer to the point than any previous child, or
      // this is the first child that is an actual hit-test candidate.
      shortest_distance = distance;
      closest_child = child;
      found_hit_test_candidate = is_hit_test_candidate;
    }
  }

  if (!closest_child.fragment)
    return layout_object_->CreatePositionWithAffinity(0);

  const auto& child = To<NGPhysicalBoxFragment>(*closest_child);
  Node* child_node = child.NonPseudoNode();
  PhysicalOffset point_in_child = point - closest_child.offset;
  if (!child.IsCSSBox() || !child_node)
    return child.PositionForPoint(point_in_child);

  // First make sure that the editability of the parent and child agree.
  // TODO(layout-dev): Could we just walk the DOM tree instead here?
  const LayoutObject* ancestor = layout_object_;
  while (ancestor && !ancestor->NonPseudoNode())
    ancestor = ancestor->Parent();
  if (!ancestor || !ancestor->Parent() ||
      (ancestor->HasLayer() && ancestor->Parent()->IsLayoutView()) ||
      HasEditableStyle(*ancestor->NonPseudoNode()) ==
          HasEditableStyle(*child_node))
    return child.PositionForPoint(point_in_child);

  // If editiability isn't the same in the ancestor and the child, then we
  // return a visible position just before or after the child, whichever side is
  // closer.
  WritingModeConverter converter(child.Style().GetWritingDirection(), Size());
  LogicalOffset logical_point = converter.ToLogical(point, pixel_size);
  LogicalOffset child_logical_offset =
      converter.ToLogical(closest_child.offset, child.Size());
  LogicalOffset logical_point_in_child = logical_point - child_logical_offset;
  LogicalSize child_logical_size = converter.ToLogical(child.Size());
  LayoutUnit child_middle = child_logical_size.inline_size / 2;
  if (logical_point_in_child.inline_offset < child_middle)
    return child.GetLayoutObject()->PositionBeforeThis();
  return child.GetLayoutObject()->PositionAfterThis();
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

NGPixelSnappedPhysicalBoxStrut NGPhysicalBoxFragment::BorderWidths() const {
  PhysicalBoxSides sides = SidesToInclude();
  NGPhysicalBoxStrut borders = Borders();
  if (!sides.top)
    borders.top = LayoutUnit();
  if (!sides.right)
    borders.right = LayoutUnit();
  if (!sides.bottom)
    borders.bottom = LayoutUnit();
  if (!sides.left)
    borders.left = LayoutUnit();
  return borders.SnapToDevicePixels();
}

#if DCHECK_IS_ON()
void NGPhysicalBoxFragment::CheckSameForSimplifiedLayout(
    const NGPhysicalBoxFragment& other,
    bool check_same_block_size) const {
  DCHECK_EQ(layout_object_, other.layout_object_);

  LogicalSize size = size_.ConvertToLogical(Style().GetWritingMode());
  LogicalSize other_size =
      other.size_.ConvertToLogical(Style().GetWritingMode());
  DCHECK_EQ(size.inline_size, other_size.inline_size);
  if (check_same_block_size)
    DCHECK_EQ(size.block_size, other_size.block_size);

  // "simplified" layout doesn't work within a fragmentation context.
  DCHECK(!break_token_ && !other.break_token_);

  DCHECK_EQ(type_, other.type_);
  DCHECK_EQ(sub_type_, other.sub_type_);
  DCHECK_EQ(style_variant_, other.style_variant_);
  DCHECK_EQ(is_hidden_for_paint_, other.is_hidden_for_paint_);
  DCHECK_EQ(is_math_fraction_, other.is_math_fraction_);
  DCHECK_EQ(is_math_operator_, other.is_math_operator_);

  // |has_floating_descendants_for_paint_| can change during simplified layout.
  DCHECK_EQ(may_have_descendant_above_block_start_,
            other.may_have_descendant_above_block_start_);
  DCHECK_EQ(depends_on_percentage_block_size_,
            other.depends_on_percentage_block_size_);

  DCHECK_EQ(is_fieldset_container_, other.is_fieldset_container_);
  DCHECK_EQ(is_table_ng_part_, other.is_table_ng_part_);
  DCHECK_EQ(is_legacy_layout_root_, other.is_legacy_layout_root_);
  DCHECK_EQ(is_painted_atomically_, other.is_painted_atomically_);
  DCHECK_EQ(has_collapsed_borders_, other.has_collapsed_borders_);

  DCHECK_EQ(is_inline_formatting_context_, other.is_inline_formatting_context_);
  DCHECK_EQ(has_fragment_items_, other.has_fragment_items_);
  DCHECK_EQ(include_border_top_, other.include_border_top_);
  DCHECK_EQ(include_border_right_, other.include_border_right_);
  DCHECK_EQ(include_border_bottom_, other.include_border_bottom_);
  DCHECK_EQ(include_border_left_, other.include_border_left_);

  // The oof_positioned_descendants_ vector can change during "simplified"
  // layout. This occurs when an OOF-descendant changes from "fixed" to
  // "absolute" (or visa versa) changing its containing block.

  // Legacy layout can (incorrectly) shift baseline position(s) during
  // "simplified" layout.
  DCHECK(IsLegacyLayoutRoot() || Baseline() == other.Baseline());
  if (check_same_block_size) {
    DCHECK(IsLegacyLayoutRoot() || LastBaseline() == other.LastBaseline());
  } else {
    DCHECK(IsLegacyLayoutRoot() || LastBaseline() == other.LastBaseline() ||
           NGBlockNode(To<LayoutBox>(GetMutableLayoutObject()))
               .UseBlockEndMarginEdgeForInlineBlockBaseline());
  }

  if (IsTableNG()) {
    DCHECK_EQ(TableGridRect(), other.TableGridRect());

    if (TableColumnGeometries()) {
      DCHECK(other.TableColumnGeometries());
      DCHECK(*TableColumnGeometries() == *other.TableColumnGeometries());
    } else {
      DCHECK(!other.TableColumnGeometries());
    }

    DCHECK_EQ(TableCollapsedBorders(), other.TableCollapsedBorders());

    if (TableCollapsedBordersGeometry()) {
      DCHECK(other.TableCollapsedBordersGeometry());
      TableCollapsedBordersGeometry()->CheckSameForSimplifiedLayout(
          *other.TableCollapsedBordersGeometry());
    } else {
      DCHECK(!other.TableCollapsedBordersGeometry());
    }
  }

  if (IsTableNGCell())
    DCHECK_EQ(TableCellColumnIndex(), other.TableCellColumnIndex());

  DCHECK(Borders() == other.Borders());
  DCHECK(Padding() == other.Padding());
}

// Check our flags represent the actual children correctly.
void NGPhysicalBoxFragment::CheckIntegrity() const {
  bool has_inflow_blocks = false;
  bool has_inlines = false;
  bool has_line_boxes = false;
  bool has_floats = false;
  bool has_list_markers = false;

  for (const NGLink& child : Children()) {
    if (child->IsFloating())
      has_floats = true;
    else if (child->IsOutOfFlowPositioned())
      ;  // OOF can be in the fragment tree regardless of |HasItems|.
    else if (child->IsLineBox())
      has_line_boxes = true;
    else if (child->IsListMarker())
      has_list_markers = true;
    else if (child->IsInline())
      has_inlines = true;
    else
      has_inflow_blocks = true;
  }

  // If we have line boxes, |IsInlineFormattingContext()| is true, but the
  // reverse is not always true.
  if (has_line_boxes || has_inlines)
    DCHECK(IsInlineFormattingContext());

  // If display-locked, we may not have any children.
  DCHECK(layout_object_);
  if (layout_object_ && layout_object_->ChildPaintBlockedByDisplayLock())
    return;

  if (RuntimeEnabledFeatures::LayoutNGFragmentItemEnabled()) {
    if (RuntimeEnabledFeatures::LayoutNGBlockFragmentationEnabled()) {
      if (has_line_boxes)
        DCHECK(HasItems());
    } else {
      DCHECK_EQ(HasItems(), has_line_boxes);
    }

    if (has_line_boxes) {
      DCHECK(!has_inlines);
      DCHECK(!has_inflow_blocks);
      // The following objects should be in the items, not in the tree. One
      // exception is that floats may occur as regular fragments in the tree
      // after a fragmentainer break.
      DCHECK(!has_floats || !IsFirstForNode());
      DCHECK(!has_list_markers);
    }
  }
}
#endif

}  // namespace blink
