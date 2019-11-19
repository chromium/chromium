// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image_backing_factory_d3d.h"

#include "base/trace_event/memory_dump_manager.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "gpu/command_buffer/common/shared_image_trace_utils.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "gpu/command_buffer/service/shared_image_backing.h"
#include "gpu/command_buffer/service/shared_image_manager.h"
#include "gpu/command_buffer/service/shared_image_representation.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gl/buildflags.h"
#include "ui/gl/direct_composition_surface_win.h"
#include "ui/gl/gl_angle_util_win.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_image_d3d.h"
#include "ui/gl/trace_util.h"

// Usage of BUILDFLAG(USE_DAWN) needs to be after the include for
// ui/gl/buildflags.h
#if BUILDFLAG(USE_DAWN)
#include <dawn_native/D3D12Backend.h>
#endif  // BUILDFLAG(USE_DAWN)

namespace gpu {

namespace {

class ScopedRestoreTexture2D {
 public:
  ScopedRestoreTexture2D(gl::GLApi* api) : api_(api) {
    GLint binding = 0;
    api->glGetIntegervFn(GL_TEXTURE_BINDING_2D, &binding);
    prev_binding_ = binding;
  }

  ~ScopedRestoreTexture2D() {
    api_->glBindTextureFn(GL_TEXTURE_2D, prev_binding_);
  }

 private:
  gl::GLApi* const api_;
  GLuint prev_binding_ = 0;
  DISALLOW_COPY_AND_ASSIGN(ScopedRestoreTexture2D);
};

bool ClearBackBuffer(Microsoft::WRL::ComPtr<IDXGISwapChain1>& swap_chain,
                     Microsoft::WRL::ComPtr<ID3D11Device>& d3d11_device) {
  Microsoft::WRL::ComPtr<ID3D11Texture2D> d3d11_texture;
  HRESULT hr = swap_chain->GetBuffer(0, IID_PPV_ARGS(&d3d11_texture));
  if (FAILED(hr)) {
    DLOG(ERROR) << "GetBuffer failed with error " << std::hex << hr;
    return false;
  }
  DCHECK(d3d11_texture);

  Microsoft::WRL::ComPtr<ID3D11RenderTargetView> render_target;
  hr = d3d11_device->CreateRenderTargetView(d3d11_texture.Get(), nullptr,
                                            &render_target);
  if (FAILED(hr)) {
    DLOG(ERROR) << "CreateRenderTargetView failed with error " << std::hex
                << hr;
    return false;
  }
  DCHECK(render_target);

  Microsoft::WRL::ComPtr<ID3D11DeviceContext> d3d11_device_context;
  d3d11_device->GetImmediateContext(&d3d11_device_context);
  DCHECK(d3d11_device_context);

  float color_rgba[4] = {0.0f, 0.0f, 0.0f, 1.0f};
  d3d11_device_context->ClearRenderTargetView(render_target.Get(), color_rgba);
  return true;
}

base::Optional<DXGI_FORMAT> VizFormatToDXGIFormat(
    viz::ResourceFormat viz_resource_format) {
  switch (viz_resource_format) {
    case viz::RGBA_F16:
      return DXGI_FORMAT_R16G16B16A16_FLOAT;
    case viz::BGRA_8888:
      return DXGI_FORMAT_B8G8R8A8_UNORM;
    case viz::RGBA_8888:
      return DXGI_FORMAT_R8G8B8A8_UNORM;
    default:
      NOTREACHED();
      return {};
  }
}

#if BUILDFLAG(USE_DAWN)
base::Optional<WGPUTextureFormat> VizResourceFormatToWGPUTextureFormat(
    viz::ResourceFormat viz_resource_format) {
  switch (viz_resource_format) {
    case viz::RGBA_F16:
      return WGPUTextureFormat_RGBA16Float;
    case viz::BGRA_8888:
      return WGPUTextureFormat_BGRA8Unorm;
    case viz::RGBA_8888:
      return WGPUTextureFormat_RGBA8Unorm;
    default:
      NOTREACHED();
      return {};
  }
}
#endif  // BUILDFLAG(USE_DAWN)

}  // anonymous namespace

// Representation of a SharedImageBackingD3D as a GL Texture.
class SharedImageRepresentationGLTextureD3D
    : public SharedImageRepresentationGLTexture {
 public:
  SharedImageRepresentationGLTextureD3D(SharedImageManager* manager,
                                        SharedImageBacking* backing,
                                        MemoryTypeTracker* tracker,
                                        gles2::Texture* texture)
      : SharedImageRepresentationGLTexture(manager, backing, tracker),
        texture_(texture) {}

  gles2::Texture* GetTexture() override { return texture_; }

 private:
  gles2::Texture* const texture_;
};

// Representation of a SharedImageBackingD3D as a GL
// TexturePassthrough.
class SharedImageRepresentationGLTexturePassthroughD3D
    : public SharedImageRepresentationGLTexturePassthrough {
 public:
  SharedImageRepresentationGLTexturePassthroughD3D(
      SharedImageManager* manager,
      SharedImageBacking* backing,
      MemoryTypeTracker* tracker,
      scoped_refptr<gles2::TexturePassthrough> texture_passthrough)
      : SharedImageRepresentationGLTexturePassthrough(manager,
                                                      backing,
                                                      tracker),
        texture_passthrough_(std::move(texture_passthrough)) {}

  const scoped_refptr<gles2::TexturePassthrough>& GetTexturePassthrough()
      override {
    return texture_passthrough_;
  }

 private:
  bool BeginAccess(GLenum mode) override;
  void EndAccess() override;

  scoped_refptr<gles2::TexturePassthrough> texture_passthrough_;
};

// Representation of a SharedImageBackingD3D as a Dawn Texture
#if BUILDFLAG(USE_DAWN)
class SharedImageRepresentationDawnD3D : public SharedImageRepresentationDawn {
 public:
  SharedImageRepresentationDawnD3D(SharedImageManager* manager,
                                   SharedImageBacking* backing,
                                   MemoryTypeTracker* tracker,
                                   WGPUDevice device)
      : SharedImageRepresentationDawn(manager, backing, tracker),
        device_(device),
        dawn_procs_(dawn_native::GetProcs()) {
    DCHECK(device_);

    // Keep a reference to the device so that it stays valid (it might become
    // lost in which case operations will be noops).
    dawn_procs_.deviceReference(device_);
  }

