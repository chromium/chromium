// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/canvas/canvas_noise_test_util.h"

#include "components/viz/test/test_raster_interface.h"

namespace blink {

// Raster interface that always returns the same randomized image when read
// back.
class CanvasNoiseTestRasterInterface : public viz::TestRasterInterface {
 public:
  CanvasNoiseTestRasterInterface() = default;

 private:
  bool ReadbackImagePixels(const gpu::Mailbox& source_mailbox,
                           const SkImageInfo& dst_info,
                           GLuint dst_row_bytes,
                           int src_x,
                           int src_y,
                           int plane_index,
                           void* dst_pixels) override;
};

UNSAFE_BUFFER_USAGE bool CanvasNoiseTestRasterInterface::ReadbackImagePixels(
    const gpu::Mailbox& source_mailbox,
    const SkImageInfo& dst_info,
    GLuint dst_row_bytes,
    int src_x,
    int src_y,
    int plane_index,
    void* dst_pixels) {
  size_t size = dst_info.computeByteSize(dst_row_bytes);
  uint8_t* data = static_cast<uint8_t*>(dst_pixels);
  for (size_t i = 0; i < size; ++i) {
    data[i] = (i % 4 == 3) ? 255 : i % 256;
  }
  return true;
}

std::unique_ptr<viz::TestRasterInterface>
CreateCanvasNoiseTestRasterInterface() {
  return std::make_unique<CanvasNoiseTestRasterInterface>();
}

}  // namespace blink
