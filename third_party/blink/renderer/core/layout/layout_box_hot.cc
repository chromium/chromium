// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/layout_box.h"

#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/layout/constraint_space.h"
#include "third_party/blink/renderer/core/layout/disable_layout_side_effects_scope.h"
#include "third_party/blink/renderer/core/layout/fragmentation_utils.h"
#include "third_party/blink/renderer/core/layout/geometry/fragment_geometry.h"
#include "third_party/blink/renderer/core/layout/hit_test_location.h"
#include "third_party/blink/renderer/core/layout/hit_test_result.h"
#include "third_party/blink/renderer/core/layout/layout_block.h"
#include "third_party/blink/renderer/core/layout/layout_result.h"
#include "third_party/blink/renderer/core/layout/layout_utils.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/physical_box_fragment.h"

namespace blink {

bool LayoutBox::HasHitTestableOverflow() const {
  // See MayIntersect() for the reason of using HasVisualOverflow here.
  if (!HasVisualOverflow()) {
    return false;
  }
  if (!ShouldClipOverflowAlongBothAxis()) {
    return true;
  }
  return ShouldApplyOverflowClipMargin() &&
         StyleRef().OverflowClipMargin()->GetMargin() > 0;
}

// Hit Testing
bool LayoutBox::MayIntersect(const HitTestResult& result,
                             const HitTestLocation& hit_test_location,
                             const PhysicalOffset& accumulated_offset) const {
  NOT_DESTROYED();
  // Check if we need to do anything at all.
  // The root scroller always fills the whole view.
  if (IsEffectiveRootScroller()) [[unlikely]] {
    return true;
  }

  PhysicalRect overflow_box;
  if (result.GetHitTestRequest().IsHitTestVisualOverflow()) [[unlikely]] {
    overflow_box = VisualOverflowRectIncludingFilters();
  } else if (HasHitTestableOverflow()) {
    // PhysicalVisualOverflowRect is an approximation of
    // ScrollableOverflowRect excluding self-painting descendants (which
    // hit test by themselves), with false-positive (which won't cause any
    // functional issues) when the point is only in visual overflow, but
    // excluding self-painting descendants is more important for performance.
    overflow_box = VisualOverflowRect();
    if (ShouldClipOverflowAlongEitherAxis()) {
      overflow_box.Intersect(OverflowClipRect(PhysicalOffset()));
    }
    overflow_box.Unite(PhysicalBorderBoxRect());
  } else {
    overflow_box = PhysicalBorderBoxRect();
  }

  overflow_box.Move(accumulated_offset);
  return hit_test_location.Intersects(overflow_box);
}

bool LayoutBox::IsUserScrollable() const {
  NOT_DESTROYED();
  return HasScrollableOverflowX() || HasScrollableOverflowY();
}

const LayoutResult* LayoutBox::CachedLayoutResult(
    const ConstraintSpace& new_space,
    const BlockBreakToken* break_token,
    const EarlyBreak* early_break,
    const ColumnSpannerPath* column_spanner_path,
    std::optional<FragmentGeometry>* initial_fragment_geometry,
    LayoutCacheStatus* out_cache_status) {
  NOT_DESTROYED();
  *out_cache_status = LayoutCacheStatus::kNeedsLayout;

  if (SelfNeedsFullLayout()) {
    return nullptr;
  }

  if (ShouldSkipLayoutCache()) {
    return nullptr;
  }

  if (early_break) {
    return nullptr;
  }

  const bool use_layout_cache_slot =
      new_space.CacheSlot() == LayoutResultCacheSlot::kLayout &&
      !layout_results_.empty();
  const LayoutResult* cached_layout_result =
      use_layout_cache_slot
          ? GetCachedLayoutResult(break_token)
          : GetCachedMeasureResult(new_space, initial_fragment_geometry);

  if (!cached_layout_result)
    return nullptr;

  DCHECK_EQ(cached_layout_result->Status(), LayoutResult::kSuccess);

  // Set our initial temporary cache status to "hit".
  LayoutCacheStatus cache_status = LayoutCacheStatus::kHit;

  const PhysicalBoxFragment& physical_fragment =
      To<PhysicalBoxFragment>(cached_layout_result->GetPhysicalFragment());

  // No fun allowed for repeated content.
  if ((physical_fragment.GetBreakToken() &&
       physical_fragment.GetBreakToken()->IsRepeated()) ||
      (break_token && break_token->IsRepeated())) {
    return nullptr;
  }

  // If the display-lock blocked child layout, then we don't clear child needs
  // layout bits. However, we can still use the cached result, since we will
  // re-layout when unlocking.
  bool is_blocked_by_display_lock = ChildLayoutBlockedByDisplayLock();
  bool child_needs_layout =
      !is_blocked_by_display_lock && ChildNeedsFullLayout();

  if (NeedsSimplifiedLayoutOnly()) {
    cache_status = LayoutCacheStatus::kNeedsSimplifiedLayout;
  } else if (child_needs_layout) {
    // If we have inline children - we can potentially reuse some of the lines.
    if (!ChildrenInline()) {
      return nullptr;
    }

    if (!physical_fragment.HasItems()) {
      return nullptr;
    }

    // Only for the layout cache slot. Measure has several special
    // optimizations that makes reusing lines complicated.
    if (!use_layout_cache_slot) {
      return nullptr;
    }

    // Propagating OOF needs re-layout.
    if (physical_fragment.NeedsOOFPositionedInfoPropagation()) {
      return nullptr;
    }

    // Any floats might need to move, causing lines to wrap differently,
    // needing re-layout, either in cached result or in new constraint space.
    if (!cached_layout_result->GetExclusionSpace().IsEmpty() ||
        new_space.HasFloats()) {
      return nullptr;
    }

    // If we've shifted our children we can't rely on their position.
    if (physical_fragment.HasMovedChildrenInBlockDirection()) {
      return nullptr;
    }

    cache_status = LayoutCacheStatus::kCanReuseLines;
  }

  BlockNode node(this);
  LayoutCacheStatus size_cache_status = LayoutCacheStatus::kHit;
  if (use_layout_cache_slot) {
    size_cache_status = CalculateSizeBasedLayoutCacheStatus(
        node, break_token, *cached_layout_result, new_space,
        initial_fragment_geometry);
  }

  // If our size may change (or we know a descendants size may change), we miss
  // the cache.
  if (size_cache_status == LayoutCacheStatus::kNeedsLayout) {
    return nullptr;
  }

  if (cached_layout_result->HasOrthogonalFallbackSizeDescendant() &&
      View()->AffectedByResizedInitialContainingBlock(*cached_layout_result)) {
    // There's an orthogonal writing-mode root somewhere inside that depends on
    // the size of the initial containing block, and the initial containing
    // block size is changing.
    return nullptr;
  }

  // If we need simplified layout, but the cached fragment's children are not
  // valid (see comment in `SetCachedLayoutResult`), don't return the fragment,
  // since it will be used to iteration the invalid children when running
  // simplified layout.
  if (!physical_fragment.ChildrenValid() &&
      (size_cache_status == LayoutCacheStatus::kNeedsSimplifiedLayout ||
       cache_status == LayoutCacheStatus::kNeedsSimplifiedLayout)) {
    return nullptr;
  }

  // Update our temporary cache status, if the size cache check indicated we
  // might need simplified layout.
  if (size_cache_status == LayoutCacheStatus::kNeedsSimplifiedLayout &&
      cache_status == LayoutCacheStatus::kHit) {
    cache_status = LayoutCacheStatus::kNeedsSimplifiedLayout;
  }

  if (cache_status == LayoutCacheStatus::kNeedsSimplifiedLayout) {
    // Only allow simplified layout for non-replaced boxes.
    if (IsLayoutReplaced())
      return nullptr;

    // Simplified layout requires children to have a cached layout result. If
    // the current box has no cached layout result, its children might not,
    // either.
    if (!use_layout_cache_slot && !GetCachedLayoutResult(break_token))
      return nullptr;
  }

  LayoutUnit bfc_line_offset = new_space.GetBfcOffset().line_offset;
  std::optional<LayoutUnit> bfc_block_offset =
      cached_layout_result->BfcBlockOffset();
  LayoutUnit block_offset_delta;
  MarginStrut end_margin_strut = cached_layout_result->EndMarginStrut();

  bool are_bfc_offsets_equal;
  bool is_margin_strut_equal;
  bool is_exclusion_space_equal;
  bool is_fragmented = IsBreakInside(break_token) ||
                       physical_fragment.GetBreakToken() ||
                       PhysicalFragmentCount() > 1;

  {
    const ConstraintSpace& old_space =
        cached_layout_result->GetConstraintSpaceForCaching();

    // Check the BFC offset. Even if they don't match, there're some cases we
    // can still reuse the fragment.
    are_bfc_offsets_equal =
        new_space.GetBfcOffset() == old_space.GetBfcOffset() &&
        new_space.ExpectedBfcBlockOffset() ==
            old_space.ExpectedBfcBlockOffset() &&
        new_space.ForcedBfcBlockOffset() == old_space.ForcedBfcBlockOffset();

    is_margin_strut_equal =
        new_space.GetMarginStrut() == old_space.GetMarginStrut();
    is_exclusion_space_equal =
        new_space.GetExclusionSpace() == old_space.GetExclusionSpace();
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
      if (cache_status == LayoutCacheStatus::kNeedsSimplifiedLayout ||
          cache_status == LayoutCacheStatus::kCanReuseLines) {
        return nullptr;
      }

      DCHECK_EQ(cache_status, LayoutCacheStatus::kHit);

      if (!MaySkipLayoutWithinBlockFormattingContext(
              *cached_layout_result, new_space, &bfc_block_offset,
              &block_offset_delta, &end_margin_strut))
        return nullptr;
    }

    if (new_space.HasBlockFragmentation()) [[unlikely]] {
      DCHECK(old_space.HasBlockFragmentation());

      // Sometimes we perform simplified layout on a block-flow which is just
      // growing in block-size. When fragmentation is present we can't hit the
      // cache for these cases as we may grow past the fragmentation line.
      if (cache_status != LayoutCacheStatus::kHit) {
        return nullptr;
      }

      // Miss the cache if we have nested multicol containers inside that also
      // have OOF descendants. OOFs in nested multicol containers are handled in
      // a special way during layout: When we have returned to the outermost
      // fragmentation context root, we'll go through the nested multicol
      // containers and lay out the OOFs inside. If we do that after having hit
      // the cache (and thus kept the fragment with the OOF), we'd end up with
      // extraneous OOF fragments.
      if (physical_fragment.HasNestedMulticolsWithOOFs()) [[unlikely]] {
        return nullptr;
      }

      // Any fragmented out-of-flow positioned items will be placed once we
      // reach the fragmentation context root rather than the containing block,
      // so we should miss the cache in this case to ensure that such OOF
      // descendants are laid out correctly.
      if (physical_fragment.HasOutOfFlowFragmentChild())
        return nullptr;

      if (column_spanner_path || cached_layout_result->GetColumnSpannerPath()) {
        return nullptr;
      }

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
          cached_layout_result->GetBreakAppeal() < kBreakAppealPerfect) {
        return nullptr;
      }

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

        if (cached_layout_result->MinimalSpaceShortage()) {
          // The fragmentation line has moved, and there was space shortage
          // reported. This value is no longer valid.
          return nullptr;
        }

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
          std::optional<LayoutUnit> space_shortage =
              cached_layout_result->MinimalSpaceShortage();
          if (space_shortage && *space_shortage > LayoutUnit())
            return nullptr;
        }