  ~SharedImageRepresentationDawnD3D() override {
    EndAccess();
    dawn_procs_.deviceRelease(device_);
  }

  WGPUTexture BeginAccess(WGPUTextureUsage usage) override;
  void EndAccess() override;

 private:
  WGPUDevice device_;
  WGPUTexture texture_ = nullptr;

  // TODO(cwallez@chromium.org): Load procs only once when the factory is
  // created and pass a pointer to them around?
  DawnProcTable dawn_procs_;
};
#endif  // BUILDFLAG(USE_DAWN)

// Implementation of SharedImageBacking that holds buffer (front buffer/back
// buffer of swap chain) texture (as gles2::Texture/gles2::TexturePassthrough)
// and a reference to created swap chain.
class SharedImageBackingD3D : public SharedImageBacking {
 public:
  SharedImageBackingD3D(
      const Mailbox& mailbox,
      viz::ResourceFormat format,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      uint32_t usage,
      Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain,
      gles2::Texture* texture,
      scoped_refptr<gles2::TexturePassthrough> texture_passthrough,
      scoped_refptr<gl::GLImageD3D> image,
      size_t buffer_index,
      Microsoft::WRL::ComPtr<ID3D11Texture2D> d3d11_texture,
      base::win::ScopedHandle shared_handle,
      Microsoft::WRL::ComPtr<IDXGIKeyedMutex> dxgi_keyed_mutex)
      : SharedImageBacking(mailbox,
                           format,
                           size,
                           color_space,
                           usage,
                           texture ? texture->estimated_size()
                                   : texture_passthrough->estimated_size(),
                           false /* is_thread_safe */),
        swap_chain_(std::move(swap_chain)),
        texture_(texture),
        texture_passthrough_(std::move(texture_passthrough)),
        image_(std::move(image)),
        buffer_index_(buffer_index),
        d3d11_texture_(std::move(d3d11_texture)),
        shared_handle_(std::move(shared_handle)),
        dxgi_keyed_mutex_(std::move(dxgi_keyed_mutex)) {
    DCHECK(d3d11_texture_);
    DCHECK((texture_ && !texture_passthrough_) ||
           (!texture_ && texture_passthrough_));
  }

