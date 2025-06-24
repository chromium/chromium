// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_DECORATION_LINE_PAINTER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_DECORATION_LINE_PAINTER_H_

#include "cc/paint/paint_flags.h"
#include "cc/paint/paint_record.h"
#include "third_party/blink/renderer/platform/graphics/styled_stroke_data.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "ui/gfx/geometry/rect_f.h"

namespace blink {

struct AutoDarkMode;
class Color;
class GraphicsContext;
class StyledStrokeData;
class TextDecorationInfo;

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
                                 int wavy_offset_factor,
                                 const WaveDefinition* custom_wave,
                                 const Color& line_color);

  float Thickness() const { return line.height(); }

  StrokeStyle style = kSolidStroke;
  gfx::RectF line;
  float double_offset = 0;

  // Only used for kWavy lines.
  int wavy_offset_factor = 0;
  gfx::RectF wavy_pattern_rect;
  cc::PaintRecord wavy_tile_record;

  bool antialias = false;
};

// Helper class for painting a text decorations. Each instance paints a single
// decoration.
class DecorationLinePainter final {
  STACK_ALLOCATED();

 public:
  DecorationLinePainter(GraphicsContext& context,
                        const TextDecorationInfo& decoration_info)
      : context_(context), decoration_info_(decoration_info) {}

  static gfx::RectF Bounds(const DecorationGeometry&);

  void Paint(const Color& color, const cc::PaintFlags* flags = nullptr);

  static void DrawLineForText(GraphicsContext& context,
                              const gfx::RectF& line_rect,
                              const StyledStrokeData& styled_stroke,
                              const AutoDarkMode& auto_dark_mode,
                              const cc::PaintFlags* paint_flags = nullptr);

 private:
  void PaintWavyTextDecoration(const DecorationGeometry&, const AutoDarkMode&);

  GraphicsContext& context_;
  const TextDecorationInfo& decoration_info_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_DECORATION_LINE_PAINTER_H_
