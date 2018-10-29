// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGContainerFragmentBuilder_h
#define NGContainerFragmentBuilder_h

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/ng/exclusions/ng_exclusion_space.h"
#include "third_party/blink/renderer/core/layout/ng/geometry/ng_bfc_offset.h"
#include "third_party/blink/renderer/core/layout/ng/geometry/ng_logical_size.h"
#include "third_party/blink/renderer/core/layout/ng/geometry/ng_margin_strut.h"
#include "third_party/blink/renderer/core/layout/ng/list/ng_unpositioned_list_marker.h"
#include "third_party/blink/renderer/core/layout/ng/ng_floats_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_fragment_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_link.h"
#include "third_party/blink/renderer/core/layout/ng/ng_out_of_flow_positioned_descendant.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/text/text_direction.h"
#include "third_party/blink/renderer/platform/text/writing_mode.h"
#include "third_party/blink/renderer/platform/wtf/allocator.h"

namespace blink {

class NGExclusionSpace;
class NGLayoutResult;
class NGPhysicalFragment;

class CORE_EXPORT NGContainerFragmentBuilder : public NGFragmentBuilder {
  STACK_ALLOCATED();

 public:
  typedef Vector<scoped_refptr<const NGPhysicalFragment>, 16> ChildrenVector;
  typedef Vector<NGLogicalOffset, 16> OffsetVector;

  LayoutUnit BfcLineOffset() const { return bfc_line_offset_; }
  NGContainerFragmentBuilder& SetBfcLineOffset(LayoutUnit bfc_line_offset) {
    bfc_line_offset_ = bfc_line_offset;
    return *this;
  }

  // The NGBfcOffset is where this fragment was positioned within the BFC. If
  // it is not set, this fragment may be placed anywhere within the BFC.
  const base::Optional<LayoutUnit>& BfcBlockOffset() const {
    return bfc_block_offset_;
  }
  NGContainerFragmentBuilder& SetBfcBlockOffset(LayoutUnit bfc_block_offset) {
    bfc_block_offset_ = bfc_block_offset;
    return *this;
  }
  NGContainerFragmentBuilder& ResetBfcBlockOffset() {
    bfc_block_offset_.reset();
    return *this;
  }

  NGContainerFragmentBuilder& SetEndMarginStrut(
      const NGMarginStrut& end_margin_strut) {
    end_margin_strut_ = end_margin_strut;
    return *this;
  }

  NGContainerFragmentBuilder& SetExclusionSpace(
      NGExclusionSpace&& exclusion_space) {
    exclusion_space_ = std::move(exclusion_space);
    return *this;
  }

  const NGUnpositionedListMarker& UnpositionedListMarker() const {
    return unpositioned_list_marker_;
  }
  NGContainerFragmentBuilder& SetUnpositionedListMarker(
      const NGUnpositionedListMarker& marker) {
    DCHECK(!unpositioned_list_marker_ || !marker);
    unpositioned_list_marker_ = marker;
    return *this;
  }

  NGContainerFragmentBuilder& AddChild(const NGLayoutResult&,
                                       const NGLogicalOffset&);

  // This version of AddChild will not propagate floats/out_of_flow.
  // Use the AddChild(NGLayoutResult) variant if NGLayoutResult is available.
  virtual NGContainerFragmentBuilder& AddChild(
      scoped_refptr<const NGPhysicalFragment>,
      const NGLogicalOffset&);

  const ChildrenVector& Children() const { return children_; }

  // Builder has non-trivial out-of-flow descendant methods.
  // These methods are building blocks for implementation of
  // out-of-flow descendants by layout algorithms.
  //
  // They are intended to be used by layout algorithm like this:
  //
  // Part 1: layout algorithm positions in-flow children.
  //   out-of-flow children, and out-of-flow descendants of fragments
  //   are stored inside builder.
  //
  // for (child : children)
  //   if (child->position == (Absolute or Fixed))
  //     builder->AddOutOfFlowChildCandidate(child);
  //   else
  //     fragment = child->Layout()
  //     builder->AddChild(fragment)
  // end
  //
  // builder->SetSize
  //
  // Part 2: Out-of-flow layout part positions out-of-flow descendants.
  //
  // NGOutOfFlowLayoutPart(container_style, builder).Run();
  //
  // See layout part for builder interaction.
  //
  // @param direction: default candidate direction is builder's direction.
  // Pass in direction if candidates direction does not match.
  NGContainerFragmentBuilder& AddOutOfFlowChildCandidate(
      NGBlockNode,
      const NGLogicalOffset& child_offset);

