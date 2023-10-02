// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/copy_shared_image_helper.h"

#include <memory>
#include <vector>

#include "base/check.h"
#include "base/strings/string_number_conversions.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image/shared_image_factory.h"
#include "gpu/command_buffer/service/shared_image/shared_image_format_service_utils.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"
#include "gpu/command_buffer/service/skia_utils.h"
#include "gpu/config/gpu_finch_features.h"
#include "skia/ext/rgba_to_yuva.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"
#include "third_party/libyuv/include/libyuv/planar_functions.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkRect.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/gpu/GrBackendSemaphore.h"
#include "third_party/skia/include/gpu/GrBackendSurface.h"
#include "third_party/skia/include/gpu/GrDirectContext.h"
#include "third_party/skia/include/gpu/GrTypes.h"
#include "third_party/skia/include/gpu/GrYUVABackendTextures.h"
#include "third_party/skia/include/gpu/ganesh/SkImageGanesh.h"
#include "third_party/skia/include/gpu/ganesh/SkSurfaceGanesh.h"
#include "third_party/skia/include/gpu/ganesh/gl/GrGLBackendSurface.h"
#include "third_party/skia/include/gpu/gl/GrGLTypes.h"
#include "third_party/skia/include/gpu/graphite/Context.h"
#include "third_party/skia/include/gpu/graphite/Image.h"
#include "third_party/skia/include/gpu/graphite/Recorder.h"
#include "third_party/skia/include/gpu/graphite/YUVABackendTextures.h"
#include "third_party/skia/include/private/chromium/GrPromiseImageTexture.h"

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
      NOTREACHED();
      return kUnknown_SkColorType;
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

// Return true if all of `sk_yuv_color_space`, `sk_plane_config`,
// `sk_subsampling`, `rgba_image, `num_yuva_images`, and `yuva_images` were
// successfully populated. Return false on error. If this returns false, some
// of the output arguments may be left populated.
base::expected<void, GLError> ConvertYUVACommon(
    const char* function_name,
    GLenum yuv_color_space_in,
    GLenum plane_config_in,
    GLenum subsampling_in,
    const volatile GLbyte* mailboxes_in,
    SharedImageRepresentationFactory* representation_factory,
    SharedContextState* shared_context_state,
    SkYUVColorSpace& sk_yuv_color_space,
    SkYUVAInfo::PlaneConfig& sk_plane_config,
    SkYUVAInfo::Subsampling& sk_subsampling,
    std::unique_ptr<SkiaImageRepresentation>& rgba_image,
    int& num_yuva_planes,
    std::array<std::unique_ptr<SkiaImageRepresentation>,
               SkYUVAInfo::kMaxPlanes>& yuva_images) {
  if (yuv_color_space_in < 0 ||
      yuv_color_space_in > kLastEnum_SkYUVColorSpace) {
    return base::unexpected(
        GLError(GL_INVALID_ENUM, function_name,
                "yuv_color_space must be a valid SkYUVColorSpace"));
  }
  sk_yuv_color_space = static_cast<SkYUVColorSpace>(yuv_color_space_in);
  if (plane_config_in < 0 ||
      plane_config_in > static_cast<GLenum>(SkYUVAInfo::PlaneConfig::kLast)) {
    return base::unexpected(
        GLError(GL_INVALID_ENUM, function_name,
                "plane_config must be a valid SkYUVAInfo::PlaneConfig"));
  }
  sk_plane_config = static_cast<SkYUVAInfo::PlaneConfig>(plane_config_in);
  if (subsampling_in < 0 ||
      subsampling_in > static_cast<GLenum>(SkYUVAInfo::Subsampling::kLast)) {
    return base::unexpected(
        GLError(GL_INVALID_ENUM, function_name,
                "subsampling must be a valid SkYUVAInfo::Subsampling"));
  }
  sk_subsampling = static_cast<SkYUVAInfo::Subsampling>(subsampling_in);

  std::array<gpu::Mailbox, SkYUVAInfo::kMaxPlanes> yuva_mailboxes;
  num_yuva_planes = SkYUVAInfo::NumPlanes(sk_plane_config);
  for (int i = 0; i < num_yuva_planes; ++i) {
    yuva_mailboxes[i] = Mailbox::FromVolatile(
        reinterpret_cast<const volatile Mailbox*>(mailboxes_in)[i]);
    DLOG_IF(ERROR, !yuva_mailboxes[i].Verify())
        << function_name
        << " was passed an invalid mailbox for YUVA plane: " << i
        << " with plane config " << plane_config_in;
  }
  gpu::Mailbox rgba_mailbox;
  rgba_mailbox =
      Mailbox::FromVolatile(reinterpret_cast<const volatile Mailbox*>(
          mailboxes_in)[SkYUVAInfo::kMaxPlanes]);
  DLOG_IF(ERROR, !rgba_mailbox.Verify())
      << function_name << " was passed an invalid mailbox for RGBA";

  for (int i = 0; i < num_yuva_planes; ++i) {
    yuva_images[i] = representation_factory->ProduceSkia(yuva_mailboxes[i],
                                                         shared_context_state);
    if (!yuva_images[i]) {
      std::string msg =
          "Attempting to operate on unknown mailbox for plane index " +
          base::NumberToString(i) + " using plane config " +
          base::NumberToString(plane_config_in) + ".";
      return base::unexpected(
          GLError(GL_INVALID_OPERATION, function_name, msg));
    }
  }
  rgba_image =
      representation_factory->ProduceSkia(rgba_mailbox, shared_context_state);
  if (!rgba_image) {
    return base::unexpected(
        GLError(GL_INVALID_OPERATION, "ConvertYUVAMailboxesToRGB",
                "Attempting to operate on unknown dest mailbox."));
  }
  return base::ok();
}

