// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image_backing_factory_iosurface.h"

#include "base/mac/scoped_cftyperef.h"
#include "base/mac/scoped_nsobject.h"
#include "base/memory/scoped_refptr.h"
#include "base/optional.h"
#include "components/viz/common/gpu/metal_context_provider.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "components/viz/common/resources/resource_sizes.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image_backing.h"
#include "gpu/command_buffer/service/shared_image_representation.h"
#include "gpu/command_buffer/service/skia_utils.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "third_party/skia/include/core/SkPromiseImageTexture.h"
#include "ui/gfx/mac/io_surface.h"
#include "ui/gl/buildflags.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_image_io_surface.h"

#import <Metal/Metal.h>

// Usage of BUILDFLAG(USE_DAWN) needs to be after the include for
// ui/gl/buildflags.h
#if BUILDFLAG(USE_DAWN)
#include <dawn_native/MetalBackend.h>
#endif  // BUILDFLAG(USE_DAWN)

namespace gpu {

namespace {

struct GLFormatInfo {
  bool supported = false;

  // GL internal_format/format/type triplet.
  GLuint internal_format = 0;
  GLenum format = 0;
  GLenum type = 0;
};

// Get GL format triplets and modify them to match the logic in
// gl_image_iosurface.mm
GLFormatInfo GetGLFormatInfo(viz::ResourceFormat format) {
  GLFormatInfo info = {
      true,
      viz::GLInternalFormat(format),
      viz::GLDataFormat(format),
      viz::GLDataType(format),
  };

  if (info.internal_format == GL_ZERO || info.format == GL_ZERO ||
      info.type == GL_ZERO) {
    return {false, GL_ZERO, GL_ZERO, GL_ZERO};
  }

  switch (format) {
    case viz::BGRA_8888:
      info.format = GL_RGBA;
      info.internal_format = GL_RGBA;
      break;

      // Technically we should use GL_RGB but CGLTexImageIOSurface2D() (and
      // OpenGL ES 3.0, for the case) support only GL_RGBA (the hardware ignores
      // the alpha channel anyway), see https://crbug.com/797347.
    case viz::BGRX_1010102:
      info.format = GL_RGBA;
      info.internal_format = GL_RGBA;
      break;

    default:
      break;
  }

  return info;
}

void FlushIOSurfaceGLOperations() {
  // The CGLTexImageIOSurface2D documentation says that we need to call
  // glFlush, otherwise there is the risk of a race between different
  // graphics contexts.
  gl::GLApi* api = gl::g_current_gl_context;
  api->glFlushFn();
}

base::Optional<WGPUTextureFormat> GetWGPUFormat(viz::ResourceFormat format) {
  switch (format) {
    case viz::RED_8:
    case viz::ALPHA_8:
    case viz::LUMINANCE_8:
      return WGPUTextureFormat_R8Unorm;
    case viz::RG_88:
      return WGPUTextureFormat_RG8Unorm;
    case viz::RGBA_8888:
    case viz::BGRA_8888:
      return WGPUTextureFormat_BGRA8Unorm;
    default:
      return {};
  }
}

base::Optional<WGPUTextureFormat> GetWGPUFormat(gfx::BufferFormat format) {
  switch (format) {
    case gfx::BufferFormat::R_8:
      return WGPUTextureFormat_R8Unorm;
    case gfx::BufferFormat::RG_88:
      return WGPUTextureFormat_RG8Unorm;
    case gfx::BufferFormat::RGBX_8888:
    case gfx::BufferFormat::RGBA_8888:
    case gfx::BufferFormat::BGRX_8888:
      return WGPUTextureFormat_BGRA8Unorm;
    default:
      return {};
  }
}

base::scoped_nsprotocol<id<MTLTexture>> API_AVAILABLE(macos(10.11))
    CreateMetalTexture(id<MTLDevice> mtl_device,
                       IOSurfaceRef io_surface,
                       const gfx::Size& size,
                       viz::ResourceFormat format) {
  TRACE_EVENT0("gpu", "SharedImageBackingFactoryIOSurface::CreateMetalTexture");
  base::scoped_nsprotocol<id<MTLTexture>> mtl_texture;
  MTLPixelFormat mtl_pixel_format;
  switch (format) {
    case viz::RED_8:
    case viz::ALPHA_8:
    case viz::LUMINANCE_8:
      mtl_pixel_format = MTLPixelFormatR8Unorm;
      break;
    case viz::RG_88:
      mtl_pixel_format = MTLPixelFormatRG8Unorm;
      break;
    case viz::RGBA_8888:
      mtl_pixel_format = MTLPixelFormatRGBA8Unorm;
      break;
    case viz::BGRA_8888:
      mtl_pixel_format = MTLPixelFormatBGRA8Unorm;
      break;
    default:
      // TODO(https://crbug.com/952063): Add support for all formats supported
      // by GLImageIOSurface.
      DLOG(ERROR) << "Resource format not yet supported in Metal.";
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
  [mtl_tex_desc setStorageMode:MTLStorageModePrivate];
  mtl_texture.reset([mtl_device newTextureWithDescriptor:mtl_tex_desc
                                               iosurface:io_surface
                                                   plane:0]);
  DCHECK(mtl_texture);
  return mtl_texture;
}

}  // anonymous namespace

// Representation of a SharedImageBackingIOSurface as a GL Texture.
class SharedImageRepresentationGLTextureIOSurface
    : public SharedImageRepresentationGLTexture {
 public:
  SharedImageRepresentationGLTextureIOSurface(SharedImageManager* manager,
                                              SharedImageBacking* backing,
                                              MemoryTypeTracker* tracker,
                                              gles2::Texture* texture)
      : SharedImageRepresentationGLTexture(manager, backing, tracker),
        texture_(texture) {
    DCHECK(texture_);
  }

  ~SharedImageRepresentationGLTextureIOSurface() override {
    texture_->RemoveLightweightRef(has_context());
  }

  gles2::Texture* GetTexture() override { return texture_; }

  bool BeginAccess(GLenum mode) override { return true; }

  void EndAccess() override { FlushIOSurfaceGLOperations(); }

 private:
  gles2::Texture* texture_;

  DISALLOW_COPY_AND_ASSIGN(SharedImageRepresentationGLTextureIOSurface);
};

// Representation of a SharedImageBackingIOSurface as a Skia Texture.
class SharedImageRepresentationSkiaIOSurface
    : public SharedImageRepresentationSkia {
 public:
  SharedImageRepresentationSkiaIOSurface(
      SharedImageManager* manager,
      SharedImageBacking* backing,
      scoped_refptr<SharedContextState> context_state,
      sk_sp<SkPromiseImageTexture> promise_texture,
      MemoryTypeTracker* tracker,
      gles2::Texture* gles2_texture)
      : SharedImageRepresentationSkia(manager, backing, tracker),
        context_state_(std::move(context_state)),
        promise_texture_(std::move(promise_texture)),
        gles2_texture_(gles2_texture) {
    DCHECK(promise_texture_);
  }

  ~SharedImageRepresentationSkiaIOSurface() override {
    if (gles2_texture_)
      gles2_texture_->RemoveLightweightRef(has_context());
  }

  sk_sp<SkSurface> BeginWriteAccess(
      int final_msaa_count,
      const SkSurfaceProps& surface_props,
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores) override {
    SkColorType sk_color_type = viz::ResourceFormatToClosestSkColorType(
        /*gpu_compositing=*/true, format());

    return SkSurface::MakeFromBackendTextureAsRenderTarget(
        context_state_->gr_context(), promise_texture_->backendTexture(),
        kTopLeft_GrSurfaceOrigin, final_msaa_count, sk_color_type,
        backing()->color_space().ToSkColorSpace(), &surface_props);
  }

  void EndWriteAccess(sk_sp<SkSurface> surface) override {
    if (context_state_->GrContextIsGL())
      FlushIOSurfaceGLOperations();

    if (gles2_texture_ &&
        gles2_texture_->IsLevelCleared(gles2_texture_->target(), 0)) {
      backing()->SetCleared();
    }
  }

  sk_sp<SkPromiseImageTexture> BeginReadAccess(
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores) override {
    return promise_texture_;
  }

  void EndReadAccess() override {
    if (context_state_->GrContextIsGL())
      FlushIOSurfaceGLOperations();
  }

 private:
  scoped_refptr<SharedContextState> context_state_;
  sk_sp<SkPromiseImageTexture> promise_texture_;
  gles2::Texture* const gles2_texture_;
};

// Representation of a SharedImageBackingIOSurface as a Dawn Texture.
#if BUILDFLAG(USE_DAWN)
class SharedImageRepresentationDawnIOSurface
    : public SharedImageRepresentationDawn {
 public:
  SharedImageRepresentationDawnIOSurface(
      SharedImageManager* manager,
      SharedImageBacking* backing,
      MemoryTypeTracker* tracker,
      WGPUDevice device,
      base::ScopedCFTypeRef<IOSurfaceRef> io_surface,
      WGPUTextureFormat wgpu_format)
      : SharedImageRepresentationDawn(manager, backing, tracker),
        io_surface_(std::move(io_surface)),
        device_(device),
        wgpu_format_(wgpu_format),
        dawn_procs_(dawn_native::GetProcs()) {
    DCHECK(device_);
    DCHECK(io_surface_);

    // Keep a reference to the device so that it stays valid (it might become
    // lost in which case operations will be noops).
    dawn_procs_.deviceReference(device_);
  }

  ~SharedImageRepresentationDawnIOSurface() override {
    EndAccess();
    dawn_procs_.deviceRelease(device_);
  }

  WGPUTexture BeginAccess(WGPUTextureUsage usage) final {
    WGPUTextureDescriptor desc;
    desc.nextInChain = nullptr;
    desc.format = wgpu_format_;
    desc.usage = usage;
    desc.dimension = WGPUTextureDimension_2D;
    desc.size = {size().width(), size().height(), 1};
    desc.arrayLayerCount = 1;
    desc.mipLevelCount = 1;
    desc.sampleCount = 1;

    texture_ =
        dawn_native::metal::WrapIOSurface(device_, &desc, io_surface_.get(), 0);

    if (texture_) {
      // Keep a reference to the texture so that it stays valid (its content
      // might be destroyed).
      dawn_procs_.textureReference(texture_);

      // Assume that the user of this representation will write to the texture
      // so set the cleared flag so that other representations don't overwrite
      // the result.
      // TODO(cwallez@chromium.org): This is incorrect and allows reading
      // uninitialized data. When !IsCleared we should tell dawn_native to
      // consider the texture lazy-cleared.
      SetCleared();
    }

    return texture_;
  }

  void EndAccess() final {
    if (!texture_) {
      return;
    }
    // TODO(cwallez@chromium.org): query dawn_native to know if the texture was
    // cleared and set IsCleared appropriately.

    // All further operations on the textures are errors (they would be racy
    // with other backings).
    dawn_procs_.textureDestroy(texture_);

    // macOS has a global GPU command queue so synchronization between APIs and
    // devices is automatic. However on Metal, wgpuQueueSubmit "commits" the
    // Metal command buffers but they aren't "scheduled" in the global queue
    // immediately. (that work seems offloaded to a different thread?)
    // Wait for all the previous submitted commands to be scheduled to have
    // scheduling races between commands using the IOSurface on different APIs.
    // This is a blocking call but should be almost instant.
    TRACE_EVENT0("gpu", "SharedImageRepresentationDawnIOSurface::EndAccess");
    dawn_native::metal::WaitForCommandsToBeScheduled(device_);

    dawn_procs_.textureRelease(texture_);
    texture_ = nullptr;
  }

 private:
  base::ScopedCFTypeRef<IOSurfaceRef> io_surface_;
  WGPUDevice device_;
  WGPUTexture texture_ = nullptr;
  WGPUTextureFormat wgpu_format_;

  // TODO(cwallez@chromium.org): Load procs only once when the factory is
  // created and pass a pointer to them around?
  DawnProcTable dawn_procs_;
};
#endif  // BUILDFLAG(USE_DAWN)

// Implementation of SharedImageBacking by wrapping IOSurfaces. Disable
// unguarded availability warnings because they are incompatible with using a
// scoped_nsprotocol for the id<MTLTexture> and because all access to Metal is
// guarded on the context provider already successfully using Metal.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunguarded-availability"
class SharedImageBackingIOSurface : public SharedImageBacking {
 public:
  SharedImageBackingIOSurface(const Mailbox& mailbox,
                              viz::ResourceFormat format,
                              const gfx::Size& size,
                              const gfx::ColorSpace& color_space,
                              uint32_t usage,
                              base::ScopedCFTypeRef<IOSurfaceRef> io_surface,
                              base::Optional<WGPUTextureFormat> dawn_format,
                              size_t estimated_size)
      : SharedImageBacking(mailbox,
                           format,
                           size,
                           color_space,
                           usage,
                           estimated_size,
                           false /* is_thread_safe */),
        io_surface_(std::move(io_surface)),
        dawn_format_(dawn_format) {
    DCHECK(io_surface_);
  }
  ~SharedImageBackingIOSurface() final { DCHECK(!io_surface_); }

