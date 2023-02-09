// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/copy_shared_image_helper.h"

#include <memory>
#include <vector>

#include "base/check.h"
#include "base/strings/string_number_conversions.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image/shared_image_factory.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"
#include "gpu/command_buffer/service/skia_utils.h"
#include "gpu/config/gpu_finch_features.h"
#include "skia/ext/rgba_to_yuva.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkPromiseImageTexture.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/core/SkYUVAInfo.h"
#include "third_party/skia/include/gpu/GrBackendSemaphore.h"
#include "third_party/skia/include/gpu/GrBackendSurface.h"
#include "third_party/skia/include/gpu/GrDirectContext.h"
#include "third_party/skia/include/gpu/GrYUVABackendTextures.h"

namespace gpu {

using GLError = CopySharedImageHelper::GLError;

namespace {

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
    return base::unexpected<GLError>(
        GLError(GL_INVALID_ENUM, function_name,
                "yuv_color_space must be a valid SkYUVColorSpace"));
  }
  sk_yuv_color_space = static_cast<SkYUVColorSpace>(yuv_color_space_in);
  if (plane_config_in < 0 ||
      plane_config_in > static_cast<GLenum>(SkYUVAInfo::PlaneConfig::kLast)) {
    return base::unexpected<GLError>(
        GLError(GL_INVALID_ENUM, function_name,
                "plane_config must be a valid SkYUVAInfo::PlaneConfig"));
  }
  sk_plane_config = static_cast<SkYUVAInfo::PlaneConfig>(plane_config_in);
  if (subsampling_in < 0 ||
      subsampling_in > static_cast<GLenum>(SkYUVAInfo::Subsampling::kLast)) {
    return base::unexpected<GLError>(
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
      return base::unexpected<GLError>(
          GLError(GL_INVALID_OPERATION, function_name, msg));
    }
  }
  rgba_image =
      representation_factory->ProduceSkia(rgba_mailbox, shared_context_state);
  if (!rgba_image) {
    return base::unexpected<GLError>(
        GLError(GL_INVALID_OPERATION, "ConvertYUVAMailboxesToRGB",
                "Attempting to operate on unknown dest mailbox."));
  }
  return base::ok();
}

void FlushSurface(SkiaImageRepresentation::ScopedWriteAccess* access) {
  int num_planes = access->representation()->format().NumberOfPlanes();
  auto end_state = access->TakeEndState();
  for (int plane_index = 0; plane_index < num_planes; plane_index++) {
    auto* surface = access->surface(plane_index);
    DCHECK(surface);
    surface->flush({}, end_state.get());
  }
}

void SubmitIfNecessary(std::vector<GrBackendSemaphore> signal_semaphores,
                       SharedContextState* context,
                       bool is_drdc_enabled) {
  // Note that when DrDc is enabled, we need to call
  // AddVulkanCleanupTaskForSkiaFlush() on gpu main thread and do skia flush.
  // This will ensure that vulkan memory allocated on gpu main thread will be
  // cleaned up.
  if (!signal_semaphores.empty() || is_drdc_enabled) {
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
    context->gr_context()->submit(sync_cpu);
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
  auto result = ConvertYUVACommon(
      "ConvertYUVAMailboxesToRGB", yuv_color_space, plane_config, subsampling,
      mailboxes_in, representation_factory_, shared_context_state_,
      dst_color_space, dst_plane_config, dst_subsampling, rgba_image,
      num_yuva_planes, yuva_images);
  if (!result.has_value()) {
    return result;
  }

  std::vector<GrBackendSemaphore> begin_semaphores;
  std::vector<GrBackendSemaphore> end_semaphores;

  auto rgba_scoped_access =
      rgba_image->BeginScopedReadAccess(&begin_semaphores, &end_semaphores);
  if (!rgba_scoped_access) {
    DCHECK(begin_semaphores.empty());
    return base::unexpected<GLError>(
        GLError(GL_INVALID_OPERATION, "glConvertYUVAMailboxesToRGB",
                "RGBA shared image is not readable"));
  }
  auto rgba_sk_image =
      rgba_scoped_access->CreateSkImage(shared_context_state_->gr_context());
  if (!rgba_sk_image) {
    return base::unexpected<GLError>(
        GLError(GL_INVALID_OPERATION, "glReadbackImagePixels",
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
      return base::unexpected<GLError>(
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
  SubmitIfNecessary(std::move(end_semaphores), shared_context_state_,
                    is_drdc_enabled_);
  return base::ok();
}

base::expected<void, GLError> CopySharedImageHelper::ConvertYUVAMailboxesToRGB(
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
  auto result = ConvertYUVACommon(
      "ConvertYUVAMailboxesToRGB", planes_yuv_color_space, plane_config,
      subsampling, bytes_in, representation_factory_, shared_context_state_,
      src_yuv_color_space, src_plane_config, src_subsampling, rgba_image,
      num_src_planes, yuva_images);
  if (!result.has_value()) {
    return result;
  }

  sk_sp<SkColorSpace> src_rgb_color_space = ReadSkColorSpace(
      bytes_in + (SkYUVAInfo::kMaxPlanes + 1) * sizeof(gpu::Mailbox));

  std::vector<GrBackendSemaphore> begin_semaphores;
  std::vector<GrBackendSemaphore> end_semaphores;

  auto dest_scoped_access = rgba_image->BeginScopedWriteAccess(
      &begin_semaphores, &end_semaphores,
      SharedImageRepresentation::AllowUnclearedAccess::kYes);
  if (!dest_scoped_access) {
    DCHECK(begin_semaphores.empty());
    return base::unexpected<GLError>(
        GLError(GL_INVALID_VALUE, "glConvertYUVAMailboxesToRGB",
                "Destination shared image is not writable"));
  }

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
      result = base::unexpected<GLError>(
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
    std::array<GrBackendTexture, SkYUVAInfo::kMaxPlanes> yuva_textures;
    for (int i = 0; i < num_src_planes; ++i) {
      yuva_textures[i] =
          source_scoped_access[i]->promise_image_texture()->backendTexture();
    }

    // Disable color space conversion if no source color space was specified.
    if (!src_rgb_color_space) {
      if (auto dest_color_space = dest_surface->imageInfo().refColorSpace()) {
        src_rgb_color_space = std::move(dest_color_space);
      }
    }

    SkISize dest_size =
        SkISize::Make(dest_surface->width(), dest_surface->height());
    SkYUVAInfo yuva_info(dest_size, src_plane_config, src_subsampling,
                         src_yuv_color_space);
    GrYUVABackendTextures yuva_backend_textures(yuva_info, yuva_textures.data(),
                                                kTopLeft_GrSurfaceOrigin);
    auto result_image = SkImage::MakeFromYUVATextures(
        shared_context_state_->gr_context(), yuva_backend_textures,
        src_rgb_color_space);
    if (!result_image) {
      result = base::unexpected<GLError>(
          GLError(GL_INVALID_OPERATION, "glConvertYUVAMailboxesToRGB",
                  "Couldn't create destination images from provided sources"));
    } else {
      SkPaint paint;
      paint.setBlendMode(SkBlendMode::kSrc);
      dest_surface->getCanvas()->drawImage(result_image, 0, 0,
                                           SkSamplingOptions(), &paint);
      drew_image = true;
    }
  }

  FlushSurface(dest_scoped_access.get());
  SubmitIfNecessary(std::move(end_semaphores), shared_context_state_,
                    is_drdc_enabled_);

  if (!rgba_image->IsCleared() && drew_image) {
    rgba_image->SetCleared();
  }

  return result;
}

}  // namespace gpu
