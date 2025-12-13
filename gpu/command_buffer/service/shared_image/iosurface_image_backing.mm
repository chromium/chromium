// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "gpu/command_buffer/service/shared_image/iosurface_image_backing.h"

#include <EGL/egl.h>
#include <EGL/eglext.h>
#import <Metal/Metal.h>
#include <dawn/native/MetalBackend.h>
#include <dawn/webgpu_cpp.h>
#include <dawn/webgpu_cpp_print.h>

#include "base/apple/scoped_cftyperef.h"
#include "base/apple/scoped_nsobject.h"
#include "base/bits.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_policy.h"
#include "base/notimplemented.h"
#include "base/trace_event/memory_dump_manager.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "gpu/command_buffer/common/shared_image_trace_utils.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/dawn_context_provider.h"
#include "gpu/command_buffer/service/metal_context_provider.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image/copy_image_plane.h"
#include "gpu/command_buffer/service/shared_image/dawn_fallback_image_representation.h"
#include "gpu/command_buffer/service/shared_image/iosurface_image_backing_factory.h"
#include "gpu/command_buffer/service/shared_image/shared_image_format_service_utils.h"
#include "gpu/command_buffer/service/shared_image/shared_image_gl_utils.h"
#include "gpu/command_buffer/service/shared_image/skia_graphite_dawn_image_representation.h"
#include "gpu/command_buffer/service/skia_utils.h"
#include "gpu/config/gpu_finch_features.h"
#include "third_party/angle/include/EGL/eglext_angle.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/gpu/ganesh/GrContextThreadSafeProxy.h"
#include "third_party/skia/include/gpu/ganesh/SkSurfaceGanesh.h"
#include "third_party/skia/include/gpu/graphite/Recorder.h"
#include "third_party/skia/include/gpu/graphite/Surface.h"
#include "third_party/skia/include/private/chromium/GrPromiseImageTexture.h"
#include "ui/gfx/mac/mtl_shared_event_fence.h"
#include "ui/gl/egl_surface_io_surface.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_display.h"
#include "ui/gl/gl_fence_egl.h"
#include "ui/gl/gl_gl_api_implementation.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/scoped_binders.h"
#include "ui/gl/scoped_make_current.h"
#include "ui/gl/scoped_restore_texture.h"

namespace gpu {

namespace {
using GraphiteTextureHolder = SkiaImageRepresentation::GraphiteTextureHolder;

// Alignment required by `CopyBufferToTexture` and `CopyTextureToBuffer`.
constexpr uint32_t kTextureBytesPerRowAlignment = 256u;

struct ScopedIOSurfaceLock {
  ScopedIOSurfaceLock(IOSurfaceRef iosurface, IOSurfaceLockOptions options)
      : io_surface_(iosurface), options_(options) {
    kern_return_t r = IOSurfaceLock(io_surface_, options_, /*seed=*/nullptr);
    CHECK_EQ(KERN_SUCCESS, r);
  }
  ~ScopedIOSurfaceLock() {
    kern_return_t r = IOSurfaceUnlock(io_surface_, options_, /*seed=*/nullptr);
    CHECK_EQ(KERN_SUCCESS, r);
  }

  ScopedIOSurfaceLock(const ScopedIOSurfaceLock&) = delete;
  ScopedIOSurfaceLock& operator=(const ScopedIOSurfaceLock&) = delete;

