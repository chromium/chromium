// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/video_frame.h"

#include <algorithm>
#include <climits>
#include <numeric>
#include <utility>

#include "base/atomic_sequence_num.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/aligned_memory.h"
#include "base/strings/string_piece.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "media/base/limits.h"
#include "media/base/scopedfd_helper.h"
#include "media/base/timestamp_constants.h"
#include "media/base/video_util.h"
#include "ui/gfx/geometry/point.h"

namespace media {

namespace {

// Helper to privide gfx::Rect::Intersect() as an expression.
gfx::Rect Intersection(gfx::Rect a, const gfx::Rect& b) {
  a.Intersect(b);
  return a;
}

}  // namespace

// Static constexpr class for generating unique identifiers for each VideoFrame.
static base::AtomicSequenceNumber g_unique_id_generator;

static bool IsPowerOfTwo(size_t x) {
  return x != 0 && (x & (x - 1)) == 0;
}

static inline size_t RoundUp(size_t value, size_t alignment) {
  DCHECK(IsPowerOfTwo(alignment));
  return ((value + (alignment - 1)) & ~(alignment - 1));
}

static inline size_t RoundDown(size_t value, size_t alignment) {
  DCHECK(IsPowerOfTwo(alignment));
  return value & ~(alignment - 1);
}

static std::string StorageTypeToString(
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
#if defined(OS_LINUX)
    case VideoFrame::STORAGE_DMABUFS:
      return "DMABUFS";
#endif
    case VideoFrame::STORAGE_MOJO_SHARED_BUFFER:
      return "MOJO_SHARED_BUFFER";
  }

  NOTREACHED() << "Invalid StorageType provided: " << storage_type;
  return "INVALID";
}

// Returns true if |frame| is accesible mapped in the VideoFrame memory space.
// static
static bool IsStorageTypeMappable(VideoFrame::StorageType storage_type) {
  return
#if defined(OS_LINUX)
      // This is not strictly needed but makes explicit that, at VideoFrame
      // level, DmaBufs are not mappable from userspace.
      storage_type != VideoFrame::STORAGE_DMABUFS &&
#endif
      (storage_type == VideoFrame::STORAGE_UNOWNED_MEMORY ||
       storage_type == VideoFrame::STORAGE_OWNED_MEMORY ||
       storage_type == VideoFrame::STORAGE_SHMEM ||
       storage_type == VideoFrame::STORAGE_MOJO_SHARED_BUFFER);
}

// Checks if |source_format| can be wrapped into a |target_format| frame.
static bool AreValidPixelFormatsForWrap(VideoPixelFormat source_format,
                                        VideoPixelFormat target_format) {
  if (source_format == target_format)
    return true;

  // It is possible to add other planar to planar format conversions here if the
  // use case is there.
  return source_format == PIXEL_FORMAT_I420A &&
         target_format == PIXEL_FORMAT_I420;
}

// If it is required to allocate aligned to multiple-of-two size overall for the
// frame of pixel |format|.
bool RequiresEvenSizeAllocation(VideoPixelFormat format) {
  switch (format) {
    case PIXEL_FORMAT_ARGB:
    case PIXEL_FORMAT_XRGB:
    case PIXEL_FORMAT_RGB24:
    case PIXEL_FORMAT_RGB32:
    case PIXEL_FORMAT_Y16:
    case PIXEL_FORMAT_ABGR:
    case PIXEL_FORMAT_XBGR:
      return false;
    case PIXEL_FORMAT_NV12:
    case PIXEL_FORMAT_NV21:
    case PIXEL_FORMAT_MT21:
    case PIXEL_FORMAT_I420:
    case PIXEL_FORMAT_MJPEG:
    case PIXEL_FORMAT_YUY2:
    case PIXEL_FORMAT_YV12:
    case PIXEL_FORMAT_I422:
    case PIXEL_FORMAT_I444:
    case PIXEL_FORMAT_YUV420P9:
    case PIXEL_FORMAT_YUV422P9:
    case PIXEL_FORMAT_YUV444P9:
    case PIXEL_FORMAT_YUV420P10:
    case PIXEL_FORMAT_YUV422P10:
    case PIXEL_FORMAT_YUV444P10:
    case PIXEL_FORMAT_YUV420P12:
    case PIXEL_FORMAT_YUV422P12:
    case PIXEL_FORMAT_YUV444P12:
    case PIXEL_FORMAT_I420A:
    case PIXEL_FORMAT_UYVY:
      return true;
    case PIXEL_FORMAT_UNKNOWN:
      break;
  }
  NOTREACHED() << "Unsupported video frame format: " << format;
  return false;
}

