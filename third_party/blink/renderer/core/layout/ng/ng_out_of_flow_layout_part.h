// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_OUT_OF_FLOW_LAYOUT_PART_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_OUT_OF_FLOW_LAYOUT_PART_H_

#include "third_party/blink/renderer/core/core_export.h"

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/core/layout/geometry/logical_rect.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_offset.h"
#include "third_party/blink/renderer/core/layout/ng/geometry/ng_static_position.h"
#include "third_party/blink/renderer/core/layout/ng/inline/inline_containing_block_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_absolute_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_node.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space.h"
#include "third_party/blink/renderer/core/style/computed_style_base_constants.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"

namespace blink {

class ComputedStyle;
class LayoutBox;
class LayoutObject;
class NGBlockBreakToken;
class NGBoxFragmentBuilder;
class NGLayoutResult;
class NGSimplifiedOOFLayoutAlgorithm;
template <typename OffsetType>
struct NGContainingBlock;
struct NGLink;
struct NGLogicalOutOfFlowPositionedNode;
template <typename OffsetType>
struct NGMulticolWithPendingOOFs;

// Helper class for positioning of out-of-flow blocks.
// It should be used together with NGBoxFragmentBuilder.
// See NGBoxFragmentBuilder::AddOutOfFlowChildCandidate documentation
// for example of using these classes together.
class CORE_EXPORT NGOutOfFlowLayoutPart {
  STACK_ALLOCATED();

 public:
  NGOutOfFlowLayoutPart(const NGBlockNode& container_node,
                        const NGConstraintSpace& container_space,
                        NGBoxFragmentBuilder* container_builder);

  // The |container_builder|, |container_space|, and |container_style|
  // parameters are all with respect to the containing block of the relevant
  // out-of-flow positioned descendants. If the CSS "containing block" of such
  // an out-of-flow positioned descendant isn't a true block (e.g. a relatively
  // positioned inline instead), the containing block here is the containing
  // block of said non-block.
  NGOutOfFlowLayoutPart(
      bool is_absolute_container,
      bool is_fixed_container,
      bool is_grid_container,
      const ComputedStyle& container_style,
      const NGConstraintSpace& container_space,
      NGBoxFragmentBuilder* container_builder,
      absl::optional<LogicalSize> initial_containing_block_fixed_size =
          absl::nullopt);

  // Normally this function lays out and positions all out-of-flow objects from
  // the container_builder and additional ones it discovers through laying out
  // those objects. However, if only_layout is specified, only that object will
  // get laid out; any additional ones will be stored as out-of-flow
  // descendants in the builder for use via
  // LayoutResult::OutOfFlowPositionedDescendants.
  void Run(const LayoutBox* only_layout = nullptr);

  // Handle the layout of any OOF elements in a fragmentation context.
  void HandleFragmentation();

  // Information needed to position descendant within a containing block.
  // Geometry expressed here is complicated:
  // There are two types of containing blocks:
  // 1) Default containing block (DCB)
  //    Containing block passed in NGOutOfFlowLayoutPart constructor.
  //    It is the block element inside which this algorighm runs.
  //    All OOF descendants not in inline containing block are placed in DCB.
  // 2) Inline containing block
  //    OOF descendants might be positioned wrt inline containing block.
  //    Inline containing block is positioned wrt default containing block.
  struct ContainingBlockInfo {
    DISALLOW_NEW();

   public:
    // The writing direction of the container.
    WritingDirectionMode writing_direction = {WritingMode::kHorizontalTb,
                                              TextDirection::kLtr};
    // Size and offset of the container.
    LogicalRect rect;
    // The relative positioned offset to be applied after fragmentation is
    // completed.
    LogicalOffset relative_offset;
    // The offset of the container to its border box, including the block
    // contribution from previous fragmentainers.
    LogicalOffset offset_to_border_box;
  };

