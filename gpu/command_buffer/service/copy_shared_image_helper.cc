// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/copy_shared_image_helper.h"

#include <array>
#include <memory>
#include <vector>

#include "base/check.h"
#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "build/build_config.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/service/gl_utils.h"
#include "gpu/command_buffer/service/graphite_shared_context.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image/shared_image_factory.h"
#include "gpu/command_buffer/service/shared_image/shared_image_format_service_utils.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"
#include "gpu/command_buffer/service/skia_utils.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "gpu/config/gpu_driver_bug_workarounds.h"
#include "skia/ext/rgba_to_yuva.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"
#include "third_party/libyuv/include/libyuv/planar_functions.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkRect.h"
#include "third_party/skia/include/core/SkSize.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/gpu/ganesh/GrBackendSemaphore.h"
#include "third_party/skia/include/gpu/ganesh/GrDirectContext.h"
#include "third_party/skia/include/gpu/ganesh/GrTypes.h"
#include "third_party/skia/include/gpu/ganesh/SkSurfaceGanesh.h"
#include "third_party/skia/include/gpu/ganesh/gl/GrGLBackendSurface.h"
#include "third_party/skia/include/gpu/ganesh/gl/GrGLTypes.h"
#include "third_party/skia/include/gpu/graphite/Context.h"
#include "third_party/skia/include/gpu/graphite/Image.h"
#include "third_party/skia/include/gpu/graphite/Recorder.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gl/progress_reporter.h"

