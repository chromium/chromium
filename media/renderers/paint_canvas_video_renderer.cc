// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/renderers/paint_canvas_video_renderer.h"

#include <GLES3/gl3.h>
#include <limits>

#include "base/barrier_closure.h"
#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
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
#include "gpu/command_buffer/client/context_support.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/client/raster_interface.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/common/capabilities.h"
#include "gpu/command_buffer/common/mailbox_holder.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "media/base/data_buffer.h"
#include "media/base/video_frame.h"
#include "media/base/wait_and_replace_sync_token_client.h"
#include "third_party/libyuv/include/libyuv.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkImageGenerator.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/gpu/GrBackendSurface.h"
#include "third_party/skia/include/gpu/GrDirectContext.h"
#include "third_party/skia/include/gpu/gl/GrGLTypes.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/skia_conversions.h"

// Skia internal format depends on a platform. On Android it is ABGR, on others
// it's ARGB. YUV_MATRIX(), YUV_ORDER() conditionally remap YUV to YVU for ABGR.
#if SK_B32_SHIFT == 0 && SK_G32_SHIFT == 8 && SK_R32_SHIFT == 16 && \
    SK_A32_SHIFT == 24
#define OUTPUT_ARGB 1
#define LIBYUV_ABGR_TO_ARGB libyuv::ABGRToARGB
#define YUV_MATRIX(matrix) matrix
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
#define YUV_MATRIX(matrix) matrix##VU
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

