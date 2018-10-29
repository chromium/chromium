// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/exclusions/ng_exclusion_space.h"

#include "base/optional.h"
#include "third_party/blink/renderer/core/layout/ng/exclusions/ng_exclusion.h"

namespace blink {

namespace {

// Inserts a layout opportunity into the completed list. The list is ordered by
// block-start, then by inline-size (shrinking) / block-size (growing).
//
// We don't explicitly check the inline-size/block-size of the opportunity as
// they are always produced in the order.
void InsertOpportunity(const NGLayoutOpportunity& opportunity,
                       Vector<NGLayoutOpportunity, 4>* opportunities) {
  if (opportunities->IsEmpty()) {
    opportunities->emplace_back(opportunity);
    return;
  }

  // We go backwards through the list as there is a higher probability that a
  // new opportunity will be at the end of the list.
  for (wtf_size_t j = opportunities->size() - 1; j >= 0; --j) {
    const NGLayoutOpportunity& other = opportunities->at(j);
    if (other.rect.BlockStartOffset() <= opportunity.rect.BlockStartOffset()) {
#if DCHECK_IS_ON()
      // If we have the same block-start offset ensure that the size of the
      // opportunity doesn't violate the order.
      if (other.rect.BlockStartOffset() ==
          opportunity.rect.BlockStartOffset()) {
        DCHECK_LE(other.rect.BlockSize(), opportunity.rect.BlockSize());
        DCHECK_GE(other.rect.InlineSize(), opportunity.rect.InlineSize());
      }
#endif

      opportunities->insert(j + 1, opportunity);
      return;
    }
  }

  NOTREACHED();
}

// Returns true if there is at least one edge between block_start and block_end.
bool HasSolidEdges(const Vector<scoped_refptr<const NGExclusion>, 1>& edges,
                   LayoutUnit block_start,
                   LayoutUnit block_end) {
  // If there aren't any adjacent exclusions, we must be the initial shelf.
  // This always has "solid" edges on either side.
  if (edges.IsEmpty())
    return true;

  for (const auto& edge : edges) {
    if (edge->rect.BlockEndOffset() > block_start &&
        edge->rect.BlockStartOffset() < block_end)
      return true;
  }

  return false;
}

// Adds any edges (other exclusions) which are within the range:
// (block_offset, LayoutUnit::Max())
// to the given out_edges vector.
// edges will be invalid after this call.
void CollectSolidEdges(Vector<scoped_refptr<const NGExclusion>, 1>* edges,
                       LayoutUnit block_offset,
                       Vector<scoped_refptr<const NGExclusion>, 1>* out_edges) {
  *out_edges = std::move(*edges);
  for (auto* it = out_edges->begin(); it != out_edges->end();) {
    if ((*it)->rect.BlockEndOffset() <= block_offset) {
      out_edges->erase(it);
    } else {
      ++it;
    }
  }
}

// Returns true if the area defined by the given offset and inline_size
// intersects with the opportunity.
//
// We only need to check the block-end of the opportunity is below the given
// offset, as the given area extends to a block-end of infinity.
bool Intersects(const NGLayoutOpportunity& opportunity,
                const NGBfcOffset& offset,
                const LayoutUnit inline_size) {
  return opportunity.rect.LineEndOffset() > offset.line_offset &&
         opportunity.rect.LineStartOffset() <
             offset.line_offset + inline_size &&
         opportunity.rect.BlockEndOffset() > offset.block_offset;
}

// Returns true if the area defined by the given offset and inline_size
// intersects with the shelfs area.
//
// No checks for the block direction are needed as the given area (defined by
// offset and inline_size) extends to a block-end of infinity, and a shelf also
// has a block-end of infinity.
//
// If the shelf is at -Infinity or +Infinity at either end, the given area
// always intersects.
bool Intersects(const NGExclusionSpaceInternal::NGShelf& shelf,
                const NGBfcOffset& offset,
                const LayoutUnit inline_size) {
  if (shelf.line_right >= offset.line_offset &&
      shelf.line_left <= offset.line_offset + inline_size)
    return true;
  // Negative available space creates a zero-width opportunity at the inline-end
  // of the shelf. Consider such shelf intersects.
  // TODO(kojii): This is correct to find layout opportunities for zero-width
  // in-flow inline or block objects (e.g., <br>,) but not correct for
  // zero-width floats.
  if (UNLIKELY(shelf.line_left > offset.line_offset ||
               shelf.line_right < offset.line_offset + inline_size))
    return true;
  return false;
}

// Creates a new layout opportunity. The given layout opportunity *must*
// intersect with the given area (defined by offset and inline_size).
NGLayoutOpportunity CreateLayoutOpportunity(const NGLayoutOpportunity& other,
                                            const NGBfcOffset& offset,
                                            const LayoutUnit inline_size) {
  DCHECK(Intersects(other, offset, inline_size));

  NGBfcOffset start_offset(
      std::max(other.rect.LineStartOffset(), offset.line_offset),
      std::max(other.rect.start_offset.block_offset, offset.block_offset));

  NGBfcOffset end_offset(
      std::min(other.rect.LineEndOffset(), offset.line_offset + inline_size),
      other.rect.BlockEndOffset());

  return NGLayoutOpportunity(NGBfcRect(start_offset, end_offset),
                             other.shape_exclusions);
}

// Creates a new layout opportunity. The given shelf *must* intersect with the
// given area (defined by offset and inline_size).
NGLayoutOpportunity CreateLayoutOpportunity(
    const NGExclusionSpaceInternal::NGShelf& shelf,
    const NGBfcOffset& offset,
    const LayoutUnit inline_size) {
  DCHECK(Intersects(shelf, offset, inline_size));

  NGBfcOffset start_offset(std::max(shelf.line_left, offset.line_offset),
                           std::max(shelf.block_offset, offset.block_offset));

  // Max with |start_offset.line_offset| in case the shelf has a negative
  // inline-size.
  NGBfcOffset end_offset(
      std::max(std::min(shelf.line_right, offset.line_offset + inline_size),
               start_offset.line_offset),
      LayoutUnit::Max());

  return NGLayoutOpportunity(
      NGBfcRect(start_offset, end_offset),
      shelf.has_shape_exclusions
          ? base::AdoptRef(new NGShapeExclusions(*shelf.shape_exclusions))
          : nullptr);
}

}  // namespace

NGExclusionSpaceInternal::NGExclusionSpaceInternal()
    : exclusions_(RefVector<scoped_refptr<const NGExclusion>>::Create()),
      num_exclusions_(0),
      both_clear_offset_(LayoutUnit::Min()),
      derived_geometry_(nullptr) {}

NGExclusionSpaceInternal::NGExclusionSpaceInternal(
    const NGExclusionSpaceInternal& other)
    : exclusions_(other.exclusions_),
      num_exclusions_(other.num_exclusions_),
      both_clear_offset_(other.both_clear_offset_),
      derived_geometry_(std::move(other.derived_geometry_)) {
  // This copy-constructor does fun things. It moves the derived_geometry_ to
  // the newly created exclusion space where it'll more-likely be used.
  other.derived_geometry_ = nullptr;
}

NGExclusionSpaceInternal::NGExclusionSpaceInternal(
    NGExclusionSpaceInternal&&) noexcept = default;

NGExclusionSpaceInternal& NGExclusionSpaceInternal::operator=(
    const NGExclusionSpaceInternal& other) {
  exclusions_ = other.exclusions_;
  num_exclusions_ = other.num_exclusions_;
  both_clear_offset_ = other.both_clear_offset_;
  derived_geometry_ = std::move(other.derived_geometry_);
  other.derived_geometry_ = nullptr;
  return *this;
}

NGExclusionSpaceInternal& NGExclusionSpaceInternal::operator=(
    NGExclusionSpaceInternal&&) noexcept = default;

NGExclusionSpaceInternal::DerivedGeometry::DerivedGeometry()
    : last_float_block_start_(LayoutUnit::Min()),
      left_float_clear_offset_(LayoutUnit::Min()),
      right_float_clear_offset_(LayoutUnit::Min()) {
  // The exclusion space must always have at least one shelf, at -Infinity.
  shelves_.emplace_back(/* block_offset */ LayoutUnit::Min());
}

void NGExclusionSpaceInternal::Add(scoped_refptr<const NGExclusion> exclusion) {
  DCHECK_LE(num_exclusions_, exclusions_->size());

  // Perform a copy-on-write if the number of exclusions has gone out of sync.
  if (num_exclusions_ != exclusions_->size()) {
    scoped_refptr<RefVector<scoped_refptr<const NGExclusion>>> exclusions =
        RefVector<scoped_refptr<const NGExclusion>>::Create();
    exclusions->GetMutableVector()->AppendRange(
        exclusions_->GetVector().begin(),
        exclusions_->GetVector().begin() + num_exclusions_);
    std::swap(exclusions_, exclusions);

    // The derived_geometry_ is now invalid.
    derived_geometry_ = nullptr;
  }

  if (derived_geometry_)
    derived_geometry_->Add(*exclusion);

  both_clear_offset_ =
      std::max(both_clear_offset_, exclusion->rect.BlockEndOffset());

  exclusions_->emplace_back(std::move(exclusion));
  num_exclusions_++;
}

void NGExclusionSpaceInternal::DerivedGeometry::Add(
    const NGExclusion& exclusion) {
  last_float_block_start_ =
      std::max(last_float_block_start_, exclusion.rect.BlockStartOffset());

  const LayoutUnit exclusion_end_offset = exclusion.rect.BlockEndOffset();

  // Update the members used for clearance calculations.
  if (exclusion.type == EFloat::kLeft) {
    left_float_clear_offset_ =
        std::max(left_float_clear_offset_, exclusion.rect.BlockEndOffset());
  } else if (exclusion.type == EFloat::kRight) {
    right_float_clear_offset_ =
        std::max(right_float_clear_offset_, exclusion.rect.BlockEndOffset());
  }

  // If the exclusion takes up no inline space, we shouldn't pay any further
  // attention to it. The only thing it can affect is block-axis positioning of
  // subsequent floats (dealt with above).
  if (exclusion.rect.LineEndOffset() <= exclusion.rect.LineStartOffset())
    return;

#if DCHECK_IS_ON()
  bool inserted = false;
#endif
  // Modify the shelves and opportunities given this new exclusion.
  //
  // NOTE: This could potentially be done lazily when we query the exclusion
  // space for a layout opportunity.
  for (wtf_size_t i = 0; i < shelves_.size(); ++i) {
    // We modify the current shelf in-place. However we need to keep a copy of
    // the shelf if we need to insert a new shelf later in the loop.
    base::Optional<NGShelf> shelf_copy;

    bool is_between_shelves;

    // A new scope is created as shelf may be removed.
    {
      NGShelf& shelf = shelves_[i];

      // Check if we need to insert a new shelf between two other shelves. E.g.
      //
      //    0 1 2 3 4 5 6 7 8
      // 0  +-----+X----X+---+
      //    |xxxxx|      |xxx|
      // 10 +-----+      |xxx|
      //      +---+      |xxx|
      // 20   |NEW|      |xxx|
      //    X-----------X|xxx|
      // 30              |xxx|
      //    X----------------X
      //
      // In the above example the "NEW" left exclusion creates a shelf between
      // the two other shelves drawn.
      //
      // NOTE: We calculate this upfront as we may remove the shelf we need to
      // check against.
      //
      // NOTE: If there is no "next" shelf, we consider this between shelves.
      is_between_shelves =
          exclusion_end_offset >= shelf.block_offset &&
          (i + 1 >= shelves_.size() ||
           exclusion_end_offset < shelves_[i + 1].block_offset);

      if (is_between_shelves)
        shelf_copy.emplace(shelf);

      // Check if the new exclusion will be below this shelf. E.g.
      //
      //    0 1 2 3 4 5 6 7 8
      // 0  +---+
      //    |xxx|
      // 10 |xxx|
      //    X---------------X
      // 20          +---+
      //             |NEW|
      //             +---+
      bool is_below = exclusion.rect.BlockStartOffset() > shelf.block_offset;

      if (is_below) {
        // We may have created a new opportunity, by closing off an area.
        // However we need to check the "edges" of the opportunity are solid.
        //
        //    0 1 2 3 4 5 6 7 8
        // 0  +---+  X----X+---+
        //    |xxx|  .     |xxx|
        // 10 |xxx|  .     |xxx|
        //    +---+  .     +---+
        // 20        .     .
        //      +---+. .+---+
        // 30   |xxx|   |NEW|
        //      |xxx|   +---+
        // 40   +---+
        //
        // In the above example we have three exclusions placed in the space.
        // And we are adding the "NEW" right exclusion.
        //
        // The drawn "shelf" has the internal values:
        // {
        //   block_offset: 0,
        //   line_left: 35,
        //   line_right: 60,
        //   line_left_edges: [{25, 40}],
        //   line_right_edges: [{0, 15}],
        // }
        // The hypothetical opportunity starts at (35,0), and ends at (60,25).
        //
        // The new exclusion *doesn't* create a new layout opportunity, as the
        // left edge doesn't have a solid "edge", i.e. there are no floats
        // against that edge.
        bool has_solid_edges =
            HasSolidEdges(shelf.line_left_edges, shelf.block_offset,
                          exclusion.rect.BlockStartOffset()) &&
            HasSolidEdges(shelf.line_right_edges, shelf.block_offset,
                          exclusion.rect.BlockStartOffset());

        // This just checks if the exclusion overlaps the bounds of the shelf.
        //
        //    0 1 2 3 4 5 6 7 8
        // 0  +---+X------X+---+
        //    |xxx|        |xxx|
        // 10 |xxx|        |xxx|
        //    +---+        +---+
        // 20
        //                  +---+
        // 30               |NEW|
        //                  +---+
        //
        // In the above example the "NEW" exclusion *doesn't* overlap with the
        // above drawn shelf, and a new opportunity hasn't been created.
        bool is_overlapping =
            exclusion.rect.LineStartOffset() < shelf.line_right &&
            exclusion.rect.LineEndOffset() > shelf.line_left;

        // Insert a closed-off layout opportunity if needed.
        if (has_solid_edges && is_overlapping) {
          NGLayoutOpportunity opportunity(
              NGBfcRect(
                  /* start_offset */ {shelf.line_left, shelf.block_offset},
                  /* end_offset */ {shelf.line_right,
                                    exclusion.rect.BlockStartOffset()}),
              shelf.has_shape_exclusions ? base::AdoptRef(new NGShapeExclusions(
                                               *shelf.shape_exclusions))
                                         : nullptr);

          InsertOpportunity(opportunity, &opportunities_);
        }
      }

      // Check if the new exclusion is going to "cut" (intersect) with this
      // shelf. E.g.
      //
      //    0 1 2 3 4 5 6 7 8
      // 0  +---+
      //    |xxx|
      // 10 |xxx|    +---+
      //    X--------|NEW|--X
      //             +---+
      bool is_intersecting =
          !is_below && exclusion_end_offset > shelf.block_offset;

      // We need to reduce the size of the shelf.
      if (is_below || is_intersecting) {
        if (exclusion.type == EFloat::kLeft) {
          if (exclusion.rect.LineEndOffset() >= shelf.line_left) {
            // The edges need to be cleared if it pushes the shelf edge in.
            if (exclusion.rect.LineEndOffset() > shelf.line_left)
              shelf.line_left_edges.clear();
            shelf.line_left = exclusion.rect.LineEndOffset();
            shelf.line_left_edges.emplace_back(&exclusion);
          }
          shelf.shape_exclusions->line_left_shapes.emplace_back(&exclusion);
        } else {
          DCHECK_EQ(exclusion.type, EFloat::kRight);
          if (exclusion.rect.LineStartOffset() <= shelf.line_right) {
            // The edges need to be cleared if it pushes the shelf edge in.
            if (exclusion.rect.LineStartOffset() < shelf.line_right)
              shelf.line_right_edges.clear();
            shelf.line_right = exclusion.rect.LineStartOffset();
            shelf.line_right_edges.emplace_back(&exclusion);
          }
          shelf.shape_exclusions->line_right_shapes.emplace_back(&exclusion);
        }

        // We collect all exclusions in shape_exclusions (even if they don't
        // have any shape data associated with them - normal floats need to be
        // included in the shape line algorithm). We use this bool to track
        // if the shape exclusions should be copied to the resulting layout
        // opportunity.
        if (exclusion.shape_data)
          shelf.has_shape_exclusions = true;

        // Just in case the shelf has a negative inline-size.
        shelf.line_right = std::max(shelf.line_left, shelf.line_right);

        // We can end up in a situation where a shelf is the same as the
        // previous one. For example:
        //
        //    0 1 2 3 4 5 6 7 8
        // 0  +---+X----------X
        //    |xxx|
        // 10 |xxx|
        //    X---------------X
        // 20
        //      +---+
        // 30   |NEW|
        //      +---+
        //
        // In the above example "NEW" will shrink the two shelves by the same
        // amount. We remove the current shelf we are working on.
        bool is_same_as_previous =
            (i > 0) && shelf.line_left == shelves_[i - 1].line_left &&
            shelf.line_right == shelves_[i - 1].line_right;
        if (is_same_as_previous) {
          shelves_.EraseAt(i);
          --i;
        }
      }
    }

    if (is_between_shelves) {
#if DCHECK_IS_ON()
      DCHECK(!inserted);
      inserted = true;
#endif
      DCHECK(shelf_copy.has_value());

      // We only want to add the shelf if it's at a different block offset.
      if (exclusion_end_offset != shelf_copy->block_offset) {
        NGShelf new_shelf(/* block_offset */ exclusion_end_offset);

        // shelf_copy->line_{left,right}_edges will not valid after these calls.
        CollectSolidEdges(&shelf_copy->line_left_edges, new_shelf.block_offset,
                          &new_shelf.line_left_edges);

        CollectSolidEdges(&shelf_copy->line_right_edges, new_shelf.block_offset,
                          &new_shelf.line_right_edges);

        // The new shelf adopts the copy exclusions. This may contain
        // exclusions which are above this shelf, however we'll filter these
        // out when/if we need to calculate the line opportunity.
        new_shelf.shape_exclusions = std::move(shelf_copy->shape_exclusions);
        new_shelf.has_shape_exclusions = shelf_copy->has_shape_exclusions;

        // If we didn't find any edges, the line_left/line_right of the shelf
        // are pushed out to be the minimum/maximum.
        new_shelf.line_left = new_shelf.line_left_edges.IsEmpty()
                                  ? LayoutUnit::Min()
                                  : shelf_copy->line_left;
        new_shelf.line_right = new_shelf.line_right_edges.IsEmpty()
                                   ? LayoutUnit::Max()
                                   : shelf_copy->line_right;

        shelves_.insert(i + 1, new_shelf);
      }

      // It's safe to early exit out of this loop now. This exclusion won't
      // have any effect on subsequent shelves.
      break;
    }
  }

#if DCHECK_IS_ON()
  // We must have performed a new shelf insertion.
  DCHECK(inserted);
#endif
}

NGLayoutOpportunity
NGExclusionSpaceInternal::DerivedGeometry::FindLayoutOpportunity(
    const NGBfcOffset& offset,
    const LayoutUnit available_inline_size,
    const NGLogicalSize& minimum_size) const {
  // TODO(ikilpatrick): Determine what to do for a -ve available_inline_size.
  // TODO(ikilpatrick): Change this so that it iterates over the
  // shelves/opportunities instead for querying for all of them.
  LayoutOpportunityVector opportunities =
      AllLayoutOpportunities(offset, available_inline_size);

  for (const auto& opportunity : opportunities) {
    // Determine if this opportunity will fit the given size.
    //
    // NOTE: There are cases where the available_inline_size may be smaller
    // than the minimum_size.inline_size. In such cases if the opportunity is
    // the same as the available_inline_size, it pretends that it "fits".
    if ((opportunity.rect.InlineSize() >= minimum_size.inline_size ||
         opportunity.rect.InlineSize() == available_inline_size) &&
        opportunity.rect.BlockSize() >= minimum_size.block_size)
      return opportunity;
  }

  NOTREACHED();
  return NGLayoutOpportunity();
}

LayoutOpportunityVector
NGExclusionSpaceInternal::DerivedGeometry::AllLayoutOpportunities(
    const NGBfcOffset& offset,
    const LayoutUnit available_inline_size) const {
  LayoutOpportunityVector opportunities;

  auto* shelves_it = shelves_.begin();
  auto* opps_it = opportunities_.begin();

  auto* const shelves_end = shelves_.end();
  auto* const opps_end = opportunities_.end();

  while (shelves_it != shelves_end || opps_it != opps_end) {
    // We should never exhaust the opportunities list before the shelves list,
    // as there is always an infinitely sized shelf at the very end.
    DCHECK_NE(shelves_it, shelves_end);
    const NGShelf& shelf = *shelves_it;

    if (!Intersects(shelf, offset, available_inline_size)) {
      ++shelves_it;
      continue;
    }

    if (opps_it != opps_end) {
      const NGLayoutOpportunity& opportunity = *opps_it;

      if (!Intersects(opportunity, offset, available_inline_size)) {
        ++opps_it;
        continue;
      }

      // We always prefer the closed-off opportunity, instead of the shelf
      // opportunity if they exist at the some offset.
      if (opportunity.rect.BlockStartOffset() <= shelf.block_offset) {
        opportunities.push_back(CreateLayoutOpportunity(opportunity, offset,
                                                        available_inline_size));
        ++opps_it;
        continue;
      }
    }

    // We may fall through to here from the above if-statement.
    bool has_solid_edges =
        HasSolidEdges(shelf.line_left_edges, offset.block_offset,
                      LayoutUnit::Max()) &&
        HasSolidEdges(shelf.line_right_edges, offset.block_offset,
                      LayoutUnit::Max());
    if (has_solid_edges) {
      opportunities.push_back(
          CreateLayoutOpportunity(shelf, offset, available_inline_size));
    }
    ++shelves_it;
  }

  return opportunities;
}

LayoutUnit NGExclusionSpaceInternal::DerivedGeometry::ClearanceOffset(
    EClear clear_type) const {
  switch (clear_type) {
    case EClear::kNone:
      return LayoutUnit::Min();  // Nothing to do here.
    case EClear::kLeft:
      return left_float_clear_offset_;
    case EClear::kRight:
      return right_float_clear_offset_;
    case EClear::kBoth:
      return std::max(left_float_clear_offset_, right_float_clear_offset_);
    default:
      NOTREACHED();
  }

  return LayoutUnit::Min();
}

const NGExclusionSpaceInternal::DerivedGeometry&
NGExclusionSpaceInternal::GetDerivedGeometry() const {
  // Re-build the geometry if it isn't present.
  if (!derived_geometry_) {
    derived_geometry_ = std::make_unique<DerivedGeometry>();
    DCHECK_LE(num_exclusions_, exclusions_->size());
    for (wtf_size_t i = 0; i < num_exclusions_; ++i)
      derived_geometry_->Add(*exclusions_->GetVector()[i]);
  }

  return *derived_geometry_;
}

bool NGExclusionSpaceInternal::operator==(
    const NGExclusionSpaceInternal& other) const {
  if (num_exclusions_ == 0 && other.num_exclusions_ == 0)
    return true;
  return num_exclusions_ == other.num_exclusions_ &&
         exclusions_ == other.exclusions_;
}

}  // namespace blink
