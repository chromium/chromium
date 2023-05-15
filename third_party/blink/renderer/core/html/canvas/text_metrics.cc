// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/canvas/text_metrics.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_baselines.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_bidi_paragraph.h"
#include "third_party/blink/renderer/platform/fonts/character_range.h"

namespace blink {

constexpr int kHangingAsPercentOfAscent = 80;

float TextMetrics::GetFontBaseline(const TextBaseline& text_baseline,
                                   const SimpleFontData& font_data) {
  FontMetrics font_metrics = font_data.GetFontMetrics();
  switch (text_baseline) {
    case kTopTextBaseline:
      return font_data.NormalizedTypoAscent().ToFloat();
    case kHangingTextBaseline:
      // According to
      // http://wiki.apache.org/xmlgraphics-fop/LineLayout/AlignmentHandling
      // "FOP (Formatting Objects Processor) puts the hanging baseline at 80% of
      // the ascender height"
      return font_metrics.FloatAscent() * kHangingAsPercentOfAscent / 100.0;
    case kIdeographicTextBaseline:
      return -font_metrics.FloatDescent();
    case kBottomTextBaseline:
      return -font_data.NormalizedTypoDescent().ToFloat();
    case kMiddleTextBaseline: {
      const FontHeight metrics = font_data.NormalizedTypoAscentAndDescent();
      return (metrics.ascent.ToFloat() - metrics.descent.ToFloat()) / 2.0f;
    }
    case kAlphabeticTextBaseline:
    default:
      // Do nothing.
      break;
  }
  return 0;
}

void TextMetrics::Trace(Visitor* visitor) const {
  visitor->Trace(baselines_);
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

  {
    // TODO(kojii): Need to figure out the desired behavior of |advances| when
    // bidi reorder occurs.
    TextRun text_run(text, direction, false);
    text_run.SetNormalizeSpace(true);
    advances_ = font.IndividualCharacterAdvances(text_run);
  }

  // x direction
  // Run bidi algorithm on the given text. Step 5 of:
  // https://html.spec.whatwg.org/multipage/canvas.html#text-preparation-algorithm
  gfx::RectF glyph_bounds;
  String text16 = text;
  text16.Ensure16Bit();
  NGBidiParagraph bidi;
  bidi.SetParagraph(text16, direction);
  NGBidiParagraph::Runs runs;
  bidi.GetLogicalRuns(text16, &runs);
  float xpos = 0;
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
  }
  double real_width = xpos;
  width_ = real_width;

  float dx = 0.0f;
  if (align == kCenterTextAlign)
    dx = real_width / 2.0f;
  else if (align == kRightTextAlign ||
           (align == kStartTextAlign && direction == TextDirection::kRtl) ||
           (align == kEndTextAlign && direction != TextDirection::kRtl))
    dx = real_width;
  actual_bounding_box_left_ = -glyph_bounds.x() + dx;
  actual_bounding_box_right_ = glyph_bounds.right() - dx;

  // y direction
  const FontMetrics& font_metrics = font_data->GetFontMetrics();
  const float ascent = font_metrics.FloatAscent();
  const float descent = font_metrics.FloatDescent();
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

  // TODO(fserb): hanging/ideographic baselines are broken.
  baselines_->setAlphabetic(-baseline_y);
  baselines_->setHanging(ascent * kHangingAsPercentOfAscent / 100.0f -
                         baseline_y);
  baselines_->setIdeographic(-descent - baseline_y);
}

}  // namespace blink
