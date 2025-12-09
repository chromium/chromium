// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/fragment_builder.h"

#include "base/containers/contains.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom-shared.h"
#include "third_party/blink/renderer/core/animation/animation_trigger.h"
#include "third_party/blink/renderer/core/display_lock/display_lock_utilities.h"
#include "third_party/blink/renderer/core/dom/column_pseudo_element.h"
#include "third_party/blink/renderer/core/layout/block_layout_algorithm_utils.h"
#include "third_party/blink/renderer/core/layout/fragmentation_utils.h"
#include "third_party/blink/renderer/core/layout/physical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/physical_fragment.h"
#include "third_party/blink/renderer/core/layout/transform_utils.h"
#include "third_party/blink/renderer/core/style/computed_style_base_constants.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

namespace {

bool IsInlineContainerForNode(const BlockNode& node,
                              const LayoutObject* inline_container) {
  return inline_container && inline_container->IsLayoutInline() &&
         inline_container->CanContainOutOfFlowPositionedElement(
             node.Style().GetPosition());
}

}  // namespace

AnchorMap::SetOptions FragmentBuilder::AnchorOptionsForChild(
    const PhysicalFragment& fragment) const {
  // If the |fragment| is not absolutely positioned, it's an in-flow anchor.
  // https://drafts.csswg.org/css-anchor-1/#determining
  if (!fragment.IsOutOfFlowPositioned()) {
    return AnchorMap::SetOptions::kInFlow;
  }

  // If the OOF |fragment| is not in a block fragmentation context, it's a child
  // of its containing block. Make it out-of-flow.
  bool maybe_out_of_order_if_oof =
      IsBlockFragmentationContextRoot() || HasItems();
  if (!maybe_out_of_order_if_oof) {
    return AnchorMap::SetOptions::kOutOfFlow;
  }

  // |container| is null if it's an inline box.
  const LayoutInputNode& container = node_;
  if (!container.GetLayoutBox()) {
    return AnchorMap::SetOptions::kOutOfFlow;
  }

  // If the OOF |fragment| is in a block fragmentation context, it's a child of
  // the fragmentation context root. If its containing block is the |container|,
  // make it out-of-flow.
  const LayoutObject* layout_object = fragment.GetLayoutObject();
  DCHECK(layout_object);
  const LayoutObject* containing_block = layout_object->Container();
  DCHECK(containing_block);
  if (containing_block == container.GetLayoutBox()) {
    return AnchorMap::SetOptions::kOutOfFlow;
  }
  // Otherwise its containing block is a descendant of the block fragmentation
  // context, so it's in-flow.
  return AnchorMap::SetOptions::kInFlow;
}

bool FragmentBuilder::IsRoot() const {
  return node_ && node_.IsView() && !space_.IsAnonymous();
}

bool FragmentBuilder::IsPaginatedRoot() const {
  return IsRoot() && node_.IsPaginatedRoot();
}

PhysicalFragment::BoxType FragmentBuilder::GetBoxType() const {
  if (box_type_ != PhysicalFragment::BoxType::kNormalBox) {
    return box_type_;
  }

  // When implicit, compute from LayoutObject.
  DCHECK(layout_object_);
  if (layout_object_->IsFloating()) {
    return PhysicalFragment::BoxType::kFloating;
  }
  if (layout_object_->IsOutOfFlowPositioned()) {
    return PhysicalFragment::BoxType::kOutOfFlowPositioned;
  }
  if (layout_object_->IsRenderedLegend()) {
    return PhysicalFragment::BoxType::kRenderedLegend;
  }
  if (layout_object_->StyleRef().IsPageMarginBox()) {
    return PhysicalFragment::BoxType::kPageMargin;
  }
  if (layout_object_->IsInline()) {
    // Check |IsAtomicInlineLevel()| after |IsInline()| because |LayoutReplaced|
    // sets |IsAtomicInlineLevel()| even when it's block-level. crbug.com/567964
    if (layout_object_->IsAtomicInlineLevel()) {
      return PhysicalFragment::BoxType::kAtomicInline;
    }
    return PhysicalFragment::BoxType::kInlineBox;
  }
  DCHECK(node_) << "Must call SetBoxType if there is no node";
  DCHECK_EQ(is_new_fc_, node_.CreatesNewFormattingContext())
      << "Forgot to call builder.SetIsNewFormattingContext";
  if (is_new_fc_) {
    return PhysicalFragment::BoxType::kBlockFlowRoot;
  }
  return PhysicalFragment::BoxType::kNormalBox;
}

void FragmentBuilder::ReplaceChild(wtf_size_t index,
                                   const PhysicalFragment& new_child,
                                   const LogicalOffset offset) {
  DCHECK_LT(index, children_.size());
  children_[index] = LogicalFragmentLink(new_child, offset);
}

GCedHeapVector<Member<LayoutBoxModelObject>>&
FragmentBuilder::EnsureStickyDescendants() {
  if (!sticky_descendants_) {
    sticky_descendants_ =
        MakeGarbageCollected<GCedHeapVector<Member<LayoutBoxModelObject>>>();
  }
  return *sticky_descendants_;
}

void FragmentBuilder::PropagateStickyDescendants(
    const PhysicalFragment& child) {
  if (child.HasStickyConstrainedPosition()) {
    EnsureStickyDescendants().push_front(
        To<LayoutBoxModelObject>(child.GetMutableLayoutObject()));
  }

  if (const auto* child_sticky_descendants =
          child.PropagatedStickyDescendants()) {
    EnsureStickyDescendants().AppendVector(*child_sticky_descendants);
  }
}

GCedHeapVector<Member<Element>>& FragmentBuilder::EnsureSnapAreas() {
  if (!snap_areas_) {
    snap_areas_ = MakeGarbageCollected<GCedHeapVector<Member<Element>>>();
  }
  return *snap_areas_;
}

