// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/test_shared_image_backing.h"
#include "build/build_config.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "skia/ext/legacy_display_globals.h"
#include "third_party/skia/include/core/SkPromiseImageTexture.h"
#include "third_party/skia/include/gpu/GrBackendSurface.h"
#include "third_party/skia/include/gpu/mock/GrMockTypes.h"

namespace gpu {
namespace {
class TestSharedImageRepresentationGLTexture
    : public SharedImageRepresentationGLTexture {
 public:
  TestSharedImageRepresentationGLTexture(SharedImageManager* manager,
                                         SharedImageBacking* backing,
                                         MemoryTypeTracker* tracker,
                                         gles2::Texture* texture)
      : SharedImageRepresentationGLTexture(manager, backing, tracker),
        texture_(texture) {}

  gles2::Texture* GetTexture() override { return texture_; }
  bool BeginAccess(GLenum mode) override {
    return static_cast<TestSharedImageBacking*>(backing())->can_access();
  }

 private:
  gles2::Texture* const texture_;
};

class TestSharedImageRepresentationGLTexturePassthrough
    : public SharedImageRepresentationGLTexturePassthrough {
 public:
  TestSharedImageRepresentationGLTexturePassthrough(
      SharedImageManager* manager,
      SharedImageBacking* backing,
      MemoryTypeTracker* tracker,
      scoped_refptr<gles2::TexturePassthrough> texture)
      : SharedImageRepresentationGLTexturePassthrough(manager,
                                                      backing,
                                                      tracker),
        texture_(std::move(texture)) {}

  const scoped_refptr<gles2::TexturePassthrough>& GetTexturePassthrough()
      override {
    return texture_;
  }
  bool BeginAccess(GLenum mode) override {
    return static_cast<TestSharedImageBacking*>(backing())->can_access();
  }

 private:
  const scoped_refptr<gles2::TexturePassthrough> texture_;
};

class TestSharedImageRepresentationSkia : public SharedImageRepresentationSkia {
 public:
  TestSharedImageRepresentationSkia(SharedImageManager* manager,
                                    SharedImageBacking* backing,
                                    MemoryTypeTracker* tracker)
      : SharedImageRepresentationSkia(manager, backing, tracker) {}

 protected:
  sk_sp<SkSurface> BeginWriteAccess(
      int final_msaa_count,
      const SkSurfaceProps& surface_props,
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores) override {
    if (!static_cast<TestSharedImageBacking*>(backing())->can_access()) {
      return nullptr;
    }
    SkSurfaceProps props = skia::LegacyDisplayGlobals::GetSkSurfaceProps();
    return SkSurface::MakeRasterN32Premul(size().width(), size().height(),
                                          &props);
  }
  sk_sp<SkPromiseImageTexture> BeginWriteAccess(
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores,
      std::unique_ptr<GrBackendSurfaceMutableState>* end_state) override {
    if (!static_cast<TestSharedImageBacking*>(backing())->can_access()) {
      return nullptr;
    }
    GrBackendTexture backend_tex(size().width(), size().height(),
                                 GrMipMapped::kNo, GrMockTextureInfo());
    return SkPromiseImageTexture::Make(backend_tex);
  }
  void EndWriteAccess(sk_sp<SkSurface> surface) override {}
  sk_sp<SkPromiseImageTexture> BeginReadAccess(
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores) override {
    if (!static_cast<TestSharedImageBacking*>(backing())->can_access()) {
      return nullptr;
    }
    GrBackendTexture backend_tex(size().width(), size().height(),
                                 GrMipMapped::kNo, GrMockTextureInfo());
    return SkPromiseImageTexture::Make(backend_tex);
  }
  void EndReadAccess() override {}
};

class TestSharedImageRepresentationDawn : public SharedImageRepresentationDawn {
 public:
  TestSharedImageRepresentationDawn(SharedImageManager* manager,
                                    SharedImageBacking* backing,
                                    MemoryTypeTracker* tracker)
      : SharedImageRepresentationDawn(manager, backing, tracker) {}

