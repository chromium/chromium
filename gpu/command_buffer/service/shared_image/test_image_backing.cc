// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/test_image_backing.h"

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image/shared_image_format_service_utils.h"
#include "skia/ext/legacy_display_globals.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/gpu/ganesh/GrBackendSurface.h"
#include "third_party/skia/include/gpu/ganesh/gl/GrGLBackendSurface.h"
#include "third_party/skia/include/gpu/ganesh/gl/GrGLTypes.h"
#include "third_party/skia/include/gpu/ganesh/mock/GrMockTypes.h"
#include "third_party/skia/include/private/chromium/GrPromiseImageTexture.h"

namespace gpu {
namespace {
class TestGLTextureImageRepresentation : public GLTextureImageRepresentation {
 public:
  TestGLTextureImageRepresentation(
      SharedImageManager* manager,
      SharedImageBacking* backing,
      MemoryTypeTracker* tracker,
      const std::vector<raw_ptr<gles2::Texture>>& textures)
      : GLTextureImageRepresentation(manager, backing, tracker),
        textures_(std::move(textures)) {}

  gles2::Texture* GetTexture(int plane_index) override {
    DCHECK(backing()->format().IsValidPlaneIndex(plane_index));
    return textures_[plane_index];
  }
  bool BeginAccess(GLenum mode) override {
    return static_cast<TestImageBacking*>(backing())->can_access();
  }
  void EndAccess() override {}

 private:
  std::vector<raw_ptr<gles2::Texture>> textures_;
};

class TestGLTexturePassthroughImageRepresentation
    : public GLTexturePassthroughImageRepresentation {
 public:
  TestGLTexturePassthroughImageRepresentation(
      SharedImageManager* manager,
      SharedImageBacking* backing,
      MemoryTypeTracker* tracker,
      const std::vector<scoped_refptr<gles2::TexturePassthrough>>& textures)
      : GLTexturePassthroughImageRepresentation(manager, backing, tracker),
        textures_(std::move(textures)) {}

  const scoped_refptr<gles2::TexturePassthrough>& GetTexturePassthrough(
      int plane_index) override {
    DCHECK(backing()->format().IsValidPlaneIndex(plane_index));
    return textures_[plane_index];
  }
  bool BeginAccess(GLenum mode) override {
    return static_cast<TestImageBacking*>(backing())->can_access();
  }
  void EndAccess() override {}

 private:
  std::vector<scoped_refptr<gles2::TexturePassthrough>> textures_;
};

class TestSkiaImageRepresentation : public SkiaGaneshImageRepresentation {
 public:
  TestSkiaImageRepresentation(
      GrDirectContext* gr_context,
      SharedImageManager* manager,
      SharedImageBacking* backing,
      MemoryTypeTracker* tracker,
      std::vector<sk_sp<GrPromiseImageTexture>> promise_textures)
      : SkiaGaneshImageRepresentation(gr_context, manager, backing, tracker),
        promise_textures_(std::move(promise_textures)) {}

 protected:
  std::vector<sk_sp<SkSurface>> BeginWriteAccess(
      int final_msaa_count,
      const SkSurfaceProps& surface_props,
      const gfx::Rect& update_rect,
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores,
      std::unique_ptr<skgpu::MutableTextureState>* end_state) override {
    if (!static_cast<TestImageBacking*>(backing())->can_access()) {
      return {};
    }
    SkSurfaceProps props = skia::LegacyDisplayGlobals::GetSkSurfaceProps();
    auto surface = SkSurfaces::Raster(
        SkImageInfo::MakeN32Premul(size().width(), size().height()), &props);
    if (!surface)
      return {};
    return {surface};
  }
  std::vector<sk_sp<GrPromiseImageTexture>> BeginWriteAccess(
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores,
      std::unique_ptr<skgpu::MutableTextureState>* end_state) override {
    if (!static_cast<TestImageBacking*>(backing())->can_access()) {
      return {};
    }
    return promise_textures_;
  }
  void EndWriteAccess() override {}
  std::vector<sk_sp<GrPromiseImageTexture>> BeginReadAccess(
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores,
      std::unique_ptr<skgpu::MutableTextureState>* end_state) override {
    if (!static_cast<TestImageBacking*>(backing())->can_access()) {
      return {};
    }
    return promise_textures_;
  }
  void EndReadAccess() override {}

