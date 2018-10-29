// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_BOX_FRAGMENT_BUILDER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_BOX_FRAGMENT_BUILDER_H_

#include "third_party/blink/renderer/core/layout/ng/geometry/ng_border_edges.h"
#include "third_party/blink/renderer/core/layout/ng/geometry/ng_box_strut.h"
#include "third_party/blink/renderer/core/layout/ng/geometry/ng_physical_offset_rect.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_baseline.h"
#include "third_party/blink/renderer/core/layout/ng/ng_break_token.h"
#include "third_party/blink/renderer/core/layout/ng/ng_container_fragment_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_result.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/platform/wtf/allocator.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"

namespace blink {

class NGPhysicalFragment;
class NGPhysicalLineBoxFragment;

class CORE_EXPORT NGBoxFragmentBuilder final
    : public NGContainerFragmentBuilder {
  DISALLOW_NEW();

 public:
  NGBoxFragmentBuilder(NGLayoutInputNode node,
                       scoped_refptr<const ComputedStyle> style,
                       WritingMode writing_mode,
                       TextDirection direction)
      : NGContainerFragmentBuilder(std::move(style), writing_mode, direction),
        node_(node),
        box_type_(NGPhysicalFragment::NGBoxType::kNormalBox),
        is_old_layout_root_(false),
        did_break_(false) {
    layout_object_ = node.GetLayoutBox();
  }

  // Build a fragment for LayoutObject without NGLayoutInputNode. LayoutInline
  // has NGInlineItem but does not have corresponding NGLayoutInputNode.
  NGBoxFragmentBuilder(LayoutObject* layout_object,
                       scoped_refptr<const ComputedStyle> style,
                       WritingMode writing_mode,
                       TextDirection direction)
      : NGContainerFragmentBuilder(std::move(style), writing_mode, direction),
        node_(nullptr),
        box_type_(NGPhysicalFragment::NGBoxType::kNormalBox),
        is_old_layout_root_(false),
        did_break_(false) {
    layout_object_ = layout_object;
  }

  NGBoxFragmentBuilder& SetIntrinsicBlockSize(LayoutUnit intrinsic_block_size) {
    intrinsic_block_size_ = intrinsic_block_size;
    return *this;
  }
  NGBoxFragmentBuilder& SetBorders(const NGBoxStrut& border) {
    DCHECK_NE(BoxType(), NGPhysicalFragment::kInlineBox);
    borders_ = border;
    return *this;
  }
  NGBoxFragmentBuilder& SetPadding(const NGBoxStrut& padding) {
    DCHECK_NE(BoxType(), NGPhysicalFragment::kInlineBox);
    padding_ = padding;
    return *this;
  }
  NGBoxFragmentBuilder& SetPadding(const NGLineBoxStrut& padding) {
    DCHECK_EQ(BoxType(), NGPhysicalFragment::kInlineBox);
    // Convert to flow-relative, because ToInlineBoxFragment() will convert
    // the padding to physical coordinates using flow-relative writing-mode.
    padding_ = NGBoxStrut(padding, IsFlippedLinesWritingMode(GetWritingMode()));
    return *this;
  }

  using NGContainerFragmentBuilder::AddChild;

  // Our version of AddChild captures any child NGBreakTokens.
  NGContainerFragmentBuilder& AddChild(scoped_refptr<const NGPhysicalFragment>,
                                       const NGLogicalOffset&) final;

  // Remove all children.
  void RemoveChildren();

  // Add a break token for a child that doesn't yet have any fragments, because
  // its first fragment is to be produced in the next fragmentainer. This will
  // add a break token for the child, but no fragment.
  NGBoxFragmentBuilder& AddBreakBeforeChild(NGLayoutInputNode child);

  // Prepare for a break token before the specified line.
  NGBoxFragmentBuilder& AddBreakBeforeLine(int line_number);

  // Update if we have fragmented in this flow.
  NGBoxFragmentBuilder& PropagateBreak(const NGLayoutResult&);
  NGBoxFragmentBuilder& PropagateBreak(const NGPhysicalFragment&);

  void AddOutOfFlowLegacyCandidate(NGBlockNode,
                                   const NGStaticPosition&,
                                   LayoutObject* inline_container);

  // Set how much of the block size we've used so far for this box.
  NGBoxFragmentBuilder& SetUsedBlockSize(LayoutUnit used_block_size) {
    used_block_size_ = used_block_size;
    return *this;
  }

  NGBoxFragmentBuilder& SetNeedsFinishedBreakToken() {
    needs_finished_break_token_ = true;
    return *this;
  }

  // Specify that we broke.
  //
  // This will result in a fragment which has an unfinished break token.
  NGBoxFragmentBuilder& SetDidBreak() {
    did_break_ = true;
    return *this;
  }

  NGBoxFragmentBuilder& SetHasForcedBreak() {
    has_forced_break_ = true;
    minimal_space_shortage_ = LayoutUnit();
    return *this;
  }

  // Report space shortage, i.e. how much more space would have been sufficient
  // to prevent some piece of content from breaking. This information may be
  // used by the column balancer to stretch columns.
  NGBoxFragmentBuilder& PropagateSpaceShortage(LayoutUnit space_shortage) {
    DCHECK_GT(space_shortage, LayoutUnit());
    if (minimal_space_shortage_ > space_shortage)
      minimal_space_shortage_ = space_shortage;
    return *this;
  }

  void SetInitialBreakBefore(EBreakBetween break_before) {
    initial_break_before_ = break_before;
  }

  void SetPreviousBreakAfter(EBreakBetween break_after) {
    previous_break_after_ = break_after;
  }

  // Join/"collapse" the previous (stored) break-after value with the next
  // break-before value, to determine how to deal with breaking between two
  // in-flow siblings.
  EBreakBetween JoinedBreakBetweenValue(EBreakBetween break_before) const;

  // Return the number of line boxes laid out.
  int LineCount() const { return inline_break_tokens_.size(); }

  // Call when we're setting an undersirable break. It may be possible to avoid
  // the break if we instead break at an earlier element.
  void SetHasLastResortBreak() { has_last_resort_break_ = true; }

  // Offsets are not supposed to be set during fragment construction, so we
  // do not provide a setter here.

  // Creates the fragment. Can only be called once.
  scoped_refptr<NGLayoutResult> ToBoxFragment() {
    DCHECK_NE(BoxType(), NGPhysicalFragment::kInlineBox);
    return ToBoxFragment(GetWritingMode());
  }
  scoped_refptr<NGLayoutResult> ToInlineBoxFragment() {
    // The logical coordinate for inline box uses line-relative writing-mode,
    // not
    // flow-relative.
    DCHECK_EQ(BoxType(), NGPhysicalFragment::kInlineBox);
    return ToBoxFragment(ToLineWritingMode(GetWritingMode()));
  }

  scoped_refptr<NGLayoutResult> Abort(NGLayoutResult::NGLayoutResultStatus);

  // A vector of child offsets. Initially set by AddChild().
  const OffsetVector& Offsets() const { return offsets_; }

  NGPhysicalFragment::NGBoxType BoxType() const;
  NGBoxFragmentBuilder& SetBoxType(NGPhysicalFragment::NGBoxType box_type) {
    box_type_ = box_type;
    return *this;
  }
  NGBoxFragmentBuilder& SetIsFieldsetContainer() {
    is_fieldset_container_ = true;
    return *this;
  }
  NGBoxFragmentBuilder& SetIsOldLayoutRoot() {
    is_old_layout_root_ = true;
    return *this;
  }

  bool DidBreak() const { return did_break_; }

  NGBoxFragmentBuilder& SetBorderEdges(NGBorderEdges border_edges) {
    border_edges_ = border_edges;
    return *this;
  }

  // Layout algorithms should call this function for each baseline request in
  // the constraint space.
  //
  // If a request should use a synthesized baseline from the box rectangle,
  // algorithms can omit the call.
  //
  // This function should be called at most once for a given algorithm/baseline
  // type pair.
  void AddBaseline(NGBaselineRequest, LayoutUnit);

  // Inline containing block geometry is defined by two fragments:
  // start and end. FragmentPair holds the information needed to compute
  // inline containing block geometry wrt enclosing container block.
  struct FragmentPair {
    DISALLOW_NEW();
    // Linebox that contains start_fragment.
    const NGPhysicalLineBoxFragment* start_linebox_fragment;
    // Offset of start_linebox from containing block.
    NGLogicalOffset start_linebox_offset;
    // Start fragment of inline containing block.
    const NGPhysicalFragment* start_fragment;
    // Start fragment rect combined with rectangles of all fragments
    // generated by same Element as start_fragment.
    NGPhysicalOffsetRect start_fragment_union_rect;
    // end_** variables are end fragment counterparts to start fragment.
    const NGPhysicalLineBoxFragment* end_linebox_fragment;
    NGLogicalOffset end_linebox_offset;
    const NGPhysicalFragment* end_fragment;
    NGPhysicalOffsetRect end_fragment_union_rect;
  };

  void ComputeInlineContainerFragments(
      HashMap<const LayoutObject*, FragmentPair>* inline_container_fragments,
      NGLogicalSize* container_size);

 private:
  scoped_refptr<NGLayoutResult> ToBoxFragment(WritingMode);

  NGLayoutInputNode node_;

  LayoutUnit intrinsic_block_size_;
  NGBoxStrut borders_;
  NGBoxStrut padding_;

  NGPhysicalFragment::NGBoxType box_type_;
  bool is_fieldset_container_ = false;
  bool is_old_layout_root_;
  bool needs_finished_break_token_ = false;
  bool did_break_;
  bool has_forced_break_ = false;
  LayoutUnit used_block_size_;

  LayoutUnit minimal_space_shortage_ = LayoutUnit::Max();

  // The break-before value on the initial child we cannot honor. There's no
  // valid class A break point before a first child, only *between* siblings.
  EBreakBetween initial_break_before_ = EBreakBetween::kAuto;

  // The break-after value of the previous in-flow sibling.
  EBreakBetween previous_break_after_ = EBreakBetween::kAuto;

  NGBreakTokenVector child_break_tokens_;
  NGBreakTokenVector inline_break_tokens_;

  Vector<NGBaseline> baselines_;

  NGBorderEdges border_edges_;

  friend class NGPhysicalBoxFragment;
  friend class NGLayoutResult;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_BOX_FRAGMENT_BUILDER_H_
