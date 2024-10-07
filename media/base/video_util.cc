// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/base/video_util.h"

#include <cmath>

#include "base/bits.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/numerics/safe_math.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/raster_interface.h"
#include "gpu/command_buffer/common/capabilities.h"
#include "media/base/limits.h"
#include "media/base/video_frame.h"
#include "media/base/video_frame_pool.h"
#include "media/base/video_types.h"
#include "media/base/wait_and_replace_sync_token_client.h"
#include "third_party/libyuv/include/libyuv.h"
#include "third_party/skia/include/core/SkAlphaType.h"
#include "third_party/skia/include/core/SkColorType.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkPixmap.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkYUVAPixmaps.h"
#include "third_party/skia/include/gpu/GpuTypes.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace media {

namespace {

// Helper to apply padding to the region outside visible rect up to the coded
// size with the repeated last column / row of the visible rect.
void FillRegionOutsideVisibleRect(uint8_t* data,
                                  size_t stride,
                                  const gfx::Size& coded_size,
                                  const gfx::Size& visible_size) {
  if (visible_size.IsEmpty()) {
    if (!coded_size.IsEmpty())
      memset(data, 0, coded_size.height() * stride);
    return;
  }

  const int coded_width = coded_size.width();
  if (visible_size.width() < coded_width) {
    const int pad_length = coded_width - visible_size.width();
    uint8_t* dst = data + visible_size.width();
    for (int i = 0; i < visible_size.height(); ++i, dst += stride)
      memset(dst, *(dst - 1), pad_length);
  }

  if (visible_size.height() < coded_size.height()) {
    uint8_t* dst = data + visible_size.height() * stride;
    uint8_t* src = dst - stride;
    for (int i = visible_size.height(); i < coded_size.height();
         ++i, dst += stride)
      memcpy(dst, src, coded_width);
  }
}

VideoPixelFormat ReadbackFormat(const VideoFrame& frame) {
  // The |frame|.BitDepth() restriction is to avoid treating a P010LE frame as a
  // low-bit depth frame.
  if (frame.RequiresExternalSampler() && frame.BitDepth() == 8u)
    return PIXEL_FORMAT_XRGB;

  switch (frame.format()) {
    case PIXEL_FORMAT_I420:
    case PIXEL_FORMAT_I420A:
    case PIXEL_FORMAT_I422:
    case PIXEL_FORMAT_I444:
    case PIXEL_FORMAT_ARGB:
    case PIXEL_FORMAT_XRGB:
    case PIXEL_FORMAT_ABGR:
    case PIXEL_FORMAT_XBGR:
    case PIXEL_FORMAT_NV12:
    case PIXEL_FORMAT_NV16:
    case PIXEL_FORMAT_NV24:
    case PIXEL_FORMAT_NV12A:
      return frame.format();
    default:
      // Currently unsupported.
      return PIXEL_FORMAT_UNKNOWN;
  }
}

bool ReadbackTexturePlaneToMemorySyncOOP(const VideoFrame& src_frame,
                                         size_t src_plane,
                                         gfx::Rect& src_rect,
                                         uint8_t* dest_pixels,
                                         size_t dest_stride,
                                         gpu::raster::RasterInterface* ri) {
  VideoPixelFormat format = ReadbackFormat(src_frame);
  if (format == PIXEL_FORMAT_UNKNOWN) {
    DLOG(ERROR) << "Readback is not possible for this frame: "
                << src_frame.AsHumanReadableString();
    return false;
  }

  bool has_alpha = !IsOpaque(format);
  SkColorType sk_color_type = SkColorTypeForPlane(format, src_plane);
  SkAlphaType sk_alpha_type =
      has_alpha ? kUnpremul_SkAlphaType : kOpaque_SkAlphaType;

  auto info = SkImageInfo::Make(src_rect.width(), src_rect.height(),
                                sk_color_type, sk_alpha_type);

  // Perform readback passing the appropriate `src_plane` for the mailbox.
  auto mailbox = src_frame.shared_image()->mailbox();
  auto sync_token = src_frame.acquire_sync_token();
  ri->WaitSyncTokenCHROMIUM(sync_token.GetConstData());
  bool result =
      ri->ReadbackImagePixels(mailbox, info, dest_stride, src_rect.x(),
                              src_rect.y(), src_plane, dest_pixels);

  return result && ri->GetGraphicsResetStatusKHR() == GL_NO_ERROR &&
         ri->GetError() == GL_NO_ERROR;
}

void LetterboxPlane(const gfx::Rect& view_area_in_bytes,
                    uint8_t* ptr,
                    int rows,
                    int row_bytes,
                    int stride,
                    int bytes_per_element,
                    uint8_t fill_byte) {
  if (view_area_in_bytes.IsEmpty()) {
    libyuv::SetPlane(ptr, stride, row_bytes, rows, fill_byte);
    return;
  }

  if (view_area_in_bytes.y() > 0) {
    libyuv::SetPlane(ptr, stride, row_bytes, view_area_in_bytes.y(), fill_byte);
    ptr += stride * view_area_in_bytes.y();
  }

  if (view_area_in_bytes.width() < row_bytes) {
    if (view_area_in_bytes.x() > 0) {
      libyuv::SetPlane(ptr, stride, view_area_in_bytes.x(),
                       view_area_in_bytes.height(), fill_byte);
    }
    if (view_area_in_bytes.right() < row_bytes) {
      libyuv::SetPlane(ptr + view_area_in_bytes.right(), stride,
                       row_bytes - view_area_in_bytes.right(),
                       view_area_in_bytes.height(), fill_byte);
    }
  }

  ptr += stride * view_area_in_bytes.height();

  if (view_area_in_bytes.bottom() < rows) {
    libyuv::SetPlane(ptr, stride, row_bytes, rows - view_area_in_bytes.bottom(),
                     fill_byte);
  }
}

void LetterboxPlane(VideoFrame* frame,
                    int plane,
                    uint8_t* ptr,
                    const gfx::Rect& view_area_in_pixels,
                    uint8_t fill_byte) {
  const int rows = frame->rows(plane);
  const int row_bytes = frame->row_bytes(plane);
  const int stride = frame->stride(plane);
  const int bytes_per_element =
      VideoFrame::BytesPerElement(frame->format(), plane);

  gfx::Rect view_area_in_bytes(view_area_in_pixels.x() * bytes_per_element,
                               view_area_in_pixels.y(),
                               view_area_in_pixels.width() * bytes_per_element,
                               view_area_in_pixels.height());

  CHECK_GE(stride, row_bytes);
  CHECK_GE(view_area_in_bytes.x(), 0);
  CHECK_GE(view_area_in_bytes.y(), 0);
  CHECK_LE(view_area_in_bytes.right(), row_bytes);
  CHECK_LE(view_area_in_bytes.bottom(), rows);

  LetterboxPlane(view_area_in_bytes, ptr, rows, row_bytes, stride,
                 bytes_per_element, fill_byte);
}

// Helper for `LetterboxVideoFrame()`, assumes that if |frame| is GMB-backed,
// the GpuMemoryBuffer is already mapped (via a call to `Map()`).
void LetterboxPlane(VideoFrame* frame,
                    VideoFrame::ScopedMapping* scoped_mapping,
                    int plane,
                    const gfx::Rect& view_area_in_pixels,
                    uint8_t fill_byte) {
  uint8_t* ptr = nullptr;
  if (frame->IsMappable()) {
    ptr = frame->writable_data(plane);
  } else if (scoped_mapping) {
    ptr = scoped_mapping->Memory(plane);
  }
  CHECK(ptr);

  LetterboxPlane(frame, plane, ptr, view_area_in_pixels, fill_byte);
}

void ProcessAsyncMappingResult(
    scoped_refptr<VideoFrame> video_frame,
    base::OnceCallback<void(scoped_refptr<VideoFrame>)> result_cb,
    std::unique_ptr<VideoFrame::ScopedMapping> scoped_mapping) {
  CHECK(video_frame);
  if (!scoped_mapping) {
    std::move(result_cb).Run(nullptr);
    return;
  }

  const size_t num_planes = VideoFrame::NumPlanes(video_frame->format());
  uint8_t* plane_addrs[VideoFrame::kMaxPlanes] = {};
  for (size_t i = 0; i < num_planes; i++) {
    plane_addrs[i] = scoped_mapping->Memory(i);
  }

  auto mapped_frame = VideoFrame::WrapExternalYuvDataWithLayout(
      video_frame->layout(), video_frame->visible_rect(),
      video_frame->natural_size(), plane_addrs[0], plane_addrs[1],
      plane_addrs[2], video_frame->timestamp());

  if (!mapped_frame) {
    std::move(result_cb).Run(nullptr);
    return;
  }

  mapped_frame->set_color_space(video_frame->ColorSpace());
  mapped_frame->metadata().MergeMetadataFrom(video_frame->metadata());

  // Pass |video_frame| so that it outlives |mapped_frame| and the mapped buffer
  // is unmapped on destruction.
  mapped_frame->AddDestructionObserver(base::BindOnce(
      [](scoped_refptr<VideoFrame> frame,
         std::unique_ptr<VideoFrame::ScopedMapping> scoped_mapping) {
        CHECK(scoped_mapping);
        // The VideoFrame::ScopedMapping must be destroyed before the
        // FrameResource that produced it in order to avoid dangling pointers.
        scoped_mapping.reset();
      },
      std::move(video_frame), std::move(scoped_mapping)));
  std::move(result_cb).Run(std::move(mapped_frame));
}

}  // namespace

