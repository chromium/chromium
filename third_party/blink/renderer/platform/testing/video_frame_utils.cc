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
    gpu::TestSharedImageInterface* test_sii,
    const gfx::ColorSpace& color_space) {
  switch (storage_type) {
    case media::VideoFrame::STORAGE_OWNED_MEMORY:
      return media::VideoFrame::CreateZeroInitializedFrame(
          pixel_format, coded_size, visible_rect, natural_size, timestamp);
    case media::VideoFrame::STORAGE_MAPPABLE_SHARED_IMAGE: {
      CHECK(test_sii);
      std::optional<viz::SharedImageFormat> si_format =
          media::VideoPixelFormatToSharedImageFormat(pixel_format);
      CHECK(si_format) << "Pixel format "
                       << media::VideoPixelFormatToString(pixel_format)
                       << " has no corresponding viz::SharedImageFormat";
      const auto si_usage = gpu::SHARED_IMAGE_USAGE_CPU_WRITE_ONLY |
                            gpu::SHARED_IMAGE_USAGE_DISPLAY_READ |
                            gpu::SHARED_IMAGE_USAGE_RASTER_READ;
      auto shared_image = test_sii->CreateSharedImage(
          {*si_format, coded_size, gfx::ColorSpace(),
           gpu::SharedImageUsageSet(si_usage), "CreateTestFrame"},
          gpu::kNullSurfaceHandle,
          gfx::BufferUsage::VEA_READ_CAMERA_AND_CPU_READ_WRITE);
      if (!shared_image) {
        LOG(ERROR) << "Failed to create a mappable shared image.";
        return nullptr;
      }
      auto frame = media::VideoFrame::WrapMappableSharedImage(
          std::move(shared_image), test_sii->GenVerifiedSyncToken(),
          base::NullCallback(), visible_rect, natural_size, timestamp);

      return frame;
    }
    case media::VideoFrame::STORAGE_OPAQUE: {
      std::optional<viz::SharedImageFormat> si_format =
          media::VideoPixelFormatToSharedImageFormat(pixel_format);
      CHECK(si_format) << "Pixel format "
                       << media::VideoPixelFormatToString(pixel_format)
                       << " has no corresponding viz::SharedImageFormat";

      gpu::SharedImageMetadata metadata;
      metadata.format = *si_format;
      metadata.size = coded_size;
      metadata.color_space = color_space;
      metadata.surface_origin = kTopLeft_GrSurfaceOrigin;
      metadata.alpha_type = kOpaque_SkAlphaType;
      metadata.usage = gpu::SharedImageUsageSet();
      scoped_refptr<gpu::ClientSharedImage> shared_image =
          gpu::ClientSharedImage::CreateForTesting(metadata);
      auto frame = media::VideoFrame::WrapSharedImage(
          pixel_format, shared_image, gpu::SyncToken(), base::NullCallback(),
          coded_size, visible_rect, natural_size, timestamp);
      frame->set_color_space(color_space);
      return frame;
    }
    default:
      NOTREACHED() << "Unsupported storage type or pixel format";
  }
}

}  // namespace blink
