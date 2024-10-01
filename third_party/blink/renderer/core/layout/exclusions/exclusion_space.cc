// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/exclusions/exclusion_space.h"

#include <algorithm>
#include <optional>

#include "third_party/blink/renderer/core/layout/exclusions/exclusion_area.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"

namespace blink {

namespace {

// Inserts a layout opportunity into the completed list. The list is ordered by
// block-start, then by inline-size (shrinking) / block-size (growing).
//
// We don't explicitly check the inline-size/block-size of the opportunity as
// they are always produced in the order.
void InsertClosedArea(
    const ExclusionSpaceInternal::ClosedArea area,
    HeapVector<ExclusionSpaceInternal::ClosedArea, 4>& areas) {
  if (areas.empty()) {
    areas.emplace_back(area);
    return;
  }

  // We go backwards through the list as there is a higher probability that a
  // new area will be at the end of the list.
  for (wtf_size_t i = areas.size(); i--;) {
    const ExclusionSpaceInternal::ClosedArea& other = areas.at(i);
    if (other.opportunity.rect.BlockStartOffset() <=
        area.opportunity.rect.BlockStartOffset()) {
#if DCHECK_IS_ON()
      // If we have the same block-start offset ensure that the size of the
      // opportunity doesn't violate the order.
      if (other.opportunity.rect.BlockStartOffset() ==
          area.opportunity.rect.BlockStartOffset()) {
        DCHECK_LE(other.opportunity.rect.BlockSize(),
                  area.opportunity.rect.BlockSize());
        DCHECK_GE(other.opportunity.rect.InlineSize(),
                  area.opportunity.rect.InlineSize());
      }
#endif

      areas.insert(i + 1, area);
      return;
    }
  }

  // The first closed-off area we insert is almost always at LayoutUnit::Min().
  //
  // However if a float is placed at LayoutUnit::Min() it is possible to get
  // into a state where this isn't the case (the first closed-off area might be
  // directly below that float for example).
  //
  // When a subsequent float gets placed, it might create a closed-off area at
  // LayoutUnit::Min(), and should be inserted at the front of the areas list.
  DCHECK_EQ(area.opportunity.rect.BlockStartOffset(), LayoutUnit::Min());
  areas.push_front(area);
}

// Returns true if there is at least one edge between block_start and block_end.
bool HasSolidEdges(const Vector<ExclusionSpaceInternal::ShelfEdge>& edges,
                   LayoutUnit block_start,
                   LayoutUnit block_end) {
  // If there aren't any adjacent exclusions, we must be the initial shelf.
  // This always has "solid" edges on either side.
  if (edges.empty())
    return true;

  for (const auto& edge : edges) {
    if (edge.block_end > block_start && edge.block_start < block_end)
      return true;
  }

  return false;
}

// Adds any edges (other exclusions) which are within the range:
// (block_offset, LayoutUnit::Max())
// to the given out_edges vector.
// edges will be invalid after this call.
void CollectSolidEdges(Vector<ExclusionSpaceInternal::ShelfEdge>* edges,
                       LayoutUnit block_offset,
                       Vector<ExclusionSpaceInternal::ShelfEdge>* out_edges) {
  *out_edges = std::move(*edges);
  auto it = std::remove_if(
      out_edges->begin(), out_edges->end(),
      [&](const auto& edge) { return edge.block_end <= block_offset; });
  out_edges->erase(it, out_edges->end());
}

// Returns true if the area defined by the given offset and inline_size
// intersects with the opportunity.
//
// We only need to check the block-end of the opportunity is below the given
// offset, as the given area extends to a block-end of infinity.
bool Intersects(const LayoutOpportunity& opportunity,
                const BfcOffset& offset,
                const LayoutUnit inline_size) {
  return opportunity.rect.LineEndOffset() >= offset.line_offset &&
         opportunity.rect.LineStartOffset() <=
             offset.line_offset + inline_size &&
         opportunity.rect.BlockEndOffset() > offset.block_offset;
}

// Creates a new layout opportunity. The given layout opportunity *must*
// intersect with the given area (defined by offset and inline_size).
LayoutOpportunity CreateLayoutOpportunity(const LayoutOpportunity& other,
                                          const BfcOffset& offset,
                                          const LayoutUnit inline_size) {
  DCHECK(Intersects(other, offset, inline_size));

  BfcOffset start_offset(
      std::max(other.rect.LineStartOffset(), offset.line_offset),
      std::max(other.rect.start_offset.block_offset, offset.block_offset));

  BfcOffset end_offset(
      std::min(other.rect.LineEndOffset(), offset.line_offset + inline_size),
      other.rect.BlockEndOffset());

  return LayoutOpportunity(
      BfcRect(start_offset, end_offset),
      other.shape_exclusions
          ? MakeGarbageCollected<ShapeExclusions>(*other.shape_exclusions)
          : nullptr);
}

// Creates a new layout opportunity. The given shelf *must* intersect with the
// given area (defined by offset and inline_size).
LayoutOpportunity CreateLayoutOpportunity(
    const ExclusionSpaceInternal::Shelf& shelf,
    const BfcOffset& offset,
    const LayoutUnit inline_size) {
  BfcOffset start_offset(std::max(shelf.line_left, offset.line_offset),
                         std::max(shelf.block_offset, offset.block_offset));

  // Max with |start_offset.line_offset| in case the shelf has a negative
  // inline-size.
  BfcOffset end_offset(
      std::max(std::min(shelf.line_right, offset.line_offset + inline_size),
               start_offset.line_offset),
      LayoutUnit::Max());

  return LayoutOpportunity(
      BfcRect(start_offset, end_offset),
      shelf.has_shape_exclusions
          ? MakeGarbageCollected<ShapeExclusions>(*shelf.shape_exclusions)
          : nullptr);
}

}  // namespace

ExclusionSpaceInternal::ExclusionSpaceInternal()
    : exclusions_(MakeGarbageCollected<ExclusionAreaPtrArray>()),
      track_shape_exclusions_(false),
      has_break_before_left_float_(false),
      has_break_before_right_float_(false),
      has_break_inside_left_float_(false),
      has_break_inside_right_float_(false) {}

ExclusionSpaceInternal::ExclusionSpaceInternal(
    const ExclusionSpaceInternal& other)
    : exclusions_(other.exclusions_),
      num_exclusions_(other.num_exclusions_),
      left_clear_offset_(other.left_clear_offset_),
      right_clear_offset_(other.right_clear_offset_),
      last_float_block_start_(other.last_float_block_start_),
      initial_letter_left_clear_offset_(
          other.initial_letter_left_clear_offset_),
      initial_letter_right_clear_offset_(
          other.initial_letter_right_clear_offset_),
      non_hidden_clear_offset_(other.non_hidden_clear_offset_),
      track_shape_exclusions_(other.track_shape_exclusions_),
      has_break_before_left_float_(other.has_break_before_left_float_),
      has_break_before_right_float_(other.has_break_before_right_float_),
      has_break_inside_left_float_(other.has_break_inside_left_float_),
      has_break_inside_right_float_(other.has_break_inside_right_float_),
      derived_geometry_(std::move(other.derived_geometry_)) {
  // This copy-constructor does fun things. It moves the derived_geometry_ to
  // the newly created exclusion space where it'll more-likely be used.
  other.derived_geometry_ = nullptr;
}

ExclusionSpaceInternal& ExclusionSpaceInternal::operator=(
    const ExclusionSpaceInternal& other) {
  CopyFrom(other);
  derived_geometry_ = std::move(other.derived_geometry_);
  other.derived_geometry_ = nullptr;
  return *this;
}

void ExclusionSpaceInternal::CopyFrom(const ExclusionSpaceInternal& other) {
  exclusions_ = other.exclusions_;
  num_exclusions_ = other.num_exclusions_;
  left_clear_offset_ = other.left_clear_offset_;
  right_clear_offset_ = other.right_clear_offset_;
  last_float_block_start_ = other.last_float_block_start_;
  initial_letter_left_clear_offset_ = other.initial_letter_left_clear_offset_;
  initial_letter_right_clear_offset_ = other.initial_letter_right_clear_offset_;
  non_hidden_clear_offset_ = other.non_hidden_clear_offset_;
  track_shape_exclusions_ = other.track_shape_exclusions_;
  has_break_before_left_float_ = other.has_break_before_left_float_;
  has_break_before_right_float_ = other.has_break_before_right_float_;
  has_break_inside_left_float_ = other.has_break_inside_left_float_;
  has_break_inside_right_float_ = other.has_break_inside_right_float_;
  // `derived_geometry_` is a cached value that can be generated when needed.
  derived_geometry_ = nullptr;
}

void ExclusionSpace::CopyFrom(const ExclusionSpace& other) {
  if (!other.exclusion_space_) {
    exclusion_space_ = nullptr;
    return;
  }
  exclusion_space_ = std::make_unique<ExclusionSpaceInternal>();
  exclusion_space_->CopyFrom(*other.exclusion_space_);
}

ExclusionSpaceInternal::DerivedGeometry::DerivedGeometry(
    LayoutUnit block_offset_limit,
    bool track_shape_exclusions)
    : block_offset_limit_(block_offset_limit),
      track_shape_exclusions_(track_shape_exclusions) {
  // The exclusion space must always have at least one shelf, at -Infinity.
  shelves_.emplace_back(/* block_offset */ LayoutUnit::Min(),
                        track_shape_exclusions_);
}

void ExclusionSpaceInternal::Add(const ExclusionArea* exclusion) {
  DCHECK_LE(num_exclusions_, exclusions_->size());

  bool already_exists = false;

  if (num_exclusions_ < exclusions_->size()) {
    if (*exclusion == *exclusions_->at(num_exclusions_)) {
      // We might be adding an exclusion seen in a previous layout pass.
      already_exists = true;
    } else {
      // Perform a copy-on-write if the number of exclusions has gone out of
      // sync.
      auto* exclusions = MakeGarbageCollected<ExclusionAreaPtrArray>();
      exclusions->AppendSpan(base::span(*exclusions_).first(num_exclusions_));
      exclusions_ = exclusions;
    }
  }

  // If this is the first exclusion with shape_data, the derived_geometry_
  // member now needs to perform additional bookkeeping, and is invalid.
  if (!track_shape_exclusions_ && exclusion->shape_data) {
    track_shape_exclusions_ = true;
    derived_geometry_ = nullptr;
  }

  LayoutUnit exclusion_block_offset = exclusion->rect.BlockStartOffset();

  // We can safely mutate the exclusion here as an exclusion will never be
  // reused if this invariant doesn't hold.
  const_cast<ExclusionArea*>(exclusion)->is_past_other_exclusions =
      exclusion_block_offset >=
      std::max({left_clear_offset_, exclusion_block_offset, right_clear_offset_,
                exclusion_block_offset, initial_letter_left_clear_offset_,
                exclusion_block_offset, initial_letter_right_clear_offset_});

  // Update the members used for clearance calculations.
  LayoutUnit clear_offset = exclusion->rect.BlockEndOffset();
  if (exclusion->IsForInitialLetterBox()) [[unlikely]] {
    if (exclusion->type == EFloat::kLeft) {
      initial_letter_left_clear_offset_ =
          std::max(initial_letter_left_clear_offset_, clear_offset);
    } else if (exclusion->type == EFloat::kRight) {
      initial_letter_right_clear_offset_ =
          std::max(initial_letter_right_clear_offset_, clear_offset);
    }

    if (!exclusion->is_hidden_for_paint) {
      non_hidden_clear_offset_ =
          std::max(non_hidden_clear_offset_, clear_offset);
    }

    if (!already_exists) {
      // Perform a copy-on-write if the number of exclusions has gone out of
      // sync.
      const auto& source_exclusions = *exclusions_;
      exclusions_ = MakeGarbageCollected<ExclusionAreaPtrArray>();
      exclusions_->resize(num_exclusions_ + 1);
      const auto source_span =
          base::span(source_exclusions).first(num_exclusions_);
      const auto source_end = source_span.end();
      // Initial-letters are special in that they can be inserted "before"
      // other floats. Ensure we insert |exclusion| in the correct place
      // (ascent order by block-start).
      auto destination_span = base::span(*exclusions_);
      auto destination = destination_span.begin();
      for (auto it = source_span.begin(); it != source_end; ++it) {
        if (exclusion->rect.BlockStartOffset() <
            (*it)->rect.BlockStartOffset()) {
          *destination = exclusion;
          destination = std::copy(it, source_end, destination + 1);
          break;
        }
        *destination++ = *it;
      }
      if (destination != destination_span.end()) {
        *destination++ = exclusion;
      }
      DCHECK(destination == destination_span.end());
    }
    num_exclusions_++;

    if (exclusions_->at(num_exclusions_ - 1) != exclusion)
      derived_geometry_ = nullptr;
    if (derived_geometry_)
      derived_geometry_->Add(*exclusion);
    return;
  }

  last_float_block_start_ =
      std::max(last_float_block_start_, exclusion_block_offset);

  if (exclusion->type == EFloat::kLeft)
    left_clear_offset_ = std::max(left_clear_offset_, clear_offset);
  else if (exclusion->type == EFloat::kRight)
    right_clear_offset_ = std::max(right_clear_offset_, clear_offset);

  if (!exclusion->is_hidden_for_paint) {
    non_hidden_clear_offset_ = std::max(non_hidden_clear_offset_, clear_offset);
  }

  if (derived_geometry_)
    derived_geometry_->Add(*exclusion);

  if (!already_exists)
    exclusions_->emplace_back(std::move(exclusion));
  num_exclusions_++;
}

void ExclusionSpaceInternal::DerivedGeometry::Add(
    const ExclusionArea& exclusion) {
  DCHECK_GE(exclusion.rect.BlockStartOffset(), block_offset_limit_);

  // If the exclusion takes up no inline space, we shouldn't pay any further
  // attention to it. The only thing it can affect is block-axis positioning of
  // subsequent floats (dealt with above).
  if (exclusion.rect.LineEndOffset() <= exclusion.rect.LineStartOffset())
    return;

  const LayoutUnit exclusion_end_offset = exclusion.rect.BlockEndOffset();

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
    std::optional<Shelf> shelf_copy;

    bool is_between_shelves;

    // A new scope is created as shelf may be removed.
    {
      Shelf& shelf = shelves_[i];

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
        //                 +---+
        // 30              |NEW|
        //                 +---+
        //
        // In the above example the "NEW" exclusion *doesn't* overlap with the
        // above drawn shelf, and a new opportunity hasn't been created.
        //
        // NOTE: below we have subtly different conditions for left/right
        // exclusions. E.g. if a right exclusion is aligned with the right edge
        // of the shelf (see above) then we *shouldn't* create a new area.
        //
        // However we may have a left exclusion whose left edge is aligned with
        // the right edge of the shelf. In this case we *do* create a new area.
        bool is_overlapping;
        if (exclusion.type == EFloat::kLeft) {
          is_overlapping =
              exclusion.rect.LineStartOffset() <= shelf.line_right &&
              exclusion.rect.LineEndOffset() > shelf.line_left;
        } else {
          DCHECK_EQ(exclusion.type, EFloat::kRight);
          is_overlapping =
              exclusion.rect.LineStartOffset() < shelf.line_right &&
              exclusion.rect.LineEndOffset() >= shelf.line_left;
        }

        // Insert a closed-off layout opportunity if needed.
        if (has_solid_edges && is_overlapping) {
          LayoutOpportunity opportunity(
              BfcRect(
                  /* start_offset */ {shelf.line_left, shelf.block_offset},
                  /* end_offset */ {shelf.line_right,
                                    exclusion.rect.BlockStartOffset()}),
              shelf.has_shape_exclusions
                  ? MakeGarbageCollected<ShapeExclusions>(
                        *shelf.shape_exclusions)
                  : nullptr);

          InsertClosedArea(ClosedArea(opportunity, shelf.line_left_edges,
                                      shelf.line_right_edges),
                           areas_);
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
            shelf.line_left_edges.emplace_back(
                exclusion.rect.BlockStartOffset(),
                exclusion.rect.BlockEndOffset());
          }
          if (shelf.shape_exclusions)
            shelf.shape_exclusions->line_left_shapes.emplace_back(&exclusion);
        } else {
          DCHECK_EQ(exclusion.type, EFloat::kRight);
          if (exclusion.rect.LineStartOffset() <= shelf.line_right) {
            // The edges need to be cleared if it pushes the shelf edge in.
            if (exclusion.rect.LineStartOffset() < shelf.line_right)
              shelf.line_right_edges.clear();
            shelf.line_right = exclusion.rect.LineStartOffset();
            shelf.line_right_edges.emplace_back(
                exclusion.rect.BlockStartOffset(),
                exclusion.rect.BlockEndOffset());
          }
          if (shelf.shape_exclusions)
            shelf.shape_exclusions->line_right_shapes.emplace_back(&exclusion);
        }

        // We collect all exclusions in shape_exclusions (even if they don't
        // have any shape data associated with them - normal floats need to be
        // included in the shape line algorithm). We use this bool to track
        // if the shape exclusions should be copied to the resulting layout
        // opportunity.
        if (exclusion.shape_data)
          shelf.has_shape_exclusions = true;

        // A shelf can be completely closed off and not needed anymore. For
        // example:
        //
        //    0 1 2 3 4 5 6 7 8
        // 0  +---+X----------X
        //    |xxx|
        // 10 |xxx|
        //    +---+
        // 20
        //       +-----------+
        // 30    |NEW (right)|
        //       +-----------+
        //
        // In the above example "NEW (right)" will have shrunk the shelf such
        // that line_right will now be smaller than line_left.
        bool is_closed_off = shelf.line_left > shelf.line_right;

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
        if (is_closed_off || is_same_as_previous) {
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
        Shelf new_shelf(/* block_offset */ exclusion_end_offset,
                        track_shape_exclusions_);

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
        new_shelf.line_left = new_shelf.line_left_edges.empty()
                                  ? LayoutUnit::Min()
                                  : shelf_copy->line_left;
        new_shelf.line_right = new_shelf.line_right_edges.empty()
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

LayoutOpportunity
ExclusionSpaceInternal::DerivedGeometry::FindLayoutOpportunity(
    const BfcOffset& offset,
    const LayoutUnit available_inline_size,
    const LayoutUnit minimum_inline_size) const {
  // TODO(ikilpatrick): Determine what to do for a -ve available_inline_size.
  DCHECK_GE(offset.block_offset, block_offset_limit_);

  LayoutOpportunity return_opportunity;
  IterateAllLayoutOpportunities(
      offset, available_inline_size,
      [&return_opportunity, &offset, &available_inline_size,
       &minimum_inline_size](const LayoutOpportunity opportunity) -> bool {
        // Determine if this opportunity will fit the given size.
        //
        // NOTE: There are cases where the |available_inline_size| may be
        // smaller than the |minimum_inline_size|. In such cases if the
        // opportunity is the same as the |available_inline_size|, it pretends
        // that it "fits".
        if (opportunity.rect.InlineSize() >= minimum_inline_size ||
            (opportunity.rect.InlineSize() == available_inline_size &&
             opportunity.rect.LineStartOffset() == offset.line_offset)) {
          return_opportunity = std::move(opportunity);
          return true;
        }

        return false;
      });

  return return_opportunity;
}

LayoutOpportunityVector
ExclusionSpaceInternal::DerivedGeometry::AllLayoutOpportunities(
    const BfcOffset& offset,
    const LayoutUnit available_inline_size) const {
  DCHECK_GE(offset.block_offset, block_offset_limit_);
  LayoutOpportunityVector opportunities;

  // This method is only used for determining the position of line-boxes.
  IterateAllLayoutOpportunities(
      offset, available_inline_size,
      [&opportunities](const LayoutOpportunity opportunity) -> bool {
        opportunities.push_back(std::move(opportunity));
        return false;
      });

  return opportunities;
}

template <typename LambdaFunc>
void ExclusionSpaceInternal::DerivedGeometry::IterateAllLayoutOpportunities(
    const BfcOffset& offset,
    const LayoutUnit available_inline_size,
    const LambdaFunc& lambda) const {
  auto shelves_span = base::span(shelves_);
  auto areas_span = base::span(areas_);
  auto shelves_it = shelves_span.begin();
  auto areas_it = areas_span.begin();
  auto const shelves_end = shelves_span.end();
  auto const areas_end = areas_span.end();

  while (shelves_it != shelves_end || areas_it != areas_end) {
    // We should never exhaust the opportunities list before the shelves list,
    // as there is always an infinitely sized shelf at the very end.
    DCHECK(shelves_it != shelves_end);
    const Shelf& shelf = *shelves_it;

    if (areas_it != areas_end) {
      const ClosedArea& area = *areas_it;

      if (!Intersects(area.opportunity, offset, available_inline_size)) {
        ++areas_it;
        continue;
      }

      LayoutUnit block_start_offset = std::max(
          area.opportunity.rect.BlockStartOffset(), offset.block_offset);

      // We always prefer the closed-off area opportunity, instead of the shelf
      // opportunity if they exist at the some offset.
      if (block_start_offset <=
          std::max(shelf.block_offset, offset.block_offset)) {
        LayoutUnit block_end_offset = area.opportunity.rect.BlockEndOffset();

        bool has_solid_edges =
            HasSolidEdges(area.line_left_edges, block_start_offset,
                          block_end_offset) &&
            HasSolidEdges(area.line_right_edges, block_start_offset,
                          block_end_offset);
        if (has_solid_edges) {
          if (lambda(CreateLayoutOpportunity(area.opportunity, offset,
                                             available_inline_size)))
            return;
        }

        ++areas_it;
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
      if (lambda(CreateLayoutOpportunity(shelf, offset, available_inline_size)))
        return;
    }
    ++shelves_it;
  }
}

const ExclusionSpaceInternal::DerivedGeometry&
ExclusionSpaceInternal::GetDerivedGeometry(
    LayoutUnit block_offset_limit) const {
  // We might have a geometry, but built at a lower block-offset limit.
  if (derived_geometry_ &&
      block_offset_limit < derived_geometry_->block_offset_limit_)
    derived_geometry_ = nullptr;

  // Re-build the geometry if it isn't present.
  if (!derived_geometry_) {
    DCHECK_LE(num_exclusions_, exclusions_->size());
    DCHECK_GE(num_exclusions_, 1u);

    auto span = base::span(*exclusions_).first(num_exclusions_);
    const auto begin = span.begin();
    const auto end = span.end();

    // Find the first exclusion whose block-start offset is "after" the
    // |block_offset_limit|.
    auto it = std::lower_bound(
        begin, end, block_offset_limit,
        [](const auto& exclusion, const auto& block_offset) -> bool {
          return exclusion->rect.BlockStartOffset() < block_offset;
        });

    if (it == begin) {
      block_offset_limit = LayoutUnit::Min();
    } else {
#if DCHECK_IS_ON()
      if (it != end)
        DCHECK_GE((*it)->rect.BlockStartOffset(), block_offset_limit);
#endif

      // Find the "highest" exclusion possible which itself is past other
      // exclusions.
      while (--it != begin) {
        if ((*it)->is_past_other_exclusions)
          break;
      }

      // This exclusion must be above the given block-offset limit.
      DCHECK_LE((*it)->rect.BlockStartOffset(), block_offset_limit);
      block_offset_limit = (*it)->rect.BlockStartOffset();
    }

    // Add all the exclusions below the block-offset limit.
    derived_geometry_ = MakeGarbageCollected<DerivedGeometry>(
        block_offset_limit, track_shape_exclusions_);
    for (; it < end; ++it)
      derived_geometry_->Add(**it);
  }

  return *derived_geometry_;
}

bool ExclusionSpaceInternal::operator==(
    const ExclusionSpaceInternal& other) const {
  if (num_exclusions_ == 0 && other.num_exclusions_ == 0)
    return true;
  return num_exclusions_ == other.num_exclusions_ &&
         exclusions_ == other.exclusions_ &&
         has_break_before_left_float_ == other.has_break_before_left_float_ &&
         has_break_before_right_float_ == other.has_break_before_right_float_ &&
         has_break_inside_left_float_ == other.has_break_inside_left_float_ &&
         has_break_inside_right_float_ == other.has_break_inside_right_float_;
}

}  // namespace blink
