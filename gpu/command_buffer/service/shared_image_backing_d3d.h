// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_BACKING_D3D_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_BACKING_D3D_H_

#include <d3d11.h>
#include <dxgi1_2.h>
#include <windows.h>
#include <wrl/client.h>

#include "base/macros.h"
#include "components/viz/common/resources/resource_format.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "gpu/command_buffer/service/memory_tracking.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image_manager.h"
#include "gpu/command_buffer/service/shared_image_representation.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "ui/gl/buildflags.h"
#include "ui/gl/gl_image_d3d.h"

// Usage of BUILDFLAG(USE_DAWN) needs to be after the include for
// ui/gl/buildflags.h
#if BUILDFLAG(USE_DAWN)
#include <dawn_native/D3D12Backend.h>
#endif  // BUILDFLAG(USE_DAWN)

namespace gfx {
class Size;
class ColorSpace;
}  // namespace gfx

namespace gpu {
class SharedImageBacking;
struct Mailbox;

// Implementation of SharedImageBacking that holds buffer (front buffer/back
// buffer of swap chain) texture (as gles2::Texture/gles2::TexturePassthrough)
// and a reference to created swap chain.
class GPU_GLES2_EXPORT SharedImageBackingD3D
    : public ClearTrackingSharedImageBacking {
 public:
  static std::unique_ptr<SharedImageBackingD3D> CreateFromSwapChainBuffer(
      const Mailbox& mailbox,
      viz::ResourceFormat format,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      GrSurfaceOrigin surface_origin,
      SkAlphaType alpha_type,
      uint32_t usage,
      Microsoft::WRL::ComPtr<ID3D11Texture2D> d3d11_texture,
      Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain,
      size_t buffer_index);

  static std::unique_ptr<SharedImageBackingD3D> CreateFromSharedHandle(
      const Mailbox& mailbox,
      viz::ResourceFormat format,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      GrSurfaceOrigin surface_origin,
      SkAlphaType alpha_type,
      uint32_t usage,
      Microsoft::WRL::ComPtr<ID3D11Texture2D> d3d11_texture,
      base::win::ScopedHandle shared_handle);

  // TODO(sunnyps): Remove this after migrating DXVA decoder to EGLImage.
  static std::unique_ptr<SharedImageBackingD3D> CreateFromGLTexture(
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
      base::win::ScopedHandle shared_handle = base::win::ScopedHandle());

  ~SharedImageBackingD3D() override;

  void Update(std::unique_ptr<gfx::GpuFence> in_fence) override;

  bool ProduceLegacyMailbox(MailboxManager* mailbox_manager) override;

  std::unique_ptr<SharedImageRepresentationDawn> ProduceDawn(
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

  HANDLE GetSharedHandle() const;
  gl::GLImage* GetGLImage() const;

  bool PresentSwapChain() override;

 protected:
  std::unique_ptr<SharedImageRepresentationGLTexturePassthrough>
  ProduceGLTexturePassthrough(SharedImageManager* manager,
                              MemoryTypeTracker* tracker) override;

  std::unique_ptr<SharedImageRepresentationOverlay> ProduceOverlay(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker) override;

  std::unique_ptr<SharedImageRepresentationSkia> ProduceSkia(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker,
      scoped_refptr<SharedContextState> context_state) override;

 private:
  class SharedState : public base::RefCountedThreadSafe<SharedState> {
   public:
    explicit SharedState(
        base::win::ScopedHandle shared_handle = base::win::ScopedHandle(),
        Microsoft::WRL::ComPtr<IDXGIKeyedMutex> dxgi_keyed_mutex = nullptr);

    bool BeginAccessD3D11();
    void EndAccessD3D11();

    bool BeginAccessD3D12();
    void EndAccessD3D12();

    HANDLE GetSharedHandle() const;

   private:
    friend class base::RefCountedThreadSafe<SharedState>;
    ~SharedState();

    // If |d3d11_texture_| has a keyed mutex, it will be stored in
    // |dxgi_keyed_mutex_|. The keyed mutex is used to synchronize D3D11 and
    // D3D12 Chromium components. |dxgi_keyed_mutex_| is the D3D11 side of the
    // keyed mutex. To create the corresponding D3D12 interface, pass the handle
    // stored in |shared_handle_| to ID3D12Device::OpenSharedHandle. Only one
    // component is allowed to read/write to the texture at a time.
    base::win::ScopedHandle shared_handle_;
    Microsoft::WRL::ComPtr<IDXGIKeyedMutex> dxgi_keyed_mutex_;
    bool acquired_for_d3d12_ = false;
    int acquired_for_d3d11_count_ = 0;
  };

  SharedImageBackingD3D(
      const Mailbox& mailbox,
      viz::ResourceFormat format,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      GrSurfaceOrigin surface_origin,
      SkAlphaType alpha_type,
      uint32_t usage,
      Microsoft::WRL::ComPtr<ID3D11Texture2D> d3d11_texture,
      scoped_refptr<gles2::TexturePassthrough> gl_texture,
      Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain = nullptr,
      size_t buffer_index = 0,
      scoped_refptr<SharedState> shared_state =
          base::MakeRefCounted<SharedState>());

  uint32_t GetAllowedDawnUsages() const;

  // Texture could be nullptr if an empty backing is needed for testing.
  Microsoft::WRL::ComPtr<ID3D11Texture2D> d3d11_texture_;

  // Can be null for backings owned by non-GL producers e.g. WebGPU.
  scoped_refptr<gles2::TexturePassthrough> gl_texture_;

  Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain_;

  const size_t buffer_index_;

  scoped_refptr<SharedState> shared_state_;

  // If external_image_ exists, it means Dawn produced the D3D12 side of the
  // D3D11 texture created by ID3D12Device::OpenSharedHandle.
#if BUILDFLAG(USE_DAWN)
  std::unique_ptr<dawn_native::d3d12::ExternalImageDXGI> external_image_;
#endif  // BUILDFLAG(USE_DAWN)

  DISALLOW_COPY_AND_ASSIGN(SharedImageBackingD3D);
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_BACKING_D3D_H_
