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
wgpu::TextureView CreatePlaneView(const wgpu::Texture& texture,
                                  int plane_index) {
  CHECK_EQ(texture.GetFormat(), wgpu::TextureFormat::R8BG8Biplanar420Unorm);
  wgpu::TextureViewDescriptor view_desc;
  if (plane_index == 0) {
    view_desc.aspect = wgpu::TextureAspect::Plane0Only;
  } else {
    CHECK_EQ(plane_index, 1);
    view_desc.aspect = wgpu::TextureAspect::Plane1Only;
  }
  return texture.CreateView(&view_desc);
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
    int plane_index,
    bool is_yuv_plane) {
  return base::WrapUnique(new SkiaGraphiteDawnImageRepresentation(
      std::move(dawn_representation), recorder, std::move(context_state),
      manager, backing, tracker, plane_index, is_yuv_plane));
}

SkiaGraphiteDawnImageRepresentation::SkiaGraphiteDawnImageRepresentation(
    std::unique_ptr<DawnImageRepresentation> dawn_representation,
    skgpu::graphite::Recorder* recorder,
    scoped_refptr<SharedContextState> context_state,
    SharedImageManager* manager,
    SharedImageBacking* backing,
    MemoryTypeTracker* tracker,
    int plane_index,
    bool is_yuv_plane)
    : SkiaGraphiteImageRepresentation(manager, backing, tracker),
      dawn_representation_(std::move(dawn_representation)),
      context_state_(std::move(context_state)),
      recorder_(recorder),
      plane_index_(plane_index),
      is_yuv_plane_(is_yuv_plane) {
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
    wgpu::Texture texture) {
  std::vector<skgpu::graphite::BackendTexture> backend_textures;
  CHECK(plane_views_.empty());
  if (format() == viz::MultiPlaneFormat::kNV12) {
    backend_textures.reserve(format().NumberOfPlanes());
    plane_views_.reserve(format().NumberOfPlanes());
    for (int plane_index = 0; plane_index < format().NumberOfPlanes();
         plane_index++) {
      wgpu::TextureView plane_view = CreatePlaneView(texture, plane_index);
      SkISize plane_size =
          gfx::SizeToSkISize(format().GetPlaneSize(plane_index, size()));
      skgpu::graphite::DawnTextureInfo plane_info =
          GetGraphiteDawnTextureInfo(format(), plane_index);
      backend_textures.emplace_back(plane_size, plane_info, plane_view.Get());
      plane_views_.push_back(std::move(plane_view));
    }
  } else if (is_yuv_plane_) {
    // Legacy multi-planar NV12 - format() is either R8 or RG8.
    wgpu::TextureView plane_view = CreatePlaneView(texture, plane_index_);
    SkISize plane_size = gfx::SizeToSkISize(size());
    skgpu::graphite::DawnTextureInfo plane_info =
        GetGraphiteDawnTextureInfo(format(), /*plane_index=*/0, is_yuv_plane_);
    backend_textures = {skgpu::graphite::BackendTexture(plane_size, plane_info,
                                                        plane_view.Get())};
    plane_views_ = {std::move(plane_view)};
  } else {
    CHECK(format().is_single_plane() && !format().IsLegacyMultiplanar());
    backend_textures = {skgpu::graphite::BackendTexture(texture.Get())};
  }

  return backend_textures;
}

std::vector<sk_sp<SkSurface>>
SkiaGraphiteDawnImageRepresentation::BeginWriteAccess(
    const SkSurfaceProps& surface_props,
    const gfx::Rect& update_rect) {
  CHECK_EQ(mode_, RepresentationAccessMode::kNone);
  CHECK(!dawn_scoped_access_);
  bool is_dcomp_surface = usage() & SHARED_IMAGE_USAGE_SCANOUT_DCOMP_SURFACE;
  wgpu::TextureUsage wgpu_usage =
      GetSupportedDawnTextureUsage(format(), is_yuv_plane_, is_dcomp_surface);
  dawn_scoped_access_ = dawn_representation_->BeginScopedAccess(
      wgpu_usage, AllowUnclearedAccess::kYes, update_rect);
  if (!dawn_scoped_access_) {
    DLOG(ERROR) << "Could not create DawnImageRepresentation::ScopedAccess";
    return {};
  }

  std::vector<skgpu::graphite::BackendTexture> backend_textures =
      CreateBackendTextures(dawn_scoped_access_->texture());

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
        &surface_props);
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

  bool is_dcomp_surface = usage() & SHARED_IMAGE_USAGE_SCANOUT_DCOMP_SURFACE;
  dawn_scoped_access_ = dawn_representation_->BeginScopedAccess(
      GetSupportedDawnTextureUsage(format(), is_yuv_plane_, is_dcomp_surface),
      AllowUnclearedAccess::kYes);
  if (!dawn_scoped_access_) {
    DLOG(ERROR) << "Could not create DawnImageRepresentation::ScopedAccess";
    return {};
  }

  mode_ = RepresentationAccessMode::kWrite;
  return CreateBackendTextures(dawn_scoped_access_->texture());
}

void SkiaGraphiteDawnImageRepresentation::EndWriteAccess() {
  CHECK_EQ(mode_, RepresentationAccessMode::kWrite);
  plane_views_.clear();
  dawn_scoped_access_.reset();
  mode_ = RepresentationAccessMode::kNone;
}

std::vector<skgpu::graphite::BackendTexture>
SkiaGraphiteDawnImageRepresentation::BeginReadAccess() {
  CHECK_EQ(mode_, RepresentationAccessMode::kNone);
  CHECK(!dawn_scoped_access_);

  bool is_dcomp_surface = usage() & SHARED_IMAGE_USAGE_SCANOUT_DCOMP_SURFACE;
  dawn_scoped_access_ = dawn_representation_->BeginScopedAccess(
      GetSupportedDawnTextureUsage(format(), is_yuv_plane_, is_dcomp_surface),
      AllowUnclearedAccess::kNo);

  if (!dawn_scoped_access_) {
    DLOG(ERROR) << "Could not create DawnImageRepresentation::ScopedAccess";
    return {};
  }

  mode_ = RepresentationAccessMode::kRead;
  return CreateBackendTextures(dawn_scoped_access_->texture());
}

void SkiaGraphiteDawnImageRepresentation::EndReadAccess() {
  CHECK_EQ(mode_, RepresentationAccessMode::kRead);
  plane_views_.clear();
  dawn_scoped_access_.reset();
  mode_ = RepresentationAccessMode::kNone;
}

}  // namespace gpu
