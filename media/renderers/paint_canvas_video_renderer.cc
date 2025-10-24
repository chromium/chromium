// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/renderers/paint_canvas_video_renderer.h"

#include <GLES3/gl3.h>

#include <array>
#include <limits>
#include <numeric>

#include "base/barrier_closure.h"
#include "base/compiler_specific.h"
#include "base/containers/span_reader.h"
#include "base/containers/span_writer.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/numerics/byte_conversions.h"
#include "base/numerics/checked_math.h"
#include "base/sequence_checker.h"
#include "base/synchronization/waitable_event.h"
#include "base/system/sys_info.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "cc/paint/paint_canvas.h"
#include "cc/paint/paint_flags.h"
#include "cc/paint/paint_image_builder.h"
#include "components/viz/common/gpu/raster_context_provider.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "gpu/command_buffer/client/context_support.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/client/raster_interface.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/common/capabilities.h"
#include "gpu/command_buffer/common/mailbox_holder.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "media/base/data_buffer.h"
#include "media/base/video_frame.h"
#include "media/base/video_util.h"
#include "media/base/wait_and_replace_sync_token_client.h"
#include "media/renderers/video_frame_yuv_converter.h"
#include "third_party/fp16/src/include/fp16.h"
#include "third_party/libyuv/include/libyuv.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkImageGenerator.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/gpu/ganesh/GrBackendSurface.h"
#include "third_party/skia/include/gpu/ganesh/GrDirectContext.h"
#include "third_party/skia/include/gpu/ganesh/SkImageGanesh.h"
#include "third_party/skia/include/gpu/ganesh/gl/GrGLBackendSurface.h"
#include "third_party/skia/include/gpu/ganesh/gl/GrGLTypes.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/skia_conversions.h"

// Skia internal format depends on a platform. On Android it is ABGR, on others
// it's ARGB. YUV_ORDER() conditionally remap YUV to YVU for ABGR.
#if SK_B32_SHIFT == 0 && SK_G32_SHIFT == 8 && SK_R32_SHIFT == 16 && \
    SK_A32_SHIFT == 24
#define OUTPUT_ARGB 1
#define LIBYUV_ABGR_TO_ARGB libyuv::ABGRToARGB
#define YUV_ORDER(y, y_stride, u, u_stride, v, v_stride) \
  (y), (y_stride), (u), (u_stride), (v), (v_stride)
#define GBR_TO_RGB_ORDER(y, y_stride, u, u_stride, v, v_stride) \
  (v), (v_stride), (y), (y_stride), (u), (u_stride)
#define LIBYUV_NV12_TO_ARGB_MATRIX libyuv::NV12ToARGBMatrix
#define SHARED_IMAGE_FORMAT viz::SinglePlaneFormat::kBGRA_8888
#elif SK_R32_SHIFT == 0 && SK_G32_SHIFT == 8 && SK_B32_SHIFT == 16 && \
    SK_A32_SHIFT == 24
#define OUTPUT_ARGB 0
#define LIBYUV_ABGR_TO_ARGB libyuv::ARGBToABGR
#define YUV_ORDER(y, y_stride, u, u_stride, v, v_stride) \
  (y), (y_stride), (v), (v_stride), (u), (u_stride)
#define GBR_TO_RGB_ORDER(y, y_stride, u, u_stride, v, v_stride) \
  (u), (u_stride), (y), (y_stride), (v), (v_stride)
#define LIBYUV_NV12_TO_ARGB_MATRIX libyuv::NV21ToARGBMatrix
#define SHARED_IMAGE_FORMAT viz::SinglePlaneFormat::kRGBA_8888
#else
#error Unexpected Skia ARGB_8888 layout!
#endif

