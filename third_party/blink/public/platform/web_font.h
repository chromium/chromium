// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_FONT_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_FONT_H_

#include <memory>

#include "third_party/blink/public/platform/web_common.h"
#include "third_party/skia/include/core/SkColor.h"

// To avoid conflicts with the DrawText macro from the Windows SDK...
#undef DrawText

namespace cc {
class PaintCanvas;
}

namespace gfx {
class PointF;
class RectF;
}

namespace blink {
struct WebFontDescription;
struct WebTextRun;

class WebFont {
 public:
  BLINK_PLATFORM_EXPORT static WebFont* Create(const WebFontDescription&);
  BLINK_PLATFORM_EXPORT ~WebFont();

  BLINK_PLATFORM_EXPORT WebFontDescription GetFontDescription() const;
  BLINK_PLATFORM_EXPORT int Ascent() const;
  BLINK_PLATFORM_EXPORT int Descent() const;
  BLINK_PLATFORM_EXPORT int Height() const;
  BLINK_PLATFORM_EXPORT int LineSpacing() const;
  BLINK_PLATFORM_EXPORT float XHeight() const;
  BLINK_PLATFORM_EXPORT void DrawText(cc::PaintCanvas*,
                                      const WebTextRun&,
                                      const gfx::PointF& left_baseline,
                                      SkColor) const;
  BLINK_PLATFORM_EXPORT int CalculateWidth(const WebTextRun&) const;
  BLINK_PLATFORM_EXPORT int OffsetForPosition(const WebTextRun&,
                                              float position) const;
  BLINK_PLATFORM_EXPORT gfx::RectF SelectionRectForText(
      const WebTextRun&,
      const gfx::PointF& left_baseline,
      int height,
      int from = 0,
      int to = -1) const;

 private:
  explicit WebFont(const WebFontDescription&);

  class Impl;
  std::unique_ptr<Impl> private_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_FONT_H_
