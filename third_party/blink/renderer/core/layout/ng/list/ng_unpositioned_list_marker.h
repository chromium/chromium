// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_LIST_NG_UNPOSITIONED_LIST_MARKER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_LIST_NG_UNPOSITIONED_LIST_MARKER_H_

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/ng/geometry/ng_box_strut.h"
#include "third_party/blink/renderer/platform/fonts/font_baseline.h"
#include "third_party/blink/renderer/platform/wtf/allocator.h"

namespace blink {

class LayoutNGListMarker;
class LayoutUnit;
class NGBlockNode;
class NGConstraintSpace;
class NGBoxFragmentBuilder;
class NGLayoutResult;
class NGPhysicalFragment;

struct NGLogicalOffset;

// Represents an unpositioned list marker.
//
// A list item can have either block children or inline children. Because
// NGBLockLayoutAlgorithm handles the former while NGInlineLayoutAlgorithm
// handles the latter, list marker can appear in either algorithm.
//
// To handle these two cases consistently, when list markers appear in these
// algorithm, they are set as "unpositioned", and are propagated to ancestors
// through NGLayoutResult until they meet the corresponding list items.
class CORE_EXPORT NGUnpositionedListMarker final {
  DISALLOW_NEW();

 public:
  NGUnpositionedListMarker() : marker_layout_object_(nullptr) {}
  explicit NGUnpositionedListMarker(LayoutNGListMarker*);
  explicit NGUnpositionedListMarker(const NGBlockNode&);

  explicit operator bool() const { return marker_layout_object_; }

  // Add a fragment for an outside list marker.
  // Returns true if the list marker was successfully added. False indicates
  // that the child content does not have a baseline to align to, and that
  // caller should try next child, or "WithoutLineBoxes" version.
  bool AddToBox(const NGConstraintSpace&,
                FontBaseline,
                const NGPhysicalFragment& content,
                NGLogicalOffset* content_offset,
                NGBoxFragmentBuilder*,
                const NGBoxStrut&) const;

  // Add a fragment for an outside list marker when the list item has no line
  // boxes.
  // Returns the block size of the list marker.
  LayoutUnit AddToBoxWithoutLineBoxes(const NGConstraintSpace&,
                                      FontBaseline,
                                      NGBoxFragmentBuilder*) const;
  LayoutUnit InlineOffset(const LayoutUnit marker_inline_size) const;

 private:
  bool IsImage() const;

  scoped_refptr<NGLayoutResult> Layout(const NGConstraintSpace&,
                                       FontBaseline) const;
  LayoutUnit ComputeIntrudedFloatOffset(const NGConstraintSpace&,
                                        const NGBoxFragmentBuilder*,
                                        const NGBoxStrut&,
                                        LayoutUnit) const;

  LayoutNGListMarker* marker_layout_object_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_LIST_NG_UNPOSITIONED_LIST_MARKER_H_
