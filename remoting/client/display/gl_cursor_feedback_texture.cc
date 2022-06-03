// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/display/gl_cursor_feedback_texture.h"

#include "base/notreached.h"
#include "remoting/client/display/gl_render_layer.h"

namespace remoting {

namespace {

const int kColorRingsCount = 5;
const int kFeedbackTexturePixelDiameter = 512;
const int kFeedbackTexturePixelRadius = kFeedbackTexturePixelDiameter / 2;

// RGBA8888 colors. From inside to outside.
const uint8_t kFeedbackRingColors[kColorRingsCount]
                                 [GlRenderLayer::kBytesPerPixel] = {
                                     {0x0, 0x0, 0x0, 0x7f},     // Black
                                     {0x0, 0x0, 0x0, 0x7f},     // Black
                                     {0xff, 0xff, 0xff, 0x7f},  // White
                                     {0xff, 0xff, 0xff, 0x7f},  // White
                                     {0xff, 0xff, 0xff, 0}  // Transparent White
};

const float kFeedbackRadiusStops[kColorRingsCount] = {0.0f, 0.85f, 0.9f, 0.95f,
                                                      1.0f};

uint32_t GetColorByRadius(float radius) {
  int ring_index = kColorRingsCount - 1;
  // Find first radius stop that is not smaller than current radius.
  while (radius < kFeedbackRadiusStops[ring_index] && ring_index >= 0) {
    ring_index--;
  }

  if (ring_index < 0) {
    NOTREACHED();
    return 0;
  }

  if (ring_index == kColorRingsCount - 1) {
    // Area outside the circle. Just use the outermost color.
    return *reinterpret_cast<const uint32_t*>(kFeedbackRingColors[ring_index]);
  }

  const uint8_t* first_color = kFeedbackRingColors[ring_index];
  const uint8_t* second_color = kFeedbackRingColors[ring_index + 1];
  float first_radius = kFeedbackRadiusStops[ring_index];
  float second_radius = kFeedbackRadiusStops[ring_index + 1];

  uint8_t progress =
      (radius - first_radius) * 256 / (second_radius - first_radius);
  uint32_t color;
  uint8_t* color_ptr = reinterpret_cast<uint8_t*>(&color);
  for (int i = 0; i < GlRenderLayer::kBytesPerPixel; i++) {
    color_ptr[i] =
        (first_color[i] * (256 - progress) + second_color[i] * progress) / 256;
  }
  return color;
}

void FillFeedbackTexture(uint32_t* texture) {
  for (int x = 0; x < kFeedbackTexturePixelRadius; x++) {
    for (int y = 0; y <= x; y++) {
      float radius = sqrt(x * x + y * y) / kFeedbackTexturePixelRadius;
      uint32_t color = GetColorByRadius(radius);

      int x1 = kFeedbackTexturePixelRadius + x;
      int x2 = kFeedbackTexturePixelRadius - 1 - x;

      int y1 = kFeedbackTexturePixelRadius + y;
      int y2 = kFeedbackTexturePixelRadius - 1 - y;

      texture[x1 + y1 * kFeedbackTexturePixelDiameter] = color;
      texture[x1 + y2 * kFeedbackTexturePixelDiameter] = color;
      texture[x2 + y1 * kFeedbackTexturePixelDiameter] = color;
      texture[x2 + y2 * kFeedbackTexturePixelDiameter] = color;
      texture[y1 + x1 * kFeedbackTexturePixelDiameter] = color;
      texture[y1 + x2 * kFeedbackTexturePixelDiameter] = color;
      texture[y2 + x1 * kFeedbackTexturePixelDiameter] = color;
      texture[y2 + x2 * kFeedbackTexturePixelDiameter] = color;
    }
  }
}

}  // namespace

// static
const int GlCursorFeedbackTexture::kTextureWidth =
    kFeedbackTexturePixelDiameter;

GlCursorFeedbackTexture* GlCursorFeedbackTexture::GetInstance() {
  return base::Singleton<GlCursorFeedbackTexture>::get();
}

const std::vector<uint8_t>& GlCursorFeedbackTexture::GetTexture() const {
  return texture_;
}

GlCursorFeedbackTexture::GlCursorFeedbackTexture() {
  texture_.resize(kTextureWidth * kTextureWidth *
                  GlRenderLayer::kBytesPerPixel);
  FillFeedbackTexture(reinterpret_cast<uint32_t*>(texture_.data()));
}

GlCursorFeedbackTexture::~GlCursorFeedbackTexture() = default;

}  // namespace remoting
