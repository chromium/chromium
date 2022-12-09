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
#include "gpu/command_buffer/service/shared_image/shared_image_format_utils.h"
#include "gpu/command_buffer/service/skia_utils.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkPromiseImageTexture.h"
#include "third_party/skia/include/gpu/GrContextThreadSafeProxy.h"
#include "ui/gl/egl_surface_io_surface.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_display.h"
#include "ui/gl/gl_fence.h"
#include "ui/gl/gl_gl_api_implementation.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/scoped_binders.h"

#include <EGL/egl.h>

#import <Metal/Metal.h>

namespace gpu {

namespace {

using ScopedRestoreTexture = GLTextureImageBackingHelper::ScopedRestoreTexture;

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// IOSurfaceBackingEGLState

IOSurfaceBackingEGLState::IOSurfaceBackingEGLState(
    Client* client,
    EGLDisplay egl_display,
    GLuint gl_target,
    scoped_refptr<gles2::TexturePassthrough> gl_texture)
    : client_(client),
      egl_display_(egl_display),
      gl_target_(gl_target),
      gl_texture_(gl_texture) {
  client_->IOSurfaceBackingEGLStateBeingCreated(this);
}

IOSurfaceBackingEGLState::~IOSurfaceBackingEGLState() {
  client_->IOSurfaceBackingEGLStateBeingDestroyed(this, !context_lost_);
  DCHECK(!gl_texture_);
}

GLuint IOSurfaceBackingEGLState::GetGLServiceId() const {
  return gl_texture_->service_id();
}

bool IOSurfaceBackingEGLState::BeginAccess(bool readonly) {
  gl::GLDisplayEGL* display = gl::GLDisplayEGL::GetDisplayForCurrentContext();
  if (!display || display->GetDisplay() != egl_display_)
    LOG(FATAL) << "Expected GLDisplayEGL not current.";
  return client_->IOSurfaceBackingEGLStateBeginAccess(this, readonly);
}

void IOSurfaceBackingEGLState::EndAccess(bool readonly) {
  client_->IOSurfaceBackingEGLStateEndAccess(this, readonly);
}

void IOSurfaceBackingEGLState::WillRelease(bool have_context) {
  context_lost_ |= !have_context;
}

///////////////////////////////////////////////////////////////////////////////
// GLTextureIOSurfaceRepresentation

GLTextureIOSurfaceRepresentation::GLTextureIOSurfaceRepresentation(
    SharedImageManager* manager,
    SharedImageBacking* backing,
    scoped_refptr<IOSurfaceBackingEGLState> egl_state,
    MemoryTypeTracker* tracker)
    : GLTexturePassthroughImageRepresentation(manager, backing, tracker),
      egl_state_(egl_state) {}

GLTextureIOSurfaceRepresentation::~GLTextureIOSurfaceRepresentation() {
  egl_state_->WillRelease(has_context());
  egl_state_.reset();
}

const scoped_refptr<gles2::TexturePassthrough>&
GLTextureIOSurfaceRepresentation::GetTexturePassthrough(int plane_index) {
  DCHECK_EQ(plane_index, 0);
  return egl_state_->GetGLTexture();
}

bool GLTextureIOSurfaceRepresentation::BeginAccess(GLenum mode) {
  DCHECK(mode_ == 0);
  mode_ = mode;
  bool readonly = mode_ != GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM;
  if (mode != GL_SHARED_IMAGE_ACCESS_MODE_OVERLAY_CHROMIUM)
    return egl_state_->BeginAccess(readonly);
  return true;
}

void GLTextureIOSurfaceRepresentation::EndAccess() {
  DCHECK(mode_ != 0);
  GLenum current_mode = mode_;
  mode_ = 0;
  egl_state_->EndAccess(current_mode !=
                        GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM);
}

///////////////////////////////////////////////////////////////////////////////
// SkiaIOSurfaceRepresentation

SkiaIOSurfaceRepresentation::SkiaIOSurfaceRepresentation(
    SharedImageManager* manager,
    SharedImageBacking* backing,
    scoped_refptr<IOSurfaceBackingEGLState> egl_state,
    scoped_refptr<SharedContextState> context_state,
    sk_sp<SkPromiseImageTexture> promise_texture,
    MemoryTypeTracker* tracker)
    : SkiaImageRepresentation(manager, backing, tracker),
      egl_state_(egl_state),
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
  if (egl_state_) {
    DCHECK(context_state_->GrContextIsGL());
    egl_state_->WillRelease(has_context());
    egl_state_.reset();
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
  if (egl_state_) {
    DCHECK(context_state_->GrContextIsGL());
    if (!egl_state_->BeginAccess(/*readonly=*/false)) {
      return {};
    }
  }

  if (write_surface_)
    return {};

  if (!promise_texture_)
    return {};

  SkColorType sk_color_type = viz::ToClosestSkColorType(
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
  if (egl_state_) {
    DCHECK(context_state_->GrContextIsGL());
    if (!egl_state_->BeginAccess(/*readonly=*/false)) {
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

  if (egl_state_)
    egl_state_->EndAccess(false /* readonly */);
}

std::vector<sk_sp<SkPromiseImageTexture>>
SkiaIOSurfaceRepresentation::BeginReadAccess(
    std::vector<GrBackendSemaphore>* begin_semaphores,
    std::vector<GrBackendSemaphore>* end_semaphores,
    std::unique_ptr<GrBackendSurfaceMutableState>* end_state) {
  CheckContext();
  if (egl_state_) {
    DCHECK(context_state_->GrContextIsGL());
    if (!egl_state_->BeginAccess(/*readonly=*/true)) {
      return {};
    }
  }
  if (!promise_texture_)
    return {};
  return {promise_texture_};
}

void SkiaIOSurfaceRepresentation::EndReadAccess() {
  if (egl_state_)
    egl_state_->EndAccess(true /* readonly */);
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
    gfx::ScopedIOSurface io_surface)
    : OverlayImageRepresentation(manager, backing, tracker),
      io_surface_(std::move(io_surface)) {}

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
  return io_surface_;
}

bool OverlayIOSurfaceRepresentation::IsInUseByWindowServer() const {
  // IOSurfaceIsInUse() will always return true if the IOSurface is wrapped in
  // a CVPixelBuffer. Ignore the signal for such IOSurfaces (which are the ones
  // output by hardware video decode and video capture).
  if (backing()->usage() & SHARED_IMAGE_USAGE_MACOS_VIDEO_TOOLBOX)
    return false;

  return IOSurfaceIsInUse(io_surface_);
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
    gfx::ScopedIOSurface io_surface,
    uint32_t io_surface_plane,
    gfx::BufferFormat io_surface_format,
    gfx::GenericSharedMemoryId io_surface_id,
    const Mailbox& mailbox,
    viz::SharedImageFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage,
    GLenum gl_target,
    bool framebuffer_attachment_angle,
    bool is_cleared)
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
      io_surface_(std::move(io_surface)),
      io_surface_plane_(io_surface_plane),
      io_surface_format_(io_surface_format),
      io_surface_id_(io_surface_id),
      gl_target_(gl_target),
      framebuffer_attachment_angle_(framebuffer_attachment_angle),
      cleared_rect_(is_cleared ? gfx::Rect(size) : gfx::Rect()),
      weak_factory_(this) {
  DCHECK(io_surface_);

  // If this will be bound to different GL backends, then make RetainGLTexture
  // and ReleaseGLTexture actually create and destroy the texture.
  // https://crbug.com/1251724
  if (usage & SHARED_IMAGE_USAGE_HIGH_PERFORMANCE_GPU)
    return;

  // NOTE: Mac currently retains GLTexture and reuses it. Not sure if this is
  // best approach as it can lead to issues with context losses.
  egl_state_for_legacy_mailbox_ = RetainGLTexture();
}

IOSurfaceImageBacking::~IOSurfaceImageBacking() {
  if (egl_state_for_legacy_mailbox_) {
    egl_state_for_legacy_mailbox_->WillRelease(have_context());
    egl_state_for_legacy_mailbox_ = nullptr;
  }
  DCHECK(egl_state_map_.empty());
}

scoped_refptr<IOSurfaceBackingEGLState>
IOSurfaceImageBacking::RetainGLTexture() {
  gl::GLDisplayEGL* display = gl::GLDisplayEGL::GetDisplayForCurrentContext();
  if (!display) {
    LOG(ERROR) << "No GLDisplayEGL current.";
    return nullptr;
  }
  const EGLDisplay egl_display = display->GetDisplay();

  auto found = egl_state_map_.find(egl_display);
  if (found != egl_state_map_.end())
    return found->second;

  // Allocate the GL texture.
  scoped_refptr<gles2::TexturePassthrough> gl_texture;
  GLTextureImageBackingHelper::MakeTextureAndSetParameters(
      gl_target_, /*service_id=*/0, framebuffer_attachment_angle_, &gl_texture,
      nullptr);

  // Set the IOSurface to be initially unbound from the GL texture.
  gl_texture->SetEstimatedSize(
      viz::ResourceSizes::UncheckedSizeInBytes<size_t>(size(), format()));
  gl_texture->set_bind_pending();

  return new IOSurfaceBackingEGLState(this, egl_display, gl_target_,
                                      gl_texture);
}

void IOSurfaceImageBacking::ReleaseGLTexture(
    IOSurfaceBackingEGLState* egl_state,
    bool have_context) {
  // The cached promise texture is referencing the GL texture so it needs to be
  // deleted, too.
  if (egl_state->cached_promise_texture_) {
    egl_state->cached_promise_texture_.reset();
  }

  if (egl_state->gl_texture_) {
    if (have_context) {
      if (egl_state->egl_surface_) {
        ScopedRestoreTexture scoped_restore(gl::g_current_gl_context,
                                            egl_state->GetGLTarget(),
                                            egl_state->GetGLServiceId());
        egl_state->egl_surface_.reset();
      }
    } else {
      egl_state->gl_texture_->MarkContextLost();
    }
    egl_state->gl_texture_.reset();
  }
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

  size_t size_bytes =
      IOSurfaceGetBytesPerRowOfPlane(io_surface_, io_surface_plane_) *
      IOSurfaceGetHeightOfPlane(io_surface_, io_surface_plane_);

  base::trace_event::MemoryAllocatorDump* dump =
      pmd->CreateAllocatorDump(dump_name);
  dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameSize,
                  base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                  static_cast<uint64_t>(size_bytes));

  // The client tracing id is to identify the GpuMemoryBuffer client that
  // created the allocation. For CVPixelBufferRefs, there is no corresponding
  // GpuMemoryBuffer, so use an invalid client id.
  if (usage() & SHARED_IMAGE_USAGE_MACOS_VIDEO_TOOLBOX) {
    client_tracing_id =
        base::trace_event::MemoryDumpManager::kInvalidTracingProcessId;
  }

  // Create an edge using the GMB GenericSharedMemoryId if the image is not
  // anonymous. Otherwise, add another nested node to account for the anonymous
  // IOSurface.
  if (io_surface_id_.is_valid()) {
    auto guid = GetGenericSharedGpuMemoryGUIDForTracing(client_tracing_id,
                                                        io_surface_id_);
    pmd->CreateSharedGlobalAllocatorDump(guid);
    pmd->AddOwnershipEdge(dump->guid(), guid);
  } else {
    std::string anonymous_dump_name = dump_name + "/anonymous-iosurface";
    base::trace_event::MemoryAllocatorDump* anonymous_dump =
        pmd->CreateAllocatorDump(anonymous_dump_name);
    anonymous_dump->AddScalar(
        base::trace_event::MemoryAllocatorDump::kNameSize,
        base::trace_event::MemoryAllocatorDump::kUnitsBytes,
        static_cast<uint64_t>(size_bytes));
    anonymous_dump->AddScalar("width", "pixels", size().width());
    anonymous_dump->AddScalar("height", "pixels", size().height());
  }
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
  // The corresponding release will be done when the returned representation is
  // destroyed, in GLTextureImageRepresentationBeingDestroyed.
  return std::make_unique<GLTextureIOSurfaceRepresentation>(
      manager, this, RetainGLTexture(), tracker);
}

std::unique_ptr<OverlayImageRepresentation>
IOSurfaceImageBacking::ProduceOverlay(SharedImageManager* manager,
                                      MemoryTypeTracker* tracker) {
  return std::make_unique<OverlayIOSurfaceRepresentation>(manager, this,
                                                          tracker, io_surface_);
}

std::unique_ptr<DawnImageRepresentation> IOSurfaceImageBacking::ProduceDawn(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker,
    WGPUDevice device,
    WGPUBackendType backend_type,
    std::vector<WGPUTextureFormat> view_formats) {
  auto result = IOSurfaceImageBackingFactory::ProduceDawn(
      manager, this, tracker, device, view_formats, io_surface_,
      io_surface_plane_);
  if (result)
    return result;

  if (!factory()) {
    DLOG(ERROR) << "No SharedImageFactory to create a dawn representation.";
    return nullptr;
  }

  return GLTextureImageBackingHelper::ProduceDawnCommon(
      factory(), manager, tracker, device, backend_type,
      std::move(view_formats), this,
      /*use_passthrough=*/true);
}

std::unique_ptr<SkiaImageRepresentation> IOSurfaceImageBacking::ProduceSkia(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker,
    scoped_refptr<SharedContextState> context_state) {
  scoped_refptr<IOSurfaceBackingEGLState> egl_state;
  sk_sp<SkPromiseImageTexture> promise_texture;

  if (context_state->GrContextIsGL()) {
    egl_state = RetainGLTexture();
    promise_texture = egl_state->cached_promise_texture_;
  }

  if (!promise_texture) {
    if (context_state->GrContextIsMetal()) {
      promise_texture =
          IOSurfaceImageBackingFactory::ProduceSkiaPromiseTextureMetal(
              this, context_state, io_surface_, io_surface_plane_);
      DCHECK(promise_texture);
    } else {
      bool angle_rgbx_internal_format = context_state->feature_info()
                                            ->feature_flags()
                                            .angle_rgbx_internal_format;
      GLenum gl_texture_storage_format = TextureStorageFormat(
          format(), angle_rgbx_internal_format, /*plane_index=*/0);
      GrBackendTexture backend_texture;
      GetGrBackendTexture(
          context_state->feature_info(), egl_state->GetGLTarget(), size(),
          egl_state->GetGLServiceId(), gl_texture_storage_format,
          context_state->gr_context()->threadSafeProxy(), &backend_texture);
      promise_texture = SkPromiseImageTexture::Make(backend_texture);
    }
  }

  if (egl_state)
    egl_state->cached_promise_texture_ = promise_texture;

  return std::make_unique<SkiaIOSurfaceRepresentation>(
      manager, this, egl_state, std::move(context_state), promise_texture,
      tracker);
}

void IOSurfaceImageBacking::SetPurgeable(bool purgeable) {
  if (purgeable_ == purgeable)
    return;
  purgeable_ = purgeable;

  if (purgeable) {
    // It is in error to purge the surface while reading or writing to it.
    DCHECK(!ongoing_write_access_);
    DCHECK(!num_ongoing_read_accesses_);

    SetClearedRect(gfx::Rect());
  }

  uint32_t old_state;
  IOSurfaceSetPurgeable(io_surface_, purgeable, &old_state);
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
  for (auto iter : egl_state_map_) {
    iter.second->gl_texture_->set_bind_pending();
  }
}

bool IOSurfaceImageBacking::IOSurfaceBackingEGLStateBeginAccess(
    IOSurfaceBackingEGLState* egl_state,
    bool readonly) {
  // It is in error to read or write an IOSurface while it is purgeable.
  DCHECK(!purgeable_);
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

  // If the GL texture is already bound (the bind is not marked as pending),
  // then early-out.
  if (!egl_state->gl_texture_->is_bind_pending())
    return true;

  if (usage() & SHARED_IMAGE_USAGE_WEBGPU &&
      gl::GetANGLEImplementation() == gl::ANGLEImplementation::kMetal) {
    // If this image could potentially be shared with WebGPU's Metal
    // device, it's necessary to synchronize between the two devices.
    // If any Metal shared events have been enqueued (the assumption
    // is that this was done by the Dawn representation), wait on
    // them.
    gl::GLDisplayEGL* display = gl::GLDisplayEGL::GetDisplayForCurrentContext();
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
  if (!egl_state->egl_surface_) {
    egl_state->egl_surface_ = gl::ScopedEGLSurfaceIOSurface::Create(
        egl_state->egl_display_, egl_state->GetGLTarget(), io_surface_,
        io_surface_plane_, io_surface_format_);
    if (!egl_state->egl_surface_) {
      LOG(ERROR) << "Failed to create ScopedEGLSurfaceIOSurface.";
      return false;
    }
  }

  ScopedRestoreTexture scoped_restore(gl::g_current_gl_context,
                                      egl_state->GetGLTarget(),
                                      egl_state->GetGLServiceId());

  // Un-bind the IOSurface from the GL texture (this will be a no-op if it is
  // not yet bound).
  egl_state->egl_surface_->ReleaseTexImage();

  // Bind the IOSurface to the GL texture.
  if (!egl_state->egl_surface_->BindTexImage()) {
    LOG(ERROR) << "Failed to bind ScopedEGLSurfaceIOSurface to target";
    return false;
  }
  egl_state->gl_texture_->clear_bind_pending();
  return true;
}

void IOSurfaceImageBacking::IOSurfaceBackingEGLStateEndAccess(
    IOSurfaceBackingEGLState* egl_state,
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

  bool needs_synchronization = needs_sync_for_swangle || needs_sync_for_metal;
  if (needs_synchronization) {
    if (needs_sync_for_metal) {
      if (@available(macOS 10.14, *)) {
        if (egl_state->egl_surface_) {
          gl::GLDisplayEGL* display =
              gl::GLDisplayEGL::GetDisplayForCurrentContext();
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

    if (!egl_state->gl_texture_->is_bind_pending()) {
      if (egl_state->egl_surface_) {
        ScopedRestoreTexture scoped_restore(gl::g_current_gl_context,
                                            egl_state->GetGLTarget(),
                                            egl_state->GetGLServiceId());
        egl_state->egl_surface_->ReleaseTexImage();
      }
      egl_state->gl_texture_->set_bind_pending();
    }
  }
}

void IOSurfaceImageBacking::IOSurfaceBackingEGLStateBeingCreated(
    IOSurfaceBackingEGLState* egl_state) {
  auto insert_result =
      egl_state_map_.insert(std::make_pair(egl_state->egl_display_, egl_state));
  CHECK(insert_result.second);
}

void IOSurfaceImageBacking::IOSurfaceBackingEGLStateBeingDestroyed(
    IOSurfaceBackingEGLState* egl_state,
    bool has_context) {
  ReleaseGLTexture(egl_state, has_context);

  // Remove `egl_state` from `egl_state_map_`.
  auto found = egl_state_map_.find(egl_state->egl_display_);
  CHECK(found != egl_state_map_.end());
  CHECK(found->second == egl_state);
  egl_state_map_.erase(found);
}

void IOSurfaceImageBacking::InitializePixels(GLenum format,
                                             GLenum type,
                                             const uint8_t* data) {
  if (IOSurfaceImageBackingFactory::InitializePixels(this, io_surface_,
                                                     io_surface_plane_, data))
    return;
}

}  // namespace gpu
