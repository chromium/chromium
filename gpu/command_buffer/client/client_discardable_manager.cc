// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/client/client_discardable_manager.h"

#include "base/atomic_sequence_num.h"
#include "base/containers/contains.h"
#include "base/containers/flat_set.h"
#include "base/numerics/safe_conversions.h"
#include "base/system/sys_info.h"
#include "build/build_config.h"

namespace gpu {
namespace {

// Stores a set of offsets, initially 0 to |element_count_|. Allows callers to
// take and return offsets from the set. Internally stores the offsets as a set
// of ranges. This means that in the worst case (every other offset taken), the
// set will use |element_count_| uints, but should typically use fewer.
class FreeOffsetSet {
 public:
  // Creates a new set, containing 0 to |element_count|.
  explicit FreeOffsetSet(uint32_t element_count);

  FreeOffsetSet(const FreeOffsetSet&) = delete;
  FreeOffsetSet& operator=(const FreeOffsetSet&) = delete;

  // Returns true if the set contains at least one element.
  bool HasFreeOffset() const;

  // Returns true if any element from the set has been taken.
  bool HasUsedOffset() const;

  // Takes a free offset from the set. Should only be called if HasFreeOffset().
  uint32_t TakeFreeOffset();

  // Returns an offset to the set.
  void ReturnFreeOffset(uint32_t offset);

 private:
  struct FreeRange {
    uint32_t start;
    uint32_t end;
  };
  struct CompareFreeRanges {
    bool operator()(const FreeRange& a, const FreeRange& b) const {
      return a.start < b.start;
    }
  };

  const uint32_t element_count_;
  base::flat_set<FreeRange, CompareFreeRanges> free_ranges_;
};

FreeOffsetSet::FreeOffsetSet(uint32_t element_count)
    : element_count_(element_count) {
  free_ranges_.insert({0, element_count_});
}

bool FreeOffsetSet::HasFreeOffset() const {
  return !free_ranges_.empty();
}

bool FreeOffsetSet::HasUsedOffset() const {
  if (free_ranges_.size() != 1 || free_ranges_.begin()->start != 0 ||
      free_ranges_.begin()->end != element_count_)
    return true;

  return false;
}

uint32_t FreeOffsetSet::TakeFreeOffset() {
  DCHECK(HasFreeOffset());

  auto it = free_ranges_.begin();
  uint32_t offset_to_return = it->start;

  FreeRange new_range{it->start + 1, it->end};
  free_ranges_.erase(it);
  if (new_range.start != new_range.end)
    free_ranges_.insert(new_range);

  return offset_to_return;
}

void FreeOffsetSet::ReturnFreeOffset(uint32_t offset) {
  FreeRange new_range{offset, offset + 1};

  // Find the FreeRange directly before/after our new range.
  auto next_range = free_ranges_.lower_bound(new_range);
  auto prev_range = free_ranges_.end();
  if (next_range != free_ranges_.begin()) {
    prev_range = std::prev(next_range);
  }

  // Collapse ranges if possible.
  if (prev_range != free_ranges_.end() && prev_range->end == new_range.start) {
    new_range.start = prev_range->start;
    // Erase invalidates the next_range iterator, so re-acquire it.
    next_range = free_ranges_.erase(prev_range);
  }

  if (next_range != free_ranges_.end() && next_range->start == new_range.end) {
    new_range.end = next_range->end;
    free_ranges_.erase(next_range);
  }

  free_ranges_.insert(new_range);
}

// Returns the size of the allocation which ClientDiscardableManager will
// sub-allocate from. This should be at least as big as the minimum shared
// memory allocation size.
size_t AllocationSize() {
#if BUILDFLAG(IS_NACL)
  // base::SysInfo isn't available under NaCl.
  size_t system_allocation_size = getpagesize();
#else
  size_t system_allocation_size = base::SysInfo::VMAllocationGranularity();
#endif

  // If the allocation is small (less than 2K), round it up to at least 2K.
  return std::max(size_t{2048}, system_allocation_size);
}

ClientDiscardableHandle::Id GetNextHandleId() {
  static base::AtomicSequenceNumber g_next_handle_id;

  // AtomicSequenceNumber is 0-based, add 1 to have a 1-based ID where 0 is
  // invalid.
  return ClientDiscardableHandle::Id::FromUnsafeValue(
      g_next_handle_id.GetNext() + 1);
}

}  // namespace

struct ClientDiscardableManager::Allocation {
  Allocation(uint32_t element_count) : free_offsets(element_count) {}