namespace gpu {

using GLError = CopySharedImageHelper::GLError;

namespace {

SkColorType GetCompatibleSurfaceColorType(GrGLenum format) {
  switch (format) {
    case GL_RGBA8:
      return kRGBA_8888_SkColorType;
    case GL_RGB565:
      return kRGB_565_SkColorType;
    case GL_RGBA16F:
      return kRGBA_F16_SkColorType;
    case GL_RGB8:
      return kRGB_888x_SkColorType;
    case GL_RGB10_A2:
      return kRGBA_1010102_SkColorType;
    case GL_RGBA4:
      return kARGB_4444_SkColorType;
    case GL_SRGB8_ALPHA8:
      return kRGBA_8888_SkColorType;
    default:
      NOTREACHED() << "Unknown format: " << format;
  }
}

GrGLenum GetSurfaceColorFormat(GrGLenum format, GrGLenum type) {
  if (format == GL_RGBA) {
    if (type == GL_UNSIGNED_BYTE) {
      return GL_RGBA8;
    }
    if (type == GL_UNSIGNED_SHORT_4_4_4_4) {
      return GL_RGBA4;
    }
  }
  if (format == GL_RGB) {
    if (type == GL_UNSIGNED_BYTE) {
      return GL_RGB8;
    }
    if (type == GL_UNSIGNED_SHORT_5_6_5) {
      return GL_RGB565;
    }
  }
  return format;
}

// Returns an SkSurface wrapping `texture_id`. Assumes the presence of a Ganesh
// GL context to do the wrapping.
sk_sp<SkSurface> CreateSkSurfaceWrappingGLTexture(
    SharedContextState* shared_context_state,
    GLuint texture_id,
    GLenum target,
    GLuint internal_format,
    GLenum type,
    GLsizei width,
    GLsizei height,
    GrSurfaceOrigin dst_origin) {
  CHECK_NE(texture_id, 0u);
  CHECK(shared_context_state->GrContextIsGL());
  GrGLTextureInfo texture_info;
  texture_info.fID = texture_id;
  texture_info.fTarget = target;
  // Get the surface color format similar to that in PaintCanvasVideoRenderer.
  texture_info.fFormat = GetSurfaceColorFormat(internal_format, type);
  auto backend_texture = GrBackendTextures::MakeGL(
      width, height, skgpu::Mipmapped::kNo, texture_info);

  auto dest_color_space = SkColorSpace::MakeSRGB();
  GrDirectContext* direct_context = shared_context_state->gr_context();
  CHECK(direct_context);
  return SkSurfaces::WrapBackendTexture(
      direct_context, backend_texture, dst_origin,
      /*sampleCnt=*/1, GetCompatibleSurfaceColorType(texture_info.fFormat),
      dest_color_space, nullptr);
}

// If the copy is performed, returns true and sets `upload_error` to any GL
// error reported by the driver during the upload.
bool CopyPixelsToTexture(
    GLint xoffset,
    GLint yoffset,
    GLint x,
    GLint y,
    GLsizei width,
    GLsizei height,
    gfx::Rect dest_cleared_rect,
    const Mailbox& source_mailbox,
    SkiaImageRepresentation* dest_shared_image,
    SkiaImageRepresentation::ScopedWriteAccess* dest_scoped_access,
    SharedImageRepresentationFactory* representation_factory,
    SharedContextState* shared_context_state,
    const std::vector<GrBackendSemaphore>& begin_semaphores,
    std::vector<GrBackendSemaphore>& end_semaphores,
    gl::GLApi* gl_api,
    GLenum& upload_error) {
  // We have implemented CompoundImageBacking::ProduceMemory() which can lead
  // to a performance regression when it's underlying GPU backing holds the
  // latest data. Previously, an unimplemented
  // CompoundImageBacking::ProduceMemory() would return nullptr below,
  // triggering a more efficient GPU-GPU copy fallback in
  // CopySharedImageHelper::CopySharedImage(). With
  // CompoundImageBacking::ProduceMemory() implementation, a GPU->CPU copy
  // occurs internally in CompoundImageBacking first, followed by a CPU->GPU
  // upload in this method, which is less performant than a direct GPU->GPU
  // transfer.
  // For cases where the underlying GPU backing has stale data compared to its
  // shm backing, CompoundImageBacking::ProduceMemory() actually results in perf
  // improvements as compared to GPU->GPU fallback since the fallback will now
  // trigger a readback (from CSI's shm backing to its GPU backing) before
  // actual GPU->GPU copy happens.
  // TODO(crbug.com/470101115): Ideally SharedImageCopyManager will replace
  // all copy operation here. But if perf regression is reported before that
  // happens, we will need to fix this issue by adding some temporary
  // workaround like querying CompoundImageBacking if its gpu backing has the
  // latest data or not and choose copy path accordingly.
  auto source_shared_image =
      representation_factory->ProduceMemory(source_mailbox);
  if (!source_shared_image) {
    return false;
  }

  if (source_shared_image->surface_origin() !=
      dest_shared_image->surface_origin()) {
    return false;
  }

  gfx::Size source_size = source_shared_image->size();
  gfx::Rect source_rect(x, y, width, height);
  if (source_rect.IsEmpty() || !gfx::Rect(source_size).Contains(source_rect)) {
    return false;
  }

  auto scoped_read_access = source_shared_image->BeginScopedReadAccess();
  if (!scoped_read_access) {
    return false;
  }

  SkPixmap pm = scoped_read_access->pixmap();
  SkIRect skIRect = RectToSkIRect(source_rect);
  SkPixmap subset;
  if (!pm.extractSubset(&subset, skIRect)) {
    return false;
  }

  if (!begin_semaphores.empty()) {
    // Wait will only fail if the context is abandoned, in which case it
    // doesn't matter that it failed.
    dest_scoped_access->surface()->wait(begin_semaphores.size(),
                                        begin_semaphores.data(),
                                        /*deleteSemaphoresAfterWait=*/false);
  }

  bool success = dest_scoped_access->surface()->getCanvas()->writePixels(
      subset.info(), subset.addr(), subset.rowBytes(), xoffset, yoffset);

  // Always flush and submit, even on write failure, to avoid leaving open work
  // on the SkSurface.
  success &= shared_context_state->FlushWriteAccess(dest_scoped_access);
  success &= shared_context_state->SubmitIfNecessary(
      std::move(end_semaphores),
      dest_scoped_access->NeedGraphiteContextSubmit());

  if (!success) {
    upload_error = GL_INVALID_OPERATION;
  } else if (!dest_shared_image->IsCleared()) {
    upload_error = DrainGLErrors(gl_api);
    if (upload_error == GL_NO_ERROR) {
      dest_shared_image->SetClearedRect(dest_cleared_rect);
    }
  }

  return true;
}

struct ReadPixelsContext {
  std::unique_ptr<const SkImage::AsyncReadResult> async_result;
  bool finished = false;
};

// Returns true if a device reset happened and the results of a readback must
// not be delivered: a reset can fire the async-read completion over
// never-written buffers. CheckResetStatus() marks the context lost when it
// returns true.
bool ShouldDiscardReadbackAfterDeviceReset(
    CopySharedImageHelper::ReadbackDelegate* delegate) {
  return delegate->workarounds().check_graphics_reset_status_after_readback &&
         !delegate->WasContextLost() && delegate->CheckResetStatus();
}

void OnReadPixelsDone(
    void* raw_ctx,
    std::unique_ptr<const SkImage::AsyncReadResult> async_result) {
  ReadPixelsContext* context = reinterpret_cast<ReadPixelsContext*>(raw_ctx);
  context->async_result = std::move(async_result);
  context->finished = true;
}

}  // namespace

CopySharedImageHelper::CopySharedImageHelper(
    SharedImageRepresentationFactory* representation_factory,
    SharedContextState* shared_context_state)
    : representation_factory_(representation_factory),
      shared_context_state_(shared_context_state) {}

CopySharedImageHelper::~CopySharedImageHelper() = default;

CopySharedImageHelper::GLError::GLError(GLenum gl_error,
                                        std::string function_name,
                                        std::string msg)
    : gl_error(gl_error),
      function_name(std::move(function_name)),
      msg(std::move(msg)) {}

base::expected<void, GLError> CopySharedImageHelper::CopySharedImage(
    GLint xoffset,
    GLint yoffset,
    GLint x,
    GLint y,
    GLsizei src_width,
    GLsizei src_height,
    GLsizei dst_width,
    GLsizei dst_height,
    const volatile GLbyte* mailboxes) {
  Mailbox source_mailbox = Mailbox::FromVolatile(
      reinterpret_cast<const volatile Mailbox*>(mailboxes)[0]);
  DLOG_IF(ERROR, !source_mailbox.Verify())
      << "CopySharedImage was passed an invalid mailbox";
  Mailbox dest_mailbox = Mailbox::FromVolatile(
      UNSAFE_TODO(reinterpret_cast<const volatile Mailbox*>(mailboxes)[1]));
  DLOG_IF(ERROR, !dest_mailbox.Verify())
      << "CopySharedImage was passed an invalid mailbox";

  if (source_mailbox == dest_mailbox) {
    return base::unexpected(
        GLError(GL_INVALID_OPERATION, "CopySharedImage",
                "source and destination mailboxes are the same"));
  }

  auto dest_shared_image = representation_factory_->ProduceSkia(
      dest_mailbox,
      scoped_refptr<gpu::SharedContextState>(shared_context_state_));
  if (!dest_shared_image) {
    return base::unexpected(
        GLError(GL_INVALID_VALUE, "CopySharedImage", "unknown mailbox"));
  }

  auto dest_format = dest_shared_image->format();
  // Destination shared image cannot prefer external sampler.
  if (dest_format.PrefersExternalSampler()) {
    return base::unexpected(
        GLError(GL_INVALID_VALUE, "CopySharedImage", "unexpected format"));
  }

  gfx::Size dest_size = dest_shared_image->size();
  gfx::Rect dest_rect(xoffset, yoffset, dst_width, dst_height);
  if (dest_rect.IsEmpty() || !gfx::Rect(dest_size).Contains(dest_rect)) {
    return base::unexpected(GLError(GL_INVALID_VALUE, "CopySharedImage",
                                    "destination texture bad dimensions."));
  }

  std::vector<GrBackendSemaphore> begin_semaphores;
  std::vector<GrBackendSemaphore> end_semaphores;

  // Allow uncleared access, as we manually handle clear tracking.
  std::unique_ptr<SkiaImageRepresentation::ScopedWriteAccess>
      dest_scoped_access = dest_shared_image->BeginScopedWriteAccess(
          &begin_semaphores, &end_semaphores,
          SharedImageRepresentation::AllowUnclearedAccess::kYes);
  if (!dest_scoped_access) {
    return base::unexpected(GLError(GL_INVALID_VALUE, "CopySharedImage",
                                    "Dest shared image is not writable"));
  }

  bool need_graphite_submit = dest_scoped_access->NeedGraphiteContextSubmit();
  bool update_cleared_rect = false;
  gfx::Rect new_cleared_rect;

  // Skia does not report GL errors from uploads, so check for driver errors
  // before marking an uncleared destination as cleared.
  const bool check_gl_upload_errors = !dest_shared_image->IsCleared() &&
                                      shared_context_state_->GrContextIsGL() &&
                                      !shared_context_state_->context_lost();
  gl::GLApi* const gl_api =
      check_gl_upload_errors ? gl::g_current_gl_context : nullptr;
  DrainGLErrors(gl_api);

  // Flush dest surface and submit if necessary before exiting.
  absl::Cleanup cleanup = [&]() {
    bool success =
        shared_context_state_->FlushWriteAccess(dest_scoped_access.get());
    success &= shared_context_state_->SubmitIfNecessary(
        std::move(end_semaphores), need_graphite_submit);
    if (success && update_cleared_rect &&
        DrainGLErrors(gl_api) == GL_NO_ERROR) {
      dest_shared_image->SetClearedRect(new_cleared_rect);
    }
  };

  gfx::Rect old_cleared_rect = dest_shared_image->ClearedRect();
  if (!gles2::TextureManager::CombineAdjacentRects(old_cleared_rect, dest_rect,
                                                   &new_cleared_rect)) {
    // No users of RasterDecoder leverage this functionality. Clearing uncleared
    // regions could be added here if needed.
    return base::unexpected(GLError(GL_INVALID_VALUE, "CopySharedImage",
                                    "Cannot clear non-combineable rects."));
  }
  DCHECK(old_cleared_rect.IsEmpty() ||
         new_cleared_rect.Contains(old_cleared_rect));

  // Attempt to upload directly from CPU shared memory to destination texture.
  // Only do this if no scaling is happening.
  GLenum upload_error = GL_NO_ERROR;
  if (src_width == dst_width && src_height == dst_height &&
      CopyPixelsToTexture(
          xoffset, yoffset, x, y, src_width, src_height, new_cleared_rect,
          source_mailbox, dest_shared_image.get(), dest_scoped_access.get(),
          representation_factory_, shared_context_state_, begin_semaphores,
          end_semaphores, gl_api, upload_error)) {
    // Cancel cleanup as CopyPixelsToTexture already handles it.
    std::move(cleanup).Cancel();
    if (upload_error != GL_NO_ERROR) {
      return base::unexpected(
          GLError(upload_error, "CopySharedImage",
                  "GL driver reported an error during texture upload"));
    }
    return base::ok();
  }

  // Fall back to GPU->GPU copy if src image is not CPU-backed.
  auto source_shared_image = representation_factory_->ProduceSkia(
      source_mailbox,
      scoped_refptr<gpu::SharedContextState>(shared_context_state_));

  // In some cases (e.g android video that is promoted to overlay) we can't
  // create representation of the valid mailbox. To avoid problems with
  // uncleared destination later, we do clear destination rect with black
  // color.
  if (!source_shared_image) {
    if (dest_format.is_single_plane()) {
      auto* canvas = dest_scoped_access->surface()->getCanvas();
      SkAutoCanvasRestore autoRestore(canvas, /*doSave=*/true);
      canvas->clipRect(gfx::RectToSkRect(dest_rect));
      canvas->clear(SkColors::kBlack);
    } else {
      std::array<SkSurface*, SkYUVAInfo::kMaxPlanes> yuva_sk_surfaces = {};
      for (int plane_index = 0; plane_index < dest_format.NumberOfPlanes();
           plane_index++) {
        // Get surface per plane from destination scoped write access.
        yuva_sk_surfaces[plane_index] =
            dest_scoped_access->surface(plane_index);
      }

      // TODO(crbug.com/41380578): This should really default to rec709.
      SkYUVColorSpace yuv_color_space = kRec601_SkYUVColorSpace;
      dest_shared_image->color_space().ToSkYUVColorSpace(
          dest_format.MultiplanarBitDepth(), &yuv_color_space);

      SkYUVAInfo yuva_info(gfx::SizeToSkISize(dest_shared_image->size()),
                           ToSkYUVAPlaneConfig(dest_format),
                           ToSkYUVASubsampling(dest_format), yuv_color_space);
      skia::BlitRGBAToYUVA(/*src_image=*/nullptr, yuva_sk_surfaces, yuva_info,
                           gfx::RectToSkRect(dest_rect));
    }

    if (!dest_shared_image->IsCleared()) {
      update_cleared_rect = true;
    }

    // Note, that we still generate error for the client to indicate there was
    // problem.
    return base::unexpected(GLError(GL_INVALID_VALUE, "CopySharedImage",
                                    "unknown source image mailbox."));
  }

  gfx::Size source_size = source_shared_image->size();
  gfx::Rect source_rect(x, y, src_width, src_height);
  if (source_rect.IsEmpty() || !gfx::Rect(source_size).Contains(source_rect)) {
    return base::unexpected(GLError(GL_INVALID_VALUE, "CopySharedImage",
                                    "source texture bad dimensions."));
  }

  std::unique_ptr<SkiaImageRepresentation::ScopedReadAccess>
      source_scoped_access = source_shared_image->BeginScopedReadAccess(
          &begin_semaphores, &end_semaphores);
  if (!begin_semaphores.empty()) {
    GrDirectContext* direct_context = shared_context_state_->gr_context();
    bool ret =
        direct_context->wait(begin_semaphores.size(), begin_semaphores.data(),
                             /*deleteSemaphoresAfterWait=*/false);
    DCHECK(ret);
  }
  if (!source_scoped_access) {
    return base::unexpected(GLError(GL_INVALID_VALUE, "CopySharedImage",
                                    "Source shared image is not accessable"));
  }

  // Update submit is needed by `source_scoped_access`.
  need_graphite_submit |= source_scoped_access->NeedGraphiteContextSubmit();

  base::expected<void, GLError> result = base::ok();
  auto source_image =
      source_scoped_access->CreateSkImage(shared_context_state_);
  if (!source_image) {
    result = base::unexpected(
        GLError(GL_INVALID_VALUE, "CopySharedImage",
                "Couldn't create SkImage from source shared image."));
  } else {
    if (dest_format.is_single_plane()) {
      auto* canvas = dest_scoped_access->surface()->getCanvas();

      // Reinterpret the source image as being in the destination color space,
      // to disable color conversion.
      auto source_image_reinterpreted = source_image;
      if (canvas->imageInfo().colorSpace()) {
        source_image_reinterpreted = source_image->reinterpretColorSpace(
            canvas->imageInfo().refColorSpace());
      }

      SkPaint paint;
      paint.setBlendMode(SkBlendMode::kSrc);

      canvas->drawImageRect(source_image_reinterpreted,
                            gfx::RectToSkRect(source_rect),
                            gfx::RectToSkRect(dest_rect), SkSamplingOptions(),
                            &paint, SkCanvas::kStrict_SrcRectConstraint);
    } else {
      std::array<SkSurface*, SkYUVAInfo::kMaxPlanes> yuva_sk_surfaces = {};
      for (int plane_index = 0; plane_index < dest_format.NumberOfPlanes();
           plane_index++) {
        // Get surface per plane from destination scoped write access.
        yuva_sk_surfaces[plane_index] =
            dest_scoped_access->surface(plane_index);
      }

      // TODO(crbug.com/41380578): This should really default to rec709.
      SkYUVColorSpace yuv_color_space = kRec601_SkYUVColorSpace;
      dest_shared_image->color_space().ToSkYUVColorSpace(
          dest_format.MultiplanarBitDepth(), &yuv_color_space);

      SkYUVAInfo yuva_info(gfx::SizeToSkISize(dest_shared_image->size()),
                           ToSkYUVAPlaneConfig(dest_format),
                           ToSkYUVASubsampling(dest_format), yuv_color_space);
      // Perform skia::BlitRGBAToYUVA for the multiplanar YUV format image.
      // TODO(crbug.com/40270413): This will scale the image if the source image
      // is smaller than the destination image. What we should actually do
      // instead is just blit the destination rect and clear out the rest.
      // However, doing that resulted in resulted in pixeltest failures due to
      // images having pixel bleeding at their borders when this codepath is
      // used by RenderableGMBVideoFramePool (see the bug for details). The
      // current behavior of scaling the image matches the legacy
      // (non-multiplanar SI) behavior in RenderableGMBVideoFramePool, so it is
      // not a regression. Nonetheless, this behavior should
      // ideally be changed to that described above for correctness.
      if (dst_width != dest_size.width() || dst_height != dest_size.height()) {
        dest_rect = gfx::Rect(dest_size);
      }
      skia::BlitRGBAToYUVA(source_image.get(), yuva_sk_surfaces, yuva_info,
                           gfx::RectToSkRect(dest_rect), false,
                           gfx::RectToSkRect(source_rect));
      new_cleared_rect = gfx::Rect(dest_shared_image->size());
      update_cleared_rect = true;
    }

    if (!dest_shared_image->IsCleared()) {
      update_cleared_rect = true;
    }
  }

  // Cancel cleanup as the cleanup order is different here.
  std::move(cleanup).Cancel();
  bool success =
      shared_context_state_->FlushWriteAccess(dest_scoped_access.get());
  source_scoped_access->ApplyBackendSurfaceEndState();
  success &= shared_context_state_->SubmitIfNecessary(std::move(end_semaphores),
                                                      need_graphite_submit);

  if (success && update_cleared_rect) {
    GLenum error = DrainGLErrors(gl_api);
    if (error != GL_NO_ERROR) {
      if (result.has_value()) {
        result = base::unexpected(
            GLError(error, "CopySharedImage",
                    "GL driver reported an error during texture upload"));
      }
    } else {
      dest_shared_image->SetClearedRect(new_cleared_rect);
    }
  }
  return result;
}

base::expected<void, GLError> CopySharedImageHelper::CopySharedImageToGLTexture(
    GLuint dest_texture_id,
    GLenum target,
    GLuint internal_format,
    GLenum type,
    GLint src_x,
    GLint src_y,
    GLsizei width,
    GLsizei height,
    GrSurfaceOrigin dst_origin,
    const volatile GLbyte* src_mailbox) {
  Mailbox source_mailbox = Mailbox::FromVolatile(
      reinterpret_cast<const volatile Mailbox*>(src_mailbox)[0]);
  DLOG_IF(ERROR, !source_mailbox.Verify())
      << "CopySharedImageToGLTexture was passed an invalid mailbox";

  GrDirectContext* direct_context = shared_context_state_->gr_context();
  CHECK(direct_context);

  sk_sp<SkSurface> dest_surface = CreateSkSurfaceWrappingGLTexture(
      shared_context_state_, dest_texture_id, target, internal_format, type,
      width, height, dst_origin);

  if (!dest_surface) {
    return base::unexpected<GLError>(
        GLError(GL_INVALID_VALUE, "CopySharedImageToGLTexture",
                "Cannot create destination surface"));
  }

  // `dest_rect` always starts from (0, 0).
  SkRect dest_rect =
      SkRect::MakeWH(SkIntToScalar(width), SkIntToScalar(height));
  auto source_shared_image = representation_factory_->ProduceSkia(
      source_mailbox,
      scoped_refptr<gpu::SharedContextState>(shared_context_state_));

  // In some cases (e.g android video that is promoted to overlay) we can't
  // create representation of the valid mailbox. To avoid problems with
  // uncleared destination later, we do clear destination rect with black
  // color.
  if (!source_shared_image) {
    auto* canvas = dest_surface->getCanvas();

    SkAutoCanvasRestore autoRestore(canvas, /*doSave=*/true);
    canvas->clipRect(dest_rect);
    canvas->clear(SkColors::kBlack);

    direct_context->flush(dest_surface.get());
    shared_context_state_->SubmitIfNecessary(/*signal_semaphores=*/{},
                                             /*need_graphite_submit=*/false);

    // Note, that we still generate error for the client to indicate there was
    // problem.
    return base::unexpected<GLError>(GLError(GL_INVALID_VALUE,
                                             "CopySharedImageToGLTexture",
                                             "unknown source image mailbox."));
  }

  gfx::Size source_size = source_shared_image->size();
  gfx::Rect source_rect(src_x, src_y, width, height);
  if (source_rect.IsEmpty() || !gfx::Rect(source_size).Contains(source_rect)) {
    return base::unexpected<GLError>(GLError(GL_INVALID_VALUE,
                                             "CopySharedImageToGLTexture",
                                             "source texture bad dimensions."));
  }

  std::vector<GrBackendSemaphore> begin_semaphores;
  std::vector<GrBackendSemaphore> end_semaphores;
  std::unique_ptr<SkiaImageRepresentation::ScopedReadAccess>
      source_scoped_access = source_shared_image->BeginScopedReadAccess(
          &begin_semaphores, &end_semaphores);
  if (!begin_semaphores.empty()) {
    bool ret =
        dest_surface->wait(begin_semaphores.size(), begin_semaphores.data(),
                           /*deleteSemaphoresAfterWait=*/false);
    DCHECK(ret);
  }
  if (!source_scoped_access) {
    // We still need to flush surface for begin semaphores above.
    direct_context->flush(dest_surface.get());
    shared_context_state_->SubmitIfNecessary(std::move(end_semaphores),
                                             /*need_graphite_submit=*/false);

    return base::unexpected<GLError>(
        GLError(GL_INVALID_VALUE, "CopySharedImageToGLTexture",
                "Source shared image is not accessable"));
  }

  base::expected<void, GLError> result = base::ok();
  auto source_image =
      source_scoped_access->CreateSkImage(shared_context_state_);
  if (!source_image) {
    result = base::unexpected<GLError>(
        GLError(GL_INVALID_VALUE, "CopySharedImageToGLTexture",
                "Couldn't create SkImage from source shared image."));
  } else {
    auto* canvas = dest_surface->getCanvas();
    SkPaint paint;
    paint.setBlendMode(SkBlendMode::kSrc);

    // Reinterpret the source image as being in the destination color space,
    // to disable color conversion.
    auto source_image_reinterpreted = source_image;
    if (canvas->imageInfo().colorSpace()) {
      source_image_reinterpreted = source_image->reinterpretColorSpace(
          canvas->imageInfo().refColorSpace());
    }
    canvas->drawImageRect(
        source_image_reinterpreted, gfx::RectToSkRect(source_rect), dest_rect,
        SkSamplingOptions(), &paint, SkCanvas::kStrict_SrcRectConstraint);
  }

  direct_context->flush(dest_surface.get());
  source_scoped_access->ApplyBackendSurfaceEndState();
  shared_context_state_->SubmitIfNecessary(std::move(end_semaphores),
                                           /*need_graphite_submit=*/false);
  return result;
}

namespace {

// Graphite only supports asynchronous reads, and the asynchronous reads require
// that the src rect is contained within the image.
bool GraphiteImageReadPixels(GraphiteSharedContext* graphite_shared_context,
                             sk_sp<SkImage> sk_image,
                             int src_x,
                             int src_y,
                             const SkImageInfo& dst_info,
                             void* pixel_address,
                             size_t row_bytes) {
  gfx::Rect src_rect(src_x, src_y, dst_info.width(), dst_info.height());
  gfx::Rect src_image_bounds(sk_image->width(), sk_image->height());
  CHECK(!src_rect.IsEmpty() && src_image_bounds.Contains(src_rect));

  CHECK(graphite_shared_context);
  ReadPixelsContext context;

  // We don't need to insert a recording since asyncRescaleAndReadPixels is a
  // context operation that inserts its own recording internally.
  const bool submitted = graphite_shared_context->RunLockedAndSubmitSync([&] {
    graphite_shared_context->asyncRescaleAndReadPixels(
        sk_image.get(), dst_info, RectToSkIRect(src_rect),
        SkImage::RescaleGamma::kSrc, SkImage::RescaleMode::kRepeatedLinear,
        base::BindOnce(&OnReadPixelsDone), &context);
  });
  if (!submitted) {
    return false;
  }

  if (!context.finished && graphite_shared_context->IsContextLost()) {
    return false;
  }

  CHECK(context.finished);
  if (!context.async_result) {
    return false;
  }

  // Must copy the async results (often a GPU-mapped buffer) to the CPU
  // pixel_address.
  const int height = dst_info.height();
  libyuv::CopyPlane(static_cast<const uint8_t*>(context.async_result->data(0)),
                    context.async_result->rowBytes(0),
                    static_cast<uint8_t*>(pixel_address), row_bytes,
                    dst_info.width() * dst_info.bytesPerPixel(), height);

  return true;
}

}  // anonymous namespace

base::expected<void, GLError> CopySharedImageHelper::ReadPixels(
    GLint src_x,
    GLint src_y,
    GLint plane_index,
    GLuint row_bytes,
    SkImageInfo dst_info,
    void* pixel_address,
    std::unique_ptr<SkiaImageRepresentation> source_shared_image) {
  std::vector<GrBackendSemaphore> begin_semaphores;
  std::vector<GrBackendSemaphore> end_semaphores;
  std::unique_ptr<SkiaImageRepresentation::ScopedReadAccess>
      source_scoped_access = source_shared_image->BeginScopedReadAccess(
          &begin_semaphores, &end_semaphores);

  if (!source_scoped_access) {
    return base::unexpected(GLError(GL_INVALID_VALUE, "ReadPixels",
                                    "Source shared image is not accessible"));
  }

  auto* gr_context = shared_context_state_->gr_context();
  if (!begin_semaphores.empty()) {
    CHECK(gr_context);
    bool wait_result =
        gr_context->wait(begin_semaphores.size(), begin_semaphores.data(),
                         /*deleteSemaphoresAfterWait=*/false);
    DCHECK(wait_result);
  }

  sk_sp<SkImage> sk_image;
  if (source_shared_image->format().is_single_plane() ||
      source_shared_image->format().PrefersExternalSampler()) {
    // Create SkImage without plane index for single planar formats or legacy
    // multiplanar formats with external sampler.
    sk_image = source_scoped_access->CreateSkImage(shared_context_state_);
  } else {
    // Pass plane index for creating an SkImage for multiplanar formats.
    sk_image = source_scoped_access->CreateSkImageForPlane(
        plane_index, shared_context_state_);
  }

  if (!sk_image) {
    source_scoped_access->ApplyBackendSurfaceEndState();
    shared_context_state_->SubmitIfNecessary(
        std::move(end_semaphores),
        source_scoped_access->NeedGraphiteContextSubmit());
    return base::unexpected(GLError(GL_INVALID_OPERATION, "ReadPixels",
                                    "Couldn't create SkImage for reading."));
  }

  gfx::Rect src_rect(src_x, src_y, dst_info.width(), dst_info.height());
  gfx::Rect src_image_bounds(sk_image->width(), sk_image->height());
  if (src_rect.IsEmpty() || !src_image_bounds.Contains(src_rect)) {
    source_scoped_access->ApplyBackendSurfaceEndState();
    shared_context_state_->SubmitIfNecessary(
        std::move(end_semaphores),
        source_scoped_access->NeedGraphiteContextSubmit());
    return base::unexpected(GLError(GL_INVALID_VALUE, "ReadPixels",
                                    "Trying to read outside source image."));
  }

  bool success = false;
  if (gr_context) {
    success = sk_image->readPixels(gr_context, dst_info, pixel_address,
                                   row_bytes, src_x, src_y);
    source_scoped_access->ApplyBackendSurfaceEndState();
    shared_context_state_->SubmitIfNecessary(
        std::move(end_semaphores),
        /*need_graphite_shared_context_submit==*/false);
  } else {
    auto* graphite_shared_context =
        shared_context_state_->graphite_shared_context();
    success = GraphiteImageReadPixels(graphite_shared_context,
                                      std::move(sk_image), src_x, src_y,
                                      dst_info, pixel_address, row_bytes);
  }
  if (!success) {
    return base::unexpected(GLError(GL_INVALID_OPERATION, "ReadPixels",
                                    "Failed to read pixels from SkImage"));
  }
  return base::ok();
}

base::expected<void, GLError> CopySharedImageHelper::ReadPixelsYUV(
    ReadbackDelegate* delegate,
    GLuint src_x,
    GLuint src_y,
    GLuint src_width,
    GLuint src_height,
    GLuint dst_width,
    GLuint dst_height,
    uint8_t* y_out,
    GLuint y_stride,
    uint8_t* u_out,
    GLuint u_stride,
    uint8_t* v_out,
    GLuint v_stride,
    std::unique_ptr<SkiaImageRepresentation> source_shared_image) {
  std::vector<GrBackendSemaphore> begin_semaphores;
  std::vector<GrBackendSemaphore> end_semaphores;

  // We don't use |end_semaphores| here because we're going to sync with
  // with the CPU later regardless.
  std::unique_ptr<SkiaImageRepresentation::ScopedReadAccess>
      source_scoped_access = source_shared_image->BeginScopedReadAccess(
          &begin_semaphores, &end_semaphores);

  if (!begin_semaphores.empty()) {
    CHECK(shared_context_state_->gr_context());
    bool wait_result = shared_context_state_->gr_context()->wait(
        begin_semaphores.size(), begin_semaphores.data(),
        /*deleteSemaphoresAfterWait=*/false);
    DCHECK(wait_result);
  }

  if (!source_scoped_access) {
    return base::unexpected(GLError(GL_INVALID_VALUE, "glReadbackImagePixels",
                                    "Source shared image is not accessible"));
  }

  const SkIRect src_rect =
      SkIRect::MakeXYWH(src_x, src_y, src_width, src_height);
  const SkISize dst_size = SkISize::Make(dst_width, dst_height);

  // NV12 sources are read back plane by plane: the Y and UV planes are
  // rescaled and read as R8 / RG8 images and the UV plane is split on the CPU.
  // This avoids the YUV->RGB conversion, the RGB->YUV conversion and the extra
  // render passes of the generic YUV420 readback used for everything else.
  // The chroma plane is subsampled, so the source rect and destination size
  // must be even for the per plane rects to be exact.
  const viz::SharedImageFormat format = source_shared_image->format();
  bool read_planes = format == viz::MultiPlaneFormat::kNV12 &&
                     !format.PrefersExternalSampler() && src_x % 2 == 0 &&
                     src_y % 2 == 0 && src_width % 2 == 0 &&
                     src_height % 2 == 0 && dst_width % 2 == 0 &&
                     dst_height % 2 == 0 && dst_width > 0 && dst_height > 0;
#if BUILDFLAG(IS_WIN)
  // With Ganesh on GL (ANGLE over D3D11) a plane can be drawn but not read
  // directly: glReadPixels() from the plane texture into a pixel pack buffer
  // tries to create an NV12 staging texture and crashes. Skia reads directly
  // when no rescale is needed, so leave the unscaled case to the generic path
  // there.
  const bool is_unscaled = src_width == dst_width && src_height == dst_height;
  if (is_unscaled && shared_context_state_->GrContextIsGL()) {
    read_planes = false;
  }
#endif  // BUILDFLAG(IS_WIN)
  if (read_planes) {
    return ReadPixelsYUVInternalNV12(
        delegate, source_scoped_access.get(), std::move(end_semaphores),
        src_rect, dst_size, y_out, y_stride, u_out, u_stride, v_out, v_stride);
  }
  return ReadPixelsYUVInternalRGB(
      delegate, *source_shared_image, source_scoped_access.get(),
      std::move(end_semaphores), src_rect, dst_size, y_out, y_stride, u_out,
      u_stride, v_out, v_stride);
}

base::unexpected<GLError> CopySharedImageHelper::EndReadAccessWithError(
    SkiaImageRepresentation::ScopedReadAccess* source_scoped_access,
    std::vector<GrBackendSemaphore> end_semaphores,
    GLenum error,
    const char* msg) {
  source_scoped_access->ApplyBackendSurfaceEndState();
  shared_context_state_->SubmitIfNecessary(
      std::move(end_semaphores),
      source_scoped_access->NeedGraphiteContextSubmit());
  return base::unexpected(GLError(error, "glReadbackYUVImagePixels", msg));
}

void CopySharedImageHelper::FinishGaneshReadback(
    ReadbackDelegate* delegate,
    SkiaImageRepresentation::ScopedReadAccess* source_scoped_access,
    std::vector<GrBackendSemaphore> end_semaphores) {
  source_scoped_access->ApplyBackendSurfaceEndState();
  if (!end_semaphores.empty()) {
    GrFlushInfo flush_info = {
        .fNumSemaphores = end_semaphores.size(),
        .fSignalSemaphores = end_semaphores.data(),
    };
    AddVulkanCleanupTaskForSkiaFlush(
        shared_context_state_->vk_context_provider(), &flush_info);
    shared_context_state_->gr_context()->flush(flush_info);
  }

  // TODO(crbug.com/40106956): Use COMMANDS_COMPLETED query for async
  // readback.
  delegate->DoFinish();
}

base::expected<void, GLError> CopySharedImageHelper::ReadPixelsYUVInternalNV12(
    ReadbackDelegate* delegate,
    SkiaImageRepresentation::ScopedReadAccess* source_scoped_access,
    std::vector<GrBackendSemaphore> end_semaphores,
    const SkIRect& src_rect,
    const SkISize& dst_size,
    uint8_t* y_out,
    GLuint y_stride,
    uint8_t* u_out,
    GLuint u_stride,
    uint8_t* v_out,
    GLuint v_stride) {
  sk_sp<SkImage> sk_y_image =
      source_scoped_access->CreateSkImageForPlane(0, shared_context_state_);
  sk_sp<SkImage> sk_uv_image =
      source_scoped_access->CreateSkImageForPlane(1, shared_context_state_);
  if (!sk_y_image || !sk_uv_image) {
    return EndReadAccessWithError(
        source_scoped_access, std::move(end_semaphores), GL_INVALID_OPERATION,
        "Couldn't create SkImage for reading NV12 planes.");
  }
  if (src_rect.isEmpty() ||
      !SkIRect::MakeSize(sk_y_image->dimensions()).contains(src_rect)) {
    return EndReadAccessWithError(
        source_scoped_access, std::move(end_semaphores), GL_INVALID_VALUE,
        "SRC rect is outside of source image dimensions");
  }

  // The planes keep their own color spaces so that no color conversion is
  // attempted; they are read as-is. The caller guarantees that the rect and
  // the size are even, so the half size UV plane rects are exact.
  const SkIRect uv_src_rect =
      SkIRect::MakeXYWH(src_rect.x() / 2, src_rect.y() / 2,
                        src_rect.width() / 2, src_rect.height() / 2);
  const SkImageInfo y_info =
      SkImageInfo::Make(dst_size, kR8_unorm_SkColorType, kOpaque_SkAlphaType,
                        sk_y_image->refColorSpace());
  const SkImageInfo uv_info = SkImageInfo::Make(
      dst_size.width() / 2, dst_size.height() / 2, kR8G8_unorm_SkColorType,
      kOpaque_SkAlphaType, sk_uv_image->refColorSpace());

  // Readback is potentially slow, so report progress here.
  gl::ScopedProgressReporter report_progress(
      shared_context_state_->progress_reporter());

  // While the Skia functions are asynchronous, the sync with the GPU below
  // runs their callbacks before this function returns.
  ReadPixelsContext y_result;
  ReadPixelsContext uv_result;
  bool finished = false;
  bool is_context_lost = false;
  if (auto* graphite_shared_context =
          shared_context_state_->graphite_shared_context()) {
    // SkImage/SkSurface asyncRescaleAndReadPixels methods won't be implemented
    // for Graphite. Instead the equivalent methods are on the Graphite context.
    // Both reads and the submit that waits for them happen under one lock, so
    // that their callbacks run on this thread before RunLockedAndSubmitSync()
    // returns.
    const bool submitted = graphite_shared_context->RunLockedAndSubmitSync([&] {
      graphite_shared_context->asyncRescaleAndReadPixels(
          sk_y_image.get(), y_info, src_rect, SkImage::RescaleGamma::kSrc,
          SkImage::RescaleMode::kRepeatedLinear,
          base::BindOnce(&OnReadPixelsDone), &y_result);
      graphite_shared_context->asyncRescaleAndReadPixels(
          sk_uv_image.get(), uv_info, uv_src_rect, SkImage::RescaleGamma::kSrc,
          SkImage::RescaleMode::kRepeatedLinear,
          base::BindOnce(&OnReadPixelsDone), &uv_result);
    });
    finished = submitted && y_result.finished && uv_result.finished;
    is_context_lost = !finished && graphite_shared_context->IsContextLost();
  } else {
    auto* gr_context = shared_context_state_->gr_context();
    CHECK(gr_context);
    sk_y_image->asyncRescaleAndReadPixels(
        y_info, src_rect, SkImage::RescaleGamma::kSrc,
        SkImage::RescaleMode::kRepeatedLinear, &OnReadPixelsDone, &y_result);
    sk_uv_image->asyncRescaleAndReadPixels(
        uv_info, uv_src_rect, SkImage::RescaleGamma::kSrc,
        SkImage::RescaleMode::kRepeatedLinear, &OnReadPixelsDone, &uv_result);
    FinishGaneshReadback(delegate, source_scoped_access,
                         std::move(end_semaphores));
    finished = y_result.finished && uv_result.finished;
    is_context_lost =
        !finished && (delegate->WasContextLost() || gr_context->abandoned());
  }

  // The sync above must have run the callbacks unless the context was lost;
  // otherwise they could still run later and write to the stack contexts.
  CHECK(finished || is_context_lost);
  if (!y_result.async_result || !uv_result.async_result || is_context_lost) {
    return base::unexpected(GLError(GL_INVALID_OPERATION,
                                    "glReadbackYUVImagePixels",
                                    "Failed to read pixels from SkImage"));
  }
  if (ShouldDiscardReadbackAfterDeviceReset(delegate)) {
    return base::unexpected(GLError(GL_INVALID_OPERATION,
                                    "glReadbackYUVImagePixels",
                                    "readback aborted: device reset detected"));
  }

  const auto& y = y_result.async_result;
  const auto& uv = uv_result.async_result;
  libyuv::CopyPlane(static_cast<const uint8_t*>(y->data(0)), y->rowBytes(0),
                    y_out, y_stride, dst_size.width(), dst_size.height());
  libyuv::SplitUVPlane(static_cast<const uint8_t*>(uv->data(0)),
                       uv->rowBytes(0), u_out, u_stride, v_out, v_stride,
                       uv_info.width(), uv_info.height());
  return base::ok();
}

base::expected<void, GLError> CopySharedImageHelper::ReadPixelsYUVInternalRGB(
    ReadbackDelegate* delegate,
    const SkiaImageRepresentation& source_shared_image,
    SkiaImageRepresentation::ScopedReadAccess* source_scoped_access,
    std::vector<GrBackendSemaphore> end_semaphores,
    const SkIRect& src_rect,
    const SkISize& dst_size,
    uint8_t* y_out,
    GLuint y_stride,
    uint8_t* u_out,
    GLuint u_stride,
    uint8_t* v_out,
    GLuint v_stride) {
  sk_sp<SkImage> sk_image =
      source_scoped_access->CreateSkImage(shared_context_state_.get());
  if (!sk_image) {
    return EndReadAccessWithError(
        source_scoped_access, std::move(end_semaphores), GL_INVALID_OPERATION,
        "Couldn't create SkImage for reading.");
  }
  if (src_rect.isEmpty() ||
      !SkIRect::MakeSize(sk_image->dimensions()).contains(src_rect)) {
    return EndReadAccessWithError(
        source_scoped_access, std::move(end_semaphores), GL_INVALID_VALUE,
        "SRC rect is outside of source image dimensions");
  }

  // YUV readback requires knowing what SkColorSpace the RGB data should be
  // converted to AND the SkYUVColorSpace that defines how YUV is derived from
  // the RGB data. To avoid colorspace conversion, this always uses the
  // SkColorSpace on `sk_image` (derived from the gfx::ColorSpace of the shared
  // image). If the source image is multi-planar, try to match its YUV color
  // space, otherwise default to Rec 709 for multi-planar and Rec 601 for
  // single-plane formats.
  SkYUVColorSpace yuv_cs = kRec601_Limited_SkYUVColorSpace;
  if (source_shared_image.format().is_multi_plane()) {
    yuv_cs = kRec709_Limited_SkYUVColorSpace;
    source_shared_image.color_space().ToSkYUVColorSpace(&yuv_cs);
  }

  // Readback is potentially slow, so report progress here.
  gl::ScopedProgressReporter report_progress(
      shared_context_state_->progress_reporter());

  // While the Skia functions are asynchronous, the sync with the GPU below
  // runs their callbacks before this function returns.
  ReadPixelsContext yuv_result;
  bool finished = false;
  bool is_context_lost = false;
  if (auto* graphite_shared_context =
          shared_context_state_->graphite_shared_context()) {
    // SkImage/SkSurface asyncRescaleAndReadPixels methods won't be implemented
    // for Graphite. Instead the equivalent methods are on the Graphite context.
    // The read and the submit that waits for it happen under one lock, so
    // that its callback runs on this thread before RunLockedAndSubmitSync()
    // returns.
    const bool submitted = graphite_shared_context->RunLockedAndSubmitSync([&] {
      graphite_shared_context->asyncRescaleAndReadPixelsYUV420(
          sk_image.get(), yuv_cs, sk_image->refColorSpace(), src_rect, dst_size,
          SkImage::RescaleGamma::kSrc, SkImage::RescaleMode::kRepeatedLinear,
          base::BindOnce(&OnReadPixelsDone), &yuv_result);
    });
    finished = submitted && yuv_result.finished;
    is_context_lost = !finished && graphite_shared_context->IsContextLost();
  } else {
    auto* gr_context = shared_context_state_->gr_context();
    CHECK(gr_context);
    sk_image->asyncRescaleAndReadPixelsYUV420(
        yuv_cs, sk_image->refColorSpace(), src_rect, dst_size,
        SkImage::RescaleGamma::kSrc, SkImage::RescaleMode::kRepeatedLinear,
        &OnReadPixelsDone, &yuv_result);
    FinishGaneshReadback(delegate, source_scoped_access,
                         std::move(end_semaphores));
    finished = yuv_result.finished;
    is_context_lost =
        !finished && (delegate->WasContextLost() || gr_context->abandoned());
  }

  // The sync above must have run the callback unless the context was lost;
  // otherwise it could still run later and write to the stack context.
  CHECK(finished || is_context_lost);
  if (!yuv_result.async_result || is_context_lost) {
    return base::unexpected(GLError(GL_INVALID_OPERATION,
                                    "glReadbackYUVImagePixels",
                                    "Failed to read pixels from SkImage"));
  }
  if (ShouldDiscardReadbackAfterDeviceReset(delegate)) {
    return base::unexpected(GLError(GL_INVALID_OPERATION,
                                    "glReadbackYUVImagePixels",
                                    "readback aborted: device reset detected"));
  }

  const auto& yuv = yuv_result.async_result;
  libyuv::I420Copy(static_cast<const uint8_t*>(yuv->data(0)), yuv->rowBytes(0),
                   static_cast<const uint8_t*>(yuv->data(1)), yuv->rowBytes(1),
                   static_cast<const uint8_t*>(yuv->data(2)), yuv->rowBytes(2),
                   y_out, y_stride, u_out, u_stride, v_out, v_stride,
                   dst_size.width(), dst_size.height());
  return base::ok();
}

base::expected<void, GLError> CopySharedImageHelper::WritePixelsYUV(
    GLuint src_width,
    GLuint src_height,
    std::array<SkPixmap, SkYUVAInfo::kMaxPlanes> pixmaps,
    std::vector<GrBackendSemaphore> end_semaphores,
    std::unique_ptr<SkiaImageRepresentation> dest_shared_image,
    std::unique_ptr<SkiaImageRepresentation::ScopedWriteAccess>
        dest_scoped_access) {
  // Order of destruction for function arguments is not specified, but the
  // ScopedWriteAccess must be destroyed before representation; so perform a
  // Cleanup before exiting.
  absl::Cleanup cleanup = [&]() { dest_scoped_access.reset(); };
  viz::SharedImageFormat dest_format = dest_shared_image->format();
  auto* gr_context = shared_context_state_->gr_context();
  const bool need_graphite_submit =
      dest_scoped_access->NeedGraphiteContextSubmit();
  const bool check_gl_upload_errors = !dest_shared_image->IsCleared() &&
                                      shared_context_state_->GrContextIsGL() &&
                                      !shared_context_state_->context_lost();
  gl::GLApi* const gl_api =
      check_gl_upload_errors ? gl::g_current_gl_context : nullptr;
  if (gl_api) {
    DrainGLErrors(gl_api);
  }

  for (int plane = 0; plane < dest_format.NumberOfPlanes(); plane++) {
    bool written = false;
    if (gr_context) {
      written = gr_context->updateBackendTexture(
          dest_scoped_access->promise_image_texture(plane)->backendTexture(),
          &pixmaps[plane], /*numLevels=*/1, dest_shared_image->surface_origin(),
          /*finishedProc=*/nullptr, /*finishedContext=*/nullptr);
    } else {
      CHECK(shared_context_state_->graphite_shared_context());
      auto graphite_texture_ref =
          dest_scoped_access->graphite_texture_holder(plane);
      auto* graphite_texture_ptr = graphite_texture_ref.release();
      using graphite_texture_ptr_type = decltype(graphite_texture_ptr);
      auto release_proc = [](void* context, skgpu::CallbackResult) {
        static_cast<graphite_texture_ptr_type>(context)->Release();
      };
      written = shared_context_state_->gpu_main_graphite_recorder()
                    ->updateBackendTexture(
                        graphite_texture_ptr->texture(), &pixmaps[plane],
                        /*numLevels=*/1, release_proc, graphite_texture_ptr);
    }
    if (!written) {
      // Ensure that any recorded work for planes is inserted.
      shared_context_state_->FlushWriteAccess(dest_scoped_access.get());
      shared_context_state_->SubmitIfNecessary(std::move(end_semaphores),
                                               need_graphite_submit);
      return base::unexpected(
          GLError(GL_INVALID_OPERATION, "WritePixelsYUV",
                  "Failed to upload pixels to dest shared image"));
    }
  }

  bool success =
      shared_context_state_->FlushWriteAccess(dest_scoped_access.get());
  success &= shared_context_state_->SubmitIfNecessary(std::move(end_semaphores),
                                                      need_graphite_submit);

  if (success && !dest_shared_image->IsCleared()) {
    if (gl_api) {
      GLenum error = DrainGLErrors(gl_api);
      if (error != GL_NO_ERROR) {
        return base::unexpected(
            GLError(error, "WritePixelsYUV",
                    "GL driver reported an error during texture upload"));
      }
    }
    dest_shared_image->SetClearedRect(gfx::Rect(src_width, src_height));
  }
  return base::ok();
}

}  // namespace gpu
