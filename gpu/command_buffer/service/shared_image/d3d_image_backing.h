// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_D3D_IMAGE_BACKING_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_D3D_IMAGE_BACKING_H_

#include <d3d11.h>
#include <dxgi1_2.h>
#include <windows.h>
#include <wrl/client.h>

#include "base/containers/flat_map.h"
#include "base/memory/scoped_refptr.h"
#include "base/types/pass_key.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "gpu/command_buffer/service/dxgi_shared_handle_manager.h"
#include "gpu/command_buffer/service/memory_tracking.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image/shared_image_manager.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/gfx/win/d3d_shared_fence.h"
#include "ui/gl/buildflags.h"
#include "ui/gl/scoped_egl_image.h"

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
  // Create a backing wrapping given D3D11 texture, optionally with a shared
  // handle and keyed mutex state. Array slice is used to specify index in
  // texture array used by video decoder and plane index is used to specify the
  // plane (Y/0 or UV/1) in NV12/P010 video textures.
  static std::unique_ptr<D3DImageBacking> Create(
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
      size_t array_slice = 0u,
      size_t plane_index = 0u);

  static std::unique_ptr<D3DImageBacking> CreateFromSwapChainBuffer(
      const Mailbox& mailbox,
      viz::SharedImageFormat format,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      GrSurfaceOrigin surface_origin,
      SkAlphaType alpha_type,
      uint32_t usage,
      Microsoft::WRL::ComPtr<ID3D11Texture2D> d3d11_texture,
      Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain,
      bool is_back_buffer);

  // Helper used by D3D11VideoDecoder to create backings directly.
  static std::vector<std::unique_ptr<SharedImageBacking>>
  CreateFromVideoTexture(
      base::span<const Mailbox> mailboxes,
      DXGI_FORMAT dxgi_format,
      const gfx::Size& size,
      uint32_t usage,
      Microsoft::WRL::ComPtr<ID3D11Texture2D> d3d11_texture,
      unsigned array_slice,
      scoped_refptr<DXGISharedHandleState> dxgi_shared_handle_state = nullptr);

  D3DImageBacking(const D3DImageBacking&) = delete;
  D3DImageBacking& operator=(const D3DImageBacking&) = delete;

  ~D3DImageBacking() override;

  // SharedImageBacking implementation.
  SharedImageBackingType GetType() const override;
  void Update(std::unique_ptr<gfx::GpuFence> in_fence) override;
  bool UploadFromMemory(const std::vector<SkPixmap>& pixmaps) override;
  bool ReadbackToMemory(const std::vector<SkPixmap>& pixmaps) override;
  bool PresentSwapChain() override;
  std::unique_ptr<DawnImageRepresentation> ProduceDawn(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker,
      const wgpu::Device& device,
      wgpu::BackendType backend_type,
      std::vector<wgpu::TextureFormat> view_formats) override;

  bool BeginAccessD3D11(Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device,
                        bool write_access);
  void EndAccessD3D11(Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device);

#if BUILDFLAG(USE_DAWN)
  wgpu::Texture BeginAccessDawn(const wgpu::Device& device,
                                wgpu::BackendType backend_type,
                                wgpu::TextureUsage usage);
  void EndAccessDawn(const wgpu::Device& device, wgpu::Texture texture);
