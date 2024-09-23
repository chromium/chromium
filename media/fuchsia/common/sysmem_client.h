// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FUCHSIA_COMMON_SYSMEM_CLIENT_H_
#define MEDIA_FUCHSIA_COMMON_SYSMEM_CLIENT_H_

#include <fuchsia/media/cpp/fidl.h>
#include <fuchsia/sysmem2/cpp/fidl.h>

#include <string_view>
#include <vector>

#include "base/functional/callback.h"
#include "base/threading/thread_checker.h"
#include "media/base/media_export.h"

namespace media {

class VmoBuffer;

// Wrapper for fuchsia.sysmem.BufferCollection . It provides the following two
// features:
//  1. Calls Sync() and ensures that it completes before buffer constrains are
//  set and shared tokens are passed to other participants.
//  2. Provides AcquireBuffers() that allows to acquire buffers and handle
//  possible errors.
class MEDIA_EXPORT SysmemCollectionClient {
 public:
  static constexpr uint32_t kDefaultNamePriority = 100;

  // Callback for GetSharedToken().
  using GetSharedTokenCB = base::OnceCallback<void(
      fuchsia::sysmem2::BufferCollectionTokenPtr token)>;

  // Callback for AcquireBuffers(). Called with an empty |buffers| if buffers
  // allocation failed.
  using AcquireBuffersCB = base::OnceCallback<void(
      std::vector<VmoBuffer> buffers,
      const fuchsia::sysmem2::SingleBufferSettings& settings)>;

  SysmemCollectionClient(
      fuchsia::sysmem2::Allocator* allocator,
      fuchsia::sysmem2::BufferCollectionTokenPtr collection_token);
  ~SysmemCollectionClient();

  SysmemCollectionClient(const SysmemCollectionClient&) = delete;
  SysmemCollectionClient& operator=(const SysmemCollectionClient&) = delete;

  // Creates one shared token to be shared with other participants and returns
  // it asynchronously, when it's safe to pass it (i.e. after Sync()). Must be
  // called before Initialize().
  void CreateSharedToken(
      GetSharedTokenCB cb,
      std::string_view debug_client_name = std::string_view(),
      uint64_t debug_client_id = 0);

  // Initializes the collection with the given name and constraints.
  void Initialize(fuchsia::sysmem2::BufferCollectionConstraints constraints,
                  std::string_view name,
                  uint32_t name_priority = kDefaultNamePriority);

  // Create VmoBuffers to access raw memory. Should be called only after
  // GetSharedToken() has been called for all shared tokens.
  void AcquireBuffers(AcquireBuffersCB cb);

 private:
  void OnSyncComplete(fuchsia::sysmem2::Node_Sync_Result sync_result);
  void OnBuffersAllocated(
      fuchsia::sysmem2::BufferCollection_WaitForAllBuffersAllocated_Result
          wait_result);
  void OnError(zx_status_t status);

  fuchsia::sysmem2::Allocator* const allocator_;
  fuchsia::sysmem2::BufferCollectionTokenPtr collection_token_;
  fuchsia::sysmem2::BufferCollectionPtr collection_;

  bool writable_ = false;
  std::vector<base::OnceClosure> shared_token_ready_closures_;
  AcquireBuffersCB acquire_buffers_cb_;

  THREAD_CHECKER(thread_checker_);
};

// Helper fuchsia.sysmem.Allocator .
class MEDIA_EXPORT SysmemAllocatorClient {
 public:
  explicit SysmemAllocatorClient(std::string_view client_name);
  ~SysmemAllocatorClient();

  SysmemAllocatorClient(const SysmemAllocatorClient&) = delete;
  SysmemAllocatorClient& operator=(const SysmemAllocatorClient&) = delete;

  fuchsia::sysmem2::BufferCollectionTokenPtr CreateNewToken();

  // Creates new buffer collection.
  std::unique_ptr<SysmemCollectionClient> AllocateNewCollection();

  // Binds the specified token to a SysmemCollectionClient.
  std::unique_ptr<SysmemCollectionClient> BindSharedCollection(
      fuchsia::sysmem2::BufferCollectionTokenPtr token);

 private:
  friend SysmemCollectionClient;

  fuchsia::sysmem2::AllocatorPtr allocator_;
};

}  // namespace media

#endif  // MEDIA_FUCHSIA_COMMON_SYSMEM_CLIENT_H_
