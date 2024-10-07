// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/base/video_frame.h"

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include <algorithm>
#include <atomic>
#include <climits>
#include <numeric>
#include <string_view>
#include <utility>

#include "base/bits.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/process/memory.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/lock.h"
#include "build/build_config.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "media/base/color_plane_layout.h"
#include "media/base/format_utils.h"
#include "media/base/limits.h"
#include "media/base/media_switches.h"
#include "media/base/timestamp_constants.h"
#include "media/base/video_util.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/gpu_memory_buffer.h"
#if BUILDFLAG(IS_APPLE)
#include "ui/gfx/mac/io_surface.h"
#endif

namespace media {

namespace {

VideoFrame::ID GetNextID() {
  static std::atomic_uint64_t counter(1u);
  return VideoFrame::ID::FromUnsafeValue(
      counter.fetch_add(1u, std::memory_order_relaxed));
}

// Helper to provide gfx::Rect::Intersect() as an expression.
gfx::Rect Intersection(gfx::Rect a, const gfx::Rect& b) {
  a.Intersect(b);
  return a;
}

void ReleaseMailboxAndDropGpuMemoryBuffer(
    VideoFrame::ReleaseMailboxCB cb,
    const gpu::SyncToken& sync_token,
    std::unique_ptr<gfx::GpuMemoryBuffer> gpu_memory_buffer) {
  std::move(cb).Run(sync_token);
}

VideoFrame::ReleaseMailboxAndGpuMemoryBufferCB WrapReleaseMailboxCB(
    VideoFrame::ReleaseMailboxCB cb) {
  if (cb.is_null())
    return VideoFrame::ReleaseMailboxAndGpuMemoryBufferCB();
  return base::BindOnce(&ReleaseMailboxAndDropGpuMemoryBuffer, std::move(cb));
}

}  // namespace

// static
std::string VideoFrame::StorageTypeToString(
    const VideoFrame::StorageType storage_type) {
  switch (storage_type) {
    case VideoFrame::STORAGE_UNKNOWN:
      return "UNKNOWN";
    case VideoFrame::STORAGE_OPAQUE:
      return "OPAQUE";
    case VideoFrame::STORAGE_UNOWNED_MEMORY:
      return "UNOWNED_MEMORY";
    case VideoFrame::STORAGE_OWNED_MEMORY:
      return "OWNED_MEMORY";
    case VideoFrame::STORAGE_SHMEM:
      return "SHMEM";
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
    case VideoFrame::STORAGE_DMABUFS:
      return "DMABUFS";
#endif
    case VideoFrame::STORAGE_GPU_MEMORY_BUFFER:
      return "GPU_MEMORY_BUFFER";
  }

  NOTREACHED_IN_MIGRATION() << "Invalid StorageType provided: " << storage_type;
  return "INVALID";
}

// static
bool VideoFrame::IsStorageTypeMappable(VideoFrame::StorageType storage_type) {
  return
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
      // This is not strictly needed but makes explicit that, at VideoFrame
      // level, DmaBufs are not mappable from userspace.
      storage_type != VideoFrame::STORAGE_DMABUFS &&
#endif
      // GpuMemoryBuffer is not mappable at VideoFrame level. In most places
      // GpuMemoryBuffer is opaque to the CPU, and for places that really need
      // to access the data on CPU they can get the buffer with
      // GetGpuMemoryBuffer() and call gfx::GpuMemoryBuffer::Map().
      (storage_type == VideoFrame::STORAGE_UNOWNED_MEMORY ||
       storage_type == VideoFrame::STORAGE_OWNED_MEMORY ||
       storage_type == VideoFrame::STORAGE_SHMEM);
}

// static
bool VideoFrame::IsValidPlane(VideoPixelFormat format, size_t plane) {
  DCHECK_LE(NumPlanes(format), static_cast<size_t>(kMaxPlanes));
  return plane < NumPlanes(format);
}

// static
gfx::Size VideoFrame::SampleSize(VideoPixelFormat format, size_t plane) {
  DCHECK(IsValidPlane(format, plane));

  switch (plane) {
    case Plane::kY:  // and Plane::kARGB:
    case Plane::kA:
      return gfx::Size(1, 1);

    case Plane::kU:  // and Plane::kUV:
    case Plane::kV:  // and Plane::kATriPlanar:
      switch (format) {
        case PIXEL_FORMAT_I444:
        case PIXEL_FORMAT_YUV444P9:
        case PIXEL_FORMAT_YUV444P10:
        case PIXEL_FORMAT_YUV444P12:
        case PIXEL_FORMAT_Y16:
        case PIXEL_FORMAT_I444A:
        case PIXEL_FORMAT_YUV444AP10:
        case PIXEL_FORMAT_NV24:
        case PIXEL_FORMAT_P410LE:
          return gfx::Size(1, 1);

        case PIXEL_FORMAT_I422:
        case PIXEL_FORMAT_YUV422P9:
        case PIXEL_FORMAT_YUV422P10:
        case PIXEL_FORMAT_YUV422P12:
        case PIXEL_FORMAT_I422A:
        case PIXEL_FORMAT_YUV422AP10:
        case PIXEL_FORMAT_NV16:
        case PIXEL_FORMAT_P210LE:
          return gfx::Size(2, 1);

        case PIXEL_FORMAT_YV12:
        case PIXEL_FORMAT_I420:
        case PIXEL_FORMAT_I420A:
        case PIXEL_FORMAT_NV12:
        case PIXEL_FORMAT_NV21:
        case PIXEL_FORMAT_YUV420P9:
        case PIXEL_FORMAT_YUV420P10:
        case PIXEL_FORMAT_YUV420P12:
        case PIXEL_FORMAT_P010LE:
        case PIXEL_FORMAT_YUV420AP10:
          return gfx::Size(2, 2);

        case PIXEL_FORMAT_NV12A:
          return plane == Plane::kUV ? gfx::Size(2, 2) : gfx::Size(1, 1);

        case PIXEL_FORMAT_UYVY:
        case PIXEL_FORMAT_UNKNOWN:
        case PIXEL_FORMAT_YUY2:
        case PIXEL_FORMAT_ARGB:
        case PIXEL_FORMAT_XRGB:
        case PIXEL_FORMAT_RGB24:
        case PIXEL_FORMAT_MJPEG:
        case PIXEL_FORMAT_ABGR:
        case PIXEL_FORMAT_XBGR:
        case PIXEL_FORMAT_XR30:
        case PIXEL_FORMAT_XB30:
        case PIXEL_FORMAT_BGRA:
        case PIXEL_FORMAT_RGBAF16:
          break;
      }
  }
  NOTREACHED();
}

// Checks if |source_format| can be wrapped into a |target_format| frame.
static bool AreValidPixelFormatsForWrap(VideoPixelFormat source_format,
                                        VideoPixelFormat target_format) {
  return source_format == target_format ||
         (source_format == PIXEL_FORMAT_I420A &&
          target_format == PIXEL_FORMAT_I420) ||
         (source_format == PIXEL_FORMAT_ARGB &&
          target_format == PIXEL_FORMAT_XRGB) ||
         (source_format == PIXEL_FORMAT_ABGR &&
          target_format == PIXEL_FORMAT_XBGR);
}

// Creates VideoFrameLayout for tightly packed frame.
static std::optional<VideoFrameLayout> GetDefaultLayout(
    VideoPixelFormat format,
    const gfx::Size& coded_size) {
  std::vector<ColorPlaneLayout> planes;

  switch (format) {
    case PIXEL_FORMAT_I420: {
      int uv_width = (coded_size.width() + 1) / 2;
      int uv_height = (coded_size.height() + 1) / 2;
      int uv_stride = uv_width;
      int uv_size = uv_stride * uv_height;
      planes = std::vector<ColorPlaneLayout>{
          ColorPlaneLayout(coded_size.width(), 0, coded_size.GetArea()),
          ColorPlaneLayout(uv_stride, coded_size.GetArea(), uv_size),
          ColorPlaneLayout(uv_stride, coded_size.GetArea() + uv_size, uv_size),
      };
      break;
    }

    case PIXEL_FORMAT_Y16:
      planes = std::vector<ColorPlaneLayout>{ColorPlaneLayout(
          coded_size.width() * 2, 0, coded_size.GetArea() * 2)};
      break;

    case PIXEL_FORMAT_ARGB:
    case PIXEL_FORMAT_XRGB:
    case PIXEL_FORMAT_ABGR:
    case PIXEL_FORMAT_XBGR:
      planes = std::vector<ColorPlaneLayout>{ColorPlaneLayout(
          coded_size.width() * 4, 0, coded_size.GetArea() * 4)};
      break;

    case PIXEL_FORMAT_NV12: {
      int uv_width = (coded_size.width() + 1) / 2;
      int uv_height = (coded_size.height() + 1) / 2;
      int uv_stride = uv_width * 2;
      int uv_size = uv_stride * uv_height;
      planes = std::vector<ColorPlaneLayout>{
          ColorPlaneLayout(coded_size.width(), 0, coded_size.GetArea()),
          ColorPlaneLayout(uv_stride, coded_size.GetArea(), uv_size),
      };
      break;
    }

    case PIXEL_FORMAT_NV12A: {
      int uv_width = (coded_size.width() + 1) / 2;
      int uv_height = (coded_size.height() + 1) / 2;
      int uv_stride = uv_width * 2;
      int uv_size = uv_stride * uv_height;
      planes = std::vector<ColorPlaneLayout>{
          ColorPlaneLayout(coded_size.width(), 0, coded_size.GetArea()),
          ColorPlaneLayout(uv_stride, coded_size.GetArea(), uv_size),
          ColorPlaneLayout(coded_size.width(), 0, coded_size.GetArea()),
      };
      break;
    }

    default:
      DLOG(ERROR) << "Unsupported pixel format"
                  << VideoPixelFormatToString(format);
      return std::nullopt;
  }

  return VideoFrameLayout::CreateWithPlanes(format, coded_size, planes);
}

// static
bool VideoFrame::IsValidConfig(VideoPixelFormat format,
                               StorageType storage_type,
                               const gfx::Size& coded_size,
                               const gfx::Rect& visible_rect,
                               const gfx::Size& natural_size) {
  return IsValidConfigInternal(format, FrameControlType::kNone, coded_size,
                               visible_rect, natural_size);
}

// static
scoped_refptr<VideoFrame> VideoFrame::CreateFrame(VideoPixelFormat format,
                                                  const gfx::Size& coded_size,
                                                  const gfx::Rect& visible_rect,
                                                  const gfx::Size& natural_size,
                                                  base::TimeDelta timestamp) {
  return CreateFrameInternal(format, coded_size, visible_rect, natural_size,
                             timestamp, false);
}

// static
scoped_refptr<VideoFrame> VideoFrame::CreateVideoHoleFrame(
    const base::UnguessableToken& overlay_plane_id,
    const gfx::Size& natural_size,
    base::TimeDelta timestamp) {
  auto layout = VideoFrameLayout::Create(PIXEL_FORMAT_UNKNOWN, natural_size);
  if (!layout) {
    DLOG(ERROR) << "Invalid layout.";
    return nullptr;
  }
  scoped_refptr<VideoFrame> frame = new VideoFrame(
      *layout, StorageType::STORAGE_OPAQUE, gfx::Rect(natural_size),
      natural_size, timestamp, FrameControlType::kVideoHole);
  frame->metadata().overlay_plane_id = overlay_plane_id;
  return frame;
}

// static
scoped_refptr<VideoFrame> VideoFrame::CreateZeroInitializedFrame(
    VideoPixelFormat format,
    const gfx::Size& coded_size,
    const gfx::Rect& visible_rect,
    const gfx::Size& natural_size,
    base::TimeDelta timestamp) {
  return CreateFrameInternal(format, coded_size, visible_rect, natural_size,
                             timestamp, true);
}

// static
scoped_refptr<VideoFrame> VideoFrame::CreateFrameForNativeTexturesInternal(
    VideoPixelFormat format,
    const gfx::Size& coded_size,
    const gfx::Rect& visible_rect,
    const gfx::Size& natural_size,
    base::TimeDelta timestamp) {
  if (format != PIXEL_FORMAT_ARGB && format != PIXEL_FORMAT_XRGB &&
      format != PIXEL_FORMAT_NV12 && format != PIXEL_FORMAT_NV16 &&
      format != PIXEL_FORMAT_NV24 && format != PIXEL_FORMAT_NV12A &&
      format != PIXEL_FORMAT_I420 && format != PIXEL_FORMAT_ABGR &&
      format != PIXEL_FORMAT_XBGR && format != PIXEL_FORMAT_XR30 &&
      format != PIXEL_FORMAT_XB30 && format != PIXEL_FORMAT_P010LE &&
      format != PIXEL_FORMAT_P210LE && format != PIXEL_FORMAT_P410LE &&
      format != PIXEL_FORMAT_RGBAF16 && format != PIXEL_FORMAT_YV12 &&
      format != PIXEL_FORMAT_BGRA) {
    DLOG(ERROR) << "Unsupported pixel format: "
                << VideoPixelFormatToString(format);
    return nullptr;
  }
  const StorageType storage = STORAGE_OPAQUE;
  if (!IsValidConfig(format, storage, coded_size, visible_rect, natural_size)) {
    DLOG(ERROR) << __func__ << " Invalid config."
                << ConfigToString(format, storage, coded_size, visible_rect,
                                  natural_size);
    return nullptr;
  }

  auto layout = VideoFrameLayout::Create(format, coded_size);
  if (!layout) {
    DLOG(ERROR) << "Invalid layout.";
    return nullptr;
  }

  scoped_refptr<VideoFrame> frame =
      new VideoFrame(*layout, storage, visible_rect, natural_size, timestamp);

  return frame;
}

scoped_refptr<VideoFrame>
VideoFrame::CreateFrameForGpuMemoryBufferOrMappableSIInternal(
    const gfx::Rect& visible_rect,
    const gfx::Size& natural_size,
    std::unique_ptr<gfx::GpuMemoryBuffer> gpu_memory_buffer,
    scoped_refptr<gpu::ClientSharedImage> shared_image,
    const bool enable_mappable_si,
    ReleaseMailboxAndGpuMemoryBufferCB mailbox_holder_and_gmb_release_cb,
    base::TimeDelta timestamp) {
  if (enable_mappable_si) {
    CHECK(shared_image && !gpu_memory_buffer);
  } else {
    CHECK(gpu_memory_buffer && !shared_image);
  }

  const gfx::BufferFormat buffer_format =
      gpu_memory_buffer
          ? gpu_memory_buffer->GetFormat()
          : viz::SharedImageFormatToBufferFormatRestrictedUtils::ToBufferFormat(
                shared_image->format());
  const std::optional<VideoPixelFormat> format =
      GfxBufferFormatToVideoPixelFormat(buffer_format);
  if (!format) {
    return nullptr;
  }
  constexpr StorageType storage = STORAGE_GPU_MEMORY_BUFFER;
  const gfx::Size& coded_size =
      gpu_memory_buffer ? gpu_memory_buffer->GetSize() : shared_image->size();
  if (!IsValidConfig(*format, storage, coded_size, visible_rect,
                     natural_size)) {
    DLOG(ERROR) << __func__ << " Invalid config"
                << ConfigToString(*format, storage, coded_size, visible_rect,
                                  natural_size);
    return nullptr;
  }

  const size_t num_planes = NumberOfPlanesForLinearBufferFormat(buffer_format);
  std::vector<ColorPlaneLayout> planes(num_planes);
  for (size_t i = 0; i < num_planes; ++i) {
    planes[i].stride = gpu_memory_buffer
                           ? gpu_memory_buffer->stride(i)
                           : shared_image->GetStrideForVideoFrame(i);
  }
  uint64_t modifier = gfx::NativePixmapHandle::kNoModifier;
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  bool is_native_buffer =
      gpu_memory_buffer
          ? (gpu_memory_buffer->GetType() != gfx::SHARED_MEMORY_BUFFER)
          : !shared_image->IsSharedMemoryForVideoFrame();
  if (is_native_buffer) {
    const auto gmb_handle = gpu_memory_buffer
                                ? gpu_memory_buffer->CloneHandle()
                                : shared_image->CloneGpuMemoryBufferHandle();
    if (gmb_handle.is_null() ||
        gmb_handle.native_pixmap_handle.planes.empty()) {
      DLOG(ERROR) << "Failed to clone the GpuMemoryBufferHandle";
      return nullptr;
    }
    if (gmb_handle.native_pixmap_handle.planes.size() != num_planes) {
      DLOG(ERROR) << "Invalid number of planes="
                  << gmb_handle.native_pixmap_handle.planes.size()
                  << ", expected num_planes=" << num_planes;
      return nullptr;
    }
    for (size_t i = 0; i < num_planes; ++i) {
      const auto& plane = gmb_handle.native_pixmap_handle.planes[i];
      planes[i].stride = plane.stride;
      planes[i].offset = plane.offset;
      planes[i].size = plane.size;
    }
    modifier = gmb_handle.native_pixmap_handle.modifier;
  }
#endif

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
    DLOG(ERROR) << __func__ << " Couldn't create VideoFrame instance";
    return nullptr;
  }
  frame->gpu_memory_buffer_ = std::move(gpu_memory_buffer);
  frame->mailbox_holder_and_gmb_release_cb_ =
      std::move(mailbox_holder_and_gmb_release_cb);
  frame->is_mappable_si_enabled_ = enable_mappable_si;
  return frame;
}