#endif

  absl::optional<gl::DCLayerOverlayImage> GetDCLayerOverlayImage();

  bool has_keyed_mutex() const {
    return dxgi_shared_handle_state_ &&
           dxgi_shared_handle_state_->has_keyed_mutex();
  }

  scoped_refptr<DXGISharedHandleState> dxgi_shared_handle_state_for_testing()
      const {
    return dxgi_shared_handle_state_;
  }

  Microsoft::WRL::ComPtr<ID3D11Texture2D> d3d11_texture_for_testing() const {
    return d3d11_texture_;
  }

  // Holds a gles2::TexturePassthrough and corresponding egl image.
  class GLTextureHolder : public base::RefCounted<GLTextureHolder> {
   public:
    GLTextureHolder(
        base::PassKey<D3DImageBacking>,
        scoped_refptr<gles2::TexturePassthrough> texture_passthrough,
        gl::ScopedEGLImage egl_image);

    const scoped_refptr<gles2::TexturePassthrough>& texture_passthrough()
        const {
      return texture_passthrough_;
    }

    void* egl_image() const { return egl_image_.get(); }

    void MarkContextLost();

   private:
    friend class base::RefCounted<GLTextureHolder>;

    ~GLTextureHolder();

    const scoped_refptr<gles2::TexturePassthrough> texture_passthrough_;
    const gl::ScopedEGLImage egl_image_;
  };

  static scoped_refptr<GLTextureHolder> CreateGLTexture(
      viz::SharedImageFormat format,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      Microsoft::WRL::ComPtr<ID3D11Texture2D> d3d11_texture,
      GLenum texture_target = GL_TEXTURE_2D,
      unsigned array_slice = 0u,
      unsigned plane_index = 0u,
      Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain = nullptr);

  // Only for test use.
  Microsoft::WRL::ComPtr<IDXGISwapChain1> GetSwapChainForTesting() {
    return swap_chain_;
  }
  Microsoft::WRL::ComPtr<ID3D11Texture2D> GetD3D11TextureForTesting() {
    return d3d11_texture_;
  }

 protected:
  std::unique_ptr<GLTexturePassthroughImageRepresentation>
  ProduceGLTexturePassthrough(SharedImageManager* manager,
                              MemoryTypeTracker* tracker) override;

  std::unique_ptr<OverlayImageRepresentation> ProduceOverlay(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker) override;

  std::unique_ptr<SkiaGaneshImageRepresentation> ProduceSkiaGanesh(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker,
      scoped_refptr<SharedContextState> context_state) override;

  std::unique_ptr<SkiaGraphiteImageRepresentation> ProduceSkiaGraphite(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker,
      scoped_refptr<SharedContextState> context_state) override;

  std::unique_ptr<VideoDecodeImageRepresentation> ProduceVideoDecode(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker,
      VideoDecodeDevice device) override;

 private:
  D3DImageBacking(
      const Mailbox& mailbox,
      viz::SharedImageFormat format,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      GrSurfaceOrigin surface_origin,
      SkAlphaType alpha_type,
      uint32_t usage,
      Microsoft::WRL::ComPtr<ID3D11Texture2D> d3d11_texture,
      std::vector<scoped_refptr<GLTextureHolder>> gl_texture_holders,
      scoped_refptr<DXGISharedHandleState> dxgi_shared_handle_state = nullptr,
      GLenum texture_target = GL_TEXTURE_2D,
      size_t array_slice = 0u,
      size_t plane_index = 0u,
      Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain = nullptr,
      bool is_back_buffer = false);

  // Helper to retrieve internal EGLImage for WebGPU GLES compat backend.
  void* GetEGLImage() const;

  // Returns a staging texture for CPU uploads/readback, creating one if needed.
  ID3D11Texture2D* GetOrCreateStagingTexture();

  // Common state tracking for both D3D11 and Dawn access.
  bool ValidateBeginAccess(bool write_access) const;
  void BeginAccessCommon(bool write_access);
  void EndAccessCommon(scoped_refptr<gfx::D3DSharedFence> fence);

  // Get a list of fences to wait on in BeginAccessD3D11/Dawn. If the waiting
  // device is backed by D3D11 (ANGLE or Dawn), |wait_d3d11_device| can be
  // specified to skip over fences for the same device since the wait will be a
  // no-op. Similarly, |wait_dawn_device| can be provided to skip over waits on
  // fences previously signaled on the same Dawn device which are cached in
  // |dawn_signaled_fence_map_|.
  std::vector<scoped_refptr<gfx::D3DSharedFence>> GetPendingWaitFences(
      const Microsoft::WRL::ComPtr<ID3D11Device>& wait_d3d11_device,
      const wgpu::Device& wait_dawn_device,
      bool write_access);