// Return the full-range RGB component of the color space of this frame's
// content. This will replace several color spaces (Rec601, Rec709, and
// Apple's Rec709) with sRGB, for compatibility with existing behavior.
gfx::ColorSpace GetVideoFrameRGBColorSpacePreferringSRGB(
    const VideoFrame* frame) {
  const auto rgb_color_space = frame->ColorSpace().GetAsFullRangeRGB();
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

// This class keeps the last image drawn.
// We delete the temporary resource if it is not used for 3 seconds.
const int kTemporaryResourceDeletionDelay = 3;  // Seconds;

// Helper class that begins/ends access to a mailbox within a scope. The mailbox
// must have been imported into |texture|.
class ScopedSharedImageAccess {
 public:
  ScopedSharedImageAccess(
      gpu::gles2::GLES2Interface* gl,
      GLuint texture,
      const gpu::Mailbox& mailbox,
      GLenum access = GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM)
      : gl(gl),
        ri(nullptr),
        texture(texture),
        is_shared_image(mailbox.IsSharedImage()) {
    if (is_shared_image)
      gl->BeginSharedImageAccessDirectCHROMIUM(texture, access);
  }

  // TODO(crbug.com/1023270): Remove this ctor once we're no longer relying on
  // texture ids for Mailbox access as that is only supported on
  // RasterImplementationGLES.
  ScopedSharedImageAccess(
      gpu::raster::RasterInterface* ri,
      GLuint texture,
      const gpu::Mailbox& mailbox,
      GLenum access = GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM)
      : gl(nullptr),
        ri(ri),
        texture(texture),
        is_shared_image(mailbox.IsSharedImage()) {
    if (is_shared_image)
      ri->BeginSharedImageAccessDirectCHROMIUM(texture, access);
  }

  ~ScopedSharedImageAccess() {
    if (is_shared_image) {
      if (gl)
        gl->EndSharedImageAccessDirectCHROMIUM(texture);
      else
        ri->EndSharedImageAccessDirectCHROMIUM(texture);
    }
  }

 private:
  raw_ptr<gpu::gles2::GLES2Interface> gl;
  raw_ptr<gpu::raster::RasterInterface> ri;
  GLuint texture;
  bool is_shared_image;
};

const gpu::MailboxHolder& GetVideoFrameMailboxHolder(VideoFrame* video_frame) {
  DCHECK(video_frame->HasTextures());
  DCHECK_EQ(video_frame->NumTextures(), 1u);

  DCHECK(PIXEL_FORMAT_ARGB == video_frame->format() ||
         PIXEL_FORMAT_XRGB == video_frame->format() ||
         PIXEL_FORMAT_RGB24 == video_frame->format() ||
         PIXEL_FORMAT_ABGR == video_frame->format() ||
         PIXEL_FORMAT_XBGR == video_frame->format() ||
         PIXEL_FORMAT_XB30 == video_frame->format() ||
         PIXEL_FORMAT_XR30 == video_frame->format() ||
         PIXEL_FORMAT_NV12 == video_frame->format() ||
         PIXEL_FORMAT_NV12A == video_frame->format() ||
         PIXEL_FORMAT_RGBAF16 == video_frame->format())
      << "Format: " << VideoPixelFormatToString(video_frame->format());

  const gpu::MailboxHolder& mailbox_holder = video_frame->mailbox_holder(0);
  return mailbox_holder;
}

// Wraps a GL RGBA texture into a SkImage.
sk_sp<SkImage> WrapGLTexture(
    GLenum target,
    GLuint texture_id,
    const gfx::Size& size,
    viz::RasterContextProvider* raster_context_provider,
    bool texture_origin_is_top_left) {
  GrGLTextureInfo texture_info;
  texture_info.fID = texture_id;
  texture_info.fTarget = target;
  // TODO(bsalomon): GrGLTextureInfo::fFormat and SkColorType passed to
  // SkImage factory should reflect video_frame->format(). Update once
  // Skia supports GL_RGB. skbug.com/7533
  texture_info.fFormat = GL_RGBA8_OES;
  GrBackendTexture backend_texture(size.width(), size.height(),
                                   GrMipMapped::kNo, texture_info);
  return SkImage::MakeFromAdoptedTexture(
      raster_context_provider->GrContext(), backend_texture,
      texture_origin_is_top_left ? kTopLeft_GrSurfaceOrigin
                                 : kBottomLeft_GrSurfaceOrigin,
      kRGBA_8888_SkColorType, kPremul_SkAlphaType);
}

void CopyMailboxToTexture(gpu::gles2::GLES2Interface* gl,
                          const gfx::Size& coded_size,
                          const gfx::Rect& visible_rect,
                          const gpu::Mailbox& source_mailbox,
                          const gpu::SyncToken& source_sync_token,
                          unsigned int target,
                          unsigned int texture,
                          unsigned int internal_format,
                          unsigned int format,
                          unsigned int type,
                          int level,
                          bool premultiply_alpha,
                          bool flip_y) {
  gl->WaitSyncTokenCHROMIUM(source_sync_token.GetConstData());
  GLuint source_texture =
      source_mailbox.IsSharedImage()
          ? gl->CreateAndTexStorage2DSharedImageCHROMIUM(source_mailbox.name)
          : gl->CreateAndConsumeTextureCHROMIUM(source_mailbox.name);
  {
    ScopedSharedImageAccess access(gl, source_texture, source_mailbox);
    // The video is stored in a unmultiplied format, so premultiply if
    // necessary. Application itself needs to take care of setting the right
    // |flip_y| value down to get the expected result. "flip_y == true" means to
    // reverse the video orientation while "flip_y == false" means to keep the
    // intrinsic orientation.
    if (visible_rect != gfx::Rect(coded_size)) {
      // Must reallocate the destination texture and copy only a sub-portion.

      // There should always be enough data in the source texture to
      // cover this copy.
      DCHECK_LE(visible_rect.width(), coded_size.width());
      DCHECK_LE(visible_rect.height(), coded_size.height());

      gl->BindTexture(target, texture);
      gl->TexImage2D(target, level, internal_format, visible_rect.width(),
                     visible_rect.height(), 0, format, type, nullptr);
      gl->CopySubTextureCHROMIUM(source_texture, 0, target, texture, level, 0,
                                 0, visible_rect.x(), visible_rect.y(),
                                 visible_rect.width(), visible_rect.height(),
                                 flip_y, premultiply_alpha, false);

    } else {
      gl->CopyTextureCHROMIUM(source_texture, 0, target, texture, level,
                              internal_format, type, flip_y, premultiply_alpha,
                              false);
    }
  }
  gl->DeleteTextures(1, &source_texture);
}

// Update |video_frame|'s release sync token to reflect the work done in |ri|,
// and ensure that |video_frame| be kept remain alive until |ri|'s commands have
// been completed. This is implemented for both gpu::gles2::GLES2Interface and
// gpu::raster::RasterInterface. This function is critical to ensure that
// |video_frame|'s resources not be returned until they are no longer in use.
// https://crbug.com/819914 (software video decode frame corruption)
// https://crbug.com/1237100 (camera capture reuse corruption)
void SynchronizeVideoFrameRead(scoped_refptr<VideoFrame> video_frame,
                               gpu::raster::RasterInterface* ri,
                               gpu::ContextSupport* context_support) {
  WaitAndReplaceSyncTokenClient client(ri);
  video_frame->UpdateReleaseSyncToken(&client);
  if (!video_frame->metadata().read_lock_fences_enabled)
    return;

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

void SynchronizeVideoFrameRead(scoped_refptr<VideoFrame> video_frame,
                               gpu::gles2::GLES2Interface* gl,
                               gpu::ContextSupport* context_support) {
  WaitAndReplaceSyncTokenClient client(gl);
  video_frame->UpdateReleaseSyncToken(&client);
  if (!video_frame->metadata().read_lock_fences_enabled)
    return;

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

// TODO(thomasanderson): Remove these and use std::gcd and std::lcm once we're
// building with C++17.
size_t GCD(size_t a, size_t b) {
  return a == 0 ? b : GCD(b % a, a);
}
size_t LCM(size_t a, size_t b) {
  return a / GCD(a, b) * b;
}

const libyuv::YuvConstants* GetYuvContantsForColorSpace(SkYUVColorSpace cs) {
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
      return &YUV_MATRIX(libyuv::kYuvV2020Constants);
    case kBT2020_8bit_Limited_SkYUVColorSpace:
    case kBT2020_10bit_Limited_SkYUVColorSpace:
    case kBT2020_12bit_Limited_SkYUVColorSpace:
      return &YUV_MATRIX(libyuv::kYuv2020Constants);
    case kIdentity_SkYUVColorSpace:
      NOTREACHED();
      return nullptr;
  };
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
                                      void* rgb_pixels,
                                      size_t row_bytes,
                                      bool premultiply_alpha,
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
      rows_per_chunk =
          LCM(rows_per_chunk, VideoFrame::SampleSize(format, plane).height());
    }
  }

  base::CheckedNumeric<size_t> chunks = height / rows_per_chunk;
  const size_t chunk_start = (chunks * task_index / n_tasks).ValueOrDie();
  const size_t chunk_end = (chunks * (task_index + 1) / n_tasks).ValueOrDie();

  // Indivisible heights must process any remaining rows in the last task.
  size_t rows = (chunk_end - chunk_start) * rows_per_chunk;
  if (task_index + 1 == n_tasks)
    rows += height % rows_per_chunk;

  struct {
    int stride;
    const uint8_t* data;
  } plane_meta[VideoFrame::kMaxPlanes];

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

  uint8_t* pixels = static_cast<uint8_t*>(rgb_pixels) +
                    row_bytes * chunk_start * rows_per_chunk;

  if (format == PIXEL_FORMAT_ARGB || format == PIXEL_FORMAT_XRGB ||
      format == PIXEL_FORMAT_ABGR || format == PIXEL_FORMAT_XBGR) {
    DCHECK_LE(width, static_cast<int>(row_bytes));
    const uint8_t* data = plane_meta[VideoFrame::kARGBPlane].data;

    if ((OUTPUT_ARGB &&
         (format == PIXEL_FORMAT_ARGB || format == PIXEL_FORMAT_XRGB)) ||
        (!OUTPUT_ARGB &&
         (format == PIXEL_FORMAT_ABGR || format == PIXEL_FORMAT_XBGR))) {
      for (size_t i = 0; i < rows; i++) {
        memcpy(pixels, data, width * 4);
        pixels += row_bytes;
        data += plane_meta[VideoFrame::kARGBPlane].stride;
      }
    } else {
      LIBYUV_ABGR_TO_ARGB(plane_meta[VideoFrame::kARGBPlane].data,
                          plane_meta[VideoFrame::kARGBPlane].stride, pixels,
                          row_bytes, width, rows);
    }
    done->Run();
    return;
  }

  // TODO(crbug.com/828599): This should default to BT.709 color space.
  auto yuv_cs = kRec601_SkYUVColorSpace;
  video_frame->ColorSpace().ToSkYUVColorSpace(video_frame->BitDepth(), &yuv_cs);
  const libyuv::YuvConstants* matrix = GetYuvContantsForColorSpace(yuv_cs);

  if (!video_frame->data(VideoFrame::kUPlane) &&
      !video_frame->data(VideoFrame::kVPlane)) {
    DCHECK_EQ(format, PIXEL_FORMAT_I420);
    // For monochrome content ARGB and ABGR are interchangeable.
    libyuv::I400ToARGBMatrix(plane_meta[VideoFrame::kYPlane].data,
                             plane_meta[VideoFrame::kYPlane].stride, pixels,
                             row_bytes, matrix, width, rows);
    done->Run();
    return;
  }

  auto convert_yuv = [&](const libyuv::YuvConstants* matrix, auto&& func) {
    func(YUV_ORDER(plane_meta[VideoFrame::kYPlane].data,
                   plane_meta[VideoFrame::kYPlane].stride,
                   plane_meta[VideoFrame::kUPlane].data,
                   plane_meta[VideoFrame::kUPlane].stride,
                   plane_meta[VideoFrame::kVPlane].data,
                   plane_meta[VideoFrame::kVPlane].stride),
         pixels, row_bytes, matrix, width, rows);
  };

  auto convert_yuv_with_filter = [&](const libyuv::YuvConstants* matrix,
                                     auto&& func) {
    func(YUV_ORDER(plane_meta[VideoFrame::kYPlane].data,
                   plane_meta[VideoFrame::kYPlane].stride,
                   plane_meta[VideoFrame::kUPlane].data,
                   plane_meta[VideoFrame::kUPlane].stride,
                   plane_meta[VideoFrame::kVPlane].data,
                   plane_meta[VideoFrame::kVPlane].stride),
         pixels, row_bytes, matrix, width, rows, filter);
  };

  auto convert_yuva = [&](const libyuv::YuvConstants* matrix, auto&& func) {
    func(YUV_ORDER(plane_meta[VideoFrame::kYPlane].data,
                   plane_meta[VideoFrame::kYPlane].stride,
                   plane_meta[VideoFrame::kUPlane].data,
                   plane_meta[VideoFrame::kUPlane].stride,
                   plane_meta[VideoFrame::kVPlane].data,
                   plane_meta[VideoFrame::kVPlane].stride),
         plane_meta[VideoFrame::kAPlane].data,
         plane_meta[VideoFrame::kAPlane].stride, pixels, row_bytes, matrix,
         width, rows, premultiply_alpha);
  };

  auto convert_yuva_with_filter = [&](const libyuv::YuvConstants* matrix,
                                      auto&& func) {
    func(YUV_ORDER(plane_meta[VideoFrame::kYPlane].data,
                   plane_meta[VideoFrame::kYPlane].stride,
                   plane_meta[VideoFrame::kUPlane].data,
                   plane_meta[VideoFrame::kUPlane].stride,
                   plane_meta[VideoFrame::kVPlane].data,
                   plane_meta[VideoFrame::kVPlane].stride),
         plane_meta[VideoFrame::kAPlane].data,
         plane_meta[VideoFrame::kAPlane].stride, pixels, row_bytes, matrix,
         width, rows, premultiply_alpha, filter);
  };

  auto convert_yuv16 = [&](const libyuv::YuvConstants* matrix, auto&& func) {
    func(YUV_ORDER(reinterpret_cast<const uint16_t*>(
                       plane_meta[VideoFrame::kYPlane].data),
                   plane_meta[VideoFrame::kYPlane].stride / 2,
                   reinterpret_cast<const uint16_t*>(
                       plane_meta[VideoFrame::kUPlane].data),
                   plane_meta[VideoFrame::kUPlane].stride / 2,
                   reinterpret_cast<const uint16_t*>(
                       plane_meta[VideoFrame::kVPlane].data),
                   plane_meta[VideoFrame::kVPlane].stride / 2),
         pixels, row_bytes, matrix, width, rows);
  };

  auto convert_yuv16_with_filter = [&](const libyuv::YuvConstants* matrix,
                                       auto&& func) {
    func(YUV_ORDER(reinterpret_cast<const uint16_t*>(
                       plane_meta[VideoFrame::kYPlane].data),
                   plane_meta[VideoFrame::kYPlane].stride / 2,
                   reinterpret_cast<const uint16_t*>(
                       plane_meta[VideoFrame::kUPlane].data),
                   plane_meta[VideoFrame::kUPlane].stride / 2,
                   reinterpret_cast<const uint16_t*>(
                       plane_meta[VideoFrame::kVPlane].data),
                   plane_meta[VideoFrame::kVPlane].stride / 2),
         pixels, row_bytes, matrix, width, rows, filter);
  };

  auto convert_yuva16 = [&](const libyuv::YuvConstants* matrix, auto&& func) {
    func(
        YUV_ORDER(reinterpret_cast<const uint16_t*>(
                      plane_meta[VideoFrame::kYPlane].data),
                  plane_meta[VideoFrame::kYPlane].stride / 2,
                  reinterpret_cast<const uint16_t*>(
                      plane_meta[VideoFrame::kUPlane].data),
                  plane_meta[VideoFrame::kUPlane].stride / 2,
                  reinterpret_cast<const uint16_t*>(
                      plane_meta[VideoFrame::kVPlane].data),
                  plane_meta[VideoFrame::kVPlane].stride / 2),
        reinterpret_cast<const uint16_t*>(plane_meta[VideoFrame::kAPlane].data),
        plane_meta[VideoFrame::kAPlane].stride / 2, pixels, row_bytes, matrix,
        width, rows, premultiply_alpha);
  };

  auto convert_yuva16_with_filter = [&](const libyuv::YuvConstants* matrix,
                                        auto&& func) {
    func(
        YUV_ORDER(reinterpret_cast<const uint16_t*>(
                      plane_meta[VideoFrame::kYPlane].data),
                  plane_meta[VideoFrame::kYPlane].stride / 2,
                  reinterpret_cast<const uint16_t*>(
                      plane_meta[VideoFrame::kUPlane].data),
                  plane_meta[VideoFrame::kUPlane].stride / 2,
                  reinterpret_cast<const uint16_t*>(
                      plane_meta[VideoFrame::kVPlane].data),
                  plane_meta[VideoFrame::kVPlane].stride / 2),
        reinterpret_cast<const uint16_t*>(plane_meta[VideoFrame::kAPlane].data),
        plane_meta[VideoFrame::kAPlane].stride / 2, pixels, row_bytes, matrix,
        width, rows, premultiply_alpha, filter);
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
            GBR_TO_RGB_ORDER(plane_meta[VideoFrame::kYPlane].data,
                             plane_meta[VideoFrame::kYPlane].stride,
                             plane_meta[VideoFrame::kUPlane].data,
                             plane_meta[VideoFrame::kUPlane].stride,
                             plane_meta[VideoFrame::kVPlane].data,
                             plane_meta[VideoFrame::kVPlane].stride),
            plane_meta[VideoFrame::kAPlane].data,
            plane_meta[VideoFrame::kAPlane].stride, pixels, row_bytes, width,
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
      LIBYUV_NV12_TO_ARGB_MATRIX(plane_meta[VideoFrame::kYPlane].data,
                                 plane_meta[VideoFrame::kYPlane].stride,
                                 plane_meta[VideoFrame::kUVPlane].data,
                                 plane_meta[VideoFrame::kUVPlane].stride,
                                 pixels, row_bytes, matrix, width, rows);
      break;
    case PIXEL_FORMAT_P016LE:
      libyuv::P016ToARGBMatrix(reinterpret_cast<const uint16_t*>(
                                   plane_meta[VideoFrame::kYPlane].data),
                               plane_meta[VideoFrame::kYPlane].stride,
                               reinterpret_cast<const uint16_t*>(
                                   plane_meta[VideoFrame::kUVPlane].data),
                               plane_meta[VideoFrame::kUVPlane].stride, pixels,
                               row_bytes, matrix, width, rows);
      if (!OUTPUT_ARGB)
        libyuv::ARGBToABGR(pixels, row_bytes, pixels, row_bytes, width, rows);
      break;

    case PIXEL_FORMAT_YUV420P9:
    case PIXEL_FORMAT_YUV422P9:
    case PIXEL_FORMAT_YUV444P9:
    case PIXEL_FORMAT_YUV422P12:
    case PIXEL_FORMAT_YUV444P12:
    case PIXEL_FORMAT_Y16:
      NOTREACHED()
          << "These cases should be handled in ConvertVideoFrameToRGBPixels";
      break;

    case PIXEL_FORMAT_UYVY:
    case PIXEL_FORMAT_NV21:
    case PIXEL_FORMAT_NV12A:
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

// Valid gl texture internal format that can try to use direct uploading path.
bool ValidFormatForDirectUploading(
    viz::RasterContextProvider* raster_context_provider,
    GrGLenum format,
    unsigned int type) {
  const gpu::Capabilities& context_caps =
      raster_context_provider->ContextCapabilities();
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
      return context_caps.major_version >= 3;
    default:
      return false;
  }
}

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

}  // anonymous namespace