void InsertRecordingAndSubmit(SharedContextState* context,
                              bool sync_cpu = false) {
  CHECK(context->graphite_context());
  auto recording = context->gpu_main_graphite_recorder()->snap();
  if (recording) {
    skgpu::graphite::InsertRecordingInfo info = {};
    info.fRecording = recording.get();
    context->graphite_context()->insertRecording(info);
  }
  context->graphite_context()->submit(sync_cpu
                                          ? skgpu::graphite::SyncToCpu::kYes
                                          : skgpu::graphite::SyncToCpu::kNo);
}

void FlushSurface(SkiaImageRepresentation::ScopedWriteAccess* access) {
  int num_planes = access->representation()->format().NumberOfPlanes();
  for (int plane_index = 0; plane_index < num_planes; plane_index++) {
    auto* surface = access->surface(plane_index);
    DCHECK(surface);
    skgpu::ganesh::Flush(surface);
  }
  access->ApplyBackendSurfaceEndState();
}

void SubmitIfNecessary(std::vector<GrBackendSemaphore> signal_semaphores,
                       SharedContextState* context,
                       bool is_drdc_enabled) {
  // Note that when DrDc is enabled, we need to call
  // AddVulkanCleanupTaskForSkiaFlush() on gpu main thread and do skia flush.
  // This will ensure that vulkan memory allocated on gpu main thread will be
  // cleaned up.
  if (!signal_semaphores.empty() || is_drdc_enabled) {
    CHECK(context->gr_context());
    GrFlushInfo flush_info = {
        .fNumSemaphores = signal_semaphores.size(),
        .fSignalSemaphores = signal_semaphores.data(),
    };
    gpu::AddVulkanCleanupTaskForSkiaFlush(context->vk_context_provider(),
                                          &flush_info);

    auto result = context->gr_context()->flush(flush_info);
    DCHECK_EQ(result, GrSemaphoresSubmitted::kYes);
  }

  bool sync_cpu =
      gpu::ShouldVulkanSyncCpuForSkiaSubmit(context->vk_context_provider());

  // If DrDc is enabled, submit the gr_context() to ensure correct ordering
  // of vulkan commands between raster and display compositor.
  // TODO(vikassoni): This submit could be happening more often than
  // intended resulting in perf penalty. Explore ways to reduce it by
  // trying to issue submit only once per draw call for both gpu main and
  // drdc thread gr_context. Also add metric to see how often submits are
  // happening per frame.
  const bool need_submit =
      sync_cpu || !signal_semaphores.empty() || is_drdc_enabled;

  if (need_submit) {
    CHECK(context->gr_context());
    context->gr_context()->submit(sync_cpu ? GrSyncCpu::kYes : GrSyncCpu::kNo);
  }

  if (context->graphite_context()) {
    InsertRecordingAndSubmit(context, sync_cpu);
  }
}

