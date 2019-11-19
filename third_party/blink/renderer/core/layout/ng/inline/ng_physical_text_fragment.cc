// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/inline/ng_physical_text_fragment.h"

#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/layout/geometry/logical_size.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_rect.h"
#include "third_party/blink/renderer/core/layout/layout_text_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_fragment_item.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_item.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_text_fragment_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_ink_overflow.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_view.h"

namespace blink {

namespace {

struct SameSizeAsNGPhysicalTextFragment : NGPhysicalFragment {
  void* pointers[3];
  unsigned offsets[2];
};

static_assert(sizeof(NGPhysicalTextFragment) ==
                  sizeof(SameSizeAsNGPhysicalTextFragment),
              "NGPhysicalTextFragment should stay small");

}  // anonymous namespace

NGPhysicalTextFragment::NGPhysicalTextFragment(
    const NGPhysicalTextFragment& source,
    unsigned start_offset,
    unsigned end_offset,
    scoped_refptr<const ShapeResultView> shape_result)
    : NGPhysicalFragment(
          source.GetMutableLayoutObject(),
          source.StyleVariant(),
          source.IsHorizontal()
              ? PhysicalSize{shape_result->SnappedWidth(), source.Size().height}
              : PhysicalSize{source.Size().width, shape_result->SnappedWidth()},
          kFragmentText,
          source.TextType()),
      text_(source.text_),
      start_offset_(start_offset),
      end_offset_(end_offset),
      shape_result_(std::move(shape_result)) {
  DCHECK_GE(start_offset_, source.StartOffset());
  DCHECK_LE(end_offset_, source.EndOffset());
  DCHECK(shape_result_ || IsFlowControl()) << *this;
  is_generated_text_ = source.is_generated_text_;
  ink_overflow_computed_ = false;
}

NGPhysicalTextFragment::NGPhysicalTextFragment(NGTextFragmentBuilder* builder)
    : NGPhysicalFragment(builder, kFragmentText, builder->text_type_),
      text_(builder->text_),
      start_offset_(builder->start_offset_),
      end_offset_(builder->end_offset_),
      shape_result_(std::move(builder->shape_result_)) {
  DCHECK(shape_result_ || IsFlowControl()) << *this;
  is_generated_text_ = builder->IsGeneratedText();
  ink_overflow_computed_ = false;
}

LayoutUnit NGPhysicalTextFragment::InlinePositionForOffset(
    unsigned offset,
    LayoutUnit (*round_function)(float),
    AdjustMidCluster adjust_mid_cluster) const {
  return NGFragmentItem(*this).InlinePositionForOffset(
      Text(), offset, round_function, adjust_mid_cluster);
}

// TODO(yosin): We should move |NGFragmentItem::InlinePositionForOffset" to
// "ng_fragment_item.cc"
// Compute the inline position from text offset, in logical coordinate relative
// to this fragment.
LayoutUnit NGFragmentItem::InlinePositionForOffset(
    StringView text,
    unsigned offset,
    LayoutUnit (*round_function)(float),
    AdjustMidCluster adjust_mid_cluster) const {
  DCHECK_GE(offset, StartOffset());
  DCHECK_LE(offset, EndOffset());
  DCHECK_EQ(text.length(), TextLength());

  offset -= StartOffset();
  if (TextShapeResult()) {
    // TODO(layout-dev): Move caret position out of ShapeResult and into a
    // separate support class that can take a ShapeResult or ShapeResultView.
    // Allows for better code separation and avoids the extra copy below.
    return round_function(
        TextShapeResult()->CreateShapeResult()->CaretPositionForOffset(
            offset, text, adjust_mid_cluster));
  }

  // This fragment is a flow control because otherwise ShapeResult exists.
  DCHECK(IsFlowControl());
  DCHECK_EQ(1u, text.length());
  if (!offset || UNLIKELY(IsRtl(Style().Direction())))
    return LayoutUnit();
  return IsHorizontal() ? Size().width : Size().height;
}

// TODO(yosin): We should move |NGFragmentItem::InlinePositionForOffset" to
// "ng_fragment_item.cc"
LayoutUnit NGFragmentItem::InlinePositionForOffset(StringView text,
                                                   unsigned offset) const {
  return InlinePositionForOffset(text, offset, LayoutUnit::FromFloatRound,
                                 AdjustMidCluster::kToEnd);
}

LayoutUnit NGPhysicalTextFragment::InlinePositionForOffset(
    unsigned offset) const {
  return NGFragmentItem(*this).InlinePositionForOffset(Text(), offset);
}

std::pair<LayoutUnit, LayoutUnit>
NGPhysicalTextFragment::LineLeftAndRightForOffsets(unsigned start_offset,
                                                   unsigned end_offset) const {
  return NGFragmentItem(*this).LineLeftAndRightForOffsets(Text(), start_offset,
                                                          end_offset);
}

// TODO(yosin): We should move |NGFragmentItem::InlinePositionForOffset" to
// "ng_fragment_item.cc"
std::pair<LayoutUnit, LayoutUnit> NGFragmentItem::LineLeftAndRightForOffsets(
    StringView text,
    unsigned start_offset,
    unsigned end_offset) const {
  DCHECK_LE(start_offset, EndOffset());
  DCHECK_GE(start_offset, StartOffset());
  DCHECK_LE(end_offset, EndOffset());

  const LayoutUnit start_position =
      InlinePositionForOffset(text, start_offset, LayoutUnit::FromFloatFloor,
                              AdjustMidCluster::kToStart);
  const LayoutUnit end_position = InlinePositionForOffset(
      text, end_offset, LayoutUnit::FromFloatCeil, AdjustMidCluster::kToEnd);

  // Swap positions if RTL.
  return (UNLIKELY(start_position > end_position))
             ? std::make_pair(end_position, start_position)
             : std::make_pair(start_position, end_position);
}

PhysicalRect NGPhysicalTextFragment::LocalRect(unsigned start_offset,
                                               unsigned end_offset) const {
  return NGFragmentItem(*this).LocalRect(Text(), start_offset, end_offset);
}

// TODO(yosin): We should move |NGFragmentItem::InlinePositionForOffset" to
// "ng_fragment_item.cc"
PhysicalRect NGFragmentItem::LocalRect(StringView text,
                                       unsigned start_offset,
                                       unsigned end_offset) const {
  if (start_offset == StartOffset() && end_offset == EndOffset())
    return LocalRect();
  LayoutUnit start_position, end_position;
  std::tie(start_position, end_position) =
      LineLeftAndRightForOffsets(text, start_offset, end_offset);
  const LayoutUnit inline_size = end_position - start_position;
  switch (GetWritingMode()) {
    case WritingMode::kHorizontalTb:
      return {start_position, LayoutUnit(), inline_size, Size().height};
    case WritingMode::kVerticalRl:
    case WritingMode::kVerticalLr:
    case WritingMode::kSidewaysRl:
      return {LayoutUnit(), start_position, Size().width, inline_size};
    case WritingMode::kSidewaysLr:
      return {LayoutUnit(), Size().height - end_position, Size().width,
              inline_size};
  }
  NOTREACHED();
  return {};
}

PhysicalRect NGPhysicalTextFragment::SelfInkOverflow() const {
  if (!ink_overflow_computed_)
    ComputeSelfInkOverflow();
  if (ink_overflow_)
    return ink_overflow_->self_ink_overflow;
  return LocalRect();
}

void NGPhysicalTextFragment::ComputeSelfInkOverflow() const {
  ink_overflow_computed_ = true;

  if (UNLIKELY(!shape_result_)) {
    ink_overflow_ = nullptr;
    return;
  }

  NGInkOverflow::ComputeTextInkOverflow(PaintInfo(), Style(), Size(),
                                        &ink_overflow_);
}

scoped_refptr<const NGPhysicalTextFragment>
NGPhysicalTextFragment::CloneAsHiddenForPaint() const {
  NGTextFragmentBuilder builder(*this);
  builder.SetIsHiddenForPaint(true);
  return builder.ToTextFragment();
}

scoped_refptr<const NGPhysicalTextFragment> NGPhysicalTextFragment::TrimText(
    unsigned new_start_offset,
    unsigned new_end_offset) const {
  DCHECK(shape_result_);
  DCHECK_GE(new_start_offset, StartOffset());
  DCHECK_GT(new_end_offset, new_start_offset);
  DCHECK_LE(new_end_offset, EndOffset());
  scoped_refptr<ShapeResultView> new_shape_result = ShapeResultView::Create(
      shape_result_.get(), new_start_offset, new_end_offset);
  return base::AdoptRef(new NGPhysicalTextFragment(
      *this, new_start_offset, new_end_offset, std::move(new_shape_result)));
}

unsigned NGPhysicalTextFragment::TextOffsetForPoint(
    const PhysicalOffset& point) const {
  const ComputedStyle& style = Style();
  const LayoutUnit& point_in_line_direction =
      style.IsHorizontalWritingMode() ? point.left : point.top;
  if (const ShapeResultView* shape_result = TextShapeResult()) {
    // TODO(layout-dev): Move caret logic out of ShapeResult into separate
    // support class for code health and to avoid this copy.
    return shape_result->CreateShapeResult()->CaretOffsetForHitTest(
               point_in_line_direction.ToFloat(), Text(), BreakGlyphs) +
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
  return DirectionFromLevel(BidiLevel());
}

}  // namespace blink