void FragmentBuilder::PropagateSnapAreas(const PhysicalFragment& child) {
  auto get_insertion_pos = [&](Element* snap_area) {
    auto& snap_areas = EnsureSnapAreas();
    // TODO(crbug.com/365680822): ::column pseudo-elements don't have layout
    // objects, and how snap areas established by them should be sorted,
    // relatively to real elements, is undefined.
    const LayoutBox* new_box = snap_area->GetLayoutBox();
    if (!new_box) {
      return snap_areas.size();
    }
    // Ensure that snap areas are added in DOM order.
    for (wtf_size_t i = snap_areas.size(); i >= 1; i--) {
      const LayoutBox* existing_box = snap_areas.at(i - 1)->GetLayoutBox();
      if (existing_box && existing_box->IsBeforeInPreOrder(*new_box)) {
        return i;
      }
    }
    return 0u;
  };
  if (child.IsSnapArea()) {
    // Insert a new snap area *once* per node, when at the last fragment
    // (i.e. when there's no outgoing break token).
    if (!To<PhysicalBoxFragment>(child).GetBreakToken()) {
      auto* snap_area = To<Element>(child.GetLayoutObject()->GetNode());
      EnsureSnapAreas().insert(get_insertion_pos(snap_area), snap_area);
    }
  }

  if (const auto* child_snap_areas = child.PropagatedSnapAreas()) {
    EnsureSnapAreas().InsertVector(get_insertion_pos(child_snap_areas->at(0)),
                                   *child_snap_areas);
  }

  if (child.IsSnapArea() && child.PropagatedSnapAreas()) {
    child.GetDocument().CountUse(WebFeature::kScrollSnapNestedSnapAreas);
  }
}

void FragmentBuilder::AddSnapAreaForColumn(ColumnPseudoElement* column_pseudo) {
  EnsureSnapAreas().push_back(column_pseudo);
}

void FragmentBuilder::PropagateChildAnchors(const PhysicalFragment& child,
                                            const LogicalOffset& child_offset) {
  if (!child.HasAnchorsToPropagate()) {
    return;
  }

  if (!has_final_size_) {
    // The container size isn't known yet. It needs to finish layout before
    // anchors can be propagated, since they are stored in physical coordinates.
    children_with_size_dependent_propagation_.push_back(
        LogicalFragmentLink(child, child_offset));
    return;
  }

  const LayoutObject* container_object = GetLayoutObject();
  CHECK(container_object);

  AnchorMap::SetOptions options = AnchorOptionsForChild(child);
  PropagateChildAnchors(child, child_offset, *container_object,
                        GetWritingDirection(), Size(), options, &anchor_map_);
}

void FragmentBuilder::PropagateChildAnchors(
    const PhysicalFragment& child,
    const LogicalOffset& child_offset,
    const LayoutObject& container_object,
    WritingDirectionMode writing_direction,
    LogicalSize container_logical_size,
    AnchorMap::SetOptions options,
    AnchorMap** out_anchor_map) {
  auto EnsureAnchorMap = [&out_anchor_map]() -> AnchorMap& {
    if (!*out_anchor_map) {
      *out_anchor_map = MakeGarbageCollected<AnchorMap>();
    }
    return **out_anchor_map;
  };

  Element* context = nullptr;
  if (auto* node = child.GetNode()) {
    if (auto* element = DynamicTo<Element>(node)) {
      if (auto* display_lock = element->GetDisplayLockContext()) {
        // An element can't anchor to the skipped contents of an element.
        // https://drafts.csswg.org/css-anchor-position-1/#target
        if (display_lock->IsLocked()) {
          return;
        }
        context = element;
      }
    }
  }
  PhysicalSize physical_container_size = ToPhysicalSize(
      container_logical_size, writing_direction.GetWritingMode());
  if (child.IsAnchor()) {
    DCHECK(child.GetLayoutObject());
    // Set the child's `anchor-name` before propagating its descendants', so
    // that ancestors have precedence over their descendants.
    LogicalRect logical_rect(
        child_offset,
        ToLogicalSize(child.Size(), writing_direction.GetWritingMode()));
    const WritingModeConverter converter(writing_direction,
                                         container_logical_size);
    PhysicalRect rect = converter.ToPhysical(logical_rect);
    TransformState transform_state(
        TransformState::kApplyTransformDirection,
        gfx::QuadF(gfx::RectF(gfx::SizeF(rect.size))));
    UpdateTransformState(child, rect.offset, container_object,
                         physical_container_size, &transform_state);

    if (child.IsExplicitAnchor()) {
      for (const ScopedCSSName* name : child.Style().AnchorName()->GetNames()) {
        AnchorScopedName* anchor_scoped_name =
            ToAnchorScopedName(*name, *child.GetLayoutObject());
        EnsureAnchorMap().Set(anchor_scoped_name, *child.GetLayoutObject(),
                              transform_state, rect, options, context);
      }
    }
    if (child.IsImplicitAnchor()) {
      EnsureAnchorMap().Set(To<Element>(child.GetNode()),
                            *child.GetLayoutObject(), transform_state, rect,
                            options, context);
    }
  }

  // Propagate any descendants' anchor references.
  if (child.GetAnchorMap()) {
    const WritingModeConverter converter(writing_direction,
                                         container_logical_size);
    PhysicalOffset additional_offset =
        converter.ToPhysical(child_offset, child.Size());
    EnsureAnchorMap().SetFromChild(child, additional_offset, container_object,
                                   physical_container_size, options, context);
  }
}

void FragmentBuilder::PropagateFromLayoutResultAndFragment(
    const LayoutResult& child_result,
    LogicalOffset child_offset,
    LogicalOffset relative_offset,
    const OofInlineContainer<LogicalOffset>* inline_container) {
  PropagateFromLayoutResult(child_result);
  PropagateFromFragment(child_result.GetPhysicalFragment(), child_offset,
                        relative_offset, inline_container);
}

void FragmentBuilder::PropagateFromLayoutResult(
    const LayoutResult& child_result) {
  has_orthogonal_fallback_size_descendant_ |=
      child_result.HasOrthogonalFallbackInlineSize() ||
      child_result.HasOrthogonalFallbackSizeDescendant();
}

void FragmentBuilder::UpdateScrollInitialTarget(
    const LayoutObject* new_target) {
  if (new_target != scroll_start_target_ &&
      (!scroll_start_target_ ||
       new_target->IsBeforeInPreOrder(*scroll_start_target_))) {
    scroll_start_target_ = new_target;
  }
}

void FragmentBuilder::PropagateScrollInitialTarget(
    const PhysicalFragment& child) {
  if (child.Style().ScrollInitialTarget() != EScrollInitialTarget::kNone) {
    if (auto* child_object = child.GetMutableLayoutObject()) {
      UpdateScrollInitialTarget(child_object);
    }
  }

  if (const Member<const LayoutObject> target =
          child.PropagatedScrollInitialTarget()) {
    UpdateScrollInitialTarget(target);
  }
}

