// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/test_image_backing.h"
#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image/shared_image_format_service_utils.h"
#include "skia/ext/legacy_display_globals.h"
#include "third_party/skia/include/gpu/GrBackendSurface.h"
#include "third_party/skia/include/gpu/ganesh/gl/GrGLBackendSurface.h"
#include "third_party/skia/include/gpu/gl/GrGLTypes.h"
#include "third_party/skia/include/gpu/mock/GrMockTypes.h"
#include "third_party/skia/include/private/chromium/GrPromiseImageTexture.h"

namespace gpu {
namespace {
class TestGLTextureImageRepresentation : public GLTextureImageRepresentation {
 public:
  TestGLTextureImageRepresentation(SharedImageManager* manager,
                                   SharedImageBacking* backing,
                                   MemoryTypeTracker* tracker,
                                   gles2::Texture* texture)
      : GLTextureImageRepresentation(manager, backing, tracker),
        texture_(texture) {}

  gles2::Texture* GetTexture(int plane_index) override {
    DCHECK_EQ(plane_index, 0);
    return texture_;
  }
  bool BeginAccess(GLenum mode) override {
    return static_cast<TestImageBacking*>(backing())->can_access();
  }
  void EndAccess() override {}

 private:
  const raw_ptr<gles2::Texture> texture_;
};

class TestGLTexturePassthroughImageRepresentation
    : public GLTexturePassthroughImageRepresentation {
 public:
  TestGLTexturePassthroughImageRepresentation(
      SharedImageManager* manager,
      SharedImageBacking* backing,
      MemoryTypeTracker* tracker,
      scoped_refptr<gles2::TexturePassthrough> texture)
      : GLTexturePassthroughImageRepresentation(manager, backing, tracker),
        texture_(std::move(texture)) {}

  const scoped_refptr<gles2::TexturePassthrough>& GetTexturePassthrough(
      int plane_index) override {
    DCHECK_EQ(plane_index, 0);
    return texture_;
  }
  bool BeginAccess(GLenum mode) override {
    return static_cast<TestImageBacking*>(backing())->can_access();
  }
  void EndAccess() override {}

 private:
  const scoped_refptr<gles2::TexturePassthrough> texture_;
};

class TestSkiaImageRepresentation : public SkiaGaneshImageRepresentation {
 public:
  TestSkiaImageRepresentation(GrDirectContext* gr_context,
                              SharedImageManager* manager,
                              SharedImageBacking* backing,
                              MemoryTypeTracker* tracker)
      : SkiaGaneshImageRepresentation(gr_context, manager, backing, tracker) {}

 protected:
  std::vector<sk_sp<SkSurface>> BeginWriteAccess(
      int final_msaa_count,
      const SkSurfaceProps& surface_props,
      const gfx::Rect& update_rect,
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores,
      std::unique_ptr<GrBackendSurfaceMutableState>* end_state) override {
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
      std::unique_ptr<GrBackendSurfaceMutableState>* end_state) override {
    if (!static_cast<TestImageBacking*>(backing())->can_access()) {
      return {};
    }

    auto promise_texture = GrPromiseImageTexture::Make(backend_tex());
    if (!promise_texture)
      return {};
    return {promise_texture};
  }
  void EndWriteAccess() override {}
  std::vector<sk_sp<GrPromiseImageTexture>> BeginReadAccess(
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores,
      std::unique_ptr<GrBackendSurfaceMutableState>* end_state) override {
    if (!static_cast<TestImageBacking*>(backing())->can_access()) {
      return {};
    }

    auto promise_texture = GrPromiseImageTexture::Make(backend_tex());
    if (!promise_texture)
      return {};
    return {promise_texture};
  }
  void EndReadAccess() override {}

 private:
  GrBackendTexture backend_tex() {
    return GrBackendTextures::MakeGL(
        size().width(), size().height(), skgpu::Mipmapped::kNo,
        GrGLTextureInfo{GL_TEXTURE_EXTERNAL_OES,
                        static_cast<TestImageBacking*>(backing())->service_id(),
                        static_cast<GrGLenum>(TextureStorageFormat(
                            format(), /*use_angle_rgbx_format=*/false))});
  }
};

class TestDawnImageRepresentation : public DawnImageRepresentation {
 public:
  TestDawnImageRepresentation(SharedImageManager* manager,
                              SharedImageBacking* backing,
                              MemoryTypeTracker* tracker)
      : DawnImageRepresentation(manager, backing, tracker) {}

  wgpu::Texture BeginAccess(wgpu::TextureUsage usage) override {
    if (!static_cast<TestImageBacking*>(backing())->can_access()) {
      return nullptr;
    }

    return wgpu::Texture(reinterpret_cast<WGPUTexture>(203));
  }

