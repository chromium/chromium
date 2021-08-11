// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image_backing_d3d.h"

#include "base/memory/ptr_util.h"
#include "base/trace_event/memory_dump_manager.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "components/viz/common/resources/resource_sizes.h"
#include "gpu/command_buffer/common/constants.h"
#include "gpu/command_buffer/common/shared_image_trace_utils.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/shared_image_representation_d3d.h"
#if BUILDFLAG(USE_DAWN) && BUILDFLAG(DAWN_ENABLE_BACKEND_OPENGLES)
#include "gpu/command_buffer/service/shared_image_representation_dawn_egl_image.h"
#endif
#include "gpu/command_buffer/service/shared_image_representation_skia_gl.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gl/trace_util.h"

namespace gpu {

namespace {

bool SupportsVideoFormat(DXGI_FORMAT dxgi_format) {
  switch (dxgi_format) {
    case DXGI_FORMAT_NV12:
    case DXGI_FORMAT_P010:
    case DXGI_FORMAT_B8G8R8A8_UNORM:
    case DXGI_FORMAT_R10G10B10A2_UNORM:
    case DXGI_FORMAT_R16G16B16A16_FLOAT:
      return true;
    default:
      return false;
  }
}

size_t NumPlanes(DXGI_FORMAT dxgi_format) {
  switch (dxgi_format) {
    case DXGI_FORMAT_NV12:
    case DXGI_FORMAT_P010:
      return 2;
    case DXGI_FORMAT_B8G8R8A8_UNORM:
    case DXGI_FORMAT_R10G10B10A2_UNORM:
    case DXGI_FORMAT_R16G16B16A16_FLOAT:
      return 1;
    default:
      NOTREACHED();
      return 0;
  }
}

viz::ResourceFormat PlaneFormat(DXGI_FORMAT dxgi_format, size_t plane) {
  DCHECK_LT(plane, NumPlanes(dxgi_format));
  switch (dxgi_format) {
    // TODO(crbug.com/1011555): P010 formats are not fully supported by Skia.
    // Treat them the same as NV12 for the time being.
    case DXGI_FORMAT_NV12:
    case DXGI_FORMAT_P010:
      // Y plane is accessed as R8 and UV plane is accessed as RG88 in D3D.
      return plane == 0 ? viz::RED_8 : viz::RG_88;
    case DXGI_FORMAT_B8G8R8A8_UNORM:
      return viz::BGRA_8888;
    case DXGI_FORMAT_R10G10B10A2_UNORM:
      return viz::RGBA_1010102;
    case DXGI_FORMAT_R16G16B16A16_FLOAT:
      return viz::RGBA_F16;
    default:
      NOTREACHED();
      return viz::BGRA_8888;
  }
}

gfx::Size PlaneSize(DXGI_FORMAT dxgi_format,
                    const gfx::Size& size,
                    size_t plane) {
  DCHECK_LT(plane, NumPlanes(dxgi_format));
  switch (dxgi_format) {
    case DXGI_FORMAT_NV12:
    case DXGI_FORMAT_P010:
      // Y plane is full size and UV plane is accessed as half size in D3D.
      return plane == 0 ? size : gfx::Size(size.width() / 2, size.height() / 2);
    case DXGI_FORMAT_B8G8R8A8_UNORM:
    case DXGI_FORMAT_R10G10B10A2_UNORM:
    case DXGI_FORMAT_R16G16B16A16_FLOAT:
      return size;
    default:
      NOTREACHED();
      return gfx::Size();
  }
}

class ScopedRestoreTexture {
 public:
  ScopedRestoreTexture(gl::GLApi* api, GLenum target)
      : api_(api), target_(target) {
    DCHECK(target == GL_TEXTURE_2D || target == GL_TEXTURE_EXTERNAL_OES);
    GLint binding = 0;
    api->glGetIntegervFn(target == GL_TEXTURE_2D
                             ? GL_TEXTURE_BINDING_2D
                             : GL_TEXTURE_BINDING_EXTERNAL_OES,
                         &binding);
    prev_binding_ = binding;
  }

  ~ScopedRestoreTexture() { api_->glBindTextureFn(target_, prev_binding_); }