#if BUILDFLAG(USE_DAWN)
  // Uses either DXGISharedHandleState or internal |dawn_external_image_|
  // depending on whether the texture has a shared handle or not.
  std::unique_ptr<ExternalImageDXGI>& GetDawnExternalImage(
      const wgpu::Device& device);
#endif  // BUILDFLAG(USE_DAWN)

  // Texture could be nullptr if an empty backing is needed for testing.
  Microsoft::WRL::ComPtr<ID3D11Texture2D> d3d11_texture_;

  // Can be null for backings owned by non-GL producers e.g. WebGPU.
  std::vector<scoped_refptr<GLTextureHolder>> gl_texture_holders_;

  // Holds DXGI shared handle and the keyed mutex if present.  Can be shared
  // between plane shared image backings of a multi-plane texture, or between
  // backings created from duplicated handles that refer to the same texture.
  scoped_refptr<DXGISharedHandleState> dxgi_shared_handle_state_;

  // GL texture target. Can be GL_TEXTURE_2D or GL_TEXTURE_EXTERNAL_OES.
  // TODO(sunnyps): Switch to GL_TEXTURE_2D for all cases.
  GLenum texture_target_;

  // Index of texture slice in texture array e.g. those used by video decoder.
  const size_t array_slice_;

  // Texture plane index corresponding to this image.
  const size_t plane_index_;

  // Swap chain corresponding to this backing.
  Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain_;

  // Set if this backing corresponds to the back buffer of |swap_chain_|.
  const bool is_back_buffer_;

  // Staging texture used for copy to/from shared memory GMB.
  Microsoft::WRL::ComPtr<ID3D11Texture2D> staging_texture_;

  // D3D11 device corresponding to the |d3d11_texture_| provided on creation.
  // Can be different from the ANGLE D3D11 device when using Graphite.
  Microsoft::WRL::ComPtr<ID3D11Device> texture_d3d11_device_;

  // D3D11 device used by ANGLE. Can be different from |d3d11_device_| when
  // using Graphite.
  Microsoft::WRL::ComPtr<ID3D11Device> angle_d3d11_device_;

  // D3D11 texture descriptor for |d3d11_texture_|.
  D3D11_TEXTURE2D_DESC d3d11_texture_desc_;

  // Whether the backing is being used for exclusive read-write access.
  bool in_write_access_ = false;

  // Number of concurrent readers for this backing.
  int num_readers_ = 0;

  // Fences for previous reads. These will be waited on by the subsequent write,
  // but not by reads.
  base::flat_set<scoped_refptr<gfx::D3DSharedFence>> read_fences_;

  // Fence for the previous write. These will be waited on by subsequent reads
  // and/or write.
  scoped_refptr<gfx::D3DSharedFence> write_fence_;

  // Fences used for signaling after D3D11 access. Lazily created as needed.
  // TODO(sunnyps): This doesn't need to be per D3DImageBacking. Find a better
  // place for this so that they can be shared by all backings.
  base::flat_map<Microsoft::WRL::ComPtr<ID3D11Device>,
                 scoped_refptr<gfx::D3DSharedFence>>
      d3d11_signaled_fence_map_;

#if BUILDFLAG(USE_DAWN)
  // If an external image exists, it means Dawn produced the D3D12 side of the
  // D3D11 texture created by ID3D12Device::OpenSharedHandle(). Only used if
  // the backing doesn't have a shared handle e.g. for mappable D3D11 textures.
  std::unique_ptr<ExternalImageDXGI> dawn_external_image_;

  // Signaled fence imported from Dawn at EndAccess. This can be reused if
  // D3DSharedFence::IsSameFenceAsHandle() is true for fence handle from Dawn.
  base::flat_map<WGPUDevice, scoped_refptr<gfx::D3DSharedFence>>
      dawn_signaled_fence_map_;
#endif  // BUILDFLAG(USE_DAWN)
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_D3D_IMAGE_BACKING_H_
