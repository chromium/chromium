// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image_backing_factory_gl_texture.h"

#include <algorithm>
#include <string>
#include <utility>

#include "base/feature_list.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/trace_event.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "gpu/command_buffer/common/gles2_cmd_utils.h"
#include "gpu/command_buffer/common/shared_image_trace_utils.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "gpu/command_buffer/service/image_factory.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "gpu/command_buffer/service/service_utils.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image_backing.h"
#include "gpu/command_buffer/service/shared_image_representation.h"
#include "gpu/command_buffer/service/skia_utils.h"
#include "gpu/config/gpu_finch_features.h"
#include "gpu/config/gpu_preferences.h"
#include "third_party/skia/include/core/SkPromiseImageTexture.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gl/buffer_format_utils.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_fence.h"
#include "ui/gl/gl_gl_api_implementation.h"
#include "ui/gl/gl_image_shared_memory.h"
#include "ui/gl/gl_version_info.h"
#include "ui/gl/trace_util.h"

namespace gpu {

namespace {

using UnpackStateAttribs =
    SharedImageBackingFactoryGLTexture::UnpackStateAttribs;

class ScopedResetAndRestoreUnpackState {
 public:
  ScopedResetAndRestoreUnpackState(gl::GLApi* api,
                                   const UnpackStateAttribs& attribs,
                                   bool uploading_data)
      : api_(api) {
    if (attribs.es3_capable) {
      // Need to unbind any GL_PIXEL_UNPACK_BUFFER for the nullptr in
      // glTexImage2D to mean "no pixels" (as opposed to offset 0 in the
      // buffer).
      api_->glGetIntegervFn(GL_PIXEL_UNPACK_BUFFER_BINDING, &unpack_buffer_);
      if (unpack_buffer_)
        api_->glBindBufferFn(GL_PIXEL_UNPACK_BUFFER, 0);
    }
    if (uploading_data) {
      api_->glGetIntegervFn(GL_UNPACK_ALIGNMENT, &unpack_alignment_);
      if (unpack_alignment_ != 4)
        api_->glPixelStoreiFn(GL_UNPACK_ALIGNMENT, 4);

      if (attribs.es3_capable || attribs.supports_unpack_subimage) {
        api_->glGetIntegervFn(GL_UNPACK_ROW_LENGTH, &unpack_row_length_);
        if (unpack_row_length_)
          api_->glPixelStoreiFn(GL_UNPACK_ROW_LENGTH, 0);
        api_->glGetIntegervFn(GL_UNPACK_SKIP_ROWS, &unpack_skip_rows_);
        if (unpack_skip_rows_)
          api_->glPixelStoreiFn(GL_UNPACK_SKIP_ROWS, 0);
        api_->glGetIntegervFn(GL_UNPACK_SKIP_PIXELS, &unpack_skip_pixels_);
        if (unpack_skip_pixels_)
          api_->glPixelStoreiFn(GL_UNPACK_SKIP_PIXELS, 0);
      }

      if (attribs.es3_capable) {
        api_->glGetIntegervFn(GL_UNPACK_SKIP_IMAGES, &unpack_skip_images_);
        if (unpack_skip_images_)
          api_->glPixelStoreiFn(GL_UNPACK_SKIP_IMAGES, 0);
        api_->glGetIntegervFn(GL_UNPACK_IMAGE_HEIGHT, &unpack_image_height_);
        if (unpack_image_height_)
          api_->glPixelStoreiFn(GL_UNPACK_IMAGE_HEIGHT, 0);
      }

      if (attribs.desktop_gl) {
        api->glGetBooleanvFn(GL_UNPACK_SWAP_BYTES, &unpack_swap_bytes_);
        if (unpack_swap_bytes_)
          api->glPixelStoreiFn(GL_UNPACK_SWAP_BYTES, GL_FALSE);
        api->glGetBooleanvFn(GL_UNPACK_LSB_FIRST, &unpack_lsb_first_);
        if (unpack_lsb_first_)
          api->glPixelStoreiFn(GL_UNPACK_LSB_FIRST, GL_FALSE);
      }
    }
  }

  ~ScopedResetAndRestoreUnpackState() {
    if (unpack_buffer_)
      api_->glBindBufferFn(GL_PIXEL_UNPACK_BUFFER, unpack_buffer_);
    if (unpack_alignment_ != 4)
      api_->glPixelStoreiFn(GL_UNPACK_ALIGNMENT, unpack_alignment_);
    if (unpack_row_length_)
      api_->glPixelStoreiFn(GL_UNPACK_ROW_LENGTH, unpack_row_length_);
    if (unpack_image_height_)
      api_->glPixelStoreiFn(GL_UNPACK_IMAGE_HEIGHT, unpack_image_height_);
    if (unpack_skip_rows_)
      api_->glPixelStoreiFn(GL_UNPACK_SKIP_ROWS, unpack_skip_rows_);
    if (unpack_skip_images_)
      api_->glPixelStoreiFn(GL_UNPACK_SKIP_IMAGES, unpack_skip_images_);
    if (unpack_skip_pixels_)
      api_->glPixelStoreiFn(GL_UNPACK_SKIP_PIXELS, unpack_skip_pixels_);
    if (unpack_swap_bytes_)
      api_->glPixelStoreiFn(GL_UNPACK_SWAP_BYTES, unpack_swap_bytes_);
    if (unpack_lsb_first_)
      api_->glPixelStoreiFn(GL_UNPACK_LSB_FIRST, unpack_lsb_first_);
  }

 private:
  gl::GLApi* const api_;

  // Always used if |es3_capable|.
  GLint unpack_buffer_ = 0;

  // Always used when |uploading_data|.
  GLint unpack_alignment_ = 4;

  // Used when |uploading_data_| and (|es3_capable| or
  // |supports_unpack_subimage|).
  GLint unpack_row_length_ = 0;
  GLint unpack_skip_pixels_ = 0;
  GLint unpack_skip_rows_ = 0;

