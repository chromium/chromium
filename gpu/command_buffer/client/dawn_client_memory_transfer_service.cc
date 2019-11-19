// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/client/dawn_client_memory_transfer_service.h"

#include "gpu/command_buffer/client/mapped_memory.h"
#include "gpu/command_buffer/common/dawn_memory_transfer_handle.h"

namespace gpu {
namespace webgpu {

class DawnClientMemoryTransferService::ReadHandleImpl
    : public dawn_wire::client::MemoryTransferService::ReadHandle {
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

  // Load initial data and open the handle for reading.
  // This function takes in the serialized result of
  // ReadHandle::SerializeInitialData.
  // It writes to |data| and |data_length| the pointer and size
  // of the mapped data for reading.
  // The allocation must live at least until the ReadHandle is destructed.
  bool DeserializeInitialData(const void* deserialize_pointer,
                              size_t deserialize_size,
                              const void** data,
                              size_t* data_length) override {
    // No data is deserialized because we're using shared memory.
    DCHECK_EQ(deserialize_size, 0u);
    DCHECK(data);
    DCHECK(data_length);

    // Write the pointer and size of the shared memory allocation.
    // |data| and |data_length| are provided by the dawn_wire client.
    *data = ptr_;
    *data_length = handle_.size;

    return true;
  }

 private:
  void* ptr_;  // Pointer to client-side shared memory.
  MemoryTransferHandle handle_;
  DawnClientMemoryTransferService* service_;
};

class DawnClientMemoryTransferService::WriteHandleImpl
    : public dawn_wire::client::MemoryTransferService::WriteHandle {
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

  // Open the handle for writing.
  // The data returned must live at least until the WriteHandle is destructed.
  std::pair<void*, size_t> Open() override {
    return std::make_pair(ptr_, handle_.size);
  }

  size_t SerializeFlushSize() override {
    // No data is serialized because we're using shared memory.
    return 0;
  }

  void SerializeFlush(void* serialize_pointer) override {
    // No data is serialized because we're using shared memory.
  }

 private:
  void* ptr_;
  MemoryTransferHandle handle_;
  DawnClientMemoryTransferService* service_;
};

DawnClientMemoryTransferService::DawnClientMemoryTransferService(
    MappedMemoryManager* mapped_memory)
    : dawn_wire::client::MemoryTransferService(),
      mapped_memory_(mapped_memory) {}

DawnClientMemoryTransferService::~DawnClientMemoryTransferService() = default;

dawn_wire::client::MemoryTransferService::ReadHandle*
DawnClientMemoryTransferService::CreateReadHandle(size_t size) {
  MemoryTransferHandle handle = {};
  void* ptr = AllocateHandle(size, &handle);
  if (ptr == nullptr) {
    return nullptr;
  }
  return new ReadHandleImpl(ptr, handle, this);
}

dawn_wire::client::MemoryTransferService::WriteHandle*
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
  if (size > std::numeric_limits<uint32_t>::max()) {
    return nullptr;
  }

  DCHECK(handle);
  handle->size = static_cast<uint32_t>(size);

  DCHECK(mapped_memory_);
  return mapped_memory_->Alloc(handle->size, &handle->shm_id,
                               &handle->shm_offset);
}

void DawnClientMemoryTransferService::MarkHandleFree(void* ptr) {
  free_blocks_.push_back(ptr);
}

void DawnClientMemoryTransferService::FreeHandlesPendingToken(int32_t token) {
  std::vector<void*> to_free = std::move(free_blocks_);
  for (void* ptr : to_free) {
    mapped_memory_->FreePendingToken(ptr, token);
  }
}

}  // namespace webgpu
}  // namespace gpu
