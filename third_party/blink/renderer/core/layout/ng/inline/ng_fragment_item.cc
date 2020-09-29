// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/inline/ng_fragment_item.h"

#include "third_party/blink/renderer/core/editing/bidi_adjustment.h"
#include "third_party/blink/renderer/core/editing/position_with_affinity.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_caret_position.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_fragment_items_builder.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_cursor.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_item.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_item_result.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/platform/wtf/size_assertions.h"

namespace blink {

namespace {

struct SameSizeAsNGFragmentItem {
  struct {
    void* pointer;
    NGTextOffset text_offset;
  } type_data;
  PhysicalRect rect;
  NGInkOverflow ink_overflow;
  void* pointer;
  wtf_size_t sizes[2];
  unsigned flags;
};

ASSERT_SIZE(NGFragmentItem, SameSizeAsNGFragmentItem);

}  // namespace

NGFragmentItem::NGFragmentItem(const NGPhysicalTextFragment& text)
    : layout_object_(text.GetLayoutObject()),
      text_({text.TextShapeResult(), text.TextOffset()}),
      rect_({PhysicalOffset(), text.Size()}),
      type_(kText),
      sub_type_(static_cast<unsigned>(text.TextType())),
      style_variant_(static_cast<unsigned>(text.StyleVariant())),
      is_hidden_for_paint_(text.IsHiddenForPaint()),
      text_direction_(static_cast<unsigned>(text.ResolvedDirection())),
      ink_overflow_type_(NGInkOverflow::kNotSet),
      is_dirty_(false),
      is_last_for_node_(true) {
#if DCHECK_IS_ON()
  if (text_.shape_result) {
    DCHECK_EQ(text_.shape_result->StartIndex(), StartOffset());
    DCHECK_EQ(text_.shape_result->EndIndex(), EndOffset());
  }
#endif
  if (text.TextType() == NGTextType::kLayoutGenerated) {
    type_ = kGeneratedText;
    // Note: Because of |text_| and |generated_text_| are in same union and
    // we initialize |text_| instead of |generated_text_|, we should construct
    // |generated_text_.text_| instead copying, |generated_text_.text = ...|.
    new (&generated_text_.text) String(text.Text().ToString());
  }
  DCHECK(!IsFormattingContextRoot());
}

NGFragmentItem::NGFragmentItem(
    const NGInlineItem& inline_item,
    scoped_refptr<const ShapeResultView> shape_result,
    const NGTextOffset& text_offset,
    const PhysicalSize& size,
    bool is_hidden_for_paint)
    : layout_object_(inline_item.GetLayoutObject()),
      text_({std::move(shape_result), text_offset}),
      rect_({PhysicalOffset(), size}),
      type_(kText),
      sub_type_(static_cast<unsigned>(inline_item.TextType())),
      style_variant_(static_cast<unsigned>(inline_item.StyleVariant())),
      is_hidden_for_paint_(is_hidden_for_paint),
      text_direction_(static_cast<unsigned>(inline_item.Direction())),
      ink_overflow_type_(NGInkOverflow::kNotSet),
      is_dirty_(false),
      is_last_for_node_(true) {
#if DCHECK_IS_ON()
  if (text_.shape_result) {
    DCHECK_EQ(text_.shape_result->StartIndex(), StartOffset());
    DCHECK_EQ(text_.shape_result->EndIndex(), EndOffset());
  }
#endif
  DCHECK_NE(TextType(), NGTextType::kLayoutGenerated);
  DCHECK(!IsFormattingContextRoot());
}

NGFragmentItem::NGFragmentItem(
    const NGInlineItem& inline_item,
    scoped_refptr<const ShapeResultView> shape_result,
    const String& text_content,
    const PhysicalSize& size,
    bool is_hidden_for_paint)
    : layout_object_(inline_item.GetLayoutObject()),
      generated_text_({std::move(shape_result), text_content}),
      rect_({PhysicalOffset(), size}),
      type_(kGeneratedText),
      sub_type_(static_cast<unsigned>(inline_item.TextType())),
      style_variant_(static_cast<unsigned>(inline_item.StyleVariant())),
      is_hidden_for_paint_(is_hidden_for_paint),
      text_direction_(static_cast<unsigned>(inline_item.Direction())),
      ink_overflow_type_(NGInkOverflow::kNotSet),
      is_dirty_(false),
      is_last_for_node_(true) {
#if DCHECK_IS_ON()
  if (text_.shape_result) {
    DCHECK_EQ(text_.shape_result->StartIndex(), StartOffset());
    DCHECK_EQ(text_.shape_result->EndIndex(), EndOffset());
  }
#endif
  DCHECK_EQ(TextType(), NGTextType::kLayoutGenerated);
  DCHECK(!IsFormattingContextRoot());
}

NGFragmentItem::NGFragmentItem(const NGPhysicalLineBoxFragment& line)
    : layout_object_(line.ContainerLayoutObject()),
      line_({&line, /* descendants_count */ 1}),
      rect_({PhysicalOffset(), line.Size()}),
      type_(kLine),
      sub_type_(static_cast<unsigned>(line.LineBoxType())),
      style_variant_(static_cast<unsigned>(line.StyleVariant())),
      is_hidden_for_paint_(false),
      text_direction_(static_cast<unsigned>(line.BaseDirection())),
      ink_overflow_type_(NGInkOverflow::kNotSet),
      is_dirty_(false),
      is_last_for_node_(true) {
  DCHECK(!IsFormattingContextRoot());
}

NGFragmentItem::NGFragmentItem(const NGPhysicalBoxFragment& box,
                               TextDirection resolved_direction)
    : layout_object_(box.GetLayoutObject()),
      box_(&box, /* descendants_count */ 1),
      rect_({PhysicalOffset(), box.Size()}),
      type_(kBox),
      style_variant_(static_cast<unsigned>(box.StyleVariant())),
      is_hidden_for_paint_(box.IsHiddenForPaint()),
      text_direction_(static_cast<unsigned>(resolved_direction)),
      ink_overflow_type_(NGInkOverflow::kNotSet),
      is_dirty_(false),
      is_last_for_node_(true) {
  DCHECK_EQ(IsFormattingContextRoot(), box.IsFormattingContextRoot());
}

NGFragmentItem::NGFragmentItem(NGLogicalLineItem&& line_item,
                               WritingMode writing_mode) {
  DCHECK(line_item.CanCreateFragmentItem());

  if (line_item.fragment) {
    new (this) NGFragmentItem(*line_item.fragment);
    return;
  }

  if (line_item.inline_item) {
    if (UNLIKELY(line_item.text_content)) {
      new (this) NGFragmentItem(
          *line_item.inline_item, std::move(line_item.shape_result),
          line_item.text_content,
          ToPhysicalSize(line_item.MarginSize(), writing_mode),
          line_item.is_hidden_for_paint);
      return;
    }

    new (this)
        NGFragmentItem(*line_item.inline_item,
                       std::move(line_item.shape_result), line_item.text_offset,
                       ToPhysicalSize(line_item.MarginSize(), writing_mode),
                       line_item.is_hidden_for_paint);
    return;
  }

  if (line_item.layout_result) {
    const NGPhysicalBoxFragment& box_fragment =
        To<NGPhysicalBoxFragment>(line_item.layout_result->PhysicalFragment());
    new (this) NGFragmentItem(box_fragment, line_item.ResolvedDirection());
    return;
  }

  // CanCreateFragmentItem()
  NOTREACHED();
  CHECK(false);
}

NGFragmentItem::NGFragmentItem(const NGFragmentItem& source)
    : layout_object_(source.layout_object_),
      rect_(source.rect_),
      fragment_id_(source.fragment_id_),
      delta_to_next_for_same_layout_object_(
          source.delta_to_next_for_same_layout_object_),
      type_(source.type_),
      sub_type_(source.sub_type_),
      style_variant_(source.style_variant_),
      is_hidden_for_paint_(source.is_hidden_for_paint_),
      text_direction_(source.text_direction_),
      ink_overflow_type_(NGInkOverflow::kNotSet),
      is_dirty_(source.is_dirty_),
      is_last_for_node_(source.is_last_for_node_) {
  switch (Type()) {
    case kText:
      new (&text_) TextItem(source.text_);
      break;
    case kGeneratedText:
      new (&generated_text_) GeneratedTextItem(source.generated_text_);
      break;
    case kLine:
      new (&line_) LineItem(source.line_);
      break;
    case kBox:
      new (&box_) BoxItem(source.box_);
      break;
  }

  if (source.IsInkOverflowComputed()) {
    ink_overflow_type_ = source.InkOverflowType();
    new (&ink_overflow_)
        NGInkOverflow(source.InkOverflowType(), source.ink_overflow_);
  }
}

NGFragmentItem::NGFragmentItem(NGFragmentItem&& source)
    : layout_object_(source.layout_object_),
      rect_(source.rect_),
      ink_overflow_(source.InkOverflowType(), std::move(source.ink_overflow_)),
      fragment_id_(source.fragment_id_),
      delta_to_next_for_same_layout_object_(
          source.delta_to_next_for_same_layout_object_),
      type_(source.type_),
      sub_type_(source.sub_type_),
      style_variant_(source.style_variant_),
      is_hidden_for_paint_(source.is_hidden_for_paint_),
      text_direction_(source.text_direction_),
      ink_overflow_type_(source.ink_overflow_type_),
      is_dirty_(source.is_dirty_),
      is_last_for_node_(source.is_last_for_node_) {
  switch (Type()) {
    case kText:
      new (&text_) TextItem(std::move(source.text_));
      break;
    case kGeneratedText:
      new (&generated_text_)
          GeneratedTextItem(std::move(source.generated_text_));
      break;
    case kLine:
      new (&line_) LineItem(std::move(source.line_));
      break;
    case kBox:
      new (&box_) BoxItem(std::move(source.box_));
      break;
  }
}

NGFragmentItem::~NGFragmentItem() {
  switch (Type()) {
    case kText:
      text_.~TextItem();
      break;
    case kGeneratedText:
      generated_text_.~GeneratedTextItem();
      break;
    case kLine:
      line_.~LineItem();
      break;
    case kBox:
      box_.~BoxItem();
      break;
  }
  ink_overflow_.Reset(InkOverflowType());
}

bool NGFragmentItem::IsInlineBox() const {
  if (Type() == kBox) {
    if (const NGPhysicalBoxFragment* box = BoxFragment())
      return box->IsInlineBox();
    NOTREACHED();
  }
  return false;
}

bool NGFragmentItem::IsAtomicInline() const {
  if (Type() != kBox)
    return false;
  if (const NGPhysicalBoxFragment* box = BoxFragment())
    return box->IsAtomicInline();
  return false;
}

bool NGFragmentItem::IsFloating() const {
  if (const NGPhysicalBoxFragment* box = BoxFragment())
    return box->IsFloating();
  return false;
}

bool NGFragmentItem::IsEmptyLineBox() const {
  return LineBoxType() == NGLineBoxType::kEmptyLineBox;
}

bool NGFragmentItem::IsGeneratedText() const {
  if (Type() == kGeneratedText) {
    DCHECK_EQ(TextType(), NGTextType::kLayoutGenerated);
    return true;
  }
  DCHECK_NE(TextType(), NGTextType::kLayoutGenerated);
  if (Type() == kText)
    return GetLayoutObject()->IsStyleGenerated();
  NOTREACHED();
  return false;
}

bool NGFragmentItem::IsListMarker() const {
  return layout_object_ && layout_object_->IsLayoutNGOutsideListMarker();
}

bool NGFragmentItem::HasNonVisibleOverflow() const {
  if (const NGPhysicalBoxFragment* fragment = BoxFragment())
    return fragment->HasNonVisibleOverflow();
  return false;
}

bool NGFragmentItem::IsScrollContainer() const {
  if (const NGPhysicalBoxFragment* fragment = BoxFragment())
    return fragment->IsScrollContainer();
  return false;
}

bool NGFragmentItem::HasSelfPaintingLayer() const {
  if (const NGPhysicalBoxFragment* fragment = BoxFragment())
    return fragment->HasSelfPaintingLayer();
  return false;
}

NGFragmentItem::BoxItem::BoxItem(const BoxItem& other)
    : box_fragment(other.box_fragment->PostLayout()),
      descendants_count(other.descendants_count) {}

NGFragmentItem::BoxItem::BoxItem(
    scoped_refptr<const NGPhysicalBoxFragment> box_fragment,
    wtf_size_t descendants_count)
    : box_fragment(std::move(box_fragment)),
      descendants_count(descendants_count) {}

const NGPhysicalBoxFragment* NGFragmentItem::BoxItem::PostLayout() const {
  if (box_fragment)
    return box_fragment->PostLayout();
  return nullptr;
}

void NGFragmentItem::LayoutObjectWillBeDestroyed() const {
  const_cast<NGFragmentItem*>(this)->layout_object_ = nullptr;
  if (const NGPhysicalBoxFragment* fragment = BoxFragment())
    fragment->LayoutObjectWillBeDestroyed();
}

void NGFragmentItem::LayoutObjectWillBeMoved() const {
  // When |Layoutobject| is moved out from the current IFC, we should not clear
  // the association with it in |ClearAssociatedFragments|, because the
  // |LayoutObject| may be moved to a different IFC and is already laid out
  // before clearing this IFC. This happens e.g., when split inlines moves
  // inline children into a child anonymous block.
  const_cast<NGFragmentItem*>(this)->layout_object_ = nullptr;
}

inline const LayoutBox* NGFragmentItem::InkOverflowOwnerBox() const {
  if (Type() == kBox)
    return ToLayoutBoxOrNull(GetLayoutObject());
  return nullptr;
}

inline LayoutBox* NGFragmentItem::MutableInkOverflowOwnerBox() {
  if (Type() == kBox)
    return ToLayoutBoxOrNull(const_cast<LayoutObject*>(layout_object_));
  return nullptr;
}

PhysicalRect NGFragmentItem::SelfInkOverflow() const {
  if (const LayoutBox* box = InkOverflowOwnerBox())
    return box->PhysicalSelfVisualOverflowRect();
  if (!HasInkOverflow())
    return LocalRect();
  return ink_overflow_.Self(InkOverflowType(), Size());
}

PhysicalRect NGFragmentItem::InkOverflow() const {
  if (const LayoutBox* box = InkOverflowOwnerBox())
    return box->PhysicalVisualOverflowRect();
  if (!HasInkOverflow())
    return LocalRect();
  if (!IsContainer() || HasNonVisibleOverflow())
    return ink_overflow_.Self(InkOverflowType(), Size());
  return ink_overflow_.SelfAndContents(InkOverflowType(), Size());
}

const ShapeResultView* NGFragmentItem::TextShapeResult() const {
  if (Type() == kText)
    return text_.shape_result.get();
  if (Type() == kGeneratedText)
    return generated_text_.shape_result.get();
  NOTREACHED();
  return nullptr;
}

NGTextOffset NGFragmentItem::TextOffset() const {
  if (Type() == kText)
    return text_.text_offset;
  if (Type() == kGeneratedText)
    return {0, generated_text_.text.length()};
  NOTREACHED();
  return {};
}

StringView NGFragmentItem::Text(const NGFragmentItems& items) const {
  if (Type() == kText) {
    return StringView(items.Text(UsesFirstLineStyle()), text_.text_offset.start,
                      text_.text_offset.Length());
  }
  if (Type() == kGeneratedText)
    return GeneratedText();
  NOTREACHED();
  return StringView();
}

NGTextFragmentPaintInfo NGFragmentItem::TextPaintInfo(
    const NGFragmentItems& items) const {
  if (Type() == kText) {
    return {items.Text(UsesFirstLineStyle()), text_.text_offset.start,
            text_.text_offset.end, text_.shape_result.get()};
  }
  if (Type() == kGeneratedText) {
    return {generated_text_.text, 0, generated_text_.text.length(),
            generated_text_.shape_result.get()};
  }
  NOTREACHED();
  return {};
}

TextDirection NGFragmentItem::BaseDirection() const {
  DCHECK_EQ(Type(), kLine);
  return static_cast<TextDirection>(text_direction_);
}

TextDirection NGFragmentItem::ResolvedDirection() const {
  DCHECK(Type() == kText || Type() == kGeneratedText || IsAtomicInline());
  return static_cast<TextDirection>(text_direction_);
}

String NGFragmentItem::ToString() const {
  // TODO(yosin): Once |NGPaintFragment| is removed, we should get rid of
  // following if-statements.
  // For ease of rebasing, we use same |DebugName()| as |NGPaintFrgment|.
  if (Type() == NGFragmentItem::kBox) {
    StringBuilder name;
    name.Append("NGPhysicalBoxFragment ");
    name.Append(layout_object_->DebugName());
    return name.ToString();
  }
  if (Type() == NGFragmentItem::kText) {
    StringBuilder name;
    name.Append("NGPhysicalTextFragment '");
    const NGFragmentItems* fragment_items = nullptr;
    if (const LayoutBlockFlow* block_flow =
            layout_object_->FragmentItemsContainer()) {
      for (unsigned i = 0; i < block_flow->PhysicalFragmentCount(); ++i) {
        const NGPhysicalBoxFragment* containing_fragment =
            block_flow->GetPhysicalFragment(i);
        fragment_items = containing_fragment->Items();
        if (fragment_items)
          break;
      }
    }
    if (fragment_items)
      name.Append(Text(*fragment_items));
    else
      name.Append("(container not found)");
    name.Append('\'');
    return name.ToString();
  }
  if (Type() == NGFragmentItem::kLine)
    return "NGPhysicalLineBoxFragment";
  return "NGFragmentItem";
}

PhysicalRect NGFragmentItem::LocalVisualRectFor(
    const LayoutObject& layout_object) {
  DCHECK(RuntimeEnabledFeatures::LayoutNGFragmentItemEnabled());
  DCHECK(layout_object.IsInLayoutNGInlineFormattingContext());

  PhysicalRect visual_rect;
  NGInlineCursor cursor;
  for (cursor.MoveTo(layout_object); cursor;
       cursor.MoveToNextForSameLayoutObject()) {
    DCHECK(cursor.Current().Item());
    const NGFragmentItem& item = *cursor.Current().Item();
    if (UNLIKELY(item.IsHiddenForPaint()))
      continue;
    PhysicalRect child_visual_rect = item.SelfInkOverflow();
    child_visual_rect.offset += item.OffsetInContainerBlock();
    visual_rect.Unite(child_visual_rect);
  }
  return visual_rect;
}

void NGFragmentItem::InvalidateInkOverflow() {
  ink_overflow_type_ = ink_overflow_.Invalidate(InkOverflowType());
}

PhysicalRect NGFragmentItem::RecalcInkOverflowForCursor(
    NGInlineCursor* cursor) {
  DCHECK(cursor);
  DCHECK(!cursor->Current() || cursor->IsAtFirst());
  PhysicalRect contents_ink_overflow;
  for (; *cursor; cursor->MoveToNextSkippingChildren()) {
    const NGFragmentItem* item = cursor->CurrentItem();
    DCHECK(item);
    if (UNLIKELY(item->IsLayoutObjectDestroyedOrMoved())) {
      // TODO(crbug.com/1099613): This should not happen, as long as it is
      // layout-clean. It looks like there are cases where the layout is dirty.
      NOTREACHED();
      continue;
    }
    if (UNLIKELY(item->HasSelfPaintingLayer()))
      continue;

    PhysicalRect child_rect;
    item->GetMutableForPainting().RecalcInkOverflow(*cursor, &child_rect);
    if (!child_rect.IsEmpty()) {
      child_rect.offset += item->OffsetInContainerBlock();
      contents_ink_overflow.Unite(child_rect);
    }
  }
  return contents_ink_overflow;
}

void NGFragmentItem::RecalcInkOverflow(
    const NGInlineCursor& cursor,
    PhysicalRect* self_and_contents_rect_out) {
  DCHECK_EQ(this, cursor.CurrentItem());

  if (UNLIKELY(IsLayoutObjectDestroyedOrMoved())) {
    // TODO(crbug.com/1099613): This should not happen, as long as it is really
    // layout-clean. It looks like there are cases where the layout is dirty.
    NOTREACHED();
    return;
  }

  if (IsText()) {
    // Re-computing text item is not necessary, because all changes that needs
    // to re-compute ink overflow invalidate layout. Except for box shadows,
    // text decorations and outlines that are invalidated before this point in
    // the code.
    if (IsInkOverflowComputed()) {
      *self_and_contents_rect_out = SelfInkOverflow();
      return;
    }

    NGTextFragmentPaintInfo paint_info = TextPaintInfo(cursor.Items());
    if (paint_info.shape_result) {
      ink_overflow_type_ = ink_overflow_.SetTextInkOverflow(
          InkOverflowType(), paint_info, Style(), Size(),
          self_and_contents_rect_out);
      return;
    }

    ink_overflow_type_ = ink_overflow_.Reset(InkOverflowType());
    *self_and_contents_rect_out = LocalRect();
    return;
  }

  // If this item has an owner |LayoutBox|, let it compute. It will call back NG
  // to compute and store the result to |LayoutBox|. Pre-paint requires ink
  // overflow to be stored in |LayoutBox|.
  if (LayoutBox* owner_box = MutableInkOverflowOwnerBox()) {
    DCHECK(!HasChildren());
    owner_box->RecalcNormalFlowChildVisualOverflowIfNeeded();
    *self_and_contents_rect_out = owner_box->PhysicalVisualOverflowRect();
    return;
  }

  // Re-compute descendants, then compute the contents ink overflow from them.
  NGInlineCursor descendants_cursor = cursor.CursorForDescendants();
  PhysicalRect contents_rect = RecalcInkOverflowForCursor(&descendants_cursor);

  // |contents_rect| is relative to the inline formatting context. Make it
  // relative to |this|.
  contents_rect.offset -= OffsetInContainerBlock();

  if (Type() == kLine) {
    // Line boxes don't have self overflow. Compute content overflow only.
    *self_and_contents_rect_out = contents_rect;
    ink_overflow_type_ =
        ink_overflow_.SetContents(InkOverflowType(), contents_rect, Size());
    return;
  }

  if (const NGPhysicalBoxFragment* box_fragment = BoxFragment()) {
    DCHECK(box_fragment->IsInlineBox());
    // Compute the self ink overflow.
    PhysicalRect self_rect = box_fragment->ComputeSelfInkOverflow();
    *self_and_contents_rect_out = UnionRect(self_rect, contents_rect);
    ink_overflow_type_ =
        ink_overflow_.Set(InkOverflowType(), self_rect, contents_rect, Size());
    return;
  }

  NOTREACHED();
}

void NGFragmentItem::SetDeltaToNextForSameLayoutObject(wtf_size_t delta) const {
  DCHECK_NE(Type(), kLine);
  delta_to_next_for_same_layout_object_ = delta;
}

PositionWithAffinity NGFragmentItem::PositionForPointInText(
    const PhysicalOffset& point,
    const NGInlineCursor& cursor) const {
  DCHECK_EQ(Type(), kText);
  DCHECK_EQ(cursor.CurrentItem(), this);
  if (IsGeneratedText())
    return PositionWithAffinity();
  const unsigned text_offset = TextOffsetForPoint(point, cursor.Items());
  const NGCaretPosition unadjusted_position{
      cursor, NGCaretPositionType::kAtTextOffset, text_offset};
  if (RuntimeEnabledFeatures::BidiCaretAffinityEnabled())
    return unadjusted_position.ToPositionInDOMTreeWithAffinity();
  if (text_offset > StartOffset() && text_offset < EndOffset())
    return unadjusted_position.ToPositionInDOMTreeWithAffinity();
  return BidiAdjustment::AdjustForHitTest(unadjusted_position)
      .ToPositionInDOMTreeWithAffinity();
}

unsigned NGFragmentItem::TextOffsetForPoint(
    const PhysicalOffset& point,
    const NGFragmentItems& items) const {
  DCHECK_EQ(Type(), kText);
  const ComputedStyle& style = Style();
  const LayoutUnit& point_in_line_direction =
      style.IsHorizontalWritingMode() ? point.left : point.top;
  if (const ShapeResultView* shape_result = TextShapeResult()) {
    // TODO(layout-dev): Move caret logic out of ShapeResult into separate
    // support class for code health and to avoid this copy.
    return shape_result->CreateShapeResult()->CaretOffsetForHitTest(
               point_in_line_direction.ToFloat(), Text(items), BreakGlyphs) +
           StartOffset();
  }

  // Flow control fragments such as forced line break, tabulation, soft-wrap
  // opportunities, etc. do not have ShapeResult.
  DCHECK(IsFlowControl());

  // Zero-inline-size objects such as newline always return the start offset.
  LogicalSize size = Size().ConvertToLogical(style.GetWritingMode());
  if (!size.inline_size)
    return StartOffset();

  // Sized objects such as tabulation returns the next offset if the given point
  // is on the right half.
  LayoutUnit inline_offset = IsLtr(ResolvedDirection())
                                 ? point_in_line_direction
                                 : size.inline_size - point_in_line_direction;
  DCHECK_EQ(1u, TextLength());
  return inline_offset <= size.inline_size / 2 ? StartOffset() : EndOffset();
}

std::ostream& operator<<(std::ostream& ostream, const NGFragmentItem& item) {
  ostream << "{";
  switch (item.Type()) {
    case NGFragmentItem::kText:
      ostream << "Text " << item.StartOffset() << "-" << item.EndOffset() << " "
              << (IsLtr(item.ResolvedDirection()) ? "LTR" : "RTL");
      break;
    case NGFragmentItem::kGeneratedText:
      ostream << "GeneratedText \"" << item.GeneratedText() << "\"";
      break;
    case NGFragmentItem::kLine:
      ostream << "Line #descendants=" << item.DescendantsCount() << " "
              << (IsLtr(item.BaseDirection()) ? "LTR" : "RTL");
      break;
    case NGFragmentItem::kBox:
      ostream << "Box #descendants=" << item.DescendantsCount();
      if (item.IsAtomicInline()) {
        ostream << " AtomicInline"
                << (IsLtr(item.ResolvedDirection()) ? "LTR" : "RTL");
      }
      break;
  }
  ostream << " ";
  switch (item.StyleVariant()) {
    case NGStyleVariant::kStandard:
      ostream << "Standard";
      break;
    case NGStyleVariant::kFirstLine:
      ostream << "FirstLine";
      break;
    case NGStyleVariant::kEllipsis:
      ostream << "Ellipsis";
      break;
  }
  return ostream << "}";
}

std::ostream& operator<<(std::ostream& ostream, const NGFragmentItem* item) {
  if (!item)
    return ostream << "<null>";
  return ostream << *item;
}

}  // namespace blink