// static
bool VideoFrame::IsValidConfig(VideoPixelFormat format,
                               StorageType storage_type,
                               const gfx::Size& coded_size,
                               const gfx::Rect& visible_rect,
                               const gfx::Size& natural_size) {
  // Check maximum limits for all formats.
  int coded_size_area = coded_size.GetCheckedArea().ValueOrDefault(INT_MAX);
  int natural_size_area = natural_size.GetCheckedArea().ValueOrDefault(INT_MAX);
  static_assert(limits::kMaxCanvas < INT_MAX, "");
  if (coded_size_area > limits::kMaxCanvas ||
      coded_size.width() > limits::kMaxDimension ||
      coded_size.height() > limits::kMaxDimension || visible_rect.x() < 0 ||
      visible_rect.y() < 0 || visible_rect.right() > coded_size.width() ||
      visible_rect.bottom() > coded_size.height() ||
      natural_size_area > limits::kMaxCanvas ||
      natural_size.width() > limits::kMaxDimension ||
      natural_size.height() > limits::kMaxDimension)
    return false;

  // TODO(mcasas): Remove parameter |storage_type| when the opaque storage types
  // comply with the checks below. Right now we skip them.
  if (!IsStorageTypeMappable(storage_type))
    return true;

  // Make sure new formats are properly accounted for in the method.
  static_assert(PIXEL_FORMAT_MAX == 28,
                "Added pixel format, please review IsValidConfig()");

  if (format == PIXEL_FORMAT_UNKNOWN) {
    return coded_size.IsEmpty() && visible_rect.IsEmpty() &&
           natural_size.IsEmpty();
  }

  // Check that software-allocated buffer formats are not empty.
  return !coded_size.IsEmpty() && !visible_rect.IsEmpty() &&
         !natural_size.IsEmpty();
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
scoped_refptr<VideoFrame> VideoFrame::WrapNativeTextures(
    VideoPixelFormat format,
    const gpu::MailboxHolder (&mailbox_holders)[kMaxPlanes],
    ReleaseMailboxCB mailbox_holder_release_cb,
    const gfx::Size& coded_size,
    const gfx::Rect& visible_rect,
    const gfx::Size& natural_size,
    base::TimeDelta timestamp) {
  if (format != PIXEL_FORMAT_ARGB && format != PIXEL_FORMAT_XRGB &&
      format != PIXEL_FORMAT_RGB32 && format != PIXEL_FORMAT_UYVY &&
      format != PIXEL_FORMAT_NV12 && format != PIXEL_FORMAT_I420) {
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
  memcpy(&frame->mailbox_holders_, mailbox_holders,
         sizeof(frame->mailbox_holders_));
  frame->mailbox_holders_release_cb_ = std::move(mailbox_holder_release_cb);

  // Wrapping native textures should... have textures. https://crbug.com/864145.
  DCHECK(frame->HasTextures());

  return frame;
}

// static
scoped_refptr<VideoFrame> VideoFrame::WrapExternalData(
    VideoPixelFormat format,
    const gfx::Size& coded_size,
    const gfx::Rect& visible_rect,
    const gfx::Size& natural_size,
    uint8_t* data,
    size_t data_size,
    base::TimeDelta timestamp) {
  return WrapExternalStorage(format, STORAGE_UNOWNED_MEMORY, coded_size,
                             visible_rect, natural_size, data, data_size,
                             timestamp, nullptr, nullptr,
                             base::SharedMemoryHandle(), 0);
}

// static
scoped_refptr<VideoFrame> VideoFrame::WrapExternalReadOnlySharedMemory(
    VideoPixelFormat format,
    const gfx::Size& coded_size,
    const gfx::Rect& visible_rect,
    const gfx::Size& natural_size,
    uint8_t* data,
    size_t data_size,
    base::ReadOnlySharedMemoryRegion* region,
    size_t data_offset,
    base::TimeDelta timestamp) {
  return WrapExternalStorage(format, STORAGE_SHMEM, coded_size, visible_rect,
                             natural_size, data, data_size, timestamp, region,
                             nullptr, base::SharedMemoryHandle(), data_offset);
}

// static
scoped_refptr<VideoFrame> VideoFrame::WrapExternalUnsafeSharedMemory(
    VideoPixelFormat format,
    const gfx::Size& coded_size,
    const gfx::Rect& visible_rect,
    const gfx::Size& natural_size,
    uint8_t* data,
    size_t data_size,
    base::UnsafeSharedMemoryRegion* region,
    size_t data_offset,
    base::TimeDelta timestamp) {
  return WrapExternalStorage(format, STORAGE_SHMEM, coded_size, visible_rect,
                             natural_size, data, data_size, timestamp, nullptr,
                             region, base::SharedMemoryHandle(), data_offset);
}

// static
scoped_refptr<VideoFrame> VideoFrame::WrapExternalSharedMemory(
    VideoPixelFormat format,
    const gfx::Size& coded_size,
    const gfx::Rect& visible_rect,
    const gfx::Size& natural_size,
    uint8_t* data,
    size_t data_size,
    base::SharedMemoryHandle handle,
    size_t data_offset,
    base::TimeDelta timestamp) {
  return WrapExternalStorage(format, STORAGE_SHMEM, coded_size, visible_rect,
                             natural_size, data, data_size, timestamp, nullptr,
                             nullptr, handle, data_offset);
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
    uint8_t* y_data,
    uint8_t* u_data,
    uint8_t* v_data,
    base::TimeDelta timestamp) {
  const StorageType storage = STORAGE_UNOWNED_MEMORY;
  if (!IsValidConfig(format, storage, coded_size, visible_rect, natural_size)) {
    DLOG(ERROR) << __func__ << " Invalid config."
                << ConfigToString(format, storage, coded_size, visible_rect,
                                  natural_size);
    return nullptr;
  }

  const size_t height = coded_size.height();
  auto layout = VideoFrameLayout::CreateWithStrides(
      format, coded_size, {y_stride, u_stride, v_stride},
      {std::abs(y_stride) * height, std::abs(u_stride) * height,
       std::abs(v_stride) * height});
  if (!layout) {
    DLOG(ERROR) << "Invalid layout.";
    return nullptr;
  }

  scoped_refptr<VideoFrame> frame(
      new VideoFrame(*layout, storage, visible_rect, natural_size, timestamp));
  frame->data_[kYPlane] = y_data;
  frame->data_[kUPlane] = u_data;
  frame->data_[kVPlane] = v_data;
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
    uint8_t* y_data,
    uint8_t* u_data,
    uint8_t* v_data,
    uint8_t* a_data,
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

  const size_t height = coded_size.height();
  auto layout = VideoFrameLayout::CreateWithStrides(
      format, coded_size, {y_stride, u_stride, v_stride, a_stride},
      {abs(y_stride) * height, abs(u_stride) * height, abs(v_stride) * height,
       abs(a_stride) * height});
  if (!layout) {
    DLOG(ERROR) << "Invalid layout";
    return nullptr;
  }

  scoped_refptr<VideoFrame> frame(
      new VideoFrame(*layout, storage, visible_rect, natural_size, timestamp));
  frame->data_[kYPlane] = y_data;
  frame->data_[kUPlane] = u_data;
  frame->data_[kVPlane] = v_data;
  frame->data_[kAPlane] = a_data;
  return frame;
}

#if defined(OS_LINUX)
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

  gpu::MailboxHolder mailbox_holders[kMaxPlanes];
  scoped_refptr<VideoFrame> frame =
      new VideoFrame(layout, storage, visible_rect, natural_size, timestamp);
  if (!frame) {
    DLOG(ERROR) << __func__ << " Couldn't create VideoFrame instance.";
    return nullptr;
  }
  memcpy(&frame->mailbox_holders_, mailbox_holders,
         sizeof(frame->mailbox_holders_));
  frame->mailbox_holders_release_cb_ = ReleaseMailboxCB();
  frame->dmabuf_fds_ = std::move(dmabuf_fds);
  DCHECK(frame->HasDmaBufs());

  return frame;
}
#endif

#if defined(OS_MACOSX)
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
  } else if (cv_format == '420v') {
    // TODO(jfroy): Use kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange when the
    // minimum OS X and iOS SDKs permits it.
    format = PIXEL_FORMAT_NV12;
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
    const scoped_refptr<VideoFrame>& frame,
    VideoPixelFormat format,
    const gfx::Rect& visible_rect,
    const gfx::Size& natural_size) {
  // Frames with textures need mailbox info propagated, and there's no support
  // for that here yet, see http://crbug/362521.
  CHECK(!frame->HasTextures());
  DCHECK(frame->visible_rect().Contains(visible_rect));

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

  scoped_refptr<VideoFrame> wrapping_frame(
      new VideoFrame(frame->layout(), frame->storage_type(), visible_rect,
                     natural_size, frame->timestamp()));

  // Copy all metadata to the wrapped frame.
  wrapping_frame->metadata()->MergeMetadataFrom(frame->metadata());

  for (size_t i = 0; i < NumPlanes(format); ++i) {
    wrapping_frame->data_[i] = frame->data(i);
  }

#if defined(OS_LINUX)
  // If there are any |dmabuf_fds_| plugged in, we should duplicate them.
  if (frame->storage_type() == STORAGE_DMABUFS) {
    wrapping_frame->dmabuf_fds_ = DuplicateFDs(frame->dmabuf_fds_);
    if (wrapping_frame->dmabuf_fds_.empty()) {
      DLOG(ERROR) << __func__ << " Couldn't duplicate fds.";
      return nullptr;
    }
  }
#endif

  if (frame->storage_type() == STORAGE_SHMEM) {
    if (frame->read_only_shared_memory_region_) {
      DCHECK(frame->read_only_shared_memory_region_->IsValid());
      wrapping_frame->AddReadOnlySharedMemoryRegion(
          frame->read_only_shared_memory_region_);
    } else if (frame->unsafe_shared_memory_region_) {
      DCHECK(frame->unsafe_shared_memory_region_->IsValid());
      wrapping_frame->AddUnsafeSharedMemoryRegion(
          frame->unsafe_shared_memory_region_);
    } else {
      DCHECK(frame->shared_memory_handle_.IsValid());
      wrapping_frame->AddSharedMemoryHandle(frame->shared_memory_handle_);
    }
  }

  return wrapping_frame;
}

