// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/inline/ng_physical_text_fragment.h"

#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/layout/layout_text_fragment.h"
#include "third_party/blink/renderer/core/layout/line/line_orientation_utils.h"
#include "third_party/blink/renderer/core/layout/ng/geometry/ng_logical_size.h"
#include "third_party/blink/renderer/core/layout/ng/geometry/ng_physical_offset_rect.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_item.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_text_fragment_builder.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {

namespace {

inline bool IsPhysicalTextFragmentAnonymousText(
    const LayoutObject* layout_object) {
  if (!layout_object)
    return false;
  if (layout_object->IsText() && ToLayoutText(layout_object)->IsTextFragment())
    return !ToLayoutTextFragment(layout_object)->AssociatedTextNode();
  const Node* node = layout_object->GetNode();
  return !node || node->IsPseudoElement();
}

NGLineOrientation ToLineOrientation(WritingMode writing_mode) {
  switch (writing_mode) {
    case WritingMode::kHorizontalTb:
      return NGLineOrientation::kHorizontal;
    case WritingMode::kVerticalRl:
    case WritingMode::kVerticalLr:
    case WritingMode::kSidewaysRl:
      return NGLineOrientation::kClockWiseVertical;
    case WritingMode::kSidewaysLr:
      return NGLineOrientation::kCounterClockWiseVertical;
  }
  NOTREACHED();
  return NGLineOrientation::kHorizontal;
}

}  // anonymous namespace

NGPhysicalTextFragment::NGPhysicalTextFragment(
    LayoutObject* layout_object,
    const ComputedStyle& style,
    NGStyleVariant style_variant,
    NGTextType text_type,
    const String& text,
    unsigned start_offset,
    unsigned end_offset,
    NGPhysicalSize size,
    NGLineOrientation line_orientation,
    NGTextEndEffect end_effect,
    scoped_refptr<const ShapeResult> shape_result)
    : NGPhysicalFragment(layout_object,
                         style,
                         style_variant,
                         size,
                         kFragmentText,
                         text_type),
      text_(text),
      start_offset_(start_offset),
      end_offset_(end_offset),
      shape_result_(shape_result),
      line_orientation_(static_cast<unsigned>(line_orientation)),
      end_effect_(static_cast<unsigned>(end_effect)),
      is_anonymous_text_(IsPhysicalTextFragmentAnonymousText(layout_object)) {
  DCHECK(shape_result_ || IsFlowControl()) << ToString();
  self_ink_overflow_ = ComputeSelfInkOverflow();
}

NGPhysicalTextFragment::NGPhysicalTextFragment(NGTextFragmentBuilder* builder)
    : NGPhysicalFragment(builder, kFragmentText, builder->text_type_),
      text_(builder->text_),
      start_offset_(builder->start_offset_),
      end_offset_(builder->end_offset_),
      shape_result_(std::move(builder->shape_result_)),
      line_orientation_(
          static_cast<unsigned>(ToLineOrientation(builder->GetWritingMode()))),
      end_effect_(static_cast<unsigned>(builder->end_effect_)),
      is_anonymous_text_(
          IsPhysicalTextFragmentAnonymousText(builder->layout_object_)) {
  DCHECK(shape_result_ || IsFlowControl()) << ToString();
  self_ink_overflow_ = ComputeSelfInkOverflow();
}

// Convert logical cooridnate to local physical coordinate.
NGPhysicalOffsetRect NGPhysicalTextFragment::ConvertToLocal(
    const LayoutRect& logical_rect) const {
  switch (LineOrientation()) {
    case NGLineOrientation::kHorizontal:
      return NGPhysicalOffsetRect(logical_rect);
    case NGLineOrientation::kClockWiseVertical:
      return {{size_.width - logical_rect.MaxY(), logical_rect.X()},
              {logical_rect.Height(), logical_rect.Width()}};
    case NGLineOrientation::kCounterClockWiseVertical:
      return {{logical_rect.Y(), size_.height - logical_rect.MaxX()},
              {logical_rect.Height(), logical_rect.Width()}};
  }
  NOTREACHED();
  return NGPhysicalOffsetRect(logical_rect);
}

