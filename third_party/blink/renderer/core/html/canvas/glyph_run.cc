// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/canvas/glyph_run.h"

#include "third_party/blink/renderer/core/html/canvas/shaped_text.h"
#include "third_party/blink/renderer/core/html/canvas/text_metrics.h"

namespace blink {

GlyphRun::GlyphRun(unsigned start,
                   unsigned end,
                   double x,
                   double y,
                   double width,
                   ShapedText& shaped_text,
                   const SimpleFontData* font_data)
    : start_(start),
      end_(end),
      x_(x),
      y_(y),
      width_(width),
      shaped_text_(shaped_text),
      font_data_(font_data) {}

TextMetrics* GlyphRun::textMetrics() {
  if (!text_metrics_) {
    text_metrics_ = MakeGarbageCollected<TextMetrics>(
        *shaped_text_, start_, end_, x_, width_, font_data_.Get());
  }
  return text_metrics_.Get();
}

void GlyphRun::Trace(Visitor* visitor) const {
  visitor->Trace(shaped_text_);
  visitor->Trace(font_data_);
  visitor->Trace(text_metrics_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