  ~SharedImageBackingD3D() override {
    // Destroy() is safe to call even if it's already been called.
    Destroy();
  }

  // Texture is cleared on initialization.
  bool IsCleared() const override { return true; }

  void SetCleared() override {}

  void Update(std::unique_ptr<gfx::GpuFence> in_fence) override {
    DLOG(ERROR) << "SharedImageBackingD3D::Update : Trying to update "
                   "Shared Images associated with swap chain.";
  }

  bool ProduceLegacyMailbox(MailboxManager* mailbox_manager) override {
    if (texture_) {
      mailbox_manager->ProduceTexture(mailbox(), texture_);
    } else {
      mailbox_manager->ProduceTexture(mailbox(), texture_passthrough_.get());
    }
    return true;
  }

  std::unique_ptr<SharedImageRepresentationDawn> ProduceDawn(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker,
      WGPUDevice device) override {
#if BUILDFLAG(USE_DAWN)
    return std::make_unique<SharedImageRepresentationDawnD3D>(manager, this,
                                                              tracker, device);
#else
    return nullptr;
#endif  // BUILDFLAG(USE_DAWN)
  }

  void Destroy() override {
    if (texture_) {
      texture_->RemoveLightweightRef(have_context());
      texture_ = nullptr;
    } else if (texture_passthrough_) {
      if (!have_context())
        texture_passthrough_->MarkContextLost();
      texture_passthrough_ = nullptr;
    }
    swap_chain_ = nullptr;
    d3d11_texture_.Reset();
    dxgi_keyed_mutex_.Reset();
    keyed_mutex_acquire_key_ = 0;
    keyed_mutex_acquired_ = false;
    shared_handle_.Close();
  }

  void OnMemoryDump(const std::string& dump_name,
                    base::trace_event::MemoryAllocatorDump* dump,
                    base::trace_event::ProcessMemoryDump* pmd,
                    uint64_t client_tracing_id) override {
    // Add a |service_guid| which expresses shared ownership between the
    // various GPU dumps.
    auto client_guid = GetSharedImageGUIDForTracing(mailbox());
    GLuint service_id =
        texture_ ? texture_->service_id() : texture_passthrough_->service_id();
    base::trace_event::MemoryAllocatorDumpGuid service_guid =
        gl::GetGLTextureServiceGUIDForTracing(service_id);
    pmd->CreateSharedGlobalAllocatorDump(service_guid);

    int importance = 2;  // This client always owns the ref.
    pmd->AddOwnershipEdge(client_guid, service_guid, importance);

    // Swap chain textures only have one level backed by an image.
    image_->OnMemoryDump(pmd, client_tracing_id, dump_name);
  }

  bool BeginAccessD3D12(uint64_t* acquire_key) {
    if (keyed_mutex_acquired_) {
      DLOG(ERROR) << "Recursive BeginAccess not supported";
      return false;
    }
    *acquire_key = keyed_mutex_acquire_key_;
    keyed_mutex_acquire_key_++;
    keyed_mutex_acquired_ = true;
    return true;
  }

  void EndAccessD3D12() { keyed_mutex_acquired_ = false; }

