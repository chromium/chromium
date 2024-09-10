// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/inline/inline_item.h"

#include "third_party/blink/renderer/core/layout/layout_inline.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_inline_text.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_buffer.h"
#include "third_party/blink/renderer/platform/wtf/size_assertions.h"

namespace blink {
namespace {

struct SameSizeAsInlineItem {
  UntracedMember<void*> members[2];
  unsigned integers[3];
  unsigned bit_fields : 32;
};

ASSERT_SIZE(InlineItem, SameSizeAsInlineItem);

// Returns true if this inline box is "empty", i.e. if the node contains only
// empty items it will produce a single zero block-size line box.
//
// While the spec defines "non-zero margins, padding, or borders" prevents
// line boxes to be zero-height, tests indicate that only inline direction
// of them do so. https://drafts.csswg.org/css2/visuren.html
bool IsInlineBoxStartEmpty(const ComputedStyle& style,
                           const LayoutObject& layout_object) {
  if (style.BorderInlineStartWidth() || !style.PaddingInlineStart().IsZero()) {
    return false;
  }

  // Non-zero margin can prevent "empty" only in non-quirks mode.
  // https://quirks.spec.whatwg.org/#the-line-height-calculation-quirk
  if (!style.MarginInlineStart().IsZero() &&
      !layout_object.GetDocument().InLineHeightQuirksMode()) {
    return false;
  }

  return true;
}

// Determines if the end of a box is "empty" as defined above.
//
// Keeping the "empty" state for start and end separately is important when they
// belong to different lines, as non-empty item can force the line it belongs to
// as non-empty.
bool IsInlineBoxEndEmpty(const ComputedStyle& style,
                         const LayoutObject& layout_object) {
  if (style.BorderInlineEndWidth() || !style.PaddingInlineEnd().IsZero()) {
    return false;
  }

  // Non-zero margin can prevent "empty" only in non-quirks mode.
  // https://quirks.spec.whatwg.org/#the-line-height-calculation-quirk
  if (!style.MarginInlineEnd().IsZero() &&
      !layout_object.GetDocument().InLineHeightQuirksMode()) {
    return false;
  }

  return true;
}

}  // namespace

InlineItem::InlineItem(InlineItemType type,
                       unsigned start,
                       unsigned end,
                       LayoutObject* layout_object)
    : start_offset_(start),
      end_offset_(end),
      // Use atomic construction to allow for concurrently marking InlineItem.
      layout_object_(layout_object,
                     Member<LayoutObject>::AtomicInitializerTag{}),
      type_(type) {
  DCHECK_GE(end, start);
  ComputeBoxProperties();
}

InlineItem::InlineItem(const InlineItem& other,
                       unsigned start,
                       unsigned end,
                       const ShapeResult* shape_result)
    : start_offset_(start),
      end_offset_(end),
      // Use atomic construction to allow for concurrently marking InlineItem.
      shape_result_(shape_result, Member<ShapeResult>::AtomicInitializerTag{}),
      layout_object_(other.layout_object_,
                     Member<LayoutObject>::AtomicInitializerTag{}),
      type_(other.type_),
      text_type_(other.text_type_),
      style_variant_(other.style_variant_),
      end_collapse_type_(other.end_collapse_type_),
      bidi_level_(other.bidi_level_),
      segment_data_(other.segment_data_),
      is_empty_item_(other.is_empty_item_),
      is_block_level_(other.is_block_level_),
      is_end_collapsible_newline_(other.is_end_collapsible_newline_),
      is_generated_for_line_break_(other.is_generated_for_line_break_),
      is_unsafe_to_reuse_shape_result_(other.is_unsafe_to_reuse_shape_result_) {
  DCHECK_GE(end, start);
}

InlineItem::InlineItem(const InlineItem& other)
    : InlineItem(other,
                 other.start_offset_,
                 other.end_offset_,
                 other.shape_result_.Get()) {}

InlineItem::~InlineItem() = default;

void InlineItem::ComputeBoxProperties() {
  DCHECK(!is_empty_item_);

  if (type_ == InlineItem::kText || type_ == InlineItem::kAtomicInline ||
      type_ == InlineItem::kControl) {
    return;
  }
  if (type_ == kInitialLetterBox) [[unlikely]] {
    return;
  }

  if (type_ == InlineItem::kOpenTag) {
    DCHECK(layout_object_ && layout_object_->IsLayoutInline());
    is_empty_item_ = IsInlineBoxStartEmpty(*Style(), *layout_object_);
    return;
  }

  if (type_ == InlineItem::kCloseTag) {
    DCHECK(layout_object_ && layout_object_->IsLayoutInline());
    is_empty_item_ = IsInlineBoxEndEmpty(*Style(), *layout_object_);
    return;
  }

  if (type_ == kBlockInInline) {
    // |is_empty_item_| can't be determined until this item is laid out.
    // |false| is a safer approximation.
    return;
  }

  if (type_ == kOutOfFlowPositioned || type_ == kFloating)
    is_block_level_ = true;

  is_empty_item_ = true;
}

const char* InlineItem::InlineItemTypeToString(InlineItemType val) const {
  switch (val) {
    case kText:
      return "Text";
    case kControl:
      return "Control";
    case kAtomicInline:
      return "AtomicInline";
    case kBlockInInline:
      return "BlockInInline";
    case kOpenTag:
      return "OpenTag";
    case kCloseTag:
      return "CloseTag";
    case kFloating:
      return "Floating";
    case kOutOfFlowPositioned:
      return "OutOfFlowPositioned";
    case kInitialLetterBox:
      return "InitialLetterBox";
    case kListMarker:
      return "ListMarker";
    case kBidiControl:
      return "BidiControl";
    case kOpenRubyColumn:
      return "OpenRubyColumn";
    case kCloseRubyColumn:
      return "CloseRubyColumn";
    case kRubyLinePlaceholder:
      return "RubyLinePlaceholder";
  }
  NOTREACHED();
}

void InlineItem::SetSegmentData(const RunSegmenter::RunSegmenterRange& range,
                                HeapVector<InlineItem>* items) {
  unsigned segment_data = InlineItemSegment::PackSegmentData(range);
  for (InlineItem& item : *items) {
    if (item.Type() == InlineItem::kText) {
      item.segment_data_ = segment_data;
    }
  }
}

// Set bidi level to a list of InlineItem from |index| to the item that ends
// with |end_offset|.
// If |end_offset| is mid of an item, the item is split to ensure each item has
// one bidi level.
// @param items The list of InlineItem.
// @param index The first index of the list to set.
// @param end_offset The exclusive end offset to set.
// @param level The level to set.
// @return The index of the next item.
unsigned InlineItem::SetBidiLevel(HeapVector<InlineItem>& items,
                                  unsigned index,
                                  unsigned end_offset,
                                  UBiDiLevel level) {
  for (; items[index].end_offset_ < end_offset; index++)
    items[index].SetBidiLevel(level);
  InlineItem* item = &items[index];
  item->SetBidiLevel(level);

  if (item->end_offset_ == end_offset) {
    // Let close items have the same bidi-level as the previous item.
    while (index + 1 < items.size() &&
           items[index + 1].Type() == InlineItem::kCloseTag) {
      items[++index].SetBidiLevel(level);
    }
  } else {
    // If a reused item needs to split, |SetNeedsLayout| to ensure the line is
    // not reused.
    LayoutObject* layout_object = item->GetLayoutObject();
    if (layout_object->EverHadLayout() && !layout_object->NeedsLayout())
      layout_object->SetNeedsLayout(layout_invalidation_reason::kStyleChange);

    Split(items, index, end_offset);
  }

  return index + 1;
}

const Font& InlineItem::FontWithSvgScaling() const {
  if (const auto* svg_text =
          DynamicTo<LayoutSVGInlineText>(layout_object_.Get())) {
    // We don't need to care about StyleVariant(). SVG 1.1 doesn't support
    // ::first-line.
    return svg_text->ScaledFont();
  }
  return Style()->GetFont();
}

String InlineItem::ToString() const {
  String object_info;
  if (const auto* layout_text = DynamicTo<LayoutText>(GetLayoutObject())) {
    object_info = layout_text->TransformedText().EncodeForDebugging();
  } else if (GetLayoutObject()) {
    object_info = GetLayoutObject()->ToString();
  }
  return String::Format("InlineItem %s. %s", InlineItemTypeToString(Type()),
                        object_info.Ascii().c_str());
}

// Split |items[index]| to 2 items at |offset|.
// All properties other than offsets are copied to the new item and it is
// inserted at |items[index + 1]|.
// @param items The list of InlineItem.
// @param index The index to split.
// @param offset The offset to split at.
void InlineItem::Split(HeapVector<InlineItem>& items,
                       unsigned index,
                       unsigned offset) {
  DCHECK_GT(offset, items[index].start_offset_);
  DCHECK_LT(offset, items[index].end_offset_);
  items[index].shape_result_ = nullptr;
  items.insert(index + 1, items[index]);
  items[index].end_offset_ = offset;
  items[index + 1].start_offset_ = offset;
}

#if DCHECK_IS_ON()
void InlineItem::CheckTextType(const String& text_content) const {
  const UChar character = Length() ? text_content[StartOffset()] : 0;
  switch (character) {
    case kNewlineCharacter:
      DCHECK_EQ(Length(), 1u);
      DCHECK_EQ(Type(), InlineItemType::kControl);
      DCHECK_EQ(TextType(), TextItemType::kForcedLineBreak);
      break;
    case kTabulationCharacter:
      DCHECK_EQ(Type(), InlineItemType::kControl);
      DCHECK_EQ(TextType(), TextItemType::kFlowControl);
      break;
    case kCarriageReturnCharacter:
    case kFormFeedCharacter:
    case kZeroWidthSpaceCharacter:
      if (Type() == InlineItemType::kControl) {
        DCHECK_EQ(Length(), 1u);
        DCHECK_EQ(TextType(), TextItemType::kFlowControl);
      } else {
        DCHECK_EQ(Type(), InlineItemType::kText);
        DCHECK_EQ(TextType(), TextItemType::kNormal);
      }
      break;
    default:
      DCHECK_NE(Type(), InlineItemType::kControl);
      DCHECK(TextType() == TextItemType::kNormal ||
             TextType() == TextItemType::kSymbolMarker);
      break;
  }
}
#endif

void InlineItem::Trace(Visitor* visitor) const {
  visitor->Trace(shape_result_);
  visitor->Trace(layout_object_);
}

}  // namespace blink