  WGPUTexture BeginAccess(WGPUTextureUsage usage) override {
    if (!static_cast<TestSharedImageBacking*>(backing())->can_access()) {
      return nullptr;
    }

    // Return a dummy value.
    return reinterpret_cast<WGPUTexture>(203);
  }

  void EndAccess() override {}
};

class TestSharedImageRepresentationOverlay
    : public SharedImageRepresentationOverlay {
 public:
  TestSharedImageRepresentationOverlay(SharedImageManager* manager,
                                       SharedImageBacking* backing,
                                       MemoryTypeTracker* tracker)
      : SharedImageRepresentationOverlay(manager, backing, tracker) {}

  bool BeginReadAccess(std::vector<gfx::GpuFence>* acquire_fences) override {
    return true;
  }
  void EndReadAccess(gfx::GpuFenceHandle release_fence) override {}
  gl::GLImage* GetGLImage() override { return nullptr; }

#if defined(OS_ANDROID)
  void NotifyOverlayPromotion(bool promotion,
                              const gfx::Rect& bounds) override {}
#endif
};

}  // namespace

TestSharedImageBacking::TestSharedImageBacking(
    const Mailbox& mailbox,
    viz::ResourceFormat format,
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
                         false /* is_thread_safe */),
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

TestSharedImageBacking::TestSharedImageBacking(
    const Mailbox& mailbox,
    viz::ResourceFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage,
    size_t estimated_size)
    : TestSharedImageBacking(mailbox,
                             format,
                             size,
                             color_space,
                             surface_origin,
                             alpha_type,
                             usage,
                             estimated_size,
                             203 /* texture_id */) {
  // Using a dummy |texture_id|, so lose our context so we don't do anything
  // real with it.
  OnContextLost();
}

TestSharedImageBacking::~TestSharedImageBacking() {
  // Pretend our context is lost to avoid actual cleanup in |texture_| or
  // |passthrough_texture_|.
  texture_->RemoveLightweightRef(false /* have_context */);
  texture_passthrough_->MarkContextLost();
  texture_passthrough_.reset();

  if (have_context())
    glDeleteTextures(1, &service_id_);
}

gfx::Rect TestSharedImageBacking::ClearedRect() const {
  return texture_->GetLevelClearedRect(texture_->target(), 0);
}

void TestSharedImageBacking::SetClearedRect(const gfx::Rect& cleared_rect) {
  texture_->SetLevelClearedRect(texture_->target(), 0, cleared_rect);
}

bool TestSharedImageBacking::ProduceLegacyMailbox(
    MailboxManager* mailbox_manager) {
  return false;
}

std::unique_ptr<SharedImageRepresentationGLTexture>
TestSharedImageBacking::ProduceGLTexture(SharedImageManager* manager,
                                         MemoryTypeTracker* tracker) {
  return std::make_unique<TestSharedImageRepresentationGLTexture>(
      manager, this, tracker, texture_);
}

std::unique_ptr<SharedImageRepresentationGLTexturePassthrough>
TestSharedImageBacking::ProduceGLTexturePassthrough(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker) {
  return std::make_unique<TestSharedImageRepresentationGLTexturePassthrough>(
      manager, this, tracker, texture_passthrough_);
}

std::unique_ptr<SharedImageRepresentationSkia>
TestSharedImageBacking::ProduceSkia(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker,
    scoped_refptr<SharedContextState> context_state) {
  return std::make_unique<TestSharedImageRepresentationSkia>(manager, this,
                                                             tracker);
}

std::unique_ptr<SharedImageRepresentationDawn>
TestSharedImageBacking::ProduceDawn(SharedImageManager* manager,
                                    MemoryTypeTracker* tracker,
                                    WGPUDevice device) {
  return std::make_unique<TestSharedImageRepresentationDawn>(manager, this,
                                                             tracker);
}

std::unique_ptr<SharedImageRepresentationOverlay>
TestSharedImageBacking::ProduceOverlay(SharedImageManager* manager,
                                       MemoryTypeTracker* tracker) {
  return std::make_unique<TestSharedImageRepresentationOverlay>(manager, this,
                                                                tracker);
}

}  // namespace gpu