// static
scoped_refptr<VideoFrame> VideoFrame::CreateEOSFrame() {
  auto layout = VideoFrameLayout::Create(PIXEL_FORMAT_UNKNOWN, gfx::Size());
  if (!layout) {
    DLOG(ERROR) << "Invalid layout.";
    return nullptr;
  }
  scoped_refptr<VideoFrame> frame = new VideoFrame(
      *layout, STORAGE_UNKNOWN, gfx::Rect(), gfx::Size(), kNoTimestamp);
  frame->metadata()->SetBoolean(VideoFrameMetadata::END_OF_STREAM, true);
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
  DCHECK(IsValidPlane(plane, format));

  int width = coded_size.width();
  int height = coded_size.height();
  if (RequiresEvenSizeAllocation(format)) {
    // Align to multiple-of-two size overall. This ensures that non-subsampled
    // planes can be addressed by pixel with the same scaling as the subsampled
    // planes.
    width = RoundUp(width, 2);
    height = RoundUp(height, 2);
  }

  const gfx::Size subsample = SampleSize(format, plane);
  DCHECK(width % subsample.width() == 0);
  DCHECK(height % subsample.height() == 0);
  return gfx::Size(BytesPerElement(format, plane) * width / subsample.width(),
                   height / subsample.height());
}