  bool IsCleared() const final { return is_cleared_; }
  void SetCleared() final {
    if (legacy_texture_) {
      legacy_texture_->SetLevelCleared(legacy_texture_->target(), 0, true);
    }

    is_cleared_ = true;
  }

  void Update(std::unique_ptr<gfx::GpuFence> in_fence) final {}

  bool ProduceLegacyMailbox(MailboxManager* mailbox_manager) final {
    DCHECK(io_surface_);

    legacy_texture_ = GenGLTexture();
    if (!legacy_texture_) {
      return false;
    }

    mailbox_manager->ProduceTexture(mailbox(), legacy_texture_);
    return true;
  }
  void Destroy() final {
    TRACE_EVENT0("gpu", "SharedImageBackingFactoryIOSurface::Destroy");
    DCHECK(io_surface_);

    if (legacy_texture_) {
      legacy_texture_->RemoveLightweightRef(have_context());
      legacy_texture_ = nullptr;
    }
    mtl_texture_.reset();
    io_surface_.reset();
  }

 protected:
  std::unique_ptr<SharedImageRepresentationGLTexture> ProduceGLTexture(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker) final {
    gles2::Texture* texture = GenGLTexture();
    if (!texture) {
      return nullptr;
    }

    return std::make_unique<SharedImageRepresentationGLTextureIOSurface>(
        manager, this, tracker, texture);
  }