// Generates an RGB image from a VideoFrame. Convert YUV to RGB plain on GPU.
class VideoImageGenerator : public cc::PaintImageGenerator {
 public:
  VideoImageGenerator() = delete;

  VideoImageGenerator(scoped_refptr<VideoFrame> frame)
      : cc::PaintImageGenerator(
            SkImageInfo::MakeN32Premul(frame->visible_rect().width(),
                                       frame->visible_rect().height(),
                                       frame->ColorSpace().ToSkColorSpace())),
        frame_(std::move(frame)) {
    DCHECK(!frame_->HasTextures());
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
        frame_.get(), dst_pixmap.writable_addr(), dst_pixmap.rowBytes());

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
    if (plane_config == SkYUVAInfo::PlaneConfig::kUnknown)
      return false;

    // Don't use the YUV conversion path for multi-plane RGB frames.
    if (frame_->format() == PIXEL_FORMAT_I444 &&
        frame_->ColorSpace().GetMatrixID() == gfx::ColorSpace::MatrixID::GBR) {
      return false;
    }

    if (!info)
      return true;

    SkYUVColorSpace yuv_color_space;
    if (!frame_->ColorSpace().ToSkYUVColorSpace(frame_->BitDepth(),
                                                &yuv_color_space)) {
      // TODO(crbug.com/828599): This should default to BT.709 color space.
      yuv_color_space = kRec601_SkYUVColorSpace;
    }

    // We use the Y plane size because it may get rounded up to an even size.
    // Our implementation of GetYUVAPlanes expects this.
    auto y_size = VideoFrame::PlaneSizeInSamples(
        frame_->format(), VideoFrame::kYPlane, frame_->visible_rect().size());
    auto yuva_info = SkYUVAInfo({y_size.width(), y_size.height()}, plane_config,
                                subsampling, yuv_color_space);
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