void FillYUV(VideoFrame* frame, uint8_t y, uint8_t u, uint8_t v) {
  libyuv::I420Rect(frame->writable_data(VideoFrame::Plane::kY),
                   frame->stride(VideoFrame::Plane::kY),
                   frame->writable_data(VideoFrame::Plane::kU),
                   frame->stride(VideoFrame::Plane::kU),
                   frame->writable_data(VideoFrame::Plane::kV),
                   frame->stride(VideoFrame::Plane::kV), 0, 0,
                   frame->coded_size().width(), frame->coded_size().height(), y,
                   u, v);
}

void FillYUVA(VideoFrame* frame, uint8_t y, uint8_t u, uint8_t v, uint8_t a) {
  // Fill Y, U and V planes.
  FillYUV(frame, y, u, v);

  // Fill the A plane.
  libyuv::SetPlane(frame->writable_data(VideoFrame::Plane::kA),
                   frame->stride(VideoFrame::Plane::kA),
                   frame->row_bytes(VideoFrame::Plane::kA),
                   frame->rows(VideoFrame::Plane::kA), a);
}

void LetterboxVideoFrame(VideoFrame* frame, const gfx::Rect& view_area) {
  std::unique_ptr<VideoFrame::ScopedMapping> scoped_mapping;
  if (!frame->IsMappable() &&
      frame->storage_type() == media::VideoFrame::STORAGE_GPU_MEMORY_BUFFER) {
    scoped_mapping = frame->MapGMBOrSharedImage();
    CHECK(scoped_mapping);
  }

  switch (frame->format()) {
    case PIXEL_FORMAT_ARGB:
      LetterboxPlane(frame, scoped_mapping.get(), VideoFrame::Plane::kARGB,
                     view_area, 0x00);
      break;
    case PIXEL_FORMAT_YV12:
    case PIXEL_FORMAT_I420: {
      DCHECK(!(view_area.x() & 1));
      DCHECK(!(view_area.y() & 1));
      DCHECK(!(view_area.width() & 1));
      DCHECK(!(view_area.height() & 1));

      LetterboxPlane(frame, scoped_mapping.get(), VideoFrame::Plane::kY,
                     view_area, 0x00);
      gfx::Rect half_view_area(view_area.x() / 2, view_area.y() / 2,
                               view_area.width() / 2, view_area.height() / 2);
      LetterboxPlane(frame, scoped_mapping.get(), VideoFrame::Plane::kU,
                     half_view_area, 0x80);
      LetterboxPlane(frame, scoped_mapping.get(), VideoFrame::Plane::kV,
                     half_view_area, 0x80);
      break;
    }
    case PIXEL_FORMAT_NV12: {
      DCHECK(!(view_area.x() & 1));
      DCHECK(!(view_area.y() & 1));
      DCHECK(!(view_area.width() & 1));
      DCHECK(!(view_area.height() & 1));

      LetterboxPlane(frame, scoped_mapping.get(), VideoFrame::Plane::kY,
                     view_area, 0x00);
      gfx::Rect half_view_area(view_area.x() / 2, view_area.y() / 2,
                               view_area.width() / 2, view_area.height() / 2);

      LetterboxPlane(frame, scoped_mapping.get(), VideoFrame::Plane::kUV,
                     half_view_area, 0x80);
      break;
    }
    case PIXEL_FORMAT_NV12A: {
      DCHECK(!(view_area.x() & 1));
      DCHECK(!(view_area.y() & 1));
      DCHECK(!(view_area.width() & 1));
      DCHECK(!(view_area.height() & 1));

      LetterboxPlane(frame, scoped_mapping.get(), VideoFrame::Plane::kY,
                     view_area, 0x00);
      gfx::Rect half_view_area(view_area.x() / 2, view_area.y() / 2,
                               view_area.width() / 2, view_area.height() / 2);
      LetterboxPlane(frame, scoped_mapping.get(), VideoFrame::Plane::kUV,
                     half_view_area, 0x80);
      LetterboxPlane(frame, scoped_mapping.get(),
                     VideoFrame::Plane::kATriPlanar, view_area, 0x00);
      break;
    }
    default:
      NOTREACHED_IN_MIGRATION();
  }
}