  std::unique_ptr<SharedImageRepresentationSkia> ProduceSkia(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker,
      scoped_refptr<SharedContextState> context_state) override {
    gles2::Texture* gles2_texture = nullptr;
    GrBackendTexture gr_backend_texture;
    if (context_state->GrContextIsGL()) {
      gles2_texture = GenGLTexture();
      if (!gles2_texture)
        return nullptr;
      GetGrBackendTexture(
          context_state->feature_info(), gles2_texture->target(), size(),
          gles2_texture->service_id(), format(), &gr_backend_texture);
    }
    if (context_state->GrContextIsMetal()) {
      if (!mtl_texture_) {
        id<MTLDevice> mtl_device =
            context_state->metal_context_provider()->GetMTLDevice();
        mtl_texture_ =
            CreateMetalTexture(mtl_device, io_surface_, size(), format());
        DCHECK(mtl_texture_);
      }
      GrMtlTextureInfo info;
      info.fTexture.retain(mtl_texture_.get());
      gr_backend_texture = GrBackendTexture(size().width(), size().height(),
                                            GrMipMapped::kNo, info);
    }
    sk_sp<SkPromiseImageTexture> promise_texture =
        SkPromiseImageTexture::Make(gr_backend_texture);
    return std::make_unique<SharedImageRepresentationSkiaIOSurface>(
        manager, this, std::move(context_state), promise_texture, tracker,
        gles2_texture);
  }