namespace media {

namespace {

// This class keeps the last image drawn.
// We delete the temporary resource if it is not used for 3 seconds.
const int kTemporaryResourceDeletionDelay = 3;  // Seconds;

void BindAndTexImage2D(gpu::gles2::GLES2Interface* gl,
                       unsigned int target,
                       unsigned int texture,
                       unsigned int internal_format,
                       unsigned int format,
                       unsigned int type,
                       int level,
                       const gfx::Size& size) {
  gl->BindTexture(target, texture);
  gl->TexImage2D(target, level, internal_format, size.width(), size.height(), 0,
                 format, type, nullptr);
}

gpu::SyncToken CopySharedImageToTexture(
    gpu::gles2::GLES2Interface* gl,
    const gfx::Size& coded_size,
    const gfx::Rect& visible_rect,
    gpu::ClientSharedImage* source_shared_image,
    const gpu::SyncToken& source_sync_token,
    unsigned int target,
    unsigned int texture,
    unsigned int internal_format,
    unsigned int format,
    unsigned int type,
    int level,
    SkAlphaType dst_alpha_type,
    GrSurfaceOrigin dst_origin) {
  auto si_texture = source_shared_image->CreateGLTexture(gl);
  auto scoped_si_access =
      si_texture->BeginAccess(source_sync_token, /*readonly=*/true);

  const bool do_premultiply_alpha =
      dst_alpha_type == kPremul_SkAlphaType &&
      source_shared_image->alpha_type() == kUnpremul_SkAlphaType;
  const bool do_unpremultiply_alpha =
      dst_alpha_type == kUnpremul_SkAlphaType &&
      source_shared_image->alpha_type() == kPremul_SkAlphaType;

  const bool do_flip_y = source_shared_image->surface_origin() != dst_origin;
  if (visible_rect != gfx::Rect(coded_size)) {
    // Must reallocate the destination texture and copy only a sub-portion.

    // There should always be enough data in the source texture to
    // cover this copy.
    DCHECK_LE(visible_rect.width(), coded_size.width());
    DCHECK_LE(visible_rect.height(), coded_size.height());

    BindAndTexImage2D(gl, target, texture, internal_format, format, type, level,
                      visible_rect.size());
    // TODO(crbug.com/378688985): `visible_rect` is always in top-left
    // coordinate space, but CopySubTextureCHROMIUM requires it to be in texture
    // space, so this is incorrect if `source_shared_image` origin is bottom
    // left.
    gl->CopySubTextureCHROMIUM(scoped_si_access->texture_id(), 0, target,
                               texture, level, 0, 0, visible_rect.x(),
                               visible_rect.y(), visible_rect.width(),
                               visible_rect.height(), do_flip_y,
                               do_premultiply_alpha, do_unpremultiply_alpha);

  } else {
    gl->CopyTextureCHROMIUM(scoped_si_access->texture_id(), 0, target, texture,
                            level, internal_format, type, do_flip_y,
                            do_premultiply_alpha, do_unpremultiply_alpha);
  }
  return gpu::SharedImageTexture::ScopedAccess::EndAccess(
      std::move(scoped_si_access));
}

// Update |video_frame|'s release sync token to reflect the work done in |ri|,
// and ensure that |video_frame| be kept remain alive until |ri|'s commands have
// been completed. This is implemented for both gpu::gles2::GLES2Interface and
// gpu::raster::RasterInterface. This function is critical to ensure that
// |video_frame|'s resources not be returned until they are no longer in use.
// https://crbug.com/819914 (software video decode frame corruption)
// https://crbug.com/1237100 (camera capture reuse corruption)
void SynchronizeVideoFrameRead(
    scoped_refptr<VideoFrame> video_frame,
    gpu::raster::RasterInterface* ri,
    gpu::ContextSupport* context_support,
    std::unique_ptr<gpu::RasterScopedAccess> ri_access = nullptr) {
  WaitAndReplaceSyncTokenClient client(ri, std::move(ri_access));
  video_frame->UpdateReleaseSyncToken(&client);
  if (!video_frame->metadata().read_lock_fences_enabled) {
    return;
  }

  unsigned query_id = 0;
  ri->GenQueriesEXT(1, &query_id);
  DCHECK(query_id);
  ri->BeginQueryEXT(GL_COMMANDS_COMPLETED_CHROMIUM, query_id);
  ri->EndQueryEXT(GL_COMMANDS_COMPLETED_CHROMIUM);

  // |on_query_done_cb| will keep |video_frame| alive.
  auto on_query_done_cb = base::BindOnce(
      [](scoped_refptr<VideoFrame> video_frame,
         gpu::raster::RasterInterface* ri,
         unsigned query_id) { ri->DeleteQueriesEXT(1, &query_id); },
      video_frame, ri, query_id);
  context_support->SignalQuery(query_id, std::move(on_query_done_cb));
}

void SynchronizeVideoFrameRead(
    scoped_refptr<VideoFrame> video_frame,
    gpu::gles2::GLES2Interface* gl,
    gpu::ContextSupport* context_support,
    std::unique_ptr<gpu::RasterScopedAccess> ri_access = nullptr) {
  WaitAndReplaceSyncTokenClient client(gl, std::move(ri_access));
  video_frame->UpdateReleaseSyncToken(&client);
  if (!video_frame->metadata().read_lock_fences_enabled) {
    return;
  }

  unsigned query_id = 0;
  gl->GenQueriesEXT(1, &query_id);
  DCHECK(query_id);
  gl->BeginQueryEXT(GL_COMMANDS_COMPLETED_CHROMIUM, query_id);
  gl->EndQueryEXT(GL_COMMANDS_COMPLETED_CHROMIUM);

  // |on_query_done_cb| will keep |video_frame| alive.
  auto on_query_done_cb = base::DoNothingWithBoundArgs(video_frame);
  context_support->SignalQuery(query_id, std::move(on_query_done_cb));

  // Delete the query immediately. This will cause |on_query_done_cb| to be
  // issued prematurely. The alternative, deleting |query_id| within
  // |on_query_done_cb|, can cause a crash because |gl| may not still exist
  // at the time of the callback. This is incorrect behavior.
  // https://crbug.com/1243763
  gl->DeleteQueriesEXT(1, &query_id);
}

libyuv::FilterMode ToLibyuvFilterMode(
    PaintCanvasVideoRenderer::FilterMode filter) {
  switch (filter) {
    case PaintCanvasVideoRenderer::kFilterNone:
      return libyuv::kFilterNone;
    case PaintCanvasVideoRenderer::kFilterBilinear:
      return libyuv::kFilterBilinear;
  }
}

size_t NumConvertVideoFrameToRGBPixelsTasks(const VideoFrame* video_frame) {
  constexpr size_t kTaskBytes = 1024 * 1024;  // 1 MiB
  const size_t frame_size = VideoFrame::AllocationSize(
      video_frame->format(), video_frame->visible_rect().size());
  const size_t n_tasks = std::max<size_t>(1, frame_size / kTaskBytes);
  return std::min<size_t>(n_tasks, base::SysInfo::NumberOfProcessors());
}

void ConvertVideoFrameToRGBPixelsTask(const VideoFrame* video_frame,
                                      const SkPixmap& dst_pixmap,
                                      libyuv::FilterMode filter,
                                      size_t task_index,
                                      size_t n_tasks,
                                      base::RepeatingClosure* done) {
  const VideoPixelFormat format = video_frame->format();
  const int width = video_frame->visible_rect().width();
  const int height = video_frame->visible_rect().height();

  size_t rows_per_chunk = 1;
  for (size_t plane = 0; plane < VideoFrame::kMaxPlanes; ++plane) {
    if (VideoFrame::IsValidPlane(format, plane)) {
      rows_per_chunk = std::lcm(rows_per_chunk,
                                VideoFrame::SampleSize(format, plane).height());
    }
  }

  base::CheckedNumeric<size_t> chunks = height / rows_per_chunk;
  const size_t chunk_start = (chunks * task_index / n_tasks).ValueOrDie();
  const size_t chunk_end = (chunks * (task_index + 1) / n_tasks).ValueOrDie();

  // Indivisible heights must process any remaining rows in the last task.
  size_t rows = (chunk_end - chunk_start) * rows_per_chunk;
  if (task_index + 1 == n_tasks) {
    rows += height % rows_per_chunk;
  }
  const auto chunk_subrect_of_visible_rect =
      SkIRect::MakeXYWH(0, chunk_start * rows_per_chunk, width, rows);

  struct PlaneMetaData {
    size_t stride;
    raw_ptr<const uint8_t> data;
  };
  std::array<PlaneMetaData, VideoFrame::kMaxPlanes> plane_meta;

  for (size_t plane = 0; plane < VideoFrame::kMaxPlanes; ++plane) {
    if (VideoFrame::IsValidPlane(format, plane)) {
      plane_meta[plane] = {
          // Note: Unlike |data|, stride does not need to be adjusted by the
          // visible rect and sample size. Adding the full frame stride to a
          // pixel on row N and column M will wrap to column M on row N + 1.
          .stride = video_frame->stride(plane),

          .data = video_frame->visible_data(plane) +
                  video_frame->stride(plane) * (chunk_start * rows_per_chunk) /
                      VideoFrame::SampleSize(format, plane).height()};
    } else {
      plane_meta[plane] = {.stride = 0, .data = nullptr};
    }
  }

  SkPixmap dst_chunk_pixmap;
  bool dst_extract_subset_result = dst_pixmap.extractSubset(
      &dst_chunk_pixmap, chunk_subrect_of_visible_rect);
  CHECK(dst_extract_subset_result);

  if (format == PIXEL_FORMAT_ARGB || format == PIXEL_FORMAT_XRGB ||
      format == PIXEL_FORMAT_ABGR || format == PIXEL_FORMAT_XBGR ||
      format == PIXEL_FORMAT_RGBAF16) {
    SkPixmap src_pm(
        SkImageInfo::Make(width, rows,
                          SkColorTypeForPlane(format, VideoFrame::Plane::kARGB),
                          media::IsOpaque(format) ? kOpaque_SkAlphaType
                                                  : kUnpremul_SkAlphaType),
        plane_meta[VideoFrame::Plane::kARGB].data,
        plane_meta[VideoFrame::Plane::kARGB].stride);
    src_pm.readPixels(dst_chunk_pixmap);
    done->Run();
    return;
  }

  // At this point, the dest must be N32 for YUV formats to write.
  CHECK_EQ(dst_pixmap.colorType(), kN32_SkColorType);
  uint8_t* const pixels =
      reinterpret_cast<uint8_t*>(dst_chunk_pixmap.writable_addr());
  const size_t row_bytes = dst_chunk_pixmap.rowBytes();
  const bool premultiply_alpha =
      dst_chunk_pixmap.alphaType() == kPremul_SkAlphaType;

  // TODO(crbug.com/41380578): This should default to BT.709 color space.
  auto yuv_cs = kRec601_SkYUVColorSpace;
  video_frame->ColorSpace().ToSkYUVColorSpace(video_frame->BitDepth(), &yuv_cs);
  const libyuv::YuvConstants* matrix =
      GetYuvContantsForColorSpace(yuv_cs, OUTPUT_ARGB);

  if (!video_frame->data(VideoFrame::Plane::kU) &&
      !video_frame->data(VideoFrame::Plane::kV)) {
    DCHECK_EQ(format, PIXEL_FORMAT_I420);
    // For monochrome content ARGB and ABGR are interchangeable.
    libyuv::I400ToARGBMatrix(plane_meta[VideoFrame::Plane::kY].data,
                             plane_meta[VideoFrame::Plane::kY].stride, pixels,
                             row_bytes, matrix, width, rows);
    done->Run();
    return;
  }

  auto convert_yuv = [&](const libyuv::YuvConstants* matrix, auto&& func) {
    func(YUV_ORDER(plane_meta[VideoFrame::Plane::kY].data,
                   plane_meta[VideoFrame::Plane::kY].stride,
                   plane_meta[VideoFrame::Plane::kU].data,
                   plane_meta[VideoFrame::Plane::kU].stride,
                   plane_meta[VideoFrame::Plane::kV].data,
                   plane_meta[VideoFrame::Plane::kV].stride),
         pixels, row_bytes, matrix, width, rows);
  };

  auto convert_yuv_with_filter = [&](const libyuv::YuvConstants* matrix,
                                     auto&& func) {
    func(YUV_ORDER(plane_meta[VideoFrame::Plane::kY].data,
                   plane_meta[VideoFrame::Plane::kY].stride,
                   plane_meta[VideoFrame::Plane::kU].data,
                   plane_meta[VideoFrame::Plane::kU].stride,
                   plane_meta[VideoFrame::Plane::kV].data,
                   plane_meta[VideoFrame::Plane::kV].stride),
         pixels, row_bytes, matrix, width, rows, filter);
  };

  auto convert_yuva = [&](const libyuv::YuvConstants* matrix, auto&& func) {
    func(YUV_ORDER(plane_meta[VideoFrame::Plane::kY].data,
                   plane_meta[VideoFrame::Plane::kY].stride,
                   plane_meta[VideoFrame::Plane::kU].data,
                   plane_meta[VideoFrame::Plane::kU].stride,
                   plane_meta[VideoFrame::Plane::kV].data,
                   plane_meta[VideoFrame::Plane::kV].stride),
         plane_meta[VideoFrame::Plane::kA].data,
         plane_meta[VideoFrame::Plane::kA].stride, pixels, row_bytes, matrix,
         width, rows, premultiply_alpha);
  };

  auto convert_yuva_with_filter = [&](const libyuv::YuvConstants* matrix,
                                      auto&& func) {
    func(YUV_ORDER(plane_meta[VideoFrame::Plane::kY].data,
                   plane_meta[VideoFrame::Plane::kY].stride,
                   plane_meta[VideoFrame::Plane::kU].data,
                   plane_meta[VideoFrame::Plane::kU].stride,
                   plane_meta[VideoFrame::Plane::kV].data,
                   plane_meta[VideoFrame::Plane::kV].stride),
         plane_meta[VideoFrame::Plane::kA].data,
         plane_meta[VideoFrame::Plane::kA].stride, pixels, row_bytes, matrix,
         width, rows, premultiply_alpha, filter);
  };

  auto convert_yuv16 = [&](const libyuv::YuvConstants* matrix, auto&& func) {
    func(YUV_ORDER(reinterpret_cast<const uint16_t*>(
                       plane_meta[VideoFrame::Plane::kY].data.get()),
                   plane_meta[VideoFrame::Plane::kY].stride / 2,
                   reinterpret_cast<const uint16_t*>(
                       plane_meta[VideoFrame::Plane::kU].data.get()),
                   plane_meta[VideoFrame::Plane::kU].stride / 2,
                   reinterpret_cast<const uint16_t*>(
                       plane_meta[VideoFrame::Plane::kV].data.get()),
                   plane_meta[VideoFrame::Plane::kV].stride / 2),
         pixels, row_bytes, matrix, width, rows);
  };

  auto convert_yuv16_with_filter = [&](const libyuv::YuvConstants* matrix,
                                       auto&& func) {
    func(YUV_ORDER(reinterpret_cast<const uint16_t*>(
                       plane_meta[VideoFrame::Plane::kY].data.get()),
                   plane_meta[VideoFrame::Plane::kY].stride / 2,
                   reinterpret_cast<const uint16_t*>(
                       plane_meta[VideoFrame::Plane::kU].data.get()),
                   plane_meta[VideoFrame::Plane::kU].stride / 2,
                   reinterpret_cast<const uint16_t*>(
                       plane_meta[VideoFrame::Plane::kV].data.get()),
                   plane_meta[VideoFrame::Plane::kV].stride / 2),
         pixels, row_bytes, matrix, width, rows, filter);
  };

  auto convert_yuva16 = [&](const libyuv::YuvConstants* matrix, auto&& func) {
    func(YUV_ORDER(reinterpret_cast<const uint16_t*>(
                       plane_meta[VideoFrame::Plane::kY].data.get()),
                   plane_meta[VideoFrame::Plane::kY].stride / 2,
                   reinterpret_cast<const uint16_t*>(
                       plane_meta[VideoFrame::Plane::kU].data.get()),
                   plane_meta[VideoFrame::Plane::kU].stride / 2,
                   reinterpret_cast<const uint16_t*>(
                       plane_meta[VideoFrame::Plane::kV].data.get()),
                   plane_meta[VideoFrame::Plane::kV].stride / 2),
         reinterpret_cast<const uint16_t*>(
             plane_meta[VideoFrame::Plane::kA].data.get()),
         plane_meta[VideoFrame::Plane::kA].stride / 2, pixels, row_bytes,
         matrix, width, rows, premultiply_alpha);
  };

  auto convert_yuva16_with_filter = [&](const libyuv::YuvConstants* matrix,
                                        auto&& func) {
    func(YUV_ORDER(reinterpret_cast<const uint16_t*>(
                       plane_meta[VideoFrame::Plane::kY].data.get()),
                   plane_meta[VideoFrame::Plane::kY].stride / 2,
                   reinterpret_cast<const uint16_t*>(
                       plane_meta[VideoFrame::Plane::kU].data.get()),
                   plane_meta[VideoFrame::Plane::kU].stride / 2,
                   reinterpret_cast<const uint16_t*>(
                       plane_meta[VideoFrame::Plane::kV].data.get()),
                   plane_meta[VideoFrame::Plane::kV].stride / 2),
         reinterpret_cast<const uint16_t*>(
             plane_meta[VideoFrame::Plane::kA].data.get()),
         plane_meta[VideoFrame::Plane::kA].stride / 2, pixels, row_bytes,
         matrix, width, rows, premultiply_alpha, filter);
  };

  switch (format) {
    case PIXEL_FORMAT_YV12:
    case PIXEL_FORMAT_I420:
      convert_yuv_with_filter(matrix, libyuv::I420ToARGBMatrixFilter);
      break;
    case PIXEL_FORMAT_I422:
      convert_yuv_with_filter(matrix, libyuv::I422ToARGBMatrixFilter);
      break;
    case PIXEL_FORMAT_I444:
      // Special case for 4:4:4 RGB encoded videos.
      if (video_frame->ColorSpace().GetMatrixID() ==
          gfx::ColorSpace::MatrixID::GBR) {
        libyuv::MergeARGBPlane(
            GBR_TO_RGB_ORDER(plane_meta[VideoFrame::Plane::kY].data,
                             plane_meta[VideoFrame::Plane::kY].stride,
                             plane_meta[VideoFrame::Plane::kU].data,
                             plane_meta[VideoFrame::Plane::kU].stride,
                             plane_meta[VideoFrame::Plane::kV].data,
                             plane_meta[VideoFrame::Plane::kV].stride),
            plane_meta[VideoFrame::Plane::kA].data,
            plane_meta[VideoFrame::Plane::kA].stride, pixels, row_bytes, width,
            rows);
      } else {
        convert_yuv(matrix, libyuv::I444ToARGBMatrix);
      }
      break;
    case PIXEL_FORMAT_YUV420P10:
      convert_yuv16_with_filter(matrix, libyuv::I010ToARGBMatrixFilter);
      break;
    case PIXEL_FORMAT_YUV422P10:
      convert_yuv16_with_filter(matrix, libyuv::I210ToARGBMatrixFilter);
      break;
    case PIXEL_FORMAT_YUV444P10:
      convert_yuv16(matrix, libyuv::I410ToARGBMatrix);
      break;
    case PIXEL_FORMAT_YUV420P12:
      convert_yuv16(matrix, libyuv::I012ToARGBMatrix);
      break;
    case PIXEL_FORMAT_I420A:
      convert_yuva_with_filter(matrix, libyuv::I420AlphaToARGBMatrixFilter);
      break;
    case PIXEL_FORMAT_I422A:
      convert_yuva_with_filter(matrix, libyuv::I422AlphaToARGBMatrixFilter);
      break;
    case PIXEL_FORMAT_I444A:
      convert_yuva(matrix, libyuv::I444AlphaToARGBMatrix);
      break;
    case PIXEL_FORMAT_YUV420AP10:
      convert_yuva16_with_filter(matrix, libyuv::I010AlphaToARGBMatrixFilter);
      break;
    case PIXEL_FORMAT_YUV422AP10:
      convert_yuva16_with_filter(matrix, libyuv::I210AlphaToARGBMatrixFilter);
      break;
    case PIXEL_FORMAT_YUV444AP10:
      convert_yuva16(matrix, libyuv::I410AlphaToARGBMatrix);
      break;

    case PIXEL_FORMAT_NV12:
      LIBYUV_NV12_TO_ARGB_MATRIX(plane_meta[VideoFrame::Plane::kY].data,
                                 plane_meta[VideoFrame::Plane::kY].stride,
                                 plane_meta[VideoFrame::Plane::kUV].data,
                                 plane_meta[VideoFrame::Plane::kUV].stride,
                                 pixels, row_bytes, matrix, width, rows);
      break;
    case PIXEL_FORMAT_P010LE:
      libyuv::P010ToARGBMatrix(
          reinterpret_cast<const uint16_t*>(
              plane_meta[VideoFrame::Plane::kY].data.get()),
          plane_meta[VideoFrame::Plane::kY].stride,
          reinterpret_cast<const uint16_t*>(
              plane_meta[VideoFrame::Plane::kUV].data.get()),
          plane_meta[VideoFrame::Plane::kUV].stride, pixels, row_bytes, matrix,
          width, rows);
      if (!OUTPUT_ARGB) {
        libyuv::ARGBToABGR(pixels, row_bytes, pixels, row_bytes, width, rows);
      }
      break;

    case PIXEL_FORMAT_YUV422P12:
    case PIXEL_FORMAT_YUV444P12:
    case PIXEL_FORMAT_Y16:
      NOTREACHED()
          << "These cases should be handled in ConvertVideoFrameToRGBPixels";

    case PIXEL_FORMAT_UYVY:
    case PIXEL_FORMAT_NV21:
    case PIXEL_FORMAT_NV16:
    case PIXEL_FORMAT_NV24:
    case PIXEL_FORMAT_NV12A:
    case PIXEL_FORMAT_P210LE:
    case PIXEL_FORMAT_P410LE:
    case PIXEL_FORMAT_YUY2:
    case PIXEL_FORMAT_ARGB:
    case PIXEL_FORMAT_BGRA:
    case PIXEL_FORMAT_XRGB:
    case PIXEL_FORMAT_RGB24:
    case PIXEL_FORMAT_MJPEG:
    case PIXEL_FORMAT_ABGR:
    case PIXEL_FORMAT_XBGR:
    case PIXEL_FORMAT_XR30:
    case PIXEL_FORMAT_XB30:
    case PIXEL_FORMAT_RGBAF16:
    case PIXEL_FORMAT_UNKNOWN:
      NOTREACHED() << "Only YUV formats and Y16 are supported, got: "
                   << media::VideoPixelFormatToString(format);
  }
  done->Run();
}

#if !BUILDFLAG(IS_ANDROID)
// Valid gl texture internal format that can try to use direct uploading path.
bool ValidFormatForDirectUploading(GrGLenum format, unsigned int type) {
  switch (format) {
    case GL_RGBA:
      return type == GL_UNSIGNED_BYTE || type == GL_UNSIGNED_SHORT_4_4_4_4;
    case GL_RGB:
      return type == GL_UNSIGNED_BYTE || type == GL_UNSIGNED_SHORT_5_6_5;
    // WebGL2 supported sized formats
    case GL_RGBA8:
    case GL_RGB565:
    case GL_RGBA16F:
    case GL_RGB8:
    case GL_RGB10_A2:
    case GL_RGBA4:
      // TODO(crbug.com/356649879): RasterContextProvider never has ES3 context.
      // Use the correct WebGL major version here.
      return false;
    default:
      return false;
  }
}
#endif  // !BUILDFLAG(IS_ANDROID)

std::tuple<SkYUVAInfo::PlaneConfig, SkYUVAInfo::Subsampling>
VideoPixelFormatAsSkYUVAInfoValues(VideoPixelFormat format) {
  // The 9, 10, and 12 bit formats could be added here if GetYUVAPlanes() were
  // updated to convert data to unorm16/float16. Similarly, alpha planes and
  // formats with interleaved planes (e.g. NV12) could  be supported if that
  // function were updated to not assume 3 separate Y, U, and V planes. Also,
  // GpuImageDecodeCache would need be able to handle plane configurations
  // other than 3 separate y, u, and v planes (crbug.com/910276).
  switch (format) {
    case PIXEL_FORMAT_I420:
      return {SkYUVAInfo::PlaneConfig::kY_U_V, SkYUVAInfo::Subsampling::k420};
    case PIXEL_FORMAT_I422:
      return {SkYUVAInfo::PlaneConfig::kY_U_V, SkYUVAInfo::Subsampling::k422};
    case PIXEL_FORMAT_I444:
      return {SkYUVAInfo::PlaneConfig::kY_U_V, SkYUVAInfo::Subsampling::k444};
    default:
      return {SkYUVAInfo::PlaneConfig::kUnknown,
              SkYUVAInfo::Subsampling::kUnknown};
  }
}

// Checks support before attempting one copy upload to GL texture.
bool SupportsOneCopyUploadToGLTexture(VideoPixelFormat video_frame_format,
                                      uint32_t shared_image_target,
                                      unsigned int dst_target,
                                      unsigned int dst_internal_format,
                                      unsigned int dst_type,
                                      int dst_level,
                                      SkAlphaType dst_alpha_type) {
  // NOTE: The direct upload path is not supported on Android (see comment on
  // CopyVideoFrameTexturesToGLTexture()).
  // TODO(crbug.com/40075313): Enable on Android.
#if BUILDFLAG(IS_ANDROID)
  return false;
#else
  bool si_usable_by_gles2_interface = shared_image_target != 0;
  // Since skia always produces premultiply alpha outputs,
  // trying direct uploading path when video format is opaque or premultiply
  // alpha been requested.
  // TODO(crbug.com/40159723): Figure out whether premultiply options here are
  // accurate.
  bool is_premul = media::IsOpaque(video_frame_format) ||
                   dst_alpha_type == kPremul_SkAlphaType;
  bool supports_one_copy_format = ValidFormatForDirectUploading(
      static_cast<GLenum>(dst_internal_format), dst_type);
  // dst texture mipLevel must be 0.
  // TODO(crbug.com/40141173): Support more texture target, e.g.
  // 2d array, 3d etc.
  return si_usable_by_gles2_interface && dst_level == 0 && is_premul &&
         dst_target == GL_TEXTURE_2D && supports_one_copy_format;
#endif  // BUILDFLAG(IS_ANDROID)
}

SkImageInfo GetVideoImageGeneratorSkImageInfo(
    const scoped_refptr<VideoFrame>& frame) {
  const auto frame_color_space = frame->CompatRGBColorSpace();
  const auto color_type = frame->format() == PIXEL_FORMAT_RGBAF16
                              ? kRGBA_F16_SkColorType
                              : kN32_SkColorType;
  return SkImageInfo::Make(
      frame->visible_rect().width(), frame->visible_rect().height(), color_type,
      kPremul_SkAlphaType, frame_color_space.ToSkColorSpace());
}

}  // anonymous namespace

// Generates an RGB image from a VideoFrame. Convert YUV to RGB plain on GPU.
class VideoImageGenerator : public cc::PaintImageGenerator {
 public:
  VideoImageGenerator() = delete;

