/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CANVAS_TEXT_METRICS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CANVAS_TEXT_METRICS_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/html/canvas/baselines.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/fonts/font.h"
#include "third_party/blink/renderer/platform/graphics/graphics_types.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

class CORE_EXPORT TextMetrics final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  TextMetrics();
  TextMetrics(const Font& font,
              const TextDirection& direction,
              const TextBaseline& baseline,
              const TextAlign& align,
              const String& text);

  double width() const { return width_; }
  const Vector<double>& advances() const { return advances_; }
  double actualBoundingBoxLeft() const { return actual_bounding_box_left_; }
  double actualBoundingBoxRight() const { return actual_bounding_box_right_; }
  double fontBoundingBoxAscent() const { return font_bounding_box_ascent_; }
  double fontBoundingBoxDescent() const { return font_bounding_box_descent_; }
  double actualBoundingBoxAscent() const { return actual_bounding_box_ascent_; }
  double actualBoundingBoxDescent() const {
    return actual_bounding_box_descent_;
  }
  double emHeightAscent() const { return em_height_ascent_; }
  double emHeightDescent() const { return em_height_descent_; }
  Baselines* getBaselines() const { return baselines_; }

  static float GetFontBaseline(const TextBaseline&, const SimpleFontData&);

  void Trace(Visitor*) override;

 private:
  void Update(const Font&,
              const TextDirection&,
              const TextBaseline&,
              const TextAlign&,
              const String&);

  // x-direction
  double width_ = 0.0;
  Vector<double> advances_;
  double actual_bounding_box_left_ = 0.0;
  double actual_bounding_box_right_ = 0.0;

  // y-direction
  double font_bounding_box_ascent_ = 0.0;
  double font_bounding_box_descent_ = 0.0;
  double actual_bounding_box_ascent_ = 0.0;
  double actual_bounding_box_descent_ = 0.0;
  double em_height_ascent_ = 0.0;
  double em_height_descent_ = 0.0;
  Member<Baselines> baselines_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CANVAS_TEXT_METRICS_H_
