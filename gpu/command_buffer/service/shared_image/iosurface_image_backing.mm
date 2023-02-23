// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/iosurface_image_backing.h"

#include "base/mac/scoped_cftyperef.h"
#include "base/mac/scoped_nsobject.h"
#include "base/trace_event/memory_dump_manager.h"
#include "components/viz/common/gpu/metal_context_provider.h"
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

// Usage of BUILDFLAG(USE_DAWN) needs to be after the include for
// ui/gl/buildflags.h
#if BUILDFLAG(USE_DAWN)
#include <dawn/native/MetalBackend.h>
#endif  // BUILDFLAG(USE_DAWN)

namespace gpu {

namespace {

using ScopedRestoreTexture = GLTextureImageBackingHelper::ScopedRestoreTexture;

// Returns BufferFormat for given multiplanar `format`.
gfx::BufferFormat GetBufferFormatForPlane(viz::SharedImageFormat format,
                                          int plane) {
  DCHECK(format.is_multi_plane());
  DCHECK(format.IsValidPlaneIndex(plane));

  // IOSurfaceBacking does not support external sampler use cases.
  int num_channels = format.NumChannelsInPlane(plane);
  DCHECK_LE(num_channels, 2);
  switch (format.channel_format()) {
    case viz::SharedImageFormat::ChannelFormat::k8:
      return num_channels == 2 ? gfx::BufferFormat::RG_88
                               : gfx::BufferFormat::R_8;
    case viz::SharedImageFormat::ChannelFormat::k10:
    case viz::SharedImageFormat::ChannelFormat::k16:
    case viz::SharedImageFormat::ChannelFormat::k16F:
      return num_channels == 2 ? gfx::BufferFormat::RG_1616
                               : gfx::BufferFormat::R_16;
  }
  NOTREACHED();
  return gfx::BufferFormat::RGBA_8888;
}

base::scoped_nsprotocol<id<MTLTexture>> CreateMetalTexture(
    id<MTLDevice> mtl_device,
    IOSurfaceRef io_surface,
    const gfx::Size& size,
    viz::SharedImageFormat format,
    int plane_index) {
  TRACE_EVENT0("gpu", "IOSurfaceImageBackingFactory::CreateMetalTexture");
  base::scoped_nsprotocol<id<MTLTexture>> mtl_texture;
  MTLPixelFormat mtl_pixel_format =
      static_cast<MTLPixelFormat>(ToMTLPixelFormat(format, plane_index));
  if (mtl_pixel_format == MTLPixelFormatInvalid) {
    return mtl_texture;
  }

  base::scoped_nsobject<MTLTextureDescriptor> mtl_tex_desc(
      [MTLTextureDescriptor new]);
  [mtl_tex_desc setTextureType:MTLTextureType2D];
  [mtl_tex_desc
      setUsage:MTLTextureUsageShaderRead | MTLTextureUsageRenderTarget];
  [mtl_tex_desc setPixelFormat:mtl_pixel_format];
  [mtl_tex_desc setWidth:size.width()];
  [mtl_tex_desc setHeight:size.height()];
  [mtl_tex_desc setDepth:1];
  [mtl_tex_desc setMipmapLevelCount:1];
  [mtl_tex_desc setArrayLength:1];
  [mtl_tex_desc setSampleCount:1];
  // TODO(https://crbug.com/952063): For zero-copy resources that are populated
  // on the CPU (e.g, video frames), it may be that MTLStorageModeManaged will
  // be more appropriate.
#if BUILDFLAG(IS_IOS)
  // On iOS we are using IOSurfaces which must use MTLStorageModeShared.
  [mtl_tex_desc setStorageMode:MTLStorageModeShared];
#else
  [mtl_tex_desc setStorageMode:MTLStorageModePrivate];
#endif
  mtl_texture.reset([mtl_device newTextureWithDescriptor:mtl_tex_desc
                                               iosurface:io_surface
                                                   plane:plane_index]);
  DCHECK(mtl_texture);
  return mtl_texture;
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// IOSurfaceBackingEGLState

IOSurfaceBackingEGLState::IOSurfaceBackingEGLState(
    Client* client,
    EGLDisplay egl_display,
    GLuint gl_target,
    std::vector<scoped_refptr<gles2::TexturePassthrough>> gl_textures)
    : client_(client),
      egl_display_(egl_display),
      gl_target_(gl_target),
      gl_textures_(std::move(gl_textures)) {
  client_->IOSurfaceBackingEGLStateBeingCreated(this);
}

IOSurfaceBackingEGLState::~IOSurfaceBackingEGLState() {
  client_->IOSurfaceBackingEGLStateBeingDestroyed(this, !context_lost_);
  DCHECK(gl_textures_.empty());
}

GLuint IOSurfaceBackingEGLState::GetGLServiceId(int plane_index) const {
  return GetGLTexture(plane_index)->service_id();
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
  return egl_state_->GetGLTexture(plane_index);
}

bool GLTextureIOSurfaceRepresentation::BeginAccess(GLenum mode) {
  DCHECK(mode_ == 0);
  mode_ = mode;
  bool readonly = mode_ != GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM;
  return egl_state_->BeginAccess(readonly);
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
    std::vector<sk_sp<SkPromiseImageTexture>> promise_textures,
    MemoryTypeTracker* tracker)
    : SkiaImageRepresentation(manager, backing, tracker),
      egl_state_(egl_state),
      context_state_(std::move(context_state)),
      promise_textures_(promise_textures) {
  DCHECK(!promise_textures_.empty());
#if DCHECK_IS_ON()
  if (context_state_->GrContextIsGL())
    context_ = gl::GLContext::GetCurrent();
#endif
}

SkiaIOSurfaceRepresentation::~SkiaIOSurfaceRepresentation() {
  if (!write_surfaces_.empty()) {
    DLOG(ERROR) << "SkiaImageRepresentation was destroyed while still "
                << "open for write access.";
  }
  promise_textures_.clear();
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

  if (!write_surfaces_.empty()) {
    return {};
  }

  if (promise_textures_.empty()) {
    return {};
  }

  DCHECK_EQ(static_cast<int>(promise_textures_.size()),
            format().NumberOfPlanes());
  std::vector<sk_sp<SkSurface>> surfaces;
  for (int plane_index = 0; plane_index < format().NumberOfPlanes();
       plane_index++) {
    // Use the color type per plane for multiplanar formats.
    SkColorType sk_color_type = viz::ToClosestSkColorType(
        /*gpu_compositing=*/true, format(), plane_index);
    // Gray is not a renderable single channel format, but alpha is.
    if (sk_color_type == kGray_8_SkColorType) {
      sk_color_type = kAlpha_8_SkColorType;
    }
    auto surface = SkSurface::MakeFromBackendTexture(
        context_state_->gr_context(),
        promise_textures_[plane_index]->backendTexture(), surface_origin(),
        final_msaa_count, sk_color_type,
        backing()->color_space().GetAsFullRangeRGB().ToSkColorSpace(),
        &surface_props);
    if (!surface) {
      return {};
    }
    surfaces.push_back(surface);
  }

  write_surfaces_ = surfaces;
  return surfaces;
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
  if (promise_textures_.empty()) {
    return {};
  }
  return promise_textures_;
}

void SkiaIOSurfaceRepresentation::EndWriteAccess() {
  for (auto& surface : write_surfaces_) {
    DCHECK(surface->unique());
  }

  CheckContext();
  write_surfaces_.clear();

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
  if (promise_textures_.empty()) {
    return {};
  }
  return promise_textures_;
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
  gl::GLDisplayEGL* display = gl::GLDisplayEGL::GetDisplayForCurrentContext();
  if (display) {
    eglWaitUntilWorkScheduledANGLE(display->GetDisplay());
  }

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

#if BUILDFLAG(USE_DAWN)
///////////////////////////////////////////////////////////////////////////////
// DawnIOSurfaceRepresentation

DawnIOSurfaceRepresentation::DawnIOSurfaceRepresentation(
    SharedImageManager* manager,
    SharedImageBacking* backing,
    MemoryTypeTracker* tracker,
    WGPUDevice device,
    base::ScopedCFTypeRef<IOSurfaceRef> io_surface,
    WGPUTextureFormat wgpu_format,
    std::vector<WGPUTextureFormat> view_formats)
    : DawnImageRepresentation(manager, backing, tracker),
      io_surface_(std::move(io_surface)),
      device_(device),
      wgpu_format_(wgpu_format),
      view_formats_(std::move(view_formats)),
      dawn_procs_(dawn::native::GetProcs()) {
  DCHECK(device_);
  DCHECK(io_surface_);

  // Keep a reference to the device so that it stays valid (it might become
  // lost in which case operations will be noops).
  dawn_procs_.deviceReference(device_);
}

DawnIOSurfaceRepresentation::~DawnIOSurfaceRepresentation() {
  EndAccess();
  dawn_procs_.deviceRelease(device_);
}

WGPUTexture DawnIOSurfaceRepresentation::BeginAccess(WGPUTextureUsage usage) {
  WGPUTextureDescriptor texture_descriptor = {};
  texture_descriptor.format = wgpu_format_;
  texture_descriptor.usage = usage;
  texture_descriptor.dimension = WGPUTextureDimension_2D;
  texture_descriptor.size = {static_cast<uint32_t>(size().width()),
                             static_cast<uint32_t>(size().height()), 1};
  texture_descriptor.mipLevelCount = 1;
  texture_descriptor.sampleCount = 1;
  texture_descriptor.viewFormatCount =
      static_cast<uint32_t>(view_formats_.size());
  texture_descriptor.viewFormats = view_formats_.data();

  // We need to have internal usages of CopySrc for copies. If texture is not
  // for video frame import, which has bi-planar format, we also need
  // RenderAttachment usage for clears, and TextureBinding for
  // copyTextureForBrowser.
  WGPUDawnTextureInternalUsageDescriptor internalDesc = {};
  internalDesc.chain.sType = WGPUSType_DawnTextureInternalUsageDescriptor;
  internalDesc.internalUsage =
      WGPUTextureUsage_CopySrc | WGPUTextureUsage_TextureBinding;
  if (wgpu_format_ != WGPUTextureFormat_R8BG8Biplanar420Unorm) {
    internalDesc.internalUsage |= WGPUTextureUsage_RenderAttachment;
  }

  texture_descriptor.nextInChain =
      reinterpret_cast<WGPUChainedStruct*>(&internalDesc);

  dawn::native::metal::ExternalImageDescriptorIOSurface descriptor;
  descriptor.cTextureDescriptor = &texture_descriptor;
  descriptor.isInitialized = IsCleared();
  descriptor.ioSurface = io_surface_.get();
  descriptor.plane = 0;

  // Synchronize with all of the MTLSharedEvents that have been
  // stored in the backing as a consequence of earlier BeginAccess/
  // EndAccess calls against other representations.
  if (gl::GetANGLEImplementation() == gl::ANGLEImplementation::kMetal) {
    if (@available(macOS 10.14, *)) {
      SharedImageBacking* backing = this->backing();
      // Not possible to reach this with any other type of backing.
      DCHECK_EQ(backing->GetType(), SharedImageBackingType::kIOSurface);
      IOSurfaceImageBacking* iosurface_backing =
          static_cast<IOSurfaceImageBacking*>(backing);
      std::vector<std::unique_ptr<SharedEventAndSignalValue>> signals =
          iosurface_backing->TakeSharedEvents();
      for (const auto& signal : signals) {
        dawn::native::metal::ExternalImageMTLSharedEventDescriptor
            external_desc;
        external_desc.sharedEvent =
            static_cast<id<MTLSharedEvent>>(signal->shared_event());
        external_desc.signaledValue = signal->signaled_value();
        descriptor.waitEvents.push_back(external_desc);
      }
    }
  }

  texture_ = dawn::native::metal::WrapIOSurface(device_, &descriptor);
  return texture_;
}

void DawnIOSurfaceRepresentation::EndAccess() {
  if (!texture_) {
    return;
  }

  dawn::native::metal::ExternalImageIOSurfaceEndAccessDescriptor descriptor;
  dawn::native::metal::IOSurfaceEndAccess(texture_, &descriptor);

  if (descriptor.isInitialized) {
    SetCleared();
  }

  if (gl::GetANGLEImplementation() == gl::ANGLEImplementation::kMetal) {
    if (@available(macOS 10.14, *)) {
      SharedImageBacking* backing = this->backing();
      // Not possible to reach this with any other type of backing.
      DCHECK_EQ(backing->GetType(), SharedImageBackingType::kIOSurface);
      IOSurfaceImageBacking* iosurface_backing =
          static_cast<IOSurfaceImageBacking*>(backing);
      // Dawn's Metal backend has enqueued a MTLSharedEvent which
      // consumers of the IOSurface must wait upon before attempting to
      // use that IOSurface on another MTLDevice. Store this event in
      // the underlying SharedImageBacking.
      iosurface_backing->AddSharedEventAndSignalValue(descriptor.sharedEvent,
                                                      descriptor.signaledValue);
    }
  }

  // All further operations on the textures are errors (they would be racy
  // with other backings).
  dawn_procs_.textureDestroy(texture_);

  // TODO(b/252731382): the following WaitForCommandsToBeScheduled call should
  // no longer be necessary, but for some reason it is. Removing it
  // reintroduces intermittent renders of black frames to the WebGPU canvas.
  // This points to another synchronization bug not resolved by the use of
  // MTLSharedEvent between Dawn and ANGLE's Metal backend.
  //
  // macOS has a global GPU command queue so synchronization between APIs and
  // devices is automatic. However on Metal, wgpuQueueSubmit "commits" the
  // Metal command buffers but they aren't "scheduled" in the global queue
  // immediately. (that work seems offloaded to a different thread?)
  // Wait for all the previous submitted commands to be scheduled to have
  // scheduling races between commands using the IOSurface on different APIs.
  // This is a blocking call but should be almost instant.
  TRACE_EVENT0("gpu", "DawnIOSurfaceRepresentation::EndAccess");
  dawn::native::metal::WaitForCommandsToBeScheduled(device_);

  dawn_procs_.textureRelease(texture_);
  texture_ = nullptr;
}
#endif  // BUILDFLAG(USE_DAWN)

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
    : SharedImageBacking(mailbox,
                         format,
                         size,
                         color_space,
                         surface_origin,
                         alpha_type,
                         usage,
                         format.EstimatedSizeInBytes(size),
                         false /* is_thread_safe */),
      io_surface_(std::move(io_surface)),
      io_surface_plane_(io_surface_plane),
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

// iOS uses Metal and doesn't need to retain the GL texture.
#if !BUILDFLAG(IS_IOS)
  // NOTE: Mac currently retains GLTexture and reuses it. Not sure if this is
  // best approach as it can lead to issues with context losses.
  egl_state_for_legacy_mailbox_ = RetainGLTexture();
#endif
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

  std::vector<scoped_refptr<gles2::TexturePassthrough>> gl_textures;
  for (int plane_index = 0; plane_index < format().NumberOfPlanes();
       plane_index++) {
    // Allocate the GL texture.
    scoped_refptr<gles2::TexturePassthrough> gl_texture;
    GLTextureImageBackingHelper::MakeTextureAndSetParameters(
        gl_target_, framebuffer_attachment_angle_, &gl_texture, nullptr);
    // Set the IOSurface to be initially unbound from the GL texture.
    gl_texture->SetEstimatedSize(GetEstimatedSize());
    gl_texture->set_bind_pending();
    gl_textures.push_back(std::move(gl_texture));
  }

  return new IOSurfaceBackingEGLState(this, egl_display, gl_target_,
                                      std::move(gl_textures));
}

void IOSurfaceImageBacking::ReleaseGLTexture(
    IOSurfaceBackingEGLState* egl_state,
    bool have_context) {
  DCHECK_EQ(static_cast<int>(egl_state->gl_textures_.size()),
            format().NumberOfPlanes());
  DCHECK(egl_state->egl_surfaces_.empty() ||
         static_cast<int>(egl_state->egl_surfaces_.size()) ==
             format().NumberOfPlanes());
  if (!egl_state->gl_textures_.empty()) {
    if (have_context) {
      for (int plane_index = 0; plane_index < format().NumberOfPlanes();
           plane_index++) {
        ScopedRestoreTexture scoped_restore(
            gl::g_current_gl_context, egl_state->GetGLTarget(),
            egl_state->GetGLServiceId(plane_index));
        if (!egl_state->egl_surfaces_.empty()) {
          egl_state->egl_surfaces_[plane_index].reset();
        }
      }
      egl_state->egl_surfaces_.clear();
    } else {
      for (const auto& texture : egl_state->gl_textures_) {
        texture->MarkContextLost();
      }
    }
    egl_state->gl_textures_.clear();
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

base::trace_event::MemoryAllocatorDump* IOSurfaceImageBacking::OnMemoryDump(
    const std::string& dump_name,
    base::trace_event::MemoryAllocatorDumpGuid client_guid,
    base::trace_event::ProcessMemoryDump* pmd,
    uint64_t client_tracing_id) {
  auto* dump = SharedImageBacking::OnMemoryDump(dump_name, client_guid, pmd,
                                                client_tracing_id);

  size_t size_bytes = 0u;
  if (format().is_single_plane()) {
    size_bytes =
        IOSurfaceGetBytesPerRowOfPlane(io_surface_, io_surface_plane_) *
        IOSurfaceGetHeightOfPlane(io_surface_, io_surface_plane_);
  } else {
    for (int plane = 0; plane < format().NumberOfPlanes(); plane++) {
      size_bytes += IOSurfaceGetBytesPerRowOfPlane(io_surface_, plane) *
                    IOSurfaceGetHeightOfPlane(io_surface_, plane);
    }
  }

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

  return dump;
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
#if BUILDFLAG(USE_DAWN)
  // See comments in IOSurfaceImageBackingFactory::CreateSharedImage
  // regarding RGBA versus BGRA.
  viz::SharedImageFormat actual_format = format();
  if (actual_format == viz::SinglePlaneFormat::kRGBA_8888) {
    actual_format = viz::SinglePlaneFormat::kBGRA_8888;
  }

  // TODO(crbug.com/1293514): Remove this if condition after using single
  // multiplanar mailbox and actual_format could report multiplanar format
  // correctly.
  if (IOSurfaceGetPixelFormat(io_surface_) == '420v') {
    actual_format = viz::SharedImageFormat::SinglePlane(viz::YUV_420_BIPLANAR);
  }

  absl::optional<WGPUTextureFormat> wgpu_format = ToWGPUFormat(actual_format);
  if (wgpu_format.value() == WGPUTextureFormat_Undefined) {
    return nullptr;
  }

  return std::make_unique<DawnIOSurfaceRepresentation>(
      manager, this, tracker, device, io_surface_, wgpu_format.value(),
      std::move(view_formats));
#else   // BUILDFLAG(USE_DAWN)
  if (!factory()) {
    DLOG(ERROR) << "No SharedImageFactory to create a dawn representation.";
    return nullptr;
  }

  return GLTextureImageBackingHelper::ProduceDawnCommon(
      factory(), manager, tracker, device, backend_type,
      std::move(view_formats), this,
      /*use_passthrough=*/true);
#endif  // BUILDFLAG(USE_DAWN)
}

std::unique_ptr<SkiaImageRepresentation> IOSurfaceImageBacking::ProduceSkia(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker,
    scoped_refptr<SharedContextState> context_state) {
  scoped_refptr<IOSurfaceBackingEGLState> egl_state;
  std::vector<sk_sp<SkPromiseImageTexture>> promise_textures;

  if (context_state->GrContextIsGL()) {
    egl_state = RetainGLTexture();
  }

  for (int plane_index = 0; plane_index < format().NumberOfPlanes();
       plane_index++) {
    sk_sp<SkPromiseImageTexture> promise_texture;
    if (context_state->GrContextIsMetal()) {
      int plane = format().is_single_plane() ? io_surface_plane_ : plane_index;
      promise_texture = ProduceSkiaPromiseTextureMetal(context_state, plane);
      DCHECK(promise_texture);
    } else {
      bool angle_rgbx_internal_format = context_state->feature_info()
                                            ->feature_flags()
                                            .angle_rgbx_internal_format;
      GLenum gl_texture_storage_format = TextureStorageFormat(
          format(), angle_rgbx_internal_format, plane_index);
      GrBackendTexture backend_texture;
      auto plane_size = format().GetPlaneSize(plane_index, size());
      GetGrBackendTexture(
          context_state->feature_info(), egl_state->GetGLTarget(), plane_size,
          egl_state->GetGLServiceId(plane_index), gl_texture_storage_format,
          context_state->gr_context()->threadSafeProxy(), &backend_texture);
      promise_texture = SkPromiseImageTexture::Make(backend_texture);
    }
    if (!promise_texture) {
      return nullptr;
    }

    promise_textures.push_back(std::move(promise_texture));
  }

  return std::make_unique<SkiaIOSurfaceRepresentation>(
      manager, this, egl_state, std::move(context_state), promise_textures,
      tracker);
}

sk_sp<SkPromiseImageTexture>
IOSurfaceImageBacking::ProduceSkiaPromiseTextureMetal(
    scoped_refptr<SharedContextState> context_state,
    int plane_index) {
  DCHECK(context_state->GrContextIsMetal());
  auto plane_size = format().GetPlaneSize(plane_index, size());

  id<MTLDevice> mtl_device =
      context_state->metal_context_provider()->GetMTLDevice();
  auto mtl_texture = CreateMetalTexture(mtl_device, io_surface_.get(),
                                        plane_size, format(), plane_index);
  DCHECK(mtl_texture);

  GrMtlTextureInfo info;
  info.fTexture.retain(mtl_texture.get());
  auto gr_backend_texture = GrBackendTexture(
      plane_size.width(), plane_size.height(), GrMipMapped::kNo, info);
  return SkPromiseImageTexture::Make(gr_backend_texture);
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

bool IOSurfaceImageBacking::IsPurgeable() const {
  return purgeable_;
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
    for (const auto& texture : iter.second->gl_textures_) {
      texture->set_bind_pending();
    }
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
  bool is_bind_pending = base::ranges::any_of(
      egl_state->gl_textures_,
      [](const auto& texture) { return texture->is_bind_pending(); });
  if (!is_bind_pending) {
    return true;
  }

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

  if (egl_state->egl_surfaces_.empty()) {
    std::vector<std::unique_ptr<gl::ScopedEGLSurfaceIOSurface>> egl_surfaces;
    for (int plane_index = 0; plane_index < format().NumberOfPlanes();
         plane_index++) {
      int plane;
      gfx::BufferFormat buffer_format;
      if (format().is_single_plane()) {
        plane = io_surface_plane_;
        buffer_format = ToBufferFormat(format());
      } else {
        // For multiplanar formats (without external sampler) get planar buffer
        // format.
        plane = plane_index;
        buffer_format = GetBufferFormatForPlane(format(), plane_index);
      }

      auto egl_surface = gl::ScopedEGLSurfaceIOSurface::Create(
          egl_state->egl_display_, egl_state->GetGLTarget(), io_surface_, plane,
          buffer_format);
      if (!egl_surface) {
        LOG(ERROR) << "Failed to create ScopedEGLSurfaceIOSurface.";
        return false;
      }

      egl_surfaces.push_back(std::move(egl_surface));
    }
    egl_state->egl_surfaces_ = std::move(egl_surfaces);
  }

  DCHECK_EQ(static_cast<int>(egl_state->gl_textures_.size()),
            format().NumberOfPlanes());
  DCHECK_EQ(static_cast<int>(egl_state->egl_surfaces_.size()),
            format().NumberOfPlanes());
  for (int plane_index = 0; plane_index < format().NumberOfPlanes();
       plane_index++) {
    ScopedRestoreTexture scoped_restore(gl::g_current_gl_context,
                                        egl_state->GetGLTarget(),
                                        egl_state->GetGLServiceId(plane_index));
    // Un-bind the IOSurface from the GL texture (this will be a no-op if it is
    // not yet bound).
    egl_state->egl_surfaces_[plane_index]->ReleaseTexImage();

    // Bind the IOSurface to the GL texture.
    if (!egl_state->egl_surfaces_[plane_index]->BindTexImage()) {
      LOG(ERROR) << "Failed to bind ScopedEGLSurfaceIOSurface to target";
      return false;
    }

    egl_state->gl_textures_[plane_index]->clear_bind_pending();
  }

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
        if (!egl_state->egl_surfaces_.empty()) {
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

    DCHECK_EQ(static_cast<int>(egl_state->gl_textures_.size()),
              format().NumberOfPlanes());
    DCHECK(egl_state->egl_surfaces_.empty() ||
           static_cast<int>(egl_state->egl_surfaces_.size()) ==
               format().NumberOfPlanes());
    for (int plane_index = 0; plane_index < format().NumberOfPlanes();
         plane_index++) {
      if (!egl_state->gl_textures_[plane_index]->is_bind_pending()) {
        if (!egl_state->egl_surfaces_.empty()) {
          ScopedRestoreTexture scoped_restore(
              gl::g_current_gl_context, egl_state->GetGLTarget(),
              egl_state->GetGLServiceId(plane_index));
          egl_state->egl_surfaces_[plane_index]->ReleaseTexImage();
        }
        egl_state->gl_textures_[plane_index]->set_bind_pending();
      }
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

bool IOSurfaceImageBacking::InitializePixels(
    base::span<const uint8_t> pixel_data) {
  IOReturn r = IOSurfaceLock(io_surface_, kIOSurfaceLockAvoidSync, nullptr);
  DCHECK_EQ(kIOReturnSuccess, r);

  uint8_t* dst_data = reinterpret_cast<uint8_t*>(
      IOSurfaceGetBaseAddressOfPlane(io_surface_, io_surface_plane_));
  size_t dst_stride =
      IOSurfaceGetBytesPerRowOfPlane(io_surface_, io_surface_plane_);
  const size_t src_stride = (BitsPerPixel(format()) / 8) * size().width();

  const uint8_t* src_data = pixel_data.data();
  size_t height = size().height();
  for (size_t y = 0; y < height; ++y) {
    memcpy(dst_data, src_data, src_stride);
    dst_data += dst_stride;
    src_data += src_stride;
  }

  r = IOSurfaceUnlock(io_surface_, 0, nullptr);
  DCHECK_EQ(kIOReturnSuccess, r);
  return true;
}

}  // namespace gpu