  VideoImageGenerator(scoped_refptr<VideoFrame> frame)
      : cc::PaintImageGenerator(GetVideoImageGeneratorSkImageInfo(frame)),
        frame_(std::move(frame)) {
    DCHECK(!frame_->HasSharedImage());
  }

  VideoImageGenerator(const VideoImageGenerator&) = delete;
  VideoImageGenerator& operator=(const VideoImageGenerator&) = delete;

  ~VideoImageGenerator() override = default;

  sk_sp<SkData> GetEncodedData() const override { return nullptr; }

  bool GetPixels(SkPixmap dst_pixmap,
                 size_t frame_index,
                 cc::PaintImage::GeneratorClientId client_id,
                 uint32_t lazy_pixel_ref) override {
    DCHECK_EQ(frame_index, 0u);

    // If skia couldn't do the YUV conversion on GPU, we will on CPU.
    PaintCanvasVideoRenderer::ConvertVideoFrameToRGBPixels(
        frame_.get(), dst_pixmap.writable_addr(), dst_pixmap.rowBytes(),
        dst_pixmap.colorType());

    if (!SkColorSpace::Equals(GetSkImageInfo().colorSpace(),
                              dst_pixmap.colorSpace())) {
      SkPixmap src(GetSkImageInfo(), dst_pixmap.writable_addr(),
                   dst_pixmap.rowBytes());
      if (!src.readPixels(dst_pixmap)) {
        return false;
      }
    }
    return true;
  }

