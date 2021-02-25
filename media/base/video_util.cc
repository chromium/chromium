// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/video_util.h"

#include <cmath>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/check_op.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/numerics/safe_math.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/raster_interface.h"
#include "media/base/status_codes.h"
#include "media/base/video_frame.h"
#include "media/base/video_frame_pool.h"
#include "third_party/libyuv/include/libyuv.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/core/SkYUVAPixmaps.h"
#include "third_party/skia/include/gpu/GrDirectContext.h"
#include "third_party/skia/include/gpu/gl/GrGLTypes.h"
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
      std::memset(dst, *(dst - 1), pad_length);
  }

  if (visible_size.height() < coded_size.height()) {
    uint8_t* dst = data + visible_size.height() * stride;
    uint8_t* src = dst - stride;
    for (int i = visible_size.height(); i < coded_size.height();
         ++i, dst += stride)
      std::memcpy(dst, src, coded_width);
  }
}

std::pair<SkColorType, GrGLenum> GetSkiaAndGlColorTypesForPlane(
    VideoPixelFormat format,
    size_t plane) {
  // TODO(eugene): There is some strange channel switch during RGB readback.
  // When frame's pixel format matches GL and Skia color types we get reversed
  // channels. But why?
  switch (format) {
    case PIXEL_FORMAT_NV12:
      if (plane == VideoFrame::kUVPlane)
        return {kR8G8_unorm_SkColorType, GL_RG8_EXT};
      if (plane == VideoFrame::kYPlane)
        return {kAlpha_8_SkColorType, GL_R8_EXT};
      break;
    case PIXEL_FORMAT_XBGR:
      if (plane == VideoFrame::kARGBPlane)
        return {kRGBA_8888_SkColorType, GL_RGBA8_OES};
      break;
    case PIXEL_FORMAT_ABGR:
      if (plane == VideoFrame::kARGBPlane)
        return {kRGBA_8888_SkColorType, GL_RGBA8_OES};
      break;
    case PIXEL_FORMAT_XRGB:
      if (plane == VideoFrame::kARGBPlane)
        return {kBGRA_8888_SkColorType, GL_BGRA8_EXT};
      break;
    case PIXEL_FORMAT_ARGB:
      if (plane == VideoFrame::kARGBPlane)
        return {kBGRA_8888_SkColorType, GL_BGRA8_EXT};
      break;
    default:
      break;
  }
  NOTREACHED();
  return {kUnknown_SkColorType, 0};
}