  // Used when |uploading_data| and |es3_capable|.
  GLint unpack_skip_images_ = 0;
  GLint unpack_image_height_ = 0;

  // Used when |desktop_gl|.
  GLboolean unpack_swap_bytes_ = GL_FALSE;
  GLboolean unpack_lsb_first_ = GL_FALSE;

  DISALLOW_COPY_AND_ASSIGN(ScopedResetAndRestoreUnpackState);
};

class ScopedRestoreTexture {
 public:
  ScopedRestoreTexture(gl::GLApi* api, GLenum target)
      : api_(api), target_(target) {
    GLenum get_target = GL_TEXTURE_BINDING_2D;
    switch (target) {
      case GL_TEXTURE_2D:
        get_target = GL_TEXTURE_BINDING_2D;
        break;
      case GL_TEXTURE_RECTANGLE_ARB:
        get_target = GL_TEXTURE_BINDING_RECTANGLE_ARB;
        break;
      case GL_TEXTURE_EXTERNAL_OES:
        get_target = GL_TEXTURE_BINDING_EXTERNAL_OES;
        break;
      default:
        NOTREACHED();
        break;
    }
    GLint old_texture_binding = 0;
    api->glGetIntegervFn(get_target, &old_texture_binding);
    old_binding_ = old_texture_binding;
  }

  ~ScopedRestoreTexture() { api_->glBindTextureFn(target_, old_binding_); }