void RotatePlaneByPixels(const uint8_t* src,
                         uint8_t* dest,
                         int width,
                         int height,
                         int rotation,  // Clockwise.
                         bool flip_vert,
                         bool flip_horiz) {
  DCHECK((width > 0) && (height > 0) &&
         ((width & 1) == 0) && ((height & 1) == 0) &&
         (rotation >= 0) && (rotation < 360) && (rotation % 90 == 0));

  // Consolidate cases. Only 0 and 90 are left.
  if (rotation == 180 || rotation == 270) {
    rotation -= 180;
    flip_vert = !flip_vert;
    flip_horiz = !flip_horiz;
  }

  int num_rows = height;
  int num_cols = width;
  int src_stride = width;
  // During pixel copying, the corresponding incremental of dest pointer
  // when src pointer moves to next row.
  int dest_row_step = width;
  // During pixel copying, the corresponding incremental of dest pointer
  // when src pointer moves to next column.
  int dest_col_step = 1;

  if (rotation == 0) {
    if (flip_horiz) {
      // Use pixel copying.
      dest_col_step = -1;
      if (flip_vert) {
        // Rotation 180.
        dest_row_step = -width;
        dest += height * width - 1;
      } else {
        dest += width - 1;
      }
    } else {
      if (flip_vert) {
        // Fast copy by rows.
        dest += width * (height - 1);
        for (int row = 0; row < height; ++row) {
          memcpy(dest, src, width);
          src += width;
          dest -= width;
        }
      } else {
        memcpy(dest, src, width * height);
      }
      return;
    }
  } else if (rotation == 90) {
    int offset;
    if (width > height) {
      offset = (width - height) / 2;
      src += offset;
      num_rows = num_cols = height;
    } else {
      offset = (height - width) / 2;
      src += width * offset;
      num_rows = num_cols = width;
    }

    dest_col_step = (flip_vert ? -width : width);
    dest_row_step = (flip_horiz ? 1 : -1);
    if (flip_horiz) {
      if (flip_vert) {
        dest += (width > height ? width * (height - 1) + offset :
                                  width * (height - offset - 1));
      } else {
        dest += (width > height ? offset : width * offset);
      }
    } else {
      if (flip_vert) {
        dest += (width > height ?  width * height - offset - 1 :
                                   width * (height - offset) - 1);
      } else {
        dest += (width > height ? width - offset - 1 :
                                  width * (offset + 1) - 1);
      }
    }
  } else {
    NOTREACHED_IN_MIGRATION();
  }

  // Copy pixels.
  for (int row = 0; row < num_rows; ++row) {
    const uint8_t* src_ptr = src;
    uint8_t* dest_ptr = dest;
    for (int col = 0; col < num_cols; ++col) {
      *dest_ptr = *src_ptr++;
      dest_ptr += dest_col_step;
    }
    src += src_stride;
    dest += dest_row_step;
  }
}