  std::unique_ptr<SharedImageRepresentationDawn> ProduceDawn(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker,
      WGPUDevice device) override {
#if BUILDFLAG(USE_DAWN)
    if (!dawn_format_) {
      LOG(ERROR) << "Format not supported for Dawn";
      return nullptr;
    }

    return std::make_unique<SharedImageRepresentationDawnIOSurface>(
        manager, this, tracker, device, io_surface_, dawn_format_.value());
#else   // BUILDFLAG(USE_DAWN)
    return nullptr;
#endif  // BUILDFLAG(USE_DAWN)
  }

 private:
  gles2::Texture* GenGLTexture() {
    TRACE_EVENT0("gpu", "SharedImageBackingFactoryIOSurface::GenGLTexture");
    GLFormatInfo gl_info = GetGLFormatInfo(format());
    DCHECK(gl_info.supported);

    // Wrap the IOSurface in a GLImageIOSurface
    scoped_refptr<gl::GLImageIOSurface> image(
        gl::GLImageIOSurface::Create(size(), gl_info.internal_format));
    if (!image->Initialize(io_surface_, gfx::GenericSharedMemoryId(),
                           viz::BufferFormat(format()))) {
      LOG(ERROR) << "Failed to create GLImageIOSurface";
      return nullptr;
    }

    gl::GLApi* api = gl::g_current_gl_context;

    // Save the currently bound rectangle texture to reset it once we are done.
    GLint old_texture_binding = 0;
    api->glGetIntegervFn(GL_TEXTURE_BINDING_RECTANGLE, &old_texture_binding);

    // Create a gles2 rectangle texture to bind to the IOSurface.
    GLuint service_id = 0;
    api->glGenTexturesFn(1, &service_id);
    api->glBindTextureFn(GL_TEXTURE_RECTANGLE, service_id);
    api->glTexParameteriFn(GL_TEXTURE_RECTANGLE, GL_TEXTURE_MIN_FILTER,
                           GL_LINEAR);
    api->glTexParameteriFn(GL_TEXTURE_RECTANGLE, GL_TEXTURE_MAG_FILTER,
                           GL_LINEAR);
    api->glTexParameteriFn(GL_TEXTURE_RECTANGLE, GL_TEXTURE_WRAP_S,
                           GL_CLAMP_TO_EDGE);
    api->glTexParameteriFn(GL_TEXTURE_RECTANGLE, GL_TEXTURE_WRAP_T,
                           GL_CLAMP_TO_EDGE);

    // Bind the GLImageIOSurface to our texture
    if (!image->BindTexImage(GL_TEXTURE_RECTANGLE)) {
      LOG(ERROR) << "Failed to bind GLImageIOSurface";
      api->glBindTextureFn(GL_TEXTURE_RECTANGLE, old_texture_binding);
      api->glDeleteTexturesFn(1, &service_id);
      return nullptr;
    }

    // If the backing is already cleared, no need to clear it again.
    gfx::Rect cleared_rect;
    if (is_cleared_) {
      cleared_rect = gfx::Rect(size());
    }

    // Manually create a gles2::Texture wrapping our driver texture.
    gles2::Texture* texture = new gles2::Texture(service_id);
    texture->SetLightweightRef();
    texture->SetTarget(GL_TEXTURE_RECTANGLE, 1);
    texture->sampler_state_.min_filter = GL_LINEAR;
    texture->sampler_state_.mag_filter = GL_LINEAR;
    texture->sampler_state_.wrap_t = GL_CLAMP_TO_EDGE;
    texture->sampler_state_.wrap_s = GL_CLAMP_TO_EDGE;
    texture->SetLevelInfo(GL_TEXTURE_RECTANGLE, 0, gl_info.internal_format,
                          size().width(), size().height(), 1, 0, gl_info.format,
                          gl_info.type, cleared_rect);
    texture->SetLevelImage(GL_TEXTURE_RECTANGLE, 0, image.get(),
                           gles2::Texture::BOUND);
    texture->SetImmutable(true, false);

    DCHECK_EQ(image->GetInternalFormat(), gl_info.internal_format);

    api->glBindTextureFn(GL_TEXTURE_RECTANGLE, old_texture_binding);
    return texture;
  }