    for (int plane = VideoFrame::kYPlane; plane <= VideoFrame::kVPlane;
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
      sk_sp<SkImage> sk_image,
      const gpu::Mailbox& mailbox,
      bool wraps_video_frame_texture,
      scoped_refptr<viz::RasterContextProvider> raster_context_provider,
      std::unique_ptr<ScopedSharedImageAccess> access)
      : sk_image_(std::move(sk_image)),
        sk_image_info_(sk_image_->imageInfo()),
        mailbox_(mailbox),
        wraps_video_frame_texture_(wraps_video_frame_texture),
        access_(std::move(access)) {
    DCHECK(sk_image_->isTextureBacked());
    raster_context_provider_ = std::move(raster_context_provider);
  }

  explicit VideoTextureBacking(
      const gpu::Mailbox& mailbox,
      const SkImageInfo& info,
      bool wraps_video_frame_texture,
      scoped_refptr<viz::RasterContextProvider> raster_context_provider)
      : sk_image_info_(info),
        mailbox_(mailbox),
        wraps_video_frame_texture_(wraps_video_frame_texture) {
    raster_context_provider_ = std::move(raster_context_provider);
  }

  ~VideoTextureBacking() override {
    auto* ri = raster_context_provider_->RasterInterface();
    if (!wraps_video_frame_texture_) {
      gpu::SyncToken sync_token;
      ri->GenUnverifiedSyncTokenCHROMIUM(sync_token.GetData());
      auto* sii = raster_context_provider_->SharedImageInterface();
      sii->DestroySharedImage(sync_token, mailbox_);
    }
  }

  const SkImageInfo& GetSkImageInfo() override { return sk_image_info_; }
  gpu::Mailbox GetMailbox() const override { return mailbox_; }
  sk_sp<SkImage> GetAcceleratedSkImage() override { return sk_image_; }
  bool wraps_video_frame_texture() const { return wraps_video_frame_texture_; }
  const scoped_refptr<viz::RasterContextProvider>& raster_context_provider()
      const {
    return raster_context_provider_;
  }

  // Used only for recycling this TextureBacking - where we need to keep the
  // texture/mailbox alive, but replace the SkImage. |access| is the access to
  // the SharedImage backing this SkImage.
  void ReplaceAcceleratedSkImage(
      sk_sp<SkImage> sk_image,
      std::unique_ptr<ScopedSharedImageAccess> access) {
    DCHECK(sk_image->isTextureBacked());
    sk_image_ = sk_image;
    sk_image_info_ = sk_image->imageInfo();

    // The client should have called clear_access() before invoking this method.
    DCHECK(!access_);
    access_ = std::move(access);
  }

  void clear_access() { access_.reset(); }

  sk_sp<SkImage> GetSkImageViaReadback() override {
    sk_sp<SkData> image_pixels =
        SkData::MakeUninitialized(sk_image_info_.computeMinByteSize());
    if (!readPixels(sk_image_info_, image_pixels->writable_data(),
                    sk_image_info_.minRowBytes(), 0, 0)) {
      DLOG(ERROR) << "VideoTextureBacking::GetSkImageViaReadback failed.";
      return nullptr;
    }
    return SkImage::MakeRasterData(sk_image_info_, std::move(image_pixels),
                                   sk_image_info_.minRowBytes());
  }

  bool readPixels(const SkImageInfo& dst_info,
                  void* dst_pixels,
                  size_t dst_row_bytes,
                  int src_x,
                  int src_y) override {
    gpu::raster::RasterInterface* ri =
        raster_context_provider_->RasterInterface();
    if (sk_image_) {
      GrGLTextureInfo texture_info;
      if (!sk_image_->getBackendTexture(/*flushPendingGrContextIO=*/true)
               .getGLTextureInfo(&texture_info)) {
        DLOG(ERROR) << "Failed to getGLTextureInfo for VideoTextureBacking.";
        return false;
      }
      return sk_image_->readPixels(dst_info, dst_pixels, dst_row_bytes, src_x,
                                   src_y);
    }
    ri->ReadbackImagePixels(mailbox_, dst_info, dst_info.minRowBytes(), src_x,
                            src_y, /*plane_index=*/0, dst_pixels);
    return true;
  }

  void FlushPendingSkiaOps() override {
    if (!raster_context_provider_ || !sk_image_)
      return;
    sk_image_->flushAndSubmit(raster_context_provider_->GrContext());
  }

 private:
  sk_sp<SkImage> sk_image_;
  SkImageInfo sk_image_info_;
  scoped_refptr<viz::RasterContextProvider> raster_context_provider_;

  // This can be either the source VideoFrame's texture (if
  // |wraps_video_frame_texture_| is true) or a newly allocated shared image
  // (if |wraps_video_frame_texture_| is false) if a copy or conversion was
  // necessary.
  const gpu::Mailbox mailbox_;

  // Whether |mailbox_| directly points to a texture of the VideoFrame
  // (if true), or to an allocated shared image (if false).
  const bool wraps_video_frame_texture_;

