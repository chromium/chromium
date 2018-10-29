// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_item.h"

#include "third_party/blink/renderer/core/layout/layout_inline.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/ng/ng_outline_utils.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_buffer.h"

namespace blink {
namespace {

const char* kNGInlineItemTypeStrings[] = {
    "Text",     "Control",  "AtomicInline",        "OpenTag",
    "CloseTag", "Floating", "OutOfFlowPositioned", "BidiControl"};

// Returns true if this inline box is "empty", i.e. if the node contains only
// empty items it will produce a single zero block-size line box.
//
// While the spec defines "non-zero margins, padding, or borders" prevents
// line boxes to be zero-height, tests indicate that only inline direction
// of them do so. https://drafts.csswg.org/css2/visuren.html
bool IsInlineBoxEmpty(const ComputedStyle& style,
                      const LayoutObject& layout_object) {
  if (style.BorderStart().NonZero() || !style.PaddingStart().IsZero() ||
      style.BorderEnd().NonZero() || !style.PaddingEnd().IsZero())
    return false;

  // Non-zero margin can prevent "empty" only in non-quirks mode.
  // https://quirks.spec.whatwg.org/#the-line-height-calculation-quirk
  if ((!style.MarginStart().IsZero() || !style.MarginEnd().IsZero()) &&
      !layout_object.GetDocument().InLineHeightQuirksMode())
    return false;

  return true;
}

// TODO(xiaochengh): Deduplicate with a similar function in ng_paint_fragment.cc
// ::before, ::after and ::first-letter can be hit test targets.
bool CanBeHitTestTargetPseudoNodeStyle(const ComputedStyle& style) {
  switch (style.StyleType()) {
    case kPseudoIdBefore:
    case kPseudoIdAfter:
    case kPseudoIdFirstLetter:
      return true;
    default:
      return false;
  }
}

}  // namespace

NGInlineItem::NGInlineItem(NGInlineItemType type,
                           unsigned start,
                           unsigned end,
                           const ComputedStyle* style,
                           LayoutObject* layout_object)
    : start_offset_(start),
      end_offset_(end),
      style_(style),
      layout_object_(layout_object),
      type_(type),
      script_(0),
      font_fallback_priority_(0),
      render_orientation_(0),
      bidi_level_(UBIDI_LTR),
      shape_options_(kPreContext | kPostContext),
      is_empty_item_(false),
      should_create_box_fragment_(false),
      style_variant_(static_cast<unsigned>(NGStyleVariant::kStandard)),
      end_collapse_type_(kNotCollapsible),
      is_end_collapsible_newline_(false),
      is_symbol_marker_(false),
      is_generated_(false) {
  DCHECK_GE(end, start);
  ComputeBoxProperties();
}

NGInlineItem::NGInlineItem(const NGInlineItem& other,
                           unsigned start,
                           unsigned end,
                           scoped_refptr<const ShapeResult> shape_result)
    : start_offset_(start),
      end_offset_(end),
      shape_result_(shape_result),
      style_(other.style_),
      layout_object_(other.layout_object_),
      type_(other.type_),
      script_(other.script_),
      font_fallback_priority_(other.font_fallback_priority_),
      render_orientation_(other.render_orientation_),
      bidi_level_(other.bidi_level_),
      shape_options_(other.shape_options_),
      is_empty_item_(other.is_empty_item_),
      should_create_box_fragment_(other.should_create_box_fragment_),
      style_variant_(other.style_variant_),
      end_collapse_type_(other.end_collapse_type_),
      is_end_collapsible_newline_(other.is_end_collapsible_newline_),
      is_symbol_marker_(other.is_symbol_marker_),
      is_generated_(other.is_generated_) {
  DCHECK_GE(end, start);
}

NGInlineItem::~NGInlineItem() = default;

void NGInlineItem::ComputeBoxProperties() {
  DCHECK(!is_empty_item_);
  DCHECK(!should_create_box_fragment_);

  if (type_ == NGInlineItem::kText || type_ == NGInlineItem::kAtomicInline ||
      type_ == NGInlineItem::kControl)
    return;

  if (type_ == NGInlineItem::kOpenTag) {
    DCHECK(style_ && layout_object_ && layout_object_->IsLayoutInline());
    if (style_->HasBoxDecorationBackground() || style_->HasPadding() ||
        style_->HasMargin()) {
      is_empty_item_ = IsInlineBoxEmpty(*style_, *layout_object_);
      should_create_box_fragment_ = true;
    } else {
      is_empty_item_ = true;
      should_create_box_fragment_ =
          ToLayoutBoxModelObject(layout_object_)->HasSelfPaintingLayer() ||
          style_->CanContainAbsolutePositionObjects() ||
          style_->CanContainFixedPositionObjects(false) ||
          NGOutlineUtils::HasPaintedOutline(*style_,
                                            layout_object_->GetNode()) ||
          ToLayoutBoxModelObject(layout_object_)
              ->ShouldApplyPaintContainment() ||
          ToLayoutBoxModelObject(layout_object_)
              ->ShouldApplyLayoutContainment() ||
          CanBeHitTestTargetPseudoNodeStyle(*style_);
    }
    return;
  }

  if (type_ == kListMarker) {
    is_empty_item_ = false;
    return;
  }

  is_empty_item_ = true;
}

const char* NGInlineItem::NGInlineItemTypeToString(int val) const {
  return kNGInlineItemTypeStrings[val];
}

UScriptCode NGInlineItem::Script() const {
  return script_ != kInvalidScript ? static_cast<UScriptCode>(script_)
                                   : USCRIPT_INVALID_CODE;
}

FontFallbackPriority NGInlineItem::GetFontFallbackPriority() const {
  return static_cast<enum FontFallbackPriority>(font_fallback_priority_);
}

OrientationIterator::RenderOrientation NGInlineItem::RenderOrientation() const {
  return static_cast<OrientationIterator::RenderOrientation>(
      render_orientation_);
}

RunSegmenter::RunSegmenterRange NGInlineItem::CreateRunSegmenterRange() const {
  return {start_offset_, end_offset_, Script(), RenderOrientation(),
          GetFontFallbackPriority()};
}

bool NGInlineItem::EqualsRunSegment(const NGInlineItem& other) const {
  return script_ == other.script_ &&
         font_fallback_priority_ == other.font_fallback_priority_ &&
         render_orientation_ == other.render_orientation_;
}

void NGInlineItem::SetRunSegment(const RunSegmenter::RunSegmenterRange& range) {
  DCHECK_EQ(Type(), NGInlineItem::kText);

  // Orientation should be set in a separate pass. See
  // NGInlineNode::SegmentScriptRuns().
  DCHECK_EQ(range.render_orientation, OrientationIterator::kOrientationKeep);

  script_ = static_cast<unsigned>(range.script);
  font_fallback_priority_ = static_cast<unsigned>(range.font_fallback_priority);

  // Ensure our bit fields are large enough by reading them back.
  DCHECK_EQ(range.script, Script());
  DCHECK_EQ(range.font_fallback_priority, GetFontFallbackPriority());
}

void NGInlineItem::SetFontOrientation(
    OrientationIterator::RenderOrientation orientation) {
  DCHECK_EQ(Type(), NGInlineItem::kText);

  // Ensure the value can fit in the bit field.
  DCHECK_LT(static_cast<unsigned>(orientation), 1u << 1);

  render_orientation_ = orientation != 0;
}

unsigned NGInlineItem::PopulateItemsFromRun(
    Vector<NGInlineItem>& items,
    unsigned index,
    const RunSegmenter::RunSegmenterRange& range) {
  DCHECK_GE(range.end, items[index].start_offset_);

  for (;; index++) {
    NGInlineItem& item = items[index];
    DCHECK_LE(item.start_offset_, range.end);

    if (item.Type() == NGInlineItem::kText)
      item.SetRunSegment(range);

    if (range.end == item.end_offset_)
      break;
    if (range.end < item.end_offset_) {
      Split(items, index, range.end);
      break;
    }
  }
  return index + 1;
}

unsigned NGInlineItem::PopulateItemsFromFontOrientation(
    Vector<NGInlineItem>& items,
    unsigned index,
    unsigned end_offset,
    OrientationIterator::RenderOrientation orientation) {
  // FontOrientaiton is set per item, end_offset should be within this item.
  NGInlineItem& item = items[index];
  DCHECK_GE(end_offset, item.start_offset_);
  DCHECK_LE(end_offset, item.end_offset_);

  item.SetFontOrientation(orientation);

  if (end_offset < item.end_offset_)
    Split(items, index, end_offset);
  return index + 1;
}

void NGInlineItem::SetBidiLevel(UBiDiLevel level) {
  // Invalidate ShapeResult because it depends on the resolved direction.
  if (DirectionFromLevel(level) != DirectionFromLevel(bidi_level_))
    shape_result_ = nullptr;
  bidi_level_ = level;
}

// Set bidi level to a list of NGInlineItem from |index| to the item that ends
// with |end_offset|.
// If |end_offset| is mid of an item, the item is split to ensure each item has
// one bidi level.
// @param items The list of NGInlineItem.
// @param index The first index of the list to set.
// @param end_offset The exclusive end offset to set.
// @param level The level to set.
// @return The index of the next item.
unsigned NGInlineItem::SetBidiLevel(Vector<NGInlineItem>& items,
                                    unsigned index,
                                    unsigned end_offset,
                                    UBiDiLevel level) {
  for (; items[index].end_offset_ < end_offset; index++)
    items[index].SetBidiLevel(level);
  items[index].SetBidiLevel(level);

  if (items[index].end_offset_ == end_offset) {
    // Let close items have the same bidi-level as the previous item.
    while (index + 1 < items.size() &&
           items[index + 1].Type() == NGInlineItem::kCloseTag) {
      items[++index].SetBidiLevel(level);
    }
  } else {
    Split(items, index, end_offset);
  }

  return index + 1;
}

UBiDiLevel NGInlineItem::BidiLevelForReorder() const {
  // List markers should not be reordered to protect it from being included into
  // unclosed inline boxes.
  return Type() != NGInlineItem::kListMarker ? BidiLevel() : 0;
}

String NGInlineItem::ToString() const {
  return String::Format("NGInlineItem. Type: '%s'. LayoutObject: '%s'",
                        NGInlineItemTypeToString(Type()),
                        GetLayoutObject()->DebugName().Ascii().data());
}

// Split |items[index]| to 2 items at |offset|.
// All properties other than offsets are copied to the new item and it is
// inserted at |items[index + 1]|.
// @param items The list of NGInlineItem.
// @param index The index to split.
// @param offset The offset to split at.
void NGInlineItem::Split(Vector<NGInlineItem>& items,
                         unsigned index,
                         unsigned offset) {
  DCHECK_GT(offset, items[index].start_offset_);
  DCHECK_LT(offset, items[index].end_offset_);
  items[index].shape_result_ = nullptr;
  items.insert(index + 1, items[index]);
  items[index].end_offset_ = offset;
  items[index + 1].start_offset_ = offset;
}

void NGInlineItem::SetOffset(unsigned start, unsigned end) {
  DCHECK_GE(end, start);
  start_offset_ = start;
  end_offset_ = end;
  // Any modification to the offset will invalidate the shape result.
  shape_result_ = nullptr;
}

void NGInlineItem::SetEndOffset(unsigned end_offset) {
  DCHECK_GE(end_offset, start_offset_);
  end_offset_ = end_offset;
  // Any modification to the offset will invalidate the shape result.
  shape_result_ = nullptr;
}

bool NGInlineItem::HasStartEdge() const {
  DCHECK(Type() == kOpenTag || Type() == kCloseTag);
  // TODO(kojii): Should use break token when NG has its own tree building.
  return !GetLayoutObject()->IsInlineElementContinuation();
}

bool NGInlineItem::HasEndEdge() const {
  DCHECK(Type() == kOpenTag || Type() == kCloseTag);
  // TODO(kojii): Should use break token when NG has its own tree building.
  return !GetLayoutObject()->IsLayoutInline() ||
         !ToLayoutInline(GetLayoutObject())->Continuation();
}

void NGInlineItem::SetEndCollapseType(NGCollapseType type) {
  DCHECK(Type() == NGInlineItem::kText || type == kOpaqueToCollapsing ||
         (Type() == NGInlineItem::kControl && type == kCollapsible));
  end_collapse_type_ = type;
}

void NGInlineItem::SetEndCollapseType(NGCollapseType type, bool is_newline) {
  SetEndCollapseType(type);
  is_end_collapsible_newline_ = is_newline;
}

}  // namespace blink