scoped_refptr<VideoFrame> ReadbackTextureBackedFrameToMemorySyncGLES(
    const VideoFrame& txt_frame,
    gpu::raster::RasterInterface* ri,
    GrDirectContext* gr_context,
    VideoFramePool* pool) {
  DCHECK(gr_context);

  if (txt_frame.NumTextures() > 2 || txt_frame.NumTextures() < 1) {
    DLOG(ERROR) << "Readback is not possible for this frame: "
                << txt_frame.AsHumanReadableString();
    return nullptr;
  }

  VideoPixelFormat result_format = txt_frame.format();
  if (txt_frame.NumTextures() == 1 && result_format == PIXEL_FORMAT_NV12) {
    // Even though |txt_frame| format is NV12 and it is NV12 in GPU memory,
    // the texture is a RGB view that is produced by a shader on the fly.
    // So we currently we currently can only read it back as RGB.
    result_format = PIXEL_FORMAT_ARGB;
  }

  scoped_refptr<VideoFrame> result =
      pool
          ? pool->CreateFrame(result_format, txt_frame.coded_size(),
                              txt_frame.visible_rect(),
                              txt_frame.natural_size(), txt_frame.timestamp())
          : VideoFrame::CreateFrame(
                result_format, txt_frame.coded_size(), txt_frame.visible_rect(),
                txt_frame.natural_size(), txt_frame.timestamp());
  result->set_color_space(txt_frame.ColorSpace());
  result->metadata().MergeMetadataFrom(txt_frame.metadata());

  size_t planes = VideoFrame::NumPlanes(result->format());
  for (size_t plane = 0; plane < planes; plane++) {
    const gpu::MailboxHolder& holder = txt_frame.mailbox_holder(plane);
    if (holder.mailbox.IsZero())
      return nullptr;
    ri->WaitSyncTokenCHROMIUM(holder.sync_token.GetConstData());

    int width = VideoFrame::Columns(plane, result->format(),
                                    result->coded_size().width());
    int height = result->rows(plane);

    auto texture_id = ri->CreateAndConsumeForGpuRaster(holder.mailbox);
    if (holder.mailbox.IsSharedImage()) {
      ri->BeginSharedImageAccessDirectCHROMIUM(
          texture_id, GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM);
    }

    auto cleanup_fn = [](GLuint texture_id, bool shared,
                         gpu::raster::RasterInterface* ri) {
      if (shared)
        ri->EndSharedImageAccessDirectCHROMIUM(texture_id);
      ri->DeleteGpuRasterTexture(texture_id);
    };
    base::ScopedClosureRunner cleanup(base::BindOnce(
        cleanup_fn, texture_id, holder.mailbox.IsSharedImage(), ri));

    GrGLenum texture_format;
    SkColorType sk_color_type;
    std::tie(sk_color_type, texture_format) =
        GetSkiaAndGlColorTypesForPlane(result->format(), plane);
    GrGLTextureInfo gl_texture_info;
    gl_texture_info.fID = texture_id;
    gl_texture_info.fTarget = holder.texture_target;
    gl_texture_info.fFormat = texture_format;

    GrBackendTexture texture(width, height, GrMipMapped::kNo, gl_texture_info);
    auto image = SkImage::MakeFromTexture(
        gr_context, texture, kTopLeft_GrSurfaceOrigin, sk_color_type,
        kOpaque_SkAlphaType, nullptr /* colorSpace */);

    if (!image) {
      DLOG(ERROR) << "Can't create SkImage from texture!"
                  << " plane:" << plane;
      return nullptr;
    }

    auto info =
        SkImageInfo::Make(width, height, sk_color_type, kOpaque_SkAlphaType);
    SkPixmap pixmap(info, result->data(plane), result->row_bytes(plane));
    if (!image->readPixels(gr_context, pixmap, 0, 0,
                           SkImage::kDisallow_CachingHint)) {
      DLOG(ERROR) << "Plane readback failed."
                  << " plane:" << plane << " width: " << width
                  << " height: " << height
                  << " minRowBytes: " << info.minRowBytes();
      return nullptr;
    }
  }

  return result;
}

scoped_refptr<VideoFrame> ReadbackTextureBackedFrameToMemorySyncOOP(
    const VideoFrame& txt_frame,
    gpu::raster::RasterInterface* ri,
    VideoFramePool* pool) {
  if (txt_frame.NumTextures() > 2 || txt_frame.NumTextures() < 1) {
    DLOG(ERROR) << "Readback is not possible for this frame: "
                << txt_frame.AsHumanReadableString();
    return nullptr;
  }

  VideoPixelFormat result_format = txt_frame.format();
  if (txt_frame.NumTextures() == 1 && result_format == PIXEL_FORMAT_NV12) {
    // Even though |txt_frame| format is NV12 and it is NV12 in GPU memory,
    // the texture is a RGB view that is produced by a shader on the fly.
    // So we currently we currently can only read it back as RGB.
    result_format = PIXEL_FORMAT_ARGB;
  }

  scoped_refptr<VideoFrame> result =
      pool
          ? pool->CreateFrame(result_format, txt_frame.coded_size(),
                              txt_frame.visible_rect(),
                              txt_frame.natural_size(), txt_frame.timestamp())
          : VideoFrame::CreateFrame(
                result_format, txt_frame.coded_size(), txt_frame.visible_rect(),
                txt_frame.natural_size(), txt_frame.timestamp());
  result->set_color_space(txt_frame.ColorSpace());
  result->metadata().MergeMetadataFrom(txt_frame.metadata());

  size_t planes = VideoFrame::NumPlanes(result->format());
  for (size_t plane = 0; plane < planes; plane++) {
    const gpu::MailboxHolder& holder = txt_frame.mailbox_holder(plane);
    if (holder.mailbox.IsZero()) {
      DLOG(ERROR) << "Can't readback video frame with Zero texture on plane "
                  << plane;
      return nullptr;
    }
    ri->WaitSyncTokenCHROMIUM(holder.sync_token.GetConstData());

    int width = VideoFrame::Columns(plane, result->format(),
                                    result->coded_size().width());
    int height = result->rows(plane);

    GrGLenum texture_format;
    SkColorType sk_color_type;
    std::tie(sk_color_type, texture_format) =
        GetSkiaAndGlColorTypesForPlane(result->format(), plane);

    auto info =
        SkImageInfo::Make(width, height, sk_color_type, kOpaque_SkAlphaType);

    ri->ReadbackImagePixels(holder.mailbox, info, info.minRowBytes(), 0, 0,
                            result->data(plane));
    if (ri->GetError() != GL_NO_ERROR) {
      DLOG(ERROR) << "Plane readback failed."
                  << " plane:" << plane << " width: " << width
                  << " height: " << height
                  << " minRowBytes: " << info.minRowBytes()
                  << " error: " << ri->GetError();
      return nullptr;
    }
  }

  return result;
}

}  // namespace

