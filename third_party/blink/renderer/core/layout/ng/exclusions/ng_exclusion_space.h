// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGExclusionSpace_h
#define NGExclusionSpace_h

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/ng/exclusions/ng_exclusion.h"
#include "third_party/blink/renderer/core/layout/ng/exclusions/ng_layout_opportunity.h"
#include "third_party/blink/renderer/core/layout/ng/geometry/ng_bfc_offset.h"
#include "third_party/blink/renderer/core/layout/ng/geometry/ng_bfc_rect.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "third_party/blink/renderer/platform/wtf/allocator.h"
#include "third_party/blink/renderer/platform/wtf/ref_vector.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

typedef Vector<NGLayoutOpportunity, 8> LayoutOpportunityVector;

// This class is an implementation detail. For use of the exclusion space,
// see NGExclusionSpace below. NGExclusionSpace was designed to be cheap
// to construct and cheap to copy if empty.
class CORE_EXPORT NGExclusionSpaceInternal {
  USING_FAST_MALLOC(NGExclusionSpaceInternal);

 public:
  NGExclusionSpaceInternal();
  NGExclusionSpaceInternal(const NGExclusionSpaceInternal&);
  NGExclusionSpaceInternal(NGExclusionSpaceInternal&&) noexcept;
  NGExclusionSpaceInternal& operator=(const NGExclusionSpaceInternal&);
  NGExclusionSpaceInternal& operator=(NGExclusionSpaceInternal&&) noexcept;
  ~NGExclusionSpaceInternal() {}

  void Add(scoped_refptr<const NGExclusion> exclusion);

  NGLayoutOpportunity FindLayoutOpportunity(
      const NGBfcOffset& offset,
      const LayoutUnit available_inline_size,
      const NGLogicalSize& minimum_size) const {
    // If the area clears all floats, we can just return the layout opportunity
    // which matches the available space.
    if (offset.block_offset >= both_clear_offset_) {
      NGBfcOffset end_offset(
          offset.line_offset + available_inline_size.ClampNegativeToZero(),
          LayoutUnit::Max());
      return NGLayoutOpportunity(NGBfcRect(offset, end_offset), nullptr);
    }

    return GetDerivedGeometry().FindLayoutOpportunity(
        offset, available_inline_size, minimum_size);
  }

  LayoutOpportunityVector AllLayoutOpportunities(
      const NGBfcOffset& offset,
      const LayoutUnit available_inline_size) const {
    // If the area clears all floats, we can just return a single layout
    // opportunity which matches the available space.
    if (offset.block_offset >= both_clear_offset_) {
      NGBfcOffset end_offset(
          offset.line_offset + available_inline_size.ClampNegativeToZero(),
          LayoutUnit::Max());
      return LayoutOpportunityVector(
          {NGLayoutOpportunity(NGBfcRect(offset, end_offset), nullptr)});
    }

    return GetDerivedGeometry().AllLayoutOpportunities(offset,
                                                       available_inline_size);
  }

  LayoutUnit ClearanceOffset(EClear clear_type) const {
    if (clear_type == EClear::kNone)
      return LayoutUnit::Min();

    return GetDerivedGeometry().ClearanceOffset(clear_type);
  }

  LayoutUnit LastFloatBlockStart() const {
    return GetDerivedGeometry().LastFloatBlockStart();
  }

  bool IsEmpty() const { return !num_exclusions_; }

  bool operator==(const NGExclusionSpaceInternal& other) const;
  bool operator!=(const NGExclusionSpaceInternal& other) const {
    return !(*this == other);
  }

  // The shelf is an internal data-structure representing the bottom of a
  // float. A shelf has a inline-size which is defined by the line_left and
  // line_right members. E.g.
  //
  //    0 1 2 3 4 5 6 7 8
  // 0  +---++--+    +---+
  //    |xxx||xx|    |xxx|
  // 10 |xxx|X-------Xxxx|
  //    +---+        +---+
  // 20
  //
  // In the above diagram the shelf is at the block-end edge of the smallest
  // float. It would have the internal values of:
  // {
  //   block_offset: 10,
  //   line_left: 20,
  //   line_right: 65,
  //   line_left_edges: [{0, 15}],
  //   line_right_edges: [{0, 15}],
  // }
  // The line_left_edges and line_right_edges are all the floats which are
  // "against" the shelf at the line_left and line_right offset respectively.
  //
  // An opportunity has a "solid" edge if there is at least one float adjacent
  // to the line-left or line-right edge. If an opportunity has no adjacent
  // floats it is invalid.
  //
  // These are used for:
  //  - When we create an opportunity, making sure it has "solid" edges.
  //  - The opportunity also holds onto a list of these edges to support
  //    css-shapes.
  struct NGShelf {
    explicit NGShelf(LayoutUnit block_offset)
        : block_offset(block_offset),
          line_left(LayoutUnit::Min()),
          line_right(LayoutUnit::Max()),
          shape_exclusions(base::AdoptRef(new NGShapeExclusions)),
          has_shape_exclusions(false) {}

