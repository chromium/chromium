// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "gpu/command_buffer/service/shared_image/d3d_image_backing.h"

#include <d3d11_3.h>

#include <functional>

// clang-format off
#include <dawn/native/D3D11Backend.h>
#include <dawn/native/D3D12Backend.h>
#include <dawn/native/D3DBackend.h>
// clang-format on

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/strcat.h"
#include "base/synchronization/waitable_event.h"
#include "gpu/command_buffer/common/shared_image_trace_utils.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/dxgi_shared_handle_manager.h"
#include "gpu/command_buffer/service/shared_image/copy_image_plane.h"
#include "gpu/command_buffer/service/shared_image/d3d_image_representation.h"
#include "gpu/command_buffer/service/shared_image/d3d_image_utils.h"
#include "gpu/command_buffer/service/shared_image/shared_image_format_service_utils.h"
#include "gpu/command_buffer/service/shared_image/skia_gl_image_representation.h"
#include "gpu/command_buffer/service/shared_image/skia_graphite_dawn_image_representation.h"
#include "gpu/config/gpu_finch_features.h"
#include "ui/gl/egl_util.h"
#include "ui/gl/gl_angle_util_win.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_surface_egl.h"
#include "ui/gl/scoped_restore_texture.h"

#if BUILDFLAG(SKIA_USE_DAWN)
#include "gpu/command_buffer/service/dawn_context_provider.h"
#endif

#if BUILDFLAG(DAWN_ENABLE_BACKEND_OPENGLES)
#include "gpu/command_buffer/service/shared_image/dawn_egl_image_representation.h"
#endif

#ifndef EGL_ANGLE_image_d3d11_texture
#define EGL_D3D11_TEXTURE_ANGLE 0x3484
#define EGL_TEXTURE_INTERNAL_FORMAT_ANGLE 0x345D
#define EGL_D3D11_TEXTURE_PLANE_ANGLE 0x3492
#define EGL_D3D11_TEXTURE_ARRAY_SLICE_ANGLE 0x3493
#endif /* EGL_ANGLE_image_d3d11_texture */