double GetPixelAspectRatio(const gfx::Rect& visible_rect,
                           const gfx::Size& natural_size) {
  double visible_width = visible_rect.width();
  double visible_height = visible_rect.height();
  double natural_width = natural_size.width();
  double natural_height = natural_size.height();
  return (visible_height * natural_width) / (visible_width * natural_height);
}

gfx::Size GetNaturalSize(const gfx::Rect& visible_rect,
                         double pixel_aspect_ratio) {
  // TODO(sandersd): Also handle conversion back to integers overflowing.
  if (!std::isfinite(pixel_aspect_ratio) || pixel_aspect_ratio <= 0.0)
    return gfx::Size();

  // The HTML spec requires that we always grow a dimension to match aspect
  // ratio, rather than modify just the width:
  // github.com/whatwg/html/commit/2e94aa64fcf9adbd2f70d8c2aecd192c8678e298
  if (pixel_aspect_ratio >= 1.0) {
    return gfx::Size(std::round(visible_rect.width() * pixel_aspect_ratio),
                     visible_rect.height());
  }

  return gfx::Size(visible_rect.width(),
                   std::round(visible_rect.height() / pixel_aspect_ratio));
}

gfx::Size GetNaturalSize(const gfx::Size& visible_size,
                         int aspect_ratio_numerator,
                         int aspect_ratio_denominator) {
  if (aspect_ratio_denominator <= 0 || aspect_ratio_numerator <= 0)
    return gfx::Size();

  double pixel_aspect_ratio =
      aspect_ratio_numerator / static_cast<double>(aspect_ratio_denominator);

  return GetNaturalSize(gfx::Rect(visible_size), pixel_aspect_ratio);
}

void FillYUV(VideoFrame* frame, uint8_t y, uint8_t u, uint8_t v) {
  // Fill the Y plane.
  uint8_t* y_plane = frame->data(VideoFrame::kYPlane);
  int y_rows = frame->rows(VideoFrame::kYPlane);
  int y_row_bytes = frame->row_bytes(VideoFrame::kYPlane);
  for (int i = 0; i < y_rows; ++i) {
    memset(y_plane, y, y_row_bytes);
    y_plane += frame->stride(VideoFrame::kYPlane);
  }

  // Fill the U and V planes.
  uint8_t* u_plane = frame->data(VideoFrame::kUPlane);
  uint8_t* v_plane = frame->data(VideoFrame::kVPlane);
  int uv_rows = frame->rows(VideoFrame::kUPlane);
  int u_row_bytes = frame->row_bytes(VideoFrame::kUPlane);
  int v_row_bytes = frame->row_bytes(VideoFrame::kVPlane);
  for (int i = 0; i < uv_rows; ++i) {
    memset(u_plane, u, u_row_bytes);
    memset(v_plane, v, v_row_bytes);
    u_plane += frame->stride(VideoFrame::kUPlane);
    v_plane += frame->stride(VideoFrame::kVPlane);
  }
}

void FillYUVA(VideoFrame* frame, uint8_t y, uint8_t u, uint8_t v, uint8_t a) {
  // Fill Y, U and V planes.
  FillYUV(frame, y, u, v);

  // Fill the A plane.
  uint8_t* a_plane = frame->data(VideoFrame::kAPlane);
  int a_rows = frame->rows(VideoFrame::kAPlane);
  int a_row_bytes = frame->row_bytes(VideoFrame::kAPlane);
  for (int i = 0; i < a_rows; ++i) {
    memset(a_plane, a, a_row_bytes);
    a_plane += frame->stride(VideoFrame::kAPlane);
  }
}

