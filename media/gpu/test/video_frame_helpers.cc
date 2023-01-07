// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/test/video_frame_helpers.h"

#include <sys/mman.h>
#include <utility>
#include <vector>

#include "base/bits.h"
#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "gpu/ipc/common/gpu_memory_buffer_support.h"
#include "media/base/color_plane_layout.h"
#include "media/base/format_utils.h"
#include "media/base/video_frame.h"
#include "media/gpu/test/image.h"
#include "media/media_buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libyuv/include/libyuv.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/gpu_memory_buffer.h"

#if BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)
#include "media/gpu/chromeos/platform_video_frame_utils.h"
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

// Split 16-bit UV plane to 16bit U plane and 16 bit V plane.
void SplitUVRow_16(const uint16_t* src_uv,
                   uint16_t* dst_u,
                   uint16_t* dst_v,
                   int width_in_samples) {
  for (int i = 0; i < width_in_samples; i++) {
    dst_u[i] = src_uv[0];
    dst_v[i] = src_uv[1];
    src_uv += 2;
  }
}

// Convert 16 bit NV12 to 16 bit I420. The strides in these arguments are in
// bytes.
void P016LEToI420P016(const uint8_t* src_y,
                      int src_stride_y,
                      const uint8_t* src_uv,
                      int src_stride_uv,
                      uint8_t* dst_y,
                      int dst_stride_y,
                      uint8_t* dst_u,
                      int dst_stride_u,
                      uint8_t* dst_v,
                      int dst_stride_v,
                      int width,
                      int height) {
  libyuv::CopyPlane_16(reinterpret_cast<const uint16_t*>(src_y),
                       src_stride_y / 2, reinterpret_cast<uint16_t*>(dst_y),
                       dst_stride_y / 2, width, height);
  const int half_width = (width + 1) / 2;
  const int half_height = (height + 1) / 2;
  for (int i = 0; i < half_height; i++) {
    SplitUVRow_16(reinterpret_cast<const uint16_t*>(src_uv),
                  reinterpret_cast<uint16_t*>(dst_u),
                  reinterpret_cast<uint16_t*>(dst_v), half_width);
    dst_u += dst_stride_u;
    dst_v += dst_stride_v;
    src_uv += src_stride_uv;
  }
}

bool ConvertVideoFrameToI420(const VideoFrame* src_frame,
                             VideoFrame* dst_frame) {
  ASSERT_TRUE_OR_RETURN(src_frame->visible_rect() == dst_frame->visible_rect(),
                        false);
  ASSERT_TRUE_OR_RETURN(dst_frame->format() == PIXEL_FORMAT_I420, false);

  // Convert the visible area.
  const auto& visible_rect = src_frame->visible_rect();
  const int width = visible_rect.width();
  const int height = visible_rect.height();
  uint8_t* const dst_y = dst_frame->GetWritableVisibleData(VideoFrame::kYPlane);
  uint8_t* const dst_u = dst_frame->GetWritableVisibleData(VideoFrame::kUPlane);
  uint8_t* const dst_v = dst_frame->GetWritableVisibleData(VideoFrame::kVPlane);
  const int dst_stride_y = dst_frame->stride(VideoFrame::kYPlane);
  const int dst_stride_u = dst_frame->stride(VideoFrame::kUPlane);
  const int dst_stride_v = dst_frame->stride(VideoFrame::kVPlane);

  switch (src_frame->format()) {
    case PIXEL_FORMAT_I420:
      return libyuv::I420Copy(src_frame->visible_data(VideoFrame::kYPlane),
                              src_frame->stride(VideoFrame::kYPlane),
                              src_frame->visible_data(VideoFrame::kUPlane),
                              src_frame->stride(VideoFrame::kUPlane),
                              src_frame->visible_data(VideoFrame::kVPlane),
                              src_frame->stride(VideoFrame::kVPlane), dst_y,
                              dst_stride_y, dst_u, dst_stride_u, dst_v,
                              dst_stride_v, width, height) == 0;
    case PIXEL_FORMAT_NV12:
      return libyuv::NV12ToI420(src_frame->visible_data(VideoFrame::kYPlane),
                                src_frame->stride(VideoFrame::kYPlane),
                                src_frame->visible_data(VideoFrame::kUVPlane),
                                src_frame->stride(VideoFrame::kUVPlane), dst_y,
                                dst_stride_y, dst_u, dst_stride_u, dst_v,
                                dst_stride_v, width, height) == 0;
    case PIXEL_FORMAT_YV12:
      // Swap U and V planes.
      return libyuv::I420Copy(src_frame->visible_data(VideoFrame::kYPlane),
                              src_frame->stride(VideoFrame::kYPlane),
                              src_frame->visible_data(VideoFrame::kVPlane),
                              src_frame->stride(VideoFrame::kVPlane),
                              src_frame->visible_data(VideoFrame::kUPlane),
                              src_frame->stride(VideoFrame::kUPlane), dst_y,
                              dst_stride_y, dst_u, dst_stride_u, dst_v,
                              dst_stride_v, width, height) == 0;
    default:
      LOG(ERROR) << "Unsupported input format: " << src_frame->format();
      return false;
  }
}