// static
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
scoped_refptr<VideoFrame> VideoFrame::WrapOOPVDMailbox(
    VideoPixelFormat format,
    const gpu::Mailbox& mailbox,
    ReleaseMailboxCB mailbox_holder_release_cb,
    const gfx::Size& coded_size,
    const gfx::Rect& visible_rect,
    const gfx::Size& natural_size,
    base::TimeDelta timestamp) {
  scoped_refptr<VideoFrame> frame = CreateFrameForNativeTexturesInternal(
      format, coded_size, visible_rect, natural_size, timestamp);
  if (!frame) {
    return nullptr;
  }

  frame->oopvd_mailbox_ = mailbox;
  frame->mailbox_holder_and_gmb_release_cb_ =
      WrapReleaseMailboxCB(std::move(mailbox_holder_release_cb));

  // Wrapping native textures should... have textures. https://crbug.com/864145.
  DCHECK(frame->HasOOPVDMailbox());

  return frame;
}
#endif  // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)

// static
scoped_refptr<VideoFrame> VideoFrame::WrapSharedImage(
    VideoPixelFormat format,
    scoped_refptr<gpu::ClientSharedImage> shared_image,
    gpu::SyncToken sync_token,
    ReleaseMailboxCB mailbox_holder_release_cb,
    const gfx::Size& coded_size,
    const gfx::Rect& visible_rect,
    const gfx::Size& natural_size,
    base::TimeDelta timestamp) {
  scoped_refptr<VideoFrame> frame = CreateFrameForNativeTexturesInternal(
      format, coded_size, visible_rect, natural_size, timestamp);
  if (!frame) {
    return nullptr;
  }

  if (shared_image) {
    frame->acquire_sync_token_ = sync_token;
    frame->shared_image_ = shared_image->MakeUnowned();
  }
  frame->mailbox_holder_and_gmb_release_cb_ =
      WrapReleaseMailboxCB(std::move(mailbox_holder_release_cb));

  DCHECK(frame->HasSharedImage());

  return frame;
}