  bool BeginAccessD3D11() {
    if (dxgi_keyed_mutex_) {
      if (keyed_mutex_acquired_) {
        DLOG(ERROR) << "Recursive BeginAccess not supported";
        return false;
      }
      const HRESULT hr =
          dxgi_keyed_mutex_->AcquireSync(keyed_mutex_acquire_key_, INFINITE);
      if (FAILED(hr)) {
        DLOG(ERROR) << "Unable to acquire the keyed mutex " << std::hex << hr;
        return false;
      }
      keyed_mutex_acquire_key_++;
      keyed_mutex_acquired_ = true;
    }
    return true;
  }
  void EndAccessD3D11() {
    if (dxgi_keyed_mutex_) {
      const HRESULT hr =
          dxgi_keyed_mutex_->ReleaseSync(keyed_mutex_acquire_key_);
      if (FAILED(hr)) {
        DLOG(ERROR) << "Unable to release the keyed mutex " << std::hex << hr;
        return;
      }
      keyed_mutex_acquired_ = false;
    }
  }

  HANDLE GetSharedHandle() const { return shared_handle_.Get(); }

  bool PresentSwapChain() override {
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
    ScopedRestoreTexture2D scoped_restore(api);

    const GLenum target = GL_TEXTURE_2D;
    const GLuint service_id =
        texture_ ? texture_->service_id() : texture_passthrough_->service_id();
    api->glBindTextureFn(target, service_id);

    if (!image_->BindTexImage(target)) {
      DLOG(ERROR) << "GLImageD3D::BindTexImage failed";
      return false;
    }

    TRACE_EVENT0("gpu", "SharedImageBackingD3D::PresentSwapChain::Flush");
    // Flush device context through ANGLE otherwise present could be deferred.
    api->glFlushFn();
    return true;
  }

 protected:
  std::unique_ptr<SharedImageRepresentationGLTexture> ProduceGLTexture(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker) override {
    DCHECK(texture_);
    TRACE_EVENT0("gpu", "SharedImageBackingD3D::ProduceGLTexture");
    return std::make_unique<SharedImageRepresentationGLTextureD3D>(
        manager, this, tracker, texture_);
  }

  std::unique_ptr<SharedImageRepresentationGLTexturePassthrough>
  ProduceGLTexturePassthrough(SharedImageManager* manager,
                              MemoryTypeTracker* tracker) override {
    DCHECK(texture_passthrough_);
    TRACE_EVENT0("gpu", "SharedImageBackingD3D::ProduceGLTexturePassthrough");
    return std::make_unique<SharedImageRepresentationGLTexturePassthroughD3D>(
        manager, this, tracker, texture_passthrough_);
  }

 private:
  Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain_;
  gles2::Texture* texture_ = nullptr;
  scoped_refptr<gles2::TexturePassthrough> texture_passthrough_;
  scoped_refptr<gl::GLImageD3D> image_;
  const size_t buffer_index_;
  Microsoft::WRL::ComPtr<ID3D11Texture2D> d3d11_texture_;

  // If d3d11_texture_ has a keyed mutex, it will be stored in
  // dxgi_keyed_mutex. The keyed mutex is used to synchronize
  // D3D11 and D3D12 Chromium components.
  // dxgi_keyed_mutex_ is the D3D11 side of the keyed mutex.
  // To create the corresponding D3D12 interface, pass the handle
  // stored in shared_handle_ to ID3D12Device::OpenSharedHandle.
  // Only one component is allowed to read/write to the texture
  // at a time. keyed_mutex_acquire_key_ is incremented on every
  // Acquire/Release usage.
  base::win::ScopedHandle shared_handle_;
  Microsoft::WRL::ComPtr<IDXGIKeyedMutex> dxgi_keyed_mutex_;
  uint64_t keyed_mutex_acquire_key_ = 0;
  bool keyed_mutex_acquired_ = false;