// Propagate data in |child| to this fragment. The |child| will then be added as
// a child fragment or a child fragment item.
void FragmentBuilder::PropagateFromFragment(
    const PhysicalFragment& child,
    LogicalOffset child_offset,
    LogicalOffset relative_offset,
    const OofInlineContainer<LogicalOffset>* inline_container) {
  if (GetBoxType() == PhysicalFragment::kPageBorderBox) {
    // This is the boundary between page boxes and document contents. No
    // propagation should take place.
    DCHECK_EQ(child.GetBoxType(), PhysicalFragment::kPageArea);
    return;
  }

  if (child.HasAnchorsToPropagate()) {
    // This child either is an anchor, or has anchors inside (or both). They are
    // to be propagated as soon as the container size is known.
    PropagateChildAnchors(child, child_offset + relative_offset);

    has_running_anchor_transform_animation_ =
        has_running_anchor_transform_animation_ ||
        child.HasRunningAnchorTransformAnimation();
  }

  PropagateStickyDescendants(child);
  PropagateSnapAreas(child);
  PropagateScrollInitialTarget(child);
  PropagateNamedTriggers(child);

  // Propagate info about OOF descendants if necessary. This part must be
  // skipped when adding OOF children to fragmentainers, as propagation is
  // special and performed manually from the OOF code in such cases, and cannot
  // be done as part of adding child fragments. First of all, the parameters to
  // PropagateOOFPositionedInfo() will be different from what we can provide
  // here, and furthermore, OOFs in fragmentation are added by recreating
  // fragmentainers, by adding old children and then appending new OOF
  // children. This may take place in several passes (if there are nested OOFs
  // that are discovered as part of laying out an outer OOF), and repropagating
  // for OOFs that were laid out previously over and over again would be wrong.
  //
  // TODO(crbug.com/40267498): Remove the comment above when removing the flag.
  if (child.NeedsOOFPositionedInfoPropagation() &&
      (RuntimeEnabledFeatures::FragmentedOofInCbEnabled() ||
       !IsFragmentainerBoxType() || !child.IsOutOfFlowPositioned())) {
    LayoutUnit adjustment_for_oof_propagation =
        BlockOffsetAdjustmentForFragmentainer();

    PropagateOOFPositionedInfo(child, child_offset, relative_offset,
                               /* offset_adjustment */ LogicalOffset(),
                               inline_container,
                               adjustment_for_oof_propagation);
  }

  // We only need to report if inflow or floating elements depend on the
  // percentage resolution block-size. OOF-positioned children resolve their
  // percentages against the "final" size of their parent.
  if (!has_descendant_that_depends_on_percentage_block_size_) {
    if (child.DependsOnPercentageBlockSize() && !child.IsOutOfFlowPositioned())
      has_descendant_that_depends_on_percentage_block_size_ = true;

    // We may have a child which has the following style:
    // <div style="position: relative; top: 50%;"></div>
    // We need to mark this as depending on our %-block-size for the its offset
    // to be correctly calculated. This is *slightly* too broad as it only
    // depends on the available block-size, rather than the %-block-size.
    const auto& child_style = child.Style();
    if (child.IsCSSBox() && child_style.GetPosition() == EPosition::kRelative) {
      if (Style().IsHorizontalWritingMode()) {
        if (child_style.Top().HasPercent() ||
            child_style.Bottom().HasPercent()) {
          has_descendant_that_depends_on_percentage_block_size_ = true;
        }
      } else {
        if (child_style.Left().HasPercent() ||
            child_style.Right().HasPercent()) {
          has_descendant_that_depends_on_percentage_block_size_ = true;
        }
      }
    }
  }

  // Compute |has_floating_descendants_for_paint_| to optimize tree traversal
  // in paint.
  if (!has_floating_descendants_for_paint_) {
    if (child.IsFloating() || (child.HasFloatingDescendantsForPaint() &&
                               !child.IsPaintedAtomically())) {
      has_floating_descendants_for_paint_ = true;
    }
  }

  // The |has_adjoining_object_descendants_| is used to determine if a fragment
  // can be re-used when preceding floats are present.
  // If a fragment doesn't have any adjoining object descendants, and is
  // self-collapsing, it can be "shifted" anywhere.
  if (!has_adjoining_object_descendants_) {
    if (!child.IsFormattingContextRoot() &&
        child.HasAdjoiningObjectDescendants())
      has_adjoining_object_descendants_ = true;
  }

  // Collect any (block) break tokens, but skip break tokens for fragmentainers,
  // as they should only escape a fragmentation context at the discretion of the
  // fragmentation context. Also skip this if there's a pre-set break token.
  if (has_block_fragmentation_ && !child.IsFragmentainerBox() &&
      !break_token_) {
    const BreakToken* child_break_token = child.GetBreakToken();
    switch (child.Type()) {
      case PhysicalFragment::kFragmentBox:
        if (child_break_token)
          child_break_tokens_.push_back(child_break_token);
        break;
      case PhysicalFragment::kFragmentLineBox:
        if (child.IsLineForParallelFlow()) {
          // This is a line that only contains a resumed float / block after a
          // fragmentation break. It should not affect orphans / widows
          // calculation.
          break;
        }

        const auto* inline_break_token =
            To<InlineBreakToken>(child_break_token);
        // TODO(mstensho): Orphans / widows calculation is wrong when regular
        // inline layout gets interrupted by a block-in-inline. We need to reset
        // line_count_ when this happens.
        //
        // We only care about the break token from the last line box added. This
        // is where we'll resume if we decide to block-fragment. Note that
        // child_break_token is nullptr if this is the last line to be generated
        // from the node.
        last_inline_break_token_ = inline_break_token;
        line_count_++;
        break;
    }
  }
}

void FragmentBuilder::AddChildInternal(const PhysicalFragment* child,
                                       const LogicalOffset& child_offset) {
  // In order to know where list-markers are within the children list (for the
  // |SimplifiedLayoutAlgorithm|) we always place them as the first child.
  if (child->IsListMarker()) {
    children_.push_front(LogicalFragmentLink(*child, child_offset));
    return;
  }

  if (child->IsTextControlPlaceholder()) {
    // ::placeholder should be followed by another block in order to paint
    // ::placeholder earlier.
    const wtf_size_t size = children_.size();
    if (size > 0) {
      children_.insert(size - 1, LogicalFragmentLink(*child, child_offset));
      return;
    }
  }

  children_.push_back(LogicalFragmentLink(*child, child_offset));
}