// static
int VideoFrame::PlaneHorizontalBitsPerPixel(VideoPixelFormat format,
                                            size_t plane) {
  DCHECK(IsValidPlane(plane, format));
  const int bits_per_element = 8 * BytesPerElement(format, plane);
  const int horiz_pixels_per_element = SampleSize(format, plane).width();
  DCHECK_EQ(bits_per_element % horiz_pixels_per_element, 0);
  return bits_per_element / horiz_pixels_per_element;
}

// static
int VideoFrame::PlaneBitsPerPixel(VideoPixelFormat format, size_t plane) {
  DCHECK(IsValidPlane(plane, format));
  return PlaneHorizontalBitsPerPixel(format, plane) /
      SampleSize(format, plane).height();
}

// static
size_t VideoFrame::RowBytes(size_t plane, VideoPixelFormat format, int width) {
  DCHECK(IsValidPlane(plane, format));
  return BytesPerElement(format, plane) * Columns(plane, format, width);
}

// static
int VideoFrame::BytesPerElement(VideoPixelFormat format, size_t plane) {
  DCHECK(IsValidPlane(plane, format));
  switch (format) {
    case PIXEL_FORMAT_ARGB:
    case PIXEL_FORMAT_XRGB:
    case PIXEL_FORMAT_RGB32:
    case PIXEL_FORMAT_ABGR:
    case PIXEL_FORMAT_XBGR:
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
      return 2;
    case PIXEL_FORMAT_NV12:
    case PIXEL_FORMAT_NV21:
    case PIXEL_FORMAT_MT21: {
      static const int bytes_per_element[] = {1, 2};
      DCHECK_LT(plane, arraysize(bytes_per_element));
      return bytes_per_element[plane];
    }
    case PIXEL_FORMAT_YV12:
    case PIXEL_FORMAT_I420:
    case PIXEL_FORMAT_I422:
    case PIXEL_FORMAT_I420A:
    case PIXEL_FORMAT_I444:
      return 1;
    case PIXEL_FORMAT_MJPEG:
      return 0;
    case PIXEL_FORMAT_UNKNOWN:
      break;
  }
  NOTREACHED();
  return 0;
}

// static
size_t VideoFrame::Rows(size_t plane, VideoPixelFormat format, int height) {
  DCHECK(IsValidPlane(plane, format));
  const int sample_height = SampleSize(format, plane).height();
  return RoundUp(height, sample_height) / sample_height;
}

// static
size_t VideoFrame::Columns(size_t plane, VideoPixelFormat format, int width) {
  DCHECK(IsValidPlane(plane, format));
  const int sample_width = SampleSize(format, plane).width();
  return RoundUp(width, sample_width) / sample_width;
}

// static
void VideoFrame::HashFrameForTesting(base::MD5Context* context,
                                     const scoped_refptr<VideoFrame>& frame) {
  DCHECK(context);
  for (size_t plane = 0; plane < NumPlanes(frame->format()); ++plane) {
    for (int row = 0; row < frame->rows(plane); ++row) {
      base::MD5Update(
          context,
          base::StringPiece(reinterpret_cast<char*>(frame->data(plane) +
                                                    frame->stride(plane) * row),
                            frame->row_bytes(plane)));
    }
  }
}

bool VideoFrame::IsMappable() const {
  return IsStorageTypeMappable(storage_type_);
}

bool VideoFrame::HasTextures() const {
  return !mailbox_holders_[0].mailbox.IsZero();
}

size_t VideoFrame::NumTextures() const {
  if (!HasTextures())
    return 0;

  size_t i = 0;
  for (; i < NumPlanes(format()); ++i) {
    if (mailbox_holders_[i].mailbox.IsZero()) {
      return i;
    }
  }
  return i;
}

