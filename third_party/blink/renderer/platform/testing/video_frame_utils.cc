// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/testing/video_frame_utils.h"

#include "base/functional/callback_helpers.h"
#include "media/base/format_utils.h"
#include "media/video/fake_gpu_memory_buffer.h"

namespace blink {

scoped_refptr<media::VideoFrame> CreateTestFrame(
    const gfx::Size& coded_size,
    const gfx::Rect& visible_rect,
    const gfx::Size& natural_size,
    media::VideoFrame::StorageType storage_type) {
  return CreateTestFrame(coded_size, visible_rect, natural_size, storage_type,
                         storage_type == media::VideoFrame::STORAGE_OWNED_MEMORY
                             ? media::PIXEL_FORMAT_I420
                             : media::PIXEL_FORMAT_NV12,
                         base::TimeDelta());
}

scoped_refptr<media::VideoFrame> CreateTestFrame(
    const gfx::Size& coded_size,
    const gfx::Rect& visible_rect,
    const gfx::Size& natural_size,
    media::VideoFrame::StorageType storage_type,
    media::VideoPixelFormat pixel_format,
    base::TimeDelta timestamp,
    std::unique_ptr<gfx::GpuMemoryBuffer> gmb) {
  switch (storage_type) {
    case media::VideoFrame::STORAGE_OWNED_MEMORY:
      return media::VideoFrame::CreateZeroInitializedFrame(
          pixel_format, coded_size, visible_rect, natural_size, timestamp);
    case media::VideoFrame::STORAGE_GPU_MEMORY_BUFFER: {
      std::optional<gfx::BufferFormat> buffer_format =
          media::VideoPixelFormatToGfxBufferFormat(pixel_format);
      CHECK(buffer_format) << "Pixel format "
                           << media::VideoPixelFormatToString(pixel_format)
                           << " has no corresponding gfx::BufferFormat";
      if (!gmb) {
        gmb = std::make_unique<media::FakeGpuMemoryBuffer>(
            coded_size, buffer_format.value());
      }
      return media::VideoFrame::WrapExternalGpuMemoryBuffer(
          visible_rect, natural_size, std::move(gmb), timestamp);
    }
    case media::VideoFrame::STORAGE_OPAQUE: {
      std::optional<gfx::BufferFormat> buffer_format =
          media::VideoPixelFormatToGfxBufferFormat(pixel_format);
      CHECK(buffer_format) << "Pixel format "
                           << media::VideoPixelFormatToString(pixel_format)
                           << " has no corresponding gfx::BufferFormat";
      scoped_refptr<gpu::ClientSharedImage> shared_image =
          gpu::ClientSharedImage::CreateForTesting();

      return media::VideoFrame::WrapSharedImage(
          pixel_format, shared_image, gpu::SyncToken(), base::NullCallback(),
          coded_size, visible_rect, natural_size, timestamp);
    }
    default:
      NOTREACHED_IN_MIGRATION() << "Unsupported storage type or pixel format";
  }
  return nullptr;
}

}  // namespace blink
