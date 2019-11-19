// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/platform/web_font.h"

#include "third_party/blink/public/platform/web_float_point.h"
#include "third_party/blink/public/platform/web_float_rect.h"
#include "third_party/blink/public/platform/web_font_description.h"
#include "third_party/blink/public/platform/web_rect.h"
#include "third_party/blink/public/platform/web_text_run.h"
#include "third_party/blink/renderer/platform/fonts/font.h"
#include "third_party/blink/renderer/platform/fonts/font_cache.h"
#include "third_party/blink/renderer/platform/fonts/font_description.h"
#include "third_party/blink/renderer/platform/fonts/text_run_paint_info.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_record_builder.h"
#include "third_party/blink/renderer/platform/text/text_run.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

WebFont* WebFont::Create(const WebFontDescription& description) {
  return new WebFont(description);
}

class WebFont::Impl final {
  USING_FAST_MALLOC(WebFont::Impl);

 public:
  explicit Impl(const WebFontDescription& description) : font_(description) {
    font_.Update(nullptr);
  }

  const Font& GetFont() const { return font_; }

 private:
  Font font_;
};

WebFont::WebFont(const WebFontDescription& description)
    : private_(std::make_unique<Impl>(description)) {}

WebFont::~WebFont() = default;

WebFontDescription WebFont::GetFontDescription() const {
  return WebFontDescription(private_->GetFont().GetFontDescription());
}

static inline const SimpleFontData* GetFontData(const Font& font) {
  const SimpleFontData* font_data = font.PrimaryFont();
  DCHECK(font_data);
  return font_data;
}

int WebFont::Ascent() const {
  const SimpleFontData* font_data = GetFontData(private_->GetFont());
  return font_data ? font_data->GetFontMetrics().Ascent() : 0;
}

int WebFont::Descent() const {
  const SimpleFontData* font_data = GetFontData(private_->GetFont());
  return font_data ? font_data->GetFontMetrics().Descent() : 0;
}

int WebFont::Height() const {
  const SimpleFontData* font_data = GetFontData(private_->GetFont());
  return font_data ? font_data->GetFontMetrics().Height() : 0;
}

int WebFont::LineSpacing() const {
  const SimpleFontData* font_data = GetFontData(private_->GetFont());
  return font_data ? font_data->GetFontMetrics().LineSpacing() : 0;
}

float WebFont::XHeight() const {
  const SimpleFontData* font_data = private_->GetFont().PrimaryFont();
  DCHECK(font_data);
  return font_data ? font_data->GetFontMetrics().XHeight() : 0;
}

void WebFont::DrawText(cc::PaintCanvas* canvas,
                       const WebTextRun& run,
                       const WebFloatPoint& left_baseline,
                       SkColor color) const {
  FontCachePurgePreventer font_cache_purge_preventer;
  TextRun text_run(run);
  TextRunPaintInfo run_info(text_run);

  PaintRecordBuilder builder;
  GraphicsContext& context = builder.Context();

  {
    DrawingRecorder recorder(context, builder, DisplayItem::kWebFont);
    context.Save();
    context.SetFillColor(color);
    context.DrawText(private_->GetFont(), run_info, left_baseline,
                     kInvalidDOMNodeId);
    context.Restore();
  }

  builder.EndRecording(*canvas);
}

int WebFont::CalculateWidth(const WebTextRun& run) const {
  return private_->GetFont().Width(run, nullptr);
}

int WebFont::OffsetForPosition(const WebTextRun& run, float position) const {
  return private_->GetFont().OffsetForPosition(
      run, position, IncludePartialGlyphs, DontBreakGlyphs);
}

WebFloatRect WebFont::SelectionRectForText(const WebTextRun& run,
                                           const WebFloatPoint& left_baseline,
                                           int height,
                                           int from,
                                           int to) const {
  return private_->GetFont().SelectionRectForText(run, left_baseline, height,
                                                  from, to);
}

}  // namespace blink