  bool QueryYUVA(const SkYUVAPixmapInfo::SupportedDataTypes&,
                 SkYUVAPixmapInfo* info) const override {
    SkYUVAInfo::PlaneConfig plane_config;
    SkYUVAInfo::Subsampling subsampling;
    std::tie(plane_config, subsampling) =
        VideoPixelFormatAsSkYUVAInfoValues(frame_->format());
    if (plane_config == SkYUVAInfo::PlaneConfig::kUnknown) {
      return false;
    }

    // Don't use the YUV conversion path for multi-plane RGB frames.
    if (frame_->format() == PIXEL_FORMAT_I444 &&
        frame_->ColorSpace().GetMatrixID() == gfx::ColorSpace::MatrixID::GBR) {
      return false;
    }

    if (!info) {
      return true;
    }

    SkYUVColorSpace yuv_color_space;
    if (!frame_->ColorSpace().ToSkYUVColorSpace(frame_->BitDepth(),
                                                &yuv_color_space)) {
      // TODO(crbug.com/41380578): This should default to BT.709 color space.
      yuv_color_space = kRec601_SkYUVColorSpace;
    }

    auto yuva_info = SkYUVAInfo(
        {frame_->visible_rect().width(), frame_->visible_rect().height()},
        plane_config, subsampling, yuv_color_space);
    *info = SkYUVAPixmapInfo(yuva_info, SkYUVAPixmapInfo::DataType::kUnorm8,
                             /*rowBytes=*/nullptr);
    return true;
  }

  bool GetYUVAPlanes(const SkYUVAPixmaps& pixmaps,
                     size_t frame_index,
                     uint32_t lazy_pixel_ref,
                     cc::PaintImage::GeneratorClientId client_id) override {
    DCHECK_EQ(frame_index, 0u);
    DCHECK_EQ(pixmaps.numPlanes(), 3);

#if DCHECK_IS_ON()
    SkYUVAInfo::PlaneConfig plane_config;
    SkYUVAInfo::Subsampling subsampling;
    std::tie(plane_config, subsampling) =
        VideoPixelFormatAsSkYUVAInfoValues(frame_->format());
    DCHECK_EQ(plane_config, pixmaps.yuvaInfo().planeConfig());
    DCHECK_EQ(subsampling, pixmaps.yuvaInfo().subsampling());
#endif

    for (int plane = VideoFrame::Plane::kY; plane <= VideoFrame::Plane::kV;
         ++plane) {
      const auto plane_size = VideoFrame::PlaneSizeInSamples(
          frame_->format(), plane, frame_->visible_rect().size());
      if (plane_size.width() != pixmaps.plane(plane).width() ||
          plane_size.height() != pixmaps.plane(plane).height()) {
        return false;
      }

      const auto& out_plane = pixmaps.plane(plane);

      // Copy the frame to the supplied memory. It'd be nice to avoid this copy,
      // but the memory is externally owned so we can't w/o an API change.
      libyuv::CopyPlane(frame_->visible_data(plane), frame_->stride(plane),
                        reinterpret_cast<uint8_t*>(out_plane.writable_addr()),
                        out_plane.rowBytes(), plane_size.width(),
                        plane_size.height());
    }
    return true;
  }

 private:
  scoped_refptr<VideoFrame> frame_;
};

class VideoTextureBacking : public cc::TextureBacking {
 public:
  explicit VideoTextureBacking(
      scoped_refptr<viz::RasterContextProvider> raster_context_provider,
      const gfx::Size& coded_size,
      const gfx::ColorSpace& color_space)
      : sk_image_info_(SkImageInfo::Make(gfx::SizeToSkISize(coded_size),
                                         kRGBA_8888_SkColorType,
                                         kPremul_SkAlphaType,
                                         color_space.ToSkColorSpace())) {
    raster_context_provider_ = std::move(raster_context_provider);
    CHECK(raster_context_provider_->ContextCapabilities().gpu_rasterization);
    auto* sii = raster_context_provider_->SharedImageInterface();

    // This SI is used to cache the VideoFrame. We copy the contents of the
    // source VideoFrame into the cached SI over the raster interface and will
    // eventually read out its contents into a destination GL texture via the
    // GLES2 interface.
    gpu::SharedImageUsageSet flags = gpu::SHARED_IMAGE_USAGE_GLES2_READ |
                                     gpu::SHARED_IMAGE_USAGE_RASTER_READ |
                                     gpu::SHARED_IMAGE_USAGE_RASTER_WRITE;
    shared_image_ =
        sii->CreateSharedImage({SHARED_IMAGE_FORMAT, coded_size, color_space,
                                flags, "PaintCanvasVideoRenderer"},
                               gpu::kNullSurfaceHandle);
    CHECK(shared_image_);
    sync_token_ = shared_image_->creation_sync_token();
  }

  ~VideoTextureBacking() override {
    gpu::SyncToken sync_token =
        gpu::RasterScopedAccess::EndAccess(std::move(ri_access_));
    auto* sii = raster_context_provider_->SharedImageInterface();
    sii->DestroySharedImage(sync_token, std::move(shared_image_));
  }

  const SkImageInfo& GetSkImageInfo() override { return sk_image_info_; }
  gpu::Mailbox GetMailbox() const override { return shared_image_->mailbox(); }
  const scoped_refptr<gpu::ClientSharedImage>& GetSharedImage() const {
    return shared_image_;
  }
  const scoped_refptr<viz::RasterContextProvider>& raster_context_provider()
      const {
    return raster_context_provider_;
  }

  void BeginAccess(gpu::raster::RasterInterface* ri) {
    CHECK(!ri_access_);
    ri_access_ =
        shared_image_->BeginRasterAccess(ri, sync_token_, /*readonly=*/true);
  }

  void clear_access() {
    CHECK(ri_access_);
    sync_token_ = gpu::RasterScopedAccess::EndAccess(std::move(ri_access_));
  }

  sk_sp<SkImage> GetSkImageViaReadback() override {
    sk_sp<SkData> image_pixels =
        SkData::MakeUninitialized(sk_image_info_.computeMinByteSize());
    if (!readPixels(sk_image_info_, image_pixels->writable_data(),
                    sk_image_info_.minRowBytes(), 0, 0)) {
      DLOG(ERROR) << "VideoTextureBacking::GetSkImageViaReadback failed.";
      return nullptr;
    }
    return SkImages::RasterFromData(sk_image_info_, std::move(image_pixels),
                                    sk_image_info_.minRowBytes());
  }

  bool readPixels(const SkImageInfo& dst_info,
                  void* dst_pixels,
                  size_t dst_row_bytes,
                  int src_x,
                  int src_y) override {
    gpu::raster::RasterInterface* ri =
        raster_context_provider_->RasterInterface();
    return ri->ReadbackImagePixels(shared_image_->mailbox(), dst_info,
                                   dst_info.minRowBytes(), src_x, src_y,
                                   /*plane_index=*/0, dst_pixels);
  }

  const gpu::SyncToken& sync_token() { return sync_token_; }
  void UpdateSyncToken(const gpu::SyncToken& sync_token) {
    sync_token_ = sync_token;
  }

 private:
  SkImageInfo sk_image_info_;
  scoped_refptr<viz::RasterContextProvider> raster_context_provider_;

  // This is a newly allocated shared image if a copy or conversion was
  // necessary.
  scoped_refptr<gpu::ClientSharedImage> shared_image_;