// Helper function to return |a| divided by |b|, rounded to the nearest integer.
static int RoundedDivision(int64_t a, int b) {
  DCHECK_GE(a, 0);
  DCHECK_GT(b, 0);
  base::CheckedNumeric<uint64_t> result(a);
  result += b / 2;
  result /= b;
  return base::ValueOrDieForType<int>(result);
}

// Common logic for the letterboxing and scale-within/scale-encompassing
// functions.  Scales |size| to either fit within or encompass |target|,
// depending on whether |fit_within_target| is true.
static gfx::Size ScaleSizeToTarget(const gfx::Size& size,
                                   const gfx::Size& target,
                                   bool fit_within_target) {
  if (size.IsEmpty())
    return gfx::Size();  // Corner case: Aspect ratio is undefined.

  const int64_t x = static_cast<int64_t>(size.width()) * target.height();
  const int64_t y = static_cast<int64_t>(size.height()) * target.width();
  const bool use_target_width = fit_within_target ? (y < x) : (x < y);
  return use_target_width ?
      gfx::Size(target.width(), RoundedDivision(y, size.width())) :
      gfx::Size(RoundedDivision(x, size.height()), target.height());
}

gfx::Rect ComputeLetterboxRegion(const gfx::Rect& bounds,
                                 const gfx::Size& content) {
  // If |content| has an undefined aspect ratio, let's not try to divide by
  // zero.
  if (content.IsEmpty())
    return gfx::Rect();

  gfx::Rect result = bounds;
  result.ClampToCenteredSize(ScaleSizeToTarget(content, bounds.size(), true));
  return result;
}

gfx::Rect ComputeLetterboxRegionForI420(const gfx::Rect& bounds,
                                        const gfx::Size& content) {
  DCHECK_EQ(bounds.x() % 2, 0);
  DCHECK_EQ(bounds.y() % 2, 0);
  DCHECK_EQ(bounds.width() % 2, 0);
  DCHECK_EQ(bounds.height() % 2, 0);

  gfx::Rect result = ComputeLetterboxRegion(bounds, content);

  if (result.x() & 1) {
    // This is always legal since bounds.x() was even and result.x() must always
    // be greater or equal to bounds.x().
    result.set_x(result.x() - 1);

    // The result.x() was nudged to the left, so if the width is odd, it should
    // be perfectly legal to nudge it up by one to make it even.
    if (result.width() & 1)
      result.set_width(result.width() + 1);
  } else /* if (result.x() is even) */ {
    if (result.width() & 1)
      result.set_width(result.width() - 1);
  }

  if (result.y() & 1) {
    // These operations are legal for the same reasons mentioned above for
    // result.x().
    result.set_y(result.y() - 1);
    if (result.height() & 1)
      result.set_height(result.height() + 1);
  } else /* if (result.y() is even) */ {
    if (result.height() & 1)
      result.set_height(result.height() - 1);
  }

  return result;
}