  // This stores the information needed to update a multicol child inside an
  // existing multicol fragment. This is used during nested fragmentation of an
  // OOF positioned element.
  struct MulticolChildInfo {
    DISALLOW_NEW();

   public:
    // The mutable link of a multicol child.
    NGLink* mutable_link;

    // The multicol break token that stores a reference to |mutable_link|'s
    // break token in its list of child break tokens.
    Member<const NGBlockBreakToken> parent_break_token;

    explicit MulticolChildInfo(NGLink* mutable_link,
                               NGBlockBreakToken* parent_break_token = nullptr)
        : mutable_link(mutable_link), parent_break_token(parent_break_token) {}

    void Trace(Visitor* visitor) const;
  };

  // Info needed to perform Layout() on an OOF positioned node.
  struct NodeInfo {
    DISALLOW_NEW();

   public:
    NGBlockNode node;
    const NGConstraintSpace constraint_space;
    const NGLogicalStaticPosition static_position;
    PhysicalSize container_physical_content_size;
    const ContainingBlockInfo container_info;
    const WritingDirectionMode default_writing_direction;
    const NGContainingBlock<LogicalOffset>& fixedpos_containing_block;
    bool inline_container = false;

    NodeInfo(NGBlockNode node,
             const NGConstraintSpace constraint_space,
             const NGLogicalStaticPosition static_position,
             PhysicalSize container_physical_content_size,
             const ContainingBlockInfo container_info,
             const WritingDirectionMode default_writing_direction,
             bool is_fragmentainer_descendant,
             const NGContainingBlock<LogicalOffset>& fixedpos_containing_block,
             bool inline_container)
        : node(node),
          constraint_space(constraint_space),
          static_position(static_position),
          container_physical_content_size(container_physical_content_size),
          container_info(container_info),
          default_writing_direction(default_writing_direction),
          fixedpos_containing_block(fixedpos_containing_block),
          inline_container(inline_container) {}
  };

  // Stores the calculated offset for an OOF positioned node, along with the
  // information that was used in calculating the offset that will be used, in
  // addition to the information in NodeInfo, to perform a final layout
  // pass.
  struct OffsetInfo {
    DISALLOW_NEW();

   public:
    LogicalOffset offset;
    // If |has_cached_layout_result| is true, this will hold the cached layout
    // result that should be returned. Otherwise, this will hold the initial
    // layout result if we needed to know the size in order to calculate the
    // offset. If an initial result is set, it will either be re-used or
    // replaced in the final layout pass.
    scoped_refptr<const NGLayoutResult> initial_layout_result;
    // The |block_estimate| is wrt. the candidate's writing mode.
    absl::optional<LayoutUnit> block_estimate;
    NGLogicalOutOfFlowDimensions node_dimensions;

    bool inline_size_depends_on_min_max_sizes = false;
    bool block_size_depends_on_layout = false;

    // If true, a cached layout result was found. See the comment for
    // |initial_layout_result| for more details.
    bool has_cached_layout_result = false;

    // The offset from the OOF to the top of the fragmentation context root.
    // This should only be used when laying out a fragmentainer descendant.
    LogicalOffset original_offset;
  };

  struct NodeToLayout {
    DISALLOW_NEW();

   public:
    NodeInfo node_info;
    OffsetInfo offset_info;
    Member<const NGBlockBreakToken> break_token;

    // The physical fragment of the containing block used when laying out a
    // fragmentainer descendant. This is the containing block as defined by the
    // spec: https://www.w3.org/TR/css-position-3/#absolute-cb.
    scoped_refptr<const NGPhysicalFragment> containing_block_fragment = nullptr;

    void Trace(Visitor* visitor) const;
  };

 private:
  bool SweepLegacyCandidates(
      HeapHashSet<Member<const LayoutObject>>* placed_objects);

  const ContainingBlockInfo GetContainingBlockInfo(
      const NGLogicalOutOfFlowPositionedNode&);

