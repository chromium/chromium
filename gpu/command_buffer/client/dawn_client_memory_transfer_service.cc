// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/client/dawn_client_memory_transfer_service.h"

#include <algorithm>

#include "base/compiler_specific.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_span.h"
#include "gpu/command_buffer/client/cmd_buffer_helper.h"
#include "gpu/command_buffer/client/mapped_memory.h"
#include "gpu/command_buffer/common/dawn_memory_transfer_handle.h"

namespace gpu {
namespace webgpu {

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

  size_t GetSerializeCreateSize() const override {
    return sizeof(MemoryTransferHandle);
  }
  void SerializeCreate(
      std::span<volatile std::byte> serialize_space) const override {
    // Note that we cannot use base::subtle::reinterpret_span here because
    // std::span for volatile types require volatile move/copy constructors
    // which would make the element type no longer trivially_copyable and hence
    // fail the check in base::subtle::reinterpret_span.
    CHECK(serialize_space.size() == GetSerializeCreateSize());
    CHECK(reinterpret_cast<uintptr_t>(serialize_space.data()) %
              alignof(MemoryTransferHandle) ==
          0u);

    // SAFETY: We checked the alignment and size above.
    auto* handle =
        UNSAFE_BUFFERS(reinterpret_cast<volatile MemoryTransferHandle*>(
            serialize_space.data()));
    handle->size = handle_.size;
    handle->shm_id = handle_.shm_id;
    handle->shm_offset = handle_.shm_offset;
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

DawnClientMemoryTransferService::DawnClientMemoryTransferService(
    MappedMemoryManager* mapped_memory)
    : dawn::wire::client::MemoryTransferService(),
      mapped_memory_(mapped_memory) {}

DawnClientMemoryTransferService::~DawnClientMemoryTransferService() = default;

std::unique_ptr<dawn::wire::client::MemoryTransferService::MemoryHandle>
DawnClientMemoryTransferService::CreateMemoryHandle(size_t size) {
  MemoryTransferHandle handle = {};
  base::span<std::byte> buffer = AllocateTransferBuffer(size, &handle);
  if (buffer.empty()) {
    return nullptr;
  }

  return std::make_unique<MemoryHandleImpl>(buffer, handle, this);
}

base::span<std::byte> DawnClientMemoryTransferService::AllocateTransferBuffer(
    size_t size,
    MemoryTransferHandle* handle) {
  if (size > std::numeric_limits<uint32_t>::max() || disconnected_) {
    return {};
  }

  DCHECK(handle);
  handle->size = static_cast<uint32_t>(size);

  // If size is zero, actually allocate a byte to prevent later failures
  size_t alloc_size = size == 0 ? 1 : size;

  DCHECK(mapped_memory_);
  return base::subtle::reinterpret_span<std::byte>(
      mapped_memory_->Alloc(alloc_size, &handle->shm_id, &handle->shm_offset,
                            TransferBufferAllocationOption::kReturnNullOnOOM));
}

void DawnClientMemoryTransferService::MarkHandleFree(void* ptr) {
  free_blocks_.push_back(ptr);
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
}

void DawnClientMemoryTransferService::Disconnect() {
  disconnected_ = true;
}

}  // namespace webgpu
}  // namespace gpu