  scoped_refptr<Buffer> buffer;
  int32_t shm_id = 0;
  FreeOffsetSet free_offsets;
};

ClientDiscardableManager::ClientDiscardableManager()
    : allocation_size_(AllocationSize()) {}
ClientDiscardableManager::~ClientDiscardableManager() = default;

ClientDiscardableHandle::Id ClientDiscardableManager::CreateHandle(
    CommandBuffer* command_buffer) {
  scoped_refptr<Buffer> buffer;
  int32_t shm_id;
  uint32_t offset = 0;
  if (!FindAllocation(command_buffer, &buffer, &shm_id, &offset)) {
    // This can fail if we've lost context, return an invalid Id.
    return ClientDiscardableHandle::Id();
  }

  uint32_t byte_offset = base::checked_cast<uint32_t>(offset * element_size_);
  ClientDiscardableHandle handle(std::move(buffer), byte_offset, shm_id);
  ClientDiscardableHandle::Id handle_id = GetNextHandleId();
  handles_.emplace(handle_id, handle);

  return handle_id;
}

bool ClientDiscardableManager::LockHandle(
    ClientDiscardableHandle::Id handle_id) {
  auto found = handles_.find(handle_id);
  if (found == handles_.end())
    return false;
  return found->second.Lock();
}

void ClientDiscardableManager::FreeHandle(
    ClientDiscardableHandle::Id handle_id) {
  auto found = handles_.find(handle_id);
  if (found == handles_.end())
    return;
  pending_handles_.push(found->second);
  handles_.erase(found);
}

bool ClientDiscardableManager::HandleIsValid(
    ClientDiscardableHandle::Id handle_id) const {
  return base::Contains(handles_, handle_id);
}

ClientDiscardableHandle ClientDiscardableManager::GetHandle(
    ClientDiscardableHandle::Id handle_id) {
  auto found = handles_.find(handle_id);
  if (found == handles_.end())
    return ClientDiscardableHandle();
  return found->second;
}

bool ClientDiscardableManager::HandleIsDeleted(
    ClientDiscardableHandle::Id handle_id) {
  auto found = handles_.find(handle_id);
  if (found == handles_.end())
    return true;

  if (found->second.CanBeReUsed()) {
    handles_.erase(found);
    return true;
  }

  return false;
}

bool ClientDiscardableManager::HandleIsDeletedForTracing(
    ClientDiscardableHandle::Id handle_id) const {
  auto found = handles_.find(handle_id);
  if (found == handles_.end())
    return true;
  return found->second.IsDeletedForTracing();
}

bool ClientDiscardableManager::FindAllocation(CommandBuffer* command_buffer,
                                              scoped_refptr<Buffer>* buffer,
                                              int32_t* shm_id,
                                              uint32_t* offset) {
  CheckPending(command_buffer);

  if (FindExistingAllocation(command_buffer, buffer, shm_id, offset))
    return true;

  // We couldn't find an existing free entry and are about to allocate more
  // space. Check whether any handles have been deleted on the service side.
  if (CheckDeleted(command_buffer)) {
    // We deleted at least one entry, try to find an allocaiton. If the entry
    // we deleted was the last one in an allocation, it's possbile that we
    // *still* won't have allocaitons, so this isn't guaranteed to succeed.
    if (FindExistingAllocation(command_buffer, buffer, shm_id, offset))
      return true;
  }

  // Allocate more space.
  auto allocation = std::make_unique<Allocation>(elements_per_allocation_);
  allocation->buffer = command_buffer->CreateTransferBuffer(
      base::checked_cast<uint32_t>(allocation_size_), &allocation->shm_id);
  if (!allocation->buffer)
    return false;

  *offset = allocation->free_offsets.TakeFreeOffset();
  *shm_id = allocation->shm_id;
  *buffer = allocation->buffer;
  allocations_.push_back(std::move(allocation));
  return true;
}

bool ClientDiscardableManager::FindExistingAllocation(
    CommandBuffer* command_buffer,
    scoped_refptr<Buffer>* buffer,
    int32_t* shm_id,
    uint32_t* offset) {
  for (auto& allocation : allocations_) {
    if (!allocation->free_offsets.HasFreeOffset())
      continue;

    *offset = allocation->free_offsets.TakeFreeOffset();
    *shm_id = allocation->shm_id;
    *buffer = allocation->buffer;
    return true;
  }

  return false;
}

void ClientDiscardableManager::ReturnAllocation(
    CommandBuffer* command_buffer,
    const ClientDiscardableHandle& handle) {
  for (auto it = allocations_.begin(); it != allocations_.end(); ++it) {
    Allocation* allocation = it->get();
    if (allocation->shm_id != handle.shm_id())
      continue;

    allocation->free_offsets.ReturnFreeOffset(
        static_cast<uint32_t>(handle.byte_offset() / element_size_));

    if (!allocation->free_offsets.HasUsedOffset()) {
      command_buffer->DestroyTransferBuffer(allocation->shm_id);
      allocations_.erase(it);
      return;
    }
  }
}

void ClientDiscardableManager::CheckPending(CommandBuffer* command_buffer) {
  while (pending_handles_.size() > 0 &&
         pending_handles_.front().CanBeReUsed()) {
    ReturnAllocation(command_buffer, pending_handles_.front());
    pending_handles_.pop();
  }
}

bool ClientDiscardableManager::CheckDeleted(CommandBuffer* command_buffer) {
  bool freed_entry = false;
  for (auto it = handles_.begin(); it != handles_.end();) {
    if (it->second.CanBeReUsed()) {
      ReturnAllocation(command_buffer, it->second);
      it = handles_.erase(it);
      freed_entry = true;
    } else {
      ++it;
    }
  }
  return freed_entry;
}

}  // namespace gpu