static void LetterboxPlane(VideoFrame* frame,
                           int plane,
                           const gfx::Rect& view_area_in_pixels,
                           uint8_t fill_byte) {
  uint8_t* ptr = frame->data(plane);
  const int rows = frame->rows(plane);
  const int row_bytes = frame->row_bytes(plane);
  const int stride = frame->stride(plane);
  const int bytes_per_element =
      VideoFrame::BytesPerElement(frame->format(), plane);
  gfx::Rect view_area(view_area_in_pixels.x() * bytes_per_element,
                      view_area_in_pixels.y(),
                      view_area_in_pixels.width() * bytes_per_element,
                      view_area_in_pixels.height());

  CHECK_GE(stride, row_bytes);
  CHECK_GE(view_area.x(), 0);
  CHECK_GE(view_area.y(), 0);
  CHECK_LE(view_area.right(), row_bytes);
  CHECK_LE(view_area.bottom(), rows);

  int y = 0;
  for (; y < view_area.y(); y++) {
    memset(ptr, fill_byte, row_bytes);
    ptr += stride;
  }
  if (view_area.width() < row_bytes) {
    for (; y < view_area.bottom(); y++) {
      if (view_area.x() > 0) {
        memset(ptr, fill_byte, view_area.x());
      }
      if (view_area.right() < row_bytes) {
        memset(ptr + view_area.right(),
               fill_byte,
               row_bytes - view_area.right());
      }
      ptr += stride;
    }
  } else {
    y += view_area.height();
    ptr += stride * view_area.height();
  }
  for (; y < rows; y++) {
    memset(ptr, fill_byte, row_bytes);
    ptr += stride;
  }
}

