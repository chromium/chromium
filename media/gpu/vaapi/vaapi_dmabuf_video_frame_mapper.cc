// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/vaapi/vaapi_dmabuf_video_frame_mapper.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/ptr_util.h"
#include "build/build_config.h"
#include "media/base/color_plane_layout.h"
#include "media/gpu/linux/platform_video_frame_utils.h"
#include "media/gpu/macros.h"
#include "media/gpu/vaapi/vaapi_utils.h"
#include "media/gpu/vaapi/vaapi_wrapper.h"

namespace media {

namespace {

constexpr VAImageFormat kImageFormatNV12{.fourcc = VA_FOURCC_NV12,
                                         .byte_order = VA_LSB_FIRST,
                                         .bits_per_pixel = 12};

void DeallocateBuffers(std::unique_ptr<ScopedVAImage> va_image,
                       scoped_refptr<const VideoFrame> /* video_frame */) {
  // The |video_frame| will be released here and it will be returned to pool if
  // client uses video frame pool.
  // Destructing ScopedVAImage releases its owned memory.
  DCHECK(va_image->IsValid());
}

scoped_refptr<VideoFrame> CreateMappedVideoFrame(
    const VideoPixelFormat format,
    scoped_refptr<const VideoFrame> src_video_frame,
    std::unique_ptr<ScopedVAImage> va_image) {
  DCHECK(va_image);
  // ScopedVAImage manages the resource of mapped data. That is, ScopedVAImage's
  // dtor releases the mapped resource.
  const size_t num_planes = VideoFrame::NumPlanes(format);
  if (num_planes != va_image->image()->num_planes) {
    VLOGF(1) << "The number of planes of VAImage is not expected. "
             << "(expected: " << num_planes
             << ", VAImage: " << va_image->image()->num_planes << ")";
    return nullptr;
  }

  // All the planes are stored in the same buffer, VAImage.va_buffer.
  std::vector<ColorPlaneLayout> planes(num_planes);
  std::vector<uint8_t*> addrs(num_planes, nullptr);
  for (size_t i = 0; i < num_planes; i++) {
    planes[i].stride = va_image->image()->pitches[i];
    planes[i].offset = va_image->image()->offsets[i];
    addrs[i] = static_cast<uint8_t*>(va_image->va_buffer()->data()) +
               va_image->image()->offsets[i];
  }

  // The size of each plane is not given by VAImage. We compute the size to be
  // mapped from offset and the entire buffer size (data_size).
  for (size_t i = 0; i < num_planes; i++) {
    if (i < num_planes - 1) {
      planes[i].size = planes[i + 1].offset - planes[i].offset;
    } else {
      planes[i].size = va_image->image()->data_size - planes[i].offset;
    }
  }

  auto mapped_layout = VideoFrameLayout::CreateWithPlanes(
      format, gfx::Size(va_image->image()->width, va_image->image()->height),
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
  return format == PIXEL_FORMAT_NV12;
}

}  // namespace

// static
std::unique_ptr<VideoFrameMapper> VaapiDmaBufVideoFrameMapper::Create(
    VideoPixelFormat format) {
  if (!IsFormatSupported(format)) {
    VLOGF(1) << " Unsupported format: " << format;
    return nullptr;
  }

  auto video_frame_mapper =
      base::WrapUnique(new VaapiDmaBufVideoFrameMapper(format));
  if (!video_frame_mapper->vaapi_wrapper_)
    return nullptr;

  return video_frame_mapper;
}

// While kDecode and H264PROFILE_MAIN are set here,  the mode and profile are
// not required for VaapiWrapper to perform pixel format conversion.
// TODO(crbug.com/898423): Create a VaapiWrapper only for pixel format
// conversion. Either mode or profile isn't required to create the VaapiWrapper.
VaapiDmaBufVideoFrameMapper::VaapiDmaBufVideoFrameMapper(
    VideoPixelFormat format)
    : VideoFrameMapper(format),
      vaapi_wrapper_(VaapiWrapper::CreateForVideoCodec(VaapiWrapper::kDecode,
                                                       H264PROFILE_MAIN,
                                                       base::DoNothing())) {}

VaapiDmaBufVideoFrameMapper::~VaapiDmaBufVideoFrameMapper() {}

scoped_refptr<VideoFrame> VaapiDmaBufVideoFrameMapper::Map(
    scoped_refptr<const VideoFrame> video_frame) const {
  DCHECK(vaapi_wrapper_);
  if (!video_frame) {
    LOG(ERROR) << "Video frame is nullptr";
    return nullptr;
  }

  if (!video_frame->HasDmaBufs()) {
    return nullptr;
  }
  if (video_frame->format() != format_) {
    VLOGF(1) << "Unexpected format: " << video_frame->format();
    return nullptr;
  }

  scoped_refptr<VASurface> va_surface =
      vaapi_wrapper_->CreateVASurfaceForVideoFrame(video_frame.get());
  if (!va_surface) {
    VLOGF(1) << "Failed to create VASurface";
    return nullptr;
  }

  // Map tiled NV12 buffer by CreateVaImage so that mapped buffers can be
  // accessed as non-tiled NV12 buffer.
  constexpr VideoPixelFormat kConvertedFormat = PIXEL_FORMAT_NV12;
  VAImageFormat va_image_format = kImageFormatNV12;
  auto va_image = vaapi_wrapper_->CreateVaImage(
      va_surface->id(), &va_image_format, va_surface->size());
  if (!va_image || !va_image->IsValid()) {
    VLOGF(1) << "Failed in CreateVaImage.";
    return nullptr;
  }

  return CreateMappedVideoFrame(kConvertedFormat, std::move(video_frame),
                                std::move(va_image));
}

}  // namespace media
