// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/gpu/vaapi/vaapi_dmabuf_video_frame_mapper.h"

#include <sys/mman.h>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "build/build_config.h"
#include "media/base/color_plane_layout.h"
#include "media/gpu/chromeos/platform_video_frame_utils.h"
#include "media/gpu/macros.h"
#include "media/gpu/vaapi/vaapi_utils.h"
#include "media/gpu/vaapi/vaapi_wrapper.h"

namespace media {

namespace {

constexpr VAImageFormat kImageFormatNV12{.fourcc = VA_FOURCC_NV12,
                                         .byte_order = VA_LSB_FIRST,
                                         .bits_per_pixel = 12};

constexpr VAImageFormat kImageFormatP010{.fourcc = VA_FOURCC_P010,
                                         .byte_order = VA_LSB_FIRST,
                                         .bits_per_pixel = 16};

void DeallocateBuffers(std::unique_ptr<ScopedVAImage> va_image,
                       scoped_refptr<const FrameResource> /* video_frame */) {
  // The |video_frame| will be released here and it will be returned to pool if
  // client uses video frame pool.
  // Destructing ScopedVAImage releases its owned memory.
  DCHECK(va_image);
}

scoped_refptr<VideoFrame> CreateMappedVideoFrame(
    scoped_refptr<const FrameResource> src_video_frame,
    std::unique_ptr<ScopedVAImage> va_image) {
  DCHECK(va_image);
  // ScopedVAImage manages the resource of mapped data. That is, ScopedVAImage's
  // dtor releases the mapped resource.
  constexpr size_t kNumPlanes = 2u;
  DCHECK_EQ(VideoFrame::NumPlanes(src_video_frame->format()), kNumPlanes);
  if (va_image->image()->num_planes != kNumPlanes) {
    VLOGF(1) << "The number of planes of VAImage is not expected. "
             << "(expected: " << kNumPlanes
             << ", VAImage: " << va_image->image()->num_planes << ")";
    return nullptr;
  }

  // All the planes are stored in the same buffer, VAImage.va_buffer.
  std::vector<ColorPlaneLayout> planes(kNumPlanes);
  uint8_t* addrs[VideoFrame::kMaxPlanes] = {};
  for (size_t i = 0; i < kNumPlanes; i++) {
    planes[i].stride = va_image->image()->pitches[i];
    planes[i].offset = va_image->image()->offsets[i];
    addrs[i] = static_cast<uint8_t*>(va_image->va_buffer()->data()) +
               va_image->image()->offsets[i];
  }

  // The size of each plane is not given by VAImage. We compute the size to be
  // mapped from offset and the entire buffer size (data_size).
  for (size_t i = 0; i < kNumPlanes; i++) {
    if (i < kNumPlanes - 1)
      planes[i].size = planes[i + 1].offset - planes[i].offset;
    else
      planes[i].size = va_image->image()->data_size - planes[i].offset;
  }

  auto mapped_layout = VideoFrameLayout::CreateWithPlanes(
      src_video_frame->format(),
      gfx::Size(va_image->image()->width, va_image->image()->height),
      std::move(planes));
  if (!mapped_layout) {
    VLOGF(1) << "Failed to create VideoFrameLayout for VAImage";
    return nullptr;
  }
  auto video_frame = VideoFrame::WrapExternalYuvDataWithLayout(
      *mapped_layout, src_video_frame->visible_rect(),
      src_video_frame->visible_rect().size(), addrs[0], addrs[1], addrs[2],
      src_video_frame->timestamp());
  if (!video_frame)
    return nullptr;

  // The source video frame should not be released until the mapped
  // |video_frame| is destructed, because |video_frame| holds |va_image|.
  video_frame->AddDestructionObserver(base::BindOnce(
      DeallocateBuffers, std::move(va_image), std::move(src_video_frame)));

  return video_frame;
}

bool IsFormatSupported(VideoPixelFormat format) {
  return format == PIXEL_FORMAT_NV12 || format == PIXEL_FORMAT_P010LE;
}

}  // namespace

// static
std::unique_ptr<VideoFrameMapper> VaapiDmaBufVideoFrameMapper::Create(
    VideoPixelFormat format) {
  if (!IsFormatSupported(format)) {
    VLOGF(1) << " Unsupported format: " << VideoPixelFormatToString(format);
    return nullptr;
  }

  auto video_frame_mapper =
      base::WrapUnique(new VaapiDmaBufVideoFrameMapper(format));

  return video_frame_mapper;
}

VaapiDmaBufVideoFrameMapper::VaapiDmaBufVideoFrameMapper(
    VideoPixelFormat format)
    : VideoFrameMapper(format) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

VaapiDmaBufVideoFrameMapper::~VaapiDmaBufVideoFrameMapper() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

scoped_refptr<VideoFrame> VaapiDmaBufVideoFrameMapper::MapFrame(
    scoped_refptr<const FrameResource> video_frame,
    int permissions) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!vaapi_wrapper_) {
    vaapi_wrapper_ =
        VaapiWrapper::Create(VaapiWrapper::kVideoProcess, VAProfileNone,
                             EncryptionScheme::kUnencrypted, base::DoNothing())
            .value_or(nullptr);
    if (!vaapi_wrapper_) {
      VLOGF(1) << "Failed to create VaapiWrapper";
      return nullptr;
    }
  }
  DCHECK(vaapi_wrapper_);
  if (!video_frame) {
    LOG(ERROR) << "Video frame is nullptr";
    return nullptr;
  }

  if (!(permissions & PROT_READ)) {
    LOG(ERROR) << "VAAPI DMA Buffer must be mapped with read permissions.";
    return nullptr;
  }

  if (video_frame->format() != format_) {
    VLOGF(1) << "Unexpected format, got: "
             << VideoPixelFormatToString(video_frame->format())
             << ", expected: " << VideoPixelFormatToString(format_);
    return nullptr;
  }

  scoped_refptr<const gfx::NativePixmap> pixmap =
      video_frame->GetNativePixmapDmaBuf();
  if (!pixmap) {
    VLOGF(1) << "Failed to create NativePixmap from VideoFrame";
    return nullptr;
  }

  const std::unique_ptr<ScopedVASurface> va_surface =
      vaapi_wrapper_->CreateVASurfaceForPixmap(std::move(pixmap));

  if (!va_surface) {
    VLOGF(1) << "Failed to create VASurface";
    return nullptr;
  }

  // Map tiled NV12 or P010 buffer by CreateVaImage so that mapped buffers can
  // be accessed as non-tiled NV12 or P010LE buffer.
  const VAImageFormat va_image_format =
      video_frame->format() == PIXEL_FORMAT_NV12 ? kImageFormatNV12
                                                 : kImageFormatP010;
  auto va_image = vaapi_wrapper_->CreateVaImage(
      va_surface->id(), va_image_format, va_surface->size());
  if (!va_image) {
    VLOGF(1) << "Failed in CreateVaImage.";
    return nullptr;
  }

  return CreateMappedVideoFrame(std::move(video_frame), std::move(va_image));
}

}  // namespace media
