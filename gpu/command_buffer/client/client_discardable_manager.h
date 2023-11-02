// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_CLIENT_CLIENT_DISCARDABLE_MANAGER_H_
#define GPU_COMMAND_BUFFER_CLIENT_CLIENT_DISCARDABLE_MANAGER_H_

#include <map>
#include <set>

#include "base/containers/queue.h"
#include "gpu/command_buffer/common/command_buffer.h"
#include "gpu/command_buffer/common/discardable_handle.h"
#include "gpu/gpu_export.h"

namespace gpu {

// ClientDiscardableManager is a helper class used by the
// ClientDiscardableTextureManager. It allows for the creation and management
// of ClientDiscardableHandles.
class GPU_EXPORT ClientDiscardableManager {
 public:
  ClientDiscardableManager();

  ClientDiscardableManager(const ClientDiscardableManager&) = delete;
  ClientDiscardableManager& operator=(const ClientDiscardableManager&) = delete;

  ~ClientDiscardableManager();

  // Note that the handles bound to an id are not guaranteed to outlive the
  // handle id. GetHandle may return an empty handle if the corresponding handle
  // was deleted on the service.
  ClientDiscardableHandle::Id CreateHandle(CommandBuffer* command_buffer);
  bool LockHandle(ClientDiscardableHandle::Id handle_id);
  void FreeHandle(ClientDiscardableHandle::Id handle_id);
  bool HandleIsValid(ClientDiscardableHandle::Id handle_id) const;
  ClientDiscardableHandle GetHandle(ClientDiscardableHandle::Id handle_id);
  bool HandleIsDeleted(ClientDiscardableHandle::Id handle_id);

  // For diagnostic tracing only.
  bool HandleIsDeletedForTracing(ClientDiscardableHandle::Id handle_id) const;

  // Test only functions.
  void CheckPendingForTesting(CommandBuffer* command_buffer) {
    CheckPending(command_buffer);
  }
  void SetElementCountForTesting(uint32_t count) {
    elements_per_allocation_ = count;
    allocation_size_ = count * element_size_;
  }

 private:
  bool FindAllocation(CommandBuffer* command_buffer,
                      scoped_refptr<Buffer>* buffer,
                      int32_t* shm_id,
                      uint32_t* offset);
  bool FindExistingAllocation(CommandBuffer* command_buffer,
                              scoped_refptr<Buffer>* buffer,
                              int32_t* shm_id,
                              uint32_t* offset);
  void ReturnAllocation(CommandBuffer* command_buffer,
                        const ClientDiscardableHandle& handle);
  void CheckPending(CommandBuffer* command_buffer);
  // Return true if we found at least one deleted entry.
  bool CheckDeleted(CommandBuffer* command_buffer);
  bool CreateNewAllocation(CommandBuffer* command_buffer);

 private:
  size_t allocation_size_;
  size_t element_size_ = sizeof(base::subtle::Atomic32);
  size_t elements_per_allocation_ = allocation_size_ / element_size_;

  struct Allocation;
  std::vector<std::unique_ptr<Allocation>> allocations_;
  std::map<ClientDiscardableHandle::Id, ClientDiscardableHandle> handles_;

  // Handles that are pending service deletion, and can be re-used once
  // ClientDiscardableHandle::CanBeReUsed returns true.
  base::queue<ClientDiscardableHandle> pending_handles_;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_CLIENT_CLIENT_DISCARDABLE_MANAGER_H_
