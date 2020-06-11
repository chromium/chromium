// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image_backing_factory_gl_texture.h"

#include <algorithm>
#include <list>
#include <string>
#include <utility>

#include "base/feature_list.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "components/viz/common/resources/resource_sizes.h"
#include "gpu/command_buffer/common/gles2_cmd_utils.h"
#include "gpu/command_buffer/common/shared_image_trace_utils.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/context_state.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "gpu/command_buffer/service/image_factory.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "gpu/command_buffer/service/service_utils.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image_backing.h"
#include "gpu/command_buffer/service/shared_image_backing_factory_gl_texture_internal.h"
#include "gpu/command_buffer/service/shared_image_factory.h"
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
#include "ui/gl/gl_image_native_pixmap.h"
#include "ui/gl/gl_image_shared_memory.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_version_info.h"
#include "ui/gl/scoped_binders.h"
#include "ui/gl/shared_gl_fence_egl.h"
#include "ui/gl/trace_util.h"

#if defined(OS_ANDROID)
#include "gpu/command_buffer/service/shared_image_backing_egl_image.h"
#include "gpu/command_buffer/service/shared_image_batch_access_manager.h"
#endif

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

std::unique_ptr<SharedImageRepresentationDawn> ProduceDawnCommon(
    SharedImageFactory* factory,
    SharedImageManager* manager,
    MemoryTypeTracker* tracker,
    WGPUDevice device,
    SharedImageBacking* backing,
    bool use_passthrough) {
  DCHECK(factory);
  // Make SharedContextState from factory the current context
  SharedContextState* shared_context_state = factory->GetSharedContextState();
  if (!shared_context_state->MakeCurrent(nullptr, true)) {
    DLOG(ERROR) << "Cannot make util SharedContextState the current context";
    return nullptr;
  }

  Mailbox dst_mailbox = Mailbox::GenerateForSharedImage();

  bool success = factory->CreateSharedImage(
      dst_mailbox, backing->format(), backing->size(), backing->color_space(),
      gpu::kNullSurfaceHandle, backing->usage() | SHARED_IMAGE_USAGE_WEBGPU);
  if (!success) {
    DLOG(ERROR) << "Cannot create a shared image resource for internal blit";
    return nullptr;
  }

  // Create a representation for current backing to avoid non-expected release
  // and using scope access methods.
  std::unique_ptr<SharedImageRepresentationGLTextureBase> src_image;
  std::unique_ptr<SharedImageRepresentationGLTextureBase> dst_image;
  if (use_passthrough) {
    src_image =
        manager->ProduceGLTexturePassthrough(backing->mailbox(), tracker);
    dst_image = manager->ProduceGLTexturePassthrough(dst_mailbox, tracker);
  } else {
    src_image = manager->ProduceGLTexture(backing->mailbox(), tracker);
    dst_image = manager->ProduceGLTexture(dst_mailbox, tracker);
  }

  if (!src_image || !dst_image) {
    DLOG(ERROR) << "ProduceDawn: Couldn't produce shared image for copy";
    return nullptr;
  }

  std::unique_ptr<SharedImageRepresentationGLTextureBase::ScopedAccess>
      source_access = src_image->BeginScopedAccess(
          GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM,
          SharedImageRepresentation::AllowUnclearedAccess::kNo);
  if (!source_access) {
    DLOG(ERROR) << "ProduceDawn: Couldn't access shared image for copy.";
    return nullptr;
  }

  std::unique_ptr<SharedImageRepresentationGLTextureBase::ScopedAccess>
      dest_access = dst_image->BeginScopedAccess(
          GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM,
          SharedImageRepresentation::AllowUnclearedAccess::kYes);
  if (!dest_access) {
    DLOG(ERROR) << "ProduceDawn: Couldn't access shared image for copy.";
    return nullptr;
  }

  GLuint source_texture = src_image->GetTextureBase()->service_id();
  GLuint dest_texture = dst_image->GetTextureBase()->service_id();
  DCHECK_NE(source_texture, dest_texture);

  GLenum target = dst_image->GetTextureBase()->target();

  // Ensure skia's internal cache of GL context state is reset before using it.
  // TODO(crbug.com/1036142: Figure out cases that need this invocation).
  shared_context_state->PessimisticallyResetGrContext();

  if (use_passthrough) {
    gl::GLApi* gl = shared_context_state->context_state()->api();

    gl->glCopyTextureCHROMIUMFn(source_texture, 0, target, dest_texture, 0,
                                viz::GLDataFormat(backing->format()),
                                viz::GLDataType(backing->format()), false,
                                false, false);
  } else {
    // TODO(crbug.com/1036142: Implement copyTextureCHROMIUM for validating
    // path).
    NOTREACHED();
    return nullptr;
  }

  // Set cleared flag for internal backing to prevent auto clear.
  dst_image->SetCleared();

  // Safe to destroy factory's ref. The backing is kept alive by GL
  // representation ref.
  factory->DestroySharedImage(dst_mailbox);

  return manager->ProduceDawn(dst_mailbox, tracker, device);
}

}  // anonymous namespace