scoped_refptr<VideoFrame> VideoFrame::WrapMappableSharedImage(
    scoped_refptr<gpu::ClientSharedImage> shared_image,
    gpu::SyncToken sync_token,
    ReleaseMailboxAndGpuMemoryBufferCB mailbox_holder_and_gmb_release_cb,
    const gfx::Rect& visible_rect,
    const gfx::Size& natural_size,
    base::TimeDelta timestamp) {
  CHECK(shared_image);
  scoped_refptr<VideoFrame> frame =
      CreateFrameForGpuMemoryBufferOrMappableSIInternal(
          visible_rect, natural_size, /*gpu_memory_buffer=*/nullptr,
          shared_image,
          /*enable_mappable_si=*/true,
          std::move(mailbox_holder_and_gmb_release_cb), timestamp);
  if (!frame) {
    return nullptr;
  }
  frame->acquire_sync_token_ = sync_token;

  // Note that we can not use |shared_image|->MakeUnOwned() here since that
  // will not work for MappableSI due to it owning a GMB internally and we can
  // not create an unowned reference to it. Additionally
  // removing the use of ClientSharedImage::MakeUnOwned() everywhere is
  // currently work in progress as a part of Automatic shared image management
  // for ClientSharedImage project, so we don't want to use it here as well. The
  // downside right now with below code is that while destroying the
  // ClientSharedImage when MappableSI is enabled, there will be more than one
  // reference of it and we will hit CHECKs in
  // ClientSharedImageInterface::DestroySharedImage(). To avoid this CHECKs, we
  // will need to replace the ClientSharedImageInterface::DestroySharedImage()
  // call sites with ClientSharedImage::UpdateDestructionSyncToken() for every
  // VideoFrame MappableSI client. This works well since it is also eventual
  // goal of ClientSharedImage for rest of the chrome. crbug.com/40286368 for
  // more details on the work.
  frame->shared_image_ = std::move(shared_image);
  return frame;
}

// static
scoped_refptr<VideoFrame> VideoFrame::WrapExternalData(
    VideoPixelFormat format,
    const gfx::Size& coded_size,
    const gfx::Rect& visible_rect,
    const gfx::Size& natural_size,
    const uint8_t* data,
    size_t data_size,
    base::TimeDelta timestamp) {
  auto layout = GetDefaultLayout(format, coded_size);
  if (!layout)
    return nullptr;
  return WrapExternalDataWithLayout(*layout, visible_rect, natural_size, data,
                                    data_size, timestamp);
}

// static
scoped_refptr<VideoFrame> VideoFrame::WrapExternalDataWithLayout(
    const VideoFrameLayout& layout,
    const gfx::Rect& visible_rect,
    const gfx::Size& natural_size,
    const uint8_t* data,
    size_t data_size,
    base::TimeDelta timestamp) {
  StorageType storage_type = STORAGE_UNOWNED_MEMORY;

  if (!IsValidConfig(layout.format(), storage_type, layout.coded_size(),
                     visible_rect, natural_size) ||
      !layout.FitsInContiguousBufferOfSize(data_size)) {
    DLOG(ERROR) << "Invalid config: "
                << ConfigToString(layout.format(), storage_type,
                                  layout.coded_size(), visible_rect,
                                  natural_size);
    return nullptr;
  }

  scoped_refptr<VideoFrame> frame = new VideoFrame(
      layout, storage_type, visible_rect, natural_size, timestamp);

  for (size_t i = 0; i < layout.planes().size(); ++i) {
    frame->data_[i] = data + layout.planes()[i].offset;
  }

  return frame;
}

// static
scoped_refptr<VideoFrame> VideoFrame::WrapExternalYuvData(
    VideoPixelFormat format,
    const gfx::Size& coded_size,
    const gfx::Rect& visible_rect,
    const gfx::Size& natural_size,
    int32_t y_stride,
    int32_t u_stride,
    int32_t v_stride,
    const uint8_t* y_data,
    const uint8_t* u_data,
    const uint8_t* v_data,
    base::TimeDelta timestamp) {
  auto layout = VideoFrameLayout::CreateWithStrides(
      format, coded_size, {y_stride, u_stride, v_stride});
  if (!layout) {
    DLOG(ERROR) << "Invalid layout.";
    return nullptr;
  }

  return WrapExternalYuvDataWithLayout(*layout, visible_rect, natural_size,
                                       y_data, u_data, v_data, timestamp);
}

// static
scoped_refptr<VideoFrame> VideoFrame::WrapExternalYuvDataWithLayout(
    const VideoFrameLayout& layout,
    const gfx::Rect& visible_rect,
    const gfx::Size& natural_size,
    const uint8_t* y_data,
    const uint8_t* u_data,
    const uint8_t* v_data,
    base::TimeDelta timestamp) {
  const StorageType storage = STORAGE_UNOWNED_MEMORY;
  const VideoPixelFormat format = layout.format();
  if (!IsValidConfig(format, storage, layout.coded_size(), visible_rect,
                     natural_size)) {
    DLOG(ERROR) << __func__ << " Invalid config."
                << ConfigToString(format, storage, layout.coded_size(),
                                  visible_rect, natural_size);
    return nullptr;
  }
  if (!IsYuvPlanar(format)) {
    DLOG(ERROR) << __func__ << " Format is not YUV. " << format;
    return nullptr;
  }

  DCHECK_LE(NumPlanes(format), 3u);
  scoped_refptr<VideoFrame> frame(
      new VideoFrame(layout, storage, visible_rect, natural_size, timestamp));
  frame->data_[Plane::kY] = y_data;
  frame->data_[Plane::kU] = u_data;
  frame->data_[Plane::kV] = v_data;
  return frame;
}

// static
scoped_refptr<VideoFrame> VideoFrame::WrapExternalYuvaData(
    VideoPixelFormat format,
    const gfx::Size& coded_size,
    const gfx::Rect& visible_rect,
    const gfx::Size& natural_size,
    int32_t y_stride,
    int32_t u_stride,
    int32_t v_stride,
    int32_t a_stride,
    const uint8_t* y_data,
    const uint8_t* u_data,
    const uint8_t* v_data,
    const uint8_t* a_data,
    base::TimeDelta timestamp) {
  const StorageType storage = STORAGE_UNOWNED_MEMORY;
  if (!IsValidConfig(format, storage, coded_size, visible_rect, natural_size)) {
    DLOG(ERROR) << __func__ << " Invalid config."
                << ConfigToString(format, storage, coded_size, visible_rect,
                                  natural_size);
    return nullptr;
  }

  if (NumPlanes(format) != 4) {
    DLOG(ERROR) << "Expecting Y, U, V and A planes to be present for the video"
                << " format.";
    return nullptr;
  }

  auto layout = VideoFrameLayout::CreateWithStrides(
      format, coded_size, {y_stride, u_stride, v_stride, a_stride});
  if (!layout) {
    DLOG(ERROR) << "Invalid layout";
    return nullptr;
  }

  scoped_refptr<VideoFrame> frame(
      new VideoFrame(*layout, storage, visible_rect, natural_size, timestamp));
  frame->data_[Plane::kY] = y_data;
  frame->data_[Plane::kU] = u_data;
  frame->data_[Plane::kV] = v_data;
  frame->data_[Plane::kA] = a_data;
  return frame;
}

// static
scoped_refptr<VideoFrame> VideoFrame::WrapExternalYuvData(
    VideoPixelFormat format,
    const gfx::Size& coded_size,
    const gfx::Rect& visible_rect,
    const gfx::Size& natural_size,
    int32_t y_stride,
    int32_t uv_stride,
    const uint8_t* y_data,
    const uint8_t* uv_data,
    base::TimeDelta timestamp) {
  const StorageType storage = STORAGE_UNOWNED_MEMORY;
  if (!IsValidConfig(format, storage, coded_size, visible_rect, natural_size)) {
    DLOG(ERROR) << __func__ << " Invalid config."
                << ConfigToString(format, storage, coded_size, visible_rect,
                                  natural_size);
    return nullptr;
  }

  if (NumPlanes(format) != 2) {
    DLOG(ERROR) << "Expecting Y, UV planes to be present for the video format.";
    return nullptr;
  }

  auto layout = VideoFrameLayout::CreateWithStrides(format, coded_size,
                                                    {y_stride, uv_stride});
  if (!layout) {
    DLOG(ERROR) << "Invalid layout";
    return nullptr;
  }

  scoped_refptr<VideoFrame> frame(
      new VideoFrame(*layout, storage, visible_rect, natural_size, timestamp));
  frame->data_[Plane::kY] = y_data;
  frame->data_[Plane::kUV] = uv_data;

  return frame;
}

// static
scoped_refptr<VideoFrame> VideoFrame::WrapExternalGpuMemoryBuffer(
    const gfx::Rect& visible_rect,
    const gfx::Size& natural_size,
    std::unique_ptr<gfx::GpuMemoryBuffer> gpu_memory_buffer,
    base::TimeDelta timestamp) {
  return CreateFrameForGpuMemoryBufferOrMappableSIInternal(
      visible_rect, natural_size, std::move(gpu_memory_buffer),
      /*shared_image=*/nullptr,
      /*enable_mappable_si=*/false, base::NullCallback(), timestamp);
}

