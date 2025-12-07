// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_SHARED_IMAGE_FACTORY_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_SHARED_IMAGE_FACTORY_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/shared_image_capabilities.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/shared_image/shared_image_manager.h"
#include "gpu/command_buffer/service/shared_image/shared_image_pool_service.h"
#include "gpu/config/gpu_driver_bug_workarounds.h"
#include "gpu/config/gpu_preferences.h"
#include "gpu/gpu_gles2_export.h"
#include "gpu/ipc/common/shared_image_pool_client_interface.mojom.h"
#include "gpu/ipc/common/surface_handle.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/gpu_extra_info.h"
#include "ui/gfx/gpu_memory_buffer_handle.h"
#include "ui/gl/gl_bindings.h"

namespace gpu {
class MemoryTracker;
class SharedContextState;
class SharedImageBackingFactory;
class SharedImageCopyManager;
class D3DImageBackingFactory;
struct GpuFeatureInfo;
struct GpuPreferences;

#if BUILDFLAG(IS_ANDROID)
class AHardwareBufferImageBackingFactory;
#endif

class GPU_GLES2_EXPORT SharedImageFactory {
 public:
  // All objects passed are expected to outlive this class.
  SharedImageFactory(const GpuPreferences& gpu_preferences,
                     const GpuDriverBugWorkarounds& workarounds,
                     const GpuFeatureInfo& gpu_feature_info,
                     SharedContextState* context_state,
                     SharedImageManager* manager,
                     scoped_refptr<MemoryTracker> memory_tracker,
                     bool is_for_display_compositor);
  ~SharedImageFactory();

  bool CreateSharedImage(
      const Mailbox& mailbox,
      viz::SharedImageFormat si_format,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      GrSurfaceOrigin surface_origin,
      SkAlphaType alpha_type,
      SurfaceHandle surface_handle,
      SharedImageUsageSet usage,
      std::string debug_label,
      std::optional<SharedImagePoolId> pool_id = std::nullopt);
  bool CreateSharedImage(const Mailbox& mailbox,
                         viz::SharedImageFormat si_format,
                         const gfx::Size& size,
                         const gfx::ColorSpace& color_space,
                         GrSurfaceOrigin surface_origin,
                         SkAlphaType alpha_type,
                         SurfaceHandle surface_handle,
                         SharedImageUsageSet usage,
                         std::string debug_label,
                         gfx::BufferUsage buffer_usage);
  bool CreateSharedImage(const Mailbox& mailbox,
                         viz::SharedImageFormat si_format,
                         const gfx::Size& size,
                         const gfx::ColorSpace& color_space,
                         GrSurfaceOrigin surface_origin,
                         SkAlphaType alpha_type,
                         SharedImageUsageSet usage,
                         std::string debug_label,
                         base::span<const uint8_t> pixel_data);
  bool CreateSharedImage(
      const Mailbox& mailbox,
      viz::SharedImageFormat si_format,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      GrSurfaceOrigin surface_origin,
      SkAlphaType alpha_type,
      SharedImageUsageSet usage,
      std::string debug_label,
      gfx::GpuMemoryBufferHandle buffer_handle,
      std::optional<SharedImagePoolId> pool_id = std::nullopt);
  bool UpdateSharedImage(const Mailbox& mailbox);
  bool UpdateSharedImage(const Mailbox& mailbox,
                         std::unique_ptr<gfx::GpuFence> in_fence);
  bool DestroySharedImage(const Mailbox& mailbox);
  bool SetSharedImagePurgeable(const Mailbox& mailbox, bool purgeable);
  bool HasImages() const { return !shared_images_.empty(); }
  void DestroyAllSharedImages(bool have_context);

#if BUILDFLAG(IS_FUCHSIA)
  void RegisterSysmemBufferCollection(zx::eventpair service_handle,
                                      zx::channel sysmem_token,
                                      const viz::SharedImageFormat& format,
                                      gfx::BufferUsage usage,
                                      bool register_with_image_pipe);
#endif  // BUILDFLAG(IS_FUCHSIA)

  bool RegisterBacking(std::unique_ptr<SharedImageBacking> backing,
                       std::optional<SharedImagePoolId> pool_id = std::nullopt);
  bool AddSecondaryReference(const gpu::Mailbox& mailbox);

