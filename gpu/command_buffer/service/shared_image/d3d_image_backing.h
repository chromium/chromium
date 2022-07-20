// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_D3D_IMAGE_BACKING_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_D3D_IMAGE_BACKING_H_

#include <d3d11.h>
#include <dxgi1_2.h>
#include <windows.h>
#include <wrl/client.h>

#include "base/memory/scoped_refptr.h"
#include "components/viz/common/resources/resource_format.h"
#include "gpu/command_buffer/service/dxgi_shared_handle_manager.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "gpu/command_buffer/service/memory_tracking.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image/shared_image_manager.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/gl/buildflags.h"
#include "ui/gl/gl_image_d3d.h"

// Usage of BUILDFLAG(USE_DAWN) needs to be after the include for
// ui/gl/buildflags.h
#if BUILDFLAG(USE_DAWN)
#include <dawn/native/D3D12Backend.h>
#endif  // BUILDFLAG(USE_DAWN)

namespace gfx {
class Size;
class ColorSpace;
}  // namespace gfx

namespace gpu {
struct Mailbox;

// Implementation of SharedImageBacking that holds buffer (front buffer/back
// buffer of swap chain) texture (as gles2::Texture/gles2::TexturePassthrough)
// and a reference to created swap chain.
class GPU_GLES2_EXPORT D3DImageBacking
    : public ClearTrackingSharedImageBacking {
 public:
  static std::unique_ptr<D3DImageBacking> CreateFromSwapChainBuffer(
      const Mailbox& mailbox,
      viz::ResourceFormat format,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      GrSurfaceOrigin surface_origin,
      SkAlphaType alpha_type,
      uint32_t usage,
      Microsoft::WRL::ComPtr<ID3D11Texture2D> d3d11_texture,
      Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain,
      bool is_back_buffer);

  static std::unique_ptr<D3DImageBacking> CreateFromDXGISharedHandle(
      const Mailbox& mailbox,
      viz::ResourceFormat format,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      GrSurfaceOrigin surface_origin,
      SkAlphaType alpha_type,
      uint32_t usage,
      Microsoft::WRL::ComPtr<ID3D11Texture2D> d3d11_texture,
      scoped_refptr<DXGISharedHandleState> dxgi_shared_handle_state);

  // TODO(sunnyps): Remove this after migrating DXVA decoder to EGLImage.
  static std::unique_ptr<D3DImageBacking> CreateFromGLTexture(
      const Mailbox& mailbox,
      viz::ResourceFormat format,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      GrSurfaceOrigin surface_origin,
      SkAlphaType alpha_type,
      uint32_t usage,
      Microsoft::WRL::ComPtr<ID3D11Texture2D> d3d11_texture,
      scoped_refptr<gles2::TexturePassthrough> gl_texture);

  static std::vector<std::unique_ptr<SharedImageBacking>>
  CreateFromVideoTexture(
      base::span<const Mailbox> mailboxes,
      DXGI_FORMAT dxgi_format,
      const gfx::Size& size,
      uint32_t usage,
      Microsoft::WRL::ComPtr<ID3D11Texture2D> d3d11_texture,
      unsigned array_slice,
      scoped_refptr<DXGISharedHandleState> dxgi_shared_handle_state = nullptr);

  static std::unique_ptr<D3DImageBacking> CreateFromSharedMemoryHandle(
      const Mailbox& mailbox,
      viz::ResourceFormat format,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      GrSurfaceOrigin surface_origin,
      SkAlphaType alpha_type,
      uint32_t usage,
      Microsoft::WRL::ComPtr<ID3D11Texture2D> d3d11_texture,
      gfx::GpuMemoryBufferHandle shared_memory_handle);

  D3DImageBacking(const D3DImageBacking&) = delete;
  D3DImageBacking& operator=(const D3DImageBacking&) = delete;

  ~D3DImageBacking() override;

  // SharedImageBacking implementation.
  SharedImageBackingType GetType() const override;
  void Update(std::unique_ptr<gfx::GpuFence> in_fence) override;
  bool CopyToGpuMemoryBuffer() override;
  bool ProduceLegacyMailbox(MailboxManager* mailbox_manager) override;
  bool PresentSwapChain() override;
  std::unique_ptr<DawnImageRepresentation> ProduceDawn(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker,
      WGPUDevice device,
      WGPUBackendType backend_type) override;
  void OnMemoryDump(const std::string& dump_name,
                    base::trace_event::MemoryAllocatorDump* dump,
                    base::trace_event::ProcessMemoryDump* pmd,
                    uint64_t client_tracing_id) override;

  bool BeginAccessD3D12();
  void EndAccessD3D12();

  bool BeginAccessD3D11();
  void EndAccessD3D11();

  scoped_refptr<DXGISharedHandleState> dxgi_shared_handle_state_for_testing()
      const {
    return dxgi_shared_handle_state_;
  }

  Microsoft::WRL::ComPtr<ID3D11Texture2D> d3d11_texture_for_testing() const {
    return d3d11_texture_;
  }

 protected:
  std::unique_ptr<GLTexturePassthroughImageRepresentation>
  ProduceGLTexturePassthrough(SharedImageManager* manager,
                              MemoryTypeTracker* tracker) override;

  std::unique_ptr<OverlayImageRepresentation> ProduceOverlay(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker) override;

  std::unique_ptr<SkiaImageRepresentation> ProduceSkia(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker,
      scoped_refptr<SharedContextState> context_state) override;

 private:
  D3DImageBacking(
      const Mailbox& mailbox,
      viz::ResourceFormat format,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      GrSurfaceOrigin surface_origin,
      SkAlphaType alpha_type,
      uint32_t usage,
      Microsoft::WRL::ComPtr<ID3D11Texture2D> d3d11_texture,
      scoped_refptr<gles2::TexturePassthrough> gl_texture,
      scoped_refptr<DXGISharedHandleState> dxgi_shared_handle_state = {},
      gfx::GpuMemoryBufferHandle shared_memory_handle = {},
      Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain = nullptr,
      bool is_back_buffer = false);

  WGPUTextureUsageFlags GetAllowedDawnUsages(
      const WGPUTextureFormat wgpu_format) const;

  gl::GLImage* GetGLImage() const;

  ID3D11Texture2D* GetOrCreateStagingTexture();

  bool UploadToGpuIfNeeded();

  // Texture could be nullptr if an empty backing is needed for testing.
  Microsoft::WRL::ComPtr<ID3D11Texture2D> d3d11_texture_;

  // Can be null for backings owned by non-GL producers e.g. WebGPU.
  scoped_refptr<gles2::TexturePassthrough> gl_texture_;

  // Holds DXGI shared handle and the keyed mutex if present.  Can be shared
  // between plane shared image backings of a multi-plane texture, or between
  // backings created from duplicated handles that refer to the same texture.
  scoped_refptr<DXGISharedHandleState> dxgi_shared_handle_state_;

  // Shared memory handle from CreateFromSharedMemoryHandle.
  gfx::GpuMemoryBufferHandle shared_memory_handle_;

  // Swap chain corresponding to this backing.
  Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain_;

  // Set if this backing corresponds to the back buffer of |swap_chain_|.
  const bool is_back_buffer_;

  // If an external image exists, it means Dawn produced the D3D12 side of the
  // D3D11 texture created by ID3D12Device::OpenSharedHandle.
#if BUILDFLAG(USE_DAWN)
  base::flat_map<WGPUDevice,
                 std::unique_ptr<dawn::native::d3d12::ExternalImageDXGI>>
      dawn_external_images_;
#endif  // BUILDFLAG(USE_DAWN)

  // Staging texture used for copy to/from shared memory GMB.
  Microsoft::WRL::ComPtr<ID3D11Texture2D> staging_texture_;

  // Tracks if we should upload from shared memory GMB to the GPU texture on the
  // next BeginAccess.
  bool needs_upload_to_gpu_ = false;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_D3D_IMAGE_BACKING_H_