// static
scoped_refptr<VideoFrame> VideoFrame::WrapExternalGpuMemoryBuffer(
    const gfx::Rect& visible_rect,
    const gfx::Size& natural_size,
    std::unique_ptr<gfx::GpuMemoryBuffer> gpu_memory_buffer,
    scoped_refptr<gpu::ClientSharedImage> shared_image,
    const gpu::SyncToken& sync_token,
    ReleaseMailboxAndGpuMemoryBufferCB mailbox_holder_and_gmb_release_cb,
    base::TimeDelta timestamp) {
  scoped_refptr<VideoFrame> frame =
      CreateFrameForGpuMemoryBufferOrMappableSIInternal(
          visible_rect, natural_size, std::move(gpu_memory_buffer),
          /*shared_image=*/nullptr,
          /*enable_mappable_si=*/false,
          std::move(mailbox_holder_and_gmb_release_cb), timestamp);
  if (!frame) {
    return nullptr;
  }

  if (shared_image) {
    frame->acquire_sync_token_ = sync_token;
    frame->shared_image_ = shared_image->MakeUnowned();
  }
  return frame;
}

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
// static
scoped_refptr<VideoFrame> VideoFrame::WrapExternalDmabufs(
    const VideoFrameLayout& layout,
    const gfx::Rect& visible_rect,
    const gfx::Size& natural_size,
    std::vector<base::ScopedFD> dmabuf_fds,
    base::TimeDelta timestamp) {
  const StorageType storage = STORAGE_DMABUFS;
  const VideoPixelFormat format = layout.format();
  const gfx::Size& coded_size = layout.coded_size();
  if (!IsValidConfig(format, storage, coded_size, visible_rect, natural_size)) {
    DLOG(ERROR) << __func__ << " Invalid config."
                << ConfigToString(format, storage, coded_size, visible_rect,
                                  natural_size);
    return nullptr;
  }

  if (dmabuf_fds.empty() || dmabuf_fds.size() > NumPlanes(format)) {
    DLOG(ERROR) << __func__ << " Incorrect number of dmabuf fds provided, got: "
                << dmabuf_fds.size() << ", expected 1 to " << NumPlanes(format);
    return nullptr;
  }

  scoped_refptr<VideoFrame> frame =
      new VideoFrame(layout, storage, visible_rect, natural_size, timestamp);
  if (!frame) {
    DLOG(ERROR) << __func__ << " Couldn't create VideoFrame instance.";
    return nullptr;
  }

  frame->mailbox_holder_and_gmb_release_cb_ =
      ReleaseMailboxAndGpuMemoryBufferCB();
  frame->dmabuf_fds_ = std::move(dmabuf_fds);
  DCHECK(frame->HasDmaBufs());

  return frame;
}
#endif

#if BUILDFLAG(IS_APPLE)
// static
scoped_refptr<VideoFrame> VideoFrame::WrapUnacceleratedIOSurface(
    gfx::GpuMemoryBufferHandle handle,
    const gfx::Rect& visible_rect,
    base::TimeDelta timestamp) {
  if (handle.type != gfx::GpuMemoryBufferType::IO_SURFACE_BUFFER) {
    DLOG(ERROR) << "Non-IOSurface handle.";
    return nullptr;
  }
  gfx::ScopedIOSurface io_surface = handle.io_surface;
  if (!io_surface) {
    return nullptr;
  }

  // Only support NV12 IOSurfaces.
  const OSType cv_pixel_format = IOSurfaceGetPixelFormat(io_surface.get());
  if (cv_pixel_format != kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange) {
    DLOG(ERROR) << "Invalid (non-NV12) pixel format.";
    return nullptr;
  }
  const VideoPixelFormat pixel_format = PIXEL_FORMAT_NV12;

  // Retrieve the layout parameters for |io_surface_|.
  const size_t num_planes = IOSurfaceGetPlaneCount(io_surface.get());
  const gfx::Size size(IOSurfaceGetWidth(io_surface.get()),
                       IOSurfaceGetHeight(io_surface.get()));
  std::vector<int32_t> strides;
  for (size_t i = 0; i < num_planes; ++i)
    strides.push_back(IOSurfaceGetBytesPerRowOfPlane(io_surface.get(), i));
  std::optional<VideoFrameLayout> layout =
      media::VideoFrameLayout::CreateWithStrides(pixel_format, size, strides);
  if (!layout) {
    DLOG(ERROR) << "Invalid layout.";
    return nullptr;
  }

  const StorageType storage_type = STORAGE_UNOWNED_MEMORY;
  if (!IsValidConfig(pixel_format, storage_type, size, visible_rect, size)) {
    DLOG(ERROR) << "Invalid config.";
    return nullptr;
  }

  // Lock the IOSurface for CPU read access. After the VideoFrame is created,
  // add a destruction callback to unlock the IOSurface.
  kern_return_t lock_result =
      IOSurfaceLock(io_surface.get(), kIOSurfaceLockReadOnly, nullptr);
  if (lock_result != kIOReturnSuccess) {
    DLOG(ERROR) << "Failed to lock IOSurface.";
    return nullptr;
  }
  auto unlock_lambda =
      [](base::apple::ScopedCFTypeRef<IOSurfaceRef> io_surface) {
        IOSurfaceUnlock(io_surface.get(), kIOSurfaceLockReadOnly, nullptr);
      };

  scoped_refptr<VideoFrame> frame =
      new VideoFrame(*layout, storage_type, visible_rect, size, timestamp);
  for (size_t i = 0; i < num_planes; ++i) {
    frame->data_[i] = reinterpret_cast<uint8_t*>(
        IOSurfaceGetBaseAddressOfPlane(io_surface.get(), i));
  }
  frame->AddDestructionObserver(
      base::BindOnce(unlock_lambda, std::move(io_surface)));
  return frame;
}

// static
scoped_refptr<VideoFrame> VideoFrame::WrapCVPixelBuffer(
    CVPixelBufferRef cv_pixel_buffer,
    base::TimeDelta timestamp) {
  DCHECK(cv_pixel_buffer);
  DCHECK(CFGetTypeID(cv_pixel_buffer) == CVPixelBufferGetTypeID());

  const OSType cv_format = CVPixelBufferGetPixelFormatType(cv_pixel_buffer);
  VideoPixelFormat format;
  // There are very few compatible CV pixel formats, so just check each.
  if (cv_format == kCVPixelFormatType_420YpCbCr8Planar) {
    format = PIXEL_FORMAT_I420;
  } else if (cv_format == kCVPixelFormatType_444YpCbCr8) {
    format = PIXEL_FORMAT_I444;
  } else if (cv_format == kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange) {
    format = PIXEL_FORMAT_NV12;
  } else if (cv_format ==
             kCVPixelFormatType_420YpCbCr8VideoRange_8A_TriPlanar) {
    format = PIXEL_FORMAT_NV12A;
  } else {
    DLOG(ERROR) << "CVPixelBuffer format not supported: " << cv_format;
    return nullptr;
  }

  const gfx::Size coded_size(CVImageBufferGetEncodedSize(cv_pixel_buffer));
  const gfx::Rect visible_rect(CVImageBufferGetCleanRect(cv_pixel_buffer));
  const gfx::Size natural_size(CVImageBufferGetDisplaySize(cv_pixel_buffer));
  const StorageType storage = STORAGE_UNOWNED_MEMORY;

  if (!IsValidConfig(format, storage, coded_size, visible_rect, natural_size)) {
    DLOG(ERROR) << __func__ << " Invalid config."
                << ConfigToString(format, storage, coded_size, visible_rect,
                                  natural_size);
    return nullptr;
  }

  auto layout = VideoFrameLayout::Create(format, coded_size);
  if (!layout) {
    DLOG(ERROR) << "Invalid layout.";
    return nullptr;
  }

  scoped_refptr<VideoFrame> frame(
      new VideoFrame(*layout, storage, visible_rect, natural_size, timestamp));

  frame->cv_pixel_buffer_.reset(cv_pixel_buffer, base::scoped_policy::RETAIN);
  return frame;
}
#endif

// static
scoped_refptr<VideoFrame> VideoFrame::WrapVideoFrame(
    scoped_refptr<VideoFrame> frame,
    VideoPixelFormat format,
    const gfx::Rect& visible_rect,
    const gfx::Size& natural_size) {
  if (!AreValidPixelFormatsForWrap(frame->format(), format)) {
    DLOG(ERROR) << __func__ << " Invalid format conversion."
                << VideoPixelFormatToString(frame->format()) << " to "
                << VideoPixelFormatToString(format);
    return nullptr;
  }

  if (!IsValidConfig(format, frame->storage_type(), frame->coded_size(),
                     visible_rect, natural_size)) {
    DLOG(ERROR) << __func__ << " Invalid config."
                << ConfigToString(format, frame->storage_type(),
                                  frame->coded_size(), visible_rect,
                                  natural_size);
    return nullptr;
  }

  size_t new_plane_count = NumPlanes(format);
  std::optional<VideoFrameLayout> new_layout;
  if (format == frame->format()) {
    new_layout = frame->layout();
  } else {
    std::vector<ColorPlaneLayout> new_planes = frame->layout().planes();
    if (new_plane_count > new_planes.size()) {
      DLOG(ERROR) << " Wrapping frame has more planes than old one."
                  << " old plane count: " << new_planes.size()
                  << " new plane count: " << new_plane_count;
      return nullptr;
    }
    new_planes.resize(new_plane_count);
    new_layout = VideoFrameLayout::CreateWithPlanes(format, frame->coded_size(),
                                                    new_planes);
  }

  if (!new_layout.has_value()) {
    DLOG(ERROR) << " Can't create layout for the wrapping frame";
    return nullptr;
  }

  scoped_refptr<VideoFrame> wrapping_frame(
      new VideoFrame(new_layout.value(), frame->storage_type(), visible_rect,
                     natural_size, frame->timestamp()));

  // Copy all metadata to the wrapped frame->
  wrapping_frame->metadata().MergeMetadataFrom(frame->metadata());
  wrapping_frame->set_color_space(frame->ColorSpace());
  wrapping_frame->set_hdr_metadata(frame->hdr_metadata());

  if (frame->IsMappable()) {
    for (size_t i = 0; i < new_plane_count; ++i) {
      wrapping_frame->data_[i] = frame->data_[i];
    }
  }

  if (frame->storage_type() == STORAGE_SHMEM) {
    DCHECK(frame->shm_region_ && frame->shm_region_->IsValid());
    wrapping_frame->BackWithSharedMemory(frame->shm_region_);
  }

  // Don't let a Matryoshka doll of frames occur. Do this here instead of above
  // since |frame| may have different metadata than |frame->wrapped_frame_|.
  //
  // We must still keep |frame| alive though since it may have destruction
  // observers which signal that the underlying resource is okay to reuse. E.g.,
  // VideoFramePool. That's why we put it into |intermediate_wrapped_frame_|.
  if (frame->wrapped_frame_) {
    wrapping_frame->intermediate_wrapped_frame_ = frame;
    frame = frame->wrapped_frame_;
  }

  wrapping_frame->wrapped_frame_ = std::move(frame);
  return wrapping_frame;
}

