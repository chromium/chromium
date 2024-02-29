// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_CLIENT_CLIENT_TRANSFER_CACHE_H_
#define GPU_COMMAND_BUFFER_CLIENT_CLIENT_TRANSFER_CACHE_H_

#include <map>
#include <optional>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
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
//   1) Create a new ClientTransferCacheEntry.
//   2) Map a memory allocation for the entry using either MapEntry() or
//      MapTransferBufferEntry().
//   3) Write the entry data to the mapped allocation using
//      ClientTransferCacheEntry::Serialize().
//   4) Unmap the allocation using UnmapAndCreateEntry(). This will ensure that
//      the entry starts locked and will trigger the creation of the
//      service-side cache entry.
//   5) Use the new entry's ID in one or more commands.
//   6) Unlock the entry via UnlockEntries() after dependent commands have been
//      issued.
//
// If an entry is needed again:
//   7) Attempt to lock the entry via LockEntry().
//      7a) If this fails, DeleteEntry() then go to (1).
//      7b) If this succeeds, go to (5).
//
// If an entry is no longer needed:
//   8) DeleteEntry().
//
// If the client wants to send the cache entry without using the |client| passed
// to the constructor, it should replace steps (2)-(4) with a call to
// StartTransferCacheEntry() and send the information needed to create the cache
// entry on the service side using some external mechanism, e.g., an IPC
// message.
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

  ClientTransferCache(const ClientTransferCache&) = delete;
  ClientTransferCache& operator=(const ClientTransferCache&) = delete;

  ~ClientTransferCache();

  // Adds a transfer cache entry with previously written memory.
  void AddTransferCacheEntry(uint32_t type,
                             uint32_t id,
                             uint32_t shm_id,
                             uint32_t shm_offset,
                             uint32_t size);

  // Similar to AddTransferCacheEntry() but doesn't use |client_| to trigger the
  // creation of the service-side cache entry. Instead, it calls
  // |create_entry_cb| passing a ClientDiscardableHandle which
  // |create_entry_cb| can use to trigger the creation of an initially locked
  // service-side cache entry using some external mechanism, e.g., an IPC
  // message. This external mechanism should guarantee that it is safe for
  // command buffer commands to reference the cache entry after
  // |create_entry_cb| returns. Note that this function calls |create_entry_cb|
  // before returning. |create_entry_cb| is not called if the
  // ClientDiscardableHandle could not be created.
  void StartTransferCacheEntry(
      uint32_t type,
      uint32_t id,
      base::OnceCallback<void(ClientDiscardableHandle)> create_entry_cb);

  // Map(of either type) must always be followed by an Unmap.
  void* MapEntry(MappedMemoryManager* mapped_memory, uint32_t size);
  void* MapTransferBufferEntry(TransferBufferInterface* transfer_buffer,
                               uint32_t size);
  void UnmapAndCreateEntry(uint32_t type, uint32_t id);
  bool LockEntry(uint32_t type, uint32_t id);
  void UnlockEntries(const std::vector<std::pair<uint32_t, uint32_t>>& entries);
  void DeleteEntry(uint32_t type, uint32_t id);

 private:
  using EntryKey = std::pair<uint32_t, uint32_t>;
  ClientDiscardableHandle::Id FindDiscardableHandleId(const EntryKey& key)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);
  ClientDiscardableHandle CreateDiscardableHandle(const EntryKey& key)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);

  const raw_ptr<Client> client_;  // not owned --- client_ outlives this

  std::optional<ScopedMappedMemoryPtr> mapped_ptr_;
  std::optional<ScopedTransferBufferPtr> transfer_buffer_ptr_;

  // Access to other members must always be done with |lock_| held.
  base::Lock lock_;
  ClientDiscardableManager discardable_manager_ GUARDED_BY(lock_);
  std::map<EntryKey, ClientDiscardableHandle::Id> discardable_handle_id_map_
      GUARDED_BY(lock_);
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_CLIENT_CLIENT_TRANSFER_CACHE_H_