  DISALLOW_COPY_AND_ASSIGN(SharedImageBackingD3D);
};

#if BUILDFLAG(USE_DAWN)
WGPUTexture SharedImageRepresentationDawnD3D::BeginAccess(
    WGPUTextureUsage usage) {
  SharedImageBackingD3D* d3d_image_backing =
      static_cast<SharedImageBackingD3D*>(backing());

  const HANDLE shared_handle = d3d_image_backing->GetSharedHandle();
  const viz::ResourceFormat viz_resource_format = d3d_image_backing->format();
  const base::Optional<WGPUTextureFormat> wgpu_texture_format =
      VizResourceFormatToWGPUTextureFormat(viz_resource_format);
  if (!wgpu_texture_format.has_value()) {
    DLOG(ERROR) << "Unsupported viz format found: " << viz_resource_format;
    return nullptr;
  }

  uint64_t shared_mutex_acquire_key;
  if (!d3d_image_backing->BeginAccessD3D12(&shared_mutex_acquire_key)) {
    return nullptr;
  }

  WGPUTextureDescriptor desc;
  desc.nextInChain = nullptr;
  desc.format = wgpu_texture_format.value();
  desc.usage = usage;
  desc.dimension = WGPUTextureDimension_2D;
  desc.size = {size().width(), size().height(), 1};
  desc.arrayLayerCount = 1;
  desc.mipLevelCount = 1;
  desc.sampleCount = 1;

  texture_ = dawn_native::d3d12::WrapSharedHandle(device_, &desc, shared_handle,
                                                  shared_mutex_acquire_key);
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
  } else {
    d3d_image_backing->EndAccessD3D12();
  }

  return texture_;
}

void SharedImageRepresentationDawnD3D::EndAccess() {
  if (!texture_) {
    return;
  }

  SharedImageBackingD3D* d3d_image_backing =
      static_cast<SharedImageBackingD3D*>(backing());

  // TODO(cwallez@chromium.org): query dawn_native to know if the texture was
  // cleared and set IsCleared appropriately.

  // All further operations on the textures are errors (they would be racy
  // with other backings).
  dawn_procs_.textureDestroy(texture_);

  dawn_procs_.textureRelease(texture_);
  texture_ = nullptr;

  d3d_image_backing->EndAccessD3D12();
}
#endif  // BUILDFLAG(USE_DAWN)

bool SharedImageRepresentationGLTexturePassthroughD3D::BeginAccess(
    GLenum mode) {
  SharedImageBackingD3D* d3d_image_backing =
      static_cast<SharedImageBackingD3D*>(backing());
  return d3d_image_backing->BeginAccessD3D11();
}

void SharedImageRepresentationGLTexturePassthroughD3D::EndAccess() {
  SharedImageBackingD3D* d3d_image_backing =
      static_cast<SharedImageBackingD3D*>(backing());
  d3d_image_backing->EndAccessD3D11();
}

SharedImageBackingFactoryD3D::SharedImageBackingFactoryD3D(bool use_passthrough)
    : use_passthrough_(use_passthrough),
      d3d11_device_(gl::QueryD3D11DeviceObjectFromANGLE()) {
}

SharedImageBackingFactoryD3D::~SharedImageBackingFactoryD3D() = default;

SharedImageBackingFactoryD3D::SwapChainBackings::SwapChainBackings(
    std::unique_ptr<SharedImageBacking> front_buffer,
    std::unique_ptr<SharedImageBacking> back_buffer)
    : front_buffer(std::move(front_buffer)),
      back_buffer(std::move(back_buffer)) {}

SharedImageBackingFactoryD3D::SwapChainBackings::~SwapChainBackings() = default;

SharedImageBackingFactoryD3D::SwapChainBackings::SwapChainBackings(
    SharedImageBackingFactoryD3D::SwapChainBackings&&) = default;

SharedImageBackingFactoryD3D::SwapChainBackings&
SharedImageBackingFactoryD3D::SwapChainBackings::operator=(
    SharedImageBackingFactoryD3D::SwapChainBackings&&) = default;

