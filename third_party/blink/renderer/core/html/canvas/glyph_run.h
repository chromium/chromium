// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CANVAS_GLYPH_RUN_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CANVAS_GLYPH_RUN_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/fonts/simple_font_data.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"

namespace blink {

class ShapedText;
class TextMetrics;

// A range of a ShapedText: either a font- and direction-resolved run
// (ShapedText::glyphRuns()) or a minimal cluster (ShapedText::glyphClusters()).
// [start, end) is the code unit range in the shaped text. (x, y) is the run
// offset from the ShapedText's alignment point, i.e. where the run is within
// the whole text: fillGlyphRun(run, x0, y0) draws it at (x0 + x, y0 + y).
// width is the run's advance.
class CORE_EXPORT GlyphRun final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  GlyphRun(unsigned start,
           unsigned end,
           double x,
           double y,
           double width,
           ShapedText& shaped_text,
           const SimpleFontData* font_data);

  unsigned start() const { return start_; }
  unsigned end() const { return end_; }
  double x() const { return x_; }
  double y() const { return y_; }
  double width() const { return width_; }

  TextMetrics* textMetrics();

  ShapedText* GetShapedText() const { return shaped_text_.Get(); }
  const SimpleFontData* GetFontData() const { return font_data_.Get(); }

  void Trace(Visitor*) const override;

 private:
  const unsigned start_ = 0;
  const unsigned end_ = 0;
  const double x_ = 0.0;
  const double y_ = 0.0;
  const double width_ = 0.0;
  const Member<ShapedText> shaped_text_;
  const Member<const SimpleFontData> font_data_;
  Member<TextMetrics> text_metrics_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CANVAS_GLYPH_RUN_H_