// static
scoped_refptr<VideoFrame> VideoFrame::CreateEOSFrame() {
  auto layout = VideoFrameLayout::Create(PIXEL_FORMAT_UNKNOWN, gfx::Size());
  if (!layout) {
    DLOG(ERROR) << "Invalid layout.";
    return nullptr;
  }
  scoped_refptr<VideoFrame> frame =
      new VideoFrame(*layout, STORAGE_UNKNOWN, gfx::Rect(), gfx::Size(),
                     kNoTimestamp, FrameControlType::kEos);
  frame->metadata().end_of_stream = true;
  return frame;
}

// static
scoped_refptr<VideoFrame> VideoFrame::CreateColorFrame(
    const gfx::Size& size,
    uint8_t y,
    uint8_t u,
    uint8_t v,
    base::TimeDelta timestamp) {
  scoped_refptr<VideoFrame> frame =
      CreateFrame(PIXEL_FORMAT_I420, size, gfx::Rect(size), size, timestamp);
  if (frame)
    FillYUV(frame.get(), y, u, v);
  return frame;
}

// static
scoped_refptr<VideoFrame> VideoFrame::CreateBlackFrame(const gfx::Size& size) {
  const uint8_t kBlackY = 0x00;
  const uint8_t kBlackUV = 0x80;
  const base::TimeDelta kZero;
  return CreateColorFrame(size, kBlackY, kBlackUV, kBlackUV, kZero);
}

// static
scoped_refptr<VideoFrame> VideoFrame::CreateTransparentFrame(
    const gfx::Size& size) {
  const uint8_t kBlackY = 0x00;
  const uint8_t kBlackUV = 0x00;
  const uint8_t kTransparentA = 0x00;
  const base::TimeDelta kZero;
  scoped_refptr<VideoFrame> frame =
      CreateFrame(PIXEL_FORMAT_I420A, size, gfx::Rect(size), size, kZero);
  if (frame)
    FillYUVA(frame.get(), kBlackY, kBlackUV, kBlackUV, kTransparentA);
  return frame;
}

// static
size_t VideoFrame::NumPlanes(VideoPixelFormat format) {
  return VideoFrameLayout::NumPlanes(format);
}

// static
size_t VideoFrame::AllocationSize(VideoPixelFormat format,
                                  const gfx::Size& coded_size) {
  size_t total = 0;
  for (size_t i = 0; i < NumPlanes(format); ++i)
    total += PlaneSize(format, i, coded_size).GetArea();
  return total;
}

// static
gfx::Size VideoFrame::PlaneSize(VideoPixelFormat format,
                                size_t plane,
                                const gfx::Size& coded_size) {
  gfx::Size size = PlaneSizeInSamples(format, plane, coded_size);
  size.set_width(size.width() * BytesPerElement(format, plane));
  return size;
}

// static
gfx::Size VideoFrame::PlaneSizeInSamples(VideoPixelFormat format,
                                         size_t plane,
                                         const gfx::Size& coded_size) {
  DCHECK(IsValidPlane(format, plane));
  const gfx::Size subsample_size = SampleSize(format, plane);
  return gfx::Size(base::bits::AlignUpDeprecatedDoNotUse(
                       coded_size.width(), subsample_size.width()) /
                       subsample_size.width(),
                   base::bits::AlignUpDeprecatedDoNotUse(
                       coded_size.height(), subsample_size.height()) /
                       subsample_size.height());
}

// static
int VideoFrame::PlaneHorizontalBitsPerPixel(VideoPixelFormat format,
                                            size_t plane) {
  DCHECK(IsValidPlane(format, plane));
  const int bits_per_element = 8 * BytesPerElement(format, plane);
  const int horiz_pixels_per_element = SampleSize(format, plane).width();
  DCHECK_EQ(bits_per_element % horiz_pixels_per_element, 0);
  return bits_per_element / horiz_pixels_per_element;
}

// static
int VideoFrame::PlaneBitsPerPixel(VideoPixelFormat format, size_t plane) {
  DCHECK(IsValidPlane(format, plane));
  return PlaneHorizontalBitsPerPixel(format, plane) /
         SampleSize(format, plane).height();
}

// static
size_t VideoFrame::RowBytes(size_t plane, VideoPixelFormat format, int width) {
  DCHECK(IsValidPlane(format, plane));
  return BytesPerElement(format, plane) * Columns(plane, format, width);
}

// static
int VideoFrame::BytesPerElement(VideoPixelFormat format, size_t plane) {
  DCHECK(IsValidPlane(format, plane));
  switch (format) {
    case PIXEL_FORMAT_RGBAF16:
      return 8;
    case PIXEL_FORMAT_ARGB:
    case PIXEL_FORMAT_BGRA:
    case PIXEL_FORMAT_XRGB:
    case PIXEL_FORMAT_ABGR:
    case PIXEL_FORMAT_XBGR:
    case PIXEL_FORMAT_XR30:
    case PIXEL_FORMAT_XB30:
      return 4;
    case PIXEL_FORMAT_RGB24:
      return 3;
    case PIXEL_FORMAT_Y16:
    case PIXEL_FORMAT_UYVY:
    case PIXEL_FORMAT_YUY2:
    case PIXEL_FORMAT_YUV420P9:
    case PIXEL_FORMAT_YUV422P9:
    case PIXEL_FORMAT_YUV444P9:
    case PIXEL_FORMAT_YUV420P10:
    case PIXEL_FORMAT_YUV422P10:
    case PIXEL_FORMAT_YUV444P10:
    case PIXEL_FORMAT_YUV420P12:
    case PIXEL_FORMAT_YUV422P12:
    case PIXEL_FORMAT_YUV444P12:
    case PIXEL_FORMAT_YUV420AP10:
    case PIXEL_FORMAT_YUV422AP10:
    case PIXEL_FORMAT_YUV444AP10:
      return 2;
    case PIXEL_FORMAT_NV12:
    case PIXEL_FORMAT_NV16:
    case PIXEL_FORMAT_NV21:
    case PIXEL_FORMAT_NV24: {
      static const int bytes_per_element[] = {1, 2};
      DCHECK_LT(plane, std::size(bytes_per_element));
      return bytes_per_element[plane];
    }
    case PIXEL_FORMAT_NV12A: {
      static const int bytes_per_element[] = {1, 2, 1};
      DCHECK_LT(plane, std::size(bytes_per_element));
      return bytes_per_element[plane];
    }
    case PIXEL_FORMAT_P010LE:
    case PIXEL_FORMAT_P210LE:
    case PIXEL_FORMAT_P410LE: {
      static const int bytes_per_element[] = {1, 2};
      DCHECK_LT(plane, std::size(bytes_per_element));
      return bytes_per_element[plane] * 2;
    }
    case PIXEL_FORMAT_YV12:
    case PIXEL_FORMAT_I420:
    case PIXEL_FORMAT_I422:
    case PIXEL_FORMAT_I420A:
    case PIXEL_FORMAT_I444:
    case PIXEL_FORMAT_I422A:
    case PIXEL_FORMAT_I444A:
      return 1;
    case PIXEL_FORMAT_MJPEG:
      return 0;
    case PIXEL_FORMAT_UNKNOWN:
      break;
  }
  NOTREACHED();
}

// static
std::vector<int32_t> VideoFrame::ComputeStrides(VideoPixelFormat format,
                                                const gfx::Size& coded_size) {
  std::vector<int32_t> strides;
  const size_t num_planes = NumPlanes(format);
  if (num_planes == 1) {
    strides.push_back(RowBytes(0, format, coded_size.width()));
  } else {
    for (size_t plane = 0; plane < num_planes; ++plane) {
      strides.push_back(base::bits::AlignUp(
          RowBytes(plane, format, coded_size.width()), kFrameAddressAlignment));
    }
  }
  return strides;
}

// static
size_t VideoFrame::Rows(size_t plane, VideoPixelFormat format, int height) {
  DCHECK(IsValidPlane(format, plane));
  const int sample_height = SampleSize(format, plane).height();
  return base::bits::AlignUpDeprecatedDoNotUse(height, sample_height) /
         sample_height;
}

// static
size_t VideoFrame::Columns(size_t plane, VideoPixelFormat format, int width) {
  DCHECK(IsValidPlane(format, plane));
  const int sample_width = SampleSize(format, plane).width();
  return base::bits::AlignUpDeprecatedDoNotUse(width, sample_width) /
         sample_width;
}

// static
void VideoFrame::HashFrameForTesting(base::MD5Context* context,
                                     const VideoFrame& frame) {
  DCHECK(context);
  for (size_t plane = 0; plane < NumPlanes(frame.format()); ++plane) {
    for (int row = 0; row < frame.rows(plane); ++row) {
      base::MD5Update(context,
                      base::span<const uint8_t>(
                          frame.data(plane) + frame.stride(plane) * row,
                          static_cast<size_t>(frame.row_bytes(plane))));
    }
  }
}

void VideoFrame::BackWithSharedMemory(
    const base::ReadOnlySharedMemoryRegion* region) {
  DCHECK(!shm_region_);
  DCHECK(!owned_shm_region_.IsValid());
  // Either we should be backing a frame created with WrapExternal*, or we are
  // wrapping an existing STORAGE_SHMEM, in which case the storage type has
  // already been set to STORAGE_SHMEM.
  DCHECK(storage_type_ == STORAGE_UNOWNED_MEMORY ||
         storage_type_ == STORAGE_SHMEM);
  DCHECK(region && region->IsValid());
  storage_type_ = STORAGE_SHMEM;
  shm_region_ = region;
}