// Compute the inline position from text offset, in logical coordinate relative
// to this fragment.
LayoutUnit NGPhysicalTextFragment::InlinePositionForOffset(
    unsigned offset,
    LayoutUnit (*round)(float),
    AdjustMidCluster adjust_mid_cluster) const {
  DCHECK_GE(offset, start_offset_);
  DCHECK_LE(offset, end_offset_);

  offset -= start_offset_;
  if (shape_result_) {
    return round(shape_result_->CaretPositionForOffset(offset, Text(),
                                                       adjust_mid_cluster));
  }

  // This fragment is a flow control because otherwise ShapeResult exists.
  DCHECK(IsFlowControl());
  DCHECK_EQ(1u, Length());
  if (!offset || UNLIKELY(IsRtl(Style().Direction())))
    return LayoutUnit();
  return IsHorizontal() ? Size().width : Size().height;
}

LayoutUnit NGPhysicalTextFragment::InlinePositionForOffset(
    unsigned offset) const {
  return InlinePositionForOffset(offset, LayoutUnit::FromFloatRound,
                                 AdjustMidCluster::kToEnd);
}

std::pair<LayoutUnit, LayoutUnit>
NGPhysicalTextFragment::LineLeftAndRightForOffsets(unsigned start_offset,
                                                   unsigned end_offset) const {
  DCHECK_LE(start_offset, end_offset);
  DCHECK_GE(start_offset, start_offset_);
  DCHECK_LE(end_offset, end_offset_);

  const LayoutUnit start_position = InlinePositionForOffset(
      start_offset, LayoutUnit::FromFloatFloor, AdjustMidCluster::kToStart);
  const LayoutUnit end_position = InlinePositionForOffset(
      end_offset, LayoutUnit::FromFloatCeil, AdjustMidCluster::kToEnd);

  // Swap positions if RTL.
  return (UNLIKELY(start_position > end_position))
             ? std::make_pair(end_position, start_position)
             : std::make_pair(start_position, end_position);
}

NGPhysicalOffsetRect NGPhysicalTextFragment::LocalRect(
    unsigned start_offset,
    unsigned end_offset) const {
  LayoutUnit start_position, end_position;
  std::tie(start_position, end_position) =
      LineLeftAndRightForOffsets(start_offset, end_offset);
  const LayoutUnit inline_size = end_position - start_position;
  switch (LineOrientation()) {
    case NGLineOrientation::kHorizontal:
      return {{start_position, LayoutUnit()}, {inline_size, Size().height}};
    case NGLineOrientation::kClockWiseVertical:
      return {{LayoutUnit(), start_position}, {Size().width, inline_size}};
    case NGLineOrientation::kCounterClockWiseVertical:
      return {{LayoutUnit(), Size().height - end_position},
              {Size().width, inline_size}};
  }
  NOTREACHED();
  return {};
}

NGPhysicalOffsetRect NGPhysicalTextFragment::ComputeSelfInkOverflow() const {
  if (UNLIKELY(!shape_result_))
    return LocalRect();

  // Glyph bounds is in logical coordinate, origin at the alphabetic baseline.
  LayoutRect ink_overflow = EnclosingLayoutRect(shape_result_->Bounds());

  // Make the origin at the logical top of this fragment.
  const ComputedStyle& style = Style();
  const Font& font = style.GetFont();
  if (const SimpleFontData* font_data = font.PrimaryFont()) {
    ink_overflow.SetY(
        ink_overflow.Y() +
        font_data->GetFontMetrics().FixedAscent(kAlphabeticBaseline));
  }

  if (float stroke_width = style.TextStrokeWidth()) {
    ink_overflow.Inflate(LayoutUnit::FromFloatCeil(stroke_width / 2.0f));
  }

  if (style.GetTextEmphasisMark() != TextEmphasisMark::kNone) {
    LayoutUnit emphasis_mark_height =
        LayoutUnit(font.EmphasisMarkHeight(style.TextEmphasisMarkString()));
    DCHECK_GT(emphasis_mark_height, LayoutUnit());
    if (style.GetTextEmphasisLineLogicalSide() == LineLogicalSide::kOver) {
      ink_overflow.ShiftYEdgeTo(
          std::min(ink_overflow.Y(), -emphasis_mark_height));
    } else {
      LayoutUnit logical_height =
          style.IsHorizontalWritingMode() ? Size().height : Size().width;
      ink_overflow.ShiftMaxYEdgeTo(
          std::max(ink_overflow.MaxY(), logical_height + emphasis_mark_height));
    }
  }

  if (ShadowList* text_shadow = style.TextShadow()) {
    LayoutRectOutsets text_shadow_logical_outsets =
        LineOrientationLayoutRectOutsets(
            LayoutRectOutsets(text_shadow->RectOutsetsIncludingOriginal()),
            style.GetWritingMode());
    text_shadow_logical_outsets.ClampNegativeToZero();
    ink_overflow.Expand(text_shadow_logical_outsets);
  }

  ink_overflow = LayoutRect(EnclosingIntRect(ink_overflow));

  // Uniting the frame rect ensures that non-ink spaces such side bearings, or
  // even space characters, are included in the visual rect for decorations.
  NGPhysicalOffsetRect local_ink_overflow = ConvertToLocal(ink_overflow);
  local_ink_overflow.Unite(LocalRect());
  return local_ink_overflow;
}

