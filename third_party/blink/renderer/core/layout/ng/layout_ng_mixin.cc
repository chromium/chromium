// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/layout_ng_mixin.h"

#include <memory>
#include <utility>

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/ng/layout_box_utils.h"
#include "third_party/blink/renderer/core/layout/ng/layout_ng_block.h"
#include "third_party/blink/renderer/core/layout/ng/layout_ng_block_flow.h"
#include "third_party/blink/renderer/core/layout/ng/ng_box_fragment_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_disable_side_effects_scope.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_result.h"
#include "third_party/blink/renderer/core/layout/ng/ng_length_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_out_of_flow_layout_part.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/svg/layout_ng_svg_text.h"
#include "third_party/blink/renderer/core/layout/ng/table/layout_ng_table_caption.h"
#include "third_party/blink/renderer/core/paint/ng/ng_box_fragment_painter.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"

namespace blink {

namespace {

bool CanUseConstraintSpaceForCaching(const NGLayoutResult* previous_result,
                                     const LayoutBox& box) {
  if (!previous_result)
    return false;
  const auto& space = previous_result->GetConstraintSpaceForCaching();
  if (space.IsFixedInlineSize() && box.HasOverrideLogicalWidth()) {
    if (space.AvailableSize().inline_size != box.OverrideLogicalWidth())
      return false;
  }
  if (space.IsFixedBlockSize() && box.HasOverrideLogicalHeight()) {
    if (space.AvailableSize().block_size != box.OverrideLogicalHeight())
      return false;
  }
  return space.GetWritingMode() == box.StyleRef().GetWritingMode();
}

}  // namespace

template <typename Base>
LayoutNGMixin<Base>::LayoutNGMixin(ContainerNode* node) : Base(node) {
  Base::CheckIsNotDestroyed();
  static_assert(
      std::is_base_of<LayoutBlock, Base>::value,
      "Base class of LayoutNGMixin must be LayoutBlock or derived class.");
}

template <typename Base>
LayoutNGMixin<Base>::~LayoutNGMixin() = default;

template <typename Base>
void LayoutNGMixin<Base>::Paint(const PaintInfo& paint_info) const {
  Base::CheckIsNotDestroyed();

  // When |this| is NG block fragmented, the painter should traverse fragments
  // instead of |LayoutObject|, because this function cannot handle block
  // fragmented objects. We can come here only when |this| cannot traverse
  // fragments, or the parent is legacy.
  DCHECK(Base::IsMonolithic() || !Base::CanTraversePhysicalFragments() ||
         !Base::Parent()->CanTraversePhysicalFragments());
  // We may get here in multiple-fragment cases if the object is repeated
  // (inside table headers and footers, for instance).
  DCHECK(Base::PhysicalFragmentCount() <= 1u ||
         Base::GetPhysicalFragment(0)->BreakToken()->IsRepeated());

  // Avoid painting dirty objects because descendants maybe already destroyed.
  if (UNLIKELY(Base::NeedsLayout() &&
               !Base::ChildLayoutBlockedByDisplayLock())) {
    NOTREACHED();
    return;
  }

  if (Base::PhysicalFragmentCount()) {
    const NGPhysicalBoxFragment* fragment = Base::GetPhysicalFragment(0);
    DCHECK(fragment);
    NGBoxFragmentPainter(*fragment).Paint(paint_info);
    return;
  }

  NOTREACHED();
  Base::Paint(paint_info);
}

template <typename Base>
bool LayoutNGMixin<Base>::NodeAtPoint(HitTestResult& result,
                                      const HitTestLocation& hit_test_location,
                                      const PhysicalOffset& accumulated_offset,
                                      HitTestPhase phase) {
  Base::CheckIsNotDestroyed();

  // See |Paint()|.
  DCHECK(Base::IsMonolithic() || !Base::CanTraversePhysicalFragments() ||
         !Base::Parent()->CanTraversePhysicalFragments());
  // We may get here in multiple-fragment cases if the object is repeated
  // (inside table headers and footers, for instance).
  DCHECK(Base::PhysicalFragmentCount() <= 1u ||
         Base::GetPhysicalFragment(0)->BreakToken()->IsRepeated());

  if (Base::PhysicalFragmentCount()) {
    const NGPhysicalBoxFragment* fragment = Base::GetPhysicalFragment(0);
    DCHECK(fragment);
    return NGBoxFragmentPainter(*fragment).NodeAtPoint(
        result, hit_test_location, accumulated_offset, phase);
  }

  return false;
}

template <typename Base>
RecalcLayoutOverflowResult LayoutNGMixin<Base>::RecalcLayoutOverflow() {
  Base::CheckIsNotDestroyed();
  DCHECK(!NGDisableSideEffectsScope::IsDisabled());
  return Base::RecalcLayoutOverflowNG();
}

template <typename Base>
void LayoutNGMixin<Base>::RecalcVisualOverflow() {
  Base::CheckIsNotDestroyed();
  if (Base::CanUseFragmentsForVisualOverflow()) {
    Base::RecalcFragmentsVisualOverflow();
    return;
  }
  Base::RecalcVisualOverflow();
}

template <typename Base>
bool LayoutNGMixin<Base>::IsLayoutNGObject() const {
  Base::CheckIsNotDestroyed();
  return true;
}

template <typename Base>
MinMaxSizes LayoutNGMixin<Base>::ComputeIntrinsicLogicalWidths() const {
  Base::CheckIsNotDestroyed();
  DCHECK(!Base::IsTableCell());

  NGBlockNode node(const_cast<LayoutNGMixin<Base>*>(this));
  CHECK(node.CanUseNewLayout());

  NGConstraintSpace space = ConstraintSpaceForMinMaxSizes();
  return node
      .ComputeMinMaxSizes(node.Style().GetWritingMode(),
                          MinMaxSizesType::kContent, space)
      .sizes;
}

template <typename Base>
NGConstraintSpace LayoutNGMixin<Base>::ConstraintSpaceForMinMaxSizes() const {
  Base::CheckIsNotDestroyed();
  DCHECK(!Base::IsTableCell());
  const ComputedStyle& style = Base::StyleRef();

  NGConstraintSpaceBuilder builder(style.GetWritingMode(),
                                   style.GetWritingDirection(),
                                   /* is_new_fc */ true);
  builder.SetAvailableSize(
      {Base::ContainingBlockLogicalWidthForContent(),
       LayoutBoxUtils::AvailableLogicalHeight(*this, Base::ContainingBlock())});

  return builder.ToConstraintSpace();
}

template <typename Base>
void LayoutNGMixin<Base>::UpdateOutOfFlowBlockLayout() {
  Base::CheckIsNotDestroyed();

  auto* css_container = To<LayoutBoxModelObject>(Base::Container());
  DCHECK(!css_container->IsBox() || css_container->IsLayoutBlock());
  auto* container = DynamicTo<LayoutBlock>(css_container);
  if (!container)
    container = Base::ContainingBlock();
  const ComputedStyle* container_style = container->Style();
  NGConstraintSpace constraint_space =
      NGConstraintSpace::CreateFromLayoutObject(*container);

  // As this is part of the Legacy->NG bridge, the container_builder is used
  // for indicating the resolved size of the OOF-positioned containing-block
  // and not used for caching purposes.
  // When we produce a layout result from it, we access its child fragments
  // which must contain *at least* this node. We use the child fragments for
  // copying back position information.
  NGBlockNode container_node(container);
  NGBoxFragmentBuilder container_builder(
      container_node, scoped_refptr<const ComputedStyle>(container_style),
      constraint_space, container_style->GetWritingDirection());
  container_builder.SetIsNewFormattingContext(
      container_node.CreatesNewFormattingContext());

  NGFragmentGeometry fragment_geometry;
  fragment_geometry.border = ComputeBorders(constraint_space, container_node);
  fragment_geometry.scrollbar =
      ComputeScrollbars(constraint_space, container_node);
  fragment_geometry.padding =
      ComputePadding(constraint_space, *container_style);

  NGBoxStrut border_scrollbar =
      fragment_geometry.border + fragment_geometry.scrollbar;

  // Calculate the border-box size of the object that's the containing block of
  // this out-of-flow positioned descendant. Note that this is not to be used as
  // the containing block size to resolve sizes and positions for the
  // descendant, since we're dealing with the border box here (not the padding
  // box, which is where the containing block is established). These sizes are
  // just used to do a fake/partial NG layout pass of the containing block (that
  // object is really managed by legacy layout).
  LayoutUnit container_border_box_logical_width;
  LayoutUnit container_border_box_logical_height;
  if (Base::HasOverrideContainingBlockContentLogicalWidth()) {
    container_border_box_logical_width =
        Base::OverrideContainingBlockContentLogicalWidth() +
        border_scrollbar.InlineSum();
  } else {
    container_border_box_logical_width = container->LogicalWidth();
  }
  if (Base::HasOverrideContainingBlockContentLogicalHeight()) {
    container_border_box_logical_height =
        Base::OverrideContainingBlockContentLogicalHeight() +
        border_scrollbar.BlockSum();
  } else {
    container_border_box_logical_height = container->LogicalHeight();
  }

  fragment_geometry.border_box_size = {container_border_box_logical_width,
                                       container_border_box_logical_height};
  container_builder.SetInitialFragmentGeometry(fragment_geometry);

  // TODO(1229581): Remove this call to determine the static position.
  NGLogicalStaticPosition static_position =
      LayoutBoxUtils::ComputeStaticPositionFromLegacy(*this, border_scrollbar);

  // Set correct container for inline containing blocks.
  container_builder.AddOutOfFlowLegacyCandidate(
      NGBlockNode(this), static_position,
      DynamicTo<LayoutInline>(css_container));

  absl::optional<LogicalSize> initial_containing_block_fixed_size =
      NGOutOfFlowLayoutPart::InitialContainingBlockFixedSize(
          NGBlockNode(container));
  // We really only want to lay out ourselves here, so we pass |this| to
  // Run(). Otherwise, NGOutOfFlowLayoutPart may also lay out other objects
  // it discovers that are part of the same containing block, but those
  // should get laid out by the actual containing block.
  NGOutOfFlowLayoutPart(css_container->CanContainAbsolutePositionObjects(),
                        css_container->CanContainFixedPositionObjects(),
                        /* is_grid_container */ false, constraint_space,
                        &container_builder, initial_containing_block_fixed_size)
      .Run(/* only_layout */ this);
  const NGLayoutResult* result = container_builder.ToBoxFragment();

  const auto& fragment = result->PhysicalFragment();
  DCHECK_GT(fragment.Children().size(), 0u);

  // Handle the unpositioned OOF descendants of the current OOF block.
  if (fragment.HasOutOfFlowPositionedDescendants()) {
    LayoutBlock* oof_container =
        LayoutObject::FindNonAnonymousContainingBlock(container);
    for (const auto& descendant : fragment.OutOfFlowPositionedDescendants())
      descendant.Node().InsertIntoLegacyPositionedObjectsOf(oof_container);
  }

  // Copy sizes of all child fragments to Legacy.
  // There could be multiple fragments, when this node has descendants whose
  // container is this node's container.
  // Example: fixed descendant of fixed element.
  for (auto& child : fragment.Children()) {
    const NGPhysicalFragment* child_fragment = child.get();
    DCHECK(child_fragment->GetLayoutObject()->IsBox());
    auto* child_legacy_box =
        To<LayoutBox>(child_fragment->GetMutableLayoutObject());
    PhysicalOffset child_offset = child.Offset();
    if (container_style->IsFlippedBlocksWritingMode()) {
      child_offset.left = container_border_box_logical_height -
                          child_offset.left - child_fragment->Size().width;
    }
    child_legacy_box->SetLocation(child_offset.ToLayoutPoint());
  }
  DCHECK_EQ(fragment.Children()[0]->GetLayoutObject(), this);
}

template <typename Base>
const NGLayoutResult* LayoutNGMixin<Base>::UpdateInFlowBlockLayout() {
  Base::CheckIsNotDestroyed();

  // This is an entry-point for LayoutNG from the legacy engine. This means that
  // we need to be at a formatting context boundary, since NG and legacy don't
  // cooperate on e.g. margin collapsing.
  DCHECK(this->CreatesNewFormattingContext());

  const NGLayoutResult* previous_result = Base::GetSingleCachedLayoutResult();

  // If we are a layout root, use the previous space if available. This will
  // include any stretched sizes if applicable.
  NGConstraintSpace constraint_space =
      CanUseConstraintSpaceForCaching(previous_result, *this)
          ? previous_result->GetConstraintSpaceForCaching()
          : NGConstraintSpace::CreateFromLayoutObject(*this);

  const NGLayoutResult* result = NGBlockNode(this).Layout(constraint_space);

  const auto& physical_fragment =
      To<NGPhysicalBoxFragment>(result->PhysicalFragment());

  for (const auto& descendant :
       physical_fragment.OutOfFlowPositionedDescendants()) {
    descendant.Node().InsertIntoLegacyPositionedObjectsOf(
        descendant.box->ContainingBlock());
  }

  // Even if we are a layout root, our baseline may have shifted. In this
  // (rare) case, mark our containing-block for layout.
  if (previous_result) {
    if (To<NGPhysicalBoxFragment>(previous_result->PhysicalFragment())
            .FirstBaseline() != physical_fragment.FirstBaseline()) {
      if (auto* containing_block = Base::ContainingBlock()) {
        // Baselines inside replaced elements don't affect other boxes.
        bool is_in_replaced = false;
        for (auto* parent = Base::Parent();
             parent && parent != containing_block; parent = parent->Parent()) {
          if (parent->IsLayoutReplaced()) {
            is_in_replaced = true;
            break;
          }
        }
        if (!is_in_replaced) {
          containing_block->SetNeedsLayout(
              layout_invalidation_reason::kChildChanged, kMarkContainerChain);
        }
      }
    }
  }

  return result;
}

template <typename Base>
void LayoutNGMixin<Base>::UpdateMargins() {
  Base::CheckIsNotDestroyed();

  const LayoutBlock* containing_block = Base::ContainingBlock();
  if (!containing_block || !containing_block->IsLayoutBlockFlow())
    return;

  // In the legacy engine, for regular block container layout, children
  // calculate and store margins on themselves, while in NG that's done by the
  // container. Since this object is a LayoutNG entry-point, we'll have to do it
  // on ourselves, since that's what the legacy container expects.
  const ComputedStyle& style = Base::StyleRef();
  const ComputedStyle& cb_style = containing_block->StyleRef();
  const auto writing_direction = cb_style.GetWritingDirection();
  LayoutUnit available_logical_width =
      LayoutBoxUtils::AvailableLogicalWidth(*this, containing_block);
  NGBoxStrut margins = ComputePhysicalMargins(style, available_logical_width)
                           .ConvertToLogical(writing_direction);
  ResolveInlineMargins(style, cb_style, available_logical_width,
                       Base::LogicalWidth(), &margins);
  Base::SetMargin(margins.ConvertToPhysical(writing_direction));
}

template class CORE_TEMPLATE_EXPORT LayoutNGMixin<LayoutBlock>;
template class CORE_TEMPLATE_EXPORT LayoutNGMixin<LayoutBlockFlow>;
template class CORE_TEMPLATE_EXPORT LayoutNGMixin<LayoutSVGBlock>;
template class CORE_TEMPLATE_EXPORT LayoutNGMixin<LayoutView>;

}  // namespace blink