gfx::ColorSpace VideoFrame::ColorSpace() const {
  return color_space_;
}

int VideoFrame::row_bytes(size_t plane) const {
  return RowBytes(plane, format(), coded_size().width());
}

int VideoFrame::rows(size_t plane) const {
  return Rows(plane, format(), coded_size().height());
}

const uint8_t* VideoFrame::visible_data(size_t plane) const {
  DCHECK(IsValidPlane(plane, format()));
  DCHECK(IsMappable());

  // Calculate an offset that is properly aligned for all planes.
  const gfx::Size alignment = CommonAlignment(format());
  const gfx::Point offset(RoundDown(visible_rect_.x(), alignment.width()),
                          RoundDown(visible_rect_.y(), alignment.height()));

  const gfx::Size subsample = SampleSize(format(), plane);
  DCHECK(offset.x() % subsample.width() == 0);
  DCHECK(offset.y() % subsample.height() == 0);
  return data(plane) +
         stride(plane) * (offset.y() / subsample.height()) +  // Row offset.
         BytesPerElement(format(), plane) *                   // Column offset.
             (offset.x() / subsample.width());
}

uint8_t* VideoFrame::visible_data(size_t plane) {
  return const_cast<uint8_t*>(
      static_cast<const VideoFrame*>(this)->visible_data(plane));
}

const gpu::MailboxHolder&
VideoFrame::mailbox_holder(size_t texture_index) const {
  DCHECK(HasTextures());
  DCHECK(IsValidPlane(texture_index, format()));
  return mailbox_holders_[texture_index];
}

base::ReadOnlySharedMemoryRegion* VideoFrame::read_only_shared_memory_region()
    const {
  DCHECK_EQ(storage_type_, STORAGE_SHMEM);
  DCHECK(read_only_shared_memory_region_ &&
         read_only_shared_memory_region_->IsValid());
  return read_only_shared_memory_region_;
}

base::UnsafeSharedMemoryRegion* VideoFrame::unsafe_shared_memory_region()
    const {
  DCHECK_EQ(storage_type_, STORAGE_SHMEM);
  DCHECK(unsafe_shared_memory_region_ &&
         unsafe_shared_memory_region_->IsValid());
  return unsafe_shared_memory_region_;
}

base::SharedMemoryHandle VideoFrame::shared_memory_handle() const {
  DCHECK_EQ(storage_type_, STORAGE_SHMEM);
  DCHECK(shared_memory_handle_.IsValid());
  return shared_memory_handle_;
}

size_t VideoFrame::shared_memory_offset() const {
  DCHECK_EQ(storage_type_, STORAGE_SHMEM);
  DCHECK((read_only_shared_memory_region_ &&
          read_only_shared_memory_region_->IsValid()) ||
         (unsafe_shared_memory_region_ &&
          unsafe_shared_memory_region_->IsValid()) ||
         shared_memory_handle_.IsValid());
  return shared_memory_offset_;
}

#if defined(OS_LINUX)
const std::vector<base::ScopedFD>& VideoFrame::DmabufFds() const {
  DCHECK_EQ(storage_type_, STORAGE_DMABUFS);

  return dmabuf_fds_;
}

bool VideoFrame::HasDmaBufs() const {
  return !dmabuf_fds_.empty();
}
#endif

void VideoFrame::AddReadOnlySharedMemoryRegion(
    base::ReadOnlySharedMemoryRegion* region) {
  storage_type_ = STORAGE_SHMEM;
  DCHECK(SharedMemoryUninitialized());
  DCHECK(region && region->IsValid());
  read_only_shared_memory_region_ = region;
}

void VideoFrame::AddUnsafeSharedMemoryRegion(
    base::UnsafeSharedMemoryRegion* region) {
  storage_type_ = STORAGE_SHMEM;
  DCHECK(SharedMemoryUninitialized());
  DCHECK(region && region->IsValid());
  unsafe_shared_memory_region_ = region;
}

void VideoFrame::AddSharedMemoryHandle(base::SharedMemoryHandle handle) {
  storage_type_ = STORAGE_SHMEM;
  DCHECK(SharedMemoryUninitialized());
  shared_memory_handle_ = handle;
}

#if defined(OS_MACOSX)
CVPixelBufferRef VideoFrame::CvPixelBuffer() const {
  return cv_pixel_buffer_.get();
}
#endif

void VideoFrame::SetReleaseMailboxCB(ReleaseMailboxCB release_mailbox_cb) {
  DCHECK(release_mailbox_cb);
  DCHECK(!mailbox_holders_release_cb_);
  mailbox_holders_release_cb_ = std::move(release_mailbox_cb);
}

bool VideoFrame::HasReleaseMailboxCB() const {
  return !!mailbox_holders_release_cb_;
}

void VideoFrame::AddDestructionObserver(base::OnceClosure callback) {
  DCHECK(!callback.is_null());
  done_callbacks_.push_back(std::move(callback));
}

