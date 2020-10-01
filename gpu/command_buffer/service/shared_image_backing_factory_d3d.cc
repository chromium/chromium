// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image_backing_factory_d3d.h"

#include <d3d11_1.h>

#include "components/viz/common/resources/resource_format_utils.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/shared_image_backing_d3d.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gl/direct_composition_surface_win.h"
#include "ui/gl/gl_angle_util_win.h"
#include "ui/gl/gl_bindings.h"

namespace gpu {

namespace {

class ScopedRestoreTexture2D {
 public:
  explicit ScopedRestoreTexture2D(gl::GLApi* api) : api_(api) {
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

}  // anonymous namespace

SharedImageBackingFactoryD3D::SharedImageBackingFactoryD3D()
    : d3d11_device_(gl::QueryD3D11DeviceObjectFromANGLE()) {}

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
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
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
    // Keyed mutexes are required for Dawn interop but are not used
    // for XR composition where fences are used instead.
    d3d11_texture.As(&dxgi_keyed_mutex);
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

  scoped_refptr<gles2::TexturePassthrough> texture =
      base::MakeRefCounted<gles2::TexturePassthrough>(service_id, target);
  texture->SetLevelImage(target, 0, image.get());
  GLint texture_memory_size = 0;
  api->glGetTexParameterivFn(target, GL_MEMORY_SIZE_ANGLE,
                             &texture_memory_size);
  texture->SetEstimatedSize(texture_memory_size);

  return std::make_unique<SharedImageBackingD3D>(
      mailbox, format, size, color_space, surface_origin, alpha_type, usage,
      std::move(swap_chain), std::move(texture), std::move(image), buffer_index,
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
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
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

  auto back_buffer_backing = MakeBacking(
      back_buffer_mailbox, format, size, color_space, surface_origin,
      alpha_type, usage, swap_chain, 0 /* buffer_index */,
      nullptr /* d3d11_texture */, base::win::ScopedHandle());
  if (!back_buffer_backing)
    return {nullptr, nullptr};

  auto front_buffer_backing = MakeBacking(
      front_buffer_mailbox, format, size, color_space, surface_origin,
      alpha_type, usage, swap_chain, 1 /* buffer_index */,
      nullptr /* d3d11_texture */, base::win::ScopedHandle());
  if (!front_buffer_backing)
    return {nullptr, nullptr};

  return {std::move(front_buffer_backing), std::move(back_buffer_backing)};
}

std::unique_ptr<SharedImageBacking>
SharedImageBackingFactoryD3D::CreateSharedImage(
    const Mailbox& mailbox,
    viz::ResourceFormat format,
    SurfaceHandle surface_handle,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
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

  return MakeBacking(mailbox, format, size, color_space, surface_origin,
                     alpha_type, usage, nullptr, 0, std::move(d3d11_texture),
                     std::move(scoped_shared_handle));
}

std::unique_ptr<SharedImageBacking>
SharedImageBackingFactoryD3D::CreateSharedImage(
    const Mailbox& mailbox,
    viz::ResourceFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
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
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage) {
  // TODO: Add support for shared memory GMBs.
  DCHECK_EQ(handle.type, gfx::DXGI_SHARED_HANDLE);
  if (!handle.dxgi_handle.IsValid()) {
    DLOG(ERROR) << "Invalid handle type passed to CreateSharedImage";
    return nullptr;
  }

  if (!gpu::IsImageSizeValidForGpuMemoryBufferFormat(size, format)) {
    DLOG(ERROR) << "Invalid image size " << size.ToString() << " for "
                << gfx::BufferFormatToString(format);
    return nullptr;
  }

  Microsoft::WRL::ComPtr<ID3D11Device1> d3d11_device1;
  HRESULT hr = d3d11_device_.As(&d3d11_device1);
  if (FAILED(hr)) {
    DLOG(ERROR) << "Failed to query for ID3D11Device1. Error: "
                << logging::SystemErrorCodeToString(hr);
    return nullptr;
  }

  Microsoft::WRL::ComPtr<ID3D11Texture2D> d3d11_texture;
  hr = d3d11_device1->OpenSharedResource1(handle.dxgi_handle.Get(),
                                          IID_PPV_ARGS(&d3d11_texture));
  if (FAILED(hr)) {
    DLOG(ERROR) << "Unable to open shared resource from DXGI handle. Error: "
                << logging::SystemErrorCodeToString(hr);
    return nullptr;
  }

  D3D11_TEXTURE2D_DESC desc;
  d3d11_texture->GetDesc(&desc);

  // TODO: Add checks for device specific limits.
  if (desc.Width != static_cast<UINT>(size.width()) ||
      desc.Height != static_cast<UINT>(size.height())) {
    DLOG(ERROR)
        << "Size passed to CreateSharedImage must match texture being opened";
    return nullptr;
  }

  return MakeBacking(mailbox, viz::GetResourceFormat(format), size, color_space,
                     surface_origin, alpha_type, usage, /*swap_chain=*/nullptr,
                     /*buffer_index=*/0, std::move(d3d11_texture),
                     std::move(handle.dxgi_handle));
}

// Returns true if the specified GpuMemoryBufferType can be imported using
// this factory.
bool SharedImageBackingFactoryD3D::CanImportGpuMemoryBuffer(
    gfx::GpuMemoryBufferType memory_buffer_type) {
  return (memory_buffer_type == gfx::DXGI_SHARED_HANDLE);
}

}  // namespace gpu