gfx::Rect MinimallyShrinkRectForI420(const gfx::Rect& rect) {
  constexpr int kMinDimension = -1 * limits::kMaxDimension;
  DCHECK(gfx::Rect(kMinDimension, kMinDimension, limits::kMaxDimension * 2,
                   limits::kMaxDimension * 2)
             .Contains(rect));

  const auto positive_mod = [](int a, int b) { return (a % b + b) % b; };

  const int left = rect.x() + positive_mod(rect.x(), 2);
  const int top = rect.y() + positive_mod(rect.y(), 2);
  const int right = rect.right() - (rect.right() % 2);
  const int bottom = rect.bottom() - (rect.bottom() % 2);

  return gfx::Rect(left, top, std::max(0, right - left),
                   std::max(0, bottom - top));
}

gfx::Size ScaleSizeToFitWithinTarget(const gfx::Size& size,
                                     const gfx::Size& target) {
  return ScaleSizeToTarget(size, target, true);
}

gfx::Size ScaleSizeToEncompassTarget(const gfx::Size& size,
                                     const gfx::Size& target) {
  return ScaleSizeToTarget(size, target, false);
}

gfx::Rect CropSizeForScalingToTarget(const gfx::Size& size,
                                     const gfx::Size& target,
                                     size_t alignment) {
  DCHECK_GT(alignment, 0u);
  if (size.IsEmpty() || target.IsEmpty())
    return gfx::Rect();

  gfx::Rect crop(ScaleSizeToFitWithinTarget(target, size));
  crop.set_width(base::checked_cast<int>(base::bits::AlignDown(
      base::checked_cast<size_t>(crop.width()), alignment)));
  crop.set_height(base::checked_cast<int>(base::bits::AlignDown(
      base::checked_cast<size_t>(crop.height()), alignment)));
  crop.set_x(base::checked_cast<int>(base::bits::AlignDown(
      base::checked_cast<size_t>((size.width() - crop.width()) / 2),
      alignment)));
  crop.set_y(base::checked_cast<int>(base::bits::AlignDown(
      base::checked_cast<size_t>((size.height() - crop.height()) / 2),
      alignment)));
  DCHECK(gfx::Rect(size).Contains(crop));
  return crop;
}

gfx::Size GetRectSizeFromOrigin(const gfx::Rect& rect) {
  return gfx::Size(rect.right(), rect.bottom());
}

gfx::Size PadToMatchAspectRatio(const gfx::Size& size,
                                const gfx::Size& target) {
  if (target.IsEmpty())
    return gfx::Size();  // Aspect ratio is undefined.

  const int64_t x = static_cast<int64_t>(size.width()) * target.height();
  const int64_t y = static_cast<int64_t>(size.height()) * target.width();
  if (x < y)
    return gfx::Size(RoundedDivision(y, target.height()), size.height());
  return gfx::Size(size.width(), RoundedDivision(x, target.width()));
}

scoped_refptr<VideoFrame> ConvertToMemoryMappedFrame(
    scoped_refptr<VideoFrame> video_frame) {
  CHECK(video_frame);
  CHECK(video_frame->HasMappableGpuBuffer());

  auto scoped_mapping = video_frame->MapGMBOrSharedImage();
  if (!scoped_mapping) {
    return nullptr;
  }

  const size_t num_planes = VideoFrame::NumPlanes(video_frame->format());
  uint8_t* plane_addrs[VideoFrame::kMaxPlanes] = {};
  for (size_t i = 0; i < num_planes; i++)
    plane_addrs[i] = scoped_mapping->Memory(i);

  auto mapped_frame = VideoFrame::WrapExternalYuvDataWithLayout(
      video_frame->layout(), video_frame->visible_rect(),
      video_frame->natural_size(), plane_addrs[0], plane_addrs[1],
      plane_addrs[2], video_frame->timestamp());

  if (!mapped_frame) {
    return nullptr;
  }

  mapped_frame->set_color_space(video_frame->ColorSpace());
  mapped_frame->metadata().MergeMetadataFrom(video_frame->metadata());

  // Pass |video_frame| so that it outlives |mapped_frame| and the mapped buffer
  // is unmapped on destruction.
  mapped_frame->AddDestructionObserver(base::BindOnce(
      [](scoped_refptr<VideoFrame> frame,
         std::unique_ptr<VideoFrame::ScopedMapping> scoped_mapping) {
        CHECK(scoped_mapping);
        // The VideoFrame::ScopedMapping must be destroyed before the
        // FrameResource that produced it in order to avoid dangling pointers.
        scoped_mapping.reset();
      },
      std::move(video_frame), std::move(scoped_mapping)));
  return mapped_frame;
}

void ConvertToMemoryMappedFrameAsync(
    scoped_refptr<VideoFrame> video_frame,
    base::OnceCallback<void(scoped_refptr<VideoFrame>)> result_cb) {
  CHECK(video_frame);
  CHECK(video_frame->HasMappableGpuBuffer());

  video_frame->MapGMBOrSharedImageAsync(base::BindOnce(
      &ProcessAsyncMappingResult, video_frame, std::move(result_cb)));
}