    // The copy constructor explicitly copies the shape_exclusions member,
    // instead of just incrementing the ref.
    NGShelf(const NGShelf& other)
        : block_offset(other.block_offset),
          line_left(other.line_left),
          line_right(other.line_right),
          line_left_edges(other.line_left_edges),
          line_right_edges(other.line_right_edges),
          shape_exclusions(
              base::AdoptRef(new NGShapeExclusions(*other.shape_exclusions))),
          has_shape_exclusions(other.has_shape_exclusions) {}

    NGShelf(NGShelf&& other) noexcept = default;
    NGShelf& operator=(NGShelf&& other) noexcept = default;

    LayoutUnit block_offset;
    LayoutUnit line_left;
    LayoutUnit line_right;

    Vector<scoped_refptr<const NGExclusion>, 1> line_left_edges;
    Vector<scoped_refptr<const NGExclusion>, 1> line_right_edges;

    // shape_exclusions contains all the floats which sit below this shelf. The
    // has_shape_exclusions member will be true if shape_exclusions contains an
    // exclusion with shape-outside specified (and therefore should be copied
    // to any layout opportunity).
    scoped_refptr<NGShapeExclusions> shape_exclusions;
    bool has_shape_exclusions;
  };

 private:
  // In order to reduce the amount of Vector copies, instances of a
  // NGExclusionSpaceInternal can share the same exclusions_ Vector. See the
  // copy constructor.
  //
  // We implement a copy-on-write behaviour when adding an exclusion (if
  // exclusions_.size(), and num_exclusions_ differs).
  //
  // num_exclusions_ is how many exclusions *this* instance of an exclusion
  // space has, which may differ to the number of exclusions in the Vector.
  scoped_refptr<RefVector<scoped_refptr<const NGExclusion>>> exclusions_;
  wtf_size_t num_exclusions_;
  LayoutUnit both_clear_offset_;

  // The derived geometry struct, is the data-structure which handles all of the
  // queries on the exclusion space. It can always be rebuilt from exclusions_
  // and num_exclusions_. This is mutable as it is passed down a chain of
  // exclusion spaces inside the copy constructor. E.g.
  //
  // NGExclusionSpace space1;
  // space1.Add(exclusion1);
  // space1.LastFloatBlockStart(); // Builds derived_geometry_ to answer query.
  //
  // NGExclusionSpace space2(space1); // Moves derived_geometry_ to space2.
  // space2.Add(exclusion2); // Modifies derived_geometry_.
  //
  // space1.LastFloatBlockStart(); // Re-builds derived_geometry_.
  //
  // This is efficient (desirable) as the common usage pattern is only the last
  // exclusion space in the copy-chain is used for answering queries. Only when
  // we trigger a (rare) re-layout case will we need to rebuild the
  // derived_geometry_ data-structure.
  struct DerivedGeometry {
    USING_FAST_MALLOC(DerivedGeometry);

   public:
    DerivedGeometry();
    DerivedGeometry(DerivedGeometry&& o) noexcept = default;

    void Add(const NGExclusion& exclusion);

    NGLayoutOpportunity FindLayoutOpportunity(
        const NGBfcOffset& offset,
        const LayoutUnit available_inline_size,
        const NGLogicalSize& minimum_size) const;

    LayoutOpportunityVector AllLayoutOpportunities(
        const NGBfcOffset& offset,
        const LayoutUnit available_inline_size) const;

    LayoutUnit ClearanceOffset(EClear clear_type) const;
    LayoutUnit LastFloatBlockStart() const { return last_float_block_start_; }

    // See NGShelf for a broad description of what shelves are. We always begin
    // with one, which has the internal value of:
    // {
    //   block_offset: LayoutUnit::Min(),
    //   line_left: LayoutUnit::Min(),
    //   line_right: LayoutUnit::Max(),
    // }
    //
    // The list of opportunities represent "closed-off" areas. E.g.
    //
    //    0 1 2 3 4 5 6 7 8
    // 0  +---+.      .+---+
    //    |xxx|.      .|xxx|
    // 10 |xxx|.      .|xxx|
    //    +---+.      .+---+
    // 20      ........
    //      +---+
    // 30   |xxx|
    //      |xxx|
    // 40   +---+
    //
    // In the above example the opportunity is represented with the dotted line.
    // It has the internal values of:
    // {
    //   start_offset: {20, LayoutUnit::Min()},
    //   end_offset: {65, 25},
    // }
    // Once an opportunity has been created, it can never been changed due to
    // the property that floats always align their block-start edges.
    //
    // We exploit this property by keeping this list of "closed-off" areas, and
    // removing shelves to make insertion faster.
    Vector<NGShelf, 4> shelves_;
    Vector<NGLayoutOpportunity, 4> opportunities_;

