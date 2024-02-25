// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_CHROMEOS_GPU_BUFFER_LAYOUT_H_
#define MEDIA_GPU_CHROMEOS_GPU_BUFFER_LAYOUT_H_

#include <optional>
#include <ostream>
#include <vector>

#include "media/base/color_plane_layout.h"
#include "media/gpu/chromeos/fourcc.h"
#include "media/gpu/media_gpu_export.h"
#include "ui/gfx/geometry/size.h"

namespace media {

class MEDIA_GPU_EXPORT GpuBufferLayout {
 public:
  static std::optional<GpuBufferLayout> Create(
      const Fourcc& fourcc,
      const gfx::Size& size,
      const std::vector<ColorPlaneLayout>& planes,
      uint64_t modifier);
  GpuBufferLayout() = delete;
  GpuBufferLayout(const GpuBufferLayout&);
  GpuBufferLayout(GpuBufferLayout&&);
  GpuBufferLayout& operator=(const GpuBufferLayout&);
  ~GpuBufferLayout();

  bool operator==(const GpuBufferLayout& rhs) const;
  bool operator!=(const GpuBufferLayout& rhs) const;

  const Fourcc& fourcc() const { return fourcc_; }
  const gfx::Size& size() const { return size_; }
  const std::vector<ColorPlaneLayout>& planes() const { return planes_; }
  uint64_t modifier() const { return modifier_; }

 private:
  GpuBufferLayout(const Fourcc& fourcc,
                  const gfx::Size& size,
                  const std::vector<ColorPlaneLayout>& planes,
                  uint64_t modifier);

  // Fourcc format of the buffer.
  Fourcc fourcc_;
  // Width and height of the buffer in pixels. This must include pixel
  // data for the whole image; i.e. for YUV formats with subsampled chroma
  // planes, in the case that the visible portion of the image does not line up
  // on a sample boundary, |size_| must be rounded up appropriately and
  // the pixel data provided for the odd pixels.
  gfx::Size size_;
  // Layout property for each color planes, e.g. stride and buffer offset.
  std::vector<ColorPlaneLayout> planes_;
  // DRM format modifier associated with buffer.
  uint64_t modifier_;
};

// Outputs GpuBufferLayout to stream.
MEDIA_GPU_EXPORT std::ostream& operator<<(std::ostream& ostream,
                                          const GpuBufferLayout& layout);

}  // namespace media

#endif  // MEDIA_GPU_CHROMEOS_GPU_BUFFER_LAYOUT_H_