  std::unique_ptr<gpu::RasterScopedAccess> ri_access_;
  gpu::SyncToken sync_token_;
};

PaintCanvasVideoRenderer::PaintCanvasVideoRenderer()
    : cache_deleting_timer_(FROM_HERE,
                            base::Seconds(kTemporaryResourceDeletionDelay),
                            this,
                            &PaintCanvasVideoRenderer::ResetCache),
      renderer_stable_id_(cc::PaintImage::GetNextId()) {}

PaintCanvasVideoRenderer::~PaintCanvasVideoRenderer() = default;

void PaintCanvasVideoRenderer::Paint(
    scoped_refptr<VideoFrame> video_frame,
    cc::PaintCanvas* canvas,
    cc::PaintFlags& flags,
    const PaintParams& params,
    viz::RasterContextProvider* raster_context_provider) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (flags.isFullyTransparent()) {
    return;
  }

  if (video_frame && video_frame->HasSharedImage()) {
    if (!raster_context_provider) {
      DLOG(ERROR)
          << "Can't render textured frames w/o viz::RasterContextProvider";
      return;  // Unable to get/create a shared main thread context.
    }
    CHECK(raster_context_provider->ContextCapabilities().gpu_rasterization);
  }

  gfx::RectF dest_rect = params.dest_rect.value_or(
      video_frame ? gfx::RectF(gfx::SizeF(video_frame->visible_rect().size()))
                  : gfx::RectF());
  SkRect dest;
  dest.setLTRB(dest_rect.x(), dest_rect.y(), dest_rect.right(),
               dest_rect.bottom());

  // Paint black rectangle if there isn't a frame available or the
  // frame has an unexpected format.
  if (!video_frame.get() || video_frame->natural_size().IsEmpty() ||
      !(media::IsYuvPlanar(video_frame->format()) ||
        video_frame->format() == PIXEL_FORMAT_Y16 ||
        video_frame->format() == PIXEL_FORMAT_ARGB ||
        video_frame->format() == PIXEL_FORMAT_XRGB ||
        video_frame->format() == PIXEL_FORMAT_ABGR ||
        video_frame->format() == PIXEL_FORMAT_XBGR ||
        video_frame->format() == PIXEL_FORMAT_RGBAF16 ||
        video_frame->HasSharedImage())) {
    cc::PaintFlags black_with_alpha_flags;
    black_with_alpha_flags.setAlphaf(flags.getAlphaf());
    canvas->drawRect(dest, black_with_alpha_flags);
    canvas->flush();
    return;
  }

  // We want to be able to cache the PaintImage, to avoid redundant readbacks if
  // the canvas is software.
  // We do not need to synchronize video frame read here since it's already
  // taken care of in UpdateLastImage().
  if (!UpdateLastImage(video_frame, raster_context_provider)) {
    return;
  }
  DCHECK(cache_);
  cc::PaintImage image = cache_->paint_image;
  if (params.reinterpret_as_srgb) {
    image = cc::PaintImageBuilder::WithCopy(image)
                .set_reinterpret_as_srgb(true)
                .TakePaintImage();
  }
  DCHECK(image);

  cc::PaintFlags video_flags;
  video_flags.setAlphaf(flags.getAlphaf());
  video_flags.setBlendMode(flags.getBlendMode());
  video_flags.setTargetedHdrHeadroom(flags.getTargetedHdrHeadroom());

  const bool need_rotation = params.transformation.rotation != VIDEO_ROTATION_0;
  const bool need_scaling =
      dest_rect.size() != gfx::SizeF(video_frame->visible_rect().size());
  const bool need_translation = !dest_rect.origin().IsOrigin();
  const bool needs_mirror = params.transformation.mirrored;
  bool need_transform =
      need_rotation || need_scaling || need_translation || needs_mirror;
  if (need_transform) {
    canvas->save();
    canvas->translate(
        SkFloatToScalar(dest_rect.x() + (dest_rect.width() * 0.5f)),
        SkFloatToScalar(dest_rect.y() + (dest_rect.height() * 0.5f)));
    SkScalar angle = SkFloatToScalar(0.0f);
    switch (params.transformation.rotation) {
      case VIDEO_ROTATION_0:
        break;
      case VIDEO_ROTATION_90:
        angle = SkFloatToScalar(90.0f);
        break;
      case VIDEO_ROTATION_180:
        angle = SkFloatToScalar(180.0f);
        break;
      case VIDEO_ROTATION_270:
        angle = SkFloatToScalar(270.0f);
        break;
    }
    canvas->rotate(angle);

    gfx::SizeF rotated_dest_size = dest_rect.size();

    const bool has_flipped_size =
        params.transformation.rotation == VIDEO_ROTATION_90 ||
        params.transformation.rotation == VIDEO_ROTATION_270;
    if (has_flipped_size) {
      rotated_dest_size =
          gfx::SizeF(rotated_dest_size.height(), rotated_dest_size.width());
    }
    auto sx = SkFloatToScalar(rotated_dest_size.width() /
                              video_frame->visible_rect().width());
    auto sy = SkFloatToScalar(rotated_dest_size.height() /
                              video_frame->visible_rect().height());
    if (needs_mirror) {
      if (has_flipped_size) {
        sy *= -1;
      } else {
        sx *= -1;
      }
    }
    canvas->scale(sx, sy);
    canvas->translate(
        -SkFloatToScalar(video_frame->visible_rect().width() * 0.5f),
        -SkFloatToScalar(video_frame->visible_rect().height() * 0.5f));
  }

  SkImageInfo info;
  size_t row_bytes;
  SkIPoint origin;
  void* pixels = nullptr;
  // This if is a special handling of video for SkiaPaintcanvas backend, where
  // the video does not need any transform and it is enough to draw the frame
  // directly into the skia canvas
  if (!need_transform && !params.reinterpret_as_srgb &&
      video_frame->IsMappable() && flags.isOpaque() &&
      flags.getBlendMode() == SkBlendMode::kSrc &&
      flags.getFilterQuality() == cc::PaintFlags::FilterQuality::kLow &&
      (pixels = canvas->accessTopLayerPixels(&info, &row_bytes, &origin)) &&
      info.colorType() == kBGRA_8888_SkColorType) {
    const size_t offset = info.computeOffset(origin.x(), origin.y(), row_bytes);
    void* const pixels_offset = reinterpret_cast<char*>(pixels) + offset;
    ConvertVideoFrameToRGBPixels(video_frame.get(), pixels_offset, row_bytes,
                                 kBGRA_8888_SkColorType);
  } else if (video_frame->HasSharedImage()) {
    DCHECK_EQ(video_frame->coded_size(),
              gfx::Size(image.width(), image.height()));
    canvas->drawImageRect(image, gfx::RectToSkRect(video_frame->visible_rect()),
                          SkRect::MakeWH(video_frame->visible_rect().width(),
                                         video_frame->visible_rect().height()),
                          cc::PaintFlags::FilterQualityToSkSamplingOptions(
                              flags.getFilterQuality()),
                          &video_flags, SkCanvas::kStrict_SrcRectConstraint);
  } else {
    DCHECK_EQ(video_frame->visible_rect().size(),
              gfx::Size(image.width(), image.height()));
    canvas->drawImage(image, 0, 0,
                      cc::PaintFlags::FilterQualityToSkSamplingOptions(
                          flags.getFilterQuality()),
                      &video_flags);
  }

  if (need_transform) {
    canvas->restore();
  }
  // Make sure to flush so we can remove the videoframe from the generator.
  canvas->flush();
}

void PaintCanvasVideoRenderer::Copy(
    scoped_refptr<VideoFrame> video_frame,
    cc::PaintCanvas* canvas,
    viz::RasterContextProvider* raster_context_provider) {
  CHECK(!raster_context_provider ||
        raster_context_provider->ContextCapabilities().gpu_rasterization);

  cc::PaintFlags flags;
  flags.setBlendMode(SkBlendMode::kSrc);
  flags.setFilterQuality(cc::PaintFlags::FilterQuality::kLow);

  Paint(std::move(video_frame), canvas, flags, PaintParams(),
        raster_context_provider);
}