gpu::SyncToken VideoFrame::UpdateReleaseSyncToken(SyncTokenClient* client) {
  DCHECK(HasTextures());
  base::AutoLock locker(release_sync_token_lock_);
  // Must wait on the previous sync point before inserting a new sync point so
  // that |mailbox_holders_release_cb_| guarantees the previous sync point
  // occurred when it waits on |release_sync_token_|.
  if (release_sync_token_.HasData())
    client->WaitSyncToken(release_sync_token_);
  client->GenerateSyncToken(&release_sync_token_);
  return release_sync_token_;
}

std::string VideoFrame::AsHumanReadableString() {
  if (metadata()->IsTrue(media::VideoFrameMetadata::END_OF_STREAM))
    return "end of stream";

  std::ostringstream s;
  s << ConfigToString(format(), storage_type_, coded_size(), visible_rect_,
                      natural_size_)
    << " timestamp:" << timestamp_.InMicroseconds();
  return s.str();
}

size_t VideoFrame::BitDepth() const {
  return ::media::BitDepth(format());
}

// static
scoped_refptr<VideoFrame> VideoFrame::WrapExternalStorage(
    VideoPixelFormat format,
    StorageType storage_type,
    const gfx::Size& coded_size,
    const gfx::Rect& visible_rect,
    const gfx::Size& natural_size,
    uint8_t* data,
    size_t data_size,
    base::TimeDelta timestamp,
    base::ReadOnlySharedMemoryRegion* read_only_region,
    base::UnsafeSharedMemoryRegion* unsafe_region,
    base::SharedMemoryHandle handle,
    size_t data_offset) {
  DCHECK(IsStorageTypeMappable(storage_type));

  // TODO(miu): This function should support any pixel format.
  // http://crbug.com/555909
  if (format != PIXEL_FORMAT_I420 && format != PIXEL_FORMAT_Y16 &&
      format != PIXEL_FORMAT_ARGB && format != PIXEL_FORMAT_NV12) {
    DLOG(ERROR)
        << "Only PIXEL_FORMAT_I420, PIXEL_FORMAT_Y16, PIXEL_FORMAT_NV12, and "
           "PIXEL_FORMAT_ARGB formats are supported: "
        << VideoPixelFormatToString(format);
    return nullptr;
  }

  if (!IsValidConfig(format, storage_type, coded_size, visible_rect,
                     natural_size)) {
    DLOG(ERROR) << __func__ << " Invalid config."
                << ConfigToString(format, storage_type, coded_size,
                                  visible_rect, natural_size);
    return nullptr;
  }

  scoped_refptr<VideoFrame> frame;
  switch (NumPlanes(format)) {
    case 1: {
      auto layout = VideoFrameLayout::CreateWithStrides(
          format, coded_size,
          std::vector<int>{RowBytes(kYPlane, format, coded_size.width())});
      if (!layout) {
        DLOG(ERROR) << "Invalid layout.";
        return nullptr;
      }
      frame = new VideoFrame(*layout, storage_type, visible_rect, natural_size,
                             timestamp);
      frame->data_[kYPlane] = data;
      break;
    }
    case 2: {
      auto layout = VideoFrameLayout::CreateWithStrides(
          format, coded_size,
          std::vector<int>{RowBytes(kYPlane, format, coded_size.width()),
                           RowBytes(kUVPlane, format, coded_size.width())});
      if (!layout) {
        DLOG(ERROR) << "Invalid layout.";
        return nullptr;
      }
      frame = new VideoFrame(*layout, storage_type, visible_rect, natural_size,
                             timestamp);
      frame->data_[kYPlane] = data;
      frame->data_[kUVPlane] = data + coded_size.GetArea();
      break;
    }
    case 3: {
      DCHECK_EQ(format, PIXEL_FORMAT_I420);
      // TODO(miu): This always rounds widths down, whereas
      // VideoFrame::RowBytes() always rounds up.  This inconsistency must be
      // resolved.  Perhaps a CommonAlignment() check should be made in
      // IsValidConfig()?
      // http://crbug.com/555909
      auto layout = VideoFrameLayout::CreateWithStrides(
          format, coded_size,
          {RowBytes(kYPlane, format, coded_size.width()),
           coded_size.width() / 2, coded_size.width() / 2});
      if (!layout) {
        DLOG(ERROR) << "Invalid layout.";
        return nullptr;
      }
      frame = new VideoFrame(*layout, storage_type, visible_rect, natural_size,
                             timestamp);
      frame->data_[kYPlane] = data;
      frame->data_[kVPlane] = data + (coded_size.GetArea() * 5 / 4);
      frame->data_[kUPlane] = data + coded_size.GetArea();
      break;
    }
    default:
      DLOG(ERROR) << "Invalid number of planes: " << NumPlanes(format)
                  << " in format: " << VideoPixelFormatToString(format);
      return nullptr;
  }

  if (storage_type == STORAGE_SHMEM) {
    if (read_only_region || unsafe_region) {
      DCHECK(!handle.IsValid());
      DCHECK_NE(!!read_only_region, !!unsafe_region)
          << "Expected exactly one read-only or unsafe region for "
          << "STORAGE_SHMEM VideoFrame";
      if (read_only_region) {
        frame->read_only_shared_memory_region_ = read_only_region;
        DCHECK(frame->read_only_shared_memory_region_->IsValid());
      } else if (unsafe_region) {
        frame->unsafe_shared_memory_region_ = unsafe_region;
        DCHECK(frame->unsafe_shared_memory_region_->IsValid());
      }
      frame->shared_memory_offset_ = data_offset;
    } else {
      frame->AddSharedMemoryHandle(handle);
      frame->shared_memory_offset_ = data_offset;
    }
  }
  return frame;
}