scoped_refptr<const NGPhysicalFragment> NGPhysicalTextFragment::TrimText(
    unsigned new_start_offset,
    unsigned new_end_offset) const {
  DCHECK(shape_result_);
  DCHECK_GE(new_start_offset, StartOffset());
  DCHECK_GT(new_end_offset, new_start_offset);
  DCHECK_LE(new_end_offset, EndOffset());
  scoped_refptr<ShapeResult> new_shape_result =
      shape_result_->SubRange(new_start_offset, new_end_offset);
  LayoutUnit new_inline_size = new_shape_result->SnappedWidth();
  return base::AdoptRef(new NGPhysicalTextFragment(
      layout_object_, Style(), static_cast<NGStyleVariant>(style_variant_),
      TextType(), text_, new_start_offset, new_end_offset,
      IsHorizontal() ? NGPhysicalSize{new_inline_size, size_.height}
                     : NGPhysicalSize{size_.width, new_inline_size},
      LineOrientation(), EndEffect(), std::move(new_shape_result)));
}

unsigned NGPhysicalTextFragment::TextOffsetForPoint(
    const NGPhysicalOffset& point) const {
  const ComputedStyle& style = Style();
  const LayoutUnit& point_in_line_direction =
      style.IsHorizontalWritingMode() ? point.left : point.top;
  if (const ShapeResult* shape_result = TextShapeResult()) {
    return shape_result->CaretOffsetForHitTest(
               point_in_line_direction.ToFloat(), Text(), BreakGlyphs) +
           StartOffset();
  }

  // Flow control fragments such as forced line break, tabulation, soft-wrap
  // opportunities, etc. do not have ShapeResult.
  DCHECK(IsFlowControl());

  // Zero-inline-size objects such as newline always return the start offset.
  NGLogicalSize size = Size().ConvertToLogical(style.GetWritingMode());
  if (!size.inline_size)
    return StartOffset();

  // Sized objects such as tabulation returns the next offset if the given point
  // is on the right half.
  LayoutUnit inline_offset = IsLtr(ResolvedDirection())
                                 ? point_in_line_direction
                                 : size.inline_size - point_in_line_direction;
  DCHECK_EQ(1u, Length());
  return inline_offset <= size.inline_size / 2 ? StartOffset() : EndOffset();
}

UBiDiLevel NGPhysicalTextFragment::BidiLevel() const {
  // TODO(xiaochengh): Make the implementation more efficient with, e.g.,
  // binary search and/or LayoutNGText::InlineItems().
  const auto& items = InlineItemsOfContainingBlock();
  const NGInlineItem* containing_item = std::find_if(
      items.begin(), items.end(), [this](const NGInlineItem& item) {
        return item.StartOffset() <= StartOffset() &&
               item.EndOffset() >= EndOffset();
      });
  DCHECK(containing_item);
  DCHECK_NE(containing_item, items.end());
  return containing_item->BidiLevel();
}

TextDirection NGPhysicalTextFragment::ResolvedDirection() const {
  if (TextShapeResult())
    return TextShapeResult()->Direction();
  return NGPhysicalFragment::ResolvedDirection();
}

}  // namespace blink
