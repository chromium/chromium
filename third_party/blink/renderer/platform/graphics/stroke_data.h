// Copyright (C) 2013 Google Inc. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_STROKE_DATA_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_STROKE_DATA_H_

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/platform/graphics/dash_array.h"
#include "third_party/blink/renderer/platform/graphics/gradient.h"
#include "third_party/blink/renderer/platform/graphics/graphics_types.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_flags.h"
#include "third_party/blink/renderer/platform/graphics/pattern.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkPathEffect.h"

namespace blink {

// Encapsulates stroke geometry information.
// It is pulled out of GraphicsContextState to enable other methods to use it.
class PLATFORM_EXPORT StrokeData final {
  DISALLOW_NEW();

 public:
  StrokeData()
      : style_(kSolidStroke),
        thickness_(0),
        line_cap_(PaintFlags::kDefault_Cap),
        line_join_(PaintFlags::kDefault_Join),
        miter_limit_(4) {}

  StrokeStyle Style() const { return style_; }
  void SetStyle(StrokeStyle style) { style_ = style; }

  float Thickness() const { return thickness_; }
  void SetThickness(float thickness) { thickness_ = thickness; }

  void SetLineCap(LineCap cap) { line_cap_ = (PaintFlags::Cap)cap; }

  void SetLineJoin(LineJoin join) { line_join_ = (PaintFlags::Join)join; }

  float MiterLimit() const { return miter_limit_; }
  void SetMiterLimit(float miter_limit) { miter_limit_ = miter_limit; }

  void SetLineDash(const DashArray&, float);

  // Sets everything on the paint except the pattern, gradient and color.
  // If a non-zero length is provided, the number of dashes/dots on a
  // dashed/dotted line will be adjusted to start and end that length with a
  // dash/dot. If non-zero, dash_thickness is the thickness to use when
  // deciding on dash sizes. Used in border painting when we stroke thick
  // to allow for clipping at corners, but still want small dashes.
  void SetupPaint(PaintFlags*,
                  const int length = 0,
                  const int dash_thickess = 0) const;

  // Setup any DashPathEffect on the paint. See SetupPaint above for parameter
  // information.
  void SetupPaintDashPathEffect(PaintFlags*,
                                const int path_length = 0,
                                const int dash_thickness = 0) const;

  // Determine whether a stroked line should be drawn using dashes. In practice,
  // we draw dashes when a dashed stroke is specified or when a dotted stroke
  // is specified but the line width is too small to draw circles.
  static bool StrokeIsDashed(float width, StrokeStyle);

  // The length of the dash relative to the line thickness for dashed stroking.
  // A different dash length may be used when dashes are adjusted to better
  // fit a given length path. Thin lines need longer dashes to avoid
  // looking like dots when drawn.
  static float DashLengthRatio(float thickness) {
    return thickness >= 3 ? 2.0 : 3.0;
  }

  // The length of the gap between dashes relative to the line thickness for
  // dashed stroking. A different gap may be used when dashes are adjusted to
  // better fit a given length path. Thin lines need longer gaps to avoid
  // looking like a continuous line when drawn.
  static float DashGapRatio(float thickness) {
    return thickness >= 3 ? 1.0 : 2.0;
  }

  // Return a dash gap size that places dashes at each end of a stroke that is
  // strokeLength long, given preferred dash and gap sizes. The gap returned is
  // the one that minimizes deviation from the preferred gap length.
  static float SelectBestDashGap(float stroke_length,
                                 float dash_length,
                                 float gap_length);

 private:
  StrokeStyle style_;
  float thickness_;
  PaintFlags::Cap line_cap_;
  PaintFlags::Join line_join_;
  float miter_limit_;
  sk_sp<SkPathEffect> dash_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_STROKE_DATA_H_