 private:
  gl::GLApi* const api_;
  const GLenum target_;
  GLuint prev_binding_ = 0;
  DISALLOW_COPY_AND_ASSIGN(ScopedRestoreTexture);
};

scoped_refptr<gles2::TexturePassthrough> CreateGLTexture(
    viz::ResourceFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    Microsoft::WRL::ComPtr<ID3D11Texture2D> d3d11_texture,
    Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain = nullptr,
    GLenum texture_target = GL_TEXTURE_2D,
    unsigned array_slice = 0u,
    unsigned plane_index = 0u) {
  gl::GLApi* const api = gl::g_current_gl_context;
  ScopedRestoreTexture scoped_restore(api, texture_target);

  GLuint service_id = 0;
  api->glGenTexturesFn(1, &service_id);
  api->glBindTextureFn(texture_target, service_id);

  // The GL internal format can differ from the underlying swap chain or texture
  // format e.g. RGBA or RGB instead of BGRA or RED/RG for NV12 texture planes.
  // See EGL_ANGLE_d3d_texture_client_buffer spec for format restrictions.
  const auto internal_format = viz::GLInternalFormat(format);
  const auto data_type = viz::GLDataType(format);
  auto image = base::MakeRefCounted<gl::GLImageD3D>(
      size, internal_format, data_type, color_space, d3d11_texture, array_slice,
      plane_index, swap_chain);
  DCHECK_EQ(image->GetDataFormat(), viz::GLDataFormat(format));
  if (!image->Initialize()) {
    DLOG(ERROR) << "GLImageD3D::Initialize failed";
    api->glDeleteTexturesFn(1, &service_id);
    return nullptr;
  }
  if (!image->BindTexImage(texture_target)) {
    DLOG(ERROR) << "GLImageD3D::BindTexImage failed";
    api->glDeleteTexturesFn(1, &service_id);
    return nullptr;
  }

  auto texture = base::MakeRefCounted<gles2::TexturePassthrough>(
      service_id, texture_target);
  texture->SetLevelImage(texture_target, 0, image.get());
  GLint texture_memory_size = 0;
  api->glGetTexParameterivFn(texture_target, GL_MEMORY_SIZE_ANGLE,
                             &texture_memory_size);
  texture->SetEstimatedSize(texture_memory_size);

  return texture;
}

}  // anonymous namespace

SharedImageBackingD3D::SharedState::SharedState(
    base::win::ScopedHandle shared_handle,
    Microsoft::WRL::ComPtr<IDXGIKeyedMutex> dxgi_keyed_mutex)
    : shared_handle_(std::move(shared_handle)),
      dxgi_keyed_mutex_(std::move(dxgi_keyed_mutex)) {}

SharedImageBackingD3D::SharedState::~SharedState() {
  DCHECK(!acquired_for_d3d12_);
  DCHECK_EQ(acquired_for_d3d11_count_, 0);
  shared_handle_.Close();
}

bool SharedImageBackingD3D::SharedState::BeginAccessD3D12() {
  if (!dxgi_keyed_mutex_) {
    DLOG(ERROR) << "D3D12 access not supported without keyed mutex";
    return false;
  }
  if (acquired_for_d3d12_ || acquired_for_d3d11_count_ > 0) {
    DLOG(ERROR) << "Recursive BeginAccess not supported";
    return false;
  }
  acquired_for_d3d12_ = true;
  return true;
}

void SharedImageBackingD3D::SharedState::EndAccessD3D12() {
  acquired_for_d3d12_ = false;
}

bool SharedImageBackingD3D::SharedState::BeginAccessD3D11() {
  // Nop for shared images that are created without keyed mutex (D3D11 only).
  if (!dxgi_keyed_mutex_)
    return true;

  if (acquired_for_d3d12_) {
    DLOG(ERROR) << "Recursive BeginAccess not supported";
    return false;
  }
  if (acquired_for_d3d11_count_ > 0) {
    acquired_for_d3d11_count_++;
    return true;
  }
  const HRESULT hr =
      dxgi_keyed_mutex_->AcquireSync(kDXGIKeyedMutexAcquireKey, INFINITE);
  if (FAILED(hr)) {
    DLOG(ERROR) << "Unable to acquire the keyed mutex " << std::hex << hr;
    return false;
  }
  acquired_for_d3d11_count_++;
  return true;
}

void SharedImageBackingD3D::SharedState::EndAccessD3D11() {
  // Nop for shared images that are created without keyed mutex (D3D11 only).
  if (!dxgi_keyed_mutex_)
    return;

  DCHECK_GT(acquired_for_d3d11_count_, 0);
  acquired_for_d3d11_count_--;
  if (acquired_for_d3d11_count_ == 0) {
    const HRESULT hr =
        dxgi_keyed_mutex_->ReleaseSync(kDXGIKeyedMutexAcquireKey);
    if (FAILED(hr))
      DLOG(ERROR) << "Unable to release the keyed mutex " << std::hex << hr;
  }
}

HANDLE SharedImageBackingD3D::SharedState::GetSharedHandle() const {
  return shared_handle_.Get();
}

// static
std::unique_ptr<SharedImageBackingD3D>
SharedImageBackingD3D::CreateFromSwapChainBuffer(
    const Mailbox& mailbox,
    viz::ResourceFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage,
    Microsoft::WRL::ComPtr<ID3D11Texture2D> d3d11_texture,
    Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain,
    size_t buffer_index) {
  auto gl_texture =
      CreateGLTexture(format, size, color_space, d3d11_texture, swap_chain);
  if (!gl_texture) {
    DLOG(ERROR) << "Failed to create GL texture";
    return nullptr;
  }
  return base::WrapUnique(new SharedImageBackingD3D(
      mailbox, format, size, color_space, surface_origin, alpha_type, usage,
      std::move(d3d11_texture), std::move(gl_texture), std::move(swap_chain),
      buffer_index));
}

// static
std::unique_ptr<SharedImageBackingD3D>
SharedImageBackingD3D::CreateFromSharedHandle(
    const Mailbox& mailbox,
    viz::ResourceFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage,
    Microsoft::WRL::ComPtr<ID3D11Texture2D> d3d11_texture,
    base::win::ScopedHandle shared_handle) {
  DCHECK(shared_handle.IsValid());

  const bool has_webgpu_usage = !!(usage & SHARED_IMAGE_USAGE_WEBGPU);
  // Keyed mutexes are required for Dawn interop but are not used for XR
  // composition where fences are used instead.
  Microsoft::WRL::ComPtr<IDXGIKeyedMutex> dxgi_keyed_mutex;
  d3d11_texture.As(&dxgi_keyed_mutex);
  DCHECK(!has_webgpu_usage || dxgi_keyed_mutex);

  auto shared_state = base::MakeRefCounted<SharedState>(
      std::move(shared_handle), std::move(dxgi_keyed_mutex));

  // Do not cache a GL texture in the backing if it could be owned by WebGPU
  // since there's no GL context to MakeCurrent in the destructor.
  scoped_refptr<gles2::TexturePassthrough> gl_texture;
  if (!has_webgpu_usage) {
    // Creating the GL texture doesn't require exclusive access to the
    // underlying D3D11 texture.
    gl_texture = CreateGLTexture(format, size, color_space, d3d11_texture);
    if (!gl_texture) {
      DLOG(ERROR) << "Failed to create GL texture";
      return nullptr;
    }
  }
  return base::WrapUnique(new SharedImageBackingD3D(
      mailbox, format, size, color_space, surface_origin, alpha_type, usage,
      std::move(d3d11_texture), std::move(gl_texture), /*swap_chain=*/nullptr,
      /*buffer_index=*/0, std::move(shared_state)));
}

std::unique_ptr<SharedImageBackingD3D>
SharedImageBackingD3D::CreateFromGLTexture(
    const Mailbox& mailbox,
    viz::ResourceFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage,
    Microsoft::WRL::ComPtr<ID3D11Texture2D> d3d11_texture,
    scoped_refptr<gles2::TexturePassthrough> gl_texture) {
  return base::WrapUnique(new SharedImageBackingD3D(
      mailbox, format, size, color_space, surface_origin, alpha_type, usage,
      std::move(d3d11_texture), std::move(gl_texture)));
}

// static
std::vector<std::unique_ptr<SharedImageBacking>>
SharedImageBackingD3D::CreateFromVideoTexture(
    base::span<const Mailbox> mailboxes,
    DXGI_FORMAT dxgi_format,
    const gfx::Size& size,
    uint32_t usage,
    Microsoft::WRL::ComPtr<ID3D11Texture2D> d3d11_texture,
    unsigned array_slice,
    base::win::ScopedHandle shared_handle) {
  DCHECK(d3d11_texture);
  DCHECK(SupportsVideoFormat(dxgi_format));
  DCHECK_EQ(mailboxes.size(), NumPlanes(dxgi_format));

  // Shared handle and keyed mutex are required for Dawn interop.
  Microsoft::WRL::ComPtr<IDXGIKeyedMutex> dxgi_keyed_mutex;
  d3d11_texture.As(&dxgi_keyed_mutex);
  DCHECK(!(usage & gpu::SHARED_IMAGE_USAGE_WEBGPU) ||
         (shared_handle.IsValid() && dxgi_keyed_mutex));

  // Share the same keyed mutex state for all the plane backings.
  auto shared_state = base::MakeRefCounted<SharedState>(
      std::move(shared_handle), std::move(dxgi_keyed_mutex));

  std::vector<std::unique_ptr<SharedImageBacking>> shared_images(
      NumPlanes(dxgi_format));
  for (size_t plane_index = 0; plane_index < shared_images.size();
       plane_index++) {
    const auto& mailbox = mailboxes[plane_index];

    const auto plane_format = PlaneFormat(dxgi_format, plane_index);
    const auto plane_size = PlaneSize(dxgi_format, size, plane_index);

    // Shared image does not need to store the colorspace since it is already
    // stored on the VideoFrame which is provided upon presenting the overlay.
    // To prevent the developer from mistakenly using it, provide the invalid
    // value from default-construction.
    constexpr gfx::ColorSpace kInvalidColorSpace;

    auto gl_texture = CreateGLTexture(
        plane_format, plane_size, kInvalidColorSpace, d3d11_texture,
        /*swap_chain=*/nullptr, GL_TEXTURE_EXTERNAL_OES, array_slice,
        plane_index);
    if (!gl_texture) {
      DLOG(ERROR) << "Failed to create GL texture";
      return {};
    }

    shared_images[plane_index] = base::WrapUnique(new SharedImageBackingD3D(
        mailbox, plane_format, plane_size, kInvalidColorSpace,
        kTopLeft_GrSurfaceOrigin, kPremul_SkAlphaType, usage, d3d11_texture,
        std::move(gl_texture), /*swap_chain=*/nullptr, /*buffer_index=*/0,
        shared_state));
    shared_images[plane_index]->SetCleared();
  }

  return shared_images;
}

SharedImageBackingD3D::SharedImageBackingD3D(
    const Mailbox& mailbox,
    viz::ResourceFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage,
    Microsoft::WRL::ComPtr<ID3D11Texture2D> d3d11_texture,
    scoped_refptr<gles2::TexturePassthrough> gl_texture,
    Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain,
    size_t buffer_index,
    scoped_refptr<SharedState> shared_state)
    : ClearTrackingSharedImageBacking(
          mailbox,
          format,
          size,
          color_space,
          surface_origin,
          alpha_type,
          usage,
          gl_texture
              ? gl_texture->estimated_size()
              : gfx::BufferSizeForBufferFormat(size, viz::BufferFormat(format)),
          false /* is_thread_safe */),
      d3d11_texture_(std::move(d3d11_texture)),
      gl_texture_(std::move(gl_texture)),
      swap_chain_(std::move(swap_chain)),
      buffer_index_(buffer_index),
      shared_state_(std::move(shared_state)) {
  const bool has_webgpu_usage = !!(usage & SHARED_IMAGE_USAGE_WEBGPU);
  DCHECK(has_webgpu_usage || gl_texture_);
}

SharedImageBackingD3D::~SharedImageBackingD3D() {
  if (!have_context())
    gl_texture_->MarkContextLost();
  gl_texture_ = nullptr;
  shared_state_ = nullptr;
  swap_chain_.Reset();
  d3d11_texture_.Reset();

#if BUILDFLAG(USE_DAWN)
  external_image_ = nullptr;
#endif  // BUILDFLAG(USE_DAWN)
}

void SharedImageBackingD3D::Update(std::unique_ptr<gfx::GpuFence> in_fence) {
  DLOG(ERROR) << "SharedImageBackingD3D::Update : Trying to update "
                 "Shared Images associated with swap chain.";
}

bool SharedImageBackingD3D::ProduceLegacyMailbox(
    MailboxManager* mailbox_manager) {
  mailbox_manager->ProduceTexture(mailbox(), gl_texture_.get());
  return true;
}

uint32_t SharedImageBackingD3D::GetAllowedDawnUsages() const {
  // TODO(crbug.com/2709243): Figure out other SI flags, if any.
  DCHECK(usage() & gpu::SHARED_IMAGE_USAGE_WEBGPU);
  return static_cast<uint32_t>(
      WGPUTextureUsage_CopySrc | WGPUTextureUsage_CopyDst |
      WGPUTextureUsage_TextureBinding | WGPUTextureUsage_RenderAttachment);
}

std::unique_ptr<SharedImageRepresentationDawn>
SharedImageBackingD3D::ProduceDawn(SharedImageManager* manager,
                                   MemoryTypeTracker* tracker,
                                   WGPUDevice device,
                                   WGPUBackendType backend_type) {
#if BUILDFLAG(USE_DAWN)
  const viz::ResourceFormat viz_resource_format = format();
  const WGPUTextureFormat wgpu_format = viz::ToWGPUFormat(viz_resource_format);
  if (wgpu_format == WGPUTextureFormat_Undefined) {
    DLOG(ERROR) << "Unsupported viz format found: " << viz_resource_format;
    return nullptr;
  }

  WGPUTextureDescriptor texture_descriptor = {};
  texture_descriptor.nextInChain = nullptr;
  texture_descriptor.format = wgpu_format;
  texture_descriptor.usage = GetAllowedDawnUsages();
  texture_descriptor.dimension = WGPUTextureDimension_2D;
  texture_descriptor.size = {static_cast<uint32_t>(size().width()),
                             static_cast<uint32_t>(size().height()), 1};
  texture_descriptor.mipLevelCount = 1;
  texture_descriptor.sampleCount = 1;

#if BUILDFLAG(DAWN_ENABLE_BACKEND_OPENGLES)
  if (backend_type == WGPUBackendType_OpenGLES) {
    // EGLImage textures do not support sampling, at the moment.
    texture_descriptor.usage &= ~WGPUTextureUsage_TextureBinding;
    EGLImage egl_image =
        static_cast<gl::GLImageD3D*>(GetGLImage())->egl_image();
    if (!egl_image) {
      DLOG(ERROR) << "Failed to create EGLImage";
      return nullptr;
    }
    return std::make_unique<SharedImageRepresentationDawnEGLImage>(
        manager, this, tracker, device, egl_image, texture_descriptor);
  }
#endif

  // Persistently open the shared handle by caching it on this backing.
  if (!external_image_) {
    DCHECK(base::win::HandleTraits::IsHandleValid(GetSharedHandle()));

    dawn_native::d3d12::ExternalImageDescriptorDXGISharedHandle
        externalImageDesc;
    externalImageDesc.cTextureDescriptor = &texture_descriptor;
    externalImageDesc.sharedHandle = GetSharedHandle();

    external_image_ = dawn_native::d3d12::ExternalImageDXGI::Create(
        device, &externalImageDesc);

    if (!external_image_) {
      DLOG(ERROR) << "Failed to create external image";
      return nullptr;
    }
  }

  return std::make_unique<SharedImageRepresentationDawnD3D>(
      manager, this, tracker, device, external_image_.get());
#else
  return nullptr;
#endif  // BUILDFLAG(USE_DAWN)
}

void SharedImageBackingD3D::OnMemoryDump(
    const std::string& dump_name,
    base::trace_event::MemoryAllocatorDump* dump,
    base::trace_event::ProcessMemoryDump* pmd,
    uint64_t client_tracing_id) {
  // Add a |service_guid| which expresses shared ownership between the
  // various GPU dumps.
  auto client_guid = GetSharedImageGUIDForTracing(mailbox());
  base::trace_event::MemoryAllocatorDumpGuid service_guid =
      gl::GetGLTextureServiceGUIDForTracing(gl_texture_->service_id());
  pmd->CreateSharedGlobalAllocatorDump(service_guid);

  int importance = 2;  // This client always owns the ref.
  pmd->AddOwnershipEdge(client_guid, service_guid, importance);

  // Swap chain textures only have one level backed by an image.
  GetGLImage()->OnMemoryDump(pmd, client_tracing_id, dump_name);
}

bool SharedImageBackingD3D::BeginAccessD3D12() {
  return shared_state_->BeginAccessD3D12();
}

void SharedImageBackingD3D::EndAccessD3D12() {
  shared_state_->EndAccessD3D12();
}

bool SharedImageBackingD3D::BeginAccessD3D11() {
  return shared_state_->BeginAccessD3D11();
}

void SharedImageBackingD3D::EndAccessD3D11() {
  shared_state_->EndAccessD3D11();
}

HANDLE SharedImageBackingD3D::GetSharedHandle() const {
  return shared_state_->GetSharedHandle();
}

gl::GLImage* SharedImageBackingD3D::GetGLImage() const {
  return gl_texture_->GetLevelImage(gl_texture_->target(), 0u);
}

bool SharedImageBackingD3D::PresentSwapChain() {
  TRACE_EVENT0("gpu", "SharedImageBackingD3D::PresentSwapChain");
  if (buffer_index_ != 0) {
    DLOG(ERROR) << "Swap chain backing does not correspond to back buffer";
    return false;
  }

  DXGI_PRESENT_PARAMETERS params = {};
  params.DirtyRectsCount = 0;
  params.pDirtyRects = nullptr;

  UINT flags = DXGI_PRESENT_ALLOW_TEARING;

  HRESULT hr = swap_chain_->Present1(0 /* interval */, flags, &params);
  if (FAILED(hr)) {
    DLOG(ERROR) << "Present1 failed with error " << std::hex << hr;
    return false;
  }

  gl::GLApi* const api = gl::g_current_gl_context;

  DCHECK_EQ(gl_texture_->target(), static_cast<unsigned>(GL_TEXTURE_2D));
  ScopedRestoreTexture scoped_restore(api, GL_TEXTURE_2D);

  api->glBindTextureFn(GL_TEXTURE_2D, gl_texture_->service_id());
  if (!GetGLImage()->BindTexImage(GL_TEXTURE_2D)) {
    DLOG(ERROR) << "GLImage::BindTexImage failed";
    return false;
  }

  TRACE_EVENT0("gpu", "SharedImageBackingD3D::PresentSwapChain::Flush");
  // Flush device context through ANGLE otherwise present could be deferred.
  api->glFlushFn();
  return true;
}

std::unique_ptr<SharedImageRepresentationGLTexturePassthrough>
SharedImageBackingD3D::ProduceGLTexturePassthrough(SharedImageManager* manager,
                                                   MemoryTypeTracker* tracker) {
  TRACE_EVENT0("gpu", "SharedImageBackingD3D::ProduceGLTexturePassthrough");
  // Lazily create a GL texture if it wasn't provided on initialization.
  auto gl_texture = gl_texture_;
  if (!gl_texture) {
    gl_texture =
        CreateGLTexture(format(), size(), color_space(), d3d11_texture_);
    if (!gl_texture) {
      DLOG(ERROR) << "Failed to create GL texture";
      return nullptr;
    }
  }
  return std::make_unique<SharedImageRepresentationGLTexturePassthroughD3D>(
      manager, this, tracker, std::move(gl_texture));
}

std::unique_ptr<SharedImageRepresentationSkia>
SharedImageBackingD3D::ProduceSkia(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker,
    scoped_refptr<SharedContextState> context_state) {
  return SharedImageRepresentationSkiaGL::Create(
      ProduceGLTexturePassthrough(manager, tracker), std::move(context_state),
      manager, this, tracker);
}

std::unique_ptr<SharedImageRepresentationOverlay>
SharedImageBackingD3D::ProduceOverlay(SharedImageManager* manager,
                                      MemoryTypeTracker* tracker) {
  TRACE_EVENT0("gpu", "SharedImageBackingD3D::ProduceOverlay");
  return std::make_unique<SharedImageRepresentationOverlayD3D>(manager, this,
                                                               tracker);
}

}  // namespace gpu
