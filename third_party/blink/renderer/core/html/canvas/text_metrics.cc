// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/canvas/text_metrics.h"

#include "base/numerics/checked_math.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_baselines.h"
#include "third_party/blink/renderer/core/geometry/dom_rect_read_only.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/fonts/character_range.h"
#include "third_party/blink/renderer/platform/fonts/font.h"
#include "third_party/blink/renderer/platform/fonts/font_metrics.h"
#include "third_party/blink/renderer/platform/text/bidi_paragraph.h"

namespace blink {

constexpr int kHangingAsPercentOfAscent = 80;

float TextMetrics::GetFontBaseline(const TextBaseline& text_baseline,
                                   const SimpleFontData& font_data) {
  FontMetrics font_metrics = font_data.GetFontMetrics();
  switch (text_baseline) {
    case kTopTextBaseline:
      return font_data.NormalizedTypoAscent().ToFloat();
    case kHangingTextBaseline:
      if (font_metrics.HangingBaseline().has_value()) {
        return font_metrics.HangingBaseline().value();
      }
      // According to
      // http://wiki.apache.org/xmlgraphics-fop/LineLayout/AlignmentHandling
      // "FOP (Formatting Objects Processor) puts the hanging baseline at 80% of
      // the ascender height"
      return font_metrics.FloatAscent(kAlphabeticBaseline,
                                      FontMetrics::ApplyBaselineTable(true)) *
             kHangingAsPercentOfAscent / 100.0;
    case kIdeographicTextBaseline:
      if (font_metrics.IdeographicBaseline().has_value()) {
        return font_metrics.IdeographicBaseline().value();
      }
      return -font_metrics.FloatDescent(kAlphabeticBaseline,
                                        FontMetrics::ApplyBaselineTable(true));
    case kBottomTextBaseline:
      return -font_data.NormalizedTypoDescent().ToFloat();
    case kMiddleTextBaseline: {
      const FontHeight metrics = font_data.NormalizedTypoAscentAndDescent();
      return (metrics.ascent.ToFloat() - metrics.descent.ToFloat()) / 2.0f;
    }
    case kAlphabeticTextBaseline:
      if (font_metrics.AlphabeticBaseline().has_value()) {
        return font_metrics.AlphabeticBaseline().value();
      }
      return 0;
    default:
      // Do nothing.
      return 0;
  }
}

void TextMetrics::Trace(Visitor* visitor) const {
  visitor->Trace(baselines_);
  visitor->Trace(font_);
  ScriptWrappable::Trace(visitor);
}

TextMetrics::TextMetrics() : baselines_(Baselines::Create()) {}

TextMetrics::TextMetrics(const Font& font,
                         const TextDirection& direction,
                         const TextBaseline& baseline,
                         const TextAlign& align,
                         const String& text)
    : TextMetrics() {
  Update(font, direction, baseline, align, text);
}

void TextMetrics::Update(const Font& font,
                         const TextDirection& direction,
                         const TextBaseline& baseline,
                         const TextAlign& align,
                         const String& text) {
  const SimpleFontData* font_data = font.PrimaryFont();
  if (!font_data)
    return;

  text_runs_.clear();
  font_ = font;
  text_length_ = text.length();

  // x direction
  // Run bidi algorithm on the given text. Step 5 of:
  // https://html.spec.whatwg.org/multipage/canvas.html#text-preparation-algorithm
  gfx::RectF glyph_bounds;
  String text16 = text;
  text16.Ensure16Bit();
  BidiParagraph bidi;
  bidi.SetParagraph(text16, direction);
  BidiParagraph::Runs runs;
  bidi.GetLogicalRuns(text16, &runs);
  float xpos = 0;
  text_runs_.reserve(runs.size());
  for (const auto& run : runs) {
    // Measure each run.
    TextRun text_run(StringView(text, run.start, run.Length()), run.Direction(),
                     /* directional_override */ false);
    text_run.SetNormalizeSpace(true);
    gfx::RectF run_glyph_bounds;
    float run_width = font.Width(text_run, &run_glyph_bounds);

    // Accumulate the position and the glyph bounding box.
    run_glyph_bounds.Offset(xpos, 0);
    glyph_bounds.Union(run_glyph_bounds);
    xpos += run_width;

    // Save the run for computing selection boxes.
    text_runs_.push_back(text_run);
  }
  double real_width = xpos;
  width_ = real_width;

  text_align_dx_ = 0.0f;
  if (align == kCenterTextAlign) {
    text_align_dx_ = real_width / 2.0f;
  } else if (align == kRightTextAlign ||
             (align == kStartTextAlign && direction == TextDirection::kRtl) ||
             (align == kEndTextAlign && direction != TextDirection::kRtl)) {
    text_align_dx_ = real_width;
  }
  actual_bounding_box_left_ = -glyph_bounds.x() + text_align_dx_;
  actual_bounding_box_right_ = glyph_bounds.right() - text_align_dx_;

  // y direction
  const FontMetrics& font_metrics = font_data->GetFontMetrics();
  const float ascent = font_metrics.FloatAscent(
      kAlphabeticBaseline, FontMetrics::ApplyBaselineTable(true));
  const float descent = font_metrics.FloatDescent(
      kAlphabeticBaseline, FontMetrics::ApplyBaselineTable(true));
  const float baseline_y = GetFontBaseline(baseline, *font_data);
  font_bounding_box_ascent_ = ascent - baseline_y;
  font_bounding_box_descent_ = descent + baseline_y;
  actual_bounding_box_ascent_ = -glyph_bounds.y() - baseline_y;
  actual_bounding_box_descent_ = glyph_bounds.bottom() + baseline_y;
  // TODO(kojii): We use normalized sTypoAscent/Descent here, but this should be
  // revisited when the spec evolves.
  const FontHeight normalized_typo_metrics =
      font_data->NormalizedTypoAscentAndDescent();
  em_height_ascent_ = normalized_typo_metrics.ascent - baseline_y;
  em_height_descent_ = normalized_typo_metrics.descent + baseline_y;

  // Setting baselines:
  if (font_metrics.AlphabeticBaseline().has_value()) {
    baselines_->setAlphabetic(font_metrics.AlphabeticBaseline().value() -
                              baseline_y);
  } else {
    baselines_->setAlphabetic(-baseline_y);
  }

  if (font_metrics.HangingBaseline().has_value()) {
    baselines_->setHanging(font_metrics.HangingBaseline().value() - baseline_y);
  } else {
    baselines_->setHanging(ascent * kHangingAsPercentOfAscent / 100.0f -
                           baseline_y);
  }

  if (font_metrics.IdeographicBaseline().has_value()) {
    baselines_->setIdeographic(font_metrics.IdeographicBaseline().value() -
                               baseline_y);
  } else {
    baselines_->setIdeographic(-descent - baseline_y);
  }
}

const HeapVector<Member<DOMRectReadOnly>> TextMetrics::getSelectionRects(
    uint32_t start,
    uint32_t end,
    ExceptionState& exception_state) {
  HeapVector<Member<DOMRectReadOnly>> selection_rects;

  // Checks indexes that go over the maximum for the text. For indexes less than
  // 0, an exception is thrown by [EnforceRange] in the idl binding.
  if (start > text_length_ || end > text_length_) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kIndexSizeError,
        String::Format("The %s index is out of bounds.",
                       start >= text_length_ ? "start" : "end"));
    return selection_rects;
  }

  const double height = font_bounding_box_ascent_ + font_bounding_box_descent_;
  const double y = -font_bounding_box_ascent_;
  double accumulated_width = 0.0;
  unsigned int accumulated_string_length = 0;

  // Handle start >= end case with end = 0 the same way the DOM does, returning
  // a zero-width rect before the start of the text.
  if (start >= end && end == 0) {
    selection_rects.push_back(DOMRectReadOnly::Create(
        -text_align_dx_, -font_bounding_box_ascent_, /*width=*/0, height));
    return selection_rects;
  }

  for (const auto& text_run : text_runs_) {
    // Accumulate string length to know the indexes of this run on the input
    // string.
    const unsigned int run_start_index = accumulated_string_length;
    const unsigned int run_end_index =
        accumulated_string_length + text_run.length();
    accumulated_string_length += text_run.length();

    // Past the selection interval.
    if (run_start_index >= end) {
      break;
    }

    // Position of the left border for this run.
    const double left_border = accumulated_width;
    accumulated_width += font_.Width(text_run);

    // Handle start >= end case the same way the DOM does, returning a
    // zero-width rect after the advance of the character right before the end
    // position.
    if (start >= end && run_start_index < end && end <= run_end_index) {
      const unsigned index =
          base::CheckSub(end - 1, run_start_index).ValueOrDie();
      gfx::RectF rect = font_.SelectionRectForText(
          text_run, gfx::PointF(left_border - text_align_dx_, y), height, index,
          index + 1);
      rect.set_x(rect.right());
      rect.set_width(0);
      selection_rects.push_back(DOMRectReadOnly::FromRectF(rect));
      break;
    }

    // Before the selection interval.
    if (run_end_index <= start) {
      continue;
    }

    // Calculate the required indexes for this specific run.
    const unsigned int starting_index =
        start > run_start_index ? start - run_start_index : 0;
    const unsigned int ending_index =
        end <= run_end_index ? end - run_start_index : text_run.length();

    gfx::RectF selection_rect = font_.SelectionRectForText(
        text_run, gfx::PointF(left_border - text_align_dx_, y), height,
        starting_index, ending_index);
    selection_rects.push_back(DOMRectReadOnly::FromRectF(selection_rect));
  }

  return selection_rects;
}

}  // namespace blink