 private:
  gl::GLApi* api_;
  GLenum target_;
  GLuint old_binding_ = 0;
  DISALLOW_COPY_AND_ASSIGN(ScopedRestoreTexture);
};

GLuint MakeTextureAndSetParameters(gl::GLApi* api,
                                   GLenum target,
                                   bool framebuffer_attachment_angle) {
  GLuint service_id = 0;
  api->glGenTexturesFn(1, &service_id);
  api->glBindTextureFn(target, service_id);
  api->glTexParameteriFn(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  api->glTexParameteriFn(target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  api->glTexParameteriFn(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  api->glTexParameteriFn(target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  if (framebuffer_attachment_angle) {
    api->glTexParameteriFn(target, GL_TEXTURE_USAGE_ANGLE,
                           GL_FRAMEBUFFER_ATTACHMENT_ANGLE);
  }
  return service_id;
}

}  // anonymous namespace

// Representation of a SharedImageBackingGLTexture as a GL Texture.
class SharedImageRepresentationGLTextureImpl
    : public SharedImageRepresentationGLTexture {
 public:
  SharedImageRepresentationGLTextureImpl(SharedImageManager* manager,
                                         SharedImageBacking* backing,
                                         MemoryTypeTracker* tracker,
                                         gles2::Texture* texture)
      : SharedImageRepresentationGLTexture(manager, backing, tracker),
        texture_(texture) {}

  gles2::Texture* GetTexture() override { return texture_; }

 private:
  gles2::Texture* texture_;
};

// Representation of a SharedImageBackingGLTexturePassthrough as a GL
// TexturePassthrough.
class SharedImageRepresentationGLTexturePassthroughImpl
    : public SharedImageRepresentationGLTexturePassthrough {
 public:
  SharedImageRepresentationGLTexturePassthroughImpl(
      SharedImageManager* manager,
      SharedImageBacking* backing,
      MemoryTypeTracker* tracker,
      scoped_refptr<gles2::TexturePassthrough> texture_passthrough)
      : SharedImageRepresentationGLTexturePassthrough(manager,
                                                      backing,
                                                      tracker),
        texture_passthrough_(std::move(texture_passthrough)) {}

  const scoped_refptr<gles2::TexturePassthrough>& GetTexturePassthrough()
      override {
    return texture_passthrough_;
  }

 private:
  scoped_refptr<gles2::TexturePassthrough> texture_passthrough_;
};

class SharedImageBackingWithReadAccess : public SharedImageBacking {
 public:
  SharedImageBackingWithReadAccess(const Mailbox& mailbox,
                                   viz::ResourceFormat format,
                                   const gfx::Size& size,
                                   const gfx::ColorSpace& color_space,
                                   uint32_t usage,
                                   size_t estimated_size,
                                   bool is_thread_safe)
      : SharedImageBacking(mailbox,
                           format,
                           size,
                           color_space,
                           usage,
                           estimated_size,
                           is_thread_safe) {}
  ~SharedImageBackingWithReadAccess() override = default;

  virtual void BeginReadAccess() = 0;
};

class SharedImageRepresentationSkiaImpl : public SharedImageRepresentationSkia {
 public:
  SharedImageRepresentationSkiaImpl(
      SharedImageManager* manager,
      SharedImageBackingWithReadAccess* backing,
      scoped_refptr<SharedContextState> context_state,
      sk_sp<SkPromiseImageTexture> cached_promise_texture,
      MemoryTypeTracker* tracker,
      GLenum target,
      GLuint service_id)
      : SharedImageRepresentationSkia(manager, backing, tracker),
        context_state_(std::move(context_state)),
        promise_texture_(cached_promise_texture) {
    if (!promise_texture_) {
      GrBackendTexture backend_texture;
      GetGrBackendTexture(context_state_->feature_info(), target, size(),
                          service_id, format(), &backend_texture);
      promise_texture_ = SkPromiseImageTexture::Make(backend_texture);
    }
#if DCHECK_IS_ON()
    context_ = gl::GLContext::GetCurrent();
#endif
  }

  ~SharedImageRepresentationSkiaImpl() override {
    if (write_surface_) {
      DLOG(ERROR) << "SharedImageRepresentationSkia was destroyed while still "
                  << "open for write access.";
    }
  }

  sk_sp<SkSurface> BeginWriteAccess(
      int final_msaa_count,
      const SkSurfaceProps& surface_props,
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores) override {
    CheckContext();
    if (write_surface_)
      return nullptr;

    if (!promise_texture_) {
      return nullptr;
    }
    SkColorType sk_color_type = viz::ResourceFormatToClosestSkColorType(
        /*gpu_compositing=*/true, format());
    auto surface = SkSurface::MakeFromBackendTextureAsRenderTarget(
        context_state_->gr_context(), promise_texture_->backendTexture(),
        kTopLeft_GrSurfaceOrigin, final_msaa_count, sk_color_type,
        backing()->color_space().ToSkColorSpace(), &surface_props);
    write_surface_ = surface.get();
    return surface;
  }

  void EndWriteAccess(sk_sp<SkSurface> surface) override {
    DCHECK_EQ(surface.get(), write_surface_);
    DCHECK(surface->unique());
    CheckContext();
    // TODO(ericrk): Keep the surface around for re-use.
    write_surface_ = nullptr;
  }

  sk_sp<SkPromiseImageTexture> BeginReadAccess(
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores) override {
    CheckContext();
    static_cast<SharedImageBackingWithReadAccess*>(backing())
        ->BeginReadAccess();
    return promise_texture_;
  }

  void EndReadAccess() override {
    // TODO(ericrk): Handle begin/end correctness checks.
  }

  sk_sp<SkPromiseImageTexture> promise_texture() { return promise_texture_; }

 private:
  void CheckContext() {
#if DCHECK_IS_ON()
    DCHECK(gl::GLContext::GetCurrent() == context_);
#endif
  }

  scoped_refptr<SharedContextState> context_state_;
  sk_sp<SkPromiseImageTexture> promise_texture_;

  SkSurface* write_surface_ = nullptr;
#if DCHECK_IS_ON()
  gl::GLContext* context_;
#endif
};

// Implementation of SharedImageBacking that creates a GL Texture and stores it
// as a gles2::Texture. Can be used with the legacy mailbox implementation.
class SharedImageBackingGLTexture : public SharedImageBackingWithReadAccess {
 public:
  SharedImageBackingGLTexture(const Mailbox& mailbox,
                              viz::ResourceFormat format,
                              const gfx::Size& size,
                              const gfx::ColorSpace& color_space,
                              uint32_t usage,
                              gles2::Texture* texture,
                              const UnpackStateAttribs& attribs)
      : SharedImageBackingWithReadAccess(mailbox,
                                         format,
                                         size,
                                         color_space,
                                         usage,
                                         texture->estimated_size(),
                                         false /* is_thread_safe */),
        texture_(texture),
        attribs_(attribs) {
    DCHECK(texture_);
  }

  ~SharedImageBackingGLTexture() override {
    DCHECK(!texture_);
    DCHECK(!rgb_emulation_texture_);
  }

  bool IsCleared() const override {
    return texture_->IsLevelCleared(texture_->target(), 0);
  }

  void SetCleared() override {
    texture_->SetLevelCleared(texture_->target(), 0, true);
  }

  void Update(std::unique_ptr<gfx::GpuFence> in_fence) override {
    GLenum target = texture_->target();
    gl::GLApi* api = gl::g_current_gl_context;
    ScopedRestoreTexture scoped_restore(api, target);
    api->glBindTextureFn(target, texture_->service_id());

    gles2::Texture::ImageState old_state = gles2::Texture::UNBOUND;
    gl::GLImage* image = texture_->GetLevelImage(target, 0, &old_state);
    if (!image)
      return;
    if (old_state == gles2::Texture::BOUND)
      image->ReleaseTexImage(target);

    if (in_fence) {
      // TODO(dcastagna): Don't wait for the fence if the SharedImage is going
      // to be scanned out as an HW overlay. Currently we don't know that at
      // this point and we always bind the image, therefore we need to wait for
      // the fence.
      std::unique_ptr<gl::GLFence> egl_fence =
          gl::GLFence::CreateFromGpuFence(*in_fence.get());
      egl_fence->ServerWait();
    }
    gles2::Texture::ImageState new_state = gles2::Texture::UNBOUND;
    if (image->ShouldBindOrCopy() == gl::GLImage::BIND &&
        image->BindTexImage(target)) {
      new_state = gles2::Texture::BOUND;
    }
    if (old_state != new_state)
      texture_->SetLevelImage(target, 0, image, new_state);
  }

  bool ProduceLegacyMailbox(MailboxManager* mailbox_manager) override {
    DCHECK(texture_);
    mailbox_manager->ProduceTexture(mailbox(), texture_);
    return true;
  }

  void Destroy() override {
    DCHECK(texture_);
    texture_->RemoveLightweightRef(have_context());
    texture_ = nullptr;

    if (rgb_emulation_texture_) {
      rgb_emulation_texture_->RemoveLightweightRef(have_context());
      rgb_emulation_texture_ = nullptr;
    }
  }

  void OnMemoryDump(const std::string& dump_name,
                    base::trace_event::MemoryAllocatorDump* dump,
                    base::trace_event::ProcessMemoryDump* pmd,
                    uint64_t client_tracing_id) override {
    // Add a |service_guid| which expresses shared ownership between the
    // various GPU dumps.
    auto client_guid = GetSharedImageGUIDForTracing(mailbox());
    auto service_guid =
        gl::GetGLTextureServiceGUIDForTracing(texture_->service_id());
    pmd->CreateSharedGlobalAllocatorDump(service_guid);
    // TODO(piman): coalesce constant with TextureManager::DumpTextureRef.
    int importance = 2;  // This client always owns the ref.

    pmd->AddOwnershipEdge(client_guid, service_guid, importance);

    // Dump all sub-levels held by the texture. They will appear below the
    // main gl/textures/client_X/mailbox_Y dump.
    texture_->DumpLevelMemory(pmd, client_tracing_id, dump_name);
  }

  void BeginReadAccess() override {
    GLenum target = texture_->target();
    gles2::Texture::ImageState old_state = gles2::Texture::UNBOUND;
    gl::GLImage* image = texture_->GetLevelImage(target, 0, &old_state);
    if (image && old_state == gpu::gles2::Texture::UNBOUND) {
      gl::GLApi* api = gl::g_current_gl_context;
      ScopedRestoreTexture scoped_restore(api, target);
      api->glBindTextureFn(target, texture_->service_id());
      gles2::Texture::ImageState new_state = gles2::Texture::UNBOUND;
      if (image->ShouldBindOrCopy() == gl::GLImage::BIND) {
        if (image->BindTexImage(target))
          new_state = gles2::Texture::BOUND;
      } else {
        ScopedResetAndRestoreUnpackState scoped_unpack_state(api, attribs_,
                                                             /*upload=*/true);
        if (image->CopyTexImage(target))
          new_state = gles2::Texture::COPIED;
      }
      if (old_state != new_state)
        texture_->SetLevelImage(target, 0, image, new_state);
    }
  }

 protected:
  std::unique_ptr<SharedImageRepresentationGLTexture> ProduceGLTexture(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker) override {
    return std::make_unique<SharedImageRepresentationGLTextureImpl>(
        manager, this, tracker, texture_);
  }

  std::unique_ptr<SharedImageRepresentationGLTexture>
  ProduceRGBEmulationGLTexture(SharedImageManager* manager,
                               MemoryTypeTracker* tracker) override {
    if (!rgb_emulation_texture_) {
      GLenum target = texture_->target();
      gl::GLApi* api = gl::g_current_gl_context;
      ScopedRestoreTexture scoped_restore(api, target);

      // Set to false as this code path is only used on Mac.
      bool framebuffer_attachment_angle = false;
      GLuint service_id = MakeTextureAndSetParameters(
          api, target, framebuffer_attachment_angle);

      gles2::Texture::ImageState image_state = gles2::Texture::BOUND;
      gl::GLImage* image = texture_->GetLevelImage(target, 0, &image_state);
      if (!image) {
        LOG(ERROR) << "Texture is not bound to an image.";
        return nullptr;
      }

      DCHECK(image->ShouldBindOrCopy() == gl::GLImage::BIND);
      const GLenum internal_format = GL_RGB;
      if (!image->BindTexImageWithInternalformat(target, internal_format)) {
        LOG(ERROR) << "Failed to bind image to rgb texture.";
        api->glDeleteTexturesFn(1, &service_id);
        return nullptr;
      }

      rgb_emulation_texture_ = new gles2::Texture(service_id);
      rgb_emulation_texture_->SetLightweightRef();
      rgb_emulation_texture_->SetTarget(target, 1);
      rgb_emulation_texture_->sampler_state_.min_filter = GL_LINEAR;
      rgb_emulation_texture_->sampler_state_.mag_filter = GL_LINEAR;
      rgb_emulation_texture_->sampler_state_.wrap_s = GL_CLAMP_TO_EDGE;
      rgb_emulation_texture_->sampler_state_.wrap_t = GL_CLAMP_TO_EDGE;

      GLenum format = gles2::TextureManager::ExtractFormatFromStorageFormat(
          internal_format);
      GLenum type =
          gles2::TextureManager::ExtractTypeFromStorageFormat(internal_format);

      const gles2::Texture::LevelInfo* info = texture_->GetLevelInfo(target, 0);
      rgb_emulation_texture_->SetLevelInfo(target, 0, internal_format,
                                           info->width, info->height, 1, 0,
                                           format, type, info->cleared_rect);

      rgb_emulation_texture_->SetLevelImage(target, 0, image, image_state);
      rgb_emulation_texture_->SetImmutable(true, false);
    }

    return std::make_unique<SharedImageRepresentationGLTextureImpl>(
        manager, this, tracker, rgb_emulation_texture_);
  }

  std::unique_ptr<SharedImageRepresentationSkia> ProduceSkia(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker,
      scoped_refptr<SharedContextState> context_state) override {
    auto result = std::make_unique<SharedImageRepresentationSkiaImpl>(
        manager, this, std::move(context_state), cached_promise_texture_,
        tracker, texture_->target(), texture_->service_id());
    cached_promise_texture_ = result->promise_texture();
    return result;
  }

 private:
  gles2::Texture* texture_ = nullptr;
  gles2::Texture* rgb_emulation_texture_ = nullptr;
  sk_sp<SkPromiseImageTexture> cached_promise_texture_;
  const UnpackStateAttribs attribs_;
};

// Implementation of SharedImageBacking that creates a GL Texture and stores it
// as a gles2::TexturePassthrough. Can be used with the legacy mailbox
// implementation.
class SharedImageBackingPassthroughGLTexture
    : public SharedImageBackingWithReadAccess {
 public:
  SharedImageBackingPassthroughGLTexture(
      const Mailbox& mailbox,
      viz::ResourceFormat format,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      uint32_t usage,
      scoped_refptr<gles2::TexturePassthrough> passthrough_texture,
      bool is_cleared)
      : SharedImageBackingWithReadAccess(mailbox,
                                         format,
                                         size,
                                         color_space,
                                         usage,
                                         passthrough_texture->estimated_size(),
                                         false /* is_thread_safe */),
        texture_passthrough_(std::move(passthrough_texture)),
        is_cleared_(is_cleared) {
    DCHECK(texture_passthrough_);
  }

  ~SharedImageBackingPassthroughGLTexture() override {
    DCHECK(!texture_passthrough_);
  }

  bool IsCleared() const override { return is_cleared_; }
  void SetCleared() override { is_cleared_ = true; }

  void Update(std::unique_ptr<gfx::GpuFence> in_fence) override {
    GLenum target = texture_passthrough_->target();
    gl::GLApi* api = gl::g_current_gl_context;
    ScopedRestoreTexture scoped_restore(api, target);
    api->glBindTextureFn(target, texture_passthrough_->service_id());

    gl::GLImage* image = texture_passthrough_->GetLevelImage(target, 0);
    if (!image)
      return;
    image->ReleaseTexImage(target);
    if (image->ShouldBindOrCopy() == gl::GLImage::BIND)
      image->BindTexImage(target);
    else
      image->CopyTexImage(target);
  }

  bool ProduceLegacyMailbox(MailboxManager* mailbox_manager) override {
    DCHECK(texture_passthrough_);
    mailbox_manager->ProduceTexture(mailbox(), texture_passthrough_.get());
    return true;
  }

  void Destroy() override {
    DCHECK(texture_passthrough_);
    if (!have_context())
      texture_passthrough_->MarkContextLost();
    texture_passthrough_.reset();
  }

  void OnMemoryDump(const std::string& dump_name,
                    base::trace_event::MemoryAllocatorDump* dump,
                    base::trace_event::ProcessMemoryDump* pmd,
                    uint64_t client_tracing_id) override {
    // Add a |service_guid| which expresses shared ownership between the
    // various GPU dumps.
    auto client_guid = GetSharedImageGUIDForTracing(mailbox());
    auto service_guid = gl::GetGLTextureServiceGUIDForTracing(
        texture_passthrough_->service_id());
    pmd->CreateSharedGlobalAllocatorDump(service_guid);

    int importance = 2;  // This client always owns the ref.
    pmd->AddOwnershipEdge(client_guid, service_guid, importance);

    auto* gl_image = texture_passthrough_->GetLevelImage(
        texture_passthrough_->target(), /*level=*/0);
    if (gl_image)
      gl_image->OnMemoryDump(pmd, client_tracing_id, dump_name);
  }

  void BeginReadAccess() override {}

 protected:
  std::unique_ptr<SharedImageRepresentationGLTexturePassthrough>
  ProduceGLTexturePassthrough(SharedImageManager* manager,
                              MemoryTypeTracker* tracker) override {
    return std::make_unique<SharedImageRepresentationGLTexturePassthroughImpl>(
        manager, this, tracker, texture_passthrough_);
  }
  std::unique_ptr<SharedImageRepresentationSkia> ProduceSkia(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker,
      scoped_refptr<SharedContextState> context_state) override {
    auto result = std::make_unique<SharedImageRepresentationSkiaImpl>(
        manager, this, std::move(context_state), cached_promise_texture_,
        tracker, texture_passthrough_->target(),
        texture_passthrough_->service_id());
    cached_promise_texture_ = result->promise_texture();
    return result;
  }

 private:
  scoped_refptr<gles2::TexturePassthrough> texture_passthrough_;
  sk_sp<SkPromiseImageTexture> cached_promise_texture_;

  bool is_cleared_ = false;
};

SharedImageBackingFactoryGLTexture::SharedImageBackingFactoryGLTexture(
    const GpuPreferences& gpu_preferences,
    const GpuDriverBugWorkarounds& workarounds,
    const GpuFeatureInfo& gpu_feature_info,
    ImageFactory* image_factory)
    : use_passthrough_(gpu_preferences.use_passthrough_cmd_decoder &&
                       gles2::PassthroughCommandDecoderSupported()),
      image_factory_(image_factory) {
  gl::GLApi* api = gl::g_current_gl_context;
  api->glGetIntegervFn(GL_MAX_TEXTURE_SIZE, &max_texture_size_);
  // When the passthrough command decoder is used, the max_texture_size
  // workaround is implemented by ANGLE. Trying to adjust the max size here
  // would cause discrepency between what we think the max size is and what
  // ANGLE tells the clients.
  if (!use_passthrough_ && workarounds.max_texture_size) {
    max_texture_size_ =
        std::min(max_texture_size_, workarounds.max_texture_size);
  }
  // Ensure max_texture_size_ is less than INT_MAX so that gfx::Rect and friends
  // can be used to accurately represent all valid sub-rects, with overflow
  // cases, clamped to INT_MAX, always invalid.
  max_texture_size_ = std::min(max_texture_size_, INT_MAX - 1);

  // TODO(piman): Can we extract the logic out of FeatureInfo?
  scoped_refptr<gles2::FeatureInfo> feature_info =
      new gles2::FeatureInfo(workarounds, gpu_feature_info);
  feature_info->Initialize(ContextType::CONTEXT_TYPE_OPENGLES2,
                           use_passthrough_, gles2::DisallowedFeatures());
  gpu_memory_buffer_formats_ =
      feature_info->feature_flags().gpu_memory_buffer_formats;
  texture_usage_angle_ = feature_info->feature_flags().angle_texture_usage;
  attribs.es3_capable = feature_info->IsES3Capable();
  attribs.desktop_gl = !feature_info->gl_version_info().is_es;
  // Can't use the value from feature_info, as we unconditionally enable this
  // extension, and assume it can't be used if PBOs are not used (which isn't
  // true for Skia used direclty against GL).
  attribs.supports_unpack_subimage =
      gl::g_current_gl_driver->ext.b_GL_EXT_unpack_subimage;
  bool enable_texture_storage =
      feature_info->feature_flags().ext_texture_storage;
  bool enable_scanout_images =
      (image_factory_ && image_factory_->SupportsCreateAnonymousImage());
  const gles2::Validators* validators = feature_info->validators();
  for (int i = 0; i <= viz::RESOURCE_FORMAT_MAX; ++i) {
    auto format = static_cast<viz::ResourceFormat>(i);
    FormatInfo& info = format_info_[i];
    if (!viz::GLSupportsFormat(format))
      continue;
    GLuint image_internal_format = viz::GLInternalFormat(format);
    GLenum gl_format = viz::GLDataFormat(format);
    GLenum gl_type = viz::GLDataType(format);
    bool uncompressed_format_valid =
        validators->texture_internal_format.IsValid(image_internal_format) &&
        validators->texture_format.IsValid(gl_format);
    bool compressed_format_valid =
        validators->compressed_texture_format.IsValid(image_internal_format);
    if ((uncompressed_format_valid || compressed_format_valid) &&
        validators->pixel_type.IsValid(gl_type)) {
      info.enabled = true;
      info.is_compressed = compressed_format_valid;
      info.gl_format = gl_format;
      info.gl_type = gl_type;
      info.swizzle = gles2::TextureManager::GetCompatibilitySwizzle(
          feature_info.get(), gl_format);
      info.image_internal_format =
          gles2::TextureManager::AdjustTexInternalFormat(
              feature_info.get(), image_internal_format, gl_type);
      info.adjusted_format =
          gles2::TextureManager::AdjustTexFormat(feature_info.get(), gl_format);
    }
    if (!info.enabled)
      continue;
    if (enable_texture_storage && !info.is_compressed) {
      GLuint storage_internal_format = viz::TextureStorageFormat(format);
      if (validators->texture_internal_format_storage.IsValid(
              storage_internal_format)) {
        info.supports_storage = true;
        info.storage_internal_format =
            gles2::TextureManager::AdjustTexStorageFormat(
                feature_info.get(), storage_internal_format);
      }
    }
    if (!info.enabled || !enable_scanout_images ||
        !IsGpuMemoryBufferFormatSupported(format))
      continue;
    gfx::BufferFormat buffer_format = viz::BufferFormat(format);
    switch (buffer_format) {
      case gfx::BufferFormat::RGBA_8888:
      case gfx::BufferFormat::BGRA_8888:
      case gfx::BufferFormat::RGBA_F16:
      case gfx::BufferFormat::R_8:
        break;
      default:
        continue;
    }
    info.allow_scanout = true;
    info.buffer_format = buffer_format;
    DCHECK_EQ(info.gl_format,
              gl::BufferFormatToGLInternalFormat(buffer_format));
    if (base::Contains(gpu_preferences.texture_target_exception_list,
                       gfx::BufferUsageAndFormat(gfx::BufferUsage::SCANOUT,
                                                 buffer_format)))
      info.target_for_scanout = gpu::GetPlatformSpecificTextureTarget();
  }
}

SharedImageBackingFactoryGLTexture::~SharedImageBackingFactoryGLTexture() =
    default;

std::unique_ptr<SharedImageBacking>
SharedImageBackingFactoryGLTexture::CreateSharedImage(
    const Mailbox& mailbox,
    viz::ResourceFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    uint32_t usage,
    bool is_thread_safe) {
  DCHECK(!is_thread_safe);
  return CreateSharedImage(mailbox, format, size, color_space, usage,
                           base::span<const uint8_t>());
}

std::unique_ptr<SharedImageBacking>
SharedImageBackingFactoryGLTexture::CreateSharedImage(
    const Mailbox& mailbox,
    viz::ResourceFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    uint32_t usage,
    base::span<const uint8_t> pixel_data) {
  const FormatInfo& format_info = format_info_[format];
  if (!format_info.enabled) {
    LOG(ERROR) << "CreateSharedImage: invalid format";
    return nullptr;
  }

  const bool use_buffer = usage & SHARED_IMAGE_USAGE_SCANOUT;
  if (use_buffer && !format_info.allow_scanout) {
    LOG(ERROR) << "CreateSharedImage: SCANOUT shared images unavailable";
    return nullptr;
  }

  if (size.width() < 1 || size.height() < 1 ||
      size.width() > max_texture_size_ || size.height() > max_texture_size_) {
    LOG(ERROR) << "CreateSharedImage: invalid size";
    return nullptr;
  }

  GLenum target = use_buffer ? format_info.target_for_scanout : GL_TEXTURE_2D;

  // If we have initial data to upload, ensure it is sized appropriately.
  if (!pixel_data.empty()) {
    if (format_info.is_compressed) {
      const char* error_message = "unspecified";
      if (!gles2::ValidateCompressedTexDimensions(
              target, 0 /* level */, size.width(), size.height(), 1 /* depth */,
              format_info.image_internal_format, &error_message)) {
        LOG(ERROR) << "CreateSharedImage: "
                      "ValidateCompressedTexDimensionsFailed with error: "
                   << error_message;
        return nullptr;
      }

      GLsizei bytes_required = 0;
      if (!gles2::GetCompressedTexSizeInBytes(
              nullptr /* function_name */, size.width(), size.height(),
              1 /* depth */, format_info.image_internal_format, &bytes_required,
              nullptr /* error_state */)) {
        LOG(ERROR) << "CreateSharedImage: Unable to compute required size for "
                      "initial texture upload.";
        return nullptr;
      }

      if (bytes_required < 0 ||
          pixel_data.size() != static_cast<size_t>(bytes_required)) {
        LOG(ERROR) << "CreateSharedImage: Initial data does not have expected "
                      "size.";
        return nullptr;
      }
    } else {
      uint32_t bytes_required;
      if (!gles2::GLES2Util::ComputeImageDataSizes(
              size.width(), size.height(), 1 /* depth */, format_info.gl_format,
              format_info.gl_type, 4 /* alignment */, &bytes_required, nullptr,
              nullptr)) {
        LOG(ERROR) << "CreateSharedImage: Unable to compute required size for "
                      "initial texture upload.";
        return nullptr;
      }
      if (pixel_data.size() != bytes_required) {
        LOG(ERROR) << "CreateSharedImage: Initial data does not have expected "
                      "size.";
        return nullptr;
      }
    }
  }

  gl::GLApi* api = gl::g_current_gl_context;
  ScopedRestoreTexture scoped_restore(api, target);

  const bool for_framebuffer_attachment =
      (usage & (SHARED_IMAGE_USAGE_RASTER |
                SHARED_IMAGE_USAGE_GLES2_FRAMEBUFFER_HINT)) != 0;
  GLuint service_id = MakeTextureAndSetParameters(
      api, target, for_framebuffer_attachment && texture_usage_angle_);

  scoped_refptr<gl::GLImage> image;
  // TODO(piman): We pretend the texture was created in an ES2 context, so that
  // it can be used in other ES2 contexts, and so we have to pass gl_format as
  // the internal format in the LevelInfo. https://crbug.com/628064
  GLuint level_info_internal_format = format_info.gl_format;
  bool is_cleared = false;
  bool needs_subimage_upload = false;
  bool has_immutable_storage = false;
  if (use_buffer) {
    image = image_factory_->CreateAnonymousImage(
        size, format_info.buffer_format, gfx::BufferUsage::SCANOUT,
        &is_cleared);
    // A SCANOUT image should not require copy.
    DCHECK(!image || image->ShouldBindOrCopy() == gl::GLImage::BIND);
    if (!image || !image->BindTexImage(target)) {
      LOG(ERROR) << "CreateSharedImage: Failed to "
                 << (image ? "bind" : "create") << " image";
      api->glDeleteTexturesFn(1, &service_id);
      return nullptr;
    }
    level_info_internal_format = image->GetInternalFormat();
    if (color_space.IsValid())
      image->SetColorSpace(color_space);
    needs_subimage_upload = !pixel_data.empty();
  } else if (format_info.supports_storage) {
    api->glTexStorage2DEXTFn(target, 1, format_info.storage_internal_format,
                             size.width(), size.height());
    has_immutable_storage = true;
    needs_subimage_upload = !pixel_data.empty();
  } else if (format_info.is_compressed) {
    ScopedResetAndRestoreUnpackState scoped_unpack_state(api, attribs,
                                                         !pixel_data.empty());
    api->glCompressedTexImage2DFn(target, 0, format_info.image_internal_format,
                                  size.width(), size.height(), 0,
                                  pixel_data.size(), pixel_data.data());
  } else {
    ScopedResetAndRestoreUnpackState scoped_unpack_state(api, attribs,
                                                         !pixel_data.empty());
    api->glTexImage2DFn(target, 0, format_info.image_internal_format,
                        size.width(), size.height(), 0,
                        format_info.adjusted_format, format_info.gl_type,
                        pixel_data.data());
  }

  // If we are using a buffer or TexStorage API but have data to upload, do so
  // now via TexSubImage2D.
  if (needs_subimage_upload) {
    ScopedResetAndRestoreUnpackState scoped_unpack_state(api, attribs,
                                                         !pixel_data.empty());
    api->glTexSubImage2DFn(target, 0, 0, 0, size.width(), size.height(),
                           format_info.adjusted_format, format_info.gl_type,
                           pixel_data.data());
  }

  return MakeBacking(
      use_passthrough_, mailbox, target, service_id, image,
      gles2::Texture::BOUND, level_info_internal_format, format_info.gl_format,
      format_info.gl_type, format_info.swizzle,
      pixel_data.empty() ? is_cleared : true, has_immutable_storage, format,
      size, color_space, usage, attribs);
}

std::unique_ptr<SharedImageBacking>
SharedImageBackingFactoryGLTexture::CreateSharedImage(
    const Mailbox& mailbox,
    int client_id,
    gfx::GpuMemoryBufferHandle handle,
    gfx::BufferFormat buffer_format,
    SurfaceHandle surface_handle,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    uint32_t usage) {
  if (!gpu_memory_buffer_formats_.Has(buffer_format)) {
    LOG(ERROR) << "CreateSharedImage: unsupported buffer format "
               << gfx::BufferFormatToString(buffer_format);
    return nullptr;
  }

  if (!gpu::IsImageSizeValidForGpuMemoryBufferFormat(size, buffer_format)) {
    LOG(ERROR) << "Invalid image size " << size.ToString() << " for "
               << gfx::BufferFormatToString(buffer_format);
    return nullptr;
  }

  GLenum target =
      (handle.type == gfx::SHARED_MEMORY_BUFFER ||
       !NativeBufferNeedsPlatformSpecificTextureTarget(buffer_format))
          ? GL_TEXTURE_2D
          : gpu::GetPlatformSpecificTextureTarget();
  scoped_refptr<gl::GLImage> image = MakeGLImage(
      client_id, std::move(handle), buffer_format, surface_handle, size);
  if (!image) {
    LOG(ERROR) << "Failed to create image.";
    return nullptr;
  }
  // If we decide to use GL_TEXTURE_2D at the target for a native buffer, we
  // would like to verify that it will actually work. If the image expects to be
  // copied, there is no way to do this verification here, because copying is
  // done lazily after the SharedImage is created, so require that the image is
  // bindable. Currently NativeBufferNeedsPlatformSpecificTextureTarget can
  // only return false on Chrome OS where GLImageNativePixmap is used which is
  // always bindable.
  DCHECK(handle.type == gfx::SHARED_MEMORY_BUFFER || target != GL_TEXTURE_2D ||
         image->ShouldBindOrCopy() == gl::GLImage::BIND);
  if (color_space.IsValid())
    image->SetColorSpace(color_space);

  viz::ResourceFormat format = viz::GetResourceFormat(buffer_format);

  gl::GLApi* api = gl::g_current_gl_context;
  ScopedRestoreTexture scoped_restore(api, target);

  const bool for_framebuffer_attachment =
      (usage & (SHARED_IMAGE_USAGE_RASTER |
                SHARED_IMAGE_USAGE_GLES2_FRAMEBUFFER_HINT)) != 0;
  GLuint service_id = MakeTextureAndSetParameters(
      api, target, for_framebuffer_attachment && texture_usage_angle_);
  bool is_rgb_emulation = usage & SHARED_IMAGE_USAGE_RGB_EMULATION;

  gles2::Texture::ImageState image_state = gles2::Texture::UNBOUND;
  if (image->ShouldBindOrCopy() == gl::GLImage::BIND) {
    bool is_bound = false;
    if (is_rgb_emulation)
      is_bound = image->BindTexImageWithInternalformat(target, GL_RGB);
    else
      is_bound = image->BindTexImage(target);
    if (is_bound) {
      image_state = gles2::Texture::BOUND;
    } else {
      LOG(ERROR) << "Failed to bind image to target.";
      api->glDeleteTexturesFn(1, &service_id);
      return nullptr;
    }
  } else if (use_passthrough_) {
    image->CopyTexImage(target);
    image_state = gles2::Texture::COPIED;
  }

  GLuint internal_format =
      is_rgb_emulation ? GL_RGB : image->GetInternalFormat();
  GLenum gl_format = is_rgb_emulation ? GL_RGB : image->GetDataFormat();
  GLenum gl_type = image->GetDataType();

  return MakeBacking(use_passthrough_, mailbox, target, service_id, image,
                     image_state, internal_format, gl_format, gl_type, nullptr,
                     true, false, format, size, color_space, usage, attribs);
}

std::unique_ptr<SharedImageBacking>
SharedImageBackingFactoryGLTexture::CreateSharedImageForTest(
    const Mailbox& mailbox,
    GLenum target,
    GLuint service_id,
    bool is_cleared,
    viz::ResourceFormat format,
    const gfx::Size& size,
    uint32_t usage) {
  return MakeBacking(false, mailbox, target, service_id, nullptr,
                     gles2::Texture::UNBOUND, viz::GLInternalFormat(format),
                     viz::GLDataFormat(format), viz::GLDataType(format),
                     nullptr, is_cleared, false, format, size,
                     gfx::ColorSpace(), usage, UnpackStateAttribs());
}

scoped_refptr<gl::GLImage> SharedImageBackingFactoryGLTexture::MakeGLImage(
    int client_id,
    gfx::GpuMemoryBufferHandle handle,
    gfx::BufferFormat format,
    SurfaceHandle surface_handle,
    const gfx::Size& size) {
  if (handle.type == gfx::SHARED_MEMORY_BUFFER) {
    if (!base::IsValueInRangeForNumericType<size_t>(handle.stride))
      return nullptr;
    auto image = base::MakeRefCounted<gl::GLImageSharedMemory>(size);
    if (!image->Initialize(handle.region, handle.id, format, handle.offset,
                           handle.stride)) {
      return nullptr;
    }

    return image;
  }

  if (!image_factory_)
    return nullptr;

  return image_factory_->CreateImageForGpuMemoryBuffer(
      std::move(handle), size, format, client_id, surface_handle);
}

bool SharedImageBackingFactoryGLTexture::CanImportGpuMemoryBuffer(
    gfx::GpuMemoryBufferType memory_buffer_type) {
  // SharedImageFactory may call CanImportGpuMemoryBuffer() in all other
  // SharedImageBackingFactory implementations except this one.
  NOTREACHED();
  return true;
}

std::unique_ptr<SharedImageBacking>
SharedImageBackingFactoryGLTexture::MakeBacking(
    bool passthrough,
    const Mailbox& mailbox,
    GLenum target,
    GLuint service_id,
    scoped_refptr<gl::GLImage> image,
    gles2::Texture::ImageState image_state,
    GLuint level_info_internal_format,
    GLuint gl_format,
    GLuint gl_type,
    const gles2::Texture::CompatibilitySwizzle* swizzle,
    bool is_cleared,
    bool has_immutable_storage,
    viz::ResourceFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    uint32_t usage,
    const UnpackStateAttribs& attribs) {
  if (passthrough) {
    scoped_refptr<gles2::TexturePassthrough> passthrough_texture =
        base::MakeRefCounted<gles2::TexturePassthrough>(service_id, target);
    if (image) {
      passthrough_texture->SetLevelImage(target, 0, image.get());
      passthrough_texture->set_is_bind_pending(image_state ==
                                               gles2::Texture::UNBOUND);
    }

    // Get the texture size from ANGLE and set it on the passthrough texture.
    GLint texture_memory_size = 0;
    gl::GLApi* api = gl::g_current_gl_context;
    api->glGetTexParameterivFn(target, GL_MEMORY_SIZE_ANGLE,
                               &texture_memory_size);
    passthrough_texture->SetEstimatedSize(texture_memory_size);

    return std::make_unique<SharedImageBackingPassthroughGLTexture>(
        mailbox, format, size, color_space, usage,
        std::move(passthrough_texture), is_cleared);
  } else {
    gles2::Texture* texture = new gles2::Texture(service_id);
    texture->SetLightweightRef();
    texture->SetTarget(target, 1);
    texture->sampler_state_.min_filter = GL_LINEAR;
    texture->sampler_state_.mag_filter = GL_LINEAR;
    texture->sampler_state_.wrap_s = GL_CLAMP_TO_EDGE;
    texture->sampler_state_.wrap_t = GL_CLAMP_TO_EDGE;
    texture->SetLevelInfo(target, 0, level_info_internal_format, size.width(),
                          size.height(), 1, 0, gl_format, gl_type,
                          is_cleared ? gfx::Rect(size) : gfx::Rect());
    if (swizzle)
      texture->SetCompatibilitySwizzle(swizzle);
    if (image)
      texture->SetLevelImage(target, 0, image.get(), image_state);
    texture->SetImmutable(true, has_immutable_storage);

    return std::make_unique<SharedImageBackingGLTexture>(
        mailbox, format, size, color_space, usage, texture, attribs);
  }
}

SharedImageBackingFactoryGLTexture::FormatInfo::FormatInfo() = default;
SharedImageBackingFactoryGLTexture::FormatInfo::~FormatInfo() = default;

}  // namespace gpu
