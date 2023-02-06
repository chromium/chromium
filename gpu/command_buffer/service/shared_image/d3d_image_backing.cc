// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/d3d_image_backing.h"

#include <d3d11_3.h>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "components/viz/common/resources/resource_sizes.h"
#include "gpu/command_buffer/common/shared_image_trace_utils.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/dxgi_shared_handle_manager.h"
#include "gpu/command_buffer/service/shared_image/d3d_image_representation.h"
#include "gpu/command_buffer/service/shared_image/shared_image_format_utils.h"
#include "gpu/command_buffer/service/shared_image/skia_gl_image_representation.h"
#include "third_party/libyuv/include/libyuv/planar_functions.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/scoped_restore_texture.h"

#if BUILDFLAG(USE_DAWN) && BUILDFLAG(DAWN_ENABLE_BACKEND_OPENGLES)
#include "gpu/command_buffer/service/shared_image/dawn_egl_image_representation.h"
#endif

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

viz::SharedImageFormat PlaneFormat(DXGI_FORMAT dxgi_format, size_t plane) {
  DCHECK_LT(plane, NumPlanes(dxgi_format));
  viz::ResourceFormat format;
  switch (dxgi_format) {
    case DXGI_FORMAT_NV12:
      // Y plane is accessed as R8 and UV plane is accessed as RG88 in D3D.
      format = plane == 0 ? viz::RED_8 : viz::RG_88;
      break;
    case DXGI_FORMAT_P010:
      format = plane == 0 ? viz::R16_EXT : viz::RG16_EXT;
      break;
    case DXGI_FORMAT_B8G8R8A8_UNORM:
      format = viz::BGRA_8888;
      break;
    case DXGI_FORMAT_R10G10B10A2_UNORM:
      format = viz::RGBA_1010102;
      break;
    case DXGI_FORMAT_R16G16B16A16_FLOAT:
      format = viz::RGBA_F16;
      break;
    default:
      NOTREACHED();
      format = viz::BGRA_8888;
  }
  return viz::SharedImageFormat::SinglePlane(format);
}