namespace {

// libyuv doesn't support all 9-, 10- nor 12-bit pixel formats yet. This
// function creates a regular 8-bit video frame which we can give to libyuv.
scoped_refptr<VideoFrame> DownShiftHighbitVideoFrame(
    const VideoFrame* video_frame) {
  VideoPixelFormat format;
  switch (video_frame->format()) {
    case PIXEL_FORMAT_YUV420P12:
    case PIXEL_FORMAT_YUV420P10:
      format = PIXEL_FORMAT_I420;
      break;

    case PIXEL_FORMAT_YUV422P12:
    case PIXEL_FORMAT_YUV422P10:
      format = PIXEL_FORMAT_I422;
      break;

    case PIXEL_FORMAT_YUV444P12:
    case PIXEL_FORMAT_YUV444P10:
      format = PIXEL_FORMAT_I444;
      break;

    default:
      NOTREACHED();
  }
  const int scale = 1 << (24 - video_frame->BitDepth());
  scoped_refptr<VideoFrame> ret = VideoFrame::CreateFrame(
      format, video_frame->coded_size(), video_frame->visible_rect(),
      video_frame->natural_size(), video_frame->timestamp());

  ret->set_color_space(video_frame->ColorSpace());
  // Copy all metadata.
  // (May be enough to copy color space)
  ret->metadata().MergeMetadataFrom(video_frame->metadata());

  for (int plane = VideoFrame::Plane::kY; plane <= VideoFrame::Plane::kV;
       ++plane) {
    int width = VideoFrame::Columns(plane, video_frame->format(),
                                    video_frame->visible_rect().width());
    int height = VideoFrame::Rows(plane, video_frame->format(),
                                  video_frame->visible_rect().height());
    const uint16_t* src =
        reinterpret_cast<const uint16_t*>(video_frame->visible_data(plane));
    uint8_t* dst = ret->GetWritableVisibleData(plane);
    if (!src) {
      // An AV1 monochrome (grayscale) frame has no U and V planes. Set all U
      // and V samples to the neutral value (128).
      DCHECK_NE(plane, VideoFrame::Plane::kY);
      memset(dst, 128, height * ret->stride(plane));
      continue;
    }
    libyuv::Convert16To8Plane(src, video_frame->stride(plane) / 2, dst,
                              ret->stride(plane), scale, width, height);
  }
  return ret;
}

// Converts 16-bit data to |out| buffer of specified GL |type|.
// When the |format| is RGBA, the converted value is fed as luminance.
void FlipAndConvertY16(const VideoFrame* video_frame,
                       uint8_t* out,
                       unsigned format,
                       unsigned type,
                       bool flip_y,
                       size_t output_row_bytes) {
  const uint8_t* row_head = video_frame->visible_data(0);
  const size_t stride = video_frame->stride(0);
  const int height = video_frame->visible_rect().height();
  for (int i = 0; i < height; ++i, row_head += stride) {
    uint8_t* out_row_head = flip_y ? out + output_row_bytes * (height - i - 1)
                                   : out + output_row_bytes * i;
    const uint16_t* row = reinterpret_cast<const uint16_t*>(row_head);
    const uint16_t* row_end = row + video_frame->visible_rect().width();
    if (type == GL_FLOAT) {
      float* out_row = reinterpret_cast<float*>(out_row_head);
      if (format == GL_RGBA) {
        while (row < row_end) {
          float gray_value = *row++ / 65535.f;
          *out_row++ = gray_value;
          *out_row++ = gray_value;
          *out_row++ = gray_value;
          *out_row++ = 1.0f;
        }
        continue;
      } else if (format == GL_RED) {
        while (row < row_end) {
          *out_row++ = *row++ / 65535.f;
        }
        continue;
      }
      // For other formats, hit NOTREACHED below.
    } else if (type == GL_UNSIGNED_BYTE) {
      // We take the upper 8 bits of 16-bit data and convert it as luminance to
      // ARGB.  We loose the precision here, but it is important not to render
      // Y16 as RG_88.  To get the full precision use float textures with WebGL1
      // and e.g. R16UI or R32F textures with WebGL2.
      DCHECK_EQ(static_cast<unsigned>(GL_RGBA), format);
      uint32_t* rgba = reinterpret_cast<uint32_t*>(out_row_head);
      while (row < row_end) {
        uint32_t gray_value = *row++ >> 8;
        *rgba++ = SkColorSetRGB(gray_value, gray_value, gray_value);
      }
      continue;
    }
    NOTREACHED() << "Unsupported Y16 conversion for format: 0x" << std::hex
                 << format << " and type: 0x" << std::hex << type;
  }
}

// Common functionality of PaintCanvasVideoRenderer's TexImage2D and
// TexSubImage2D. Allocates a buffer required for conversion and converts
// |frame| content to desired |format|. Returns true if calling glTex(Sub)Image
// is supported for provided |frame| format and parameters.
bool TexImageHelper(VideoFrame* frame,
                    unsigned format,
                    unsigned type,
                    GrSurfaceOrigin dst_origin,
                    scoped_refptr<DataBuffer>* temp_buffer) {
  unsigned output_bytes_per_pixel = 0;
  switch (frame->format()) {
    case PIXEL_FORMAT_Y16:
      // Converting single component unsigned short here to FLOAT luminance.
      switch (format) {
        case GL_RGBA:
          if (type == GL_FLOAT) {
            output_bytes_per_pixel = 4 * sizeof(GLfloat);
            break;
          }
          return false;
        case GL_RED:
          if (type == GL_FLOAT) {
            output_bytes_per_pixel = sizeof(GLfloat);
            break;
          }
          return false;
        default:
          return false;
      }
      break;
    default:
      return false;
  }

  // VideoFrame that isn't backed by shared image always top-left, so if
  // destination is bottom-left we need to flip.
  const bool flip_y = dst_origin != kTopLeft_GrSurfaceOrigin;

  size_t output_row_bytes =
      frame->visible_rect().width() * output_bytes_per_pixel;
  *temp_buffer = base::MakeRefCounted<DataBuffer>(
      output_row_bytes * frame->visible_rect().height());
  FlipAndConvertY16(frame, (*temp_buffer)->writable_data().data(), format, type,
                    flip_y, output_row_bytes);
  return true;
}

// Upload the |frame| data to temporary texture of |temp_format|,
// |temp_internalformat| and |temp_type| and then copy intermediate texture
// subimage to destination |texture|. The destination |texture| is bound to the
// |target| before the call.
void TextureSubImageUsingIntermediate(unsigned target,
                                      unsigned texture,
                                      gpu::gles2::GLES2Interface* gl,
                                      VideoFrame* frame,
                                      int temp_internalformat,
                                      unsigned temp_format,
                                      unsigned temp_type,
                                      int level,
                                      int xoffset,
                                      int yoffset,
                                      GrSurfaceOrigin dst_origin,
                                      SkAlphaType dst_alpha_type) {
  unsigned temp_texture = 0;
  gl->GenTextures(1, &temp_texture);
  gl->BindTexture(target, temp_texture);
  gl->TexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  gl->TexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  gl->TexImage2D(target, 0, temp_internalformat, frame->visible_rect().width(),
                 frame->visible_rect().height(), 0, temp_format, temp_type,
                 frame->visible_data(0));
  gl->BindTexture(target, texture);

  // VideoFrame that is not backed by shared image has always top-left origin.
  // We uploaded data to `temp_texture` as is, so need to flip when copy to
  // destination if it's bottom left.
  const bool do_flip_y = dst_origin != kTopLeft_GrSurfaceOrigin;

  // VideoFrame data is not premultiplied, so we need to premultiply if
  // requested.
  const bool do_premultiply_alpha = dst_alpha_type == kPremul_SkAlphaType;

  gl->CopySubTextureCHROMIUM(temp_texture, 0, target, texture, level, 0, 0,
                             xoffset, yoffset, frame->visible_rect().width(),
                             frame->visible_rect().height(), do_flip_y,
                             do_premultiply_alpha, false);
  gl->DeleteTextures(1, &temp_texture);
}

}  // anonymous namespace

// static
void PaintCanvasVideoRenderer::ConvertVideoFrameToRGBPixels(
    const VideoFrame* video_frame,
    void* rgb_pixels,
    size_t row_bytes,
    SkColorType dst_color_type,
    bool premultiply_alpha,
    FilterMode filter,
    bool disable_threading) {
  if (!video_frame->IsMappable()) {
    NOTREACHED() << "Cannot extract pixels from non-CPU frame formats.";
  }

  SkPixmap dst_pixmap(
      SkImageInfo::Make(
          gfx::SizeToSkISize(video_frame->visible_rect().size()),
          dst_color_type,
          premultiply_alpha ? kPremul_SkAlphaType : kUnpremul_SkAlphaType),
      rgb_pixels, row_bytes);

  scoped_refptr<VideoFrame> temporary_frame;
  // TODO(thomasanderson): Parallelize converting these formats.
  switch (video_frame->format()) {
    case PIXEL_FORMAT_YUV422P12:
    case PIXEL_FORMAT_YUV444P12:
      temporary_frame = DownShiftHighbitVideoFrame(video_frame);
      video_frame = temporary_frame.get();
      break;
    case PIXEL_FORMAT_YUV420P10:
    case PIXEL_FORMAT_YUV420P12:
      // In AV1, a monochrome (grayscale) frame is represented as a YUV 4:2:0
      // frame with no U and V planes. Since there are no 10-bit and 12-bit
      // versions of libyuv::I400ToARGBMatrix(), convert the frame to an 8-bit
      // YUV 4:2:0 frame with U and V planes.
      if (!video_frame->data(VideoFrame::Plane::kU) &&
          !video_frame->data(VideoFrame::Plane::kV)) {
        temporary_frame = DownShiftHighbitVideoFrame(video_frame);
        video_frame = temporary_frame.get();
      }
      break;
    case PIXEL_FORMAT_Y16:
      // Since it is grayscale conversion, we disregard
      // SK_PMCOLOR_BYTE_ORDER and always use GL_RGBA.
      FlipAndConvertY16(
          video_frame, reinterpret_cast<uint8_t*>(dst_pixmap.writable_addr()),
          GL_RGBA, GL_UNSIGNED_BYTE, false /*flip_y*/, dst_pixmap.rowBytes());
      return;
    default:
      break;
  }

  const size_t n_tasks =
      disable_threading ? 1 : NumConvertVideoFrameToRGBPixelsTasks(video_frame);
  base::WaitableEvent event;
  base::RepeatingClosure barrier = base::BarrierClosure(
      n_tasks,
      base::BindOnce(&base::WaitableEvent::Signal, base::Unretained(&event)));

  const libyuv::FilterMode libyuv_filter = ToLibyuvFilterMode(filter);
  for (size_t i = 1; i < n_tasks; ++i) {
    base::ThreadPool::PostTask(
        FROM_HERE, base::BindOnce(ConvertVideoFrameToRGBPixelsTask,
                                  base::Unretained(video_frame), dst_pixmap,
                                  libyuv_filter, i, n_tasks, &barrier));
  }
  ConvertVideoFrameToRGBPixelsTask(video_frame, dst_pixmap, libyuv_filter, 0,
                                   n_tasks, &barrier);
  {
    base::ScopedAllowBaseSyncPrimitivesOutsideBlockingScope allow_wait;
    event.Wait();
  }
}

// static
viz::SharedImageFormat PaintCanvasVideoRenderer::GetRGBPixelsOutputFormat() {
  return SHARED_IMAGE_FORMAT;
}