VideoFrame::VideoFrame(const VideoFrameLayout& layout,
                       StorageType storage_type,
                       const gfx::Rect& visible_rect,
                       const gfx::Size& natural_size,
                       base::TimeDelta timestamp)
    : layout_(layout),
      storage_type_(storage_type),
      visible_rect_(Intersection(visible_rect, gfx::Rect(layout.coded_size()))),
      natural_size_(natural_size),
      shared_memory_offset_(0),
      timestamp_(timestamp),
      unique_id_(g_unique_id_generator.GetNext()) {
  DCHECK(IsValidConfig(format(), storage_type, coded_size(), visible_rect_,
                       natural_size_));
  DCHECK(visible_rect_ == visible_rect)
      << "visible_rect " << visible_rect.ToString() << " exceeds coded_size "
      << coded_size().ToString();
  memset(&mailbox_holders_, 0, sizeof(mailbox_holders_));
  memset(&data_, 0, sizeof(data_));
}

VideoFrame::~VideoFrame() {
  if (mailbox_holders_release_cb_) {
    gpu::SyncToken release_sync_token;
    {
      // To ensure that changes to |release_sync_token_| are visible on this
      // thread (imply a memory barrier).
      base::AutoLock locker(release_sync_token_lock_);
      release_sync_token = release_sync_token_;
    }
    std::move(mailbox_holders_release_cb_).Run(release_sync_token);
  }

  for (auto& callback : done_callbacks_)
    std::move(callback).Run();
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
bool VideoFrame::IsValidPlane(size_t plane, VideoPixelFormat format) {
  DCHECK_LE(NumPlanes(format), static_cast<size_t>(kMaxPlanes));
  return (plane < NumPlanes(format));
}

// static
gfx::Size VideoFrame::DetermineAlignedSize(VideoPixelFormat format,
                                           const gfx::Size& dimensions) {
  const gfx::Size alignment = CommonAlignment(format);
  const gfx::Size adjusted =
      gfx::Size(RoundUp(dimensions.width(), alignment.width()),
                RoundUp(dimensions.height(), alignment.height()));
  DCHECK((adjusted.width() % alignment.width() == 0) &&
         (adjusted.height() % alignment.height() == 0));
  return adjusted;
}

// static
scoped_refptr<VideoFrame> VideoFrame::CreateFrameInternal(
    VideoPixelFormat format,
    const gfx::Size& coded_size,
    const gfx::Rect& visible_rect,
    const gfx::Size& natural_size,
    base::TimeDelta timestamp,
    bool zero_initialize_memory) {
  // Since we're creating a new frame (and allocating memory for it ourselves),
  // we can pad the requested |coded_size| if necessary if the request does not
  // line up on sample boundaries. See discussion at http://crrev.com/1240833003
  const gfx::Size new_coded_size = DetermineAlignedSize(format, coded_size);
  auto layout = VideoFrameLayout::CreateWithStrides(
      format, new_coded_size, ComputeStrides(format, coded_size));
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
  frame->AllocateMemory(zero_initialize_memory);
  return frame;
}

bool VideoFrame::SharedMemoryUninitialized() {
  return !read_only_shared_memory_region_ && !unsafe_shared_memory_region_ &&
         !shared_memory_handle_.IsValid();
}

// static
gfx::Size VideoFrame::SampleSize(VideoPixelFormat format, size_t plane) {
  DCHECK(IsValidPlane(plane, format));

  switch (plane) {
    case kYPlane:  // and kARGBPlane:
    case kAPlane:
      return gfx::Size(1, 1);

    case kUPlane:  // and kUVPlane:
    case kVPlane:
      switch (format) {
        case PIXEL_FORMAT_I444:
        case PIXEL_FORMAT_YUV444P9:
        case PIXEL_FORMAT_YUV444P10:
        case PIXEL_FORMAT_YUV444P12:
        case PIXEL_FORMAT_Y16:
          return gfx::Size(1, 1);

        case PIXEL_FORMAT_I422:
        case PIXEL_FORMAT_YUV422P9:
        case PIXEL_FORMAT_YUV422P10:
        case PIXEL_FORMAT_YUV422P12:
          return gfx::Size(2, 1);

        case PIXEL_FORMAT_YV12:
        case PIXEL_FORMAT_I420:
        case PIXEL_FORMAT_I420A:
        case PIXEL_FORMAT_NV12:
        case PIXEL_FORMAT_NV21:
        case PIXEL_FORMAT_MT21:
        case PIXEL_FORMAT_YUV420P9:
        case PIXEL_FORMAT_YUV420P10:
        case PIXEL_FORMAT_YUV420P12:
          return gfx::Size(2, 2);

        case PIXEL_FORMAT_UNKNOWN:
        case PIXEL_FORMAT_UYVY:
        case PIXEL_FORMAT_YUY2:
        case PIXEL_FORMAT_ARGB:
        case PIXEL_FORMAT_XRGB:
        case PIXEL_FORMAT_RGB24:
        case PIXEL_FORMAT_RGB32:
        case PIXEL_FORMAT_MJPEG:
        case PIXEL_FORMAT_ABGR:
        case PIXEL_FORMAT_XBGR:
          break;
      }
  }
  NOTREACHED();
  return gfx::Size();
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

void VideoFrame::AllocateMemory(bool zero_initialize_memory) {
  DCHECK_EQ(storage_type_, STORAGE_OWNED_MEMORY);
  static_assert(0 == kYPlane, "y plane data must be index 0");

  std::vector<size_t> plane_size = CalculatePlaneSize();
  size_t total_buffer_size = layout_.GetTotalBufferSize();
  // If caller does not provide buffer layout, it uses sum of calculated color
  // planes' size as buffer size VideoFrame needs to allocate.
  if (total_buffer_size == 0) {
    total_buffer_size =
        std::accumulate(plane_size.begin(), plane_size.end(), 0u);
  }

  uint8_t* data = reinterpret_cast<uint8_t*>(
      base::AlignedAlloc(total_buffer_size, kFrameAddressAlignment));
  if (zero_initialize_memory) {
    memset(data, 0, total_buffer_size);
  }
  AddDestructionObserver(base::BindOnce(&base::AlignedFree, data));

  // Note that if layout.buffer_sizes is specified, color planes' layout is the
  // same as buffers'. See CalculatePlaneSize() for detail.
  for (size_t plane = 0, offset = 0; plane < NumPlanes(format()); ++plane) {
    data_[plane] = data + offset;
    offset += plane_size[plane];
  }
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
      strides.push_back(RoundUp(RowBytes(plane, format, coded_size.width()),
                                kFrameAddressAlignment));
    }
  }
  return strides;
}

