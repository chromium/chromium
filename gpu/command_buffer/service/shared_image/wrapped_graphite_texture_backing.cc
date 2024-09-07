// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/wrapped_graphite_texture_backing.h"

#include <utility>

#include "base/logging.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/shared_image/copy_image_plane.h"
#include "gpu/command_buffer/service/shared_image/gl_texture_passthrough_fallback_image_representation.h"
#include "gpu/command_buffer/service/shared_image/shared_image_format_service_utils.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"
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

#if BUILDFLAG(SKIA_USE_DAWN)
#include "gpu/command_buffer/service/dawn_context_provider.h"
#include "gpu/command_buffer/service/shared_image/dawn_fallback_image_representation.h"
#endif

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
    const auto& texture_holders =
        backing_impl()->GetWrappedGraphiteTextureHolders();
    CHECK(write_surfaces_.empty());
    write_surfaces_.reserve(texture_holders.size());
    for (int plane = 0; plane < format().NumberOfPlanes(); ++plane) {
      auto color_type =
          viz::ToClosestSkColorType(/*gpu_compositing=*/true, format(), plane);
      void* release_context =
          scoped_refptr<WrappedGraphiteTextureHolder>(texture_holders[plane])
              .release();
      auto release_proc = [](void* context) {
        static_cast<WrappedGraphiteTextureHolder*>(context)->Release();
      };
      auto surface = SkSurfaces::WrapBackendTexture(
          context_state_->gpu_main_graphite_recorder(),
          texture_holders[plane]->texture(), color_type,
          color_space().ToSkColorSpace(), &surface_props, release_proc,
          release_context, WrappedTextureDebugLabel(plane));
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

  // Graphite context submit is done only once per frame for Dawn D3D backend.
  bool NeedGraphiteContextSubmitBeforeEndAccess() override {
    return !context_state_->IsGraphiteDawnD3D();
  }

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
    gpu::SharedImageUsageSet usage,
    std::string debug_label,
    scoped_refptr<SharedContextState> context_state,
    const bool thread_safe)
    : ClearTrackingSharedImageBacking(mailbox,
                                      format,
                                      size,
                                      color_space,
                                      surface_origin,
                                      alpha_type,
                                      usage,
                                      std::move(debug_label),
                                      format.EstimatedSizeInBytes(size),
                                      thread_safe),
      context_state_(std::move(context_state)) {
  CHECK(context_state_);
  CHECK(context_state_->graphite_context());

  if (is_thread_safe()) {
    // If the backing is thread safe then it may be destroyed on a different
    // thread. Store the task runner so textures can be destroyed on the same
    // thread they were created on.
    CHECK(base::SingleThreadTaskRunner::HasCurrentDefault());
    created_task_runner_ = base::SingleThreadTaskRunner::GetCurrentDefault();
  }
}

WrappedGraphiteTextureBacking::~WrappedGraphiteTextureBacking() {
  if (created_task_runner_ && !created_task_runner_->BelongsToCurrentThread()) {
    // The `context_state_` ref needs to be dropped on original thread for cases
    // like DrDC with Android.
    created_task_runner_->PostTask(
        FROM_HERE, base::DoNothingWithBoundArgs(std::move(context_state_)));
  }
}

bool WrappedGraphiteTextureBacking::Initialize() {
  CHECK(!format().IsCompressed());
  const bool mipmapped = usage().Has(SHARED_IMAGE_USAGE_MIPMAP);
  const int num_planes = format().NumberOfPlanes();
  texture_holders_.resize(num_planes);
  for (int plane = 0; plane < num_planes; ++plane) {
    // is_yuv_plane is false here because the planes are separate single plane
    // textures, not planes of a multi-planar YUV texture.
    constexpr bool is_yuv_plane = false;
    skgpu::graphite::TextureInfo texture_info = gpu::GraphiteBackendTextureInfo(
        context_state_->gr_context_type(), format(), /*readonly=*/false, plane,
        is_yuv_plane, mipmapped, /*scanout_dcomp_surface=*/false,
        /*supports_multiplanar_rendering=*/false,
        /*supports_multiplanar_copy=*/false);
    auto sk_size = gfx::SizeToSkISize(format().GetPlaneSize(plane, size()));
    auto texture = recorder()->createBackendTexture(sk_size, texture_info);
    if (!texture.isValid()) {
      LOG(ERROR) << "createBackendTexture() failed with format:"
                 << format().ToString();
      texture_holders_.clear();
      return false;
    }
    texture_holders_[plane] =
        base::MakeRefCounted<WrappedGraphiteTextureHolder>(
            std::move(texture), context_state_, created_task_runner_);
  }
  return true;
}

