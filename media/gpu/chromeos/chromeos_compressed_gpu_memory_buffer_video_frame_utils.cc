// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/chromeos/chromeos_compressed_gpu_memory_buffer_video_frame_utils.h"

#include <drm_fourcc.h>
#include <stdint.h>

#include "base/logging.h"
#include "base/time/time.h"
#include "media/base/color_plane_layout.h"
#include "media/base/format_utils.h"
#include "media/base/video_frame.h"
#include "media/base/video_types.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace media {

scoped_refptr<VideoFrame> WrapChromeOSCompressedGpuMemoryBufferAsVideoFrame(
    const gfx::Rect& visible_rect,
    const gfx::Size& natural_size,
    std::unique_ptr<gfx::GpuMemoryBuffer> gpu_memory_buffer,
    base::TimeDelta timestamp) {
  const absl::optional<VideoPixelFormat> format =
      GfxBufferFormatToVideoPixelFormat(gpu_memory_buffer->GetFormat());
  if (!format ||
      (*format != PIXEL_FORMAT_NV12 && *format != PIXEL_FORMAT_P016LE)) {
    return nullptr;
  }

  constexpr VideoFrame::StorageType storage =
      VideoFrame::STORAGE_GPU_MEMORY_BUFFER;
  const gfx::Size& coded_size = gpu_memory_buffer->GetSize();
  if (!VideoFrame::IsValidConfig(*format, storage, coded_size, visible_rect,
                                 natural_size)) {
    DLOG(ERROR) << __func__ << " Invalid config"
                << VideoFrame::ConfigToString(*format, storage, coded_size,
                                              visible_rect, natural_size);
    return nullptr;
  }

  CHECK_EQ(gpu_memory_buffer->GetType(), gfx::NATIVE_PIXMAP);

  gfx::GpuMemoryBufferHandle gmb_handle = gpu_memory_buffer->CloneHandle();
  if (gmb_handle.is_null() || gmb_handle.native_pixmap_handle.planes.empty()) {
    DLOG(ERROR) << "Failed to clone the GpuMemoryBufferHandle";
    return nullptr;
  }

  const uint64_t modifier = gmb_handle.native_pixmap_handle.modifier;
  if (modifier != I915_FORMAT_MOD_Y_TILED_GEN12_MC_CCS)
    return nullptr;

  constexpr size_t kExpectedNumberOfPlanes = 4u;
  if (gmb_handle.native_pixmap_handle.planes.size() !=
      kExpectedNumberOfPlanes) {
    DLOG(ERROR) << "Invalid number of planes="
                << gmb_handle.native_pixmap_handle.planes.size()
                << ", expected num_planes=" << kExpectedNumberOfPlanes;
    return nullptr;
  }

  std::vector<ColorPlaneLayout> planes(kExpectedNumberOfPlanes);
  for (size_t i = 0; i < kExpectedNumberOfPlanes; ++i) {
    const auto& plane = gmb_handle.native_pixmap_handle.planes[i];
    planes[i].stride = plane.stride;
    planes[i].offset = plane.offset;
    planes[i].size = plane.size;
  }

  const auto layout = VideoFrameLayout::CreateWithPlanes(
      *format, coded_size, std::move(planes),
      VideoFrameLayout::kBufferAddressAlignment, modifier);
  if (!layout) {
    DLOG(ERROR) << __func__ << " Invalid layout";
    return nullptr;
  }

  scoped_refptr<VideoFrame> frame =
      new VideoFrame(*layout, storage, visible_rect, natural_size, timestamp);
  if (!frame) {
    DLOG(ERROR) << __func__ << " Couldn't create ChromeOS VideoFrame instance";
    return nullptr;
  }
  frame->gpu_memory_buffer_ = std::move(gpu_memory_buffer);
  return frame;
}

}  // namespace media