void VideoFrame::BackWithOwnedSharedMemory(
    base::ReadOnlySharedMemoryRegion region,
    base::ReadOnlySharedMemoryMapping mapping) {
  DCHECK(!shm_region_);
  DCHECK(!owned_shm_region_.IsValid());
  // We should be backing a frame created with WrapExternal*. We cannot be
  // wrapping an existing STORAGE_SHMEM, as the region is unowned in that case.
  DCHECK(storage_type_ == STORAGE_UNOWNED_MEMORY);
  storage_type_ = STORAGE_SHMEM;
  owned_shm_region_ = std::move(region);
  shm_region_ = &owned_shm_region_;
  owned_shm_mapping_ = std::move(mapping);
}

bool VideoFrame::IsMappable() const {
  return IsStorageTypeMappable(storage_type_);
}

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
bool VideoFrame::HasOOPVDMailbox() const {
  return wrapped_frame_ ? wrapped_frame_->HasOOPVDMailbox()
                        : !oopvd_mailbox_.IsZero();
}
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

bool VideoFrame::HasSharedImage() const {
  return wrapped_frame_ ? wrapped_frame_->HasSharedImage()
                        : shared_image_ != nullptr;
}

bool VideoFrame::HasMappableGpuBuffer() const {
  return storage_type_ == STORAGE_GPU_MEMORY_BUFFER;
}

bool VideoFrame::HasNativeGpuMemoryBuffer() const {
  if (wrapped_frame_) {
    return wrapped_frame_->HasNativeGpuMemoryBuffer();
  } else if (is_mappable_si_enabled_) {
    CHECK(shared_image_);
    return !shared_image_->IsSharedMemoryForVideoFrame();
  } else if (gpu_memory_buffer_) {
    return gpu_memory_buffer_->GetType() != gfx::SHARED_MEMORY_BUFFER;
  }
  return false;
}

gfx::GpuMemoryBuffer* VideoFrame::GetGpuMemoryBufferForTesting() const {
  return GetGpuMemoryBuffer();
}

gfx::GpuMemoryBuffer* VideoFrame::GetGpuMemoryBuffer() const {
  return wrapped_frame_ ? wrapped_frame_->GetGpuMemoryBuffer()
                        : gpu_memory_buffer_.get();
}

std::unique_ptr<VideoFrame::ScopedMapping> VideoFrame::MapGMBOrSharedImage()
    const {
  if (wrapped_frame_) {
    return wrapped_frame_->MapGMBOrSharedImage();
  }
  if (is_mappable_si_enabled_) {
    // If MappableSI is used, there must be a shared image.
    CHECK(HasSharedImage());
    if (auto mapping = shared_image_->Map()) {
      return base::WrapUnique(
          new VideoFrame::ScopedMapping(nullptr, std::move(mapping)));
    }
  }
  if (gpu_memory_buffer_ && gpu_memory_buffer_->Map()) {
    return base::WrapUnique(
        new VideoFrame::ScopedMapping(gpu_memory_buffer_.get(), nullptr));
  }
  return nullptr;
}

void VideoFrame::MapGMBOrSharedImageAsync(
    base::OnceCallback<void(std::unique_ptr<VideoFrame::ScopedMapping>)>
        result_cb) const {
  if (wrapped_frame_) {
    wrapped_frame_->MapGMBOrSharedImageAsync(std::move(result_cb));
    return;
  }
  if (is_mappable_si_enabled_) {
    CHECK(HasSharedImage());
    // `base::Unretained()` is safe because of the requirement for callers to
    // keep the VideoFrame alive until the callback executes.
    shared_image_->MapAsync(
        base::BindOnce(&VideoFrame::WrapScopedSharedImageMapping,
                       base::Unretained(this), std::move(result_cb)));
    return;
  }
  if (gpu_memory_buffer_) {
    // `base::Unretained()` is safe because of the requirement for callers to
    // keep the VideoFrame alive until the callback executes.
    gpu_memory_buffer_->MapAsync(
        base::BindOnce(&VideoFrame::MakeScopedMappingForGpuMemoryBuffer,
                       base::Unretained(this), std::move(result_cb)));
    return;
  }
  std::move(result_cb).Run(nullptr);
}

bool VideoFrame::AsyncMappingIsNonBlocking() const {
  if (wrapped_frame_) {
    return wrapped_frame_->AsyncMappingIsNonBlocking();
  }
  CHECK(HasMappableGpuBuffer());
  if (is_mappable_si_enabled_) {
    CHECK(shared_image_);
    return shared_image_->AsyncMappingIsNonBlocking();
  }
  return gpu_memory_buffer_->AsyncMappingIsNonBlocking();
}

gfx::GpuMemoryBufferHandle VideoFrame::GetGpuMemoryBufferHandle() const {
  if (wrapped_frame_) {
    return wrapped_frame_->GetGpuMemoryBufferHandle();
  }
  if (is_mappable_si_enabled_) {
    // If MappableSI is used, there must be a shared image.
    CHECK(HasSharedImage());
    return shared_image_->CloneGpuMemoryBufferHandle();
  }
  if (gpu_memory_buffer_) {
    return gpu_memory_buffer_->CloneHandle();
  }
  return gfx::GpuMemoryBufferHandle();
}

bool VideoFrame::IsSameAllocation(VideoPixelFormat format,
                                  const gfx::Size& coded_size,
                                  const gfx::Rect& visible_rect,
                                  const gfx::Size& natural_size) const {
  // CreateFrameInternal() changes coded_size to new_coded_size. Match that
  // behavior here.
  const gfx::Size new_coded_size = DetermineAlignedSize(format, coded_size);
  return this->format() == format && this->coded_size() == new_coded_size &&
         visible_rect_ == visible_rect && natural_size_ == natural_size;
}

gfx::ColorSpace VideoFrame::ColorSpace() const {
  return color_space_;
}

gfx::ColorSpace VideoFrame::CompatRGBColorSpace() const {
  const auto rgb_color_space = ColorSpace().GetAsFullRangeRGB();
  if (!rgb_color_space.IsValid()) {
    return gfx::ColorSpace::CreateSRGB();
  }

  auto primary_id = rgb_color_space.GetPrimaryID();
  switch (primary_id) {
    case gfx::ColorSpace::PrimaryID::CUSTOM:
      return rgb_color_space;
    case gfx::ColorSpace::PrimaryID::SMPTE170M:
    case gfx::ColorSpace::PrimaryID::SMPTE240M:
      primary_id = gfx::ColorSpace::PrimaryID::BT709;
      break;
    default:
      break;
  }
  auto transfer_id = rgb_color_space.GetTransferID();
  switch (transfer_id) {
    case gfx::ColorSpace::TransferID::CUSTOM:
    case gfx::ColorSpace::TransferID::CUSTOM_HDR:
      return rgb_color_space;
    case gfx::ColorSpace::TransferID::BT709_APPLE:
    case gfx::ColorSpace::TransferID::SMPTE170M:
    case gfx::ColorSpace::TransferID::SMPTE240M:
      transfer_id = gfx::ColorSpace::TransferID::SRGB;
      break;
    default:
      break;
  }
  return gfx::ColorSpace(primary_id, transfer_id);
}

bool VideoFrame::RequiresExternalSampler() const {
  const bool is_multiplanar_pixel_format =
      format() == PIXEL_FORMAT_NV12 || format() == PIXEL_FORMAT_NV12A ||
      format() == PIXEL_FORMAT_YV12 || format() == PIXEL_FORMAT_P010LE;

  // Note that kSharedImageFormatExternalSampler is set only for multiplanar
  // formats.
  const bool requires_external_sampler =
      is_multiplanar_pixel_format &&
      shared_image_format_type() ==
          SharedImageFormatType::kSharedImageFormatExternalSampler;

  // The texture target can be 0 for Fuchsia.
  DCHECK(!requires_external_sampler ||
         (is_multiplanar_pixel_format &&
          (mailbox_holder(0).texture_target == GL_TEXTURE_EXTERNAL_OES ||
           mailbox_holder(0).texture_target == 0u)));
  return requires_external_sampler;
}

int VideoFrame::row_bytes(size_t plane) const {
  return RowBytes(plane, format(), coded_size().width());
}

int VideoFrame::rows(size_t plane) const {
  return Rows(plane, format(), coded_size().height());
}

int VideoFrame::columns(size_t plane) const {
  return Columns(plane, format(), coded_size().width());
}

template <typename T>
T VideoFrame::GetVisibleDataInternal(T data, size_t plane) const {
  DCHECK(IsValidPlane(format(), plane));
  DCHECK(IsMappable());
  if (!data) [[unlikely]] {
    return nullptr;
  }

  // Calculate an offset that is properly aligned for all planes.
  const gfx::Size alignment = CommonAlignment(format());
  const gfx::Point offset(base::bits::AlignDownDeprecatedDoNotUse(
                              visible_rect_.x(), alignment.width()),
                          base::bits::AlignDownDeprecatedDoNotUse(
                              visible_rect_.y(), alignment.height()));

  const gfx::Size subsample = SampleSize(format(), plane);
  DCHECK(offset.x() % subsample.width() == 0);
  DCHECK(offset.y() % subsample.height() == 0);
  return data +
         stride(plane) * (offset.y() / subsample.height()) +  // Row offset.
         BytesPerElement(format(), plane) *                   // Column offset.
             (offset.x() / subsample.width());
}

const uint8_t* VideoFrame::visible_data(size_t plane) const {
  return GetVisibleDataInternal(data(plane), plane);
}

uint8_t* VideoFrame::GetWritableVisibleData(size_t plane) {
  // TODO(crbug.com/40265179): Also CHECK that the storage type isn't
  // STORAGE_UNOWNED_MEMORY once non-compliant usages are fixed.
  CHECK_NE(storage_type_, STORAGE_SHMEM);
  return GetVisibleDataInternal(writable_data(plane), plane);
}