///////////////////////////////////////////////////////////////////////////////
// SharedImageRepresentationGLTextureImpl

// Representation of a SharedImageBackingGLTexture as a GL Texture.
SharedImageRepresentationGLTextureImpl::SharedImageRepresentationGLTextureImpl(
    SharedImageManager* manager,
    SharedImageBacking* backing,
    MemoryTypeTracker* tracker,
    gles2::Texture* texture)
    : SharedImageRepresentationGLTexture(manager, backing, tracker),
      texture_(texture) {}

gles2::Texture* SharedImageRepresentationGLTextureImpl::GetTexture() {
  return texture_;
}

///////////////////////////////////////////////////////////////////////////////
// SharedImageRepresentationGLTexturePassthroughImpl

SharedImageRepresentationGLTexturePassthroughImpl::
    SharedImageRepresentationGLTexturePassthroughImpl(
        SharedImageManager* manager,
        SharedImageBacking* backing,
        MemoryTypeTracker* tracker,
        scoped_refptr<gles2::TexturePassthrough> texture_passthrough)
    : SharedImageRepresentationGLTexturePassthrough(manager, backing, tracker),
      texture_passthrough_(std::move(texture_passthrough)) {}

SharedImageRepresentationGLTexturePassthroughImpl::
    ~SharedImageRepresentationGLTexturePassthroughImpl() = default;

const scoped_refptr<gles2::TexturePassthrough>&
SharedImageRepresentationGLTexturePassthroughImpl::GetTexturePassthrough() {
  return texture_passthrough_;
}

void SharedImageRepresentationGLTexturePassthroughImpl::EndAccess() {
  GLenum target = texture_passthrough_->target();
  gl::GLImage* image = texture_passthrough_->GetLevelImage(target, 0);
  if (!image)
    return;
  if (image->ShouldBindOrCopy() == gl::GLImage::BIND) {
    gl::ScopedTextureBinder binder(target, texture_passthrough_->service_id());
    image->ReleaseTexImage(target);
    image->BindTexImage(target);
  }
}

///////////////////////////////////////////////////////////////////////////////
// SharedImageBackingWithReadAccess