void FragmentBuilder::AddOutOfFlowChildCandidate(
    const BlockNode& child,
    const LogicalStaticPosition& static_pos,
    bool allow_top_layer_nodes) {
  DCHECK(child);
  // Top-layer elements are processed separately in the OutOfFlowLayoutPart.
  if (child.IsInTopOrViewTransitionLayer() && !allow_top_layer_nodes) {
    return;
  }

  oof_candidates_may_have_anchors_ |= child.MayContainAnchor();
  oof_positioned_candidates_.emplace_back(child, /*break_token=*/nullptr,
                                          static_pos,
                                          RequiresContentBeforeBreaking());
}

void FragmentBuilder::AddOutOfFlowChildCandidate(
    const BlockNode& child,
    const BlockBreakToken& child_break_token) {
  oof_candidates_may_have_anchors_ |= child.MayContainAnchor();
  oof_positioned_candidates_.emplace_back(child, &child_break_token,
                                          LogicalStaticPosition(),
                                          RequiresContentBeforeBreaking());
}

void FragmentBuilder::AddOutOfFlowInlineChildCandidate(
    BlockNode child,
    const LogicalOffset& child_offset,
    WritingDirectionMode inline_container_writing_direction,
    LayoutUnit line_box_block_size) {
  DCHECK(node_.IsInline() || layout_object_->IsLayoutInline());

  LogicalStaticPosition static_pos(child_offset);

  // 'align-items' and 'justify-items' don't apply in inline layout, so don't
  // apply them to OOF items.
  static_pos.inline_edge = InlineStaticPositionEdge(
      child, /*justify_items_style=*/nullptr,
      inline_container_writing_direction,
      /*should_swap_inline_axis=*/
      !IsLtr(inline_container_writing_direction.Direction()));
  static_pos.block_edge = BlockStaticPositionEdge(
      child, /*align_items_style=*/nullptr, inline_container_writing_direction);

  // The alignment container for inline OOF elements is a zero-thickness line in
  // the block direction. As such, we need to adjust the block static position
  // offset for end/center alignment to ensure the OOF ends up aligned correctly
  // within its alignment container. The inline offset will not change.
  //
  // https://drafts.csswg.org/css-position-3/#staticpos-rect
  switch (static_pos.block_edge) {
    case LogicalStaticPosition::BlockEdge::kBlockCenter:
      static_pos.offset.block_offset += line_box_block_size / 2;
      break;
    case LogicalStaticPosition::BlockEdge::kBlockEnd:
      static_pos.offset.block_offset += line_box_block_size;
      break;
    case LogicalStaticPosition::BlockEdge::kBlockStart:
      // The static position is already correct in this case.
      break;
  }

  // As all inline-level fragments are built in the line-logical coordinate
  // system (Direction() is kLtr), we need to know the direction of the
  // parent element to correctly determine an OOF childs static position.
  AddOutOfFlowChildCandidate(child, static_pos);
}

void FragmentBuilder::AddOutOfFlowFragmentainerDescendant(
    const LogicalOofNodeForFragmentation& descendant) {
  DCHECK(!RuntimeEnabledFeatures::FragmentedOofInCbEnabled());
  oof_fragmentainer_descendants_may_have_anchors_ |=
      descendant.box->MayContainAnchor();
  oof_positioned_fragmentainer_descendants_.push_back(descendant);
}

void FragmentBuilder::AddOutOfFlowFragmentainerDescendant(
    const LogicalOofPositionedNode& descendant) {
  DCHECK(!RuntimeEnabledFeatures::FragmentedOofInCbEnabled());
  DCHECK(!descendant.is_for_fragmentation);
  LogicalOofNodeForFragmentation fragmentainer_descendant(descendant);
  AddOutOfFlowFragmentainerDescendant(fragmentainer_descendant);
}

void FragmentBuilder::AddOutOfFlowDescendant(
    const LogicalOofPositionedNode& descendant) {
  oof_positioned_descendants_.push_back(descendant);
}

void FragmentBuilder::SwapOutOfFlowPositionedCandidates(
    HeapVector<LogicalOofPositionedNode>* candidates) {
  DCHECK(candidates->empty());
  if (oof_candidates_may_have_anchors_) {
    auto compare = [](const LogicalOofPositionedNode& a,
                      const LogicalOofPositionedNode& b) -> bool {
      // Positioned elements with the deepest inline containing-block
      // should have layout performed first.
      const auto* a_inline = a.inline_container.container.Get();
      const auto* b_inline = b.inline_container.container.Get();
      if (a_inline != b_inline) {
        const wtf_size_t a_inline_depth = a_inline ? a_inline->Depth() : 0u;
        const wtf_size_t b_inline_depth = b_inline ? b_inline->Depth() : 0u;
        if (a_inline_depth > b_inline_depth) {
          return true;
        }
        if (b_inline_depth > a_inline_depth) {
          return false;
        }
      }
      return a.box->IsBeforeInPreOrder(*b.box);
    };
    std::sort(oof_positioned_candidates_.begin(),
              oof_positioned_candidates_.end(), compare);
    oof_candidates_may_have_anchors_ = false;
  }
  std::swap(oof_positioned_candidates_, *candidates);
}

void FragmentBuilder::ClearOutOfFlowPositionedCandidates() {
  oof_candidates_may_have_anchors_ = false;
  oof_positioned_candidates_.clear();
}

void FragmentBuilder::AddMulticolWithPendingOOFs(
    const BlockNode& multicol,
    MulticolWithPendingOofs<LogicalOffset>* multicol_info) {
  DCHECK(!RuntimeEnabledFeatures::FragmentedOofInCbEnabled());
  DCHECK(multicol.GetLayoutBox()->IsMulticolContainer());
  auto it = multicols_with_pending_oofs_.find(multicol.GetLayoutBox());
  if (it != multicols_with_pending_oofs_.end())
    return;
  multicols_with_pending_oofs_.insert(multicol.GetLayoutBox(), multicol_info);
}

void FragmentBuilder::SwapMulticolsWithPendingOOFs(
    MulticolCollection* multicols_with_pending_oofs) {
  DCHECK(!RuntimeEnabledFeatures::FragmentedOofInCbEnabled());
  DCHECK(multicols_with_pending_oofs->empty());
  std::swap(multicols_with_pending_oofs_, *multicols_with_pending_oofs);
}

