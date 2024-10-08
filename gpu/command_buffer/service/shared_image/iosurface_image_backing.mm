// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "gpu/command_buffer/service/shared_image/iosurface_image_backing.h"

#include <EGL/egl.h>
#import <Metal/Metal.h>
#include <dawn/native/MetalBackend.h>

#include "base/apple/scoped_cftyperef.h"
#include "base/apple/scoped_nsobject.h"
#include "base/memory/scoped_policy.h"
#include "base/trace_event/memory_dump_manager.h"
#include "components/viz/common/gpu/metal_context_provider.h"
#include "components/viz/common/resources/resource_sizes.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "gpu/command_buffer/common/shared_image_trace_utils.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/dawn_context_provider.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image/copy_image_plane.h"
#include "gpu/command_buffer/service/shared_image/dawn_fallback_image_representation.h"
#include "gpu/command_buffer/service/shared_image/iosurface_image_backing_factory.h"
#include "gpu/command_buffer/service/shared_image/shared_image_format_service_utils.h"
#include "gpu/command_buffer/service/shared_image/shared_image_gl_utils.h"
#include "gpu/command_buffer/service/shared_image/skia_graphite_dawn_image_representation.h"
#include "gpu/command_buffer/service/skia_utils.h"
#include "gpu/config/gpu_finch_features.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/gpu/ganesh/GrContextThreadSafeProxy.h"
#include "third_party/skia/include/gpu/ganesh/SkSurfaceGanesh.h"
#include "third_party/skia/include/gpu/graphite/Recorder.h"
#include "third_party/skia/include/gpu/graphite/Surface.h"
#include "third_party/skia/include/private/chromium/GrPromiseImageTexture.h"
#include "ui/gl/egl_surface_io_surface.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_display.h"
#include "ui/gl/gl_fence.h"
#include "ui/gl/gl_gl_api_implementation.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/scoped_binders.h"
#include "ui/gl/scoped_make_current.h"
#include "ui/gl/scoped_restore_texture.h"

namespace gpu {

namespace {
struct ScopedIOSurfaceLock {
  ScopedIOSurfaceLock(IOSurfaceRef iosurface, IOSurfaceLockOptions options)
      : io_surface_(iosurface) {
    IOReturn r = IOSurfaceLock(io_surface_, options, nullptr);
    CHECK_EQ(kIOReturnSuccess, r);
  }
  ~ScopedIOSurfaceLock() {
    IOReturn r = IOSurfaceUnlock(io_surface_, 0, nullptr);
    CHECK_EQ(kIOReturnSuccess, r);
  }

  ScopedIOSurfaceLock(const ScopedIOSurfaceLock&) = delete;
  ScopedIOSurfaceLock& operator=(const ScopedIOSurfaceLock&) = delete;

