// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/skia_graphite_dawn_image_representation.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "components/viz/common/gpu/dawn_context_provider.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/gpu/graphite/Recorder.h"
#include "third_party/skia/include/gpu/graphite/Surface.h"

#include <webgpu/webgpu.h>

namespace {
// This should match the texture usage set by GetGraphiteTextureInfo() - Dawn
// will validate this on dcheck builds.
constexpr WGPUTextureUsage kDefaultTextureUsage = static_cast<WGPUTextureUsage>(
    WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_TextureBinding |
    WGPUTextureUsage_CopySrc | WGPUTextureUsage_CopyDst);
}

namespace gpu {

// static method.
std::unique_ptr<SkiaGraphiteDawnImageRepresentation>
SkiaGraphiteDawnImageRepresentation::Create(
    std::unique_ptr<DawnImageRepresentation> dawn_representation,
    scoped_refptr<SharedContextState> context_state,
    skgpu::graphite::Recorder* recorder,
    SharedImageManager* manager,
    SharedImageBacking* backing,
    MemoryTypeTracker* tracker) {
  return base::WrapUnique(new SkiaGraphiteDawnImageRepresentation(
      std::move(dawn_representation), recorder, std::move(context_state),
      manager, backing, tracker));
}

SkiaGraphiteDawnImageRepresentation::SkiaGraphiteDawnImageRepresentation(
    std::unique_ptr<DawnImageRepresentation> dawn_representation,
    skgpu::graphite::Recorder* recorder,
    scoped_refptr<SharedContextState> context_state,
    SharedImageManager* manager,
    SharedImageBacking* backing,
    MemoryTypeTracker* tracker)
    : SkiaGraphiteImageRepresentation(manager, backing, tracker),
      dawn_representation_(std::move(dawn_representation)),
      context_state_(std::move(context_state)),
      recorder_(recorder) {
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

std::vector<sk_sp<SkSurface>>
SkiaGraphiteDawnImageRepresentation::BeginWriteAccess(
    const SkSurfaceProps& surface_props,
    const gfx::Rect& update_rect) {
  CHECK_EQ(mode_, RepresentationAccessMode::kNone);
  CHECK(!dawn_scoped_access_);
  dawn_scoped_access_ = dawn_representation_->BeginScopedAccess(
      kDefaultTextureUsage, AllowUnclearedAccess::kYes);
  if (!dawn_scoped_access_) {
    DLOG(ERROR) << "Could not create DawnImageRepresentation::ScopedAccess";
    return {};
  }

  // TODO(crbug.com/1430206): Add multiplanar format support.
  if (!format().is_single_plane()) {
    DLOG(ERROR) << "BeginWriteAccess called for unsupported format = "
                << format().ToString();
    return {};
  }

  viz::SharedImageFormat actual_format = format();
#if BUILDFLAG(IS_MAC)
  // IOSurfaces are allocated as BGRA_8888 if the requested format is RGBA_8888,
  // so adjust the format to create the correct color type.
  // TODO(crbug.com/1423576): Rationalize RGBA vs BGRA logic for IOSurfaces.
  if (actual_format == viz::SinglePlaneFormat::kRGBA_8888) {
    actual_format = viz::SinglePlaneFormat::kBGRA_8888;
  }
#endif

  SkColorType sk_color_type = viz::ToClosestSkColorType(
      /*gpu_compositing=*/true, actual_format);
  // Gray is not a renderable single channel format, but alpha is.
  if (sk_color_type == kGray_8_SkColorType) {
    sk_color_type = kAlpha_8_SkColorType;
  }

  auto surface = SkSurfaces::WrapBackendTexture(
      recorder_,
      skgpu::graphite::BackendTexture(dawn_scoped_access_->texture()),
      sk_color_type,
      backing()->color_space().GetAsFullRangeRGB().ToSkColorSpace(),
      &surface_props);
  if (!surface) {
    DLOG(ERROR) << "Could not create SkSurface";
    dawn_scoped_access_.reset();
    return {};
  }
  mode_ = RepresentationAccessMode::kWrite;
  return {std::move(surface)};
}

std::vector<skgpu::graphite::BackendTexture>
SkiaGraphiteDawnImageRepresentation::BeginWriteAccess() {
  CHECK_EQ(mode_, RepresentationAccessMode::kNone);
  CHECK(!dawn_scoped_access_);
  // TODO(crbug.com/1430206): Add multiplanar format support.
  if (!format().is_single_plane()) {
    DLOG(ERROR) << "BeginWriteAccess called for unsupported format = "
                << format().ToString();
    return {};
  }

  dawn_scoped_access_ = dawn_representation_->BeginScopedAccess(
      kDefaultTextureUsage, AllowUnclearedAccess::kYes);
  if (!dawn_scoped_access_) {
    DLOG(ERROR) << "Could not create DawnImageRepresentation::ScopedAccess";
    return {};
  }
  mode_ = RepresentationAccessMode::kWrite;
  return {skgpu::graphite::BackendTexture(dawn_scoped_access_->texture())};
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
  // TODO(crbug.com/1430206): Add multiplanar format support.
  if (!format().is_single_plane()) {
    DLOG(ERROR) << "BeginReadAccess called for unsupported format = "
                << format().ToString();
    return {};
  }

  dawn_scoped_access_ = dawn_representation_->BeginScopedAccess(
      kDefaultTextureUsage, AllowUnclearedAccess::kNo);
  if (!dawn_scoped_access_) {
    DLOG(ERROR) << "Could not create DawnImageRepresentation::ScopedAccess";
    return {};
  }
  mode_ = RepresentationAccessMode::kRead;
  return {skgpu::graphite::BackendTexture(dawn_scoped_access_->texture())};
}

void SkiaGraphiteDawnImageRepresentation::EndReadAccess() {
  CHECK_EQ(mode_, RepresentationAccessMode::kRead);
  dawn_scoped_access_.reset();
  mode_ = RepresentationAccessMode::kNone;
}

}  // namespace gpu