void FragmentBuilder::SwapOutOfFlowFragmentainerDescendants(
    HeapVector<LogicalOofNodeForFragmentation>* descendants) {
  DCHECK(!RuntimeEnabledFeatures::FragmentedOofInCbEnabled());
  DCHECK(descendants->empty());
  // If we have anchors *somewhere* in below the OOFs we need to ensure they
  // are in pre-order so we perform layout in the correct order.
  if (oof_fragmentainer_descendants_may_have_anchors_) {
    std::sort(oof_positioned_fragmentainer_descendants_.begin(),
              oof_positioned_fragmentainer_descendants_.end(),
              [](const LogicalOofNodeForFragmentation& a,
                 const LogicalOofNodeForFragmentation& b) {
                return a.box->IsBeforeInPreOrder(*b.box);
              });
    oof_fragmentainer_descendants_may_have_anchors_ = false;
  }
  std::swap(oof_positioned_fragmentainer_descendants_, *descendants);
}

void FragmentBuilder::TransferOutOfFlowCandidates(
    FragmentBuilder* destination_builder,
    LogicalOffset additional_offset,
    const MulticolWithPendingOofs<LogicalOffset>* multicol) {
  DCHECK(!RuntimeEnabledFeatures::FragmentedOofInCbEnabled());
  for (auto& candidate : oof_positioned_candidates_) {
    BlockNode node = candidate.Node();
    candidate.static_position.offset += additional_offset;
    if (multicol && multicol->fixedpos_containing_block.Fragment() &&
        node.Style().GetPosition() == EPosition::kFixed) {
      // A fixedpos containing block was found in |multicol|. Add the fixedpos
      // as a fragmentainer descendant instead.
      DCHECK(!candidate.inline_container.container);
      destination_builder->AddOutOfFlowFragmentainerDescendant(
          {node, candidate.static_position,
           !!candidate.requires_content_before_breaking,
           multicol->fixedpos_inline_container,
           multicol->fixedpos_containing_block,
           multicol->fixedpos_containing_block,
           multicol->fixedpos_inline_container});
      continue;
    }
    destination_builder->oof_positioned_candidates_.emplace_back(candidate);
  }
  destination_builder->oof_candidates_may_have_anchors_ |=
      oof_candidates_may_have_anchors_;

  ClearOutOfFlowPositionedCandidates();
}

void FragmentBuilder::MoveOutOfFlowDescendantCandidatesToDescendants() {
  DCHECK(oof_positioned_descendants_.empty());
  std::swap(oof_positioned_candidates_, oof_positioned_descendants_);

  if (!layout_object_->IsInline())
    return;

  for (auto& candidate : oof_positioned_descendants_) {
    // If we are inside the inline algorithm, (and creating a fragment for a
    // <span> or similar), we may add a child (e.g. an atomic-inline) which has
    // OOF descandants.
    //
    // This checks if the object creating this box will be the container for
    // the given descendant.
    if (!candidate.inline_container.container &&
        IsInlineContainerForNode(candidate.Node(), layout_object_)) {
      candidate.inline_container = OofInlineContainer<LogicalOffset>(
          To<LayoutInline>(layout_object_),
          /* relative_offset */ LogicalOffset());
    }
  }
}

LayoutUnit FragmentBuilder::BlockOffsetAdjustmentForFragmentainer(
    LayoutUnit fragmentainer_consumed_block_size) const {
  if (RuntimeEnabledFeatures::FragmentedOofInCbEnabled()) {
    return LayoutUnit();
  }
  if (IsFragmentainerBoxType() && PreviousBreakToken()) {
    return To<BlockBreakToken>(PreviousBreakToken())->ConsumedBlockSize();
  }
  return fragmentainer_consumed_block_size;
}