// static
bool SharedImageBackingFactoryD3D::IsSwapChainSupported() {
  return gl::DirectCompositionSurfaceWin::IsDirectCompositionSupported() &&
         gl::DirectCompositionSurfaceWin::IsSwapChainTearingSupported();
}

std::unique_ptr<SharedImageBacking> SharedImageBackingFactoryD3D::MakeBacking(
    const Mailbox& mailbox,
    viz::ResourceFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    uint32_t usage,
    Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain,
    size_t buffer_index,
    Microsoft::WRL::ComPtr<ID3D11Texture2D> d3d11_texture,
    base::win::ScopedHandle shared_handle) {
  gl::GLApi* const api = gl::g_current_gl_context;
  ScopedRestoreTexture2D scoped_restore(api);

  const GLenum target = GL_TEXTURE_2D;
  GLuint service_id = 0;
  api->glGenTexturesFn(1, &service_id);
  api->glBindTextureFn(target, service_id);
  api->glTexParameteriFn(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  api->glTexParameteriFn(target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  api->glTexParameteriFn(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  api->glTexParameteriFn(target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  Microsoft::WRL::ComPtr<IDXGIKeyedMutex> dxgi_keyed_mutex;

  if (swap_chain) {
    DCHECK(!d3d11_texture);
    DCHECK(!shared_handle.IsValid());
    const HRESULT hr =
        swap_chain->GetBuffer(buffer_index, IID_PPV_ARGS(&d3d11_texture));
    if (FAILED(hr)) {
      DLOG(ERROR) << "GetBuffer failed with error " << std::hex;
      return nullptr;
    }
  } else if (shared_handle.IsValid()) {
    const HRESULT hr = d3d11_texture.As(&dxgi_keyed_mutex);
    if (FAILED(hr)) {
      DLOG(ERROR) << "Unable to QueryInterface for IDXGIKeyedMutex on texture "
                     "with shared handle "
                  << std::hex;
      return nullptr;
    }
  }
  DCHECK(d3d11_texture);

  // The GL internal format can differ from the underlying swap chain format
  // e.g. RGBA8 or RGB8 instead of BGRA8.
  const GLenum internal_format = viz::GLInternalFormat(format);
  const GLenum data_type = viz::GLDataType(format);
  const GLenum data_format = viz::GLDataFormat(format);
  auto image = base::MakeRefCounted<gl::GLImageD3D>(
      size, internal_format, data_type, d3d11_texture, swap_chain);
  DCHECK_EQ(image->GetDataFormat(), data_format);
  if (!image->Initialize()) {
    DLOG(ERROR) << "GLImageD3D::Initialize failed";
    return nullptr;
  }
  if (!image->BindTexImage(target)) {
    DLOG(ERROR) << "GLImageD3D::BindTexImage failed";
    return nullptr;
  }

  gles2::Texture* texture = nullptr;
  scoped_refptr<gles2::TexturePassthrough> texture_passthrough;

  if (use_passthrough_) {
    texture_passthrough =
        base::MakeRefCounted<gles2::TexturePassthrough>(service_id, target);
    texture_passthrough->SetLevelImage(target, 0, image.get());
    GLint texture_memory_size = 0;
    api->glGetTexParameterivFn(target, GL_MEMORY_SIZE_ANGLE,
                               &texture_memory_size);
    texture_passthrough->SetEstimatedSize(texture_memory_size);
  } else {
    texture = new gles2::Texture(service_id);
    texture->SetLightweightRef();
    texture->SetTarget(target, 1);
    texture->sampler_state_.min_filter = GL_LINEAR;
    texture->sampler_state_.mag_filter = GL_LINEAR;
    texture->sampler_state_.wrap_s = GL_CLAMP_TO_EDGE;
    texture->sampler_state_.wrap_t = GL_CLAMP_TO_EDGE;
    texture->SetLevelInfo(target, 0 /* level */, internal_format, size.width(),
                          size.height(), 1 /* depth */, 0 /* border */,
                          data_format, data_type, gfx::Rect(size));
    texture->SetLevelImage(target, 0 /* level */, image.get(),
                           gles2::Texture::BOUND);
    texture->SetImmutable(true, false);
  }

  return std::make_unique<SharedImageBackingD3D>(
      mailbox, format, size, color_space, usage, std::move(swap_chain), texture,
      std::move(texture_passthrough), std::move(image), buffer_index,
      std::move(d3d11_texture), std::move(shared_handle),
      std::move(dxgi_keyed_mutex));
}

SharedImageBackingFactoryD3D::SwapChainBackings
SharedImageBackingFactoryD3D::CreateSwapChain(
    const Mailbox& front_buffer_mailbox,
    const Mailbox& back_buffer_mailbox,
    viz::ResourceFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    uint32_t usage) {
  if (!SharedImageBackingFactoryD3D::IsSwapChainSupported())
    return {nullptr, nullptr};

  DXGI_FORMAT swap_chain_format;
  switch (format) {
    case viz::RGBA_8888:
    case viz::RGBX_8888:
    case viz::BGRA_8888:
      swap_chain_format = DXGI_FORMAT_B8G8R8A8_UNORM;
      break;
    case viz::RGBA_F16:
      swap_chain_format = DXGI_FORMAT_R16G16B16A16_FLOAT;
      break;
    default:
      DLOG(ERROR) << gfx::BufferFormatToString(viz::BufferFormat(format))
                  << " format is not supported by swap chain.";
      return {nullptr, nullptr};
  }

  Microsoft::WRL::ComPtr<IDXGIDevice> dxgi_device;
  d3d11_device_.As(&dxgi_device);
  DCHECK(dxgi_device);
  Microsoft::WRL::ComPtr<IDXGIAdapter> dxgi_adapter;
  dxgi_device->GetAdapter(&dxgi_adapter);
  DCHECK(dxgi_adapter);
  Microsoft::WRL::ComPtr<IDXGIFactory2> dxgi_factory;
  dxgi_adapter->GetParent(IID_PPV_ARGS(&dxgi_factory));
  DCHECK(dxgi_factory);

  DXGI_SWAP_CHAIN_DESC1 desc = {};
  desc.Width = size.width();
  desc.Height = size.height();
  desc.Format = swap_chain_format;
  desc.Stereo = FALSE;
  desc.SampleDesc.Count = 1;
  desc.BufferCount = 2;
  desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT | DXGI_USAGE_SHADER_INPUT;
  desc.Scaling = DXGI_SCALING_STRETCH;
  desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
  desc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
  desc.AlphaMode = viz::HasAlpha(format) ? DXGI_ALPHA_MODE_PREMULTIPLIED
                                         : DXGI_ALPHA_MODE_IGNORE;

  Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain;

  HRESULT hr = dxgi_factory->CreateSwapChainForComposition(
      d3d11_device_.Get(), &desc, nullptr, &swap_chain);

  if (FAILED(hr)) {
    DLOG(ERROR) << "CreateSwapChainForComposition failed with error "
                << std::hex << hr;
    return {nullptr, nullptr};
  }

  // Explicitly clear front and back buffers to ensure that there are no
  // uninitialized pixels.
  if (!ClearBackBuffer(swap_chain, d3d11_device_))
    return {nullptr, nullptr};

  DXGI_PRESENT_PARAMETERS params = {};
  params.DirtyRectsCount = 0;
  params.pDirtyRects = nullptr;
  hr = swap_chain->Present1(0 /* interval */, 0 /* flags */, &params);
  if (FAILED(hr)) {
    DLOG(ERROR) << "Present1 failed with error " << std::hex << hr;
    return {nullptr, nullptr};
  }

  if (!ClearBackBuffer(swap_chain, d3d11_device_))
    return {nullptr, nullptr};

  auto back_buffer_backing =
      MakeBacking(back_buffer_mailbox, format, size, color_space, usage,
                  swap_chain, 0 /* buffer_index */, nullptr /* d3d11_texture */,
                  base::win::ScopedHandle());
  if (!back_buffer_backing)
    return {nullptr, nullptr};

  auto front_buffer_backing =
      MakeBacking(front_buffer_mailbox, format, size, color_space, usage,
                  swap_chain, 1 /* buffer_index */, nullptr /* d3d11_texture */,
                  base::win::ScopedHandle());
  if (!front_buffer_backing)
    return {nullptr, nullptr};

  return {std::move(front_buffer_backing), std::move(back_buffer_backing)};
}

std::unique_ptr<SharedImageBacking>
SharedImageBackingFactoryD3D::CreateSharedImage(
    const Mailbox& mailbox,
    viz::ResourceFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    uint32_t usage,
    bool is_thread_safe) {
  DCHECK(!is_thread_safe);

  // Without D3D11, we cannot do shared images. This will happen if we're
  // running with Vulkan, D3D9, GL or with the non-passthrough command decoder
  // in tests.
  if (!d3d11_device_) {
    return nullptr;
  }

  const base::Optional<DXGI_FORMAT> dxgi_format = VizFormatToDXGIFormat(format);
  if (!dxgi_format.has_value()) {
    DLOG(ERROR) << "Unsupported viz format found: " << format;
    return nullptr;
  }

  D3D11_TEXTURE2D_DESC desc;
  desc.Width = size.width();
  desc.Height = size.height();
  desc.MipLevels = 1;
  desc.ArraySize = 1;
  desc.Format = dxgi_format.value();
  desc.SampleDesc.Count = 1;
  desc.SampleDesc.Quality = 0;
  desc.Usage = D3D11_USAGE_DEFAULT;
  desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
  desc.CPUAccessFlags = 0;
  desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED_NTHANDLE |
                   D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX;
  Microsoft::WRL::ComPtr<ID3D11Texture2D> d3d11_texture;
  HRESULT hr = d3d11_device_->CreateTexture2D(&desc, nullptr, &d3d11_texture);
  if (FAILED(hr)) {
    DLOG(ERROR) << "CreateTexture2D failed with error " << std::hex << hr;
    return nullptr;
  }

  Microsoft::WRL::ComPtr<IDXGIResource1> dxgi_resource;
  hr = d3d11_texture.As(&dxgi_resource);
  if (FAILED(hr)) {
    DLOG(ERROR) << "QueryInterface for IDXGIResource failed with error "
                << std::hex << hr;
    return nullptr;
  }

  HANDLE shared_handle;
  hr = dxgi_resource->CreateSharedHandle(
      nullptr, DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE, nullptr,
      &shared_handle);
  if (FAILED(hr)) {
    DLOG(ERROR) << "Unable to create shared handle for DXGIResource "
                << std::hex << hr;
    return nullptr;
  }

  // Put the shared handle into an RAII object as quickly as possible to
  // ensure we do not leak it.
  base::win::ScopedHandle scoped_shared_handle(shared_handle);

  return MakeBacking(mailbox, format, size, color_space, usage, nullptr, 0,
                     std::move(d3d11_texture), std::move(scoped_shared_handle));
}

std::unique_ptr<SharedImageBacking>
SharedImageBackingFactoryD3D::CreateSharedImage(
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
SharedImageBackingFactoryD3D::CreateSharedImage(
    const Mailbox& mailbox,
    int client_id,
    gfx::GpuMemoryBufferHandle handle,
    gfx::BufferFormat format,
    SurfaceHandle surface_handle,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    uint32_t usage) {
  NOTIMPLEMENTED();
  return nullptr;
}

// Returns true if the specified GpuMemoryBufferType can be imported using
// this factory.
bool SharedImageBackingFactoryD3D::CanImportGpuMemoryBuffer(
    gfx::GpuMemoryBufferType memory_buffer_type) {
  return false;
}

}  // namespace gpu
