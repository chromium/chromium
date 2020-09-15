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
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/paint/ng/ng_paint_fragment.h"
#include "third_party/blink/renderer/core/paint/ng/ng_paint_fragment_traversal.h"

namespace blink {

inline void NGInlineCursor::MoveToItem(const ItemsSpan::iterator& iter) {
  DCHECK(IsItemCursor());
  DCHECK(iter >= items_.begin() && iter <= items_.end());
  if (iter != items_.end()) {
    current_.Set(iter);
    return;
  }
  MakeNull();
}

void NGInlineCursor::SetRoot(const NGFragmentItems& fragment_items,
                             ItemsSpan items) {
  DCHECK(items.data() || !items.size());
  fragment_items_ = &fragment_items;
  items_ = items;
  DCHECK(fragment_items_->IsSubSpan(items_));
  MoveToItem(items_.begin());
}

void NGInlineCursor::SetRoot(const NGFragmentItems& items) {
  SetRoot(items, items.Items());
}

void NGInlineCursor::SetRoot(const NGPaintFragment& root_paint_fragment) {
  DCHECK(&root_paint_fragment);
  DCHECK(!HasRoot());
  root_paint_fragment_ = &root_paint_fragment;
  current_.paint_fragment_ = root_paint_fragment.FirstChild();
}

bool NGInlineCursor::TrySetRootFragmentItems() {
  DCHECK(root_block_flow_);
  DCHECK(!fragment_items_ || fragment_items_->Equals(items_));
  for (; fragment_index_ <= max_fragment_index_; ++fragment_index_) {
    const NGPhysicalBoxFragment* fragment =
        root_block_flow_->GetPhysicalFragment(fragment_index_);
    DCHECK(fragment);
    if (const NGFragmentItems* items = fragment->Items()) {
      SetRoot(*items);
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
    fragment_index_ = 0;
    if (TrySetRootFragmentItems())
      return;
  }

  if (const NGPaintFragment* paint_fragment = block_flow.PaintFragment()) {
    SetRoot(*paint_fragment);
    return;
  }

  // We reach here in case of |ScrollANchor::NotifyBeforeLayout()| via
  // |LayoutText::PhysicalLinesBoundingBox()|
  // See external/wpt/css/css-scroll-anchoring/wrapped-text.html
}

NGInlineCursor::NGInlineCursor(const LayoutBlockFlow& block_flow) {
  SetRoot(block_flow);
}

NGInlineCursor::NGInlineCursor(const NGFragmentItems& fragment_items,
                               ItemsSpan items) {
  SetRoot(fragment_items, items);
}

NGInlineCursor::NGInlineCursor(const NGFragmentItems& items) {
  SetRoot(items);
}

NGInlineCursor::NGInlineCursor(const NGPaintFragment& root_paint_fragment) {
  SetRoot(root_paint_fragment);
}

NGInlineCursor::NGInlineCursor(const NGInlineBackwardCursor& backward_cursor)
    : NGInlineCursor(backward_cursor.cursor_) {
  MoveTo(backward_cursor.Current());
}

bool NGInlineCursor::operator==(const NGInlineCursor& other) const {
  if (root_paint_fragment_) {
    return root_paint_fragment_ == other.root_paint_fragment_ &&
           current_.paint_fragment_ == other.current_.paint_fragment_;
  }
  if (current_.item_ != other.current_.item_)
    return false;
  DCHECK_EQ(items_.data(), other.items_.data());
  DCHECK_EQ(items_.size(), other.items_.size());
  DCHECK_EQ(fragment_items_, other.fragment_items_);
  DCHECK(current_.item_iter_ == other.current_.item_iter_);
  return true;
}

const LayoutBlockFlow* NGInlineCursor::GetLayoutBlockFlow() const {
  if (IsPaintFragmentCursor()) {
    // |root_paint_fragment_| is either |LayoutBlockFlow| or |LayoutInline|.
    const NGPhysicalFragment& physical_fragment =
        root_paint_fragment_->PhysicalFragment();
    const LayoutObject* layout_object =
        physical_fragment.IsLineBox()
            ? To<NGPhysicalLineBoxFragment>(physical_fragment)
                  .ContainerLayoutObject()
            : physical_fragment.GetLayoutObject();
    if (const LayoutBlockFlow* block_flow =
            DynamicTo<LayoutBlockFlow>(layout_object))
      return block_flow;
    DCHECK(layout_object->IsLayoutInline());
    return layout_object->RootInlineFormattingContext();
  }
  if (IsItemCursor()) {
    const NGFragmentItem& item = fragment_items_->front();
    const LayoutObject* layout_object = item.GetLayoutObject();
    if (item.Type() == NGFragmentItem::kLine)
      return To<LayoutBlockFlow>(layout_object);
    return layout_object->RootInlineFormattingContext();
  }
  NOTREACHED();
  return nullptr;
}

bool NGInlineCursorPosition::HasChildren() const {
  if (paint_fragment_)
    return paint_fragment_->FirstChild();
  if (item_)
    return item_->HasChildren();
  NOTREACHED();
  return false;
}

NGInlineCursor NGInlineCursor::CursorForDescendants() const {
  if (current_.paint_fragment_)
    return NGInlineCursor(*current_.paint_fragment_);
  if (current_.item_) {
    unsigned descendants_count = current_.item_->DescendantsCount();
    if (descendants_count > 1) {
      DCHECK(fragment_items_);
      return NGInlineCursor(
          *fragment_items_,
          ItemsSpan(&*(current_.item_iter_ + 1), descendants_count - 1));
    }
    return NGInlineCursor();
  }
  NOTREACHED();
  return NGInlineCursor();
}

void NGInlineCursor::ExpandRootToContainingBlock() {
  if (root_paint_fragment_) {
    root_paint_fragment_ = root_paint_fragment_->Root();
    return;
  }
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
  DCHECK(break_token);
  return !break_token->IsFinished() && !break_token->IsForcedBreak();
}

bool NGInlineCursorPosition::IsInlineBox() const {
  if (paint_fragment_)
    return paint_fragment_->PhysicalFragment().IsInlineBox();
  if (item_)
    return item_->IsInlineBox();
  NOTREACHED();
  return false;
}

bool NGInlineCursorPosition::IsAtomicInline() const {
  if (paint_fragment_)
    return paint_fragment_->PhysicalFragment().IsAtomicInline();
  if (item_)
    return item_->IsAtomicInline();
  NOTREACHED();
  return false;
}

bool NGInlineCursorPosition::IsEllipsis() const {
  if (paint_fragment_)
    return paint_fragment_->IsEllipsis();
  if (item_)
    return item_->IsEllipsis();
  NOTREACHED();
  return false;
}

bool NGInlineCursorPosition::IsGeneratedText() const {
  if (paint_fragment_) {
    if (auto* text_fragment = DynamicTo<NGPhysicalTextFragment>(
            paint_fragment_->PhysicalFragment()))
      return text_fragment->IsGeneratedText();
    return false;
  }
  if (item_)
    return item_->IsGeneratedText();
  NOTREACHED();
  return false;
}

bool NGInlineCursorPosition::IsLayoutGeneratedText() const {
  if (paint_fragment_) {
    if (auto* text_fragment = DynamicTo<NGPhysicalTextFragment>(
            paint_fragment_->PhysicalFragment())) {
      return text_fragment->TextType() == NGTextType::kLayoutGenerated;
    }
    return false;
  }
  if (item_)
    return item_->Type() == NGFragmentItem::kGeneratedText;
  NOTREACHED();
  return false;
}

bool NGInlineCursorPosition::IsHiddenForPaint() const {
  if (paint_fragment_)
    return paint_fragment_->PhysicalFragment().IsHiddenForPaint();
  if (item_)
    return item_->IsHiddenForPaint();
  NOTREACHED();
  return false;
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

bool NGInlineCursorPosition::IsPartOfCulledInlineBox(
    const LayoutInline& layout_inline) const {
  DCHECK(!layout_inline.ShouldCreateBoxFragment());
  DCHECK(*this);
  const LayoutObject* const layout_object = GetLayoutObject();
  // We use |IsInline()| to exclude floating and out-of-flow objects.
  if (!layout_object || !layout_object->IsInline() ||
      layout_object->IsAtomicInlineLevel())
    return false;
  DCHECK(!layout_object->IsFloatingOrOutOfFlowPositioned());
  DCHECK(!BoxFragment() || !BoxFragment()->IsFormattingContextRoot());
  for (const LayoutObject* parent = layout_object->Parent(); parent;
       parent = parent->Parent()) {
    // Children of culled inline should be included.
    if (parent == &layout_inline)
      return true;
    // Grand children should be included only if children are also culled.
    if (const auto* parent_layout_inline = ToLayoutInlineOrNull(parent)) {
      if (!parent_layout_inline->ShouldCreateBoxFragment())
        continue;
    }
    return false;
  }
  return false;
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

bool NGInlineCursorPosition::IsLineBreak() const {
  if (paint_fragment_) {
    if (auto* text_fragment = DynamicTo<NGPhysicalTextFragment>(
            paint_fragment_->PhysicalFragment()))
      return text_fragment->IsLineBreak();
    return false;
  }
  if (item_)
    return IsText() && item_->IsLineBreak();
  NOTREACHED();
  return false;
}

bool NGInlineCursorPosition::IsListMarker() const {
  if (paint_fragment_)
    return paint_fragment_->PhysicalFragment().IsListMarker();
  if (item_)
    return item_->IsListMarker();
  NOTREACHED();
  return false;
}

bool NGInlineCursorPosition::IsText() const {
  if (paint_fragment_)
    return paint_fragment_->PhysicalFragment().IsText();
  if (item_)
    return item_->IsText();
  NOTREACHED();
  return false;
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
  if (paint_fragment_)
    return paint_fragment_->PhysicalFragment().IsContainer();
  if (item_) {
    return item_->Type() == NGFragmentItem::kLine ||
           (item_->Type() == NGFragmentItem::kBox && !item_->IsAtomicInline());
  }
  NOTREACHED();
  return false;
}

bool NGInlineCursorPosition::IsEmptyLineBox() const {
  DCHECK(IsLineBox());
  if (paint_fragment_) {
    return To<NGPhysicalLineBoxFragment>(paint_fragment_->PhysicalFragment())
        .IsEmptyLineBox();
  }
  if (item_)
    return item_->IsEmptyLineBox();
  NOTREACHED();
  return false;
}

bool NGInlineCursorPosition::IsLineBox() const {
  if (paint_fragment_)
    return paint_fragment_->PhysicalFragment().IsLineBox();
  if (item_)
    return item_->Type() == NGFragmentItem::kLine;
  NOTREACHED();
  return false;
}

TextDirection NGInlineCursorPosition::BaseDirection() const {
  DCHECK(IsLineBox());
  if (paint_fragment_) {
    return To<NGPhysicalLineBoxFragment>(paint_fragment_->PhysicalFragment())
        .BaseDirection();
  }
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
    const LayoutText& layout_text = *ToLayoutText(GetLayoutObject());
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
    const NGPhysicalBoxFragment* fragmentainer =
        GetLayoutObject()->ContainingBlockFlowFragment();
    DCHECK(fragmentainer);
    const LayoutBlockFlow& block_flow =
        *To<LayoutBlockFlow>(fragmentainer->GetLayoutObject());
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

const NGPhysicalBoxFragment* NGInlineCursorPosition::BoxFragment() const {
  if (paint_fragment_) {
    return DynamicTo<NGPhysicalBoxFragment>(
        &paint_fragment_->PhysicalFragment());
  }
  if (item_)
    return item_->BoxFragment();
  NOTREACHED();
  return nullptr;
}

const DisplayItemClient* NGInlineCursorPosition::GetDisplayItemClient() const {
  if (paint_fragment_)
    return paint_fragment_;
  if (item_)
    return item_->GetDisplayItemClient();
  NOTREACHED();
  return nullptr;
}

const DisplayItemClient* NGInlineCursorPosition::GetSelectionDisplayItemClient()
    const {
  if (const auto* client = GetLayoutObject()->GetSelectionDisplayItemClient())
    return client;
  return GetDisplayItemClient();
}

wtf_size_t NGInlineCursorPosition::FragmentId() const {
  if (paint_fragment_)
    return 0;
  DCHECK(item_);
  return item_->FragmentId();
}

const NGInlineBreakToken* NGInlineCursorPosition::InlineBreakToken() const {
  DCHECK(IsLineBox());
  if (paint_fragment_) {
    return To<NGInlineBreakToken>(
        To<NGPhysicalLineBoxFragment>(paint_fragment_->PhysicalFragment())
            .BreakToken());
  }
  DCHECK(item_);
  return item_->InlineBreakToken();
}

const LayoutObject* NGInlineCursorPosition::GetLayoutObject() const {
  if (paint_fragment_)
    return paint_fragment_->GetLayoutObject();
  if (item_)
    return item_->GetLayoutObject();
  NOTREACHED();
  return nullptr;
}

LayoutObject* NGInlineCursorPosition::GetMutableLayoutObject() const {
  if (paint_fragment_)
    return paint_fragment_->GetMutableLayoutObject();
  if (item_)
    return item_->GetMutableLayoutObject();
  NOTREACHED();
  return nullptr;
}

const Node* NGInlineCursorPosition::GetNode() const {
  if (const LayoutObject* layout_object = GetLayoutObject())
    return layout_object->GetNode();
  return nullptr;
}

const PhysicalRect NGInlineCursorPosition::InkOverflow() const {
  if (paint_fragment_)
    return paint_fragment_->InkOverflow();
  if (item_)
    return item_->InkOverflow();
  NOTREACHED();
  return PhysicalRect();
}

const PhysicalOffset NGInlineCursorPosition::OffsetInContainerBlock() const {
  if (paint_fragment_)
    return paint_fragment_->OffsetInContainerBlock();
  if (item_)
    return item_->OffsetInContainerBlock();
  NOTREACHED();
  return PhysicalOffset();
}

const PhysicalSize NGInlineCursorPosition::Size() const {
  if (paint_fragment_)
    return paint_fragment_->Size();
  if (item_)
    return item_->Size();
  NOTREACHED();
  return PhysicalSize();
}

const PhysicalRect NGInlineCursorPosition::RectInContainerBlock() const {
  if (paint_fragment_) {
    return {paint_fragment_->OffsetInContainerBlock(), paint_fragment_->Size()};
  }
  if (item_)
    return item_->RectInContainerBlock();
  NOTREACHED();
  return PhysicalRect();
}

const PhysicalRect NGInlineCursorPosition::SelfInkOverflow() const {
  if (paint_fragment_)
    return paint_fragment_->SelfInkOverflow();
  if (item_)
    return item_->SelfInkOverflow();
  NOTREACHED();
  return PhysicalRect();
}

TextDirection NGInlineCursorPosition::ResolvedDirection() const {
  if (paint_fragment_)
    return paint_fragment_->PhysicalFragment().ResolvedDirection();
  if (item_)
    return item_->ResolvedDirection();
  NOTREACHED();
  return TextDirection::kLtr;
}

const ComputedStyle& NGInlineCursorPosition::Style() const {
  if (paint_fragment_)
    return paint_fragment_->Style();
  return item_->Style();
}

NGStyleVariant NGInlineCursorPosition::StyleVariant() const {
  if (paint_fragment_)
    return paint_fragment_->PhysicalFragment().StyleVariant();
  return item_->StyleVariant();
}

bool NGInlineCursorPosition::UsesFirstLineStyle() const {
  return StyleVariant() == NGStyleVariant::kFirstLine;
}

NGTextOffset NGInlineCursorPosition::TextOffset() const {
  if (paint_fragment_) {
    const auto& text_fragment =
        To<NGPhysicalTextFragment>(paint_fragment_->PhysicalFragment());
    return text_fragment.TextOffset();
  }
  if (item_)
    return item_->TextOffset();
  NOTREACHED();
  return {};
}

StringView NGInlineCursorPosition::Text(const NGInlineCursor& cursor) const {
  DCHECK(IsText());
  cursor.CheckValid(*this);
  if (paint_fragment_) {
    return To<NGPhysicalTextFragment>(paint_fragment_->PhysicalFragment())
        .Text();
  }
  if (item_)
    return item_->Text(cursor.Items());
  NOTREACHED();
  return "";
}

const ShapeResultView* NGInlineCursorPosition::TextShapeResult() const {
  DCHECK(IsText());
  if (paint_fragment_) {
    return To<NGPhysicalTextFragment>(paint_fragment_->PhysicalFragment())
        .TextShapeResult();
  }
  if (item_)
    return item_->TextShapeResult();
  NOTREACHED();
  return nullptr;
}

PhysicalRect NGInlineCursor::CurrentLocalRect(unsigned start_offset,
                                              unsigned end_offset) const {
  DCHECK(Current().IsText());
  if (current_.paint_fragment_) {
    return To<NGPhysicalTextFragment>(
               current_.paint_fragment_->PhysicalFragment())
        .LocalRect(start_offset, end_offset);
  }
  if (current_.item_) {
    return current_.item_->LocalRect(current_.item_->Text(*fragment_items_),
                                     start_offset, end_offset);
  }
  NOTREACHED();
  return PhysicalRect();
}

LayoutUnit NGInlineCursor::InlinePositionForOffset(unsigned offset) const {
  DCHECK(Current().IsText());
  if (current_.paint_fragment_) {
    return To<NGPhysicalTextFragment>(
               current_.paint_fragment_->PhysicalFragment())
        .InlinePositionForOffset(offset);
  }
  if (current_.item_) {
    return current_.item_->InlinePositionForOffset(
        current_.item_->Text(*fragment_items_), offset);
  }
  NOTREACHED();
  return LayoutUnit();
}

PhysicalOffset NGInlineCursorPosition::LineStartPoint() const {
  DCHECK(IsLineBox()) << this;
  const LogicalOffset logical_start;  // (0, 0)
  const PhysicalSize pixel_size(LayoutUnit(1), LayoutUnit(1));
  return logical_start.ConvertToPhysical(Style().GetWritingMode(),
                                         BaseDirection(), Size(), pixel_size);
}

PhysicalOffset NGInlineCursorPosition::LineEndPoint() const {
  DCHECK(IsLineBox()) << this;
  const WritingMode writing_mode = Style().GetWritingMode();
  const LayoutUnit inline_size =
      IsHorizontalWritingMode(writing_mode) ? Size().width : Size().height;
  const LogicalOffset logical_end(inline_size, LayoutUnit());
  const PhysicalSize pixel_size(LayoutUnit(1), LayoutUnit(1));
  return logical_end.ConvertToPhysical(writing_mode, BaseDirection(), Size(),
                                       pixel_size);
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
  DCHECK(IsItemCursor());
  const ComputedStyle& container_style = container.Style();
  const WritingMode writing_mode = container_style.GetWritingMode();
  const TextDirection direction = container_style.Direction();
  const PhysicalSize& container_size = container.Size();
  const LayoutUnit point_block_offset =
      point
          .ConvertToLogical(writing_mode, direction, container_size,
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
        MoveToNextItemSkippingChildren();
        continue;
      }
      // Try to resolve if |point| falls in a line box in block direction.
      const LayoutUnit child_block_offset =
          child_item->OffsetInContainerBlock()
              .ConvertToLogical(writing_mode, direction, container_size,
                                child_item->Size())
              .block_offset;
      if (point_block_offset < child_block_offset) {
        if (child_block_offset < closest_line_before_block_offset) {
          closest_line_before_block_offset = child_block_offset;
          closest_line_before = Current();
        }
        MoveToNextItemSkippingChildren();
        continue;
      }

      // Hitting on line bottom doesn't count, to match legacy behavior.
      const LayoutUnit child_block_end_offset =
          child_block_offset +
          child_item->Size().ConvertToLogical(writing_mode).block_size;
      if (point_block_offset >= child_block_end_offset) {
        if (child_block_end_offset > closest_line_after_block_offset) {
          closest_line_after_block_offset = child_block_end_offset;
          closest_line_after = Current();
        }
        MoveToNextItemSkippingChildren();
        continue;
      }

      if (const PositionWithAffinity child_position =
              PositionForPointInInlineBox(point))
        return child_position;
      MoveToNextItemSkippingChildren();
      continue;
    }
    DCHECK_NE(child_item->Type(), NGFragmentItem::kText);
    MoveToNextItem();
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
  if (const NGPaintFragment* paint_fragment = CurrentPaintFragment()) {
    DCHECK(paint_fragment->PhysicalFragment().IsLineBox());
    return paint_fragment->PositionForPoint(point);
  }
  const NGFragmentItem* container = CurrentItem();
  DCHECK(container);
  DCHECK(container->Type() == NGFragmentItem::kLine ||
         container->Type() == NGFragmentItem::kBox);
  const ComputedStyle& container_style = container->Style();
  const WritingMode writing_mode = container_style.GetWritingMode();
  const TextDirection direction = container_style.Direction();
  const PhysicalSize& container_size = container->Size();
  const LayoutUnit point_inline_offset =
      point
          .ConvertToLogical(writing_mode, direction, container_size,
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
        child_item->OffsetInContainerBlock()
            .ConvertToLogical(writing_mode, direction, container_size,
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
        child_item->Size().ConvertToLogical(writing_mode).inline_size;
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
  if (auto* paint_fragment = CurrentPaintFragment()) {
    const PhysicalOffset point_in_child =
        point_in_container - paint_fragment->OffsetInContainerBlock();
    return paint_fragment->PositionForPoint(point_in_child);
  }
  DCHECK(CurrentItem());
  const NGFragmentItem& child_item = *CurrentItem();
  switch (child_item.Type()) {
    case NGFragmentItem::kText:
      return child_item.PositionForPointInText(
          point_in_container - child_item.OffsetInContainerBlock(), *this);
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
              point_in_container - child_item.OffsetInContainerBlock());
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

PositionWithAffinity NGInlineCursor::PositionForStartOfLine() const {
  DCHECK(Current().IsLineBox());
  const PhysicalOffset point_in_line = Current().LineStartPoint();
  if (IsItemCursor()) {
    return PositionForPointInInlineBox(point_in_line +
                                       Current().OffsetInContainerBlock());
  }
  return CurrentPaintFragment()->PositionForPoint(point_in_line);
}

PositionWithAffinity NGInlineCursor::PositionForEndOfLine() const {
  DCHECK(Current().IsLineBox());
  const PhysicalOffset point_in_line = Current().LineEndPoint();
  if (IsItemCursor()) {
    return PositionForPointInInlineBox(point_in_line +
                                       Current().OffsetInContainerBlock());
  }
  return CurrentPaintFragment()->PositionForPoint(point_in_line);
}

void NGInlineCursor::MoveTo(const NGInlineCursorPosition& position) {
  CheckValid(position);
  current_ = position;
}

inline wtf_size_t NGInlineCursor::SpanBeginItemIndex() const {
  DCHECK(IsItemCursor());
  DCHECK(!items_.empty());
  DCHECK(fragment_items_->IsSubSpan(items_));
  const wtf_size_t delta = items_.data() - fragment_items_->Items().data();
  DCHECK_LT(delta, fragment_items_->Items().size());
  return delta;
}

inline wtf_size_t NGInlineCursor::SpanIndexFromItemIndex(unsigned index) const {
  DCHECK(IsItemCursor());
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
  DCHECK(!root_paint_fragment_ && !current_.paint_fragment_);
  MoveTo(*fragment_item.GetLayoutObject());
  while (IsNotNull()) {
    if (CurrentItem() == &fragment_item)
      return;
    MoveToNext();
  }
  NOTREACHED();
}

void NGInlineCursor::MoveTo(const NGInlineCursor& cursor) {
  if (const NGPaintFragment* paint_fragment = cursor.CurrentPaintFragment()) {
    MoveTo(*paint_fragment);
    return;
  }
  if (cursor.current_.item_) {
    if (!fragment_items_)
      SetRoot(*cursor.fragment_items_);
    // Note: We use address instead of iterato because we can't compare
    // iterators in different span. See |base::CheckedContiguousIterator<T>|.
    const ptrdiff_t index = &*cursor.current_.item_iter_ - &*items_.begin();
    DCHECK_GE(index, 0);
    DCHECK_LT(static_cast<size_t>(index), items_.size());
    MoveToItem(items_.begin() + index);
    return;
  }
  *this = cursor;
}

void NGInlineCursor::MoveTo(const NGPaintFragment& paint_fragment) {
  DCHECK(!fragment_items_);
  if (!root_paint_fragment_)
    root_paint_fragment_ = paint_fragment.Root();
  DCHECK(root_paint_fragment_);
  DCHECK(paint_fragment.IsDescendantOfNotSelf(*root_paint_fragment_))
      << paint_fragment << " " << root_paint_fragment_;
  current_.paint_fragment_ = &paint_fragment;
}

void NGInlineCursor::MoveTo(const NGPaintFragment* paint_fragment) {
  if (paint_fragment) {
    MoveTo(*paint_fragment);
    return;
  }
  MakeNull();
}

void NGInlineCursor::MoveToContainingLine() {
  DCHECK(!Current().IsLineBox());
  if (current_.paint_fragment_) {
    current_.paint_fragment_ = current_.paint_fragment_->ContainerLineBox();
    return;
  }
  if (current_.item_) {
    while (current_.item_ && !Current().IsLineBox())
      MoveToPreviousItem();
    return;
  }
  NOTREACHED();
}

bool NGInlineCursor::IsAtFirst() const {
  if (const NGPaintFragment* paint_fragment = Current().PaintFragment())
    return paint_fragment == root_paint_fragment_->FirstChild();
  if (const NGFragmentItem* item = Current().Item())
    return item == &items_.front();
  return false;
}

void NGInlineCursor::MoveToFirst() {
  if (root_paint_fragment_) {
    current_.paint_fragment_ = root_paint_fragment_->FirstChild();
    return;
  }
  if (IsItemCursor()) {
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
  if (root_paint_fragment_) {
    MoveTo(root_paint_fragment_->FirstLineBox());
    return;
  }
  if (IsItemCursor()) {
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

void NGInlineCursor::MoveToLastChild() {
  DCHECK(Current().CanHaveChildren());
  if (!TryToMoveToLastChild())
    MakeNull();
}

void NGInlineCursor::MoveToLastLine() {
  DCHECK(IsItemCursor());
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

void NGInlineCursor::MoveToNext() {
  if (root_paint_fragment_)
    return MoveToNextPaintFragment();
  MoveToNextItem();
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
  if (current_.paint_fragment_) {
    if (auto* paint_fragment = current_.paint_fragment_->NextSibling())
      return MoveTo(*paint_fragment);
    return MakeNull();
  }
  if (current_.item_) {
    do {
      MoveToNextItem();
    } while (Current() && !Current().IsLineBox());
    return;
  }
  NOTREACHED();
}

void NGInlineCursor::MoveToNextSkippingChildren() {
  if (root_paint_fragment_)
    return MoveToNextPaintFragmentSkippingChildren();
  MoveToNextItemSkippingChildren();
}

void NGInlineCursor::MoveToPrevious() {
  if (root_paint_fragment_)
    return MoveToPreviousPaintFragment();
  MoveToPreviousItem();
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
  if (current_.paint_fragment_) {
    do {
      MoveToPreviousSiblingPaintFragment();
    } while (Current() && !Current().IsLineBox());
    return;
  }
  if (current_.item_) {
    do {
      MoveToPreviousItem();
    } while (Current() && !Current().IsLineBox());
    return;
  }
  NOTREACHED();
}

bool NGInlineCursor::TryToMoveToFirstChild() {
  if (!Current().HasChildren())
    return false;
  if (root_paint_fragment_) {
    MoveTo(*current_.paint_fragment_->FirstChild());
    return true;
  }
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
  if (root_paint_fragment_) {
    MoveTo(current_.paint_fragment_->Children().back());
    return true;
  }
  const auto end = current_.item_iter_ + CurrentItem()->DescendantsCount();
  MoveToNextItem();  // Move to the first child.
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

void NGInlineCursor::MoveToNextItem() {
  DCHECK(IsItemCursor());
  if (UNLIKELY(!current_.item_))
    return;
  DCHECK(current_.item_iter_ != items_.end());
  if (++current_.item_iter_ != items_.end()) {
    current_.item_ = &*current_.item_iter_;
    return;
  }
  MakeNull();
}

void NGInlineCursor::MoveToNextItemSkippingChildren() {
  DCHECK(IsItemCursor());
  if (UNLIKELY(!current_.item_))
    return;
  // If the current item has |DescendantsCount|, add it to move to the next
  // sibling, skipping all children and their descendants.
  if (wtf_size_t descendants_count = current_.item_->DescendantsCount())
    return MoveToItem(current_.item_iter_ + descendants_count);
  return MoveToNextItem();
}

void NGInlineCursor::MoveToPreviousItem() {
  DCHECK(IsItemCursor());
  if (UNLIKELY(!current_.item_))
    return;
  if (current_.item_iter_ == items_.begin())
    return MakeNull();
  --current_.item_iter_;
  current_.item_ = &*current_.item_iter_;
}

void NGInlineCursor::MoveToParentPaintFragment() {
  DCHECK(IsPaintFragmentCursor() && current_.paint_fragment_);
  const NGPaintFragment* parent = current_.paint_fragment_->Parent();
  if (parent && parent != root_paint_fragment_) {
    current_.paint_fragment_ = parent;
    return;
  }
  current_.paint_fragment_ = nullptr;
}

void NGInlineCursor::MoveToNextPaintFragment() {
  DCHECK(IsPaintFragmentCursor() && current_.paint_fragment_);
  if (const NGPaintFragment* child = current_.paint_fragment_->FirstChild()) {
    current_.paint_fragment_ = child;
    return;
  }
  MoveToNextPaintFragmentSkippingChildren();
}

void NGInlineCursor::MoveToNextSiblingPaintFragment() {
  DCHECK(IsPaintFragmentCursor() && current_.paint_fragment_);
  if (const NGPaintFragment* next = current_.paint_fragment_->NextSibling()) {
    current_.paint_fragment_ = next;
    return;
  }
  current_.paint_fragment_ = nullptr;
}

void NGInlineCursor::MoveToNextPaintFragmentSkippingChildren() {
  DCHECK(IsPaintFragmentCursor() && current_.paint_fragment_);
  while (current_.paint_fragment_) {
    if (const NGPaintFragment* next = current_.paint_fragment_->NextSibling()) {
      current_.paint_fragment_ = next;
      return;
    }
    MoveToParentPaintFragment();
  }
}

void NGInlineCursor::MoveToPreviousPaintFragment() {
  DCHECK(IsPaintFragmentCursor() && current_.paint_fragment_);
  const NGPaintFragment* const parent = current_.paint_fragment_->Parent();
  MoveToPreviousSiblingPaintFragment();
  if (current_.paint_fragment_) {
    while (TryToMoveToLastChild())
      continue;
    return;
  }
  current_.paint_fragment_ = parent == root_paint_fragment_ ? nullptr : parent;
}

void NGInlineCursor::MoveToPreviousSiblingPaintFragment() {
  DCHECK(IsPaintFragmentCursor() && current_.paint_fragment_);
  const NGPaintFragment* const current = current_.paint_fragment_;
  current_.paint_fragment_ = nullptr;
  for (auto* sibling : current->Parent()->Children()) {
    if (sibling == current)
      return;
    current_.paint_fragment_ = sibling;
  }
  NOTREACHED();
}

void NGInlineCursor::MoveToFirstIncludingFragmentainer() {
  if (!fragment_index_ || IsPaintFragmentCursor()) {
    MoveToFirst();
    return;
  }

  fragment_index_ = 0;
  if (!TrySetRootFragmentItems())
    MakeNull();
}

void NGInlineCursor::MoveToNextFragmentainer() {
  DCHECK(CanMoveAcrossFragmentainer());
  if (fragment_index_ < max_fragment_index_) {
    ++fragment_index_;
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

  if (!RuntimeEnabledFeatures::LayoutNGFragmentItemEnabled()) {
    // If this cursor is rootless, find the root of the inline formatting
    // context.
    if (!HasRoot()) {
      const LayoutBlockFlow& root =
          *layout_object.RootInlineFormattingContext();
      DCHECK(&root);
      SetRoot(root);
      if (!HasRoot()) {
        const auto fragments =
            NGPaintFragment::InlineFragmentsFor(&layout_object);
        if (!fragments.IsInLayoutNGInlineFormattingContext() ||
            fragments.IsEmpty())
          return MakeNull();
        // external/wpt/css/css-scroll-anchoring/text-anchor-in-vertical-rl.html
        // reaches here.
        root_paint_fragment_ = fragments.front().Root();
      }
      DCHECK(HasRoot());
    }

    const auto fragments = NGPaintFragment::InlineFragmentsFor(&layout_object);
    if (!fragments.IsEmpty()) {
      // If |this| is IFC root, just move to the first fragment.
      if (!root_paint_fragment_->Parent()) {
        DCHECK(fragments.front().IsDescendantOfNotSelf(*root_paint_fragment_));
        MoveTo(fragments.front());
        return;
      }
      // If |this| is limited, find the first fragment in the range.
      for (const auto* fragment : fragments) {
        if (fragment->IsDescendantOfNotSelf(*root_paint_fragment_)) {
          MoveTo(*fragment);
          return;
        }
      }
    }
    return MakeNull();
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
  if (current_.paint_fragment_) {
    if (auto* paint_fragment =
            current_.paint_fragment_->NextForSameLayoutObject()) {
      if (!root_paint_fragment_->Parent()) {
        // |paint_fragment| can be in another fragment tree rooted by
        // |root_paint_fragment_|, e.g. "multicol-span-all-restyle-002.html"
        root_paint_fragment_ = paint_fragment->Root();
        MoveTo(*paint_fragment);
        return;
      }
      // If |this| is limited, make sure the result is in the range.
      if (paint_fragment->IsDescendantOfNotSelf(*root_paint_fragment_)) {
        MoveTo(*paint_fragment);
        return;
      }
    }
    return MakeNull();
  }
  if (current_.item_) {
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

    if (const LayoutInline* child_layout_inline = ToLayoutInlineOrNull(child)) {
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

void NGInlineCursor::CulledInlineTraversal::SetUseFragmentTree(
    const LayoutInline& layout_inline) {
  layout_inline_ = &layout_inline;
  use_fragment_tree_ = true;
}

const LayoutObject* NGInlineCursor::CulledInlineTraversal::MoveToFirstFor(
    const LayoutInline& layout_inline) {
  layout_inline_ = &layout_inline;
  use_fragment_tree_ = false;
  current_object_ = Find(layout_inline.FirstChild());
  return current_object_;
}

const LayoutObject* NGInlineCursor::CulledInlineTraversal::MoveToNext() {
  if (!current_object_)
    return nullptr;
  current_object_ =
      Find(current_object_->NextInPreOrderAfterChildren(layout_inline_));
  return current_object_;
}

void NGInlineCursor::MoveToFirstForCulledInline(
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

void NGInlineCursor::MoveToNextForCulledInline() {
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

void NGInlineCursor::MoveToIncludingCulledInline(
    const LayoutObject& layout_object) {
  DCHECK(layout_object.IsInLayoutNGInlineFormattingContext()) << layout_object;

  culled_inline_.Reset();
  MoveTo(layout_object);
  if (Current() || !HasRoot())
    return;

  // If this is a culled inline, find fragments for descendant |LayoutObject|s
  // that contribute to the culled inline.
  if (const LayoutInline* layout_inline =
          ToLayoutInlineOrNull(&layout_object)) {
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
  if (cursor.root_paint_fragment_) {
    DCHECK(!cursor.CurrentPaintFragment() ||
           cursor.CurrentPaintFragment()->Parent()->FirstChild() ==
               cursor.CurrentPaintFragment());
    for (NGInlineCursor sibling(cursor); sibling;
         sibling.MoveToNextSiblingPaintFragment())
      sibling_paint_fragments_.push_back(sibling.CurrentPaintFragment());
    current_index_ = sibling_paint_fragments_.size();
    if (current_index_)
      current_.paint_fragment_ = sibling_paint_fragments_[--current_index_];
    return;
  }
  if (cursor.IsItemCursor()) {
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
  if (const NGPaintFragment* current_paint_fragment = Current().PaintFragment())
    return NGInlineCursor(*current_paint_fragment);
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
    if (current_.paint_fragment_) {
      current_.paint_fragment_ = sibling_paint_fragments_[--current_index_];
      return;
    }
    if (current_.item_) {
      current_.Set(sibling_item_iterators_[--current_index_]);
      return;
    }
    NOTREACHED();
  }
  current_.paint_fragment_ = nullptr;
  current_.item_ = nullptr;
}

std::ostream& operator<<(std::ostream& ostream, const NGInlineCursor& cursor) {
  if (cursor.IsNull())
    return ostream << "NGInlineCursor()";
  if (cursor.IsPaintFragmentCursor()) {
    return ostream << "NGInlineCursor(" << *cursor.CurrentPaintFragment()
                   << ")";
  }
  DCHECK(cursor.IsItemCursor());
  return ostream << "NGInlineCursor(" << *cursor.CurrentItem() << ")";
}

std::ostream& operator<<(std::ostream& ostream, const NGInlineCursor* cursor) {
  if (!cursor)
    return ostream << "<null>";
  return ostream << *cursor;
}

#if DCHECK_IS_ON()
void NGInlineCursor::CheckValid(const NGInlineCursorPosition& position) const {
  if (position.PaintFragment()) {
    DCHECK(root_paint_fragment_);
    DCHECK(
        position.PaintFragment()->IsDescendantOfNotSelf(*root_paint_fragment_));
  } else if (position.Item()) {
    DCHECK(IsItemCursor());
    const unsigned index = position.item_iter_ - items_.begin();
    DCHECK_LT(index, items_.size());
  }
}
#endif

}  // namespace blink