void FragmentBuilder::PropagateOOFPositionedInfo(
    const PhysicalFragment& fragment,
    LogicalOffset offset,
    LogicalOffset relative_offset,
    LogicalOffset offset_adjustment,
    const OofInlineContainer<LogicalOffset>* inline_container,
    LayoutUnit containing_block_adjustment,
    const OofContainingBlock<LogicalOffset>* containing_block,
    const OofContainingBlock<LogicalOffset>* fixedpos_containing_block,
    const OofInlineContainer<LogicalOffset>* fixedpos_inline_container,
    LogicalOffset additional_fixedpos_offset) {
  // Calling this method without any work to do is expensive, even if it ends up
  // skipping all its parts (probably due to its size). Make sure that we have a
  // reason to be here.
  DCHECK(fragment.NeedsOOFPositionedInfoPropagation());

  LogicalOffset adjusted_offset = offset + offset_adjustment + relative_offset;

  // Collect the child's out of flow descendants.
  const WritingModeConverter converter(GetWritingDirection(), fragment.Size());
  for (const PhysicalOofPositionedNode& descendant :
       fragment.OutOfFlowPositionedDescendants()) {
    BlockNode node = descendant.Node();
    LogicalStaticPosition static_position =
        descendant.StaticPosition().ConvertToLogical(converter);

    OofInlineContainer<LogicalOffset> new_inline_container;
    if (descendant.inline_container.container) {
      new_inline_container.container = descendant.inline_container.container;
      new_inline_container.relative_offset =
          converter.ToLogical(descendant.inline_container.relative_offset,
                              PhysicalSize()) +
          relative_offset;
    } else if (inline_container &&
               IsInlineContainerForNode(node, inline_container->container)) {
      new_inline_container = *inline_container;
    }

    // If an OOF element is inside a fragmentation context, it will be laid out
    // once it reaches the fragmentation context root. However, if such OOF
    // elements have fixedpos descendants, those descendants will not find their
    // containing block if the containing block lives inside the fragmentation
    // context root. In this case, the containing block will be passed in via
    // |fixedpos_containing_block|. If one exists, add the fixedpos as a
    // fragmentainer descendant with the correct containing block and static
    // position. In the case of nested fragmentation, the fixedpos containing
    // block may be in an outer fragmentation context root. In such cases,
    // the fixedpos will be added as a fragmentainer descendant at a later time.
    // However, an |additional_fixedpos_offset| should be applied if one is
    // provided.
    if (!RuntimeEnabledFeatures::FragmentedOofInCbEnabled() &&
        (fixedpos_containing_block ||
         additional_fixedpos_offset != LogicalOffset()) &&
        node.Style().GetPosition() == EPosition::kFixed) {
      static_position.offset += additional_fixedpos_offset;
      // Relative offsets should be applied after fragmentation. However, if
      // there is any relative offset that occurrend before the fixedpos reached
      // its containing block, that relative offset should be applied to the
      // static position (before fragmentation).
      static_position.offset +=
          relative_offset - fixedpos_containing_block->RelativeOffset();
      if (fixedpos_inline_container)
        static_position.offset -= fixedpos_inline_container->relative_offset;
      // The containing block for fixed-positioned elements should normally
      // already be laid out, and therefore have a fragment - with one
      // exception: If this is the pagination root, it obviously won't have a
      // fragment, since it hasn't finished layout yet. But we still need to
      // propagate the fixed-positioned descendant, so that it gets laid out
      // inside the fragmentation context (and repeated on every page), instead
      // of becoming a direct child of the LayoutView fragment (and thus a
      // sibling of the page fragments).
      if (fixedpos_containing_block &&
          (fixedpos_containing_block->Fragment() || node_.IsPaginatedRoot())) {
        OofInlineContainer<LogicalOffset> new_fixedpos_inline_container;
        if (fixedpos_inline_container)
          new_fixedpos_inline_container = *fixedpos_inline_container;
        AddOutOfFlowFragmentainerDescendant(
            {node, static_position,
             !!descendant.requires_content_before_breaking,
             new_fixedpos_inline_container, *fixedpos_containing_block,
             *fixedpos_containing_block, new_fixedpos_inline_container});
        continue;
      }
    }
    static_position.offset += adjusted_offset;

    // |oof_positioned_candidates_| should not have duplicated entries.
    DCHECK(!base::Contains(oof_positioned_candidates_, node,
                           &LogicalOofPositionedNode::Node));
    oof_candidates_may_have_anchors_ |= node.MayContainAnchor();
    oof_positioned_candidates_.emplace_back(
        node, descendant.break_token, static_position,
        descendant.requires_content_before_breaking, new_inline_container);
  }

  const auto* oof_data = fragment.GetFragmentedOofData();
  if (!oof_data)
    return;
  DCHECK(!oof_data->multicols_with_pending_oofs.empty() ||
         !oof_data->oof_positioned_fragmentainer_descendants.empty());
  const auto* box_fragment = DynamicTo<PhysicalBoxFragment>(&fragment);
  bool is_column_spanner = box_fragment && box_fragment->IsColumnSpanAll();

  if (!oof_data->multicols_with_pending_oofs.empty()) {
    const auto& multicols_with_pending_oofs =
        oof_data->multicols_with_pending_oofs;
    for (auto& multicol : multicols_with_pending_oofs) {
      auto& multicol_info = multicol.value;
      LogicalOffset multicol_offset =
          converter.ToLogical(multicol_info->multicol_offset, PhysicalSize());

      LogicalOffset fixedpos_inline_relative_offset = converter.ToLogical(
          multicol_info->fixedpos_inline_container.relative_offset,
          PhysicalSize());
      OofInlineContainer<LogicalOffset> new_fixedpos_inline_container(
          multicol_info->fixedpos_inline_container.container,
          fixedpos_inline_relative_offset);
      const PhysicalFragment* fixedpos_containing_block_fragment =
          multicol_info->fixedpos_containing_block.Fragment();

      AdjustFixedposContainerInfo(box_fragment, relative_offset,
                                  &new_fixedpos_inline_container,
                                  &fixedpos_containing_block_fragment);

      // If a fixedpos containing block was found, the |multicol_offset|
      // should remain relative to the fixedpos containing block. Otherwise,
      // continue to adjust the |multicol_offset| to be relative to the current
      // |fragment|.
      LogicalOffset fixedpos_containing_block_offset;
      LogicalOffset fixedpos_containing_block_rel_offset;
      bool is_inside_column_spanner =
          multicol_info->fixedpos_containing_block.IsInsideColumnSpanner();
      if (fixedpos_containing_block_fragment) {
        fixedpos_containing_block_offset = converter.ToLogical(
            multicol_info->fixedpos_containing_block.Offset(),
            fixedpos_containing_block_fragment->Size());
        fixedpos_containing_block_rel_offset = RelativeInsetToLogical(
            multicol_info->fixedpos_containing_block.RelativeOffset(),
            GetWritingDirection());
        fixedpos_containing_block_rel_offset += relative_offset;
        // We want the fixedpos containing block offset to be the offset from
        // the containing block to the top of the fragmentation context root,
        // such that it includes the block offset contributions of previous
        // fragmentainers. The block contribution from previous fragmentainers
        // has already been applied. As such, avoid unnecessarily adding an
        // additional inline/block offset of any fragmentainers.
        if (!fragment.IsFragmentainerBox())
          fixedpos_containing_block_offset += offset;
        fixedpos_containing_block_offset.block_offset +=
            containing_block_adjustment;

        if (is_column_spanner)
          is_inside_column_spanner = true;
      } else {
        multicol_offset += adjusted_offset;
      }

      // TODO(layout-dev): Adjust any clipped container block-offset. For now,
      // just reset it, rather than passing an incorrect value.
      std::optional<LayoutUnit> fixedpos_clipped_container_block_offset;

      AddMulticolWithPendingOOFs(
          BlockNode(multicol.key),
          MakeGarbageCollected<MulticolWithPendingOofs<LogicalOffset>>(
              multicol_offset,
              OofContainingBlock<LogicalOffset>(
                  fixedpos_containing_block_offset,
                  fixedpos_containing_block_rel_offset,
                  fixedpos_containing_block_fragment,
                  fixedpos_clipped_container_block_offset,
                  is_inside_column_spanner),
              new_fixedpos_inline_container));
    }
  }

  PropagateOOFFragmentainerDescendants(
      fragment, offset, relative_offset, containing_block_adjustment,
      containing_block, fixedpos_containing_block);
}

