// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/client/dawn_client_memory_transfer_service.h"

#include <algorithm>

#include "base/bits.h"
#include "base/compiler_specific.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_span.h"
#include "base/numerics/safe_conversions.h"
#include "base/system/sys_info.h"
#include "gpu/command_buffer/client/cmd_buffer_helper.h"
#include "gpu/command_buffer/client/mapped_memory.h"
#include "gpu/command_buffer/common/dawn_memory_transfer_handle.h"

namespace gpu {
namespace webgpu {

namespace {

bool CanUseDedicatedTransferBuffer(
    bool enable_dedicated_transfer_buffer,
    DawnClientMemoryTransferService::MemoryHandleUse memoryHandleUse,
    size_t size) {
  // Only do dedicated allocation on large buffers (>= 4MB) as shared allocation
  // performs better on small-sized buffers.
  constexpr size_t kMinSize = 4 * 1024 * 1024;
  // A dedicated chunk is its own shared memory region, so its size is rounded
  // up to the OS allocation granularity (64KB on Windows) and it is never
  // sub-allocated or reused. Sending small allocations down that path would
  // cost a full granule each; keep them in the sub-allocated pool instead.
  return enable_dedicated_transfer_buffer &&
         memoryHandleUse ==
             DawnClientMemoryTransferService::MemoryHandleUse::MappedBuffer &&
         size >= kMinSize;
}

void SerializeMemoryTransferHandle(
    std::span<volatile std::byte> serialize_space,
    const MemoryTransferHandle& handle) {
  // Note that we cannot use base::subtle::reinterpret_span here because
  // std::span for volatile types require volatile move/copy constructors
  // which would make the element type no longer trivially_copyable and hence
  // fail the check in base::subtle::reinterpret_span.
  CHECK(serialize_space.size() == sizeof(MemoryTransferHandle));
  CHECK(reinterpret_cast<uintptr_t>(serialize_space.data()) %
            alignof(MemoryTransferHandle) ==
        0u);

  // SAFETY: We checked the alignment and size above.
  auto* serialized = UNSAFE_BUFFERS(
      reinterpret_cast<volatile MemoryTransferHandle*>(serialize_space.data()));
  serialized->size = handle.size;
  serialized->shm_id = handle.shm_id;
  serialized->shm_offset = handle.shm_offset;
  serialized->type = handle.type;
}

}  // anonymous namespace

class DawnClientMemoryTransferService::MemoryHandleImpl
    : public dawn::wire::client::MemoryTransferService::MemoryHandle {
 public:
  MemoryHandleImpl(base::span<std::byte> buffer,
                   MemoryTransferHandle handle,
                   DawnClientMemoryTransferService* service)
      : buffer_(buffer), handle_(handle), service_(service) {}

  ~MemoryHandleImpl() override {
    // The shared memory can't be freed until the server consumes it. Add
    // the pointer to a list of blocks to process on the next Flush.
    service_->MarkHandleFree(buffer_.data());
  }

  // Shared transfer buffer is not initialized as it may be reused from a pool
  // and has dirty data.
  bool IsInitialized() const override { return false; }

  size_t GetSerializeCreateSize() const override {
    return sizeof(MemoryTransferHandle);
  }
  void SerializeCreate(
      std::span<volatile std::byte> serialize_space) const override {
    SerializeMemoryTransferHandle(serialize_space, handle_);
  }

  std::span<std::byte> GetData() const override { return buffer_; }

  size_t GetSerializeDataUpdateSize(size_t offset, size_t size) const override {
    // No data is serialized because we're using shared memory.
    return 0;
  }
  void SerializeDataUpdate(std::span<volatile std::byte> serialize_data,
                           size_t offset,
                           size_t size) const override {
    // No data is serialized because we're using shared memory.
    DCHECK(serialize_data.size() == GetSerializeDataUpdateSize(offset, size));
  }

  bool DeserializeDataUpdate(std::span<const std::byte> deserialize_data,
                             size_t offset,
                             size_t size) override {
    if (offset > buffer_.size() ||
        deserialize_data.size() > buffer_.size() - offset) {
      return false;
    }

    // No data is deserialized because we're using shared memory.
    DCHECK(deserialize_data.empty());
    return true;
  }

 private:
  base::raw_span<std::byte> buffer_;
  MemoryTransferHandle handle_;
  raw_ptr<DawnClientMemoryTransferService> service_;
};

// A memory handle backed by a chunk of shared memory dedicated to this
// allocation. The chunk's transfer buffer is destroyed when the handle is.
class DawnClientMemoryTransferService::MemoryHandleImplWithDedicatedChunk
    : public dawn::wire::client::MemoryTransferService::MemoryHandle {
 public:
  MemoryHandleImplWithDedicatedChunk(ScopedDedicatedChunk chunk,
                                     DawnClientMemoryTransferService* service)
      : chunk_(std::move(chunk)), service_(service) {}

  ~MemoryHandleImplWithDedicatedChunk() override {
    // Destroying the chunk destroys its transfer buffer, which must not happen
    // until the wire commands referencing it have been committed to the
    // command buffer.
    service_->MarkDedicatedChunkFree(std::move(chunk_));
  }

  // A dedicated chunk is always a freshly allocated shared memory region
  // (never reused), and freshly allocated OS memory is zero-initialized on all
  // OSes.
  bool IsInitialized() const override { return true; }

  size_t GetSerializeCreateSize() const override {
    return sizeof(MemoryTransferHandle);
  }
  void SerializeCreate(
      std::span<volatile std::byte> serialize_space) const override {
    // A dedicated chunk is never sub-allocated, so the allocation always spans
    // the whole chunk starting at offset 0.
    SerializeMemoryTransferHandle(
        serialize_space,
        {.size = base::checked_cast<uint32_t>(chunk_.span().size()),
         .shm_id = chunk_.shm_id(),
         .shm_offset = 0u,
         .type = TransferBufferType::kDedicated});
  }

  std::span<std::byte> GetData() const override {
    return base::subtle::reinterpret_span<std::byte>(chunk_.span());
  }

  size_t GetSerializeDataUpdateSize(size_t offset, size_t size) const override {
    // No data is serialized because we're using shared memory.
    return 0;
  }
  void SerializeDataUpdate(std::span<volatile std::byte> serialize_data,
                           size_t offset,
                           size_t size) const override {
    // No data is serialized because we're using shared memory.
    DCHECK(serialize_data.size() == GetSerializeDataUpdateSize(offset, size));
  }

  bool DeserializeDataUpdate(std::span<const std::byte> deserialize_data,
                             size_t offset,
                             size_t size) override {
    const size_t buffer_size = chunk_.span().size();
    if (offset > buffer_size ||
        deserialize_data.size() > buffer_size - offset) {
      return false;
    }

    // No data is deserialized because we're using shared memory.
    DCHECK(deserialize_data.empty());
    return true;
  }

 private:
  ScopedDedicatedChunk chunk_;
  raw_ptr<DawnClientMemoryTransferService> service_;
};

DawnClientMemoryTransferService::DawnClientMemoryTransferService(
    MappedMemoryManager* mapped_memory,
    bool enable_dedicated_transfer_buffer)
    : mapped_memory_(mapped_memory),
      enable_dedicated_transfer_buffer_(enable_dedicated_transfer_buffer) {}

DawnClientMemoryTransferService::~DawnClientMemoryTransferService() = default;

std::unique_ptr<dawn::wire::client::MemoryTransferService::MemoryHandle>
DawnClientMemoryTransferService::CreateMemoryHandle(
    size_t size,
    MemoryHandleUse memory_handle_use) {
  if (disconnected_) {
    return {};
  }

  if (CanUseDedicatedTransferBuffer(enable_dedicated_transfer_buffer_,
                                    memory_handle_use, size)) {
    ScopedDedicatedChunk chunk = AllocateDedicatedChunk(size);
    if (chunk.valid()) {
      return std::make_unique<MemoryHandleImplWithDedicatedChunk>(
          std::move(chunk), this);
    }
    // Fall back to a shared transfer buffer if the dedicated chunk could not
    // be allocated.
  }

  MemoryTransferHandle handle = {};
  base::span<std::byte> buffer = AllocateSharedTransferBuffer(size, &handle);
  if (buffer.empty()) {
    return {};
  }

  return std::make_unique<MemoryHandleImpl>(buffer, handle, this);
}

base::span<std::byte>
DawnClientMemoryTransferService::AllocateSharedTransferBuffer(
    size_t size,
    MemoryTransferHandle* handle) {
  if (size > std::numeric_limits<uint32_t>::max() || disconnected_) {
    return {};
  }

  DCHECK(handle);
  handle->size = static_cast<uint32_t>(size);
  handle->type = TransferBufferType::kShared;

  // If size is zero, actually allocate a byte to prevent later failures
  size_t alloc_size = size == 0 ? 1 : size;

  DCHECK(mapped_memory_);
  return base::subtle::reinterpret_span<std::byte>(
      mapped_memory_->Alloc(alloc_size, &handle->shm_id, &handle->shm_offset,
                            TransferBufferAllocationOption::kReturnNullOnOOM));
}

ScopedDedicatedChunk DawnClientMemoryTransferService::AllocateDedicatedChunk(
    size_t size) {
  DCHECK(size > 0);
  DCHECK(!disconnected_);

  // Align the dedicated chunk size to the system's VM allocation granularity
  // (for example, 64KB on Windows) which is required by the WebGPU feature
  // `SharedBufferMemoryHostPointer`.
  size_t alignment = base::SysInfo::VMAllocationGranularity();
  size_t aligned_size = base::bits::AlignUp(size, alignment);
  if (aligned_size > std::numeric_limits<uint32_t>::max()) {
    return {};
  }

  DCHECK(mapped_memory_);
  return mapped_memory_->AllocDedicatedChunk(
      static_cast<uint32_t>(aligned_size));
}

void DawnClientMemoryTransferService::MarkHandleFree(void* ptr) {
  free_blocks_.push_back(ptr);
}

void DawnClientMemoryTransferService::MarkDedicatedChunkFree(
    ScopedDedicatedChunk chunk) {
  free_dedicated_chunks_.push_back(std::move(chunk));
}

void DawnClientMemoryTransferService::FreeHandles(CommandBufferHelper* helper) {
  std::vector<raw_ptr<void, VectorExperimental>> to_free =
      std::move(free_blocks_);
  if (to_free.size() > 0) {
    int32_t token = helper->InsertToken();
    for (void* ptr : to_free) {
      mapped_memory_->FreePendingToken(ptr, token);
    }
  }

  // Each of these issues an ordering barrier followed by an out-of-band
  // DestroyTransferBuffer, so the commands referencing them must already be in
  // the command buffer by now.
  free_dedicated_chunks_.clear();
}

void DawnClientMemoryTransferService::Disconnect() {
  disconnected_ = true;
}

}  // namespace webgpu
}  // namespace gpu
