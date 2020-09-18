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
#include "ui/gl/gl_image_d3d.h"

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
  SharedImageBackingD3D(
      const Mailbox& mailbox,
      viz::ResourceFormat format,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      GrSurfaceOrigin surface_origin,
      SkAlphaType alpha_type,
      uint32_t usage,
      Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain,
      scoped_refptr<gles2::TexturePassthrough> texture,
      scoped_refptr<gl::GLImage> image,
      size_t buffer_index,
      Microsoft::WRL::ComPtr<ID3D11Texture2D> d3d11_texture,
      base::win::ScopedHandle shared_handle,
      Microsoft::WRL::ComPtr<IDXGIKeyedMutex> dxgi_keyed_mutex);

  ~SharedImageBackingD3D() override;

  void Update(std::unique_ptr<gfx::GpuFence> in_fence) override;

  bool ProduceLegacyMailbox(MailboxManager* mailbox_manager) override;

  std::unique_ptr<SharedImageRepresentationDawn> ProduceDawn(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker,
      WGPUDevice device) override;

  void OnMemoryDump(const std::string& dump_name,
                    base::trace_event::MemoryAllocatorDump* dump,
                    base::trace_event::ProcessMemoryDump* pmd,
                    uint64_t client_tracing_id) override;

  bool BeginAccessD3D12(uint64_t* acquire_key);
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
  Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain_;
  scoped_refptr<gles2::TexturePassthrough> texture_;
  scoped_refptr<gl::GLImage> image_;
  const size_t buffer_index_;

  // Texture could be nullptr if an empty backing is needed for testing.
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

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_BACKING_D3D_H_