void FragmentBuilder::PropagateOOFFragmentainerDescendants(
    const PhysicalFragment& fragment,
    LogicalOffset offset,
    LogicalOffset relative_offset,
    LayoutUnit containing_block_adjustment,
    const OofContainingBlock<LogicalOffset>* containing_block,
    const OofContainingBlock<LogicalOffset>* fixedpos_containing_block,
    HeapVector<LogicalOofNodeForFragmentation>* out_list) {
  DCHECK(!RuntimeEnabledFeatures::FragmentedOofInCbEnabled());
  const auto* oof_data = fragment.GetFragmentedOofData();
  if (!oof_data || oof_data->oof_positioned_fragmentainer_descendants.empty())
    return;

  const WritingModeConverter converter(GetWritingDirection(), fragment.Size());
  const auto* box_fragment = DynamicTo<PhysicalBoxFragment>(&fragment);
  bool is_column_spanner = box_fragment && box_fragment->IsColumnSpanAll();

  for (const PhysicalOofNodeForFragmentation& descendant :
       oof_data->oof_positioned_fragmentainer_descendants) {
    const PhysicalFragment* containing_block_fragment =
        descendant.containing_block.Fragment();
    bool container_inside_column_spanner =
        descendant.containing_block.IsInsideColumnSpanner();
    bool fixedpos_container_inside_column_spanner =
        descendant.fixedpos_containing_block.IsInsideColumnSpanner();

    if (!containing_block_fragment) {
      DCHECK(box_fragment);
      containing_block_fragment = box_fragment;
    } else if (box_fragment && box_fragment->IsFragmentationContextRoot()) {
      // If we find a multicol with OOF positioned fragmentainer descendants,
      // then that multicol is an inner multicol with pending OOFs. Those OOFs
      // will be laid out inside the inner multicol when we reach the
      // outermost fragmentation context, so we should not propagate those
      // OOFs up the tree any further. However, if the containing block is
      // inside a column spanner contained by the current fragmentation root, we
      // should continue to propagate that OOF up the tree so it can be laid out
      // in the next fragmentation context.
      if (container_inside_column_spanner) {
        // Reset the OOF node's column spanner tags so that we don't propagate
        // the OOF past the next fragmentation context root ancestor.
        container_inside_column_spanner = false;
        fixedpos_container_inside_column_spanner = false;
      } else {
        DCHECK(!fixedpos_container_inside_column_spanner);
        continue;
      }
    }

    if (is_column_spanner)
      container_inside_column_spanner = true;

    LogicalOffset containing_block_offset =
        converter.ToLogical(descendant.containing_block.Offset(),
                            containing_block_fragment->Size());
    LogicalOffset containing_block_rel_offset = RelativeInsetToLogical(
        descendant.containing_block.RelativeOffset(), GetWritingDirection());
    containing_block_rel_offset += relative_offset;
    if (!fragment.IsFragmentainerBox())
      containing_block_offset += offset;
    containing_block_offset.block_offset += containing_block_adjustment;

    // If the containing block of the OOF is inside a clipped container, update
    // this offset.
    auto UpdatedClippedContainerBlockOffset =
        [&containing_block, &offset, &fragment,
         &containing_block_adjustment](const OofContainingBlock<PhysicalOffset>&
                                           descendant_containing_block) {
          std::optional<LayoutUnit> clipped_container_offset =
              descendant_containing_block.ClippedContainerBlockOffset();
          if (!clipped_container_offset &&
              fragment.HasNonVisibleBlockOverflow()) {
            // We just found a clipped container.
            clipped_container_offset.emplace();
          }
          if (clipped_container_offset) {
            // We're inside a clipped container. Adjust the offset.
            if (!fragment.IsFragmentainerBox()) {
              *clipped_container_offset += offset.block_offset;
            }
            *clipped_container_offset += containing_block_adjustment;
          }
          if (!clipped_container_offset && containing_block &&
              containing_block->ClippedContainerBlockOffset()) {
            // We were not inside a clipped container, but we're contained by an
            // OOF which is inside one. E.g.: <clipped><relpos><abspos><abspos>
            // This happens when we're at the inner abspos in this example.
            clipped_container_offset =
                containing_block->ClippedContainerBlockOffset();
          }
          return clipped_container_offset;
        };

    std::optional<LayoutUnit> clipped_container_block_offset =
        UpdatedClippedContainerBlockOffset(descendant.containing_block);

    LogicalOffset inline_relative_offset = converter.ToLogical(
        descendant.inline_container.relative_offset, PhysicalSize());
    OofInlineContainer<LogicalOffset> new_inline_container(
        descendant.inline_container.container, inline_relative_offset);

    // The static position should remain relative to its containing block
    // fragment.
    const WritingModeConverter containing_block_converter(
        GetWritingDirection(), containing_block_fragment->Size());
    LogicalStaticPosition static_position =
        descendant.StaticPosition().ConvertToLogical(
            containing_block_converter);

    // The relative offset should be applied after fragmentation. Subtract out
    // the accumulated relative offset from the inline container to the
    // containing block so that it can be re-applied at the correct time.
    if (new_inline_container.container && box_fragment &&
        containing_block_fragment == box_fragment)
      static_position.offset -= inline_relative_offset;

    LogicalOffset fixedpos_inline_relative_offset = converter.ToLogical(
        descendant.fixedpos_inline_container.relative_offset, PhysicalSize());
    OofInlineContainer<LogicalOffset> new_fixedpos_inline_container(
        descendant.fixedpos_inline_container.container,
        fixedpos_inline_relative_offset);
    const PhysicalFragment* fixedpos_containing_block_fragment =
        descendant.fixedpos_containing_block.Fragment();

    AdjustFixedposContainerInfo(
        box_fragment, relative_offset, &new_fixedpos_inline_container,
        &fixedpos_containing_block_fragment, &new_inline_container);

    LogicalOffset fixedpos_containing_block_offset;
    LogicalOffset fixedpos_containing_block_rel_offset;
    std::optional<LayoutUnit> fixedpos_clipped_container_block_offset;
    if (fixedpos_containing_block_fragment) {
      fixedpos_containing_block_offset =
          converter.ToLogical(descendant.fixedpos_containing_block.Offset(),
                              fixedpos_containing_block_fragment->Size());
      fixedpos_containing_block_rel_offset = RelativeInsetToLogical(
          descendant.fixedpos_containing_block.RelativeOffset(),
          GetWritingDirection());
      fixedpos_containing_block_rel_offset += relative_offset;
      if (!fragment.IsFragmentainerBox())
        fixedpos_containing_block_offset += offset;
      fixedpos_containing_block_offset.block_offset +=
          containing_block_adjustment;

      fixedpos_clipped_container_block_offset =
          UpdatedClippedContainerBlockOffset(
              descendant.fixedpos_containing_block);

      if (is_column_spanner)
        fixedpos_container_inside_column_spanner = true;
    }

    if (!fixedpos_containing_block_fragment && fixedpos_containing_block) {
      fixedpos_containing_block_fragment =
          fixedpos_containing_block->Fragment();
      fixedpos_containing_block_offset = fixedpos_containing_block->Offset();
      fixedpos_containing_block_rel_offset =
          fixedpos_containing_block->RelativeOffset();
    }
    LogicalOofNodeForFragmentation oof_node(
        descendant.Node(), static_position,
        descendant.requires_content_before_breaking, new_inline_container,
        OofContainingBlock<LogicalOffset>(
            containing_block_offset, containing_block_rel_offset,
            containing_block_fragment, clipped_container_block_offset,
            container_inside_column_spanner),
        OofContainingBlock<LogicalOffset>(
            fixedpos_containing_block_offset,
            fixedpos_containing_block_rel_offset,
            fixedpos_containing_block_fragment,
            fixedpos_clipped_container_block_offset,
            fixedpos_container_inside_column_spanner),
        new_fixedpos_inline_container);

    if (out_list) {
      out_list->emplace_back(oof_node);
    } else {
      AddOutOfFlowFragmentainerDescendant(oof_node);
    }
  }
}