scoped_refptr<VideoFrame> WrapAsI420VideoFrame(
    scoped_refptr<VideoFrame> frame) {
  DCHECK_EQ(VideoFrame::STORAGE_OWNED_MEMORY, frame->storage_type());
  DCHECK_EQ(PIXEL_FORMAT_I420A, frame->format());

  scoped_refptr<VideoFrame> wrapped_frame = VideoFrame::WrapVideoFrame(
      frame, PIXEL_FORMAT_I420, frame->visible_rect(), frame->natural_size());
  return wrapped_frame;
}

bool I420CopyWithPadding(const VideoFrame& src_frame, VideoFrame* dst_frame) {
  if (!dst_frame || !dst_frame->IsMappable())
    return false;

  DCHECK_GE(dst_frame->coded_size().width(), src_frame.visible_rect().width());
  DCHECK_GE(dst_frame->coded_size().height(),
            src_frame.visible_rect().height());
  DCHECK(dst_frame->visible_rect().origin().IsOrigin());

  if (libyuv::I420Copy(src_frame.visible_data(VideoFrame::Plane::kY),
                       src_frame.stride(VideoFrame::Plane::kY),
                       src_frame.visible_data(VideoFrame::Plane::kU),
                       src_frame.stride(VideoFrame::Plane::kU),
                       src_frame.visible_data(VideoFrame::Plane::kV),
                       src_frame.stride(VideoFrame::Plane::kV),
                       dst_frame->writable_data(VideoFrame::Plane::kY),
                       dst_frame->stride(VideoFrame::Plane::kY),
                       dst_frame->writable_data(VideoFrame::Plane::kU),
                       dst_frame->stride(VideoFrame::Plane::kU),
                       dst_frame->writable_data(VideoFrame::Plane::kV),
                       dst_frame->stride(VideoFrame::Plane::kV),
                       src_frame.visible_rect().width(),
                       src_frame.visible_rect().height())) {
    return false;
  }

  // Padding the region outside the visible rect with the repeated last
  // column / row of the visible rect. This can improve the coding efficiency.
  FillRegionOutsideVisibleRect(dst_frame->writable_data(VideoFrame::Plane::kY),
                               dst_frame->stride(VideoFrame::Plane::kY),
                               dst_frame->coded_size(),
                               src_frame.visible_rect().size());
  FillRegionOutsideVisibleRect(
      dst_frame->writable_data(VideoFrame::Plane::kU),
      dst_frame->stride(VideoFrame::Plane::kU),
      VideoFrame::PlaneSize(PIXEL_FORMAT_I420, VideoFrame::Plane::kU,
                            dst_frame->coded_size()),
      VideoFrame::PlaneSize(PIXEL_FORMAT_I420, VideoFrame::Plane::kU,
                            src_frame.visible_rect().size()));
  FillRegionOutsideVisibleRect(
      dst_frame->writable_data(VideoFrame::Plane::kV),
      dst_frame->stride(VideoFrame::Plane::kV),
      VideoFrame::PlaneSize(PIXEL_FORMAT_I420, VideoFrame::Plane::kV,
                            dst_frame->coded_size()),
      VideoFrame::PlaneSize(PIXEL_FORMAT_I420, VideoFrame::Plane::kV,
                            src_frame.visible_rect().size()));

  return true;
}

scoped_refptr<VideoFrame> ReadbackTextureBackedFrameToMemorySync(
    VideoFrame& txt_frame,
    gpu::raster::RasterInterface* ri,
    const gpu::Capabilities& caps,
    VideoFramePool* pool) {
  DCHECK(ri);

  TRACE_EVENT1("media", "ReadbackTextureBackedFrameToMemorySync", "timestamp",
               txt_frame.timestamp());
  VideoPixelFormat format = ReadbackFormat(txt_frame);
  if (format == PIXEL_FORMAT_UNKNOWN) {
    DLOG(ERROR) << "Readback is not possible for this frame: "
                << txt_frame.AsHumanReadableString();
    return nullptr;
  }

  scoped_refptr<VideoFrame> result =
      pool ? pool->CreateFrame(format, txt_frame.coded_size(),
                               txt_frame.visible_rect(),
                               txt_frame.natural_size(), txt_frame.timestamp())
           : VideoFrame::CreateFrame(
                 format, txt_frame.coded_size(), txt_frame.visible_rect(),
                 txt_frame.natural_size(), txt_frame.timestamp());
  result->set_color_space(txt_frame.ColorSpace());
  result->metadata().MergeMetadataFrom(txt_frame.metadata());
  result->metadata().ClearTextureFrameMetadata();

  // NOTE: Iterating over the number of planes of the readback format (rather
  // than `txt_frame`) ensures that frames with external
  // sampling are correctly sampled as a single opaque texture, as
  // ReadbackFormat() returns RGB for such frames.
  size_t planes = VideoFrame::NumPlanes(format);
  for (size_t plane = 0; plane < planes; plane++) {
    gfx::Rect src_rect(0, 0, txt_frame.columns(plane), txt_frame.rows(plane));
    if (!ReadbackTexturePlaneToMemorySync(txt_frame, plane, src_rect,
                                          result->writable_data(plane),
                                          result->stride(plane), ri, caps)) {
      return nullptr;
    }
  }
  return result;
}

