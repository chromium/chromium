// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_DECORATION_LINE_PAINTER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_DECORATION_LINE_PAINTER_H_

#include "third_party/blink/renderer/platform/graphics/styled_stroke_data.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "ui/gfx/geometry/rect_f.h"

namespace cc {
class PaintFlags;
}  // namespace cc

namespace blink {

struct AutoDarkMode;
class Color;
class GraphicsContext;

// Defines a "wave" for painting a kWavyStroke. See the .cc file for a detailed
// description.
struct WaveDefinition {
  float wavelength;              // Wavelength of the waveform.
  float control_point_distance;  // Almost-but-not-quite the amplitude of the
                                 // waveform (the real amplitude will be less
                                 // than this value).
  float phase;                   // Phase of the waveform.

  bool operator==(const WaveDefinition&) const = default;
};

struct DecorationGeometry {
  STACK_ALLOCATED();

 public:
  static DecorationGeometry Make(StrokeStyle style,
                                 const gfx::RectF& line,
                                 float double_offset,
                                 float wavy_offset,
                                 const WaveDefinition* custom_wave);

  float Thickness() const { return line.height(); }

  StrokeStyle style = kSolidStroke;
  gfx::RectF line;

  // Only used for kDoubleStroke lines.
  float double_offset = 0;

  // Only used for kWavyStroke lines.
  float wavy_offset = 0;
  WaveDefinition wavy_wave;

  bool antialias = false;
};

// Helper class for painting a text decorations. Each instance paints a single
// decoration.
class DecorationLinePainter final {
  STACK_ALLOCATED();

 public:
  explicit DecorationLinePainter(GraphicsContext& context)
      : context_(context) {}

  static gfx::RectF Bounds(const DecorationGeometry&);

  void Paint(const DecorationGeometry&,
             const Color& color,
             const AutoDarkMode& auto_dark_mode,
             const cc::PaintFlags* flags = nullptr);

 private:
  void PaintWavyTextDecoration(const DecorationGeometry&,
                               const Color&,
                               const AutoDarkMode&);

  GraphicsContext& context_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_DECORATION_LINE_PAINTER_H_
