// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_FACTORY_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_FACTORY_H_

#include <memory>

#include "base/containers/flat_set.h"
#include "base/memory/scoped_refptr.h"
#include "build/build_config.h"
#include "components/viz/common/resources/resource_format.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/service/shared_image_manager.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "gpu/gpu_gles2_export.h"
#include "gpu/ipc/common/surface_handle.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/gl/gl_bindings.h"

namespace viz {
class VulkanContextProvider;
}  // namespace viz

namespace gpu {
class GpuDriverBugWorkarounds;
class ImageFactory;
class MailboxManager;
class MemoryTracker;
class SharedContextState;
class SharedImageBackingFactory;
class SharedImageBackingFactoryGLTexture;
struct GpuFeatureInfo;
struct GpuPreferences;

#if defined(OS_FUCHSIA)
class SysmemBufferCollection;
#endif  // OS_FUCHSIA

namespace raster {
class WrappedSkImageFactory;
}  // namespace raster

// TODO(ericrk): Make this a very thin wrapper around SharedImageManager like
// SharedImageRepresentationFactory.
class GPU_GLES2_EXPORT SharedImageFactory {
 public:
  SharedImageFactory(const GpuPreferences& gpu_preferences,
                     const GpuDriverBugWorkarounds& workarounds,
                     const GpuFeatureInfo& gpu_feature_info,
                     SharedContextState* context_state,
                     MailboxManager* mailbox_manager,
                     SharedImageManager* manager,
                     ImageFactory* image_factory,
                     MemoryTracker* tracker,
                     bool enable_wrapped_sk_image);
  ~SharedImageFactory();

  bool CreateSharedImage(const Mailbox& mailbox,
                         viz::ResourceFormat format,
                         const gfx::Size& size,
                         const gfx::ColorSpace& color_space,
                         uint32_t usage);
  bool CreateSharedImage(const Mailbox& mailbox,
                         viz::ResourceFormat format,
                         const gfx::Size& size,
                         const gfx::ColorSpace& color_space,
                         uint32_t usage,
                         base::span<const uint8_t> pixel_data);
  bool CreateSharedImage(const Mailbox& mailbox,
                         int client_id,
                         gfx::GpuMemoryBufferHandle handle,
                         gfx::BufferFormat format,
                         SurfaceHandle surface_handle,
                         const gfx::Size& size,
                         const gfx::ColorSpace& color_space,
                         uint32_t usage);
  bool UpdateSharedImage(const Mailbox& mailbox);
  bool UpdateSharedImage(const Mailbox& mailbox,
                         std::unique_ptr<gfx::GpuFence> in_fence);
  bool DestroySharedImage(const Mailbox& mailbox);
  bool HasImages() const { return !shared_images_.empty(); }
  void DestroyAllSharedImages(bool have_context);

#if defined(OS_WIN)
  bool CreateSwapChain(const Mailbox& front_buffer_mailbox,
                       const Mailbox& back_buffer_mailbox,
                       viz::ResourceFormat format,
                       const gfx::Size& size,
                       const gfx::ColorSpace& color_space,
                       uint32_t usage);
  bool PresentSwapChain(const Mailbox& mailbox);
#endif  // OS_WIN

#if defined(OS_FUCHSIA)
  bool RegisterSysmemBufferCollection(gfx::SysmemBufferCollectionId id,
                                      zx::channel token);
  bool ReleaseSysmemBufferCollection(gfx::SysmemBufferCollectionId id);
#endif  // defined(OS_FUCHSIA)

  bool OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                    base::trace_event::ProcessMemoryDump* pmd,
                    int client_id,
                    uint64_t client_tracing_id);
  bool RegisterBacking(std::unique_ptr<SharedImageBacking> backing,
                       bool allow_legacy_mailbox);

  void RegisterSharedImageBackingFactoryForTesting(
      SharedImageBackingFactory* factory);

 private:
  bool IsSharedBetweenThreads(uint32_t usage);
  SharedImageBackingFactory* GetFactoryByUsage(
      uint32_t usage,
      bool* allow_legacy_mailbox,
      gfx::GpuMemoryBufferType gmb_type = gfx::EMPTY_BUFFER);
  MailboxManager* mailbox_manager_;
  SharedImageManager* shared_image_manager_;
  std::unique_ptr<MemoryTypeTracker> memory_tracker_;
  const bool using_vulkan_;
  const bool using_metal_;
  const bool using_dawn_;

  // The set of SharedImages which have been created (and are being kept alive)
  // by this factory.
  base::flat_set<std::unique_ptr<SharedImageRepresentationFactoryRef>>
      shared_images_;

  // TODO(ericrk): This should be some sort of map from usage to factory
  // eventually.
  std::unique_ptr<SharedImageBackingFactoryGLTexture> gl_backing_factory_;

  // Used for creating shared image which can be shared between GL, Vulkan and
  // D3D12.
  std::unique_ptr<SharedImageBackingFactory> interop_backing_factory_;

  // Non-null if compositing with SkiaRenderer.
  std::unique_ptr<raster::WrappedSkImageFactory> wrapped_sk_image_factory_;

#if defined(OS_FUCHSIA)
  viz::VulkanContextProvider* vulkan_context_provider_;
  base::flat_map<gfx::SysmemBufferCollectionId,
                 std::unique_ptr<gpu::SysmemBufferCollection>>
      buffer_collections_;
#endif  // OS_FUCHSIA

  SharedImageBackingFactory* backing_factory_for_testing_ = nullptr;
};

class GPU_GLES2_EXPORT SharedImageRepresentationFactory {
 public:
  SharedImageRepresentationFactory(SharedImageManager* manager,
                                   MemoryTracker* tracker);
  ~SharedImageRepresentationFactory();

  // Helpers which call similar classes on SharedImageManager, providing a
  // MemoryTypeTracker.
  std::unique_ptr<SharedImageRepresentationGLTexture> ProduceGLTexture(
      const Mailbox& mailbox);
  std::unique_ptr<SharedImageRepresentationGLTexture>
  ProduceRGBEmulationGLTexture(const Mailbox& mailbox);
  std::unique_ptr<SharedImageRepresentationGLTexturePassthrough>
  ProduceGLTexturePassthrough(const Mailbox& mailbox);
  std::unique_ptr<SharedImageRepresentationSkia> ProduceSkia(
      const Mailbox& mailbox,
      scoped_refptr<SharedContextState> context_State);
  std::unique_ptr<SharedImageRepresentationDawn> ProduceDawn(
      const Mailbox& mailbox,
      WGPUDevice device);
  std::unique_ptr<SharedImageRepresentationOverlay> ProduceOverlay(
      const Mailbox& mailbox);

 private:
  SharedImageManager* const manager_;
  std::unique_ptr<MemoryTypeTracker> tracker_;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_FACTORY_H_
