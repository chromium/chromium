// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/test/video_frame_helpers.h"

#include <utility>
#include <vector>

#include "base/bind_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "gpu/ipc/common/gpu_memory_buffer_support.h"
#include "gpu/ipc/service/gpu_memory_buffer_factory.h"
#include "media/base/color_plane_layout.h"
#include "media/base/format_utils.h"
#include "media/base/video_frame.h"
#include "media/gpu/buildflags.h"
#include "media/gpu/test/image.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libyuv/include/libyuv.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/gpu_memory_buffer.h"

#if BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)
#include "media/gpu/linux/platform_video_frame_utils.h"
#include "media/gpu/video_frame_mapper.h"
#include "media/gpu/video_frame_mapper_factory.h"
#endif  // BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)

namespace media {
namespace test {

namespace {

#define ASSERT_TRUE_OR_RETURN(predicate, return_value) \
  do {                                                 \
    if (!(predicate)) {                                \
      ADD_FAILURE();                                   \
      return (return_value);                           \
    }                                                  \
  } while (0)

bool ConvertVideoFrameToI420(const VideoFrame* src_frame,
                             VideoFrame* dst_frame) {
  ASSERT_TRUE_OR_RETURN(src_frame->visible_rect() == dst_frame->visible_rect(),
                        false);
  ASSERT_TRUE_OR_RETURN(dst_frame->format() == PIXEL_FORMAT_I420, false);

  const auto& visible_rect = src_frame->visible_rect();
  const int width = visible_rect.width();
  const int height = visible_rect.height();
  uint8_t* const dst_y = dst_frame->data(VideoFrame::kYPlane);
  uint8_t* const dst_u = dst_frame->data(VideoFrame::kUPlane);
  uint8_t* const dst_v = dst_frame->data(VideoFrame::kVPlane);
  const int dst_stride_y = dst_frame->stride(VideoFrame::kYPlane);
  const int dst_stride_u = dst_frame->stride(VideoFrame::kUPlane);
  const int dst_stride_v = dst_frame->stride(VideoFrame::kVPlane);

  switch (src_frame->format()) {
    case PIXEL_FORMAT_I420:
      return libyuv::I420Copy(src_frame->data(VideoFrame::kYPlane),
                              src_frame->stride(VideoFrame::kYPlane),
                              src_frame->data(VideoFrame::kUPlane),
                              src_frame->stride(VideoFrame::kUPlane),
                              src_frame->data(VideoFrame::kVPlane),
                              src_frame->stride(VideoFrame::kVPlane), dst_y,
                              dst_stride_y, dst_u, dst_stride_u, dst_v,
                              dst_stride_v, width, height) == 0;
    case PIXEL_FORMAT_NV12:
      return libyuv::NV12ToI420(src_frame->data(VideoFrame::kYPlane),
                                src_frame->stride(VideoFrame::kYPlane),
                                src_frame->data(VideoFrame::kUVPlane),
                                src_frame->stride(VideoFrame::kUVPlane), dst_y,
                                dst_stride_y, dst_u, dst_stride_u, dst_v,
                                dst_stride_v, width, height) == 0;
    case PIXEL_FORMAT_YV12:
      // Swap U and V planes.
      return libyuv::I420Copy(src_frame->data(VideoFrame::kYPlane),
                              src_frame->stride(VideoFrame::kYPlane),
                              src_frame->data(VideoFrame::kVPlane),
                              src_frame->stride(VideoFrame::kVPlane),
                              src_frame->data(VideoFrame::kUPlane),
                              src_frame->stride(VideoFrame::kUPlane), dst_y,
                              dst_stride_y, dst_u, dst_stride_u, dst_v,
                              dst_stride_v, width, height) == 0;
    default:
      LOG(ERROR) << "Unsupported input format: " << src_frame->format();
      return false;
  }
}

bool ConvertVideoFrameToARGB(const VideoFrame* src_frame,
                             VideoFrame* dst_frame) {
  ASSERT_TRUE_OR_RETURN(src_frame->visible_rect() == dst_frame->visible_rect(),
                        false);
  ASSERT_TRUE_OR_RETURN(dst_frame->format() == PIXEL_FORMAT_ARGB, false);

  const auto& visible_rect = src_frame->visible_rect();
  const int width = visible_rect.width();
  const int height = visible_rect.height();
  uint8_t* const dst_argb = dst_frame->data(VideoFrame::kARGBPlane);
  const int dst_stride = dst_frame->stride(VideoFrame::kARGBPlane);

  switch (src_frame->format()) {
    case PIXEL_FORMAT_I420:
      // Note that we use J420ToARGB instead of I420ToARGB so that the
      // kYuvJPEGConstants YUV-to-RGB conversion matrix is used.
      return libyuv::J420ToARGB(src_frame->data(VideoFrame::kYPlane),
                                src_frame->stride(VideoFrame::kYPlane),
                                src_frame->data(VideoFrame::kUPlane),
                                src_frame->stride(VideoFrame::kUPlane),
                                src_frame->data(VideoFrame::kVPlane),
                                src_frame->stride(VideoFrame::kVPlane),
                                dst_argb, dst_stride, width, height) == 0;
    case PIXEL_FORMAT_NV12:
      return libyuv::NV12ToARGB(src_frame->data(VideoFrame::kYPlane),
                                src_frame->stride(VideoFrame::kYPlane),
                                src_frame->data(VideoFrame::kUVPlane),
                                src_frame->stride(VideoFrame::kUVPlane),
                                dst_argb, dst_stride, width, height) == 0;
    case PIXEL_FORMAT_YV12:
      // Same as I420, but U and V planes are swapped.
      return libyuv::J420ToARGB(src_frame->data(VideoFrame::kYPlane),
                                src_frame->stride(VideoFrame::kYPlane),
                                src_frame->data(VideoFrame::kVPlane),
                                src_frame->stride(VideoFrame::kVPlane),
                                src_frame->data(VideoFrame::kUPlane),
                                src_frame->stride(VideoFrame::kUPlane),
                                dst_argb, dst_stride, width, height) == 0;
      break;
    default:
      LOG(ERROR) << "Unsupported input format: " << src_frame->format();
      return false;
  }
}

// Copy memory based |src_frame| buffer to |dst_frame| buffer.
bool CopyVideoFrame(const VideoFrame* src_frame,
                    scoped_refptr<VideoFrame> dst_frame) {
  ASSERT_TRUE_OR_RETURN(src_frame->IsMappable(), false);
#if BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)
  // If |dst_frame| is a Dmabuf-backed VideoFrame, we need to map its underlying
  // buffer into memory. We use a VideoFrameMapper to create a memory-based
  // VideoFrame that refers to the |dst_frame|'s buffer.
  if (dst_frame->storage_type() == VideoFrame::STORAGE_DMABUFS) {
    auto video_frame_mapper = VideoFrameMapperFactory::CreateMapper(
        dst_frame->format(), VideoFrame::STORAGE_DMABUFS, true);
    ASSERT_TRUE_OR_RETURN(video_frame_mapper, false);
    dst_frame = video_frame_mapper->Map(std::move(dst_frame));
    if (!dst_frame) {
      LOG(ERROR) << "Failed to map DMABuf video frame.";
      return false;
    }
  }
#endif  // BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)
  ASSERT_TRUE_OR_RETURN(dst_frame->IsMappable(), false);
  ASSERT_TRUE_OR_RETURN(src_frame->format() == dst_frame->format(), false);

  // Copy every plane's content from |src_frame| to |dst_frame|.
  const size_t num_planes = VideoFrame::NumPlanes(dst_frame->format());
  ASSERT_TRUE_OR_RETURN(dst_frame->layout().planes().size() == num_planes,
                        false);
  ASSERT_TRUE_OR_RETURN(src_frame->layout().planes().size() == num_planes,
                        false);
  for (size_t i = 0; i < num_planes; ++i) {
    // |width| in libyuv::CopyPlane() is in bytes, not pixels.
    gfx::Size plane_size = VideoFrame::PlaneSize(dst_frame->format(), i,
                                                 dst_frame->natural_size());
    libyuv::CopyPlane(
        src_frame->data(i), src_frame->layout().planes()[i].stride,
        dst_frame->data(i), dst_frame->layout().planes()[i].stride,
        plane_size.width(), plane_size.height());
  }
  return true;
}

}  // namespace

bool ConvertVideoFrame(const VideoFrame* src_frame, VideoFrame* dst_frame) {
  ASSERT_TRUE_OR_RETURN(src_frame->visible_rect() == dst_frame->visible_rect(),
                        false);
  ASSERT_TRUE_OR_RETURN(src_frame->IsMappable() && dst_frame->IsMappable(),
                        false);

  // Writing into non-owned memory might produce some unexpected side effects.
  if (dst_frame->storage_type() != VideoFrame::STORAGE_OWNED_MEMORY)
    LOG(WARNING) << "writing into non-owned memory";

  // Only I420 and ARGB are currently supported as output formats.
  switch (dst_frame->format()) {
    case PIXEL_FORMAT_I420:
      return ConvertVideoFrameToI420(src_frame, dst_frame);
    case PIXEL_FORMAT_ARGB:
      return ConvertVideoFrameToARGB(src_frame, dst_frame);
    default:
      LOG(ERROR) << "Unsupported output format: " << dst_frame->format();
      return false;
  }
}

scoped_refptr<VideoFrame> ConvertVideoFrame(const VideoFrame* src_frame,
                                            VideoPixelFormat dst_pixel_format) {
  gfx::Rect visible_rect = src_frame->visible_rect();
  auto dst_frame = VideoFrame::CreateFrame(
      dst_pixel_format, visible_rect.size(), visible_rect, visible_rect.size(),
      base::TimeDelta());
  if (!dst_frame) {
    LOG(ERROR) << "Failed to convert video frame to " << dst_frame->format();
    return nullptr;
  }
  bool conversion_success = ConvertVideoFrame(src_frame, dst_frame.get());
  if (!conversion_success) {
    LOG(ERROR) << "Failed to convert video frame to " << dst_frame->format();
    return nullptr;
  }
  return dst_frame;
}

scoped_refptr<VideoFrame> CloneVideoFrame(
    gpu::GpuMemoryBufferFactory* gpu_memory_buffer_factory,
    const VideoFrame* const src_frame,
    const VideoFrameLayout& dst_layout,
    VideoFrame::StorageType dst_storage_type,
    base::Optional<gfx::BufferUsage> dst_buffer_usage) {
  if (!src_frame)
    return nullptr;
  if (!src_frame->IsMappable()) {
    LOG(ERROR) << "The source video frame must be memory-backed VideoFrame";
    return nullptr;
  }

  scoped_refptr<VideoFrame> dst_frame;
  switch (dst_storage_type) {
#if BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)
    case VideoFrame::STORAGE_GPU_MEMORY_BUFFER:
    case VideoFrame::STORAGE_DMABUFS:
      if (!dst_buffer_usage) {
        LOG(ERROR) << "Buffer usage is not specified for a graphic buffer";
        return nullptr;
      }
      dst_frame = CreatePlatformVideoFrame(
          gpu_memory_buffer_factory, dst_layout.format(),
          dst_layout.coded_size(), src_frame->visible_rect(),
          src_frame->visible_rect().size(), src_frame->timestamp(),
          *dst_buffer_usage);
      break;
#endif  // BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)
    case VideoFrame::STORAGE_OWNED_MEMORY:
      // Create VideoFrame, which allocates and owns data.
      dst_frame = VideoFrame::CreateFrameWithLayout(
          dst_layout, src_frame->visible_rect(), src_frame->natural_size(),
          src_frame->timestamp(), false /* zero_initialize_memory*/);
      break;
    default:
      LOG(ERROR) << "Clone video frame must have the ownership of the buffer";
      return nullptr;
  }

  if (!dst_frame) {
    LOG(ERROR) << "Failed to create VideoFrame";
    return nullptr;
  }

  if (!CopyVideoFrame(src_frame, dst_frame)) {
    LOG(ERROR) << "Failed to copy VideoFrame";
    return nullptr;
  }

  if (dst_storage_type == VideoFrame::STORAGE_GPU_MEMORY_BUFFER) {
    // Here, the content in |src_frame| is already copied to |dst_frame|, which
    // is a DMABUF based VideoFrame.
    // Create GpuMemoryBuffer based VideoFrame from |dst_frame|.
    dst_frame = CreateGpuMemoryBufferVideoFrame(
        gpu_memory_buffer_factory, dst_frame.get(), *dst_buffer_usage);
  }

  return dst_frame;
}

scoped_refptr<VideoFrame> CreateGpuMemoryBufferVideoFrame(
    gpu::GpuMemoryBufferFactory* gpu_memory_buffer_factory,
    const VideoFrame* const frame,
    gfx::BufferUsage buffer_usage) {
  gfx::GpuMemoryBufferHandle gmb_handle;
#if BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)
  gmb_handle = CreateGpuMemoryBufferHandle(frame);
#endif
  if (gmb_handle.is_null() || gmb_handle.type != gfx::NATIVE_PIXMAP) {
    LOG(ERROR) << "Failed to create native GpuMemoryBufferHandle";
    return nullptr;
  }

  base::Optional<gfx::BufferFormat> buffer_format =
      VideoPixelFormatToGfxBufferFormat(frame->format());
  if (!buffer_format) {
    LOG(ERROR) << "Unexpected format: " << frame->format();
    return nullptr;
  }

  // Create GpuMemoryBuffer from GpuMemoryBufferHandle.
  gpu::GpuMemoryBufferSupport support;
  std::unique_ptr<gfx::GpuMemoryBuffer> gpu_memory_buffer =
      support.CreateGpuMemoryBufferImplFromHandle(
          std::move(gmb_handle), frame->coded_size(), *buffer_format,
          buffer_usage, base::DoNothing());
  if (!gpu_memory_buffer) {
    LOG(ERROR) << "Failed to create GpuMemoryBuffer from GpuMemoryBufferHandle";
    return nullptr;
  }

  gpu::MailboxHolder dummy_mailbox[media::VideoFrame::kMaxPlanes];
  return media::VideoFrame::WrapExternalGpuMemoryBuffer(
      frame->visible_rect(), frame->natural_size(),
      std::move(gpu_memory_buffer), dummy_mailbox,
      base::DoNothing() /* mailbox_holder_release_cb_ */, frame->timestamp());
}

scoped_refptr<const VideoFrame> CreateVideoFrameFromImage(const Image& image) {
  DCHECK(image.IsLoaded());
  const auto format = image.PixelFormat();
  const auto& visible_size = image.Size();
  // Loaded image data must be tight.
  DCHECK_EQ(image.DataSize(), VideoFrame::AllocationSize(format, visible_size));

  // Create planes for layout. We cannot use WrapExternalData() because it
  // calls GetDefaultLayout() and it supports only a few pixel formats.
  base::Optional<VideoFrameLayout> layout =
      CreateVideoFrameLayout(format, visible_size);
  if (!layout) {
    LOG(ERROR) << "Failed to create VideoFrameLayout";
    return nullptr;
  }

  scoped_refptr<const VideoFrame> video_frame =
      VideoFrame::WrapExternalDataWithLayout(
          *layout, gfx::Rect(visible_size), visible_size, image.Data(),
          image.DataSize(), base::TimeDelta());
  if (!video_frame) {
    LOG(ERROR) << "Failed to create VideoFrame";
    return nullptr;
  }

  return video_frame;
}

base::Optional<VideoFrameLayout> CreateVideoFrameLayout(VideoPixelFormat format,
                                                        const gfx::Size& size) {
  const size_t num_planes = VideoFrame::NumPlanes(format);

  std::vector<ColorPlaneLayout> planes(num_planes);
  const auto strides = VideoFrame::ComputeStrides(format, size);
  size_t offset = 0;
  for (size_t i = 0; i < num_planes; ++i) {
    planes[i].stride = strides[i];
    planes[i].offset = offset;
    planes[i].size = VideoFrame::PlaneSize(format, i, size).GetArea();
    offset += planes[i].size;
  }
  return VideoFrameLayout::CreateWithPlanes(format, size, std::move(planes));
}

size_t CompareFramesWithErrorDiff(const VideoFrame& frame1,
                                  const VideoFrame& frame2,
                                  uint8_t tolerance) {
  ASSERT_TRUE_OR_RETURN(frame1.format() == frame2.format(),
                        std::numeric_limits<std::size_t>::max());
  ASSERT_TRUE_OR_RETURN(frame1.visible_rect() == frame2.visible_rect(),
                        std::numeric_limits<std::size_t>::max());
  ASSERT_TRUE_OR_RETURN(frame1.visible_rect().origin() == gfx::Point(0, 0),
                        std::numeric_limits<std::size_t>::max());
  ASSERT_TRUE_OR_RETURN(frame1.IsMappable() && frame2.IsMappable(),
                        std::numeric_limits<std::size_t>::max());
  size_t diff_cnt = 0;

  const VideoPixelFormat format = frame1.format();
  const size_t num_planes = VideoFrame::NumPlanes(format);
  const gfx::Size& visible_size = frame1.visible_rect().size();
  for (size_t i = 0; i < num_planes; ++i) {
    const uint8_t* data1 = frame1.data(i);
    const int stride1 = frame1.stride(i);
    const uint8_t* data2 = frame2.data(i);
    const int stride2 = frame2.stride(i);
    const size_t rows = VideoFrame::Rows(i, format, visible_size.height());
    const int row_bytes = VideoFrame::RowBytes(i, format, visible_size.width());
    for (size_t r = 0; r < rows; ++r) {
      for (int c = 0; c < row_bytes; c++) {
        uint8_t b1 = data1[(stride1 * r) + c];
        uint8_t b2 = data2[(stride2 * r) + c];
        uint8_t diff = std::max(b1, b2) - std::min(b1, b2);
        diff_cnt += diff > tolerance;
      }
    }
  }
  return diff_cnt;
}

}  // namespace test
}  // namespace media