namespace gpu {

namespace {

bool BindEGLImageToTexture(GLenum texture_target, void* egl_image) {
  if (!egl_image) {
    LOG(ERROR) << "EGL image is null";
    return false;
  }
  glEGLImageTargetTexture2DOES(texture_target, egl_image);
  if (eglGetError() != static_cast<EGLint>(EGL_SUCCESS)) {
    LOG(ERROR) << "Failed to bind EGL image to the texture"
               << ui::GetLastEGLErrorString();
    return false;
  }
  return true;
}

bool CanUseUpdateSubresource(const std::vector<SkPixmap>& pixmaps) {
  if (pixmaps.size() == 1u) {
    return true;
  }

  const uint8_t* addr = static_cast<const uint8_t*>(pixmaps[0].addr());
  size_t plane_offset = pixmaps[0].computeByteSize();
  for (size_t i = 1; i < pixmaps.size(); ++i) {
    // UpdateSubresource() cannot update planes individually, so the planes'
    // data has to be packed in one memory block.
    if (static_cast<const uint8_t*>(pixmaps[i].addr()) != addr + plane_offset) {
      return false;
    }
    plane_offset += pixmaps[i].computeByteSize();
  }

  return true;
}

}  // namespace

D3DImageBacking::GLTextureHolder::GLTextureHolder(
    base::PassKey<D3DImageBacking>,
    scoped_refptr<gles2::TexturePassthrough> texture_passthrough,
    gl::ScopedEGLImage egl_image)
    : texture_passthrough_(std::move(texture_passthrough)),
      egl_image_(std::move(egl_image)) {}

bool D3DImageBacking::GLTextureHolder::BindEGLImageToTexture() {
  if (!needs_rebind_) {
    return true;
  }

  gl::GLApi* const api = gl::g_current_gl_context;
  gl::ScopedRestoreTexture scoped_restore(api, GL_TEXTURE_2D);

  DCHECK_EQ(texture_passthrough_->target(),
            static_cast<unsigned>(GL_TEXTURE_2D));
  api->glBindTextureFn(GL_TEXTURE_2D, texture_passthrough_->service_id());

  if (!::gpu::BindEGLImageToTexture(GL_TEXTURE_2D, egl_image_.get())) {
    return false;
  }

  needs_rebind_ = false;
  return true;
}

void D3DImageBacking::GLTextureHolder::MarkContextLost() {
  if (texture_passthrough_) {
    texture_passthrough_->MarkContextLost();
  }
}

D3DImageBacking::GLTextureHolder::~GLTextureHolder() = default;

// static
scoped_refptr<D3DImageBacking::GLTextureHolder>
D3DImageBacking::CreateGLTexture(
    const GLFormatDesc& gl_format_desc,
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

  const EGLint egl_attrib_list[] = {
      EGL_TEXTURE_INTERNAL_FORMAT_ANGLE,
      static_cast<EGLint>(gl_format_desc.image_internal_format),
      EGL_D3D11_TEXTURE_ARRAY_SLICE_ANGLE,
      static_cast<EGLint>(array_slice),
      EGL_D3D11_TEXTURE_PLANE_ANGLE,
      static_cast<EGLint>(plane_index),
      EGL_NONE};

  auto egl_image = gl::MakeScopedEGLImage(
      EGL_NO_CONTEXT, EGL_D3D11_TEXTURE_ANGLE,
      static_cast<EGLClientBuffer>(d3d11_texture.Get()), egl_attrib_list);

  if (!egl_image.get()) {
    LOG(ERROR) << "Failed to create an EGL image";
    api->glDeleteTexturesFn(1, &service_id);
    return nullptr;
  }

  if (!BindEGLImageToTexture(texture_target, egl_image.get())) {
    return nullptr;
  }

  auto texture = base::MakeRefCounted<gles2::TexturePassthrough>(
      service_id, texture_target);
  GLint texture_memory_size = 0;
  api->glGetTexParameterivFn(texture_target, GL_MEMORY_SIZE_ANGLE,
                             &texture_memory_size);
  texture->SetEstimatedSize(texture_memory_size);

  return base::MakeRefCounted<GLTextureHolder>(base::PassKey<D3DImageBacking>(),
                                               std::move(texture),
                                               std::move(egl_image));
}

// static
std::unique_ptr<D3DImageBacking> D3DImageBacking::CreateFromSwapChainBuffer(
    const Mailbox& mailbox,
    viz::SharedImageFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    gpu::SharedImageUsageSet usage,
    Microsoft::WRL::ComPtr<ID3D11Texture2D> d3d11_texture,
    Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain,
    const GLFormatCaps& gl_format_caps,
    bool is_back_buffer) {
  DCHECK(format.is_single_plane());
  return base::WrapUnique(new D3DImageBacking(
      mailbox, format, size, color_space, surface_origin, alpha_type, usage,
      "SwapChainBuffer", std::move(d3d11_texture), /*dcomp_texture=*/nullptr,
      /*dxgi_shared_handle_state=*/nullptr, gl_format_caps, GL_TEXTURE_2D,
      /*array_slice=*/0u, std::move(swap_chain), is_back_buffer));
}

// static
std::unique_ptr<D3DImageBacking> D3DImageBacking::CreateFromD3D12Resource(
    const Mailbox& mailbox,
    const gfx::Size& size,
    gpu::SharedImageUsageSet usage,
    std::string debug_label,
    Microsoft::WRL::ComPtr<ID3D12Resource> d3d12_resource) {
  auto backing = base::WrapUnique(new D3DImageBacking(
      mailbox, size, usage, std::move(debug_label), std::move(d3d12_resource)));
  return backing;
}

// static
std::unique_ptr<D3DImageBacking> D3DImageBacking::Create(
    const Mailbox& mailbox,
    viz::SharedImageFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    gpu::SharedImageUsageSet usage,
    std::string debug_label,
    Microsoft::WRL::ComPtr<ID3D11Texture2D> d3d11_texture,
    Microsoft::WRL::ComPtr<IDCompositionTexture> dcomp_texture,
    scoped_refptr<DXGISharedHandleState> dxgi_shared_handle_state,
    const GLFormatCaps& gl_format_caps,
    GLenum texture_target,
    size_t array_slice,
    bool use_update_subresource1,
    bool is_thread_safe) {
  const bool has_webgpu_usage = usage.HasAny(SHARED_IMAGE_USAGE_WEBGPU_READ |
                                             SHARED_IMAGE_USAGE_WEBGPU_WRITE);
  // DXGI shared handle is required for WebGPU/Dawn/D3D12 interop.
  CHECK(!has_webgpu_usage || dxgi_shared_handle_state);
  auto backing = base::WrapUnique(new D3DImageBacking(
      mailbox, format, size, color_space, surface_origin, alpha_type, usage,
      std::move(debug_label), std::move(d3d11_texture),
      std::move(dcomp_texture), std::move(dxgi_shared_handle_state),
      gl_format_caps, texture_target, array_slice, /*swap_chain=*/nullptr,
      /*is_back_buffer=*/false, use_update_subresource1, is_thread_safe));
  return backing;
}

D3DImageBacking::D3DImageBacking(
    const Mailbox& mailbox,
    viz::SharedImageFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    gpu::SharedImageUsageSet usage,
    std::string debug_label,
    Microsoft::WRL::ComPtr<ID3D11Texture2D> d3d11_texture,
    Microsoft::WRL::ComPtr<IDCompositionTexture> dcomp_texture,
    scoped_refptr<DXGISharedHandleState> dxgi_shared_handle_state,
    const GLFormatCaps& gl_format_caps,
    GLenum texture_target,
    size_t array_slice,
    Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain,
    bool is_back_buffer,
    bool use_update_subresource1,
    bool is_thread_safe)
    : ClearTrackingSharedImageBacking(mailbox,
                                      format,
                                      size,
                                      color_space,
                                      surface_origin,
                                      alpha_type,
                                      usage,
                                      std::move(debug_label),
                                      format.EstimatedSizeInBytes(size),
                                      is_thread_safe),
      d3d11_texture_(std::move(d3d11_texture)),
      dcomp_texture_(std::move(dcomp_texture)),
      dxgi_shared_handle_state_(std::move(dxgi_shared_handle_state)),
      gl_format_caps_(gl_format_caps),
      texture_target_(texture_target),
      array_slice_(array_slice),
      swap_chain_(std::move(swap_chain)),
      is_back_buffer_(is_back_buffer),
      use_update_subresource1_(use_update_subresource1),
      angle_d3d11_device_(gl::QueryD3D11DeviceObjectFromANGLE()) {
  if (d3d11_texture_) {
    d3d11_texture_->GetDevice(&texture_d3d11_device_);
    d3d11_texture_->GetDesc(&d3d11_texture_desc_);
  }
}

D3DImageBacking::D3DImageBacking(
    const Mailbox& mailbox,
    const gfx::Size& size,
    gpu::SharedImageUsageSet usage,
    std::string debug_label,
    Microsoft::WRL::ComPtr<ID3D12Resource> d3d12_resource)
    : ClearTrackingSharedImageBacking(mailbox,
                                      viz::SharedImageFormat(),
                                      size,
                                      gfx::ColorSpace(),
                                      GrSurfaceOrigin::kTopLeft_GrSurfaceOrigin,
                                      SkAlphaType::kUnknown_SkAlphaType,
                                      usage,
                                      std::move(debug_label),
                                      size.width(),
                                      /*is_thread_safe=*/false),
      d3d12_resource_(std::move(d3d12_resource)),
      texture_target_(0),
      array_slice_(0),
      is_back_buffer_(false),
      use_update_subresource1_(false) {}

D3DImageBacking::~D3DImageBacking() {
  if (!have_context()) {
    for (auto& texture_holder : gl_texture_holders_) {
      if (texture_holder) {
        texture_holder->MarkContextLost();
      }
    }
  }
}

ID3D11Texture2D* D3DImageBacking::GetOrCreateStagingTexture() {
  if (!staging_texture_) {
    D3D11_TEXTURE2D_DESC staging_desc = {};
    staging_desc.Width = d3d11_texture_desc_.Width;
    staging_desc.Height = d3d11_texture_desc_.Height;
    staging_desc.Format = d3d11_texture_desc_.Format;
    staging_desc.MipLevels = 1;
    staging_desc.ArraySize = 1;
    staging_desc.SampleDesc.Count = 1;
    staging_desc.Usage = D3D11_USAGE_STAGING;
    staging_desc.CPUAccessFlags =
        D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;

    CHECK(texture_d3d11_device_);
    HRESULT hr = texture_d3d11_device_->CreateTexture2D(&staging_desc, nullptr,
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
  AutoLock auto_lock(this);
  DCHECK_EQ(pixmaps.size(), static_cast<size_t>(format().NumberOfPlanes()));

  if (use_update_subresource1_ && CanUseUpdateSubresource(pixmaps)) {
    CHECK(texture_d3d11_device_);
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> device_context;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext1> device_context_1;
    texture_d3d11_device_->GetImmediateContext(&device_context);
    device_context.As(&device_context_1);

    device_context_1->UpdateSubresource1(
        d3d11_texture_.Get(), /*DstSubresource=*/0, /*pDstBox=*/nullptr,
        pixmaps[0].addr(), pixmaps[0].rowBytes(), /*SrcDepthPitch=*/0,
        D3D11_COPY_DISCARD);

    return true;
  }

  ID3D11Texture2D* staging_texture = GetOrCreateStagingTexture();
  if (!staging_texture) {
    return false;
  }

  CHECK(texture_d3d11_device_);
  Microsoft::WRL::ComPtr<ID3D11DeviceContext> device_context;
  texture_d3d11_device_->GetImmediateContext(&device_context);

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
    CopyImagePlane(source_memory, source_stride, dest_memory, dest_stride,
                   pixmap.info().minRowBytes(), plane_size.height());

    dest_offset += mapped_resource.RowPitch * plane_size.height();
  }

  device_context->Unmap(staging_texture, 0);
  device_context->CopyResource(d3d11_texture_.Get(), staging_texture);

  return true;
}

bool D3DImageBacking::CopyToStagingTexture() {
  TRACE_EVENT0("gpu", "D3DImageBacking::CopyToStagingTexture");
  ID3D11Texture2D* staging_texture = GetOrCreateStagingTexture();
  if (!staging_texture) {
    return false;
  }
  Microsoft::WRL::ComPtr<ID3D11DeviceContext> device_context;
  texture_d3d11_device_->GetImmediateContext(&device_context);
  device_context->CopyResource(staging_texture, d3d11_texture_.Get());
  return true;
}

bool D3DImageBacking::ReadbackFromStagingTexture(
    const std::vector<SkPixmap>& pixmaps) {
  TRACE_EVENT0("gpu", "D3DImageBacking::ReadbackFromStagingTexture");
  Microsoft::WRL::ComPtr<ID3D11DeviceContext> device_context;
  texture_d3d11_device_->GetImmediateContext(&device_context);

  ID3D11Texture2D* staging_texture = GetOrCreateStagingTexture();

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
    CopyImagePlane(source_memory, source_stride, dest_memory, dest_stride,
                   pixmap.info().minRowBytes(), plane_size.height());

    source_offset += mapped_resource.RowPitch * plane_size.height();
  }

  device_context->Unmap(staging_texture, 0);
  return true;
}

bool D3DImageBacking::ReadbackToMemory(const std::vector<SkPixmap>& pixmaps) {
  TRACE_EVENT0("gpu", "D3DImageBacking::ReadbackToMemory");
  AutoLock auto_lock(this);
  return CopyToStagingTexture() && ReadbackFromStagingTexture(pixmaps);
}

void D3DImageBacking::ReadbackToMemoryAsync(
    const std::vector<SkPixmap>& pixmaps,
    base::OnceCallback<void(bool)> callback) {
  AutoLock auto_lock(this);
  TRACE_EVENT0("gpu", "D3DImageBacking::ReadbackToMemoryAsync");

  if (pending_copy_event_watcher_) {
    LOG(ERROR) << "Existing ReadbackToMemory operation pending";
    std::move(callback).Run(false);
    return;
  }

  if (!CopyToStagingTexture()) {
    std::move(callback).Run(false);
    return;
  }

  base::WaitableEvent copy_complete_event;
  Microsoft::WRL::ComPtr<IDXGIDevice2> dxgi_device;
  texture_d3d11_device_.As(&dxgi_device);
  dxgi_device->EnqueueSetEvent(copy_complete_event.handle());

  pending_copy_event_watcher_.emplace();
  CHECK(pending_copy_event_watcher_->StartWatching(
      &copy_complete_event,
      base::IgnoreArgs<base::WaitableEvent*>(base::BindOnce(
          &D3DImageBacking::OnCopyToStagingTextureDone,
          weak_ptr_factory_.GetWeakPtr(), pixmaps, std::move(callback))),
      base::SingleThreadTaskRunner::GetCurrentDefault()));
}

void D3DImageBacking::OnCopyToStagingTextureDone(
    const std::vector<SkPixmap>& pixmaps,
    base::OnceCallback<void(bool)> readback_cb) {
  AutoLock auto_lock(this);
  pending_copy_event_watcher_.reset();
  std::move(readback_cb).Run(ReadbackFromStagingTexture(pixmaps));
}

std::unique_ptr<DawnImageRepresentation> D3DImageBacking::ProduceDawn(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker,
    const wgpu::Device& device,
    wgpu::BackendType backend_type,
    std::vector<wgpu::TextureFormat> view_formats,
    scoped_refptr<SharedContextState> context_state) {
#if BUILDFLAG(DAWN_ENABLE_BACKEND_OPENGLES)
  if (backend_type == wgpu::BackendType::OpenGLES) {
    std::unique_ptr<GLTextureImageRepresentationBase> gl_representation =
        ProduceGLTexturePassthrough(manager, tracker);
    auto* d3d_representation =
        static_cast<GLTexturePassthroughD3DImageRepresentation*>(
            gl_representation.get());
    void* egl_image = d3d_representation->GetEGLImage();
    if (!egl_image) {
      LOG(ERROR) << "EGL image is null.";
      return nullptr;
    }
    return std::make_unique<DawnEGLImageRepresentation>(
        std::move(gl_representation), egl_image, manager, this, tracker,
        device);
  }
#endif

  if (backend_type != wgpu::BackendType::D3D11 &&
      backend_type != wgpu::BackendType::D3D12) {
    LOG(ERROR) << "Unsupported Dawn backend: "
               << static_cast<WGPUBackendType>(backend_type);
    return nullptr;
  }

  {
    AutoLock auto_lock(this);
    // Persistently open the shared handle by caching it on this backing.
    auto shared_texture_memory = GetSharedTextureMemory(device);
    if (!shared_texture_memory) {
      Microsoft::WRL::ComPtr<ID3D11Device> dawn_d3d11_device;
      if (backend_type == wgpu::BackendType::D3D11) {
        dawn_d3d11_device = dawn::native::d3d11::GetD3D11Device(device.Get());
      }
      if (dawn_d3d11_device == texture_d3d11_device_) {
        shared_texture_memory =
            CreateDawnSharedTextureMemory(device, d3d11_texture_);
      } else {
        CHECK(dxgi_shared_handle_state_);
        const HANDLE shared_handle =
            dxgi_shared_handle_state_->GetSharedHandle();
        CHECK(base::win::HandleTraits::IsHandleValid(shared_handle));
        bool use_keyed_mutex = d3d11_texture_desc_.MiscFlags &
                               D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX;
        shared_texture_memory = CreateDawnSharedTextureMemory(
            device, use_keyed_mutex, shared_handle);
      }

      if (!shared_texture_memory) {
        LOG(ERROR) << "Failed to create shared_texture_memory.";
        return nullptr;
      }

      if (dxgi_shared_handle_state_) {
        dxgi_shared_handle_state_->MaybeCacheSharedTextureMemory(
            device, shared_texture_memory);
      }
    }

    auto* dawn_context_provider = context_state->dawn_context_provider();
    if (dawn_context_provider &&
        dawn_context_provider->GetDevice().Get() == device.Get()) {
      // Cache SharedTextureMemory only for Graphite device.
      // TODO(crbug.com/345674550): Cache the textures for WebGPU device.
      DCHECK(shared_texture_memory);
      auto cached_shared_texture_memory =
          dawn_shared_texture_holder_.GetSharedTextureMemory(device);
      if (!cached_shared_texture_memory) {
        // GetSharedTextureMemory() might return non-null while nothing is
        // cached inside dawn_shared_texture_holder_. This can happen if a 2nd
        // backing is created from an existing DXGISharedHandleState. The STM is
        // stored inside DXGISharedHandleState but not in the 2nd backing's
        // dawn_shared_texture_holder_ yet. In this case, we also need to cache
        // the STM in dawn_shared_texture_holder_ to allow wgpu::Texture to be
        // cached. Otherwise the wgpu::Texture would have been destroyed in
        // EndAccessDawn.
        dawn_shared_texture_holder_.MaybeCacheSharedTextureMemory(
            device, shared_texture_memory);
      } else {
        CHECK_EQ(cached_shared_texture_memory.Get(),
                 shared_texture_memory.Get());
      }
    }
  }  // AutoLock scope

  return std::make_unique<DawnD3DImageRepresentation>(
      manager, this, tracker, device, backend_type, view_formats);
}

void D3DImageBacking::UpdateExternalFence(
    scoped_refptr<gfx::D3DSharedFence> external_fence) {
  // TODO(crbug.com/40192861): Handle cases that write_fences_ is not empty.
  AutoLock auto_lock(this);
  write_fences_.insert(std::move(external_fence));
}

std::unique_ptr<VideoImageRepresentation> D3DImageBacking::ProduceVideo(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker,
    VideoDevice device) {
  return std::make_unique<D3D11VideoImageRepresentation>(
      manager, this, tracker, device, d3d11_texture_);
}

std::vector<scoped_refptr<gfx::D3DSharedFence>>
D3DImageBacking::GetPendingWaitFences(
    const Microsoft::WRL::ComPtr<ID3D11Device>& wait_d3d11_device,
    const wgpu::Device& wait_dawn_device,
    bool write_access) {
  // We don't need to use fences for single device scenarios (no shared handle),
  // or if we're using a keyed mutex instead. The existence of |dcomp_texture_|
  // (i.e. scanout cases) means we always need to check for the presence of the
  // availability fence.
  if (!use_cross_device_fence_synchronization() && !dcomp_texture_) {
    return {};
  }

  // Lazily create and signal the D3D11 fence on the texture's original device
  // if not present and we're using the backing on another device.
  auto& texture_device_fence = d3d11_signaled_fence_map_[texture_d3d11_device_];
  if (wait_d3d11_device != texture_d3d11_device_ && !texture_device_fence) {
    texture_device_fence =
        gfx::D3DSharedFence::CreateForD3D11(texture_d3d11_device_);
    if (!texture_device_fence) {
      LOG(ERROR) << "Failed to retrieve D3D11 signal fence";
      return {};
    }
    // Make D3D11 device wait for |write_fences_| since we'll replace it below.
    for (auto& fence : write_fences_) {
      if (!fence->WaitD3D11(texture_d3d11_device_)) {
        LOG(ERROR) << "Failed to wait for write fence";
        return {};
      }
    }
    if (!texture_device_fence->IncrementAndSignalD3D11()) {
      LOG(ERROR) << "Failed to signal D3D11 signal fence";
      return {};
    }
    // Store it in |write_fences_| so it's waited on for all subsequent access.
    write_fences_.clear();
    write_fences_.insert(texture_device_fence);
  }

  // TODO(crbug.com/335003893): Investigate how to avoid passing any fences back
  // to Dawn that were previously signaled by Dawn. Currently there's no way to
  // determine which of the fences that Dawn returns to us in EndAccess fit this
  // criteria.
  std::vector<scoped_refptr<gfx::D3DSharedFence>> wait_fences;
  // Always wait for previous write for both read-only or read-write access.
  // Skip the wait if it's for the fence last signaled by the Dawn device, or
  // for D3D11 if the fence was issued for the same device since D3D11 has a
  // single immediate context for issuing commands.
  for (auto& fence : write_fences_) {
    wait_fences.push_back(fence);
  }
  // Also wait for all previous reads for read-write access.
  if (write_access) {
    for (const auto& read_fence : read_fences_) {
      wait_fences.push_back(read_fence);
    }
    // The presence of a DComp texture fence is considered an outstanding read
    // that must be waited on.
    if (auto fence = GetDCompTextureAvailabilityFenceForCurrentFrame()) {
      wait_fences.push_back(std::move(fence));
    }
  }
  return wait_fences;
}

int D3DImageBacking::TrackBeginAccessToWGPUTexture(wgpu::Texture texture) {
  return wgpu_texture_ongoing_accesses_[texture.Get()]++;
}

int D3DImageBacking::TrackEndAccessToWGPUTexture(wgpu::Texture texture) {
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

wgpu::Texture D3DImageBacking::BeginAccessDawn(
    const wgpu::Device& device,
    wgpu::BackendType backend_type,
    wgpu::TextureUsage wgpu_usage,
    wgpu::TextureUsage wgpu_internal_usage,
    std::vector<wgpu::TextureFormat> view_formats) {
  AutoLock auto_lock(this);

  const auto kWriteUsage = wgpu::TextureUsage::CopyDst |
                           wgpu::TextureUsage::StorageBinding |
                           wgpu::TextureUsage::RenderAttachment;
  bool write_access = wgpu_usage & kWriteUsage;
  if (wgpu_internal_usage & kWriteUsage) {
    write_access = true;
  }

  if (!ValidateBeginAccess(write_access)) {
    return nullptr;
  }

  Microsoft::WRL::ComPtr<ID3D11Device> dawn_d3d11_device;
  if (backend_type == wgpu::BackendType::D3D11) {
    dawn_d3d11_device = dawn::native::d3d11::GetD3D11Device(device.Get());
    CHECK(dawn_d3d11_device);
  }

  // Dawn access is allowed without shared handle for single device scenarios.
  CHECK(dxgi_shared_handle_state_ ||
        dawn_d3d11_device == texture_d3d11_device_);

  auto shared_texture_memory = GetSharedTextureMemory(device);
  CHECK(shared_texture_memory);

  // Defer clearing fences until later to handle Dawn failure to import texture.
  std::vector<scoped_refptr<gfx::D3DSharedFence>> wait_fences =
      GetPendingWaitFences(dawn_d3d11_device, device, write_access);
  std::vector<wgpu::SharedFence> shared_fences;
  std::vector<uint64_t> signaled_values;
  for (auto& wait_fence : wait_fences) {
    // TODO(crbug.com/335003893): Look into caching the wgpu::SharedFence object
    // in gfx::D3DSharedFence.
    shared_fences.push_back(CreateDawnSharedFence(device, wait_fence));
    signaled_values.push_back(wait_fence->GetFenceValue());
  }
  wgpu::SharedTextureMemoryD3DSwapchainBeginState swapchain_begin_state = {};
  swapchain_begin_state.isSwapchain =
      usage().Has(SHARED_IMAGE_USAGE_WEBGPU_SWAP_CHAIN_TEXTURE);

  wgpu::SharedTextureMemoryBeginAccessDescriptor desc = {};
  const bool is_clear = ClearedRectInternal() == gfx::Rect(size());
  desc.initialized = is_clear;
  // TODO(crbug.com/335003893): Figure out a clear way to express what's allowed
  // and what's not to the client.
  desc.concurrentRead = !write_access && is_clear;
  desc.fenceCount = shared_fences.size();
  desc.fences = shared_fences.data();
  desc.signaledValues = signaled_values.data();
  desc.nextInChain = &swapchain_begin_state;

  wgpu::Texture texture =
      dawn_shared_texture_holder_.GetCachedWGPUTexture(device, wgpu_usage);
  if (!texture) {
    texture = CreateDawnSharedTexture(shared_texture_memory, wgpu_usage,
                                      wgpu_internal_usage, view_formats);
    if (!texture) {
      LOG(ERROR) << "Failed to create WGPUTexture";
      return nullptr;
    }

    std::string label = base::StrCat({GetName(), "_", debug_label()});
    texture.SetLabel(label.c_str());

    dawn_shared_texture_holder_.MaybeCacheWGPUTexture(device, texture);
  }

  // If there is already an ongoing Dawn access for this texture, then the
  // necessary work for starting the access (i.e., waiting on fences and
  // informing SharedTextureMemory) already happened as part of the initial
  // BeginAccess().
  // NOTE: SharedTextureMemory does not allow a BeginAccess() call on a texture
  // that already has an ongoing access (at the internal wgpu::Texture
  // level), so short-circuiting out here is not simply an optimization but
  // is actually necessary.
  int num_accesses_already_present = TrackBeginAccessToWGPUTexture(texture);
  if (num_accesses_already_present > 0) {
    return texture;
  }

  if (shared_texture_memory.BeginAccess(texture, &desc) !=
      wgpu::Status::Success) {
    LOG(ERROR) << "SharedTextureMemory::BeginAccess() failed.";
    dawn_shared_texture_holder_.RemoveWGPUTextureFromCache(device, texture);
    TrackEndAccessToWGPUTexture(texture);
    return nullptr;
  }

  // Clear fences and update state iff Dawn BeginAccess succeeds.
  BeginAccessCommon(write_access);
  return texture;
}

void D3DImageBacking::EndAccessDawn(const wgpu::Device& device,
                                    wgpu::Texture texture) {
  DCHECK(texture);
  if (dawn::native::IsTextureSubresourceInitialized(texture.Get(), 0, 1, 0,
                                                    1)) {
    SetCleared();
  }
  AutoLock auto_lock(this);

  auto shared_texture_memory = GetSharedTextureMemory(device);
  // Shared texture memory is removed from cache on first EndAccess after device
  // is lost. It's ok to skip synchronization because it should've already been
  // synchronized before the entry was removed from the cache.
  if (!shared_texture_memory) {
    EndAccessCommon(/*signaled_fences=*/{});
    return;
  }

  int num_outstanding_accesses = TrackEndAccessToWGPUTexture(texture);
  // If there is still an ongoing Dawn access on this texture,
  // short-circuit out of doing any other work. In particular, do not consume
  // fences or end the access at the level of SharedTextureMemory. That work
  // will happen when the last ongoing Dawn access finishes.
  if (num_outstanding_accesses > 0) {
    return;
  }

  // EndAccess returns a null fence handle if the device was lost, but that's
  // OK since we check for it explicitly below.
  wgpu::SharedTextureMemoryEndAccessState end_state = {};
  shared_texture_memory.EndAccess(texture.Get(), &end_state);

  D3DSharedFenceSet signaled_fences;
  if (use_cross_device_fence_synchronization()) {
    auto& cached_fences = dawn_signaled_fences_map_[device.Get()];
    for (size_t i = 0; i < end_state.fenceCount; ++i) {
      auto& signaled_value = end_state.signaledValues[i];
      auto& fence = end_state.fences[i];
      wgpu::SharedFenceDXGISharedHandleExportInfo shared_handle_info;
      wgpu::SharedFenceExportInfo export_info;
      export_info.nextInChain = &shared_handle_info;
      fence.ExportInfo(&export_info);
      DCHECK_EQ(export_info.type, wgpu::SharedFenceType::DXGISharedHandle);

      // Try to find and reuse the last signaled fence if it's the same fence.
      scoped_refptr<gfx::D3DSharedFence> signaled_fence;
      for (auto& cached_fence : cached_fences) {
        if (cached_fence->IsSameFenceAsHandle(shared_handle_info.handle)) {
          signaled_fence = cached_fence;
          break;
        }
      }
      if (!signaled_fence) {
        signaled_fence = gfx::D3DSharedFence::CreateFromUnownedHandle(
            shared_handle_info.handle);
      }
      if (signaled_fence) {
        signaled_fence->Update(signaled_value);
        signaled_fences.insert(signaled_fence);
      } else {
        LOG(ERROR) << "Failed to import D3D fence from Dawn on EndAccess";
      }
    }
    // Cache the fences.
    cached_fences = signaled_fences;
  }

  // Erase from cache if shared texture memory is invalid i.e. device was lost.
  if (shared_texture_memory.IsDeviceLost()) {
    dawn_shared_texture_holder_.EraseDataIfDeviceLost();
    dawn_signaled_fences_map_.erase(device.Get());
    if (dxgi_shared_handle_state_) {
      dxgi_shared_handle_state_->EraseDawnSharedTextureMemory(device);
    }
  }

  dawn_shared_texture_holder_.DestroyWGPUTextureIfNotCached(device, texture);
  EndAccessCommon(signaled_fences);
}

wgpu::SharedTextureMemory D3DImageBacking::GetSharedTextureMemory(
    const wgpu::Device& device) {
  if (dxgi_shared_handle_state_) {
    return dxgi_shared_handle_state_->GetSharedTextureMemory(device);
  }
  return dawn_shared_texture_holder_.GetSharedTextureMemory(device);
}

bool D3DImageBacking::BeginAccessD3D11(
    Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device,
    bool write_access) {
  AutoLock auto_lock(this);
  if (!ValidateBeginAccess(write_access)) {
    return false;
  }

  // Defer clearing fences until later to handle D3D11 failure to synchronize.
  std::vector<scoped_refptr<gfx::D3DSharedFence>> wait_fences =
      GetPendingWaitFences(d3d11_device, /*dawn_device=*/nullptr, write_access);
  for (auto& wait_fence : wait_fences) {
    if (!wait_fence->WaitD3D11(d3d11_device)) {
      LOG(ERROR) << "Failed to wait for fence";
      return false;
    }
  }

  // D3D11 access is allowed without shared handle for single device scenarios.
  CHECK(dxgi_shared_handle_state_ || d3d11_device == texture_d3d11_device_);
  if (dxgi_shared_handle_state_ &&
      !dxgi_shared_handle_state_->AcquireKeyedMutex(d3d11_device)) {
    LOG(ERROR) << "Failed to synchronize using keyed mutex";
    return false;
  }

  // Clear fences and update state iff D3D11 BeginAccess succeeds.
  BeginAccessCommon(write_access);
  return true;
}

void D3DImageBacking::EndAccessD3D11(
    Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device) {
  const bool is_texture_device = d3d11_device == texture_d3d11_device_;
  // If shared handle is not present, we can only access on the same device.
  AutoLock auto_lock(this);
  CHECK(dxgi_shared_handle_state_ || is_texture_device);
  // Do not create a fence for the texture's original device if we're only using
  // the texture on one device or using a keyed mutex. The fence is lazily
  // created on the first access from another device in GetPendingWaitFences().
  D3DSharedFenceSet signaled_fence;
  if (use_cross_device_fence_synchronization()) {
    auto& d3d11_signal_fence = d3d11_signaled_fence_map_[d3d11_device];
    if (!d3d11_signal_fence) {
      d3d11_signal_fence = gfx::D3DSharedFence::CreateForD3D11(d3d11_device);
    }
    if (d3d11_signal_fence && d3d11_signal_fence->IncrementAndSignalD3D11()) {
      signaled_fence.insert(d3d11_signal_fence);
    } else {
      LOG(ERROR) << "Failed to signal D3D11 device fence on EndAccess";
    }
  }

  if (dxgi_shared_handle_state_) {
    dxgi_shared_handle_state_->ReleaseKeyedMutex(d3d11_device);
  }

  EndAccessCommon(signaled_fence);
}

scoped_refptr<gfx::D3DSharedFence>
D3DImageBacking::GetDCompTextureAvailabilityFenceForCurrentFrame() const {
  if (!dcomp_texture_) {
    // No |dcomp_texture_| means no waiting is needed.
    return nullptr;
  }

  Microsoft::WRL::ComPtr<ID3D11Fence> d3d11_fence;
  uint64_t fence_value = 0;
  HRESULT hr = dcomp_texture_->GetAvailableFence(&fence_value,
                                                 IID_PPV_ARGS(&d3d11_fence));
  CHECK_EQ(hr, S_OK) << ", GetAvailableFence failed: "
                     << logging::SystemErrorCodeToString(hr);

  // |GetAvailableFence| will return a null fence if the texture is still
  // attached to the DComp tree. We cannot end the read access at this point
  // since DWM can still scanout from the texture. This is probably a bug where
  // the output device returned an overlay access while the overlay image was
  // still in the DComp tree.
  //
  // This can also trigger if we have multiple concurrent outstanding overlay
  // read accesses, which is not currently supported.
  CHECK(d3d11_fence) << "Overlay access is still in use by DWM.";

  // If the fence is already passed the wait value, we don't need to wait on it.
  if (d3d11_fence->GetCompletedValue() >= fence_value) {
    return nullptr;
  }

  // Note we're passing a null device since the DWM internal device will signal
  // this fence.
  return gfx::D3DSharedFence::CreateFromD3D11Fence(
      /*d3d11_signal_device=*/nullptr, std::move(d3d11_fence), fence_value);
}

std::unique_ptr<DawnBufferRepresentation> D3DImageBacking::ProduceDawnBuffer(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker,
    const wgpu::Device& device,
    wgpu::BackendType backend_type) {
  DCHECK(usage() & SHARED_IMAGE_USAGE_WEBGPU_SHARED_BUFFER);
  DCHECK(d3d12_resource_.Get() != nullptr);

  if (backend_type != wgpu::BackendType::D3D12) {
    LOG(ERROR) << "Unsupported Dawn backend: "
               << static_cast<WGPUBackendType>(backend_type);
    return nullptr;
  }

  {
    AutoLock auto_lock(this);
    // Persistently open the shared handle by caching it on this backing.
    if (!dawn_shared_buffer_memory_) {
      Microsoft::WRL::ComPtr<ID3D12Device> dawn_d3d12_device;
      if (backend_type == wgpu::BackendType::D3D12) {
        dawn_d3d12_device = dawn::native::d3d12::GetD3D12Device(device.Get());
      }

      dawn_shared_buffer_memory_ =
          CreateDawnSharedBufferMemory(device, d3d12_resource_);

      if (!dawn_shared_buffer_memory_) {
        LOG(ERROR) << "Failed to create shared_buffer_memory.";
        return nullptr;
      }
    }
  }  // AutoLock scope

  return std::make_unique<DawnD3DBufferRepresentation>(manager, this, tracker,
                                                       device, backend_type);
}

wgpu::Buffer D3DImageBacking::BeginAccessDawnBuffer(
    const wgpu::Device& device,
    wgpu::BackendType backend_type,
    wgpu::BufferUsage usage) {
  AutoLock auto_lock(this);
  Microsoft::WRL::ComPtr<ID3D12Device> dawn_d3d12_device;
  if (backend_type == wgpu::BackendType::D3D12) {
    dawn_d3d12_device = dawn::native::d3d12::GetD3D12Device(device.Get());
    CHECK(dawn_d3d12_device);
  }

  CHECK(dawn_shared_buffer_memory_);

  // Pass all fences on the backing to Dawn. In the future, consider optimizing
  // this based on read/write usages and/or using the Dawn fence cache.
  std::vector<wgpu::SharedFence> shared_fences;
  shared_fences.reserve(read_fences_.size() + write_fences_.size());
  std::vector<uint64_t> signaled_values;
  signaled_values.reserve(read_fences_.size() + write_fences_.size());

  for (auto& write_fence : write_fences_) {
    shared_fences.push_back(CreateDawnSharedFence(device, write_fence));
    signaled_values.push_back(write_fence->GetFenceValue());
  }

  for (const auto& read_fence : read_fences_) {
    shared_fences.push_back(CreateDawnSharedFence(device, read_fence));
    signaled_values.push_back(read_fence->GetFenceValue());
  }

  wgpu::SharedBufferMemoryBeginAccessDescriptor desc = {};
  desc.initialized = true;
  desc.fenceCount = shared_fences.size();
  desc.fences = shared_fences.data();
  desc.signaledValues = signaled_values.data();

  wgpu::Buffer buffer =
      CreateDawnSharedBuffer(dawn_shared_buffer_memory_, usage);
  if (!buffer) {
    LOG(ERROR) << "Failed to produce WGPUBuffer";
    return nullptr;
  }

  if (dawn_shared_buffer_memory_.BeginAccess(buffer, &desc) !=
      wgpu::Status::Success) {
    LOG(ERROR) << "Failed to begin access on WGPUBuffer";
    return nullptr;
  }

  // Clear fences and update state if Dawn BeginAccess succeeds.
  BeginAccessCommon(true);
  return buffer;
}

void D3DImageBacking::EndAccessDawnBuffer(const wgpu::Device& device,
                                          wgpu::Buffer buffer) {
  AutoLock auto_lock(this);
  DCHECK(buffer);
  CHECK(dawn_shared_buffer_memory_);

  wgpu::SharedBufferMemoryEndAccessState end_state = {};
  dawn_shared_buffer_memory_.EndAccess(buffer.Get(), &end_state);

  D3DSharedFenceSet signaled_fences;
  signaled_fences.reserve(end_state.fenceCount);
  for (size_t i = 0; i < end_state.fenceCount; ++i) {
    auto& signaled_value = end_state.signaledValues[i];
    auto& fence = end_state.fences[i];
    wgpu::SharedFenceDXGISharedHandleExportInfo shared_handle_info;
    wgpu::SharedFenceExportInfo export_info;
    export_info.nextInChain = &shared_handle_info;
    fence.ExportInfo(&export_info);
    DCHECK_EQ(export_info.type, wgpu::SharedFenceType::DXGISharedHandle);

    scoped_refptr<gfx::D3DSharedFence> signaled_fence =
        gfx::D3DSharedFence::CreateFromUnownedHandle(shared_handle_info.handle);

    if (signaled_fence) {
      signaled_fence->Update(signaled_value);
      signaled_fences.insert(signaled_fence);
    } else {
      LOG(ERROR) << "Failed to import D3D fence from Dawn on EndAccess";
    }
  }

  EndAccessCommon(signaled_fences);
}

bool D3DImageBacking::ValidateBeginAccess(bool write_access) const {
  if (in_write_access_) {
    LOG(ERROR) << "Already being accessed for write";
    return false;
  }
  if (write_access && num_readers_ > 0) {
    LOG(ERROR) << "Already being accessed for read";
    return false;
  }
  return true;
}

void D3DImageBacking::BeginAccessCommon(bool write_access) {
  if (write_access) {
    // For read-write access, we wait for all previous reads and reset fences
    // since all subsequent access will wait on |write_fence_| generated when
    // this access ends.
    write_fences_.clear();
    read_fences_.clear();
    in_write_access_ = true;
  } else {
    num_readers_++;
  }
}

void D3DImageBacking::EndAccessCommon(
    const D3DSharedFenceSet& signaled_fences) {
  DCHECK(base::ranges::all_of(signaled_fences, std::identity()));
  if (in_write_access_) {
    DCHECK(write_fences_.empty());
    DCHECK(read_fences_.empty());
    in_write_access_ = false;
    write_fences_ = signaled_fences;
  } else {
    num_readers_--;
    for (const auto& signaled_fence : signaled_fences) {
      read_fences_.insert(signaled_fence);
    }
  }
}

void* D3DImageBacking::GetEGLImage() const {
  AutoLock auto_lock(this);
  DCHECK(format().is_single_plane());
  return gl_texture_holders_[0] ? gl_texture_holders_[0]->egl_image() : nullptr;
}

bool D3DImageBacking::PresentSwapChain() {
  AutoLock auto_lock(this);
  TRACE_EVENT0("gpu", "D3DImageBacking::PresentSwapChain");
  if (!swap_chain_ || !is_back_buffer_) {
    LOG(ERROR) << "Backing does not correspond to back buffer of swap chain";
    return false;
  }

  constexpr UINT kFlags = DXGI_PRESENT_ALLOW_TEARING;
  constexpr DXGI_PRESENT_PARAMETERS kParams = {};

  HRESULT hr = swap_chain_->Present1(/*interval=*/0, kFlags, &kParams);
  if (FAILED(hr)) {
    LOG(ERROR) << "Present1 failed with error " << std::hex << hr;
    return false;
  }

  DCHECK(format().is_single_plane());

  // we're rebinding to ensure that underlying D3D11 resource views are
  // recreated in ANGLE.
  if (gl_texture_holders_[0]) {
    gl_texture_holders_[0]->set_needs_rebind(true);
  }

  // Flush device context otherwise present could be deferred.
  Microsoft::WRL::ComPtr<ID3D11DeviceContext> d3d11_device_context;
  texture_d3d11_device_->GetImmediateContext(&d3d11_device_context);
  d3d11_device_context->Flush();

  return true;
}

std::unique_ptr<GLTexturePassthroughImageRepresentation>
D3DImageBacking::ProduceGLTexturePassthrough(SharedImageManager* manager,
                                             MemoryTypeTracker* tracker) {
  TRACE_EVENT0("gpu", "D3DImageBacking::ProduceGLTexturePassthrough");

  const auto number_of_planes = static_cast<size_t>(format().NumberOfPlanes());
  std::vector<scoped_refptr<GLTextureHolder>> gl_texture_holders(
      number_of_planes);
  {
    AutoLock auto_lock(this);
    DCHECK_GE(gl_texture_holders_.size(), number_of_planes);

    // If DXGI shared handle is present, the |d3d11_texture_| might belong to a
    // different device with Graphite so retrieve the ANGLE specific D3D11
    // texture from the |dxgi_shared_handle_state_|.
    const bool is_angle_texture = texture_d3d11_device_ == angle_d3d11_device_;
    CHECK(is_angle_texture || dxgi_shared_handle_state_);
    auto d3d11_texture =
        is_angle_texture ? d3d11_texture_
                         : dxgi_shared_handle_state_->GetOrCreateD3D11Texture(
                               angle_d3d11_device_);
    if (!d3d11_texture) {
      LOG(ERROR) << "Failed to open DXGI shared handle";
      return nullptr;
    }

    for (int plane = 0; plane < format().NumberOfPlanes(); ++plane) {
      auto& holder = gl_texture_holders[plane];
      if (gl_texture_holders_[plane]) {
        holder = gl_texture_holders_[plane].get();
        continue;
      }

      // The GL internal format can differ from the underlying swap chain or
      // texture format e.g. RGBA or RGB instead of BGRA or RED/RG for NV12
      // texture planes. See EGL_ANGLE_d3d_texture_client_buffer spec for format
      // restrictions.
      GLFormatDesc gl_format_desc;
      if (format().is_multi_plane()) {
        gl_format_desc = gl_format_caps_.ToGLFormatDesc(format(), plane);
      } else {
        // For legacy multiplanar formats, `format` is already plane format (eg.
        // RED, RG), so we pass plane_index=0.
        gl_format_desc =
            gl_format_caps_.ToGLFormatDesc(format(), /*plane_index=*/0);
      }

      gfx::Size plane_size = format().GetPlaneSize(plane, size());
      // Creating the GL texture doesn't require exclusive access to the
      // underlying D3D11 texture.
      holder = CreateGLTexture(gl_format_desc, plane_size, color_space(),
                               d3d11_texture, texture_target_, array_slice_,
                               plane, swap_chain_);
      if (!holder) {
        LOG(ERROR) << "Failed to create GL texture for plane: " << plane;
        return nullptr;
      }
      // Cache the gl textures using weak pointers.
      gl_texture_holders_[plane] = holder->GetWeakPtr();
    }
  }

  return std::make_unique<GLTexturePassthroughD3DImageRepresentation>(
      manager, this, tracker, angle_d3d11_device_,
      std::move(gl_texture_holders));
}

std::unique_ptr<SkiaGaneshImageRepresentation>
D3DImageBacking::ProduceSkiaGanesh(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker,
    scoped_refptr<SharedContextState> context_state) {
  auto gl_representation = ProduceGLTexturePassthrough(manager, tracker);
  if (!gl_representation) {
    return nullptr;
  }
  return SkiaGLImageRepresentation::Create(std::move(gl_representation),
                                           std::move(context_state), manager,
                                           this, tracker);
}

#if BUILDFLAG(SKIA_USE_DAWN)
std::unique_ptr<SkiaGraphiteImageRepresentation>
D3DImageBacking::ProduceSkiaGraphite(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker,
    scoped_refptr<SharedContextState> context_state) {
  auto device = context_state->dawn_context_provider()->GetDevice();
  wgpu::AdapterInfo adapter_info;
  device.GetAdapter().GetInfo(&adapter_info);
  auto dawn_representation =
      ProduceDawn(manager, tracker, device.Get(), adapter_info.backendType, {},
                  context_state);
  if (!dawn_representation) {
    LOG(ERROR) << "Could not create Dawn Representation";
    return nullptr;
  }
  return SkiaGraphiteDawnImageRepresentation::Create(
      std::move(dawn_representation), context_state,
      context_state->gpu_main_graphite_recorder(), manager, this, tracker,
      array_slice_);
}
#endif  // BUILDFLAG(SKIA_USE_DAWN)

std::unique_ptr<OverlayImageRepresentation> D3DImageBacking::ProduceOverlay(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker) {
  TRACE_EVENT0("gpu", "D3DImageBacking::ProduceOverlay");
  return std::make_unique<OverlayD3DImageRepresentation>(manager, this, tracker,
                                                         texture_d3d11_device_);
}

std::optional<gl::DCLayerOverlayImage>
D3DImageBacking::GetDCLayerOverlayImage() {
  if (dcomp_texture_) {
    return std::make_optional<gl::DCLayerOverlayImage>(
        size(), dcomp_texture_, /*dcomp_surface_serial=*/0);
  }
  if (swap_chain_) {
    return std::make_optional<gl::DCLayerOverlayImage>(size(), swap_chain_);
  }
  return std::make_optional<gl::DCLayerOverlayImage>(size(), d3d11_texture_,
                                                     array_slice_);
}

bool D3DImageBacking::HasStagingTextureForTesting() const {
  AutoLock auto_lock(this);
  return !!staging_texture_;
}

}  // namespace gpu
