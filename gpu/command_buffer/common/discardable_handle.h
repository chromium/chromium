// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_COMMON_DISCARDABLE_HANDLE_H_
#define GPU_COMMAND_BUFFER_COMMON_DISCARDABLE_HANDLE_H_

#include "base/memory/ref_counted.h"
#include "base/util/type_safety/id_type.h"
#include "gpu/gpu_export.h"

namespace gpu {

class Buffer;

struct SerializableSkiaHandle {
  SerializableSkiaHandle() = default;
  SerializableSkiaHandle(uint32_t handle_id,
                         uint32_t shm_id,
                         uint32_t byte_offset)
      : handle_id(handle_id), shm_id(shm_id), byte_offset(byte_offset) {}
  ~SerializableSkiaHandle() = default;

  uint32_t handle_id = 0u;
  uint32_t shm_id = 0u;
  uint32_t byte_offset = 0u;
};

// DiscardableHandleBase is the base class for the discardable handle
// implementation. In order to facilitate transfering handles across the
// command buffer, DiscardableHandleBase is backed by a gpu::Buffer and an
// offset into that buffer. It uses a single uint32_t of data at the given
// offset.
//
// DiscardableHandleBase is never used directly, but is instead modified by the
// Client/ServiceDiscardableHandle subclasses. These subclasses implement the
// Lock/Unlock/Delete functionality, making it explicit which operations occur
// in which process.
//
// Via these subclasses, a discardable handle can be transitioned between one
// of three states:
//  ╔════════════╗         ╔════════════╗         ╔═══════════╗
//  ║   Locked   ║ ──────> ║  Unlocked  ║ ──────> ║  Deleted  ║
//  ╚════════════╝         ╚════════════╝         ╚═══════════╝
//         └───────────<──────────┘
//
// Note that a handle can be locked multiple times, and stores a lock-count.
class GPU_EXPORT DiscardableHandleBase {
 public:
  int32_t shm_id() const { return shm_id_; }
  uint32_t byte_offset() const { return byte_offset_; }

  // Ensures this is a valid allocation for use with a DiscardableHandleBase.
  static bool ValidateParameters(const Buffer* buffer, uint32_t byte_offset);

  // Functions for tracing only.
  bool IsDeletedForTracing() const;

  // Test only functions.
  bool IsLockedForTesting() const;
  bool IsDeletedForTesting() const;
  scoped_refptr<Buffer> BufferForTesting() const;

 protected:
  DiscardableHandleBase(scoped_refptr<Buffer> buffer,
                        uint32_t byte_offset,
                        int32_t shm_id);
  DiscardableHandleBase(const DiscardableHandleBase& other);
  DiscardableHandleBase(DiscardableHandleBase&& other);
  DiscardableHandleBase& operator=(const DiscardableHandleBase& other);
  DiscardableHandleBase& operator=(DiscardableHandleBase&& other);
  ~DiscardableHandleBase();

  volatile base::subtle::Atomic32* AsAtomic() const;

 private:
  scoped_refptr<Buffer> buffer_;
  uint32_t byte_offset_ = 0;
  uint32_t shm_id_ = 0;
};

// ClientDiscardableHandle enables the instantiation of a new discardable
// handle (via the constructor), and can Lock an existing handle.
class GPU_EXPORT ClientDiscardableHandle : public DiscardableHandleBase {
 public:
  using Id = util::IdType32<ClientDiscardableHandle>;

  ClientDiscardableHandle();  // Constructs an invalid handle.
  ClientDiscardableHandle(scoped_refptr<Buffer> buffer,
                          uint32_t byte_offset,
                          int32_t shm_id);
  ClientDiscardableHandle(const ClientDiscardableHandle& other);
  ClientDiscardableHandle(ClientDiscardableHandle&& other);
  ClientDiscardableHandle& operator=(const ClientDiscardableHandle& other);
  ClientDiscardableHandle& operator=(ClientDiscardableHandle&& other);

  // Tries to lock the handle. Returns true if successfully locked. Returns
  // false if the handle has already been deleted on the service.
  bool Lock();

  // Returns true if the handle has been deleted on service side and can be
  // re-used on the client.
  bool CanBeReUsed() const;

  // Returns true if this handle is backed by valid shared memory.
  bool IsValid() const { return shm_id() > 0; }
};

// ServiceDiscardableHandle can wrap an existing handle (via the constructor),
// and can unlock and delete this handle.
class GPU_EXPORT ServiceDiscardableHandle : public DiscardableHandleBase {
 public:
  ServiceDiscardableHandle();  // Constructs an invalid handle.
  ServiceDiscardableHandle(scoped_refptr<Buffer> buffer,
                           uint32_t byte_offset,
                           int32_t shm_id);
  ServiceDiscardableHandle(const ServiceDiscardableHandle& other);
  ServiceDiscardableHandle(ServiceDiscardableHandle&& other);
  ServiceDiscardableHandle& operator=(const ServiceDiscardableHandle& other);
  ServiceDiscardableHandle& operator=(ServiceDiscardableHandle&& other);

  // Unlocks the handle. This should always be paired with a client-side call
  // to lock, or with a new handle, which starts locked.
  void Unlock();

  // Tries to delete the handle. Returns true if successfully deleted. Returns
  // false if the handle is locked client-side and cannot be deleted.
  bool Delete();

  // Deletes the handle, regardless of the handle's state. This should be
  // called in response to glDeleteTextures, which may be called while the
  // handle is in the locked or unlocked state.
  void ForceDelete();
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_COMMON_DISCARDABLE_HANDLE_H_