  void EndAccess() override {}
};

}  // namespace

class TestOverlayImageRepresentation : public OverlayImageRepresentation {
 public:
  TestOverlayImageRepresentation(SharedImageManager* manager,
                                 SharedImageBacking* backing,
                                 MemoryTypeTracker* tracker)
      : OverlayImageRepresentation(manager, backing, tracker) {}

  bool BeginReadAccess(gfx::GpuFenceHandle& acquire_fence) override {
    return true;
  }
  void EndReadAccess(gfx::GpuFenceHandle release_fence) override {}

#if BUILDFLAG(IS_ANDROID)
  std::unique_ptr<base::android::ScopedHardwareBufferFenceSync>
  GetAHardwareBufferFenceSync() override {
    return nullptr;
  }
#endif
};

TestImageBacking::TestImageBacking(const Mailbox& mailbox,
                                   viz::SharedImageFormat format,
                                   const gfx::Size& size,
                                   const gfx::ColorSpace& color_space,
                                   GrSurfaceOrigin surface_origin,
                                   SkAlphaType alpha_type,
                                   uint32_t usage,
                                   size_t estimated_size,
                                   GLuint texture_id)
    : SharedImageBacking(mailbox,
                         format,
                         size,
                         color_space,
                         surface_origin,
                         alpha_type,
                         usage,
                         estimated_size,
                         /*is_thread_safe=*/false),
      service_id_(texture_id) {
  texture_ = new gles2::Texture(service_id_);
  texture_->SetLightweightRef();
  texture_->SetTarget(GL_TEXTURE_2D, 1);
  texture_->set_min_filter(GL_LINEAR);
  texture_->set_mag_filter(GL_LINEAR);
  texture_->set_wrap_t(GL_CLAMP_TO_EDGE);
  texture_->set_wrap_s(GL_CLAMP_TO_EDGE);
  texture_->SetLevelInfo(GL_TEXTURE_2D, 0, GLInternalFormat(format),
                         size.width(), size.height(), 1, 0,
                         GLDataFormat(format), GLDataType(format), gfx::Rect());
  texture_->SetImmutable(true, true);
  texture_passthrough_ = base::MakeRefCounted<gles2::TexturePassthrough>(
      service_id_, GL_TEXTURE_2D);
}

TestImageBacking::TestImageBacking(const Mailbox& mailbox,
                                   viz::SharedImageFormat format,
                                   const gfx::Size& size,
                                   const gfx::ColorSpace& color_space,
                                   GrSurfaceOrigin surface_origin,
                                   SkAlphaType alpha_type,
                                   uint32_t usage,
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
  // Pretend our context is lost to avoid actual cleanup in |texture_| or
  // |passthrough_texture_|.
  texture_.ExtractAsDangling()->RemoveLightweightRef(/*have_context=*/false);
  texture_passthrough_->MarkContextLost();
  texture_passthrough_.reset();

  if (have_context())
    glDeleteTextures(1, &service_id_);
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
  return texture_->GetLevelClearedRect(texture_->target(), 0);
}

void TestImageBacking::SetClearedRect(const gfx::Rect& cleared_rect) {
  texture_->SetLevelClearedRect(texture_->target(), 0, cleared_rect);
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
                                                            tracker, texture_);
}

std::unique_ptr<GLTexturePassthroughImageRepresentation>
TestImageBacking::ProduceGLTexturePassthrough(SharedImageManager* manager,
                                              MemoryTypeTracker* tracker) {
  return std::make_unique<TestGLTexturePassthroughImageRepresentation>(
      manager, this, tracker, texture_passthrough_);
}

std::unique_ptr<SkiaGaneshImageRepresentation>
TestImageBacking::ProduceSkiaGanesh(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker,
    scoped_refptr<SharedContextState> context_state) {
  return std::make_unique<TestSkiaImageRepresentation>(
      context_state ? context_state->gr_context() : nullptr, manager, this,
      tracker);
}

std::unique_ptr<DawnImageRepresentation> TestImageBacking::ProduceDawn(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker,
    const wgpu::Device& device,
    wgpu::BackendType backend_type,
    std::vector<wgpu::TextureFormat> view_formats) {
  return std::make_unique<TestDawnImageRepresentation>(manager, this, tracker);
}

std::unique_ptr<OverlayImageRepresentation> TestImageBacking::ProduceOverlay(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker) {
  return std::make_unique<TestOverlayImageRepresentation>(manager, this,
                                                          tracker);
}

}  // namespace gpu