bool ReadbackTexturePlaneToMemorySync(VideoFrame& src_frame,
                                      size_t src_plane,
                                      gfx::Rect& src_rect,
                                      uint8_t* dest_pixels,
                                      size_t dest_stride,
                                      gpu::raster::RasterInterface* ri,
                                      const gpu::Capabilities& caps) {
  DCHECK(ri);

  bool result = ReadbackTexturePlaneToMemorySyncOOP(
      src_frame, src_plane, src_rect, dest_pixels, dest_stride, ri);
  WaitAndReplaceSyncTokenClient client(ri);
  src_frame.UpdateReleaseSyncToken(&client);
  return result;
}

// Media pixel format enums have names opposite to their byte order.
// That's why PIXEL_FORMAT_ABGR corresponds to kRGBA_8888_SkColorType
// and so on.
MEDIA_EXPORT SkColorType SkColorTypeForPlane(VideoPixelFormat format,
                                             size_t plane) {
  switch (format) {
    case PIXEL_FORMAT_I420:
    case PIXEL_FORMAT_I420A:
    case PIXEL_FORMAT_I422:
    case PIXEL_FORMAT_I444:
      // kGray_8_SkColorType would make more sense but doesn't work on Windows.
      return kAlpha_8_SkColorType;
    case PIXEL_FORMAT_NV12:
    case PIXEL_FORMAT_NV16:
    case PIXEL_FORMAT_NV24:
      return plane == VideoFrame::Plane::kY ? kAlpha_8_SkColorType
                                            : kR8G8_unorm_SkColorType;
    case PIXEL_FORMAT_NV12A:
      return plane == VideoFrame::Plane::kY ||
                     plane == VideoFrame::Plane::kATriPlanar
                 ? kAlpha_8_SkColorType
                 : kR8G8_unorm_SkColorType;
    case PIXEL_FORMAT_P010LE:
    case PIXEL_FORMAT_P210LE:
    case PIXEL_FORMAT_P410LE:
      return plane == VideoFrame::Plane::kY ? kA16_unorm_SkColorType
                                            : kR16G16_unorm_SkColorType;
    case PIXEL_FORMAT_XBGR:
    case PIXEL_FORMAT_ABGR:
      return kRGBA_8888_SkColorType;
    case PIXEL_FORMAT_XRGB:
    case PIXEL_FORMAT_ARGB:
      return kBGRA_8888_SkColorType;
    default:
      NOTREACHED();
  }
}

MEDIA_EXPORT VideoPixelFormat
VideoPixelFormatFromSkColorType(SkColorType sk_color_type, bool is_opaque) {
  switch (sk_color_type) {
    case kRGBA_8888_SkColorType:
      return is_opaque ? PIXEL_FORMAT_XBGR : PIXEL_FORMAT_ABGR;
    case kBGRA_8888_SkColorType:
      return is_opaque ? PIXEL_FORMAT_XRGB : PIXEL_FORMAT_ARGB;
    default:
      // TODO(crbug.com/40686604): Add F16 support.
      return PIXEL_FORMAT_UNKNOWN;
  }
}

scoped_refptr<VideoFrame> CreateFromSkImage(sk_sp<SkImage> sk_image,
                                            const gfx::Rect& visible_rect,
                                            const gfx::Size& natural_size,
                                            base::TimeDelta timestamp,
                                            bool force_opaque) {
  DCHECK(!sk_image->isTextureBacked());

  // A given SkImage may not exist until it's rasterized.
  if (sk_image->isLazyGenerated())
    sk_image = sk_image->makeRasterImage();

  const auto format = VideoPixelFormatFromSkColorType(
      sk_image->colorType(), sk_image->isOpaque() || force_opaque);
  if (VideoFrameLayout::NumPlanes(format) != 1) {
    DLOG(ERROR) << "Invalid SkColorType for CreateFromSkImage";
    return nullptr;
  }

  SkPixmap pm;
  const bool peek_result = sk_image->peekPixels(&pm);
  DCHECK(peek_result);

  auto coded_size = gfx::Size(sk_image->width(), sk_image->height());
  auto layout = VideoFrameLayout::CreateWithStrides(
      format, coded_size, std::vector<int32_t>(1, pm.rowBytes()));
  if (!layout)
    return nullptr;

  auto frame = VideoFrame::WrapExternalDataWithLayout(
      *layout, visible_rect, natural_size,
      // TODO(crbug.com/40162403): We should be able to wrap readonly memory in
      // a VideoFrame instead of using writable_addr() here.
      reinterpret_cast<uint8_t*>(pm.writable_addr()), pm.computeByteSize(),
      timestamp);
  if (!frame)
    return nullptr;

  frame->AddDestructionObserver(
      base::DoNothingWithBoundArgs(std::move(sk_image)));
  return frame;
}