SharedImageBackingWithReadAccess::SharedImageBackingWithReadAccess(
    const Mailbox& mailbox,
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

SharedImageBackingWithReadAccess::~SharedImageBackingWithReadAccess() = default;

///////////////////////////////////////////////////////////////////////////////
// SharedImageRepresentationSkiaImpl

SharedImageRepresentationSkiaImpl::SharedImageRepresentationSkiaImpl(
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

SharedImageRepresentationSkiaImpl::~SharedImageRepresentationSkiaImpl() {
  if (write_surface_) {
    DLOG(ERROR) << "SharedImageRepresentationSkia was destroyed while still "
                << "open for write access.";
  }
}

sk_sp<SkSurface> SharedImageRepresentationSkiaImpl::BeginWriteAccess(
    int final_msaa_count,
    const SkSurfaceProps& surface_props,
    std::vector<GrBackendSemaphore>* begin_semaphores,
    std::vector<GrBackendSemaphore>* end_semaphores) {
  CheckContext();
  if (write_surface_)
    return nullptr;

  if (!promise_texture_) {
    return nullptr;
  }
  SkColorType sk_color_type = viz::ResourceFormatToClosestSkColorType(
      /*gpu_compositing=*/true, format());
  auto surface = SkSurface::MakeFromBackendTexture(
      context_state_->gr_context(), promise_texture_->backendTexture(),
      kTopLeft_GrSurfaceOrigin, final_msaa_count, sk_color_type,
      backing()->color_space().ToSkColorSpace(), &surface_props);
  write_surface_ = surface.get();
  return surface;
}

void SharedImageRepresentationSkiaImpl::EndWriteAccess(
    sk_sp<SkSurface> surface) {
  DCHECK_EQ(surface.get(), write_surface_);
  DCHECK(surface->unique());
  CheckContext();
  // TODO(ericrk): Keep the surface around for re-use.
  write_surface_ = nullptr;
}

sk_sp<SkPromiseImageTexture> SharedImageRepresentationSkiaImpl::BeginReadAccess(
    std::vector<GrBackendSemaphore>* begin_semaphores,
    std::vector<GrBackendSemaphore>* end_semaphores) {
  CheckContext();
  static_cast<SharedImageBackingWithReadAccess*>(backing())->BeginReadAccess();
  return promise_texture_;
}

void SharedImageRepresentationSkiaImpl::EndReadAccess() {
  // TODO(ericrk): Handle begin/end correctness checks.
}

bool SharedImageRepresentationSkiaImpl::SupportsMultipleConcurrentReadAccess() {
  return true;
}

sk_sp<SkPromiseImageTexture>
SharedImageRepresentationSkiaImpl::promise_texture() {
  return promise_texture_;
}

void SharedImageRepresentationSkiaImpl::CheckContext() {
#if DCHECK_IS_ON()
    DCHECK(gl::GLContext::GetCurrent() == context_);
#endif
}

///////////////////////////////////////////////////////////////////////////////
// SharedImageBackingGLTexture

SharedImageBackingGLTexture::SharedImageBackingGLTexture(
    const Mailbox& mailbox,
    viz::ResourceFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    uint32_t usage,
    gles2::Texture* texture,
    scoped_refptr<gles2::TexturePassthrough> passthrough_texture)
    : SharedImageBackingWithReadAccess(
          mailbox,
          format,
          size,
          color_space,
          usage,
          texture ? texture->estimated_size()
                  : passthrough_texture->estimated_size(),
          false /* is_thread_safe */),
      texture_(texture),
      passthrough_texture_(passthrough_texture) {
  if (IsPassthrough()) {
    DCHECK(passthrough_texture_ && !texture_);
    DCHECK(!passthrough_texture_->GetLevelImage(passthrough_texture_->target(),
                                                0));
  } else {
    DCHECK(texture_ && !passthrough_texture_);
    DCHECK(!texture_->GetLevelImage(texture_->target(), 0, nullptr));
  }
}

SharedImageBackingGLTexture::~SharedImageBackingGLTexture() {
  if (IsPassthrough()) {
    if (!have_context())
      passthrough_texture_->MarkContextLost();
    passthrough_texture_.reset();
  } else {
    DCHECK(texture_);
    texture_->RemoveLightweightRef(have_context());
    texture_ = nullptr;
  }
}

gfx::Rect SharedImageBackingGLTexture::ClearedRect() const {
  if (IsPassthrough())
    return gfx::Rect(size());
  else
    return texture_->GetLevelClearedRect(texture_->target(), 0);
}

void SharedImageBackingGLTexture::SetClearedRect(
    const gfx::Rect& cleared_rect) {
  if (!IsPassthrough())
    texture_->SetLevelClearedRect(texture_->target(), 0, cleared_rect);
}

void SharedImageBackingGLTexture::Update(
    std::unique_ptr<gfx::GpuFence> in_fence) {}

bool SharedImageBackingGLTexture::ProduceLegacyMailbox(
    MailboxManager* mailbox_manager) {
  if (IsPassthrough())
    mailbox_manager->ProduceTexture(mailbox(), passthrough_texture_.get());
  else
    mailbox_manager->ProduceTexture(mailbox(), texture_);
  return true;
}

void SharedImageBackingGLTexture::OnMemoryDump(
    const std::string& dump_name,
    base::trace_event::MemoryAllocatorDump* dump,
    base::trace_event::ProcessMemoryDump* pmd,
    uint64_t client_tracing_id) {
  const auto client_guid = GetSharedImageGUIDForTracing(mailbox());
  if (!IsPassthrough()) {
    const auto service_guid =
        gl::GetGLTextureServiceGUIDForTracing(texture_->service_id());
    pmd->CreateSharedGlobalAllocatorDump(service_guid);
    pmd->AddOwnershipEdge(client_guid, service_guid, /* importance */ 2);
    texture_->DumpLevelMemory(pmd, client_tracing_id, dump_name);
  }
}

std::unique_ptr<SharedImageRepresentationGLTexture>
SharedImageBackingGLTexture::ProduceGLTexture(SharedImageManager* manager,
                                              MemoryTypeTracker* tracker) {
  DCHECK(texture_);
  return std::make_unique<SharedImageRepresentationGLTextureImpl>(
      manager, this, tracker, texture_);
}

std::unique_ptr<SharedImageRepresentationGLTexturePassthrough>
SharedImageBackingGLTexture::ProduceGLTexturePassthrough(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker) {
  DCHECK(passthrough_texture_);
  return std::make_unique<SharedImageRepresentationGLTexturePassthroughImpl>(
      manager, this, tracker, passthrough_texture_);
}

std::unique_ptr<SharedImageRepresentationSkia>
SharedImageBackingGLTexture::ProduceSkia(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker,
    scoped_refptr<SharedContextState> context_state) {
  std::unique_ptr<SharedImageRepresentationSkiaImpl> result;
  if (IsPassthrough()) {
    result = std::make_unique<SharedImageRepresentationSkiaImpl>(
        manager, this, std::move(context_state), cached_promise_texture_,
        tracker, passthrough_texture_->target(),
        passthrough_texture_->service_id());
  } else {
    result = std::make_unique<SharedImageRepresentationSkiaImpl>(
        manager, this, std::move(context_state), cached_promise_texture_,
        tracker, texture_->target(), texture_->service_id());
  }
  cached_promise_texture_ = result->promise_texture();
  return result;
}

std::unique_ptr<SharedImageRepresentationDawn>
SharedImageBackingGLTexture::ProduceDawn(SharedImageManager* manager,
                                         MemoryTypeTracker* tracker,
                                         WGPUDevice device) {
  if (!factory()) {
    DLOG(ERROR) << "No SharedImageFactory to create a dawn representation.";
    return nullptr;
  }

  return ProduceDawnCommon(factory(), manager, tracker, device, this,
                           IsPassthrough());
}

void SharedImageBackingGLTexture::BeginReadAccess() {
  // TODO(https://crbug.com/1092155): Sub-class this from SharedImageBacking
  // instead of SharedImageBackingReadAccess.
}

bool SharedImageBackingGLTexture::IsPassthrough() const {
  return !!passthrough_texture_;
}

///////////////////////////////////////////////////////////////////////////////
// SharedImageBackingGLImage

SharedImageBackingGLImage::SharedImageBackingGLImage(
    scoped_refptr<gl::GLImage> image,
    const Mailbox& mailbox,
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
      image_(image),
      texture_(texture),
      attribs_(attribs) {
  DCHECK(texture_);
  DCHECK(image_);
}

SharedImageBackingGLImage::~SharedImageBackingGLImage() {
  DCHECK(texture_);
  texture_->RemoveLightweightRef(have_context());
  texture_ = nullptr;

  if (rgb_emulation_texture_) {
    rgb_emulation_texture_->RemoveLightweightRef(have_context());
    rgb_emulation_texture_ = nullptr;
  }
}

gfx::Rect SharedImageBackingGLImage::ClearedRect() const {
  return texture_->GetLevelClearedRect(texture_->target(), 0);
}

void SharedImageBackingGLImage::SetClearedRect(const gfx::Rect& cleared_rect) {
  texture_->SetLevelClearedRect(texture_->target(), 0, cleared_rect);
}

void SharedImageBackingGLImage::Update(
    std::unique_ptr<gfx::GpuFence> in_fence) {
  GLenum target = texture_->target();
  gl::GLApi* api = gl::g_current_gl_context;
  ScopedRestoreTexture scoped_restore(api, target);
  api->glBindTextureFn(target, texture_->service_id());

  gles2::Texture::ImageState old_state = gles2::Texture::UNBOUND;
  gl::GLImage* image = texture_->GetLevelImage(target, 0, &old_state);
  DCHECK_EQ(image, image_.get());
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

bool SharedImageBackingGLImage::ProduceLegacyMailbox(
    MailboxManager* mailbox_manager) {
  DCHECK(texture_);
  mailbox_manager->ProduceTexture(mailbox(), texture_);
  return true;
}

void SharedImageBackingGLImage::OnMemoryDump(
    const std::string& dump_name,
    base::trace_event::MemoryAllocatorDump* dump,
    base::trace_event::ProcessMemoryDump* pmd,
    uint64_t client_tracing_id) {
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

void SharedImageBackingGLImage::BeginReadAccess() {
  GLenum target = texture_->target();
  gles2::Texture::ImageState old_state = gles2::Texture::UNBOUND;
  gl::GLImage* image = texture_->GetLevelImage(target, 0, &old_state);
  DCHECK_EQ(image, image_.get());
  if (old_state == gpu::gles2::Texture::UNBOUND) {
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

scoped_refptr<gfx::NativePixmap> SharedImageBackingGLImage::GetNativePixmap() {
  return image_->GetNativePixmap();
}

std::unique_ptr<SharedImageRepresentationGLTexture>
SharedImageBackingGLImage::ProduceGLTexture(SharedImageManager* manager,
                                            MemoryTypeTracker* tracker) {
  return std::make_unique<SharedImageRepresentationGLTextureImpl>(
      manager, this, tracker, texture_);
}

std::unique_ptr<SharedImageRepresentationGLTexture>
SharedImageBackingGLImage::ProduceRGBEmulationGLTexture(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker) {
  if (!rgb_emulation_texture_) {
    GLenum target = texture_->target();
    gl::GLApi* api = gl::g_current_gl_context;
    ScopedRestoreTexture scoped_restore(api, target);

    // Set to false as this code path is only used on Mac.
    bool framebuffer_attachment_angle = false;
    GLuint service_id =
        MakeTextureAndSetParameters(api, target, framebuffer_attachment_angle);

    gles2::Texture::ImageState image_state = gles2::Texture::BOUND;
    gl::GLImage* image = texture_->GetLevelImage(target, 0, &image_state);
    DCHECK_EQ(image, image_.get());
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

    GLenum format =
        gles2::TextureManager::ExtractFormatFromStorageFormat(internal_format);
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

std::unique_ptr<SharedImageRepresentationSkia>
SharedImageBackingGLImage::ProduceSkia(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker,
    scoped_refptr<SharedContextState> context_state) {
  auto result = std::make_unique<SharedImageRepresentationSkiaImpl>(
      manager, this, std::move(context_state), cached_promise_texture_, tracker,
      texture_->target(), texture_->service_id());
  cached_promise_texture_ = result->promise_texture();
  return result;
}

std::unique_ptr<SharedImageRepresentationDawn>
SharedImageBackingGLImage::ProduceDawn(SharedImageManager* manager,
                                       MemoryTypeTracker* tracker,
                                       WGPUDevice device) {
  if (!factory()) {
    DLOG(ERROR) << "No SharedImageFactory to create a dawn representation.";
    return nullptr;
  }

  return ProduceDawnCommon(factory(), manager, tracker, device, this, false);
}

///////////////////////////////////////////////////////////////////////////////
// SharedImageBackingPassthroughGLImage

SharedImageBackingPassthroughGLImage::SharedImageBackingPassthroughGLImage(
    scoped_refptr<gl::GLImage> image,
    const Mailbox& mailbox,
    viz::ResourceFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    uint32_t usage,
    scoped_refptr<gles2::TexturePassthrough> passthrough_texture)
    : SharedImageBackingWithReadAccess(mailbox,
                                       format,
                                       size,
                                       color_space,
                                       usage,
                                       passthrough_texture->estimated_size(),
                                       false /* is_thread_safe */),
      image_(image),
      texture_passthrough_(std::move(passthrough_texture)) {
  DCHECK_EQ(
      texture_passthrough_->GetLevelImage(texture_passthrough_->target(), 0),
      image_.get());
  DCHECK(texture_passthrough_);
}

SharedImageBackingPassthroughGLImage::~SharedImageBackingPassthroughGLImage() {
  DCHECK(texture_passthrough_);
  if (!have_context())
    texture_passthrough_->MarkContextLost();
  texture_passthrough_.reset();
}

gfx::Rect SharedImageBackingPassthroughGLImage::ClearedRect() const {
  // This backing is used exclusively with ANGLE which handles clear tracking
  // internally. Act as though the texture is always cleared.
  return gfx::Rect(size());
}

void SharedImageBackingPassthroughGLImage::SetClearedRect(
    const gfx::Rect& cleared_rect) {}

void SharedImageBackingPassthroughGLImage::Update(
    std::unique_ptr<gfx::GpuFence> in_fence) {
  GLenum target = texture_passthrough_->target();
  gl::GLApi* api = gl::g_current_gl_context;
  ScopedRestoreTexture scoped_restore(api, target);
  api->glBindTextureFn(target, texture_passthrough_->service_id());

  gl::GLImage* image = texture_passthrough_->GetLevelImage(target, 0);
  DCHECK_EQ(image, image_.get());
  image->ReleaseTexImage(target);
  if (image->ShouldBindOrCopy() == gl::GLImage::BIND)
    image->BindTexImage(target);
  else
    image->CopyTexImage(target);
}

bool SharedImageBackingPassthroughGLImage::ProduceLegacyMailbox(
    MailboxManager* mailbox_manager) {
  DCHECK(texture_passthrough_);
  mailbox_manager->ProduceTexture(mailbox(), texture_passthrough_.get());
  return true;
}

void SharedImageBackingPassthroughGLImage::OnMemoryDump(
    const std::string& dump_name,
    base::trace_event::MemoryAllocatorDump* dump,
    base::trace_event::ProcessMemoryDump* pmd,
    uint64_t client_tracing_id) {
  // Add a |service_guid| which expresses shared ownership between the
  // various GPU dumps.
  auto client_guid = GetSharedImageGUIDForTracing(mailbox());
  auto service_guid =
      gl::GetGLTextureServiceGUIDForTracing(texture_passthrough_->service_id());
  pmd->CreateSharedGlobalAllocatorDump(service_guid);

  int importance = 2;  // This client always owns the ref.
  pmd->AddOwnershipEdge(client_guid, service_guid, importance);

  auto* gl_image = texture_passthrough_->GetLevelImage(
      texture_passthrough_->target(), /*level=*/0);
  if (gl_image)
    gl_image->OnMemoryDump(pmd, client_tracing_id, dump_name);
}

void SharedImageBackingPassthroughGLImage::BeginReadAccess() {}

std::unique_ptr<SharedImageRepresentationGLTexturePassthrough>
SharedImageBackingPassthroughGLImage::ProduceGLTexturePassthrough(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker) {
  return std::make_unique<SharedImageRepresentationGLTexturePassthroughImpl>(
      manager, this, tracker, texture_passthrough_);
}

std::unique_ptr<SharedImageRepresentationSkia>
SharedImageBackingPassthroughGLImage::ProduceSkia(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker,
    scoped_refptr<SharedContextState> context_state) {
  auto result = std::make_unique<SharedImageRepresentationSkiaImpl>(
      manager, this, std::move(context_state), cached_promise_texture_, tracker,
      texture_passthrough_->target(), texture_passthrough_->service_id());
  cached_promise_texture_ = result->promise_texture();
  return result;
}

std::unique_ptr<SharedImageRepresentationDawn>
SharedImageBackingPassthroughGLImage::ProduceDawn(SharedImageManager* manager,
                                                  MemoryTypeTracker* tracker,
                                                  WGPUDevice device) {
  if (!factory()) {
    DLOG(ERROR) << "No SharedImageFactory to create a dawn representation.";
    return nullptr;
  }

  return ProduceDawnCommon(factory(), manager, tracker, device, this, true);
}

///////////////////////////////////////////////////////////////////////////////
// SharedImageBackingFactoryGLTexture

SharedImageBackingFactoryGLTexture::SharedImageBackingFactoryGLTexture(
    const GpuPreferences& gpu_preferences,
    const GpuDriverBugWorkarounds& workarounds,
    const GpuFeatureInfo& gpu_feature_info,
    ImageFactory* image_factory,
    SharedImageBatchAccessManager* batch_access_manager)
    : use_passthrough_(gpu_preferences.use_passthrough_cmd_decoder &&
                       gles2::PassthroughCommandDecoderSupported()),
      image_factory_(image_factory),
      workarounds_(workarounds) {
#if defined(OS_ANDROID)
  batch_access_manager_ = batch_access_manager;
#endif
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
    const GLuint image_internal_format = viz::GLInternalFormat(format);
    const GLenum gl_format = viz::GLDataFormat(format);
    const GLenum gl_type = viz::GLDataType(format);
    const bool uncompressed_format_valid =
        validators->texture_internal_format.IsValid(image_internal_format) &&
        validators->texture_format.IsValid(gl_format);
    const bool compressed_format_valid =
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
        !IsGpuMemoryBufferFormatSupported(format)) {
      continue;
    }
    const gfx::BufferFormat buffer_format = viz::BufferFormat(format);
    switch (buffer_format) {
      case gfx::BufferFormat::RGBA_8888:
      case gfx::BufferFormat::BGRA_8888:
      case gfx::BufferFormat::RGBA_F16:
      case gfx::BufferFormat::R_8:
      case gfx::BufferFormat::BGRA_1010102:
      case gfx::BufferFormat::RGBA_1010102:
        break;
      default:
        continue;
    }
    if (!gpu_memory_buffer_formats_.Has(buffer_format))
      continue;
    info.allow_scanout = true;
    info.buffer_format = buffer_format;
    DCHECK_EQ(info.image_internal_format,
              gl::BufferFormatToGLInternalFormat(buffer_format));
    if (base::Contains(gpu_preferences.texture_target_exception_list,
                       gfx::BufferUsageAndFormat(gfx::BufferUsage::SCANOUT,
                                                 buffer_format))) {
      info.target_for_scanout = gpu::GetPlatformSpecificTextureTarget();
    }
  }
}

SharedImageBackingFactoryGLTexture::~SharedImageBackingFactoryGLTexture() =
    default;

std::unique_ptr<SharedImageBacking>
SharedImageBackingFactoryGLTexture::CreateSharedImage(
    const Mailbox& mailbox,
    viz::ResourceFormat format,
    SurfaceHandle surface_handle,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    uint32_t usage,
    bool is_thread_safe) {
  if (is_thread_safe) {
    return MakeEglImageBacking(mailbox, format, size, color_space, usage);
  } else {
    return CreateSharedImageInternal(mailbox, format, surface_handle, size,
                                     color_space, usage,
                                     base::span<const uint8_t>());
  }
}

std::unique_ptr<SharedImageBacking>
SharedImageBackingFactoryGLTexture::CreateSharedImage(
    const Mailbox& mailbox,
    viz::ResourceFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    uint32_t usage,
    base::span<const uint8_t> pixel_data) {
  return CreateSharedImageInternal(mailbox, format, kNullSurfaceHandle, size,
                                   color_space, usage, pixel_data);
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
#if DCHECK_IS_ON()
  bool texture_2d_support = false;
#if defined(OS_MACOSX)
  // If the PlatformSpecificTextureTarget on Mac is GL_TEXTURE_2D, this is
  // supported.
  texture_2d_support =
      (gpu::GetPlatformSpecificTextureTarget() == GL_TEXTURE_2D);
#endif  // defined(OS_MACOSX)
  DCHECK(handle.type == gfx::SHARED_MEMORY_BUFFER || target != GL_TEXTURE_2D ||
         texture_2d_support || image->ShouldBindOrCopy() == gl::GLImage::BIND);
#endif  // DCHECK_IS_ON()
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

    if (image) {
      return std::make_unique<SharedImageBackingPassthroughGLImage>(
          image, mailbox, format, size, color_space, usage,
          std::move(passthrough_texture));
    } else {
      return std::make_unique<SharedImageBackingGLTexture>(
          mailbox, format, size, color_space, usage, nullptr,
          std::move(passthrough_texture));
    }
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

    if (image) {
      return std::make_unique<SharedImageBackingGLImage>(
          image, mailbox, format, size, color_space, usage, texture, attribs);
    } else {
      return std::make_unique<SharedImageBackingGLTexture>(
          mailbox, format, size, color_space, usage, texture, nullptr);
    }
  }
}

std::unique_ptr<SharedImageBacking>
SharedImageBackingFactoryGLTexture::MakeEglImageBacking(
    const Mailbox& mailbox,
    viz::ResourceFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    uint32_t usage) {
#if defined(OS_ANDROID)
  const FormatInfo& format_info = format_info_[format];
  if (!format_info.enabled) {
    DLOG(ERROR) << "MakeEglImageBacking: invalid format";
    return nullptr;
  }

  DCHECK(!(usage & SHARED_IMAGE_USAGE_SCANOUT));

  if (size.width() < 1 || size.height() < 1 ||
      size.width() > max_texture_size_ || size.height() > max_texture_size_) {
    DLOG(ERROR) << "MakeEglImageBacking: Invalid size";
    return nullptr;
  }

  // Calculate SharedImage size in bytes.
  size_t estimated_size;
  if (!viz::ResourceSizes::MaybeSizeInBytes(size, format, &estimated_size)) {
    DLOG(ERROR) << "MakeEglImageBacking: Failed to calculate SharedImage size";
    return nullptr;
  }

  return std::make_unique<SharedImageBackingEglImage>(
      mailbox, format, size, color_space, usage, estimated_size,
      format_info.gl_format, format_info.gl_type, batch_access_manager_,
      workarounds_);
#else
  return nullptr;
#endif
}

std::unique_ptr<SharedImageBacking>
SharedImageBackingFactoryGLTexture::CreateSharedImageInternal(
    const Mailbox& mailbox,
    viz::ResourceFormat format,
    SurfaceHandle surface_handle,
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
      uint32_t unpadded_row_size = 0u;
      uint32_t padded_row_size = 0u;
      if (!gles2::GLES2Util::ComputeImageDataSizes(
              size.width(), size.height(), 1 /* depth */, format_info.gl_format,
              format_info.gl_type, 4 /* alignment */, &bytes_required,
              &unpadded_row_size, &padded_row_size)) {
        LOG(ERROR) << "CreateSharedImage: Unable to compute required size for "
                      "initial texture upload.";
        return nullptr;
      }

      // The GL spec, used in the computation for required bytes in the function
      // above, assumes no padding is required for the last row in the image.
      // But the client data does include this padding, so we add it for the
      // data validation check here.
      uint32_t padding = padded_row_size - unpadded_row_size;
      bytes_required += padding;
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
        surface_handle, &is_cleared);
    // Scanout images have different constraints than GL images and might fail
    // to allocate even if GL images can be created.
    if (!image) {
      // TODO(dcastagna): Use BufferUsage::GPU_READ_WRITE instead
      // BufferUsage::GPU_READ once we add it.
      image = image_factory_->CreateAnonymousImage(
          size, format_info.buffer_format, gfx::BufferUsage::GPU_READ,
          surface_handle, &is_cleared);
    }
    // The allocated image should not require copy.
    if (!image || image->ShouldBindOrCopy() != gl::GLImage::BIND ||
        !image->BindTexImage(target)) {
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

///////////////////////////////////////////////////////////////////////////////
// SharedImageBackingFactoryGLTexture::FormatInfo

SharedImageBackingFactoryGLTexture::FormatInfo::FormatInfo() = default;
SharedImageBackingFactoryGLTexture::FormatInfo::~FormatInfo() = default;

}  // namespace gpu