  void ComputeInlineContainingBlocks(
      const Vector<NGLogicalOutOfFlowPositionedNode>&);
  void ComputeInlineContainingBlocksForFragmentainer(
      const Vector<NGLogicalOutOfFlowPositionedNode>&);
  // |containing_block_relative_offset| is the accumulated relative offset from
  // the inline's containing block to the fragmentation context root.
  // |containing_block_offset| is the offset of the inline's containing block
  // relative to the fragmentation context root (not including any offset from
  // relative positioning).
  void AddInlineContainingBlockInfo(
      const InlineContainingBlockUtils::InlineContainingBlockMap&,
      const WritingDirectionMode container_writing_direction,
      PhysicalSize container_builder_size,
      LogicalOffset containing_block_relative_offset = LogicalOffset(),
      LogicalOffset containing_block_offset = LogicalOffset(),
      bool adjust_for_fragmentation = false);

  void LayoutCandidates(
      Vector<NGLogicalOutOfFlowPositionedNode>* candidates,
      const LayoutBox* only_layout,
      HeapHashSet<Member<const LayoutObject>>* placed_objects);

  void HandleMulticolsWithPendingOOFs(NGBoxFragmentBuilder* container_builder);
  void LayoutOOFsInMulticol(
      const NGBlockNode& multicol,
      const NGMulticolWithPendingOOFs<LogicalOffset>* multicol_info);

  // Layout the OOF nodes that are descendants of a fragmentation context root.
  // |multicol_children| holds the children of an inner multicol if
  // we are laying out OOF elements inside a nested fragmentation context.
  void LayoutFragmentainerDescendants(
      Vector<NGLogicalOutOfFlowPositionedNode>* descendants,
      LayoutUnit column_inline_progression,
      bool outer_context_has_fixedpos_container = false,
      HeapVector<MulticolChildInfo>* multicol_children = nullptr);

  NodeInfo SetupNodeInfo(const NGLogicalOutOfFlowPositionedNode& oof_node);

  scoped_refptr<const NGLayoutResult> LayoutOOFNode(
      NodeToLayout& oof_node_to_layout,
      const LayoutBox* only_layout,
      const NGConstraintSpace* fragmentainer_constraint_space = nullptr);

  // TODO(almaher): We are calculating more than just the offset. Consider
  // changing this to a more accurate name.
  OffsetInfo CalculateOffset(const NodeInfo& node_info,
                             const LayoutBox* only_layout,
                             bool is_first_run = true);

  scoped_refptr<const NGLayoutResult> Layout(
      const NodeToLayout& oof_node_to_layout,
      const NGConstraintSpace* fragmentainer_constraint_space);

  bool IsContainingBlockForCandidate(const NGLogicalOutOfFlowPositionedNode&);

  scoped_refptr<const NGLayoutResult> GenerateFragment(
      NGBlockNode node,
      const LogicalSize& container_content_size_in_child_writing_mode,
      const absl::optional<LayoutUnit>& block_estimate,
      const NGLogicalOutOfFlowDimensions& node_dimensions,
      const LayoutUnit block_offset,
      const NGBlockBreakToken* break_token,
      const NGConstraintSpace* fragmentainer_constraint_space,
      bool should_use_fixed_block_size);

