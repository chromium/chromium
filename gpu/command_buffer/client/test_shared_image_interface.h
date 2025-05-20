// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_CLIENT_TEST_SHARED_IMAGE_INTERFACE_H_
#define GPU_COMMAND_BUFFER_CLIENT_TEST_SHARED_IMAGE_INTERFACE_H_

#include <memory>

#include "base/synchronization/lock.h"
#include "build/build_config.h"
#include "gpu/command_buffer/client/fake_gpu_memory_buffer.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/client/test_gpu_memory_buffer_manager.h"
#include "gpu/command_buffer/common/shared_image_capabilities.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/ipc/client/shared_image_interface_proxy.h"
#include "gpu/ipc/common/shared_image_pool_client_interface.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"

namespace gpu {

class TestBufferCollection;

class TestSharedImageInterfaceClient {
 public:
  virtual ~TestSharedImageInterfaceClient() {}
  virtual void DidDestroySharedImage() = 0;
};

class TestSharedImageInterface : public SharedImageInterface {
 public:
  TestSharedImageInterface();

  scoped_refptr<ClientSharedImage> CreateSharedImage(
      const SharedImageInfo& si_info,
      SurfaceHandle surface_handle,
      std::optional<SharedImagePoolId> pool_id = std::nullopt) override;

  scoped_refptr<ClientSharedImage> CreateSharedImage(
      const SharedImageInfo& si_info,
      base::span<const uint8_t> pixel_data) override;

  scoped_refptr<ClientSharedImage> CreateSharedImage(
      const SharedImageInfo& si_info,
      SurfaceHandle surface_handle,
      gfx::BufferUsage buffer_usage,
      std::optional<SharedImagePoolId> pool_id = std::nullopt) override;

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

  scoped_refptr<ClientSharedImage> CreateSharedImageForMLTensor(
      std::string debug_label,
      viz::SharedImageFormat format,
      const gfx::Size& size,
      gpu::SharedImageUsageSet usage) override;

  scoped_refptr<ClientSharedImage> CreateSharedImageForSoftwareCompositor(
      const SharedImageInfo& si_info) override;

  void UpdateSharedImage(const SyncToken& sync_token,
                         const Mailbox& mailbox) override;
  void UpdateSharedImage(const SyncToken& sync_token,
                         std::unique_ptr<gfx::GpuFence> acquire_fence,
                         const Mailbox& mailbox) override;

  scoped_refptr<ClientSharedImage> ImportSharedImage(
      ExportedSharedImage exported_shared_image) override;

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

  // This is used only on windows for webrtc tests where test wants the
  // production code to trigger ClientSharedImage::MapAsync() but wants
  // to control when the callback runs from inside the test. This is achieved by
  // using a custom MapCallbackController. The callback execution is deferred
  // by registering the callback with the provided |controller|. The test
  // manually triggers the mapping completion by invoking the |controller|
  // later, simulating a delayed (asynchronous) mapping. This is required to
  // test delayed mapping behavior.
  scoped_refptr<ClientSharedImage> CreateSharedImageWithMapCallbackController(
      const SharedImageInfo& si_info,
      gfx::BufferUsage buffer_usage,
      bool premapped,
      FakeGpuMemoryBuffer::MapCallbackController* controller);

  void CreateSharedImagePool(
      const SharedImagePoolId& pool_id,
      mojo::PendingRemote<mojom::SharedImagePoolClientInterface> client_remote)
      override {
    auto it = remote_map_.find(pool_id);
    CHECK(it == remote_map_.end());

    mojo::Remote<mojom::SharedImagePoolClientInterface> remote;
    remote.Bind(std::move(client_remote));
    remote_map_.emplace(pool_id, std::move(remote));
  }

  void DestroySharedImagePool(const SharedImagePoolId& pool_id) override {
    auto it = remote_map_.find(pool_id);
    if (it != remote_map_.end()) {
      // Disconnect the remote and remove the entry.
      it->second.reset();
      remote_map_.erase(it);
    }
  }

  void SetClient(TestSharedImageInterfaceClient* client) {
    test_client_ = client;
  }

  size_t shared_image_count() const { return shared_images_.size(); }
  size_t num_update_shared_image_no_fence_calls() const {
    return num_update_shared_image_no_fence_calls_;
  }
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

  raw_ptr<TestSharedImageInterfaceClient> test_client_ = nullptr;

  uint64_t release_id_ = 0;
  size_t num_update_shared_image_no_fence_calls_ = 0;
  gfx::Size most_recent_size_;
  SyncToken most_recent_generated_token_;
  SyncToken most_recent_destroy_token_;
  absl::flat_hash_set<Mailbox> shared_images_;
  bool emulate_client_provided_native_buffer_ = false;

#if BUILDFLAG(IS_FUCHSIA)
  absl::flat_hash_map<zx_koid_t, std::unique_ptr<TestBufferCollection>>
      sysmem_buffer_collections_;
#endif
  SharedImageCapabilities shared_image_capabilities_;
  bool fail_shared_image_creation_with_buffer_usage_ = false;

  // If non-null, this will be used to back mappable SharedImages with test
  // GpuMemoryBuffers.
  std::unique_ptr<TestGpuMemoryBufferManager> test_gmb_manager_;

  // This is used to simply keep the SharedImagePoolClientInterface alive for
  // the duration of the SharedImagePool. Not keeping it alive and bound
  // triggers diconnect_handlers causing unexpected behaviour in the test.
  absl::flat_hash_map<SharedImagePoolId,
                      mojo::Remote<mojom::SharedImagePoolClientInterface>>
      remote_map_;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_CLIENT_TEST_SHARED_IMAGE_INTERFACE_H_