std::vector<size_t> VideoFrame::CalculatePlaneSize() const {
  const size_t num_planes = NumPlanes(format());
  const size_t num_buffers = layout_.num_buffers();
  const bool buffer_equals_plane = num_buffers == num_planes;
  const bool buffer_assigned = layout_.GetTotalBufferSize() > 0;

  // We have three cases for plane size mapping:
  // 1) buffer size assigned, and #buffers == #planes: use buffers' size as
  //    color planes' size.
  // 2) buffer size unassigned: use legacy calculation formula.
  // 3) buffer size assigned, and #buffers < #planes: map first B-1 buffers'
  //    size to first B-1 color planes. And for the rest color planes' size,
  //    fallback to use legacy calculation formula.
  // The reason to use buffer size (if available) as color plane size is that
  // color plane size is used to calculate each plane's starting address.
  // For caller who already specify a buffer for each plane, use buffer size
  // to calculate buffer/plane head address is the trivial choice.
  if (buffer_equals_plane && buffer_assigned) {
    return layout_.buffer_sizes();
  }

  size_t mappable_buffers = 0;
  if (buffer_assigned)
    mappable_buffers = num_buffers - (buffer_equals_plane ? 0 : 1);

  std::vector<size_t> plane_size;
  for (size_t plane = 0; plane < num_planes; ++plane) {
    if (plane < mappable_buffers) {
      DCHECK_LT(plane, num_buffers);
      plane_size.push_back(layout_.buffer_sizes()[plane]);
    } else {
      // These values were chosen to mirror ffmpeg's get_video_buffer().
      // TODO(dalecurtis): This should be configurable; eventually ffmpeg wants
      // us to use av_cpu_max_align(), but... for now, they just hard-code 32.
      const size_t height = RoundUp(rows(plane), kFrameAddressAlignment);
      const size_t width = std::abs(stride(plane));
      plane_size.push_back(width * height);
    }
  }
  if (num_planes > 1 && mappable_buffers < num_planes) {
    // The extra line of UV being allocated is because h264 chroma MC
    // overreads by one line in some cases, see libavcodec/utils.c:
    // avcodec_align_dimensions2() and libavcodec/x86/h264_chromamc.asm:
    // put_h264_chroma_mc4_ssse3().
    DCHECK(IsValidPlane(kUPlane, format()));
    plane_size.back() += std::abs(stride(kUPlane)) + kFrameSizePadding;
  }
  return plane_size;
}

}  // namespace media