  base::ScopedCFTypeRef<IOSurfaceRef> io_surface_;
  base::Optional<WGPUTextureFormat> dawn_format_;
  base::scoped_nsprotocol<id<MTLTexture>> mtl_texture_;
  bool is_cleared_ = false;

  // A texture for the associated legacy mailbox.
  gles2::Texture* legacy_texture_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(SharedImageBackingIOSurface);
};
#pragma clang diagnostic pop

// Implementation of SharedImageBackingFactoryIOSurface that creates
// SharedImageBackings wrapping IOSurfaces.
SharedImageBackingFactoryIOSurface::SharedImageBackingFactoryIOSurface(
    const GpuDriverBugWorkarounds& workarounds,
    const GpuFeatureInfo& gpu_feature_info,
    bool use_gl)
    : use_gl_(use_gl) {
  if (use_gl_) {
    CollectGLFormatInfo(workarounds, gpu_feature_info);
  }
}

void SharedImageBackingFactoryIOSurface::CollectGLFormatInfo(
    const GpuDriverBugWorkarounds& workarounds,
    const GpuFeatureInfo& gpu_feature_info) {
  scoped_refptr<gles2::FeatureInfo> feature_info =
      new gles2::FeatureInfo(workarounds, gpu_feature_info);
  feature_info->Initialize(ContextType::CONTEXT_TYPE_OPENGLES2, false,
                           gles2::DisallowedFeatures());
  const gles2::Validators* validators = feature_info->validators();

  // Precompute for each format if we can use it with GL.
  for (int i = 0; i <= viz::RESOURCE_FORMAT_MAX; ++i) {
    viz::ResourceFormat format = static_cast<viz::ResourceFormat>(i);
    GLFormatInfo gl_info = GetGLFormatInfo(format);

    format_supported_by_gl_[i] =
        gl_info.supported &&
        validators->texture_internal_format.IsValid(gl_info.internal_format) &&
        validators->texture_format.IsValid(gl_info.format) &&
        validators->pixel_type.IsValid(gl_info.type);
  }
}

SharedImageBackingFactoryIOSurface::~SharedImageBackingFactoryIOSurface() =
    default;

std::unique_ptr<SharedImageBacking>
SharedImageBackingFactoryIOSurface::CreateSharedImage(
    const Mailbox& mailbox,
    viz::ResourceFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    uint32_t usage,
    bool is_thread_safe) {
  TRACE_EVENT0("gpu", "SharedImageBackingFactoryIOSurface::CreateSharedImage");
  DCHECK(!is_thread_safe);
  // Check the format is supported and for simplicity always require it to be
  // supported for GL.
  if (use_gl_ && !format_supported_by_gl_[format]) {
    LOG(ERROR) << "viz::ResourceFormat " << format
               << " not supported by IOSurfaces";
    return nullptr;
  }

  // Calculate SharedImage size in bytes.
  size_t estimated_size;
  if (!viz::ResourceSizes::MaybeSizeInBytes(size, format, &estimated_size)) {
    LOG(ERROR) << "Failed to calculate SharedImage size";
    return nullptr;
  }

  base::ScopedCFTypeRef<IOSurfaceRef> io_surface(
      gfx::CreateIOSurface(size, viz::BufferFormat(format), false));
  if (!io_surface) {
    LOG(ERROR) << "Failed to allocate IOSurface.";
    return nullptr;
  }

  gfx::IOSurfaceSetColorSpace(io_surface, color_space);

  return std::make_unique<SharedImageBackingIOSurface>(
      mailbox, format, size, color_space, usage, std::move(io_surface),
      GetWGPUFormat(format), estimated_size);
}

std::unique_ptr<SharedImageBacking>
SharedImageBackingFactoryIOSurface::CreateSharedImage(
    const Mailbox& mailbox,
    viz::ResourceFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    uint32_t usage,
    base::span<const uint8_t> pixel_data) {
  NOTIMPLEMENTED();
  return nullptr;
}

std::unique_ptr<SharedImageBacking>
SharedImageBackingFactoryIOSurface::CreateSharedImage(
    const Mailbox& mailbox,
    int client_id,
    gfx::GpuMemoryBufferHandle handle,
    gfx::BufferFormat format,
    SurfaceHandle surface_handle,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    uint32_t usage) {
  if (handle.type != gfx::GpuMemoryBufferType::IO_SURFACE_BUFFER) {
    NOTIMPLEMENTED();
    return nullptr;
  }

  base::ScopedCFTypeRef<IOSurfaceRef> io_surface(
      IOSurfaceLookupFromMachPort(handle.mach_port.get()));
  if (!io_surface) {
    DLOG(ERROR) << "IOSurfaceLookupFromMachPort failed.";
    return nullptr;
  }

  viz::ResourceFormat resource_format = viz::GetResourceFormat(format);
  size_t estimated_size = 0;
  if (!viz::ResourceSizes::MaybeSizeInBytes(size, resource_format,
                                            &estimated_size)) {
    DLOG(ERROR) << "Failed to calculate SharedImage size";
    return nullptr;
  }

  return std::make_unique<SharedImageBackingIOSurface>(
      mailbox, resource_format, size, color_space, usage, std::move(io_surface),
      GetWGPUFormat(format), estimated_size);
}

bool SharedImageBackingFactoryIOSurface::CanImportGpuMemoryBuffer(
    gfx::GpuMemoryBufferType memory_buffer_type) {
  return false;
}

}  // namespace gpu