bool ConvertVideoFrameToYUV420P10(const VideoFrame* src_frame,
                                  VideoFrame* dst_frame) {
  if (src_frame->format() != PIXEL_FORMAT_P016LE) {
    LOG(ERROR) << "Unsupported input format: "
               << VideoPixelFormatToString(src_frame->format());
    return false;
  }

  // Convert the visible area.
  const auto& visible_rect = src_frame->visible_rect();
  const int width = visible_rect.width();
  const int height = visible_rect.height();
  uint8_t* const dst_y = dst_frame->GetWritableVisibleData(VideoFrame::kYPlane);
  uint8_t* const dst_u = dst_frame->GetWritableVisibleData(VideoFrame::kUPlane);
  uint8_t* const dst_v = dst_frame->GetWritableVisibleData(VideoFrame::kVPlane);
  const int dst_stride_y = dst_frame->stride(VideoFrame::kYPlane);
  const int dst_stride_u = dst_frame->stride(VideoFrame::kUPlane);
  const int dst_stride_v = dst_frame->stride(VideoFrame::kVPlane);
  P016LEToI420P016(src_frame->visible_data(VideoFrame::kYPlane),
                   src_frame->stride(VideoFrame::kYPlane),
                   src_frame->visible_data(VideoFrame::kUVPlane),
                   src_frame->stride(VideoFrame::kUVPlane), dst_y, dst_stride_y,
                   dst_u, dst_stride_u, dst_v, dst_stride_v, width, height);
  return true;
}