// TODO(crbug.com/332564976): Update method to not take in param.
const gpu::MailboxHolder VideoFrame::mailbox_holder(
    size_t texture_index) const {
  CHECK_EQ(texture_index, 0u);
  CHECK(HasSharedImage());
  if (wrapped_frame_) {
    return wrapped_frame_->mailbox_holder(texture_index);
  }
  return gpu::MailboxHolder(shared_image_->mailbox(), acquire_sync_token_,
                            shared_image_->GetTextureTarget());
}

gpu::SyncToken VideoFrame::acquire_sync_token() const {
  CHECK(HasSharedImage());
  return wrapped_frame_ ? wrapped_frame_->acquire_sync_token()
                        : acquire_sync_token_;
}

scoped_refptr<gpu::ClientSharedImage> VideoFrame::shared_image() const {
  CHECK(HasSharedImage());
  return wrapped_frame_ ? wrapped_frame_->shared_image() : shared_image_;
}

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
size_t VideoFrame::NumDmabufFds() const {
  if (wrapped_frame_) {
    return wrapped_frame_->NumDmabufFds();
  }
  return dmabuf_fds_.size();
}

bool VideoFrame::HasDmaBufs() const {
  return NumDmabufFds() > 0;
}

int VideoFrame::GetDmabufFd(size_t i) const {
  if (wrapped_frame_) {
    return wrapped_frame_->GetDmabufFd(i);
  }

  DCHECK_EQ(storage_type_, STORAGE_DMABUFS);
  return dmabuf_fds_[i].get();
}
#endif

#if BUILDFLAG(IS_APPLE)
CVPixelBufferRef VideoFrame::CvPixelBuffer() const {
  return cv_pixel_buffer_.get();
}
#endif

void VideoFrame::SetReleaseMailboxCB(ReleaseMailboxCB release_mailbox_cb) {
  DCHECK(release_mailbox_cb);
  DCHECK(!mailbox_holder_and_gmb_release_cb_);
  // We don't relay SetReleaseMailboxCB to |wrapped_frame_| because the method
  // is not thread safe.  This method should only be called by the owner of
  // |wrapped_frame_| directly.
  DCHECK(!wrapped_frame_);
  mailbox_holder_and_gmb_release_cb_ =
      WrapReleaseMailboxCB(std::move(release_mailbox_cb));
}

void VideoFrame::SetReleaseMailboxAndGpuMemoryBufferCB(
    ReleaseMailboxAndGpuMemoryBufferCB release_mailbox_cb) {
  // See remarks in SetReleaseMailboxCB.
  DCHECK(release_mailbox_cb);
  DCHECK(!mailbox_holder_and_gmb_release_cb_);
  DCHECK(!wrapped_frame_);
  mailbox_holder_and_gmb_release_cb_ = std::move(release_mailbox_cb);
}

bool VideoFrame::HasReleaseMailboxCB() const {
  return wrapped_frame_ ? wrapped_frame_->HasReleaseMailboxCB()
                        : !!mailbox_holder_and_gmb_release_cb_;
}

void VideoFrame::AddDestructionObserver(base::OnceClosure callback) {
  DCHECK(!callback.is_null());
  base::AutoLock lock(done_callbacks_lock_);
  done_callbacks_.push_back(std::move(callback));
}

gpu::SyncToken VideoFrame::UpdateReleaseSyncToken(SyncTokenClient* client) {
  DCHECK(HasSharedImage());
  if (wrapped_frame_) {
    return wrapped_frame_->UpdateReleaseSyncToken(client);
  }
  base::AutoLock locker(release_sync_token_lock_);
  // Must wait on the previous sync point before inserting a new sync point so
  // that |mailbox_holder_and_gmb_release_cb_| guarantees the previous sync
  // point occurred when it waits on |release_sync_token_|.
  if (release_sync_token_.HasData())
    client->WaitSyncToken(release_sync_token_);
  client->GenerateSyncToken(&release_sync_token_);
  return release_sync_token_;
}

gpu::SyncToken VideoFrame::UpdateMailboxHolderSyncToken(
    SyncTokenClient* client) {
  DCHECK(HasOneRef());
  DCHECK(HasSharedImage());
  DCHECK(!wrapped_frame_);

  // No lock is required due to the HasOneRef() check.
  auto& token = acquire_sync_token_;
  if (token.HasData())
    client->WaitSyncToken(token);
  client->GenerateSyncToken(&token);
  return token;
}

std::string VideoFrame::AsHumanReadableString() const {
  if (metadata().end_of_stream)
    return "end of stream";

  std::ostringstream s;
  s << ConfigToString(format(), storage_type_, coded_size(), visible_rect_,
                      natural_size_)
    << " timestamp:" << timestamp_.InMicroseconds();
  if (HasSharedImage()) {
    s << " shared_image: true";
  }
  return s.str();
}

size_t VideoFrame::BitDepth() const {
  return media::BitDepth(format());
}

VideoFrame::VideoFrame(const VideoFrameLayout& layout,
                       StorageType storage_type,
                       const gfx::Rect& visible_rect,
                       const gfx::Size& natural_size,
                       base::TimeDelta timestamp,
                       FrameControlType frame_control_type)
    : layout_(layout),
      storage_type_(storage_type),
      visible_rect_(Intersection(visible_rect, gfx::Rect(layout.coded_size()))),
      natural_size_(natural_size),
      timestamp_(timestamp),
      unique_id_(GetNextID()) {
  DCHECK(IsValidConfigInternal(format(), frame_control_type, coded_size(),
                               visible_rect_, natural_size_));
  DCHECK(visible_rect_ == visible_rect)
      << "visible_rect " << visible_rect.ToString() << " exceeds coded_size "
      << coded_size().ToString();
  memset(&data_, 0, sizeof(data_));
}

VideoFrame::~VideoFrame() {
  if (mailbox_holder_and_gmb_release_cb_) {
    gpu::SyncToken release_sync_token;
    {
      // To ensure that changes to |release_sync_token_| are visible on this
      // thread (imply a memory barrier).
      base::AutoLock locker(release_sync_token_lock_);
      release_sync_token = release_sync_token_;
    }
    std::move(mailbox_holder_and_gmb_release_cb_)
        .Run(release_sync_token, std::move(gpu_memory_buffer_));
  }

  // Prevents dangling raw ptr, see https://docs.google.com/document/d/156O7kBZqIhe1dUcqTMcN5T-6YEAcg0yNnj5QlnZu9xU/edit?usp=sharing.
  shm_region_ = nullptr;

  std::vector<base::OnceClosure> done_callbacks;
  {
    base::AutoLock lock(done_callbacks_lock_);
    done_callbacks = std::move(done_callbacks_);
  }
  for (auto& callback : done_callbacks) {
    std::move(callback).Run();
  }

  // This flattens the call graph avoiding recursion while walking
  // `intermediate_wrapped_frame_` pointer chain, otherwise we might get
  // a stack overflow while deleting the whole chain of nested frames.
  auto frame_to_release = std::move(intermediate_wrapped_frame_);
  // Delete all the frames for which `intermediate_wrapped_frame_` is
  // the only reference.
  while (frame_to_release && frame_to_release->HasOneRef()) {
    auto next_frame = std::move(frame_to_release->intermediate_wrapped_frame_);
    frame_to_release = next_frame;
  }
}

// static
std::string VideoFrame::ConfigToString(const VideoPixelFormat format,
                                       const StorageType storage_type,
                                       const gfx::Size& coded_size,
                                       const gfx::Rect& visible_rect,
                                       const gfx::Size& natural_size) {
  return base::StringPrintf(
      "format:%s storage_type:%s coded_size:%s visible_rect:%s natural_size:%s",
      VideoPixelFormatToString(format).c_str(),
      StorageTypeToString(storage_type).c_str(), coded_size.ToString().c_str(),
      visible_rect.ToString().c_str(), natural_size.ToString().c_str());
}

// static
gfx::Size VideoFrame::DetermineAlignedSize(VideoPixelFormat format,
                                           const gfx::Size& dimensions) {
  const gfx::Size alignment = CommonAlignment(format);
  const gfx::Size adjusted =
      gfx::Size(base::bits::AlignUpDeprecatedDoNotUse(dimensions.width(),
                                                      alignment.width()),
                base::bits::AlignUpDeprecatedDoNotUse(dimensions.height(),
                                                      alignment.height()));
  DCHECK((adjusted.width() % alignment.width() == 0) &&
         (adjusted.height() % alignment.height() == 0));
  return adjusted;
}

// static
bool VideoFrame::IsValidSize(const gfx::Size& coded_size,
                             const gfx::Rect& visible_rect,
                             const gfx::Size& natural_size) {
  return IsValidCodedSize(coded_size) && IsValidCodedSize(natural_size) &&
         !(visible_rect.x() < 0 || visible_rect.y() < 0 ||
           visible_rect.right() > coded_size.width() ||
           visible_rect.bottom() > coded_size.height());
}

// static
bool VideoFrame::IsValidCodedSize(const gfx::Size& size) {
  const int size_area = size.GetCheckedArea().ValueOrDefault(INT_MAX);
  static_assert(limits::kMaxCanvas < INT_MAX, "");
  return size_area <= limits::kMaxCanvas &&
         size.width() <= limits::kMaxDimension &&
         size.height() <= limits::kMaxDimension;
}

// static
bool VideoFrame::IsValidConfigInternal(VideoPixelFormat format,
                                       FrameControlType frame_control_type,
                                       const gfx::Size& coded_size,
                                       const gfx::Rect& visible_rect,
                                       const gfx::Size& natural_size) {
  // Check maximum limits for all formats.
  if (!IsValidSize(coded_size, visible_rect, natural_size)) {
    return false;
  }

  switch (frame_control_type) {
    case FrameControlType::kNone:
      // Check that software-allocated buffer formats are not empty.
      return !coded_size.IsEmpty() && !visible_rect.IsEmpty() &&
             !natural_size.IsEmpty();
    case FrameControlType::kEos:
      DCHECK_EQ(format, PIXEL_FORMAT_UNKNOWN);
      return coded_size.IsEmpty() && visible_rect.IsEmpty() &&
             natural_size.IsEmpty();
    case FrameControlType::kVideoHole:
      DCHECK_EQ(format, PIXEL_FORMAT_UNKNOWN);
      return !coded_size.IsEmpty() && !visible_rect.IsEmpty() &&
             !natural_size.IsEmpty();
  }
}

