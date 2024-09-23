// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_SHARED_IMAGE_MANAGER_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_SHARED_IMAGE_MANAGER_H_

#include <optional>

#include "base/containers/flat_set.h"
#include "base/memory/scoped_refptr.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_checker.h"
#include "base/trace_event/memory_dump_provider.h"
#include "build/build_config.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/shared_image/shared_image_backing.h"
#include "gpu/gpu_gles2_export.h"
#include "gpu/vulkan/buildflags.h"

#if BUILDFLAG(IS_WIN)
namespace gfx {
class D3DSharedFence;
}
#endif

namespace gpu {
class DXGISharedHandleManager;
class SharedImageRepresentationFactoryRef;

class GPU_GLES2_EXPORT SharedImageManager
    : public base::trace_event::MemoryDumpProvider {
 public:
  // If |thread_safe| is set, the manager itself can be safely accessed from
  // other threads but the backings themselves may not be thread-safe so
  // representations should not be created on other threads. When
  // |display_context_on_another_thread| is set, we make sure that all
  // SharedImages that will be used in the display context have thread-safe
  // backings and therefore it is safe to create representations on the thread
  // that holds the display context.
  explicit SharedImageManager(bool thread_safe = false,
                              bool display_context_on_another_thread = false);

  SharedImageManager(const SharedImageManager&) = delete;
  SharedImageManager& operator=(const SharedImageManager&) = delete;

  ~SharedImageManager() override;

  // base::trace_event::MemoryDumpProvider implementation:
  bool OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                    base::trace_event::ProcessMemoryDump* pmd) override;

  // Registers a SharedImageBacking with the manager and returns a
  // SharedImageRepresentationFactoryRef which holds a ref on the SharedImage.
  // The factory should delete this object to release the ref.
  std::unique_ptr<SharedImageRepresentationFactoryRef> Register(
      std::unique_ptr<SharedImageBacking> backing,
      MemoryTypeTracker* ref);

  std::unique_ptr<SharedImageRepresentationFactoryRef> AddSecondaryReference(
      const Mailbox& mailbox,
      MemoryTypeTracker* tracker);

  // Accessors which return a SharedImageRepresentation. Representations also
  // take a ref on the mailbox, releasing it when the representation is
  // destroyed.
  std::unique_ptr<GLTextureImageRepresentation> ProduceGLTexture(
      const Mailbox& mailbox,
      MemoryTypeTracker* ref);
  std::unique_ptr<GLTexturePassthroughImageRepresentation>
  ProduceGLTexturePassthrough(const Mailbox& mailbox, MemoryTypeTracker* ref);
  std::unique_ptr<SkiaImageRepresentation> ProduceSkia(
      const Mailbox& mailbox,
      MemoryTypeTracker* ref,
      scoped_refptr<SharedContextState> context_state);

  // ProduceDawn must also be called using same |device| if
  // using the same |mailbox|. This is because the underlying shared image
  // compatibility also depends on the WGPUAdapter which ProduceDawn does not
  // associate with the representation.
  // TODO(crbug.com/40730564): Revisit this in the future for WebGPU
  // multi-adapter support.
  std::unique_ptr<DawnImageRepresentation> ProduceDawn(
      const Mailbox& mailbox,
      MemoryTypeTracker* ref,
      const wgpu::Device& device,
      wgpu::BackendType backend_type,
      std::vector<wgpu::TextureFormat> view_formats,
      scoped_refptr<SharedContextState> context_state);
  std::unique_ptr<DawnBufferRepresentation> ProduceDawnBuffer(
      const Mailbox& mailbox,
      MemoryTypeTracker* ref,
      const wgpu::Device& device,
      wgpu::BackendType backend_type);
  std::unique_ptr<OverlayImageRepresentation> ProduceOverlay(
      const Mailbox& mailbox,
      MemoryTypeTracker* ref);
  std::unique_ptr<MemoryImageRepresentation> ProduceMemory(
      const Mailbox& mailbox,
      MemoryTypeTracker* ref);
  std::unique_ptr<RasterImageRepresentation> ProduceRaster(
      const Mailbox& mailbox,
      MemoryTypeTracker* ref);
  std::unique_ptr<VideoImageRepresentation> ProduceVideo(
      VideoDevice device,
      const Mailbox& mailbox,
      MemoryTypeTracker* ref);

#if BUILDFLAG(ENABLE_VULKAN)
  std::unique_ptr<VulkanImageRepresentation> ProduceVulkan(
      const Mailbox& mailbox,
      MemoryTypeTracker* ref,
      gpu::VulkanDeviceQueue* vulkan_device_queue,
      gpu::VulkanImplementation& vulkan_impl,
      bool needs_detiling);
#endif

#if BUILDFLAG(IS_ANDROID)
  std::unique_ptr<LegacyOverlayImageRepresentation> ProduceLegacyOverlay(
      const Mailbox& mailbox,
      MemoryTypeTracker* ref);
#endif

#if BUILDFLAG(IS_WIN)
  void UpdateExternalFence(const Mailbox& mailbox,
                           scoped_refptr<gfx::D3DSharedFence> external_fence);
#endif

  // Provides the usage flags supported by the given |mailbox|. Returns nullopt
  // if no backing is registered for `mailbox`.
  std::optional<SharedImageUsageSet> GetUsageForMailbox(const Mailbox& mailbox);

  // Called by SharedImageRepresentation in the destructor.
  void OnRepresentationDestroyed(const Mailbox& mailbox,
                                 SharedImageRepresentation* representation);

  void SetPurgeable(const Mailbox& mailbox, bool purgeable);

  bool is_thread_safe() const { return !!lock_; }

  bool display_context_on_another_thread() const {
    return display_context_on_another_thread_;
  }

  static bool SupportsScanoutImages();

  // Returns the NativePixmap backing |mailbox|. Returns null if the SharedImage
  // doesn't exist or is not backed by a NativePixmap. The caller is not
  // expected to read from or write into the provided NativePixmap because it
  // can be modified by the client at any time. The primary purpose of this
  // method is to facilitate pageflip testing on the viz thread.
  scoped_refptr<gfx::NativePixmap> GetNativePixmap(const gpu::Mailbox& mailbox);

#if BUILDFLAG(IS_WIN)
  const scoped_refptr<DXGISharedHandleManager>& dxgi_shared_handle_manager()
      const {
    return dxgi_shared_handle_manager_;
  }
#endif

 private:
  class AutoLock;
  // The lock for protecting |images_|.
  std::optional<base::Lock> lock_;

  base::flat_set<std::unique_ptr<SharedImageBacking>> images_ GUARDED_BY(lock_);

  const bool display_context_on_another_thread_;

  bool is_registered_as_memory_dump_provider_ = false;

#if BUILDFLAG(IS_WIN)
  scoped_refptr<DXGISharedHandleManager> dxgi_shared_handle_manager_;
#endif

  THREAD_CHECKER(thread_checker_);
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_SHARED_IMAGE_MANAGER_H_
