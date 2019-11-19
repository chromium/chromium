// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/canvas/text_metrics.h"
#include "third_party/blink/renderer/core/html/canvas/baselines.h"
#include "third_party/blink/renderer/platform/fonts/character_range.h"

namespace blink {

constexpr int kHangingAsPercentOfAscent = 80;

float TextMetrics::GetFontBaseline(const TextBaseline& text_baseline,
                                   const SimpleFontData& font_data) {
  FontMetrics font_metrics = font_data.GetFontMetrics();
  switch (text_baseline) {
    case kTopTextBaseline:
      return font_data.EmHeightAscent().ToFloat();
    case kHangingTextBaseline:
      // According to
      // http://wiki.apache.org/xmlgraphics-fop/LineLayout/AlignmentHandling
      // "FOP (Formatting Objects Processor) puts the hanging baseline at 80% of
      // the ascender height"
      return font_metrics.FloatAscent() * kHangingAsPercentOfAscent / 100.0;
    case kIdeographicTextBaseline:
      return -font_metrics.FloatDescent();
    case kBottomTextBaseline:
      return -font_data.EmHeightDescent().ToFloat();
    case kMiddleTextBaseline:
      return (font_data.EmHeightAscent().ToFloat() -
              font_data.EmHeightDescent().ToFloat()) /
             2.0f;
    case kAlphabeticTextBaseline:
    default:
      // Do nothing.
      break;
  }
  return 0;
}

void TextMetrics::Trace(Visitor* visitor) {
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

  TextRun text_run(
      text, /* xpos */ 0, /* expansion */ 0,
      TextRun::kAllowTrailingExpansion | TextRun::kForbidLeadingExpansion,
      direction, false);
  text_run.SetNormalizeSpace(true);
  FloatRect bbox = font.BoundingBox(text_run);
  const FontMetrics& font_metrics = font_data->GetFontMetrics();

  advances_ = font.IndividualCharacterAdvances(text_run);

  // x direction
  width_ = bbox.Width();
  FloatRect glyph_bounds;
  double real_width = font.Width(text_run, nullptr, &glyph_bounds);

  float dx = 0.0f;
  if (align == kCenterTextAlign)
    dx = real_width / 2.0f;
  else if (align == kRightTextAlign ||
           (align == kStartTextAlign && direction == TextDirection::kRtl) ||
           (align == kEndTextAlign && direction != TextDirection::kRtl))
    dx = real_width;
  actual_bounding_box_left_ = -glyph_bounds.X() + dx;
  actual_bounding_box_right_ = glyph_bounds.MaxX() - dx;

  // y direction
  const float ascent = font_metrics.FloatAscent();
  const float descent = font_metrics.FloatDescent();
  const float baseline_y = GetFontBaseline(baseline, *font_data);
  font_bounding_box_ascent_ = ascent - baseline_y;
  font_bounding_box_descent_ = descent + baseline_y;
  actual_bounding_box_ascent_ = -bbox.Y() - baseline_y;
  actual_bounding_box_descent_ = bbox.MaxY() + baseline_y;
  em_height_ascent_ = font_data->EmHeightAscent() - baseline_y;
  em_height_descent_ = font_data->EmHeightDescent() + baseline_y;

  // TODO(fserb): hanging/ideographic baselines are broken.
  baselines_->setAlphabetic(-baseline_y);
  baselines_->setHanging(ascent * kHangingAsPercentOfAscent / 100.0f -
                         baseline_y);
  baselines_->setIdeographic(-descent - baseline_y);
}

}  // namespace blink
