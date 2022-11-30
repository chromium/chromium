// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/common/discardable_handle.h"

#include "base/atomicops.h"
#include "base/logging.h"
#include "gpu/command_buffer/common/buffer.h"

namespace gpu {
namespace {
const int32_t kHandleDeleted = 0;
const int32_t kHandleUnlocked = 1;
const int32_t kHandleLockedStart = 2;

}  // namespace

DiscardableHandleBase::DiscardableHandleBase(scoped_refptr<Buffer> buffer,
                                             uint32_t byte_offset,
                                             int32_t shm_id)
    : buffer_(std::move(buffer)), byte_offset_(byte_offset), shm_id_(shm_id) {}

DiscardableHandleBase::DiscardableHandleBase(
    const DiscardableHandleBase& other) = default;
DiscardableHandleBase::DiscardableHandleBase(DiscardableHandleBase&& other) =
    default;
DiscardableHandleBase::~DiscardableHandleBase() = default;
DiscardableHandleBase& DiscardableHandleBase::operator=(
    const DiscardableHandleBase& other) = default;
DiscardableHandleBase& DiscardableHandleBase::operator=(
    DiscardableHandleBase&& other) = default;

bool DiscardableHandleBase::ValidateParameters(const Buffer* buffer,
                                               uint32_t byte_offset) {
  if (!buffer)
    return false;
  if (byte_offset % sizeof(base::subtle::Atomic32))
    return false;
  if (!buffer->GetDataAddress(byte_offset, sizeof(base::subtle::Atomic32)))
    return false;

  return true;
}

bool DiscardableHandleBase::IsDeletedForTracing() const {
  return kHandleDeleted == base::subtle::NoBarrier_Load(AsAtomic());
}

bool DiscardableHandleBase::IsLockedForTesting() const {
  return kHandleLockedStart <= base::subtle::NoBarrier_Load(AsAtomic());
}

bool DiscardableHandleBase::IsDeletedForTesting() const {
  return IsDeletedForTracing();
}

scoped_refptr<Buffer> DiscardableHandleBase::BufferForTesting() const {
  return buffer_;
}

volatile base::subtle::Atomic32* DiscardableHandleBase::AsAtomic() const {
  return reinterpret_cast<volatile base::subtle::Atomic32*>(
      buffer_->GetDataAddress(byte_offset_, sizeof(base::subtle::Atomic32)));
}

ClientDiscardableHandle::ClientDiscardableHandle()
    : DiscardableHandleBase(nullptr, 0, 0) {}

ClientDiscardableHandle::ClientDiscardableHandle(scoped_refptr<Buffer> buffer,
                                                 uint32_t byte_offset,
                                                 int32_t shm_id)
    : DiscardableHandleBase(std::move(buffer), byte_offset, shm_id) {
  // Handle always starts locked.
  base::subtle::NoBarrier_Store(AsAtomic(), kHandleLockedStart);
}

ClientDiscardableHandle::ClientDiscardableHandle(
    const ClientDiscardableHandle& other) = default;
ClientDiscardableHandle::ClientDiscardableHandle(
    ClientDiscardableHandle&& other) = default;
ClientDiscardableHandle& ClientDiscardableHandle::operator=(
    const ClientDiscardableHandle& other) = default;
ClientDiscardableHandle& ClientDiscardableHandle::operator=(
    ClientDiscardableHandle&& other) = default;

bool ClientDiscardableHandle::Lock() {
  while (true) {
    base::subtle::Atomic32 current_value =
        base::subtle::NoBarrier_Load(AsAtomic());
    if (current_value == kHandleDeleted) {
      // Once a handle is deleted, it cannot be modified further.
      return false;
    }
    base::subtle::Atomic32 new_value = current_value + 1;
    // No barrier is needed, as any commands which depend on this operation
    // will flow over the command buffer, which ensures a memory barrier
    // between here and where these commands are executed on the GPU process.
    base::subtle::Atomic32 previous_value =
        base::subtle::NoBarrier_CompareAndSwap(AsAtomic(), current_value,
                                               new_value);
    if (current_value == previous_value) {
      return true;
    }
  }
}

bool ClientDiscardableHandle::CanBeReUsed() const {
  return kHandleDeleted == base::subtle::Acquire_Load(AsAtomic());
}

ServiceDiscardableHandle::ServiceDiscardableHandle()
    : DiscardableHandleBase(nullptr, 0, 0) {}

ServiceDiscardableHandle::ServiceDiscardableHandle(scoped_refptr<Buffer> buffer,
                                                   uint32_t byte_offset,
                                                   int32_t shm_id)
    : DiscardableHandleBase(std::move(buffer), byte_offset, shm_id) {}

ServiceDiscardableHandle::ServiceDiscardableHandle(
    const ServiceDiscardableHandle& other) = default;
ServiceDiscardableHandle::ServiceDiscardableHandle(
    ServiceDiscardableHandle&& other) = default;
ServiceDiscardableHandle& ServiceDiscardableHandle::operator=(
    const ServiceDiscardableHandle& other) = default;
ServiceDiscardableHandle& ServiceDiscardableHandle::operator=(
    ServiceDiscardableHandle&& other) = default;

void ServiceDiscardableHandle::Unlock() {
  // No barrier is needed as all GPU process access happens on a single thread,
  // and communication of dependent data between the GPU process and the
  // renderer process happens across the command buffer and includes barriers.

  // This check notifies a non-malicious caller that they've issued unbalanced
  // lock/unlock calls.
  DLOG_IF(ERROR, kHandleLockedStart > base::subtle::NoBarrier_Load(AsAtomic()));

  base::subtle::NoBarrier_AtomicIncrement(AsAtomic(), -1);
}

bool ServiceDiscardableHandle::Delete() {
  // No barrier is needed as all GPU process access happens on a single thread,
  // and communication of dependent data between the GPU process and the
  // renderer process happens across the command buffer and includes barriers.
  return kHandleUnlocked == base::subtle::NoBarrier_CompareAndSwap(
                                AsAtomic(), kHandleUnlocked, kHandleDeleted);
}

void ServiceDiscardableHandle::ForceDelete() {
  // No barrier is needed as all GPU process access happens on a single thread,
  // and communication of dependent data between the GPU process and the
  // renderer process happens across the command buffer and includes barriers.
  base::subtle::NoBarrier_Store(AsAtomic(), kHandleDeleted);
}

}  // namespace gpu
