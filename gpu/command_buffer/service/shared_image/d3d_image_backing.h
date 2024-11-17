// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_D3D_IMAGE_BACKING_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_D3D_IMAGE_BACKING_H_

#include <windows.h>

#include <d3d11.h>
#include <d3d12.h>
#include <dcomp.h>
#include <dxgi1_2.h>
#include <wrl/client.h>

#include <array>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/waitable_event_watcher.h"
#include "base/types/pass_key.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "gpu/command_buffer/service/dxgi_shared_handle_manager.h"
#include "gpu/command_buffer/service/memory_tracking.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image/dawn_shared_texture_holder.h"
#include "gpu/command_buffer/service/shared_image/shared_image_format_service_utils.h"
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
class GPU_GLES2_EXPORT D3DImageBacking final
    : public ClearTrackingSharedImageBacking {
 public:
  // Create a backing wrapping given D3D11 texture, optionally with a shared
  // handle and keyed mutex state. Array slice is used to specify index in
  // texture array used by video decoder.
  static std::unique_ptr<D3DImageBacking> Create(
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
      bool use_update_subresource1 = false,
      bool is_thread_safe = false);

  // Creation method meant for buffer resources originating as ID3D12Resources.
  static std::unique_ptr<D3DImageBacking> CreateFromD3D12Resource(
      const Mailbox& mailbox,
      const gfx::Size& size,
      gpu::SharedImageUsageSet usage,
      std::string debug_label,
      Microsoft::WRL::ComPtr<ID3D12Resource> d3d12_resource);

  static std::unique_ptr<D3DImageBacking> CreateFromSwapChainBuffer(
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
      bool is_back_buffer);

  D3DImageBacking(const D3DImageBacking&) = delete;
  D3DImageBacking& operator=(const D3DImageBacking&) = delete;

  ~D3DImageBacking() override;

  // SharedImageBacking implementation.
  SharedImageBackingType GetType() const override;
  void Update(std::unique_ptr<gfx::GpuFence> in_fence) override;
  bool UploadFromMemory(const std::vector<SkPixmap>& pixmaps) override;
  bool ReadbackToMemory(const std::vector<SkPixmap>& pixmaps) override;
  void ReadbackToMemoryAsync(const std::vector<SkPixmap>& pixmaps,
                             base::OnceCallback<void(bool)> callback) override;
  bool PresentSwapChain() override;
  std::unique_ptr<DawnImageRepresentation> ProduceDawn(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker,
      const wgpu::Device& device,
      wgpu::BackendType backend_type,
      std::vector<wgpu::TextureFormat> view_formats,
      scoped_refptr<SharedContextState> context_state) override;
  void UpdateExternalFence(
      scoped_refptr<gfx::D3DSharedFence> external_fence) override;

  bool BeginAccessD3D11(Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device,
                        bool write_access);
  void EndAccessD3D11(Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device);

  // Get the availability fence for |dcomp_texture_|. Returns a fence if the
  // texture is soon-to-be available, meaning that the caller must wait on the
  // fence. Returns null if it would be immediately available or there is no
  // |dcomp_texture_|, meaning there is no need to wait. The return value is
  // only valid until the next DComp commit call.
  //
  // |dcomp_texture_| must not be "unavailable", i.e. attached to a DComp tree.
  scoped_refptr<gfx::D3DSharedFence>
  GetDCompTextureAvailabilityFenceForCurrentFrame() const;

  wgpu::Texture BeginAccessDawn(const wgpu::Device& device,
                                wgpu::BackendType backend_type,
                                wgpu::TextureUsage usage,
                                wgpu::TextureUsage internal_usage,
                                std::vector<wgpu::TextureFormat> view_formats);
  void EndAccessDawn(const wgpu::Device& device, wgpu::Texture texture);

  std::unique_ptr<DawnBufferRepresentation> ProduceDawnBuffer(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker,
      const wgpu::Device& device,
      wgpu::BackendType backend_type) override;
  wgpu::Buffer BeginAccessDawnBuffer(const wgpu::Device& device,
                                     wgpu::BackendType backend_type,
                                     wgpu::BufferUsage usage);
  void EndAccessDawnBuffer(const wgpu::Device& device, wgpu::Buffer buffer);

  std::optional<gl::DCLayerOverlayImage> GetDCLayerOverlayImage();

  bool has_keyed_mutex() const {
    return dxgi_shared_handle_state_ &&
           dxgi_shared_handle_state_->has_keyed_mutex();
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
    void set_needs_rebind(bool needs_rebind) { needs_rebind_ = needs_rebind; }

    bool BindEGLImageToTexture();
    void MarkContextLost();

    base::WeakPtr<GLTextureHolder> GetWeakPtr() {
      return weak_ptr_factory_.GetWeakPtr();
    }

   private:
    friend class base::RefCounted<GLTextureHolder>;

    ~GLTextureHolder();

    const scoped_refptr<gles2::TexturePassthrough> texture_passthrough_;
    const gl::ScopedEGLImage egl_image_;
    bool needs_rebind_ = false;

    base::WeakPtrFactory<GLTextureHolder> weak_ptr_factory_{this};
  };

  static scoped_refptr<GLTextureHolder> CreateGLTexture(
      const GLFormatDesc& gl_format_desc,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      Microsoft::WRL::ComPtr<ID3D11Texture2D> d3d11_texture,
      GLenum texture_target = GL_TEXTURE_2D,
      unsigned array_slice = 0u,
      unsigned plane_index = 0u,
      Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain = nullptr);

  // Only for test use.
  bool HasStagingTextureForTesting() const;
  Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain_for_testing() const {
    return swap_chain_;
  }
  Microsoft::WRL::ComPtr<ID3D11Texture2D> d3d11_texture_for_testing() const {
    AutoLock auto_lock(this);
    return d3d11_texture_;
  }
  scoped_refptr<DXGISharedHandleState> dxgi_shared_handle_state_for_testing()
      const {
    return dxgi_shared_handle_state_;
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

#if BUILDFLAG(SKIA_USE_DAWN)
  std::unique_ptr<SkiaGraphiteImageRepresentation> ProduceSkiaGraphite(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker,
      scoped_refptr<SharedContextState> context_state) override;
#endif  // BUILDFLAG(SKIA_USE_DAWN)

  std::unique_ptr<VideoImageRepresentation> ProduceVideo(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker,
      VideoDevice device) override;

 private:
  using D3DSharedFenceSet = base::flat_set<scoped_refptr<gfx::D3DSharedFence>>;
  D3DImageBacking(const Mailbox& mailbox,
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
                  GLenum texture_target = GL_TEXTURE_2D,
                  size_t array_slice = 0u,
                  Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain = nullptr,
                  bool is_back_buffer = false,
                  bool use_update_subresource1 = false,
                  bool is_thread_safe = false);

  D3DImageBacking(const Mailbox& mailbox,
                  const gfx::Size& size,
                  gpu::SharedImageUsageSet usage,
                  std::string debug_label,
                  Microsoft::WRL::ComPtr<ID3D12Resource> d3d12_resource);

  bool use_cross_device_fence_synchronization() const {
    // Fences are needed if we're sharing between devices and there's no keyed
    // mutex for synchronization.
    return dxgi_shared_handle_state_ &&
           !dxgi_shared_handle_state_->has_keyed_mutex();
  }

  // Helper to retrieve internal EGLImage for WebGPU GLES compat backend.
  void* GetEGLImage() const;

  // Returns a staging texture for CPU uploads/readback, creating one if needed.
  ID3D11Texture2D* GetOrCreateStagingTexture() EXCLUSIVE_LOCKS_REQUIRED(lock_);

  bool CopyToStagingTexture() EXCLUSIVE_LOCKS_REQUIRED(lock_);
  bool ReadbackFromStagingTexture(const std::vector<SkPixmap>& pixmaps)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);

  void OnCopyToStagingTextureDone(const std::vector<SkPixmap>& pixmaps,
                                  base::OnceCallback<void(bool)> readback_cb);

  // Common state tracking for both D3D11 and Dawn access.
  bool ValidateBeginAccess(bool write_access) const
      EXCLUSIVE_LOCKS_REQUIRED(lock_);
  void BeginAccessCommon(bool write_access) EXCLUSIVE_LOCKS_REQUIRED(lock_);
  void EndAccessCommon(const D3DSharedFenceSet& fences)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // Get a list of fences to wait on in BeginAccessD3D11/Dawn. If the waiting
  // device is backed by D3D11 (ANGLE or Dawn), |wait_d3d11_device| can be
  // specified to skip over fences for the same device since the wait will be a
  // no-op. Similarly, |wait_dawn_device| can be provided to skip over waits on
  // fences previously signaled on the same Dawn device which are cached in
  // |dawn_signaled_fence_map_|.
  std::vector<scoped_refptr<gfx::D3DSharedFence>> GetPendingWaitFences(
      const Microsoft::WRL::ComPtr<ID3D11Device>& wait_d3d11_device,
      const wgpu::Device& wait_dawn_device,
      bool write_access) EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // Uses either DXGISharedHandleState or internal |dawn_shared_texture_holder_|
  // depending on whether the texture has a shared handle or not.
  wgpu::SharedTextureMemory GetSharedTextureMemory(const wgpu::Device& device)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // Returns the number of ongoing accesses that were already present on this
  // texture prior to beginning this access.
  int TrackBeginAccessToWGPUTexture(wgpu::Texture texture)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // Returns the number of ongoing accesses that will still be present on this
  // texture after ending this access.
  int TrackEndAccessToWGPUTexture(wgpu::Texture texture)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // Texture could be nullptr if an empty backing is needed for testing.
  const Microsoft::WRL::ComPtr<ID3D11Texture2D> d3d11_texture_;

  // Set if this backing is used for a D3D12 resource, otherwise will be
  // nullptr.
  const Microsoft::WRL::ComPtr<ID3D12Resource> d3d12_resource_;

  // Set if this backing was used for |DCompTextureOverlayImageRepresentation|.
  // Once set, this is cached and reused for future overlay representations.
  const Microsoft::WRL::ComPtr<IDCompositionTexture> dcomp_texture_;

  // Holds DXGI shared handle and the keyed mutex if present.  Can be shared
  // between plane shared image backings of a multi-plane texture, or between
  // backings created from duplicated handles that refer to the same texture.
  const scoped_refptr<DXGISharedHandleState> dxgi_shared_handle_state_;

  // Capabilities needed for getting the correct GL format for creating GL
  // textures.
  const GLFormatCaps gl_format_caps_;

  // Weak pointers for gl textures which are owned by GL texture representation.
  std::array<base::WeakPtr<GLTextureHolder>, 3> gl_texture_holders_
      GUARDED_BY(lock_);

  // GL texture target. Can be GL_TEXTURE_2D or GL_TEXTURE_EXTERNAL_OES.
  // TODO(sunnyps): Switch to GL_TEXTURE_2D for all cases.
  const GLenum texture_target_;

  // Index of texture slice in texture array e.g. those used by video decoder.
  const size_t array_slice_;

  // Swap chain corresponding to this backing.
  const Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain_;

  // Set if this backing corresponds to the back buffer of |swap_chain_|.
  const bool is_back_buffer_;

  // True if using UpdateSubresource1() in UploadFromMemory() is allowed.
  const bool use_update_subresource1_;

  // Staging texture used for copy to/from shared memory GMB.
  Microsoft::WRL::ComPtr<ID3D11Texture2D> staging_texture_ GUARDED_BY(lock_);

  // D3D11 device corresponding to the |d3d11_texture_| provided on creation.
  // Can be different from the ANGLE D3D11 device when using Graphite.
  Microsoft::WRL::ComPtr<ID3D11Device> texture_d3d11_device_;

  // D3D11 device used by ANGLE. Can be different from |d3d11_device_| when
  // using Graphite.
  Microsoft::WRL::ComPtr<ID3D11Device> angle_d3d11_device_;

  // D3D11 texture descriptor for |d3d11_texture_|.
  D3D11_TEXTURE2D_DESC d3d11_texture_desc_;

  // Whether the backing is being used for exclusive read-write access.
  bool in_write_access_ GUARDED_BY(lock_) = false;

  // Number of concurrent readers for this backing.
  int num_readers_ GUARDED_BY(lock_) = 0;

  // Fences for previous reads. These will be waited on by the subsequent write,
  // but not by reads.
  D3DSharedFenceSet read_fences_ GUARDED_BY(lock_);

  // Fences for the previous write. These will be waited on by subsequent reads
  // and/or write.
  D3DSharedFenceSet write_fences_ GUARDED_BY(lock_);

  // Fences used for signaling after D3D11 access. Lazily created as needed.
  // TODO(sunnyps): This doesn't need to be per D3DImageBacking. Find a better
  // place for this so that they can be shared by all backings.
  base::flat_map<Microsoft::WRL::ComPtr<ID3D11Device>,
                 scoped_refptr<gfx::D3DSharedFence>>
      d3d11_signaled_fence_map_ GUARDED_BY(lock_);

  // DawnSharedTextureHolder that keeps an internal cache of per-device
  // SharedTextureData that vends WebGPU textures for the underlying d3d
  // texture. Only used if the backing doesn't have a shared handle.
  DawnSharedTextureHolder dawn_shared_texture_holder_ GUARDED_BY(lock_);

  // Dawn SharedBufferMemory will exist when backing is being used for buffer
  // interop.
  wgpu::SharedBufferMemory dawn_shared_buffer_memory_ GUARDED_BY(lock_);

  // TODO(crbug.com/348598119, hitawala): Move texture begin/end access tracking
  // to DawnSharedTextureHolder. Tracks the number of currently-ongoing accesses
  // to a given WGPU texture.
  base::flat_map<WGPUTexture, int> wgpu_texture_ongoing_accesses_
      GUARDED_BY(lock_);

  // Signaled fences imported from Dawn at EndAccess. This can be reused if
  // D3DSharedFence::IsSameFenceAsHandle() is true for fence handle from Dawn.
  base::flat_map<WGPUDevice, D3DSharedFenceSet> dawn_signaled_fences_map_
      GUARDED_BY(lock_);

  std::optional<base::WaitableEventWatcher> pending_copy_event_watcher_
      GUARDED_BY(lock_);

  base::WeakPtrFactory<D3DImageBacking> weak_ptr_factory_{this};
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_D3D_IMAGE_BACKING_H_
