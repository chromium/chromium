// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/layout_utils.h"

#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/layout/constraint_space.h"
#include "third_party/blink/renderer/core/layout/layout_result.h"
#include "third_party/blink/renderer/core/layout/logical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/block_layout_algorithm_utils.h"
#include "third_party/blink/renderer/core/layout/length_utils.h"
#include "third_party/blink/renderer/core/layout/physical_box_fragment.h"

namespace blink {

namespace {

// LengthResolveType indicates what type length the function is being passed
// based on its CSS property. E.g.
// kMinSize - min-width / min-height
// kMaxSize - max-width / max-height
// kMainSize - width / height
enum class LengthResolveType { kMinSize, kMaxSize, kMainSize };

inline bool InlineLengthMayChange(const ComputedStyle& style,
                                  const Length& length,
                                  LengthResolveType type,
                                  const ConstraintSpace& new_space,
                                  const ConstraintSpace& old_space,
                                  const LayoutResult& layout_result) {
  DCHECK_EQ(new_space.InlineAutoBehavior(), old_space.InlineAutoBehavior());

  bool is_unspecified =
      (length.HasAuto() && type != LengthResolveType::kMinSize) ||
      length.HasFitContent() || length.HasStretch();

  // Percentage inline margins will affect the size if the size is unspecified
  // (auto and similar).
  if (is_unspecified && style.MayHaveMargin() &&
      (style.MarginInlineStart().HasPercent() ||
       style.MarginInlineEnd().HasPercent()) &&
      (new_space.PercentageResolutionInlineSize() !=
       old_space.PercentageResolutionInlineSize())) {
    return true;
  }

  if (is_unspecified) {
    if (new_space.AvailableSize().inline_size !=
        old_space.AvailableSize().inline_size)
      return true;
  }

  if (length.MayHavePercentDependence()) {
    if (new_space.PercentageResolutionInlineSize() !=
        old_space.PercentageResolutionInlineSize())
      return true;
  }
  return false;
}

inline bool BlockLengthMayChange(const Length& length,
                                 const ConstraintSpace& new_space,
                                 const ConstraintSpace& old_space) {
  DCHECK_EQ(new_space.BlockAutoBehavior(), old_space.BlockAutoBehavior());
  if (length.HasStretch() ||
      (length.HasAuto() && new_space.IsBlockAutoBehaviorStretch())) {
    if (new_space.AvailableSize().block_size !=
        old_space.AvailableSize().block_size)
      return true;
  }

  return false;
}

bool BlockSizeMayChange(const BlockNode& node,
                        const ConstraintSpace& new_space,
                        const ConstraintSpace& old_space,
                        const LayoutResult& layout_result) {
  DCHECK_EQ(new_space.IsFixedBlockSize(), old_space.IsFixedBlockSize());
  DCHECK_EQ(new_space.IsInitialBlockSizeIndefinite(),
            old_space.IsInitialBlockSizeIndefinite());
  DCHECK_EQ(new_space.BlockAutoBehavior(), old_space.BlockAutoBehavior());
  DCHECK_EQ(new_space.IsTableCellChild(), old_space.IsTableCellChild());
  DCHECK_EQ(new_space.IsRestrictedBlockSizeTableCellChild(),
            old_space.IsRestrictedBlockSizeTableCellChild());

  if (node.IsQuirkyAndFillsViewport())
    return true;

  if (new_space.IsFixedBlockSize()) {
    if (new_space.AvailableSize().block_size !=
        old_space.AvailableSize().block_size)
      return true;
  } else {
    const ComputedStyle& style = node.Style();
    if (BlockLengthMayChange(style.LogicalHeight(), new_space, old_space) ||
        BlockLengthMayChange(style.LogicalMinHeight(), new_space, old_space) ||
        BlockLengthMayChange(style.LogicalMaxHeight(), new_space, old_space))
      return true;
    // We only need to check if the PercentageResolutionBlockSizes match if the
    // layout result has explicitly marked itself as dependent.
    if (layout_result.GetPhysicalFragment().DependsOnPercentageBlockSize()) {
      if (new_space.PercentageResolutionBlockSize() !=
          old_space.PercentageResolutionBlockSize())
        return true;
      if (new_space.ReplacedPercentageResolutionBlockSize() !=
          old_space.ReplacedPercentageResolutionBlockSize())
        return true;
    }
  }

  return false;
}

// Return true if it's possible (but not necessarily guaranteed) that the new
// constraint space will give a different size compared to the old one, when
// computed style and child content remain unchanged.
bool SizeMayChange(const BlockNode& node,
                   const ConstraintSpace& new_space,
                   const ConstraintSpace& old_space,
                   const LayoutResult& layout_result) {
  DCHECK_EQ(new_space.IsFixedInlineSize(), old_space.IsFixedInlineSize());
  DCHECK_EQ(new_space.BlockAutoBehavior(), old_space.BlockAutoBehavior());

  const ComputedStyle& style = node.Style();

  // Go through all length properties, and, depending on length type
  // (percentages, auto, etc.), check whether the constraint spaces differ in
  // such a way that the resulting size *may* change. There are currently many
  // possible false-positive situations here, as we don't rule out length
  // changes that won't have any effect on the final size (e.g. if inline-size
  // is 100px, max-inline-size is 50%, and percentage resolution inline size
  // changes from 1000px to 500px). If the constraint space has "fixed" size in
  // a dimension, we can skip checking properties in that dimension and just
  // look for available size changes, since that's how a "fixed" constraint
  // space works.
  if (new_space.IsFixedInlineSize()) {
    if (new_space.AvailableSize().inline_size !=
        old_space.AvailableSize().inline_size)
      return true;
  } else {
    if (InlineLengthMayChange(style, style.LogicalWidth(),
                              LengthResolveType::kMainSize, new_space,
                              old_space, layout_result) ||
        InlineLengthMayChange(style, style.LogicalMinWidth(),
                              LengthResolveType::kMinSize, new_space, old_space,
                              layout_result) ||
        InlineLengthMayChange(style, style.LogicalMaxWidth(),
                              LengthResolveType::kMaxSize, new_space, old_space,
                              layout_result))
      return true;
  }

  if (style.MayHavePadding() &&
      new_space.PercentageResolutionInlineSize() !=
          old_space.PercentageResolutionInlineSize()) {
    // Percentage-based padding is resolved against the inline content box size
    // of the containing block.
    if (style.PaddingTop().HasPercent() || style.PaddingRight().HasPercent() ||
        style.PaddingBottom().HasPercent() ||
        style.PaddingLeft().HasPercent()) {
      return true;
    }
  }

  return BlockSizeMayChange(node, new_space, old_space, layout_result);
}

// Given the pre-computed |fragment_geometry| calcuates the
// |LayoutCacheStatus| based on this sizing information. Returns:
//  - |LayoutCacheStatus::kNeedsLayout| if the |new_space| will produce a
//    different sized fragment, or if any %-block-size children will change
//    size.
//  - |LayoutCacheStatus::kNeedsSimplifiedLayout| if the block-size of the
//    fragment will change, *without* affecting any descendants (no descendants
//    have %-block-sizes).
//  - |LayoutCacheStatus::kHit| otherwise.
LayoutCacheStatus CalculateSizeBasedLayoutCacheStatusWithGeometry(
    const BlockNode& node,
    const FragmentGeometry& fragment_geometry,
    const LayoutResult& layout_result,
    const ConstraintSpace& new_space,
    const ConstraintSpace& old_space) {
  const ComputedStyle& style = node.Style();
  const auto& physical_fragment =
      To<PhysicalBoxFragment>(layout_result.GetPhysicalFragment());
  LogicalBoxFragment fragment(style.GetWritingDirection(), physical_fragment);

  if (fragment_geometry.border_box_size.inline_size != fragment.InlineSize())
    return LayoutCacheStatus::kNeedsLayout;

  if (style.MayHavePadding() && fragment_geometry.padding != fragment.Padding())
    return LayoutCacheStatus::kNeedsLayout;

  // Tables are special - we can't determine the final block-size ahead of time
  // (or based on the previous intrinsic size).
  // Instead if the block-size *may* change, force a layout. If we definitely
  // know the block-size won't change (the size constraints haven't changed) we
  // can hit the cache.
  //
  // *NOTE* - any logic below this branch shouldn't apply to tables.
  if (node.IsTable()) {
    if (!new_space.AreBlockSizeConstraintsEqual(old_space) ||
        BlockSizeMayChange(node, new_space, old_space, layout_result))
      return LayoutCacheStatus::kNeedsLayout;
    return LayoutCacheStatus::kHit;
  }

  LayoutUnit block_size = fragment_geometry.border_box_size.block_size;
  bool is_initial_block_size_indefinite = block_size == kIndefiniteSize;
  if (is_initial_block_size_indefinite) {
    LayoutUnit intrinsic_block_size;
    // Intrinsic block-size is only defined if the node is unfragmented.
    if (!physical_fragment.IsFirstForNode() ||
        physical_fragment.GetBreakToken()) {
      intrinsic_block_size = kIndefiniteSize;
    } else {
      intrinsic_block_size = layout_result.IntrinsicBlockSize();
    }

    // Grid/flex/fieldset can have their children calculate their size based on
    // their parent's final block-size. E.g.
    // <div style="display: flex;">
    //   <div style="display: flex;"> <!-- or "display: grid;" -->
    //     <!-- Child will stretch to the parent's block-size -->
    //     <div></div>
    //   </div>
    // </div>
    // <div style="display: flex;">
    //   <div style="display: flex; flex-direction: column;">
    //     <!-- Child will grow to the parent's fixed block-size -->
    //     <div style="flex: 1;"></div>
    //   </div>
    // </div>
    //
    // If the previous |layout_result| was produced by a space which had a
    // fixed block-size we can't use |intrinsic_block_size| for determining
    // the new block-size.
    //
    // TODO(ikilpatrick): Similar to %-block-size descendants we could store a
    // bit on the |LayoutResult| which indicates if it had a child which
    // sized itself based on the parent's block-size.
    // We should consider this optimization if we are missing this cache often
    // within this branch (and could have re-used the result).
    // TODO(ikilaptrick): This may occur for other layout modes, e.g.
    // custom-layout.
    if (old_space.IsFixedBlockSize() ||
        (old_space.IsBlockAutoBehaviorStretch() &&
         style.LogicalHeight().HasAuto())) {
      if (node.IsFlexibleBox() || node.IsGrid() || node.IsFieldsetContainer())
        intrinsic_block_size = kIndefiniteSize;
    }

    // Grid/flex can have their intrinsic block-size depend on the
    // %-block-size. This occurs when:
    //  - A column flex-box has "max-height: 100%" (or similar) on itself.
    //  - A row flex-box has "height: 100%" (or similar) and children which
    //    stretch to this size.
    //  - A grid with "grid-template-rows: repeat(auto-fill, 50px)" or similar.
    //
    // Similar to above we can't use the |intrinsic_block_size| for determining
    // the new block-size.
    //
    // TODO(dgrogan): We can hit the cache here for row flexboxes when they
    // don't have stretchy children.
    if (physical_fragment.DependsOnPercentageBlockSize() &&
        new_space.PercentageResolutionBlockSize() !=
            old_space.PercentageResolutionBlockSize()) {
      if (node.IsFlexibleBox() || node.IsGrid())
        intrinsic_block_size = kIndefiniteSize;
    }

    block_size = ComputeBlockSizeForFragment(
        new_space, node, fragment_geometry.border + fragment_geometry.padding,
        intrinsic_block_size, fragment_geometry.border_box_size.inline_size);

    if (block_size == kIndefiniteSize)
      return LayoutCacheStatus::kNeedsLayout;
  }

  bool is_block_size_equal = block_size == fragment.BlockSize();

  if (!is_block_size_equal) {
    // Only block-flow supports changing the block-size for simplified layout.
    if (!node.IsBlockFlow() || node.IsCustom()) {
      return LayoutCacheStatus::kNeedsLayout;
    }

    // Fieldsets stretch their content to the final block-size, which might
    // affect scrollbars.
    if (node.IsFieldsetContainer())
      return LayoutCacheStatus::kNeedsLayout;

    if (node.IsBlockFlow() && style.AlignContentBlockCenter()) {
      return LayoutCacheStatus::kNeedsLayout;
    }

    // If we are the document or body element in quirks mode, changing our size
    // means that a scrollbar was added/removed. Require full layout.
    if (node.IsQuirkyAndFillsViewport())
      return LayoutCacheStatus::kNeedsLayout;

    // If a block (within a formatting-context) changes to/from an empty-block,
    // margins may collapse through this node, requiring full layout. We
    // approximate this check by checking if the block-size is/was zero.
    if (!physical_fragment.IsFormattingContextRoot() &&
        !block_size != !fragment.BlockSize())
      return LayoutCacheStatus::kNeedsLayout;
  }

  const bool has_descendant_that_depends_on_percentage_block_size =
      layout_result.HasDescendantThatDependsOnPercentageBlockSize();
  const bool is_old_initial_block_size_indefinite =
      layout_result.IsInitialBlockSizeIndefinite();

  // Miss the cache if the initial block-size change from indefinite to
  // definite (or visa-versa), and:
  //  - We have a descendant which depends on the %-block-size.
  //  - We are a grid.
  //
  // TODO(ikilpatrick): There is an "optimization" for grid which would involve
  // *always* setting the initial block-size for grid as indefinite, then
  // re-running computing the grid if we have any "auto" tracks etc.
  if (is_old_initial_block_size_indefinite !=
      is_initial_block_size_indefinite) {
    if (node.IsGrid() || has_descendant_that_depends_on_percentage_block_size)
      return LayoutCacheStatus::kNeedsLayout;
  }

  if (has_descendant_that_depends_on_percentage_block_size) {
    // If our initial block-size is definite, we know that if we change our
    // block-size we'll affect any descendant that depends on the resulting
    // percentage block-size.
    if (!is_block_size_equal && !is_initial_block_size_indefinite)
      return LayoutCacheStatus::kNeedsLayout;

    DCHECK(is_block_size_equal || is_initial_block_size_indefinite);

    // At this point we know that either we have the same block-size for our
    // fragment, or our initial block-size was indefinite.
    //
    // The |PhysicalFragment::DependsOnPercentageBlockSize| flag
    // will returns true if we are in quirks mode, and have a descendant that
    // depends on a percentage block-size, however it will also return true if
    // the node itself depends on the %-block-size.
    //
    // As we only care about the quirks-mode %-block-size behavior we remove
    // this false-positive by checking if we have an initial indefinite
    // block-size.
    if (is_initial_block_size_indefinite &&
        physical_fragment.DependsOnPercentageBlockSize()) {
      DCHECK(is_old_initial_block_size_indefinite);
      if (new_space.PercentageResolutionBlockSize() !=
          old_space.PercentageResolutionBlockSize())
        return LayoutCacheStatus::kNeedsLayout;
      if (new_space.ReplacedPercentageResolutionBlockSize() !=
          old_space.ReplacedPercentageResolutionBlockSize())
        return LayoutCacheStatus::kNeedsLayout;
    }
  }

  // Table-cells with vertical alignment might shift their contents if the
  // block-size changes.
  if (new_space.IsTableCell()) {
    DCHECK(old_space.IsTableCell());

    switch (ComputeContentAlignmentForTableCell(style)) {
      case BlockContentAlignment::kStart:
        // Do nothing special for 'top' vertical alignment.
        break;
      case BlockContentAlignment::kBaseline: {
        auto new_alignment_baseline = new_space.TableCellAlignmentBaseline();
        auto old_alignment_baseline = old_space.TableCellAlignmentBaseline();

        // Do nothing if neither alignment baseline is set.
        if (!new_alignment_baseline && !old_alignment_baseline)
          break;

        // If we only have an old alignment baseline set, we need layout, as we
        // can't determine where the un-adjusted baseline is.
        if (!new_alignment_baseline && old_alignment_baseline)
          return LayoutCacheStatus::kNeedsLayout;

        // We've been provided a new alignment baseline, just check that it
        // matches the previously generated baseline.
        if (!old_alignment_baseline) {
          if (*new_alignment_baseline != physical_fragment.FirstBaseline())
            return LayoutCacheStatus::kNeedsLayout;
          break;
        }

        // If the alignment baselines differ at this stage, we need layout.
        if (*new_alignment_baseline != *old_alignment_baseline)
          return LayoutCacheStatus::kNeedsLayout;
        break;
      }
      case BlockContentAlignment::kUnsafeCenter:
      case BlockContentAlignment::kSafeCenter:
      case BlockContentAlignment::kUnsafeEnd:
      case BlockContentAlignment::kSafeEnd:
        // 'middle', and 'bottom' vertical alignment depend on the block-size.
        if (!is_block_size_equal)
          return LayoutCacheStatus::kNeedsLayout;
        break;
    }
  } else {
    switch (ComputeContentAlignmentForBlock(style)) {
      case BlockContentAlignment::kStart:
      case BlockContentAlignment::kBaseline:
        // Do nothing special.
        break;
      case BlockContentAlignment::kUnsafeCenter:
      case BlockContentAlignment::kSafeCenter:
      case BlockContentAlignment::kUnsafeEnd:
      case BlockContentAlignment::kSafeEnd:
        if (!is_block_size_equal) {
          return LayoutCacheStatus::kNeedsLayout;
        }
        break;
    }
  }

  // If we've reached here we know that we can potentially "stretch"/"shrink"
  // ourselves without affecting any of our children.
  // In that case we may be able to perform "simplified" layout.
  DCHECK(!node.IsTable());
  return is_block_size_equal ? LayoutCacheStatus::kHit
                             : LayoutCacheStatus::kNeedsSimplifiedLayout;
}

bool IntrinsicSizeWillChange(
    const BlockNode& node,
    const BlockBreakToken* break_token,
    const LayoutResult& cached_layout_result,
    const ConstraintSpace& new_space,
    std::optional<FragmentGeometry>* fragment_geometry) {
  const ComputedStyle& style = node.Style();
  if (new_space.IsInlineAutoBehaviorStretch() && !NeedMinMaxSize(style))
    return false;

  if (!*fragment_geometry) {
    *fragment_geometry =
        CalculateInitialFragmentGeometry(new_space, node, break_token);
  }

  LayoutUnit inline_size =
      LogicalFragment(style.GetWritingDirection(),
                      cached_layout_result.GetPhysicalFragment())
          .InlineSize();

  if ((*fragment_geometry)->border_box_size.inline_size != inline_size)
    return true;

  return false;
}

}  // namespace

LayoutCacheStatus CalculateSizeBasedLayoutCacheStatus(
    const BlockNode& node,
    const BlockBreakToken* break_token,
    const LayoutResult& cached_layout_result,
    const ConstraintSpace& new_space,
    std::optional<FragmentGeometry>* fragment_geometry) {
  DCHECK_EQ(cached_layout_result.Status(), LayoutResult::kSuccess);

  const ConstraintSpace& old_space =
      cached_layout_result.GetConstraintSpaceForCaching();

  if (!new_space.MaySkipLayout(old_space))
    return LayoutCacheStatus::kNeedsLayout;

  if (new_space.AreInlineSizeConstraintsEqual(old_space) &&
      new_space.AreBlockSizeConstraintsEqual(old_space)) {
    // It is possible that our intrinsic size has changed, check for that here.
    if (IntrinsicSizeWillChange(node, break_token, cached_layout_result,
                                new_space, fragment_geometry))
      return LayoutCacheStatus::kNeedsLayout;

    // We don't have to check our style if we know the constraint space sizes
    // will remain the same.
    if (new_space.AreSizesEqual(old_space))
      return LayoutCacheStatus::kHit;

    // TODO(ikilpatrick): Always miss the cache for tables whose block
    // size-constraints change.
    if (!SizeMayChange(node, new_space, old_space, cached_layout_result))
      return LayoutCacheStatus::kHit;
  }

  if (!*fragment_geometry) {
    *fragment_geometry =
        CalculateInitialFragmentGeometry(new_space, node, break_token);
  }

  return CalculateSizeBasedLayoutCacheStatusWithGeometry(
      node, **fragment_geometry, cached_layout_result, new_space, old_space);
}

bool MaySkipLayoutWithinBlockFormattingContext(
    const LayoutResult& cached_layout_result,
    const ConstraintSpace& new_space,
    std::optional<LayoutUnit>* bfc_block_offset,
    LayoutUnit* block_offset_delta,
    MarginStrut* end_margin_strut) {
  DCHECK_EQ(cached_layout_result.Status(), LayoutResult::kSuccess);
  DCHECK(bfc_block_offset);
  DCHECK(block_offset_delta);
  DCHECK(end_margin_strut);

  const ConstraintSpace& old_space =
      cached_layout_result.GetConstraintSpaceForCaching();

  bool is_margin_strut_equal =
      old_space.GetMarginStrut() == new_space.GetMarginStrut();

  LayoutUnit old_clearance_offset = old_space.ClearanceOffset();
  LayoutUnit new_clearance_offset = new_space.ClearanceOffset();

  // Determine if we can reuse a result if it was affected by clearance.
  bool is_pushed_by_floats = cached_layout_result.IsPushedByFloats();
  if (is_pushed_by_floats) {
    DCHECK(old_space.HasFloats());

    // We don't attempt to reuse the cached result if our margins have changed.
    if (!is_margin_strut_equal)
      return false;

    // We don't attempt to reuse the cached result if the clearance offset
    // differs from the final BFC-block-offset.
    //
    // The |is_pushed_by_floats| flag is also used by nodes who have a *child*
    // which was pushed by floats. In this case the node may not have a
    // BFC-block-offset or one equal to the clearance offset.
    if (!cached_layout_result.BfcBlockOffset() ||
        *cached_layout_result.BfcBlockOffset() != old_space.ClearanceOffset())
      return false;

    // We only reuse the cached result if the delta between the
    // BFC-block-offset, and the clearance offset grows or remains the same. If
    // it shrinks it may not be affected by clearance anymore as a margin may
    // push the fragment below the clearance offset instead.
    //
    // TODO(layout-dev): If we track if any margins affected this calculation
    // (with an additional bit on the layout result) we could potentially skip
    // this check.
    if (old_clearance_offset - old_space.GetBfcOffset().block_offset >
        new_clearance_offset - new_space.GetBfcOffset().block_offset) {
      return false;
    }
  }

  // We can't reuse the layout result if the subtree modified its incoming
  // margin-strut, and the incoming margin-strut has changed. E.g.
  // <div style="margin-top: 5px;"> <!-- changes to 15px -->
  //   <div style="margin-top: 10px;"></div>
  //   text
  // </div>
  if (cached_layout_result.SubtreeModifiedMarginStrut() &&
      !is_margin_strut_equal)
    return false;

  const auto& physical_fragment =
      To<PhysicalBoxFragment>(cached_layout_result.GetPhysicalFragment());

  // Check we have a descendant that *may* be positioned above the block-start
  // edge. We abort if either the old or new space has floats, as we don't keep
  // track of how far above the child could be. This case is relatively rare,
  // and only occurs with negative margins.
  if (physical_fragment.MayHaveDescendantAboveBlockStart() &&
      (old_space.HasFloats() || new_space.HasFloats()))
    return false;

  // Self collapsing blocks have different "shifting" rules applied to them.
  if (cached_layout_result.IsSelfCollapsing()) {
    // If a self-collapsing block got pushed by floats due to clearance, all
    // bets are off.
    if (is_pushed_by_floats)
      return false;

    // The "expected" BFC block-offset is where adjoining objects will be
    // placed (which may be wrong due to adjoining margins).
    LayoutUnit old_expected = old_space.ExpectedBfcBlockOffset();
    LayoutUnit new_expected = new_space.ExpectedBfcBlockOffset();

    // If we have any adjoining object descendants (floats), we need to ensure
    // that their position wouldn't be impacted by any preceding floats.
    if (physical_fragment.HasAdjoiningObjectDescendants()) {
      // Check if the previous position intersects with any floats.
      if (old_expected <
          old_space.GetExclusionSpace().ClearanceOffset(EClear::kBoth)) {
        return false;
      }

      // Check if the new position intersects with any floats.
      if (new_expected <
          new_space.GetExclusionSpace().ClearanceOffset(EClear::kBoth)) {
        return false;
      }
    }

    *block_offset_delta = new_expected - old_expected;

    // Self-collapsing blocks with a "forced" BFC block-offset input receive a
    // "resolved" BFC block-offset on their layout result.
    *bfc_block_offset = new_space.ForcedBfcBlockOffset();

    // If this sub-tree didn't append any margins to the incoming margin-strut,
    // the new "start" margin-strut becomes the new "end" margin-strut (as we
    // are self-collapsing).
    if (!cached_layout_result.SubtreeModifiedMarginStrut()) {
      *end_margin_strut = new_space.GetMarginStrut();
    } else {
      DCHECK(is_margin_strut_equal);
    }

    return true;
  }

  // We can now try to adjust the BFC block-offset for regular blocks.
  DCHECK(*bfc_block_offset);
  DCHECK_EQ(old_space.AncestorHasClearancePastAdjoiningFloats(),
            new_space.AncestorHasClearancePastAdjoiningFloats());

  bool ancestor_has_clearance_past_adjoining_floats =
      new_space.AncestorHasClearancePastAdjoiningFloats();

  if (ancestor_has_clearance_past_adjoining_floats) {
    // The subsequent code will break if these invariants don't hold true.
    DCHECK(old_space.ForcedBfcBlockOffset());
    DCHECK(new_space.ForcedBfcBlockOffset());
    DCHECK_EQ(*old_space.ForcedBfcBlockOffset(), old_clearance_offset);
    DCHECK_EQ(*new_space.ForcedBfcBlockOffset(), new_clearance_offset);
  } else {
    // New formatting-contexts have (potentially) complex positioning logic. In
    // some cases they will resolve a BFC block-offset twice (with their margins
    // adjoining, and not adjoining), resulting in two different "forced" BFC
    // block-offsets. We don't allow caching as we can't determine which pass a
    // layout result belongs to for this case.
    if (old_space.ForcedBfcBlockOffset() != new_space.ForcedBfcBlockOffset())
      return false;
  }

  // Check if the previous position intersects with any floats.
  if (**bfc_block_offset <
      old_space.GetExclusionSpace().ClearanceOffset(EClear::kBoth)) {
    return false;
  }

  if (is_pushed_by_floats || ancestor_has_clearance_past_adjoining_floats) {
    // If we've been pushed by floats, we assume the new clearance offset.
    DCHECK_EQ(**bfc_block_offset, old_clearance_offset);
    *block_offset_delta = new_clearance_offset - old_clearance_offset;
    *bfc_block_offset = new_clearance_offset;
  } else if (is_margin_strut_equal) {
    // If our incoming margin-strut is equal, we are just shifted by the BFC
    // block-offset amount.
    *block_offset_delta = new_space.GetBfcOffset().block_offset -
                          old_space.GetBfcOffset().block_offset;
    *bfc_block_offset = **bfc_block_offset + *block_offset_delta;
  } else {
    // If our incoming margin-strut isn't equal, we need to account for the
    // difference in the incoming margin-struts.
#if DCHECK_IS_ON()
    DCHECK(!cached_layout_result.SubtreeModifiedMarginStrut());
    LayoutUnit old_bfc_block_offset = old_space.GetBfcOffset().block_offset +
                                      old_space.GetMarginStrut().Sum();
    DCHECK_EQ(old_bfc_block_offset, **bfc_block_offset);
#endif

    LayoutUnit new_bfc_block_offset = new_space.GetBfcOffset().block_offset +
                                      new_space.GetMarginStrut().Sum();
    *block_offset_delta = new_bfc_block_offset - **bfc_block_offset;
    *bfc_block_offset = **bfc_block_offset + *block_offset_delta;
  }

  // Check if the new position intersects with any floats.
  if (**bfc_block_offset <
      new_space.GetExclusionSpace().ClearanceOffset(EClear::kBoth)) {
    return false;
  }

  return true;
}

}  // namespace blink