  std::unique_ptr<ScopedSharedImageAccess> access_;
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
    const gfx::RectF& dest_rect,
    cc::PaintFlags& flags,
    VideoTransformation video_transformation,
    viz::RasterContextProvider* raster_context_provider) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (flags.getAlpha() == 0) {
    return;
  }

  if (video_frame && video_frame->HasTextures()) {
    if (!raster_context_provider) {
      DLOG(ERROR)
          << "Can't render textured frames w/o viz::RasterContextProvider";
      return;  // Unable to get/create a shared main thread context.
    }
    if (!raster_context_provider->GrContext() &&
        !raster_context_provider->ContextCapabilities().supports_oop_raster) {
      DLOG(ERROR)
          << "Can't render textured frames w/o valid GrContext or OOP raster.";
      return;  // The context has been lost.
    }
  }

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
        video_frame->HasTextures())) {
    cc::PaintFlags black_with_alpha_flags;
    black_with_alpha_flags.setAlphaf(flags.getAlphaf());
    canvas->drawRect(dest, black_with_alpha_flags);
    canvas->flush();
    return;
  }

  // Don't allow wrapping the VideoFrame texture, as we want to be able to cache
  // the PaintImage, to avoid redundant readbacks if the canvas is software.
  if (!UpdateLastImage(video_frame, raster_context_provider,
                       false /* allow_wrap_texture */)) {
    return;
  }
  DCHECK(cache_);
  cc::PaintImage image = cache_->paint_image;
  DCHECK(image);

  cc::PaintFlags video_flags;
  video_flags.setAlphaf(flags.getAlphaf());
  video_flags.setBlendMode(flags.getBlendMode());

  const bool need_rotation = video_transformation.rotation != VIDEO_ROTATION_0;
  const bool need_scaling =
      dest_rect.size() != gfx::SizeF(video_frame->visible_rect().size());
  const bool need_translation = !dest_rect.origin().IsOrigin();
  const bool needs_mirror = video_transformation.mirrored;
  bool need_transform =
      need_rotation || need_scaling || need_translation || needs_mirror;
  if (need_transform) {
    canvas->save();
    canvas->translate(
        SkFloatToScalar(dest_rect.x() + (dest_rect.width() * 0.5f)),
        SkFloatToScalar(dest_rect.y() + (dest_rect.height() * 0.5f)));
    SkScalar angle = SkFloatToScalar(0.0f);
    switch (video_transformation.rotation) {
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
        video_transformation.rotation == VIDEO_ROTATION_90 ||
        video_transformation.rotation == VIDEO_ROTATION_270;
    if (has_flipped_size) {
      rotated_dest_size =
          gfx::SizeF(rotated_dest_size.height(), rotated_dest_size.width());
    }
    auto sx = SkFloatToScalar(rotated_dest_size.width() /
                              video_frame->visible_rect().width());
    auto sy = SkFloatToScalar(rotated_dest_size.height() /
                              video_frame->visible_rect().height());
    if (needs_mirror) {
      if (has_flipped_size)
        sy *= -1;
      else
        sx *= -1;
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
  if (!need_transform && video_frame->IsMappable() &&
      flags.getAlpha() == SK_AlphaOPAQUE &&
      flags.getBlendMode() == SkBlendMode::kSrc &&
      flags.getFilterQuality() == cc::PaintFlags::FilterQuality::kLow &&
      (pixels = canvas->accessTopLayerPixels(&info, &row_bytes, &origin)) &&
      info.colorType() == kBGRA_8888_SkColorType) {
    const size_t offset = info.computeOffset(origin.x(), origin.y(), row_bytes);
    void* const pixels_offset = reinterpret_cast<char*>(pixels) + offset;
    ConvertVideoFrameToRGBPixels(video_frame.get(), pixels_offset, row_bytes);
  } else if (video_frame->HasTextures()) {
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

  if (need_transform)
    canvas->restore();
  // Make sure to flush so we can remove the videoframe from the generator.
  canvas->flush();

  if (video_frame->HasTextures()) {
    // Synchronize |video_frame| with the read operations in UpdateLastImage(),
    // which are triggered by canvas->flush().
    SynchronizeVideoFrameRead(std::move(video_frame),
                              raster_context_provider->RasterInterface(),
                              raster_context_provider->ContextSupport());
  }
  // Because we are not retaining a reference to the VideoFrame, it would be
  // invalid for the texture_backing to directly wrap its texture(s), as they
  // will be recycled.
  DCHECK(!CacheBackingWrapsTexture());
}

void PaintCanvasVideoRenderer::Copy(
    scoped_refptr<VideoFrame> video_frame,
    cc::PaintCanvas* canvas,
    viz::RasterContextProvider* raster_context_provider) {
  cc::PaintFlags flags;
  flags.setBlendMode(SkBlendMode::kSrc);
  flags.setFilterQuality(cc::PaintFlags::FilterQuality::kLow);

  auto dest_rect = gfx::RectF(gfx::SizeF(video_frame->visible_rect().size()));
  Paint(std::move(video_frame), canvas, dest_rect, flags,
        media::kNoTransformation, raster_context_provider);
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
    case PIXEL_FORMAT_YUV420P9:
      format = PIXEL_FORMAT_I420;
      break;

    case PIXEL_FORMAT_YUV422P12:
    case PIXEL_FORMAT_YUV422P10:
    case PIXEL_FORMAT_YUV422P9:
      format = PIXEL_FORMAT_I422;
      break;

    case PIXEL_FORMAT_YUV444P12:
    case PIXEL_FORMAT_YUV444P10:
    case PIXEL_FORMAT_YUV444P9:
      format = PIXEL_FORMAT_I444;
      break;

    default:
      NOTREACHED();
      return nullptr;
  }
  const int scale = 1 << (24 - video_frame->BitDepth());
  scoped_refptr<VideoFrame> ret = VideoFrame::CreateFrame(
      format, video_frame->coded_size(), video_frame->visible_rect(),
      video_frame->natural_size(), video_frame->timestamp());

  ret->set_color_space(video_frame->ColorSpace());
  // Copy all metadata.
  // (May be enough to copy color space)
  ret->metadata().MergeMetadataFrom(video_frame->metadata());

  for (int plane = VideoFrame::kYPlane; plane <= VideoFrame::kVPlane; ++plane) {
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
      DCHECK_NE(plane, VideoFrame::kYPlane);
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
        while (row < row_end)
          *out_row++ = *row++ / 65535.f;
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
                    bool flip_y,
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

  size_t output_row_bytes =
      frame->visible_rect().width() * output_bytes_per_pixel;
  *temp_buffer =
      new DataBuffer(output_row_bytes * frame->visible_rect().height());
  FlipAndConvertY16(frame, (*temp_buffer)->writable_data(), format, type,
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
                                      bool flip_y,
                                      bool premultiply_alpha) {
  unsigned temp_texture = 0;
  gl->GenTextures(1, &temp_texture);
  gl->BindTexture(target, temp_texture);
  gl->TexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  gl->TexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  gl->TexImage2D(target, 0, temp_internalformat, frame->visible_rect().width(),
                 frame->visible_rect().height(), 0, temp_format, temp_type,
                 frame->visible_data(0));
  gl->BindTexture(target, texture);
  gl->CopySubTextureCHROMIUM(temp_texture, 0, target, texture, level, 0, 0,
                             xoffset, yoffset, frame->visible_rect().width(),
                             frame->visible_rect().height(), flip_y,
                             premultiply_alpha, false);
  gl->DeleteTextures(1, &temp_texture);
}

}  // anonymous namespace

// static
void PaintCanvasVideoRenderer::ConvertVideoFrameToRGBPixels(
    const VideoFrame* video_frame,
    void* rgb_pixels,
    size_t row_bytes,
    bool premultiply_alpha,
    FilterMode filter,
    bool disable_threading) {
  if (!video_frame->IsMappable()) {
    NOTREACHED() << "Cannot extract pixels from non-CPU frame formats.";
    return;
  }

  scoped_refptr<VideoFrame> temporary_frame;
  // TODO(thomasanderson): Parallelize converting these formats.
  switch (video_frame->format()) {
    case PIXEL_FORMAT_YUV420P9:
    case PIXEL_FORMAT_YUV422P9:
    case PIXEL_FORMAT_YUV444P9:
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
      if (!video_frame->data(VideoFrame::kUPlane) &&
          !video_frame->data(VideoFrame::kVPlane)) {
        temporary_frame = DownShiftHighbitVideoFrame(video_frame);
        video_frame = temporary_frame.get();
      }
      break;
    case PIXEL_FORMAT_Y16:
      // Since it is grayscale conversion, we disregard
      // SK_PMCOLOR_BYTE_ORDER and always use GL_RGBA.
      FlipAndConvertY16(video_frame, static_cast<uint8_t*>(rgb_pixels), GL_RGBA,
                        GL_UNSIGNED_BYTE, false /*flip_y*/, row_bytes);
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
        FROM_HERE,
        base::BindOnce(ConvertVideoFrameToRGBPixelsTask,
                       base::Unretained(video_frame), rgb_pixels, row_bytes,
                       premultiply_alpha, libyuv_filter, i, n_tasks, &barrier));
  }
  ConvertVideoFrameToRGBPixelsTask(video_frame, rgb_pixels, row_bytes,
                                   premultiply_alpha, libyuv_filter, 0, n_tasks,
                                   &barrier);
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
    bool premultiply_alpha,
    bool flip_y) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(video_frame);
  DCHECK(video_frame->HasTextures());

  if (video_frame->NumTextures() > 1 ||
      video_frame->metadata().read_lock_fences_enabled) {
    DCHECK(video_frame->metadata().texture_origin_is_top_left);
    if (!raster_context_provider)
      return false;
    GrDirectContext* gr_context = raster_context_provider->GrContext();
    if (!gr_context &&
        !raster_context_provider->ContextCapabilities().supports_oop_raster)
      return false;
    // Since skia always produces premultiply alpha outputs,
    // trying direct uploading path when video format is opaque or premultiply
    // alpha been requested. And dst texture mipLevel must be 0.
    // TODO(crbug.com/1155003): Figure out whether premultiply options here are
    // accurate.
    if ((media::IsOpaque(video_frame->format()) || premultiply_alpha) &&
        level == 0 && video_frame->NumTextures() > 1) {
      if (UploadVideoFrameToGLTexture(raster_context_provider, destination_gl,
                                      video_frame.get(), target, texture,
                                      internal_format, format, type, flip_y)) {
        return true;
      }
    }

    if (!UpdateLastImage(video_frame, raster_context_provider,
                         true /* allow_wrap_texture */)) {
      return false;
    }

    DCHECK(cache_);
    DCHECK(cache_->texture_backing);
    gpu::raster::RasterInterface* canvas_ri =
        raster_context_provider->RasterInterface();

    gpu::SyncToken sync_token;
    // Wait for mailbox creation on canvas context before consuming it and
    // copying from it on the consumer context.
    canvas_ri->GenUnverifiedSyncTokenCHROMIUM(sync_token.GetData());

    CopyMailboxToTexture(
        destination_gl, cache_->coded_size, cache_->visible_rect,
        cache_->texture_backing->GetMailbox(), sync_token, target, texture,
        internal_format, format, type, level, premultiply_alpha, flip_y);

    // Wait for destination context to consume mailbox before deleting it in
    // canvas context.
    gpu::SyncToken dest_sync_token;
    destination_gl->GenUnverifiedSyncTokenCHROMIUM(dest_sync_token.GetData());
    canvas_ri->WaitSyncTokenCHROMIUM(dest_sync_token.GetConstData());

    // Because we are not retaining a reference to the VideoFrame, it would be
    // invalid to keep |cache_| around if it directly wraps |video_frame|.
    if (cache_->texture_backing->wraps_video_frame_texture()) {
      cache_.reset();
      // Ensure that |video_frame| not be destroyed until the above
      // CopyMailboxToTexture completes.
      SynchronizeVideoFrameRead(std::move(video_frame), destination_gl,
                                raster_context_provider->ContextSupport());
    }
  } else {
    // Correct Y-flip. flip_y should take precedent when
    // texture_origin_is_top_left is true, and invert the setting when
    // texture_origin_is_top_left is false.
    if (!video_frame->metadata().texture_origin_is_top_left)
      flip_y = !flip_y;

    const gpu::MailboxHolder& mailbox_holder =
        GetVideoFrameMailboxHolder(video_frame.get());
    DCHECK(mailbox_holder.texture_target == GL_TEXTURE_2D ||
           mailbox_holder.texture_target == GL_TEXTURE_RECTANGLE_ARB ||
           mailbox_holder.texture_target == GL_TEXTURE_EXTERNAL_OES)
        << mailbox_holder.texture_target;
    CopyMailboxToTexture(
        destination_gl, video_frame->coded_size(), video_frame->visible_rect(),
        mailbox_holder.mailbox, mailbox_holder.sync_token, target, texture,
        internal_format, format, type, level, premultiply_alpha, flip_y);
    destination_gl->ShallowFlushCHROMIUM();

    SynchronizeVideoFrameRead(std::move(video_frame), destination_gl,
                              raster_context_provider->ContextSupport());
  }
  DCHECK(!CacheBackingWrapsTexture());
  return true;
}

bool PaintCanvasVideoRenderer::UploadVideoFrameToGLTexture(
    viz::RasterContextProvider* raster_context_provider,
    gpu::gles2::GLES2Interface* destination_gl,
    scoped_refptr<VideoFrame> video_frame,
    unsigned int target,
    unsigned int texture,
    unsigned int internal_format,
    unsigned int format,
    unsigned int type,
    bool flip_y) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(video_frame);
  // Support uploading for NV12 and I420 video frame only.
  if (!VideoFrameYUVConverter::IsVideoFrameFormatSupported(*video_frame)) {
    return false;
  }

  // TODO(crbug.com/1108154): Support more texture target, e.g.
  // 2d array, 3d etc.
  if (target != GL_TEXTURE_2D) {
    return false;
  }

  if (!ValidFormatForDirectUploading(raster_context_provider,
                                     static_cast<GLenum>(internal_format),
                                     type)) {
    return false;
  }

  // TODO(nazabris): Support OOP-R code path here that does not have GrContext.
  if (!raster_context_provider || !raster_context_provider->GrContext())
    return false;

  if (raster_context_provider->ContextCapabilities().disable_legacy_mailbox)
    return false;

  DCHECK(video_frame->metadata().texture_origin_is_top_left);

  // Trigger resource allocation for dst texture to back SkSurface.
  // Dst texture size should equal to video frame visible rect.
  destination_gl->BindTexture(target, texture);
  destination_gl->TexImage2D(
      target, 0, internal_format, video_frame->visible_rect().width(),
      video_frame->visible_rect().height(), 0, format, type, nullptr);

  gpu::MailboxHolder mailbox_holder;
  mailbox_holder.texture_target = target;
  destination_gl->ProduceTextureDirectCHROMIUM(texture,
                                               mailbox_holder.mailbox.name);

  destination_gl->GenUnverifiedSyncTokenCHROMIUM(
      mailbox_holder.sync_token.GetData());

  VideoFrameYUVConverter::GrParams yuv_gr_params;
  yuv_gr_params.internal_format = internal_format;
  yuv_gr_params.type = type;
  yuv_gr_params.flip_y = flip_y;
  yuv_gr_params.use_visible_rect = true;
  if (!VideoFrameYUVConverter::ConvertYUVVideoFrameNoCaching(
          video_frame.get(), raster_context_provider, mailbox_holder,
          yuv_gr_params)) {
    return false;
  }

  gpu::raster::RasterInterface* source_ri =
      raster_context_provider->RasterInterface();
  // Wait for mailbox creation on canvas context before consuming it.
  source_ri->GenUnverifiedSyncTokenCHROMIUM(
      mailbox_holder.sync_token.GetData());

  destination_gl->WaitSyncTokenCHROMIUM(
      mailbox_holder.sync_token.GetConstData());

  if (video_frame->HasTextures()) {
    SynchronizeVideoFrameRead(std::move(video_frame), source_ri,
                              raster_context_provider->ContextSupport());
  }

  return true;
}