bool ConvertVideoFrameToARGB(const VideoFrame* src_frame,
                             VideoFrame* dst_frame) {
  ASSERT_TRUE_OR_RETURN(src_frame->visible_rect() == dst_frame->visible_rect(),
                        false);
  ASSERT_TRUE_OR_RETURN(dst_frame->format() == PIXEL_FORMAT_ARGB, false);

  // Convert the visible area.
  const auto& visible_rect = src_frame->visible_rect();
  const int width = visible_rect.width();
  const int height = visible_rect.height();
  uint8_t* const dst_argb =
      dst_frame->GetWritableVisibleData(VideoFrame::kARGBPlane);
  const int dst_stride = dst_frame->stride(VideoFrame::kARGBPlane);

  switch (src_frame->format()) {
    case PIXEL_FORMAT_I420:
      // Note that we use J420ToARGB instead of I420ToARGB so that the
      // kYuvJPEGConstants YUV-to-RGB conversion matrix is used.
      return libyuv::J420ToARGB(src_frame->visible_data(VideoFrame::kYPlane),
                                src_frame->stride(VideoFrame::kYPlane),
                                src_frame->visible_data(VideoFrame::kUPlane),
                                src_frame->stride(VideoFrame::kUPlane),
                                src_frame->visible_data(VideoFrame::kVPlane),
                                src_frame->stride(VideoFrame::kVPlane),
                                dst_argb, dst_stride, width, height) == 0;
    case PIXEL_FORMAT_NV12:
      return libyuv::NV12ToARGB(src_frame->visible_data(VideoFrame::kYPlane),
                                src_frame->stride(VideoFrame::kYPlane),
                                src_frame->visible_data(VideoFrame::kUVPlane),
                                src_frame->stride(VideoFrame::kUVPlane),
                                dst_argb, dst_stride, width, height) == 0;
    case PIXEL_FORMAT_YV12:
      // Same as I420, but U and V planes are swapped.
      return libyuv::J420ToARGB(src_frame->visible_data(VideoFrame::kYPlane),
                                src_frame->stride(VideoFrame::kYPlane),
                                src_frame->visible_data(VideoFrame::kVPlane),
                                src_frame->stride(VideoFrame::kVPlane),
                                src_frame->visible_data(VideoFrame::kUPlane),
                                src_frame->stride(VideoFrame::kUPlane),
                                dst_argb, dst_stride, width, height) == 0;
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
    dst_frame =
        video_frame_mapper->Map(std::move(dst_frame), PROT_READ | PROT_WRITE);
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
    gfx::Size plane_size =
        VideoFrame::PlaneSize(dst_frame->format(), i, dst_frame->coded_size());
    libyuv::CopyPlane(
        src_frame->data(i), src_frame->layout().planes()[i].stride,
        dst_frame->writable_data(i), dst_frame->layout().planes()[i].stride,
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

  // Only I420, YUV420P10 and ARGB are currently supported as output formats.
  switch (dst_frame->format()) {
    case PIXEL_FORMAT_I420:
      return ConvertVideoFrameToI420(src_frame, dst_frame);
    case PIXEL_FORMAT_YUV420P10:
      return ConvertVideoFrameToYUV420P10(src_frame, dst_frame);
    case PIXEL_FORMAT_ARGB:
      return ConvertVideoFrameToARGB(src_frame, dst_frame);
    default:
      LOG(ERROR) << "Unsupported output format: " << dst_frame->format();
      return false;
  }
}

scoped_refptr<VideoFrame> ConvertVideoFrame(const VideoFrame* src_frame,
                                            VideoPixelFormat dst_pixel_format) {
  auto dst_frame = VideoFrame::CreateFrame(
      dst_pixel_format, src_frame->coded_size(), src_frame->visible_rect(),
      src_frame->natural_size(), src_frame->timestamp());
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

scoped_refptr<VideoFrame> ScaleVideoFrame(const VideoFrame* src_frame,
                                          const gfx::Size& dst_resolution) {
  if (src_frame->format() != PIXEL_FORMAT_NV12) {
    LOG(ERROR) << src_frame->format() << " is not supported";
    return nullptr;
  }
  auto scaled_frame = VideoFrame::CreateFrame(
      PIXEL_FORMAT_NV12, dst_resolution, gfx::Rect(dst_resolution),
      dst_resolution, src_frame->timestamp());
  const int fail_scaling = libyuv::NV12Scale(
      src_frame->visible_data(VideoFrame::kYPlane),
      src_frame->stride(VideoFrame::kYPlane),
      src_frame->visible_data(VideoFrame::kUVPlane),
      src_frame->stride(VideoFrame::kUVPlane),
      src_frame->visible_rect().width(), src_frame->visible_rect().height(),
      scaled_frame->GetWritableVisibleData(VideoFrame::kYPlane),
      scaled_frame->stride(VideoFrame::kYPlane),
      scaled_frame->GetWritableVisibleData(VideoFrame::kUVPlane),
      scaled_frame->stride(VideoFrame::kUVPlane), dst_resolution.width(),
      dst_resolution.height(), libyuv::FilterMode::kFilterBilinear);
  if (fail_scaling) {
    LOG(ERROR) << "Failed scaling the source frame";
    return nullptr;
  }
  return scaled_frame;
}

scoped_refptr<VideoFrame> CloneVideoFrame(
    const VideoFrame* const src_frame,
    const VideoFrameLayout& dst_layout,
    VideoFrame::StorageType dst_storage_type,
    absl::optional<gfx::BufferUsage> dst_buffer_usage) {
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
          dst_layout.format(), dst_layout.coded_size(),
          src_frame->visible_rect(), src_frame->natural_size(),
          src_frame->timestamp(), *dst_buffer_usage);
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
    dst_frame =
        CreateGpuMemoryBufferVideoFrame(dst_frame.get(), *dst_buffer_usage);
  }

  return dst_frame;
}

scoped_refptr<VideoFrame> CreateDmabufVideoFrame(
    const VideoFrame* const frame) {
#if BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)
  if (!frame || frame->storage_type() != VideoFrame::STORAGE_GPU_MEMORY_BUFFER)
    return nullptr;
  gfx::GpuMemoryBuffer* gmb = frame->GetGpuMemoryBuffer();
  gfx::GpuMemoryBufferHandle gmb_handle = gmb->CloneHandle();
  DCHECK_EQ(gmb_handle.type, gfx::GpuMemoryBufferType::NATIVE_PIXMAP);
  std::vector<ColorPlaneLayout> planes;
  std::vector<base::ScopedFD> dmabuf_fds;
  for (auto& plane : gmb_handle.native_pixmap_handle.planes) {
    planes.emplace_back(plane.stride, plane.offset, plane.size);
    dmabuf_fds.emplace_back(plane.fd.release());
  }
  return VideoFrame::WrapExternalDmabufs(
      frame->layout(), frame->visible_rect(), frame->natural_size(),
      std::move(dmabuf_fds), frame->timestamp());
#else
  return nullptr;
#endif  // BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)}
}

scoped_refptr<VideoFrame> CreateGpuMemoryBufferVideoFrame(
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

  absl::optional<gfx::BufferFormat> buffer_format =
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
      std::move(gpu_memory_buffer), dummy_mailbox, base::NullCallback(),
      frame->timestamp());
}