  // Inline candidates are laid out line-relative, not fragment-relative.
  NGContainerFragmentBuilder& AddInlineOutOfFlowChildCandidate(
      NGBlockNode,
      const NGLogicalOffset& child_line_offset,
      TextDirection line_direction,
      LayoutObject* inline_container);

  NGContainerFragmentBuilder& AddOutOfFlowDescendant(
      NGOutOfFlowPositionedDescendant descendant);

  void GetAndClearOutOfFlowDescendantCandidates(
      Vector<NGOutOfFlowPositionedDescendant>* descendant_candidates,
      const LayoutObject* container);

  // Utility routine to move all OOF descendant candidates to descendants.
  // Use if fragment cannot position any OOF children.
  void MoveOutOfFlowDescendantCandidatesToDescendants(
      const LayoutObject* inline_container);

  NGContainerFragmentBuilder& SetIsPushedByFloats() {
    is_pushed_by_floats_ = true;
    return *this;
  }
  bool IsPushedByFloats() const { return is_pushed_by_floats_; }

  NGContainerFragmentBuilder& ResetAdjoiningFloatTypes() {
    adjoining_floats_ = kFloatTypeNone;
    return *this;
  }
  NGContainerFragmentBuilder& AddAdjoiningFloatTypes(NGFloatTypes floats) {
    adjoining_floats_ |= floats;
    return *this;
  }
  NGFloatTypes AdjoiningFloatTypes() const { return adjoining_floats_; }

#ifndef NDEBUG
  String ToString() const;
#endif

 protected:
  // An out-of-flow positioned-candidate is a temporary data structure used
  // within the NGBoxFragmentBuilder.
  //
  // A positioned-candidate can be:
  // 1. A direct out-of-flow positioned child. The child_offset is (0,0).
  // 2. A fragment containing an out-of-flow positioned-descendant. The
  //    child_offset in this case is the containing fragment's offset.
  //
  // The child_offset is stored as a NGLogicalOffset as the physical offset
  // cannot be computed until we know the current fragment's size.
  //
  // When returning the positioned-candidates (from
  // GetAndClearOutOfFlowDescendantCandidates), the NGBoxFragmentBuilder will
  // convert the positioned-candidate to a positioned-descendant using the
  // physical size the fragment builder.
  struct NGOutOfFlowPositionedCandidate {
    NGOutOfFlowPositionedDescendant descendant;
    NGLogicalOffset child_offset;  // Logical offset of child's top left vertex.
    bool is_line_relative;  // True if offset is relative to line, not fragment.
    TextDirection line_direction;

    NGOutOfFlowPositionedCandidate(
        NGOutOfFlowPositionedDescendant descendant_arg,
        NGLogicalOffset child_offset_arg)
        : descendant(descendant_arg),
          child_offset(child_offset_arg),
          is_line_relative(false) {}

    NGOutOfFlowPositionedCandidate(
        NGOutOfFlowPositionedDescendant descendant_arg,
        NGLogicalOffset child_offset_arg,
        TextDirection line_direction_arg)
        : descendant(descendant_arg),
          child_offset(child_offset_arg),
          is_line_relative(true),
          line_direction(line_direction_arg) {}
  };

  NGContainerFragmentBuilder(scoped_refptr<const ComputedStyle> style,
                             WritingMode writing_mode,
                             TextDirection direction)
      : NGFragmentBuilder(std::move(style), writing_mode, direction) {}

  LayoutUnit bfc_line_offset_;
  base::Optional<LayoutUnit> bfc_block_offset_;
  NGMarginStrut end_margin_strut_;
  NGExclusionSpace exclusion_space_;

  Vector<NGOutOfFlowPositionedCandidate> oof_positioned_candidates_;
  Vector<NGOutOfFlowPositionedDescendant> oof_positioned_descendants_;

  NGUnpositionedListMarker unpositioned_list_marker_;

  ChildrenVector children_;

  // Logical offsets for the children. Stored as logical offsets as we can't
  // convert to physical offsets until layout of all children has been
  // determined.
  OffsetVector offsets_;

  NGFloatTypes adjoining_floats_ = kFloatTypeNone;

  bool has_last_resort_break_ = false;

  bool is_pushed_by_floats_ = false;

  friend class NGPhysicalContainerFragment;
};

}  // namespace blink

#endif  // NGContainerFragmentBuilder
