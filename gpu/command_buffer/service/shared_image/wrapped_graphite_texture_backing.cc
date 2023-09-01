// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/wrapped_graphite_texture_backing.h"

#include <utility>

#include "base/logging.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/dawn_context_provider.h"
#include "gpu/command_buffer/service/shared_image/dawn_fallback_image_representation.h"
#include "gpu/command_buffer/service/shared_image/gl_texture_passthrough_fallback_image_representation.h"
#include "gpu/command_buffer/service/shared_image/shared_image_format_service_utils.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"
#include "third_party/libyuv/include/libyuv/planar_functions.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/core/SkSurfaceProps.h"
#include "third_party/skia/include/gpu/graphite/BackendTexture.h"
#include "third_party/skia/include/gpu/graphite/Context.h"
#include "third_party/skia/include/gpu/graphite/Image.h"
#include "third_party/skia/include/gpu/graphite/Recorder.h"
#include "third_party/skia/include/gpu/graphite/Recording.h"
#include "third_party/skia/include/gpu/graphite/Surface.h"
#include "third_party/skia/include/gpu/graphite/TextureInfo.h"
#include "ui/gfx/geometry/skia_conversions.h"

namespace gpu {
namespace {
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

class WrappedGraphiteTextureBacking::SkiaGraphiteImageRepresentationImpl
    : public SkiaGraphiteImageRepresentation {
 public:
  SkiaGraphiteImageRepresentationImpl(
      SharedImageManager* manager,
      SharedImageBacking* backing,
      MemoryTypeTracker* tracker,
      scoped_refptr<SharedContextState> context_state)
      : SkiaGraphiteImageRepresentation(manager, backing, tracker),
        context_state_(std::move(context_state)) {}

  ~SkiaGraphiteImageRepresentationImpl() override {
    CHECK(write_surfaces_.empty());
  }

  std::vector<sk_sp<SkSurface>> BeginWriteAccess(
      const SkSurfaceProps& surface_props,
      const gfx::Rect& update_rect) override {
    const auto& graphite_textures =
        backing_impl()->GetGraphiteBackendTextures();
    CHECK(write_surfaces_.empty());
    write_surfaces_.reserve(graphite_textures.size());
    for (int plane = 0; plane < format().NumberOfPlanes(); ++plane) {
      auto surface = SkSurfaces::WrapBackendTexture(
          context_state_->gpu_main_graphite_recorder(),
          graphite_textures[plane], backing_impl()->GetSkColorType(plane),
          color_space().ToSkColorSpace(), &surface_props);
      if (!surface) {
        LOG(ERROR) << "MakeGraphiteFromBackendTexture() failed.";
        write_surfaces_.clear();
        return {};
      }
      write_surfaces_.push_back(std::move(surface));
    }
    return write_surfaces_;
  }

  std::vector<skgpu::graphite::BackendTexture> BeginWriteAccess() override {
    return backing_impl()->GetGraphiteBackendTextures();
  }

  void EndWriteAccess() override {
    for (auto& write_surface : write_surfaces_) {
      CHECK(write_surface->unique());
    }
    write_surfaces_.clear();
  }

  std::vector<skgpu::graphite::BackendTexture> BeginReadAccess() override {
    CHECK(write_surfaces_.empty());
    return backing_impl()->GetGraphiteBackendTextures();
  }

  void EndReadAccess() override { CHECK(write_surfaces_.empty()); }

  bool SupportsMultipleConcurrentReadAccess() override { return true; }

 private:
  WrappedGraphiteTextureBacking* backing_impl() {
    return static_cast<WrappedGraphiteTextureBacking*>(backing());
  }

  std::vector<sk_sp<SkSurface>> write_surfaces_;
  scoped_refptr<SharedContextState> context_state_;
};

WrappedGraphiteTextureBacking::WrappedGraphiteTextureBacking(
    base::PassKey<WrappedSkImageBackingFactory>,
    const Mailbox& mailbox,
    viz::SharedImageFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage,
    scoped_refptr<SharedContextState> context_state,
    const bool thread_safe)
    : ClearTrackingSharedImageBacking(mailbox,
                                      format,
                                      size,
                                      color_space,
                                      surface_origin,
                                      alpha_type,
                                      usage,
                                      format.EstimatedSizeInBytes(size),
                                      thread_safe),
      context_state_(std::move(context_state)) {
  CHECK(!!context_state_);
  CHECK(context_state_->graphite_context());
}

WrappedGraphiteTextureBacking::~WrappedGraphiteTextureBacking() {
  for (auto& texture : graphite_textures_) {
    if (texture.isValid()) {
      context_state_->graphite_context()->deleteBackendTexture(texture);
    }
  }
}

bool WrappedGraphiteTextureBacking::Initialize() {
  CHECK(!format().IsCompressed());
  const bool mipmapped = usage() & SHARED_IMAGE_USAGE_MIPMAP;
  const int num_planes = format().NumberOfPlanes();
  graphite_textures_.resize(num_planes);
  for (int plane = 0; plane < num_planes; ++plane) {
    skgpu::graphite::TextureInfo texture_info = gpu::GetGraphiteTextureInfo(
        context_state_->gr_context_type(), format(), plane,
        /*is_yuv_plane=*/false, mipmapped);
    auto sk_size = gfx::SizeToSkISize(format().GetPlaneSize(plane, size()));
    auto texture = recorder()->createBackendTexture(sk_size, texture_info);
    if (!texture.isValid()) {
      LOG(ERROR) << "createBackendTexture() failed with format:"
                 << format().ToString();
      graphite_textures_.clear();
      return false;
    }
    graphite_textures_[plane] = std::move(texture);
  }
  return true;
}

bool WrappedGraphiteTextureBacking::InitializeWithData(
    base::span<const uint8_t> pixels) {
  CHECK(format().is_single_plane());
  CHECK(pixels.data());

  if (format().IsCompressed()) {
    // TODO(crbug.com/1430206): Add support for compressed backend textures.
    NOTIMPLEMENTED_LOG_ONCE();
    return false;
  }

  graphite_textures_.resize(1);
  auto image_info = AsSkImageInfo();
  if (pixels.size() != image_info.computeMinByteSize()) {
    LOG(ERROR) << "Invalid initial pixel data size";
    return false;
  }
  SkPixmap pixmap(image_info, pixels.data(), image_info.minRowBytes());

  auto& texture = graphite_textures_[0];
  skgpu::graphite::TextureInfo texture_info =
      gpu::GetGraphiteTextureInfo(context_state_->gr_context_type(), format());
  texture = recorder()->createBackendTexture(gfx::SizeToSkISize(size()),
                                             texture_info);
  if (!texture.isValid()) {
    LOG(ERROR) << "Graphite createBackendTexture() failed with format: "
               << format().ToString();
    return false;
  }

  if (!recorder()->updateBackendTexture(texture, &pixmap, /*numLevels=*/1)) {
    LOG(ERROR) << "Graphite updateBackendTexture() failed for format: "
               << format().ToString();
    return false;
  }

  if (!InsertRecordingAndSubmit()) {
    return false;
  }
  SetCleared();
  return true;
}

SharedImageBackingType WrappedGraphiteTextureBacking::GetType() const {
  return SharedImageBackingType::kWrappedGraphiteTexture;
}

void WrappedGraphiteTextureBacking::Update(
    std::unique_ptr<gfx::GpuFence> in_fence) {
  NOTREACHED();
}

bool WrappedGraphiteTextureBacking::UploadFromMemory(
    const std::vector<SkPixmap>& pixmaps) {
  CHECK_EQ(pixmaps.size(), graphite_textures_.size());
  if (context_state_->context_lost()) {
    return false;
  }

  bool updated = true;
  for (size_t i = 0; i < graphite_textures_.size(); ++i) {
    updated =
        updated && recorder()->updateBackendTexture(
                       graphite_textures_[i], &pixmaps[i], /*numLevels=*/1);
  }
  if (!updated) {
    LOG(ERROR) << "Graphite updateBackendTexture() failed";
    return false;
  }

  return InsertRecordingAndSubmit();
}

bool WrappedGraphiteTextureBacking::ReadbackToMemory(
    const std::vector<SkPixmap>& pixmaps) {
  CHECK_EQ(pixmaps.size(), graphite_textures_.size());
  if (context_state_->context_lost()) {
    return false;
  }

  std::vector<ReadPixelsContext> contexts(format().NumberOfPlanes());
  for (int i = 0; i < format().NumberOfPlanes(); i++) {
    const auto color_type =
        viz::ToClosestSkColorType(/*gpu_compositing=*/true, format(), i);
    sk_sp<SkImage> sk_image = SkImages::AdoptTextureFrom(
        context_state_->gpu_main_graphite_recorder(), graphite_textures_[i],
        color_type, kOpaque_SkAlphaType, /*colorSpace=*/nullptr);
    if (!sk_image) {
      return false;
    }
    const gfx::Size plane_size = format().GetPlaneSize(i, size());
    const SkIRect src_rect =
        SkIRect::MakeWH(plane_size.width(), plane_size.height());
    context_state_->graphite_context()->asyncRescaleAndReadPixels(
        sk_image.get(), pixmaps[i].info(), src_rect,
        SkImage::RescaleGamma::kSrc, SkImage::RescaleMode::kRepeatedLinear,
        &OnReadPixelsDone, &contexts[i]);
  }

  if (!context_state_->graphite_context()->submit(
          skgpu::graphite::SyncToCpu::kYes)) {
    LOG(ERROR) << "Graphite context submit() failed";
    return false;
  }

  for (int i = 0; i < format().NumberOfPlanes(); i++) {
    CHECK(contexts[i].finished);
    const gfx::Size plane_size = format().GetPlaneSize(i, size());
    libyuv::CopyPlane(
        static_cast<const uint8_t*>(contexts[i].async_result->data(0)),
        contexts[i].async_result->rowBytes(0),
        static_cast<uint8_t*>(pixmaps[i].writable_addr()),
        pixmaps[i].rowBytes(), pixmaps[i].info().minRowBytes(),
        plane_size.height());
  }

  return true;
}

bool WrappedGraphiteTextureBacking::InsertRecordingAndSubmit() {
  auto recording = recorder()->snap();
  if (!recording) {
    LOG(ERROR) << "Graphite failed to snap recording from GPU main recorder";
    return false;
  }
  skgpu::graphite::InsertRecordingInfo info = {};
  info.fRecording = recording.get();
  if (!context_state_->graphite_context()->insertRecording(info)) {
    LOG(ERROR) << "Graphite insertRecording() failed";
    return false;
  }
  if (!context_state_->graphite_context()->submit()) {
    LOG(ERROR) << "Graphite context submit() failed";
    return false;
  }
  return true;
}

const std::vector<skgpu::graphite::BackendTexture>&
WrappedGraphiteTextureBacking::GetGraphiteBackendTextures() {
  return graphite_textures_;
}

SkColorType WrappedGraphiteTextureBacking::GetSkColorType(int plane_index) {
  return viz::ToClosestSkColorType(/*gpu_compositing=*/true, format(),
                                   plane_index);
}

std::unique_ptr<SkiaGraphiteImageRepresentation>
WrappedGraphiteTextureBacking::ProduceSkiaGraphite(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker,
    scoped_refptr<SharedContextState> context_state) {
  if (context_state->context_lost()) {
    return nullptr;
  }
  // This method should only be called on the same thread on which this
  // backing is created on. Hence adding a check on context_state to ensure
  // this.
  CHECK_EQ(context_state_, context_state);
  return std::make_unique<SkiaGraphiteImageRepresentationImpl>(
      manager, this, tracker, std::move(context_state));
}

#if BUILDFLAG(SKIA_USE_DAWN)
std::unique_ptr<GLTexturePassthroughImageRepresentation>
WrappedGraphiteTextureBacking::ProduceGLTexturePassthrough(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker) {
  CHECK(context_state_->IsGraphiteDawnVulkan());
  if (context_state_->context_lost()) {
    return nullptr;
  }
  return std::make_unique<GLTexturePassthroughFallbackImageRepresentation>(
      manager, this, tracker, context_state_->progress_reporter());
}

std::unique_ptr<DawnImageRepresentation>
WrappedGraphiteTextureBacking::ProduceDawn(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker,
    const wgpu::Device& device,
    wgpu::BackendType backend_type,
    std::vector<wgpu::TextureFormat> view_formats) {
  CHECK(context_state_->IsGraphiteDawnVulkan());
  if (context_state_->context_lost()) {
    return nullptr;
  }
  return std::make_unique<DawnFallbackImageRepresentation>(
      manager, this, tracker, device, ToDawnFormat(format()),
      std::move(view_formats));
}
#endif  // BUILDFLAG(SKIA_USE_DAWN)

}  // namespace gpu