  // Returns the usage for the shared image backing. If no backing is registered
  // for `mailbox` this will return 0. This can only get usages for mailboxes
  // registered on this factory. If you need to query all mailboxes use
  // |SharedImageManager::GetUsageForMailbox|.
  SharedImageUsageSet GetUsageForMailbox(const Mailbox& mailbox);

  bool CopyToGpuMemoryBuffer(const Mailbox& mailbox);

  // Creation of native buffer handles is not supported on Android (the
  // only way that a non-null GpuMemoryBufferHandle can be created on
  // Android is by importing an external AHB).
#if !BUILDFLAG(IS_ANDROID)
  // Creates a native GpuMemoryBufferHandle for MappableSI.
  gfx::GpuMemoryBufferHandle CreateNativeGpuMemoryBufferHandle(
      const gfx::Size& size,
      viz::SharedImageFormat format,
      gfx::BufferUsage usage);
#endif

  // Fills |shared_memory| with the contents of the provided
  // |buffer_handle|. Returns whether the operation succeeded.
  bool CopyNativeBufferToSharedMemoryAsync(
      gfx::GpuMemoryBufferHandle buffer_handle,
      base::UnsafeSharedMemoryRegion shared_memory);
#if BUILDFLAG(IS_WIN)
  bool CopyToGpuMemoryBufferAsync(const Mailbox& mailbox,
                                  base::OnceCallback<void(bool)> callback);
#endif

  void SetGpuExtraInfo(const gfx::GpuExtraInfo& gpu_info);
  bool GetGpuMemoryBufferHandleInfo(const Mailbox& mailbox,
                                    gfx::GpuMemoryBufferHandle& handle,
                                    gfx::BufferUsage& buffer_usage);

  bool CreateSharedImagePool(
      const SharedImagePoolId& pool_id,
      mojo::PendingRemote<mojom::SharedImagePoolClientInterface> client_remote);
  bool DestroySharedImagePool(const SharedImagePoolId& pool_id);

  void RegisterSharedImageBackingFactoryForTesting(
      SharedImageBackingFactory* factory);

  gpu::SharedImageCapabilities MakeCapabilities();

  bool HasSharedImage(const Mailbox& mailbox) const;

  SharedContextState* shared_context_state() { return context_state_.get(); }
  const scoped_refptr<SharedImageCopyManager>& copy_manager();

  static bool IsNativeBufferSupported(viz::SharedImageFormat format,
                                      gfx::BufferUsage usage,
                                      const gfx::GpuExtraInfo& gpu_extra_info);

  base::WeakPtr<SharedImageFactory> GetWeakPtr();

 private:
  friend class CompoundImageBacking;

  bool IsSharedBetweenThreads(gpu::SharedImageUsageSet usage);

  SharedImageRepresentationFactoryRef* GetFactoryRef(
      const Mailbox& mailbox) const;

  SharedImageBackingFactory* GetFactoryByUsage(
      SharedImageUsageSet usage,
      viz::SharedImageFormat format,
      const gfx::Size& size,
      base::span<const uint8_t> pixel_data,
      gfx::GpuMemoryBufferType gmb_type);
  void LogGetFactoryFailed(gpu::SharedImageUsageSet usage,
                           viz::SharedImageFormat format,
                           gfx::GpuMemoryBufferType gmb_type,
                           const gfx::Size& size,
                           const std::string& debug_label);
#if BUILDFLAG(IS_WIN)
  bool IsD3DSharedImageSupported() const;
#endif

  raw_ptr<SharedImageManager> shared_image_manager_;
  const scoped_refptr<SharedContextState> context_state_;
  std::unique_ptr<MemoryTypeTracker> memory_type_tracker_;
  scoped_refptr<SharedImageCopyManager> copy_manager_;

  // This is used if the factory is created on display compositor to check for
  // sharing between threads.
  const bool is_for_display_compositor_;

  // This is |context_state_|'s GrContextType or GrContextType::kNone if there
  // is no shared context.
  const GrContextType gr_context_type_;

