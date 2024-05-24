// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_INK_INK_BRUSH_PAINT_H_
#define PDF_INK_INK_BRUSH_PAINT_H_

#include <optional>
#include <string>
#include <vector>

namespace chrome_pdf {

struct InkBrushPaint {
  InkBrushPaint();
  InkBrushPaint(const InkBrushPaint&) = delete;
  InkBrushPaint& operator=(const InkBrushPaint&) = delete;
  InkBrushPaint(InkBrushPaint&&) noexcept;
  InkBrushPaint& operator=(InkBrushPaint&&) noexcept;
  ~InkBrushPaint();

  enum class TextureMapping {
    kTiling,
    kWinding,
  };

  enum class TextureSizeUnit {
    kBrushSize,
    kStrokeSize,
    kStrokeCoordinates,
  };

  enum class BlendMode {
    kModulate,
    kDstIn,
    kDstOut,
    kSrcAtop,
    kSrcIn,
    kSrcOver,
  };

  struct TextureKeyframe {
    float progress;

    std::optional<float> size_x;
    std::optional<float> size_y;
    std::optional<float> offset_x;
    std::optional<float> offset_y;
    std::optional<float> rotation_in_radians;

    std::optional<float> opacity;
  };

  struct TextureLayer {
    TextureLayer();
    TextureLayer(const TextureLayer&);
    TextureLayer& operator=(const TextureLayer&);
    ~TextureLayer();

    std::string color_texture_uri;

    TextureMapping mapping = TextureMapping::kTiling;
    TextureSizeUnit size_unit = TextureSizeUnit::kStrokeCoordinates;

    float size_x = 1;
    float size_y = 1;
    float offset_x = 0;
    float offset_y = 0;
    float rotation_in_radians = 0;

    float size_jitter_x = 0;
    float size_jitter_y = 0;
    float offset_jitter_x = 0;
    float offset_jitter_y = 0;
    float rotation_jitter_in_radians = 0;

    float opacity = 1;

    std::vector<TextureKeyframe> keyframes;

    BlendMode blend_mode = BlendMode::kModulate;
  };

  std::vector<TextureLayer> texture_layers;
};

}  // namespace chrome_pdf

#endif  // PDF_INK_INK_BRUSH_PAINT_H_
