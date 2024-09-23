// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/core/layout/inline/inline_cursor.h"

#include "base/containers/adapters.h"
#include "base/not_fatal_until.h"
#include "base/ranges/algorithm.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/position_with_affinity.h"
#include "third_party/blink/renderer/core/html/html_br_element.h"
#include "third_party/blink/renderer/core/layout/block_break_token.h"
#include "third_party/blink/renderer/core/layout/geometry/writing_mode_converter.h"
#include "third_party/blink/renderer/core/layout/inline/fragment_items.h"
#include "third_party/blink/renderer/core/layout/inline/inline_item_span.h"
#include "third_party/blink/renderer/core/layout/inline/physical_line_box_fragment.h"
#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/core/layout/layout_text.h"
#include "third_party/blink/renderer/core/layout/layout_text_combine.h"
#include "third_party/blink/renderer/core/layout/physical_box_fragment.h"
#include "third_party/blink/renderer/core/paint/inline_paint_context.h"

namespace blink {
class HTMLBRElement;

namespace {

bool IsBidiControl(StringView string) {
  return string.length() == 1 && Character::IsBidiControl(string[0]);
}

LogicalRect ExpandedSelectionRectForSoftLineBreakIfNeeded(
    const LogicalRect& rect,
    const InlineCursor& cursor,
    const LayoutSelectionStatus& selection_status) {
  // Expand paint rect if selection covers multiple lines and
  // this fragment is at the end of line.
  if (selection_status.line_break == SelectSoftLineBreak::kNotSelected)
    return rect;
  const LayoutBlockFlow* const layout_block_flow = cursor.GetLayoutBlockFlow();
  if (layout_block_flow && layout_block_flow->ShouldTruncateOverflowingText())
    return rect;
  // Copy from InlineTextBoxPainter::PaintSelection.
  const LayoutUnit space_width(cursor.Current().Style().GetFont().SpaceWidth());
  return {rect.offset,
          {rect.size.inline_size + space_width, rect.size.block_size}};
}

// Expands selection height so that the selection rect fills entire line.
LogicalRect ExpandSelectionRectToLineHeight(
    const LogicalRect& rect,
    const LogicalRect& line_logical_rect) {
  // Unite the rect only in the block direction.
  const LayoutUnit selection_top =
      std::min(rect.offset.block_offset, line_logical_rect.offset.block_offset);
  const LayoutUnit selection_bottom =
      std::max(rect.BlockEndOffset(), line_logical_rect.BlockEndOffset());
  return {{rect.offset.inline_offset, selection_top},
          {rect.size.inline_size, selection_bottom - selection_top}};
}

LogicalRect ExpandSelectionRectToLineHeight(const LogicalRect& rect,
                                            const InlineCursor& cursor) {
  InlineCursor line(cursor);
  line.MoveToContainingLine();
  const PhysicalRect line_physical_rect(
      line.Current().OffsetInContainerFragment() -
          cursor.Current().OffsetInContainerFragment(),
      line.Current().Size());
  return ExpandSelectionRectToLineHeight(
      rect, cursor.Current().ConvertChildToLogical(line_physical_rect));
}

bool IsLastBRInPage(const LayoutObject& layout_object) {
  return layout_object.IsBR() && !layout_object.NextInPreOrder();
}

bool ShouldIgnoreForPositionForPoint(const FragmentItem& item) {
  switch (item.Type()) {
    case FragmentItem::kBox:
      if (auto* box_fragment = item.BoxFragment()) {
        if (box_fragment->IsInlineBox()) {
          // We ignore inline box to avoid to call |PositionForPointInChild()|
          // with empty inline box, e.g. <div>ab<b></b></div>.
          // // All/LayoutViewHitTestTest.EmptySpan needs this.
          return true;
        }
        if (box_fragment->IsBlockInInline()) {
          // "label-contains-other-interactive-content.html" reaches here.
          return false;
        }
        // Skip pseudo element ::before/::after
        // All/LayoutViewHitTestTest.PseudoElementAfter* needs this.
        return !item.GetLayoutObject()->NonPseudoNode();
      }
      // Skip virtually "culled" inline box, e.g. <span>foo</span>
      // "editing/selection/shift-click.html" reaches here.
      DCHECK(item.GetLayoutObject()->IsLayoutInline()) << item;
      return true;
    case FragmentItem::kGeneratedText:
      return true;
    case FragmentItem::kText:
      if (item.IsLayoutObjectDestroyedOrMoved()) [[unlikely]] {
        // See http://crbug.com/1217079
        NOTREACHED_IN_MIGRATION() << item;
        return true;
      }
      // Returns true when |item.GetLayoutObject().IsStyleGenerated()|.
      // All/LayoutViewHitTestTest.PseudoElementAfter* needs this.
      return item.IsGeneratedText();
    case FragmentItem::kLine:
      DCHECK(RuntimeEnabledFeatures::RubyLineBreakableEnabled());
      return true;
    case FragmentItem::kInvalid:
      NOTREACHED_IN_MIGRATION();
      break;
  }
  return false;
}

bool ShouldIgnoreForPositionForPoint(const InlineCursor& line) {
  if (line.CurrentItem()->Type() != FragmentItem::kLine) {
    return false;
  }
  for (auto cursor = line.CursorForDescendants(); cursor; cursor.MoveToNext()) {
    if (cursor.CurrentItem()->IsBlockInInline()) {
      // We should enter block-in-inline. Following tests require this:
      //  * editing/pasteboard/paste-sanitize-crash-2.html
      //  * editing/selection/click-after-nested-block.html
      return false;
    }
    // See also |InlineCursor::TryMoveToFirstInlineLeafChild()|.
    if (cursor.Current().IsInlineLeaf())
      return false;
  }
  // There are no block-in-inline and inline leaf.
  // Note: editing/selection/last-empty-inline.html requires this to skip
  // empty <span> with padding.
  return true;
}

}  // namespace

inline void InlineCursor::MoveToItem(const ItemsSpan::iterator& iter) {
  DCHECK(HasRoot());
  DCHECK(iter >= items_.begin() && iter <= items_.end());
  if (iter != items_.end()) {
    current_.Set(iter);
    return;
  }
  MakeNull();
}

void InlineCursor::SetRoot(const PhysicalBoxFragment& box_fragment,
                           const FragmentItems& fragment_items,
                           ItemsSpan items) {
  DCHECK_EQ(box_fragment.Items(), &fragment_items);
  DCHECK(items.data() || !items.size());
  root_box_fragment_ = &box_fragment;
  fragment_items_ = &fragment_items;
  items_ = items;
  DCHECK(fragment_items_->IsSubSpan(items_));
  MoveToItem(items_.begin());
}

void InlineCursor::SetRoot(const PhysicalBoxFragment& box_fragment,
                           const FragmentItems& items) {
  SetRoot(box_fragment, items, items.Items());
}

bool InlineCursor::TrySetRootFragmentItems() {
  DCHECK(root_block_flow_);
  DCHECK(!fragment_items_ || fragment_items_->Equals(items_));
  if (!root_block_flow_->MayHaveFragmentItems()) [[unlikely]] {
#if EXPENSIVE_DCHECKS_ARE_ON()
    DCHECK(!root_block_flow_->PhysicalFragments().SlowHasFragmentItems());
#endif
    fragment_index_ = max_fragment_index_ + 1;
    return false;
  }
  for (; fragment_index_ <= max_fragment_index_; IncrementFragmentIndex()) {
    const PhysicalBoxFragment* fragment =
        root_block_flow_->GetPhysicalFragment(fragment_index_);
    DCHECK(fragment);
    if (const FragmentItems* items = fragment->Items()) {
      SetRoot(*fragment, *items);
      return true;
    }
  }
  return false;
}

void InlineCursor::SetRoot(const LayoutBlockFlow& block_flow) {
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

InlineCursor::InlineCursor(const LayoutBlockFlow& block_flow) {
  SetRoot(block_flow);
}

InlineCursor::InlineCursor(const PhysicalBoxFragment& box_fragment,
                           const FragmentItems& fragment_items,
                           ItemsSpan items) {
  SetRoot(box_fragment, fragment_items, items);
}

InlineCursor::InlineCursor(const PhysicalBoxFragment& box_fragment,
                           const FragmentItems& items) {
  SetRoot(box_fragment, items);
}

InlineCursor::InlineCursor(const PhysicalBoxFragment& box_fragment) {
  if (const FragmentItems* items = box_fragment.Items()) {
    SetRoot(box_fragment, *items);
  }
}

InlineCursor::InlineCursor(const InlineBackwardCursor& backward_cursor)
    : InlineCursor(backward_cursor.cursor_) {
  MoveTo(backward_cursor.Current());
}

bool InlineCursor::operator==(const InlineCursor& other) const {
  if (current_.item_ != other.current_.item_)
    return false;
  DCHECK_EQ(items_.data(), other.items_.data());
  DCHECK_EQ(items_.size(), other.items_.size());
  DCHECK_EQ(fragment_items_, other.fragment_items_);
  DCHECK(current_.item_iter_ == other.current_.item_iter_);
  return true;
}

const LayoutBlockFlow* InlineCursor::GetLayoutBlockFlow() const {
  DCHECK_EQ(HasRoot(), !!root_box_fragment_);
  if (root_box_fragment_) {
    const LayoutObject* layout_object =
        root_box_fragment_->GetSelfOrContainerLayoutObject();
    DCHECK(layout_object);
    DCHECK(!layout_object->IsLayoutFlowThread());
    return To<LayoutBlockFlow>(layout_object);
  }
  NOTREACHED_IN_MIGRATION();
  return nullptr;
}

bool InlineCursorPosition::HasChildren() const {
  if (item_)
    return item_->HasChildren();
  NOTREACHED_IN_MIGRATION();
  return false;
}

InlineCursor InlineCursor::CursorForDescendants() const {
  if (current_.item_) {
    unsigned descendants_count = current_.item_->DescendantsCount();
    if (descendants_count > 1) {
      DCHECK(root_box_fragment_);
      DCHECK(fragment_items_);
      return InlineCursor(
          *root_box_fragment_, *fragment_items_,
          ItemsSpan(&*(current_.item_iter_ + 1), descendants_count - 1));
    }
    return InlineCursor();
  }
  NOTREACHED_IN_MIGRATION();
  return InlineCursor();
}

InlineCursor InlineCursor::CursorForMovingAcrossFragmentainer() const {
  DCHECK(IsNotNull());
  if (IsBlockFragmented())
    return *this;
  InlineCursor cursor(*GetLayoutBlockFlow());
  const auto& item = *CurrentItem();
  while (cursor && !cursor.TryMoveTo(item))
    cursor.MoveToNextFragmentainer();
  DCHECK(cursor) << *this;
  return cursor;
}

void InlineCursor::ExpandRootToContainingBlock() {
  if (fragment_items_) {
    const unsigned index_diff = base::checked_cast<unsigned>(
        items_.data() - fragment_items_->Items().data());
    DCHECK_LT(index_diff, fragment_items_->Items().size());
    const unsigned item_index =
        base::checked_cast<unsigned>(current_.item_iter_ - items_.begin());
    items_ = fragment_items_->Items();
    // Update the iterator to the one for the new span.
    MoveToItem(items_.begin() + item_index + index_diff);
    return;
  }
  NOTREACHED_IN_MIGRATION();
}

bool InlineCursorPosition::HasSoftWrapToNextLine() const {
  DCHECK(IsLineBox());
  const InlineBreakToken* break_token = GetInlineBreakToken();
  return break_token && !break_token->IsForcedBreak();
}

bool InlineCursorPosition::IsInlineLeaf() const {
  if (IsHiddenForPaint()) {
    return false;
  }
  if (IsText()) {
    return !IsLayoutGeneratedText();
  }
  if (IsAtomicInline()) {
    return !IsListMarker();
  }
  return false;
}

bool InlineCursorPosition::IsPartOfCulledInlineBox(
    const LayoutInline& layout_inline) const {
  DCHECK(!layout_inline.ShouldCreateBoxFragment());
  DCHECK(*this);
  const LayoutObject* const layout_object = GetLayoutObject();
  // We use |IsInline()| to exclude floating and out-of-flow objects.
  if (!layout_object || layout_object->IsAtomicInlineLevel())
    return false;
  // When |Current()| is block-in-inline, e.g. <span><div>foo</div></span>, it
  // should be part of culled inline box[1].
  // [1]
  // external/wpt/shadow-dom/DocumentOrShadowRoot-prototype-elementFromPoint.html
  if (!layout_object->IsInline() && !layout_object->IsBlockInInline())
    return false;
  DCHECK(!layout_object->IsFloatingOrOutOfFlowPositioned());
  DCHECK(!BoxFragment() || !BoxFragment()->IsFormattingContextRoot());
  for (const LayoutObject* parent = layout_object->Parent(); parent;
       parent = parent->Parent()) {
    // Children of culled inline should be included.
    if (parent == &layout_inline)
      return true;
    // Grand children should be included only if children are also culled.
    if (const auto* parent_layout_inline = DynamicTo<LayoutInline>(parent)) {
      if (!parent_layout_inline->ShouldCreateBoxFragment())
        continue;
    }
    return false;
  }
  return false;
}

bool InlineCursor::IsLastLineInInlineBlock() const {
  DCHECK(Current().IsLineBox());
  if (!GetLayoutBlockFlow()->IsAtomicInlineLevel())
    return false;
  InlineCursor next_sibling(*this);
  for (;;) {
    next_sibling.MoveToNextSkippingChildren();
    if (!next_sibling)
      return true;
    if (next_sibling.Current().IsLineBox())
      return false;
    // There maybe other top-level objects such as floats, OOF, or list-markers.
  }
}

bool InlineCursor::IsBeforeSoftLineBreak() const {
  if (Current().IsLineBreak())
    return false;
  // Inline block is not be container line box.
  // See paint/selection/text-selection-inline-block.html.
  InlineCursor line(*this);
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
  InlineCursor last_leaf(line);
  last_leaf.MoveToLastLogicalLeaf();
  if (last_leaf != *this)
    return false;
  // Even If |fragment| is before linebreak, if its direction differs to line
  // direction, we don't paint line break. See
  // paint/selection/text-selection-newline-mixed-ltr-rtl.html.
  return line.Current().BaseDirection() == Current().ResolvedDirection();
}

bool InlineCursorPosition::CanHaveChildren() const {
  if (item_) {
    return item_->Type() == FragmentItem::kLine ||
           (item_->Type() == FragmentItem::kBox && !item_->IsAtomicInline());
  }
  NOTREACHED_IN_MIGRATION();
  return false;
}

TextDirection InlineCursorPosition::BaseDirection() const {
  DCHECK(IsLineBox());
  if (item_)
    return item_->BaseDirection();
  NOTREACHED_IN_MIGRATION();
  return TextDirection::kLtr;
}

UBiDiLevel InlineCursorPosition::BidiLevel() const {
  if (IsText()) {
    if (IsLayoutGeneratedText()) {
      // TODO(yosin): Until we have clients, we don't support bidi-level for
      // ellipsis and soft hyphens. crbug.com/1423660
      return 0;
    }
    const auto& layout_text = *To<LayoutText>(GetLayoutObject());
    DCHECK(!layout_text.NeedsLayout()) << this;
    const auto* const items = layout_text.GetInlineItems();
    if (!items || items->size() == 0) {
      // In case of <br>, <wbr>, text-combine-upright, etc.
      return 0;
    }
    const TextOffsetRange offset = TextOffset();
    auto* const item =
        base::ranges::find_if(*items, [offset](const InlineItem& item) {
          return item.StartOffset() <= offset.start &&
                 item.EndOffset() >= offset.end;
        });
    CHECK(item != items->end(), base::NotFatalUntil::M130) << this;
    return item->BidiLevel();
  }

  if (IsAtomicInline()) {
    DCHECK(GetLayoutObject()->FragmentItemsContainer());
    const LayoutBlockFlow& block_flow =
        *GetLayoutObject()->FragmentItemsContainer();
    const auto& items =
        block_flow.GetInlineNodeData()->ItemsData(UsesFirstLineStyle()).items;
    const auto item = base::ranges::find(items, GetLayoutObject(),
                                         &InlineItem::GetLayoutObject);
    CHECK(item != items.end(), base::NotFatalUntil::M130) << this;
    return item->BidiLevel();
  }

  NOTREACHED_IN_MIGRATION();
  return 0;
}

const DisplayItemClient* InlineCursorPosition::GetSelectionDisplayItemClient()
    const {
  if (const auto* client = GetLayoutObject()->GetSelectionDisplayItemClient())
    return client;
  return GetDisplayItemClient();
}

const Node* InlineCursorPosition::GetNode() const {
  if (const LayoutObject* layout_object = GetLayoutObject())
    return layout_object->GetNode();
  return nullptr;
}

gfx::RectF InlineCursorPosition::ObjectBoundingBox(
    const InlineCursor& cursor) const {
  return item_->ObjectBoundingBox(cursor.Items());
}

void InlineCursorPosition::RecalcInkOverflow(
    const InlineCursor& cursor,
    InlinePaintContext* inline_context) const {
  DCHECK(item_);
  DCHECK_EQ(item_, cursor.Current().Item());
  PhysicalRect self_and_contents_rect;
  item_->GetMutableForPainting().RecalcInkOverflow(cursor, inline_context,
                                                   &self_and_contents_rect);
}

StringView InlineCursorPosition::Text(const InlineCursor& cursor) const {
  DCHECK(IsText());
  cursor.CheckValid(*this);
  if (item_)
    return item_->Text(cursor.Items());
  NOTREACHED_IN_MIGRATION();
  return "";
}

PhysicalRect InlineCursor::CurrentLocalRect(unsigned start_offset,
                                            unsigned end_offset) const {
  DCHECK(Current().IsText());
  if (current_.item_) {
    return current_.item_->LocalRect(current_.item_->Text(*fragment_items_),
                                     start_offset, end_offset);
  }
  NOTREACHED_IN_MIGRATION();
  return PhysicalRect();
}

PhysicalRect InlineCursor::CurrentLocalSelectionRectForText(
    const LayoutSelectionStatus& selection_status) const {
  const PhysicalRect selection_rect =
      CurrentLocalRect(selection_status.start, selection_status.end);
  LogicalRect logical_rect = Current().ConvertChildToLogical(selection_rect);
  if (Current()->IsSvgText()) {
    return Current().ConvertChildToPhysical(logical_rect);
  }
  // Let LocalRect for line break have a space width to paint line break
  // when it is only character in a line or only selected in a line.
  if (selection_status.start != selection_status.end &&
      Current().IsLineBreak() &&
      // This is for old compatible that old doesn't paint last br in a page.
      !IsLastBRInPage(*Current().GetLayoutObject())) {
    logical_rect.size.inline_size =
        LayoutUnit(Current().Style().GetFont().SpaceWidth());
  }
  const LogicalRect line_break_extended_rect =
      Current().IsLineBreak() ? logical_rect
                              : ExpandedSelectionRectForSoftLineBreakIfNeeded(
                                    logical_rect, *this, selection_status);
  const LogicalRect line_height_expanded_rect =
      ExpandSelectionRectToLineHeight(line_break_extended_rect, *this);
  const PhysicalRect physical_rect =
      Current().ConvertChildToPhysical(line_height_expanded_rect);
  return physical_rect;
}

PhysicalRect InlineCursor::CurrentLocalSelectionRectForReplaced() const {
  DCHECK(Current().GetLayoutObject()->IsLayoutReplaced());
  const PhysicalRect selection_rect = PhysicalRect({}, Current().Size());
  LogicalRect logical_rect = Current().ConvertChildToLogical(selection_rect);
  const LogicalRect line_height_expanded_rect =
      ExpandSelectionRectToLineHeight(logical_rect, *this);
  const PhysicalRect physical_rect =
      Current().ConvertChildToPhysical(line_height_expanded_rect);
  return physical_rect;
}

PhysicalRect InlineCursor::CurrentRectInBlockFlow() const {
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

LayoutUnit InlineCursor::CaretInlinePositionForOffset(unsigned offset) const {
  DCHECK(Current().IsText());
  if (current_.item_) {
    return current_.item_->CaretInlinePositionForOffset(
        current_.item_->Text(*fragment_items_), offset);
  }
  NOTREACHED_IN_MIGRATION();
  return LayoutUnit();
}

LogicalRect InlineCursorPosition::ConvertChildToLogical(
    const PhysicalRect& physical_rect) const {
  return WritingModeConverter(
             {Style().GetWritingMode(), ResolvedOrBaseDirection()}, Size())
      .ToLogical(physical_rect);
}

PhysicalRect InlineCursorPosition::ConvertChildToPhysical(
    const LogicalRect& logical_rect) const {
  return WritingModeConverter(
             {Style().GetWritingMode(), ResolvedOrBaseDirection()}, Size())
      .ToPhysical(logical_rect);
}

PositionWithAffinity InlineCursor::PositionForPointInInlineFormattingContext(
    const PhysicalOffset& point,
    const PhysicalBoxFragment& container) {
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
  InlineCursorPosition closest_line_after;
  LayoutUnit closest_line_after_block_offset = LayoutUnit::Min();

  // Stores the closest line box child before |point| in the block direction.
  // Used if we can't find any child |point| falls in to resolve the position.
  InlineCursorPosition closest_line_before;
  LayoutUnit closest_line_before_block_offset = LayoutUnit::Max();

  while (*this) {
    const FragmentItem* child_item = CurrentItem();
    DCHECK(child_item);
    if (child_item->Type() == FragmentItem::kLine) {
      if (ShouldIgnoreForPositionForPoint(*this)) {
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
    DCHECK_NE(child_item->Type(), FragmentItem::kText);
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

PositionWithAffinity InlineCursor::PositionForPointInInlineBox(
    const PhysicalOffset& point_in) const {
  const FragmentItem* container = CurrentItem();
  DCHECK(container);
  DCHECK(container->Type() == FragmentItem::kLine ||
         container->Type() == FragmentItem::kBox);
  const auto* const text_combine =
      DynamicTo<LayoutTextCombine>(container->GetLayoutObject());
  PhysicalOffset point;
  if (text_combine) [[unlikely]] {
    point = text_combine->AdjustOffsetForHitTest(point_in);
  } else {
    point = point_in;
  }
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
  InlineCursorPosition closest_child_before;
  LayoutUnit closest_child_before_inline_offset = LayoutUnit::Min();

  // Stores the closest child after |point| in the inline direction. Used if we
  // can't find any child |point| falls in to resolve the position.
  InlineCursorPosition closest_child_after;
  LayoutUnit closest_child_after_inline_offset = LayoutUnit::Max();

  InlineCursor descendants = CursorForDescendants();
  for (; descendants; descendants.MoveToNext()) {
    const FragmentItem* child_item = descendants.CurrentItem();
    DCHECK(child_item);
    if (ShouldIgnoreForPositionForPoint(*child_item))
      continue;
    const LayoutUnit child_inline_offset =
        child_item->OffsetInContainerFragment()
            .ConvertToLogical(writing_direction, container_size,
                              child_item->Size())
            .inline_offset;
    if (point_inline_offset < child_inline_offset) {
      if (child_item->IsFloating())
        continue;
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
    if (point_inline_offset >= child_inline_end_offset) {
      if (child_item->IsFloating())
        continue;
      if (child_inline_end_offset > closest_child_before_inline_offset) {
        closest_child_before_inline_offset = child_inline_end_offset;
        closest_child_before = descendants.Current();
      }
      continue;
    }

    // |point_inline_offset| is in |child_item|.
    if (const PositionWithAffinity child_position =
            descendants.PositionForPointInChild(point))
      return child_position;
  }

  // Note: We don't snap a point before/after of "float" to "float",
  // |closest_child_after| and |closest_child_before| can not be a box for
  // "float".
  // Note: Float boxes are appeared in |FragmentItems| as DOM order, so,
  // "float:right" can be placed anywhere instead of at end of items.
  // See LayoutViewHitTest.Float{Left,Right}*
  if (closest_child_after) {
    descendants.MoveTo(closest_child_after);
    if (const PositionWithAffinity child_position =
            descendants.PositionForPointInChild(point))
      return child_position;
    if (closest_child_after->BoxFragment()) {
      DCHECK(!closest_child_after->IsFloating());
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
      DCHECK(!closest_child_before->IsFloating());
      // LayoutViewHitTest.HitTestHorizontal "Top-right corner (outside) of div"
      // reach here.
      if (const PositionWithAffinity child_position =
              descendants.PositionForPointInInlineBox(point))
        return child_position;
    }
  }

  return PositionWithAffinity();
}

PositionWithAffinity InlineCursor::PositionForPointInChild(
    const PhysicalOffset& point_in_container) const {
  DCHECK(CurrentItem());
  const FragmentItem& child_item = *CurrentItem();
  switch (child_item.Type()) {
    case FragmentItem::kText:
      return child_item.PositionForPointInText(
          point_in_container - child_item.OffsetInContainerFragment(), *this);
    case FragmentItem::kGeneratedText:
      break;
    case FragmentItem::kBox:
      if (const PhysicalBoxFragment* box_fragment = child_item.BoxFragment()) {
        if (!box_fragment->IsInlineBox()) {
          // In case of inline block with with block formatting context that
          // has block children[1].
          // Example: <b style="display:inline-block"><div>b</div></b>
          // [1] InlineCursorTest.PositionForPointInChildBlockChildren
          return child_item.GetLayoutObject()->PositionForPoint(
              point_in_container - child_item.OffsetInContainerFragment());
        }
      } else {
        // |LayoutInline| used to be culled.
      }
      DCHECK(child_item.GetLayoutObject()->IsLayoutInline()) << child_item;
      break;
    case FragmentItem::kLine:
    case FragmentItem::kInvalid:
      NOTREACHED_IN_MIGRATION();
      break;
  }
  return PositionWithAffinity();
}

PositionWithAffinity InlineCursor::PositionForPointInText(
    unsigned text_offset) const {
  DCHECK(Current().IsText()) << this;
  if (HasRoot())
    return Current()->PositionForPointInText(text_offset, *this);
  return PositionWithAffinity();
}

PositionWithAffinity InlineCursor::PositionForStartOfLine() const {
  DCHECK(Current().IsLineBox());
  InlineCursor first_leaf = CursorForDescendants();
  if (IsLtr(Current().BaseDirection()))
    first_leaf.MoveToFirstNonPseudoLeaf();
  else
    first_leaf.MoveToLastNonPseudoLeaf();
  if (!first_leaf)
    return PositionWithAffinity();
  const auto& layout_object = first_leaf.Current()->IsBlockInInline()
                                  ? first_leaf.Current()->BlockInInline()
                                  : *first_leaf.Current().GetLayoutObject();
  Node* const node = layout_object.NonPseudoNode();
  if (!node) {
    NOTREACHED_IN_MIGRATION()
        << "MoveToFirstLeaf returns invalid node: " << first_leaf;
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

PositionWithAffinity InlineCursor::PositionForEndOfLine() const {
  DCHECK(Current().IsLineBox());
  InlineCursor last_leaf = CursorForDescendants();
  if (IsLtr(Current().BaseDirection()))
    last_leaf.MoveToLastNonPseudoLeaf();
  else
    last_leaf.MoveToFirstNonPseudoLeaf();
  if (!last_leaf)
    return PositionWithAffinity();
  const auto& layout_object = last_leaf.Current()->IsBlockInInline()
                                  ? last_leaf.Current()->BlockInInline()
                                  : *last_leaf.Current().GetLayoutObject();
  Node* const node = layout_object.NonPseudoNode();
  if (!node) {
    NOTREACHED_IN_MIGRATION()
        << "MoveToLastLeaf returns invalid node: " << last_leaf;
    return PositionWithAffinity();
  }
  if (IsA<HTMLBRElement>(node))
    return PositionWithAffinity(Position::BeforeNode(*node));
  if (!IsA<Text>(node))
    return PositionWithAffinity(Position::AfterNode(*node));
  const wtf_size_t text_offset = GetTextOffsetForEndOfLine(last_leaf);
  return last_leaf.PositionForPointInText(text_offset);
}

inline wtf_size_t InlineCursor::GetTextOffsetForEndOfLine(
    InlineCursor& last_leaf) const {
  wtf_size_t text_offset = last_leaf.Current().TextOffset().start;
  if (Current().BaseDirection() == last_leaf.Current().ResolvedDirection() &&
      !last_leaf.Current().IsLineBreak()) {
    text_offset = last_leaf.Current().TextOffset().end;
  }
  return text_offset;
}

void InlineCursor::MoveTo(const InlineCursorPosition& position) {
  CheckValid(position);
  current_ = position;
}

inline wtf_size_t InlineCursor::SpanBeginItemIndex() const {
  DCHECK(HasRoot());
  DCHECK(!items_.empty());
  DCHECK(fragment_items_->IsSubSpan(items_));
  const wtf_size_t delta = base::checked_cast<wtf_size_t>(
      items_.data() - fragment_items_->Items().data());
  DCHECK_LT(delta, fragment_items_->Items().size());
  return delta;
}

inline wtf_size_t InlineCursor::SpanIndexFromItemIndex(unsigned index) const {
  DCHECK(HasRoot());
  DCHECK(!items_.empty());
  DCHECK(fragment_items_->IsSubSpan(items_));
  if (items_.data() == fragment_items_->Items().data())
    return index;
  const wtf_size_t span_index = base::checked_cast<wtf_size_t>(
      fragment_items_->Items().data() - items_.data() + index);
  DCHECK_LT(span_index, items_.size());
  return span_index;
}

void InlineCursor::MoveTo(const FragmentItem& fragment_item) {
  if (TryMoveTo(fragment_item))
    return;
  NOTREACHED_IN_MIGRATION() << *this << " " << fragment_item;
}

bool InlineCursor::TryMoveTo(const FragmentItem& fragment_item) {
  DCHECK(HasRoot());
  // Note: We use address instead of iterator because we can't compare
  // iterators in different span. See |base::CheckedContiguousIterator<T>|.
  const ptrdiff_t index = &fragment_item - &*items_.begin();
  if (index < 0 || static_cast<size_t>(index) >= items_.size())
    return false;
  MoveToItem(items_.begin() + index);
  return true;
}

void InlineCursor::MoveTo(const InlineCursor& cursor) {
  if (cursor.current_.item_) {
    if (!fragment_items_)
      SetRoot(*cursor.root_box_fragment_, *cursor.fragment_items_);
    return MoveTo(*cursor.current_.item_);
  }
  *this = cursor;
}

void InlineCursor::MoveToParent() {
  wtf_size_t count = 0;
  if (!Current()) [[unlikely]] {
    return;
  }
  for (;;) {
    MoveToPrevious();
    if (!Current())
      return;
    ++count;
    if (Current()->DescendantsCount() > count)
      return;
  }
}

void InlineCursor::MoveToContainingLine() {
  DCHECK(!Current().IsLineBox());
  if (current_.item_) {
    while (current_.item_ && !Current().IsLineBox())
      MoveToPrevious();
    return;
  }
  NOTREACHED_IN_MIGRATION();
}

bool InlineCursor::IsAtFirst() const {
  if (const FragmentItem* item = Current().Item()) {
    return item == &items_.front();
  }
  return false;
}

void InlineCursor::MoveToFirst() {
  if (HasRoot()) {
    MoveToItem(items_.begin());
    return;
  }
  NOTREACHED_IN_MIGRATION();
}

void InlineCursor::MoveToFirstChild() {
  DCHECK(Current().CanHaveChildren());
  if (!TryMoveToFirstChild())
    MakeNull();
}

void InlineCursor::MoveToFirstLine() {
  if (HasRoot()) {
    auto iter =
        base::ranges::find(items_, FragmentItem::kLine, &FragmentItem::Type);
    if (iter != items_.end()) {
      MoveToItem(iter);
      return;
    }
    MakeNull();
    return;
  }
  NOTREACHED_IN_MIGRATION();
}

void InlineCursor::MoveToFirstLogicalLeaf() {
  DCHECK(Current().IsLineBox());
  // TODO(yosin): This isn't correct for mixed Bidi. Fix it. Besides, we
  // should compute and store it during layout.
  // TODO(yosin): We should check direction of each container instead of line
  // box.
  if (IsLtr(Current().Style().Direction())) {
    while (TryMoveToFirstChild())
      continue;
    return;
  }
  while (TryMoveToLastChild())
    continue;
}

void InlineCursor::MoveToFirstNonPseudoLeaf() {
  for (InlineCursor cursor = *this; cursor; cursor.MoveToNext()) {
    if (cursor.Current().IsLineBox())
      continue;
    if (cursor.Current()->IsBlockInInline()) {
      if (cursor.Current()->BlockInInline().NonPseudoNode()) {
        *this = cursor;
        return;
      }
      continue;
    }
    if (!cursor.Current().GetLayoutObject()->NonPseudoNode())
      continue;
    if (cursor.Current().IsText()) {
      // Note: We should not skip bidi control only text item to return
      // position after bibi control character, e.g.
      // <p dir=rtl>&#x202B;xyz ABC.&#x202C;</p>
      // See "editing/selection/home-end.html".
      DCHECK(!cursor.Current().IsLayoutGeneratedText()) << cursor;
      if (cursor.Current().IsLineBreak()) {
        // We ignore line break character, e.g. newline with white-space:pre,
        // like |MoveToLastNonPseudoLeaf()| as consistency.
        // See |ParameterizedVisibleUnitsLineTest.EndOfLineWithWhiteSpacePre|
        auto next = cursor;
        next.MoveToNext();
        if (next)
          continue;
      }
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

void InlineCursor::MoveToLastChild() {
  DCHECK(Current().CanHaveChildren());
  if (!TryMoveToLastChild())
    MakeNull();
}

void InlineCursor::MoveToLastLine() {
  DCHECK(HasRoot());
  auto iter = base::ranges::find(base::Reversed(items_), FragmentItem::kLine,
                                 &FragmentItem::Type);
  if (iter != items_.rend())
    MoveToItem(std::next(iter).base());
  else
    MakeNull();
}

void InlineCursor::MoveToLastLogicalLeaf() {
  DCHECK(Current().IsLineBox());
  // TODO(yosin): This isn't correct for mixed Bidi. Fix it. Besides, we
  // should compute and store it during layout.
  // TODO(yosin): We should check direction of each container instead of line
  // box.
  if (IsLtr(Current().Style().Direction())) {
    while (TryMoveToLastChild())
      continue;
    return;
  }
  while (TryMoveToFirstChild())
    continue;
}

void InlineCursor::MoveToLastNonPseudoLeaf() {
  // TODO(yosin): We should introduce |IsTruncated()| to avoid to use
  // |in_hidden_for_paint|. See also |LayoutText::GetTextBoxInfo()|.
  // When "text-overflow:ellipsis" specified, items usually are:
  //  [i+0] original non-truncated text (IsHiddenForPaint()=true)
  //  [i+1] truncated text
  //  [i+2] ellipsis (IsLayoutGeneratedText())
  // But this is also possible:
  //  [i+0] atomic inline box
  //  [i+1] ellipsis (IsLayoutGeneratedText())
  InlineCursor last_leaf;
  bool in_hidden_for_paint = false;
  for (InlineCursor cursor = *this; cursor; cursor.MoveToNext()) {
    if (cursor.Current().IsLineBox())
      continue;
    if (cursor.Current()->IsBlockInInline()) {
      if (cursor.Current()->BlockInInline().NonPseudoNode())
        last_leaf = cursor;
      continue;
    }
    if (!cursor.Current().GetLayoutObject()->NonPseudoNode())
      continue;
    if (cursor.Current().IsLineBreak() && last_leaf)
      break;
    if (cursor.Current().IsText()) {
      if (cursor.Current().IsLayoutGeneratedText()) {
        // |cursor| is at ellipsis.
        break;
      }
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

void InlineCursor::MoveToNextInlineLeaf() {
  if (Current() && Current().IsInlineLeaf())
    MoveToNext();
  while (Current() && !Current().IsInlineLeaf())
    MoveToNext();
}

void InlineCursor::MoveToNextInlineLeafIgnoringLineBreak() {
  do {
    MoveToNextInlineLeaf();
  } while (Current() && Current().IsLineBreak());
}

void InlineCursor::MoveToNextInlineLeafOnLine() {
  MoveToLastForSameLayoutObject();
  if (IsNull())
    return;
  InlineCursor last_item = *this;
  MoveToContainingLine();
  InlineCursor cursor = CursorForDescendants();
  cursor.MoveTo(last_item);
  // Note: AX requires this for AccessibilityLayoutTest.NextOnLine.
  // If the cursor is on a container, move to the next content
  // not within the container.
  if (cursor.Current().IsInlineLeaf()) {
    cursor.MoveToNextInlineLeaf();
  } else {
    // Skip over descendants.
    cursor.MoveToNextSkippingChildren();  // Skip over descendants.
    // Ensure that a leaf is returned.
    if (cursor.Current() && !cursor.Current().IsInlineLeaf())
      cursor.MoveToNextInlineLeaf();
  }
  MoveTo(cursor);
  DCHECK(!cursor.Current() || cursor.Current().IsInlineLeaf())
      << "Must return an empty or inline leaf position, returned: "
      << cursor.CurrentMutableLayoutObject();
}

void InlineCursor::MoveToNextLine() {
  DCHECK(Current().IsLineBox());
  if (current_.item_) {
    do {
      MoveToNextSkippingChildren();
    } while (Current() && !Current().IsLineBox());
    return;
  }
  NOTREACHED_IN_MIGRATION();
}

void InlineCursor::MoveToNextLineIncludingFragmentainer() {
  MoveToNextLine();
  if (!Current() && max_fragment_index_ && CanMoveAcrossFragmentainer()) {
    MoveToNextFragmentainer();
    if (Current() && !Current().IsLineBox())
      MoveToFirstLine();
  }
}

void InlineCursor::MoveToPreviousInlineLeaf() {
  if (Current() && Current().IsInlineLeaf())
    MoveToPrevious();
  while (Current() && !Current().IsInlineLeaf())
    MoveToPrevious();
}

void InlineCursor::MoveToPreviousInlineLeafIgnoringLineBreak() {
  do {
    MoveToPreviousInlineLeaf();
  } while (Current() && Current().IsLineBreak());
}

void InlineCursor::MoveToPreviousInlineLeafOnLine() {
  if (IsNull())
    return;
  InlineCursor first_item = *this;
  MoveToContainingLine();
  InlineCursor cursor = CursorForDescendants();
  cursor.MoveTo(first_item);
  cursor.MoveToPreviousInlineLeaf();
  MoveTo(cursor);
}

void InlineCursor::MoveToPreviousLine() {
  // Note: List marker is sibling of line box.
  DCHECK(Current().IsLineBox());
  if (current_.item_) {
    do {
      MoveToPrevious();
    } while (Current() && !Current().IsLineBox());
    return;
  }
  NOTREACHED_IN_MIGRATION();
}

bool InlineCursor::TryMoveToFirstChild() {
  if (!Current().HasChildren())
    return false;
  MoveToItem(current_.item_iter_ + 1);
  return true;
}

bool InlineCursor::TryMoveToFirstInlineLeafChild() {
  while (IsNotNull()) {
    if (Current().IsInlineLeaf())
      return true;
    MoveToNext();
  }
  return false;
}

bool InlineCursor::TryMoveToLastChild() {
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

void InlineCursor::MoveToNext() {
  DCHECK(HasRoot());
  if (!current_.item_) [[unlikely]] {
    return;
  }
  // Expensive DCHECK as MoveToNext() is called frequently.
  DCHECK(current_.item_iter_ != items_.end());
  if (++current_.item_iter_ != items_.end()) {
    current_.item_ = &*current_.item_iter_;
    return;
  }
  MakeNull();
}

void InlineCursor::MoveToNextSkippingChildren() {
  DCHECK(HasRoot());
  if (!current_.item_) [[unlikely]] {
    return;
  }
  // If the current item has |DescendantsCount|, add it to move to the next
  // sibling, skipping all children and their descendants.
  if (wtf_size_t descendants_count = current_.item_->DescendantsCount())
    return MoveToItem(current_.item_iter_ + descendants_count);
  return MoveToNext();
}

void InlineCursor::MoveToPrevious() {
  DCHECK(HasRoot());
  if (!current_.item_) [[unlikely]] {
    return;
  }
  if (current_.item_iter_ == items_.begin())
    return MakeNull();
  --current_.item_iter_;
  current_.item_ = &*current_.item_iter_;
}

void InlineCursor::MoveToPreviousFragmentainer() {
  DCHECK(CanMoveAcrossFragmentainer());
  if (fragment_index_) {
    DecrementFragmentIndex();
    if (TrySetRootFragmentItems()) {
      MoveToItem(items_.end() - 1);
      return;
    }
  }
  MakeNull();
}

void InlineCursor::MoveToPreviousIncludingFragmentainer() {
  MoveToPrevious();
  if (!Current() && max_fragment_index_ && CanMoveAcrossFragmentainer())
    MoveToPreviousFragmentainer();
}

void InlineCursor::MoveToFirstIncludingFragmentainer() {
  if (!fragment_index_) {
    MoveToFirst();
    return;
  }

  ResetFragmentIndex();
  if (!TrySetRootFragmentItems())
    MakeNull();
}

void InlineCursor::MoveToNextFragmentainer() {
  DCHECK(CanMoveAcrossFragmentainer());
  if (fragment_index_ < max_fragment_index_) {
    IncrementFragmentIndex();
    if (TrySetRootFragmentItems())
      return;
  }
  MakeNull();
}

void InlineCursor::MoveToNextIncludingFragmentainer() {
  MoveToNext();
  if (!Current() && max_fragment_index_ && CanMoveAcrossFragmentainer())
    MoveToNextFragmentainer();
}

void InlineCursor::SlowMoveToForIfNeeded(const LayoutObject& layout_object) {
  while (Current() && Current().GetLayoutObject() != &layout_object)
    MoveToNextIncludingFragmentainer();
}

void InlineCursor::SlowMoveToFirstFor(const LayoutObject& layout_object) {
  MoveToFirstIncludingFragmentainer();
  SlowMoveToForIfNeeded(layout_object);
}

void InlineCursor::SlowMoveToNextForSameLayoutObject(
    const LayoutObject& layout_object) {
  MoveToNextIncludingFragmentainer();
  SlowMoveToForIfNeeded(layout_object);
}

void InlineCursor::MoveTo(const LayoutObject& layout_object) {
  DCHECK(layout_object.IsInLayoutNGInlineFormattingContext());
  if (layout_object.IsOutOfFlowPositioned()) [[unlikely]] {
    NOTREACHED_IN_MIGRATION();
    MakeNull();
    return;
  }

  // If this cursor is rootless, find the root of the inline formatting context.
  bool is_descendants_cursor = false;
  if (!HasRoot()) {
    const LayoutBlockFlow* root = layout_object.FragmentItemsContainer();
    DCHECK(root);
    SetRoot(*root);
    if (!HasRoot()) [[unlikely]] {
      MakeNull();
      return;
    }
    DCHECK(!IsDescendantsCursor());
  } else {
    is_descendants_cursor = IsDescendantsCursor();
  }

  wtf_size_t item_index = layout_object.FirstInlineFragmentItemIndex();
  if (!item_index) [[unlikely]] {
#if EXPENSIVE_DCHECKS_ARE_ON()
    const LayoutBlockFlow* root = layout_object.FragmentItemsContainer();
    InlineCursor check_cursor(*root);
    check_cursor.SlowMoveToFirstFor(layout_object);
    DCHECK(!check_cursor);
#endif
    MakeNull();
    return;
  }
  // |FirstInlineFragmentItemIndex| is 1-based. Convert to 0-based index.
  DCHECK_GT(item_index, 0UL);
  --item_index;

  // Find |FragmentItems| that contains |item_index|.
  DCHECK_EQ(is_descendants_cursor, IsDescendantsCursor());
  if (root_block_flow_) {
    DCHECK(!is_descendants_cursor);
    while (item_index >= fragment_items_->EndItemIndex()) {
      MoveToNextFragmentainer();
      if (!Current())
        return;
    }
    DCHECK_GE(item_index, fragment_items_->SizeOfEarlierFragments());
    item_index -= fragment_items_->SizeOfEarlierFragments();
#if EXPENSIVE_DCHECKS_ARE_ON()
    InlineCursor check_cursor(*root_block_flow_);
    check_cursor.SlowMoveToFirstFor(layout_object);
    DCHECK_EQ(check_cursor.Current().Item(),
              &fragment_items_->Items()[item_index]);
#endif
  } else {
    // If |this| is not rooted at |LayoutBlockFlow|, iterate |FragmentItems|
    // from |LayoutBlockFlow|.
    if (fragment_items_->HasItemIndex(item_index)) {
      DCHECK_GE(item_index, fragment_items_->SizeOfEarlierFragments());
      item_index -= fragment_items_->SizeOfEarlierFragments();
    } else {
      InlineCursor cursor;
      for (cursor.MoveTo(layout_object);;
           cursor.MoveToNextForSameLayoutObject()) {
        if (!cursor || cursor.fragment_items_->SizeOfEarlierFragments() >
                           fragment_items_->SizeOfEarlierFragments()) {
          MakeNull();
          return;
        }
        if (cursor.fragment_items_ == fragment_items_) {
          DCHECK_GE(cursor.Current().Item(), fragment_items_->Items().data());
          item_index = base::checked_cast<wtf_size_t>(
              cursor.Current().Item() - fragment_items_->Items().data());
          break;
        }
      }
    }
#if EXPENSIVE_DCHECKS_ARE_ON()
    const LayoutBlockFlow* root = layout_object.FragmentItemsContainer();
    InlineCursor check_cursor(*root);
    check_cursor.SlowMoveToFirstFor(layout_object);
    while (check_cursor && fragment_items_ != check_cursor.fragment_items_)
      check_cursor.SlowMoveToNextForSameLayoutObject(layout_object);
    DCHECK_EQ(check_cursor.Current().Item(),
              &fragment_items_->Items()[item_index]);
#endif

    // Skip items before |items_|, in case |this| is part of IFC.
    if (is_descendants_cursor) [[unlikely]] {
      const wtf_size_t span_begin_item_index = SpanBeginItemIndex();
      while (item_index < span_begin_item_index) [[unlikely]] {
        const FragmentItem& item = fragment_items_->Items()[item_index];
        const wtf_size_t next_delta = item.DeltaToNextForSameLayoutObject();
        if (!next_delta) {
          MakeNull();
          return;
        }
        item_index += next_delta;
      }
      if (item_index >= span_begin_item_index + items_.size()) [[unlikely]] {
        MakeNull();
        return;
      }
      DCHECK_GE(item_index, span_begin_item_index);
      item_index -= span_begin_item_index;
    }
  }

  DCHECK_LT(item_index, items_.size());
  current_.Set(items_.begin() + item_index);
}

void InlineCursor::MoveToNextForSameLayoutObjectExceptCulledInline() {
  if (!Current())
    return;
  if (wtf_size_t delta = current_.item_->DeltaToNextForSameLayoutObject()) {
    while (true) {
      // Return if the next index is in the current range.
      const wtf_size_t delta_to_end =
          base::checked_cast<wtf_size_t>(items_.end() - current_.item_iter_);
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
        NOTREACHED_IN_MIGRATION();
        break;
      }
      DCHECK_GE(delta, delta_to_end);
      delta -= delta_to_end;
    }
  }
  MakeNull();
}

void InlineCursor::MoveToLastForSameLayoutObject() {
  if (!Current())
    return;
  InlineCursorPosition last;
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
const LayoutObject* InlineCursor::CulledInlineTraversal::Find(
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

void InlineCursor::CulledInlineTraversal::SetUseFragmentTree(
    const LayoutInline& layout_inline) {
  layout_inline_ = &layout_inline;
  use_fragment_tree_ = true;
}

const LayoutObject* InlineCursor::CulledInlineTraversal::MoveToFirstFor(
    const LayoutInline& layout_inline) {
  layout_inline_ = &layout_inline;
  use_fragment_tree_ = false;
  current_object_ = Find(layout_inline.FirstChild());
  return current_object_;
}

const LayoutObject* InlineCursor::CulledInlineTraversal::MoveToNext() {
  if (!current_object_)
    return nullptr;
  current_object_ =
      Find(current_object_->NextInPreOrderAfterChildren(layout_inline_));
  return current_object_;
}

void InlineCursor::MoveToFirstForCulledInline(
    const LayoutInline& layout_inline) {
  // When |this| is a descendant cursor, |this| may be limited to a very small
  // subset of the |LayoutObject| descendants, and that traversing
  // |LayoutObject| descendants is much more expensive. Prefer checking every
  // fragment in that case.
  if (IsDescendantsCursor()) {
    culled_inline_.SetUseFragmentTree(layout_inline);
    DCHECK(!CanMoveAcrossFragmentainer());
    MoveToFirst();
    while (Current() && !Current().IsPartOfCulledInlineBox(layout_inline))
      MoveToNext();
    return;
  }

  if (const LayoutObject* layout_object =
          culled_inline_.MoveToFirstFor(layout_inline)) {
    MoveTo(*layout_object);
    // This |MoveTo| may fail if |this| is a descendant cursor. Try the next
    // |LayoutObject|.
    MoveToNextCulledInlineDescendantIfNeeded();
  }
}

void InlineCursor::MoveToNextForCulledInline() {
  DCHECK(culled_inline_);
  if (culled_inline_.UseFragmentTree()) {
    const LayoutInline* layout_inline = culled_inline_.GetLayoutInline();
    DCHECK(layout_inline);
    DCHECK(!CanMoveAcrossFragmentainer());
    do {
      MoveToNext();
    } while (Current() && !Current().IsPartOfCulledInlineBox(*layout_inline));
    return;
  }

  MoveToNextForSameLayoutObjectExceptCulledInline();
  // If we're at the end of fragments for the current |LayoutObject| that
  // contributes to the current culled inline, find the next |LayoutObject|.
  MoveToNextCulledInlineDescendantIfNeeded();
}

void InlineCursor::MoveToNextCulledInlineDescendantIfNeeded() {
  DCHECK(culled_inline_);
  if (Current())
    return;

  while (const LayoutObject* layout_object = culled_inline_.MoveToNext()) {
    MoveTo(*layout_object);
    if (Current())
      return;
  }
}

void InlineCursor::ResetFragmentIndex() {
  fragment_index_ = 0;
  previously_consumed_block_size_ = LayoutUnit();
}

void InlineCursor::DecrementFragmentIndex() {
  DCHECK(fragment_index_);
  --fragment_index_;
  previously_consumed_block_size_ = LayoutUnit();
  if (!fragment_index_)
    return;
  // Note: |LayoutBox::GetPhysicalFragment(wtf_size_t)| is O(1).
  const auto& root_box_fragment =
      *root_block_flow_->GetPhysicalFragment(fragment_index_ - 1);
  if (const BlockBreakToken* break_token = root_box_fragment.GetBreakToken()) {
    previously_consumed_block_size_ = break_token->ConsumedBlockSize();
  }
}

void InlineCursor::IncrementFragmentIndex() {
  DCHECK_LE(fragment_index_, max_fragment_index_);
  fragment_index_++;
  if (!root_box_fragment_)
    return;
  if (const BlockBreakToken* break_token =
          root_box_fragment_->GetBreakToken()) {
    previously_consumed_block_size_ = break_token->ConsumedBlockSize();
  }
}

void InlineCursor::MoveToIncludingCulledInline(
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

void InlineCursor::MoveToNextForSameLayoutObject() {
  if (culled_inline_) [[unlikely]] {
    MoveToNextForCulledInline();
    return;
  }
  MoveToNextForSameLayoutObjectExceptCulledInline();
}

void InlineCursor::MoveToVisualLastForSameLayoutObject() {
  if (culled_inline_)
    MoveToVisualFirstOrLastForCulledInline(true);
  else
    MoveToLastForSameLayoutObject();
}

void InlineCursor::MoveToVisualFirstForSameLayoutObject() {
  if (culled_inline_)
    MoveToVisualFirstOrLastForCulledInline(false);
}

void InlineCursor::MoveToVisualFirstOrLastForCulledInline(bool last) {
  InlineCursorPosition found_position;
  std::optional<size_t> found_index;
  wtf_size_t found_fragment_index = 0;

  // Iterate through the remaining fragments to find the lowest/greatest index.
  for (; Current(); MoveToNextForSameLayoutObject()) {
    // Index of the current fragment into |fragment_items_|.
    size_t index = Current().Item() - fragment_items_->Items().data();
    DCHECK_LT(index, fragment_items_->Size());
    if (!found_index || (last && index > *found_index) ||
        (!last && index < *found_index)) {
      found_position = Current();
      found_index = index;
      found_fragment_index = fragment_index_;

      // Break if there cannot be any fragment lower/greater than this one.
      if ((last && index == fragment_items_->Size() - 1) ||
          (!last && index == 0))
        break;
    }
  }

  DCHECK(found_position);
  if (fragment_index_ > found_fragment_index) {
    while (fragment_index_ > found_fragment_index) {
      DecrementFragmentIndex();
    }
    CHECK(TrySetRootFragmentItems());
  }
  MoveTo(found_position);
}

//
// |InlineBackwardCursor| functions.
//
InlineBackwardCursor::InlineBackwardCursor(const InlineCursor& cursor)
    : cursor_(cursor) {
  if (cursor.HasRoot()) {
    DCHECK(!cursor || cursor.items_.begin() == cursor.Current().item_iter_);
    for (InlineCursor sibling(cursor); sibling;
         sibling.MoveToNextSkippingChildren()) {
      sibling_item_iterators_.push_back(sibling.Current().item_iter_);
    }
    current_index_ = sibling_item_iterators_.size();
    if (current_index_)
      current_.Set(sibling_item_iterators_[--current_index_]);
    return;
  }
  DCHECK(!cursor);
}

InlineCursor InlineBackwardCursor::CursorForDescendants() const {
  if (current_.item_) {
    InlineCursor cursor(cursor_);
    cursor.MoveToItem(sibling_item_iterators_[current_index_]);
    return cursor.CursorForDescendants();
  }
  NOTREACHED_IN_MIGRATION();
  return InlineCursor();
}

void InlineBackwardCursor::MoveToPreviousSibling() {
  if (current_index_) {
    if (current_.item_) {
      current_.Set(sibling_item_iterators_[--current_index_]);
      return;
    }
    NOTREACHED_IN_MIGRATION();
  }
  current_.Clear();
}

std::ostream& operator<<(std::ostream& ostream, const InlineCursor& cursor) {
  if (!cursor)
    return ostream << "InlineCursor()";
  DCHECK(cursor.HasRoot());
  return ostream << "InlineCursor(" << *cursor.CurrentItem() << ")";
}

std::ostream& operator<<(std::ostream& ostream, const InlineCursor* cursor) {
  if (!cursor)
    return ostream << "<null>";
  return ostream << *cursor;
}

#if DCHECK_IS_ON()
void InlineCursor::CheckValid(const InlineCursorPosition& position) const {
  if (position.Item()) {
    DCHECK(HasRoot());
    DCHECK_EQ(position.item_, &*position.item_iter_);
    const unsigned index =
        base::checked_cast<unsigned>(position.item_iter_ - items_.begin());
    DCHECK_LT(index, items_.size());
  }
}
#endif

}  // namespace blink
