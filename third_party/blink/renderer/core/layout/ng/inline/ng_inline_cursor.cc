// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_cursor.h"

#include "third_party/blink/renderer/core/editing/position_with_affinity.h"
#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/core/layout/layout_text.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_fragment_items.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_physical_line_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/paint/ng/ng_paint_fragment.h"
#include "third_party/blink/renderer/core/paint/ng/ng_paint_fragment_traversal.h"

namespace blink {

void NGInlineCursor::MoveToItem(const ItemsSpan::iterator& iter) {
  DCHECK(IsItemCursor());
  DCHECK(iter >= items_.begin() && iter <= items_.end());
  item_iter_ = iter;
  current_item_ = iter == items_.end() ? nullptr : iter->get();
}

void NGInlineCursor::SetRoot(const NGFragmentItems& fragment_items,
                             ItemsSpan items) {
  DCHECK(items.data() || !items.size());
  DCHECK(!HasRoot());
  fragment_items_ = &fragment_items;
  items_ = items;
  MoveToItem(items_.begin());
}

void NGInlineCursor::SetRoot(const NGFragmentItems& items) {
  SetRoot(items, items.Items());
}

void NGInlineCursor::SetRoot(const NGPaintFragment& root_paint_fragment) {
  DCHECK(&root_paint_fragment);
  DCHECK(!HasRoot());
  root_paint_fragment_ = &root_paint_fragment;
  current_paint_fragment_ = root_paint_fragment.FirstChild();
}

void NGInlineCursor::SetRoot(const LayoutBlockFlow& block_flow) {
  DCHECK(&block_flow);
  DCHECK(!HasRoot());

  if (const NGPhysicalBoxFragment* fragment = block_flow.CurrentFragment()) {
    if (const NGFragmentItems* items = fragment->Items()) {
      SetRoot(*items);
      return;
    }
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

NGInlineCursor::NGInlineCursor(const NGInlineCursor& other)
    : items_(other.items_),
      item_iter_(other.item_iter_),
      current_item_(other.current_item_),
      fragment_items_(other.fragment_items_),
      root_paint_fragment_(other.root_paint_fragment_),
      current_paint_fragment_(other.current_paint_fragment_),
      layout_inline_(other.layout_inline_) {}

NGInlineCursor::NGInlineCursor() = default;

bool NGInlineCursor::operator==(const NGInlineCursor& other) const {
  if (root_paint_fragment_) {
    return root_paint_fragment_ == other.root_paint_fragment_ &&
           current_paint_fragment_ == other.current_paint_fragment_;
  }
  if (current_item_ != other.current_item_)
    return false;
  DCHECK_EQ(items_.data(), other.items_.data());
  DCHECK_EQ(items_.size(), other.items_.size());
  DCHECK_EQ(fragment_items_, other.fragment_items_);
  DCHECK(item_iter_ == other.item_iter_);
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
    for (const auto& item : items_) {
      const LayoutObject* layout_object = item->GetLayoutObject();
      if (layout_object && layout_object->IsInline())
        return layout_object->RootInlineFormattingContext();
    }
  }
  NOTREACHED();
  return nullptr;
}

bool NGInlineCursor::HasChildren() const {
  if (current_paint_fragment_)
    return current_paint_fragment_->FirstChild();
  if (current_item_)
    return current_item_->HasChildren();
  NOTREACHED();
  return false;
}

NGInlineCursor NGInlineCursor::CursorForDescendants() const {
  if (current_paint_fragment_)
    return NGInlineCursor(*current_paint_fragment_);
  if (current_item_) {
    unsigned descendants_count = current_item_->DescendantsCount();
    if (descendants_count > 1) {
      DCHECK(fragment_items_);
      return NGInlineCursor(*fragment_items_, ItemsSpan(&*(item_iter_ + 1),
                                                        descendants_count - 1));
    }
    return NGInlineCursor();
  }
  NOTREACHED();
  return NGInlineCursor();
}

bool NGInlineCursor::HasSoftWrapToNextLine() const {
  DCHECK(IsLineBox());
  const NGInlineBreakToken& break_token = CurrentInlineBreakToken();
  return !break_token.IsFinished() && !break_token.IsForcedBreak();
}

bool NGInlineCursor::IsAtomicInline() const {
  if (current_paint_fragment_)
    return current_paint_fragment_->PhysicalFragment().IsAtomicInline();
  if (current_item_)
    return current_item_->IsAtomicInline();
  NOTREACHED();
  return false;
}

bool NGInlineCursor::IsEllipsis() const {
  if (current_paint_fragment_)
    return current_paint_fragment_->IsEllipsis();
  if (current_item_)
    return current_item_->IsEllipsis();
  NOTREACHED();
  return false;
}

bool NGInlineCursor::IsGeneratedText() const {
  if (current_paint_fragment_) {
    if (auto* text_fragment = DynamicTo<NGPhysicalTextFragment>(
            current_paint_fragment_->PhysicalFragment()))
      return text_fragment->IsGeneratedText();
    return false;
  }
  if (current_item_)
    return current_item_->IsGeneratedText();
  NOTREACHED();
  return false;
}

bool NGInlineCursor::IsGeneratedTextType() const {
  if (current_paint_fragment_) {
    if (auto* text_fragment = DynamicTo<NGPhysicalTextFragment>(
            current_paint_fragment_->PhysicalFragment())) {
      return text_fragment->TextType() ==
             NGPhysicalTextFragment::kGeneratedText;
    }
    return false;
  }
  if (current_item_)
    return current_item_->Type() == NGFragmentItem::kGeneratedText;
  NOTREACHED();
  return false;
}

bool NGInlineCursor::IsHiddenForPaint() const {
  if (current_paint_fragment_)
    return current_paint_fragment_->PhysicalFragment().IsHiddenForPaint();
  if (current_item_)
    return current_item_->IsHiddenForPaint();
  NOTREACHED();
  return false;
}

bool NGInlineCursor::IsHorizontal() const {
  return CurrentStyle().GetWritingMode() == WritingMode::kHorizontalTb;
}

bool NGInlineCursor::IsInlineLeaf() const {
  if (IsHiddenForPaint())
    return false;
  if (IsText())
    return !IsGeneratedTextType();
  if (!IsAtomicInline())
    return false;
  return !IsListMarker();
}

bool NGInlineCursor::IsInclusiveDescendantOf(
    const LayoutObject& layout_object) const {
  return CurrentLayoutObject() &&
         CurrentLayoutObject()->IsDescendantOf(&layout_object);
}

bool NGInlineCursor::IsLastLineInInlineBlock() const {
  DCHECK(IsLineBox());
  if (!GetLayoutBlockFlow()->IsAtomicInlineLevel())
    return false;
  NGInlineCursor next_sibling(*this);
  for (;;) {
    next_sibling.MoveToNextSibling();
    if (!next_sibling)
      return true;
    if (next_sibling.IsLineBox())
      return false;
    // There maybe other top-level objects such as floats, OOF, or list-markers.
  }
}

bool NGInlineCursor::IsLineBreak() const {
  if (current_paint_fragment_) {
    if (auto* text_fragment = DynamicTo<NGPhysicalTextFragment>(
            current_paint_fragment_->PhysicalFragment()))
      return text_fragment->IsLineBreak();
    return false;
  }
  if (current_item_)
    return IsText() && current_item_->IsLineBreak();
  NOTREACHED();
  return false;
}

bool NGInlineCursor::IsListMarker() const {
  if (current_paint_fragment_)
    return current_paint_fragment_->PhysicalFragment().IsListMarker();
  if (current_item_)
    return current_item_->IsListMarker();
  NOTREACHED();
  return false;
}

bool NGInlineCursor::IsText() const {
  if (current_paint_fragment_)
    return current_paint_fragment_->PhysicalFragment().IsText();
  if (current_item_)
    return current_item_->IsText();
  NOTREACHED();
  return false;
}

bool NGInlineCursor::IsBeforeSoftLineBreak() const {
  if (IsLineBreak())
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
  return line.CurrentBaseDirection() == CurrentResolvedDirection();
}

bool NGInlineCursor::CanHaveChildren() const {
  if (current_paint_fragment_)
    return current_paint_fragment_->PhysicalFragment().IsContainer();
  if (current_item_) {
    return current_item_->Type() == NGFragmentItem::kLine ||
           (current_item_->Type() == NGFragmentItem::kBox &&
            !current_item_->IsAtomicInline());
  }
  NOTREACHED();
  return false;
}

bool NGInlineCursor::IsEmptyLineBox() const {
  DCHECK(IsLineBox());
  if (current_paint_fragment_) {
    return To<NGPhysicalLineBoxFragment>(
               current_paint_fragment_->PhysicalFragment())
        .IsEmptyLineBox();
  }
  if (current_item_)
    return current_item_->IsEmptyLineBox();
  NOTREACHED();
  return false;
}

bool NGInlineCursor::IsLineBox() const {
  if (current_paint_fragment_)
    return current_paint_fragment_->PhysicalFragment().IsLineBox();
  if (current_item_)
    return current_item_->Type() == NGFragmentItem::kLine;
  NOTREACHED();
  return false;
}

TextDirection NGInlineCursor::CurrentBaseDirection() const {
  DCHECK(IsLineBox());
  if (current_paint_fragment_) {
    return To<NGPhysicalLineBoxFragment>(
               current_paint_fragment_->PhysicalFragment())
        .BaseDirection();
  }
  if (current_item_)
    return current_item_->BaseDirection();
  NOTREACHED();
  return TextDirection::kLtr;
}

UBiDiLevel NGInlineCursor::CurrentBidiLevel() const {
  if (IsText()) {
    if (IsGeneratedTextType()) {
      // TODO(yosin): Until we have clients, we don't support bidi-level for
      // ellipsis and soft hyphens.
      NOTREACHED() << this;
      return 0;
    }
    const LayoutText& layout_text = *ToLayoutText(CurrentLayoutObject());
    DCHECK(!layout_text.NeedsLayout()) << this;
    const auto* const items = layout_text.GetNGInlineItems();
    if (!items || items->size() == 0) {
      // In case of <br>, <wbr>, text-combine-upright, etc.
      return 0;
    }
    const auto& item = std::find_if(
        items->begin(), items->end(), [this](const NGInlineItem& item) {
          return item.StartOffset() <= CurrentTextStartOffset() &&
                 item.EndOffset() >= CurrentTextEndOffset();
        });
    DCHECK(item != items->end()) << this;
    return item->BidiLevel();
  }

  if (IsAtomicInline()) {
    const NGPhysicalBoxFragment* fragmentainer =
        CurrentLayoutObject()->ContainingBlockFlowFragment();
    DCHECK(fragmentainer);
    const LayoutBlockFlow& block_flow =
        *To<LayoutBlockFlow>(fragmentainer->GetLayoutObject());
    const Vector<NGInlineItem> items =
        block_flow.GetNGInlineNodeData()->ItemsData(UsesFirstLineStyle()).items;
    const LayoutObject* const layout_object = CurrentLayoutObject();
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

const NGPhysicalBoxFragment* NGInlineCursor::CurrentBoxFragment() const {
  if (current_paint_fragment_) {
    return DynamicTo<NGPhysicalBoxFragment>(
        &current_paint_fragment_->PhysicalFragment());
  }
  if (current_item_)
    return current_item_->BoxFragment();
  NOTREACHED();
  return nullptr;
}

const NGInlineBreakToken& NGInlineCursor::CurrentInlineBreakToken() const {
  DCHECK(IsLineBox());
  if (current_paint_fragment_) {
    return To<NGInlineBreakToken>(
        *To<NGPhysicalLineBoxFragment>(
             current_paint_fragment_->PhysicalFragment())
             .BreakToken());
  }
  DCHECK(current_item_);
  return *current_item_->InlineBreakToken();
}

const LayoutObject* NGInlineCursor::CurrentLayoutObject() const {
  if (current_paint_fragment_)
    return current_paint_fragment_->GetLayoutObject();
  if (current_item_)
    return current_item_->GetLayoutObject();
  NOTREACHED();
  return nullptr;
}

Node* NGInlineCursor::CurrentNode() const {
  if (const LayoutObject* layout_object = CurrentLayoutObject())
    return layout_object->GetNode();
  return nullptr;
}

const PhysicalRect NGInlineCursor::CurrentInkOverflow() const {
  if (current_paint_fragment_)
    return current_paint_fragment_->InkOverflow();
  if (current_item_)
    return current_item_->InkOverflow();
  NOTREACHED();
  return PhysicalRect();
}

const PhysicalOffset NGInlineCursor::CurrentOffset() const {
  if (current_paint_fragment_)
    return current_paint_fragment_->InlineOffsetToContainerBox();
  if (current_item_)
    return current_item_->Offset();
  NOTREACHED();
  return PhysicalOffset();
}

const PhysicalRect NGInlineCursor::CurrentRect() const {
  return PhysicalRect(CurrentOffset(), CurrentSize());
}

TextDirection NGInlineCursor::CurrentResolvedDirection() const {
  if (current_paint_fragment_)
    return current_paint_fragment_->PhysicalFragment().ResolvedDirection();
  if (current_item_)
    return current_item_->ResolvedDirection();
  NOTREACHED();
  return TextDirection::kLtr;
}

const PhysicalSize NGInlineCursor::CurrentSize() const {
  if (current_paint_fragment_)
    return current_paint_fragment_->Size();
  if (current_item_)
    return current_item_->Size();
  NOTREACHED();
  return PhysicalSize();
}

const ComputedStyle& NGInlineCursor::CurrentStyle() const {
  if (current_paint_fragment_)
    return current_paint_fragment_->Style();
  return current_item_->Style();
}

NGStyleVariant NGInlineCursor::CurrentStyleVariant() const {
  if (current_paint_fragment_)
    return current_paint_fragment_->PhysicalFragment().StyleVariant();
  return current_item_->StyleVariant();
}

bool NGInlineCursor::UsesFirstLineStyle() const {
  return CurrentStyleVariant() == NGStyleVariant::kFirstLine;
}

NGTextOffset NGInlineCursor::CurrentTextOffset() const {
  if (current_paint_fragment_) {
    const auto& text_fragment =
        To<NGPhysicalTextFragment>(current_paint_fragment_->PhysicalFragment());
    return {text_fragment.StartOffset(), text_fragment.EndOffset()};
  }
  if (current_item_)
    return {current_item_->StartOffset(), current_item_->EndOffset()};
  NOTREACHED();
  return {};
}

StringView NGInlineCursor::CurrentText() const {
  DCHECK(IsText());
  if (current_paint_fragment_) {
    return To<NGPhysicalTextFragment>(
               current_paint_fragment_->PhysicalFragment())
        .Text();
  }
  if (current_item_)
    return current_item_->Text(*fragment_items_);
  NOTREACHED();
  return "";
}

const ShapeResultView* NGInlineCursor::CurrentTextShapeResult() const {
  DCHECK(IsText());
  if (current_paint_fragment_) {
    return To<NGPhysicalTextFragment>(
               current_paint_fragment_->PhysicalFragment())
        .TextShapeResult();
  }
  if (current_item_)
    return current_item_->TextShapeResult();
  NOTREACHED();
  return nullptr;
}

PhysicalRect NGInlineCursor::CurrentLocalRect(unsigned start_offset,
                                              unsigned end_offset) const {
  DCHECK(IsText());
  if (current_paint_fragment_) {
    return To<NGPhysicalTextFragment>(
               current_paint_fragment_->PhysicalFragment())
        .LocalRect(start_offset, end_offset);
  }
  if (current_item_) {
    return current_item_->LocalRect(current_item_->Text(*fragment_items_),
                                    start_offset, end_offset);
  }
  NOTREACHED();
  return PhysicalRect();
}

LayoutUnit NGInlineCursor::InlinePositionForOffset(unsigned offset) const {
  DCHECK(IsText());
  if (current_paint_fragment_) {
    return To<NGPhysicalTextFragment>(
               current_paint_fragment_->PhysicalFragment())
        .InlinePositionForOffset(offset);
  }
  if (current_item_) {
    return current_item_->InlinePositionForOffset(
        current_item_->Text(*fragment_items_), offset);
  }
  NOTREACHED();
  return LayoutUnit();
}

PhysicalOffset NGInlineCursor::LineStartPoint() const {
  DCHECK(IsLineBox()) << this;
  const LogicalOffset logical_start;  // (0, 0)
  const PhysicalSize pixel_size(LayoutUnit(1), LayoutUnit(1));
  return logical_start.ConvertToPhysical(CurrentStyle().GetWritingMode(),
                                         CurrentBaseDirection(), CurrentSize(),
                                         pixel_size);
}

PhysicalOffset NGInlineCursor::LineEndPoint() const {
  DCHECK(IsLineBox()) << this;
  const LayoutUnit inline_size =
      IsHorizontal() ? CurrentSize().width : CurrentSize().height;
  const LogicalOffset logical_end(inline_size, LayoutUnit());
  const PhysicalSize pixel_size(LayoutUnit(1), LayoutUnit(1));
  return logical_end.ConvertToPhysical(CurrentStyle().GetWritingMode(),
                                       CurrentBaseDirection(), CurrentSize(),
                                       pixel_size);
}

PositionWithAffinity NGInlineCursor::PositionForPoint(
    const PhysicalOffset& point) const {
  if (current_paint_fragment_)
    return current_paint_fragment_->PositionForPoint(point);
  if (current_item_)
    return current_item_->PositionForPoint(point);
  NOTREACHED();
  return PositionWithAffinity();
}

void NGInlineCursor::MakeNull() {
  if (root_paint_fragment_) {
    current_paint_fragment_ = nullptr;
    return;
  }
  if (fragment_items_)
    return MoveToItem(items_.end());
}

void NGInlineCursor::InternalMoveTo(const LayoutObject& layout_object) {
  DCHECK(layout_object.IsInLayoutNGInlineFormattingContext());
  // If this cursor is rootless, find the root of the inline formatting context.
  if (!HasRoot()) {
    const LayoutBlockFlow& root = *layout_object.RootInlineFormattingContext();
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
  }
  if (fragment_items_) {
    item_iter_ = items_.begin();
    while (current_item_ && CurrentLayoutObject() != &layout_object)
      MoveToNextItem();
    return;
  }
  if (root_paint_fragment_) {
    const auto fragments = NGPaintFragment::InlineFragmentsFor(&layout_object);
    if (!fragments.IsInLayoutNGInlineFormattingContext() || fragments.IsEmpty())
      return MakeNull();
    return MoveTo(fragments.front());
  }
}

void NGInlineCursor::MoveTo(const LayoutObject& layout_object) {
  DCHECK(layout_object.IsInLayoutNGInlineFormattingContext()) << layout_object;
  InternalMoveTo(layout_object);
  if (*this || !HasRoot()) {
    layout_inline_ = nullptr;
    return;
  }

  // This |layout_object| did not produce any fragments.
  //
  // Try to find ancestors if this is a culled inline.
  layout_inline_ = ToLayoutInlineOrNull(&layout_object);
  if (!layout_inline_)
    return;

  MoveToFirst();
  while (IsNotNull() && !IsInclusiveDescendantOf(layout_object))
    MoveToNext();
}

void NGInlineCursor::MoveTo(const NGInlineCursor& cursor) {
  if (const NGPaintFragment* paint_fragment = cursor.CurrentPaintFragment()) {
    MoveTo(*paint_fragment);
    return;
  }
  if (cursor.current_item_) {
    if (!fragment_items_)
      SetRoot(*cursor.fragment_items_);
    // Note: We use address instead of iterato because we can't compare
    // iterators in different span. See |base::CheckedContiguousIterator<T>|.
    const ptrdiff_t index = &*cursor.item_iter_ - &*items_.begin();
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
  current_paint_fragment_ = &paint_fragment;
}

void NGInlineCursor::MoveToContainingLine() {
  DCHECK(!IsLineBox());
  if (current_paint_fragment_) {
    current_paint_fragment_ = current_paint_fragment_->ContainerLineBox();
    return;
  }
  if (current_item_) {
    do {
      MoveToPreviousItem();
    } while (current_item_ && !IsLineBox());
    return;
  }
  NOTREACHED();
}

void NGInlineCursor::MoveToFirst() {
  if (root_paint_fragment_) {
    current_paint_fragment_ = root_paint_fragment_->FirstChild();
    return;
  }
  if (IsItemCursor()) {
    MoveToItem(items_.begin());
    return;
  }
  NOTREACHED();
}

void NGInlineCursor::MoveToFirstChild() {
  DCHECK(CanHaveChildren());
  if (!TryToMoveToFirstChild())
    MakeNull();
}

void NGInlineCursor::MoveToFirstLogicalLeaf() {
  DCHECK(IsLineBox());
  // TODO(yosin): This isn't correct for mixed Bidi. Fix it. Besides, we
  // should compute and store it during layout.
  // TODO(yosin): We should check direction of each container instead of line
  // box.
  if (IsLtr(CurrentStyle().Direction())) {
    while (TryToMoveToFirstChild())
      continue;
    return;
  }
  while (TryToMoveToLastChild())
    continue;
}

void NGInlineCursor::MoveToLastChild() {
  DCHECK(CanHaveChildren());
  if (!TryToMoveToLastChild())
    MakeNull();
}

void NGInlineCursor::MoveToLastLogicalLeaf() {
  DCHECK(IsLineBox());
  // TODO(yosin): This isn't correct for mixed Bidi. Fix it. Besides, we
  // should compute and store it during layout.
  // TODO(yosin): We should check direction of each container instead of line
  // box.
  if (IsLtr(CurrentStyle().Direction())) {
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

void NGInlineCursor::MoveToNextForSameLayoutObject() {
  if (layout_inline_) {
    // Move to next fragment in culled inline box undef |layout_inline_|.
    do {
      MoveToNext();
    } while (IsNotNull() && !IsInclusiveDescendantOf(*layout_inline_));
    return;
  }
  if (current_paint_fragment_) {
    if (auto* paint_fragment =
            current_paint_fragment_->NextForSameLayoutObject()) {
      // |paint_fragment| can be in another fragment tree rooted by
      // |root_paint_fragment_|, e.g. "multicol-span-all-restyle-002.html"
      root_paint_fragment_ = paint_fragment->Root();
      return MoveTo(*paint_fragment);
    }
    return MakeNull();
  }
  if (current_item_) {
    const LayoutObject* const layout_object = CurrentLayoutObject();
    DCHECK(layout_object);
    do {
      MoveToNextItem();
    } while (current_item_ && CurrentLayoutObject() != layout_object);
    return;
  }
}

void NGInlineCursor::MoveToNextInlineLeaf() {
  if (IsNotNull() && IsInlineLeaf())
    MoveToNext();
  while (IsNotNull() && !IsInlineLeaf())
    MoveToNext();
}

void NGInlineCursor::MoveToNextInlineLeafIgnoringLineBreak() {
  do {
    MoveToNextInlineLeaf();
  } while (IsNotNull() && IsLineBreak());
}

void NGInlineCursor::MoveToNextLine() {
  DCHECK(IsLineBox());
  if (current_paint_fragment_) {
    if (auto* paint_fragment = current_paint_fragment_->NextSibling())
      return MoveTo(*paint_fragment);
    return MakeNull();
  }
  if (current_item_) {
    do {
      MoveToNextItem();
    } while (IsNotNull() && !IsLineBox());
    return;
  }
  NOTREACHED();
}

void NGInlineCursor::MoveToNextSibling() {
  if (current_paint_fragment_)
    return MoveToNextSiblingPaintFragment();
  return MoveToNextSiblingItem();
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
  if (IsNotNull() && IsInlineLeaf())
    MoveToPrevious();
  while (IsNotNull() && !IsInlineLeaf())
    MoveToPrevious();
}

void NGInlineCursor::MoveToPreviousInlineLeafIgnoringLineBreak() {
  do {
    MoveToPreviousInlineLeaf();
  } while (IsNotNull() && IsLineBreak());
}

void NGInlineCursor::MoveToPreviousLine() {
  // Note: List marker is sibling of line box.
  DCHECK(IsLineBox());
  if (current_paint_fragment_) {
    do {
      MoveToPreviousSiblingPaintFragment();
    } while (IsNotNull() && !IsLineBox());
    return;
  }
  if (current_item_) {
    do {
      MoveToPreviousItem();
    } while (IsNotNull() && !IsLineBox());
    return;
  }
  NOTREACHED();
}

bool NGInlineCursor::TryToMoveToFirstChild() {
  if (!HasChildren())
    return false;
  if (root_paint_fragment_) {
    MoveTo(*current_paint_fragment_->FirstChild());
    return true;
  }
  MoveToItem(item_iter_ + 1);
  return true;
}

bool NGInlineCursor::TryToMoveToLastChild() {
  if (!HasChildren())
    return false;
  if (root_paint_fragment_) {
    MoveTo(current_paint_fragment_->Children().back());
    return true;
  }
  const auto end = item_iter_ + CurrentItem()->DescendantsCount();
  MoveToNextItem();
  DCHECK(!IsNull());
  for (auto it = item_iter_ + 1; it != end; ++it) {
    if (CurrentItem()->HasSameParent(**it))
      MoveToItem(it);
  }
  return true;
}

void NGInlineCursor::MoveToNextItem() {
  DCHECK(IsItemCursor());
  if (UNLIKELY(!current_item_))
    return;
  DCHECK(item_iter_ != items_.end());
  ++item_iter_;
  MoveToItem(item_iter_);
}

void NGInlineCursor::MoveToNextItemSkippingChildren() {
  DCHECK(IsItemCursor());
  if (UNLIKELY(!current_item_))
    return;
  // If the current item has |DescendantsCount|, add it to move to the next
  // sibling, skipping all children and their descendants.
  if (wtf_size_t descendants_count = current_item_->DescendantsCount())
    return MoveToItem(item_iter_ + descendants_count);
  return MoveToNextItem();
}

void NGInlineCursor::MoveToNextSiblingItem() {
  DCHECK(IsItemCursor());
  if (UNLIKELY(!current_item_))
    return;
  const NGFragmentItem& item = *CurrentItem();
  MoveToNextItemSkippingChildren();
  if (IsNull() || item.HasSameParent(*CurrentItem()))
    return;
  MakeNull();
}

void NGInlineCursor::MoveToPreviousItem() {
  DCHECK(IsItemCursor());
  if (UNLIKELY(!current_item_))
    return;
  if (item_iter_ == items_.begin())
    return MakeNull();
  --item_iter_;
  current_item_ = item_iter_->get();
}

void NGInlineCursor::MoveToParentPaintFragment() {
  DCHECK(IsPaintFragmentCursor() && current_paint_fragment_);
  const NGPaintFragment* parent = current_paint_fragment_->Parent();
  if (parent && parent != root_paint_fragment_) {
    current_paint_fragment_ = parent;
    return;
  }
  current_paint_fragment_ = nullptr;
}

void NGInlineCursor::MoveToNextPaintFragment() {
  DCHECK(IsPaintFragmentCursor() && current_paint_fragment_);
  if (const NGPaintFragment* child = current_paint_fragment_->FirstChild()) {
    current_paint_fragment_ = child;
    return;
  }
  MoveToNextPaintFragmentSkippingChildren();
}

void NGInlineCursor::MoveToNextSiblingPaintFragment() {
  DCHECK(IsPaintFragmentCursor() && current_paint_fragment_);
  if (const NGPaintFragment* next = current_paint_fragment_->NextSibling()) {
    current_paint_fragment_ = next;
    return;
  }
  current_paint_fragment_ = nullptr;
}

void NGInlineCursor::MoveToNextPaintFragmentSkippingChildren() {
  DCHECK(IsPaintFragmentCursor() && current_paint_fragment_);
  while (current_paint_fragment_) {
    if (const NGPaintFragment* next = current_paint_fragment_->NextSibling()) {
      current_paint_fragment_ = next;
      return;
    }
    MoveToParentPaintFragment();
  }
}

void NGInlineCursor::MoveToPreviousPaintFragment() {
  DCHECK(IsPaintFragmentCursor() && current_paint_fragment_);
  const NGPaintFragment* const parent = current_paint_fragment_->Parent();
  MoveToPreviousSiblingPaintFragment();
  if (current_paint_fragment_) {
    while (TryToMoveToLastChild())
      continue;
    return;
  }
  current_paint_fragment_ = parent == root_paint_fragment_ ? nullptr : parent;
}

void NGInlineCursor::MoveToPreviousSiblingPaintFragment() {
  DCHECK(IsPaintFragmentCursor() && current_paint_fragment_);
  const NGPaintFragment* const current = current_paint_fragment_;
  current_paint_fragment_ = nullptr;
  for (auto* sibling : current->Parent()->Children()) {
    if (sibling == current)
      return;
    current_paint_fragment_ = sibling;
  }
  NOTREACHED();
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

}  // namespace blink
