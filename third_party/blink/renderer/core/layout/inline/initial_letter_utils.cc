// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/inline/initial_letter_utils.h"

#include "third_party/blink/renderer/core/layout/exclusions/exclusion_area.h"
#include "third_party/blink/renderer/core/layout/geometry/writing_mode_converter.h"
#include "third_party/blink/renderer/core/layout/inline/inline_cursor.h"
#include "third_party/blink/renderer/core/layout/inline/line_info.h"
#include "third_party/blink/renderer/core/layout/inline/line_utils.h"
#include "third_party/blink/renderer/core/layout/inline/logical_line_item.h"
#include "third_party/blink/renderer/core/layout/logical_fragment.h"
#include "third_party/blink/renderer/core/layout/physical_box_fragment.h"
#include "third_party/blink/renderer/platform/text/writing_direction_mode.h"

namespace blink {

namespace {

// Returns border-box block offset of initial letter box to contain initial
// letter text and shift down amount of surrounding text in
// `initial_letter_block_start_adjust`,
LayoutUnit ComputeInitialLetterBoxBlockOffset(
    const PhysicalBoxFragment& initial_letter_box_fragment,
    const LayoutUnit block_size,
    const ComputedStyle& initial_letter_box_style,
    const ComputedStyle& paragraph_style,
    LayoutUnit* initial_letter_block_start_adjust) {
  const auto& initial_letter = initial_letter_box_style.InitialLetter();
  DCHECK(!initial_letter.IsNormal());

  // Note: `line_height` may be smaller than actual line height due by
  // using taller inline item or fallback font, e.g. using Japanese text with
  // English font.
  const LayoutUnit line_height = paragraph_style.ComputedLineHeightAsFixed();

  const int size =
      std::ceil(RuntimeEnabledFeatures::InitialLetterRaiseBySpecifiedEnabled()
                    ? initial_letter.Size()
                    : block_size / line_height.ToFloat());
  const int sink = initial_letter.IsRaise() || initial_letter.IsIntegerSink()
                       ? initial_letter.Sink()
                       : size;

  // Block-axis positioning[1]:
  //   size >= sink: => under alignment with shift (sink - 1) * line_height
  //   size < sink   => over alignment
  // For `initial-letter-align: alphabetic`[2], over is cap-height and under is
  // baseline.
  //
  // Note: Even if the spec[3] says baseline of initial letter text to align
  // with n'th line of baseline. It is hard to do so, because the first line
  // has half leading but following lines are not, all lines have half leading
  // in quirk mode.
  // [1] https://drafts.csswg.org/css-inline/#initial-letter-block-position
  // [2]
  // https://drafts.csswg.org/css-inline-3/#valdef-initial-letter-align-alphabetic
  // [3] https://drafts.csswg.org/css-inline/#drop-initial

  if (size < sink) {
    // Example: `initial-letter: 5 7`
    //  *     * line 1                      line 1
    //  *     * line 2                      line 2
    //  ******* line 3  = sink =>   *     * line 3
    //  *     * line 4              *     * line 4
    //  *     * line 5              ******* line 5
    //  line 6                      *     * line 6
    //  line 7                      *     * line 7
    return line_height * sink - block_size;
  }

  // Shift down surrounding text
  // Example: `initial-letter: 5 2`
  //    *     * line 1              *     *
  //    *     * line 2              *     *
  //    ******* line 3  = shift =>  ******* line 1
  //    *     * line 4              *     * line 2
  //    *     * line 5              *     * line 3
  //    line 6                      line 4
  //    line 7                      line 5
  *initial_letter_block_start_adjust = line_height * (size - sink);

  if (paragraph_style.IsHorizontalWritingMode() ||
      initial_letter_box_style.GetTextOrientation() ==
          ETextOrientation::kSideways) {
    // `writing-mode: horizontal-tb` or `text-orientation: sideways`
    // `baseline` is ascent for not `vertical-lr`, descent for `vertical-lr`.
    const LayoutUnit baseline =
        initial_letter_box_fragment.FirstBaseline().value_or(LayoutUnit());
    const LayoutUnit ascent = paragraph_style.IsFlippedLinesWritingMode()
                                  ? block_size - baseline
                                  : baseline;
    const LayoutUnit block_offset =
        LayoutUnit(line_height * initial_letter.Size()) - ascent;
    const FontHeight text_metrics = paragraph_style.GetFontHeight();
    FontHeight line_metrics = text_metrics;
    FontHeight leading_space = CalculateLeadingSpace(
        paragraph_style.ComputedLineHeightAsFixed(), line_metrics);
    line_metrics.AddLeading(leading_space);
    const LayoutUnit descent = line_metrics.descent;
    return block_offset - descent;
  }

  // In vertical writing mode, `block_offset` will be physical offset x.
  // Align initial letter box in center.
  return (line_height * size - block_size) / 2;
}

// Returns left-top origin of text ink bounds.
LogicalRect ComputeTextInkBounds(const ShapeResultView& shape_result,
                                 const ComputedStyle& style,
                                 LayoutUnit* out_baseline = nullptr) {
  // The origin is the alphabetic baseline.
  const gfx::RectF text_ink_float_bounds = shape_result.ComputeInkBounds();

  // We get same ink bounds for all `writing-mode` aka logical rect.
  const LogicalRect text_ink_bounds =
      LogicalRect::EnclosingRect(text_ink_float_bounds);

  // Calculation of `ascent` should be as same as
  // `NGTextFragmentPainter::Paint()`
  const FontMetrics& font_metrics =
      style.GetFont().PrimaryFont()->GetFontMetrics();
  const int ascent = font_metrics.Ascent();
  if (out_baseline)
    *out_baseline = LayoutUnit(ascent);

  // Convert to left-top origin.
  return text_ink_bounds + LogicalOffset(LayoutUnit(), LayoutUnit(ascent));
}

// `origin` holds left-top for LTR, right-top for RTL.
const ExclusionArea* CreateExclusionSpaceForInitialLetterBox(
    EFloat float_type,
    BfcOffset origin,
    const BfcOffset& border_box_offset,
    const LogicalSize& border_box_size,
    const BoxStrut& margins,
    bool is_hidden_for_paint) {
  // Note: In case of `margins.inline_start` or `margins.line_over` are
  // negative, left top of `ExclusionSpace` are out of `ConstraintSpace`.
  const BfcOffset local_start_offset(
      border_box_offset.line_offset - margins.inline_start,
      border_box_offset.block_offset - margins.block_start);

  // Size of box should be grater than or equal to zero, even if negative
  // margins, e.g. `margin-right: -100px; width: 50px`.
  const LogicalSize margin_box_size(
      (border_box_size.inline_size + margins.InlineSum()).ClampNegativeToZero(),
      (border_box_size.block_size + margins.BlockSum()).ClampNegativeToZero());

  // Note: The block offset of `ExclusionSpace` is `origin.block_offset`
  // when initial letter positioned below the first line.
  //
  // Example:
  // initial-letter 3 5
  //
  // `origin.block_offset`
  //  +----------------->
  //  |         line 1
  //  |         line 2
  //  |  *****  line 3
  //  |    *    line 4
  //  |    *    line 5
  //  |    *    line 6
  //  V         line 7
  const BfcOffset start_offset(
      float_type == EFloat::kLeft
          ? origin.line_offset + local_start_offset.line_offset
          : origin.line_offset - margin_box_size.inline_size +
                margins.inline_end,
      origin.block_offset +
          std::min(local_start_offset.block_offset, LayoutUnit()));

  const BfcOffset end_offset(
      start_offset.line_offset + margin_box_size.inline_size,
      origin.block_offset + local_start_offset.block_offset +
          margin_box_size.block_size);

  return ExclusionArea::CreateForInitialLetterBox(
      BfcRect(start_offset, end_offset), float_type, is_hidden_for_paint);
}

}  // namespace

FontHeight AdjustInitialLetterInTextPosition(const FontHeight& line_box_metrics,
                                             LogicalLineItems* line_box) {
  FontHeight font_height = FontHeight::Empty();
  for (LogicalLineItem& line_item : *line_box) {
    const ShapeResultView* const shape_result = line_item.shape_result.Get();
    if (!shape_result || !line_item.inline_item ||
        line_item.inline_item->Type() != InlineItem::kText) {
      continue;
    }

    LayoutUnit baseline;
    const ComputedStyle& style = *line_item.Style();
    const LogicalRect text_ink_bounds =
        ComputeTextInkBounds(*shape_result, style, &baseline);

    // Set `line_item.rect`, text paint origin, to left-top of
    // `text_ink_bounds`. Note: ` NGTextFragmentPainter::Paint()` adds font
    // ascent to block offset.
    line_item.rect.offset.inline_offset +=
        -text_ink_bounds.offset.inline_offset;
    line_item.rect.offset.block_offset = -style.GetFontHeight().ascent;
    line_item.inline_size = text_ink_bounds.size.inline_size;

    if (style.IsHorizontalWritingMode() ||
        style.GetTextOrientation() == ETextOrientation::kSideways) {
      const LayoutUnit line_height = text_ink_bounds.size.block_size;
      const LayoutUnit ascent = baseline - text_ink_bounds.offset.block_offset;
      font_height.Unite(FontHeight(ascent, line_height - ascent));
      continue;
    }

    const LayoutUnit line_height = text_ink_bounds.size.block_size;
    const LayoutUnit ascent = LayoutUnit::FromFloatFloor(line_height / 2);
    font_height.Unite(FontHeight(ascent, line_height - ascent));
  }
  return font_height;
}

LayoutUnit CalculateInitialLetterBoxInlineSize(const LineInfo& line_info) {
  LayoutUnit inline_size = line_info.TextIndent();
  for (const InlineItemResult& item_result : line_info.Results()) {
    const ShapeResultView* const shape_result = item_result.shape_result.Get();
    if (!shape_result || item_result.item->Type() != InlineItem::kText) {
      inline_size += item_result.inline_size;
      continue;
    }
    const auto& style = *item_result.item->Style();
    const LogicalRect text_ink_bounds =
        ComputeTextInkBounds(*shape_result, style);

    // Example of `text_ink_bounds`
    //   - <i>f</i>   -16,18+59x81
    //   - <i>T</i>   6,21+53x59
    //   - T          2,21+51x59
    //   - U+05E9     2,20+50x79 (HEBREW)
    //   - U+3042     5,10+80x73 (HIRAGANA LETTER)
    // See also `AdjustInitialLetterInTextPosition()`.
    inline_size += text_ink_bounds.size.inline_size;
  }
  return inline_size;
}

const ExclusionArea* PostPlaceInitialLetterBox(
    const FontHeight& line_box_metrics,
    const BoxStrut& initial_letter_box_margins,
    LogicalLineItems* line_box,
    const BfcOffset& line_origin,
    LineInfo* line_info) {
  auto initial_letter_line_item = std::find_if(
      line_box->begin(), line_box->end(),
      [](const auto& line_item) { return line_item.IsInitialLetterBox(); });

  const auto& initial_letter_box_fragment =
      *To<PhysicalBoxFragment>(initial_letter_line_item->GetPhysicalFragment());

  DCHECK(initial_letter_box_fragment.IsInitialLetterBox());
  DCHECK(!initial_letter_box_fragment.Style().InitialLetter().IsNormal());

  const ComputedStyle& line_style = line_info->LineStyle();
  const WritingDirectionMode writing_direction_mode =
      line_style.GetWritingDirection();

  const LogicalSize initial_letter_box_size =
      LogicalFragment(writing_direction_mode, initial_letter_box_fragment)
          .Size();

  LayoutUnit initial_letter_block_start_adjust;
  const LayoutUnit initial_letter_border_box_block_offset =
      ComputeInitialLetterBoxBlockOffset(
          initial_letter_box_fragment, initial_letter_box_size.block_size,
          *initial_letter_line_item->Style(), line_style,
          &initial_letter_block_start_adjust) +
      initial_letter_box_margins.block_start;
  DCHECK_GE(initial_letter_block_start_adjust, LayoutUnit());
  line_info->SetInitialLetterBlockStartAdjustment(
      initial_letter_block_start_adjust);

  LayoutUnit adjusted_block_offset = initial_letter_border_box_block_offset;

  // Surrounding texts are shift down by `initial_letter_block_start_adjust`,
  // but initial letter box.
  adjusted_block_offset -= initial_letter_block_start_adjust;

  if (writing_direction_mode.IsFlippedLines()) {
    // Note: `FragmentItemsBuilder::ConvertToPhysical()` uses `kVerticalRl`
    // for items in line, by `ToLineWritingMode(kVerticalLR)`.
    // Conversion is done as below expression:
    //   * physical.x = outer_width - logical.block_offset - inner_width
    //   * logical.block_offset = outer_width - inner_width - physical.x
    //   * where outer_width = line_height, inner_width = physical.width
    adjusted_block_offset = line_style.ComputedLineHeightAsFixed() +
                            -adjusted_block_offset +
                            -initial_letter_box_size.block_size;
  }

  // Convert to baseline origin block offset.
  initial_letter_line_item->rect.offset.block_offset =
      adjusted_block_offset - line_box_metrics.ascent;

  const LayoutUnit initial_letter_border_box_inline_offset =
      initial_letter_line_item->rect.offset.inline_offset;

  const BfcOffset initial_letter_box_origin(
      writing_direction_mode.IsLtr()
          ? line_origin.line_offset
          : line_origin.line_offset + initial_letter_border_box_inline_offset +
                initial_letter_box_size.inline_size,
      line_origin.block_offset);

  const ExclusionArea* exclusion = CreateExclusionSpaceForInitialLetterBox(
      writing_direction_mode.IsLtr() ? EFloat::kLeft : EFloat::kRight,
      initial_letter_box_origin,
      BfcOffset(initial_letter_border_box_inline_offset,
                initial_letter_border_box_block_offset +
                    line_info->ComputeInitialLetterBoxBlockStartAdjustment()),
      initial_letter_box_size, initial_letter_box_margins,
      initial_letter_box_fragment.IsHiddenForPaint());

  line_info->SetInitialLetterBoxBlockSize(exclusion->rect.BlockSize());
  return exclusion;
}

}  // namespace blink
