// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_box_fragment_builder.h"

#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/ng/exclusions/ng_exclusion_space.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_break_token.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_fragment_traversal.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_node.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_physical_line_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_break_token.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_node.h"
#include "third_party/blink/renderer/core/layout/ng/ng_break_token.h"
#include "third_party/blink/renderer/core/layout/ng/ng_fragmentation_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_result.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_positioned_float.h"

namespace blink {

namespace {

NGPhysicalFragment::NGBoxType BoxTypeFromLayoutObject(
    const LayoutObject* layout_object) {
  DCHECK(layout_object);
  if (layout_object->IsFloating())
    return NGPhysicalFragment::NGBoxType::kFloating;
  if (layout_object->IsOutOfFlowPositioned())
    return NGPhysicalFragment::NGBoxType::kOutOfFlowPositioned;
  if (layout_object->IsAtomicInlineLevel())
    return NGPhysicalFragment::NGBoxType::kAtomicInline;
  if (layout_object->IsInline())
    return NGPhysicalFragment::NGBoxType::kInlineBox;
  if (layout_object->IsLayoutBlock() &&
      ToLayoutBlock(layout_object)->CreatesNewFormattingContext())
    return NGPhysicalFragment::NGBoxType::kBlockFlowRoot;
  return NGPhysicalFragment::NGBoxType::kNormalBox;
}

}  // namespace

NGContainerFragmentBuilder& NGBoxFragmentBuilder::AddChild(
    scoped_refptr<const NGPhysicalFragment> child,
    const NGLogicalOffset& child_offset) {
  switch (child->Type()) {
    case NGPhysicalBoxFragment::kFragmentBox:
    case NGPhysicalBoxFragment::kFragmentRenderedLegend:
      if (child->BreakToken())
        child_break_tokens_.push_back(child->BreakToken());
      break;
    case NGPhysicalBoxFragment::kFragmentLineBox:
      // NGInlineNode produces multiple line boxes in an anonymous box. We won't
      // know up front which line box to insert a fragment break before (due to
      // widows), so keep them all until we know.
      DCHECK(child->BreakToken());
      DCHECK(child->BreakToken()->InputNode() == node_);
      inline_break_tokens_.push_back(child->BreakToken());
      break;
    case NGPhysicalBoxFragment::kFragmentText:
      DCHECK(!child->BreakToken());
      break;
    default:
      NOTREACHED();
      break;
  }

  return NGContainerFragmentBuilder::AddChild(std::move(child), child_offset);
}

void NGBoxFragmentBuilder::RemoveChildren() {
  child_break_tokens_.resize(0);
  inline_break_tokens_.resize(0);
  children_.resize(0);
  offsets_.resize(0);
}

NGBoxFragmentBuilder& NGBoxFragmentBuilder::AddBreakBeforeChild(
    NGLayoutInputNode child) {
  if (child.IsInline()) {
    if (inline_break_tokens_.IsEmpty()) {
      // In some cases we may want to break before the first line, as a last
      // resort. We need a break token for that as well, so that the machinery
      // will understand that we should resume at the beginning of the inline
      // formatting context, rather than concluding that we're done with the
      // whole thing.
      inline_break_tokens_.push_back(NGInlineBreakToken::Create(
          ToNGInlineNode(child), nullptr, 0, 0, NGInlineBreakToken::kDefault));
    }
    return *this;
  }
  auto token = NGBlockBreakToken::CreateBreakBefore(child);
  child_break_tokens_.push_back(token);
  return *this;
}

NGBoxFragmentBuilder& NGBoxFragmentBuilder::AddBreakBeforeLine(
    int line_number) {
  DCHECK_GT(line_number, 0);
  DCHECK_LE(unsigned(line_number), inline_break_tokens_.size());
  int lines_to_remove = inline_break_tokens_.size() - line_number;
  if (lines_to_remove > 0) {
    // Remove widows that should be pushed to the next fragment. We'll also
    // remove all other child fragments than line boxes (typically floats) that
    // come after the first line that's moved, as those also have to be re-laid
    // out in the next fragment.
    inline_break_tokens_.resize(line_number);
    DCHECK_GT(children_.size(), 0UL);
    for (int i = children_.size() - 1; i >= 0; i--) {
      DCHECK_NE(i, 0);
      if (!children_[i]->IsLineBox())
        continue;
      if (!--lines_to_remove) {
        // This is the first line that is going to the next fragment. Remove it,
        // and everything after it.
        children_.resize(i);
        offsets_.resize(i);
        break;
      }
    }
  }

  // We need to resume at the right inline location in the next fragment, but
  // broken floats, which are resumed and positioned by the parent block layout
  // algorithm, need to be ignored by the inline layout algorithm.
  ToNGInlineBreakToken(inline_break_tokens_.back().get())->SetIgnoreFloats();
  return *this;
}

NGBoxFragmentBuilder& NGBoxFragmentBuilder::PropagateBreak(
    const NGLayoutResult& child_layout_result) {
  if (!did_break_)
    PropagateBreak(*child_layout_result.PhysicalFragment());
  if (child_layout_result.HasForcedBreak())
    SetHasForcedBreak();
  else
    PropagateSpaceShortage(child_layout_result.MinimalSpaceShortage());
  return *this;
}

NGBoxFragmentBuilder& NGBoxFragmentBuilder::PropagateBreak(
    const NGPhysicalFragment& child_fragment) {
  if (!did_break_) {
    const auto* token = child_fragment.BreakToken();
    did_break_ = token && !token->IsFinished();
  }
  return *this;
}

void NGBoxFragmentBuilder::AddOutOfFlowLegacyCandidate(
    NGBlockNode node,
    const NGStaticPosition& static_position,
    LayoutObject* inline_container) {
  DCHECK_GE(InlineSize(), LayoutUnit());
  DCHECK_GE(BlockSize(), LayoutUnit());

  NGOutOfFlowPositionedDescendant descendant{node, static_position,
                                             inline_container};
  // Need 0,0 physical coordinates as child offset. Because offset
  // is stored as logical, must convert physical 0,0 to logical.
  NGLogicalOffset zero_offset;
  switch (GetWritingMode()) {
    case WritingMode::kHorizontalTb:
      if (IsLtr(Direction()))
        zero_offset = NGLogicalOffset();
      else
        zero_offset = NGLogicalOffset(InlineSize(), LayoutUnit());
      break;
    case WritingMode::kVerticalRl:
    case WritingMode::kSidewaysRl:
      if (IsLtr(Direction()))
        zero_offset = NGLogicalOffset(LayoutUnit(), BlockSize());
      else
        zero_offset = NGLogicalOffset(InlineSize(), BlockSize());
      break;
    case WritingMode::kVerticalLr:
    case WritingMode::kSidewaysLr:
      if (IsLtr(Direction()))
        zero_offset = NGLogicalOffset();
      else
        zero_offset = NGLogicalOffset(InlineSize(), LayoutUnit());
      break;
  }
  oof_positioned_candidates_.push_back(
      NGOutOfFlowPositionedCandidate{descendant, zero_offset});
}

NGPhysicalFragment::NGBoxType NGBoxFragmentBuilder::BoxType() const {
  if (box_type_ != NGPhysicalFragment::NGBoxType::kNormalBox)
    return box_type_;

  // When implicit, compute from LayoutObject.
  return BoxTypeFromLayoutObject(layout_object_);
}

void NGBoxFragmentBuilder::AddBaseline(NGBaselineRequest request,
                                       LayoutUnit offset) {
#if DCHECK_IS_ON()
  for (const auto& baseline : baselines_)
    DCHECK(baseline.request != request);
#endif
  baselines_.push_back(NGBaseline{request, offset});
}

EBreakBetween NGBoxFragmentBuilder::JoinedBreakBetweenValue(
    EBreakBetween break_before) const {
  return JoinFragmentainerBreakValues(previous_break_after_, break_before);
}

scoped_refptr<NGLayoutResult> NGBoxFragmentBuilder::ToBoxFragment(
    WritingMode block_or_line_writing_mode) {
  if (node_) {
    if (!inline_break_tokens_.IsEmpty()) {
      if (auto token = inline_break_tokens_.back()) {
        if (!token->IsFinished())
          child_break_tokens_.push_back(std::move(token));
      }
    }
    if (did_break_) {
      break_token_ = NGBlockBreakToken::Create(
          node_, used_block_size_, child_break_tokens_, has_last_resort_break_);
    } else if (needs_finished_break_token_) {
      break_token_ = NGBlockBreakToken::Create(node_, used_block_size_,
                                               has_last_resort_break_);
    }
  }

  scoped_refptr<const NGPhysicalBoxFragment> fragment =
      NGPhysicalBoxFragment::Create(this, block_or_line_writing_mode);

  return base::AdoptRef(new NGLayoutResult(std::move(fragment), this));
}

scoped_refptr<NGLayoutResult> NGBoxFragmentBuilder::Abort(
    NGLayoutResult::NGLayoutResultStatus status) {
  return base::AdoptRef(new NGLayoutResult(status, this));
}

// Finds FragmentPairs that define inline containing blocks.
// inline_container_fragments is a map whose keys specify which
// inline containing blocks are required.
// Not finding a required block is an unexpected behavior (DCHECK).
void NGBoxFragmentBuilder::ComputeInlineContainerFragments(
    HashMap<const LayoutObject*, FragmentPair>* inline_container_fragments,
    NGLogicalSize* container_size) {
  // This function has detailed knowledge of inline fragment tree structure,
  // and will break if this changes.
  DCHECK_GE(InlineSize(), LayoutUnit());
  DCHECK_GE(BlockSize(), LayoutUnit());
  *container_size = Size();

  for (wtf_size_t i = 0; i < children_.size(); i++) {
    if (children_[i]->IsLineBox()) {
      const NGPhysicalLineBoxFragment* linebox =
          ToNGPhysicalLineBoxFragment(children_[i].get());
      for (auto& descendant :
           NGInlineFragmentTraversal::DescendantsOf(*linebox)) {
        LayoutObject* key = {};
        if (descendant.fragment->IsText()) {
          key = descendant.fragment->GetLayoutObject();
          DCHECK(key);
          key = key->Parent();
          DCHECK(key);
        } else if (descendant.fragment->IsBox()) {
          key = descendant.fragment->GetLayoutObject();
        }
        if (key && inline_container_fragments->Contains(key)) {
          NGBoxFragmentBuilder::FragmentPair value =
              inline_container_fragments->at(key);
          if (!value.start_fragment) {
            value.start_fragment = descendant.fragment.get();
            value.start_fragment_union_rect.offset =
                descendant.offset_to_container_box;
            value.start_fragment_union_rect =
                NGPhysicalOffsetRect(descendant.offset_to_container_box,
                                     value.start_fragment->Size());
            value.start_linebox_fragment = linebox;
            value.start_linebox_offset = offsets_.at(i);
          }
          if (!value.end_fragment || value.end_linebox_fragment != linebox) {
            value.end_fragment = descendant.fragment.get();
            value.end_fragment_union_rect = NGPhysicalOffsetRect(
                descendant.offset_to_container_box, value.end_fragment->Size());
            value.end_linebox_fragment = linebox;
            value.end_linebox_offset = offsets_.at(i);
          }
          // Extend the union size
          if (value.start_linebox_fragment == linebox) {
            // std::max because initial box might have larger extent than its
            // descendants.
            value.start_fragment_union_rect.size.width =
                std::max(descendant.offset_to_container_box.left +
                             descendant.fragment->Size().width -
                             value.start_fragment_union_rect.offset.left,
                         value.start_fragment_union_rect.size.width);
            value.start_fragment_union_rect.size.height =
                std::max(descendant.offset_to_container_box.top +
                             descendant.fragment->Size().height -
                             value.start_fragment_union_rect.offset.top,
                         value.start_fragment_union_rect.size.height);
          }
          if (value.end_linebox_fragment == linebox) {
            value.end_fragment_union_rect.size.width =
                std::max(descendant.offset_to_container_box.left +
                             descendant.fragment->Size().width -
                             value.start_fragment_union_rect.offset.left,
                         value.end_fragment_union_rect.size.width);
            value.end_fragment_union_rect.size.height =
                std::max(descendant.offset_to_container_box.top +
                             descendant.fragment->Size().height -
                             value.start_fragment_union_rect.offset.top,
                         value.end_fragment_union_rect.size.height);
          }
          inline_container_fragments->Set(key, value);
        }
      }
    }
  }
}

}  // namespace blink
