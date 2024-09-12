// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/physical_box_fragment.h"

#include "build/chromeos_buildflags.h"
#include "third_party/blink/renderer/core/display_lock/display_lock_utilities.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/position_with_affinity.h"
#include "third_party/blink/renderer/core/editing/text_affinity.h"
#include "third_party/blink/renderer/core/html/html_anchor_element.h"
#include "third_party/blink/renderer/core/layout/block_break_token.h"
#include "third_party/blink/renderer/core/layout/box_fragment_builder.h"
#include "third_party/blink/renderer/core/layout/disable_layout_side_effects_scope.h"
#include "third_party/blink/renderer/core/layout/geometry/box_strut.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_rect.h"
#include "third_party/blink/renderer/core/layout/geometry/writing_mode_converter.h"
#include "third_party/blink/renderer/core/layout/inline/fragment_item.h"
#include "third_party/blink/renderer/core/layout/inline/inline_cursor.h"
#include "third_party/blink/renderer/core/layout/inline/inline_item.h"
#include "third_party/blink/renderer/core/layout/inline/physical_line_box_fragment.h"
#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/core/layout/layout_inline.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/layout_object_inlines.h"
#include "third_party/blink/renderer/core/layout/layout_text_combine.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/outline_utils.h"
#include "third_party/blink/renderer/core/layout/relative_utils.h"
#include "third_party/blink/renderer/core/layout/table/layout_table_cell.h"
#include "third_party/blink/renderer/core/paint/inline_paint_context.h"
#include "third_party/blink/renderer/core/paint/outline_painter.h"
#include "third_party/blink/renderer/platform/wtf/size_assertions.h"

