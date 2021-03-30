// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_LIST_NG_UNPOSITIONED_LIST_MARKER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_LIST_NG_UNPOSITIONED_LIST_MARKER_H_

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/ng/geometry/ng_box_strut.h"
#include "third_party/blink/renderer/platform/fonts/font_baseline.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class ComputedStyle;
class LayoutNGOutsideListMarker;
class LayoutUnit;
class NGBlockNode;
class NGConstraintSpace;
class NGBoxFragmentBuilder;
class NGLayoutResult;
class NGPhysicalFragment;

struct LogicalOffset;

// Represents an unpositioned list marker.
//
// A list item can have either block children or inline children. Because
// NGBLockLayoutAlgorithm handles the former while NGInlineLayoutAlgorithm
// handles the latter, list marker can appear in either algorithm.
//
// To handle these two cases consistently, when list markers appear in these
// algorithm, they are set as "unpositioned", and are propagated to ancestors
// through NGLayoutResult until they meet the corresponding list items.
//
// In order to adjust with the other content of LI, marker will be handled
// after other children.
// First, try to find the alignment-baseline for the marker. See
// |ContentAlignmentBaseline()| for details.
// If found, layout marker, compute the content adjusted offset and float
// intuded offset. See |AddToBox()| for details.
// If not, layout marker and deal with it in |AddToBoxWithoutLineBoxes()|.
//
// In addition, marker makes LI non self-collapsing. If the BFC block-offset of
// LI isn't resolved after layout marker, we'll resolve it. See
// |NGBlockLayoutAlgorithm::PositionOrPropagateListMarker()| and
// |NGBlockLayoutAlgorithm::PositionListMarkerWithoutLineBoxes()| for details.
class CORE_EXPORT NGUnpositionedListMarker final {
  DISALLOW_NEW();

 public:
  NGUnpositionedListMarker() : marker_layout_object_(nullptr) {}
  explicit NGUnpositionedListMarker(LayoutNGOutsideListMarker*);
  explicit NGUnpositionedListMarker(const NGBlockNode&);

  explicit operator bool() const { return marker_layout_object_; }

  // Returns the baseline that the list-marker should place itself along.
  //
  // |base::nullopt| indicates that the child |content| does not have a baseline
  // to align to, and that caller should try next child, or use the
  // |AddToBoxWithoutLineBoxes()| method.
  base::Optional<LayoutUnit> ContentAlignmentBaseline(
      const NGConstraintSpace&,
      FontBaseline,
      const NGPhysicalFragment& content) const;
  // Add a fragment for an outside list marker.
  void AddToBox(const NGConstraintSpace&,
                FontBaseline,
                const NGPhysicalFragment& content,
                const NGBoxStrut&,
                const NGLayoutResult& marker_layout_result,
                LayoutUnit content_baseline,
                LogicalOffset* content_offset,
                NGBoxFragmentBuilder*) const;

  // Add a fragment for an outside list marker when the list item has no line
  // boxes.
  // Returns the block size of the list marker.
  LayoutUnit AddToBoxWithoutLineBoxes(
      const NGConstraintSpace&,
      FontBaseline,
      const NGLayoutResult& marker_layout_result,
      NGBoxFragmentBuilder*) const;
  LayoutUnit InlineOffset(const LayoutUnit marker_inline_size) const;

  bool operator==(const NGUnpositionedListMarker& other) const {
    return marker_layout_object_ == other.marker_layout_object_;
  }

  scoped_refptr<const NGLayoutResult> Layout(
      const NGConstraintSpace& parent_space,
      const ComputedStyle& parent_style,
      FontBaseline) const;

#if DCHECK_IS_ON()
  void CheckMargin() const;
#endif

 private:
  LayoutUnit ComputeIntrudedFloatOffset(const NGConstraintSpace&,
                                        const NGBoxFragmentBuilder*,
                                        const NGBoxStrut&,
                                        LayoutUnit) const;

  LayoutNGOutsideListMarker* marker_layout_object_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_LIST_NG_UNPOSITIONED_LIST_MARKER_H_