void FragmentBuilder::AdjustFixedposContainerInfo(
    const PhysicalFragment* box_fragment,
    LogicalOffset relative_offset,
    OofInlineContainer<LogicalOffset>* fixedpos_inline_container,
    const PhysicalFragment** fixedpos_containing_block_fragment,
    const OofInlineContainer<LogicalOffset>* current_inline_container) const {
  DCHECK(fixedpos_inline_container);
  DCHECK(fixedpos_containing_block_fragment);
  if (!box_fragment)
    return;

  if (!*fixedpos_containing_block_fragment && box_fragment->GetLayoutObject()) {
    if (current_inline_container && current_inline_container->container &&
        current_inline_container->container->CanContainFixedPositionObjects()) {
      *fixedpos_inline_container = *current_inline_container;
      *fixedpos_containing_block_fragment = box_fragment;
    } else if (box_fragment->GetLayoutObject()
                   ->CanContainFixedPositionObjects()) {
      if (!fixedpos_inline_container->container &&
          box_fragment->GetLayoutObject()->IsLayoutInline()) {
        *fixedpos_inline_container = OofInlineContainer<LogicalOffset>(
            To<LayoutInline>(box_fragment->GetLayoutObject()), relative_offset);
      } else if (!box_fragment->GetLayoutObject()->IsLayoutInline()) {
        *fixedpos_containing_block_fragment = box_fragment;
      }
    } else if (fixedpos_inline_container->container) {
      // Candidates whose containing block is inline are always positioned
      // inside closest parent block flow.
      if (box_fragment->GetLayoutObject() ==
          fixedpos_inline_container->container->ContainingBlock())
        *fixedpos_containing_block_fragment = box_fragment;
    }
  }
}

void FragmentBuilder::PropagateSpaceShortage(
    std::optional<LayoutUnit> space_shortage) {
  // Space shortage should only be reported when we already have a tentative
  // fragmentainer block-size. It's meaningless to talk about space shortage
  // in the initial column balancing pass, because then we have no
  // fragmentainer block-size at all, so who's to tell what's too short or
  // not?
  DCHECK(!IsInitialColumnBalancingPass());
  UpdateMinimalSpaceShortage(space_shortage, &minimal_space_shortage_);
}

void FragmentBuilder::Finalize() {
#if DCHECK_IS_ON()
  DCHECK(!is_finalized_);
  is_finalized_ = true;
#endif

  CreateNamedTriggersForSelf();

  has_final_size_ = true;
  PropagateSizeDependentData();
}

const LayoutResult* FragmentBuilder::Abort(LayoutResult::EStatus status) {
  return MakeGarbageCollected<LayoutResult>(
      LayoutResult::FragmentBuilderPassKey(), status, this);
}

#if DCHECK_IS_ON()

String FragmentBuilder::ToString() const {
  StringBuilder builder;
  builder.AppendFormat("FragmentBuilder %.2fx%.2f, Children %u\n",
                       InlineSize().ToFloat(), BlockSize().ToFloat(),
                       children_.size());
  for (auto& child : children_) {
    builder.Append(child.fragment->DumpFragmentTree(
        PhysicalFragment::DumpAll & ~PhysicalFragment::DumpHeaderText));
  }
  return builder.ToString();
}

#endif

void FragmentBuilder::PropagateSizeDependentData() {
  DCHECK(has_final_size_);
  for (const LogicalFragmentLink& link :
       children_with_size_dependent_propagation_) {
    PropagateChildAnchors(*link.fragment, link.offset);
  }
  children_with_size_dependent_propagation_.clear();
}

void FragmentBuilder::SetNamedTrigger(
    const TriggerScopedName& trigger_scoped_name,
    AnimationTrigger* trigger) {
  TriggerScopedNameMap& named_triggers = EnsureNamedTriggers();

  auto it = named_triggers.find(&trigger_scoped_name);
  if (it == named_triggers.end()) {
    named_triggers.Set(&trigger_scoped_name, trigger);
    return;
  }

  if (it->value == trigger) {
    // If we have the same name, scope and trigger, there is nothing to update.
    // We can get here with elements that generate multiple fragments.
    // IsBeforeInPreOrder below doesn't like looking at the same LayoutObjects.
    return;
  }

  DCHECK(trigger->OwningElement());
  DCHECK(trigger->OwningElement()->GetLayoutObject());
  DCHECK(it->value->OwningElement());
  DCHECK(it->value->OwningElement()->GetLayoutObject());
  const LayoutObject* existing_layout_object =
      it->value->OwningElement()->GetLayoutObject();

  if (existing_layout_object->IsBeforeInPreOrder(
          *trigger->OwningElement()->GetLayoutObject())) {
    named_triggers.Set(&trigger_scoped_name, trigger);
    it = named_triggers.find(&trigger_scoped_name);
    DCHECK_EQ(it->value->OwningElement()->GetLayoutObject(),
              trigger->OwningElement()->GetLayoutObject());
  }
}

void FragmentBuilder::PropagateNamedTriggers(const PhysicalFragment& child) {
  if (!child.NamedTriggers()) {
    return;
  }

  const TriggerScopedNameMap* trigger_scoped_name_map = child.NamedTriggers();
  for (const auto& entry : *trigger_scoped_name_map) {
    SetNamedTrigger(*entry.key, entry.value);
  }
}

void FragmentBuilder::CreateNamedTriggersForSelf() {
  if (!node_) {
    return;
  }

  const Element* element = DynamicTo<Element>(node_.GetDOMNode());
  if (!element || !element->NamedTriggers()) {
    return;
  }

  if (const CSSAnimationData* data = Style().Animations()) {
    for (const auto& name : data->TimelineTriggerNameList()) {
      if (name) {
        AnimationTrigger* trigger = element->NamedTrigger(name);
        DCHECK(trigger);

        TriggerScopedName* trigger_scoped_name =
            ToTriggerScopedName(*name, *element);
        SetNamedTrigger(*trigger_scoped_name, trigger);
      }
    }
  }
}

TriggerScopedNameMap& FragmentBuilder::EnsureNamedTriggers() {
  if (!named_triggers_) {
    named_triggers_ = MakeGarbageCollected<TriggerScopedNameMap>();
  }
  return *named_triggers_;
}

}  // namespace blink