void LetterboxVideoFrame(VideoFrame* frame, const gfx::Rect& view_area) {
  switch (frame->format()) {
    case PIXEL_FORMAT_ARGB:
      LetterboxPlane(frame, VideoFrame::kARGBPlane, view_area, 0x00);
      break;
    case PIXEL_FORMAT_YV12:
    case PIXEL_FORMAT_I420: {
      DCHECK(!(view_area.x() & 1));
      DCHECK(!(view_area.y() & 1));
      DCHECK(!(view_area.width() & 1));
      DCHECK(!(view_area.height() & 1));
      LetterboxPlane(frame, VideoFrame::kYPlane, view_area, 0x00);
      gfx::Rect half_view_area(view_area.x() / 2, view_area.y() / 2,
                               view_area.width() / 2, view_area.height() / 2);
      LetterboxPlane(frame, VideoFrame::kUPlane, half_view_area, 0x80);
      LetterboxPlane(frame, VideoFrame::kVPlane, half_view_area, 0x80);
      break;
    }
    default:
      NOTREACHED();
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
    NOTREACHED();
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

gfx::Size ScaleSizeToFitWithinTarget(const gfx::Size& size,
                                     const gfx::Size& target) {
  return ScaleSizeToTarget(size, target, true);
}

gfx::Size ScaleSizeToEncompassTarget(const gfx::Size& size,
                                     const gfx::Size& target) {
  return ScaleSizeToTarget(size, target, false);
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
  DCHECK(video_frame);
  DCHECK(video_frame->HasGpuMemoryBuffer());

  auto* gmb = video_frame->GetGpuMemoryBuffer();
  if (!gmb->Map())
    return nullptr;

  const size_t num_planes = VideoFrame::NumPlanes(video_frame->format());
  uint8_t* plane_addrs[VideoFrame::kMaxPlanes] = {};
  for (size_t i = 0; i < num_planes; i++)
    plane_addrs[i] = static_cast<uint8_t*>(gmb->memory(i));

  auto mapped_frame = VideoFrame::WrapExternalYuvDataWithLayout(
      video_frame->layout(), video_frame->visible_rect(),
      video_frame->natural_size(), plane_addrs[0], plane_addrs[1],
      plane_addrs[2], video_frame->timestamp());

  if (!mapped_frame) {
    gmb->Unmap();
    return nullptr;
  }

  mapped_frame->set_color_space(video_frame->ColorSpace());
  mapped_frame->metadata().MergeMetadataFrom(video_frame->metadata());

  // Pass |video_frame| so that it outlives |mapped_frame| and the mapped buffer
  // is unmapped on destruction.
  mapped_frame->AddDestructionObserver(base::BindOnce(
      [](scoped_refptr<VideoFrame> frame) {
        DCHECK(frame->HasGpuMemoryBuffer());
        frame->GetGpuMemoryBuffer()->Unmap();
      },
      std::move(video_frame)));
  return mapped_frame;
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

  if (libyuv::I420Copy(src_frame.visible_data(VideoFrame::kYPlane),
                       src_frame.stride(VideoFrame::kYPlane),
                       src_frame.visible_data(VideoFrame::kUPlane),
                       src_frame.stride(VideoFrame::kUPlane),
                       src_frame.visible_data(VideoFrame::kVPlane),
                       src_frame.stride(VideoFrame::kVPlane),
                       dst_frame->data(VideoFrame::kYPlane),
                       dst_frame->stride(VideoFrame::kYPlane),
                       dst_frame->data(VideoFrame::kUPlane),
                       dst_frame->stride(VideoFrame::kUPlane),
                       dst_frame->data(VideoFrame::kVPlane),
                       dst_frame->stride(VideoFrame::kVPlane),
                       src_frame.visible_rect().width(),
                       src_frame.visible_rect().height()))
    return false;

  // Padding the region outside the visible rect with the repeated last
  // column / row of the visible rect. This can improve the coding efficiency.
  FillRegionOutsideVisibleRect(dst_frame->data(VideoFrame::kYPlane),
                               dst_frame->stride(VideoFrame::kYPlane),
                               dst_frame->coded_size(),
                               src_frame.visible_rect().size());
  FillRegionOutsideVisibleRect(
      dst_frame->data(VideoFrame::kUPlane),
      dst_frame->stride(VideoFrame::kUPlane),
      VideoFrame::PlaneSize(PIXEL_FORMAT_I420, VideoFrame::kUPlane,
                            dst_frame->coded_size()),
      VideoFrame::PlaneSize(PIXEL_FORMAT_I420, VideoFrame::kUPlane,
                            src_frame.visible_rect().size()));
  FillRegionOutsideVisibleRect(
      dst_frame->data(VideoFrame::kVPlane),
      dst_frame->stride(VideoFrame::kVPlane),
      VideoFrame::PlaneSize(PIXEL_FORMAT_I420, VideoFrame::kVPlane,
                            dst_frame->coded_size()),
      VideoFrame::PlaneSize(PIXEL_FORMAT_I420, VideoFrame::kVPlane,
                            src_frame.visible_rect().size()));

  return true;
}

scoped_refptr<VideoFrame> ReadbackTextureBackedFrameToMemorySync(
    const VideoFrame& txt_frame,
    gpu::raster::RasterInterface* ri,
    GrDirectContext* gr_context,
    VideoFramePool* pool) {
  DCHECK(ri);

  if (gr_context) {
    return ReadbackTextureBackedFrameToMemorySyncGLES(txt_frame, ri, gr_context,
                                                      pool);
  }
  return ReadbackTextureBackedFrameToMemorySyncOOP(txt_frame, ri, pool);
}

Status ConvertAndScaleFrame(const VideoFrame& src_frame,
                            VideoFrame& dst_frame,
                            std::vector<uint8_t>& tmp_buf) {
  constexpr auto kDefaultFiltering = libyuv::kFilterBox;
  if (!src_frame.IsMappable() || !dst_frame.IsMappable())
    return Status(StatusCode::kUnsupportedFrameFormatError);

  if ((dst_frame.format() == PIXEL_FORMAT_I420 ||
       dst_frame.format() == PIXEL_FORMAT_NV12) &&
      (src_frame.format() == PIXEL_FORMAT_XBGR ||
       src_frame.format() == PIXEL_FORMAT_XRGB ||
       src_frame.format() == PIXEL_FORMAT_ABGR ||
       src_frame.format() == PIXEL_FORMAT_ARGB)) {
    // libyuv's RGB to YUV methods always output BT.601.
    dst_frame.set_color_space(gfx::ColorSpace::CreateREC601());

    size_t src_stride = src_frame.stride(VideoFrame::kARGBPlane);
    const uint8_t* src_data = src_frame.visible_data(VideoFrame::kARGBPlane);
    if (src_frame.visible_rect() != dst_frame.visible_rect()) {
      size_t tmp_buffer_size = VideoFrame::AllocationSize(
          src_frame.format(), dst_frame.coded_size());
      if (tmp_buf.size() < tmp_buffer_size)
        tmp_buf.resize(tmp_buffer_size);

      size_t stride =
          VideoFrame::RowBytes(VideoFrame::kARGBPlane, src_frame.format(),
                               dst_frame.visible_rect().width());
      int error = libyuv::ARGBScale(
          src_data, src_stride, src_frame.visible_rect().width(),
          src_frame.visible_rect().height(), tmp_buf.data(), stride,
          dst_frame.visible_rect().width(), dst_frame.visible_rect().height(),
          kDefaultFiltering);
      if (error)
        return Status(StatusCode::kInvalidArgument);
      src_data = tmp_buf.data();
      src_stride = stride;
    }

    if (dst_frame.format() == PIXEL_FORMAT_I420) {
      auto convert_fn = (src_frame.format() == PIXEL_FORMAT_XBGR ||
                         src_frame.format() == PIXEL_FORMAT_ABGR)
                            ? libyuv::ABGRToI420
                            : libyuv::ARGBToI420;
      int error = convert_fn(
          src_data, src_stride, dst_frame.visible_data(VideoFrame::kYPlane),
          dst_frame.stride(VideoFrame::kYPlane),
          dst_frame.visible_data(VideoFrame::kUPlane),
          dst_frame.stride(VideoFrame::kUPlane),
          dst_frame.visible_data(VideoFrame::kVPlane),
          dst_frame.stride(VideoFrame::kVPlane),
          dst_frame.visible_rect().width(), dst_frame.visible_rect().height());
      return error ? Status(StatusCode::kInvalidArgument) : Status();
    }

    auto convert_fn = (src_frame.format() == PIXEL_FORMAT_XBGR ||
                       src_frame.format() == PIXEL_FORMAT_ABGR)
                          ? libyuv::ABGRToNV12
                          : libyuv::ARGBToNV12;
    int error = convert_fn(
        src_data, src_stride, dst_frame.visible_data(VideoFrame::kYPlane),
        dst_frame.stride(VideoFrame::kYPlane),
        dst_frame.visible_data(VideoFrame::kUVPlane),
        dst_frame.stride(VideoFrame::kUVPlane),
        dst_frame.visible_rect().width(), dst_frame.visible_rect().height());
    return error ? Status(StatusCode::kInvalidArgument) : Status();
  }

  // Converting between YUV formats doesn't change the color space.
  dst_frame.set_color_space(src_frame.ColorSpace());

  // Both frames are I420, only scaling is required.
  if (dst_frame.format() == PIXEL_FORMAT_I420 &&
      src_frame.format() == PIXEL_FORMAT_I420) {
    int error = libyuv::I420Scale(
        src_frame.visible_data(VideoFrame::kYPlane),
        src_frame.stride(VideoFrame::kYPlane),
        src_frame.visible_data(VideoFrame::kUPlane),
        src_frame.stride(VideoFrame::kUPlane),
        src_frame.visible_data(VideoFrame::kVPlane),
        src_frame.stride(VideoFrame::kVPlane), src_frame.visible_rect().width(),
        src_frame.visible_rect().height(),
        dst_frame.visible_data(VideoFrame::kYPlane),
        dst_frame.stride(VideoFrame::kYPlane),
        dst_frame.visible_data(VideoFrame::kUPlane),
        dst_frame.stride(VideoFrame::kUPlane),
        dst_frame.visible_data(VideoFrame::kVPlane),
        dst_frame.stride(VideoFrame::kVPlane), dst_frame.visible_rect().width(),
        dst_frame.visible_rect().height(), kDefaultFiltering);
    return error ? Status(StatusCode::kInvalidArgument) : Status();
  }

  // Both frames are NV12, only scaling is required.
  if (dst_frame.format() == PIXEL_FORMAT_NV12 &&
      src_frame.format() == PIXEL_FORMAT_NV12) {
    int error = libyuv::NV12Scale(
        src_frame.visible_data(VideoFrame::kYPlane),
        src_frame.stride(VideoFrame::kYPlane),
        src_frame.visible_data(VideoFrame::kUVPlane),
        src_frame.stride(VideoFrame::kUVPlane),
        src_frame.visible_rect().width(), src_frame.visible_rect().height(),
        dst_frame.visible_data(VideoFrame::kYPlane),
        dst_frame.stride(VideoFrame::kYPlane),
        dst_frame.visible_data(VideoFrame::kUVPlane),
        dst_frame.stride(VideoFrame::kUVPlane),
        dst_frame.visible_rect().width(), dst_frame.visible_rect().height(),
        kDefaultFiltering);
    return error ? Status(StatusCode::kInvalidArgument) : Status();
  }

  if (dst_frame.format() == PIXEL_FORMAT_I420 &&
      src_frame.format() == PIXEL_FORMAT_NV12) {
    if (src_frame.visible_rect() == dst_frame.visible_rect()) {
      // Both frames have the same size, only NV12-to-I420 conversion is
      // required.
      int error = libyuv::NV12ToI420(
          src_frame.visible_data(VideoFrame::kYPlane),
          src_frame.stride(VideoFrame::kYPlane),
          src_frame.visible_data(VideoFrame::kUVPlane),
          src_frame.stride(VideoFrame::kUVPlane),
          dst_frame.visible_data(VideoFrame::kYPlane),
          dst_frame.stride(VideoFrame::kYPlane),
          dst_frame.visible_data(VideoFrame::kUPlane),
          dst_frame.stride(VideoFrame::kUPlane),
          dst_frame.visible_data(VideoFrame::kVPlane),
          dst_frame.stride(VideoFrame::kVPlane),
          dst_frame.visible_rect().width(), dst_frame.visible_rect().height());
      return error ? Status(StatusCode::kInvalidArgument) : Status();
    } else {
      // Both resize and NV12-to-I420 conversion are required.
      // First, split UV planes into two, basically producing a I420 frame.
      const int tmp_uv_width = (src_frame.visible_rect().width() + 1) / 2;
      const int tmp_uv_height = (src_frame.visible_rect().height() + 1) / 2;
      size_t tmp_buffer_size = tmp_uv_width * tmp_uv_height * 2;
      if (tmp_buf.size() < tmp_buffer_size)
        tmp_buf.resize(tmp_buffer_size);

      uint8_t* tmp_u = tmp_buf.data();
      uint8_t* tmp_v = tmp_u + tmp_uv_width * tmp_uv_height;
      DCHECK_EQ(tmp_buf.data() + tmp_buffer_size,
                tmp_v + (tmp_uv_width * tmp_uv_height));
      libyuv::SplitUVPlane(src_frame.visible_data(VideoFrame::kUVPlane),
                           src_frame.stride(VideoFrame::kUVPlane), tmp_u,
                           tmp_uv_width, tmp_v, tmp_uv_width, tmp_uv_width,
                           tmp_uv_height);

      // Second, scale resulting I420 frame into the destination.
      int error = libyuv::I420Scale(
          src_frame.visible_data(VideoFrame::kYPlane),
          src_frame.stride(VideoFrame::kYPlane),
          tmp_u,  // Temporary U-plane for src UV-plane.
          tmp_uv_width,
          tmp_v,  // Temporary V-plane for src UV-plane.
          tmp_uv_width, src_frame.visible_rect().width(),
          src_frame.visible_rect().height(),
          dst_frame.visible_data(VideoFrame::kYPlane),
          dst_frame.stride(VideoFrame::kYPlane),
          dst_frame.visible_data(VideoFrame::kUPlane),
          dst_frame.stride(VideoFrame::kUPlane),
          dst_frame.visible_data(VideoFrame::kVPlane),
          dst_frame.stride(VideoFrame::kVPlane),
          dst_frame.visible_rect().width(), dst_frame.visible_rect().height(),
          kDefaultFiltering);
      return error ? Status(StatusCode::kInvalidArgument) : Status();
    }
  }

  if (dst_frame.format() == PIXEL_FORMAT_NV12 &&
      src_frame.format() == PIXEL_FORMAT_I420) {
    if (src_frame.visible_rect() == dst_frame.visible_rect()) {
      // Both frames have the same size, only I420-to-NV12 conversion is
      // required.
      int error = libyuv::I420ToNV12(
          src_frame.visible_data(VideoFrame::kYPlane),
          src_frame.stride(VideoFrame::kYPlane),
          src_frame.visible_data(VideoFrame::kUPlane),
          src_frame.stride(VideoFrame::kUPlane),
          src_frame.visible_data(VideoFrame::kVPlane),
          src_frame.stride(VideoFrame::kVPlane),
          dst_frame.visible_data(VideoFrame::kYPlane),
          dst_frame.stride(VideoFrame::kYPlane),
          dst_frame.visible_data(VideoFrame::kUVPlane),
          dst_frame.stride(VideoFrame::kUVPlane),
          dst_frame.visible_rect().width(), dst_frame.visible_rect().height());
      return error ? Status(StatusCode::kInvalidArgument) : Status();
    } else {
      // Both resize and I420-to-NV12 conversion are required.
      // First, merge U and V planes into one, basically producing a NV12 frame.
      const int tmp_uv_width = (src_frame.visible_rect().width() + 1) / 2;
      const int tmp_uv_height = (src_frame.visible_rect().height() + 1) / 2;
      size_t tmp_buffer_size = tmp_uv_width * tmp_uv_height * 2;
      if (tmp_buf.size() < tmp_buffer_size)
        tmp_buf.resize(tmp_buffer_size);

      uint8_t* tmp_uv = tmp_buf.data();
      size_t stride_uv = tmp_uv_width * 2;
      libyuv::MergeUVPlane(src_frame.visible_data(VideoFrame::kUPlane),
                           src_frame.stride(VideoFrame::kUPlane),
                           src_frame.visible_data(VideoFrame::kVPlane),
                           src_frame.stride(VideoFrame::kVPlane),
                           tmp_uv,     // Temporary for merged UV-plane
                           stride_uv,  // Temporary stride
                           tmp_uv_width, tmp_uv_height);

      // Second, scale resulting NV12 frame into the destination.
      int error = libyuv::NV12Scale(
          src_frame.visible_data(VideoFrame::kYPlane),
          src_frame.stride(VideoFrame::kYPlane),
          tmp_uv,     // Temporary for merged UV-plane
          stride_uv,  // Temporary stride
          src_frame.visible_rect().width(), src_frame.visible_rect().height(),
          dst_frame.visible_data(VideoFrame::kYPlane),
          dst_frame.stride(VideoFrame::kYPlane),
          dst_frame.visible_data(VideoFrame::kUVPlane),
          dst_frame.stride(VideoFrame::kUVPlane),
          dst_frame.visible_rect().width(), dst_frame.visible_rect().height(),
          kDefaultFiltering);
      return error ? Status(StatusCode::kInvalidArgument) : Status();
    }
  }

  return Status(StatusCode::kUnsupportedFrameFormatError)
      .WithData("src", src_frame.AsHumanReadableString())
      .WithData("dst", dst_frame.AsHumanReadableString());
}

scoped_refptr<VideoFrame> CreateFromSkImage(sk_sp<SkImage> sk_image,
                                            const gfx::Rect& visible_rect,
                                            const gfx::Size& natural_size,
                                            base::TimeDelta timestamp) {
  DCHECK(!sk_image->isTextureBacked());

  // TODO(crbug.com/1073995): Add F16 support.
  auto sk_color_type = sk_image->colorType();
  if (sk_color_type != kRGBA_8888_SkColorType &&
      sk_color_type != kBGRA_8888_SkColorType) {
    return nullptr;
  }

  SkPixmap pm;
  const bool peek_result = sk_image->peekPixels(&pm);
  DCHECK(peek_result);

  const auto format =
      sk_image->isOpaque()
          ? (sk_color_type == kRGBA_8888_SkColorType ? PIXEL_FORMAT_XBGR
                                                     : PIXEL_FORMAT_XRGB)
          : (sk_color_type == kRGBA_8888_SkColorType ? PIXEL_FORMAT_ABGR
                                                     : PIXEL_FORMAT_ARGB);

  auto coded_size = gfx::Size(sk_image->width(), sk_image->height());
  auto layout = VideoFrameLayout::CreateWithStrides(
      format, coded_size, std::vector<int32_t>(1, pm.rowBytes()));
  if (!layout)
    return nullptr;

  auto frame = VideoFrame::WrapExternalDataWithLayout(
      *layout, visible_rect, natural_size,
      // TODO(crbug.com/1161304): We should be able to wrap readonly memory in
      // a VideoFrame instead of using writable_addr() here.
      reinterpret_cast<uint8_t*>(pm.writable_addr()), pm.computeByteSize(),
      timestamp);
  if (!frame)
    return nullptr;

  frame->AddDestructionObserver(base::BindOnce(
      base::DoNothing::Once<sk_sp<SkImage>>(), std::move(sk_image)));
  return frame;
}

}  // namespace media