        // Returns true if there are any floats added by |cached_layout_result|
        // which will end up crossing the fragmentation line.
        auto DoFloatsCrossFragmentationLine = [&]() -> bool {
          const auto& result_exclusion_space =
              cached_layout_result->GetExclusionSpace();
          if (result_exclusion_space != old_space.GetExclusionSpace()) {
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
          // NOTE: It's fine to use LayoutResult::BlockSizeForFragmentation()
          // directly here, rather than the helper BlockSizeForFragmentation()
          // in fragmentation_utils.cc, since what the latter does shouldn't
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
    if (cached_layout_result->GetExclusionSpace().HasFragmentainerBreak()) {
      // The final exclusion space is a processed version of the old one when
      // hitting the cache. One thing we don't support is copying the
      // fragmentation bits over correctly. That's something we could fix, if
      // the new resulting exclusion space otherwise is identical to the old
      // one. But for now, keep it simple, and just give up.
      return nullptr;
    }

    // Simplified layout doesn't support fragmented nodes.
    if (cache_status == LayoutCacheStatus::kNeedsSimplifiedLayout) {
      return nullptr;
    }
  }

  // We've performed all of the cache checks at this point. If we need
  // "simplified" layout then abort now.
  *out_cache_status = cache_status;
  if (cache_status == LayoutCacheStatus::kNeedsSimplifiedLayout ||
      cache_status == LayoutCacheStatus::kCanReuseLines) {
    return cached_layout_result;
  }

  physical_fragment.CheckType();

  DCHECK_EQ(*out_cache_status, LayoutCacheStatus::kHit);

  // For example, for elements with a transform change we can re-use the cached
  // result but we still need to recalculate the scrollable overflow.
  if (use_layout_cache_slot && !is_blocked_by_display_lock &&
      NeedsScrollableOverflowRecalc()) {
#if DCHECK_IS_ON()
    const LayoutResult* cloned_cached_layout_result =
        LayoutResult::CloneWithPostLayoutFragments(*cached_layout_result);
#endif
    if (!DisableLayoutSideEffectsScope::IsDisabled()) {
      RecalcScrollableOverflow();
    }

    // We need to update the cached layout result, as the call to
    // RecalcScrollableOverflow() might have modified it.
    cached_layout_result = GetCachedLayoutResult(break_token);

#if DCHECK_IS_ON()
    // We haven't actually performed simplified layout. Skip the checks for no
    // fragmentation, since it's okay to be fragmented in this case.
    cloned_cached_layout_result->CheckSameForSimplifiedLayout(
        *cached_layout_result, /* check_same_block_size */ true,
        /* check_no_fragmentation*/ false);
#endif
  }

  // Optimization: TableConstraintSpaceData can be large, and it is shared
  // between all the rows in a table. Make constraint space table data for
  // reused row fragment be identical to the one used by other row fragments.
  if (IsTableRow() && IsLayoutNGObject()) {
    const_cast<ConstraintSpace&>(
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
  // See |BlockNode::CachedLayoutResultForOutOfFlowPositioned|.
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
    cached_layout_result->GetExclusionSpace().MoveAndUpdateDerivedGeometry(
        new_space.GetExclusionSpace());
    return cached_layout_result;
  }

  const auto* new_result = MakeGarbageCollected<LayoutResult>(
      *cached_layout_result, new_space, end_margin_strut, bfc_line_offset,
      bfc_block_offset, block_offset_delta);

  if (needs_cached_result_update &&
      !DisableLayoutSideEffectsScope::IsDisabled()) {
    SetCachedLayoutResult(new_result, FragmentIndex(break_token));
  }

  return new_result;
}

const PhysicalBoxFragment* LayoutBox::GetPhysicalFragment(wtf_size_t i) const {
  NOT_DESTROYED();
  return &To<PhysicalBoxFragment>(layout_results_[i]->GetPhysicalFragment());
}

}  // namespace blink
