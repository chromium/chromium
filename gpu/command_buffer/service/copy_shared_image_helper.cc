// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/copy_shared_image_helper.h"

#include <array>
#include <memory>
#include <vector>

#include "base/check.h"
#include "base/compiler_specific.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/service/graphite_shared_context.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image/shared_image_factory.h"
#include "gpu/command_buffer/service/shared_image/shared_image_format_service_utils.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "skia/ext/rgba_to_yuva.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"
#include "third_party/libyuv/include/libyuv/planar_functions.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkRect.h"
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
#include "ui/gfx/geometry/skia_conversions.h"

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
  // Get the surface color format similar to that in VideoFrameYUVConverter.
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
    std::vector<GrBackendSemaphore>& end_semaphores) {
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
  if (!gfx::Rect(source_size).Contains(source_rect)) {
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
    bool result = dest_scoped_access->surface()->wait(
        begin_semaphores.size(), begin_semaphores.data(),
        /*deleteSemaphoresAfterWait=*/false);
    DCHECK(result);
  }

  dest_scoped_access->surface()->writePixels(subset, xoffset, yoffset);

  shared_context_state->FlushWriteAccess(dest_scoped_access);
  shared_context_state->SubmitIfNecessary(
      std::move(end_semaphores),
      dest_scoped_access->NeedGraphiteContextSubmit());

  if (!dest_shared_image->IsCleared()) {
    dest_shared_image->SetClearedRect(dest_cleared_rect);
  }

  return true;
}