bool PaintCanvasVideoRenderer::CopyVideoFrameTexturesToGLTexture(
    viz::RasterContextProvider* raster_context_provider,
    gpu::gles2::GLES2Interface* destination_gl,
    scoped_refptr<VideoFrame> video_frame,
    unsigned int target,
    unsigned int texture,
    unsigned int internal_format,
    unsigned int format,
    unsigned int type,
    int level,
    SkAlphaType dst_alpha_type,
    GrSurfaceOrigin dst_origin) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(video_frame);
  CHECK(video_frame->HasSharedImage());
  CHECK(destination_gl);

  const auto shared_image = video_frame->shared_image();
  const auto si_format = shared_image->format();
  const bool si_format_has_single_texture =
      si_format.is_single_plane() || si_format.PrefersExternalSampler();
  const bool si_usable_by_gles2_interface =
      shared_image->GetTextureTarget() != 0;

  // Copying shared image using GL directly require shared image to be either
  // single plane or external sampler, and should be usable by GL.
  if (si_format_has_single_texture && si_usable_by_gles2_interface) {
    CopySharedImageToTexture(
        destination_gl, video_frame->coded_size(), video_frame->visible_rect(),
        shared_image.get(), video_frame->acquire_sync_token(), target, texture,
        internal_format, format, type, level, dst_alpha_type, dst_origin);
    destination_gl->ShallowFlushCHROMIUM();

    SynchronizeVideoFrameRead(std::move(video_frame), destination_gl,
                              raster_context_provider->ContextSupport());
    return true;
  }

  // It is not possible to support one-copy upload of pure software
  // VideoFrames via MultiplanarSharedImage: these VideoFrames have format
  // I420, and it is not possible across all platforms to upload the
  // VideoFrame's data via raster to a MultiplanarSI with format I420 that is
  // accessible by WebGL. Such an SI must be backed by a native buffer to be
  // accessible to WebGL, and native buffer-backed I420 SharedImages are in
  // general not supported (and *cannot* be supported on Windows). NOTE:
  // Whether 1 GPU-GPU copy or 2 GPU-GPU copies are performed for pure video
  // software upload should not be a significant factor in performance, as the
  // dominant factor in terms of performance will be the fact that the
  // VideoFrame's data needs to be uploaded from the CPU to the GPU.
  if (SupportsOneCopyUploadToGLTexture(
          video_frame->format(), shared_image->GetTextureTarget(), target,
          internal_format, type, level, dst_alpha_type)) {
    // Trigger resource allocation for dst texture to back SkSurface.
    // Dst texture size should equal to video frame visible rect.
    BindAndTexImage2D(destination_gl, target, texture, internal_format, format,
                      type, /*level=*/0, video_frame->visible_rect().size());

    auto destination_access = shared_image->BeginGLAccessForCopySharedImage(
        destination_gl, video_frame->acquire_sync_token(), /*readonly=*/true);

    // Copy shared image to gl texture for hardware video decode with
    // multiplanar shared image formats.
    const bool is_dst_origin_top_left = dst_origin == kTopLeft_GrSurfaceOrigin;
    destination_gl->CopySharedImageToTextureINTERNAL(
        texture, target, internal_format, type, video_frame->visible_rect().x(),
        video_frame->visible_rect().y(), video_frame->visible_rect().width(),
        video_frame->visible_rect().height(), is_dst_origin_top_left,
        shared_image->mailbox().name);

    SynchronizeVideoFrameRead(std::move(video_frame), destination_gl,
                              raster_context_provider->ContextSupport(),
                              std::move(destination_access));
    return true;
  }

  DCHECK_EQ(shared_image->surface_origin(), kTopLeft_GrSurfaceOrigin);
  if (!raster_context_provider) {
    return false;
  }
  CHECK(raster_context_provider->ContextCapabilities().gpu_rasterization);
  gpu::raster::RasterInterface* canvas_ri =
      raster_context_provider->RasterInterface();
  DCHECK(canvas_ri);

  // Take the two-copy path.
  // Create the intermediate rgb shared image cache if not already present.
  if (!rgb_shared_image_cache_) {
    rgb_shared_image_cache_ = std::make_unique<VideoFrameSharedImageCache>();
  }

  // This SI is used to cache the VideoFrame. We copy the contents of the source
  // VideoFrame into the cached SI over the raster interface and will eventually
  // read out its contents into a destination GL texture via the GLES2
  // interface.
  gpu::SharedImageUsageSet src_usage =
      gpu::SHARED_IMAGE_USAGE_GLES2_READ | gpu::SHARED_IMAGE_USAGE_RASTER_WRITE;
  auto [rgb_shared_image, rgb_sync_token, status] =
      rgb_shared_image_cache_->GetOrCreateSharedImage(
          video_frame.get(), raster_context_provider, src_usage,
          SHARED_IMAGE_FORMAT, kPremul_SkAlphaType,
          video_frame->CompatRGBColorSpace());
  CHECK(rgb_shared_image);

  // Wait on the `rgb_sync_token` passed from the cache that may have been
  // updated from the previous frame.
  std::unique_ptr<gpu::RasterScopedAccess> dst_ri_access =
      rgb_shared_image->BeginRasterAccess(canvas_ri, rgb_sync_token,
                                          /*readonly=*/false);

  // If there's no cache hit, perform a copy.
  if (status != VideoFrameSharedImageCache::Status::kMatchedVideoFrameId) {
    // Copy into the shared image backing of the cached copy.
    std::unique_ptr<gpu::RasterScopedAccess> src_ri_access =
        shared_image->BeginRasterAccess(canvas_ri,
                                        video_frame->acquire_sync_token(),
                                        /*readonly=*/true);
    canvas_ri->CopySharedImage(
        shared_image->mailbox(), rgb_shared_image->mailbox(), 0, 0, 0, 0,
        video_frame->coded_size().width(), video_frame->coded_size().height());

    // Ensure that |video_frame| not be deleted until the above copy is
    // completed.
    SynchronizeVideoFrameRead(video_frame, canvas_ri,
                              raster_context_provider->ContextSupport(),
                              std::move(src_ri_access));
  }

  // Wait for mailbox creation on canvas context before consuming it and
  // copying from it on the consumer context.
  gpu::SyncToken sync_token =
      gpu::RasterScopedAccess::EndAccess(std::move(dst_ri_access));

  gpu::SyncToken dest_sync_token = CopySharedImageToTexture(
      destination_gl, video_frame->coded_size(), video_frame->visible_rect(),
      rgb_shared_image.get(), sync_token, target, texture, internal_format,
      format, type, level, dst_alpha_type, dst_origin);

  // Update the `rgb_sync_token` to be waited upon based on gles tasks performed
  // earlier.
  rgb_shared_image_cache_->UpdateSyncToken(dest_sync_token);

  // We do not need to synchronize video frame read here since it's already
  // taken care of earlier.
  // Kick off a timer to release the cache.
  cache_deleting_timer_.Reset();
  return true;
}

bool PaintCanvasVideoRenderer::CopyVideoFrameYUVDataToGLTexture(
    viz::RasterContextProvider* raster_context_provider,
    gpu::gles2::GLES2Interface* destination_gl,
    scoped_refptr<VideoFrame> video_frame,
    unsigned int target,
    unsigned int texture,
    unsigned int internal_format,
    unsigned int format,
    unsigned int type,
    int level,
    SkAlphaType dst_alpha_type,
    GrSurfaceOrigin dst_origin) {
  if (!raster_context_provider) {
    return false;
  }
#if BUILDFLAG(IS_ANDROID)
  // TODO(crbug.com/40751207): These formats don't work with the passthrough
  // command decoder on Android for some reason.
  const auto format_enum = static_cast<GLenum>(internal_format);
  if (format_enum == GL_RGB10_A2 || format_enum == GL_RGB565 ||
      format_enum == GL_RGB5_A1 || format_enum == GL_RGBA4) {
    return false;
  }
#endif

  if (!video_frame->IsMappable()) {
    return false;
  }

  if (!internals::IsPixelFormatSupportedForYuvSharedImageConversion(
          video_frame->format())) {
    return false;
  }
  // Could handle NV12 here as well. See NewSkImageFromVideoFrameYUV.

  CHECK(!video_frame->HasSharedImage());

  // We copy the contents of the source VideoFrame into the intermediate SI
  // over the raster interface and read out the contents of the intermediate
  // SI into the destination GL texture via the GLES2 interface.
  CHECK(raster_context_provider->ContextCapabilities().gpu_rasterization);
  gpu::SharedImageUsageSet src_usage =
      gpu::SHARED_IMAGE_USAGE_RASTER_WRITE | gpu::SHARED_IMAGE_USAGE_GLES2_READ;

  // Recreate both the caches if not set.
  if (!rgb_shared_image_cache_) {
    rgb_shared_image_cache_ = std::make_unique<VideoFrameSharedImageCache>();
  }
  if (!yuv_shared_image_cache_) {
    yuv_shared_image_cache_ = std::make_unique<VideoFrameSharedImageCache>();
  }

  // We need a shared image to receive the intermediate RGB result. Try to reuse
  // one if compatible, otherwise create a new one.
  auto [rgb_shared_image, rgb_sync_token, status] =
      rgb_shared_image_cache_->GetOrCreateSharedImage(
          video_frame.get(), raster_context_provider, src_usage,
          SHARED_IMAGE_FORMAT, kPremul_SkAlphaType,
          video_frame->CompatRGBColorSpace());
  CHECK(rgb_shared_image);

  // On the source Raster context, do the YUV->RGB conversion.
  // Pass the rgb sync token here to be waited upon before performing raster
  // tasks.
  gpu::SyncToken post_conversion_sync_token =
      internals::ConvertYuvVideoFrameToRgbSharedImage(
          video_frame.get(), raster_context_provider, rgb_shared_image,
          rgb_sync_token, /*use_visible_rect=*/false,
          yuv_shared_image_cache_.get());

  // On the destination GL context, do a copy (with cropping) into the
  // destination texture.
  rgb_sync_token = CopySharedImageToTexture(
      destination_gl, video_frame->coded_size(), video_frame->visible_rect(),
      rgb_shared_image.get(), post_conversion_sync_token, target, texture,
      internal_format, format, type, level, dst_alpha_type, dst_origin);

  // Update the rgb sync token to be waited upon based on gles tasks performed
  // earlier.
  rgb_shared_image_cache_->UpdateSyncToken(rgb_sync_token);

  // video_frame->UpdateReleaseSyncToken is not necessary since the video frame
  // data we used was CPU-side (IsMappable) to begin with. If there were any
  // textures, we didn't use them.

  // Kick off a timer to release the cache.
  cache_deleting_timer_.Reset();
  return true;
}

