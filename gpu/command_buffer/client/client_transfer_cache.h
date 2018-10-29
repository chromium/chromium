// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_CLIENT_CLIENT_TRANSFER_CACHE_H_
#define GPU_COMMAND_BUFFER_CLIENT_CLIENT_TRANSFER_CACHE_H_

#include <map>

#include "base/optional.h"
#include "base/synchronization/lock.h"
#include "gpu/command_buffer/client/client_discardable_manager.h"
#include "gpu/command_buffer/client/gles2_impl_export.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/client/mapped_memory.h"
#include "gpu/command_buffer/client/transfer_buffer.h"

namespace gpu {
class MappedMemoryManager;

// ClientTransferCache allows for ClientTransferCacheEntries to be inserted
// into the cache, which will send them to the ServiceTransferCache, making
// them available for consumption in the GPU process. Typical usage is:
//   1) Insert a new entry via CreateCacheEntry. It starts locked.
//   2) Use the new entry's ID in one or more commands.
//   3) Unlock the entry via UnlockTransferCacheEntries after dependent commands
//      have been issued.
//
// If an entry is needed again:
//   4) Attempt to lock the entry via LockTransferCacheEntry.
//      4a) If this fails, DeleteTransferCacheEntry then go to (1)
//      4b) If this succeeds, go to (2).
//
// If an entry is no longer needed:
//   5) DeleteTransferCacheEntry
//
// NOTE: The presence of locking on this class does not make it threadsafe.
// The underlying locking *only* allows calling LockTransferCacheEntry
// without holding the GL context lock. All other calls still require that
// the context lock be held.
class GLES2_IMPL_EXPORT ClientTransferCache {
 public:
  class Client {
   public:
    virtual void IssueCreateTransferCacheEntry(GLuint entry_type,
                                               GLuint entry_id,
                                               GLuint handle_shm_id,
                                               GLuint handle_shm_offset,
                                               GLuint data_shm_id,
                                               GLuint data_shm_offset,
                                               GLuint data_size) = 0;
    virtual void IssueDeleteTransferCacheEntry(GLuint entry_type,
                                               GLuint entry_id) = 0;
    virtual void IssueUnlockTransferCacheEntry(GLuint entry_type,
                                               GLuint entry_id) = 0;
    virtual CommandBufferHelper* cmd_buffer_helper() = 0;
    virtual CommandBuffer* command_buffer() const = 0;
  };

  explicit ClientTransferCache(Client* client);
  ~ClientTransferCache();

  // Adds a transfer cache entry with previously written memory.
  void AddTransferCacheEntry(uint32_t type,
                             uint32_t id,
                             uint32_t shm_id,
                             uint32_t shm_offset,
                             size_t size);

  // Map(of either type) must always be followed by an Unmap.
  void* MapEntry(MappedMemoryManager* mapped_memory, size_t size);
  void* MapTransferBufferEntry(TransferBufferInterface* transfer_buffer,
                               size_t size);
  void UnmapAndCreateEntry(uint32_t type, uint32_t id);
  bool LockEntry(uint32_t type, uint32_t id);
  void UnlockEntries(const std::vector<std::pair<uint32_t, uint32_t>>& entries);
  void DeleteEntry(uint32_t type, uint32_t id);

 private:
  using EntryKey = std::pair<uint32_t, uint32_t>;
  ClientDiscardableHandle::Id FindDiscardableHandleId(const EntryKey& key);
  ClientDiscardableHandle CreateDiscardableHandle(const EntryKey& key);

  Client* const client_;  // not owned --- client_ outlives this

  base::Optional<ScopedMappedMemoryPtr> mapped_ptr_;
  base::Optional<ScopedTransferBufferPtr> transfer_buffer_ptr_;

  // Access to other members must always be done with |lock_| held.
  base::Lock lock_;
  ClientDiscardableManager discardable_manager_;
  std::map<EntryKey, ClientDiscardableHandle::Id> discardable_handle_id_map_;

  DISALLOW_COPY_AND_ASSIGN(ClientTransferCache);
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_CLIENT_CLIENT_TRANSFER_CACHE_H_
