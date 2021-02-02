// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image_backing_gl_image.h"

#include "base/trace_event/memory_dump_manager.h"
#include "build/build_config.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "components/viz/common/resources/resource_sizes.h"
#include "gpu/command_buffer/common/shared_image_trace_utils.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/skia_utils.h"
#include "third_party/skia/include/core/SkPromiseImageTexture.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_fence.h"
#include "ui/gl/gl_gl_api_implementation.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/scoped_binders.h"
#include "ui/gl/trace_util.h"

#if defined(OS_MAC)
#include "gpu/command_buffer/service/shared_image_backing_factory_iosurface.h"
#endif

namespace gpu {

namespace {

size_t EstimatedSize(viz::ResourceFormat format, const gfx::Size& size) {
  size_t estimated_size = 0;
  viz::ResourceSizes::MaybeSizeInBytes(size, format, &estimated_size);
  return estimated_size;
}

using UnpackStateAttribs = SharedImageBackingGLCommon::UnpackStateAttribs;

using ScopedResetAndRestoreUnpackState =
    SharedImageBackingGLCommon::ScopedResetAndRestoreUnpackState;

using ScopedRestoreTexture = SharedImageBackingGLCommon::ScopedRestoreTexture;

using InitializeGLTextureParams =
    SharedImageBackingGLCommon::InitializeGLTextureParams;

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// SharedImageRepresentationGLTextureImpl

// Representation of a SharedImageBackingGLTexture as a GL Texture.
SharedImageRepresentationGLTextureImpl::SharedImageRepresentationGLTextureImpl(
    SharedImageManager* manager,
    SharedImageBacking* backing,
    SharedImageRepresentationGLTextureClient* client,
    MemoryTypeTracker* tracker,
    gles2::Texture* texture)
    : SharedImageRepresentationGLTexture(manager, backing, tracker),
      client_(client),
      texture_(texture) {}

SharedImageRepresentationGLTextureImpl::
    ~SharedImageRepresentationGLTextureImpl() {
  texture_ = nullptr;
  if (client_)
    client_->SharedImageRepresentationGLTextureRelease(has_context());
}

gles2::Texture* SharedImageRepresentationGLTextureImpl::GetTexture() {
  return texture_;
}

bool SharedImageRepresentationGLTextureImpl::BeginAccess(GLenum mode) {
  DCHECK(mode_ == 0);
  mode_ = mode;
  if (client_ && mode != GL_SHARED_IMAGE_ACCESS_MODE_OVERLAY_CHROMIUM)
    return client_->SharedImageRepresentationGLTextureBeginAccess();
  return true;
}

void SharedImageRepresentationGLTextureImpl::EndAccess() {
  DCHECK(mode_ != 0);
  GLenum current_mode = mode_;
  mode_ = 0;
  if (client_)
    return client_->SharedImageRepresentationGLTextureEndAccess(
        current_mode != GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM);
}

///////////////////////////////////////////////////////////////////////////////
// SharedImageRepresentationGLTexturePassthroughImpl

SharedImageRepresentationGLTexturePassthroughImpl::
    SharedImageRepresentationGLTexturePassthroughImpl(
        SharedImageManager* manager,
        SharedImageBacking* backing,
        SharedImageRepresentationGLTextureClient* client,
        MemoryTypeTracker* tracker,
        scoped_refptr<gles2::TexturePassthrough> texture_passthrough)
    : SharedImageRepresentationGLTexturePassthrough(manager, backing, tracker),
      client_(client),
      texture_passthrough_(std::move(texture_passthrough)) {
  // TODO(https://crbug.com/1172769): Remove this CHECK.
  CHECK(texture_passthrough_);
}

SharedImageRepresentationGLTexturePassthroughImpl::
    ~SharedImageRepresentationGLTexturePassthroughImpl() {
  texture_passthrough_.reset();
  if (client_)
    client_->SharedImageRepresentationGLTextureRelease(has_context());
}

const scoped_refptr<gles2::TexturePassthrough>&
SharedImageRepresentationGLTexturePassthroughImpl::GetTexturePassthrough() {
  return texture_passthrough_;
}

bool SharedImageRepresentationGLTexturePassthroughImpl::BeginAccess(
    GLenum mode) {
  DCHECK(mode_ == 0);
  mode_ = mode;
  if (client_ && mode != GL_SHARED_IMAGE_ACCESS_MODE_OVERLAY_CHROMIUM)
    return client_->SharedImageRepresentationGLTextureBeginAccess();
  return true;
}

void SharedImageRepresentationGLTexturePassthroughImpl::EndAccess() {
  DCHECK(mode_ != 0);
  GLenum current_mode = mode_;
  mode_ = 0;
  if (client_)
    return client_->SharedImageRepresentationGLTextureEndAccess(
        current_mode != GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM);
}

///////////////////////////////////////////////////////////////////////////////
// SharedImageRepresentationSkiaImpl

SharedImageRepresentationSkiaImpl::SharedImageRepresentationSkiaImpl(
    SharedImageManager* manager,
    SharedImageBacking* backing,
    SharedImageRepresentationGLTextureClient* client,
    scoped_refptr<SharedContextState> context_state,
    sk_sp<SkPromiseImageTexture> promise_texture,
    MemoryTypeTracker* tracker)
    : SharedImageRepresentationSkia(manager, backing, tracker),
      client_(client),
      context_state_(std::move(context_state)),
      promise_texture_(promise_texture) {
  DCHECK(promise_texture_);
#if DCHECK_IS_ON()
  if (context_state_->GrContextIsGL())
    context_ = gl::GLContext::GetCurrent();
#endif
}

SharedImageRepresentationSkiaImpl::~SharedImageRepresentationSkiaImpl() {
  if (write_surface_) {
    DLOG(ERROR) << "SharedImageRepresentationSkia was destroyed while still "
                << "open for write access.";
  }
  promise_texture_.reset();
  if (client_) {
    DCHECK(context_state_->GrContextIsGL());
    client_->SharedImageRepresentationGLTextureRelease(has_context());
  }
}

sk_sp<SkSurface> SharedImageRepresentationSkiaImpl::BeginWriteAccess(
    int final_msaa_count,
    const SkSurfaceProps& surface_props,
    std::vector<GrBackendSemaphore>* begin_semaphores,
    std::vector<GrBackendSemaphore>* end_semaphores) {
  CheckContext();
  if (client_) {
    DCHECK(context_state_->GrContextIsGL());
    if (!client_->SharedImageRepresentationGLTextureBeginAccess())
      return nullptr;
  }

  if (write_surface_)
    return nullptr;

  if (!promise_texture_)
    return nullptr;

  SkColorType sk_color_type = viz::ResourceFormatToClosestSkColorType(
      /*gpu_compositing=*/true, format());
  auto surface = SkSurface::MakeFromBackendTexture(
      context_state_->gr_context(), promise_texture_->backendTexture(),
      surface_origin(), final_msaa_count, sk_color_type,
      backing()->color_space().ToSkColorSpace(), &surface_props);
  write_surface_ = surface.get();
  return surface;
}

sk_sp<SkPromiseImageTexture>
SharedImageRepresentationSkiaImpl::BeginWriteAccess(
    std::vector<GrBackendSemaphore>* begin_semaphores,
    std::vector<GrBackendSemaphore>* end_semaphores,
    std::unique_ptr<GrBackendSurfaceMutableState>* end_state) {
  CheckContext();
  if (client_) {
    DCHECK(context_state_->GrContextIsGL());
    if (!client_->SharedImageRepresentationGLTextureBeginAccess())
      return nullptr;
  }
  return promise_texture_;
}

void SharedImageRepresentationSkiaImpl::EndWriteAccess(
    sk_sp<SkSurface> surface) {
  if (surface) {
    DCHECK_EQ(surface.get(), write_surface_);
    DCHECK(surface->unique());
    CheckContext();
    // TODO(ericrk): Keep the surface around for re-use.
    write_surface_ = nullptr;
  }

  if (client_)
    client_->SharedImageRepresentationGLTextureEndAccess(false /* readonly */);
}

sk_sp<SkPromiseImageTexture> SharedImageRepresentationSkiaImpl::BeginReadAccess(
    std::vector<GrBackendSemaphore>* begin_semaphores,
    std::vector<GrBackendSemaphore>* end_semaphores) {
  CheckContext();
  if (client_) {
    DCHECK(context_state_->GrContextIsGL());
    if (!client_->SharedImageRepresentationGLTextureBeginAccess())
      return nullptr;
  }
  return promise_texture_;
}

void SharedImageRepresentationSkiaImpl::EndReadAccess() {
  if (client_)
    client_->SharedImageRepresentationGLTextureEndAccess(true /* readonly */);
}

bool SharedImageRepresentationSkiaImpl::SupportsMultipleConcurrentReadAccess() {
  return true;
}

void SharedImageRepresentationSkiaImpl::CheckContext() {
#if DCHECK_IS_ON()
  if (context_)
    DCHECK(gl::GLContext::GetCurrent() == context_);
#endif
}

///////////////////////////////////////////////////////////////////////////////
// SharedImageRepresentationOverlayImpl

SharedImageRepresentationOverlayImpl::SharedImageRepresentationOverlayImpl(
    SharedImageManager* manager,
    SharedImageBacking* backing,
    MemoryTypeTracker* tracker,
    scoped_refptr<gl::GLImage> gl_image)
    : SharedImageRepresentationOverlay(manager, backing, tracker),
      gl_image_(gl_image) {}

SharedImageRepresentationOverlayImpl::~SharedImageRepresentationOverlayImpl() =
    default;

bool SharedImageRepresentationOverlayImpl::BeginReadAccess(
    std::vector<gfx::GpuFence>* acquire_fences) {
  auto* gl_backing = static_cast<SharedImageBackingGLImage*>(backing());
  std::unique_ptr<gfx::GpuFence> fence = gl_backing->GetLastWriteGpuFence();
  if (fence)
    acquire_fences->push_back(std::move(*fence));
  return true;
}

void SharedImageRepresentationOverlayImpl::EndReadAccess(
    gfx::GpuFenceHandle release_fence) {
  DCHECK(release_fence.is_null());
}

gl::GLImage* SharedImageRepresentationOverlayImpl::GetGLImage() {
  return gl_image_.get();
}

///////////////////////////////////////////////////////////////////////////////
// SharedImageBackingGLImage

SharedImageBackingGLImage::SharedImageBackingGLImage(
    scoped_refptr<gl::GLImage> image,
    const Mailbox& mailbox,
    viz::ResourceFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage,
    const InitializeGLTextureParams& params,
    const UnpackStateAttribs& attribs,
    bool is_passthrough)
    : SharedImageBacking(mailbox,
                         format,
                         size,
                         color_space,
                         surface_origin,
                         alpha_type,
                         usage,
                         EstimatedSize(format, size),
                         false /* is_thread_safe */),
      image_(image),
      gl_params_(params),
      gl_unpack_attribs_(attribs),
      is_passthrough_(is_passthrough),
      cleared_rect_(params.is_cleared ? gfx::Rect(size) : gfx::Rect()),
      weak_factory_(this) {
  DCHECK(image_);
}

SharedImageBackingGLImage::~SharedImageBackingGLImage() {
  if (gl_texture_retained_for_legacy_mailbox_)
    ReleaseGLTexture(have_context());
  DCHECK_EQ(gl_texture_retain_count_, 0u);
}

void SharedImageBackingGLImage::RetainGLTexture() {
  gl_texture_retain_count_ += 1;
  if (gl_texture_retain_count_ > 1)
    return;

  // Allocate the GL texture.
  SharedImageBackingGLCommon::MakeTextureAndSetParameters(
      gl_params_.target, 0 /* service_id */,
      gl_params_.framebuffer_attachment_angle,
      is_passthrough_ ? &passthrough_texture_ : nullptr,
      is_passthrough_ ? nullptr : &texture_);

  // Set the GLImage to be initially unbound from the GL texture.
  image_bind_or_copy_needed_ = true;
  if (is_passthrough_) {
    passthrough_texture_->SetEstimatedSize(EstimatedSize(format(), size()));
    passthrough_texture_->SetLevelImage(gl_params_.target, 0, image_.get());
    passthrough_texture_->set_is_bind_pending(true);
  } else {
    texture_->SetLevelInfo(gl_params_.target, 0, gl_params_.internal_format,
                           size().width(), size().height(), 1, 0,
                           gl_params_.format, gl_params_.type, cleared_rect_);
    texture_->SetLevelImage(gl_params_.target, 0, image_.get(),
                            gles2::Texture::UNBOUND);
    texture_->SetImmutable(true, false /* has_immutable_storage */);
  }
}

void SharedImageBackingGLImage::ReleaseGLTexture(bool have_context) {
  DCHECK_GT(gl_texture_retain_count_, 0u);
  gl_texture_retain_count_ -= 1;
  if (gl_texture_retain_count_ > 0)
    return;

  // If the cached promise texture is referencing the GL texture, then it needs
  // to be deleted, too.
  if (cached_promise_texture_) {
    if (cached_promise_texture_->backendTexture().backend() ==
        GrBackendApi::kOpenGL) {
      cached_promise_texture_.reset();
    }
  }

  if (rgb_emulation_texture_) {
    rgb_emulation_texture_->RemoveLightweightRef(have_context);
    rgb_emulation_texture_ = nullptr;
  }
  if (IsPassthrough()) {
    if (passthrough_texture_) {
      if (!have_context)
        passthrough_texture_->MarkContextLost();
      passthrough_texture_.reset();
    }
  } else {
    if (texture_) {
      cleared_rect_ = texture_->GetLevelClearedRect(texture_->target(), 0);
      texture_->RemoveLightweightRef(have_context);
      texture_ = nullptr;
    }
  }
}

GLenum SharedImageBackingGLImage::GetGLTarget() const {
  return gl_params_.target;
}

GLuint SharedImageBackingGLImage::GetGLServiceId() const {
  if (texture_)
    return texture_->service_id();
  if (passthrough_texture_)
    return passthrough_texture_->service_id();
  return 0;
}

std::unique_ptr<gfx::GpuFence>
SharedImageBackingGLImage::GetLastWriteGpuFence() {
  return last_write_gl_fence_ ? last_write_gl_fence_->GetGpuFence() : nullptr;
}

scoped_refptr<gfx::NativePixmap> SharedImageBackingGLImage::GetNativePixmap() {
  return image_->GetNativePixmap();
}

void SharedImageBackingGLImage::OnMemoryDump(
    const std::string& dump_name,
    base::trace_event::MemoryAllocatorDump* dump,
    base::trace_event::ProcessMemoryDump* pmd,
    uint64_t client_tracing_id) {
  // Add a |service_guid| which expresses shared ownership between the
  // various GPU dumps.
  auto client_guid = GetSharedImageGUIDForTracing(mailbox());
  if (auto service_id = GetGLServiceId()) {
    auto service_guid = gl::GetGLTextureServiceGUIDForTracing(GetGLServiceId());
    pmd->CreateSharedGlobalAllocatorDump(service_guid);
    // TODO(piman): coalesce constant with TextureManager::DumpTextureRef.
    int importance = 2;  // This client always owns the ref.
    pmd->AddOwnershipEdge(client_guid, service_guid, importance);
  }
  image_->OnMemoryDump(pmd, client_tracing_id, dump_name);
}

gfx::Rect SharedImageBackingGLImage::ClearedRect() const {
  if (texture_)
    return texture_->GetLevelClearedRect(texture_->target(), 0);
  return cleared_rect_;
}

void SharedImageBackingGLImage::SetClearedRect(const gfx::Rect& cleared_rect) {
  if (texture_)
    texture_->SetLevelClearedRect(texture_->target(), 0, cleared_rect);
  else
    cleared_rect_ = cleared_rect;
}

bool SharedImageBackingGLImage::ProduceLegacyMailbox(
    MailboxManager* mailbox_manager) {
  if (!gl_texture_retained_for_legacy_mailbox_) {
    RetainGLTexture();
    gl_texture_retained_for_legacy_mailbox_ = true;
  }

  if (IsPassthrough())
    mailbox_manager->ProduceTexture(mailbox(), passthrough_texture_.get());
  else
    mailbox_manager->ProduceTexture(mailbox(), texture_);
  return true;
}

std::unique_ptr<SharedImageRepresentationGLTexture>
SharedImageBackingGLImage::ProduceGLTexture(SharedImageManager* manager,
                                            MemoryTypeTracker* tracker) {
  // The corresponding release will be done when the returned representation is
  // destroyed, in SharedImageRepresentationGLTextureRelease.
  RetainGLTexture();
  DCHECK(texture_);
  return std::make_unique<SharedImageRepresentationGLTextureImpl>(
      manager, this, this, tracker, texture_);
}
std::unique_ptr<SharedImageRepresentationGLTexturePassthrough>
SharedImageBackingGLImage::ProduceGLTexturePassthrough(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker) {
  // The corresponding release will be done when the returned representation is
  // destroyed, in SharedImageRepresentationGLTextureRelease.
  RetainGLTexture();
  DCHECK(passthrough_texture_);
  return std::make_unique<SharedImageRepresentationGLTexturePassthroughImpl>(
      manager, this, this, tracker, passthrough_texture_);
}

std::unique_ptr<SharedImageRepresentationOverlay>
SharedImageBackingGLImage::ProduceOverlay(SharedImageManager* manager,
                                          MemoryTypeTracker* tracker) {
#if defined(OS_MAC) || defined(USE_OZONE)
  return std::make_unique<SharedImageRepresentationOverlayImpl>(
      manager, this, tracker, image_);
#else   // !(defined(OS_MAC) || defined(USE_OZONE))
  return SharedImageBacking::ProduceOverlay(manager, tracker);
#endif  // defined(OS_MAC) || defined(USE_OZONE)
}

std::unique_ptr<SharedImageRepresentationDawn>
SharedImageBackingGLImage::ProduceDawn(SharedImageManager* manager,
                                       MemoryTypeTracker* tracker,
                                       WGPUDevice device) {
#if defined(OS_MAC)
  auto result = SharedImageBackingFactoryIOSurface::ProduceDawn(
      manager, this, tracker, device, image_);
  if (result)
    return result;
#endif  // defined(OS_MAC)
  if (!factory()) {
    DLOG(ERROR) << "No SharedImageFactory to create a dawn representation.";
    return nullptr;
  }

  return SharedImageBackingGLCommon::ProduceDawnCommon(
      factory(), manager, tracker, device, this, IsPassthrough());
}

std::unique_ptr<SharedImageRepresentationSkia>
SharedImageBackingGLImage::ProduceSkia(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker,
    scoped_refptr<SharedContextState> context_state) {
  SharedImageRepresentationGLTextureClient* gl_client = nullptr;
  if (context_state->GrContextIsGL()) {
    // The corresponding release will be done when the returned representation
    // is destroyed, in SharedImageRepresentationGLTextureRelease.
    RetainGLTexture();
    gl_client = this;
  }

  if (!cached_promise_texture_) {
    if (context_state->GrContextIsMetal()) {
#if defined(OS_MAC)
      cached_promise_texture_ =
          SharedImageBackingFactoryIOSurface::ProduceSkiaPromiseTextureMetal(
              this, context_state, image_);
      DCHECK(cached_promise_texture_);
#endif
    } else {
      GrBackendTexture backend_texture;
      GetGrBackendTexture(context_state->feature_info(), GetGLTarget(), size(),
                          GetGLServiceId(), format(), &backend_texture);
      cached_promise_texture_ = SkPromiseImageTexture::Make(backend_texture);
    }
  }
  return std::make_unique<SharedImageRepresentationSkiaImpl>(
      manager, this, gl_client, std::move(context_state),
      cached_promise_texture_, tracker);
}

SharedImageRepresentationMemoryImpl::SharedImageRepresentationMemoryImpl(
    SharedImageManager* manager,
    SharedImageBacking* backing,
    MemoryTypeTracker* tracker,
    scoped_refptr<gl::GLImageMemory> image_memory)
    : SharedImageRepresentationMemory(manager, backing, tracker),
      image_memory_(std::move(image_memory)) {}

SharedImageRepresentationMemoryImpl::~SharedImageRepresentationMemoryImpl() =
    default;

SkPixmap SharedImageRepresentationMemoryImpl::BeginReadAccess() {
  SkImageInfo info = SkImageInfo::Make(
      backing()->size().width(), backing()->size().height(),
      viz::ResourceFormatToClosestSkColorType(true, backing()->format()),
      backing()->alpha_type(), backing()->color_space().ToSkColorSpace());
  return SkPixmap(info, image_memory_->memory(), image_memory_->stride());
}

std::unique_ptr<SharedImageRepresentationMemory>
SharedImageBackingGLImage::ProduceMemory(SharedImageManager* manager,
                                         MemoryTypeTracker* tracker) {
  gl::GLImageMemory* image_memory =
      gl::GLImageMemory::FromGLImage(image_.get());
  if (!image_memory)
    return nullptr;

  return std::make_unique<SharedImageRepresentationMemoryImpl>(
      manager, this, tracker, base::WrapRefCounted(image_memory));
}

std::unique_ptr<SharedImageRepresentationGLTexture>
SharedImageBackingGLImage::ProduceRGBEmulationGLTexture(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker) {
  if (IsPassthrough())
    return nullptr;

  RetainGLTexture();
  if (!rgb_emulation_texture_) {
    const GLenum target = GetGLTarget();
    gl::GLApi* api = gl::g_current_gl_context;
    ScopedRestoreTexture scoped_restore(api, target);

    // Set to false as this code path is only used on Mac.
    const bool framebuffer_attachment_angle = false;
    SharedImageBackingGLCommon::MakeTextureAndSetParameters(
        target, 0 /* service_id */, framebuffer_attachment_angle, nullptr,
        &rgb_emulation_texture_);
    api->glBindTextureFn(target, rgb_emulation_texture_->service_id());

    gles2::Texture::ImageState image_state = gles2::Texture::BOUND;
    gl::GLImage* image = texture_->GetLevelImage(target, 0, &image_state);
    DCHECK_EQ(image, image_.get());

    DCHECK(image->ShouldBindOrCopy() == gl::GLImage::BIND);
    const GLenum internal_format = GL_RGB;
    if (!image->BindTexImageWithInternalformat(target, internal_format)) {
      LOG(ERROR) << "Failed to bind image to rgb texture.";
      rgb_emulation_texture_->RemoveLightweightRef(true /* have_context */);
      rgb_emulation_texture_ = nullptr;
      ReleaseGLTexture(true /* has_context */);
      return nullptr;
    }
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
      manager, this, this, tracker, rgb_emulation_texture_);
}

void SharedImageBackingGLImage::Update(
    std::unique_ptr<gfx::GpuFence> in_fence) {
  if (in_fence) {
    // TODO(dcastagna): Don't wait for the fence if the SharedImage is going
    // to be scanned out as an HW overlay. Currently we don't know that at
    // this point and we always bind the image, therefore we need to wait for
    // the fence.
    std::unique_ptr<gl::GLFence> egl_fence =
        gl::GLFence::CreateFromGpuFence(*in_fence.get());
    egl_fence->ServerWait();
  }
  image_bind_or_copy_needed_ = true;
}

bool SharedImageBackingGLImage::
    SharedImageRepresentationGLTextureBeginAccess() {
  return BindOrCopyImageIfNeeded();
}

void SharedImageBackingGLImage::SharedImageRepresentationGLTextureEndAccess(
    bool readonly) {
#if defined(OS_MAC)
  // If this image could potentially be shared with Metal via WebGPU, then flush
  // the GL context to ensure Metal will see it.
  if (usage() & SHARED_IMAGE_USAGE_WEBGPU) {
    gl::GLApi* api = gl::g_current_gl_context;
    api->glFlushFn();
  }

  // When SwANGLE is used as the GL implementation, we have to call
  // ReleaseTexImage to signal an UnlockIOSurface call to sync the surface
  // between the CPU and GPU. The next time this texture is accessed we will
  // call BindTexImage to signal a LockIOSurface call before rendering to it via
  // the CPU.
  if (IsPassthrough() &&
      gl::GetANGLEImplementation() == gl::ANGLEImplementation::kSwiftShader &&
      image_->ShouldBindOrCopy() == gl::GLImage::BIND) {
    const GLenum target = GetGLTarget();
    gl::ScopedTextureBinder binder(target, passthrough_texture_->service_id());
    if (!passthrough_texture_->is_bind_pending()) {
      image_->ReleaseTexImage(target);
      image_bind_or_copy_needed_ = true;
    }
  }
#else
  // If the image will be used for an overlay, we insert a fence that can be
  // used by OutputPresenter to synchronize image writes with presentation.
  if (!readonly && usage() & SHARED_IMAGE_USAGE_SCANOUT &&
      gl::GLFence::IsGpuFenceSupported()) {
    last_write_gl_fence_ = gl::GLFence::CreateForGpuFence();
    DCHECK(last_write_gl_fence_);
  }
#endif
}

void SharedImageBackingGLImage::SharedImageRepresentationGLTextureRelease(
    bool has_context) {
  ReleaseGLTexture(has_context);
}

bool SharedImageBackingGLImage::BindOrCopyImageIfNeeded() {
  // This is called by code that has retained the GL texture.
  DCHECK(texture_ || passthrough_texture_);
  if (!image_bind_or_copy_needed_)
    return true;

  const GLenum target = GetGLTarget();
  gl::GLApi* api = gl::g_current_gl_context;
  ScopedRestoreTexture scoped_restore(api, target);
  api->glBindTextureFn(target, GetGLServiceId());

  // Un-bind the GLImage from the texture if it is currently bound.
  if (image_->ShouldBindOrCopy() == gl::GLImage::BIND) {
    bool is_bound = false;
    if (IsPassthrough()) {
      is_bound = !passthrough_texture_->is_bind_pending();
    } else {
      gles2::Texture::ImageState old_state = gles2::Texture::UNBOUND;
      texture_->GetLevelImage(target, 0, &old_state);
      is_bound = old_state == gles2::Texture::BOUND;
    }
    if (is_bound)
      image_->ReleaseTexImage(target);
  }

  // Bind or copy the GLImage to the texture.
  gles2::Texture::ImageState new_state = gles2::Texture::UNBOUND;
  if (image_->ShouldBindOrCopy() == gl::GLImage::BIND) {
    if (gl_params_.is_rgb_emulation) {
      if (!image_->BindTexImageWithInternalformat(target, GL_RGB)) {
        LOG(ERROR) << "Failed to bind GLImage to RGB target";
        return false;
      }
    } else {
      if (!image_->BindTexImage(target)) {
        LOG(ERROR) << "Failed to bind GLImage to target";
        return false;
      }
    }
    new_state = gles2::Texture::BOUND;
  } else {
    ScopedResetAndRestoreUnpackState scoped_unpack_state(api,
                                                         gl_unpack_attribs_,
                                                         /*upload=*/true);
    if (!image_->CopyTexImage(target)) {
      LOG(ERROR) << "Failed to copy GLImage to target";
      return false;
    }
    new_state = gles2::Texture::COPIED;
  }
  if (IsPassthrough()) {
    passthrough_texture_->set_is_bind_pending(new_state ==
                                              gles2::Texture::UNBOUND);
  } else {
    texture_->SetLevelImage(target, 0, image_.get(), new_state);
  }

  image_bind_or_copy_needed_ = false;
  return true;
}

void SharedImageBackingGLImage::InitializePixels(GLenum format,
                                                 GLenum type,
                                                 const uint8_t* data) {
  DCHECK_EQ(image_->ShouldBindOrCopy(), gl::GLImage::BIND);
#if defined(OS_MAC)
  if (SharedImageBackingFactoryIOSurface::InitializePixels(this, image_, data))
    return;
#else
  RetainGLTexture();
  BindOrCopyImageIfNeeded();

  const GLenum target = GetGLTarget();
  gl::GLApi* api = gl::g_current_gl_context;
  ScopedRestoreTexture scoped_restore(api, target);
  api->glBindTextureFn(target, GetGLServiceId());
  ScopedResetAndRestoreUnpackState scoped_unpack_state(
      api, gl_unpack_attribs_, true /* uploading_data */);
  api->glTexSubImage2DFn(target, 0, 0, 0, size().width(), size().height(),
                         format, type, data);
  ReleaseGLTexture(true /* have_context */);
#endif
}

}  // namespace gpu
