// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/iosurface_image_backing.h"

#include "base/trace_event/memory_dump_manager.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "components/viz/common/resources/resource_sizes.h"
#include "gpu/command_buffer/common/shared_image_trace_utils.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image/iosurface_image_backing_factory.h"
#include "gpu/command_buffer/service/skia_utils.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkPromiseImageTexture.h"
#include "third_party/skia/include/gpu/GrContextThreadSafeProxy.h"
#include "ui/gl/egl_surface_io_surface.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_display.h"
#include "ui/gl/gl_fence.h"
#include "ui/gl/gl_gl_api_implementation.h"
#include "ui/gl/gl_image_io_surface.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/scoped_binders.h"
#include "ui/gl/trace_util.h"

#include <EGL/egl.h>

#import <Metal/Metal.h>

namespace gpu {

namespace {

using ScopedRestoreTexture = GLTextureImageBackingHelper::ScopedRestoreTexture;

using InitializeGLTextureParams =
    GLTextureImageBackingHelper::InitializeGLTextureParams;

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// GLTextureIOSurfaceRepresentation

GLTextureIOSurfaceRepresentation::GLTextureIOSurfaceRepresentation(
    SharedImageManager* manager,
    SharedImageBacking* backing,
    GLTextureIOSurfaceRepresentationClient* client,
    MemoryTypeTracker* tracker,
    scoped_refptr<gles2::TexturePassthrough> texture_passthrough)
    : GLTexturePassthroughImageRepresentation(manager, backing, tracker),
      client_(client),
      texture_(std::move(texture_passthrough)) {
  // TODO(https://crbug.com/1172769): Remove this CHECK.
  CHECK(texture_);
}

GLTextureIOSurfaceRepresentation::~GLTextureIOSurfaceRepresentation() {
  texture_.reset();
  if (client_)
    client_->GLTextureImageRepresentationRelease(
        gl::GLDisplayEGL::GetDisplayForCurrentContext()->GetDisplay(),
        has_context());
}

const scoped_refptr<gles2::TexturePassthrough>&
GLTextureIOSurfaceRepresentation::GetTexturePassthrough(int plane_index) {
  DCHECK_EQ(plane_index, 0);
  return texture_;
}

bool GLTextureIOSurfaceRepresentation::BeginAccess(GLenum mode) {
  DCHECK(mode_ == 0);
  mode_ = mode;
  bool readonly = mode_ != GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM;
  if (client_ && mode != GL_SHARED_IMAGE_ACCESS_MODE_OVERLAY_CHROMIUM)
    return client_->GLTextureImageRepresentationBeginAccess(
        gl::GLDisplayEGL::GetDisplayForCurrentContext()->GetDisplay(),
        readonly);
  return true;
}

void GLTextureIOSurfaceRepresentation::EndAccess() {
  DCHECK(mode_ != 0);
  GLenum current_mode = mode_;
  mode_ = 0;
  if (client_)
    return client_->GLTextureImageRepresentationEndAccess(
        gl::GLDisplayEGL::GetDisplayForCurrentContext()->GetDisplay(),
        current_mode != GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM);
}

///////////////////////////////////////////////////////////////////////////////
// SkiaIOSurfaceRepresentation

SkiaIOSurfaceRepresentation::SkiaIOSurfaceRepresentation(
    SharedImageManager* manager,
    SharedImageBacking* backing,
    GLTextureIOSurfaceRepresentationClient* client,
    scoped_refptr<SharedContextState> context_state,
    sk_sp<SkPromiseImageTexture> promise_texture,
    MemoryTypeTracker* tracker)
    : SkiaImageRepresentation(manager, backing, tracker),
      client_(client),
      context_state_(std::move(context_state)),
      promise_texture_(promise_texture) {
  DCHECK(promise_texture_);
#if DCHECK_IS_ON()
  if (context_state_->GrContextIsGL())
    context_ = gl::GLContext::GetCurrent();
#endif
}

SkiaIOSurfaceRepresentation::~SkiaIOSurfaceRepresentation() {
  if (write_surface_) {
    DLOG(ERROR) << "SkiaImageRepresentation was destroyed while still "
                << "open for write access.";
  }
  promise_texture_.reset();
  if (client_) {
    DCHECK(context_state_->GrContextIsGL());
    client_->GLTextureImageRepresentationRelease(
        context_state_->display()->GetDisplay(), has_context());
  }
}

std::vector<sk_sp<SkSurface>> SkiaIOSurfaceRepresentation::BeginWriteAccess(
    int final_msaa_count,
    const SkSurfaceProps& surface_props,
    const gfx::Rect& update_rect,
    std::vector<GrBackendSemaphore>* begin_semaphores,
    std::vector<GrBackendSemaphore>* end_semaphores,
    std::unique_ptr<GrBackendSurfaceMutableState>* end_state) {
  CheckContext();
  if (client_) {
    DCHECK(context_state_->GrContextIsGL());
    if (!client_->GLTextureImageRepresentationBeginAccess(
            context_state_->display()->GetDisplay(),
            /*readonly=*/false)) {
      return {};
    }
  }

  if (write_surface_)
    return {};

  if (!promise_texture_)
    return {};

  SkColorType sk_color_type = viz::ResourceFormatToClosestSkColorType(
      /*gpu_compositing=*/true, format());
  // Gray is not a renderable single channel format, but alpha is.
  if (sk_color_type == kGray_8_SkColorType)
    sk_color_type = kAlpha_8_SkColorType;
  auto surface = SkSurface::MakeFromBackendTexture(
      context_state_->gr_context(), promise_texture_->backendTexture(),
      surface_origin(), final_msaa_count, sk_color_type,
      backing()->color_space().GetAsFullRangeRGB().ToSkColorSpace(),
      &surface_props);
  write_surface_ = surface;

  if (!surface)
    return {};
  return {surface};
}

std::vector<sk_sp<SkPromiseImageTexture>>
SkiaIOSurfaceRepresentation::BeginWriteAccess(
    std::vector<GrBackendSemaphore>* begin_semaphores,
    std::vector<GrBackendSemaphore>* end_semaphores,
    std::unique_ptr<GrBackendSurfaceMutableState>* end_state) {
  CheckContext();
  if (client_) {
    DCHECK(context_state_->GrContextIsGL());
    if (!client_->GLTextureImageRepresentationBeginAccess(
            context_state_->display()->GetDisplay(),
            /*readonly=*/false)) {
      return {};
    }
  }
  if (!promise_texture_)
    return {};
  return {promise_texture_};
}

void SkiaIOSurfaceRepresentation::EndWriteAccess() {
  if (write_surface_) {
    DCHECK(write_surface_->unique());
    CheckContext();
    // TODO(ericrk): Keep the surface around for re-use.
    write_surface_.reset();
  }

  if (client_)
    client_->GLTextureImageRepresentationEndAccess(
        context_state_->display()->GetDisplay(), false /* readonly */);
}

std::vector<sk_sp<SkPromiseImageTexture>>
SkiaIOSurfaceRepresentation::BeginReadAccess(
    std::vector<GrBackendSemaphore>* begin_semaphores,
    std::vector<GrBackendSemaphore>* end_semaphores,
    std::unique_ptr<GrBackendSurfaceMutableState>* end_state) {
  CheckContext();
  if (client_) {
    DCHECK(context_state_->GrContextIsGL());
    if (!client_->GLTextureImageRepresentationBeginAccess(
            context_state_->display()->GetDisplay(),
            /*readonly=*/true)) {
      return {};
    }
  }
  if (!promise_texture_)
    return {};
  return {promise_texture_};
}

void SkiaIOSurfaceRepresentation::EndReadAccess() {
  if (client_)
    client_->GLTextureImageRepresentationEndAccess(
        context_state_->display()->GetDisplay(), true /* readonly */);
}

bool SkiaIOSurfaceRepresentation::SupportsMultipleConcurrentReadAccess() {
  return true;
}

void SkiaIOSurfaceRepresentation::CheckContext() {
#if DCHECK_IS_ON()
  if (!context_state_->context_lost() && context_)
    DCHECK(gl::GLContext::GetCurrent() == context_);
#endif
}

///////////////////////////////////////////////////////////////////////////////
// OverlayIOSurfaceRepresentation

OverlayIOSurfaceRepresentation::OverlayIOSurfaceRepresentation(
    SharedImageManager* manager,
    SharedImageBacking* backing,
    MemoryTypeTracker* tracker,
    scoped_refptr<gl::GLImage> gl_image)
    : OverlayImageRepresentation(manager, backing, tracker),
      gl_image_(gl_image) {}

OverlayIOSurfaceRepresentation::~OverlayIOSurfaceRepresentation() = default;

bool OverlayIOSurfaceRepresentation::BeginReadAccess(
    gfx::GpuFenceHandle& acquire_fence) {
  auto* gl_backing = static_cast<IOSurfaceImageBacking*>(backing());
  std::unique_ptr<gfx::GpuFence> fence = gl_backing->GetLastWriteGpuFence();
  if (fence)
    acquire_fence = fence->GetGpuFenceHandle().Clone();
  return true;
}

void OverlayIOSurfaceRepresentation::EndReadAccess(
    gfx::GpuFenceHandle release_fence) {
  auto* gl_backing = static_cast<IOSurfaceImageBacking*>(backing());
  gl_backing->SetReleaseFence(std::move(release_fence));
}

gfx::ScopedIOSurface OverlayIOSurfaceRepresentation::GetIOSurface() const {
  if (gl_image_->GetType() != gl::GLImage::Type::IOSURFACE)
    return gfx::ScopedIOSurface();
  return static_cast<gl::GLImageIOSurface*>(gl_image_.get())->io_surface();
}

bool OverlayIOSurfaceRepresentation::IsInUseByWindowServer() const {
  // IOSurfaceIsInUse() will always return true if the IOSurface is wrapped in
  // a CVPixelBuffer. Ignore the signal for such IOSurfaces (which are the ones
  // output by hardware video decode and video capture).
  if (backing()->usage() & SHARED_IMAGE_USAGE_MACOS_VIDEO_TOOLBOX)
    return false;

  if (gl_image_->GetType() != gl::GLImage::Type::IOSURFACE)
    return false;

  return IOSurfaceIsInUse(
      static_cast<gl::GLImageIOSurface*>(gl_image_.get())->io_surface());
}

////////////////////////////////////////////////////////////////////////////////
// SharedEventAndSignalValue

SharedEventAndSignalValue::SharedEventAndSignalValue(id shared_event,
                                                     uint64_t signaled_value)
    : shared_event_(shared_event), signaled_value_(signaled_value) {
  if (@available(macOS 10.14, *)) {
    if (shared_event_) {
      [static_cast<id<MTLSharedEvent>>(shared_event_) retain];
    }
  }
}

SharedEventAndSignalValue::~SharedEventAndSignalValue() {
  if (@available(macOS 10.14, *)) {
    if (shared_event_) {
      [static_cast<id<MTLSharedEvent>>(shared_event_) release];
    }
  }
  shared_event_ = nil;
}

///////////////////////////////////////////////////////////////////////////////
// IOSurfaceImageBacking

IOSurfaceImageBacking::IOSurfaceImageBacking(
    scoped_refptr<gl::GLImage> image,
    const Mailbox& mailbox,
    viz::SharedImageFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage,
    const InitializeGLTextureParams& params)
    : SharedImageBacking(
          mailbox,
          format,
          size,
          color_space,
          surface_origin,
          alpha_type,
          usage,
          viz::ResourceSizes::UncheckedSizeInBytes<size_t>(size, format),
          false /* is_thread_safe */),
      image_(image),
      gl_params_(params),
      cleared_rect_(params.is_cleared ? gfx::Rect(size) : gfx::Rect()),
      weak_factory_(this) {
  DCHECK(image_);

  // If this will be bound to different GL backends, then make RetainGLTexture
  // and ReleaseGLTexture actually create and destroy the texture.
  // https://crbug.com/1251724
  if (usage & SHARED_IMAGE_USAGE_HIGH_PERFORMANCE_GPU)
    return;
}

IOSurfaceImageBacking::~IOSurfaceImageBacking() {
  for (auto& iter : texture_infos_) {
    DCHECK_EQ(iter.second.gl_texture_retain_count_, 0u);
  }
}

void IOSurfaceImageBacking::RetainGLTexture(EGLDisplay display) {
  auto& texture_info = texture_infos_[display];
  texture_info.gl_texture_retain_count_ += 1;
  if (texture_info.gl_texture_retain_count_ > 1)
    return;

  // Allocate the GL texture.
  GLTextureImageBackingHelper::MakeTextureAndSetParameters(
      gl_params_.target, 0 /* service_id */,
      gl_params_.framebuffer_attachment_angle, &texture_info.gl_texture_,
      nullptr);

  // Set the GLImage to be initially unbound from the GL texture.
  texture_info.gl_texture_->SetEstimatedSize(
      viz::ResourceSizes::UncheckedSizeInBytes<size_t>(size(), format()));
  texture_info.gl_texture_->SetLevelImage(gl_params_.target, 0, image_.get());
  texture_info.gl_texture_->set_is_bind_pending(true);
}

void IOSurfaceImageBacking::ReleaseGLTexture(EGLDisplay display,
                                             bool have_context) {
  auto& texture_info = GetTextureInfo(display);
  DCHECK_GT(texture_info.gl_texture_retain_count_, 0u);
  texture_info.gl_texture_retain_count_ -= 1;
  if (texture_info.gl_texture_retain_count_ > 0)
    return;

  // If the cached promise texture is referencing the GL texture, then it needs
  // to be deleted, too.
  if (cached_promise_texture_) {
    if (cached_promise_texture_->backendTexture().backend() ==
        GrBackendApi::kOpenGL) {
      cached_promise_texture_.reset();
    }
  }

  if (texture_info.gl_texture_) {
    if (have_context) {
      if (texture_info.egl_surface_) {
        ScopedRestoreTexture scoped_restore(
            gl::g_current_gl_context, GetGLTarget(), GetGLServiceId(display));
        texture_info.egl_surface_.reset();
      }
    } else {
      texture_info.gl_texture_->MarkContextLost();
    }
    texture_info.gl_texture_.reset();
  }
}

IOSurfaceImageBacking::TextureInfo& IOSurfaceImageBacking::GetTextureInfo(
    EGLDisplay display) {
#if DCHECK_IS_ON()
  auto iter = texture_infos_.find(display);
  DCHECK(iter != texture_infos_.end());
#endif
  return texture_infos_[display];
}

IOSurfaceImageBacking::TextureInfo::TextureInfo() = default;

IOSurfaceImageBacking::TextureInfo::~TextureInfo() = default;

GLenum IOSurfaceImageBacking::GetGLTarget() const {
  return gl_params_.target;
}

GLuint IOSurfaceImageBacking::GetGLServiceId(EGLDisplay display) const {
  auto iter = texture_infos_.find(display);
  if (texture_infos_.end() == iter)
    return 0;
  if (iter->second.gl_texture_)
    return iter->second.gl_texture_->service_id();
  return 0;
}

std::unique_ptr<gfx::GpuFence> IOSurfaceImageBacking::GetLastWriteGpuFence() {
  return last_write_gl_fence_ ? last_write_gl_fence_->GetGpuFence() : nullptr;
}

void IOSurfaceImageBacking::SetReleaseFence(gfx::GpuFenceHandle release_fence) {
  release_fence_ = std::move(release_fence);
}

void IOSurfaceImageBacking::AddSharedEventAndSignalValue(
    id shared_event,
    uint64_t signal_value) {
  shared_events_and_signal_values_.push_back(
      std::make_unique<SharedEventAndSignalValue>(shared_event, signal_value));
}

std::vector<std::unique_ptr<SharedEventAndSignalValue>>
IOSurfaceImageBacking::TakeSharedEvents() {
  return std::move(shared_events_and_signal_values_);
}

void IOSurfaceImageBacking::OnMemoryDump(
    const std::string& dump_name,
    base::trace_event::MemoryAllocatorDumpGuid client_guid,
    base::trace_event::ProcessMemoryDump* pmd,
    uint64_t client_tracing_id) {
  SharedImageBacking::OnMemoryDump(dump_name, client_guid, pmd,
                                   client_tracing_id);

  // Add a |service_guid| which expresses shared ownership between the
  // various GPU dumps.
  EGLDisplay display =
      gl::GLDisplayEGL::GetDisplayForCurrentContext()->GetDisplay();
  if (auto service_id = GetGLServiceId(display)) {
    auto service_guid =
        gl::GetGLTextureServiceGUIDForTracing(GetGLServiceId(display));
    pmd->CreateSharedGlobalAllocatorDump(service_guid);
    pmd->AddOwnershipEdge(client_guid, service_guid, kOwningEdgeImportance);
  }
  image_->OnMemoryDump(pmd, client_tracing_id, dump_name);
}

SharedImageBackingType IOSurfaceImageBacking::GetType() const {
  return SharedImageBackingType::kIOSurface;
}

gfx::Rect IOSurfaceImageBacking::ClearedRect() const {
  return cleared_rect_;
}

void IOSurfaceImageBacking::SetClearedRect(const gfx::Rect& cleared_rect) {
  cleared_rect_ = cleared_rect;
}

std::unique_ptr<GLTextureImageRepresentation>
IOSurfaceImageBacking::ProduceGLTexture(SharedImageManager* manager,
                                        MemoryTypeTracker* tracker) {
  return nullptr;
}

std::unique_ptr<GLTexturePassthroughImageRepresentation>
IOSurfaceImageBacking::ProduceGLTexturePassthrough(SharedImageManager* manager,
                                                   MemoryTypeTracker* tracker) {
  gl::GLDisplayEGL* display = gl::GLDisplayEGL::GetDisplayForCurrentContext();
  // The corresponding release will be done when the returned representation is
  // destroyed, in GLTextureImageRepresentationRelease.
  RetainGLTexture(display->GetDisplay());
  auto& texture_info = GetTextureInfo(display->GetDisplay());
  DCHECK(texture_info.gl_texture_);
  return std::make_unique<GLTextureIOSurfaceRepresentation>(
      manager, this, this, tracker, texture_info.gl_texture_);
}

std::unique_ptr<OverlayImageRepresentation>
IOSurfaceImageBacking::ProduceOverlay(SharedImageManager* manager,
                                      MemoryTypeTracker* tracker) {
  return std::make_unique<OverlayIOSurfaceRepresentation>(manager, this,
                                                          tracker, image_);
}

std::unique_ptr<DawnImageRepresentation> IOSurfaceImageBacking::ProduceDawn(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker,
    WGPUDevice device,
    WGPUBackendType backend_type) {
  auto result = IOSurfaceImageBackingFactory::ProduceDawn(
      manager, this, tracker, device, image_);
  if (result)
    return result;

  if (!factory()) {
    DLOG(ERROR) << "No SharedImageFactory to create a dawn representation.";
    return nullptr;
  }

  return GLTextureImageBackingHelper::ProduceDawnCommon(
      factory(), manager, tracker, device, backend_type, this, IsPassthrough());
}

std::unique_ptr<SkiaImageRepresentation> IOSurfaceImageBacking::ProduceSkia(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker,
    scoped_refptr<SharedContextState> context_state) {
  GLTextureIOSurfaceRepresentationClient* gl_client = nullptr;
  if (context_state->GrContextIsGL()) {
    // The corresponding release will be done when the returned representation
    // is destroyed, in GLTextureImageRepresentationRelease.
    RetainGLTexture(
        gl::GLDisplayEGL::GetDisplayForCurrentContext()->GetDisplay());
    gl_client = this;
  }

  if (!cached_promise_texture_) {
    if (context_state->GrContextIsMetal()) {
      cached_promise_texture_ =
          IOSurfaceImageBackingFactory::ProduceSkiaPromiseTextureMetal(
              this, context_state, image_);
      DCHECK(cached_promise_texture_);
    } else {
      GrBackendTexture backend_texture;
      GetGrBackendTexture(
          context_state->feature_info(), GetGLTarget(), size(),
          GetGLServiceId(
              gl::GLDisplayEGL::GetDisplayForCurrentContext()->GetDisplay()),
          format().resource_format(),
          context_state->gr_context()->threadSafeProxy(), &backend_texture);
      cached_promise_texture_ = SkPromiseImageTexture::Make(backend_texture);
    }
  }
  return std::make_unique<SkiaIOSurfaceRepresentation>(
      manager, this, gl_client, std::move(context_state),
      cached_promise_texture_, tracker);
}

MemoryIOSurfaceRepresentation::MemoryIOSurfaceRepresentation(
    SharedImageManager* manager,
    SharedImageBacking* backing,
    MemoryTypeTracker* tracker,
    scoped_refptr<gl::GLImageMemory> image_memory)
    : MemoryImageRepresentation(manager, backing, tracker),
      image_memory_(std::move(image_memory)) {}

MemoryIOSurfaceRepresentation::~MemoryIOSurfaceRepresentation() = default;

SkPixmap MemoryIOSurfaceRepresentation::BeginReadAccess() {
  return SkPixmap(backing()->AsSkImageInfo(), image_memory_->memory(),
                  image_memory_->stride());
}

std::unique_ptr<MemoryImageRepresentation> IOSurfaceImageBacking::ProduceMemory(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker) {
  gl::GLImageMemory* image_memory =
      gl::GLImageMemory::FromGLImage(image_.get());
  if (!image_memory)
    return nullptr;

  return std::make_unique<MemoryIOSurfaceRepresentation>(
      manager, this, tracker, base::WrapRefCounted(image_memory));
}

void IOSurfaceImageBacking::Update(std::unique_ptr<gfx::GpuFence> in_fence) {
  if (in_fence) {
    // TODO(dcastagna): Don't wait for the fence if the SharedImage is going
    // to be scanned out as an HW overlay. Currently we don't know that at
    // this point and we always bind the image, therefore we need to wait for
    // the fence.
    std::unique_ptr<gl::GLFence> egl_fence =
        gl::GLFence::CreateFromGpuFence(*in_fence.get());
    egl_fence->ServerWait();
  }
  gl::GLDisplayEGL* display = gl::GLDisplayEGL::GetDisplayForCurrentContext();
  auto iter = texture_infos_.find(display);
  if (iter != texture_infos_.end() && iter->second.gl_texture_)
    iter->second.gl_texture_->set_is_bind_pending(true);
}

bool IOSurfaceImageBacking::GLTextureImageRepresentationBeginAccess(
    EGLDisplay egl_display,
    bool readonly) {
  DCHECK(!ongoing_write_access_);
  if (readonly) {
    num_ongoing_read_accesses_++;
  } else {
    if (!(usage() & SHARED_IMAGE_USAGE_CONCURRENT_READ_WRITE)) {
      DCHECK(num_ongoing_read_accesses_ == 0);
    }
    ongoing_write_access_ = true;
  }

  if (!release_fence_.is_null()) {
    auto fence = gfx::GpuFence(std::move(release_fence_));
    if (gl::GLFence::IsGpuFenceSupported()) {
      gl::GLFence::CreateFromGpuFence(std::move(fence))->ServerWait();
    } else {
      fence.Wait();
    }
  }
  gl::GLDisplayEGL* display = gl::GLDisplayEGL::GetDisplayForCurrentContext();
  DCHECK_EQ(display->GetDisplay(), egl_display);
  auto& texture_info = GetTextureInfo(egl_display);

  // If the GL texture is already bound (the bind is not marked as pending),
  // then early-out.
  if (!texture_info.gl_texture_->is_bind_pending())
    return true;

  if (usage() & SHARED_IMAGE_USAGE_WEBGPU &&
      gl::GetANGLEImplementation() == gl::ANGLEImplementation::kMetal) {
    // If this image could potentially be shared with WebGPU's Metal
    // device, it's necessary to synchronize between the two devices.
    // If any Metal shared events have been enqueued (the assumption
    // is that this was done by the Dawn representation), wait on
    // them.
    if (display && display->IsANGLEMetalSharedEventSyncSupported()) {
      std::vector<std::unique_ptr<SharedEventAndSignalValue>> signals =
          TakeSharedEvents();
      for (const auto& signal : signals) {
        display->WaitForMetalSharedEvent(signal->shared_event(),
                                         signal->signaled_value());
      }
    }
  }

  // Create the EGL surface to bind to the GL texture, if it doesn't exist
  // already.
  if (!texture_info.egl_surface_) {
    auto* gl_image_io_surface =
        static_cast<gl::GLImageIOSurface*>(image_.get());
    if (!display) {
      LOG(ERROR) << "No GLDisplayEGL current.";
      return false;
    }
    texture_info.egl_surface_ = gl::ScopedEGLSurfaceIOSurface::Create(
        display->GetDisplay(), GetGLTarget(), gl_image_io_surface->io_surface(),
        gl_image_io_surface->io_surface_plane(), gl_image_io_surface->format());
    if (!texture_info.egl_surface_) {
      LOG(ERROR) << "Failed to create ScopedEGLSurfaceIOSurface.";
      return false;
    }
  }

  ScopedRestoreTexture scoped_restore(gl::g_current_gl_context, GetGLTarget(),
                                      texture_info.gl_texture_->service_id());

  // Un-bind the IOSurface from the GL texture (this will be a no-op if it is
  // not yet bound).
  texture_info.egl_surface_->ReleaseTexImage();

  // Bind the IOSurface to the GL texture.
  if (!texture_info.egl_surface_->BindTexImage()) {
    LOG(ERROR) << "Failed to bind ScopedEGLSurfaceIOSurface to target";
    return false;
  }
  texture_info.gl_texture_->set_is_bind_pending(false);
  return true;
}

void IOSurfaceImageBacking::GLTextureImageRepresentationEndAccess(
    EGLDisplay egl_display,
    bool readonly) {
  if (readonly) {
    DCHECK(num_ongoing_read_accesses_ > 0);
    if (!(usage() & SHARED_IMAGE_USAGE_CONCURRENT_READ_WRITE)) {
      DCHECK(!ongoing_write_access_);
    }
    num_ongoing_read_accesses_--;
  } else {
    DCHECK(ongoing_write_access_);
    if (!(usage() & SHARED_IMAGE_USAGE_CONCURRENT_READ_WRITE)) {
      DCHECK(num_ongoing_read_accesses_ == 0);
    }
    ongoing_write_access_ = false;
  }

  // If this image could potentially be shared with Metal via WebGPU, then flush
  // the GL context to ensure Metal will see it.
  if (usage() & SHARED_IMAGE_USAGE_WEBGPU) {
    gl::GLApi* api = gl::g_current_gl_context;
    api->glFlushFn();
  }

  // When SwANGLE is used as the GL implementation, it holds an internal
  // texture. We have to call ReleaseTexImage here to trigger a copy from that
  // internal texture to the IOSurface (the next Bind() will then trigger an
  // IOSurface->internal texture copy). We do this only when there are no
  // ongoing reads in order to ensure that it does not result in the GLES2
  // decoders needing to perform on-demand binding (rather, the binding will be
  // performed at the next BeginAccess()). Note that it is not sufficient to
  // release the image only at the end of a write: the CPU can write directly to
  // the IOSurface when the GPU is not accessing the internal texture (in the
  // case of zero-copy raster), and any such IOSurface-side modifications need
  // to be copied to the internal texture via a Bind() when the GPU starts a
  // subsequent read. Note also that this logic assumes that writes are
  // serialized with respect to reads (so that the end of a write always
  // triggers a release and copy). By design, GLImageBackingFactory enforces
  // this property for this use case.
  bool needs_sync_for_swangle =
      (gl::GetANGLEImplementation() == gl::ANGLEImplementation::kSwiftShader &&
       (num_ongoing_read_accesses_ == 0));

  // Similarly, when ANGLE's metal backend is used, we have to signal a call to
  // waitUntilScheduled() using the same method on EndAccess to ensure IOSurface
  // synchronization. In this case, it is sufficient to release the image at the
  // end of a write. As above, GLImageBackingFactory enforces serialization of
  // reads and writes for this use case.
  // TODO(https://anglebug.com/7626): Enable on Metal only when
  // CPU_READ or SCANOUT is specified. When doing so, adjust the conditions for
  // disallowing concurrent read/write in GLImageBackingFactory as suitable.
  bool needs_sync_for_metal =
      (gl::GetANGLEImplementation() == gl::ANGLEImplementation::kMetal &&
       !readonly);

  gl::GLDisplayEGL* display = gl::GLDisplayEGL::GetDisplayForCurrentContext();
  DCHECK_EQ(display->GetDisplay(), egl_display);
  auto& texture_info = GetTextureInfo(egl_display);
  bool needs_synchronization = needs_sync_for_swangle || needs_sync_for_metal;
  if (needs_synchronization) {
    if (needs_sync_for_metal) {
      if (@available(macOS 10.14, *)) {
        if (texture_info.egl_surface_) {
          if (display) {
            metal::MTLSharedEventPtr shared_event = nullptr;
            uint64_t signal_value = 0;
            if (display->CreateMetalSharedEvent(&shared_event, &signal_value)) {
              AddSharedEventAndSignalValue(shared_event, signal_value);
            } else {
              LOG(DFATAL) << "Failed to create Metal shared event";
            }
          }
        }
      }
    }

    if (!texture_info.gl_texture_->is_bind_pending()) {
      if (texture_info.egl_surface_) {
        ScopedRestoreTexture scoped_restore(
            gl::g_current_gl_context, GetGLTarget(),
            texture_info.gl_texture_->service_id());
        texture_info.egl_surface_->ReleaseTexImage();
      }
      texture_info.gl_texture_->set_is_bind_pending(true);
    }
  }
}

void IOSurfaceImageBacking::GLTextureImageRepresentationRelease(
    EGLDisplay egl_display,
    bool has_context) {
  DCHECK_EQ(gl::GLDisplayEGL::GetDisplayForCurrentContext()->GetDisplay(),
            egl_display);
  ReleaseGLTexture(egl_display, has_context);
}

void IOSurfaceImageBacking::InitializePixels(GLenum format,
                                             GLenum type,
                                             const uint8_t* data) {
  if (IOSurfaceImageBackingFactory::InitializePixels(this, image_, data))
    return;
}

}  // namespace gpu