scoped_refptr<const VideoFrame> CreateVideoFrameFromImage(const Image& image) {
  DCHECK(image.IsLoaded());
  const auto format = image.PixelFormat();
  const auto& image_size = image.Size();
  // Loaded image data must be tight.
  DCHECK_EQ(image.DataSize(), VideoFrame::AllocationSize(format, image_size));

  // Create planes for layout. We cannot use WrapExternalData() because it
  // calls GetDefaultLayout() and it supports only a few pixel formats.
  absl::optional<VideoFrameLayout> layout =
      CreateVideoFrameLayout(format, image_size, /*alignment=*/1u);
  if (!layout) {
    LOG(ERROR) << "Failed to create VideoFrameLayout";
    return nullptr;
  }

  scoped_refptr<const VideoFrame> video_frame =
      VideoFrame::WrapExternalDataWithLayout(
          *layout, image.VisibleRect(), image.VisibleRect().size(),
          image.Data(), image.DataSize(), base::TimeDelta());
  if (!video_frame) {
    LOG(ERROR) << "Failed to create VideoFrame";
    return nullptr;
  }

  return video_frame;
}

absl::optional<VideoFrameLayout> CreateVideoFrameLayout(
    VideoPixelFormat pixel_format,
    const gfx::Size& dimension,
    const uint32_t alignment,
    std::vector<size_t>* plane_rows) {
  const size_t num_planes = VideoFrame::NumPlanes(pixel_format);

  std::vector<ColorPlaneLayout> planes(num_planes);
  size_t offset = 0;
  if (plane_rows)
    plane_rows->resize(num_planes);
  for (size_t i = 0; i < num_planes; ++i) {
    const int32_t stride =
        VideoFrame::RowBytes(i, pixel_format, dimension.width());
    const size_t rows = VideoFrame::Rows(i, pixel_format, dimension.height());
    const size_t plane_size = stride * rows;
    const size_t aligned_size =
        base::bits::AlignUp(plane_size, size_t{alignment});
    planes[i].stride = stride;
    planes[i].offset = offset;
    planes[i].size = aligned_size;
    offset += planes[i].size;
    if (plane_rows)
      (*plane_rows)[i] = rows;
  }
  return VideoFrameLayout::CreateWithPlanes(pixel_format, dimension,
                                            std::move(planes));
}

}  // namespace test
}  // namespace media
