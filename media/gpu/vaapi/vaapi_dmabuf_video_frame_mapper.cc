// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/vaapi/vaapi_dmabuf_video_frame_mapper.h"

#include <sys/mman.h>

#include "base/bind.h"
#include "base/callback_helpers.h"
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

void ConvertP010ToP016LE(const uint16_t* src,
                         int src_stride,
                         uint16_t* dst,
                         int dst_stride,
                         int width,
                         int height) {
  // The P010 buffer layout is (meaningful 10bits:0) in two bytes like
  // ABCDEFGHIJ000000. However, libvpx's output is (0:meaningful 10bits) in two
  // bytes like 000000ABCDEFGHIJ. Although the P016LE buffer layout is
  // undefined, we locally define the layout as the same as libvpx's layout and
  // convert here for testing.
  for (int i = 0; i < height; i++) {
    for (int j = 0; j < width; j++) {
      constexpr int kShiftBits = 6;
      const uint16_t v = src[j];
      dst[j] = v >> kShiftBits;
    }
    src = reinterpret_cast<const uint16_t*>(
        reinterpret_cast<const uint8_t*>(src) + src_stride);
    dst = reinterpret_cast<uint16_t*>(reinterpret_cast<uint8_t*>(dst) +
                                      dst_stride);
  }
}

void DeallocateBuffers(std::unique_ptr<ScopedVAImage> va_image,
                       scoped_refptr<const VideoFrame> /* video_frame */) {
  // The |video_frame| will be released here and it will be returned to pool if
  // client uses video frame pool.
  // Destructing ScopedVAImage releases its owned memory.
  DCHECK(va_image->IsValid());
}

scoped_refptr<VideoFrame> CreateMappedVideoFrame(
    scoped_refptr<const VideoFrame> src_video_frame,
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

  // Create new buffers and copy P010 buffers into because we should not modify
  // va_image->va_buffer->data().
  std::vector<std::unique_ptr<uint16_t[]>> p016le_buffers(kNumPlanes);
  if (src_video_frame->format() == PIXEL_FORMAT_P016LE) {
    for (size_t i = 0; i < kNumPlanes; i++) {
      const gfx::Size plane_dimensions_in_bytes = VideoFrame::PlaneSize(
          PIXEL_FORMAT_P016LE, i, src_video_frame->visible_rect().size());
      const gfx::Size plane_dimensions_in_16bit_words(
          plane_dimensions_in_bytes.width() / 2,
          plane_dimensions_in_bytes.height());
      p016le_buffers[i] = std::make_unique<uint16_t[]>(planes[i].size / 2);
      ConvertP010ToP016LE(reinterpret_cast<const uint16_t*>(addrs[i]),
                          planes[i].stride, p016le_buffers[i].get(),
                          planes[i].stride,
                          plane_dimensions_in_16bit_words.width(),
                          plane_dimensions_in_16bit_words.height());
      addrs[i] = reinterpret_cast<uint8_t*>(p016le_buffers[i].get());
    }
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
  for (auto&& buffer : p016le_buffers) {
    video_frame->AddDestructionObserver(
        base::DoNothingWithBoundArgs(std::move(buffer)));
  }
  return video_frame;
}

bool IsFormatSupported(VideoPixelFormat format) {
  return format == PIXEL_FORMAT_NV12 || format == PIXEL_FORMAT_P016LE;
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
  if (!video_frame_mapper->vaapi_wrapper_)
    return nullptr;

  return video_frame_mapper;
}

VaapiDmaBufVideoFrameMapper::VaapiDmaBufVideoFrameMapper(
    VideoPixelFormat format)
    : VideoFrameMapper(format),
      vaapi_wrapper_(VaapiWrapper::Create(VaapiWrapper::kVideoProcess,
                                          VAProfileNone,
                                          EncryptionScheme::kUnencrypted,
                                          base::DoNothing())) {}

VaapiDmaBufVideoFrameMapper::~VaapiDmaBufVideoFrameMapper() {}

scoped_refptr<VideoFrame> VaapiDmaBufVideoFrameMapper::Map(
    scoped_refptr<const VideoFrame> video_frame,
    int permissions) const {
  DCHECK(vaapi_wrapper_);
  if (!video_frame) {
    LOG(ERROR) << "Video frame is nullptr";
    return nullptr;
  }

  if (!(permissions & PROT_READ && permissions & PROT_WRITE)) {
    LOG(ERROR) << "VAAPI DMA Buffer must be mapped read/write.";
    return nullptr;
  }

  if (!video_frame->HasDmaBufs())
    return nullptr;

  if (video_frame->format() != format_) {
    VLOGF(1) << "Unexpected format, got: "
             << VideoPixelFormatToString(video_frame->format())
             << ", expected: " << VideoPixelFormatToString(format_);
    return nullptr;
  }

  scoped_refptr<gfx::NativePixmap> pixmap =
      CreateNativePixmapDmaBuf(video_frame.get());
  if (!pixmap) {
    VLOGF(1) << "Failed to create NativePixmap from VideoFrame";
    return nullptr;
  }

  scoped_refptr<VASurface> va_surface =
      vaapi_wrapper_->CreateVASurfaceForPixmap(std::move(pixmap));

  if (!va_surface) {
    VLOGF(1) << "Failed to create VASurface";
    return nullptr;
  }

  // Map tiled NV12 or P010 buffer by CreateVaImage so that mapped buffers can
  // be accessed as non-tiled NV12 or P016LE buffer.
  VAImageFormat va_image_format = video_frame->format() == PIXEL_FORMAT_NV12
                                      ? kImageFormatNV12
                                      : kImageFormatP010;
  auto va_image = vaapi_wrapper_->CreateVaImage(
      va_surface->id(), &va_image_format, va_surface->size());
  if (!va_image || !va_image->IsValid()) {
    VLOGF(1) << "Failed in CreateVaImage.";
    return nullptr;
  }

  return CreateMappedVideoFrame(std::move(video_frame), std::move(va_image));
}

}  // namespace media
