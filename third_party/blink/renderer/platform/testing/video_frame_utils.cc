// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/testing/video_frame_utils.h"

#include "base/functional/callback_helpers.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "gpu/command_buffer/client/test_shared_image_interface.h"
#include "media/base/format_utils.h"

namespace blink {

scoped_refptr<media::VideoFrame> CreateTestFrame(
    const gfx::Size& coded_size,
    const gfx::Rect& visible_rect,
    const gfx::Size& natural_size,
    media::VideoFrame::StorageType storage_type,
    gpu::TestSharedImageInterface* test_sii) {
  return CreateTestFrame(coded_size, visible_rect, natural_size, storage_type,
                         storage_type == media::VideoFrame::STORAGE_OWNED_MEMORY
                             ? media::PIXEL_FORMAT_I420
                             : media::PIXEL_FORMAT_NV12,
                         base::TimeDelta(), test_sii);
}

scoped_refptr<media::VideoFrame> CreateTestFrame(
    const gfx::Size& coded_size,
    const gfx::Rect& visible_rect,
    const gfx::Size& natural_size,
    media::VideoFrame::StorageType storage_type,
    media::VideoPixelFormat pixel_format,
    base::TimeDelta timestamp,
    gpu::TestSharedImageInterface* test_sii) {
  switch (storage_type) {
    case media::VideoFrame::STORAGE_OWNED_MEMORY:
      return media::VideoFrame::CreateZeroInitializedFrame(
          pixel_format, coded_size, visible_rect, natural_size, timestamp);
    case media::VideoFrame::STORAGE_GPU_MEMORY_BUFFER: {
      CHECK(test_sii);
      std::optional<gfx::BufferFormat> buffer_format =
          media::VideoPixelFormatToGfxBufferFormat(pixel_format);
      CHECK(buffer_format) << "Pixel format "
                           << media::VideoPixelFormatToString(pixel_format)
                           << " has no corresponding gfx::BufferFormat";
      const auto si_usage = gpu::SHARED_IMAGE_USAGE_CPU_WRITE_ONLY |
                            gpu::SHARED_IMAGE_USAGE_DISPLAY_READ;
      auto shared_image = test_sii->CreateSharedImage(
          {viz::GetSharedImageFormat(*buffer_format), coded_size,
           gfx::ColorSpace(), gpu::SharedImageUsageSet(si_usage),
           "CreateTestFrame"},
          gpu::kNullSurfaceHandle,
          gfx::BufferUsage::VEA_READ_CAMERA_AND_CPU_READ_WRITE);
      if (!shared_image) {
        LOG(ERROR) << "Failed to create a mappable shared image.";
        return nullptr;
      }
      auto frame = media::VideoFrame::WrapMappableSharedImage(
          std::move(shared_image), test_sii->GenVerifiedSyncToken(),
          base::NullCallback(), visible_rect, natural_size, timestamp);

      // Frame created here are not intended for rendering. Hence explicitly
      // marking it as non texturable since checking
      // VideoFrame::HasSharedImage() is not enough in this case.
      frame->DisableTexturingForTesting();
      return frame;
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
      NOTREACHED() << "Unsupported storage type or pixel format";
  }
}

}  // namespace blink