std::tuple<SkYUVAInfo::PlaneConfig, SkYUVAInfo::Subsampling>
VideoPixelFormatToSkiaValues(VideoPixelFormat video_format) {
  // To expand support for additional VideoFormats expand this switch.
  switch (video_format) {
    case PIXEL_FORMAT_NV12:
    case PIXEL_FORMAT_P010LE:
      return {SkYUVAInfo::PlaneConfig::kY_UV, SkYUVAInfo::Subsampling::k420};
    case PIXEL_FORMAT_NV16:
    case PIXEL_FORMAT_P210LE:
      return {SkYUVAInfo::PlaneConfig::kY_UV, SkYUVAInfo::Subsampling::k422};
    case PIXEL_FORMAT_NV24:
    case PIXEL_FORMAT_P410LE:
      return {SkYUVAInfo::PlaneConfig::kY_UV, SkYUVAInfo::Subsampling::k444};
    case PIXEL_FORMAT_NV12A:
      return {SkYUVAInfo::PlaneConfig::kY_UV_A, SkYUVAInfo::Subsampling::k420};
    case PIXEL_FORMAT_I420:
      return {SkYUVAInfo::PlaneConfig::kY_U_V, SkYUVAInfo::Subsampling::k420};
    case PIXEL_FORMAT_I420A:
      return {SkYUVAInfo::PlaneConfig::kY_U_V_A, SkYUVAInfo::Subsampling::k420};
    default:
      return {SkYUVAInfo::PlaneConfig::kUnknown,
              SkYUVAInfo::Subsampling::kUnknown};
  }
}

const libyuv::YuvConstants* GetYuvContantsForColorSpace(
    SkYUVColorSpace cs,
    bool output_argb_matrix) {
#define YUV_MATRIX(matrix) (output_argb_matrix ? matrix : matrix##VU)
  switch (cs) {
    case kJPEG_Full_SkYUVColorSpace:
      return &YUV_MATRIX(libyuv::kYuvJPEGConstants);
    case kRec601_Limited_SkYUVColorSpace:
      return &YUV_MATRIX(libyuv::kYuvI601Constants);
    case kRec709_Full_SkYUVColorSpace:
      return &YUV_MATRIX(libyuv::kYuvF709Constants);
    case kRec709_Limited_SkYUVColorSpace:
      return &YUV_MATRIX(libyuv::kYuvH709Constants);
    case kBT2020_8bit_Full_SkYUVColorSpace:
    case kBT2020_10bit_Full_SkYUVColorSpace:
    case kBT2020_12bit_Full_SkYUVColorSpace:
    case kBT2020_16bit_Full_SkYUVColorSpace:
      return &YUV_MATRIX(libyuv::kYuvV2020Constants);
    case kBT2020_8bit_Limited_SkYUVColorSpace:
    case kBT2020_10bit_Limited_SkYUVColorSpace:
    case kBT2020_12bit_Limited_SkYUVColorSpace:
    case kBT2020_16bit_Limited_SkYUVColorSpace:
      return &YUV_MATRIX(libyuv::kYuv2020Constants);
    case kFCC_Full_SkYUVColorSpace:
    case kFCC_Limited_SkYUVColorSpace:
    case kSMPTE240_Full_SkYUVColorSpace:
    case kSMPTE240_Limited_SkYUVColorSpace:
    case kYDZDX_Full_SkYUVColorSpace:
    case kYDZDX_Limited_SkYUVColorSpace:
    case kGBR_Full_SkYUVColorSpace:
    case kGBR_Limited_SkYUVColorSpace:
    case kYCgCo_8bit_Full_SkYUVColorSpace:
    case kYCgCo_8bit_Limited_SkYUVColorSpace:
    case kYCgCo_10bit_Full_SkYUVColorSpace:
    case kYCgCo_10bit_Limited_SkYUVColorSpace:
    case kYCgCo_12bit_Full_SkYUVColorSpace:
    case kYCgCo_12bit_Limited_SkYUVColorSpace:
    case kYCgCo_16bit_Full_SkYUVColorSpace:
    case kYCgCo_16bit_Limited_SkYUVColorSpace:
      // TODO(crbug.com/41486014): Return color space for default
      // kRec601_SkYUVColorSpace as libyuv does not have FCC, SMPTE240M, YDZDX,
      // GBR, YCgCo equivalent support.
      return &YUV_MATRIX(libyuv::kYuvI601Constants);
    case kIdentity_SkYUVColorSpace:
      NOTREACHED();
  };
#undef YUV_MATRIX
}

}  // namespace media