namespace blink {

#if DCHECK_IS_ON()
unsigned PhysicalBoxFragment::AllowPostLayoutScope::allow_count_ = 0;
#endif

namespace {

struct SameSizeAsPhysicalBoxFragment : PhysicalFragment {
  unsigned flags;
  LayoutUnit baseline;
  LayoutUnit last_baseline;
  Member<void*> rare;
  InkOverflow ink_overflow;
  HeapVector<PhysicalFragmentLink> children;
};

ASSERT_SIZE(PhysicalBoxFragment, SameSizeAsPhysicalBoxFragment);

bool HasControlClip(const PhysicalBoxFragment& self) {
  const LayoutBox* box = DynamicTo<LayoutBox>(self.GetLayoutObject());
  return box && box->HasControlClip();
}

bool ShouldUsePositionForPointInBlockFlowDirection(
    const LayoutObject& layout_object) {
  const LayoutBlockFlow* const layout_block_flow =
      DynamicTo<LayoutBlockFlow>(layout_object);
  if (!layout_block_flow) {
    // For <tr>, see editing/selection/click-before-and-after-table.html
    return false;
  }
  if (layout_block_flow->StyleRef().SpecifiesColumns()) {
    // Columns are laid out in inline direction.
    return false;
  }
  return true;
}

inline bool IsHitTestCandidate(const PhysicalBoxFragment& fragment) {
  return fragment.Size().height &&
         fragment.Style().UsedVisibility() == EVisibility::kVisible &&
         !fragment.IsFloatingOrOutOfFlowPositioned();
}

// Applies the overflow clip to |result|. For any axis that is clipped, |result|
// is reset to |no_overflow_rect|. If neither axis is clipped, nothing is
// changed.
void ApplyOverflowClip(OverflowClipAxes overflow_clip_axes,
                       const PhysicalRect& no_overflow_rect,
                       PhysicalRect* result) {
  if (overflow_clip_axes & kOverflowClipX) {
    result->SetX(no_overflow_rect.X());
    result->SetWidth(no_overflow_rect.Width());
  }
  if (overflow_clip_axes & kOverflowClipY) {
    result->SetY(no_overflow_rect.Y());
    result->SetHeight(no_overflow_rect.Height());
  }
}

}  // namespace

// static
const PhysicalBoxFragment* PhysicalBoxFragment::Create(
    BoxFragmentBuilder* builder,
    WritingMode block_or_line_writing_mode) {
  const auto writing_direction = builder->GetWritingDirection();
  const PhysicalBoxStrut borders =
      builder->ApplicableBorders().ConvertToPhysical(writing_direction);
  const PhysicalBoxStrut scrollbar =
      builder->ApplicableScrollbar().ConvertToPhysical(writing_direction);
  const PhysicalBoxStrut padding =
      builder->ApplicablePadding().ConvertToPhysical(writing_direction);

  const PhysicalSize physical_size =
      ToPhysicalSize(builder->Size(), builder->GetWritingMode());
  WritingModeConverter converter(writing_direction, physical_size);

  std::optional<PhysicalRect> inflow_bounds;
  if (builder->inflow_bounds_)
    inflow_bounds = converter.ToPhysical(*builder->inflow_bounds_);

#if DCHECK_IS_ON()
  if (builder->needs_inflow_bounds_explicitly_set_ && builder->node_ &&
      builder->node_.IsScrollContainer() && !builder->IsFragmentainerBoxType())
    DCHECK(builder->is_inflow_bounds_explicitly_set_);
  if (builder->needs_may_have_descendant_above_block_start_explicitly_set_)
    DCHECK(builder->is_may_have_descendant_above_block_start_explicitly_set_);
#endif

  PhysicalRect scrollable_overflow = {PhysicalOffset(), physical_size};
  if (builder->ShouldCalculateScrollableOverflow()) {
    ScrollableOverflowCalculator calculator(
        To<BlockNode>(builder->node_),
        /* is_css_box */ !builder->IsFragmentainerBoxType(),
        builder->GetConstraintSpace().HasBlockFragmentation(), borders,
        scrollbar, padding, physical_size, writing_direction);

    if (FragmentItemsBuilder* items_builder = builder->ItemsBuilder()) {
      calculator.AddItems(builder->GetLayoutObject(),
                          items_builder->Items(physical_size));
    }

    for (auto& child : builder->children_) {
      const auto* box_fragment =
          DynamicTo<PhysicalBoxFragment>(*child.fragment);
      if (!box_fragment)
        continue;

      calculator.AddChild(*box_fragment, child.offset.ConvertToPhysical(
                                             writing_direction, physical_size,
                                             box_fragment->Size()));
    }

    if (builder->table_collapsed_borders_)
      calculator.AddTableSelfRect();

    scrollable_overflow = calculator.Result(inflow_bounds);
  }

  // For the purposes of object allocation we have scrollable-overflow if it
  // differs from the fragment size.
  bool has_scrollable_overflow =
      scrollable_overflow != PhysicalRect({}, physical_size);

  // Omit |FragmentItems| if there were no items; e.g., display-lock.
  bool has_fragment_items = false;
  if (FragmentItemsBuilder* items_builder = builder->ItemsBuilder()) {
    if (items_builder->Size())
      has_fragment_items = true;
  }

  size_t byte_size = AdditionalByteSize(has_fragment_items);

  // We store the children list inline in the fragment as a flexible
  // array. Therefore, we need to make sure to allocate enough space for
  // that array here, which requires a manual allocation + placement new.
  // The initialization of the array is done by PhysicalFragment;
  // we pass the buffer as a constructor argument.
  return MakeGarbageCollected<PhysicalBoxFragment>(
      AdditionalBytes(byte_size), PassKey(), builder, has_scrollable_overflow,
      scrollable_overflow, borders.IsZero() ? nullptr : &borders,
      scrollbar.IsZero() ? nullptr : &scrollbar,
      padding.IsZero() ? nullptr : &padding, inflow_bounds, has_fragment_items,
      block_or_line_writing_mode);
}

// static
const PhysicalBoxFragment* PhysicalBoxFragment::Clone(
    const PhysicalBoxFragment& other) {
  // The size of the new fragment shouldn't differ from the old one.
  size_t byte_size = AdditionalByteSize(other.HasItems());

  return MakeGarbageCollected<PhysicalBoxFragment>(
      AdditionalBytes(byte_size), PassKey(), other,
      other.HasScrollableOverflow(), other.ScrollableOverflow());
}

// static
const PhysicalBoxFragment* PhysicalBoxFragment::CloneWithPostLayoutFragments(
    const PhysicalBoxFragment& other) {
  PhysicalRect scrollable_overflow = other.ScrollableOverflow();
  bool has_scrollable_overflow = other.HasScrollableOverflow();

  // The size of the new fragment shouldn't differ from the old one.
  size_t byte_size = AdditionalByteSize(other.HasItems());

  const auto* cloned_fragment = MakeGarbageCollected<PhysicalBoxFragment>(
      AdditionalBytes(byte_size), PassKey(), other, has_scrollable_overflow,
      scrollable_overflow);

  // To ensure the fragment tree is consistent, use the post-layout fragment.
#if DCHECK_IS_ON()
  AllowPostLayoutScope allow_post_layout_scope;
#endif

  for (PhysicalFragmentLink& child :
       cloned_fragment->GetMutableForCloning().Children()) {
    child.fragment = child->PostLayout();
    DCHECK(child.fragment);

    if (!child->IsFragmentainerBox())
      continue;

    // Fragmentainers don't have the concept of post-layout fragments, so if
    // this is a fragmentation context root (such as a multicol container), we
    // need to not only update its children, but also the children of the
    // children that are fragmentainers.
    auto& fragmentainer = *To<PhysicalBoxFragment>(child.fragment.Get());
    for (PhysicalFragmentLink& fragmentainer_child :
         fragmentainer.GetMutableForCloning().Children()) {
      auto& old_child =
          *To<PhysicalBoxFragment>(fragmentainer_child.fragment.Get());
      fragmentainer_child.fragment = old_child.PostLayout();
    }
  }

  if (cloned_fragment->HasItems()) {
    // Replace box fragment items with post layout fragments.
    for (const auto& cloned_item : cloned_fragment->Items()->Items()) {
      const PhysicalBoxFragment* box = cloned_item.BoxFragment();
      if (!box)
        continue;
      box = box->PostLayout();
      DCHECK(box);
      cloned_item.GetMutableForCloning().ReplaceBoxFragment(*box);
    }
  }

  return cloned_fragment;
}

namespace {
template <typename T>
constexpr void AccountSizeAndPadding(size_t& current_size) {
  const size_t current_size_with_padding =
      base::bits::AlignUp(current_size, alignof(T));
  current_size = current_size_with_padding + sizeof(T);
}
}  // namespace

// static
size_t PhysicalBoxFragment::AdditionalByteSize(bool has_fragment_items) {
  size_t additional_size = 0;
  if (has_fragment_items) {
    AccountSizeAndPadding<FragmentItems>(additional_size);
  }
  return additional_size;
}

PhysicalBoxFragment::PhysicalBoxFragment(
    PassKey key,
    BoxFragmentBuilder* builder,
    bool has_scrollable_overflow,
    const PhysicalRect& scrollable_overflow,
    const PhysicalBoxStrut* borders,
    const PhysicalBoxStrut* scrollbar,
    const PhysicalBoxStrut* padding,
    const std::optional<PhysicalRect>& inflow_bounds,
    bool has_fragment_items,
    WritingMode block_or_line_writing_mode)
    : PhysicalFragment(builder,
                       block_or_line_writing_mode,
                       kFragmentBox,
                       builder->GetBoxType()),
      bit_field_(ConstHasFragmentItemsFlag::encode(has_fragment_items) |
                 HasDescendantsForTablePartFlag::encode(false) |
                 IsFragmentationContextRootFlag::encode(
                     builder->is_fragmentation_context_root_) |
                 IsMonolithicFlag::encode(builder->is_monolithic_) |
                 IsMonolithicOverflowPropagationDisabledFlag::encode(
                     builder->GetConstraintSpace()
                         .IsMonolithicOverflowPropagationDisabled()) |
                 HasMovedChildrenInBlockDirectionFlag::encode(
                     builder->has_moved_children_in_block_direction_)) {
  DCHECK(layout_object_);
  DCHECK(layout_object_->IsBoxModelObject());
  DCHECK(!builder->break_token_ || builder->break_token_->IsBlockType());

  children_.resize(builder->children_.size());
  PhysicalSize size = Size();
  const WritingModeConverter converter(
      {block_or_line_writing_mode, builder->Direction()}, size);
  wtf_size_t i = 0;
  for (auto& child : builder->children_) {
    children_[i].offset =
        converter.ToPhysical(child.offset, child.fragment->Size());
    // Fragments in |builder| are not used after |this| was constructed.
    children_[i].fragment = child.fragment.Release();
    ++i;
  }

  if (HasItems()) {
    FragmentItemsBuilder* items_builder = builder->ItemsBuilder();
    auto* items = const_cast<FragmentItems*>(ComputeItemsAddress());
    DCHECK_EQ(items_builder->GetWritingMode(), block_or_line_writing_mode);
    DCHECK_EQ(items_builder->Direction(), builder->Direction());
    std::optional<PhysicalSize> new_size =
        items_builder->ToFragmentItems(Size(), items);
    if (new_size)
      size_ = *new_size;
  }

  SetInkOverflowType(InkOverflow::Type::kNotSet);

  wtf_size_t rare_fields_size =
      has_scrollable_overflow + !!builder->frame_set_layout_data_ +
      !!builder->mathml_paint_info_ + !!builder->table_grid_rect_ +
      !!builder->table_collapsed_borders_ +
      !!builder->table_collapsed_borders_geometry_ +
      !!builder->table_cell_column_index_ +
      (builder->table_section_row_offsets_.empty() ? 0 : 2) +
      !!builder->page_name_ + !!borders + !!scrollbar + !!padding +
      inflow_bounds.has_value() + !!builder->Style().MayHaveMargin();

  if (rare_fields_size > 0 || !builder->table_column_geometries_.empty() ||
      !builder->reading_flow_elements_.empty()) {
    rare_data_ = MakeGarbageCollected<PhysicalFragmentRareData>(
        has_scrollable_overflow ? &scrollable_overflow : nullptr, borders,
        scrollbar, padding, inflow_bounds, *builder, rare_fields_size);
  }

  bit_field_.set<IsFirstForNodeFlag>(builder->is_first_for_node_);
  is_fieldset_container_ = builder->is_fieldset_container_;
  is_table_part_ = builder->is_table_part_;
  is_painted_atomically_ = builder->space_.IsPaintedAtomically();
  PhysicalBoxSides sides_to_include(builder->sides_to_include_,
                                    builder->GetWritingMode());
  bit_field_.set<IncludeBorderTopFlag>(sides_to_include.top);
  bit_field_.set<IncludeBorderRightFlag>(sides_to_include.right);
  bit_field_.set<IncludeBorderBottomFlag>(sides_to_include.bottom);
  bit_field_.set<IncludeBorderLeftFlag>(sides_to_include.left);
  bit_field_.set<IsInlineFormattingContextFlag>(
      builder->is_inline_formatting_context_);
  is_math_fraction_ = builder->is_math_fraction_;
  is_math_operator_ = builder->is_math_operator_;

  const bool allow_baseline = !layout_object_->ShouldApplyLayoutContainment() ||
                              layout_object_->IsTableCell();
  if (allow_baseline && builder->first_baseline_.has_value()) {
    has_first_baseline_ = true;
    first_baseline_ = *builder->first_baseline_;
  } else {
    has_first_baseline_ = false;
    first_baseline_ = LayoutUnit::Min();
  }
  if (allow_baseline && builder->last_baseline_.has_value()) {
    has_last_baseline_ = true;
    last_baseline_ = *builder->last_baseline_;
  } else {
    has_last_baseline_ = false;
    last_baseline_ = LayoutUnit::Min();
  }
  use_last_baseline_for_inline_baseline_ =
      builder->use_last_baseline_for_inline_baseline_;

  bit_field_.set<HasDescendantsForTablePartFlag>(
      children_.size() || NeedsOOFPositionedInfoPropagation());

#if DCHECK_IS_ON()
  CheckIntegrity();
#endif
}

PhysicalBoxFragment::PhysicalBoxFragment(
    PassKey key,
    const PhysicalBoxFragment& other,
    bool has_scrollable_overflow,
    const PhysicalRect& scrollable_overflow)
    : PhysicalFragment(other),
      bit_field_(other.bit_field_),
      first_baseline_(other.first_baseline_),
      last_baseline_(other.last_baseline_),
      ink_overflow_(other.InkOverflowType(), other.ink_overflow_),
      children_(other.children_) {
  SetInkOverflowType(other.InkOverflowType());
  if (HasItems()) {
    auto* items = const_cast<FragmentItems*>(ComputeItemsAddress());
    new (items) FragmentItems(*other.ComputeItemsAddress());
  }
  if (other.rare_data_) {
    rare_data_ =
        MakeGarbageCollected<PhysicalFragmentRareData>(*other.rare_data_);
  }
}

PhysicalBoxFragment::~PhysicalBoxFragment() {
  if (HasInkOverflow())
    SetInkOverflowType(ink_overflow_.Reset(InkOverflowType()));
  if (HasItems())
    ComputeItemsAddress()->~FragmentItems();
}

PhysicalRect PhysicalBoxFragment::ContentRect() const {
  PhysicalRect rect(PhysicalOffset(), Size());
  rect.Contract(Borders() + Padding());
  DCHECK_GE(rect.size.width, LayoutUnit());
  DCHECK_GE(rect.size.height, LayoutUnit());
  return rect;
}

const LayoutBox* PhysicalBoxFragment::OwnerLayoutBox() const {
  // TODO(layout-dev): We should probably get rid of this method, now that it
  // does nothing, apart from some checking. The checks are useful, but could be
  // moved elsewhere.
  const LayoutBox* owner_box =
      DynamicTo<LayoutBox>(GetSelfOrContainerLayoutObject());

#if DCHECK_IS_ON()
  DCHECK(owner_box);
  if (IsFragmentainerBox()) [[unlikely]] {
    if (owner_box->IsLayoutView()) {
      DCHECK_EQ(GetBoxType(), kPageArea);
      DCHECK(To<LayoutView>(owner_box)->ShouldUsePaginatedLayout());
    } else {
      DCHECK(IsColumnBox());
    }
  } else {
    // Check |this| and the |LayoutBox| that produced it are in sync.
    DCHECK(owner_box->PhysicalFragments().Contains(*this));
    DCHECK_EQ(IsFirstForNode(), this == owner_box->GetPhysicalFragment(0));
  }
#endif

  return owner_box;
}

LayoutBox* PhysicalBoxFragment::MutableOwnerLayoutBox() const {
  return const_cast<LayoutBox*>(OwnerLayoutBox());
}

PhysicalOffset PhysicalBoxFragment::OffsetFromOwnerLayoutBox() const {
  DCHECK(IsCSSBox());

  // This function uses |FragmentData|, so must be |kPrePaintClean|.
  DCHECK_GE(GetDocument().Lifecycle().GetState(),
            DocumentLifecycle::kPrePaintClean);

  const LayoutBox* owner_box = OwnerLayoutBox();
  DCHECK(owner_box);
  DCHECK(owner_box->PhysicalFragments().Contains(*this));
  if (owner_box->PhysicalFragmentCount() <= 1)
    return PhysicalOffset();

  // When LTR, compute the offset from the first fragment. The first fragment is
  // at the left top of the |LayoutBox| regardless of the writing mode.
  const auto* containing_block = owner_box->ContainingBlock();
  const ComputedStyle& containing_block_style = containing_block->StyleRef();
  if (IsLtr(containing_block_style.Direction())) {
    DCHECK_EQ(IsFirstForNode(), this == owner_box->GetPhysicalFragment(0));
    if (IsFirstForNode())
      return PhysicalOffset();

    const FragmentData* fragment_data =
        owner_box->FragmentDataFromPhysicalFragment(*this);
    DCHECK(fragment_data);
    const FragmentData& first_fragment_data = owner_box->FirstFragment();
    // All |FragmentData| for an NG block fragmented |LayoutObject| should be in
    // the same transform node that their |PaintOffset()| are in the same
    // coordinate system.
    return fragment_data->PaintOffset() - first_fragment_data.PaintOffset();
  }

  // When RTL, compute the offset from the last fragment.
  const FragmentData* fragment_data =
      owner_box->FragmentDataFromPhysicalFragment(*this);
  DCHECK(fragment_data);
  const FragmentData& last_fragment_data = owner_box->FragmentList().back();
  return fragment_data->PaintOffset() - last_fragment_data.PaintOffset();
}

const PhysicalBoxFragment* PhysicalBoxFragment::PostLayout() const {
  // While side effects are disabled, new fragments are not copied to
  // |LayoutBox|. Just return the given fragment.
  if (DisableLayoutSideEffectsScope::IsDisabled()) {
    return this;
  }

  const LayoutObject* layout_object = GetLayoutObject();
  if (!layout_object) [[unlikely]] {
    // Some fragments don't have a layout object associated directly with
    // them. This is the case for lines and fragmentainers (columns / pages).
    // We don't need to do anything special for such fragments. Any post-layout
    // fragmentainers should be found as children of the post-layout fragments
    // of the containing block.
    //
    // In some cases the layout object may also have been removed. This can of
    // course not happen if we have actually performed layout, but we may in
    // some cases clone a fragment *before* layout, to ensure that the fragment
    // tree spine is correctly rebuilt after a subtree layout.
    return this;
  }
  const auto* box = DynamicTo<LayoutBox>(layout_object);
  if (!box) [[unlikely]] {
    DCHECK(IsInlineBox());
    return this;
  }

  const wtf_size_t fragment_count = box->PhysicalFragmentCount();
  if (fragment_count == 0) [[unlikely]] {
#if DCHECK_IS_ON()
    DCHECK(AllowPostLayoutScope::IsAllowed());
#endif
    return nullptr;
  }

  const PhysicalBoxFragment* post_layout = nullptr;
  if (fragment_count == 1) {
    post_layout = box->GetPhysicalFragment(0);
    DCHECK(post_layout);
  } else if (const auto* break_token = GetBreakToken()) {
    const unsigned index = break_token->SequenceNumber();
    if (index < fragment_count) {
      post_layout = box->GetPhysicalFragment(index);
      DCHECK(post_layout);
      DCHECK(!post_layout->GetBreakToken() ||
             post_layout->GetBreakToken()->SequenceNumber() == index);
    }
  } else {
    post_layout = &box->PhysicalFragments().back();
  }

  if (post_layout == this)
    return this;

// TODO(crbug.com/1241721): Revert https://crrev.com/c/3108806 to re-enable this
// DCHECK on CrOS.
#if DCHECK_IS_ON() && !BUILDFLAG(IS_CHROMEOS_ASH)
  DCHECK(AllowPostLayoutScope::IsAllowed());
#endif
  return post_layout;
}

PhysicalRect PhysicalBoxFragment::SelfInkOverflowRect() const {
  if (!CanUseFragmentsForInkOverflow()) [[unlikely]] {
    const auto* owner_box = DynamicTo<LayoutBox>(GetLayoutObject());
    return owner_box->SelfVisualOverflowRect();
  }
  if (!HasInkOverflow())
    return LocalRect();
  return ink_overflow_.Self(InkOverflowType(), Size());
}

PhysicalRect PhysicalBoxFragment::ContentsInkOverflowRect() const {
  if (!CanUseFragmentsForInkOverflow()) [[unlikely]] {
    const auto* owner_box = DynamicTo<LayoutBox>(GetLayoutObject());
    return owner_box->ContentsVisualOverflowRect();
  }
  if (!HasInkOverflow())
    return LocalRect();
  return ink_overflow_.Contents(InkOverflowType(), Size());
}

PhysicalRect PhysicalBoxFragment::InkOverflowRect() const {
  if (!CanUseFragmentsForInkOverflow()) [[unlikely]] {
    const auto* owner_box = DynamicTo<LayoutBox>(GetLayoutObject());
    return owner_box->VisualOverflowRect();
  }

  if (!HasInkOverflow())
    return LocalRect();

  const PhysicalRect self_rect = ink_overflow_.Self(InkOverflowType(), Size());
  const ComputedStyle& style = Style();
  if (style.HasMask())
    return self_rect;

  const OverflowClipAxes overflow_clip_axes = GetOverflowClipAxes();
  if (overflow_clip_axes == kNoOverflowClip) {
    return UnionRect(self_rect,
                     ink_overflow_.Contents(InkOverflowType(), Size()));
  }

  if (overflow_clip_axes == kOverflowClipBothAxis) {
    if (ShouldApplyOverflowClipMargin()) {
      const PhysicalRect& contents_rect =
          ink_overflow_.Contents(InkOverflowType(), Size());
      if (!contents_rect.IsEmpty()) {
        PhysicalRect result = LocalRect();
        result.Expand(OverflowClipMarginOutsets());
        result.Intersect(contents_rect);
        result.Unite(self_rect);
        return result;
      }
    }
    return self_rect;
  }

  PhysicalRect result = ink_overflow_.Contents(InkOverflowType(), Size());
  result.Unite(self_rect);
  ApplyOverflowClip(overflow_clip_axes, self_rect, &result);
  return result;
}

PhysicalRect PhysicalBoxFragment::OverflowClipRect(
    const PhysicalOffset& location,
    OverlayScrollbarClipBehavior overlay_scrollbar_clip_behavior) const {
  DCHECK(GetLayoutObject() && GetLayoutObject()->IsBox());
  const LayoutBox* box = To<LayoutBox>(GetLayoutObject());
  return box->OverflowClipRect(location, overlay_scrollbar_clip_behavior);
}

PhysicalRect PhysicalBoxFragment::OverflowClipRect(
    const PhysicalOffset& location,
    const BlockBreakToken* incoming_break_token,
    OverlayScrollbarClipBehavior overlay_scrollbar_clip_behavior) const {
  PhysicalRect clip_rect =
      OverflowClipRect(location, overlay_scrollbar_clip_behavior);
  if (!incoming_break_token && !GetBreakToken()) {
    return clip_rect;
  }

  // Clip the stitched box clip rectangle against the bounds of the fragment.
  //
  // TODO(layout-dev): It's most likely better to actually store the clip
  // rectangle in each fragment, rather than post-processing the stitched clip
  // rectangle like this.
  auto writing_direction = Style().GetWritingDirection();
  const LayoutBox* box = To<LayoutBox>(GetLayoutObject());
  WritingModeConverter converter(writing_direction, PhysicalSize(box->Size()));
  // Make the clip rectangle relative to the layout box.
  clip_rect.offset -= location;
  LogicalOffset stitched_offset;
  if (incoming_break_token)
    stitched_offset.block_offset = incoming_break_token->ConsumedBlockSize();
  LogicalRect logical_fragment_rect(
      stitched_offset,
      Size().ConvertToLogical(writing_direction.GetWritingMode()));
  PhysicalRect physical_fragment_rect =
      converter.ToPhysical(logical_fragment_rect);

  // For monolithic descendants that get sliced (for certain values of "sliced";
  // keep on reading) when printing, we will keep the stitched box clip
  // rectangle, and just translate it so that it becomes relative to this
  // fragment. The problem this addresses is the fact that monolithic
  // descendants only get sliced visually and overflow nicely into the next
  // pages, whereas, internally, a monolithic element always generates only one
  // fragment. If we clip it strictly against the originating fragment, we risk
  // losing content.
  //
  // This is a work-around for the fact that we never break monolithic content
  // into fragments (which the spec actually suggests that we do in such cases).
  //
  // This work-around only makes sense when printing, since pages are simply
  // stacked in the writing direction internally when printing, so that
  // overflowing content from one page "accidentally" ends up at the right place
  // on the next page. This isn't the case for multicol for instance (where this
  // problem is "unfixable" unless we implement support for breaking monolithic
  // content into fragments), so if we're not printing, clip it against the
  // bounds of the fragment now.
  if (!GetDocument().Printing()) {
    const auto overflow_clip = box->GetOverflowClipAxes();
    PhysicalRect overflow_physical_fragment_rect = physical_fragment_rect;
    if (overflow_clip != kOverflowClipBothAxis) {
      ApplyVisibleOverflowToClipRect(overflow_clip,
                                     overflow_physical_fragment_rect);
    } else if (box->ShouldApplyOverflowClipMargin()) {
      overflow_physical_fragment_rect.Expand(OverflowClipMarginOutsets());
    }

    // Clip against the fragment's bounds.
    clip_rect.Intersect(overflow_physical_fragment_rect);
  }

  // Make the clip rectangle relative to the fragment.
  clip_rect.offset -= physical_fragment_rect.offset;
  // Make the clip rectangle relative to whatever the caller wants.
  clip_rect.offset += location;
  return clip_rect;
}

bool PhysicalBoxFragment::MayIntersect(
    const HitTestResult& result,
    const HitTestLocation& hit_test_location,
    const PhysicalOffset& accumulated_offset) const {
  if (const auto* box = DynamicTo<LayoutBox>(GetLayoutObject()))
    return box->MayIntersect(result, hit_test_location, accumulated_offset);
  // TODO(kojii): (!IsCSSBox() || IsInlineBox()) is not supported yet. Implement
  // if needed. For now, just return |true| not to do early return.
  return true;
}

gfx::Vector2d PhysicalBoxFragment::PixelSnappedScrolledContentOffset() const {
  DCHECK(GetLayoutObject());
  return To<LayoutBox>(*GetLayoutObject()).PixelSnappedScrolledContentOffset();
}

PhysicalSize PhysicalBoxFragment::ScrollSize() const {
  DCHECK(GetLayoutObject());
  const LayoutBox* box = To<LayoutBox>(GetLayoutObject());
  return {box->ScrollWidth(), box->ScrollHeight()};
}

const PhysicalBoxFragment*
PhysicalBoxFragment::InlineContainerFragmentIfOutlineOwner() const {
  DCHECK(IsInlineBox());
  // In order to compute united outlines, collect all rectangles of inline
  // fragments for |LayoutInline| if |this| is the first inline fragment.
  // Otherwise return none.
  const LayoutObject* layout_object = GetLayoutObject();
  DCHECK(layout_object);
  DCHECK(layout_object->IsLayoutInline());
  InlineCursor cursor;
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

PhysicalFragmentRareData::RareField& PhysicalBoxFragment::EnsureRareField(
    FieldId id) {
  if (!rare_data_) {
    rare_data_ = MakeGarbageCollected<PhysicalFragmentRareData>(1);
  }
  return rare_data_->EnsureField(id);
}

PhysicalBoxFragment::MutableForStyleRecalc::MutableForStyleRecalc(
    base::PassKey<PhysicalBoxFragment>,
    PhysicalBoxFragment& fragment)
    : fragment_(fragment) {}

void PhysicalBoxFragment::MutableForStyleRecalc::SetScrollableOverflow(
    const PhysicalRect& scrollable_overflow) {
  bool has_scrollable_overflow =
      scrollable_overflow != PhysicalRect({}, fragment_.Size());
  if (has_scrollable_overflow) {
    // This can be called even without rare_data_.
    fragment_.EnsureRareField(FieldId::kScrollableOverflow)
        .scrollable_overflow = scrollable_overflow;
  } else if (fragment_.HasScrollableOverflow()) {
    fragment_.rare_data_->RemoveField(FieldId::kScrollableOverflow);
  }
}

PhysicalBoxFragment::MutableForStyleRecalc
PhysicalBoxFragment::GetMutableForStyleRecalc() const {
  DCHECK(layout_object_->GetDocument().Lifecycle().GetState() ==
             DocumentLifecycle::kInStyleRecalc ||
         layout_object_->GetDocument().Lifecycle().GetState() ==
             DocumentLifecycle::kInPerformLayout);
  return MutableForStyleRecalc(base::PassKey<PhysicalBoxFragment>(),
                               const_cast<PhysicalBoxFragment&>(*this));
}

PhysicalBoxFragment::MutableForContainerLayout::MutableForContainerLayout(
    base::PassKey<PhysicalBoxFragment>,
    PhysicalBoxFragment& fragment)
    : fragment_(fragment) {}

void PhysicalBoxFragment::MutableForContainerLayout::SetMargins(
    const PhysicalBoxStrut& margins) {
  // This can be called even without rare_data_.
  fragment_.EnsureRareField(FieldId::kMargins).margins = margins;
}

PhysicalBoxFragment::MutableForContainerLayout
PhysicalBoxFragment::GetMutableForContainerLayout() const {
  DCHECK(layout_object_->GetFrameView()->IsInPerformLayout());
  return MutableForContainerLayout(base::PassKey<PhysicalBoxFragment>(),
                                   const_cast<PhysicalBoxFragment&>(*this));
}

void PhysicalBoxFragment::MutableForOofFragmentation::AddChildFragmentainer(
    const PhysicalBoxFragment& child_fragment,
    LogicalOffset child_offset) {
  // We should only end up here when updating a nested multicol container that
  // has already being laid out, to add new fragmentainers to hold OOFs.
  DCHECK(fragment_.IsFragmentationContextRoot());
  DCHECK(child_fragment.IsFragmentainerBox());

  WritingModeConverter converter(fragment_.Style().GetWritingDirection(),
                                 fragment_.Size());
  PhysicalFragmentLink link;
  link.offset = converter.ToPhysical(child_offset, child_fragment.Size());
  link.fragment = &child_fragment;
  fragment_.children_.push_back(link);
}

void PhysicalBoxFragment::MutableForOofFragmentation::Merge(
    const PhysicalBoxFragment& placeholder_fragmentainer) {
  DCHECK(placeholder_fragmentainer.IsFragmentainerBox());

  // Copy all child fragments.
  for (const PhysicalFragmentLink& new_child :
       placeholder_fragmentainer.children_) {
    fragment_.children_.push_back(new_child);
    DCHECK(new_child->IsOutOfFlowPositioned());
    fragment_.has_out_of_flow_fragment_child_ = true;
  }

  // The existing break token may need to be updated, because of monolithic
  // overflow (printing).
  if (const BlockBreakToken* new_break_token =
          placeholder_fragmentainer.GetBreakToken()) {
    if (const BlockBreakToken* old_break_token = fragment_.GetBreakToken()) {
      old_break_token->GetMutableForOofFragmentation().Merge(*new_break_token);
    } else {
      fragment_.break_token_ = new_break_token;
    }
  }

  // Copy over any additional anchor queries.
  if (const PhysicalAnchorQuery* query =
          placeholder_fragmentainer.AnchorQuery()) {
    if (!fragment_.oof_data_) {
      fragment_.oof_data_ = MakeGarbageCollected<OofData>();
    }
    for (auto entry : *query) {
      fragment_.oof_data_->AnchorQuery().insert(entry.key, entry.value);
    }
  }

  UpdateOverflow();
}

void PhysicalBoxFragment::MutableForOofFragmentation::UpdateOverflow() {
  PhysicalRect overflow =
      ScrollableOverflowCalculator::RecalculateScrollableOverflowForFragment(
          fragment_, /* has_block_fragmentation */ true);
  fragment_.GetMutableForStyleRecalc().SetScrollableOverflow(overflow);
}

void PhysicalBoxFragment::SetInkOverflow(const PhysicalRect& self,
                                         const PhysicalRect& contents) {
  SetInkOverflowType(
      ink_overflow_.Set(InkOverflowType(), self, contents, Size()));
}

void PhysicalBoxFragment::RecalcInkOverflow(const PhysicalRect& contents) {
  const PhysicalRect self_rect = ComputeSelfInkOverflow();
  SetInkOverflow(self_rect, contents);
}

void PhysicalBoxFragment::RecalcInkOverflow() {
  DCHECK(CanUseFragmentsForInkOverflow());
  const LayoutObject* layout_object = GetSelfOrContainerLayoutObject();
  DCHECK(layout_object);
  DCHECK(
      !DisplayLockUtilities::LockedAncestorPreventingPrePaint(*layout_object));

  PhysicalRect contents_rect;
  if (!layout_object->ChildPrePaintBlockedByDisplayLock())
    contents_rect = RecalcContentsInkOverflow();
  RecalcInkOverflow(contents_rect);

  // Copy the computed values to the |OwnerBox| if |this| is the last fragment.

  // Fragmentainers may or may not have |BreakToken|s, and that
  // |CopyVisualOverflowFromFragments| cannot compute stitched coordinate for
  // them. See crbug.com/1197561.
  if (IsFragmentainerBox()) [[unlikely]] {
    return;
  }

  if (GetBreakToken()) {
    DCHECK_NE(this, &OwnerLayoutBox()->PhysicalFragments().back());
    return;
  }
  DCHECK_EQ(this, &OwnerLayoutBox()->PhysicalFragments().back());

  // We need to copy to the owner box, but |OwnerLayoutBox| should be equal to
  // |GetLayoutObject| except for column boxes, and since we early-return for
  // column boxes, |GetMutableLayoutObject| should do the work.
  DCHECK_EQ(MutableOwnerLayoutBox(), GetMutableLayoutObject());
  LayoutBox* owner_box = To<LayoutBox>(GetMutableLayoutObject());
  DCHECK(owner_box);
  DCHECK(owner_box->PhysicalFragments().Contains(*this));
  owner_box->CopyVisualOverflowFromFragments();
}

// Recalculate ink overflow of children. Returns the contents ink overflow
// for |this|.
PhysicalRect PhysicalBoxFragment::RecalcContentsInkOverflow() {
  DCHECK(GetSelfOrContainerLayoutObject());
  DCHECK(!DisplayLockUtilities::LockedAncestorPreventingPrePaint(
      *GetSelfOrContainerLayoutObject()));
  DCHECK(
      !GetSelfOrContainerLayoutObject()->ChildPrePaintBlockedByDisplayLock());

  PhysicalRect contents_rect;
  if (const FragmentItems* items = Items()) {
    InlineCursor cursor(*this, *items);
    InlinePaintContext child_inline_context;
    contents_rect = FragmentItem::RecalcInkOverflowForCursor(
        &cursor, &child_inline_context);

    // Add text decorations and emphasis mark ink over flow for combined
    // text.
    const auto* const text_combine =
        DynamicTo<LayoutTextCombine>(GetLayoutObject());
    if (text_combine) [[unlikely]] {
      // Reset the cursor for text combine to provide a current item for
      // decorations.
      InlineCursor text_combine_cursor(*this, *items);
      contents_rect.Unite(
          text_combine->RecalcContentsInkOverflow(text_combine_cursor));
    }

    // Even if this turned out to be an inline formatting context with
    // fragment items (handled above), we need to handle floating descendants.
    // If a float is block-fragmented, it is resumed as a regular box fragment
    // child, rather than becoming a fragment item.
    if (!HasFloatingDescendantsForPaint())
      return contents_rect;
  }

  for (const PhysicalFragmentLink& child : PostLayoutChildren()) {
    const auto* child_fragment = DynamicTo<PhysicalBoxFragment>(child.get());
    if (!child_fragment || child_fragment->HasSelfPaintingLayer())
      continue;
    DCHECK(!child_fragment->IsOutOfFlowPositioned());

    PhysicalRect child_rect;
    if (child_fragment->CanUseFragmentsForInkOverflow()) {
      child_fragment->GetMutableForPainting().RecalcInkOverflow();
      child_rect = child_fragment->InkOverflowRect();
    } else {
      LayoutBox* child_layout_object = child_fragment->MutableOwnerLayoutBox();
      DCHECK(child_layout_object);
      DCHECK(!child_layout_object->CanUseFragmentsForVisualOverflow());
      child_layout_object->RecalcVisualOverflow();
      // TODO(crbug.com/1144203): Reconsider this when fragment-based ink
      // overflow supports block fragmentation. Never allow flow threads to
      // propagate overflow up to a parent.
      DCHECK_EQ(child_fragment->IsColumnBox(),
                child_layout_object->IsLayoutFlowThread());
      if (child_fragment->IsColumnBox())
        continue;
      child_rect = child_layout_object->VisualOverflowRect();
    }
    child_rect.offset += child.offset;
    contents_rect.Unite(child_rect);
  }
  return contents_rect;
}

PhysicalRect PhysicalBoxFragment::ComputeSelfInkOverflow() const {
  DCHECK_EQ(PostLayout(), this);
  const ComputedStyle& style = Style();

  PhysicalRect ink_overflow(LocalRect());
  if (IsTableRow()) [[unlikely]] {
    // This is necessary because table-rows paints beyond border box if it
    // contains rowspanned cells.
    for (const PhysicalFragmentLink& child : PostLayoutChildren()) {
      const auto& child_fragment = To<PhysicalBoxFragment>(*child);
      if (!child_fragment.IsTableCell()) {
        continue;
      }
      const auto* child_layout_object =
          To<LayoutTableCell>(child_fragment.GetLayoutObject());
      if (child_layout_object->ComputedRowSpan() == 1)
        continue;
      PhysicalRect child_rect;
      if (child_fragment.CanUseFragmentsForInkOverflow())
        child_rect = child_fragment.InkOverflowRect();
      else
        child_rect = child_layout_object->VisualOverflowRect();
      child_rect.offset += child.offset;
      ink_overflow.Unite(child_rect);
    }
  }

  if (!style.HasVisualOverflowingEffect())
    return ink_overflow;

  ink_overflow.Expand(style.BoxDecorationOutsets());

  if (style.HasOutline() && IsOutlineOwner()) {
    UnionOutlineRectCollector collector;
    LayoutObject::OutlineInfo info;
    // The result rects are in coordinates of this object's border box.
    AddSelfOutlineRects(PhysicalOffset(),
                        style.OutlineRectsShouldIncludeBlockInkOverflow(),
                        collector, &info);
    PhysicalRect rect = collector.Rect();
    rect.Inflate(LayoutUnit(OutlinePainter::OutlineOutsetExtent(style, info)));
    ink_overflow.Unite(rect);
  }
  return ink_overflow;
}

#if DCHECK_IS_ON()
void PhysicalBoxFragment::InvalidateInkOverflow() {
  SetInkOverflowType(ink_overflow_.Invalidate(InkOverflowType()));
}
#endif

void PhysicalBoxFragment::AddSelfOutlineRects(
    const PhysicalOffset& additional_offset,
    OutlineType outline_type,
    OutlineRectCollector& collector,
    LayoutObject::OutlineInfo* info) const {
  if (info) {
    if (IsSvgText())
      *info = LayoutObject::OutlineInfo::GetUnzoomedFromStyle(Style());
    else
      *info = LayoutObject::OutlineInfo::GetFromStyle(Style());
  }

  if (ShouldIncludeBlockInkOverflow(outline_type) &&
      IsA<HTMLAnchorElement>(GetNode())) {
    outline_type = OutlineType::kIncludeBlockInkOverflowForAnchor;
  }

  AddOutlineRects(additional_offset, outline_type,
                  /* container_relative */ false, collector);
}

void PhysicalBoxFragment::AddOutlineRects(
    const PhysicalOffset& additional_offset,
    OutlineType outline_type,
    OutlineRectCollector& collector) const {
  AddOutlineRects(additional_offset, outline_type,
                  /* container_relative */ true, collector);
}

void PhysicalBoxFragment::AddOutlineRects(
    const PhysicalOffset& additional_offset,
    OutlineType outline_type,
    bool inline_container_relative,
    OutlineRectCollector& collector) const {
  DCHECK_EQ(PostLayout(), this);

  if (IsInlineBox()) {
    AddOutlineRectsForInlineBox(additional_offset, outline_type,
                                inline_container_relative, collector);
    return;
  }
  DCHECK(IsOutlineOwner());

  // For anonymous blocks, the children add outline rects.
  if (!IsAnonymousBlock() || GetBoxType() == kPageBorderBox) {
    if (IsSvgText()) {
      if (Items()) {
        collector.AddRect(PhysicalRect::EnclosingRect(
            GetLayoutObject()->ObjectBoundingBox()));
      }
    } else {
      collector.AddRect(PhysicalRect(additional_offset, Size()));
    }
  }

  if (ShouldIncludeBlockInkOverflow(outline_type) && !HasNonVisibleOverflow() &&
      !HasControlClip(*this)) {
    // Tricky code ahead: we pass a 0,0 additional_offset to
    // AddOutlineRectsForNormalChildren, and add it in after the call.
    // This is necessary because AddOutlineRectsForNormalChildren expects
    // additional_offset to be an offset from containing_block.
    // Since containing_block is our layout object, offset must be 0,0.
    // https://crbug.com/968019
    std::unique_ptr<OutlineRectCollector> child_collector =
        collector.ForDescendantCollector();
    AddOutlineRectsForNormalChildren(
        *child_collector, PhysicalOffset(), outline_type,
        To<LayoutBoxModelObject>(GetLayoutObject()));
    collector.Combine(child_collector.get(), additional_offset);

    if (ShouldIncludeBlockInkOverflowForAnchorOnly(outline_type)) {
      for (const auto& child : PostLayoutChildren()) {
        if (!child->IsOutOfFlowPositioned()) {
          continue;
        }

        AddOutlineRectsForDescendant(
            child, collector, additional_offset, outline_type,
            To<LayoutBoxModelObject>(GetLayoutObject()));
      }
    }
  }
  // TODO(kojii): Needs inline_element_continuation logic from
  // LayoutBlockFlow::AddOutlineRects?
}

void PhysicalBoxFragment::AddOutlineRectsForInlineBox(
    PhysicalOffset additional_offset,
    OutlineType outline_type,
    bool container_relative,
    OutlineRectCollector& collector) const {
  DCHECK_EQ(PostLayout(), this);
  DCHECK(IsInlineBox());

  const PhysicalBoxFragment* container =
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
  std::unique_ptr<OutlineRectCollector> cursor_collector =
      collector.ForDescendantCollector();
  InlineCursor cursor(*container);
  cursor.MoveTo(*layout_object);
  DCHECK(cursor);
  const PhysicalOffset this_offset_in_container =
      cursor.Current()->OffsetInContainerFragment();
#if DCHECK_IS_ON()
  bool has_this_fragment = false;
#endif
  for (; cursor; cursor.MoveToNextForSameLayoutObject()) {
    const InlineCursorPosition& current = cursor.Current();
#if DCHECK_IS_ON()
    has_this_fragment = has_this_fragment || current.BoxFragment() == this;
#endif
    if (!current.Size().IsZero()) {
      const PhysicalBoxFragment* fragment = current.BoxFragment();
      DCHECK(fragment);
      if (!fragment->IsOpaque() && !fragment->IsSvg()) {
        cursor_collector->AddRect(current.RectInContainerFragment());
      }
    }

    // Add descendants if any, in the container-relative coordinate.
    if (!current.HasChildren())
      continue;
    InlineCursor descendants = cursor.CursorForDescendants();
    AddOutlineRectsForCursor(*cursor_collector, PhysicalOffset(), outline_type,
                             layout_object, &descendants);
  }
#if DCHECK_IS_ON()
  DCHECK(has_this_fragment);
#endif
  // TODO(vmpstr): Is this correct? Should AddOutlineRectsForDescendants below
  // be skipped?
  if (cursor_collector->IsEmpty()) {
    return;
  }

  // At this point, |rects| are in the container coordinate space.
  // Adjust the rectangles using |additional_offset| and |container_relative|.
  if (!container_relative)
    additional_offset -= this_offset_in_container;
  collector.Combine(cursor_collector.get(), additional_offset);

  if (ShouldIncludeBlockInkOverflowForAnchorOnly(outline_type) &&
      !HasNonVisibleOverflow() && !HasControlClip(*this)) {
    for (const auto& child : container->PostLayoutChildren()) {
      if (!child->IsOutOfFlowPositioned() ||
          child->GetLayoutObject()->ContainerForAbsolutePosition() !=
              layout_object) {
        continue;
      }

      AddOutlineRectsForDescendant(child, collector, additional_offset,
                                   outline_type, layout_object);
    }
  }
}

PositionWithAffinity PhysicalBoxFragment::PositionForPoint(
    PhysicalOffset point) const {
  if (layout_object_->IsBox() && !layout_object_->IsLayoutNGObject()) {
    // Layout engine boundary. Enter legacy PositionForPoint().
    return layout_object_->PositionForPoint(point);
  }

  const PhysicalOffset point_in_contents =
      IsScrollContainer()
          ? point + PhysicalOffset(PixelSnappedScrolledContentOffset())
          : point;

  if (!layout_object_->ChildPaintBlockedByDisplayLock()) {
    if (const FragmentItems* items = Items()) {
      InlineCursor cursor(*this, *items);
      if (const PositionWithAffinity position =
              cursor.PositionForPointInInlineFormattingContext(
                  point_in_contents, *this))
        return AdjustForEditingBoundary(position);
      return layout_object_->CreatePositionWithAffinity(0);
    }
  }

  if (IsA<LayoutBlockFlow>(*layout_object_) &&
      layout_object_->ChildrenInline()) {
    // Here |this| may have out-of-flow children without inline children, we
    // don't find closest child of |point| for out-of-flow children.
    // See WebFrameTest.SmartClipData
    return layout_object_->CreatePositionWithAffinity(0);
  }

  if (layout_object_->IsTable())
    return PositionForPointInTable(point_in_contents);

  if (ShouldUsePositionForPointInBlockFlowDirection(*layout_object_))
    return PositionForPointInBlockFlowDirection(point_in_contents);

  return PositionForPointByClosestChild(point_in_contents);
}

PositionWithAffinity PhysicalBoxFragment::PositionForPointByClosestChild(
    PhysicalOffset point_in_contents) const {
  if (layout_object_->ChildPaintBlockedByDisplayLock()) {
    // If this node is DisplayLocked, then Children() will have invalid layout
    // information.
    return AdjustForEditingBoundary(
        FirstPositionInOrBeforeNode(*layout_object_->GetNode()));
  }

  PhysicalFragmentLink closest_child = {nullptr};
  LayoutUnit shortest_distance = LayoutUnit::Max();
  bool found_hit_test_candidate = false;
  const PhysicalSize pixel_size(LayoutUnit(1), LayoutUnit(1));
  const PhysicalRect point_rect(point_in_contents, pixel_size);

  // This is a general-purpose algorithm for finding the nearest child. There
  // may be cases where want to introduce specialized algorithms that e.g. takes
  // the progression direction into account (so that we can break earlier, or
  // even add special behavior). Children in block containers progress in the
  // block direction, for instance, while table cells progress in the inline
  // direction. Flex containers may progress in the inline direction, reverse
  // inline direction, block direction or reverse block direction. Multicol
  // containers progress both in the inline direction (columns) and block
  // direction (column rows and spanners).
  for (const PhysicalFragmentLink& child : Children()) {
    const auto& box_fragment = To<PhysicalBoxFragment>(*child.fragment);
    bool is_hit_test_candidate = IsHitTestCandidate(box_fragment);
    if (!is_hit_test_candidate) {
      if (found_hit_test_candidate)
        continue;
      // We prefer valid hit-test candidates, but if there are no such children,
      // we'll lower our requirements somewhat. The exact reasoning behind the
      // details here is unknown, but it is something that evolved during
      // WebKit's early years.
      if (box_fragment.Style().UsedVisibility() != EVisibility::kVisible ||
          (box_fragment.Children().empty() && !box_fragment.IsBlockFlow())) {
        continue;
      }
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
    return layout_object_->FirstPositionInOrBeforeThis();
  return To<PhysicalBoxFragment>(*closest_child)
      .PositionForPoint(point_in_contents - closest_child.offset);
}

PositionWithAffinity PhysicalBoxFragment::PositionForPointInBlockFlowDirection(
    PhysicalOffset point_in_contents) const {
  // Note: Children of <table> and "columns" are not laid out in block flow
  // direction.
  DCHECK(!layout_object_->IsTable()) << this;
  DCHECK(ShouldUsePositionForPointInBlockFlowDirection(*layout_object_))
      << this;

  if (layout_object_->ChildPaintBlockedByDisplayLock()) {
    // If this node is DisplayLocked, then Children() will have invalid layout
    // information.
    return AdjustForEditingBoundary(
        FirstPositionInOrBeforeNode(*layout_object_->GetNode()));
  }

  const bool blocks_are_flipped = Style().IsFlippedBlocksWritingMode();
  WritingModeConverter converter(Style().GetWritingDirection(), Size());
  const LogicalOffset logical_point_in_contents =
      converter.ToLogical(point_in_contents, PhysicalSize());

  // Loop over block children to find a child logically below
  // |point_in_contents|.
  const PhysicalFragmentLink* last_candidate_box = nullptr;
  for (const PhysicalFragmentLink& child : Children()) {
    const auto& box_fragment = To<PhysicalBoxFragment>(*child.fragment);
    if (!IsHitTestCandidate(box_fragment))
      continue;
    // We hit child if our click is above the bottom of its padding box (like
    // IE6/7 and FF3).
    const LogicalRect logical_child_rect =
        converter.ToLogical(PhysicalRect(child.offset, box_fragment.Size()));
    if (logical_point_in_contents.block_offset <
            logical_child_rect.BlockEndOffset() ||
        (blocks_are_flipped && logical_point_in_contents.block_offset ==
                                   logical_child_rect.BlockEndOffset())) {
      // |child| is logically below |point_in_contents|.
      return PositionForPointRespectingEditingBoundaries(
          To<PhysicalBoxFragment>(*child.fragment),
          point_in_contents - child.offset);
    }

    // |last_candidate_box| is logical above |point_in_contents|.
    last_candidate_box = &child;
  }

  // Here all children are logically above |point_in_contents|.
  if (last_candidate_box) {
    // editing/selection/block-with-positioned-lastchild.html reaches here.
    return PositionForPointRespectingEditingBoundaries(
        To<PhysicalBoxFragment>(*last_candidate_box->fragment),
        point_in_contents - last_candidate_box->offset);
  }

  // We only get here if there are no hit test candidate children below the
  // click.
  return PositionForPointByClosestChild(point_in_contents);
}

PositionWithAffinity PhysicalBoxFragment::PositionForPointInTable(
    PhysicalOffset point_in_contents) const {
  DCHECK(layout_object_->IsTable()) << this;
  if (!layout_object_->NonPseudoNode())
    return PositionForPointByClosestChild(point_in_contents);

  // Adjust for writing-mode:vertical-rl
  const LayoutUnit adjusted_left = Style().IsFlippedBlocksWritingMode()
                                       ? Size().width - point_in_contents.left
                                       : point_in_contents.left;
  if (adjusted_left < 0 || adjusted_left > Size().width ||
      point_in_contents.top < 0 || point_in_contents.top > Size().height) {
    // |point_in_contents| is outside of <table>.
    // See editing/selection/click-before-and-after-table.html
    if (adjusted_left <= Size().width / 2)
      return layout_object_->FirstPositionInOrBeforeThis();
    return layout_object_->LastPositionInOrAfterThis();
  }

  return PositionForPointByClosestChild(point_in_contents);
}

PositionWithAffinity
PhysicalBoxFragment::PositionForPointRespectingEditingBoundaries(
    const PhysicalBoxFragment& child,
    PhysicalOffset point_in_child) const {
  Node* const child_node = child.NonPseudoNode();
  if (!child.IsCSSBox() || !child_node)
    return child.PositionForPoint(point_in_child);

  // First make sure that the editability of the parent and child agree.
  // TODO(layout-dev): Could we just walk the DOM tree instead here?
  const LayoutObject* ancestor = layout_object_;
  while (ancestor && !ancestor->NonPseudoNode())
    ancestor = ancestor->Parent();
  if (!ancestor || !ancestor->Parent() ||
      (ancestor->HasLayer() && ancestor->Parent()->IsLayoutView()) ||
      IsEditable(*ancestor->NonPseudoNode()) == IsEditable(*child_node)) {
    return child.PositionForPoint(point_in_child);
  }

  // If editiability isn't the same in the ancestor and the child, then we
  // return a visible position just before or after the child, whichever side is
  // closer.
  WritingModeConverter converter(child.Style().GetWritingDirection(),
                                 child.Size());
  const LogicalOffset logical_point_in_child =
      converter.ToLogical(point_in_child, PhysicalSize());
  const LayoutUnit logical_child_inline_size =
      converter.ToLogical(child.Size()).inline_size;
  if (logical_point_in_child.inline_offset < logical_child_inline_size / 2)
    return child.GetLayoutObject()->PositionBeforeThis();
  return child.GetLayoutObject()->PositionAfterThis();
}

PhysicalBoxStrut PhysicalBoxFragment::OverflowClipMarginOutsets() const {
  DCHECK(Style().OverflowClipMargin());
  DCHECK(ShouldApplyOverflowClipMargin());
  DCHECK(!IsScrollContainer());

  const auto& overflow_clip_margin = Style().OverflowClipMargin();
  PhysicalBoxStrut outsets;

  // First inset the overflow rect based on the reference box. The
  // |child_overflow_rect| initialized above assumes clipping to
  // border-box.
  switch (overflow_clip_margin->GetReferenceBox()) {
    case StyleOverflowClipMargin::ReferenceBox::kBorderBox:
      break;
    case StyleOverflowClipMargin::ReferenceBox::kPaddingBox:
      outsets -= Borders();
      break;
    case StyleOverflowClipMargin::ReferenceBox::kContentBox:
      outsets -= Borders();
      outsets -= Padding();
      break;
  }

  // Now expand the rect based on the given margin. The margin only
  // applies if the side is a painted with this child fragment.
  outsets += PhysicalBoxStrut(overflow_clip_margin->GetMargin());
  outsets.TruncateSides(SidesToInclude());

  return outsets;
}

#if DCHECK_IS_ON()
PhysicalBoxFragment::AllowPostLayoutScope::AllowPostLayoutScope() {
  ++allow_count_;
}

PhysicalBoxFragment::AllowPostLayoutScope::~AllowPostLayoutScope() {
  DCHECK(allow_count_);
  --allow_count_;
}

void PhysicalBoxFragment::CheckSameForSimplifiedLayout(
    const PhysicalBoxFragment& other,
    bool check_same_block_size,
    bool check_no_fragmentation) const {
  DCHECK_EQ(layout_object_, other.layout_object_);

  LogicalSize size = size_.ConvertToLogical(Style().GetWritingMode());
  LogicalSize other_size =
      other.size_.ConvertToLogical(Style().GetWritingMode());
  DCHECK_EQ(size.inline_size, other_size.inline_size);
  if (check_same_block_size)
    DCHECK_EQ(size.block_size, other_size.block_size);

  if (check_no_fragmentation) {
    // "simplified" layout doesn't work within a fragmentation context.
    DCHECK(!break_token_ && !other.break_token_);
  }

  DCHECK_EQ(type_, other.type_);
  DCHECK_EQ(sub_type_, other.sub_type_);
  DCHECK_EQ(style_variant_, other.style_variant_);
  DCHECK_EQ(is_hidden_for_paint_, other.is_hidden_for_paint_);
  DCHECK_EQ(is_opaque_, other.is_opaque_);
  DCHECK_EQ(is_block_in_inline_, other.is_block_in_inline_);
  DCHECK_EQ(is_math_fraction_, other.is_math_fraction_);
  DCHECK_EQ(is_math_operator_, other.is_math_operator_);

  // |has_floating_descendants_for_paint_| can change during simplified layout.
  DCHECK_EQ(has_adjoining_object_descendants_,
            other.has_adjoining_object_descendants_);
  DCHECK_EQ(may_have_descendant_above_block_start_,
            other.may_have_descendant_above_block_start_);
  DCHECK_EQ(bit_field_.get<HasDescendantsForTablePartFlag>(),
            other.bit_field_.get<HasDescendantsForTablePartFlag>());
  DCHECK_EQ(IsFragmentationContextRoot(), other.IsFragmentationContextRoot());

  // `depends_on_percentage_block_size_` can change within out-of-flow
  // simplified layout (a different position-try rule can be selected).
  if (!IsOutOfFlowPositioned()) {
    DCHECK_EQ(depends_on_percentage_block_size_,
              other.depends_on_percentage_block_size_);
  }

  DCHECK_EQ(is_fieldset_container_, other.is_fieldset_container_);
  DCHECK_EQ(is_table_part_, other.is_table_part_);
  DCHECK_EQ(is_painted_atomically_, other.is_painted_atomically_);
  DCHECK_EQ(has_collapsed_borders_, other.has_collapsed_borders_);

  DCHECK_EQ(HasItems(), other.HasItems());
  DCHECK_EQ(IsInlineFormattingContext(), other.IsInlineFormattingContext());
  DCHECK_EQ(IncludeBorderTop(), other.IncludeBorderTop());
  DCHECK_EQ(IncludeBorderRight(), other.IncludeBorderRight());
  DCHECK_EQ(IncludeBorderBottom(), other.IncludeBorderBottom());
  DCHECK_EQ(IncludeBorderLeft(), other.IncludeBorderLeft());

  // The oof_positioned_descendants_ vector can change during "simplified"
  // layout. This occurs when an OOF-descendant changes from "fixed" to
  // "absolute" (or visa versa) changing its containing block.

  DCHECK(FirstBaseline() == other.FirstBaseline());
  DCHECK(LastBaseline() == other.LastBaseline());

  if (IsTable()) {
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

  if (IsTableCell()) {
    DCHECK_EQ(TableCellColumnIndex(), other.TableCellColumnIndex());
  }

  DCHECK(Borders() == other.Borders());
  DCHECK(Padding() == other.Padding());
  // NOTE: The |InflowBounds| can change if scrollbars are added/removed.
}

// Check our flags represent the actual children correctly.
void PhysicalBoxFragment::CheckIntegrity() const {
  bool has_inflow_blocks = false;
  bool has_inlines = false;
  bool has_line_boxes = false;
  bool has_floats = false;
  bool has_list_markers = false;

  for (const PhysicalFragmentLink& child : Children()) {
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
  if (has_line_boxes || has_inlines) {
    DCHECK(IsInlineFormattingContext());
  }

  // If display-locked, we may not have any children.
  DCHECK(layout_object_);
  if (layout_object_ && layout_object_->ChildPaintBlockedByDisplayLock())
    return;

  if (has_line_boxes) {
    DCHECK(HasItems());
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

void PhysicalBoxFragment::AssertFragmentTreeSelf() const {
  DCHECK(!IsInlineBox());
  DCHECK(OwnerLayoutBox());
  DCHECK_EQ(this, PostLayout());
}

void PhysicalBoxFragment::AssertFragmentTreeChildren(
    bool allow_destroyed_or_moved) const {
  if (const FragmentItems* items = Items()) {
    for (InlineCursor cursor(*this, *items); cursor; cursor.MoveToNext()) {
      const FragmentItem& item = *cursor.Current();
      if (item.IsLayoutObjectDestroyedOrMoved()) {
        DCHECK(allow_destroyed_or_moved);
        continue;
      }
      if (const auto* box = item.BoxFragment()) {
        DCHECK(!box->IsLayoutObjectDestroyedOrMoved());
        if (!box->IsInlineBox())
          box->AssertFragmentTreeSelf();
      }
    }
  }

  for (const PhysicalFragmentLink& child : Children()) {
    if (child->IsLayoutObjectDestroyedOrMoved()) {
      DCHECK(allow_destroyed_or_moved);
      continue;
    }
    if (const auto* box =
            DynamicTo<PhysicalBoxFragment>(child.fragment.Get())) {
      box->AssertFragmentTreeSelf();
    }
  }
}
#endif

void PhysicalBoxFragment::TraceAfterDispatch(Visitor* visitor) const {
  visitor->Trace(children_);
  visitor->Trace(rare_data_);
  // |HasItems()| and |ConstHasRareData()| are const and set
  // in ctor so they do not cause TOCTOU.
  if (HasItems())
    visitor->Trace(*ComputeItemsAddress());
  PhysicalFragment::TraceAfterDispatch(visitor);
}

}  // namespace blink
