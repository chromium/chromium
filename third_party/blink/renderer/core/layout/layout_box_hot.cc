// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/layout_box.h"

#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/layout/layout_block.h"
#include "third_party/blink/renderer/core/layout/ng/geometry/ng_fragment_geometry.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space.h"
#include "third_party/blink/renderer/core/layout/ng/ng_disable_side_effects_scope.h"
#include "third_party/blink/renderer/core/layout/ng/ng_fragmentation_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_result.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"

namespace blink {

DISABLE_CFI_PERF
bool LayoutBox::ShrinkToAvoidFloats() const {
  NOT_DESTROYED();
  // Floating objects don't shrink.  Objects that don't avoid floats don't
  // shrink.
  if (IsInline() || !CreatesNewFormattingContext() || IsFloating())
    return false;

  // Only auto width objects can possibly shrink to avoid floats.
  if (!StyleRef().Width().IsAuto())
    return false;

  // If the containing block is LayoutNG, we will not let legacy layout deal
  // with positioning of floats or sizing of auto-width new formatting context
  // block level objects adjacent to them.
  if (const auto* containing_block = ContainingBlock()) {
    if (containing_block->IsLayoutNGObject())
      return false;
  }

  // Legends are taken out of the normal flow, and are laid out at the very
  // start of the fieldset, and are therefore not affected by floats (that may
  // appear earlier in the DOM).
  if (IsRenderedLegend())
    return false;

  return true;
}

// Hit Testing
bool LayoutBox::MayIntersect(const HitTestResult& result,
                             const HitTestLocation& hit_test_location,
                             const PhysicalOffset& accumulated_offset) const {
  NOT_DESTROYED();
  // Check if we need to do anything at all.
  // If we have clipping, then we can't have any spillout.
  // TODO(pdr): Why is this optimization not valid for the effective root?
  if (UNLIKELY(IsEffectiveRootScroller()))
    return true;

  PhysicalRect overflow_box;
  if (UNLIKELY(result.GetHitTestRequest().IsHitTestVisualOverflow())) {
    overflow_box = PhysicalVisualOverflowRectIncludingFilters();
  } else {
    overflow_box = PhysicalBorderBoxRect();
    if (!ShouldClipOverflowAlongBothAxis() && HasVisualOverflow()) {
      // PhysicalVisualOverflowRect is an approximation of
      // PhsyicalLayoutOverflowRect excluding self-painting descendants (which
      // hit test by themselves), with false-positive (which won't cause any
      // functional issues) when the point is only in visual overflow, but
      // excluding self-painting descendants is more important for performance.
      overflow_box.Unite(PhysicalVisualOverflowRect());
    }
  }

  overflow_box.Move(accumulated_offset);
  return hit_test_location.Intersects(overflow_box);
}

bool LayoutBox::CanBeProgrammaticallyScrolled() const {
  NOT_DESTROYED();
  Node* node = GetNode();
  if (node && node->IsDocumentNode())
    return true;

  if (!IsScrollContainer())
    return false;

  bool has_scrollable_overflow =
      HasScrollableOverflowX() || HasScrollableOverflowY();
  if (ScrollsOverflow() && has_scrollable_overflow)
    return true;

  return node && IsEditable(*node);
}

const NGLayoutResult* LayoutBox::CachedLayoutResult(
    const NGConstraintSpace& new_space,
    const NGBlockBreakToken* break_token,
    const NGEarlyBreak* early_break,
    const NGColumnSpannerPath* column_spanner_path,
    absl::optional<NGFragmentGeometry>* initial_fragment_geometry,
    NGLayoutCacheStatus* out_cache_status) {
  NOT_DESTROYED();
  *out_cache_status = NGLayoutCacheStatus::kNeedsLayout;

  const bool use_layout_cache_slot =
      new_space.CacheSlot() == NGCacheSlot::kLayout && !layout_results_.empty();
  const NGLayoutResult* cached_layout_result =
      use_layout_cache_slot ? GetCachedLayoutResult(break_token)
                            : GetCachedMeasureResult();

  if (!cached_layout_result)
    return nullptr;

  if (early_break)
    return nullptr;

  DCHECK_EQ(cached_layout_result->Status(), NGLayoutResult::kSuccess);

  // Set our initial temporary cache status to "hit".
  NGLayoutCacheStatus cache_status = NGLayoutCacheStatus::kHit;

  // If the display-lock blocked child layout, then we don't clear child needs
  // layout bits. However, we can still use the cached result, since we will
  // re-layout when unlocking.
  bool is_blocked_by_display_lock = ChildLayoutBlockedByDisplayLock();
  bool child_needs_layout_unless_locked =
      !is_blocked_by_display_lock &&
      (PosChildNeedsLayout() || NormalChildNeedsLayout());

  const NGPhysicalBoxFragment& physical_fragment =
      To<NGPhysicalBoxFragment>(cached_layout_result->PhysicalFragment());

  // No fun allowed for repeated content.
  if ((physical_fragment.BreakToken() &&
       physical_fragment.BreakToken()->IsRepeated()) ||
      (break_token && break_token->IsRepeated()))
    return nullptr;

  if (SelfNeedsLayoutForStyle() || child_needs_layout_unless_locked ||
      NeedsSimplifiedNormalFlowLayout() ||
      (NeedsPositionedMovementLayout() &&
       !NeedsPositionedMovementLayoutOnly())) {
    if (!ChildrenInline()) {
      // Check if we only need "simplified" layout. We don't abort yet, as we
      // need to check if other things (like floats) will require us to perform
      // a full layout.
      if (!NeedsSimplifiedLayoutOnly())
        return nullptr;

      cache_status = NGLayoutCacheStatus::kNeedsSimplifiedLayout;
    } else if (!NeedsSimplifiedLayoutOnly() ||
               NeedsSimplifiedNormalFlowLayout()) {
      // We don't regenerate any lineboxes during our "simplified" layout pass.
      // If something needs "simplified" layout within a linebox, (e.g. an
      // atomic-inline) we miss the cache.

      // Check if some of line boxes are reusable.

      // Only for the layout cache slot. Measure has several special
      // optimizations that makes reusing lines complicated.
      if (!use_layout_cache_slot)
        return nullptr;

      if (SelfNeedsLayout())
        return nullptr;

      if (!physical_fragment.HasItems())
        return nullptr;

      // Propagating OOF needs re-layout.
      if (physical_fragment.NeedsOOFPositionedInfoPropagation())
        return nullptr;

      // Any floats might need to move, causing lines to wrap differently,
      // needing re-layout, either in cached result or in new constraint space.
      if (!cached_layout_result->ExclusionSpace().IsEmpty() ||
          new_space.HasFloats())
        return nullptr;

      cache_status = NGLayoutCacheStatus::kCanReuseLines;
    } else {
      cache_status = NGLayoutCacheStatus::kNeedsSimplifiedLayout;
    }
  }

  NGBlockNode node(this);
  NGLayoutCacheStatus size_cache_status = CalculateSizeBasedLayoutCacheStatus(
      node, break_token, *cached_layout_result, new_space,
      initial_fragment_geometry);

  // If our size may change (or we know a descendants size may change), we miss
  // the cache.
  if (size_cache_status == NGLayoutCacheStatus::kNeedsLayout)
    return nullptr;

  // If we need simplified layout, but the cached fragment's children are not
  // valid (see comment in `SetCachedLayoutResult`), don't return the fragment,
  // since it will be used to iteration the invalid children when running
  // simplified layout.
  if ((!physical_fragment.ChildrenValid() || IsShapingDeferred()) &&
      (size_cache_status == NGLayoutCacheStatus::kNeedsSimplifiedLayout ||
       cache_status == NGLayoutCacheStatus::kNeedsSimplifiedLayout))
    return nullptr;

  // Update our temporary cache status, if the size cache check indicated we
  // might need simplified layout.
  if (size_cache_status == NGLayoutCacheStatus::kNeedsSimplifiedLayout &&
      cache_status == NGLayoutCacheStatus::kHit)
    cache_status = NGLayoutCacheStatus::kNeedsSimplifiedLayout;

  if (cache_status == NGLayoutCacheStatus::kNeedsSimplifiedLayout) {
    // Only allow simplified layout for non-replaced boxes.
    if (IsLayoutReplaced())
      return nullptr;

    // Simplified layout requires children to have a cached layout result. If
    // the current box has no cached layout result, its children might not,
    // either.
    if (!use_layout_cache_slot && !GetCachedLayoutResult(break_token))
      return nullptr;
  }

  LayoutUnit bfc_line_offset = new_space.BfcOffset().line_offset;
  absl::optional<LayoutUnit> bfc_block_offset =
      cached_layout_result->BfcBlockOffset();
  LayoutUnit block_offset_delta;
  NGMarginStrut end_margin_strut = cached_layout_result->EndMarginStrut();

  bool are_bfc_offsets_equal;
  bool is_margin_strut_equal;
  bool is_exclusion_space_equal;
  bool is_fragmented = IsBreakInside(break_token) ||
                       physical_fragment.BreakToken() ||
                       PhysicalFragmentCount() > 1;

  {
    const NGConstraintSpace& old_space =
        cached_layout_result->GetConstraintSpaceForCaching();

    // Check the BFC offset. Even if they don't match, there're some cases we
    // can still reuse the fragment.
    are_bfc_offsets_equal =
        new_space.BfcOffset() == old_space.BfcOffset() &&
        new_space.ExpectedBfcBlockOffset() ==
            old_space.ExpectedBfcBlockOffset() &&
        new_space.ForcedBfcBlockOffset() == old_space.ForcedBfcBlockOffset();

    is_margin_strut_equal = new_space.MarginStrut() == old_space.MarginStrut();
    is_exclusion_space_equal =
        new_space.ExclusionSpace() == old_space.ExclusionSpace();
    bool is_clearance_offset_equal =
        new_space.ClearanceOffset() == old_space.ClearanceOffset();

    bool is_new_formatting_context =
        physical_fragment.IsFormattingContextRoot();

    // If a node *doesn't* establish a new formatting context it may be affected
    // by floats, or clearance.
    // If anything has changed prior to us (different exclusion space, etc), we
    // need to perform a series of additional checks if we can still reuse this
    // layout result.
    if (!is_new_formatting_context &&
        (!are_bfc_offsets_equal || !is_exclusion_space_equal ||
         !is_margin_strut_equal || !is_clearance_offset_equal)) {
      DCHECK(!CreatesNewFormattingContext());

      // If we have a different BFC offset, or exclusion space we can't perform
      // "simplified" layout.
      // This may occur if our %-block-size has changed (allowing "simplified"
      // layout), and we've been pushed down in the BFC coordinate space by a
      // sibling.
      // The "simplified" layout algorithm doesn't have the required logic to
      // shift any added exclusions within the output exclusion space.
      if (cache_status == NGLayoutCacheStatus::kNeedsSimplifiedLayout ||
          cache_status == NGLayoutCacheStatus::kCanReuseLines)
        return nullptr;

      DCHECK_EQ(cache_status, NGLayoutCacheStatus::kHit);

      if (!MaySkipLayoutWithinBlockFormattingContext(
              *cached_layout_result, new_space, &bfc_block_offset,
              &block_offset_delta, &end_margin_strut))
        return nullptr;
    }

    if (UNLIKELY(new_space.HasBlockFragmentation())) {
      DCHECK(old_space.HasBlockFragmentation());

      // Sometimes we perform simplified layout on a block-flow which is just
      // growing in block-size. When fragmentation is present we can't hit the
      // cache for these cases as we may grow past the fragmentation line.
      if (cache_status != NGLayoutCacheStatus::kHit)
        return nullptr;

      // Miss the cache if we have nested multicol containers inside that also
      // have OOF descendants. OOFs in nested multicol containers are handled in
      // a special way during layout: When we have returned to the outermost
      // fragmentation context root, we'll go through the nested multicol
      // containers and lay out the OOFs inside. If we do that after having hit
      // the cache (and thus kept the fragment with the OOF), we'd end up with
      // extraneous OOF fragments.
      if (UNLIKELY(physical_fragment.HasNestedMulticolsWithOOFs()))
        return nullptr;

      // Any fragmented out-of-flow positioned items will be placed once we
      // reach the fragmentation context root rather than the containing block,
      // so we should miss the cache in this case to ensure that such OOF
      // descendants are laid out correctly.
      if (physical_fragment.HasOutOfFlowFragmentChild())
        return nullptr;

      if (column_spanner_path || cached_layout_result->ColumnSpannerPath())
        return nullptr;

      // Break appeal may have been reduced because the fragment crosses the
      // fragmentation line, to send a strong signal to break before it
      // instead. If we actually ended up breaking before it, this break appeal
      // may no longer be valid, since there could be more room in the next
      // fragmentainer. Miss the cache.
      //
      // TODO(mstensho): Maybe this shouldn't be necessary. Look into how
      // FinishFragmentation() clamps break appeal down to
      // kBreakAppealLastResort. Maybe there are better ways.
      if (break_token && break_token->IsBreakBefore() &&
          cached_layout_result->BreakAppeal() < kBreakAppealPerfect)
        return nullptr;

      // If the node didn't break into multiple fragments, we might be able to
      // re-use the result. If the fragmentainer block-size has changed, or if
      // the fragment's block-offset within the fragmentainer has changed, we
      // need to check if the node will still fit as one fragment. If we cannot
      // be sure that this is the case, we need to miss the cache.
      if (new_space.IsInitialColumnBalancingPass()) {
        if (!old_space.IsInitialColumnBalancingPass()) {
          // If the previous result was generated with a known fragmentainer
          // size (i.e. not in the initial column balancing pass),
          // TallestUnbreakableBlockSize() won't be stored in the layout result,
          // because we currently only calculate this in the initial column
          // balancing pass. Since we're now in an initial column balancing pass
          // again, we cannot re-use the result, because not propagating the
          // tallest unbreakable block-size might cause incorrect layout.
          //
          // Another problem is OOF descendants. In the initial column balancing
          // pass, they affect FragmentainerBlockSize() (because OOFs are
          // supposed to affect column balancing), while in actual layout
          // passes, OOFs will escape their actual containing block and become
          // direct children of some fragmentainer. In other words, any relevant
          // information about OOFs and how they might affect balancing has been
          // lost.
          return nullptr;
        }
        // (On the other hand, if the previous result was also generated in the
        // initial column balancing pass, we don't need to perform any
        // additional checks.)
      } else if (new_space.FragmentainerBlockSize() !=
                     old_space.FragmentainerBlockSize() ||
                 new_space.FragmentainerOffset() !=
                     old_space.FragmentainerOffset()) {
        // The fragment block-offset will either change, or the fragmentainer
        // block-size has changed. If the node is fragmented, we're going to
        // have to refragment, since the fragmentation line has moved,
        // relatively to the fragment.
        if (is_fragmented)
          return nullptr;

        // Fragmentation inside a nested multicol container depends on the
        // amount of remaining space in the outer fragmentation context, so if
        // this has changed, we cannot necessarily re-use it. To keep things
        // simple (lol, take a look around!), just don't re-use a nested
        // fragmentation context root.
        if (physical_fragment.IsFragmentationContextRoot())
          return nullptr;

        // If the fragment was forced to stay in a fragmentainer (even if it
        // overflowed), BlockSizeForFragmentation() cannot be used for cache
        // testing.
        if (cached_layout_result->IsBlockSizeForFragmentationClamped())
          return nullptr;

        // If the fragment was truncated at the fragmentation line, and since we
        // have now moved relatively to the fragmentation line, we cannot re-use
        // the fragment.
        if (cached_layout_result->IsTruncatedByFragmentationLine())
          return nullptr;

        // TODO(layout-dev): This likely shouldn't be scoped to just OOFs, but
        // scoping it more widely results in several perf regressions[1].
        //
        // [1] https://bugs.chromium.org/p/chromium/issues/detail?id=1362550
        if (node.IsOutOfFlowPositioned()) {
          // If the fragmentainer size has changed, and there previously was
          // space shortage reported, we should re-run layout to avoid reporting
          // the same space shortage again.
          absl::optional<LayoutUnit> space_shortage =
              cached_layout_result->MinimalSpaceShortage();
          if (space_shortage && *space_shortage > LayoutUnit())
            return nullptr;
        }

        // Returns true if there are any floats added by |cached_layout_result|
        // which will end up crossing the fragmentation line.
        auto DoFloatsCrossFragmentationLine = [&]() -> bool {
          const auto& result_exclusion_space =
              cached_layout_result->ExclusionSpace();
          if (result_exclusion_space != old_space.ExclusionSpace()) {
            LayoutUnit block_end_offset =
                FragmentainerOffsetAtBfc(new_space) +
                result_exclusion_space.ClearanceOffset(EClear::kBoth);
            if (block_end_offset > new_space.FragmentainerBlockSize())
              return true;
          }
          return false;
        };

        if (!bfc_block_offset && cached_layout_result->IsSelfCollapsing()) {
          // Self-collapsing blocks may have floats and OOF descendants.
          // Checking if floats cross the fragmentation line is easy enough
          // (check the exclusion space), but we currently have no way of
          // checking OOF descendants. OOFs are included in
          // BlockSizeForFragmentation() in the initial column balancing pass
          // only, but since we don't know the start offset of this node,
          // there's nothing we can do about it. Give up if this is the case.
          if (old_space.IsInitialColumnBalancingPass())
            return nullptr;

          if (DoFloatsCrossFragmentationLine())
            return nullptr;
        } else {
          // If floats were added inside an inline formatting context, they
          // might extrude (and not included within the block-size for
          // fragmentation calculation above, unlike block formatting contexts).
          if (physical_fragment.IsInlineFormattingContext() &&
              !is_new_formatting_context) {
            if (DoFloatsCrossFragmentationLine())
              return nullptr;
          }

          // Check if we have content which might cross the fragmentation line.
          //
          // NOTE: It's fine to use NGLayoutResult::BlockSizeForFragmentation()
          // directly here, rather than the helper BlockSizeForFragmentation()
          // in ng_fragmentation_utils.cc, since what the latter does shouldn't
          // matter, since we're not monolithic content
          // (HasBlockFragmentation() is true), and we're not a line box.
          LayoutUnit block_size_for_fragmentation =
              cached_layout_result->BlockSizeForFragmentation();

          LayoutUnit block_end_offset =
              FragmentainerOffsetAtBfc(new_space) +
              bfc_block_offset.value_or(LayoutUnit()) +
              block_size_for_fragmentation;
          if (block_end_offset > new_space.FragmentainerBlockSize())
            return nullptr;
        }

        // Multi-cols behave differently between the initial column balancing
        // pass, and the regular pass (specifically when forced breaks or OOFs
        // are present), we just miss the cache for these cases.
        if (old_space.IsInitialColumnBalancingPass()) {
          if (physical_fragment.HasOutOfFlowInFragmentainerSubtree())
            return nullptr;
          if (auto* block = DynamicTo<LayoutBlock>(this)) {
            if (block->IsFragmentationContextRoot())
              return nullptr;
          }
        }
      }
    }
  }

  if (is_fragmented) {
    if (cached_layout_result->ExclusionSpace().HasFragmentainerBreak()) {
      // The final exclusion space is a processed version of the old one when
      // hitting the cache. One thing we don't support is copying the
      // fragmentation bits over correctly. That's something we could fix, if
      // the new resulting exclusion space otherwise is identical to the old
      // one. But for now, keep it simple, and just give up.
      return nullptr;
    }

    // Simplified layout doesn't support fragmented nodes.
    if (cache_status == NGLayoutCacheStatus::kNeedsSimplifiedLayout)
      return nullptr;
  }

  // We've performed all of the cache checks at this point. If we need
  // "simplified" layout then abort now.
  *out_cache_status = cache_status;
  if (cache_status == NGLayoutCacheStatus::kNeedsSimplifiedLayout ||
      cache_status == NGLayoutCacheStatus::kCanReuseLines)
    return cached_layout_result;

  physical_fragment.CheckType();

  DCHECK_EQ(*out_cache_status, NGLayoutCacheStatus::kHit);

  // We can safely re-use this fragment if we are positioned, and only our
  // position constraints changed (left/top/etc). However we need to clear the
  // dirty layout bit(s). Note that we may be here because we are display locked
  // and have cached a locked layout result. In that case, this function will
  // not clear the child dirty bits.
  if (NeedsLayout())
    ClearNeedsLayout();

  // For example, for elements with a transform change we can re-use the cached
  // result but we still need to recalculate the layout overflow.
  if (use_layout_cache_slot && !is_blocked_by_display_lock &&
      NeedsLayoutOverflowRecalc()) {
#if DCHECK_IS_ON()
    const NGLayoutResult* cloned_cached_layout_result =
        NGLayoutResult::CloneWithPostLayoutFragments(*cached_layout_result);
#endif
    if (!NGDisableSideEffectsScope::IsDisabled())
      RecalcLayoutOverflow();

    // We need to update the cached layout result, as the call to
    // RecalcLayoutOverflow() might have modified it.
    cached_layout_result = GetCachedLayoutResult(break_token);

#if DCHECK_IS_ON()
    // We haven't actually performed simplified layout. Skip the checks for no
    // fragmentation, since it's okay to be fragmented in this case.
    cloned_cached_layout_result->CheckSameForSimplifiedLayout(
        *cached_layout_result, /* check_same_block_size */ true,
        /* check_no_fragmentation*/ false);
#endif
  }

  // Optimization: NGTableConstraintSpaceData can be large, and it is shared
  // between all the rows in a table. Make constraint space table data for
  // reused row fragment be identical to the one used by other row fragments.
  if (IsTableRow() && IsLayoutNGObject()) {
    const_cast<NGConstraintSpace&>(
        cached_layout_result->GetConstraintSpaceForCaching())
        .ReplaceTableRowData(*new_space.TableData(), new_space.TableRowIndex());
  }

  // OOF-positioned nodes have to two-tier cache. The additional cache check
  // runs before the OOF-positioned sizing, and positioning calculations.
  //
  // This additional check compares the percentage resolution size.
  //
  // As a result, the cached layout result always needs to contain the previous
  // percentage resolution size in order for the first-tier cache to work.
  // See |NGBlockNode::CachedLayoutResultForOutOfFlowPositioned|.
  bool needs_cached_result_update =
      node.IsOutOfFlowPositioned() &&
      new_space.PercentageResolutionSize() !=
          cached_layout_result->GetConstraintSpaceForCaching()
              .PercentageResolutionSize();

  // We can safely reuse this result if our BFC and "input" exclusion spaces
  // were equal.
  if (are_bfc_offsets_equal && is_exclusion_space_equal &&
      is_margin_strut_equal && !needs_cached_result_update) {
    // In order not to rebuild the internal derived-geometry "cache" of float
    // data, we need to move this to the new "output" exclusion space.
    cached_layout_result->ExclusionSpace().MoveAndUpdateDerivedGeometry(
        new_space.ExclusionSpace());
    return cached_layout_result;
  }

  const NGLayoutResult* new_result = MakeGarbageCollected<NGLayoutResult>(
      *cached_layout_result, new_space, end_margin_strut, bfc_line_offset,
      bfc_block_offset, block_offset_delta);

  if (needs_cached_result_update && !NGDisableSideEffectsScope::IsDisabled())
    SetCachedLayoutResult(new_result, FragmentIndex(break_token));

  return new_result;
}

void LayoutBox::SetSnapContainer(LayoutBox* new_container) {
  NOT_DESTROYED();
  LayoutBox* old_container = SnapContainer();
  if (old_container == new_container)
    return;

  if (old_container)
    old_container->RemoveSnapArea(*this);

  EnsureRareData().snap_container_ = new_container;

  if (new_container)
    new_container->AddSnapArea(*this);
}

const NGPhysicalBoxFragment* LayoutBox::GetPhysicalFragment(
    wtf_size_t i) const {
  NOT_DESTROYED();
  return &To<NGPhysicalBoxFragment>(layout_results_[i]->PhysicalFragment());
}

}  // namespace blink