bool WrappedGraphiteTextureBacking::InitializeWithData(
    base::span<const uint8_t> pixels) {
  CHECK(format().is_single_plane());
  CHECK(pixels.data());

  skgpu::graphite::TextureInfo texture_info = gpu::GraphiteBackendTextureInfo(
      context_state_->gr_context_type(), format(), /*readonly=*/false,
      /*plane_index=*/0, /*is_yuv_plane=*/false,
      /*mipmapped=*/false, /*scanout_dcomp_surface=*/false,
      /*supports_multiplanar_rendering=*/false,
      /*supports_multiplanar_copy=*/false);
  skgpu::graphite::BackendTexture texture = recorder()->createBackendTexture(
      gfx::SizeToSkISize(size()), texture_info);
  if (!texture.isValid()) {
    LOG(ERROR) << "Graphite createBackendTexture() failed with format: "
               << format().ToString();
    return false;
  }

  if (format().IsCompressed()) {
    if (!recorder()->updateCompressedBackendTexture(texture, pixels.data(),
                                                    pixels.size())) {
      LOG(ERROR) << "updateCompressedBackendTexture() failed for "
                 << format().ToString();
      return false;
    }
  } else {
    auto image_info = AsSkImageInfo();
    if (pixels.size() != image_info.computeMinByteSize()) {
      LOG(ERROR) << "Invalid initial pixel data size";
      return false;
    }

    SkPixmap pixmap(image_info, pixels.data(), image_info.minRowBytes());
    if (!recorder()->updateBackendTexture(texture, &pixmap, /*numLevels=*/1)) {
      LOG(ERROR) << "updateBackendTexture() failed for " << format().ToString();
      return false;
    }
  }

  if (!InsertRecordingAndSubmit()) {
    return false;
  }

  texture_holders_ = std::vector<scoped_refptr<WrappedGraphiteTextureHolder>>{
      base::MakeRefCounted<WrappedGraphiteTextureHolder>(
          std::move(texture), context_state_, created_task_runner_)};
  SetCleared();
  return true;
}

SharedImageBackingType WrappedGraphiteTextureBacking::GetType() const {
  return SharedImageBackingType::kWrappedGraphiteTexture;
}

void WrappedGraphiteTextureBacking::Update(
    std::unique_ptr<gfx::GpuFence> in_fence) {
  NOTREACHED_IN_MIGRATION();
}

bool WrappedGraphiteTextureBacking::UploadFromMemory(
    const std::vector<SkPixmap>& pixmaps) {
  // Using `context_state_` isn't compatible with a thread safe backing.
  CHECK(!is_thread_safe());
  CHECK_EQ(pixmaps.size(), texture_holders_.size());

  if (context_state_->context_lost()) {
    return false;
  }

  bool updated = true;
  for (size_t i = 0; i < texture_holders_.size(); ++i) {
    updated = updated &&
              recorder()->updateBackendTexture(texture_holders_[i]->texture(),
                                               &pixmaps[i], /*numLevels=*/1);
  }
  if (!updated) {
    LOG(ERROR) << "Graphite updateBackendTexture() failed";
    return false;
  }

  return InsertRecordingAndSubmit();
}

bool WrappedGraphiteTextureBacking::ReadbackToMemory(
    const std::vector<SkPixmap>& pixmaps) {
  // Using `context_state_` isn't compatible with a thread safe backing.
  CHECK(!is_thread_safe());
  CHECK_EQ(pixmaps.size(), texture_holders_.size());

  if (context_state_->context_lost()) {
    return false;
  }

  std::vector<ReadPixelsContext> contexts(format().NumberOfPlanes());
  for (int i = 0; i < format().NumberOfPlanes(); i++) {
    const auto color_type =
        viz::ToClosestSkColorType(/*gpu_compositing=*/true, format(), i);
    sk_sp<SkImage> sk_image =
        SkImages::WrapTexture(context_state_->gpu_main_graphite_recorder(),
                              texture_holders_[i]->texture(), color_type,
                              kOpaque_SkAlphaType, /*colorSpace=*/nullptr);
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
    CopyImagePlane(
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

const std::vector<scoped_refptr<WrappedGraphiteTextureHolder>>&
WrappedGraphiteTextureBacking::GetWrappedGraphiteTextureHolders() {
  return texture_holders_;
}

std::vector<skgpu::graphite::BackendTexture>
WrappedGraphiteTextureBacking::GetGraphiteBackendTextures() {
  std::vector<skgpu::graphite::BackendTexture> textures;
  for (auto holder : texture_holders_) {
    textures.push_back(std::move(holder->texture()));
  }
  return textures;
}

std::unique_ptr<SkiaGraphiteImageRepresentation>
WrappedGraphiteTextureBacking::ProduceSkiaGraphite(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker,
    scoped_refptr<SharedContextState> context_state) {
  if (context_state->context_lost()) {
    return nullptr;
  }
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
      manager, this, tracker, context_state_->progress_reporter(),
      context_state_->GetGLFormatCaps());
}

std::unique_ptr<DawnImageRepresentation>
WrappedGraphiteTextureBacking::ProduceDawn(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker,
    const wgpu::Device& device,
    wgpu::BackendType backend_type,
    std::vector<wgpu::TextureFormat> view_formats,
    scoped_refptr<SharedContextState> context_state) {
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