  // The set of SharedImages which have been created (and are being kept alive)
  // by this factory.
  absl::flat_hash_map<gpu::Mailbox,
                      std::unique_ptr<SharedImageRepresentationFactoryRef>>
      shared_images_;

  // Map of all the SharedImagePoolService objects corresponding to its unique
  // pool id.
  absl::flat_hash_map<SharedImagePoolId,
                      std::unique_ptr<SharedImagePoolService>>
      shared_image_pool_map_;

  // Array of all the backing factories to choose from for creating shared
  // images.
  std::vector<std::unique_ptr<SharedImageBackingFactory>> factories_;

#if BUILDFLAG(IS_WIN)
  // Used for creating swap chains
  raw_ptr<D3DImageBackingFactory> d3d_backing_factory_ = nullptr;
#endif

  gfx::GpuExtraInfo gpu_extra_info_;
  gpu::GpuPreferences gpu_preferences_;
#if BUILDFLAG(IS_MAC)
  uint32_t texture_target_for_io_surfaces_;
#endif
  gpu::GpuDriverBugWorkarounds workarounds_;

#if BUILDFLAG(IS_ANDROID)
  raw_ptr<AHardwareBufferImageBackingFactory> ahb_factory_ = nullptr;
#endif

  raw_ptr<SharedImageBackingFactory> backing_factory_for_testing_ = nullptr;
  base::WeakPtrFactory<SharedImageFactory> weak_ptr_factory_{this};
};

class GPU_GLES2_EXPORT SharedImageRepresentationFactory {
 public:
  // All arguments must outlive this object.
  SharedImageRepresentationFactory(SharedImageManager* manager,
                                   scoped_refptr<MemoryTracker> memory_tracker);
  ~SharedImageRepresentationFactory();

  // Helpers which call similar classes on SharedImageManager, providing a
  // MemoryTypeTracker.
  // NOTE: This object *must* outlive all objects created via the below methods,
  // as the |memory_type_tracker_| instance variable that it supplies to them is
  // used in their destruction process.
  std::unique_ptr<GLTextureImageRepresentation> ProduceGLTexture(
      const Mailbox& mailbox);
  std::unique_ptr<GLTexturePassthroughImageRepresentation>
  ProduceGLTexturePassthrough(const Mailbox& mailbox);
  std::unique_ptr<SkiaImageRepresentation> ProduceSkia(
      const Mailbox& mailbox,
      scoped_refptr<SharedContextState> context_State);
  std::unique_ptr<DawnImageRepresentation> ProduceDawn(
      const Mailbox& mailbox,
      const wgpu::Device& device,
      wgpu::BackendType backend_type,
      std::vector<wgpu::TextureFormat> view_formats,
      scoped_refptr<SharedContextState> context_state);
  std::unique_ptr<DawnBufferRepresentation> ProduceDawnBuffer(
      const Mailbox& mailbox,
      const wgpu::Device& device,
      wgpu::BackendType backend_type,
      scoped_refptr<SharedContextState> context_state);
  std::unique_ptr<WebNNTensorRepresentation> ProduceWebNNTensor(
      const Mailbox& mailbox);
  std::unique_ptr<OverlayImageRepresentation> ProduceOverlay(
      const Mailbox& mailbox);
  std::unique_ptr<MemoryImageRepresentation> ProduceMemory(
      const Mailbox& mailbox);
  std::unique_ptr<RasterImageRepresentation> ProduceRaster(
      const Mailbox& mailbox);

#if BUILDFLAG(IS_ANDROID)
  std::unique_ptr<LegacyOverlayImageRepresentation> ProduceLegacyOverlay(
      const Mailbox& mailbox);
#endif

#if BUILDFLAG(ENABLE_VULKAN) && BUILDFLAG(IS_OZONE)
  std::unique_ptr<VulkanImageRepresentation> ProduceVulkan(
      const Mailbox& mailbox,
      gpu::VulkanDeviceQueue* vulkan_device_queue,
      gpu::VulkanImplementation& vulkan_impl,
      bool needs_detiling);
#endif

 private:
  const raw_ptr<SharedImageManager> manager_;
  std::unique_ptr<MemoryTypeTracker> memory_type_tracker_;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_SHARED_IMAGE_FACTORY_H_