 private:
  IOSurfaceRef io_surface_;
};

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
  NOTREACHED_IN_MIGRATION();
  return gfx::BufferFormat::RGBA_8888;
}

wgpu::Texture CreateWGPUTexture(wgpu::SharedTextureMemory shared_texture_memory,
                                SharedImageUsageSet shared_image_usage,
                                const gfx::Size& io_surface_size,
                                wgpu::TextureFormat wgpu_format,
                                std::vector<wgpu::TextureFormat> view_formats,
                                wgpu::TextureUsage wgpu_texture_usage,
                                wgpu::TextureUsage internal_usage) {
  const std::string debug_label =
      "IOSurface(" + CreateLabelForSharedImageUsage(shared_image_usage) + ")";

  wgpu::TextureDescriptor texture_descriptor;
  texture_descriptor.label = debug_label.c_str();
  texture_descriptor.format = wgpu_format;
  texture_descriptor.usage =
      static_cast<wgpu::TextureUsage>(wgpu_texture_usage);
  texture_descriptor.dimension = wgpu::TextureDimension::e2D;
  texture_descriptor.size = {static_cast<uint32_t>(io_surface_size.width()),
                             static_cast<uint32_t>(io_surface_size.height()),
                             1};
  texture_descriptor.mipLevelCount = 1;
  texture_descriptor.sampleCount = 1;
  texture_descriptor.viewFormatCount = view_formats.size();
  texture_descriptor.viewFormats = view_formats.data();

  wgpu::DawnTextureInternalUsageDescriptor internalDesc;
  internalDesc.internalUsage = internal_usage;

  texture_descriptor.nextInChain = &internalDesc;

  return shared_texture_memory.CreateTexture(&texture_descriptor);
}

#if BUILDFLAG(SKIA_USE_METAL)

base::apple::scoped_nsprotocol<id<MTLTexture>> CreateMetalTexture(
    id<MTLDevice> mtl_device,
    IOSurfaceRef io_surface,
    const gfx::Size& size,
    viz::SharedImageFormat format,
    int plane_index) {
  TRACE_EVENT0("gpu", "IOSurfaceImageBackingFactory::CreateMetalTexture");
  base::apple::scoped_nsprotocol<id<MTLTexture>> mtl_texture;
  MTLPixelFormat mtl_pixel_format =
      static_cast<MTLPixelFormat>(ToMTLPixelFormat(format, plane_index));
  if (mtl_pixel_format == MTLPixelFormatInvalid) {
    return mtl_texture;
  }

  base::apple::scoped_nsobject<MTLTextureDescriptor> mtl_tex_desc(
      [[MTLTextureDescriptor alloc] init]);
  [mtl_tex_desc.get() setTextureType:MTLTextureType2D];
  [mtl_tex_desc.get()
      setUsage:MTLTextureUsageShaderRead | MTLTextureUsageRenderTarget];
  [mtl_tex_desc.get() setPixelFormat:mtl_pixel_format];
  [mtl_tex_desc.get() setWidth:size.width()];
  [mtl_tex_desc.get() setHeight:size.height()];
  [mtl_tex_desc.get() setDepth:1];
  [mtl_tex_desc.get() setMipmapLevelCount:1];
  [mtl_tex_desc.get() setArrayLength:1];
  [mtl_tex_desc.get() setSampleCount:1];
  // TODO(crbug.com/40622826): For zero-copy resources that are populated
  // on the CPU (e.g, video frames), it may be that MTLStorageModeManaged will
  // be more appropriate.
#if BUILDFLAG(IS_IOS)
  // On iOS we are using IOSurfaces which must use MTLStorageModeShared.
  [mtl_tex_desc.get() setStorageMode:MTLStorageModeShared];
#else
  [mtl_tex_desc.get() setStorageMode:MTLStorageModeManaged];
#endif
  mtl_texture.reset([mtl_device newTextureWithDescriptor:mtl_tex_desc.get()
                                               iosurface:io_surface
                                                   plane:plane_index]);
  DCHECK(mtl_texture);
  return mtl_texture;
}

std::vector<skgpu::graphite::BackendTexture> CreateGraphiteMetalTextures(
    std::vector<base::apple::scoped_nsprotocol<id<MTLTexture>>> mtl_textures,
    const viz::SharedImageFormat format,
    const gfx::Size& size) {
  int num_planes = format.NumberOfPlanes();
  std::vector<skgpu::graphite::BackendTexture> graphite_textures;
  graphite_textures.reserve(num_planes);
  for (int plane = 0; plane < num_planes; plane++) {
    SkISize sk_size = gfx::SizeToSkISize(format.GetPlaneSize(plane, size));
    graphite_textures.emplace_back(skgpu::graphite::BackendTextures::MakeMetal(
        sk_size, mtl_textures[plane].get()));
  }
  return graphite_textures;
}
#endif

class BackpressureMetalSharedEventImpl final
    : public BackpressureMetalSharedEvent {
 public:
  BackpressureMetalSharedEventImpl(
      base::apple::scoped_nsprotocol<id<MTLSharedEvent>> shared_event,
      uint64_t signaled_value)
      : shared_event_(std::move(shared_event)),
        signaled_value_(signaled_value) {}
  ~BackpressureMetalSharedEventImpl() override = default;

  BackpressureMetalSharedEventImpl(
      const BackpressureMetalSharedEventImpl& other) = delete;
  BackpressureMetalSharedEventImpl(BackpressureMetalSharedEventImpl&& other) =
      delete;
  BackpressureMetalSharedEventImpl& operator=(
      const BackpressureMetalSharedEventImpl& other) = delete;

  bool HasCompleted() const override {
    if (shared_event_) {
      return shared_event_.get().signaledValue >= signaled_value_;
    }
    return true;
  }

  id<MTLSharedEvent> shared_event() const { return shared_event_.get(); }

  // This is the value which will be signaled on the associated MTLSharedEvent.
  uint64_t signaled_value() const { return signaled_value_; }

 private:
  base::apple::scoped_nsprotocol<id<MTLSharedEvent>> shared_event_;
  uint64_t signaled_value_;
};

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// IOSurfaceBackingEGLState

IOSurfaceBackingEGLState::IOSurfaceBackingEGLState(
    Client* client,
    EGLDisplay egl_display,
    gl::GLContext* gl_context,
    gl::GLSurface* gl_surface,
    GLuint gl_target,
    std::vector<scoped_refptr<gles2::TexturePassthrough>> gl_textures)
    : client_(client),
      egl_display_(egl_display),
      context_(gl_context),
      surface_(gl_surface),
      gl_target_(gl_target),
      gl_textures_(std::move(gl_textures)) {
  client_->IOSurfaceBackingEGLStateBeingCreated(this);
}

IOSurfaceBackingEGLState::~IOSurfaceBackingEGLState() {
  ui::ScopedMakeCurrent smc(context_.get(), surface_.get());
  client_->IOSurfaceBackingEGLStateBeingDestroyed(this, !context_lost_);
  DCHECK(gl_textures_.empty());
}

GLuint IOSurfaceBackingEGLState::GetGLServiceId(int plane_index) const {
  return GetGLTexture(plane_index)->service_id();
}

bool IOSurfaceBackingEGLState::BeginAccess(bool readonly) {
  gl::GLDisplayEGL* display = gl::GLDisplayEGL::GetDisplayForCurrentContext();
  CHECK(display);
  CHECK(display->GetDisplay() == egl_display_);
  return client_->IOSurfaceBackingEGLStateBeginAccess(this, readonly);
}

void IOSurfaceBackingEGLState::EndAccess(bool readonly) {
  client_->IOSurfaceBackingEGLStateEndAccess(this, readonly);
}

void IOSurfaceBackingEGLState::WillRelease(bool have_context) {
  context_lost_ |= !have_context;
}

///////////////////////////////////////////////////////////////////////////////
// GLTextureIRepresentation
class IOSurfaceImageBacking::GLTextureIRepresentation final
    : public GLTexturePassthroughImageRepresentation {
 public:
  GLTextureIRepresentation(SharedImageManager* manager,
                           SharedImageBacking* backing,
                           scoped_refptr<IOSurfaceBackingEGLState> egl_state,
                           MemoryTypeTracker* tracker)
      : GLTexturePassthroughImageRepresentation(manager, backing, tracker),
        egl_state_(egl_state) {}
  ~GLTextureIRepresentation() override {
    egl_state_->WillRelease(has_context());
    egl_state_.reset();
  }

 private:
  // GLTexturePassthroughImageRepresentation:
  const scoped_refptr<gles2::TexturePassthrough>& GetTexturePassthrough(
      int plane_index) override {
    return egl_state_->GetGLTexture(plane_index);
  }

  bool BeginAccess(GLenum mode) override {
    DCHECK(mode_ == 0);
    mode_ = mode;
    bool readonly = mode_ != GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM;
    return egl_state_->BeginAccess(readonly);
  }

  void EndAccess() override {
    DCHECK(mode_ != 0);
    GLenum current_mode = mode_;
    mode_ = 0;
    egl_state_->EndAccess(current_mode !=
                          GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM);
  }

  scoped_refptr<IOSurfaceBackingEGLState> egl_state_;
  GLenum mode_ = 0;
};

///////////////////////////////////////////////////////////////////////////////
// SkiaGaneshRepresentation

class IOSurfaceImageBacking::SkiaGaneshRepresentation final
    : public SkiaGaneshImageRepresentation {
 public:
  SkiaGaneshRepresentation(
      SharedImageManager* manager,
      SharedImageBacking* backing,
      scoped_refptr<IOSurfaceBackingEGLState> egl_state,
      scoped_refptr<SharedContextState> context_state,
      std::vector<sk_sp<GrPromiseImageTexture>> promise_textures,
      MemoryTypeTracker* tracker);
  ~SkiaGaneshRepresentation() override;

  void SetBeginReadAccessCallback(
      base::RepeatingClosure begin_read_access_callback);

 private:
  // SkiaGaneshImageRepresentation:
  std::vector<sk_sp<SkSurface>> BeginWriteAccess(
      int final_msaa_count,
      const SkSurfaceProps& surface_props,
      const gfx::Rect& update_rect,
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores,
      std::unique_ptr<skgpu::MutableTextureState>* end_state) override;
  std::vector<sk_sp<GrPromiseImageTexture>> BeginWriteAccess(
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphore,
      std::unique_ptr<skgpu::MutableTextureState>* end_state) override;
  void EndWriteAccess() override;
  std::vector<sk_sp<GrPromiseImageTexture>> BeginReadAccess(
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores,
      std::unique_ptr<skgpu::MutableTextureState>* end_state) override;
  void EndReadAccess() override;
  bool SupportsMultipleConcurrentReadAccess() override;

  void CheckContext();

  scoped_refptr<IOSurfaceBackingEGLState> egl_state_;
  scoped_refptr<SharedContextState> context_state_;
  std::vector<sk_sp<GrPromiseImageTexture>> promise_textures_;
  std::vector<sk_sp<SkSurface>> write_surfaces_;
#if DCHECK_IS_ON()
  raw_ptr<gl::GLContext> context_ = nullptr;
#endif
};

IOSurfaceImageBacking::SkiaGaneshRepresentation::SkiaGaneshRepresentation(
    SharedImageManager* manager,
    SharedImageBacking* backing,
    scoped_refptr<IOSurfaceBackingEGLState> egl_state,
    scoped_refptr<SharedContextState> context_state,
    std::vector<sk_sp<GrPromiseImageTexture>> promise_textures,
    MemoryTypeTracker* tracker)
    : SkiaGaneshImageRepresentation(context_state->gr_context(),
                                    manager,
                                    backing,
                                    tracker),
      egl_state_(egl_state),
      context_state_(std::move(context_state)),
      promise_textures_(promise_textures) {
  DCHECK(!promise_textures_.empty());
#if DCHECK_IS_ON()
  if (context_state_->GrContextIsGL())
    context_ = gl::GLContext::GetCurrent();
#endif
}

IOSurfaceImageBacking::SkiaGaneshRepresentation::~SkiaGaneshRepresentation() {
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

std::vector<sk_sp<SkSurface>>
IOSurfaceImageBacking::SkiaGaneshRepresentation::BeginWriteAccess(
    int final_msaa_count,
    const SkSurfaceProps& surface_props,
    const gfx::Rect& update_rect,
    std::vector<GrBackendSemaphore>* begin_semaphores,
    std::vector<GrBackendSemaphore>* end_semaphores,
    std::unique_ptr<skgpu::MutableTextureState>* end_state) {
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
    auto surface = SkSurfaces::WrapBackendTexture(
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

std::vector<sk_sp<GrPromiseImageTexture>>
IOSurfaceImageBacking::SkiaGaneshRepresentation::BeginWriteAccess(
    std::vector<GrBackendSemaphore>* begin_semaphores,
    std::vector<GrBackendSemaphore>* end_semaphores,
    std::unique_ptr<skgpu::MutableTextureState>* end_state) {
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

void IOSurfaceImageBacking::SkiaGaneshRepresentation::EndWriteAccess() {
#if DCHECK_IS_ON()
  for (auto& surface : write_surfaces_) {
    DCHECK(surface->unique());
  }
#endif

  CheckContext();
  write_surfaces_.clear();

  if (egl_state_)
    egl_state_->EndAccess(/*readonly=*/false);
}

std::vector<sk_sp<GrPromiseImageTexture>>
IOSurfaceImageBacking::SkiaGaneshRepresentation::BeginReadAccess(
    std::vector<GrBackendSemaphore>* begin_semaphores,
    std::vector<GrBackendSemaphore>* end_semaphores,
    std::unique_ptr<skgpu::MutableTextureState>* end_state) {
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

void IOSurfaceImageBacking::SkiaGaneshRepresentation::EndReadAccess() {
  if (egl_state_)
    egl_state_->EndAccess(/*readonly=*/true);
}

bool IOSurfaceImageBacking::SkiaGaneshRepresentation::
    SupportsMultipleConcurrentReadAccess() {
  return true;
}

void IOSurfaceImageBacking::SkiaGaneshRepresentation::CheckContext() {
#if DCHECK_IS_ON()
  if (!context_state_->context_lost() && context_)
    DCHECK(gl::GLContext::GetCurrent() == context_);
#endif
}

#if BUILDFLAG(SKIA_USE_METAL)
///////////////////////////////////////////////////////////////////////////////
// SkiaGraphiteRepresentation

class IOSurfaceImageBacking::SkiaGraphiteRepresentation final
    : public SkiaGraphiteImageRepresentation {
 public:
  // Graphite does not keep track of the MetalTexture like Ganesh, so the
  // representation/backing needs to keep the Metal texture alive.
  SkiaGraphiteRepresentation(
      SharedImageManager* manager,
      SharedImageBacking* backing,
      MemoryTypeTracker* tracker,
      skgpu::graphite::Recorder* recorder,
      std::vector<base::apple::scoped_nsprotocol<id<MTLTexture>>> mtl_textures)
      : SkiaGraphiteImageRepresentation(manager, backing, tracker),
        recorder_(recorder),
        mtl_textures_(std::move(mtl_textures)) {
    CHECK_EQ(mtl_textures_.size(), NumPlanesExpected());
  }

  ~SkiaGraphiteRepresentation() override {
    if (!write_surfaces_.empty()) {
      DLOG(ERROR) << "SkiaImageRepresentation was destroyed while still "
                  << "open for write access.";
    }
  }

 private:
  // SkiaGraphiteImageRepresentation:
  std::vector<sk_sp<SkSurface>> BeginWriteAccess(
      const SkSurfaceProps& surface_props,
      const gfx::Rect& update_rect) override;
  std::vector<skgpu::graphite::BackendTexture> BeginWriteAccess() override;
  void EndWriteAccess() override;
  std::vector<skgpu::graphite::BackendTexture> BeginReadAccess() override;
  void EndReadAccess() override;

  IOSurfaceImageBacking* backing_impl() const {
    return static_cast<IOSurfaceImageBacking*>(backing());
  }

  const raw_ptr<skgpu::graphite::Recorder> recorder_;
  std::vector<base::apple::scoped_nsprotocol<id<MTLTexture>>> mtl_textures_;
  std::vector<sk_sp<SkSurface>> write_surfaces_;
};

std::vector<sk_sp<SkSurface>>
IOSurfaceImageBacking::SkiaGraphiteRepresentation::BeginWriteAccess(
    const SkSurfaceProps& surface_props,
    const gfx::Rect& update_rect) {
  if (!write_surfaces_.empty()) {
    // Write access is already in progress.
    return {};
  }

  if (!backing_impl()->BeginAccess(/*readonly=*/false)) {
    return {};
  }

  int num_planes = format().NumberOfPlanes();
  write_surfaces_.reserve(num_planes);
  for (int plane = 0; plane < num_planes; plane++) {
    SkColorType sk_color_type = viz::ToClosestSkColorType(
        /*gpu_compositing=*/true, format(), plane);
    // Gray is not a renderable single channel format, but alpha is.
    if (sk_color_type == kGray_8_SkColorType) {
      sk_color_type = kAlpha_8_SkColorType;
    }
    SkISize sk_size = gfx::SizeToSkISize(format().GetPlaneSize(plane, size()));

    auto backend_texture = skgpu::graphite::BackendTextures::MakeMetal(
        sk_size, mtl_textures_[plane].get());
    auto surface = SkSurfaces::WrapBackendTexture(
        recorder_, backend_texture, sk_color_type,
        backing()->color_space().GetAsFullRangeRGB().ToSkColorSpace(),
        &surface_props);
    write_surfaces_.emplace_back(std::move(surface));
  }
  return write_surfaces_;
}

std::vector<skgpu::graphite::BackendTexture>
IOSurfaceImageBacking::SkiaGraphiteRepresentation::BeginWriteAccess() {
  if (!backing_impl()->BeginAccess(/*readonly=*/false)) {
    return {};
  }
  return CreateGraphiteMetalTextures(mtl_textures_, format(), size());
}

void IOSurfaceImageBacking::SkiaGraphiteRepresentation::EndWriteAccess() {
#if DCHECK_IS_ON()
  for (auto& surface : write_surfaces_) {
    DCHECK(surface->unique());
  }
#endif
  write_surfaces_.clear();
  backing_impl()->EndAccess(/*readonly=*/false);
}

std::vector<skgpu::graphite::BackendTexture>
IOSurfaceImageBacking::SkiaGraphiteRepresentation::BeginReadAccess() {
  if (!backing_impl()->BeginAccess(/*readonly=*/true)) {
    return {};
  }
  return CreateGraphiteMetalTextures(mtl_textures_, format(), size());
}

void IOSurfaceImageBacking::SkiaGraphiteRepresentation::EndReadAccess() {
  backing_impl()->EndAccess(/*readonly=*/true);
}
#endif

///////////////////////////////////////////////////////////////////////////////
// OverlayRepresentation

class IOSurfaceImageBacking::OverlayRepresentation final
    : public OverlayImageRepresentation {
 public:
  OverlayRepresentation(SharedImageManager* manager,
                        SharedImageBacking* backing,
                        MemoryTypeTracker* tracker,
                        gfx::ScopedIOSurface io_surface)
      : OverlayImageRepresentation(manager, backing, tracker),
        io_surface_(std::move(io_surface)) {}
  ~OverlayRepresentation() override = default;

 private:
  bool BeginReadAccess(gfx::GpuFenceHandle& acquire_fence) override;
  void EndReadAccess(gfx::GpuFenceHandle release_fence) override;
  gfx::ScopedIOSurface GetIOSurface() const override;
  bool IsInUseByWindowServer() const override;

  gfx::ScopedIOSurface io_surface_;
};

bool IOSurfaceImageBacking::OverlayRepresentation::BeginReadAccess(
    gfx::GpuFenceHandle& acquire_fence) {
  auto* iosurface_backing = static_cast<IOSurfaceImageBacking*>(backing());

  if (!iosurface_backing->BeginAccess(/*readonly=*/true)) {
    return false;
  }

  // This will transition the image to be accessed by CoreAnimation. So
  // WaitForANGLECommandsToBeScheduled() call is required.
  iosurface_backing->WaitForANGLECommandsToBeScheduled();

  // Likewise do the same for Dawn's commands.
  iosurface_backing->WaitForDawnCommandsToBeScheduled(
      /*device_to_exclude=*/nullptr);

  gl::GLContext* context = gl::GLContext::GetCurrent();
  if (context) {
    const auto& signals = static_cast<IOSurfaceImageBacking*>(backing())
                              ->exclusive_shared_events_;
    std::vector<std::unique_ptr<BackpressureMetalSharedEvent>>
        backpressure_events;
    for (const auto& [shared_event, signaled_value] : signals) {
      backpressure_events.push_back(
          std::make_unique<BackpressureMetalSharedEventImpl>(shared_event,
                                                             signaled_value));
    }
    context->AddMetalSharedEventsForBackpressure(
        std::move(backpressure_events));
  }

  return true;
}

void IOSurfaceImageBacking::OverlayRepresentation::EndReadAccess(
    gfx::GpuFenceHandle release_fence) {
  DCHECK(release_fence.is_null());
  static_cast<IOSurfaceImageBacking*>(backing())->EndAccess(/*readonly=*/true);
}

gfx::ScopedIOSurface
IOSurfaceImageBacking::OverlayRepresentation::GetIOSurface() const {
  return io_surface_;
}

bool IOSurfaceImageBacking::OverlayRepresentation::IsInUseByWindowServer()
    const {
  // IOSurfaceIsInUse() will always return true if the IOSurface is wrapped in
  // a CVPixelBuffer. Ignore the signal for such IOSurfaces (which are the
  // ones output by hardware video decode and video capture).
  if (backing()->usage() & SHARED_IMAGE_USAGE_MACOS_VIDEO_TOOLBOX) {
    return false;
  }

  return IOSurfaceIsInUse(io_surface_.get());
}

///////////////////////////////////////////////////////////////////////////////
// DawnRepresentation

class IOSurfaceImageBacking::DawnRepresentation final
    : public DawnImageRepresentation {
 public:
  DawnRepresentation(SharedImageManager* manager,
                     SharedImageBacking* backing,
                     MemoryTypeTracker* tracker,
                     wgpu::Device device,
                     wgpu::SharedTextureMemory shared_texture_memory,
                     const gfx::Size& io_surface_size,
                     wgpu::TextureFormat wgpu_format,
                     std::vector<wgpu::TextureFormat> view_formats)
      : DawnImageRepresentation(manager, backing, tracker),
        device_(std::move(device)),
        shared_texture_memory_(shared_texture_memory),
        io_surface_size_(io_surface_size),
        wgpu_format_(wgpu_format),
        view_formats_(std::move(view_formats)) {
    CHECK(device_);
    CHECK(device_.HasFeature(wgpu::FeatureName::SharedTextureMemoryIOSurface));
    CHECK(shared_texture_memory);
  }
  ~DawnRepresentation() override { EndAccess(); }

  wgpu::Texture BeginAccess(wgpu::TextureUsage usage,
                            wgpu::TextureUsage internal_usage) final;
  void EndAccess() final;
  bool SupportsMultipleConcurrentReadAccess() final;

 private:
  static constexpr wgpu::TextureUsage kReadOnlyUsage =
      wgpu::TextureUsage::CopySrc | wgpu::TextureUsage::TextureBinding;
  const wgpu::Device device_;
  wgpu::SharedTextureMemory shared_texture_memory_;
  const gfx::Size io_surface_size_;
  const wgpu::TextureFormat wgpu_format_;
  const std::vector<wgpu::TextureFormat> view_formats_;

  // NOTE: `usage_`, `internal_usage_`, and `texture_` are valid only within
  // the duration of a BeginAccess()/EndAccess() pair.
  wgpu::TextureUsage usage_;
  wgpu::TextureUsage internal_usage_;
  wgpu::Texture texture_;
};

wgpu::Texture IOSurfaceImageBacking::DawnRepresentation::BeginAccess(
    wgpu::TextureUsage wgpu_texture_usage,
    wgpu::TextureUsage internal_usage) {
  const bool readonly = (wgpu_texture_usage & ~kReadOnlyUsage) == 0 &&
                        (internal_usage & ~kReadOnlyUsage) == 0;

  IOSurfaceImageBacking* iosurface_backing =
      static_cast<IOSurfaceImageBacking*>(backing());
  if (!iosurface_backing->BeginAccess(readonly)) {
    return {};
  }

  // IOSurface might be written on a different GPU. We need to wait for
  // previous Dawn and ANGLE commands to be scheduled first.
  // Note: we don't need to wait for the commands from the same wgpu::Device to
  // be scheduled.
  // TODO(crbug.com/40260114): Skip this if we're not on a dual-GPU system.
  iosurface_backing->WaitForANGLECommandsToBeScheduled();
  iosurface_backing->WaitForDawnCommandsToBeScheduled(
      /*device_to_exclude=*/device_);

  usage_ = wgpu_texture_usage;
  internal_usage_ = internal_usage;

  texture_ = iosurface_backing->GetDawnTextureHolder()->GetCachedWGPUTexture(
      device_, usage_);
  if (!texture_) {
    texture_ = CreateWGPUTexture(shared_texture_memory_, usage(),
                                 io_surface_size_, wgpu_format_, view_formats_,
                                 wgpu_texture_usage, internal_usage);
    iosurface_backing->GetDawnTextureHolder()->MaybeCacheWGPUTexture(device_,
                                                                     texture_);
  }

  // If there is already an ongoing Dawn access for this texture, then the
  // necessary work for starting the access (i.e., waiting on fences and
  // informing SharedTextureMemory) already happened as part of the initial
  // BeginAccess().
  // NOTE: SharedTextureMemory does not allow a BeginAccess() call on a texture
  // that already has an ongoing access (at the internal wgpu::Texture
  // level), so short-circuiting out here is not simply an optimization but
  // is actually necessary.
  int num_accesses_already_present =
      iosurface_backing->TrackBeginAccessToWGPUTexture(texture_);
  if (num_accesses_already_present > 0) {
    return texture_;
  }

  wgpu::SharedTextureMemoryBeginAccessDescriptor begin_access_desc = {};
  begin_access_desc.initialized = IsCleared();

  // NOTE: WebGPU allows reads of uncleared textures, in which case Dawn clears
  // the texture on its initial access. Such reads must take exclusive access.
  begin_access_desc.concurrentRead = readonly && IsCleared();

  std::vector<wgpu::SharedFence> shared_fences;
  std::vector<uint64_t> signaled_values;

  // Synchronize with all of the MTLSharedEvents that have been
  // stored in the backing as a consequence of earlier BeginAccess/
  // EndAccess calls against other representations.
  if (gl::GetANGLEImplementation() == gl::ANGLEImplementation::kMetal) {
    // Not possible to reach this with any other type of backing.
    DCHECK_EQ(backing()->GetType(), SharedImageBackingType::kIOSurface);

    iosurface_backing->ProcessSharedEventsForBeginAccess(
        readonly,
        [&](id<MTLSharedEvent> shared_event, uint64_t signaled_value) {
          wgpu::SharedFenceMTLSharedEventDescriptor shared_event_desc;
          shared_event_desc.sharedEvent = shared_event;

          wgpu::SharedFenceDescriptor fence_desc;
          fence_desc.nextInChain = &shared_event_desc;

          shared_fences.push_back(device_.ImportSharedFence(&fence_desc));
          signaled_values.push_back(signaled_value);
        });
  }

  // Populate `begin_access_desc` with the fence data.
  CHECK(shared_fences.size() == signaled_values.size());
  begin_access_desc.fenceCount = shared_fences.size();
  begin_access_desc.fences = shared_fences.data();
  begin_access_desc.signaledValues = signaled_values.data();

  if (shared_texture_memory_.BeginAccess(texture_, &begin_access_desc) !=
      wgpu::Status::Success) {
    // NOTE: WebGPU CTS tests intentionally pass in formats that are
    // incompatible with the format of the backing IOSurface to check error
    // handling.
    LOG(ERROR) << "SharedTextureMemory::BeginAccess() failed";
    iosurface_backing->TrackEndAccessToWGPUTexture(texture_);
    iosurface_backing->GetDawnTextureHolder()->RemoveWGPUTextureFromCache(
        device_, texture_);
    texture_ = {};

    iosurface_backing->EndAccess(readonly);
  }

  return texture_.Get();
}

void IOSurfaceImageBacking::DawnRepresentation::EndAccess() {
  if (!texture_) {
    // The only valid cases in which this could occur are (a) if
    // SharedTextureMemory::BeginAccess() failed, in which case we already
    // called EndAccess() on the backing when we detected the failure, or (b)
    // this is a call from the destructor after another EndAccess() had already
    // been made, in which case we already executed the below code on the first
    // call (resulting in setting `texture_` to null).
    return;
  }

  // Inform the backing that an access has ended so that it can properly update
  // its state tracking.
  IOSurfaceImageBacking* iosurface_backing =
      static_cast<IOSurfaceImageBacking*>(backing());
  const bool readonly = (usage_ & ~kReadOnlyUsage) == 0 &&
                        (internal_usage_ & ~kReadOnlyUsage) == 0;
  iosurface_backing->EndAccess(readonly);
  int num_outstanding_accesses =
      iosurface_backing->TrackEndAccessToWGPUTexture(texture_);

  // However, if there is still an ongoing Dawn access on this texture,
  // short-circuit out of doing any other work. In particular, do not consume
  // fences or end the access at the level of SharedTextureMemory. That work
  // will happen when the last ongoing Dawn access finishes.
  if (num_outstanding_accesses > 0) {
    texture_ = nullptr;
    usage_ = internal_usage_ = wgpu::TextureUsage::None;
    return;
  }

  wgpu::SharedTextureMemoryEndAccessState end_access_desc;
  CHECK_EQ(shared_texture_memory_.EndAccess(texture_.Get(), &end_access_desc),
           wgpu::Status::Success);

  if (end_access_desc.initialized) {
    SetCleared();
  }

  // Not possible to reach this with any other type of backing.
  DCHECK_EQ(backing()->GetType(), SharedImageBackingType::kIOSurface);

  // Dawn's Metal backend has enqueued MTLSharedEvents which consumers of the
  // IOSurface must wait upon before attempting to use that IOSurface on
  // another MTLDevice. Store these events in the underlying
  // SharedImageBacking.
  for (size_t i = 0; i < end_access_desc.fenceCount; i++) {
    auto fence = end_access_desc.fences[i];
    auto signaled_value = end_access_desc.signaledValues[i];

    wgpu::SharedFenceExportInfo fence_export_info;
    wgpu::SharedFenceMTLSharedEventExportInfo fence_mtl_export_info;
    fence_export_info.nextInChain = &fence_mtl_export_info;
    fence.ExportInfo(&fence_export_info);
    auto shared_event =
        static_cast<id<MTLSharedEvent>>(fence_mtl_export_info.sharedEvent);
    iosurface_backing->AddSharedEventForEndAccess(shared_event, signaled_value,
                                                  readonly);
  }

  iosurface_backing->GetDawnTextureHolder()->DestroyWGPUTextureIfNotCached(
      device_, texture_);

  if (end_access_desc.fenceCount > 0) {
    // For write access, we would need to WaitForCommandsToBeScheduled
    // before the image is used by CoreAnimation or WebGL later.
    // However, we defer the wait on this device until CoreAnimation
    // or WebGL actually needs to access the image. This could avoid repeated
    // and unnecessary waits.
    // TODO(b/328411251): Investigate whether this is needed if the access
    // is readonly.
    iosurface_backing->AddWGPUDeviceWithPendingCommands(device_);
  }

  texture_ = nullptr;
  usage_ = internal_usage_ = wgpu::TextureUsage::None;
}

// Enabling this functionality reduces overhead in the compositor by lowering
// the frequency of begin/end access pairs. The semantic constraints for a
// representation being able to return true are the following:
// * It is valid to call BeginScopedReadAccess() concurrently on two
//   different representations of the same image
// * The backing supports true concurrent read access rather than emulating
//   concurrent reads by "pausing" a first read when a second read of a
//   different representation type begins, which requires that the second
//   representation's read finish within the scope of its GPU task in order
//   to ensure that nothing actually accesses the first representation
//   while it is paused. Some backings that support only exclusive access
//   from the SI perspective do the latter (e.g.,
//   ExternalVulkanImageBacking as its "support" of concurrent GL and
//   Vulkan access). SupportsMultipleConcurrentReadAccess() results in the
//   compositor's read access being long-lived (i.e., beyond the scope of
//   a single GPU task).
// The Graphite Skia representation returns true if the underlying Dawn
// representation does so. This representation meets both of the above
// constraints.
bool IOSurfaceImageBacking::DawnRepresentation::
    SupportsMultipleConcurrentReadAccess() {
  return true;
}

///////////////////////////////////////////////////////////////////////////////
// IOSurfaceImageBacking

IOSurfaceImageBacking::IOSurfaceImageBacking(
    gfx::ScopedIOSurface io_surface,
    gfx::GenericSharedMemoryId io_surface_id,
    const Mailbox& mailbox,
    viz::SharedImageFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    gpu::SharedImageUsageSet usage,
    std::string debug_label,
    GLenum gl_target,
    bool framebuffer_attachment_angle,
    bool is_cleared,
    GrContextType gr_context_type,
    std::optional<gfx::BufferUsage> buffer_usage)
    : SharedImageBacking(mailbox,
                         format,
                         size,
                         color_space,
                         surface_origin,
                         alpha_type,
                         usage,
                         std::move(debug_label),
                         format.EstimatedSizeInBytes(size),
                         /*is_thread_safe=*/false,
                         std::move(buffer_usage)),
      io_surface_(std::move(io_surface)),
      io_surface_size_(IOSurfaceGetWidth(io_surface_.get()),
                       IOSurfaceGetHeight(io_surface_.get())),
      io_surface_format_(IOSurfaceGetPixelFormat(io_surface_.get())),
      io_surface_id_(io_surface_id),
      dawn_texture_holder_(std::make_unique<DawnSharedTextureHolder>()),
      gl_target_(gl_target),
      framebuffer_attachment_angle_(framebuffer_attachment_angle),
      cleared_rect_(is_cleared ? gfx::Rect(size) : gfx::Rect()),
      gr_context_type_(gr_context_type),
      weak_factory_(this) {
  CHECK(io_surface_);

  // If this will be bound to different GL backends, then make RetainGLTexture
  // and ReleaseGLTexture actually create and destroy the texture.
  // https://crbug.com/1251724
  if (usage & SHARED_IMAGE_USAGE_HIGH_PERFORMANCE_GPU) {
    return;
  }

  // NOTE: Mac currently retains GLTexture and reuses it. This might lead to
  // issues with context losses, but is also beneficial to performance at
  // least on perf benchmarks.
  if (gr_context_type == GrContextType::kGL) {
    // NOTE: We do not CHECK here that the current GL context is that of the
    // SharedContextState due to not having easy access to the
    // SharedContextState here. However, all codepaths that create SharedImage
    // backings make the SharedContextState's context current before doing so.
    egl_state_for_skia_gl_context_ = RetainGLTexture();
  }
}

IOSurfaceImageBacking::~IOSurfaceImageBacking() {
  if (egl_state_for_skia_gl_context_) {
    egl_state_for_skia_gl_context_->WillRelease(have_context());
    egl_state_for_skia_gl_context_ = nullptr;
  }
  DCHECK(egl_state_map_.empty());
}

bool IOSurfaceImageBacking::ReadbackToMemory(
    const std::vector<SkPixmap>& pixmaps) {
  CHECK_LE(pixmaps.size(), 3u);

  // Make sure any pending ANGLE EGLDisplays and Dawn devices are flushed.
  WaitForANGLECommandsToBeScheduled();
  WaitForDawnCommandsToBeScheduled(/*device_to_exclude=*/nullptr);

  ScopedIOSurfaceLock io_surface_lock(io_surface_.get(), /*options=*/0);

  for (int plane_index = 0; plane_index < static_cast<int>(pixmaps.size());
       ++plane_index) {
    const gfx::Size plane_size = format().GetPlaneSize(plane_index, size());

    const void* io_surface_base_address =
        IOSurfaceGetBaseAddressOfPlane(io_surface_.get(), plane_index);
    DCHECK_EQ(plane_size.width(), static_cast<int>(IOSurfaceGetWidthOfPlane(
                                      io_surface_.get(), plane_index)));
    DCHECK_EQ(plane_size.height(), static_cast<int>(IOSurfaceGetHeightOfPlane(
                                       io_surface_.get(), plane_index)));

    int io_surface_row_bytes = 0;
    int dst_bytes_per_row = 0;

    base::CheckedNumeric<int> checked_io_surface_row_bytes =
        IOSurfaceGetBytesPerRowOfPlane(io_surface_.get(), plane_index);
    base::CheckedNumeric<int> checked_dst_bytes_per_row =
        pixmaps[plane_index].rowBytes();

    if (!checked_io_surface_row_bytes.AssignIfValid(&io_surface_row_bytes) ||
        !checked_dst_bytes_per_row.AssignIfValid(&dst_bytes_per_row)) {
      return false;
    }

    const uint8_t* src_ptr =
        static_cast<const uint8_t*>(io_surface_base_address);
    uint8_t* dst_ptr =
        static_cast<uint8_t*>(pixmaps[plane_index].writable_addr());

    const int copy_bytes =
        static_cast<int>(pixmaps[plane_index].info().minRowBytes());
    DCHECK_LE(copy_bytes, io_surface_row_bytes);
    DCHECK_LE(copy_bytes, dst_bytes_per_row);

    CopyImagePlane(src_ptr, io_surface_row_bytes, dst_ptr, dst_bytes_per_row,
                   copy_bytes, plane_size.height());
  }

  return true;
}

bool IOSurfaceImageBacking::UploadFromMemory(
    const std::vector<SkPixmap>& pixmaps) {
  CHECK_LE(pixmaps.size(), 3u);

  // Make sure any pending ANGLE EGLDisplays and Dawn devices are flushed.
  WaitForANGLECommandsToBeScheduled();
  WaitForDawnCommandsToBeScheduled(/*device_to_exclude=*/nullptr);

  ScopedIOSurfaceLock io_surface_lock(io_surface_.get(), /*options=*/0);

  for (int plane_index = 0; plane_index < static_cast<int>(pixmaps.size());
       ++plane_index) {
    const gfx::Size plane_size = format().GetPlaneSize(plane_index, size());

    void* io_surface_base_address =
        IOSurfaceGetBaseAddressOfPlane(io_surface_.get(), plane_index);
    DCHECK_EQ(plane_size.width(), static_cast<int>(IOSurfaceGetWidthOfPlane(
                                      io_surface_.get(), plane_index)));
    DCHECK_EQ(plane_size.height(), static_cast<int>(IOSurfaceGetHeightOfPlane(
                                       io_surface_.get(), plane_index)));

    int io_surface_row_bytes = 0;
    int src_bytes_per_row = 0;

    base::CheckedNumeric<int> checked_io_surface_row_bytes =
        IOSurfaceGetBytesPerRowOfPlane(io_surface_.get(), plane_index);
    base::CheckedNumeric<int> checked_src_bytes_per_row =
        pixmaps[plane_index].rowBytes();

    if (!checked_io_surface_row_bytes.AssignIfValid(&io_surface_row_bytes) ||
        !checked_src_bytes_per_row.AssignIfValid(&src_bytes_per_row)) {
      return false;
    }

    const uint8_t* src_ptr =
        static_cast<const uint8_t*>(pixmaps[plane_index].addr());

    const int copy_bytes =
        static_cast<int>(pixmaps[plane_index].info().minRowBytes());
    DCHECK_LE(copy_bytes, src_bytes_per_row);
    DCHECK_LE(copy_bytes, io_surface_row_bytes);

    uint8_t* dst_ptr = static_cast<uint8_t*>(io_surface_base_address);

    CopyImagePlane(src_ptr, src_bytes_per_row, dst_ptr, io_surface_row_bytes,
                   copy_bytes, plane_size.height());
  }

  return true;
}

scoped_refptr<IOSurfaceBackingEGLState>
IOSurfaceImageBacking::RetainGLTexture() {
  gl::GLContext* context = gl::GLContext::GetCurrent();
  gl::GLDisplayEGL* display = context ? context->GetGLDisplayEGL() : nullptr;
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
    MakeTextureAndSetParameters(gl_target_, framebuffer_attachment_angle_,
                                &gl_texture, nullptr);
    // Set the IOSurface to be initially unbound from the GL texture.
    gl_texture->SetEstimatedSize(GetEstimatedSize());
    gl_textures.push_back(std::move(gl_texture));
  }

  scoped_refptr<IOSurfaceBackingEGLState> egl_state =
      new IOSurfaceBackingEGLState(this, egl_display, context,
                                   gl::GLSurface::GetCurrent(), gl_target_,
                                   std::move(gl_textures));
  egl_state->set_bind_pending();
  return egl_state;
}

void IOSurfaceImageBacking::ReleaseGLTexture(
    IOSurfaceBackingEGLState* egl_state,
    bool have_context) {
  DCHECK_EQ(static_cast<int>(egl_state->gl_textures_.size()),
            format().NumberOfPlanes());
  DCHECK(egl_state->egl_surfaces_.empty() ||
         static_cast<int>(egl_state->egl_surfaces_.size()) ==
             format().NumberOfPlanes());
  if (!have_context) {
    for (const auto& texture : egl_state->gl_textures_) {
      texture->MarkContextLost();
    }
  }
  egl_state->gl_textures_.clear();
}

base::trace_event::MemoryAllocatorDump* IOSurfaceImageBacking::OnMemoryDump(
    const std::string& dump_name,
    base::trace_event::MemoryAllocatorDumpGuid client_guid,
    base::trace_event::ProcessMemoryDump* pmd,
    uint64_t client_tracing_id) {
  auto* dump = SharedImageBacking::OnMemoryDump(dump_name, client_guid, pmd,
                                                client_tracing_id);

  size_t size_bytes = 0u;
  for (int plane = 0; plane < format().NumberOfPlanes(); plane++) {
    size_bytes += IOSurfaceGetBytesPerRowOfPlane(io_surface_.get(), plane) *
                  IOSurfaceGetHeightOfPlane(io_surface_.get(), plane);
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
  return std::make_unique<GLTextureIRepresentation>(manager, this,
                                                    RetainGLTexture(), tracker);
}

std::unique_ptr<OverlayImageRepresentation>
IOSurfaceImageBacking::ProduceOverlay(SharedImageManager* manager,
                                      MemoryTypeTracker* tracker) {
  return std::make_unique<OverlayRepresentation>(manager, this, tracker,
                                                 io_surface_);
}

int IOSurfaceImageBacking::TrackBeginAccessToWGPUTexture(
    wgpu::Texture texture) {
  return wgpu_texture_ongoing_accesses_[texture.Get()]++;
}

int IOSurfaceImageBacking::TrackEndAccessToWGPUTexture(wgpu::Texture texture) {
  if (!wgpu_texture_ongoing_accesses_.contains(texture.Get())) {
    return 0;
  }

  int num_outstanding_accesses =
      --wgpu_texture_ongoing_accesses_[texture.Get()];
  CHECK_GE(num_outstanding_accesses, 0);

  if (num_outstanding_accesses == 0) {
    wgpu_texture_ongoing_accesses_.erase(texture.Get());
  }

  return num_outstanding_accesses;
}

DawnSharedTextureHolder* IOSurfaceImageBacking::GetDawnTextureHolder() {
  return dawn_texture_holder_.get();
}

void IOSurfaceImageBacking::AddWGPUDeviceWithPendingCommands(
    wgpu::Device device) {
  wgpu_devices_pending_flush_.insert(std::move(device));
}

void IOSurfaceImageBacking::WaitForDawnCommandsToBeScheduled(
    const wgpu::Device& device_to_exclude) {
  TRACE_EVENT0("gpu",
               "IOSurfaceImageBacking::WaitForDawnCommandsToBeScheduled");
  bool excluded_device_was_pending_flush = false;
  for (const auto& device : std::move(wgpu_devices_pending_flush_)) {
    if (device.Get() == device_to_exclude.Get()) {
      excluded_device_was_pending_flush = true;
      continue;
    }
    dawn::native::metal::WaitForCommandsToBeScheduled(device.Get());
  }
  if (excluded_device_was_pending_flush) {
    // This device wasn't flushed, so we need to add it to the list again.
    wgpu_devices_pending_flush_.insert(device_to_exclude);
  }
}

void IOSurfaceImageBacking::AddEGLDisplayWithPendingCommands(
    gl::GLDisplayEGL* display) {
  egl_displays_pending_flush_.insert(display);
}

void IOSurfaceImageBacking::WaitForANGLECommandsToBeScheduled() {
  TRACE_EVENT0("gpu",
               "IOSurfaceImageBacking::WaitForANGLECommandsToBeScheduled");
  for (auto* display : std::move(egl_displays_pending_flush_)) {
    eglWaitUntilWorkScheduledANGLE(display->GetDisplay());
  }
}

void IOSurfaceImageBacking::ClearEGLDisplaysWithPendingCommands(
    gl::GLDisplayEGL* display_to_keep) {
  if (std::move(egl_displays_pending_flush_).contains(display_to_keep)) {
    egl_displays_pending_flush_.insert(display_to_keep);
  }
}

std::unique_ptr<DawnImageRepresentation> IOSurfaceImageBacking::ProduceDawn(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker,
    const wgpu::Device& device,
    wgpu::BackendType backend_type,
    std::vector<wgpu::TextureFormat> view_formats,
    scoped_refptr<SharedContextState> context_state) {
  wgpu::TextureFormat wgpu_format = ToDawnFormat(format());
  // See comments in IOSurfaceImageBackingFactory::CreateSharedImage about
  // RGBA versus BGRA when using Skia Ganesh GL backend or ANGLE.
  if (io_surface_format_ == 'BGRA') {
    wgpu_format = wgpu::TextureFormat::BGRA8Unorm;
  }
  if (wgpu_format == wgpu::TextureFormat::Undefined) {
    LOG(ERROR) << "Unsupported format for Dawn: " << format().ToString();
    return nullptr;
  }

  if (backend_type == wgpu::BackendType::Metal) {
    // Clear out any cached SharedTextureMemory instances for which the
    // associated Device has been lost - this both saves memory and more
    // importantly ensures that a new SharedTextureMemory instance will be
    // created if another Device occupies the same memory as a previously-used,
    // now-lost Device.
    dawn_texture_holder_->EraseDataIfDeviceLost();

    CHECK(device.HasFeature(wgpu::FeatureName::SharedTextureMemoryIOSurface));

    wgpu::SharedTextureMemory shared_texture_memory =
        dawn_texture_holder_->GetSharedTextureMemory(device);
    if (!shared_texture_memory) {
      wgpu::SharedTextureMemoryIOSurfaceDescriptor io_surface_desc;
      io_surface_desc.ioSurface = io_surface_.get();
      wgpu::SharedTextureMemoryDescriptor desc = {};
      desc.nextInChain = &io_surface_desc;

      shared_texture_memory = device.ImportSharedTextureMemory(&desc);
      if (!shared_texture_memory) {
        LOG(ERROR) << "Unable to create SharedTextureMemory - device lost?";
        return nullptr;
      }

      // We cache the SharedTextureMemory instance that is associated with the
      // Graphite device.
      // TODO(crbug.com/345674550): Extend caching to WebGPU devices as well.
      // NOTE: `dawn_context_provider` may be null if Graphite is not being
      // used.
      auto* dawn_context_provider = context_state->dawn_context_provider();
      if (dawn_context_provider &&
          dawn_context_provider->GetDevice().Get() == device.Get()) {
        // This is the Graphite device, so we cache its SharedTextureMemory
        // instance.
        dawn_texture_holder_->MaybeCacheSharedTextureMemory(
            device, shared_texture_memory);
      }
    }

    return std::make_unique<DawnRepresentation>(
        manager, this, tracker, wgpu::Device(device),
        std::move(shared_texture_memory), io_surface_size_, wgpu_format,
        std::move(view_formats));
  }

  CHECK_EQ(backend_type, wgpu::BackendType::Vulkan);
  return std::make_unique<DawnFallbackImageRepresentation>(
      manager, this, tracker, wgpu::Device(device), wgpu_format,
      std::move(view_formats));
}

std::unique_ptr<SkiaGaneshImageRepresentation>
IOSurfaceImageBacking::ProduceSkiaGanesh(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker,
    scoped_refptr<SharedContextState> context_state) {
  scoped_refptr<IOSurfaceBackingEGLState> egl_state;
  std::vector<sk_sp<GrPromiseImageTexture>> promise_textures;

  if (context_state->GrContextIsGL()) {
    egl_state = RetainGLTexture();
  }

  for (int plane_index = 0; plane_index < format().NumberOfPlanes();
       plane_index++) {
    GLFormatDesc format_desc =
        context_state->GetGLFormatCaps().ToGLFormatDesc(format(), plane_index);
    GrBackendTexture backend_texture;
    auto plane_size = format().GetPlaneSize(plane_index, size());
    GetGrBackendTexture(context_state->feature_info(), egl_state->GetGLTarget(),
                        plane_size, egl_state->GetGLServiceId(plane_index),
                        format_desc.storage_internal_format,
                        context_state->gr_context()->threadSafeProxy(),
                        &backend_texture);
    sk_sp<GrPromiseImageTexture> promise_texture =
        GrPromiseImageTexture::Make(backend_texture);
    if (!promise_texture) {
      return nullptr;
    }
    promise_textures.push_back(std::move(promise_texture));
  }

  return std::make_unique<SkiaGaneshRepresentation>(manager, this, egl_state,
                                                    std::move(context_state),
                                                    promise_textures, tracker);
}

std::unique_ptr<SkiaGraphiteImageRepresentation>
IOSurfaceImageBacking::ProduceSkiaGraphite(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker,
    scoped_refptr<SharedContextState> context_state) {
  CHECK(context_state);
  if (context_state->IsGraphiteDawn()) {
#if BUILDFLAG(SKIA_USE_DAWN)
    auto device = context_state->dawn_context_provider()->GetDevice();
    auto backend_type = context_state->dawn_context_provider()->backend_type();
    auto dawn_representation =
        ProduceDawn(manager, tracker, device, backend_type, /*view_formats=*/{},
                    context_state);
    if (!dawn_representation) {
      LOG(ERROR) << "Could not create Dawn Representation";
      return nullptr;
    }
    // Use GPU main recorder since this should only be called for
    // fulfilling Graphite promise images on GPU main thread.
    return SkiaGraphiteDawnImageRepresentation::Create(
        std::move(dawn_representation), context_state,
        context_state->gpu_main_graphite_recorder(), manager, this, tracker);
#else
    NOTREACHED();
#endif
  } else {
    CHECK(context_state->IsGraphiteMetal());
#if BUILDFLAG(SKIA_USE_METAL)
    std::vector<base::apple::scoped_nsprotocol<id<MTLTexture>>> mtl_textures;
    mtl_textures.reserve(format().NumberOfPlanes());

    for (int plane = 0; plane < format().NumberOfPlanes(); plane++) {
      auto plane_size = format().GetPlaneSize(plane, size());
      base::apple::scoped_nsprotocol<id<MTLTexture>> mtl_texture =
          CreateMetalTexture(
              context_state->metal_context_provider()->GetMTLDevice(),
              io_surface_.get(), plane_size, format(), plane);
      if (!mtl_texture) {
        LOG(ERROR) << "Failed to create MTLTexture from IOSurface";
        return nullptr;
      }
      mtl_textures.push_back(std::move(mtl_texture));
    }

    // Use GPU main recorder since this should only be called for
    // fulfilling Graphite promise images on GPU main thread.
    return std::make_unique<SkiaGraphiteRepresentation>(
        manager, this, tracker, context_state->gpu_main_graphite_recorder(),
        std::move(mtl_textures));
#else
    NOTREACHED();
#endif
  }
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
  IOSurfaceSetPurgeable(io_surface_.get(), purgeable, &old_state);
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
    iter.second->set_bind_pending();
  }
}

gfx::GpuMemoryBufferHandle IOSurfaceImageBacking::GetGpuMemoryBufferHandle() {
  gfx::GpuMemoryBufferHandle handle;
  handle.type = gfx::IO_SURFACE_BUFFER;
  handle.io_surface = io_surface_;
  return handle;
}

bool IOSurfaceImageBacking::BeginAccess(bool readonly) {
  if (!readonly && ongoing_write_access_) {
    DLOG(ERROR) << "Unable to begin write access because another "
                   "write access is in progress";
    return false;
  }
  // Track reads and writes if not being used for concurrent read/writes.
  if (!(usage() & SHARED_IMAGE_USAGE_CONCURRENT_READ_WRITE)) {
    if (readonly && ongoing_write_access_) {
      DLOG(ERROR) << "Unable to begin read access because another "
                     "write access is in progress";
      return false;
    }
    if (!readonly && num_ongoing_read_accesses_) {
      DLOG(ERROR) << "Unable to begin write access because a read access is in "
                     "progress";
      return false;
    }
  }

  if (readonly) {
    num_ongoing_read_accesses_++;
  } else {
    ongoing_write_access_ = true;
  }

  return true;
}

void IOSurfaceImageBacking::EndAccess(bool readonly) {
  if (readonly) {
    CHECK_GT(num_ongoing_read_accesses_, 0u);
    if (!(usage() & SHARED_IMAGE_USAGE_CONCURRENT_READ_WRITE)) {
      CHECK(!ongoing_write_access_);
    }
    num_ongoing_read_accesses_--;
  } else {
    CHECK(ongoing_write_access_);
    if (!(usage() & SHARED_IMAGE_USAGE_CONCURRENT_READ_WRITE)) {
      CHECK_EQ(num_ongoing_read_accesses_, 0u);
    }
    ongoing_write_access_ = false;
  }
}

bool IOSurfaceImageBacking::IOSurfaceBackingEGLStateBeginAccess(
    IOSurfaceBackingEGLState* egl_state,
    bool readonly) {
  // It is in error to read or write an IOSurface while it is purgeable.
  CHECK(!purgeable_);
  if (!BeginAccess(readonly)) {
    return false;
  }

  gl::GLDisplayEGL* display = gl::GLDisplayEGL::GetDisplayForCurrentContext();
  CHECK(display);
  CHECK_EQ(display->GetDisplay(), egl_state->egl_display_);

  // IOSurface might be written on a different GPU. So we have to wait for the
  // previous Dawn and ANGLE commands to be scheduled first.
  // TODO(crbug.com/40260114): Skip this if we're not on a dual-GPU system.
  WaitForDawnCommandsToBeScheduled(/*device_to_exclude=*/nullptr);

  // Note that we don't need to call WaitForANGLECommandsToBeScheduled for other
  // EGLDisplays because it is already done when the previous GL context is made
  // uncurrent. We can simply remove the other EGLDisplays from the list.
  ClearEGLDisplaysWithPendingCommands(/*display_to_keep=*/display);

  if (gl::GetANGLEImplementation() == gl::ANGLEImplementation::kMetal) {
    // If this image could potentially be shared with another Metal device,
    // it's necessary to synchronize between the two devices. If any Metal
    // shared events have been enqueued (the assumption is that this was done by
    // for a Dawn device or another ANGLE Metal EGLDisplay), wait on them.
    ProcessSharedEventsForBeginAccess(
        readonly,
        [display](id<MTLSharedEvent> shared_event, uint64_t signaled_value) {
          display->WaitForMetalSharedEvent(shared_event, signaled_value);
        });
  }

  // If the GL texture is already bound (the bind is not marked as pending),
  // then early-out.
  if (!egl_state->is_bind_pending()) {
    CHECK(!egl_state->egl_surfaces_.empty());
    return true;
  }

  if (egl_state->egl_surfaces_.empty()) {
    std::vector<std::unique_ptr<gl::ScopedEGLSurfaceIOSurface>> egl_surfaces;
    for (int plane_index = 0; plane_index < format().NumberOfPlanes();
         plane_index++) {
      gfx::BufferFormat buffer_format;
      if (format().is_single_plane()) {
        buffer_format = ToBufferFormat(format());
        // See comments in IOSurfaceImageBackingFactory::CreateSharedImage about
        // RGBA versus BGRA when using Skia Ganesh GL backend or ANGLE.
        if (io_surface_format_ == 'BGRA') {
          if (buffer_format == gfx::BufferFormat::RGBA_8888) {
            buffer_format = gfx::BufferFormat::BGRA_8888;
          } else if (buffer_format == gfx::BufferFormat::RGBX_8888) {
            buffer_format = gfx::BufferFormat::BGRX_8888;
          }
        }
      } else {
        // For multiplanar formats (without external sampler) get planar buffer
        // format.
        buffer_format = GetBufferFormatForPlane(format(), plane_index);
      }

      auto egl_surface = gl::ScopedEGLSurfaceIOSurface::Create(
          egl_state->egl_display_, egl_state->GetGLTarget(), io_surface_.get(),
          plane_index, buffer_format);
      if (!egl_surface) {
        LOG(ERROR) << "Failed to create ScopedEGLSurfaceIOSurface.";
        return false;
      }

      egl_surfaces.push_back(std::move(egl_surface));
    }
    egl_state->egl_surfaces_ = std::move(egl_surfaces);
  }

  CHECK_EQ(static_cast<int>(egl_state->gl_textures_.size()),
           format().NumberOfPlanes());
  CHECK_EQ(static_cast<int>(egl_state->egl_surfaces_.size()),
           format().NumberOfPlanes());
  for (int plane_index = 0; plane_index < format().NumberOfPlanes();
       plane_index++) {
    gl::ScopedRestoreTexture scoped_restore(
        gl::g_current_gl_context, egl_state->GetGLTarget(),
        egl_state->GetGLServiceId(plane_index));
    // Un-bind the IOSurface from the GL texture (this will be a no-op if it is
    // not yet bound).
    egl_state->egl_surfaces_[plane_index]->ReleaseTexImage();

    // Bind the IOSurface to the GL texture.
    if (!egl_state->egl_surfaces_[plane_index]->BindTexImage()) {
      LOG(ERROR) << "Failed to bind ScopedEGLSurfaceIOSurface to target";
      return false;
    }
  }
  egl_state->clear_bind_pending();

  return true;
}

void IOSurfaceImageBacking::IOSurfaceBackingEGLStateEndAccess(
    IOSurfaceBackingEGLState* egl_state,
    bool readonly) {
  EndAccess(readonly);

  // Early out if BeginAccess didn't succeed and we didn't bind any surfaces.
  if (egl_state->is_bind_pending()) {
    return;
  }

  gl::GLDisplayEGL* display = gl::GLDisplayEGL::GetDisplayForCurrentContext();
  CHECK(display);
  CHECK_EQ(display->GetDisplay(), egl_state->egl_display_);

  if (gl::GetANGLEImplementation() == gl::ANGLEImplementation::kMetal) {
    id<MTLSharedEvent> shared_event = nil;
    uint64_t signal_value = 0;
    if (display->CreateMetalSharedEvent(&shared_event, &signal_value)) {
      AddSharedEventForEndAccess(shared_event, signal_value, readonly);
    } else {
      LOG(DFATAL) << "Failed to create Metal shared event";
    }
  }

  // We have to call eglWaitUntilWorkScheduledANGLE on multi-GPU systems for
  // IOSurface synchronization by the kernel e.g. using waitUntilScheduled on
  // Metal or glFlush on OpenGL. Defer the call until CoreAnimation, Dawn,
  // or another ANGLE EGLDisplay needs to access to avoid unnecessary overhead.
  AddEGLDisplayWithPendingCommands(display);

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
  // triggers a release and copy). By design, IOSurfaceImageBackingFactory
  // enforces this property for this use case.
  const bool is_swangle =
      gl::GetANGLEImplementation() == gl::ANGLEImplementation::kSwiftShader;

  // We also need to ReleaseTexImage for Graphite to ensure that any shared
  // events enqueued are signaled in the flush inside ReleaseTexImage.
  const bool needs_release_tex_image =
      (is_swangle || gr_context_type_ != GrContextType::kGL) &&
      num_ongoing_read_accesses_ == 0;

  if (needs_release_tex_image) {
    CHECK_EQ(static_cast<int>(egl_state->gl_textures_.size()),
             format().NumberOfPlanes());
    CHECK_EQ(static_cast<int>(egl_state->egl_surfaces_.size()),
             format().NumberOfPlanes());
    for (int plane_index = 0; plane_index < format().NumberOfPlanes();
         plane_index++) {
      gl::ScopedRestoreTexture scoped_restore(
          gl::g_current_gl_context, egl_state->GetGLTarget(),
          egl_state->GetGLServiceId(plane_index));
      egl_state->egl_surfaces_[plane_index]->ReleaseTexImage();
    }
    egl_state->set_bind_pending();
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

  egl_state->egl_surfaces_.clear();

  // Remove `egl_state` from `egl_state_map_`.
  auto found = egl_state_map_.find(egl_state->egl_display_);
  CHECK(found != egl_state_map_.end());
  CHECK(found->second == egl_state);
  egl_state_map_.erase(found);
}

bool IOSurfaceImageBacking::InitializePixels(
    base::span<const uint8_t> pixel_data) {
  CHECK(format().is_single_plane());
  ScopedIOSurfaceLock io_surface_lock(io_surface_.get(),
                                      kIOSurfaceLockAvoidSync);

  uint8_t* dst_data = reinterpret_cast<uint8_t*>(
      IOSurfaceGetBaseAddressOfPlane(io_surface_.get(), 0));
  size_t dst_stride = IOSurfaceGetBytesPerRowOfPlane(io_surface_.get(), 0);

  const uint8_t* src_data = pixel_data.data();
  const size_t src_stride = (format().BitsPerPixel() / 8) * size().width();
  const size_t height = size().height();

  if (pixel_data.size() != src_stride * height) {
    DLOG(ERROR) << "Invalid initial pixel data size";
    return false;
  }

  for (size_t y = 0; y < height; ++y) {
    memcpy(dst_data, src_data, src_stride);
    dst_data += dst_stride;
    src_data += src_stride;
  }

  return true;
}

void IOSurfaceImageBacking::AddSharedEventForEndAccess(
    id<MTLSharedEvent> shared_event,
    uint64_t signal_value,
    bool readonly) {
  SharedEventMap& shared_events =
      readonly ? non_exclusive_shared_events_ : exclusive_shared_events_;
  auto [it, _] = shared_events.insert(
      {ScopedSharedEvent(shared_event, base::scoped_policy::RETAIN), 0});
  it->second = std::max(it->second, signal_value);
}

template <typename Fn>
void IOSurfaceImageBacking::ProcessSharedEventsForBeginAccess(bool readonly,
                                                              const Fn& fn) {
  // Always need wait on exclusive access end events.
  for (const auto& [shared_event, signal_value] : exclusive_shared_events_) {
    fn(shared_event.get(), signal_value);
  }

  if (!readonly) {
    // For read-write (exclusive) access, non execlusive access end events
    // should be waited on as well.
    for (const auto& [shared_event, signal_value] :
         non_exclusive_shared_events_) {
      fn(shared_event.get(), signal_value);
    }

    // Clear events, since this read-write (exclusive) access will provide an
    // event when the access is finished.
    exclusive_shared_events_.clear();
    non_exclusive_shared_events_.clear();
  }
}

}  // namespace gpu
