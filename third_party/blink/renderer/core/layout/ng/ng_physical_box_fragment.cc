// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"

#include "build/chromeos_buildflags.h"
#include "third_party/blink/renderer/core/display_lock/display_lock_utilities.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/position_with_affinity.h"
#include "third_party/blink/renderer/core/editing/text_affinity.h"
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
#include "third_party/blink/renderer/core/layout/ng/ng_block_break_token.h"
#include "third_party/blink/renderer/core/layout/ng/ng_box_fragment_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_disable_side_effects_scope.h"
#include "third_party/blink/renderer/core/layout/ng/ng_outline_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_relative_utils.h"
#include "third_party/blink/renderer/core/layout/table/layout_table_cell.h"
#include "third_party/blink/renderer/core/paint/ng/ng_inline_paint_context.h"
#include "third_party/blink/renderer/core/paint/outline_painter.h"
#include "third_party/blink/renderer/platform/wtf/size_assertions.h"

namespace blink {

#if DCHECK_IS_ON()
unsigned NGPhysicalBoxFragment::AllowPostLayoutScope::allow_count_ = 0;
#endif

namespace {

struct SameSizeAsNGPhysicalBoxFragment : NGPhysicalFragment {
  unsigned flags;
  LayoutUnit baseline;
  LayoutUnit last_baseline;
  Member<void*> rare;
  NGInkOverflow ink_overflow;
  HeapVector<NGLink> children;
};

ASSERT_SIZE(NGPhysicalBoxFragment, SameSizeAsNGPhysicalBoxFragment);

bool HasControlClip(const NGPhysicalBoxFragment& self) {
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

inline bool IsHitTestCandidate(const NGPhysicalBoxFragment& fragment) {
  return fragment.Size().height &&
         fragment.Style().Visibility() == EVisibility::kVisible &&
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

NGContainingBlock<PhysicalOffset> PhysicalContainingBlock(
    NGFragmentBuilder* builder,
    PhysicalSize outer_size,
    PhysicalSize inner_size,
    const NGContainingBlock<LogicalOffset>& containing_block) {
  return NGContainingBlock<PhysicalOffset>(
      containing_block.Offset().ConvertToPhysical(
          builder->Style().GetWritingDirection(), outer_size, inner_size),
      RelativeInsetToPhysical(containing_block.RelativeOffset(),
                              builder->Style().GetWritingDirection()),
      containing_block.Fragment(),
      containing_block.ClippedContainerBlockOffset(),
      containing_block.IsInsideColumnSpanner(),
      containing_block.RequiresContentBeforeBreaking());
}

NGContainingBlock<PhysicalOffset> PhysicalContainingBlock(
    NGFragmentBuilder* builder,
    PhysicalSize size,
    const NGContainingBlock<LogicalOffset>& containing_block) {
  PhysicalSize containing_block_size =
      containing_block.Fragment() ? containing_block.Fragment()->Size() : size;
  return PhysicalContainingBlock(builder, size, containing_block_size,
                                 containing_block);
}

}  // namespace

// static
const NGPhysicalBoxFragment* NGPhysicalBoxFragment::Create(
    NGBoxFragmentBuilder* builder,
    WritingMode block_or_line_writing_mode) {
  const auto writing_direction = builder->GetWritingDirection();
  const PhysicalBoxStrut borders =
      builder->initial_fragment_geometry_->border.ConvertToPhysical(
          writing_direction);
  bool has_borders = !borders.IsZero();
  const PhysicalBoxStrut padding =
      builder->initial_fragment_geometry_->padding.ConvertToPhysical(
          writing_direction);
  bool has_padding = !padding.IsZero();

  const PhysicalSize physical_size =
      ToPhysicalSize(builder->Size(), builder->GetWritingMode());
  WritingModeConverter converter(writing_direction, physical_size);

  absl::optional<PhysicalRect> inflow_bounds;
  if (builder->inflow_bounds_)
    inflow_bounds = converter.ToPhysical(*builder->inflow_bounds_);

#if DCHECK_IS_ON()
  if (builder->needs_inflow_bounds_explicitly_set_ && builder->node_ &&
      builder->node_.IsScrollContainer() && !builder->IsFragmentainerBoxType())
    DCHECK(builder->is_inflow_bounds_explicitly_set_);
  if (builder->needs_may_have_descendant_above_block_start_explicitly_set_)
    DCHECK(builder->is_may_have_descendant_above_block_start_explicitly_set_);
#endif

  PhysicalRect layout_overflow = {PhysicalOffset(), physical_size};
  if (builder->node_ && !builder->node_.IsReplaced()) {
    const PhysicalBoxStrut scrollbar =
        builder->initial_fragment_geometry_->scrollbar.ConvertToPhysical(
            writing_direction);
    NGLayoutOverflowCalculator calculator(
        To<NGBlockNode>(builder->node_),
        /* is_css_box */ !builder->IsFragmentainerBoxType(),
        builder->ConstraintSpace().HasBlockFragmentation(), borders, scrollbar,
        padding, physical_size, writing_direction);

    if (FragmentItemsBuilder* items_builder = builder->ItemsBuilder()) {
      calculator.AddItems(builder->GetLayoutObject(),
                          items_builder->Items(physical_size));
    }

    for (auto& child : builder->children_) {
      const auto* box_fragment =
          DynamicTo<NGPhysicalBoxFragment>(*child.fragment);
      if (!box_fragment)
        continue;

      calculator.AddChild(*box_fragment, child.offset.ConvertToPhysical(
                                             writing_direction, physical_size,
                                             box_fragment->Size()));
    }

    if (builder->table_collapsed_borders_)
      calculator.AddTableSelfRect();

    layout_overflow = calculator.Result(inflow_bounds);
  }

  // For the purposes of object allocation we have layout-overflow if it
  // differs from the fragment size.
  bool has_layout_overflow = layout_overflow != PhysicalRect({}, physical_size);

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
  // The initialization of the array is done by NGPhysicalFragment;
  // we pass the buffer as a constructor argument.
  return MakeGarbageCollected<NGPhysicalBoxFragment>(
      AdditionalBytes(byte_size), PassKey(), builder, has_layout_overflow,
      layout_overflow, has_borders, borders, has_padding, padding,
      inflow_bounds, has_fragment_items, block_or_line_writing_mode);
}

// static
const NGPhysicalBoxFragment* NGPhysicalBoxFragment::Clone(
    const NGPhysicalBoxFragment& other) {
  // The size of the new fragment shouldn't differ from the old one.
  size_t byte_size = AdditionalByteSize(other.HasItems());

  return MakeGarbageCollected<NGPhysicalBoxFragment>(
      AdditionalBytes(byte_size), PassKey(), other, other.HasLayoutOverflow(),
      other.LayoutOverflow());
}

// static
const NGPhysicalBoxFragment*
NGPhysicalBoxFragment::CloneWithPostLayoutFragments(
    const NGPhysicalBoxFragment& other) {
  PhysicalRect layout_overflow = other.LayoutOverflow();
  bool has_layout_overflow = other.HasLayoutOverflow();

  // The size of the new fragment shouldn't differ from the old one.
  size_t byte_size = AdditionalByteSize(other.HasItems());

  const auto* cloned_fragment = MakeGarbageCollected<NGPhysicalBoxFragment>(
      AdditionalBytes(byte_size), PassKey(), other, has_layout_overflow,
      layout_overflow);

  // To ensure the fragment tree is consistent, use the post-layout fragment.
#if DCHECK_IS_ON()
  AllowPostLayoutScope allow_post_layout_scope;
#endif

  for (NGLink& child : cloned_fragment->GetMutableForCloning().Children()) {
    child.fragment = child->PostLayout();
    DCHECK(child.fragment);

    if (!child->IsFragmentainerBox())
      continue;

    // Fragmentainers don't have the concept of post-layout fragments, so if
    // this is a fragmentation context root (such as a multicol container), we
    // need to not only update its children, but also the children of the
    // children that are fragmentainers.
    auto& fragmentainer = *To<NGPhysicalBoxFragment>(child.fragment.Get());
    for (NGLink& fragmentainer_child :
         fragmentainer.GetMutableForCloning().Children()) {
      auto& old_child =
          *To<NGPhysicalBoxFragment>(fragmentainer_child.fragment.Get());
      fragmentainer_child.fragment = old_child.PostLayout();
    }
  }

  if (cloned_fragment->HasItems()) {
    // Replace box fragment items with post layout fragments.
    for (const auto& cloned_item : cloned_fragment->Items()->Items()) {
      const NGPhysicalBoxFragment* box = cloned_item.BoxFragment();
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
size_t NGPhysicalBoxFragment::AdditionalByteSize(bool has_fragment_items) {
  size_t additional_size = 0;
  if (has_fragment_items) {
    AccountSizeAndPadding<FragmentItems>(additional_size);
  }
  return additional_size;
}

NGPhysicalBoxFragment::NGPhysicalBoxFragment(
    PassKey key,
    NGBoxFragmentBuilder* builder,
    bool has_layout_overflow,
    const PhysicalRect& layout_overflow,
    bool has_borders,
    const PhysicalBoxStrut& borders,
    bool has_padding,
    const PhysicalBoxStrut& padding,
    const absl::optional<PhysicalRect>& inflow_bounds,
    bool has_fragment_items,
    WritingMode block_or_line_writing_mode)
    : NGPhysicalFragment(builder,
                         block_or_line_writing_mode,
                         kFragmentBox,
                         builder->BoxType()),
      bit_field_(ConstHasFragmentItemsFlag::encode(has_fragment_items) |
                 HasDescendantsForTablePartFlag::encode(false) |
                 IsFragmentationContextRootFlag::encode(
                     builder->is_fragmentation_context_root_) |
                 IsMonolithicFlag::encode(builder->is_monolithic_)) {
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
    absl::optional<PhysicalSize> new_size =
        items_builder->ToFragmentItems(Size(), items);
    if (new_size)
      size_ = *new_size;
  }

  SetInkOverflowType(NGInkOverflow::Type::kNotSet);

  wtf_size_t rare_fields_size =
      has_layout_overflow + !!builder->frame_set_layout_data_ +
      !!builder->mathml_paint_info_ + !!builder->table_grid_rect_ +
      !!builder->table_collapsed_borders_ +
      !!builder->table_collapsed_borders_geometry_ +
      !!builder->table_cell_column_index_ +
      (builder->table_section_row_offsets_.empty() ? 0 : 2) +
      !!builder->page_name_ + has_borders + has_padding +
      inflow_bounds.has_value();
  if (RuntimeEnabledFeatures::LayoutNGNoCopyBackEnabled()) {
    rare_fields_size += !!builder->Style().MayHaveMargin();
  }

  if (rare_fields_size > 0 || !builder->table_column_geometries_.empty()) {
    rare_data_ = MakeGarbageCollected<PhysicalFragmentRareData>(
        has_layout_overflow ? &layout_overflow : nullptr,
        has_borders ? &borders : nullptr, has_padding ? &padding : nullptr,
        inflow_bounds, *builder, rare_fields_size);
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

NGPhysicalBoxFragment::NGPhysicalBoxFragment(
    PassKey key,
    const NGPhysicalBoxFragment& other,
    bool has_layout_overflow,
    const PhysicalRect& layout_overflow)
    : NGPhysicalFragment(other),
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

NGPhysicalBoxFragment::~NGPhysicalBoxFragment() {
  // Note: This function may not always be called because the dtor of
  // NGPhysicalFragment is made non-virtual for memory efficiency.
  SetInkOverflowType(ink_overflow_.Reset(InkOverflowType()));
}

void NGPhysicalBoxFragment::Dispose() {
  if (HasInkOverflow())
    SetInkOverflowType(ink_overflow_.Reset(InkOverflowType()));
  if (HasItems())
    ComputeItemsAddress()->~FragmentItems();
}

// TODO(kojii): Move to ng_physical_fragment.cc
NGPhysicalFragment::OutOfFlowData*
NGPhysicalFragment::FragmentedOutOfFlowDataFromBuilder(
    NGFragmentBuilder* builder) {
  DCHECK(has_fragmented_out_of_flow_data_);
  DCHECK_EQ(has_fragmented_out_of_flow_data_,
            !builder->oof_positioned_fragmentainer_descendants_.empty() ||
                !builder->multicols_with_pending_oofs_.empty());
  NGFragmentedOutOfFlowData* fragmented_data =
      MakeGarbageCollected<NGFragmentedOutOfFlowData>();
  fragmented_data->oof_positioned_fragmentainer_descendants.reserve(
      builder->oof_positioned_fragmentainer_descendants_.size());
  const PhysicalSize& size = Size();
  WritingDirectionMode writing_direction = builder->GetWritingDirection();
  const WritingModeConverter converter(writing_direction, size);
  for (const auto& descendant :
       builder->oof_positioned_fragmentainer_descendants_) {
    NGInlineContainer<PhysicalOffset> inline_container(
        descendant.inline_container.container,
        converter.ToPhysical(descendant.inline_container.relative_offset,
                             PhysicalSize()));
    NGInlineContainer<PhysicalOffset> fixedpos_inline_container(
        descendant.fixedpos_inline_container.container,
        converter.ToPhysical(
            descendant.fixedpos_inline_container.relative_offset,
            PhysicalSize()));

    // The static position should remain relative to the containing block.
    PhysicalSize containing_block_size =
        descendant.containing_block.Fragment()
            ? descendant.containing_block.Fragment()->Size()
            : size;
    const WritingModeConverter containing_block_converter(
        writing_direction, containing_block_size);

    fragmented_data->oof_positioned_fragmentainer_descendants.emplace_back(
        descendant.Node(),
        descendant.static_position.ConvertToPhysical(
            containing_block_converter),
        inline_container,
        PhysicalContainingBlock(builder, size, containing_block_size,
                                descendant.containing_block),
        PhysicalContainingBlock(builder, size,
                                descendant.fixedpos_containing_block),
        fixedpos_inline_container);
  }
  for (const auto& multicol : builder->multicols_with_pending_oofs_) {
    auto& value = multicol.value;
    NGInlineContainer<PhysicalOffset> fixedpos_inline_container(
        value->fixedpos_inline_container.container,
        converter.ToPhysical(value->fixedpos_inline_container.relative_offset,
                             PhysicalSize()));
    fragmented_data->multicols_with_pending_oofs.insert(
        multicol.key,
        MakeGarbageCollected<NGMulticolWithPendingOOFs<PhysicalOffset>>(
            value->multicol_offset.ConvertToPhysical(
                builder->Style().GetWritingDirection(), size, PhysicalSize()),
            PhysicalContainingBlock(builder, size,
                                    value->fixedpos_containing_block),
            fixedpos_inline_container));
  }
  return fragmented_data;
}

const LayoutBox* NGPhysicalBoxFragment::OwnerLayoutBox() const {
  // TODO(layout-dev): We should probably get rid of this method, now that it
  // does nothing, apart from some checking. The checks are useful, but could be
  // moved elsewhere.
  const LayoutBox* owner_box =
      DynamicTo<LayoutBox>(GetSelfOrContainerLayoutObject());

#if DCHECK_IS_ON()
  DCHECK(owner_box);
  if (UNLIKELY(IsFragmentainerBox())) {
    if (owner_box->IsLayoutView()) {
      DCHECK(IsPageBox());
      DCHECK(To<LayoutView>(owner_box)->ShouldUsePrintingLayout());
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

LayoutBox* NGPhysicalBoxFragment::MutableOwnerLayoutBox() const {
  return const_cast<LayoutBox*>(OwnerLayoutBox());
}

PhysicalOffset NGPhysicalBoxFragment::OffsetFromOwnerLayoutBox() const {
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

const NGPhysicalBoxFragment* NGPhysicalBoxFragment::PostLayout() const {
  // While side effects are disabled, new fragments are not copied to
  // |LayoutBox|. Just return the given fragment.
  if (NGDisableSideEffectsScope::IsDisabled())
    return this;

  const auto* layout_object = GetSelfOrContainerLayoutObject();
  if (UNLIKELY(!layout_object)) {
    // In some cases the layout object may have been removed. This can of course
    // not happen if we have actually performed layout, but we may in some cases
    // clone a fragment *before* layout, to ensure that the fragment tree spine
    // is correctly rebuilt after a subtree layout.
    return this;
  }
  const auto* box = DynamicTo<LayoutBox>(layout_object);
  if (UNLIKELY(!box)) {
    DCHECK(IsInlineBox());
    return this;
  }
  if (UNLIKELY(!IsCSSBox())) {
    // We don't need to do anything special for fragments that don't correspond
    // to entries in the CSS box tree (such as fragmentainers). Any post-layout
    // fragmentainers should be found as children of the post-layout fragments
    // of the containing block.
    //
    // TODO(mstensho): Clean up this method. Rather than calling
    // GetSelfOrContainerLayoutObject() above, we first bail on !IsCSSBox(), and
    // then simply use GetLayoutObject().
    return this;
  }

  const wtf_size_t fragment_count = box->PhysicalFragmentCount();
  if (UNLIKELY(fragment_count == 0)) {
#if DCHECK_IS_ON()
    DCHECK(AllowPostLayoutScope::IsAllowed());
#endif
    return nullptr;
  }

  const NGPhysicalBoxFragment* post_layout = nullptr;
  if (fragment_count == 1) {
    post_layout = box->GetPhysicalFragment(0);
    DCHECK(post_layout);
  } else if (const auto* break_token = BreakToken()) {
    const unsigned index = break_token->SequenceNumber();
    if (index < fragment_count) {
      post_layout = box->GetPhysicalFragment(index);
      DCHECK(post_layout);
      DCHECK(!post_layout->BreakToken() ||
             post_layout->BreakToken()->SequenceNumber() == index);
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

PhysicalRect NGPhysicalBoxFragment::SelfInkOverflow() const {
  if (UNLIKELY(!CanUseFragmentsForInkOverflow())) {
    const auto* owner_box = DynamicTo<LayoutBox>(GetLayoutObject());
    return owner_box->SelfVisualOverflowRect();
  }
  if (!HasInkOverflow())
    return LocalRect();
  return ink_overflow_.Self(InkOverflowType(), Size());
}

PhysicalRect NGPhysicalBoxFragment::ContentsInkOverflow() const {
  if (UNLIKELY(!CanUseFragmentsForInkOverflow())) {
    const auto* owner_box = DynamicTo<LayoutBox>(GetLayoutObject());
    return owner_box->ContentsVisualOverflowRect();
  }
  if (!HasInkOverflow())
    return LocalRect();
  return ink_overflow_.Contents(InkOverflowType(), Size());
}

PhysicalRect NGPhysicalBoxFragment::InkOverflow() const {
  if (UNLIKELY(!CanUseFragmentsForInkOverflow())) {
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

PhysicalRect NGPhysicalBoxFragment::OverflowClipRect(
    const PhysicalOffset& location,
    OverlayScrollbarClipBehavior overlay_scrollbar_clip_behavior) const {
  DCHECK(GetLayoutObject() && GetLayoutObject()->IsBox());
  const LayoutBox* box = To<LayoutBox>(GetLayoutObject());
  return box->OverflowClipRect(location, overlay_scrollbar_clip_behavior);
}

PhysicalRect NGPhysicalBoxFragment::OverflowClipRect(
    const PhysicalOffset& location,
    const NGBlockBreakToken* incoming_break_token,
    OverlayScrollbarClipBehavior overlay_scrollbar_clip_behavior) const {
  PhysicalRect clip_rect =
      OverflowClipRect(location, overlay_scrollbar_clip_behavior);
  if (!incoming_break_token && !BreakToken())
    return clip_rect;

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
  // TODO(kojii): Scrollable overflow is computed after layout, and that the
  // tree needs to be consistent, except for Ruby where it is computed during
  // layout. It might be that |ComputeAnnotationOverflow| should move to layout
  // overflow recalc, but it is to be thought out.
  DCHECK(height_type == TextHeightType::kEmHeight || PostLayout() == this);
  if (UNLIKELY(IsLayoutObjectDestroyedOrMoved())) {
    NOTREACHED();
    return PhysicalRect();
  }
  const LayoutObject* layout_object = GetLayoutObject();
  if (height_type == TextHeightType::kEmHeight && IsRubyBox()) {
    return ScrollableOverflowFromChildren(height_type);
  }
  if (const auto* layout_box = DynamicTo<LayoutBox>(layout_object)) {
    if (HasNonVisibleOverflow())
      return PhysicalRect({}, Size());
    // Legacy is the source of truth for overflow
    return layout_box->PhysicalLayoutOverflowRect();
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
  // TODO(kojii): See |ScrollableOverflow|.
  DCHECK(height_type == TextHeightType::kEmHeight || PostLayout() == this);
  const FragmentItems* items = Items();
  if (Children().empty() && !items)
    return PhysicalRect();

  // Internal struct to share logic between child fragments and child items.
  // - Inline children's overflow expands by padding end/after.
  // - Float / OOF overflow is added as is.
  // - Children not reachable by scroll overflow do not contribute to it.
  struct ComputeOverflowContext {
    STACK_ALLOCATED();

   public:
    ComputeOverflowContext(const NGPhysicalBoxFragment& container,
                           TextHeightType height_type)
        : container(container),
          style(container.Style()),
          writing_direction(style.GetWritingDirection()),
          border_inline_start(LayoutUnit(style.BorderInlineStartWidth())),
          border_block_start(LayoutUnit(style.BorderBlockStartWidth())),
          height_type(height_type) {
      DCHECK_EQ(&style, container.GetLayoutObject()->Style(
                            container.UsesFirstLineStyle()));

      // End and under padding are added to scroll overflow of inline children.
      // https://github.com/w3c/csswg-drafts/issues/129
      DCHECK_EQ(container.HasNonVisibleOverflow(),
                container.GetLayoutObject()->HasNonVisibleOverflow());
      if (container.HasNonVisibleOverflow()) {
        const auto* layout_object = To<LayoutBox>(container.GetLayoutObject());
        padding_strut =
            BoxStrut(LayoutUnit(), layout_object->PaddingInlineEnd(),
                     LayoutUnit(), layout_object->PaddingBlockEnd())
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

    void AddLineBoxChild(const PhysicalLineBoxFragment& child,
                         const PhysicalOffset& child_offset) {
      if (padding_strut)
        AddLineBoxRect({child_offset, child.Size()});
      PhysicalRect child_scrollable_overflow =
          child.ScrollableOverflow(container, style, height_type);
      child_scrollable_overflow.offset += child_offset;
      AddChild(child_scrollable_overflow);
    }

    void AddLineBoxChild(const FragmentItem& child,
                         const InlineCursor& cursor) {
      DCHECK_EQ(&child, cursor.CurrentItem());
      DCHECK_EQ(child.Type(), FragmentItem::kLine);
      if (padding_strut)
        AddLineBoxRect(child.RectInContainerFragment());
      const PhysicalLineBoxFragment* line_box = child.LineBoxFragment();
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
    absl::optional<PhysicalBoxStrut> padding_strut;
    absl::optional<PhysicalRect> lineboxes_enclosing_rect;
    PhysicalRect children_overflow;
    TextHeightType height_type;
  } context(*this, height_type);

  // Traverse child items.
  if (items) {
    for (InlineCursor cursor(*this, *items); cursor;
         cursor.MoveToNextSkippingChildren()) {
      const FragmentItem* item = cursor.CurrentItem();
      if (item->Type() == FragmentItem::kLine) {
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
      context.AddLineBoxChild(To<PhysicalLineBoxFragment>(*child),
                              child.Offset());
    } else if (height_type == TextHeightType::kEmHeight && IsRubyColumn()) {
      PhysicalRect r = child->ScrollableOverflow(*this, height_type);
      r.offset += child.offset;
      context.AddChild(r);
    }
  }

  context.AddPaddingToLineBoxChildren();

  return context.children_overflow;
}

gfx::Vector2d NGPhysicalBoxFragment::PixelSnappedScrolledContentOffset() const {
  DCHECK(GetLayoutObject());
  return To<LayoutBox>(*GetLayoutObject()).PixelSnappedScrolledContentOffset();
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

PhysicalFragmentRareData::RareField& NGPhysicalBoxFragment::EnsureRareField(
    FieldId id) {
  if (!rare_data_) {
    rare_data_ = MakeGarbageCollected<PhysicalFragmentRareData>(1);
  }
  return rare_data_->EnsureField(id);
}

NGPhysicalBoxFragment::MutableForStyleRecalc::MutableForStyleRecalc(
    base::PassKey<NGPhysicalBoxFragment>,
    NGPhysicalBoxFragment& fragment)
    : fragment_(fragment) {}

void NGPhysicalBoxFragment::MutableForStyleRecalc::SetLayoutOverflow(
    const PhysicalRect& layout_overflow) {
  bool has_layout_overflow =
      layout_overflow != PhysicalRect({}, fragment_.Size());
  if (has_layout_overflow) {
    // This can be called even without rare_data_.
    fragment_.EnsureRareField(FieldId::kLayoutOverflow).layout_overflow =
        layout_overflow;
  } else if (fragment_.HasLayoutOverflow()) {
    fragment_.rare_data_->RemoveField(FieldId::kLayoutOverflow);
  }
}

NGPhysicalBoxFragment::MutableForStyleRecalc
NGPhysicalBoxFragment::GetMutableForStyleRecalc() const {
  DCHECK(layout_object_->GetDocument().Lifecycle().GetState() ==
             DocumentLifecycle::kInStyleRecalc ||
         layout_object_->GetDocument().Lifecycle().GetState() ==
             DocumentLifecycle::kInPerformLayout);
  return MutableForStyleRecalc(base::PassKey<NGPhysicalBoxFragment>(),
                               const_cast<NGPhysicalBoxFragment&>(*this));
}

NGPhysicalBoxFragment::MutableForContainerLayout::MutableForContainerLayout(
    base::PassKey<NGPhysicalBoxFragment>,
    NGPhysicalBoxFragment& fragment)
    : fragment_(fragment) {}

void NGPhysicalBoxFragment::MutableForContainerLayout::SetMargins(
    const PhysicalBoxStrut& margins) {
  DCHECK(RuntimeEnabledFeatures::LayoutNGNoCopyBackEnabled());
  // This can be called even without rare_data_.
  fragment_.EnsureRareField(FieldId::kMargins).margins = margins;
}

NGPhysicalBoxFragment::MutableForContainerLayout
NGPhysicalBoxFragment::GetMutableForContainerLayout() const {
  DCHECK(layout_object_->GetFrameView()->IsInPerformLayout());
  return MutableForContainerLayout(base::PassKey<NGPhysicalBoxFragment>(),
                                   const_cast<NGPhysicalBoxFragment&>(*this));
}

void NGPhysicalBoxFragment::SetInkOverflow(const PhysicalRect& self,
                                           const PhysicalRect& contents) {
  SetInkOverflowType(
      ink_overflow_.Set(InkOverflowType(), self, contents, Size()));
}

void NGPhysicalBoxFragment::RecalcInkOverflow(const PhysicalRect& contents) {
  const PhysicalRect self_rect = ComputeSelfInkOverflow();
  SetInkOverflow(self_rect, contents);
}

void NGPhysicalBoxFragment::RecalcInkOverflow() {
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
  if (UNLIKELY(IsFragmentainerBox()))
    return;

  if (BreakToken()) {
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
PhysicalRect NGPhysicalBoxFragment::RecalcContentsInkOverflow() {
  DCHECK(GetSelfOrContainerLayoutObject());
  DCHECK(!DisplayLockUtilities::LockedAncestorPreventingPrePaint(
      *GetSelfOrContainerLayoutObject()));
  DCHECK(
      !GetSelfOrContainerLayoutObject()->ChildPrePaintBlockedByDisplayLock());

  PhysicalRect contents_rect;
  if (const FragmentItems* items = Items()) {
    InlineCursor cursor(*this, *items);
    NGInlinePaintContext child_inline_context;
    contents_rect = FragmentItem::RecalcInkOverflowForCursor(
        &cursor, &child_inline_context);

    // Add text decorations and emphasis mark ink over flow for combined
    // text.
    const auto* const text_combine =
        DynamicTo<LayoutTextCombine>(GetLayoutObject());
    if (UNLIKELY(text_combine)) {
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

  for (const NGLink& child : PostLayoutChildren()) {
    const auto* child_fragment = DynamicTo<NGPhysicalBoxFragment>(child.get());
    if (!child_fragment || child_fragment->HasSelfPaintingLayer())
      continue;
    DCHECK(!child_fragment->IsOutOfFlowPositioned());

    PhysicalRect child_rect;
    if (child_fragment->CanUseFragmentsForInkOverflow()) {
      child_fragment->GetMutableForPainting().RecalcInkOverflow();
      child_rect = child_fragment->InkOverflow();
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

PhysicalRect NGPhysicalBoxFragment::ComputeSelfInkOverflow() const {
  DCHECK_EQ(PostLayout(), this);
  const ComputedStyle& style = Style();

  PhysicalRect ink_overflow(LocalRect());
  if (UNLIKELY(IsTableRow())) {
    // This is necessary because table-rows paints beyond border box if it
    // contains rowspanned cells.
    for (const NGLink& child : PostLayoutChildren()) {
      const auto& child_fragment = To<NGPhysicalBoxFragment>(*child);
      if (!child_fragment.IsTableCell()) {
        continue;
      }
      const auto* child_layout_object =
          To<LayoutTableCell>(child_fragment.GetLayoutObject());
      if (child_layout_object->ComputedRowSpan() == 1)
        continue;
      PhysicalRect child_rect;
      if (child_fragment.CanUseFragmentsForInkOverflow())
        child_rect = child_fragment.InkOverflow();
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
                        style.OutlineRectsShouldIncludeBlockVisualOverflow(),
                        collector, &info);
    PhysicalRect rect = collector.Rect();
    rect.Inflate(LayoutUnit(OutlinePainter::OutlineOutsetExtent(style, info)));
    ink_overflow.Unite(rect);
  }
  return ink_overflow;
}

#if DCHECK_IS_ON()
void NGPhysicalBoxFragment::InvalidateInkOverflow() {
  SetInkOverflowType(ink_overflow_.Invalidate(InkOverflowType()));
}
#endif

void NGPhysicalBoxFragment::AddSelfOutlineRects(
    const PhysicalOffset& additional_offset,
    NGOutlineType outline_type,
    OutlineRectCollector& collector,
    LayoutObject::OutlineInfo* info) const {
  if (info) {
    if (IsSvgText())
      *info = LayoutObject::OutlineInfo::GetUnzoomedFromStyle(Style());
    else
      *info = LayoutObject::OutlineInfo::GetFromStyle(Style());
  }

  if (ShouldIncludeBlockVisualOverflow(outline_type) &&
      IsA<HTMLAnchorElement>(GetNode())) {
    outline_type = NGOutlineType::kIncludeBlockVisualOverflowForAnchor;
  }

  AddOutlineRects(additional_offset, outline_type,
                  /* container_relative */ false, collector);
}

void NGPhysicalBoxFragment::AddOutlineRects(
    const PhysicalOffset& additional_offset,
    NGOutlineType outline_type,
    OutlineRectCollector& collector) const {
  AddOutlineRects(additional_offset, outline_type,
                  /* container_relative */ true, collector);
}

void NGPhysicalBoxFragment::AddOutlineRects(
    const PhysicalOffset& additional_offset,
    NGOutlineType outline_type,
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
  if (!IsAnonymousBlock()) {
    if (IsSvgText()) {
      if (Items()) {
        collector.AddRect(PhysicalRect::EnclosingRect(
            GetLayoutObject()->ObjectBoundingBox()));
      }
    } else {
      collector.AddRect(PhysicalRect(additional_offset, Size()));
    }
  }

  if (ShouldIncludeBlockVisualOverflow(outline_type) &&
      !HasNonVisibleOverflow() && !HasControlClip(*this)) {
    // Tricky code ahead: we pass a 0,0 additional_offset to
    // AddOutlineRectsForNormalChildren, and add it in after the call.
    // This is necessary because AddOutlineRectsForNormalChildren expects
    // additional_offset to be an offset from containing_block.
    // Since containing_block is our layout object, offset must be 0,0.
    // https://crbug.com/968019
    OutlineRectCollector* child_collector = collector.ForDescendantCollector();
    AddOutlineRectsForNormalChildren(
        *child_collector, PhysicalOffset(), outline_type,
        To<LayoutBoxModelObject>(GetLayoutObject()));
    collector.Combine(child_collector, additional_offset);

    if (ShouldIncludeBlockVisualOverflowForAnchorOnly(outline_type)) {
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

void NGPhysicalBoxFragment::AddOutlineRectsForInlineBox(
    PhysicalOffset additional_offset,
    NGOutlineType outline_type,
    bool container_relative,
    OutlineRectCollector& collector) const {
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
  auto* cursor_collector = collector.ForDescendantCollector();
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
      const NGPhysicalBoxFragment* fragment = current.BoxFragment();
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
  collector.Combine(cursor_collector, additional_offset);

  if (ShouldIncludeBlockVisualOverflowForAnchorOnly(outline_type) &&
      !HasNonVisibleOverflow() && !HasControlClip(*this)) {
    if (container->IsAnonymousBlock()) {
      const auto* container_box = DynamicTo<LayoutBox>(
          container->GetLayoutObject()->NonAnonymousAncestor());
      if (!container_box)
        return;
      // TODO(crbug.com/1380673): Just picking the first fragment isn't right.
      container = container_box->GetPhysicalFragment(0);
      DCHECK(container);
    }

    for (const auto& child : container->PostLayoutChildren()) {
      if (!child->IsOutOfFlowPositioned() ||
          child->GetLayoutObject()->ContainerForAbsolutePosition() !=
              layout_object) {
        continue;
      }

      AddOutlineRectsForDescendant(child, collector, additional_offset,
                                   outline_type,
                                   To<LayoutBoxModelObject>(layout_object));
    }
  }
}

PositionWithAffinity NGPhysicalBoxFragment::PositionForPoint(
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

PositionWithAffinity NGPhysicalBoxFragment::PositionForPointByClosestChild(
    PhysicalOffset point_in_contents) const {
  if (layout_object_->ChildPaintBlockedByDisplayLock()) {
    // If this node is DisplayLocked, then Children() will have invalid layout
    // information.
    return AdjustForEditingBoundary(
        FirstPositionInOrBeforeNode(*layout_object_->GetNode()));
  }

  NGLink closest_child = {nullptr};
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
    return layout_object_->FirstPositionInOrBeforeThis();
  return To<NGPhysicalBoxFragment>(*closest_child)
      .PositionForPoint(point_in_contents - closest_child.offset);
}

PositionWithAffinity
NGPhysicalBoxFragment::PositionForPointInBlockFlowDirection(
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
  const NGLink* last_candidate_box = nullptr;
  for (const NGLink& child : Children()) {
    const auto& box_fragment = To<NGPhysicalBoxFragment>(*child.fragment);
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
          To<NGPhysicalBoxFragment>(*child.fragment),
          point_in_contents - child.offset);
    }

    // |last_candidate_box| is logical above |point_in_contents|.
    last_candidate_box = &child;
  }

  // Here all children are logically above |point_in_contents|.
  if (last_candidate_box) {
    // editing/selection/block-with-positioned-lastchild.html reaches here.
    return PositionForPointRespectingEditingBoundaries(
        To<NGPhysicalBoxFragment>(*last_candidate_box->fragment),
        point_in_contents - last_candidate_box->offset);
  }

  // We only get here if there are no hit test candidate children below the
  // click.
  return PositionForPointByClosestChild(point_in_contents);
}

PositionWithAffinity NGPhysicalBoxFragment::PositionForPointInTable(
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
NGPhysicalBoxFragment::PositionForPointRespectingEditingBoundaries(
    const NGPhysicalBoxFragment& child,
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

PhysicalBoxStrut NGPhysicalBoxFragment::OverflowClipMarginOutsets() const {
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
NGPhysicalBoxFragment::AllowPostLayoutScope::AllowPostLayoutScope() {
  ++allow_count_;
}

NGPhysicalBoxFragment::AllowPostLayoutScope::~AllowPostLayoutScope() {
  DCHECK(allow_count_);
  --allow_count_;
}

void NGPhysicalBoxFragment::CheckSameForSimplifiedLayout(
    const NGPhysicalBoxFragment& other,
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
  DCHECK_EQ(may_have_descendant_above_block_start_,
            other.may_have_descendant_above_block_start_);
  DCHECK_EQ(depends_on_percentage_block_size_,
            other.depends_on_percentage_block_size_);
  DCHECK_EQ(bit_field_.get<HasDescendantsForTablePartFlag>(),
            other.bit_field_.get<HasDescendantsForTablePartFlag>());
  DCHECK_EQ(IsFragmentationContextRoot(), other.IsFragmentationContextRoot());

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

void NGPhysicalBoxFragment::AssertFragmentTreeSelf() const {
  DCHECK(!IsInlineBox());
  DCHECK(OwnerLayoutBox());
  DCHECK_EQ(this, PostLayout());
}

void NGPhysicalBoxFragment::AssertFragmentTreeChildren(
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

  for (const NGLink& child : Children()) {
    if (child->IsLayoutObjectDestroyedOrMoved()) {
      DCHECK(allow_destroyed_or_moved);
      continue;
    }
    if (const auto* box =
            DynamicTo<NGPhysicalBoxFragment>(child.fragment.Get()))
      box->AssertFragmentTreeSelf();
  }
}
#endif

void NGPhysicalBoxFragment::TraceAfterDispatch(Visitor* visitor) const {
  visitor->Trace(children_);
  visitor->Trace(rare_data_);
  // |HasItems()| and |ConstHasRareData()| are const and set
  // in ctor so they do not cause TOCTOU.
  if (HasItems())
    visitor->Trace(*ComputeItemsAddress());
  NGPhysicalFragment::TraceAfterDispatch(visitor);
}

}  // namespace blink