// static
std::optional<VideoFrameLayout>
VideoFrame::CreateFullySpecifiedLayoutWithStrides(VideoPixelFormat format,
                                                  const gfx::Size& coded_size) {
  const gfx::Size new_coded_size = DetermineAlignedSize(format, coded_size);
  auto layout = VideoFrameLayout::CreateWithStrides(
      format, new_coded_size, ComputeStrides(format, new_coded_size));
  if (!layout) {
    return {};
  }
  // This whole method would be in `VideoFrameLayout::CreateWithStrides()`
  // instead of here, except that we know how to calculate the plane sizes.
  // This should be refactored.
  auto plane_sizes = CalculatePlaneSize(*layout);
  // Fill in the offsets as well, since WrapExternalDataWithLayout() uses them
  // to figure out where the data is.
  size_t offset = 0u;
  const size_t num_planes = plane_sizes.size();
  std::vector<ColorPlaneLayout> new_planes;
  new_planes.reserve(num_planes);
  for (size_t plane = 0; plane < plane_sizes.size(); plane++) {
    new_planes.emplace_back(layout->planes()[plane].stride, offset,
                            plane_sizes[plane]);
    offset += plane_sizes[plane];
  }
  return VideoFrameLayout::CreateWithPlanes(format, new_coded_size,
                                            std::move(new_planes));
}

// static
scoped_refptr<VideoFrame> VideoFrame::CreateFrameInternal(
    VideoPixelFormat format,
    const gfx::Size& coded_size,
    const gfx::Rect& visible_rect,
    const gfx::Size& natural_size,
    base::TimeDelta timestamp,
    bool zero_initialize_memory) {
  // Since we're creating a new frame (and allocating memory for it
  // ourselves), we can pad the requested |coded_size| if necessary if the
  // request does not line up on sample boundaries. See discussion at
  // http://crrev.com/1240833003
  const gfx::Size new_coded_size = DetermineAlignedSize(format, coded_size);
  auto layout = VideoFrameLayout::CreateWithStrides(
      format, new_coded_size, ComputeStrides(format, new_coded_size));
  if (!layout) {
    DLOG(ERROR) << "Invalid layout.";
    return nullptr;
  }

  return CreateFrameWithLayout(*layout, visible_rect, natural_size, timestamp,
                               zero_initialize_memory);
}

scoped_refptr<VideoFrame> VideoFrame::CreateFrameWithLayout(
    const VideoFrameLayout& layout,
    const gfx::Rect& visible_rect,
    const gfx::Size& natural_size,
    base::TimeDelta timestamp,
    bool zero_initialize_memory) {
  const StorageType storage = STORAGE_OWNED_MEMORY;
  if (!IsValidConfig(layout.format(), storage, layout.coded_size(),
                     visible_rect, natural_size)) {
    DLOG(ERROR) << __func__ << " Invalid config."
                << ConfigToString(layout.format(), storage, layout.coded_size(),
                                  visible_rect, natural_size);
    return nullptr;
  }

  scoped_refptr<VideoFrame> frame(new VideoFrame(
      std::move(layout), storage, visible_rect, natural_size, timestamp));
  return frame->AllocateMemory(zero_initialize_memory) ? frame : nullptr;
}

// static
gfx::Size VideoFrame::CommonAlignment(VideoPixelFormat format) {
  int max_sample_width = 0;
  int max_sample_height = 0;
  for (size_t plane = 0; plane < NumPlanes(format); ++plane) {
    const gfx::Size sample_size = SampleSize(format, plane);
    max_sample_width = std::max(max_sample_width, sample_size.width());
    max_sample_height = std::max(max_sample_height, sample_size.height());
  }
  return gfx::Size(max_sample_width, max_sample_height);
}

bool VideoFrame::AllocateMemory(bool zero_initialize_memory) {
  DCHECK_EQ(storage_type_, STORAGE_OWNED_MEMORY);
  static_assert(0 == Plane::kY, "y plane data must be index 0");

  std::vector<size_t> plane_size = CalculatePlaneSize();
  const size_t buffer_size =
      std::accumulate(plane_size.begin(), plane_size.end(), 0u);
  const size_t allocation_size =
      buffer_size + (layout_.buffer_addr_align() - 1);

  uint8_t* data = nullptr;
  if (zero_initialize_memory) {
    if (!base::UncheckedCalloc(1, allocation_size,
                               reinterpret_cast<void**>(&data)) ||
        !data) {
      return false;
    }
  } else {
    if (!base::UncheckedMalloc(allocation_size,
                               reinterpret_cast<void**>(&data)) ||
        !data) {
      return false;
    }
  }
  private_data_.reset(data);

  data = base::bits::AlignUp(data, layout_.buffer_addr_align());
  DCHECK_LE(data + buffer_size, private_data_.get() + allocation_size);

  // Note that if layout.buffer_sizes is specified, color planes' layout is
  // the same as buffers'. See CalculatePlaneSize() for detail.
  for (size_t plane = 0, offset = 0; plane < NumPlanes(format()); ++plane) {
    data_[plane] = data + offset;
    offset += plane_size[plane];
  }

  return true;
}

bool VideoFrame::IsValidSharedMemoryFrame() const {
  if (storage_type_ == STORAGE_SHMEM)
    return shm_region_ && shm_region_->IsValid();
  return false;
}

// static
std::vector<size_t> VideoFrame::CalculatePlaneSize(
    const VideoFrameLayout& layout) {
  // We have two cases for plane size mapping:
  // 1) If plane size is specified: use planes' size.
  // 2) VideoFrameLayout::size is unassigned: use legacy calculation formula.

  const auto format = layout.format();
  const size_t num_planes = NumPlanes(format);
  const auto& planes = layout.planes();
  std::vector<size_t> plane_size(num_planes);
  bool plane_size_assigned = true;
  DCHECK_EQ(planes.size(), num_planes);
  for (size_t i = 0; i < num_planes; ++i) {
    plane_size[i] = planes[i].size;
    plane_size_assigned &= plane_size[i] != 0;
  }

  if (plane_size_assigned)
    return plane_size;

  // Reset plane size.
  std::fill(plane_size.begin(), plane_size.end(), 0u);
  for (size_t plane = 0; plane < num_planes; ++plane) {
    // These values were chosen to mirror ffmpeg's get_video_buffer().
    // TODO(dalecurtis): This should be configurable; eventually ffmpeg wants
    // us to use av_cpu_max_align(), but... for now, they just hard-code 32.
    const size_t height =
        base::bits::AlignUp(Rows(plane, format, layout.coded_size().height()),
                            kFrameAddressAlignment);
    const size_t width = std::abs(layout.planes()[plane].stride);
    plane_size[plane] = width * height;
  }

  if (num_planes > 1) {
    // The extra line of UV being allocated is because h264 chroma MC
    // overreads by one line in some cases, see libavcodec/utils.c:
    // avcodec_align_dimensions2() and libavcodec/x86/h264_chromamc.asm:
    // put_h264_chroma_mc4_ssse3().
    DCHECK(IsValidPlane(format, Plane::kU));
    DCHECK(Plane::kU < num_planes);
    plane_size.back() +=
        std::abs(layout.planes()[Plane::kU].stride) + kFrameSizePadding;
  }
  return plane_size;
}

std::vector<size_t> VideoFrame::CalculatePlaneSize() const {
  return CalculatePlaneSize(layout_);
}

VideoFrame::ScopedMapping::ScopedMapping(
    gfx::GpuMemoryBuffer* gpu_memory_buffer,
    std::unique_ptr<gpu::ClientSharedImage::ScopedMapping> scoped_mapping)
    : gpu_memory_buffer_(gpu_memory_buffer),
      scoped_mapping_(std::move(scoped_mapping)) {
  // It should be backed by either one below.
  CHECK_NE(!!gpu_memory_buffer, !!scoped_mapping_);
}

VideoFrame::ScopedMapping::~ScopedMapping() {
  if (gpu_memory_buffer_) {
    gpu_memory_buffer_->Unmap();
  }
}

uint8_t* VideoFrame::ScopedMapping::Memory(uint32_t plane_index) {
  return static_cast<uint8_t*>(
      gpu_memory_buffer_
          ? gpu_memory_buffer_->memory(plane_index)
          : scoped_mapping_->GetMemoryForPlane(plane_index).data());
}

size_t VideoFrame::ScopedMapping::Stride(uint32_t plane_index) {
  return gpu_memory_buffer_ ? base::checked_cast<size_t>(
                                  gpu_memory_buffer_->stride(plane_index))
                            : scoped_mapping_->Stride(plane_index);
}

gfx::Size VideoFrame::ScopedMapping::Size() {
  return gpu_memory_buffer_ ? gpu_memory_buffer_->GetSize()
                            : scoped_mapping_->Size();
}
void VideoFrame::MakeScopedMappingForGpuMemoryBuffer(
    base::OnceCallback<void(std::unique_ptr<VideoFrame::ScopedMapping>)>
        result_cb,
    bool success) const {
  std::move(result_cb).Run(success
                               ? base::WrapUnique(new VideoFrame::ScopedMapping(
                                     gpu_memory_buffer_.get(), nullptr))
                               : nullptr);
}

void VideoFrame::WrapScopedSharedImageMapping(
    base::OnceCallback<void(std::unique_ptr<VideoFrame::ScopedMapping>)>
        result_cb,
    std::unique_ptr<gpu::ClientSharedImage::ScopedMapping> mapping) const {
  std::move(result_cb).Run(mapping
                               ? base::WrapUnique(new VideoFrame::ScopedMapping(
                                     nullptr, std::move(mapping)))
                               : nullptr);
}

}  // namespace media