struct ReadPixelsContext {
  std::unique_ptr<const SkImage::AsyncReadResult> async_result;
  bool finished = false;
};

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
      << "CopySubTexture was passed an invalid mailbox";
  Mailbox dest_mailbox = Mailbox::FromVolatile(
      UNSAFE_TODO(reinterpret_cast<const volatile Mailbox*>(mailboxes)[1]));
  DLOG_IF(ERROR, !dest_mailbox.Verify())
      << "CopySubTexture was passed an invalid mailbox";

  if (source_mailbox == dest_mailbox) {
    return base::unexpected(
        GLError(GL_INVALID_OPERATION, "glCopySubTexture",
                "source and destination mailboxes are the same"));
  }

  auto dest_shared_image = representation_factory_->ProduceSkia(
      dest_mailbox,
      scoped_refptr<gpu::SharedContextState>(shared_context_state_));
  if (!dest_shared_image) {
    return base::unexpected(
        GLError(GL_INVALID_VALUE, "glCopySubTexture", "unknown mailbox"));
  }

  auto dest_format = dest_shared_image->format();
  // Destination shared image cannot prefer external sampler.
  if (dest_format.PrefersExternalSampler()) {
    return base::unexpected(
        GLError(GL_INVALID_VALUE, "glCopySubTexture", "unexpected format"));
  }

  gfx::Size dest_size = dest_shared_image->size();
  gfx::Rect dest_rect(xoffset, yoffset, dst_width, dst_height);
  if (!gfx::Rect(dest_size).Contains(dest_rect)) {
    return base::unexpected(GLError(GL_INVALID_VALUE, "glCopySubTexture",
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
    return base::unexpected(GLError(GL_INVALID_VALUE, "glCopySubTexture",
                                    "Dest shared image is not writable"));
  }

  bool need_graphite_submit = dest_scoped_access->NeedGraphiteContextSubmit();
  // Flush dest surface and submit if necessary before exiting.
  absl::Cleanup cleanup = [&]() {
    shared_context_state_->FlushWriteAccess(dest_scoped_access.get());
    shared_context_state_->SubmitIfNecessary(std::move(end_semaphores),
                                             need_graphite_submit);
  };

  gfx::Rect new_cleared_rect;
  gfx::Rect old_cleared_rect = dest_shared_image->ClearedRect();
  if (!gles2::TextureManager::CombineAdjacentRects(old_cleared_rect, dest_rect,
                                                   &new_cleared_rect)) {
    // No users of RasterDecoder leverage this functionality. Clearing uncleared
    // regions could be added here if needed.
    return base::unexpected(GLError(GL_INVALID_VALUE, "glCopySubTexture",
                                    "Cannot clear non-combineable rects."));
  }
  DCHECK(old_cleared_rect.IsEmpty() ||
         new_cleared_rect.Contains(old_cleared_rect));

  // Attempt to upload directly from CPU shared memory to destination texture.
  // Only do this if no scaling is happening.
  if (src_width == dst_width && src_height == dst_height &&
      CopyPixelsToTexture(xoffset, yoffset, x, y, src_width, src_height,
                          new_cleared_rect, source_mailbox,
                          dest_shared_image.get(), dest_scoped_access.get(),
                          representation_factory_, shared_context_state_,
                          begin_semaphores, end_semaphores)) {
    // Cancel cleanup as CopyPixelsToTexture already handles it.
    std::move(cleanup).Cancel();
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
      dest_shared_image->SetClearedRect(new_cleared_rect);
    }

    // Note, that we still generate error for the client to indicate there was
    // problem.
    return base::unexpected(GLError(GL_INVALID_VALUE, "glCopySubTexture",
                                    "unknown source image mailbox."));
  }

  gfx::Size source_size = source_shared_image->size();
  gfx::Rect source_rect(x, y, src_width, src_height);
  if (!gfx::Rect(source_size).Contains(source_rect)) {
    return base::unexpected(GLError(GL_INVALID_VALUE, "glCopySubTexture",
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
    return base::unexpected(GLError(GL_INVALID_VALUE, "glCopySubTexture",
                                    "Source shared image is not accessable"));
  }

  // Update submit is needed by `source_scoped_access`.
  need_graphite_submit |= source_scoped_access->NeedGraphiteContextSubmit();

  base::expected<void, GLError> result = base::ok();
  auto source_image =
      source_scoped_access->CreateSkImage(shared_context_state_);
  if (!source_image) {
    result = base::unexpected(
        GLError(GL_INVALID_VALUE, "glCopySubTexture",
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
      dest_shared_image->SetCleared();
    }

    if (!dest_shared_image->IsCleared()) {
      dest_shared_image->SetClearedRect(new_cleared_rect);
    }
  }

  // Cancel cleanup as the cleanup order is different here.
  std::move(cleanup).Cancel();
  shared_context_state_->FlushWriteAccess(dest_scoped_access.get());
  source_scoped_access->ApplyBackendSurfaceEndState();
  shared_context_state_->SubmitIfNecessary(std::move(end_semaphores),
                                           need_graphite_submit);
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
        GLError(GL_INVALID_VALUE, "glCopySharedImageToTexture",
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
                                             "glCopySharedImageToTexture",
                                             "unknown source image mailbox."));
  }

  gfx::Size source_size = source_shared_image->size();
  gfx::Rect source_rect(src_x, src_y, width, height);
  if (!gfx::Rect(source_size).Contains(source_rect)) {
    return base::unexpected<GLError>(GLError(GL_INVALID_VALUE,
                                             "glCopySharedImageToTexture",
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
        GLError(GL_INVALID_VALUE, "glCopySharedImageToTexture",
                "Source shared image is not accessable"));
  }

  base::expected<void, GLError> result = base::ok();
  auto source_image =
      source_scoped_access->CreateSkImage(shared_context_state_);
  if (!source_image) {
    result = base::unexpected<GLError>(
        GLError(GL_INVALID_VALUE, "glCopySharedImageToTexture",
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
// that the src rect is contained within the image. This function automatically
// adjusts the parameters to match the permissiveness of Ganesh's
// SkImage::readPixels and makes it synchronous.
bool GraphiteImageReadPixels(GraphiteSharedContext* graphite_shared_context,
                             sk_sp<SkImage> sk_image,
                             GrSurfaceOrigin src_surface_origin,
                             int src_x,
                             int src_y,
                             const SkImageInfo& dst_info,
                             void* pixel_address,
                             size_t row_bytes) {
  gfx::Rect src_rect(src_x, src_y, dst_info.width(), dst_info.height());
  gfx::Rect src_image_bounds(sk_image->width(), sk_image->height());

  // TODO(crbug.com/40942998): Once all src rects are required to be contained
  // in the image, the !Contains branch can be removed.
  if (!src_image_bounds.Contains(src_rect)) {
    src_rect.Intersect(src_image_bounds);
    if (src_rect.IsEmpty()) {
      // NOTE: This is consistent with SkImage::readPixels on a Ganesh image,
      // which permits src_rect to not be fully contained, but can't be disjoint
      return false;
    }

    // Adjust the pixel address to account for any intersection, so that the
    // available content remains aligned with the intended dst pixel data.
    // When `src_rect` was originally contained in the src image bounds, this
    // is equal to the original `pixel_address`.
    uint8_t* subset_pixel_addr =
        UNSAFE_TODO(static_cast<uint8_t*>(pixel_address) +
                    (src_rect.y() - src_y) * row_bytes +
                    (src_rect.x() - src_x) * dst_info.bytesPerPixel());
    SkImageInfo subset_dst_info =
        dst_info.makeWH(src_rect.width(), src_rect.height());

    // src_rect.Intersect(src_image_bounds) should ensure this call skips the
    // !Contains branch and actually reads the pixels. Check here to prevent
    // infinite recursion.
    CHECK(src_image_bounds.Contains(src_rect));
    return GraphiteImageReadPixels(graphite_shared_context, std::move(sk_image),
                                   src_surface_origin, src_rect.x(),
                                   src_rect.y(), subset_dst_info,
                                   subset_pixel_addr, row_bytes);
  }

  // Now that `src_rect` meets the requirements of the asyncRead API, call the
  // async function, then submit and block until it's completed.
  CHECK(graphite_shared_context);
  ReadPixelsContext context;

#if !defined(SK_GRAPHITE_READ_PIXELS_SUPPORTS_BOTTOM_LEFT)
  // Make the `src_rect` relative to the bottom left origin if needed. This
  // works around lack of support for bottom-left origin data during readback in
  // Graphite. Graphite correctly handles bottom-left origins when rendering, so
  // if asyncRescaleAndReadPixels() were to render `sk_image` for any reason,
  // this adjustment won't work. But this call isn't scaling and is presumably
  // for a shared image that has copy-src usage, so it shouldn't happen. This is
  // also a no-op when the src rect is the entire image, regardless of origin.
  if (src_surface_origin == kBottomLeft_GrSurfaceOrigin) {
    src_y = src_image_bounds.height() - src_rect.y() - src_rect.height();
    src_rect = gfx::Rect(
        src_rect.x(), src_y, src_rect.width(), src_rect.height());
    // This adjustment should not change the fact that src_rect is still valid
    CHECK(src_image_bounds.Contains(src_rect));
  }
#endif

  // We don't need to insert a recording since asyncRescaleAndReadPixels is a
  // context operation that inserts its own recording internally.
  if (!graphite_shared_context->asyncRescaleAndReadPixelsAndSubmit(
          sk_image.get(), dst_info, RectToSkIRect(src_rect),
          SkImage::RescaleGamma::kSrc, SkImage::RescaleMode::kRepeatedLinear,
          base::BindOnce(&OnReadPixelsDone), &context)) {
    return false;
  }

  CHECK(context.finished);
  if (!context.async_result) {
    return false;
  }

#if !defined(SK_GRAPHITE_READ_PIXELS_SUPPORTS_BOTTOM_LEFT)
  // Use CopyPlane to flip as Graphite doesn't support bottom left origin
  // images. Using a negative height causes CopyPlane to flip while copying.
  // TODO(crbug.com/40269891): Remove this if Graphite performs the flip
  // once it supports bottom left origin images.
  const int height = src_surface_origin == kTopLeft_GrSurfaceOrigin
                         ? dst_info.height()
                         : -dst_info.height();
#else
  // Must copy the async results (often a GPU-mapped buffer) to the CPU
  // pixel_address.
  const int height = dst_info.height();
#endif
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
    return base::unexpected(GLError(GL_INVALID_VALUE, "glReadbackImagePixels",
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
    return base::unexpected(GLError(GL_INVALID_OPERATION,
                                    "glReadbackImagePixels",
                                    "Couldn't create SkImage for reading."));
  }

  // TODO(crbug.com/40942998): Add back src_rect validation once renderer passes
  // a correct rect size.
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
    success =
        GraphiteImageReadPixels(graphite_shared_context, std::move(sk_image),
                                source_shared_image->surface_origin(), src_x,
                                src_y, dst_info, pixel_address, row_bytes);
  }
  if (!success) {
    return base::unexpected(GLError(GL_INVALID_OPERATION,
                                    "glReadbackImagePixels",
                                    "Failed to read pixels from SkImage"));
  }
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
      dest_scoped_access->ApplyBackendSurfaceEndState();
      shared_context_state_->SubmitIfNecessary(std::move(end_semaphores),
                                               need_graphite_submit);
      return base::unexpected(
          GLError(GL_INVALID_OPERATION, "glWritePixelsYUV",
                  "Failed to upload pixels to dest shared image"));
    }
  }

  shared_context_state_->FlushWriteAccess(dest_scoped_access.get());
  shared_context_state_->SubmitIfNecessary(std::move(end_semaphores),
                                           need_graphite_submit);

  if (!dest_shared_image->IsCleared()) {
    dest_shared_image->SetClearedRect(gfx::Rect(src_width, src_height));
  }
  return base::ok();
}

}  // namespace gpu