 private:
  std::vector<sk_sp<GrPromiseImageTexture>> promise_textures_;
};

class TestDawnImageRepresentation : public DawnImageRepresentation {
 public:
  TestDawnImageRepresentation(SharedImageManager* manager,
                              SharedImageBacking* backing,
                              MemoryTypeTracker* tracker)
      : DawnImageRepresentation(manager, backing, tracker) {}

  wgpu::Texture BeginAccess(wgpu::TextureUsage usage,
                            wgpu::TextureUsage internal_usage) override {
    if (!static_cast<TestImageBacking*>(backing())->can_access()) {
      return nullptr;
    }

    return wgpu::Texture(reinterpret_cast<WGPUTexture>(203));
  }

  void EndAccess() override {}
};

class TestMetalSkiaGraphiteImageRepresentation
    : public SkiaGraphiteImageRepresentation {
 public:
  TestMetalSkiaGraphiteImageRepresentation(SharedImageManager* manager,
                                           SharedImageBacking* backing,
                                           MemoryTypeTracker* tracker)
      : SkiaGraphiteImageRepresentation(manager, backing, tracker) {}

  std::vector<skgpu::graphite::BackendTexture> BeginReadAccess() override {
    return {};
  }
  void EndReadAccess() override {}

  std::vector<sk_sp<SkSurface>> BeginWriteAccess(
      const SkSurfaceProps& surface_props,
      const gfx::Rect& update_rect) override {
    std::vector<sk_sp<SkSurface>> surfaces;
    for (int plane = 0; plane < format().NumberOfPlanes(); plane++) {
      auto plane_size = format().GetPlaneSize(plane, size());
      surfaces.push_back(
          SkSurfaces::Null(plane_size.width(), plane_size.height()));
    }
    return surfaces;
  }
  std::vector<skgpu::graphite::BackendTexture> BeginWriteAccess() override {
    return {};
  }
  void EndWriteAccess() override {}
};

}  // namespace

TestImageBacking::TestImageBacking(const Mailbox& mailbox,
                                   viz::SharedImageFormat format,
                                   const gfx::Size& size,
                                   const gfx::ColorSpace& color_space,
                                   GrSurfaceOrigin surface_origin,
                                   SkAlphaType alpha_type,
                                   SharedImageUsageSet usage,
                                   size_t estimated_size,
                                   GLuint texture_id)
    : SharedImageBacking(mailbox,
                         format,
                         size,
                         color_space,
                         surface_origin,
                         alpha_type,
                         usage,
                         "TestBacking",
                         estimated_size,
                         /*is_thread_safe=*/false) {
  const int num_textures =
      format.PrefersExternalSampler() ? 1 : format.NumberOfPlanes();
  textures_.reserve(num_textures);
  passthrough_textures_.reserve(num_textures);

  const int num_planes = format.NumberOfPlanes();
  for (int plane = 0; plane < num_planes; ++plane) {
    auto* texture = new gles2::Texture(texture_id + plane);
    texture->SetLightweightRef();
    texture->SetTarget(GL_TEXTURE_2D, 1);
    texture->set_min_filter(GL_LINEAR);
    texture->set_mag_filter(GL_LINEAR);
    texture->set_wrap_t(GL_CLAMP_TO_EDGE);
    texture->set_wrap_s(GL_CLAMP_TO_EDGE);
    GLFormatDesc format_desc =
        GLFormatCaps().ToGLFormatDesc(format, /*plane_index=*/plane);
    gfx::Size plane_size = format.GetPlaneSize(plane, size);
    texture->SetLevelInfo(GL_TEXTURE_2D, 0, format_desc.image_internal_format,
                          plane_size.width(), plane_size.height(), 1, 0,
                          format_desc.data_format, format_desc.data_type,
                          gfx::Rect());
    texture->SetImmutable(true, true);
    auto passthrough_texture = base::MakeRefCounted<gles2::TexturePassthrough>(
        texture_id + plane, GL_TEXTURE_2D);
    textures_.push_back(texture);
    passthrough_textures_.push_back(std::move(passthrough_texture));
  }
}

TestImageBacking::TestImageBacking(const Mailbox& mailbox,
                                   viz::SharedImageFormat format,
                                   const gfx::Size& size,
                                   const gfx::ColorSpace& color_space,
                                   GrSurfaceOrigin surface_origin,
                                   SkAlphaType alpha_type,
                                   SharedImageUsageSet usage,
                                   size_t estimated_size)
    : TestImageBacking(mailbox,
                       format,
                       size,
                       color_space,
                       surface_origin,
                       alpha_type,
                       usage,
                       estimated_size,
                       /*texture_id=*/203) {
  // Using a dummy |texture_id|, so lose our context so we don't do anything
  // real with it.
  OnContextLost();
}

TestImageBacking::~TestImageBacking() {
  // Pretend our context is lost to avoid actual cleanup in |textures_| or
  // |passthrough_textures_|.
  GLuint texture_id = service_id();
  for (auto& texture : textures_) {
    texture.ExtractAsDangling()->RemoveLightweightRef(/*have_context=*/false);
  }
  for (auto& passthrough_texture : passthrough_textures_) {
    passthrough_texture->MarkContextLost();
    passthrough_texture.reset();
  }
  if (have_context())
    glDeleteTextures(1, &texture_id);
}

bool TestImageBacking::GetUploadFromMemoryCalledAndReset() {
  return std::exchange(upload_from_memory_called_, false);
}

bool TestImageBacking::GetReadbackToMemoryCalledAndReset() {
  return std::exchange(readback_to_memory_called_, false);
}

SharedImageBackingType TestImageBacking::GetType() const {
  return SharedImageBackingType::kTest;
}

gfx::Rect TestImageBacking::ClearedRect() const {
  return textures_[0]->GetLevelClearedRect(textures_[0]->target(), 0);
}

void TestImageBacking::SetClearedRect(const gfx::Rect& cleared_rect) {
  textures_[0]->SetLevelClearedRect(textures_[0]->target(), 0, cleared_rect);
}

void TestImageBacking::SetPurgeable(bool purgeable) {
  if (purgeable) {
    if (set_purgeable_callback_)
      set_purgeable_callback_.Run(mailbox());
  } else {
    if (set_not_purgeable_callback_)
      set_not_purgeable_callback_.Run(mailbox());
  }
}

bool TestImageBacking::UploadFromMemory(const std::vector<SkPixmap>& pixmap) {
  DCHECK_EQ(format().NumberOfPlanes(), static_cast<int>(pixmap.size()));
  upload_from_memory_called_ = true;
  return true;
}

bool TestImageBacking::ReadbackToMemory(const std::vector<SkPixmap>& pixmaps) {
  DCHECK_EQ(format().NumberOfPlanes(), static_cast<int>(pixmaps.size()));
  readback_to_memory_called_ = true;
  return true;
}

std::unique_ptr<GLTextureImageRepresentation>
TestImageBacking::ProduceGLTexture(SharedImageManager* manager,
                                   MemoryTypeTracker* tracker) {
  return std::make_unique<TestGLTextureImageRepresentation>(manager, this,
                                                            tracker, textures_);
}

std::unique_ptr<GLTexturePassthroughImageRepresentation>
TestImageBacking::ProduceGLTexturePassthrough(SharedImageManager* manager,
                                              MemoryTypeTracker* tracker) {
  return std::make_unique<TestGLTexturePassthroughImageRepresentation>(
      manager, this, tracker, passthrough_textures_);
}

std::unique_ptr<SkiaGaneshImageRepresentation>
TestImageBacking::ProduceSkiaGanesh(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker,
    scoped_refptr<SharedContextState> context_state) {
  DCHECK(!textures_.empty());
  std::vector<sk_sp<GrPromiseImageTexture>> promise_textures;
  const GLFormatCaps caps =
      context_state ? context_state->GetGLFormatCaps() : GLFormatCaps();
  if (format().is_single_plane() || format().PrefersExternalSampler()) {
    GLFormatDesc format_desc =
        format().PrefersExternalSampler()
            ? caps.ToGLFormatDescExternalSampler(format())
            : caps.ToGLFormatDesc(format(), /*plane_index=*/0);
    // TODO(b/346406519): Investigate if possible to change target to
    // GL_TEXTURE_2D.
    auto backend_texture = GrBackendTextures::MakeGL(
        size().width(), size().height(), skgpu::Mipmapped::kNo,
        GrGLTextureInfo{
            GL_TEXTURE_EXTERNAL_OES, textures_[0]->service_id(),
            static_cast<GrGLenum>(format_desc.storage_internal_format)});
    if (!backend_texture.isValid()) {
      return nullptr;
    }

    auto promise_texture = GrPromiseImageTexture::Make(backend_texture);
    if (!promise_texture) {
      return nullptr;
    }
    promise_textures.push_back(std::move(promise_texture));
  } else {
    DCHECK_EQ(format().NumberOfPlanes(), static_cast<int>(textures_.size()));
    for (int plane_index = 0; plane_index < format().NumberOfPlanes();
         plane_index++) {
      // Use the format and size per plane for multiplanar formats.
      gfx::Size plane_size = format().GetPlaneSize(plane_index, size());
      GLFormatDesc format_desc = caps.ToGLFormatDesc(format(), plane_index);
      // TODO(b/346406519): Investigate if possible to change target to
      // GL_TEXTURE_2D.
      auto backend_texture = GrBackendTextures::MakeGL(
          plane_size.width(), plane_size.height(), skgpu::Mipmapped::kNo,
          GrGLTextureInfo{
              GL_TEXTURE_EXTERNAL_OES, textures_[plane_index]->service_id(),
              static_cast<GrGLenum>(format_desc.storage_internal_format)});

      if (!backend_texture.isValid()) {
        return nullptr;
      }

      auto promise_texture = GrPromiseImageTexture::Make(backend_texture);
      if (!promise_texture) {
        return nullptr;
      }
      promise_textures.push_back(std::move(promise_texture));
    }
  }
  return std::make_unique<TestSkiaImageRepresentation>(
      context_state ? context_state->gr_context() : nullptr, manager, this,
      tracker, std::move(promise_textures));
}

std::unique_ptr<DawnImageRepresentation> TestImageBacking::ProduceDawn(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker,
    const wgpu::Device& device,
    wgpu::BackendType backend_type,
    std::vector<wgpu::TextureFormat> view_formats,
    scoped_refptr<SharedContextState> context_state) {
  return std::make_unique<TestDawnImageRepresentation>(manager, this, tracker);
}

std::unique_ptr<SkiaGraphiteImageRepresentation>
TestImageBacking::ProduceSkiaGraphite(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker,
    scoped_refptr<SharedContextState> context_state) {
#if BUILDFLAG(SKIA_USE_METAL)
  return std::make_unique<TestMetalSkiaGraphiteImageRepresentation>(
      manager, this, tracker);
#else
  return nullptr;
#endif  // BUILDFLAG(SKIA_USE_METAL)
}

std::unique_ptr<OverlayImageRepresentation> TestImageBacking::ProduceOverlay(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker) {
  return std::make_unique<TestOverlayImageRepresentation>(manager, this,
                                                          tracker);
}

bool TestOverlayImageRepresentation::BeginReadAccess(
    gfx::GpuFenceHandle& acquire_fence) {
  return true;
}

void TestOverlayImageRepresentation::EndReadAccess(
    gfx::GpuFenceHandle release_fence) {}

#if BUILDFLAG(IS_ANDROID)
std::unique_ptr<base::android::ScopedHardwareBufferFenceSync>
TestOverlayImageRepresentation::GetAHardwareBufferFenceSync() {
  return nullptr;
}
#endif

#if BUILDFLAG(IS_APPLE)
bool TestOverlayImageRepresentation::IsInUseByWindowServer() const {
  return static_cast<TestImageBacking*>(backing())->in_use_by_window_server();
}
#endif  // BUILDFLAG(IS_APPLE)

}  // namespace gpu