 private:
  const IOSurfaceRef io_surface_;
  const IOSurfaceLockOptions options_;
};

// Returns planar format for given multiplanar `format`.
viz::SharedImageFormat GetFormatForPlane(viz::SharedImageFormat format,
                                         int plane) {
  DCHECK(format.is_multi_plane());
  DCHECK(format.IsValidPlaneIndex(plane));

  // IOSurfaceBacking does not support external sampler use cases.
  int num_channels = format.NumChannelsInPlane(plane);
  DCHECK_LE(num_channels, 2);
  switch (format.channel_format()) {
    case viz::SharedImageFormat::ChannelFormat::k8:
      return num_channels == 2 ? viz::SinglePlaneFormat::kRG_88
                               : viz::SinglePlaneFormat::kR_8;
    case viz::SharedImageFormat::ChannelFormat::k10:
    case viz::SharedImageFormat::ChannelFormat::k16:
    case viz::SharedImageFormat::ChannelFormat::k16F:
      return num_channels == 2 ? viz::SinglePlaneFormat::kRG_1616
                               : viz::SinglePlaneFormat::kR_16;
  }
  NOTREACHED();
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

std::vector<scoped_refptr<GraphiteTextureHolder>> CreateGraphiteMetalTextures(
    std::vector<base::apple::scoped_nsprotocol<id<MTLTexture>>> mtl_textures,
    const viz::SharedImageFormat format,
    const gfx::Size& size) {
  int num_planes = format.NumberOfPlanes();
  std::vector<scoped_refptr<GraphiteTextureHolder>> graphite_textures;
  graphite_textures.reserve(num_planes);
  for (int plane = 0; plane < num_planes; plane++) {
    SkISize sk_size = gfx::SizeToSkISize(format.GetPlaneSize(plane, size));
    graphite_textures.emplace_back(base::MakeRefCounted<GraphiteTextureHolder>(
        skgpu::graphite::BackendTextures::MakeMetal(
            sk_size, mtl_textures[plane].get())));
  }
  return graphite_textures;
}
#endif

id<MTLDevice> QueryMetalDeviceFromANGLE(EGLDisplay display) {
  id<MTLDevice> metal_device = nil;
  if (gl::GetANGLEImplementation() == gl::ANGLEImplementation::kMetal) {
    EGLAttrib angle_device_attrib = 0;
    if (eglQueryDisplayAttribEXT(display, EGL_DEVICE_EXT,
                                 &angle_device_attrib)) {
      EGLDeviceEXT angle_device =
          reinterpret_cast<EGLDeviceEXT>(angle_device_attrib);
      EGLAttrib metal_device_attrib = 0;
      if (eglQueryDeviceAttribEXT(angle_device, EGL_METAL_DEVICE_ANGLE,
                                  &metal_device_attrib)) {
        metal_device = (__bridge id)(void*)metal_device_attrib;
      }
    }
  }
  return metal_device;
}

}  // namespace

IOSurfaceImageBacking::DawnBufferCopyRepresentation::
    DawnBufferCopyRepresentation(
        SharedImageManager* manager,
        SharedImageBacking* backing,
        MemoryTypeTracker* tracker,
        const wgpu::Device& device,
        std::unique_ptr<DawnImageRepresentation> dawn_image_representation)
    : DawnBufferRepresentation(manager, backing, tracker),
      device_(device),
      dawn_image_representation_(std::move(dawn_image_representation)) {}

IOSurfaceImageBacking::DawnBufferCopyRepresentation::
    ~DawnBufferCopyRepresentation() = default;

wgpu::Buffer IOSurfaceImageBacking::DawnBufferCopyRepresentation::BeginAccess(
    wgpu::BufferUsage usage) {
  auto scoped_access = dawn_image_representation_->BeginScopedAccess(
      wgpu::TextureUsage::CopySrc, wgpu::TextureUsage::None,
      /*allow_uncleared=*/AllowUnclearedAccess::kYes);
  wgpu::Texture texture = scoped_access->texture();
  if (!texture) {
    LOG(ERROR) << "Failed to begin access to Dawn texture.";
    return nullptr;
  }

  // TODO(crbug.com/427252761): Directly import as a buffer then copy to a
  // packed buffer. This requries SharedBufferMemory implementation.

  // 1. Create a temporary staging buffer with the required alignment for
  // CopyTextureToBuffer
  uint32_t bytes_per_pixel = format().BytesPerPixel();
  uint32_t packed_bytes_per_row = bytes_per_pixel * size().width();
  uint32_t aligned_bytes_per_row =
      base::bits::AlignUp(packed_bytes_per_row, kTextureBytesPerRowAlignment);

  wgpu::BufferDescriptor staging_desc;
  staging_desc.size = aligned_bytes_per_row * size().height();
  staging_desc.usage = wgpu::BufferUsage::CopySrc | wgpu::BufferUsage::CopyDst;
  wgpu::Buffer staging_buffer = device_.CreateBuffer(&staging_desc);

  // 2. Copy from the texture to the aligned staging buffer
  wgpu::TexelCopyTextureInfo source;
  source.texture = texture;
  source.mipLevel = 0;
  source.origin = {0, 0, 0};
  source.aspect = wgpu::TextureAspect::All;

  wgpu::TexelCopyBufferInfo staging_destination;
  staging_destination.buffer = staging_buffer;
  staging_destination.layout.offset = 0;
  staging_destination.layout.bytesPerRow = aligned_bytes_per_row;
  staging_destination.layout.rowsPerImage = size().height();

  wgpu::Extent3D copySize = {static_cast<uint32_t>(size().width()),
                             static_cast<uint32_t>(size().height()), 1};

  wgpu::CommandEncoder encoder = device_.CreateCommandEncoder();
  encoder.CopyTextureToBuffer(&source, &staging_destination, &copySize);

  // 3. Create the final packed destination buffer
  wgpu::BufferDescriptor final_buffer_desc;
  final_buffer_desc.size = packed_bytes_per_row * size().height();
  final_buffer_desc.usage = usage | wgpu::BufferUsage::CopySrc;
  buffer_ = device_.CreateBuffer(&final_buffer_desc);

  // 4. Copy from the staging buffer to the final packed buffer, row by row.
  for (int y = 0; y < size().height(); ++y) {
    uint64_t source_offset = y * aligned_bytes_per_row;
    uint64_t destination_offset = y * packed_bytes_per_row;
    encoder.CopyBufferToBuffer(staging_buffer, source_offset, buffer_,
                               destination_offset, packed_bytes_per_row);
  }

  wgpu::CommandBuffer command_buffer = encoder.Finish();
  device_.GetQueue().Submit(1, &command_buffer);

  return buffer_;
}

void IOSurfaceImageBacking::DawnBufferCopyRepresentation::EndAccess() {
  // Copy the data back to the texture.
  auto scoped_access = dawn_image_representation_->BeginScopedAccess(
      wgpu::TextureUsage::CopyDst, wgpu::TextureUsage::None,
      /*allow_uncleared=*/AllowUnclearedAccess::kNo);
  wgpu::Texture texture = scoped_access->texture();
  if (!texture) {
    DLOG(ERROR) << "Failed to begin access to Dawn texture.";
    return;
  }

  // Create a staging buffer with the required alignment for
  // CopyBufferToTexture.
  uint32_t bytes_per_pixel = format().BytesPerPixel();
  uint32_t packed_bytes_per_row = bytes_per_pixel * size().width();

  uint32_t aligned_bytes_per_row =
      base::bits::AlignUp(packed_bytes_per_row, kTextureBytesPerRowAlignment);

  wgpu::BufferDescriptor staging_desc;
  staging_desc.size = aligned_bytes_per_row * size().height();
  staging_desc.usage = wgpu::BufferUsage::CopySrc | wgpu::BufferUsage::CopyDst;
  wgpu::Buffer staging_buffer = device_.CreateBuffer(&staging_desc);

  wgpu::CommandEncoder encoder = device_.CreateCommandEncoder();

  // Copy from the packed buffer to the staging buffer row by row.
  for (int y = 0; y < size().height(); ++y) {
    uint64_t source_offset = y * packed_bytes_per_row;
    uint64_t destination_offset = y * aligned_bytes_per_row;
    encoder.CopyBufferToBuffer(buffer_, source_offset, staging_buffer,
                               destination_offset, packed_bytes_per_row);
  }

  // Now copy from the staging buffer to the texture.
  wgpu::TexelCopyBufferInfo source;
  source.buffer = staging_buffer;
  source.layout.offset = 0;
  source.layout.bytesPerRow = aligned_bytes_per_row;
  source.layout.rowsPerImage = size().height();

  wgpu::TexelCopyTextureInfo destination;
  destination.texture = texture;
  destination.mipLevel = 0;
  destination.origin = {0, 0, 0};
  destination.aspect = wgpu::TextureAspect::All;

  wgpu::Extent3D copySize = {static_cast<uint32_t>(size().width()),
                             static_cast<uint32_t>(size().height()), 1};

  encoder.CopyBufferToTexture(&source, &destination, &copySize);

  wgpu::CommandBuffer command_buffer = encoder.Finish();
  device_.GetQueue().Submit(1, &command_buffer);
}

WebNNIOSurfaceTensorRepresentation::WebNNIOSurfaceTensorRepresentation(
    SharedImageManager* manager,
    SharedImageBacking* backing,
    MemoryTypeTracker* tracker)
    : WebNNTensorRepresentation(manager, backing, tracker) {}

WebNNIOSurfaceTensorRepresentation::~WebNNIOSurfaceTensorRepresentation() =
    default;

IOSurfaceRef WebNNIOSurfaceTensorRepresentation::GetIOSurface() const {
  return static_cast<IOSurfaceImageBacking*>(backing())->GetIOSurface();
}

bool WebNNIOSurfaceTensorRepresentation::BeginAccess() {
  return static_cast<IOSurfaceImageBacking*>(backing())->BeginAccessWebNN();
}

void WebNNIOSurfaceTensorRepresentation::EndAccess() {
  static_cast<IOSurfaceImageBacking*>(backing())->EndAccessWebNN();
}

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
      gl_textures_(std::move(gl_textures)),
      created_task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()) {
  client_->IOSurfaceBackingEGLStateBeingCreated(this);
}

IOSurfaceBackingEGLState::~IOSurfaceBackingEGLState() {
  // To use ui::ScopedMakeCurrent, this funciton must be called on the same
  // thread where it got its current context context during creation.
  ui::ScopedMakeCurrent smc(context_.get(), surface_.get());
  if (client_) {
    // IOSurfaceBackingEGLState is destroyed directly from the same thread.
    client_->IOSurfaceBackingEGLStateBeingDestroyed(this, !context_lost_);
    DCHECK(gl_textures_.empty());
  } else {
    // ~IOSurfaceBackingEGLState is posted from the other thread when the
    // client_ (IOSurfaceImageBacking) was destroyed.
    if (context_lost_) {
      for (const auto& texture : gl_textures_) {
        texture->MarkContextLost();
      }
    }
    gl_textures_.clear();
    egl_surfaces_.clear();
  }
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
  if (!have_context) {
    context_lost_ = true;
  }
}

void IOSurfaceBackingEGLState::RemoveClient() {
  client_ = nullptr;
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
    AutoLock auto_lock(backing());
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
    AutoLock auto_lock(backing());
    mode_ = mode;
    bool readonly = mode_ != GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM;
    return egl_state_->BeginAccess(readonly);
  }

