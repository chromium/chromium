// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/skia_graphite_dawn_image_representation.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/dawn_context_provider.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image/shared_image_format_service_utils.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/gpu/graphite/Recorder.h"
#include "third_party/skia/include/gpu/graphite/Surface.h"
#include "ui/gfx/geometry/skia_conversions.h"

namespace gpu {

namespace {

bool SupportsMultiplanarRendering(SharedContextState* context_state) {
  auto* dawn_context_provider = context_state->dawn_context_provider();
  if (!dawn_context_provider) {
    return false;
  }
  return dawn_context_provider->SupportsFeature(
      wgpu::FeatureName::MultiPlanarRenderTargets);
}

bool SupportsMultiplanarCopy(SharedContextState* context_state) {
  auto* dawn_context_provider = context_state->dawn_context_provider();
  if (!dawn_context_provider) {
    return false;
  }
  return dawn_context_provider->SupportsFeature(
      wgpu::FeatureName::MultiPlanarFormatExtendedUsages);
}
}  // namespace

// static method.
std::unique_ptr<SkiaGraphiteDawnImageRepresentation>
SkiaGraphiteDawnImageRepresentation::Create(
    std::unique_ptr<DawnImageRepresentation> dawn_representation,
    scoped_refptr<SharedContextState> context_state,
    skgpu::graphite::Recorder* recorder,
    SharedImageManager* manager,
    SharedImageBacking* backing,
    MemoryTypeTracker* tracker,
    int array_slice) {
  CHECK(dawn_representation);
  const bool is_dcomp_surface =
      backing->usage().Has(SHARED_IMAGE_USAGE_SCANOUT_DCOMP_SURFACE);
  const bool supports_multiplanar_rendering =
      SupportsMultiplanarRendering(context_state.get());
  const bool supports_multiplanar_copy =
      SupportsMultiplanarCopy(context_state.get());
  wgpu::TextureUsage supported_tex_usages = SupportedDawnTextureUsage(
      backing->format(), backing->format().is_multi_plane(), is_dcomp_surface,
      supports_multiplanar_rendering, supports_multiplanar_copy);
  return base::WrapUnique(new SkiaGraphiteDawnImageRepresentation(
      std::move(dawn_representation), recorder, std::move(context_state),
      manager, backing, tracker, array_slice, supported_tex_usages));
}

SkiaGraphiteDawnImageRepresentation::SkiaGraphiteDawnImageRepresentation(
    std::unique_ptr<DawnImageRepresentation> dawn_representation,
    skgpu::graphite::Recorder* recorder,
    scoped_refptr<SharedContextState> context_state,
    SharedImageManager* manager,
    SharedImageBacking* backing,
    MemoryTypeTracker* tracker,
    int array_slice,
    wgpu::TextureUsage supported_tex_usages)
    : SkiaGraphiteImageRepresentation(manager, backing, tracker),
      dawn_representation_(std::move(dawn_representation)),
      context_state_(std::move(context_state)),
      recorder_(recorder),
      array_slice_(array_slice),
      supported_tex_usages_(supported_tex_usages) {
  CHECK(dawn_representation_);
}

SkiaGraphiteDawnImageRepresentation::~SkiaGraphiteDawnImageRepresentation() {
  CHECK_EQ(RepresentationAccessMode::kNone, mode_);
  CHECK_EQ(!has_context(), context_state_->context_lost());
  dawn_scoped_access_.reset();
  if (!has_context()) {
    dawn_representation_->OnContextLost();
  }
}

std::vector<skgpu::graphite::BackendTexture>
SkiaGraphiteDawnImageRepresentation::CreateBackendTextures(
    wgpu::Texture texture,
    bool readonly) {
  std::vector<skgpu::graphite::BackendTexture> backend_textures;
  const bool supports_multiplanar_rendering =
      SupportsMultiplanarRendering(context_state_.get());
  const bool supports_multiplanar_copy =
      SupportsMultiplanarCopy(context_state_.get());
  if (format().is_multi_plane()) {
    CHECK(format() == viz::MultiPlaneFormat::kP010 ||
          format() == viz::MultiPlaneFormat::kP210 ||
          format() == viz::MultiPlaneFormat::kP410 ||
          format() == viz::MultiPlaneFormat::kNV12 ||
          format() == viz::MultiPlaneFormat::kNV16 ||
          format() == viz::MultiPlaneFormat::kNV24 ||
          format() == viz::MultiPlaneFormat::kNV12A);
    backend_textures.reserve(format().NumberOfPlanes());
    for (int plane_index = 0; plane_index < format().NumberOfPlanes();
         plane_index++) {
      SkISize plane_size =
          gfx::SizeToSkISize(format().GetPlaneSize(plane_index, size()));
      skgpu::graphite::DawnTextureInfo plane_info = DawnBackendTextureInfo(
          format(), readonly, /*is_yuv_plane=*/true, plane_index, array_slice_,
          /*mipmapped=*/false,
          /*scanout_dcomp_surface=*/false, supports_multiplanar_rendering,
          supports_multiplanar_copy);
      backend_textures.emplace_back(skgpu::graphite::BackendTextures::MakeDawn(
          plane_size, plane_info, texture.Get()));
    }
  } else {
    backend_textures = {
        skgpu::graphite::BackendTextures::MakeDawn(texture.Get())};
  }

  return backend_textures;
}

std::vector<sk_sp<SkSurface>>
SkiaGraphiteDawnImageRepresentation::BeginWriteAccess(
    const SkSurfaceProps& surface_props,
    const gfx::Rect& update_rect) {
  CHECK_EQ(mode_, RepresentationAccessMode::kNone);
  CHECK(!dawn_scoped_access_);
  dawn_scoped_access_ = dawn_representation_->BeginScopedAccess(
      supported_tex_usages_, AllowUnclearedAccess::kYes, update_rect);
  if (!dawn_scoped_access_) {
    DLOG(ERROR) << "Could not create DawnImageRepresentation::ScopedAccess";
    return {};
  }

  std::vector<skgpu::graphite::BackendTexture> backend_textures =
      CreateBackendTextures(dawn_scoped_access_->texture(), /*readonly=*/false);

  std::vector<sk_sp<SkSurface>> surfaces;
  surfaces.reserve(format().NumberOfPlanes());
  for (int plane = 0; plane < format().NumberOfPlanes(); plane++) {
    SkColorType sk_color_type = viz::ToClosestSkColorType(
        /*gpu_compositing=*/true, format(), plane);
    // Gray is not a renderable single channel format, but alpha is.
    if (sk_color_type == kGray_8_SkColorType) {
      sk_color_type = kAlpha_8_SkColorType;
    }

    auto surface = SkSurfaces::WrapBackendTexture(
        recorder_, backend_textures[plane], sk_color_type,
        backing()->color_space().GetAsFullRangeRGB().ToSkColorSpace(),
        &surface_props, /*textureReleaseProc=*/nullptr,
        /*releaseContext=*/nullptr, WrappedTextureDebugLabel(plane));
    if (!surface) {
      DLOG(ERROR) << "Could not create SkSurface";
      dawn_scoped_access_.reset();
      return {};
    }
    surfaces.push_back(std::move(surface));
  }

  mode_ = RepresentationAccessMode::kWrite;
  return surfaces;
}

std::vector<skgpu::graphite::BackendTexture>
SkiaGraphiteDawnImageRepresentation::BeginWriteAccess() {
  CHECK_EQ(mode_, RepresentationAccessMode::kNone);
  CHECK(!dawn_scoped_access_);

  dawn_scoped_access_ = dawn_representation_->BeginScopedAccess(
      supported_tex_usages_, AllowUnclearedAccess::kYes);
  if (!dawn_scoped_access_) {
    DLOG(ERROR) << "Could not create DawnImageRepresentation::ScopedAccess";
    return {};
  }

  mode_ = RepresentationAccessMode::kWrite;
  return CreateBackendTextures(dawn_scoped_access_->texture(),
                               /*readonly=*/false);
}

void SkiaGraphiteDawnImageRepresentation::EndWriteAccess() {
  CHECK_EQ(mode_, RepresentationAccessMode::kWrite);
  dawn_scoped_access_.reset();
  mode_ = RepresentationAccessMode::kNone;
}

std::vector<skgpu::graphite::BackendTexture>
SkiaGraphiteDawnImageRepresentation::BeginReadAccess() {
  CHECK_EQ(mode_, RepresentationAccessMode::kNone);
  CHECK(!dawn_scoped_access_);
  constexpr wgpu::TextureUsage kReadOnlyTextureUsage =
      wgpu::TextureUsage::CopySrc | wgpu::TextureUsage::TextureBinding;
  dawn_scoped_access_ = dawn_representation_->BeginScopedAccess(
      supported_tex_usages_ & kReadOnlyTextureUsage, AllowUnclearedAccess::kNo);
  if (!dawn_scoped_access_) {
    DLOG(ERROR) << "Could not create DawnImageRepresentation::ScopedAccess";
    return {};
  }

  mode_ = RepresentationAccessMode::kRead;
  return CreateBackendTextures(dawn_scoped_access_->texture(),
                               /*readonly=*/true);
}

void SkiaGraphiteDawnImageRepresentation::EndReadAccess() {
  CHECK_EQ(mode_, RepresentationAccessMode::kRead);
  dawn_scoped_access_.reset();
  mode_ = RepresentationAccessMode::kNone;
}

bool SkiaGraphiteDawnImageRepresentation::
    SupportsMultipleConcurrentReadAccess() {
  return dawn_representation_->SupportsMultipleConcurrentReadAccess();
}

}  // namespace gpu
