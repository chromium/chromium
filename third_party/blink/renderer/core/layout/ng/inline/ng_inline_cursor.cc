// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_cursor.h"

#include "third_party/blink/renderer/core/editing/position_with_affinity.h"
#include "third_party/blink/renderer/core/layout/geometry/writing_mode_converter.h"
#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/core/layout/layout_text.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_fragment_items.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_physical_line_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_break_token.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"

namespace blink {
class HTMLBRElement;

namespace {

bool IsBidiControl(StringView string) {
  return string.length() == 1 && Character::IsBidiControl(string[0]);
}

}  // namespace

inline void NGInlineCursor::MoveToItem(const ItemsSpan::iterator& iter) {
  DCHECK(HasRoot());
  DCHECK(iter >= items_.begin() && iter <= items_.end());
  if (iter != items_.end()) {
    current_.Set(iter);
    return;
  }
  MakeNull();
}

void NGInlineCursor::SetRoot(const NGPhysicalBoxFragment& box_fragment,
                             const NGFragmentItems& fragment_items,
                             ItemsSpan items) {
  DCHECK_EQ(box_fragment.Items(), &fragment_items);
  DCHECK(items.data() || !items.size());
  root_box_fragment_ = &box_fragment;
  fragment_items_ = &fragment_items;
  items_ = items;
  DCHECK(fragment_items_->IsSubSpan(items_));
  MoveToItem(items_.begin());
}

void NGInlineCursor::SetRoot(const NGPhysicalBoxFragment& box_fragment,
                             const NGFragmentItems& items) {
  SetRoot(box_fragment, items, items.Items());
}

bool NGInlineCursor::TrySetRootFragmentItems() {
  DCHECK(root_block_flow_);
  DCHECK(!fragment_items_ || fragment_items_->Equals(items_));
  for (; fragment_index_ <= max_fragment_index_; AdvanceFragmentIndex()) {
    const NGPhysicalBoxFragment* fragment =
        root_block_flow_->GetPhysicalFragment(fragment_index_);
    DCHECK(fragment);
    if (const NGFragmentItems* items = fragment->Items()) {
      SetRoot(*fragment, *items);
      return true;
    }
  }
  return false;
}

void NGInlineCursor::SetRoot(const LayoutBlockFlow& block_flow) {
  DCHECK(&block_flow);
  DCHECK(!HasRoot());

  if (const wtf_size_t fragment_count = block_flow.PhysicalFragmentCount()) {
    root_block_flow_ = &block_flow;
    max_fragment_index_ = fragment_count - 1;
    ResetFragmentIndex();
    if (TrySetRootFragmentItems())
      return;
  }

  // We reach here in case of |ScrollANchor::NotifyBeforeLayout()| via
  // |LayoutText::PhysicalLinesBoundingBox()|
  // See external/wpt/css/css-scroll-anchoring/wrapped-text.html
}

NGInlineCursor::NGInlineCursor(const LayoutBlockFlow& block_flow) {
  SetRoot(block_flow);
}

NGInlineCursor::NGInlineCursor(const NGPhysicalBoxFragment& box_fragment,
                               const NGFragmentItems& fragment_items,
                               ItemsSpan items) {
  SetRoot(box_fragment, fragment_items, items);
}

NGInlineCursor::NGInlineCursor(const NGPhysicalBoxFragment& box_fragment,
                               const NGFragmentItems& items) {
  SetRoot(box_fragment, items);
}

NGInlineCursor::NGInlineCursor(const NGPhysicalBoxFragment& box_fragment) {
  if (const NGFragmentItems* items = box_fragment.Items())
    SetRoot(box_fragment, *items);
}

NGInlineCursor::NGInlineCursor(const NGInlineBackwardCursor& backward_cursor)
    : NGInlineCursor(backward_cursor.cursor_) {
  MoveTo(backward_cursor.Current());
}

bool NGInlineCursor::operator==(const NGInlineCursor& other) const {
  if (current_.item_ != other.current_.item_)
    return false;
  DCHECK_EQ(items_.data(), other.items_.data());
  DCHECK_EQ(items_.size(), other.items_.size());
  DCHECK_EQ(fragment_items_, other.fragment_items_);
  DCHECK(current_.item_iter_ == other.current_.item_iter_);
  return true;
}

const LayoutBlockFlow* NGInlineCursor::GetLayoutBlockFlow() const {
  DCHECK_EQ(HasRoot(), !!root_box_fragment_);
  if (root_box_fragment_) {
    const LayoutObject* layout_object =
        root_box_fragment_->GetSelfOrContainerLayoutObject();
    DCHECK(layout_object);
    DCHECK(!layout_object->IsLayoutFlowThread());
    return To<LayoutBlockFlow>(layout_object);
  }
  NOTREACHED();
  return nullptr;
}

bool NGInlineCursorPosition::HasChildren() const {
  if (item_)
    return item_->HasChildren();
  NOTREACHED();
  return false;
}

NGInlineCursor NGInlineCursor::CursorForDescendants() const {
  if (current_.item_) {
    unsigned descendants_count = current_.item_->DescendantsCount();
    if (descendants_count > 1) {
      DCHECK(root_box_fragment_);
      DCHECK(fragment_items_);
      return NGInlineCursor(
          *root_box_fragment_, *fragment_items_,
          ItemsSpan(&*(current_.item_iter_ + 1), descendants_count - 1));
    }
    return NGInlineCursor();
  }
  NOTREACHED();
  return NGInlineCursor();
}

void NGInlineCursor::ExpandRootToContainingBlock() {
  if (fragment_items_) {
    const unsigned index_diff = items_.data() - fragment_items_->Items().data();
    DCHECK_LT(index_diff, fragment_items_->Items().size());
    const unsigned item_index = current_.item_iter_ - items_.begin();
    items_ = fragment_items_->Items();
    // Update the iterator to the one for the new span.
    MoveToItem(items_.begin() + item_index + index_diff);
    return;
  }
  NOTREACHED();
}

bool NGInlineCursorPosition::HasSoftWrapToNextLine() const {
  DCHECK(IsLineBox());
  const NGInlineBreakToken* break_token = InlineBreakToken();
  return break_token && !break_token->IsForcedBreak();
}

bool NGInlineCursorPosition::IsInlineLeaf() const {
  if (IsHiddenForPaint())
    return false;
  if (IsText())
    return !IsLayoutGeneratedText();
  if (!IsAtomicInline())
    return false;
  return !IsListMarker();
}

bool NGInlineCursor::IsLastLineInInlineBlock() const {
  DCHECK(Current().IsLineBox());
  if (!GetLayoutBlockFlow()->IsAtomicInlineLevel())
    return false;
  NGInlineCursor next_sibling(*this);
  for (;;) {
    next_sibling.MoveToNextSkippingChildren();
    if (!next_sibling)
      return true;
    if (next_sibling.Current().IsLineBox())
      return false;
    // There maybe other top-level objects such as floats, OOF, or list-markers.
  }
}

bool NGInlineCursor::IsBeforeSoftLineBreak() const {
  if (Current().IsLineBreak())
    return false;
  // Inline block is not be container line box.
  // See paint/selection/text-selection-inline-block.html.
  NGInlineCursor line(*this);
  line.MoveToContainingLine();
  if (line.IsLastLineInInlineBlock()) {
    // We don't paint a line break the end of inline-block
    // because if an inline-block is at the middle of line, we should not paint
    // a line break.
    // Old layout paints line break if the inline-block is at the end of line,
    // but since its complex to determine if the inline-block is at the end of
    // line on NG, we just cancels block-end line break painting for any
    // inline-block.
    return false;
  }
  NGInlineCursor last_leaf(line);
  last_leaf.MoveToLastLogicalLeaf();
  if (last_leaf != *this)
    return false;
  // Even If |fragment| is before linebreak, if its direction differs to line
  // direction, we don't paint line break. See
  // paint/selection/text-selection-newline-mixed-ltr-rtl.html.
  return line.Current().BaseDirection() == Current().ResolvedDirection();
}

bool NGInlineCursorPosition::CanHaveChildren() const {
  if (item_) {
    return item_->Type() == NGFragmentItem::kLine ||
           (item_->Type() == NGFragmentItem::kBox && !item_->IsAtomicInline());
  }
  NOTREACHED();
  return false;
}

TextDirection NGInlineCursorPosition::BaseDirection() const {
  DCHECK(IsLineBox());
  if (item_)
    return item_->BaseDirection();
  NOTREACHED();
  return TextDirection::kLtr;
}

UBiDiLevel NGInlineCursorPosition::BidiLevel() const {
  if (IsText()) {
    if (IsLayoutGeneratedText()) {
      // TODO(yosin): Until we have clients, we don't support bidi-level for
      // ellipsis and soft hyphens.
      NOTREACHED() << this;
      return 0;
    }
    const auto& layout_text = *To<LayoutText>(GetLayoutObject());
    DCHECK(!layout_text.NeedsLayout()) << this;
    const auto* const items = layout_text.GetNGInlineItems();
    if (!items || items->size() == 0) {
      // In case of <br>, <wbr>, text-combine-upright, etc.
      return 0;
    }
    const NGTextOffset offset = TextOffset();
    const auto& item = std::find_if(
        items->begin(), items->end(), [offset](const NGInlineItem& item) {
          return item.StartOffset() <= offset.start &&
                 item.EndOffset() >= offset.end;
        });
    DCHECK(item != items->end()) << this;
    return item->BidiLevel();
  }

  if (IsAtomicInline()) {
    DCHECK(GetLayoutObject()->ContainingNGBlockFlow());
    const LayoutBlockFlow& block_flow =
        *GetLayoutObject()->ContainingNGBlockFlow();
    const Vector<NGInlineItem> items =
        block_flow.GetNGInlineNodeData()->ItemsData(UsesFirstLineStyle()).items;
    const LayoutObject* const layout_object = GetLayoutObject();
    const auto* const item = std::find_if(
        items.begin(), items.end(), [layout_object](const NGInlineItem& item) {
          return item.GetLayoutObject() == layout_object;
        });
    DCHECK(item != items.end()) << this;
    return item->BidiLevel();
  }

  NOTREACHED();
  return 0;
}

const DisplayItemClient* NGInlineCursorPosition::GetSelectionDisplayItemClient()
    const {
  if (const auto* client = GetLayoutObject()->GetSelectionDisplayItemClient())
    return client;
  return GetDisplayItemClient();
}

const Node* NGInlineCursorPosition::GetNode() const {
  if (const LayoutObject* layout_object = GetLayoutObject())
    return layout_object->GetNode();
  return nullptr;
}

void NGInlineCursorPosition::RecalcInkOverflow(
    const NGInlineCursor& cursor) const {
  DCHECK(item_);
  DCHECK_EQ(item_, cursor.Current().Item());
  PhysicalRect self_and_contents_rect;
  item_->GetMutableForPainting().RecalcInkOverflow(cursor,
                                                   &self_and_contents_rect);
}

StringView NGInlineCursorPosition::Text(const NGInlineCursor& cursor) const {
  DCHECK(IsText());
  cursor.CheckValid(*this);
  if (item_)
    return item_->Text(cursor.Items());
  NOTREACHED();
  return "";
}

PhysicalRect NGInlineCursor::CurrentLocalRect(unsigned start_offset,
                                              unsigned end_offset) const {
  DCHECK(Current().IsText());
  if (current_.item_) {
    return current_.item_->LocalRect(current_.item_->Text(*fragment_items_),
                                     start_offset, end_offset);
  }
  NOTREACHED();
  return PhysicalRect();
}

PhysicalRect NGInlineCursor::CurrentRectInBlockFlow() const {
  PhysicalRect rect = Current().RectInContainerFragment();
  // We'll now convert the offset from being relative to the containing fragment
  // to being relative to the containing LayoutBlockFlow. For writing modes that
  // don't flip the block direction, this is easy: just add the block-size
  // consumed in previous fragments.
  auto writing_direction = ContainerFragment().Style().GetWritingDirection();
  switch (writing_direction.GetWritingMode()) {
    default:
      rect.offset.top += previously_consumed_block_size_;
      break;
    case WritingMode::kVerticalLr:
      rect.offset.left += previously_consumed_block_size_;
      break;
    case WritingMode::kVerticalRl: {
      // For vertical-rl writing-mode it's a bit more complicated. We need to
      // convert to logical coordinates in the containing box fragment, in order
      // to add the consumed block-size to make it relative to the
      // LayoutBlockFlow ("flow thread coordinate space"), and then we convert
      // back to physical coordinates.
      const LayoutBlock* containing_block =
          Current().GetLayoutObject()->ContainingBlock();
      DCHECK_EQ(containing_block->StyleRef().GetWritingDirection(),
                ContainerFragment().Style().GetWritingDirection());
      LogicalOffset logical_offset = rect.offset.ConvertToLogical(
          writing_direction, ContainerFragment().Size(), rect.size);
      LogicalOffset logical_offset_in_flow_thread(
          logical_offset.inline_offset,
          logical_offset.block_offset + previously_consumed_block_size_);
      rect.offset = logical_offset_in_flow_thread.ConvertToPhysical(
          writing_direction, PhysicalSize(containing_block->Size()), rect.size);
      break;
    }
  };
  return rect;
}

LayoutUnit NGInlineCursor::InlinePositionForOffset(unsigned offset) const {
  DCHECK(Current().IsText());
  if (current_.item_) {
    return current_.item_->InlinePositionForOffset(
        current_.item_->Text(*fragment_items_), offset);
  }
  NOTREACHED();
  return LayoutUnit();
}

LogicalRect NGInlineCursorPosition::ConvertChildToLogical(
    const PhysicalRect& physical_rect) const {
  return WritingModeConverter(
             {Style().GetWritingMode(), ResolvedOrBaseDirection()}, Size())
      .ToLogical(physical_rect);
}

PhysicalRect NGInlineCursorPosition::ConvertChildToPhysical(
    const LogicalRect& logical_rect) const {
  return WritingModeConverter(
             {Style().GetWritingMode(), ResolvedOrBaseDirection()}, Size())
      .ToPhysical(logical_rect);
}

PositionWithAffinity NGInlineCursor::PositionForPointInInlineFormattingContext(
    const PhysicalOffset& point,
    const NGPhysicalBoxFragment& container) {
  DCHECK(HasRoot());
  const auto writing_direction = container.Style().GetWritingDirection();
  const PhysicalSize& container_size = container.Size();
  const LayoutUnit point_block_offset =
      point
          .ConvertToLogical(writing_direction, container_size,
                            // |point| is actually a pixel with size 1x1.
                            PhysicalSize(LayoutUnit(1), LayoutUnit(1)))
          .block_offset;

  // Stores the closest line box child after |point| in the block direction.
  // Used if we can't find any child |point| falls in to resolve the position.
  NGInlineCursorPosition closest_line_after;
  LayoutUnit closest_line_after_block_offset = LayoutUnit::Min();

  // Stores the closest line box child before |point| in the block direction.
  // Used if we can't find any child |point| falls in to resolve the position.
  NGInlineCursorPosition closest_line_before;
  LayoutUnit closest_line_before_block_offset = LayoutUnit::Max();

  while (*this) {
    const NGFragmentItem* child_item = CurrentItem();
    DCHECK(child_item);
    if (child_item->Type() == NGFragmentItem::kLine) {
      if (!CursorForDescendants().TryToMoveToFirstInlineLeafChild()) {
        // editing/selection/last-empty-inline.html requires this to skip
        // empty <span> with padding.
        MoveToNextSkippingChildren();
        continue;
      }
      // Try to resolve if |point| falls in a line box in block direction.
      const LayoutUnit child_block_offset =
          child_item->OffsetInContainerFragment()
              .ConvertToLogical(writing_direction, container_size,
                                child_item->Size())
              .block_offset;
      if (point_block_offset < child_block_offset) {
        if (child_block_offset < closest_line_before_block_offset) {
          closest_line_before_block_offset = child_block_offset;
          closest_line_before = Current();
        }
        MoveToNextSkippingChildren();
        continue;
      }

      // Hitting on line bottom doesn't count, to match legacy behavior.
      const LayoutUnit child_block_end_offset =
          child_block_offset +
          child_item->Size()
              .ConvertToLogical(writing_direction.GetWritingMode())
              .block_size;
      if (point_block_offset >= child_block_end_offset) {
        if (child_block_end_offset > closest_line_after_block_offset) {
          closest_line_after_block_offset = child_block_end_offset;
          closest_line_after = Current();
        }
        MoveToNextSkippingChildren();
        continue;
      }

      if (const PositionWithAffinity child_position =
              PositionForPointInInlineBox(point))
        return child_position;
      MoveToNextSkippingChildren();
      continue;
    }
    DCHECK_NE(child_item->Type(), NGFragmentItem::kText);
    MoveToNext();
  }

  // At here, |point| is not inside any line in |this|:
  //   |closest_line_before|
  //   |point|
  //   |closest_line_after|
  if (closest_line_before) {
    MoveTo(closest_line_before);
    // Note: |move_caret_to_boundary| is true for Mac and Unix.
    const bool move_caret_to_boundary =
        To<LayoutBlockFlow>(Current().GetLayoutObject())
            ->ShouldMoveCaretToHorizontalBoundaryWhenPastTopOrBottom();
    if (move_caret_to_boundary) {
      // Tests[1-3] reach here.
      // [1] editing/selection/click-in-margins-inside-editable-div.html
      // [2] fast/writing-mode/flipped-blocks-hit-test-line-edges.html
      // [3] All/LayoutViewHitTestTest.HitTestHorizontal/4
      if (auto first_position = PositionForStartOfLine())
        return PositionWithAffinity(first_position.GetPosition());
    } else if (const PositionWithAffinity child_position =
                   PositionForPointInInlineBox(point))
      return child_position;
  }

  if (closest_line_after) {
    MoveTo(closest_line_after);
    // Note: |move_caret_to_boundary| is true for Mac and Unix.
    const bool move_caret_to_boundary =
        To<LayoutBlockFlow>(Current().GetLayoutObject())
            ->ShouldMoveCaretToHorizontalBoundaryWhenPastTopOrBottom();
    if (move_caret_to_boundary) {
      // Tests[1-3] reach here.
      // [1] editing/selection/click-in-margins-inside-editable-div.html
      // [2] fast/writing-mode/flipped-blocks-hit-test-line-edges.html
      // [3] All/LayoutViewHitTestTest.HitTestHorizontal/4
      if (auto last_position = PositionForEndOfLine())
        return PositionWithAffinity(last_position.GetPosition());
    } else if (const PositionWithAffinity child_position =
                   PositionForPointInInlineBox(point)) {
      // Test[1] reaches here.
      // [1] editing/selection/last-empty-inline.html
      return child_position;
    }
  }

  return PositionWithAffinity();
}

PositionWithAffinity NGInlineCursor::PositionForPointInInlineBox(
    const PhysicalOffset& point) const {
  const NGFragmentItem* container = CurrentItem();
  DCHECK(container);
  DCHECK(container->Type() == NGFragmentItem::kLine ||
         container->Type() == NGFragmentItem::kBox);
  const auto writing_direction = container->Style().GetWritingDirection();
  const PhysicalSize& container_size = container->Size();
  const LayoutUnit point_inline_offset =
      point
          .ConvertToLogical(writing_direction, container_size,
                            // |point| is actually a pixel with size 1x1.
                            PhysicalSize(LayoutUnit(1), LayoutUnit(1)))
          .inline_offset;

  // Stores the closest child before |point| in the inline direction. Used if we
  // can't find any child |point| falls in to resolve the position.
  NGInlineCursorPosition closest_child_before;
  LayoutUnit closest_child_before_inline_offset = LayoutUnit::Min();

  // Stores the closest child after |point| in the inline direction. Used if we
  // can't find any child |point| falls in to resolve the position.
  NGInlineCursorPosition closest_child_after;
  LayoutUnit closest_child_after_inline_offset = LayoutUnit::Max();

  NGInlineCursor descendants = CursorForDescendants();
  for (; descendants; descendants.MoveToNext()) {
    const NGFragmentItem* child_item = descendants.CurrentItem();
    DCHECK(child_item);
    if (child_item->Type() == NGFragmentItem::kBox &&
        !child_item->BoxFragment()) {
      // Skip virtually "culled" inline box, e.g. <span>foo</span>
      // "editing/selection/shift-click.html" reaches here.
      DCHECK(child_item->GetLayoutObject()->IsLayoutInline()) << child_item;
      continue;
    }
    const LayoutUnit child_inline_offset =
        child_item->OffsetInContainerFragment()
            .ConvertToLogical(writing_direction, container_size,
                              child_item->Size())
            .inline_offset;
    if (point_inline_offset < child_inline_offset) {
      if (child_inline_offset < closest_child_after_inline_offset) {
        closest_child_after_inline_offset = child_inline_offset;
        closest_child_after = descendants.Current();
      }
      continue;
    }
    const LayoutUnit child_inline_end_offset =
        child_inline_offset +
        child_item->Size()
            .ConvertToLogical(writing_direction.GetWritingMode())
            .inline_size;
    if (point_inline_offset > child_inline_end_offset) {
      if (child_inline_end_offset > closest_child_before_inline_offset) {
        closest_child_before_inline_offset = child_inline_end_offset;
        closest_child_before = descendants.Current();
      }
      continue;
    }

    if (const PositionWithAffinity child_position =
            descendants.PositionForPointInChild(point))
      return child_position;
  }

  if (closest_child_after) {
    descendants.MoveTo(closest_child_after);
    if (const PositionWithAffinity child_position =
            descendants.PositionForPointInChild(point))
      return child_position;
    if (closest_child_after->BoxFragment()) {
      // Hit test at left of "12"[1] and after "cd"[2] reache here.
      // "<span dir="rtl">12<b>&#x05E7;&#x05D0;43</b></span>ab"
      // [1] "editing/selection/caret-at-bidi-boundary.html"
      // [2] HitTestingTest.PseudoElementAfter
      if (const PositionWithAffinity child_position =
              descendants.PositionForPointInInlineBox(point))
        return child_position;
    }
  }

  if (closest_child_before) {
    descendants.MoveTo(closest_child_before);
    if (const PositionWithAffinity child_position =
            descendants.PositionForPointInChild(point))
      return child_position;
    if (closest_child_before->BoxFragment()) {
      // LayoutViewHitTest.HitTestHorizontal "Top-right corner (outside) of div"
      // reach here.
      if (const PositionWithAffinity child_position =
              descendants.PositionForPointInInlineBox(point))
        return child_position;
    }
  }

  return PositionWithAffinity();
}

PositionWithAffinity NGInlineCursor::PositionForPointInChild(
    const PhysicalOffset& point_in_container) const {
  DCHECK(CurrentItem());
  const NGFragmentItem& child_item = *CurrentItem();
  switch (child_item.Type()) {
    case NGFragmentItem::kText:
      return child_item.PositionForPointInText(
          point_in_container - child_item.OffsetInContainerFragment(), *this);
    case NGFragmentItem::kGeneratedText:
      break;
    case NGFragmentItem::kBox:
      if (const NGPhysicalBoxFragment* box_fragment =
              child_item.BoxFragment()) {
        if (!box_fragment->IsInlineBox()) {
          // In case of inline block with with block formatting context that
          // has block children[1].
          // Example: <b style="display:inline-block"><div>b</div></b>
          // [1] NGInlineCursorTest.PositionForPointInChildBlockChildren
          return child_item.GetLayoutObject()->PositionForPoint(
              point_in_container - child_item.OffsetInContainerFragment());
        }
      } else {
        // |LayoutInline| used to be culled.
      }
      DCHECK(child_item.GetLayoutObject()->IsLayoutInline()) << child_item;
      break;
    case NGFragmentItem::kLine:
      NOTREACHED();
      break;
  }
  return PositionWithAffinity();
}

PositionWithAffinity NGInlineCursor::PositionForPointInText(
    unsigned text_offset) const {
  DCHECK(Current().IsText()) << this;
  if (HasRoot())
    return Current()->PositionForPointInText(text_offset, *this);
  return PositionWithAffinity();
}

PositionWithAffinity NGInlineCursor::PositionForStartOfLine() const {
  DCHECK(Current().IsLineBox());
  NGInlineCursor first_leaf = CursorForDescendants();
  if (IsLtr(Current().BaseDirection()))
    first_leaf.MoveToFirstNonPseudoLeaf();
  else
    first_leaf.MoveToLastNonPseudoLeaf();
  if (!first_leaf)
    return PositionWithAffinity();
  Node* const node = first_leaf.Current().GetLayoutObject()->NonPseudoNode();
  if (!node) {
    NOTREACHED() << "MoveToFirstLeaf returns invalid node: " << first_leaf;
    return PositionWithAffinity();
  }
  if (!IsA<Text>(node))
    return PositionWithAffinity(Position::BeforeNode(*node));
  const unsigned text_offset =
      Current().BaseDirection() == first_leaf.Current().ResolvedDirection()
          ? first_leaf.Current().TextOffset().start
          : first_leaf.Current().TextOffset().end;
  return first_leaf.PositionForPointInText(text_offset);
}

PositionWithAffinity NGInlineCursor::PositionForEndOfLine() const {
  DCHECK(Current().IsLineBox());
  NGInlineCursor last_leaf = CursorForDescendants();
  if (IsLtr(Current().BaseDirection()))
    last_leaf.MoveToLastNonPseudoLeaf();
  else
    last_leaf.MoveToFirstNonPseudoLeaf();
  if (!last_leaf)
    return PositionWithAffinity();
  Node* const node = last_leaf.Current().GetLayoutObject()->NonPseudoNode();
  if (!node) {
    NOTREACHED() << "MoveToLastLeaf returns invalid node: " << last_leaf;
    return PositionWithAffinity();
  }
  if (IsA<HTMLBRElement>(node))
    return PositionWithAffinity(Position::BeforeNode(*node));
  if (!IsA<Text>(node))
    return PositionWithAffinity(Position::AfterNode(*node));
  const unsigned text_offset =
      Current().BaseDirection() == last_leaf.Current().ResolvedDirection()
          ? last_leaf.Current().TextOffset().end
          : last_leaf.Current().TextOffset().start;
  return last_leaf.PositionForPointInText(text_offset);
}

void NGInlineCursor::MoveTo(const NGInlineCursorPosition& position) {
  CheckValid(position);
  current_ = position;
}

inline wtf_size_t NGInlineCursor::SpanBeginItemIndex() const {
  DCHECK(HasRoot());
  DCHECK(!items_.empty());
  DCHECK(fragment_items_->IsSubSpan(items_));
  const wtf_size_t delta = items_.data() - fragment_items_->Items().data();
  DCHECK_LT(delta, fragment_items_->Items().size());
  return delta;
}

inline wtf_size_t NGInlineCursor::SpanIndexFromItemIndex(unsigned index) const {
  DCHECK(HasRoot());
  DCHECK(!items_.empty());
  DCHECK(fragment_items_->IsSubSpan(items_));
  if (items_.data() == fragment_items_->Items().data())
    return index;
  const wtf_size_t span_index =
      fragment_items_->Items().data() - items_.data() + index;
  DCHECK_LT(span_index, items_.size());
  return span_index;
}

void NGInlineCursor::MoveTo(const NGFragmentItem& fragment_item) {
  MoveTo(*fragment_item.GetLayoutObject());
  while (IsNotNull()) {
    if (CurrentItem() == &fragment_item)
      return;
    MoveToNext();
  }
  NOTREACHED();
}

void NGInlineCursor::MoveTo(const NGInlineCursor& cursor) {
  if (cursor.current_.item_) {
    if (!fragment_items_)
      SetRoot(*cursor.root_box_fragment_, *cursor.fragment_items_);
    // Note: We use address instead of iterator because we can't compare
    // iterators in different span. See |base::CheckedContiguousIterator<T>|.
    const ptrdiff_t index = &*cursor.current_.item_iter_ - &*items_.begin();
    DCHECK_GE(index, 0);
    DCHECK_LT(static_cast<size_t>(index), items_.size());
    MoveToItem(items_.begin() + index);
    return;
  }
  *this = cursor;
}

void NGInlineCursor::MoveToContainingLine() {
  DCHECK(!Current().IsLineBox());
  if (current_.item_) {
    while (current_.item_ && !Current().IsLineBox())
      MoveToPrevious();
    return;
  }
  NOTREACHED();
}

bool NGInlineCursor::IsAtFirst() const {
  if (const NGFragmentItem* item = Current().Item())
    return item == &items_.front();
  return false;
}

void NGInlineCursor::MoveToFirst() {
  if (HasRoot()) {
    MoveToItem(items_.begin());
    return;
  }
  NOTREACHED();
}

void NGInlineCursor::MoveToFirstChild() {
  DCHECK(Current().CanHaveChildren());
  if (!TryToMoveToFirstChild())
    MakeNull();
}

void NGInlineCursor::MoveToFirstLine() {
  if (HasRoot()) {
    auto iter = std::find_if(
        items_.begin(), items_.end(),
        [](const auto& item) { return item.Type() == NGFragmentItem::kLine; });
    if (iter != items_.end()) {
      MoveToItem(iter);
      return;
    }
    MakeNull();
    return;
  }
  NOTREACHED();
}

void NGInlineCursor::MoveToFirstLogicalLeaf() {
  DCHECK(Current().IsLineBox());
  // TODO(yosin): This isn't correct for mixed Bidi. Fix it. Besides, we
  // should compute and store it during layout.
  // TODO(yosin): We should check direction of each container instead of line
  // box.
  if (IsLtr(Current().Style().Direction())) {
    while (TryToMoveToFirstChild())
      continue;
    return;
  }
  while (TryToMoveToLastChild())
    continue;
}

void NGInlineCursor::MoveToFirstNonPseudoLeaf() {
  for (NGInlineCursor cursor = *this; cursor; cursor.MoveToNext()) {
    if (!cursor.Current().GetLayoutObject()->NonPseudoNode())
      continue;
    if (cursor.Current().IsText()) {
      // Note: We should not skip bidi control only text item to return
      // position after bibi control character, e.g.
      // <p dir=rtl>&#x202B;xyz ABC.&#x202C;</p>
      // See "editing/selection/home-end.html".
      DCHECK(!cursor.Current().IsLayoutGeneratedText()) << cursor;
      *this = cursor;
      return;
    }
    if (cursor.Current().IsInlineLeaf()) {
      *this = cursor;
      return;
    }
  }
  MakeNull();
}

void NGInlineCursor::MoveToLastChild() {
  DCHECK(Current().CanHaveChildren());
  if (!TryToMoveToLastChild())
    MakeNull();
}

void NGInlineCursor::MoveToLastLine() {
  DCHECK(HasRoot());
  auto iter = std::find_if(
      items_.rbegin(), items_.rend(),
      [](const auto& item) { return item.Type() == NGFragmentItem::kLine; });
  if (iter != items_.rend())
    MoveToItem(std::next(iter).base());
  else
    MakeNull();
}

void NGInlineCursor::MoveToLastLogicalLeaf() {
  DCHECK(Current().IsLineBox());
  // TODO(yosin): This isn't correct for mixed Bidi. Fix it. Besides, we
  // should compute and store it during layout.
  // TODO(yosin): We should check direction of each container instead of line
  // box.
  if (IsLtr(Current().Style().Direction())) {
    while (TryToMoveToLastChild())
      continue;
    return;
  }
  while (TryToMoveToFirstChild())
    continue;
}

void NGInlineCursor::MoveToLastNonPseudoLeaf() {
  // TODO(yosin): We should introduce |IsTruncated()| to avoid to use
  // |in_hidden_for_paint|. See also |LayoutText::GetTextBoxInfo()|.
  // When "text-overflow:ellipsis" specified, items are:
  //  [i+0] original non-truncated text (IsHiddenForPaint()=true)
  //  [i+1] truncated text
  //  [i+2] ellipsis (IsLayoutGeneratedText())
  NGInlineCursor last_leaf;
  bool in_hidden_for_paint = false;
  for (NGInlineCursor cursor = *this; cursor; cursor.MoveToNext()) {
    if (!cursor.Current().GetLayoutObject()->NonPseudoNode())
      continue;
    if (cursor.Current().IsLineBreak() && last_leaf)
      break;
    if (cursor.Current().IsText()) {
      DCHECK(!cursor.Current().IsLayoutGeneratedText());
      if (in_hidden_for_paint && !cursor.Current().IsHiddenForPaint()) {
        // |cursor| is at truncated text.
        break;
      }
      in_hidden_for_paint = cursor.Current().IsHiddenForPaint();
      // Exclude bidi control only fragment, e.g.
      // <p dir=ltr>&#x202B;xyz ABC.&#x202C;</p> has
      //  [0] "\u202Bxyz "
      //  [1] "ABC"
      //  [2] "."
      //  [3] "\u202C"
      // See "editing/selection/home-end.html"
      if (IsBidiControl(cursor.Current().Text(cursor)))
        continue;
      last_leaf = cursor;
      continue;
    }
    if (cursor.Current().IsInlineLeaf())
      last_leaf = cursor;
  }
  *this = last_leaf;
}

void NGInlineCursor::MoveToNextInlineLeaf() {
  if (Current() && Current().IsInlineLeaf())
    MoveToNext();
  while (Current() && !Current().IsInlineLeaf())
    MoveToNext();
}

void NGInlineCursor::MoveToNextInlineLeafIgnoringLineBreak() {
  do {
    MoveToNextInlineLeaf();
  } while (Current() && Current().IsLineBreak());
}

void NGInlineCursor::MoveToNextInlineLeafOnLine() {
  MoveToLastForSameLayoutObject();
  if (IsNull())
    return;
  NGInlineCursor last_item = *this;
  MoveToContainingLine();
  NGInlineCursor cursor = CursorForDescendants();
  cursor.MoveTo(last_item);
  // Note: AX requires this for AccessibilityLayoutTest.NextOnLine.
  if (!cursor.Current().IsInlineLeaf())
    cursor.MoveToNextInlineLeaf();
  cursor.MoveToNextInlineLeaf();
  MoveTo(cursor);
}

void NGInlineCursor::MoveToNextLine() {
  DCHECK(Current().IsLineBox());
  if (current_.item_) {
    do {
      MoveToNext();
    } while (Current() && !Current().IsLineBox());
    return;
  }
  NOTREACHED();
}

void NGInlineCursor::MoveToPreviousInlineLeaf() {
  if (Current() && Current().IsInlineLeaf())
    MoveToPrevious();
  while (Current() && !Current().IsInlineLeaf())
    MoveToPrevious();
}

void NGInlineCursor::MoveToPreviousInlineLeafIgnoringLineBreak() {
  do {
    MoveToPreviousInlineLeaf();
  } while (Current() && Current().IsLineBreak());
}

void NGInlineCursor::MoveToPreviousInlineLeafOnLine() {
  if (IsNull())
    return;
  NGInlineCursor first_item = *this;
  MoveToContainingLine();
  NGInlineCursor cursor = CursorForDescendants();
  cursor.MoveTo(first_item);
  // Note: AX requires this for AccessibilityLayoutTest.NextOnLine.
  if (!cursor.Current().IsInlineLeaf())
    cursor.MoveToPreviousInlineLeaf();
  cursor.MoveToPreviousInlineLeaf();
  MoveTo(cursor);
}

void NGInlineCursor::MoveToPreviousLine() {
  // Note: List marker is sibling of line box.
  DCHECK(Current().IsLineBox());
  if (current_.item_) {
    do {
      MoveToPrevious();
    } while (Current() && !Current().IsLineBox());
    return;
  }
  NOTREACHED();
}

bool NGInlineCursor::TryToMoveToFirstChild() {
  if (!Current().HasChildren())
    return false;
  MoveToItem(current_.item_iter_ + 1);
  return true;
}

bool NGInlineCursor::TryToMoveToFirstInlineLeafChild() {
  while (IsNotNull()) {
    if (Current().IsInlineLeaf())
      return true;
    MoveToNext();
  }
  return false;
}

bool NGInlineCursor::TryToMoveToLastChild() {
  if (!Current().HasChildren())
    return false;
  const auto end = current_.item_iter_ + CurrentItem()->DescendantsCount();
  MoveToNext();  // Move to the first child.
  DCHECK(!IsNull());
  while (true) {
    ItemsSpan::iterator previous = Current().item_iter_;
    DCHECK(previous < end);
    MoveToNextSkippingChildren();
    if (!Current() || Current().item_iter_ == end) {
      MoveToItem(previous);
      break;
    }
  }
  return true;
}

void NGInlineCursor::MoveToNext() {
  DCHECK(HasRoot());
  if (UNLIKELY(!current_.item_))
    return;
  DCHECK(current_.item_iter_ != items_.end());
  if (++current_.item_iter_ != items_.end()) {
    current_.item_ = &*current_.item_iter_;
    return;
  }
  MakeNull();
}

void NGInlineCursor::MoveToNextSkippingChildren() {
  DCHECK(HasRoot());
  if (UNLIKELY(!current_.item_))
    return;
  // If the current item has |DescendantsCount|, add it to move to the next
  // sibling, skipping all children and their descendants.
  if (wtf_size_t descendants_count = current_.item_->DescendantsCount())
    return MoveToItem(current_.item_iter_ + descendants_count);
  return MoveToNext();
}

void NGInlineCursor::MoveToPrevious() {
  DCHECK(HasRoot());
  if (UNLIKELY(!current_.item_))
    return;
  if (current_.item_iter_ == items_.begin())
    return MakeNull();
  --current_.item_iter_;
  current_.item_ = &*current_.item_iter_;
}

void NGInlineCursor::MoveToFirstIncludingFragmentainer() {
  if (!fragment_index_) {
    MoveToFirst();
    return;
  }

  ResetFragmentIndex();
  if (!TrySetRootFragmentItems())
    MakeNull();
}

void NGInlineCursor::MoveToNextFragmentainer() {
  DCHECK(CanMoveAcrossFragmentainer());
  if (fragment_index_ < max_fragment_index_) {
    AdvanceFragmentIndex();
    if (TrySetRootFragmentItems())
      return;
  }
  MakeNull();
}

void NGInlineCursor::MoveToNextIncludingFragmentainer() {
  MoveToNext();
  if (!Current() && max_fragment_index_ && CanMoveAcrossFragmentainer())
    MoveToNextFragmentainer();
}

void NGInlineCursor::SlowMoveToForIfNeeded(const LayoutObject& layout_object) {
  while (Current() && Current().GetLayoutObject() != &layout_object)
    MoveToNextIncludingFragmentainer();
}

void NGInlineCursor::SlowMoveToFirstFor(const LayoutObject& layout_object) {
  MoveToFirstIncludingFragmentainer();
  SlowMoveToForIfNeeded(layout_object);
}

void NGInlineCursor::SlowMoveToNextForSameLayoutObject(
    const LayoutObject& layout_object) {
  MoveToNextIncludingFragmentainer();
  SlowMoveToForIfNeeded(layout_object);
}

void NGInlineCursor::MoveTo(const LayoutObject& layout_object) {
  DCHECK(layout_object.IsInLayoutNGInlineFormattingContext());
  if (UNLIKELY(layout_object.IsOutOfFlowPositioned())) {
    NOTREACHED();
    MakeNull();
    return;
  }

  // If this cursor is rootless, find the root of the inline formatting context.
  bool is_descendants_cursor = false;
  if (!HasRoot()) {
    const LayoutBlockFlow* root = layout_object.FragmentItemsContainer();
    DCHECK(root);
    SetRoot(*root);
    if (UNLIKELY(!HasRoot())) {
      MakeNull();
      return;
    }
    DCHECK(!IsDescendantsCursor());
  } else {
    is_descendants_cursor = IsDescendantsCursor();
  }

  wtf_size_t item_index = layout_object.FirstInlineFragmentItemIndex();
  if (UNLIKELY(!item_index)) {
#if DCHECK_IS_ON()
    const LayoutBlockFlow* root = layout_object.FragmentItemsContainer();
    NGInlineCursor check_cursor(*root);
    check_cursor.SlowMoveToFirstFor(layout_object);
    DCHECK(!check_cursor);
#endif
    MakeNull();
    return;
  }
  // |FirstInlineFragmentItemIndex| is 1-based. Convert to 0-based index.
  --item_index;

  // Find |NGFragmentItems| that contains |item_index|.
  DCHECK_EQ(is_descendants_cursor, IsDescendantsCursor());
  if (root_block_flow_) {
    DCHECK(!is_descendants_cursor);
    while (item_index >= fragment_items_->EndItemIndex()) {
      MoveToNextFragmentainer();
      if (!Current()) {
        NOTREACHED();
        return;
      }
    }
    item_index -= fragment_items_->SizeOfEarlierFragments();
#if DCHECK_IS_ON()
    NGInlineCursor check_cursor(*root_block_flow_);
    check_cursor.SlowMoveToFirstFor(layout_object);
    DCHECK_EQ(check_cursor.Current().Item(),
              &fragment_items_->Items()[item_index]);
#endif
  } else {
    // If |this| is not rooted at |LayoutBlockFlow|, iterate |NGFragmentItems|
    // from |LayoutBlockFlow|.
    if (fragment_items_->HasItemIndex(item_index)) {
      item_index -= fragment_items_->SizeOfEarlierFragments();
    } else {
      NGInlineCursor cursor;
      for (cursor.MoveTo(layout_object);;
           cursor.MoveToNextForSameLayoutObject()) {
        if (!cursor || cursor.fragment_items_->SizeOfEarlierFragments() >
                           fragment_items_->SizeOfEarlierFragments()) {
          MakeNull();
          return;
        }
        if (cursor.fragment_items_ == fragment_items_) {
          item_index =
              cursor.Current().Item() - fragment_items_->Items().data();
          break;
        }
      }
    }
#if DCHECK_IS_ON()
    const LayoutBlockFlow* root = layout_object.FragmentItemsContainer();
    NGInlineCursor check_cursor(*root);
    check_cursor.SlowMoveToFirstFor(layout_object);
    while (check_cursor && fragment_items_ != check_cursor.fragment_items_)
      check_cursor.SlowMoveToNextForSameLayoutObject(layout_object);
    DCHECK_EQ(check_cursor.Current().Item(),
              &fragment_items_->Items()[item_index]);
#endif

    // Skip items before |items_|, in case |this| is part of IFC.
    if (UNLIKELY(is_descendants_cursor)) {
      const wtf_size_t span_begin_item_index = SpanBeginItemIndex();
      while (UNLIKELY(item_index < span_begin_item_index)) {
        const NGFragmentItem& item = fragment_items_->Items()[item_index];
        const wtf_size_t next_delta = item.DeltaToNextForSameLayoutObject();
        if (!next_delta) {
          MakeNull();
          return;
        }
        item_index += next_delta;
      }
      if (UNLIKELY(item_index >= span_begin_item_index + items_.size())) {
        MakeNull();
        return;
      }
      item_index -= span_begin_item_index;
    }
  }

  DCHECK_LT(item_index, items_.size());
  current_.Set(items_.begin() + item_index);
}

void NGInlineCursor::MoveToNextForSameLayoutObjectExceptCulledInline() {
  if (!Current())
    return;
  if (wtf_size_t delta = current_.item_->DeltaToNextForSameLayoutObject()) {
    while (true) {
      // Return if the next index is in the current range.
      const wtf_size_t delta_to_end = items_.end() - current_.item_iter_;
      if (delta < delta_to_end) {
        MoveToItem(current_.item_iter_ + delta);
        return;
      }

      // |this| is |IsDescendantsCursor| and the next item is out of the
      // specified range, or the next item is in following fragmentainers.
      if (!CanMoveAcrossFragmentainer())
        break;

      MoveToNextFragmentainer();
      if (!Current()) {
        NOTREACHED();
        break;
      }
      DCHECK_GE(delta, delta_to_end);
      delta -= delta_to_end;
    }
  }
  MakeNull();
}

void NGInlineCursor::MoveToLastForSameLayoutObject() {
  if (!Current())
    return;
  NGInlineCursorPosition last;
  do {
    last = Current();
    MoveToNextForSameLayoutObject();
  } while (Current());
  MoveTo(last);
}

//
// Functions to enumerate fragments that contribute to a culled inline.
//

// Traverse the |LayoutObject| tree in pre-order DFS and find a |LayoutObject|
// that contributes to the culled inline.
const LayoutObject* NGInlineCursor::CulledInlineTraversal::Find(
    const LayoutObject* child) const {
  while (child) {
    if (child->IsText())
      return child;

    if (child->IsBox()) {
      if (!child->IsFloatingOrOutOfFlowPositioned())
        return child;
      child = child->NextInPreOrderAfterChildren(layout_inline_);
      continue;
    }

    if (const auto* child_layout_inline = DynamicTo<LayoutInline>(child)) {
      if (child_layout_inline->ShouldCreateBoxFragment())
        return child;

      // A culled inline can be computed from its direct children, but when the
      // child is also culled, traverse its grand children.
      if (const LayoutObject* grand_child = child_layout_inline->FirstChild()) {
        child = grand_child;
        continue;
      }
    }

    child = child->NextInPreOrderAfterChildren(layout_inline_);
  }
  return nullptr;
}

const LayoutObject* NGInlineCursor::CulledInlineTraversal::MoveToFirstFor(
    const LayoutInline& layout_inline) {
  layout_inline_ = &layout_inline;
  current_object_ = Find(layout_inline.FirstChild());
  return current_object_;
}

const LayoutObject* NGInlineCursor::CulledInlineTraversal::MoveToNext() {
  DCHECK(current_object_);
  current_object_ =
      Find(current_object_->NextInPreOrderAfterChildren(layout_inline_));
  return current_object_;
}

void NGInlineCursor::MoveToFirstForCulledInline(
    const LayoutInline& layout_inline) {
  if (const LayoutObject* layout_object =
          culled_inline_.MoveToFirstFor(layout_inline)) {
    MoveTo(*layout_object);
    // This |MoveTo| may fail if |this| is a descendant cursor. Try the next
    // |LayoutObject|.
    MoveToNextCulledInlineDescendantIfNeeded();
  }
}

void NGInlineCursor::MoveToNextForCulledInline() {
  DCHECK(culled_inline_);
  MoveToNextForSameLayoutObjectExceptCulledInline();
  // If we're at the end of fragments for the current |LayoutObject| that
  // contributes to the current culled inline, find the next |LayoutObject|.
  MoveToNextCulledInlineDescendantIfNeeded();
}

void NGInlineCursor::MoveToNextCulledInlineDescendantIfNeeded() {
  DCHECK(culled_inline_);
  if (Current())
    return;

  while (const LayoutObject* layout_object = culled_inline_.MoveToNext()) {
    MoveTo(*layout_object);
    if (Current())
      return;
  }
}

void NGInlineCursor::ResetFragmentIndex() {
  fragment_index_ = 0;
  previously_consumed_block_size_ = LayoutUnit();
}

void NGInlineCursor::AdvanceFragmentIndex() {
  fragment_index_++;
  if (!root_box_fragment_)
    return;
  if (const auto* break_token =
          To<NGBlockBreakToken>(root_box_fragment_->BreakToken()))
    previously_consumed_block_size_ = break_token->ConsumedBlockSize();
}

void NGInlineCursor::MoveToIncludingCulledInline(
    const LayoutObject& layout_object) {
  DCHECK(layout_object.IsInLayoutNGInlineFormattingContext()) << layout_object;

  culled_inline_.Reset();
  MoveTo(layout_object);
  if (Current() || !HasRoot())
    return;

  // If this is a culled inline, find fragments for descendant |LayoutObject|s
  // that contribute to the culled inline.
  if (const auto* layout_inline = DynamicTo<LayoutInline>(layout_object)) {
    if (!layout_inline->ShouldCreateBoxFragment())
      MoveToFirstForCulledInline(*layout_inline);
  }
}

void NGInlineCursor::MoveToNextForSameLayoutObject() {
  if (UNLIKELY(culled_inline_)) {
    MoveToNextForCulledInline();
    return;
  }
  MoveToNextForSameLayoutObjectExceptCulledInline();
}

//
// |NGInlineBackwardCursor| functions.
//
NGInlineBackwardCursor::NGInlineBackwardCursor(const NGInlineCursor& cursor)
    : cursor_(cursor) {
  if (cursor.HasRoot()) {
    DCHECK(!cursor || cursor.items_.begin() == cursor.Current().item_iter_);
    for (NGInlineCursor sibling(cursor); sibling;
         sibling.MoveToNextSkippingChildren())
      sibling_item_iterators_.push_back(sibling.Current().item_iter_);
    current_index_ = sibling_item_iterators_.size();
    if (current_index_)
      current_.Set(sibling_item_iterators_[--current_index_]);
    return;
  }
  DCHECK(!cursor);
}

NGInlineCursor NGInlineBackwardCursor::CursorForDescendants() const {
  if (current_.item_) {
    NGInlineCursor cursor(cursor_);
    cursor.MoveToItem(sibling_item_iterators_[current_index_]);
    return cursor.CursorForDescendants();
  }
  NOTREACHED();
  return NGInlineCursor();
}

void NGInlineBackwardCursor::MoveToPreviousSibling() {
  if (current_index_) {
    if (current_.item_) {
      current_.Set(sibling_item_iterators_[--current_index_]);
      return;
    }
    NOTREACHED();
  }
  current_.Clear();
}

std::ostream& operator<<(std::ostream& ostream, const NGInlineCursor& cursor) {
  if (!cursor)
    return ostream << "NGInlineCursor()";
  DCHECK(cursor.HasRoot());
  return ostream << "NGInlineCursor(" << *cursor.CurrentItem() << ")";
}

std::ostream& operator<<(std::ostream& ostream, const NGInlineCursor* cursor) {
  if (!cursor)
    return ostream << "<null>";
  return ostream << *cursor;
}

#if DCHECK_IS_ON()
void NGInlineCursor::CheckValid(const NGInlineCursorPosition& position) const {
  if (position.Item()) {
    DCHECK(HasRoot());
    DCHECK_EQ(position.item_, &*position.item_iter_);
    const unsigned index = position.item_iter_ - items_.begin();
    DCHECK_LT(index, items_.size());
  }
}
#endif

}  // namespace blink