// static
bool PaintCanvasVideoRenderer::PrepareVideoFrameForWebGL(
    viz::RasterContextProvider* raster_context_provider,
    gpu::gles2::GLES2Interface* destination_gl,
    scoped_refptr<VideoFrame> video_frame,
    unsigned int target,
    unsigned int texture) {
  // TODO(776222): This static function uses no common functionality in
  // PaintCanvasVideoRenderer, and should be removed from this class.

  DCHECK(video_frame);
  DCHECK(video_frame->HasTextures());
  if (video_frame->NumTextures() == 1) {
    if (target == GL_TEXTURE_EXTERNAL_OES) {
      // We don't support Android now.
      // TODO(crbug.com/776222): support Android.
      return false;
    }
    // We don't support sharing single video frame texture now.
    // TODO(crbug.com/776222): deal with single video frame texture.
    return false;
  }

  // TODO(nazabris): Support OOP-R code path here that does not have GrContext.
  if (!raster_context_provider || !raster_context_provider->GrContext())
    return false;

  DCHECK(video_frame->metadata().texture_origin_is_top_left);

  // Take webgl video texture as 2D texture. Setting it as external render
  // target backend for skia.
  destination_gl->BindTexture(target, texture);
  destination_gl->TexImage2D(target, 0, GL_RGBA,
                             video_frame->coded_size().width(),
                             video_frame->coded_size().height(), 0, GL_RGBA,
                             GL_UNSIGNED_BYTE, nullptr);

  gpu::raster::RasterInterface* source_ri =
      raster_context_provider->RasterInterface();
  gpu::MailboxHolder mailbox_holder;
  mailbox_holder.texture_target = target;
  destination_gl->ProduceTextureDirectCHROMIUM(texture,
                                               mailbox_holder.mailbox.name);

  destination_gl->GenUnverifiedSyncTokenCHROMIUM(
      mailbox_holder.sync_token.GetData());

  // Generate a new image.
  if (video_frame->HasTextures()) {
    if (video_frame->NumTextures() > 1) {
      VideoFrameYUVConverter::ConvertYUVVideoFrameNoCaching(
          video_frame.get(), raster_context_provider, mailbox_holder);
    } else {
      // We don't support Android now.
      return false;
    }
  }

  // Wait for mailbox creation on canvas context before consuming it and
  // copying from it on the consumer context.
  source_ri->GenUnverifiedSyncTokenCHROMIUM(
      mailbox_holder.sync_token.GetData());

  destination_gl->WaitSyncTokenCHROMIUM(
      mailbox_holder.sync_token.GetConstData());

  WaitAndReplaceSyncTokenClient client(source_ri);
  video_frame->UpdateReleaseSyncToken(&client);

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
    bool premultiply_alpha,
    bool flip_y) {
  if (!raster_context_provider)
    return false;
#if BUILDFLAG(IS_ANDROID)
  // TODO(crbug.com/1181993): These formats don't work with the passthrough
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

  if (!VideoFrameYUVConverter::IsVideoFrameFormatSupported(*video_frame)) {
    return false;
  }
  // Could handle NV12 here as well. See NewSkImageFromVideoFrameYUV.

  DCHECK(video_frame->metadata().texture_origin_is_top_left);

  // Since skia always produces premultiply alpha outputs,
  // trying direct uploading path when video format is opaque or premultiply
  // alpha been requested. And dst texture mipLevel must be 0.
  // TODO(crbug.com/1155003): Figure out whether premultiply options here are
  // accurate.
  if ((media::IsOpaque(video_frame->format()) || premultiply_alpha) &&
      level == 0) {
    if (UploadVideoFrameToGLTexture(raster_context_provider, destination_gl,
                                    video_frame, target, texture,
                                    internal_format, format, type, flip_y)) {
      return true;
    }
  }

  auto* sii = raster_context_provider->SharedImageInterface();
  gpu::raster::RasterInterface* source_ri =
      raster_context_provider->RasterInterface();

  // We need a shared image to receive the intermediate RGB result. Try to reuse
  // one if compatible, otherwise create a new one.
  gpu::SyncToken token;
  if (!yuv_cache_.mailbox.IsZero() &&
      yuv_cache_.size == video_frame->coded_size() &&
      yuv_cache_.raster_context_provider == raster_context_provider) {
    token = yuv_cache_.sync_token;
  } else {
    yuv_cache_.Reset();
    yuv_cache_.raster_context_provider = raster_context_provider;
    yuv_cache_.size = video_frame->coded_size();

    uint32_t usage = gpu::SHARED_IMAGE_USAGE_GLES2;
    if (raster_context_provider->ContextCapabilities().supports_oop_raster) {
      usage |= gpu::SHARED_IMAGE_USAGE_RASTER |
               gpu::SHARED_IMAGE_USAGE_OOP_RASTERIZATION;
    }

    yuv_cache_.mailbox = sii->CreateSharedImage(
        SHARED_IMAGE_FORMAT, video_frame->coded_size(),
        GetVideoFrameRGBColorSpacePreferringSRGB(video_frame.get()),
        kTopLeft_GrSurfaceOrigin, kPremul_SkAlphaType, usage,
        gpu::kNullSurfaceHandle);
    token = sii->GenUnverifiedSyncToken();
  }

  // On the source Raster context, do the YUV->RGB conversion.
  gpu::MailboxHolder dest_holder;
  dest_holder.mailbox = yuv_cache_.mailbox;
  dest_holder.texture_target = GL_TEXTURE_2D;
  dest_holder.sync_token = token;
  yuv_cache_.yuv_converter.ConvertYUVVideoFrame(
      video_frame.get(), raster_context_provider, dest_holder);

  gpu::SyncToken post_conversion_sync_token;
  source_ri->GenUnverifiedSyncTokenCHROMIUM(
      post_conversion_sync_token.GetData());

  // On the destination GL context, do a copy (with cropping) into the
  // destination texture.
  CopyMailboxToTexture(
      destination_gl, video_frame->coded_size(), video_frame->visible_rect(),
      yuv_cache_.mailbox, post_conversion_sync_token, target, texture,
      internal_format, format, type, level, premultiply_alpha, flip_y);
  destination_gl->GenUnverifiedSyncTokenCHROMIUM(
      yuv_cache_.sync_token.GetData());

  // video_frame->UpdateReleaseSyncToken is not necessary since the video frame
  // data we used was CPU-side (IsMappable) to begin with. If there were any
  // textures, we didn't use them.

  // The temporary SkImages should be automatically cleaned up here.

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
    bool flip_y,
    bool premultiply_alpha) {
  DCHECK(frame);
  DCHECK(!frame->HasTextures());
  DCHECK(frame->metadata().texture_origin_is_top_left);

  // Note: CopyTextureCHROMIUM uses mediump for color computation. Don't use
  // it if the precision would lead to data loss when converting 16-bit
  // normalized to float. medium_float.precision > 15 means that the approach
  // below is not used on Android, where the extension EXT_texture_norm16 is
  // not widely supported. It is used on Windows, Linux and OSX.
  // Android support is not required for now because Tango depth camera already
  // provides floating point data (projected point cloud). See crbug.com/674440.
  if (gpu_capabilities.texture_norm16 &&
      gpu_capabilities.fragment_shader_precisions.medium_float.precision > 15 &&
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
                                     flip_y, premultiply_alpha);
    return true;
  }
  scoped_refptr<DataBuffer> temp_buffer;
  if (!TexImageHelper(frame, format, type, flip_y, &temp_buffer))
    return false;

  gl->TexImage2D(target, level, internalformat, frame->visible_rect().width(),
                 frame->visible_rect().height(), 0, format, type,
                 temp_buffer->data());
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
                                             bool flip_y,
                                             bool premultiply_alpha) {
  DCHECK(frame);
  DCHECK(!frame->HasTextures());
  DCHECK(frame->metadata().texture_origin_is_top_left);

  scoped_refptr<DataBuffer> temp_buffer;
  if (!TexImageHelper(frame, format, type, flip_y, &temp_buffer))
    return false;

  gl->TexSubImage2D(
      target, level, xoffset, yoffset, frame->visible_rect().width(),
      frame->visible_rect().height(), format, type, temp_buffer->data());
  return true;
}