WGPUTextureFormat DXGIToWGPUFormat(DXGI_FORMAT dxgi_format) {
  switch (dxgi_format) {
    case DXGI_FORMAT_R8G8B8A8_UNORM:
      return WGPUTextureFormat_RGBA8Unorm;
    case DXGI_FORMAT_B8G8R8A8_UNORM:
      return WGPUTextureFormat_BGRA8Unorm;
    case DXGI_FORMAT_R8_UNORM:
      return WGPUTextureFormat_R8Unorm;
    case DXGI_FORMAT_R8G8_UNORM:
      return WGPUTextureFormat_RG8Unorm;
    case DXGI_FORMAT_R16G16B16A16_FLOAT:
      return WGPUTextureFormat_RGBA16Float;
    case DXGI_FORMAT_R10G10B10A2_UNORM:
      return WGPUTextureFormat_RGB10A2Unorm;
    case DXGI_FORMAT_NV12:
      return WGPUTextureFormat_R8BG8Biplanar420Unorm;
    default:
      NOTREACHED();
      return WGPUTextureFormat_Undefined;
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

// `row_bytes` is the number of bytes that need to be copied in each row, which
// can be smaller than `source_stride` or `dest_stride`.
void CopyPlane(const uint8_t* source_memory,
               size_t source_stride,
               uint8_t* dest_memory,
               size_t dest_stride,
               size_t row_bytes,
               const gfx::Size& size) {
  libyuv::CopyPlane(source_memory, source_stride, dest_memory, dest_stride,
                    row_bytes, size.height());
}

}  // namespace

// static
scoped_refptr<gles2::TexturePassthrough> D3DImageBacking::CreateGLTexture(
    viz::SharedImageFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    Microsoft::WRL::ComPtr<ID3D11Texture2D> d3d11_texture,
    GLenum texture_target,
    unsigned array_slice,
    unsigned plane_index,
    Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain) {
  gl::GLApi* const api = gl::g_current_gl_context;
  gl::ScopedRestoreTexture scoped_restore(api, texture_target);

  GLuint service_id = 0;
  api->glGenTexturesFn(1, &service_id);
  api->glBindTextureFn(texture_target, service_id);

  // These need to be set for the texture to be considered mipmap complete.
  api->glTexParameteriFn(texture_target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  api->glTexParameteriFn(texture_target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  // These are not strictly required but guard against some checks if NPOT
  // texture support is disabled.
  api->glTexParameteriFn(texture_target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  api->glTexParameteriFn(texture_target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  // The GL internal format can differ from the underlying swap chain or texture
  // format e.g. RGBA or RGB instead of BGRA or RED/RG for NV12 texture planes.
  // See EGL_ANGLE_d3d_texture_client_buffer spec for format restrictions.
  GLFormatDesc gl_format_desc;
  if (format.is_multi_plane()) {
    gl_format_desc =
        ToGLFormatDesc(format, plane_index, /*use_angle_rgbx_format=*/false);
  } else {
    // For legacy multiplanar formats, `format` is already plane format (eg.
    // RED, RG), so we pass plane_index=0.
    gl_format_desc = ToGLFormatDesc(format, /*plane_index=*/0,
                                    /*use_angle_rgbx_format=*/false);
  }
  auto image = base::MakeRefCounted<gl::GLImageD3D>(
      size, gl_format_desc.image_internal_format, d3d11_texture, array_slice,
      plane_index, swap_chain);
  if (!image->Initialize()) {
    LOG(ERROR) << "GLImageD3D::Initialize failed";
    api->glDeleteTexturesFn(1, &service_id);
    return nullptr;
  }
  if (!image->BindTexImage(texture_target)) {
    LOG(ERROR) << "GLImageD3D::BindTexImage failed";
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

#if BUILDFLAG(USE_DAWN)
D3DImageBacking::DawnExternalImageState::DawnExternalImageState() = default;
D3DImageBacking::DawnExternalImageState::~DawnExternalImageState() = default;
D3DImageBacking::DawnExternalImageState::DawnExternalImageState(
    DawnExternalImageState&&) = default;
D3DImageBacking::DawnExternalImageState&
D3DImageBacking::DawnExternalImageState::operator=(DawnExternalImageState&&) =
    default;
#endif

// static
std::unique_ptr<D3DImageBacking> D3DImageBacking::CreateFromSwapChainBuffer(
    const Mailbox& mailbox,
    viz::SharedImageFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage,
    Microsoft::WRL::ComPtr<ID3D11Texture2D> d3d11_texture,
    Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain,
    bool is_back_buffer) {
  DCHECK(format.is_single_plane());
  auto gl_texture =
      CreateGLTexture(format, size, color_space, d3d11_texture, GL_TEXTURE_2D,
                      /*array_slice=*/0u, /*plane_index=*/0u, swap_chain);
  if (!gl_texture) {
    LOG(ERROR) << "Failed to create GL texture";
    return nullptr;
  }
  return base::WrapUnique(new D3DImageBacking(
      mailbox, format, size, color_space, surface_origin, alpha_type, usage,
      std::move(d3d11_texture), {gl_texture},
      /*dxgi_shared_handle_state=*/nullptr, GL_TEXTURE_2D, /*array_slice=*/0u,
      /*plane_index=*/0u, std::move(swap_chain), is_back_buffer));
}

// static
std::unique_ptr<D3DImageBacking> D3DImageBacking::Create(
    const Mailbox& mailbox,
    viz::SharedImageFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage,
    Microsoft::WRL::ComPtr<ID3D11Texture2D> d3d11_texture,
    scoped_refptr<DXGISharedHandleState> dxgi_shared_handle_state,
    GLenum texture_target,
    size_t array_slice,
    size_t plane_index) {
  const bool has_webgpu_usage = !!(usage & SHARED_IMAGE_USAGE_WEBGPU);
  // Keyed mutexes are required for Dawn interop but are not used for XR
  // composition where fences are used instead.
  DCHECK(!has_webgpu_usage || dxgi_shared_handle_state);

  std::vector<scoped_refptr<gles2::TexturePassthrough>> gl_textures;
  // Do not cache a GL texture in the backing if it could be owned by WebGPU
  // since there's no GL context to MakeCurrent in the destructor.
  if (!has_webgpu_usage) {
    for (int plane = 0; plane < format.NumberOfPlanes(); plane++) {
      gfx::Size plane_size = format.GetPlaneSize(plane, size);
      // For legacy multiplanar formats, format() is plane format (eg. RED, RG)
      // which is_single_plane(), but the real plane is in plane_index so we
      // pass that.
      unsigned plane_id = format.is_single_plane() ? plane_index : plane;
      // Creating the GL texture doesn't require exclusive access to the
      // underlying D3D11 texture.
      scoped_refptr<gles2::TexturePassthrough> gl_texture =
          CreateGLTexture(format, plane_size, color_space, d3d11_texture,
                          texture_target, array_slice, plane_id);
      if (!gl_texture) {
        LOG(ERROR) << "Failed to create GL texture";
        return nullptr;
      }
      gl_textures.push_back(std::move(gl_texture));
    }
  }
  auto backing = base::WrapUnique(new D3DImageBacking(
      mailbox, format, size, color_space, surface_origin, alpha_type, usage,
      std::move(d3d11_texture), std::move(gl_textures),
      std::move(dxgi_shared_handle_state), texture_target, array_slice,
      plane_index));
  return backing;
}

std::unique_ptr<D3DImageBacking> D3DImageBacking::CreateFromGLTexture(
    const Mailbox& mailbox,
    viz::ResourceFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage,
    Microsoft::WRL::ComPtr<ID3D11Texture2D> d3d11_texture,
    scoped_refptr<gles2::TexturePassthrough> gl_texture,
    size_t array_slice) {
  return base::WrapUnique(new D3DImageBacking(
      mailbox, viz::SharedImageFormat::SinglePlane(format), size, color_space,
      surface_origin, alpha_type, usage, std::move(d3d11_texture), {gl_texture},
      /*dxgi_shared_handle_state=*/nullptr, gl_texture->target(), array_slice));
}

// static
std::vector<std::unique_ptr<SharedImageBacking>>
D3DImageBacking::CreateFromVideoTexture(
    base::span<const Mailbox> mailboxes,
    DXGI_FORMAT dxgi_format,
    const gfx::Size& size,
    uint32_t usage,
    Microsoft::WRL::ComPtr<ID3D11Texture2D> d3d11_texture,
    unsigned array_slice,
    scoped_refptr<DXGISharedHandleState> dxgi_shared_handle_state) {
  DCHECK(d3d11_texture);
  DCHECK(SupportsVideoFormat(dxgi_format));
  DCHECK_EQ(mailboxes.size(), NumPlanes(dxgi_format));

  // Shared handle and keyed mutex are required for Dawn interop.
  const bool has_webgpu_usage = usage & gpu::SHARED_IMAGE_USAGE_WEBGPU;
  DCHECK(!has_webgpu_usage || dxgi_shared_handle_state);

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

    // TODO(sunnyps): Switch to GL_TEXTURE_2D since it's now supported by ANGLE.
    constexpr GLenum kTextureTarget = GL_TEXTURE_EXTERNAL_OES;

    // Do not cache a GL texture in the backing if it could be owned by WebGPU
    // since there's no GL context to MakeCurrent in the destructor.
    std::vector<scoped_refptr<gles2::TexturePassthrough>> gl_textures;
    if (!has_webgpu_usage) {
      // Creating the GL texture doesn't require exclusive access to the
      // underlying D3D11 texture.
      auto texture = CreateGLTexture(plane_format, plane_size,
                                     kInvalidColorSpace, d3d11_texture,
                                     kTextureTarget, array_slice, plane_index);
      if (!texture) {
        LOG(ERROR) << "Failed to create GL texture";
        return {};
      }
      gl_textures.push_back(std::move(texture));
    }

    shared_images[plane_index] = base::WrapUnique(new D3DImageBacking(
        mailbox, plane_format, plane_size, kInvalidColorSpace,
        kTopLeft_GrSurfaceOrigin, kPremul_SkAlphaType, usage, d3d11_texture,
        std::move(gl_textures), dxgi_shared_handle_state, kTextureTarget,
        array_slice, plane_index));
    if (!shared_images[plane_index])
      return {};
    shared_images[plane_index]->SetCleared();
  }

  return shared_images;
}

D3DImageBacking::D3DImageBacking(
    const Mailbox& mailbox,
    viz::SharedImageFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage,
    Microsoft::WRL::ComPtr<ID3D11Texture2D> d3d11_texture,
    std::vector<scoped_refptr<gles2::TexturePassthrough>> gl_textures,
    scoped_refptr<DXGISharedHandleState> dxgi_shared_handle_state,
    GLenum texture_target,
    size_t array_slice,
    size_t plane_index,
    Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain,
    bool is_back_buffer)
    : ClearTrackingSharedImageBacking(mailbox,
                                      format,
                                      size,
                                      color_space,
                                      surface_origin,
                                      alpha_type,
                                      usage,
                                      format.EstimatedSizeInBytes(size),
                                      false /* is_thread_safe */),
      d3d11_texture_(std::move(d3d11_texture)),
      gl_textures_(std::move(gl_textures)),
      dxgi_shared_handle_state_(std::move(dxgi_shared_handle_state)),
      texture_target_(texture_target),
      array_slice_(array_slice),
      plane_index_(plane_index),
      swap_chain_(std::move(swap_chain)),
      is_back_buffer_(is_back_buffer) {
  const bool has_webgpu_usage = !!(usage & SHARED_IMAGE_USAGE_WEBGPU);
  DCHECK(has_webgpu_usage || !gl_textures_.empty());
  if (d3d11_texture_)
    d3d11_texture_->GetDevice(&d3d11_device_);
}

D3DImageBacking::~D3DImageBacking() {
  if (!have_context()) {
    for (auto& texture : gl_textures_) {
      texture->MarkContextLost();
    }
  }
  gl_textures_.clear();
  dxgi_shared_handle_state_.reset();
  swap_chain_.Reset();
  d3d11_texture_.Reset();
#if BUILDFLAG(USE_DAWN)
  dawn_external_image_cache_.clear();
#endif  // BUILDFLAG(USE_DAWN)
}

ID3D11Texture2D* D3DImageBacking::GetOrCreateStagingTexture() {
  if (!staging_texture_) {
    Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device;
    DCHECK(d3d11_texture_);
    d3d11_texture_->GetDevice(&d3d11_device);

    D3D11_TEXTURE2D_DESC texture_desc;
    d3d11_texture_->GetDesc(&texture_desc);

    D3D11_TEXTURE2D_DESC staging_desc = {};
    staging_desc.Width = texture_desc.Width;
    staging_desc.Height = texture_desc.Height;
    staging_desc.Format = texture_desc.Format;
    staging_desc.MipLevels = 1;
    staging_desc.ArraySize = 1;
    staging_desc.SampleDesc.Count = 1;
    staging_desc.Usage = D3D11_USAGE_STAGING;
    staging_desc.CPUAccessFlags =
        D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;

    HRESULT hr = d3d11_device->CreateTexture2D(&staging_desc, nullptr,
                                               &staging_texture_);
    if (FAILED(hr)) {
      LOG(ERROR) << "Failed to create staging texture. hr=" << std::hex << hr;
      return nullptr;
    }

    constexpr char kStagingTextureLabel[] = "SharedImageD3D_StagingTexture";
    // Add debug label to the long lived texture.
    staging_texture_->SetPrivateData(WKPDID_D3DDebugObjectName,
                                     strlen(kStagingTextureLabel),
                                     kStagingTextureLabel);
  }
  return staging_texture_.Get();
}

SharedImageBackingType D3DImageBacking::GetType() const {
  return SharedImageBackingType::kD3D;
}

void D3DImageBacking::Update(std::unique_ptr<gfx::GpuFence> in_fence) {
  // Do nothing since D3DImageBackings are only ever backed by DXGI GMB handles,
  // which are synonymous with D3D textures, and no explicit update is needed.
}

bool D3DImageBacking::UploadFromMemory(const std::vector<SkPixmap>& pixmaps) {
  DCHECK_EQ(pixmaps.size(), static_cast<size_t>(format().NumberOfPlanes()));

  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device;
  DCHECK(d3d11_texture_);
  d3d11_texture_->GetDevice(&d3d11_device);

  Microsoft::WRL::ComPtr<ID3D11DeviceContext> device_context;
  d3d11_device->GetImmediateContext(&device_context);

  D3D11_TEXTURE2D_DESC texture_desc;
  d3d11_texture_->GetDesc(&texture_desc);

  if (texture_desc.CPUAccessFlags & D3D11_CPU_ACCESS_WRITE) {
    // D3D doesn't support mappable+default YUV textures.
    DCHECK(format().is_single_plane());

    Microsoft::WRL::ComPtr<ID3D11Device3> device3;
    HRESULT hr = d3d11_device.As(&device3);
    if (FAILED(hr)) {
      LOG(ERROR) << "Failed to retrieve ID3D11Device3. hr=" << std::hex << hr;
      return false;
    }
    hr = device_context->Map(d3d11_texture_.Get(), 0, D3D11_MAP_WRITE, 0,
                             nullptr);
    if (FAILED(hr)) {
      LOG(ERROR) << "Failed to map texture for write. hr = " << std::hex << hr;
      return false;
    }

    const uint8_t* source_memory =
        static_cast<const uint8_t*>(pixmaps[0].addr());
    const size_t source_stride = pixmaps[0].rowBytes();
    device3->WriteToSubresource(d3d11_texture_.Get(), 0, nullptr, source_memory,
                                source_stride, 0);
    device_context->Unmap(d3d11_texture_.Get(), 0);
  } else {
    ID3D11Texture2D* staging_texture = GetOrCreateStagingTexture();
    if (!staging_texture) {
      return false;
    }
    D3D11_MAPPED_SUBRESOURCE mapped_resource = {};
    HRESULT hr = device_context->Map(staging_texture, 0, D3D11_MAP_WRITE, 0,
                                     &mapped_resource);
    if (FAILED(hr)) {
      LOG(ERROR) << "Failed to map texture for write. hr=" << std::hex << hr;
      return false;
    }

    // The mapped staging texture pData points to the first plane's data so an
    // offset is needed for subsequent planes.
    size_t dest_offset = 0;

    for (int plane = 0; plane < format().NumberOfPlanes(); ++plane) {
      auto& pixmap = pixmaps[plane];
      const uint8_t* source_memory = static_cast<const uint8_t*>(pixmap.addr());
      const size_t source_stride = pixmap.rowBytes();

      uint8_t* dest_memory =
          static_cast<uint8_t*>(mapped_resource.pData) + dest_offset;
      const size_t dest_stride = mapped_resource.RowPitch;

      gfx::Size plane_size = format().GetPlaneSize(plane, size());
      CopyPlane(source_memory, source_stride, dest_memory, dest_stride,
                pixmap.info().minRowBytes(), plane_size);

      dest_offset += mapped_resource.RowPitch * plane_size.height();
    }

    device_context->Unmap(staging_texture, 0);
    device_context->CopyResource(d3d11_texture_.Get(), staging_texture);
  }
  return true;
}

bool D3DImageBacking::ReadbackToMemory(const std::vector<SkPixmap>& pixmaps) {
  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device;
  DCHECK(d3d11_texture_);
  d3d11_texture_->GetDevice(&d3d11_device);

  Microsoft::WRL::ComPtr<ID3D11DeviceContext> device_context;
  d3d11_device->GetImmediateContext(&device_context);

  D3D11_TEXTURE2D_DESC texture_desc;
  d3d11_texture_->GetDesc(&texture_desc);

  if (texture_desc.CPUAccessFlags & D3D11_CPU_ACCESS_READ) {
    // D3D doesn't support mappable+default YUV textures.
    DCHECK(format().is_single_plane());

    Microsoft::WRL::ComPtr<ID3D11Device3> device3;
    HRESULT hr = d3d11_device.As(&device3);
    if (FAILED(hr)) {
      LOG(ERROR) << "Failed to retrieve ID3D11Device3. hr=" << std::hex << hr;
      return false;
    }
    hr = device_context->Map(d3d11_texture_.Get(), 0, D3D11_MAP_READ, 0,
                             nullptr);
    if (FAILED(hr)) {
      LOG(ERROR) << "Failed to map texture for read. hr=" << std::hex << hr;
      return false;
    }

    uint8_t* dest_memory = static_cast<uint8_t*>(pixmaps[0].writable_addr());
    const size_t dest_stride = pixmaps[0].rowBytes();
    device3->ReadFromSubresource(dest_memory, dest_stride, 0,
                                 d3d11_texture_.Get(), 0, nullptr);
    device_context->Unmap(d3d11_texture_.Get(), 0);
  } else {
    ID3D11Texture2D* staging_texture = GetOrCreateStagingTexture();
    if (!staging_texture) {
      return false;
    }
    device_context->CopyResource(staging_texture, d3d11_texture_.Get());
    D3D11_MAPPED_SUBRESOURCE mapped_resource = {};
    HRESULT hr = device_context->Map(staging_texture, 0, D3D11_MAP_READ, 0,
                                     &mapped_resource);
    if (FAILED(hr)) {
      LOG(ERROR) << "Failed to map texture for read. hr=" << std::hex << hr;
      return false;
    }

    // The mapped staging texture pData points to the first plane's data so an
    // offset is needed for subsequent planes.
    size_t source_offset = 0;

    for (int plane = 0; plane < format().NumberOfPlanes(); ++plane) {
      auto& pixmap = pixmaps[plane];
      uint8_t* dest_memory = static_cast<uint8_t*>(pixmap.writable_addr());
      const size_t dest_stride = pixmap.rowBytes();

      const uint8_t* source_memory =
          static_cast<uint8_t*>(mapped_resource.pData) + source_offset;
      const size_t source_stride = mapped_resource.RowPitch;

      gfx::Size plane_size = format().GetPlaneSize(plane, size());
      CopyPlane(source_memory, source_stride, dest_memory, dest_stride,
                pixmap.info().minRowBytes(), plane_size);

      source_offset += mapped_resource.RowPitch * plane_size.height();
    }

    device_context->Unmap(staging_texture, 0);
  }
  return true;
}

WGPUTextureUsageFlags D3DImageBacking::GetAllowedDawnUsages(
    const WGPUTextureFormat wgpu_format) const {
  // TODO(crbug.com/2709243): Figure out other SI flags, if any.
  DCHECK(usage() & gpu::SHARED_IMAGE_USAGE_WEBGPU);
  const WGPUTextureUsageFlags kBasicUsage =
      WGPUTextureUsage_CopySrc | WGPUTextureUsage_CopyDst |
      WGPUTextureUsage_TextureBinding | WGPUTextureUsage_RenderAttachment;
  switch (wgpu_format) {
    case WGPUTextureFormat_BGRA8Unorm:
    case WGPUTextureFormat_R8Unorm:
    case WGPUTextureFormat_RG8Unorm:
      return kBasicUsage;
    case WGPUTextureFormat_RGBA8Unorm:
    case WGPUTextureFormat_RGBA16Float:
      return kBasicUsage | WGPUTextureUsage_StorageBinding;
    case WGPUTextureFormat_R8BG8Biplanar420Unorm:
      return WGPUTextureUsage_TextureBinding;
    default:
      return WGPUTextureUsage_None;
  }
}

std::unique_ptr<DawnImageRepresentation> D3DImageBacking::ProduceDawn(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker,
    WGPUDevice device,
    WGPUBackendType backend_type,
    std::vector<WGPUTextureFormat> view_formats) {
#if BUILDFLAG(USE_DAWN)
#if BUILDFLAG(DAWN_ENABLE_BACKEND_OPENGLES)
  if (backend_type == WGPUBackendType_OpenGLES) {
    std::unique_ptr<GLTextureImageRepresentationBase> gl_representation =
        ProduceGLTexturePassthrough(manager, tracker);
    gpu::TextureBase* texture = gl_representation->GetTextureBase();
    const auto* image = gl::GLImage::ToGLImageD3D(
        static_cast<gles2::TexturePassthrough*>(texture)->GetLevelImage(
            texture->target(), 0u));
    DCHECK(image);
    return std::make_unique<DawnEGLImageRepresentation>(
        std::move(gl_representation), image->GetEGLImage(), manager, this,
        tracker, device);
  }
#endif
  D3D11_TEXTURE2D_DESC desc;
  d3d11_texture_->GetDesc(&desc);
  const WGPUTextureFormat wgpu_format = DXGIToWGPUFormat(desc.Format);
  if (wgpu_format == WGPUTextureFormat_Undefined) {
    LOG(ERROR) << "Unsupported DXGI_FORMAT found: " << desc.Format;
    return nullptr;
  }

  WGPUTextureUsageFlags allowed_usage = GetAllowedDawnUsages(wgpu_format);
  if (allowed_usage == WGPUTextureUsage_None) {
    LOG(ERROR) << "Allowed WGPUTextureUsage is unknown for WGPUTextureFormat: "
               << wgpu_format;
    return nullptr;
  }

  // We need to have an internal usage of CopySrc in order to use
  // CopyTextureToTextureInternal if texture format allows these usage.
  WGPUTextureUsageFlags internal_usage =
      (WGPUTextureUsage_CopySrc | WGPUTextureUsage_RenderAttachment |
       WGPUTextureUsage_TextureBinding) &
      allowed_usage;

  WGPUTextureDescriptor texture_descriptor = {};
  texture_descriptor.format = wgpu_format;
  texture_descriptor.usage = allowed_usage;
  texture_descriptor.dimension = WGPUTextureDimension_2D;
  texture_descriptor.size = {static_cast<uint32_t>(size().width()),
                             static_cast<uint32_t>(size().height()), 1};
  texture_descriptor.mipLevelCount = 1;
  texture_descriptor.sampleCount = 1;
  texture_descriptor.viewFormatCount =
      static_cast<uint32_t>(view_formats.size());
  texture_descriptor.viewFormats = view_formats.data();

  // We need to have internal usages of CopySrc for copies,
  // RenderAttachment for clears, and TextureBinding for copyTextureForBrowser
  // if texture format allows these usages.
  WGPUDawnTextureInternalUsageDescriptor internalDesc = {};
  internalDesc.chain.sType = WGPUSType_DawnTextureInternalUsageDescriptor;
  internalDesc.internalUsage = internal_usage;
  texture_descriptor.nextInChain =
      reinterpret_cast<WGPUChainedStruct*>(&internalDesc);

  // Persistently open the shared handle by caching it on this backing.
  auto it = dawn_external_image_cache_.find(device);
  if (it == dawn_external_image_cache_.end()) {
    DCHECK(dxgi_shared_handle_state_);
    const HANDLE shared_handle = dxgi_shared_handle_state_->GetSharedHandle();
    DCHECK(base::win::HandleTraits::IsHandleValid(shared_handle));

    D3D11_TEXTURE2D_DESC texture_desc = {};
    d3d11_texture_->GetDesc(&texture_desc);

    ExternalImageDescriptorDXGISharedHandle externalImageDesc;
    externalImageDesc.cTextureDescriptor = &texture_descriptor;
    externalImageDesc.sharedHandle = shared_handle;
    externalImageDesc.useFenceSynchronization =
        !(texture_desc.MiscFlags & D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX);

    DawnExternalImageState state;
    state.external_image =
        ExternalImageDXGI::Create(device, &externalImageDesc);
    if (!state.external_image) {
      LOG(ERROR) << "Failed to create external image";
      return nullptr;
    }
    DCHECK(state.external_image->IsValid());
    dawn_external_image_cache_.emplace(device, std::move(state));
  }

  return std::make_unique<DawnD3DImageRepresentation>(manager, this, tracker,
                                                      device);
#else
  return nullptr;
#endif  // BUILDFLAG(USE_DAWN)
}

std::unique_ptr<VideoDecodeImageRepresentation>
D3DImageBacking::ProduceVideoDecode(SharedImageManager* manager,
                                    MemoryTypeTracker* tracker,
                                    VideoDecodeDevice device) {
  return std::make_unique<D3D11VideoDecodeImageRepresentation>(
      manager, this, tracker, d3d11_texture_);
}

#if BUILDFLAG(USE_DAWN)
WGPUTexture D3DImageBacking::BeginAccessDawn(WGPUDevice device,
                                             WGPUTextureUsage wgpu_usage) {
  const bool write_access =
      wgpu_usage & (WGPUTextureUsage_CopyDst | WGPUTextureUsage_StorageBinding |
                    WGPUTextureUsage_RenderAttachment);

  if (!ValidateBeginAccess(write_access))
    return nullptr;

  // D3D12 access is only allowed with shared handle. Note that BeginAccessD3D12
  // is a no-op if fences are used instead of keyed mutex.
  if (!dxgi_shared_handle_state_ ||
      !dxgi_shared_handle_state_->BeginAccessD3D12()) {
    DLOG(ERROR) << "Missing shared handle state or BeginAccessD3D12 failed";
    return nullptr;
  }

  // Create the D3D11 device fence on first Dawn access.
  if (!dxgi_shared_handle_state_->has_keyed_mutex() && !d3d11_device_fence_) {
    d3d11_device_fence_ = D3DSharedFence::CreateForD3D11(d3d11_device_);
    if (!d3d11_device_fence_) {
      DLOG(ERROR) << "Failed to create D3D11 signal fence";
      return nullptr;
    }
    // Make D3D11 device wait for |write_fence_| since we'll replace it below.
    if (write_fence_ && !write_fence_->WaitD3D11(d3d11_device_)) {
      DLOG(ERROR) << "Failed to wait for write fence";
      return nullptr;
    }
    if (!d3d11_device_fence_->IncrementAndSignalD3D11()) {
      DLOG(ERROR) << "Failed to signal D3D11 signal fence";
      return nullptr;
    }
    // Store it in |write_fence_| so it's waited on for all subsequent access.
    write_fence_ = d3d11_device_fence_;
  }

  auto it = dawn_external_image_cache_.find(device);
  DCHECK(it != dawn_external_image_cache_.end());

  const D3DSharedFence* dawn_signaled_fence = it->second.signaled_fence.get();

  // Defer clearing fences until later to handle Dawn failure to import texture.
  std::vector<ExternalImageDXGIFenceDescriptor> wait_fences;
  // Always wait for previous write for both read-only or read-write access.
  // Skip the wait if it's for the fence last signaled by the device.
  if (write_fence_ && write_fence_.get() != dawn_signaled_fence)
    wait_fences.push_back(ExternalImageDXGIFenceDescriptor{
        write_fence_->GetSharedHandle(), write_fence_->GetFenceValue()});
  // Also wait for all previous reads for read-write access.
  if (write_access) {
    for (const auto& read_fence : read_fences_) {
      // Skip the wait if it's for the fence last signaled by the device.
      if (read_fence != dawn_signaled_fence) {
        wait_fences.push_back(ExternalImageDXGIFenceDescriptor{
            read_fence->GetSharedHandle(), read_fence->GetFenceValue()});
      }
    }
  }

  ExternalImageDXGIBeginAccessDescriptor descriptor;
  descriptor.isInitialized = IsCleared();
  descriptor.isSwapChainTexture =
      (usage() & SHARED_IMAGE_USAGE_WEBGPU_SWAP_CHAIN_TEXTURE);
  descriptor.usage = wgpu_usage;
  descriptor.waitFences = std::move(wait_fences);

  ExternalImageDXGI* external_image = it->second.external_image.get();
  DCHECK(external_image);
  WGPUTexture texture = external_image->BeginAccess(&descriptor);
  if (!texture) {
    DLOG(ERROR) << "Failed to begin access and produce WGPUTexture";
    dxgi_shared_handle_state_->EndAccessD3D12();
    return nullptr;
  }

  // Clear fences and update state iff Dawn BeginAccess succeeds.
  if (write_access) {
    in_write_access_ = true;
    write_fence_.reset();
    read_fences_.clear();
  } else {
    num_readers_++;
  }

  return texture;
}

void D3DImageBacking::EndAccessDawn(WGPUDevice device, WGPUTexture texture) {
  DCHECK(texture);
  if (dawn::native::IsTextureSubresourceInitialized(texture, 0, 1, 0, 1))
    SetCleared();

  // External image is removed from cache on first EndAccess after device is
  // lost. It's ok to skip synchronization because it should've already been
  // synchronized before the entry was removed from the cache.
  auto it = dawn_external_image_cache_.find(device);
  if (it != dawn_external_image_cache_.end()) {
    ExternalImageDXGI* external_image = it->second.external_image.get();
    DCHECK(external_image);

    // EndAccess will only succeed if the external image is still valid.
    if (external_image->IsValid()) {
      ExternalImageDXGIFenceDescriptor descriptor;
      external_image->EndAccess(texture, &descriptor);

      scoped_refptr<D3DSharedFence> signaled_fence;
      if (descriptor.fenceHandle != nullptr) {
        scoped_refptr<D3DSharedFence>& cached_fence = it->second.signaled_fence;
        // Try to reuse the last signaled fence if it's the same fence.
        if (!cached_fence ||
            !cached_fence->IsSameFenceAsHandle(descriptor.fenceHandle)) {
          cached_fence =
              D3DSharedFence::CreateFromHandle(descriptor.fenceHandle);
          DCHECK(cached_fence);
        }
        cached_fence->Update(descriptor.fenceValue);
        signaled_fence = cached_fence;
      }
      // Dawn should be using either keyed mutex or fence synchronization.
      DCHECK((dxgi_shared_handle_state_ &&
              dxgi_shared_handle_state_->has_keyed_mutex()) ||
             signaled_fence);
      EndAccessCommon(std::move(signaled_fence));
    } else {
      // Erase from cache if external image is invalid i.e. device was lost.
      dawn_external_image_cache_.erase(it);
    }
  }

  if (dxgi_shared_handle_state_)
    dxgi_shared_handle_state_->EndAccessD3D12();
}
#endif

bool D3DImageBacking::BeginAccessD3D11(bool write_access) {
  if (!ValidateBeginAccess(write_access))
    return false;

  // If read fences or write fence are present, shared handle should be too.
  DCHECK((read_fences_.empty() && !write_fence_) || dxgi_shared_handle_state_);

  // Always wait for the write fence for both read-write and read-only access.
  // We don't wait for previous read fences for read-only access since there's
  // no dependency between concurrent reads and instead wait for the last write.
  if (write_fence_ && !write_fence_->WaitD3D11(d3d11_device_)) {
    DLOG(ERROR) << "Failed to wait for write fence";
    return false;
  }
  if (write_access) {
    // For read-write access, wait for all previous reads, and reset fences.
    for (const auto& fence : read_fences_) {
      if (!fence->WaitD3D11(d3d11_device_)) {
        DLOG(ERROR) << "Failed to wait for read fence";
        return false;
      }
    }
    write_fence_.reset();
    read_fences_.clear();
    in_write_access_ = true;
  } else {
    num_readers_++;
  }

  if (dxgi_shared_handle_state_)
    return dxgi_shared_handle_state_->BeginAccessD3D11();
  // D3D11 access is allowed without shared handle.
  return true;
}

void D3DImageBacking::EndAccessD3D11() {
  // If D3D11 device signaling fence is present, shared handle should be too.
  DCHECK(!d3d11_device_fence_ || dxgi_shared_handle_state_);

  scoped_refptr<D3DSharedFence> signaled_fence;
  if (d3d11_device_fence_ && d3d11_device_fence_->IncrementAndSignalD3D11())
    signaled_fence = d3d11_device_fence_;

  EndAccessCommon(std::move(signaled_fence));

  if (dxgi_shared_handle_state_)
    dxgi_shared_handle_state_->EndAccessD3D11();
}

bool D3DImageBacking::ValidateBeginAccess(bool write_access) const {
  if (in_write_access_) {
    DLOG(ERROR) << "Already being accessed for write";
    return false;
  }
  if (write_access && num_readers_ > 0) {
    DLOG(ERROR) << "Already being accessed for read";
    return false;
  }
  return true;
}

void D3DImageBacking::EndAccessCommon(
    scoped_refptr<D3DSharedFence> signaled_fence) {
  if (in_write_access_) {
    DCHECK(!write_fence_);
    DCHECK(read_fences_.empty());
    in_write_access_ = false;
    write_fence_ = std::move(signaled_fence);
  } else {
    num_readers_--;
    if (signaled_fence)
      read_fences_.insert(std::move(signaled_fence));
  }
}

gl::GLImage* D3DImageBacking::GetGLImage() const {
  DCHECK(format().is_single_plane());
  return !gl_textures_.empty()
             ? gl_textures_[0]->GetLevelImage(gl_textures_[0]->target(), 0u)
             : nullptr;
}

bool D3DImageBacking::PresentSwapChain() {
  TRACE_EVENT0("gpu", "D3DImageBacking::PresentSwapChain");
  if (!swap_chain_ || !is_back_buffer_) {
    LOG(ERROR) << "Backing does not correspond to back buffer of swap chain";
    return false;
  }

  DXGI_PRESENT_PARAMETERS params = {};
  params.DirtyRectsCount = 0;
  params.pDirtyRects = nullptr;

  UINT flags = DXGI_PRESENT_ALLOW_TEARING;

  HRESULT hr = swap_chain_->Present1(0 /* interval */, flags, &params);
  if (FAILED(hr)) {
    LOG(ERROR) << "Present1 failed with error " << std::hex << hr;
    return false;
  }

  gl::GLApi* const api = gl::g_current_gl_context;
  gl::ScopedRestoreTexture scoped_restore(api, GL_TEXTURE_2D);

  DCHECK(format().is_single_plane());
  DCHECK_EQ(gl_textures_[0]->target(), static_cast<unsigned>(GL_TEXTURE_2D));
  api->glBindTextureFn(GL_TEXTURE_2D, gl_textures_[0]->service_id());
  DCHECK(GetGLImage());
  if (auto* gl_image_d3d = gl::GLImage::ToGLImageD3D(GetGLImage())) {
    if (!gl_image_d3d->BindTexImage(GL_TEXTURE_2D)) {
      LOG(ERROR) << "GLImageD3D::BindTexImage failed";
      return false;
    }
  }

  TRACE_EVENT0("gpu", "D3DImageBacking::PresentSwapChain::Flush");
  // Flush device context through ANGLE otherwise present could be deferred.
  api->glFlushFn();
  return true;
}

std::unique_ptr<GLTexturePassthroughImageRepresentation>
D3DImageBacking::ProduceGLTexturePassthrough(SharedImageManager* manager,
                                             MemoryTypeTracker* tracker) {
  TRACE_EVENT0("gpu", "D3DImageBacking::ProduceGLTexturePassthrough");
  // Lazily create a GL texture if it wasn't provided on initialization.
  auto gl_textures = gl_textures_;
  if (gl_textures.empty()) {
    for (int plane = 0; plane < format().NumberOfPlanes(); plane++) {
      gfx::Size plane_size = format().GetPlaneSize(plane, size());
      // For legacy multiplanar formats, format() is plane format (eg. RED, RG)
      // which is_single_plane(), but the real plane is in plane_index_ so we
      // pass that.
      unsigned plane_id = format().is_single_plane() ? plane_index_ : plane;
      // Creating the GL texture doesn't require exclusive access to the
      // underlying D3D11 texture.
      scoped_refptr<gles2::TexturePassthrough> gl_texture =
          CreateGLTexture(format(), plane_size, color_space(), d3d11_texture_,
                          texture_target_, array_slice_, plane_id, swap_chain_);
      if (!gl_texture) {
        LOG(ERROR) << "Failed to create GL texture";
        return nullptr;
      }
      gl_textures.push_back(std::move(gl_texture));
    }
  }
  return std::make_unique<GLTexturePassthroughD3DImageRepresentation>(
      manager, this, tracker, std::move(gl_textures));
}

std::unique_ptr<SkiaImageRepresentation> D3DImageBacking::ProduceSkia(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker,
    scoped_refptr<SharedContextState> context_state) {
  return SkiaGLImageRepresentation::Create(
      ProduceGLTexturePassthrough(manager, tracker), std::move(context_state),
      manager, this, tracker);
}

std::unique_ptr<OverlayImageRepresentation> D3DImageBacking::ProduceOverlay(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker) {
  TRACE_EVENT0("gpu", "D3DImageBacking::ProduceOverlay");
  return std::make_unique<OverlayD3DImageRepresentation>(manager, this,
                                                         tracker);
}

absl::optional<gl::DCLayerOverlayImage>
D3DImageBacking::GetDCLayerOverlayImage() {
  if (swap_chain_) {
    return absl::make_optional<gl::DCLayerOverlayImage>(size(), swap_chain_);
  }
  // Set only if access isn't synchronized using the shared handle state.
  Microsoft::WRL::ComPtr<IDXGIKeyedMutex> keyed_mutex;
  if (!dxgi_shared_handle_state_) {
    d3d11_texture_.As(&keyed_mutex);
  }
  return absl::make_optional<gl::DCLayerOverlayImage>(
      size(), d3d11_texture_, array_slice_, std::move(keyed_mutex));
}

}  // namespace gpu