    // This member is used for implementing the "top edge alignment rule" for
    // floats. Floats can be positioned at negative offsets, hence is
    // initialized the minimum value.
    LayoutUnit last_float_block_start_;

    // These members are used for keeping track of the "lowest" offset for each
    // type of float. This is used for implementing float clearance.
    LayoutUnit left_float_clear_offset_;
    LayoutUnit right_float_clear_offset_;
  };

  // Returns the derived_geometry_ member, potentially re-built from the
  // exclusions_, and num_exclusions_ members.
  const DerivedGeometry& GetDerivedGeometry() const;

  // See DerivedGeometry struct description.
  mutable std::unique_ptr<DerivedGeometry> derived_geometry_;
};

// The exclusion space represents all of the exclusions within a block
// formatting context.
//
// The space is mutated simply by adding exclusions, and various information
// can be queried based on the exclusions.
class CORE_EXPORT NGExclusionSpace {
  DISALLOW_NEW();

 public:
  NGExclusionSpace() = default;
  NGExclusionSpace(const NGExclusionSpace& other)
      : exclusion_space_(other.exclusion_space_ ? new NGExclusionSpaceInternal(
                                                      *other.exclusion_space_)
                                                : nullptr) {}
  NGExclusionSpace(NGExclusionSpace&& other) noexcept = default;

  NGExclusionSpace& operator=(const NGExclusionSpace& other) {
    exclusion_space_ = other.exclusion_space_
                           ? std::make_unique<NGExclusionSpaceInternal>(
                                 *other.exclusion_space_)
                           : nullptr;
    return *this;
  }
  NGExclusionSpace& operator=(NGExclusionSpace&& other) = default;

  void Add(scoped_refptr<const NGExclusion> exclusion) {
    if (!exclusion_space_)
      exclusion_space_ = std::make_unique<NGExclusionSpaceInternal>();
    exclusion_space_->Add(std::move(exclusion));
  }

  // Returns a layout opportunity, within the BFC.
  // The area to search for layout opportunities is defined by the given offset,
  // and available_inline_size. The layout opportunity must be greater than the
  // given minimum_size.
  NGLayoutOpportunity FindLayoutOpportunity(
      const NGBfcOffset& offset,
      const LayoutUnit available_inline_size,
      const NGLogicalSize& minimum_size) const {
    if (!exclusion_space_) {
      NGBfcOffset end_offset(
          offset.line_offset + available_inline_size.ClampNegativeToZero(),
          LayoutUnit::Max());
      return NGLayoutOpportunity(NGBfcRect(offset, end_offset), nullptr);
    }
    return exclusion_space_->FindLayoutOpportunity(
        offset, available_inline_size, minimum_size);
  }

  LayoutOpportunityVector AllLayoutOpportunities(
      const NGBfcOffset& offset,
      const LayoutUnit available_inline_size) const {
    if (!exclusion_space_) {
      NGBfcOffset end_offset(
          offset.line_offset + available_inline_size.ClampNegativeToZero(),
          LayoutUnit::Max());
      return LayoutOpportunityVector(
          {NGLayoutOpportunity(NGBfcRect(offset, end_offset), nullptr)});
    }
    return exclusion_space_->AllLayoutOpportunities(offset,
                                                    available_inline_size);
  }

  // Returns the clearance offset based on the provided {@code clear_type}.
  LayoutUnit ClearanceOffset(EClear clear_type) const {
    if (!exclusion_space_)
      return LayoutUnit::Min();
    return exclusion_space_->ClearanceOffset(clear_type);
  }

  // Returns the block start offset of the last float added.
  LayoutUnit LastFloatBlockStart() const {
    if (!exclusion_space_)
      return LayoutUnit::Min();
    return exclusion_space_->LastFloatBlockStart();
  }

  bool IsEmpty() const {
    return !exclusion_space_ || exclusion_space_->IsEmpty();
  }

  bool operator==(const NGExclusionSpace& other) const {
    if (exclusion_space_ == other.exclusion_space_)
      return true;
    if (exclusion_space_ && other.exclusion_space_)
      return *exclusion_space_ == *other.exclusion_space_;
    return false;
  }
  bool operator!=(const NGExclusionSpace& other) const {
    return !(*this == other);
  }

 private:
  std::unique_ptr<NGExclusionSpaceInternal> exclusion_space_;
};

}  // namespace blink

#endif  // NGExclusionSpace_h