sk_sp<SkColorSpace> ReadSkColorSpace(const volatile GLbyte* bytes) {
  size_t offset = 0;
  const volatile skcms_TransferFunction* transfer =
      reinterpret_cast<const volatile skcms_TransferFunction*>(bytes + offset);
  offset += sizeof(skcms_TransferFunction);
  const volatile skcms_Matrix3x3* primaries =
      reinterpret_cast<const volatile skcms_Matrix3x3*>(bytes + offset);
  return SkColorSpace::MakeRGB(
      const_cast<const skcms_TransferFunction&>(*transfer),
      const_cast<const skcms_Matrix3x3&>(*primaries));
}

bool TryCopySubTextureINTERNALMemory(
    GLint xoffset,
    GLint yoffset,
    GLint x,
    GLint y,
    GLsizei width,
    GLsizei height,
    gfx::Rect dest_cleared_rect,
    GLboolean unpack_flip_y,
    const Mailbox& source_mailbox,
    SkiaImageRepresentation* dest_shared_image,
    SkiaImageRepresentation::ScopedWriteAccess* dest_scoped_access,
    SharedImageRepresentationFactory* representation_factory,
    SharedContextState* shared_context_state,
    bool is_drdc_enabled,
    const std::vector<GrBackendSemaphore>& begin_semaphores,
    std::vector<GrBackendSemaphore>& end_semaphores) {
  if (unpack_flip_y) {
    return false;
  }

  auto source_shared_image =
      representation_factory->ProduceMemory(source_mailbox);
  if (!source_shared_image) {
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

  FlushSurface(dest_scoped_access);
  SubmitIfNecessary(std::move(end_semaphores), shared_context_state,
                    is_drdc_enabled);

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
      shared_context_state_(shared_context_state),
      is_drdc_enabled_(
          features::IsDrDcEnabled() &&
          !shared_context_state->feature_info()->workarounds().disable_drdc) {}

CopySharedImageHelper::~CopySharedImageHelper() = default;

CopySharedImageHelper::GLError::GLError(GLenum gl_error,
                                        std::string function_name,
                                        std::string msg)
    : gl_error(gl_error),
      function_name(std::move(function_name)),
      msg(std::move(msg)) {}

base::expected<void, GLError> CopySharedImageHelper::ConvertRGBAToYUVAMailboxes(
    GLenum yuv_color_space,
    GLenum plane_config,
    GLenum subsampling,
    const volatile GLbyte* mailboxes_in) {
  SkYUVColorSpace dst_color_space;
  SkYUVAInfo::PlaneConfig dst_plane_config;
  SkYUVAInfo::Subsampling dst_subsampling;
  std::unique_ptr<SkiaImageRepresentation> rgba_image;
  int num_yuva_planes;
  std::array<std::unique_ptr<SkiaImageRepresentation>, SkYUVAInfo::kMaxPlanes>
      yuva_images;
  RETURN_IF_ERROR(ConvertYUVACommon(
      "ConvertYUVAMailboxesToRGB", yuv_color_space, plane_config, subsampling,
      mailboxes_in, representation_factory_, shared_context_state_,
      dst_color_space, dst_plane_config, dst_subsampling, rgba_image,
      num_yuva_planes, yuva_images));

  std::vector<GrBackendSemaphore> begin_semaphores;
  std::vector<GrBackendSemaphore> end_semaphores;

  auto rgba_scoped_access =
      rgba_image->BeginScopedReadAccess(&begin_semaphores, &end_semaphores);
  if (!rgba_scoped_access) {
    DCHECK(begin_semaphores.empty());
    return base::unexpected(GLError(GL_INVALID_OPERATION,
                                    "glConvertYUVAMailboxesToRGB",
                                    "RGBA shared image is not readable"));
  }
  // Perform ApplyBackendSurfaceEndState() on the ScopedReadAccess before
  // exiting.
  absl::Cleanup cleanup = [&]() {
    rgba_scoped_access->ApplyBackendSurfaceEndState();
    SubmitIfNecessary(std::move(end_semaphores), shared_context_state_,
                      is_drdc_enabled_);
  };

  auto rgba_sk_image = rgba_scoped_access->CreateSkImage(shared_context_state_);
  if (!rgba_sk_image) {
    return base::unexpected(GLError(GL_INVALID_OPERATION,
                                    "glReadbackImagePixels",
                                    "Couldn't create SkImage for reading."));
  }

  std::array<std::unique_ptr<SkiaImageRepresentation::ScopedWriteAccess>,
             SkYUVAInfo::kMaxPlanes>
      yuva_scoped_access;
  for (int i = 0; i < num_yuva_planes; ++i) {
    yuva_scoped_access[i] = yuva_images[i]->BeginScopedWriteAccess(
        &begin_semaphores, &end_semaphores,
        SharedImageRepresentation::AllowUnclearedAccess::kYes);
    if (!yuva_scoped_access[i]) {
      std::string msg =
          "Couldn't write shared image for mailbox of plane index " +
          base::NumberToString(i) + " using plane config " +
          base::NumberToString(plane_config) + ".";
      return base::unexpected(
          GLError(GL_INVALID_OPERATION, "glConvertRGBAToYUVAMailboxes", msg));
    }
  }
  SkSurface* yuva_sk_surfaces[SkYUVAInfo::kMaxPlanes];
  for (int i = 0; i < num_yuva_planes; ++i) {
    yuva_sk_surfaces[i] = yuva_scoped_access[i]->surface();
    if (!begin_semaphores.empty()) {
      bool ret = yuva_sk_surfaces[i]->wait(begin_semaphores.size(),
                                           begin_semaphores.data(),
                                           /*deleteSemaphoresAfterWait=*/false);
      DCHECK(ret);
    }
  }

  SkYUVAInfo yuva_info(rgba_sk_image->dimensions(), dst_plane_config,
                       dst_subsampling, dst_color_space);
  skia::BlitRGBAToYUVA(rgba_sk_image.get(), yuva_sk_surfaces, yuva_info);

  for (int i = 0; i < num_yuva_planes; ++i) {
    FlushSurface(yuva_scoped_access[i].get());
    if (!yuva_images[i]->IsCleared()) {
      yuva_images[i]->SetCleared();
    }
  }
  return base::ok();
}

base::expected<void, GLError> CopySharedImageHelper::ConvertYUVAMailboxesToRGB(
    GLint src_x,
    GLint src_y,
    GLsizei width,
    GLsizei height,
    GLenum planes_yuv_color_space,
    GLenum plane_config,
    GLenum subsampling,
    const volatile GLbyte* bytes_in) {
  SkYUVColorSpace src_yuv_color_space;
  SkYUVAInfo::PlaneConfig src_plane_config;
  SkYUVAInfo::Subsampling src_subsampling;
  std::unique_ptr<SkiaImageRepresentation> rgba_image;
  int num_src_planes;
  std::array<std::unique_ptr<SkiaImageRepresentation>, SkYUVAInfo::kMaxPlanes>
      yuva_images;
  RETURN_IF_ERROR(ConvertYUVACommon(
      "ConvertYUVAMailboxesToRGB", planes_yuv_color_space, plane_config,
      subsampling, bytes_in, representation_factory_, shared_context_state_,
      src_yuv_color_space, src_plane_config, src_subsampling, rgba_image,
      num_src_planes, yuva_images));

  sk_sp<SkColorSpace> src_rgb_color_space = ReadSkColorSpace(
      bytes_in + (SkYUVAInfo::kMaxPlanes + 1) * sizeof(gpu::Mailbox));

  std::vector<GrBackendSemaphore> begin_semaphores;
  std::vector<GrBackendSemaphore> end_semaphores;

  auto dest_scoped_access = rgba_image->BeginScopedWriteAccess(
      &begin_semaphores, &end_semaphores,
      SharedImageRepresentation::AllowUnclearedAccess::kYes);
  if (!dest_scoped_access) {
    DCHECK(begin_semaphores.empty());
    return base::unexpected(
        GLError(GL_INVALID_VALUE, "glConvertYUVAMailboxesToRGB",
                "Destination shared image is not writable"));
  }

  base::expected<void, GLError> result;
  bool source_access_valid = true;
  std::array<std::unique_ptr<SkiaImageRepresentation::ScopedReadAccess>,
             SkYUVAInfo::kMaxPlanes>
      source_scoped_access;
  for (int i = 0; i < num_src_planes; ++i) {
    source_scoped_access[i] = yuva_images[i]->BeginScopedReadAccess(
        &begin_semaphores, &end_semaphores);

    if (!source_scoped_access[i]) {
      source_access_valid = false;
      std::string msg =
          "Couldn't access shared image for mailbox of plane index " +
          base::NumberToString(i) + " using plane config " +
          base::NumberToString(plane_config) + ".";
      result = base::unexpected(
          GLError(GL_INVALID_OPERATION, "glConvertYUVAMailboxesToRGB", msg));
      break;
    }
  }

  auto* dest_surface = dest_scoped_access->surface();
  if (!begin_semaphores.empty()) {
    bool ret =
        dest_surface->wait(begin_semaphores.size(), begin_semaphores.data(),
                           /*deleteSemaphoresAfterWait=*/false);
    DCHECK(ret);
  }

  bool drew_image = false;
  if (source_access_valid) {
    // Disable color space conversion if no source color space was specified.
    if (!src_rgb_color_space) {
      if (auto dest_color_space = dest_surface->imageInfo().refColorSpace()) {
        src_rgb_color_space = std::move(dest_color_space);
      }
    }

    sk_sp<SkImage> result_image;

    gfx::Size dest_size =
        gfx::Size(dest_surface->width(), dest_surface->height());

    gfx::Rect dest_rect(0, 0, width, height);
    if (!gfx::Rect(dest_size).Contains(dest_rect)) {
      return base::unexpected(GLError(GL_INVALID_VALUE,
                                      "ConvertYUVAMailboxesToRGB",
                                      "destination texture bad dimensions."));
    }

    auto src_size = yuva_images[0]->size();
    SkYUVAInfo yuva_info(gfx::SizeToSkISize(src_size), src_plane_config,
                         src_subsampling, src_yuv_color_space);
    if (auto* gr_context = shared_context_state_->gr_context()) {
      std::array<GrBackendTexture, SkYUVAInfo::kMaxPlanes> yuva_textures;
      for (int i = 0; i < num_src_planes; ++i) {
        yuva_textures[i] =
            source_scoped_access[i]->promise_image_texture()->backendTexture();
      }
      GrYUVABackendTextures yuva_backend_textures(
          yuva_info, yuva_textures.data(), kTopLeft_GrSurfaceOrigin);
      result_image = SkImages::TextureFromYUVATextures(
          gr_context, yuva_backend_textures, src_rgb_color_space);
    } else {
      CHECK(shared_context_state_->graphite_context());
      auto* recorder = shared_context_state_->gpu_main_graphite_recorder();
      std::array<skgpu::graphite::BackendTexture, SkYUVAInfo::kMaxPlanes>
          yuva_textures;
      for (int i = 0; i < num_src_planes; ++i) {
        yuva_textures[i] = source_scoped_access[i]->graphite_texture();
      }
      skgpu::graphite::YUVABackendTextures yuva_backend_textures(
          recorder, yuva_info, yuva_textures);
      result_image = SkImages::TextureFromYUVATextures(
          recorder, yuva_backend_textures, src_rgb_color_space);
    }

    if (!result_image) {
      result = base::unexpected(
          GLError(GL_INVALID_OPERATION, "glConvertYUVAMailboxesToRGB",
                  "Couldn't create destination images from provided sources"));
    } else {
      SkPaint paint;
      paint.setBlendMode(SkBlendMode::kSrc);
      SkRect src_rect = SkRect::MakeXYWH(src_x, src_y, width, height);
      dest_surface->getCanvas()->drawImageRect(
          result_image, src_rect, gfx::RectToSkRect(dest_rect),
          SkSamplingOptions(), &paint, SkCanvas::kStrict_SrcRectConstraint);
      drew_image = true;
    }
  }

  FlushSurface(dest_scoped_access.get());
  for (int i = 0; i < num_src_planes; ++i) {
    if (source_scoped_access[i]) {
      source_scoped_access[i]->ApplyBackendSurfaceEndState();
    }
  }
  SubmitIfNecessary(std::move(end_semaphores), shared_context_state_,
                    is_drdc_enabled_);

  if (!rgba_image->IsCleared() && drew_image) {
    rgba_image->SetCleared();
  }

  return result;
}

base::expected<void, GLError> CopySharedImageHelper::CopySharedImage(
    GLint xoffset,
    GLint yoffset,
    GLint x,
    GLint y,
    GLsizei width,
    GLsizei height,
    GLboolean unpack_flip_y,
    const volatile GLbyte* mailboxes) {
  Mailbox source_mailbox = Mailbox::FromVolatile(
      reinterpret_cast<const volatile Mailbox*>(mailboxes)[0]);
  DLOG_IF(ERROR, !source_mailbox.Verify())
      << "CopySubTexture was passed an invalid mailbox";
  Mailbox dest_mailbox = Mailbox::FromVolatile(
      reinterpret_cast<const volatile Mailbox*>(mailboxes)[1]);
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
  if (dest_format.IsLegacyMultiplanar() ||
      dest_format.PrefersExternalSampler()) {
    return base::unexpected(
        GLError(GL_INVALID_VALUE, "glCopySubTexture", "unexpected format"));
  }

  gfx::Size dest_size = dest_shared_image->size();
  gfx::Rect dest_rect(xoffset, yoffset, width, height);
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

  // Flush dest surface and submit if necessary before exiting.
  absl::Cleanup cleanup = [&]() {
    FlushSurface(dest_scoped_access.get());
    SubmitIfNecessary(std::move(end_semaphores), shared_context_state_,
                      is_drdc_enabled_);
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
  if (TryCopySubTextureINTERNALMemory(
          xoffset, yoffset, x, y, width, height, new_cleared_rect,
          unpack_flip_y, source_mailbox, dest_shared_image.get(),
          dest_scoped_access.get(), representation_factory_,
          shared_context_state_, is_drdc_enabled_, begin_semaphores,
          end_semaphores)) {
    // Cancel cleanup as TryCopySubTextureINTERNALMemory already handles it.
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
    auto* canvas = dest_scoped_access->surface()->getCanvas();

    SkAutoCanvasRestore autoRestore(canvas, /*doSave=*/true);
    canvas->clipRect(gfx::RectToSkRect(dest_rect));
    canvas->clear(SkColors::kBlack);

    if (!dest_shared_image->IsCleared()) {
      dest_shared_image->SetClearedRect(new_cleared_rect);
    }

    // Note, that we still generate error for the client to indicate there was
    // problem.
    return base::unexpected(GLError(GL_INVALID_VALUE, "glCopySubTexture",
                                    "unknown source image mailbox."));
  }

  gfx::Size source_size = source_shared_image->size();
  gfx::Rect source_rect(x, y, width, height);
  if (!gfx::Rect(source_size).Contains(source_rect)) {
    return base::unexpected(GLError(GL_INVALID_VALUE, "glCopySubTexture",
                                    "source texture bad dimensions."));
  }

  std::unique_ptr<SkiaImageRepresentation::ScopedReadAccess>
      source_scoped_access = source_shared_image->BeginScopedReadAccess(
          &begin_semaphores, &end_semaphores);
  if (!begin_semaphores.empty()) {
    bool ret = dest_scoped_access->surface()->wait(
        begin_semaphores.size(), begin_semaphores.data(),
        /*deleteSemaphoresAfterWait=*/false);
    DCHECK(ret);
  }
  if (!source_scoped_access) {
    return base::unexpected(GLError(GL_INVALID_VALUE, "glCopySubTexture",
                                    "Source shared image is not accessable"));
  }

  base::expected<void, GLError> result = base::ok();
  auto source_image =
      source_scoped_access->CreateSkImage(shared_context_state_);
  if (!source_image) {
    result = base::unexpected(
        GLError(GL_INVALID_VALUE, "glCopySubTexture",
                "Couldn't create SkImage from source shared image."));
  } else {
    // Skia will flip the image if the surface origins do not match.
    DCHECK_EQ(unpack_flip_y, source_shared_image->surface_origin() !=
                                 dest_shared_image->surface_origin());
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
      // TODO(crbug.com/1450879): Make this path work for Graphite after Dawn
      // supports multiplanar rendering and we integrate it into Chrome.
      CHECK(shared_context_state_->gr_context());
      SkSurface* yuva_sk_surfaces[SkYUVAInfo::kMaxPlanes] = {};
      for (int plane_index = 0; plane_index < dest_format.NumberOfPlanes();
           plane_index++) {
        // Get surface per plane from destination scoped write access.
        yuva_sk_surfaces[plane_index] =
            dest_scoped_access->surface(plane_index);
      }

      // TODO(crbug.com/828599): This should really default to rec709.
      SkYUVColorSpace yuv_color_space = kRec601_SkYUVColorSpace;
      dest_shared_image->color_space().ToSkYUVColorSpace(
          dest_format.MultiplanarBitDepth(), &yuv_color_space);

      SkYUVAInfo yuva_info(gfx::SizeToSkISize(dest_shared_image->size()),
                           ToSkYUVAPlaneConfig(dest_format),
                           ToSkYUVASubsampling(dest_format), yuv_color_space);
      // Perform skia::BlitRGBAToYUVA for the multiplanar YUV format image.
      // TODO(crbug.com/1451025): This will scale the image if the source image
      // is smaller than the destination image. What we should actually do
      // instead is just blit the destination rect and clear out the rest.
      // However, doing that resulted in resulted in pixeltest failures due to
      // images having pixel bleeding at their borders when this codepath is
      // used by RenderableGMBVideoFramePool (see the bug for details). The
      // current behavior of scaling the image matches the legacy
      // (non-multiplanar SI) behavior in RenderableGMBVideoFramePool, so it is
      // not a regression. Nonetheless, this behavior should
      // ideally be changed to that described above for correctness.
      skia::BlitRGBAToYUVA(source_image.get(), yuva_sk_surfaces, yuva_info);
      dest_shared_image->SetCleared();
    }

    if (!dest_shared_image->IsCleared()) {
      dest_shared_image->SetClearedRect(new_cleared_rect);
    }
  }

  // Cancel cleanup as the cleanup order is different here.
  std::move(cleanup).Cancel();
  FlushSurface(dest_scoped_access.get());
  source_scoped_access->ApplyBackendSurfaceEndState();
  SubmitIfNecessary(std::move(end_semaphores), shared_context_state_,
                    is_drdc_enabled_);
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
    GLboolean flip_y,
    const volatile GLbyte* src_mailbox) {
  CHECK(shared_context_state_->gr_context());
  Mailbox source_mailbox = Mailbox::FromVolatile(
      reinterpret_cast<const volatile Mailbox*>(src_mailbox)[0]);
  DLOG_IF(ERROR, !source_mailbox.Verify())
      << "CopySharedImageToGLTexture was passed an invalid mailbox";

  CHECK_NE(dest_texture_id, 0u);
  CHECK(shared_context_state_->GrContextIsGL());
  GrGLTextureInfo texture_info;
  texture_info.fID = dest_texture_id;
  texture_info.fTarget = target;
  // Get the surface color format similar to that in VideoFrameYUVConverter.
  texture_info.fFormat = GetSurfaceColorFormat(internal_format, type);
  auto backend_texture = GrBackendTextures::MakeGL(
      width, height, skgpu::Mipmapped::kNo, texture_info);

  auto dest_color_space = SkColorSpace::MakeSRGB();
  GrDirectContext* direct_context = shared_context_state_->gr_context();
  CHECK(direct_context);
  sk_sp<SkSurface> dest_surface = SkSurfaces::WrapBackendTexture(
      direct_context, backend_texture,
      flip_y ? GrSurfaceOrigin::kBottomLeft_GrSurfaceOrigin
             : GrSurfaceOrigin::kTopLeft_GrSurfaceOrigin,
      /*sampleCnt=*/1, GetCompatibleSurfaceColorType(texture_info.fFormat),
      dest_color_space, nullptr);
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
    SubmitIfNecessary({}, shared_context_state_, is_drdc_enabled_);

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
    SubmitIfNecessary(std::move(end_semaphores), shared_context_state_,
                      is_drdc_enabled_);

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
  SubmitIfNecessary(std::move(end_semaphores), shared_context_state_,
                    is_drdc_enabled_);
  return result;
}

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
    SubmitIfNecessary(std::move(end_semaphores), shared_context_state_,
                      is_drdc_enabled_);
    return base::unexpected(GLError(GL_INVALID_OPERATION,
                                    "glReadbackImagePixels",
                                    "Couldn't create SkImage for reading."));
  }

  bool success = false;
  if (gr_context) {
    success = sk_image->readPixels(gr_context, dst_info, pixel_address,
                                   row_bytes, src_x, src_y);
  } else {
    CHECK(shared_context_state_->graphite_context());
    ReadPixelsContext context;
    const SkIRect src_rect =
        SkIRect::MakeXYWH(src_x, src_y, dst_info.width(), dst_info.height());
    shared_context_state_->graphite_context()->asyncRescaleAndReadPixels(
        sk_image.get(), dst_info, src_rect, SkImage::RescaleGamma::kSrc,
        SkImage::RescaleMode::kRepeatedLinear, &OnReadPixelsDone, &context);
    InsertRecordingAndSubmit(shared_context_state_, /*sync_cpu=*/true);
    CHECK(context.finished);
    if (context.async_result) {
      success = true;
      // Use CopyPlane to flip as Graphite doesn't support bottom left origin
      // images. Using a negative height causes CopyPlane to flip while copying.
      // TODO(crbug.com/1449764): Remove this if Graphite performs the flip once
      // it supports bottom left origin images.
      const int height =
          source_shared_image->surface_origin() == kTopLeft_GrSurfaceOrigin
              ? dst_info.height()
              : -dst_info.height();
      libyuv::CopyPlane(
          static_cast<const uint8_t*>(context.async_result->data(0)),
          context.async_result->rowBytes(0),
          static_cast<uint8_t*>(pixel_address), row_bytes,
          dst_info.width() * dst_info.bytesPerPixel(), height);
    } else {
      success = false;
    }
  }

  source_scoped_access->ApplyBackendSurfaceEndState();
  SubmitIfNecessary(std::move(end_semaphores), shared_context_state_,
                    is_drdc_enabled_);
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
  // Order of destruction for moved unique pointers is not guaranteed and the
  // ScopedWriteAccess must be destroyed before representation; so perform a
  // Cleanup before exiting.
  absl::Cleanup cleanup = [&]() { dest_scoped_access.reset(); };
  viz::SharedImageFormat dest_format = dest_shared_image->format();
  auto* gr_context = shared_context_state_->gr_context();
  for (int plane = 0; plane < dest_format.NumberOfPlanes(); plane++) {
    bool written = false;
    if (gr_context) {
      written = gr_context->updateBackendTexture(
          dest_scoped_access->promise_image_texture(plane)->backendTexture(),
          &pixmaps[plane],
          /*numLevels=*/1, dest_shared_image->surface_origin(), nullptr,
          nullptr);
    } else {
      CHECK(shared_context_state_->graphite_context());
      written =
          shared_context_state_->gpu_main_graphite_recorder()
              ->updateBackendTexture(
                  dest_scoped_access->graphite_texture(plane), &pixmaps[plane],
                  /*numLevels=*/1);
    }
    if (!written) {
      return base::unexpected(
          GLError(GL_INVALID_OPERATION, "glWritePixelsYUV",
                  "Failed to upload pixels to dest shared image"));
    }
  }

  if (gr_context) {
    dest_scoped_access->ApplyBackendSurfaceEndState();
  }
  SubmitIfNecessary(std::move(end_semaphores), shared_context_state_,
                    is_drdc_enabled_);

  if (!dest_shared_image->IsCleared()) {
    dest_shared_image->SetClearedRect(gfx::Rect(src_width, src_height));
  }
  return base::ok();
}

}  // namespace gpu