void PaintCanvasVideoRenderer::ResetCache() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  cache_.reset();
  yuv_cache_.Reset();
}

PaintCanvasVideoRenderer::Cache::Cache(VideoFrame::ID frame_id)
    : frame_id(frame_id) {}

PaintCanvasVideoRenderer::Cache::~Cache() = default;

bool PaintCanvasVideoRenderer::Cache::Recycle() {
  DCHECK(!texture_backing->wraps_video_frame_texture());

  paint_image = cc::PaintImage();
  if (!texture_backing->unique())
    return false;

  // Flush any pending GPU work using this texture.
  texture_backing->FlushPendingSkiaOps();
  return true;
}

bool PaintCanvasVideoRenderer::UpdateLastImage(
    scoped_refptr<VideoFrame> video_frame,
    viz::RasterContextProvider* raster_context_provider,
    bool allow_wrap_texture) {
  DCHECK(!CacheBackingWrapsTexture());

  // Check for a cache hit.
  if (cache_ && video_frame->unique_id() == cache_->frame_id &&
      cache_->paint_image) {
    cache_deleting_timer_.Reset();
    return true;
  }

  auto paint_image_builder =
      cc::PaintImageBuilder::WithDefault()
          .set_id(renderer_stable_id_)
          .set_animation_type(cc::PaintImage::AnimationType::VIDEO)
          .set_completion_state(cc::PaintImage::CompletionState::DONE);

  // Generate a new image.
  // Note: Skia will hold onto |video_frame| via |video_generator| only when
  // |video_frame| is software.
  // Holding |video_frame| longer than this call when using GPUVideoDecoder
  // could cause problems since the pool of VideoFrames has a fixed size.
  if (video_frame->HasTextures()) {
    DCHECK(raster_context_provider);
    bool supports_oop_raster =
        raster_context_provider->ContextCapabilities().supports_oop_raster;
    DCHECK(supports_oop_raster || raster_context_provider->GrContext());
    auto* ri = raster_context_provider->RasterInterface();
    DCHECK(ri);
    bool wraps_video_frame_texture = false;
    gpu::Mailbox mailbox;

    // `texture_target` is set only when the image backing the frame is
    // compatible with GL.
    bool can_wrap_texture = video_frame->NumTextures() == 1 &&
                            video_frame->mailbox_holder(0).texture_target != 0;

    if (allow_wrap_texture && can_wrap_texture) {
      cache_.emplace(video_frame->unique_id());
      const gpu::MailboxHolder& holder =
          GetVideoFrameMailboxHolder(video_frame.get());
      mailbox = holder.mailbox;
      ri->WaitSyncTokenCHROMIUM(holder.sync_token.GetConstData());
      wraps_video_frame_texture = true;
    } else {
      // Create or reuse a texture backing for the cached copy.
      if (cache_ && cache_->texture_backing &&
          cache_->texture_backing->raster_context_provider() ==
              raster_context_provider &&
          cache_->coded_size == video_frame->coded_size() &&
          // We always convert to top left origin when copying, so if the
          // cache was previously for a wrapped texture it can't be used.
          cache_->texture_origin_is_top_left && cache_->Recycle()) {
        // We can reuse the shared image from the previous cache.
        cache_->frame_id = video_frame->unique_id();
        mailbox = cache_->texture_backing->GetMailbox();

        // NOTE: It is necessary to let go of read access to the cached copy
        // here because the below copy operation takes readwrite access to that
        // cached copy, and requesting RW access while already holding R access
        // on a single service-side texture causes a DCHECK to fire.
        cache_->texture_backing->clear_access();
      } else {
        cache_.emplace(video_frame->unique_id());
        auto* sii = raster_context_provider->SharedImageInterface();
        // TODO(nazabris): Sort out what to do when GLES2 is needed but the
        // cached shared image is created without it.
        uint32_t flags =
            gpu::SHARED_IMAGE_USAGE_GLES2 | gpu::SHARED_IMAGE_USAGE_RASTER;
        if (supports_oop_raster) {
          flags |= gpu::SHARED_IMAGE_USAGE_OOP_RASTERIZATION;
        }
        mailbox = sii->CreateSharedImage(
            SHARED_IMAGE_FORMAT, video_frame->coded_size(),
            GetVideoFrameRGBColorSpacePreferringSRGB(video_frame.get()),
            kTopLeft_GrSurfaceOrigin, kPremul_SkAlphaType, flags,
            gpu::kNullSurfaceHandle);
        ri->WaitSyncTokenCHROMIUM(sii->GenUnverifiedSyncToken().GetConstData());
      }

      // Copy into the texture backing of the cached copy.
      if (video_frame->NumTextures() == 1) {
        const gpu::MailboxHolder& frame_mailbox_holder =
            GetVideoFrameMailboxHolder(video_frame.get());
        ri->WaitSyncTokenCHROMIUM(
            frame_mailbox_holder.sync_token.GetConstData());
        ri->CopySharedImage(
            frame_mailbox_holder.mailbox, mailbox, GL_TEXTURE_2D, 0, 0, 0, 0,
            video_frame->coded_size().width(),
            video_frame->coded_size().height(),
            !video_frame->metadata().texture_origin_is_top_left, GL_FALSE);
      } else {
        DCHECK(video_frame->metadata().texture_origin_is_top_left);
        gpu::MailboxHolder dest_holder{mailbox, gpu::SyncToken(),
                                       GL_TEXTURE_2D};
        VideoFrameYUVConverter::ConvertYUVVideoFrameNoCaching(
            video_frame.get(), raster_context_provider, dest_holder);
      }
      if (!supports_oop_raster)
        raster_context_provider->GrContext()->flushAndSubmit();

      // Ensure that |video_frame| not be deleted until the above copy is
      // completed.
      SynchronizeVideoFrameRead(video_frame, ri,
                                raster_context_provider->ContextSupport());
    }

    cache_->coded_size = video_frame->coded_size();
    cache_->visible_rect = video_frame->visible_rect();
    cache_->texture_origin_is_top_left =
        wraps_video_frame_texture
            ? video_frame->metadata().texture_origin_is_top_left
            : true;

    // In OOPR mode, we can keep the entire TextureBacking. In non-OOPR,
    // we can recycle the mailbox/texture, but have to replace the SkImage.
    if (!supports_oop_raster) {
      cache_->source_texture = ri->CreateAndConsumeForGpuRaster(mailbox);

      auto access = std::make_unique<ScopedSharedImageAccess>(
          ri, cache_->source_texture, mailbox);
      auto source_image = WrapGLTexture(
          wraps_video_frame_texture
              ? video_frame->mailbox_holder(0).texture_target
              : GL_TEXTURE_2D,
          cache_->source_texture, video_frame->coded_size(),
          raster_context_provider, cache_->texture_origin_is_top_left);
      if (!source_image) {
        // Couldn't create the SkImage.
        cache_.reset();
        return false;
      }
      if (!cache_->texture_backing) {
        cache_->texture_backing = sk_make_sp<VideoTextureBacking>(
            std::move(source_image), mailbox, wraps_video_frame_texture,
            raster_context_provider, std::move(access));
      } else {
        cache_->texture_backing->ReplaceAcceleratedSkImage(
            std::move(source_image), std::move(access));
      }
    } else if (!cache_->texture_backing) {
      SkImageInfo sk_image_info =
          SkImageInfo::Make(gfx::SizeToSkISize(cache_->coded_size),
                            kRGBA_8888_SkColorType, kPremul_SkAlphaType);
      cache_->texture_backing = sk_make_sp<VideoTextureBacking>(
          mailbox, sk_image_info, wraps_video_frame_texture,
          raster_context_provider);
    }
    paint_image_builder.set_texture_backing(cache_->texture_backing,
                                            cc::PaintImage::GetNextContentId());
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

PaintCanvasVideoRenderer::YUVTextureCache::YUVTextureCache() = default;
PaintCanvasVideoRenderer::YUVTextureCache::~YUVTextureCache() {
  Reset();
}

void PaintCanvasVideoRenderer::YUVTextureCache::Reset() {
  if (mailbox.IsZero())
    return;
  DCHECK(raster_context_provider);

  gpu::raster::RasterInterface* ri = raster_context_provider->RasterInterface();
  ri->WaitSyncTokenCHROMIUM(sync_token.GetConstData());
  ri->OrderingBarrierCHROMIUM();

  auto* sii = raster_context_provider->SharedImageInterface();
  sii->DestroySharedImage(sync_token, mailbox);
  mailbox.SetZero();

  yuv_converter.ReleaseCachedData();

  // Kick off the GL work up to the OrderingBarrierCHROMIUM above as well as the
  // SharedImageInterface work, to ensure the shared image memory is released in
  // a timely fashion.
  raster_context_provider->ContextSupport()->FlushPendingWork();
  raster_context_provider.reset();
}

gfx::Size PaintCanvasVideoRenderer::LastImageDimensionsForTesting() {
  DCHECK(cache_);
  DCHECK(cache_->paint_image);
  return gfx::Size(cache_->paint_image.width(), cache_->paint_image.height());
}

bool PaintCanvasVideoRenderer::CacheBackingWrapsTexture() const {
  return cache_ && cache_->texture_backing &&
         cache_->texture_backing->wraps_video_frame_texture();
}

}  // namespace media
