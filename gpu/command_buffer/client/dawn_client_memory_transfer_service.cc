// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/client/dawn_client_memory_transfer_service.h"

#include "base/memory/raw_ptr.h"
#include "gpu/command_buffer/client/cmd_buffer_helper.h"
#include "gpu/command_buffer/client/mapped_memory.h"
#include "gpu/command_buffer/common/dawn_memory_transfer_handle.h"

namespace gpu {
namespace webgpu {

class DawnClientMemoryTransferService::ReadHandleImpl
    : public dawn::wire::client::MemoryTransferService::ReadHandle {
 public:
  ReadHandleImpl(void* ptr,
                 MemoryTransferHandle handle,
                 DawnClientMemoryTransferService* service)
      : ReadHandle(), ptr_(ptr), handle_(handle), service_(service) {}

  ~ReadHandleImpl() override {
    // The shared memory can't be freed until the server consumes it. Add the
    // the pointer to a list of blocks to process on the next Flush.
    service_->MarkHandleFree(ptr_);
  }

  // Get the serialization size of SerializeCreate.
  size_t SerializeCreateSize() override { return sizeof(MemoryTransferHandle); }

  // Serialize the handle into |serialize_pointer| so it can be received by the
  // service.
  void SerializeCreate(void* serialize_pointer) override {
    *reinterpret_cast<MemoryTransferHandle*>(serialize_pointer) = handle_;
  }

  const void* GetData() override { return ptr_; }

  bool DeserializeDataUpdate(const void* deserialize_pointer,
                             size_t deserialize_size,
                             size_t offset,
                             size_t size) override {
    // No data is deserialized because we're using shared memory.
    DCHECK_EQ(deserialize_size, 0u);
    return true;
  }

 private:
  raw_ptr<void> ptr_;  // Pointer to client-side shared memory.
  MemoryTransferHandle handle_;
  raw_ptr<DawnClientMemoryTransferService> service_;
};

class DawnClientMemoryTransferService::WriteHandleImpl
    : public dawn::wire::client::MemoryTransferService::WriteHandle {
 public:
  WriteHandleImpl(void* ptr,
                  MemoryTransferHandle handle,
                  DawnClientMemoryTransferService* service)
      : WriteHandle(), ptr_(ptr), handle_(handle), service_(service) {}

  ~WriteHandleImpl() override {
    // The shared memory can't be freed until the server consumes it. Add
    // the pointer to a list of blocks to process on the next Flush.
    service_->MarkHandleFree(ptr_);
  }

  // Get the serialization size of SerializeCreate.
  size_t SerializeCreateSize() override { return sizeof(MemoryTransferHandle); }

  // Serialize the handle into |serialize_pointer| so it can be received by the
  // service.
  void SerializeCreate(void* serialize_pointer) override {
    *reinterpret_cast<MemoryTransferHandle*>(serialize_pointer) = handle_;
  }

  void* GetData() override { return ptr_; }

  size_t SizeOfSerializeDataUpdate(size_t offset, size_t size) override {
    // No data is serialized because we're using shared memory.
    return 0;
  }

  void SerializeDataUpdate(void* serialize_pointer,
                           size_t offset,
                           size_t size) override {
    // No data is serialized because we're using shared memory.
  }

 private:
  raw_ptr<void> ptr_;
  MemoryTransferHandle handle_;
  raw_ptr<DawnClientMemoryTransferService> service_;
};

DawnClientMemoryTransferService::DawnClientMemoryTransferService(
    MappedMemoryManager* mapped_memory)
    : dawn::wire::client::MemoryTransferService(),
      mapped_memory_(mapped_memory) {}

DawnClientMemoryTransferService::~DawnClientMemoryTransferService() = default;

dawn::wire::client::MemoryTransferService::ReadHandle*
DawnClientMemoryTransferService::CreateReadHandle(size_t size) {
  MemoryTransferHandle handle = {};
  void* ptr = AllocateHandle(size, &handle);
  if (ptr == nullptr) {
    return nullptr;
  }
  return new ReadHandleImpl(ptr, handle, this);
}

dawn::wire::client::MemoryTransferService::WriteHandle*
DawnClientMemoryTransferService::CreateWriteHandle(size_t size) {
  MemoryTransferHandle handle = {};
  void* ptr = AllocateHandle(size, &handle);
  if (ptr == nullptr) {
    return nullptr;
  }
  // Zero-initialize the data.
  memset(ptr, 0, handle.size);
  return new WriteHandleImpl(ptr, handle, this);
}

void* DawnClientMemoryTransferService::AllocateHandle(
    size_t size,
    MemoryTransferHandle* handle) {
  if (size > std::numeric_limits<uint32_t>::max() || disconnected_) {
    return nullptr;
  }

  DCHECK(handle);
  handle->size = static_cast<uint32_t>(size);

  // If size is zero, actually allocate a byte to prevent later failures
  size_t alloc_size = size == 0 ? 1 : size;

  DCHECK(mapped_memory_);
  return mapped_memory_->Alloc(
      alloc_size, &handle->shm_id, &handle->shm_offset,
      TransferBufferAllocationOption::kReturnNullOnOOM);
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
