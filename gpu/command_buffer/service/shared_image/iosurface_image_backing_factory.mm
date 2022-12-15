// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/iosurface_image_backing_factory.h"

#include "base/mac/scoped_cftyperef.h"
#include "base/mac/scoped_nsobject.h"
#include "base/memory/scoped_refptr.h"
#include "build/build_config.h"
#include "components/viz/common/gpu/metal_context_provider.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "components/viz/common/resources/resource_sizes.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/image_factory.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image/iosurface_image_backing.h"
#include "gpu/command_buffer/service/shared_image/shared_image_backing.h"
#include "gpu/command_buffer/service/shared_image/shared_image_factory.h"
#include "gpu/command_buffer/service/shared_image/shared_image_format_utils.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"
#include "gpu/command_buffer/service/skia_utils.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/skia/include/core/SkPromiseImageTexture.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/mac/display_icc_profiles.h"
#include "ui/gfx/mac/io_surface.h"
#include "ui/gl/buffer_format_utils.h"
#include "ui/gl/buildflags.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_implementation.h"

#import <Metal/Metal.h>

// Usage of BUILDFLAG(USE_DAWN) needs to be after the include for
// ui/gl/buildflags.h
#if BUILDFLAG(USE_DAWN)
#include <dawn/native/MetalBackend.h>
#endif  // BUILDFLAG(USE_DAWN)