  // Performs layout on the OOFs stored in |pending_descendants| and
  // |fragmented_descendants|, adding them as children in the fragmentainer
  // found at the provided |index|. If a fragmentainer does not already exist at
  // the given |index|, one will be created (unless we are in a nested
  // fragmentation context). The OOFs stored in |fragmented_descendants| are
  // those that are continuing layout from a previous fragmentainer.
  // |fragmented_descendants| is also an output variable in that any OOF that
  // has not finished layout in the current pass will be added back to
  // |fragmented_descendants| to continue layout in the next fragmentainer.
  void LayoutOOFsInFragmentainer(
      HeapVector<NodeToLayout>& pending_descendants,
      wtf_size_t index,
      LayoutUnit column_inline_progression,
      HeapVector<NodeToLayout>* fragmented_descendants);
  void AddOOFToFragmentainer(NodeToLayout& descendant,
                             const NGConstraintSpace* fragmentainer_space,
                             LogicalOffset fragmentainer_offset,
                             wtf_size_t index,
                             NGSimplifiedOOFLayoutAlgorithm* algorithm,
                             HeapVector<NodeToLayout>* fragmented_descendants);
  void ReplaceFragmentainer(wtf_size_t index,
                            LogicalOffset offset,
                            bool create_new_fragment,
                            NGSimplifiedOOFLayoutAlgorithm* algorithm);
  LogicalOffset UpdatedFragmentainerOffset(LogicalOffset offset,
                                           wtf_size_t index,
                                           LayoutUnit column_inline_progression,
                                           bool create_new_fragment);
  NGConstraintSpace GetFragmentainerConstraintSpace(wtf_size_t index);
  void ComputeStartFragmentIndexAndRelativeOffset(
      WritingMode default_writing_mode,
      LayoutUnit block_estimate,
      wtf_size_t* start_index,
      LogicalOffset* offset) const;

  void ReplaceFragment(scoped_refptr<const NGLayoutResult> new_result,
                       const NGPhysicalBoxFragment& old_fragment,
                       wtf_size_t index);

  // This saves the static-position for an OOF-positioned object into its
  // paint-layer.
  void SaveStaticPositionOnPaintLayer(
      LayoutBox* layout_box,
      const NGLogicalStaticPosition& position) const;
  NGLogicalStaticPosition ToStaticPositionForLegacy(
      NGLogicalStaticPosition position) const;

  NGBoxFragmentBuilder* container_builder_;
  ContainingBlockInfo default_containing_block_info_for_absolute_;
  ContainingBlockInfo default_containing_block_info_for_fixed_;
  HeapHashMap<Member<const LayoutObject>, ContainingBlockInfo>
      containing_blocks_map_;
  const WritingMode writing_mode_;
  const WritingDirectionMode default_writing_direction_;

  // Out-of-flow positioned nodes that we should lay out at a later time. For
  // example, if the containing block has not finished layout.
  Vector<NGLogicalOutOfFlowPositionedNode> delayed_descendants_;

  // Holds the children of an inner multicol if we are laying out OOF elements
  // inside a nested fragmentation context.
  HeapVector<MulticolChildInfo>* multicol_children_;
  // The block size of the multi-column (before adjustment for spanners, etc.)
  // This is used to calculate the column size of any newly added proxy
  // fragments when handling fragmentation for abspos elements.
  LayoutUnit original_column_block_size_ = kIndefiniteSize;
  // The consumed block size of previous fragmentainers. This is accumulated and
  // used as we add OOF elements to fragmentainers.
  LayoutUnit fragmentainer_consumed_block_size_;
  bool is_absolute_container_ = false;
  bool is_fixed_container_ = false;
  bool allow_first_tier_oof_cache_ = false;
  bool has_block_fragmentation_ = false;
  // A fixedpos containing block was found in an outer fragmentation context.
  bool outer_context_has_fixedpos_container_ = false;

  // Set to true if there's a legacy flexbox inside a (non-containing) legacy
  // object (so that it's found in LayoutBlock::PositionedObjects()). E.g.:
  //
  // <div style="position:relative;">
  //   <div id="legacy" style="columns:2;">
  //     <div style="display:flex; position:absolute;">
  bool has_legacy_flex_box_ = false;

  bool performing_extra_legacy_check_ = false;
};

}  // namespace blink

WTF_ALLOW_CLEAR_UNUSED_SLOTS_WITH_MEM_FUNCTIONS(
    blink::NGOutOfFlowLayoutPart::MulticolChildInfo)
WTF_ALLOW_CLEAR_UNUSED_SLOTS_WITH_MEM_FUNCTIONS(
    blink::NGOutOfFlowLayoutPart::NodeToLayout)

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_OUT_OF_FLOW_LAYOUT_PART_H_
