// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_CLIENT_TEST_SHARED_IMAGE_INTERFACE_H_
#define GPU_COMMAND_BUFFER_CLIENT_TEST_SHARED_IMAGE_INTERFACE_H_

#include <memory>

#include "base/containers/flat_set.h"
#include "base/synchronization/lock.h"
#include "build/build_config.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/client/test_gpu_memory_buffer_manager.h"
#include "gpu/command_buffer/common/shared_image_capabilities.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/ipc/client/shared_image_interface_proxy.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gpu {

class TestBufferCollection;

class TestSharedImageInterface : public SharedImageInterface {
 public:
  TestSharedImageInterface();

  scoped_refptr<ClientSharedImage> CreateSharedImage(
      const SharedImageInfo& si_info,
      SurfaceHandle surface_handle) override;

  scoped_refptr<ClientSharedImage> CreateSharedImage(
      const SharedImageInfo& si_info,
      base::span<const uint8_t> pixel_data) override;

  scoped_refptr<ClientSharedImage> CreateSharedImage(
      const SharedImageInfo& si_info,
      SurfaceHandle surface_handle,
      gfx::BufferUsage buffer_usage) override;

  MOCK_METHOD4(DoCreateSharedImage,
               void(const gfx::Size& size,
                    const viz::SharedImageFormat& format,
                    gpu::SurfaceHandle surface_handle,
                    gfx::BufferUsage buffer_usage));

  scoped_refptr<ClientSharedImage> CreateSharedImage(
      const SharedImageInfo& si_info,
      SurfaceHandle surface_handle,
      gfx::BufferUsage buffer_usage,
      gfx::GpuMemoryBufferHandle buffer_handle) override;

  scoped_refptr<ClientSharedImage> CreateSharedImage(
      const SharedImageInfo& si_info,
      gfx::GpuMemoryBufferHandle buffer_handle) override;

  SharedImageInterface::SharedImageMapping CreateSharedImage(
      const SharedImageInfo& si_info) override;

  void UpdateSharedImage(const SyncToken& sync_token,
                         const Mailbox& mailbox) override;
  void UpdateSharedImage(const SyncToken& sync_token,
                         std::unique_ptr<gfx::GpuFence> acquire_fence,
                         const Mailbox& mailbox) override;

  scoped_refptr<ClientSharedImage> ImportSharedImage(
      const ExportedSharedImage& exported_shared_image) override;

  void DestroySharedImage(const SyncToken& sync_token,
                          const Mailbox& mailbox) override;
  void DestroySharedImage(
      const SyncToken& sync_token,
      scoped_refptr<ClientSharedImage> client_shared_image) override;

  SwapChainSharedImages CreateSwapChain(viz::SharedImageFormat format,
                                        const gfx::Size& size,
                                        const gfx::ColorSpace& color_space,
                                        GrSurfaceOrigin surface_origin,
                                        SkAlphaType alpha_type,
                                        SharedImageUsageSet usage) override;
  void PresentSwapChain(const SyncToken& sync_token,
                        const Mailbox& mailbox) override;

#if BUILDFLAG(IS_FUCHSIA)
  void RegisterSysmemBufferCollection(zx::eventpair service_handle,
                                      zx::channel sysmem_token,
                                      const viz::SharedImageFormat& format,
                                      gfx::BufferUsage usage,
                                      bool register_with_image_pipe) override;
#endif  // BUILDFLAG(IS_FUCHSIA)

  SyncToken GenVerifiedSyncToken() override;
  SyncToken GenUnverifiedSyncToken() override;
  void VerifySyncToken(SyncToken& sync_token) override;
  void WaitSyncToken(const SyncToken& sync_token) override;

  void Flush() override;
  scoped_refptr<gfx::NativePixmap> GetNativePixmap(
      const Mailbox& mailbox) override;

  size_t shared_image_count() const { return shared_images_.size(); }
  const gfx::Size& MostRecentSize() const { return most_recent_size_; }
  const SyncToken& MostRecentGeneratedToken() const {
    return most_recent_generated_token_;
  }
  const SyncToken& MostRecentDestroyToken() const {
    return most_recent_destroy_token_;
  }
  bool CheckSharedImageExists(const Mailbox& mailbox) const;

  const SharedImageCapabilities& GetCapabilities() override;
  void SetCapabilities(const SharedImageCapabilities& caps);

  void SetFailSharedImageCreationWithBufferUsage(bool value) {
    fail_shared_image_creation_with_buffer_usage_ = value;
  }

  void UseTestGMBInSharedImageCreationWithBufferUsage() {
    // Create |test_gmb_manager_| only if it doesn't already exist.
    if (!test_gmb_manager_) {
      test_gmb_manager_ = std::make_unique<TestGpuMemoryBufferManager>();
    }
  }

  void emulate_client_provided_native_buffer() {
    emulate_client_provided_native_buffer_ = true;
  }

#if BUILDFLAG(IS_MAC)
  void set_texture_target_for_io_surfaces(uint32_t target) {
    shared_image_capabilities_.texture_target_for_io_surfaces = target;
  }
#endif

 protected:
  ~TestSharedImageInterface() override;

 private:
  void InitializeSharedImageCapabilities();

  mutable base::Lock lock_;

  uint64_t release_id_ = 0;
  gfx::Size most_recent_size_;
  SyncToken most_recent_generated_token_;
  SyncToken most_recent_destroy_token_;
  base::flat_set<Mailbox> shared_images_;
  bool emulate_client_provided_native_buffer_ = false;

#if BUILDFLAG(IS_FUCHSIA)
  base::flat_map<zx_koid_t, std::unique_ptr<TestBufferCollection>>
      sysmem_buffer_collections_;
#endif
  SharedImageCapabilities shared_image_capabilities_;
  bool fail_shared_image_creation_with_buffer_usage_ = false;

  // If non-null, this will be used to back mappable SharedImages with test
  // GpuMemoryBuffers.
  std::unique_ptr<TestGpuMemoryBufferManager> test_gmb_manager_;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_CLIENT_TEST_SHARED_IMAGE_INTERFACE_H_