namespace gpu {

namespace {
base::scoped_nsprotocol<id<MTLTexture>> CreateMetalTexture(
    id<MTLDevice> mtl_device,
    IOSurfaceRef io_surface,
    const gfx::Size& size,
    viz::SharedImageFormat format) {
  TRACE_EVENT0("gpu", "IOSurfaceImageBackingFactory::CreateMetalTexture");
  base::scoped_nsprotocol<id<MTLTexture>> mtl_texture;
  MTLPixelFormat mtl_pixel_format =
      static_cast<MTLPixelFormat>(ToMTLPixelFormat(format));
  if (mtl_pixel_format == MTLPixelFormatInvalid)
    return mtl_texture;

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
  [mtl_tex_desc setStorageMode:MTLStorageModePrivate];
  mtl_texture.reset([mtl_device newTextureWithDescriptor:mtl_tex_desc
                                               iosurface:io_surface
                                                   plane:0]);
  DCHECK(mtl_texture);
  return mtl_texture;
}

bool IsFormatSupported(viz::ResourceFormat resource_format) {
  switch (resource_format) {
    case viz::ResourceFormat::RGBA_8888:
    case viz::ResourceFormat::RGBX_8888:
    case viz::ResourceFormat::BGRA_8888:
    case viz::ResourceFormat::BGRX_8888:
    case viz::ResourceFormat::RGBA_F16:
    case viz::ResourceFormat::RED_8:
    case viz::ResourceFormat::BGRA_1010102:
    case viz::ResourceFormat::RGBA_1010102:
      return true;
    default:
      return false;
  }
}

void SetIOSurfaceColorSpace(IOSurfaceRef io_surface,
                            const gfx::ColorSpace& color_space) {
  if (!color_space.IsValid()) {
    return;
  }

  base::ScopedCFTypeRef<CFDataRef> cf_data =
      gfx::DisplayICCProfiles::GetInstance()->GetDataForColorSpace(color_space);
  if (cf_data) {
    IOSurfaceSetValue(io_surface, CFSTR("IOSurfaceColorSpace"), cf_data);
  } else {
    IOSurfaceSetColorSpace(io_surface, color_space);
  }
}

}  // anonymous namespace

// Representation of a SharedImageBackingIOSurface as a Dawn Texture.
#if BUILDFLAG(USE_DAWN)
class DawnIOSurfaceRepresentation : public DawnImageRepresentation {
 public:
  DawnIOSurfaceRepresentation(SharedImageManager* manager,
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

  ~DawnIOSurfaceRepresentation() override {
    EndAccess();
    dawn_procs_.deviceRelease(device_);
  }

  WGPUTexture BeginAccess(WGPUTextureUsage usage) final {
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

    // If the backing is compatible - essentially, a GLImageIOSurface -
    // then synchronize with all of the MTLSharedEvents which have been
    // stored in it as a consequence of earlier BeginAccess/EndAccess calls
    // against other representations.
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

  void EndAccess() final {
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
        iosurface_backing->AddSharedEventAndSignalValue(
            descriptor.sharedEvent, descriptor.signaledValue);
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

 private:
  base::ScopedCFTypeRef<IOSurfaceRef> io_surface_;
  WGPUDevice device_;
  WGPUTexture texture_ = nullptr;
  WGPUTextureFormat wgpu_format_;
  std::vector<WGPUTextureFormat> view_formats_;

  // TODO(cwallez@chromium.org): Load procs only once when the factory is
  // created and pass a pointer to them around?
  DawnProcTable dawn_procs_;
};
#endif  // BUILDFLAG(USE_DAWN)

// static
sk_sp<SkPromiseImageTexture>
IOSurfaceImageBackingFactory::ProduceSkiaPromiseTextureMetal(
    SharedImageBacking* backing,
    scoped_refptr<SharedContextState> context_state,
    gfx::ScopedIOSurface io_surface,
    uint32_t io_surface_plane) {
  DCHECK(context_state->GrContextIsMetal());
  DCHECK(!io_surface_plane);

  id<MTLDevice> mtl_device =
      context_state->metal_context_provider()->GetMTLDevice();
  auto mtl_texture = CreateMetalTexture(mtl_device, io_surface.get(),
                                        backing->size(), backing->format());
  DCHECK(mtl_texture);

  GrMtlTextureInfo info;
  info.fTexture.retain(mtl_texture.get());
  auto gr_backend_texture =
      GrBackendTexture(backing->size().width(), backing->size().height(),
                       GrMipMapped::kNo, info);
  return SkPromiseImageTexture::Make(gr_backend_texture);
}

// static
std::unique_ptr<DawnImageRepresentation>
IOSurfaceImageBackingFactory::ProduceDawn(
    SharedImageManager* manager,
    SharedImageBacking* backing,
    MemoryTypeTracker* tracker,
    WGPUDevice device,
    std::vector<WGPUTextureFormat> view_formats,
    gfx::ScopedIOSurface io_surface,
    uint32_t io_surface_plane) {
  DCHECK(!io_surface_plane);
#if BUILDFLAG(USE_DAWN)
  // See comments in IOSurfaceImageBackingFactory::CreateSharedImage
  // regarding RGBA versus BGRA.
  viz::SharedImageFormat actual_format = backing->format();
  if (actual_format == viz::SharedImageFormat::kRGBA_8888)
    actual_format = viz::SharedImageFormat::kBGRA_8888;

  // TODO(crbug.com/1293514): Remove this if condition after using single
  // multiplanar mailbox and actual_format could report multiplanar format
  // correctly.
  if (IOSurfaceGetPixelFormat(io_surface) == '420v')
    actual_format = viz::SharedImageFormat::SinglePlane(viz::YUV_420_BIPLANAR);

  absl::optional<WGPUTextureFormat> wgpu_format = ToWGPUFormat(actual_format);
  if (wgpu_format.value() == WGPUTextureFormat_Undefined)
    return nullptr;

  return std::make_unique<DawnIOSurfaceRepresentation>(
      manager, backing, tracker, device, io_surface, wgpu_format.value(),
      std::move(view_formats));
#else   // BUILDFLAG(USE_DAWN)
  return nullptr;
#endif  // BUILDFLAG(USE_DAWN)
}

// static
bool IOSurfaceImageBackingFactory::InitializePixels(
    SharedImageBacking* backing,
    gfx::ScopedIOSurface io_surface,
    uint32_t io_surface_plane,
    const uint8_t* src_data) {
  IOReturn r = IOSurfaceLock(io_surface, kIOSurfaceLockAvoidSync, nullptr);
  DCHECK_EQ(kIOReturnSuccess, r);

  uint8_t* dst_data = reinterpret_cast<uint8_t*>(
      IOSurfaceGetBaseAddressOfPlane(io_surface, io_surface_plane));
  size_t dst_stride =
      IOSurfaceGetBytesPerRowOfPlane(io_surface, io_surface_plane);
  const size_t src_stride =
      (BitsPerPixel(backing->format()) / 8) * backing->size().width();

  size_t height = backing->size().height();
  for (size_t y = 0; y < height; ++y) {
    memcpy(dst_data, src_data, src_stride);
    dst_data += dst_stride;
    src_data += src_stride;
  }

  r = IOSurfaceUnlock(io_surface, 0, nullptr);
  DCHECK_EQ(kIOReturnSuccess, r);
  return true;
}

///////////////////////////////////////////////////////////////////////////////
// IOSurfaceImageBackingFactory

IOSurfaceImageBackingFactory::IOSurfaceImageBackingFactory(
    const GpuPreferences& gpu_preferences,
    const GpuDriverBugWorkarounds& workarounds,
    const gles2::FeatureInfo* feature_info,
    ImageFactory* image_factory,
    gl::ProgressReporter* progress_reporter)
    : GLCommonImageBackingFactory(gpu_preferences,
                                  workarounds,
                                  feature_info,
                                  progress_reporter),
      image_factory_(image_factory) {
  gpu_memory_buffer_formats_ =
      feature_info->feature_flags().gpu_memory_buffer_formats;
}

IOSurfaceImageBackingFactory::~IOSurfaceImageBackingFactory() = default;

std::unique_ptr<SharedImageBacking>
IOSurfaceImageBackingFactory::CreateSharedImage(
    const Mailbox& mailbox,
    viz::SharedImageFormat format,
    SurfaceHandle surface_handle,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage,
    bool is_thread_safe) {
  DCHECK(!is_thread_safe);
  return CreateSharedImageInternal(mailbox, format, surface_handle, size,
                                   color_space, surface_origin, alpha_type,
                                   usage, base::span<const uint8_t>());
}

std::unique_ptr<SharedImageBacking>
IOSurfaceImageBackingFactory::CreateSharedImage(
    const Mailbox& mailbox,
    viz::SharedImageFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage,
    base::span<const uint8_t> pixel_data) {
  return CreateSharedImageInternal(mailbox, format, kNullSurfaceHandle, size,
                                   color_space, surface_origin, alpha_type,
                                   usage, pixel_data);
}

std::unique_ptr<SharedImageBacking>
IOSurfaceImageBackingFactory::CreateSharedImage(
    const Mailbox& mailbox,
    int client_id,
    gfx::GpuMemoryBufferHandle handle,
    gfx::BufferFormat buffer_format,
    gfx::BufferPlane plane,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage) {
  if (handle.type != gfx::IO_SURFACE_BUFFER || !handle.io_surface) {
    LOG(ERROR) << "Invalid IOSurface GpuMemoryBufferHandle.";
    return nullptr;
  }

  if (!gpu_memory_buffer_formats_.Has(buffer_format)) {
    LOG(ERROR) << "CreateSharedImage: unsupported buffer format "
               << gfx::BufferFormatToString(buffer_format);
    return nullptr;
  }

  if (!gpu::IsPlaneValidForGpuMemoryBufferFormat(plane, buffer_format)) {
    LOG(ERROR) << "Invalid plane " << gfx::BufferPlaneToString(plane) << " for "
               << gfx::BufferFormatToString(buffer_format);
    return nullptr;
  }

  // Note that `size` refers to the size of the IOSurface, not the `plane`
  // that is specified. This parameter should probably be ignored.
  if (!gpu::IsImageSizeValidForGpuMemoryBufferFormat(size, buffer_format)) {
    LOG(ERROR) << "Invalid size " << size.ToString() << " for "
               << gfx::BufferFormatToString(buffer_format);
    return nullptr;
  }

  const GLenum target = gpu::GetPlatformSpecificTextureTarget();
  auto io_surface = handle.io_surface;
  const auto io_surface_id = handle.id;
  const uint32_t io_surface_plane = GetPlaneIndex(plane, buffer_format);

  // Ensure that the IOSurface has the same size and pixel format as those
  // specified by `size` and `buffer_format`. A malicious client could lie about
  // this, which, if subsequently used to determine parameters for bounds
  // checking, could result in an out-of-bounds memory access.
  {
    uint32_t io_surface_format = IOSurfaceGetPixelFormat(io_surface);
    if (io_surface_format !=
        BufferFormatToIOSurfacePixelFormat(buffer_format)) {
      DLOG(ERROR)
          << "IOSurface pixel format does not match specified buffer format.";
      return nullptr;
    }
    gfx::Size io_surface_size(IOSurfaceGetWidth(io_surface),
                              IOSurfaceGetHeight(io_surface));
    if (io_surface_size != size) {
      DLOG(ERROR) << "IOSurface size does not match specified size.";
      return nullptr;
    }
  }

  const gfx::BufferFormat plane_buffer_format =
      GetPlaneBufferFormat(plane, buffer_format);
  const viz::ResourceFormat plane_resource_format =
      viz::GetResourceFormat(plane_buffer_format);
  const gfx::Size plane_size = gpu::GetPlaneSize(plane, size);

  const bool for_framebuffer_attachment =
      (usage & (SHARED_IMAGE_USAGE_RASTER |
                SHARED_IMAGE_USAGE_GLES2_FRAMEBUFFER_HINT)) != 0;

  const bool framebuffer_attachment_angle =
      for_framebuffer_attachment && texture_usage_angle_;

  auto si_format = viz::SharedImageFormat::SinglePlane(plane_resource_format);
  DCHECK(use_passthrough_);
  return std::make_unique<IOSurfaceImageBacking>(
      io_surface, io_surface_plane, plane_buffer_format, io_surface_id, mailbox,
      si_format, plane_size, color_space, surface_origin, alpha_type, usage,
      target, framebuffer_attachment_angle, /*is_cleared=*/true);
}

bool IOSurfaceImageBackingFactory::IsSupported(
    uint32_t usage,
    viz::SharedImageFormat format,
    const gfx::Size& size,
    bool thread_safe,
    gfx::GpuMemoryBufferType gmb_type,
    GrContextType gr_context_type,
    base::span<const uint8_t> pixel_data) {
  if (format.is_multi_plane()) {
    return false;
  }
  if (!pixel_data.empty() && gr_context_type != GrContextType::kGL) {
    return false;
  }
  if (thread_safe) {
    return false;
  }
  // Never used with shared memory GMBs.
  if (gmb_type == gfx::SHARED_MEMORY_BUFFER) {
    return false;
  }
  if (usage & SHARED_IMAGE_USAGE_CPU_UPLOAD) {
    return false;
  }
  // On macOS, there is no separate interop factory. Any GpuMemoryBuffer-backed
  // image can be used with both OpenGL and Metal

  // In certain modes on Mac, Angle needs the image to be released when ending a
  // write. To avoid that release resulting in the GLES2 command decoders
  // needing to perform on-demand binding, we disallow concurrent read/write in
  // these modes. See
  // IOSurfaceImageBacking::GLTextureImageRepresentationEndAccess() for further
  // details.
  // TODO(https://anglebug.com/7626): Adjust the Metal-related conditions here
  // if/as they are adjusted in
  // IOSurfaceImageBacking::GLTextureImageRepresentationEndAccess().
  if (gl::GetANGLEImplementation() == gl::ANGLEImplementation::kSwiftShader ||
      gl::GetANGLEImplementation() == gl::ANGLEImplementation::kMetal) {
    if (usage & SHARED_IMAGE_USAGE_CONCURRENT_READ_WRITE) {
      return false;
    }
  }

  return true;
}

std::unique_ptr<SharedImageBacking>
IOSurfaceImageBackingFactory::CreateSharedImageInternal(
    const Mailbox& mailbox,
    viz::SharedImageFormat format,
    SurfaceHandle surface_handle,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage,
    base::span<const uint8_t> pixel_data) {
  const FormatInfo& format_info = GetFormatInfo(format);
  const gfx::BufferFormat buffer_format =
      viz::BufferFormat(format.resource_format());

  if (!IsFormatSupported(format.resource_format()) ||
      !gpu_memory_buffer_formats_.Has(buffer_format)) {
    LOG(ERROR) << "CreateSharedImage: SCANOUT shared images unavailable. "
                  "Format= "
               << format.ToString();
    return nullptr;
  }

  GLenum texture_target = gpu::GetPlatformSpecificTextureTarget();
  if (!CanCreateSharedImage(size, pixel_data, format_info, texture_target)) {
    return nullptr;
  }

  const bool for_framebuffer_attachment =
      (usage & (SHARED_IMAGE_USAGE_RASTER |
                SHARED_IMAGE_USAGE_GLES2_FRAMEBUFFER_HINT)) != 0;

  // |scoped_progress_reporter| will notify |progress_reporter_| upon
  // construction and destruction. We limit the scope so that progress is
  // reported immediately after allocation/upload and before other GL
  // operations.
  gfx::ScopedIOSurface io_surface;
  const uint32_t io_surface_plane = 0;
  const gfx::GenericSharedMemoryId io_surface_id;
  {
    gl::ScopedProgressReporter scoped_progress_reporter(progress_reporter_);
    const bool should_clear = false;
    io_surface.reset(gfx::CreateIOSurface(size, buffer_format, should_clear));
    if (!io_surface) {
      LOG(ERROR) << "CreateSharedImage: Failed to create bindable image";
      return nullptr;
    }
  }
  SetIOSurfaceColorSpace(io_surface.get(), color_space);

  const bool is_cleared = !pixel_data.empty();
  const bool framebuffer_attachment_angle =
      for_framebuffer_attachment && texture_usage_angle_;

  DCHECK(!format_info.swizzle);
  DCHECK(use_passthrough_);
  auto result = std::make_unique<IOSurfaceImageBacking>(
      io_surface, io_surface_plane, buffer_format, io_surface_id, mailbox,
      format, size, color_space, surface_origin, alpha_type, usage,
      texture_target, framebuffer_attachment_angle, is_cleared);
  if (!pixel_data.empty()) {
    gl::ScopedProgressReporter scoped_progress_reporter(progress_reporter_);
    result->InitializePixels(format_info.adjusted_format, format_info.gl_type,
                             pixel_data.data());
  }
  return std::move(result);
}

}  // namespace gpu