bool PaintCanvasVideoRenderer::TexImage2D(
    unsigned target,
    unsigned texture,
    gpu::gles2::GLES2Interface* gl,
    const gpu::Capabilities& gpu_capabilities,
    VideoFrame* frame,
    int level,
    int internalformat,
    unsigned format,
    unsigned type,
    GrSurfaceOrigin dst_origin,
    SkAlphaType dst_alpha_type) {
  DCHECK(frame);
  DCHECK(!frame->HasSharedImage());

  GLint precision = 0;
  GLint range[2] = {0, 0};
  gl->GetShaderPrecisionFormat(GL_FRAGMENT_SHADER, GL_MEDIUM_FLOAT, range,
                               &precision);

  // Note: CopyTextureCHROMIUM uses mediump for color computation. Don't use
  // it if the precision would lead to data loss when converting 16-bit
  // normalized to float. medium_float.precision > 15 means that the approach
  // below is not used on Android, where the extension EXT_texture_norm16 is
  // not widely supported. It is used on Windows, Linux and OSX.
  // Android support is not required for now because Tango depth camera already
  // provides floating point data (projected point cloud). See crbug.com/674440.
  if (gpu_capabilities.texture_norm16 && precision > 15 &&
      target == GL_TEXTURE_2D &&
      (type == GL_FLOAT || type == GL_UNSIGNED_BYTE)) {
    // TODO(aleksandar.stojiljkovic): Extend the approach to TexSubImage2D
    // implementation and other types. See https://crbug.com/624436.

    // Allocate the destination texture.
    gl->TexImage2D(target, level, internalformat, frame->visible_rect().width(),
                   frame->visible_rect().height(), 0, format, type, nullptr);
    // We use sized internal format GL_R16_EXT instead of unsized GL_RED.
    // See angleproject:1952
    TextureSubImageUsingIntermediate(target, texture, gl, frame, GL_R16_EXT,
                                     GL_RED, GL_UNSIGNED_SHORT, level, 0, 0,
                                     dst_origin, dst_alpha_type);
    return true;
  }
  scoped_refptr<DataBuffer> temp_buffer;
  if (!TexImageHelper(frame, format, type, dst_origin, &temp_buffer)) {
    return false;
  }

  gl->TexImage2D(target, level, internalformat, frame->visible_rect().width(),
                 frame->visible_rect().height(), 0, format, type,
                 temp_buffer->data().data());
  return true;
}

bool PaintCanvasVideoRenderer::TexSubImage2D(unsigned target,
                                             gpu::gles2::GLES2Interface* gl,
                                             VideoFrame* frame,
                                             int level,
                                             unsigned format,
                                             unsigned type,
                                             int xoffset,
                                             int yoffset,
                                             GrSurfaceOrigin dst_origin,
                                             SkAlphaType dst_alpha_type) {
  DCHECK(frame);
  DCHECK(!frame->HasSharedImage());

  scoped_refptr<DataBuffer> temp_buffer;
  if (!TexImageHelper(frame, format, type, dst_origin, &temp_buffer)) {
    return false;
  }

  gl->TexSubImage2D(
      target, level, xoffset, yoffset, frame->visible_rect().width(),
      frame->visible_rect().height(), format, type, temp_buffer->data().data());
  return true;
}

void PaintCanvasVideoRenderer::ResetCache() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  cache_.reset();
  rgb_shared_image_cache_.reset();
  yuv_shared_image_cache_.reset();
  // ClientSharedImage destructor calls DestroySharedImage which in turn ensures
  // that the deferred destroy request is flushed. Thus, clients don't need to
  // call SharedImageInterface::Flush explicitly.
}

PaintCanvasVideoRenderer::Cache::Cache(VideoFrame::ID frame_id)
    : frame_id(frame_id) {}

PaintCanvasVideoRenderer::Cache::~Cache() = default;

bool PaintCanvasVideoRenderer::Cache::Recycle() {
  paint_image = cc::PaintImage();
  return texture_backing->unique();
}

bool PaintCanvasVideoRenderer::UpdateLastImage(
    scoped_refptr<VideoFrame> video_frame,
    viz::RasterContextProvider* raster_context_provider) {
  // Check for a cache hit.
  if (cache_ && video_frame->unique_id() == cache_->frame_id &&
      cache_->paint_image) {
    cache_deleting_timer_.Reset();
    return true;
  }

  auto paint_image_builder =
      cc::PaintImageBuilder::WithDefault()
          .set_id(renderer_stable_id_)
          .set_animation_type(cc::PaintImage::AnimationType::kVideo)
          .set_completion_state(cc::PaintImage::CompletionState::kDone);

  // Generate a new image.
  // Note: Skia will hold onto |video_frame| via |video_generator| only when
  // |video_frame| is software.
  // Holding |video_frame| longer than this call when using GPUVideoDecoder
  // could cause problems since the pool of VideoFrames has a fixed size.
  if (video_frame->HasSharedImage()) {
    CHECK(raster_context_provider);
    CHECK(raster_context_provider->ContextCapabilities().gpu_rasterization);
    auto* ri = raster_context_provider->RasterInterface();
    DCHECK(ri);
    const auto video_frame_si = video_frame->shared_image();

    // Create or reuse a texture backing for the cached copy.
    if (cache_ && cache_->texture_backing &&
        cache_->texture_backing->raster_context_provider() ==
            raster_context_provider &&
        cache_->coded_size == video_frame->coded_size() && cache_->Recycle()) {
      // We can reuse the shared image from the previous cache.
      cache_->frame_id = video_frame->unique_id();

      // NOTE: It is necessary to let go of read access to the cached copy
      // here because the below copy operation takes readwrite access to that
      // cached copy, and requesting RW access while already holding R access
      // on a single service-side texture causes a DCHECK to fire.
      cache_->texture_backing->clear_access();
    } else {
      cache_.emplace(video_frame->unique_id());
      cache_->texture_backing = sk_make_sp<VideoTextureBacking>(
          raster_context_provider, video_frame->coded_size(),
          video_frame->CompatRGBColorSpace());
    }
    scoped_refptr<gpu::ClientSharedImage> client_shared_image =
        cache_->texture_backing->GetSharedImage();

    // Copy into the shared image backing of the cached copy.
    std::unique_ptr<gpu::RasterScopedAccess> dst_ri_access =
        client_shared_image->BeginRasterAccess(
            ri, cache_->texture_backing->sync_token(),
            /*readonly=*/false);
    std::unique_ptr<gpu::RasterScopedAccess> src_ri_access =
        video_frame_si->BeginRasterAccess(ri, video_frame->acquire_sync_token(),
                                          /*readonly=*/true);
    ri->CopySharedImage(
        video_frame_si->mailbox(), client_shared_image->mailbox(), 0, 0, 0, 0,
        video_frame->coded_size().width(), video_frame->coded_size().height());

    // Ensure that |video_frame| not be deleted until the above copy is
    // completed.
    SynchronizeVideoFrameRead(video_frame, ri,
                              raster_context_provider->ContextSupport(),
                              std::move(src_ri_access));
    gpu::SyncToken sync_token =
        gpu::RasterScopedAccess::EndAccess(std::move(dst_ri_access));
    cache_->texture_backing->UpdateSyncToken(sync_token);

    cache_->coded_size = video_frame->coded_size();

    paint_image_builder.set_texture_backing(cache_->texture_backing,
                                            cc::PaintImage::GetNextContentId());
    cache_->texture_backing->BeginAccess(ri);
  } else {
    cache_.emplace(video_frame->unique_id());
    paint_image_builder.set_paint_image_generator(
        sk_make_sp<VideoImageGenerator>(video_frame));
  }

  cache_->paint_image = paint_image_builder.TakePaintImage();
  if (!cache_->paint_image) {
    // Couldn't create the PaintImage.
    cache_.reset();
    return false;
  }
  cache_deleting_timer_.Reset();
  return true;
}

bool PaintCanvasVideoRenderer::CanUseCopyVideoFrameToSharedImage(
    const VideoFrame& video_frame) {
  return video_frame.HasSharedImage() ||
         internals::IsPixelFormatSupportedForYuvSharedImageConversion(
             video_frame.format());
}

gpu::SyncToken PaintCanvasVideoRenderer::CopyVideoFrameToSharedImage(
    viz::RasterContextProvider* raster_context_provider,
    scoped_refptr<VideoFrame> video_frame,
    scoped_refptr<gpu::ClientSharedImage> dest_shared_image,
    const gpu::SyncToken& dest_sync_token,
    bool use_visible_rect) {
  auto* ri = raster_context_provider->RasterInterface();

  gpu::SyncToken sync_token;
  // If we have single source shared image, just use CopySharedImage().
  if (video_frame->HasSharedImage()) {
    auto source_rect = use_visible_rect ? video_frame->visible_rect()
                                        : gfx::Rect(video_frame->coded_size());
    std::unique_ptr<gpu::RasterScopedAccess> dst_ri_access =
        dest_shared_image->BeginRasterAccess(ri, dest_sync_token,
                                             /*readonly=*/false);
    std::unique_ptr<gpu::RasterScopedAccess> src_ri_access =
        video_frame->shared_image()->BeginRasterAccess(
            ri, video_frame->acquire_sync_token(), /*readonly=*/true);
    ri->CopySharedImage(video_frame->shared_image()->mailbox(),
                        dest_shared_image->mailbox(), 0, 0, source_rect.x(),
                        source_rect.y(), source_rect.width(),
                        source_rect.height());
    sync_token = gpu::RasterScopedAccess::EndAccess(std::move(dst_ri_access));

    // If VideoFrame has textures, we need to update SyncToken or to keep frame
    // alive until gpu is done with copy if `read_lock_fences_enabled` is set.
    // This is to make sure decoder doesn't re-use frame before copy is done.
    SynchronizeVideoFrameRead(std::move(video_frame), ri,
                              raster_context_provider->ContextSupport(),
                              std::move(src_ri_access));
  } else {
    // TODO(vasilyt): Add caching support
    sync_token = internals::ConvertYuvVideoFrameToRgbSharedImage(
        video_frame.get(), raster_context_provider, dest_shared_image,
        dest_sync_token, use_visible_rect, /*shared_image_cache=*/nullptr);
  }

  return sync_token;
}

gfx::Size PaintCanvasVideoRenderer::LastImageDimensionsForTesting() {
  DCHECK(cache_);
  DCHECK(cache_->paint_image);
  return gfx::Size(cache_->paint_image.width(), cache_->paint_image.height());
}

}  // namespace media