  void EndAccess() override {
    DCHECK(mode_ != 0);
    AutoLock auto_lock(backing());
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
  const scoped_refptr<SharedContextState> context_state_;
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

    AutoLock auto_lock(backing());
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
  AutoLock auto_lock(backing());
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
    SkColorType sk_color_type =
        viz::ToClosestSkColorType(format(), plane_index);
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
  AutoLock auto_lock(backing());
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
  AutoLock auto_lock(backing());
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
  AutoLock auto_lock(backing());
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
  AutoLock auto_lock(backing());

  if (egl_state_) {
    egl_state_->EndAccess(/*readonly=*/true);
  }
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
// SkiaGraphiteMetalRepresentation

class IOSurfaceImageBacking::SkiaGraphiteMetalRepresentation final
    : public SkiaGraphiteImageRepresentation {
 public:
  // Graphite does not keep track of the MetalTexture like Ganesh, so the
  // representation/backing needs to keep the Metal texture alive.
  SkiaGraphiteMetalRepresentation(
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

  ~SkiaGraphiteMetalRepresentation() override {
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
  std::vector<scoped_refptr<GraphiteTextureHolder>> BeginWriteAccess() override;
  void EndWriteAccess() override;
  std::vector<scoped_refptr<GraphiteTextureHolder>> BeginReadAccess() override;
  void EndReadAccess() override;

  IOSurfaceImageBacking* backing_impl() const {
    return static_cast<IOSurfaceImageBacking*>(backing());
  }

  const raw_ptr<skgpu::graphite::Recorder> recorder_;
  std::vector<base::apple::scoped_nsprotocol<id<MTLTexture>>> mtl_textures_;
  std::vector<sk_sp<SkSurface>> write_surfaces_;
};

std::vector<sk_sp<SkSurface>>
IOSurfaceImageBacking::SkiaGraphiteMetalRepresentation::BeginWriteAccess(
    const SkSurfaceProps& surface_props,
    const gfx::Rect& update_rect) {
  AutoLock auto_lock(backing_impl());

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
    SkColorType sk_color_type = viz::ToClosestSkColorType(format(), plane);
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

std::vector<scoped_refptr<GraphiteTextureHolder>>
IOSurfaceImageBacking::SkiaGraphiteMetalRepresentation::BeginWriteAccess() {
  AutoLock auto_lock(backing_impl());

  if (!backing_impl()->BeginAccess(/*readonly=*/false)) {
    return {};
  }
  return CreateGraphiteMetalTextures(mtl_textures_, format(), size());
}

void IOSurfaceImageBacking::SkiaGraphiteMetalRepresentation::EndWriteAccess() {
  AutoLock auto_lock(backing_impl());
#if DCHECK_IS_ON()
  for (auto& surface : write_surfaces_) {
    DCHECK(surface->unique());
  }
#endif
  write_surfaces_.clear();
  backing_impl()->EndAccess(/*readonly=*/false);
}

std::vector<scoped_refptr<GraphiteTextureHolder>>
IOSurfaceImageBacking::SkiaGraphiteMetalRepresentation::BeginReadAccess() {
  AutoLock auto_lock(backing_impl());
  if (!backing_impl()->BeginAccess(/*readonly=*/true)) {
    return {};
  }
  return CreateGraphiteMetalTextures(mtl_textures_, format(), size());
}

void IOSurfaceImageBacking::SkiaGraphiteMetalRepresentation::EndReadAccess() {
  AutoLock auto_lock(backing_impl());
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
  std::vector<gfx::MTLSharedEventFence> GetBackpressureFences()
      const override;
  bool IsInUseByWindowServer() const override;

  gfx::ScopedIOSurface io_surface_;
};

bool IOSurfaceImageBacking::OverlayRepresentation::BeginReadAccess(
    gfx::GpuFenceHandle& acquire_fence) {
  auto* iosurface_backing = static_cast<IOSurfaceImageBacking*>(backing());
  AutoLock auto_lock(iosurface_backing);

  if (!iosurface_backing->BeginAccess(/*readonly=*/true)) {
    return false;
  }

  // This will transition the image to be accessed by CoreAnimation.
  iosurface_backing->WaitForCommandsToBeScheduled();

  return true;
}

void IOSurfaceImageBacking::OverlayRepresentation::EndReadAccess(
    gfx::GpuFenceHandle release_fence) {
  auto* iosurface_backing = static_cast<IOSurfaceImageBacking*>(backing());
  AutoLock auto_lock(iosurface_backing);
  DCHECK(release_fence.is_null());

  iosurface_backing->EndAccess(/*readonly=*/true);
}

gfx::ScopedIOSurface
IOSurfaceImageBacking::OverlayRepresentation::GetIOSurface() const {
  return io_surface_;
}

std::vector<gfx::MTLSharedEventFence>
IOSurfaceImageBacking::OverlayRepresentation::GetBackpressureFences() const {
  auto* iosurface_backing = static_cast<IOSurfaceImageBacking*>(backing());
  AutoLock auto_lock(iosurface_backing);

  std::vector<gfx::MTLSharedEventFence> backpressure_fences;
  for (const auto& [shared_event, signaled_value] :
       iosurface_backing->exclusive_shared_events_) {
    backpressure_fences.emplace_back(shared_event.get(), signaled_value);
  }
  return backpressure_fences;
}

bool IOSurfaceImageBacking::OverlayRepresentation::IsInUseByWindowServer()
    const {
  // IOSurfaceIsInUse() will always return true if the IOSurface is wrapped in
  // a CVPixelBuffer. Ignore the signal for such IOSurfaces (which are the
  // ones output by hardware video decode and video capture).
  if (backing()->usage().Has(SHARED_IMAGE_USAGE_MACOS_VIDEO_TOOLBOX)) {
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
  IOSurfaceImageBacking* iosurface_backing =
      static_cast<IOSurfaceImageBacking*>(backing());
  AutoLock auto_lock(iosurface_backing);

  const bool readonly = (wgpu_texture_usage & ~kReadOnlyUsage) == 0 &&
                        (internal_usage & ~kReadOnlyUsage) == 0;

  if (!iosurface_backing->BeginAccess(readonly)) {
    return {};
  }

  usage_ = wgpu_texture_usage;
  internal_usage_ = internal_usage;

  texture_ = iosurface_backing->GetDawnTextureCache()->GetCachedWGPUTexture(
      device_, usage_, internal_usage_, view_formats_);
  if (!texture_) {
    texture_ = CreateWGPUTexture(shared_texture_memory_, usage(),
                                 io_surface_size_, wgpu_format_, view_formats_,
                                 wgpu_texture_usage, internal_usage);
    iosurface_backing->GetDawnTextureCache()->MaybeCacheWGPUTexture(
        device_, texture_, usage_, internal_usage_, view_formats_);
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

  // IOSurface might be written on a different GPU. We need to wait for previous
  // Dawn and ANGLE commands to be scheduled first.
  iosurface_backing->WaitForCommandsToBeScheduled(
      dawn::native::metal::GetMTLDevice(device_.Get()));

  bool is_cleared = iosurface_backing->IsClearedInternal();
  wgpu::SharedTextureMemoryBeginAccessDescriptor begin_access_desc = {};
  begin_access_desc.initialized = is_cleared;

  // NOTE: WebGPU allows reads of uncleared textures, in which case Dawn clears
  // the texture on its initial access. Such reads must take exclusive access.
  begin_access_desc.concurrentRead = readonly && is_cleared;

  std::vector<wgpu::SharedFence> shared_fences;
  std::vector<uint64_t> signaled_values;

  // Synchronize with all of the MTLSharedEvents that have been stored in the
  // backing as a consequence of earlier BeginAccess/EndAccess calls against
  // other representations.
  iosurface_backing->ProcessSharedEventsForBeginAccess(
      readonly, [&](id<MTLSharedEvent> shared_event, uint64_t signaled_value) {
        wgpu::SharedFenceMTLSharedEventDescriptor shared_event_desc;
        shared_event_desc.sharedEvent = shared_event;

        wgpu::SharedFenceDescriptor fence_desc;
        fence_desc.nextInChain = &shared_event_desc;

        shared_fences.push_back(device_.ImportSharedFence(&fence_desc));
        signaled_values.push_back(signaled_value);
      });

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
    iosurface_backing->GetDawnTextureCache()->RemoveWGPUTextureFromCache(
        device_, texture_);
    texture_ = nullptr;

    iosurface_backing->EndAccess(readonly);
  }

  return texture_;
}

void IOSurfaceImageBacking::DawnRepresentation::EndAccess() {
  IOSurfaceImageBacking* iosurface_backing =
      static_cast<IOSurfaceImageBacking*>(backing());
  AutoLock auto_lock(iosurface_backing);

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

  wgpu::SharedTextureMemoryEndAccessState end_access_state = {};
  wgpu::SharedTextureMemoryMetalEndAccessState metal_end_access_state = {};
  end_access_state.nextInChain = &metal_end_access_state;

  CHECK_EQ(shared_texture_memory_.EndAccess(texture_, &end_access_state),
           wgpu::Status::Success);

  if (end_access_state.initialized) {
    iosurface_backing->SetClearedInternal();
  }

  // Dawn's Metal backend has enqueued MTLSharedEvents which consumers of the
  // IOSurface must wait upon before attempting to use that IOSurface on another
  // command queue. Store these events in the underlying IOSurfaceImageBacking.
  for (size_t i = 0; i < end_access_state.fenceCount; i++) {
    auto fence = end_access_state.fences[i];
    auto signaled_value = end_access_state.signaledValues[i];

    wgpu::SharedFenceExportInfo fence_export_info;
    wgpu::SharedFenceMTLSharedEventExportInfo fence_mtl_export_info;
    fence_export_info.nextInChain = &fence_mtl_export_info;
    fence.ExportInfo(&fence_export_info);
    auto shared_event =
        static_cast<id<MTLSharedEvent>>(fence_mtl_export_info.sharedEvent);
    iosurface_backing->AddSharedEventForEndAccess(shared_event, signaled_value,
                                                  readonly);
  }

  // TODO(crbug.com/328411251): Investigate whether this is needed for readonly
  // access.
  if (metal_end_access_state.commandsScheduledFuture.id != 0) {
    iosurface_backing->wgpu_commands_scheduled_futures_.insert_or_assign(
        device_, metal_end_access_state.commandsScheduledFuture);
  }

  iosurface_backing->GetDawnTextureCache()->DestroyWGPUTextureIfNotCached(
      device_, texture_);

  texture_ = nullptr;
  usage_ = internal_usage_ = wgpu::TextureUsage::None;
}

///////////////////////////////////////////////////////////////////////////////
// SkiaGraphiteDawnMetalRepresentation

class IOSurfaceImageBacking::SkiaGraphiteDawnMetalRepresentation
    : public SkiaGraphiteDawnImageRepresentation {
 public:
  using SkiaGraphiteDawnImageRepresentation::
      SkiaGraphiteDawnImageRepresentation;
  ~SkiaGraphiteDawnMetalRepresentation() override = default;

  bool SupportsMultipleConcurrentReadAccess() final;
};

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
// This representation meets both of the above constraints.
bool IOSurfaceImageBacking::SkiaGraphiteDawnMetalRepresentation::
    SupportsMultipleConcurrentReadAccess() {
  return true;
}

///////////////////////////////////////////////////////////////////////////////
// IOSurfaceImageBacking

IOSurfaceImageBacking::IOSurfaceImageBacking(
    gfx::ScopedIOSurface io_surface,
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
    bool is_thread_safe,
    GrContextType gr_context_type,
    std::optional<gfx::BufferUsage> buffer_usage)
    : ClearTrackingSharedImageBacking(mailbox,
                                      format,
                                      size,
                                      color_space,
                                      surface_origin,
                                      alpha_type,
                                      usage,
                                      std::move(debug_label),
                                      format.EstimatedSizeInBytes(size),
                                      is_thread_safe,
                                      std::move(buffer_usage)),
      io_surface_(std::move(io_surface)),
      io_surface_size_(IOSurfaceGetWidth(io_surface_.get()),
                       IOSurfaceGetHeight(io_surface_.get())),
      io_surface_format_(IOSurfaceGetPixelFormat(io_surface_.get())),
      dawn_texture_cache_(base::MakeRefCounted<DawnSharedTextureCache>()),
      gl_target_(gl_target),
      framebuffer_attachment_angle_(framebuffer_attachment_angle),
      weak_factory_(this) {
  CHECK(io_surface_);
  CHECK(!is_thread_safe ||
        base::FeatureList::IsEnabled(features::kIOSurfaceMultiThreading));

  // Set the color space for the underlying IOSurface when it's used as overlay.
  gfx::IOSurfaceSetColorSpace(io_surface_.get(), color_space);

  // If this will be bound to different GL backends, then make RetainGLTexture
  // and ReleaseGLTexture actually create and destroy the texture.
  // https://crbug.com/1251724
  if (usage.Has(SHARED_IMAGE_USAGE_HIGH_PERFORMANCE_GPU)) {
    return;
  }

  SetClearedRectInternal((is_cleared ? gfx::Rect(size) : gfx::Rect()));

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
  AutoLock auto_lock(this);
  if (egl_state_for_skia_gl_context_) {
    egl_state_for_skia_gl_context_->WillRelease(have_context());

    if (egl_state_for_skia_gl_context_->created_task_runner()
            ->BelongsToCurrentThread()) {
      egl_state_for_skia_gl_context_ = nullptr;
    } else {
      // Remove `egl_state` from `egl_state_map_`.
      IOSurfaceBackingEGLState* egl_state =
          egl_state_for_skia_gl_context_.get();
      auto key = std::make_pair(egl_state->egl_display_,
                                egl_state->created_task_runner());
      auto found = egl_state_map_.find(key);
      CHECK(found != egl_state_map_.end());
      CHECK(found->second == egl_state);
      egl_state_map_.erase(found);

      // Send egl_state to the original thread for delete. Making
      // the original context current can only be done on the same thread.
      egl_state_for_skia_gl_context_->RemoveClient();
      base::SingleThreadTaskRunner* task_runner =
          egl_state_for_skia_gl_context_->created_task_runner_.get();
      task_runner->PostTask(FROM_HERE, base::DoNothingWithBoundArgs(std::move(
                                           egl_state_for_skia_gl_context_)));
    }
  }
  DCHECK(egl_state_map_.empty());
}

bool IOSurfaceImageBacking::ReadbackToMemory(
    const std::vector<SkPixmap>& pixmaps) {
  AutoLock auto_lock(this);
  CHECK_LE(pixmaps.size(), 3u);

  // Make sure any pending ANGLE EGLDisplays and Dawn devices are flushed.
  WaitForCommandsToBeScheduled();

  ScopedIOSurfaceLock io_surface_lock(io_surface_.get(),
                                      kIOSurfaceLockReadOnly);

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
  AutoLock auto_lock(this);
  CHECK_LE(pixmaps.size(), 3u);

  // Make sure any pending ANGLE EGLDisplays and Dawn devices are flushed.
  WaitForCommandsToBeScheduled();

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
  AutoLock auto_lock(this);

  gl::GLContext* context = gl::GLContext::GetCurrent();
  gl::GLDisplayEGL* display = context ? context->GetGLDisplayEGL() : nullptr;
  if (!display) {
    LOG(ERROR) << "No GLDisplayEGL current.";
    return nullptr;
  }
  const EGLDisplay egl_display = display->GetDisplay();

  auto key = std::make_pair(egl_display,
                            base::SingleThreadTaskRunner::GetCurrentDefault());
  auto found = egl_state_map_.find(key);
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
    gl_texture->SetEstimatedSize(format().EstimatedSizeInBytes(size()));

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
  AssertLockAcquired();
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

  return dump;
}

SharedImageBackingType IOSurfaceImageBacking::GetType() const {
  return SharedImageBackingType::kIOSurface;
}

std::unique_ptr<GLTextureImageRepresentation>
IOSurfaceImageBacking::ProduceGLTexture(SharedImageManager* manager,
                                        MemoryTypeTracker* tracker) {
  return nullptr;
}

std::unique_ptr<GLTexturePassthroughImageRepresentation>
IOSurfaceImageBacking::ProduceGLTexturePassthrough(SharedImageManager* manager,
                                                   MemoryTypeTracker* tracker) {
  scoped_refptr<IOSurfaceBackingEGLState> egl_state;
  egl_state = RetainGLTexture();

  // The corresponding release will be done when the returned representation is
  // destroyed, in GLTextureImageRepresentationBeingDestroyed.
  return std::make_unique<GLTextureIRepresentation>(
      manager, this, std::move(egl_state), tracker);
}

std::unique_ptr<OverlayImageRepresentation>
IOSurfaceImageBacking::ProduceOverlay(SharedImageManager* manager,
                                      MemoryTypeTracker* tracker) {
  return std::make_unique<OverlayRepresentation>(manager, this, tracker,
                                                 io_surface_);
}

int IOSurfaceImageBacking::TrackBeginAccessToWGPUTexture(
    wgpu::Texture texture) {
  AssertLockAcquired();

  return wgpu_texture_ongoing_accesses_[texture.Get()]++;
}

int IOSurfaceImageBacking::TrackEndAccessToWGPUTexture(wgpu::Texture texture) {
  AssertLockAcquired();

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

const scoped_refptr<DawnSharedTextureCache>&
IOSurfaceImageBacking::GetDawnTextureCache() {
  AssertLockAcquired();
  return dawn_texture_cache_;
}

void IOSurfaceImageBacking::WaitForCommandsToBeScheduled(
    id<MTLDevice> waiting_device) {
  AssertLockAcquired();
  TRACE_EVENT0("gpu", "IOSurfaceImageBacking::WaitForCommandsToBeScheduled");

  base::flat_map<wgpu::Device, wgpu::Future, WGPUDeviceCompare> futures_to_keep;
  for (const auto& [device, future] : wgpu_commands_scheduled_futures_) {
    id<MTLDevice> mtl_device = dawn::native::metal::GetMTLDevice(device.Get());
    if (mtl_device == waiting_device) {
      futures_to_keep.emplace(device, future);
      continue;
    }
    TRACE_EVENT0("gpu",
                 "IOSurfaceImageBacking::WaitForCommandsToBeScheduled::Dawn");
    wgpu::WaitStatus status =
        device.GetAdapter().GetInstance().WaitAny(future, UINT64_MAX);
    if (status != wgpu::WaitStatus::Success) {
      LOG(ERROR) << "WaitAny on commandsScheduledFuture failed with " << status;
    }
  }
  wgpu_commands_scheduled_futures_ = std::move(futures_to_keep);

  base::flat_map<EGLDisplay, std::unique_ptr<gl::GLFenceEGL>> fences_to_keep;
  for (auto& [display, fence] : egl_commands_scheduled_fences_) {
    if (QueryMetalDeviceFromANGLE(display) == waiting_device) {
      fences_to_keep.emplace(display, std::move(fence));
      continue;
    }
    TRACE_EVENT0("gpu",
                 "IOSurfaceImageBacking::WaitForCommandsToBeScheduled::ANGLE");
    fence->ClientWait();
  }
  egl_commands_scheduled_fences_ = std::move(fences_to_keep);
}

IOSurfaceRef IOSurfaceImageBacking::GetIOSurface() {
  return io_surface_.get();
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
    wgpu::SharedTextureMemory shared_texture_memory;
    {
      AutoLock auto_lock(this);

      // Clear out any cached SharedTextureMemory instances for which the
      // associated Device has been lost - this both saves memory and more
      // importantly ensures that a new SharedTextureMemory instance will be
      // created if another Device occupies the same memory as a
      // previously-used, now-lost Device.
      dawn_texture_cache_->EraseDataIfDeviceLost();

      CHECK(device.HasFeature(wgpu::FeatureName::SharedTextureMemoryIOSurface));

      shared_texture_memory =
          dawn_texture_cache_->GetSharedTextureMemory(device);
      if (!shared_texture_memory) {
        // NOTE: `shared_dawn_context` may be null if Graphite is not being
        // used.
        const auto* shared_dawn_context =
            context_state->dawn_context_provider();
        const bool is_graphite_device =
            shared_dawn_context &&
            shared_dawn_context->GetDevice().Get() == device.Get();

        wgpu::SharedTextureMemoryIOSurfaceDescriptor io_surface_desc;
        io_surface_desc.ioSurface = io_surface_.get();
        // Set storage binding usage only if explicitly needed for WebGPU - this
        // forces the MTLTexture wrapping the IOSurface to have ShaderWrite
        // usage which in turn prevents texture compression. It's possible this
        // doesn't have any effect given that IOSurfaces have linear layout, but
        // it might if the kernel chooses to create a separate allocation for
        // the GPU.
        io_surface_desc.allowStorageBinding =
            (usage().Has(SHARED_IMAGE_USAGE_WEBGPU_STORAGE_TEXTURE)) &&
            !is_graphite_device;

        wgpu::SharedTextureMemoryDescriptor desc = {};
        desc.nextInChain = &io_surface_desc;

        shared_texture_memory = device.ImportSharedTextureMemory(&desc);
        // If ImportSharedTextureMemory is not successful and the device is not
        // lost, an error SharedTextureMemory object will be returned, which
        // will cause an error upon usage.
        if (shared_texture_memory.IsDeviceLost()) {
          LOG(ERROR)
              << "Failed to create shared texture memory due to device loss.";
          return nullptr;
        }

        // We cache the SharedTextureMemory instance that is associated with the
        // Graphite device.
        // TODO(crbug.com/345674550): Extend caching to WebGPU devices as well.
        if (is_graphite_device) {
          // This is the Graphite device, so we cache its SharedTextureMemory
          // instance.
          dawn_texture_cache_->MaybeCacheSharedTextureMemory(
              device, shared_texture_memory);
        }
      }
    }

    // SharedImageRepresentation handles lock.
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

  {
    AutoLock auto_lock(this);
    for (int plane_index = 0; plane_index < format().NumberOfPlanes();
         plane_index++) {
      GLFormatDesc format_desc =
          context_state->GetGLFormatCaps().ToGLFormatDesc(format(),
                                                          plane_index);
      GrBackendTexture backend_texture;
      auto plane_size = format().GetPlaneSize(plane_index, size());
      GetGrBackendTexture(
          context_state->feature_info(), egl_state->GetGLTarget(), plane_size,
          egl_state->GetGLServiceId(plane_index),
          format_desc.storage_internal_format,
          context_state->gr_context()->threadSafeProxy(), &backend_texture);
      sk_sp<GrPromiseImageTexture> promise_texture =
          GrPromiseImageTexture::Make(backend_texture);
      if (!promise_texture) {
        return nullptr;
      }
      promise_textures.push_back(std::move(promise_texture));
    }
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
    // No AutoLock here. Lock is handled in ProduceDawn().
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
    if (backend_type == wgpu::BackendType::Metal) {
      return std::make_unique<SkiaGraphiteDawnMetalRepresentation>(
          std::move(dawn_representation), context_state,
          context_state->gpu_main_graphite_recorder(), manager, this, tracker);
    }

    // Use default skia representation
    CHECK_EQ(backend_type, wgpu::BackendType::Vulkan);
    return std::make_unique<SkiaGraphiteDawnImageRepresentation>(
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
    return std::make_unique<SkiaGraphiteMetalRepresentation>(
        manager, this, tracker, context_state->gpu_main_graphite_recorder(),
        std::move(mtl_textures));
#else
    NOTREACHED();
#endif
  }
}

void IOSurfaceImageBacking::SetPurgeable(bool purgeable) {
  AutoLock auto_lock(this);
  if (purgeable_ == purgeable)
    return;
  purgeable_ = purgeable;

  if (purgeable) {
    // It is in error to purge the surface while reading or writing to it.
    DCHECK(!ongoing_write_access_);
    DCHECK(!num_ongoing_read_accesses_);

    SetClearedRectInternal(gfx::Rect());
  }

  uint32_t old_state;
  IOSurfaceSetPurgeable(io_surface_.get(), purgeable, &old_state);
}

bool IOSurfaceImageBacking::IsPurgeable() const {
  AutoLock auto_lock(this);
  return purgeable_;
}

void IOSurfaceImageBacking::Update(std::unique_ptr<gfx::GpuFence> in_fence) {
  AutoLock auto_lock(this);
#if BUILDFLAG(IS_IOS)
  {
    // On iOS, we can't use IOKit to access IOSurfaces in the renderer process,
    // so we share the memory segment backing the IOSurface as shared memory
    // which is then mapped in the renderer process. We need to signal that the
    // IOSurface was updated on the CPU so we do an IOSurfaceLock+Unlock here in
    // case there are other consumers of the IOSurface that rely on its internal
    // seed value to detect updates - the lock+unlock updates the seed value.
    // TODO(crbug.com/40254930): Assert that we have CPU_WRITE_ONLY usage so
    // that we never have the client's CPU-written data overwritten due to a
    // shadow copy from the GPU - we can also use kIOSurfaceLockAvoidSync then.
    ScopedIOSurfaceLock io_surface_lock(io_surface_.get(), /*options=*/0);
  }
#endif
  for (auto iter : egl_state_map_) {
    iter.second->set_bind_pending();
  }
}

gfx::GpuMemoryBufferHandle IOSurfaceImageBacking::GetGpuMemoryBufferHandle() {
  return gfx::GpuMemoryBufferHandle(io_surface_);
}

std::unique_ptr<WebNNTensorRepresentation>
IOSurfaceImageBacking::ProduceWebNNTensor(SharedImageManager* manager,
                                          MemoryTypeTracker* tracker) {
  CHECK(usage().Has(SHARED_IMAGE_USAGE_WEBNN_SHARED_TENSOR));
  return std::make_unique<WebNNIOSurfaceTensorRepresentation>(manager, this,
                                                              tracker);
}

bool IOSurfaceImageBacking::BeginAccessWebNN() {
  AutoLock auto_lock(this);
  if (!BeginAccess(/*readonly=*/false)) {
    return false;
  }
  WaitForCommandsToBeScheduled();
  return true;
}

void IOSurfaceImageBacking::EndAccessWebNN() {
  AutoLock auto_lock(this);

  EndAccess(/*readonly=*/false);
}

std::unique_ptr<DawnBufferRepresentation>
IOSurfaceImageBacking::ProduceDawnBuffer(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker,
    const wgpu::Device& device,
    wgpu::BackendType backend_type,
    scoped_refptr<SharedContextState> context_state) {
  auto dawn_image_representation =
      ProduceDawn(manager, tracker, device, backend_type, /*view_formats=*/{},
                  context_state);
  if (!dawn_image_representation) {
    return nullptr;
  }
  return std::make_unique<DawnBufferCopyRepresentation>(
      manager, this, tracker, device, std::move(dawn_image_representation));
}

bool IOSurfaceImageBacking::BeginAccess(bool readonly) {
  AssertLockAcquired();

  CHECK_GE(num_ongoing_read_accesses_, 0);

  if (!readonly && ongoing_write_access_) {
    DLOG(ERROR) << "Unable to begin write access because another "
                   "write access is in progress";
    return false;
  }
  // Track reads and writes if not being used for concurrent read/writes.
  if (!(usage().Has(SHARED_IMAGE_USAGE_CONCURRENT_READ_WRITE))) {
    if (readonly && ongoing_write_access_) {
      DLOG(ERROR) << "Unable to begin read access because another "
                     "write access is in progress";
      return false;
    }
    if (!readonly && num_ongoing_read_accesses_ > 0) {
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
  AssertLockAcquired();

  if (readonly) {
    CHECK_GT(num_ongoing_read_accesses_, 0);
    if (!(usage().Has(SHARED_IMAGE_USAGE_CONCURRENT_READ_WRITE))) {
      CHECK(!ongoing_write_access_);
    }
    num_ongoing_read_accesses_--;
  } else {
    CHECK(ongoing_write_access_);
    if (!(usage().Has(SHARED_IMAGE_USAGE_CONCURRENT_READ_WRITE))) {
      CHECK_EQ(num_ongoing_read_accesses_, 0);
    }
    ongoing_write_access_ = false;
  }
}

bool IOSurfaceImageBacking::IOSurfaceBackingEGLStateBeginAccess(
    IOSurfaceBackingEGLState* egl_state,
    bool readonly) {
  AssertLockAcquired();

  // It is in error to read or write an IOSurface while it is purgeable.
  CHECK(!purgeable_);
  if (!BeginAccess(readonly)) {
    return false;
  }

  CHECK_GE(egl_state->num_ongoing_accesses_, 0);

  gl::GLDisplayEGL* display = gl::GLDisplayEGL::GetDisplayForCurrentContext();
  CHECK(display);

  EGLDisplay egl_display = display->GetDisplay();
  CHECK_EQ(egl_display, egl_state->egl_display_);

  // Note that we don't need to wait for commands to be scheduled for other
  // EGLDisplays because it is already done when the previous GL context is made
  // uncurrent. We can simply remove fences for the other EGLDisplays.
  base::EraseIf(egl_commands_scheduled_fences_, [egl_display](const auto& kv) {
    return kv.first != egl_display;
  });

  // IOSurface might be written on a different queue. So we have to wait for the
  // previous Dawn and ANGLE commands to be scheduled first so that the kernel
  // knows about the pending update to the IOSurface.
  WaitForCommandsToBeScheduled(QueryMetalDeviceFromANGLE(egl_display));

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
    egl_state->num_ongoing_accesses_++;
    return true;
  }

  if (egl_state->egl_surfaces_.empty()) {
    std::vector<std::unique_ptr<gl::ScopedEGLSurfaceIOSurface>> egl_surfaces;
    for (int plane_index = 0; plane_index < format().NumberOfPlanes();
         plane_index++) {
      viz::SharedImageFormat plane_format;
      if (format().is_single_plane()) {
        plane_format = format();
        // See comments in IOSurfaceImageBackingFactory::CreateSharedImage about
        // RGBA versus BGRA when using Skia Ganesh GL backend or ANGLE.
        if (io_surface_format_ == 'BGRA') {
          if (plane_format == viz::SinglePlaneFormat::kRGBA_8888) {
            plane_format = viz::SinglePlaneFormat::kBGRA_8888;
          } else if (plane_format == viz::SinglePlaneFormat::kRGBX_8888) {
            plane_format = viz::SinglePlaneFormat::kBGRX_8888;
          }
        }
      } else {
        // For multiplanar formats (without external sampler) get planar buffer
        // format.
        plane_format = GetFormatForPlane(format(), plane_index);
      }

      auto egl_surface = gl::ScopedEGLSurfaceIOSurface::Create(
          egl_state->egl_display_, egl_state->GetGLTarget(), io_surface_.get(),
          plane_index, plane_format);
      if (!egl_surface) {
        LOG(ERROR) << "Failed to create ScopedEGLSurfaceIOSurface.";
        EndAccess(readonly);
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
      LOG(ERROR) << "Failed to bind ScopedEGLSurfaceIOSurface to target.";
      EndAccess(readonly);
      return false;
    }
  }
  egl_state->clear_bind_pending();
  egl_state->num_ongoing_accesses_++;

  return true;
}

void IOSurfaceImageBacking::IOSurfaceBackingEGLStateEndAccess(
    IOSurfaceBackingEGLState* egl_state,
    bool readonly) {
  AssertLockAcquired();

  // Early out if BeginAccess didn't succeed and we didn't bind any surfaces.
  if (egl_state->is_bind_pending()) {
    return;
  }

  CHECK_GT(egl_state->num_ongoing_accesses_, 0);
  egl_state->num_ongoing_accesses_--;

  EndAccess(readonly);

  gl::GLDisplayEGL* display = gl::GLDisplayEGL::GetDisplayForCurrentContext();
  CHECK(display);

  EGLDisplay egl_display = display->GetDisplay();
  CHECK_EQ(egl_display, egl_state->egl_display_);

  const bool is_angle_metal =
      gl::GetANGLEImplementation() == gl::ANGLEImplementation::kMetal;
  if (is_angle_metal) {
    id<MTLSharedEvent> shared_event = nil;
    uint64_t signal_value = 0;
    if (display->CreateMetalSharedEvent(&shared_event, &signal_value)) {
      AddSharedEventForEndAccess(shared_event, signal_value, readonly);
    } else {
      LOG(DFATAL) << "Failed to create Metal shared event";
    }
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
  // triggers a release and copy). By design, IOSurfaceImageBackingFactory
  // enforces this property for this use case.
  //
  // For ANGLE Metal, we need to rebind the texture so that the BindTexImage
  // adds a synchronization dependency on the command buffer which contains the
  // shared event wait before the next ANGLE access. Otherwise, ANGLE might skip
  // waiting on the command buffer and hence the shared event and do a CPU
  // readback from the IOSurface in some cases without synchronization.
  const bool is_swangle =
      gl::GetANGLEImplementation() == gl::ANGLEImplementation::kSwiftShader;
  if ((is_swangle || is_angle_metal) && egl_state->num_ongoing_accesses_ == 0) {
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

  // We have to wait for pending work to be scheduled on the GPU for IOSurface
  // synchronization by the kernel e.g. using waitUntilScheduled on Metal or
  // glFlush on OpenGL.
  if (is_angle_metal) {
    // Defer the wait until CoreAnimation, Dawn, or another ANGLE EGLDisplay
    // needs to access to avoid unnecessary overhead. This also ensures that the
    // Metal shared event signal which is enqueued above is flushed.
    auto fence = gl::GLFenceEGL::Create(EGL_SYNC_METAL_COMMANDS_SCHEDULED_ANGLE,
                                        nullptr);
    if (fence) {
      egl_commands_scheduled_fences_.insert_or_assign(egl_display,
                                                      std::move(fence));
    } else {
      LOG(ERROR)
          << "Failed to create EGL_SYNC_METAL_COMMANDS_SCHEDULED_ANGLE fence";
    }
  } else {
    eglWaitUntilWorkScheduledANGLE(egl_display);
  }
}

void IOSurfaceImageBacking::IOSurfaceBackingEGLStateBeingCreated(
    IOSurfaceBackingEGLState* egl_state) {
  AssertLockAcquired();

  auto key =
      std::make_pair(egl_state->egl_display_, egl_state->created_task_runner());
  auto insert_result = egl_state_map_.insert(std::make_pair(key, egl_state));
  CHECK(insert_result.second);
}

void IOSurfaceImageBacking::IOSurfaceBackingEGLStateBeingDestroyed(
    IOSurfaceBackingEGLState* egl_state,
    bool has_context) {
  AssertLockAcquired();
  ReleaseGLTexture(egl_state, has_context);

  egl_state->egl_surfaces_.clear();

  // Remove `egl_state` from `egl_state_map_`.
  auto key =
      std::make_pair(egl_state->egl_display_, egl_state->created_task_runner());
  auto found = egl_state_map_.find(key);
  CHECK(found != egl_state_map_.end());
  CHECK(found->second == egl_state);
  egl_state_map_.erase(found);
}

bool IOSurfaceImageBacking::InitializePixels(
    base::span<const uint8_t> pixel_data) {
  AutoLock auto_lock(this);
  CHECK(format().is_single_plane());
  ScopedIOSurfaceLock io_surface_lock(io_surface_.get(),
                                      kIOSurfaceLockAvoidSync);

  uint8_t* dst_data = reinterpret_cast<uint8_t*>(
      IOSurfaceGetBaseAddressOfPlane(io_surface_.get(), 0));
  size_t dst_stride = IOSurfaceGetBytesPerRowOfPlane(io_surface_.get(), 0);

  const uint8_t* src_data = pixel_data.data();
  const size_t src_stride = format().BytesPerPixel() * size().width();
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
  AssertLockAcquired();

  SharedEventMap& shared_events =
      readonly ? non_exclusive_shared_events_ : exclusive_shared_events_;
  auto [it, _] = shared_events.insert(
      {ScopedSharedEvent(shared_event, base::scoped_policy::RETAIN), 0});
  it->second = std::max(it->second, signal_value);
}

void IOSurfaceImageBacking::ProcessSharedEventsForBeginAccess(
    bool readonly,
    base::FunctionRef<void(id<MTLSharedEvent> shared_event,
                           uint64_t signaled_value)> process_fn) {
  AssertLockAcquired();

  // Always need wait on exclusive access end events.
  for (const auto& [shared_event, signal_value] : exclusive_shared_events_) {
    process_fn(shared_event.get(), signal_value);
  }

  if (!readonly) {
    // For read-write (exclusive) access, non execlusive access end events
    // should be waited on as well.
    for (const auto& [shared_event, signal_value] :
         non_exclusive_shared_events_) {
      process_fn(shared_event.get(), signal_value);
    }

    // Clear events, since this read-write (exclusive) access will provide an
    // event when the access is finished.
    exclusive_shared_events_.clear();
    non_exclusive_shared_events_.clear();
  }
}

}  // namespace gpu
