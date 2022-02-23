// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/dawn_service_memory_transfer_service.h"

#include "base/memory/raw_ptr.h"
#include "gpu/command_buffer/common/dawn_memory_transfer_handle.h"
#include "gpu/command_buffer/service/common_decoder.h"

namespace gpu {
namespace webgpu {

namespace {

class ReadHandleImpl
    : public dawn::wire::server::MemoryTransferService::ReadHandle {
 public:
  ReadHandleImpl(void* ptr, uint32_t size)
      : ReadHandle(), ptr_(ptr), size_(size) {}

  ~ReadHandleImpl() override = default;

  size_t SizeOfSerializeDataUpdate(size_t offset, size_t size) override {
    // Nothing is serialized because we're using shared memory.
    return 0;
  }

  void SerializeDataUpdate(const void* data,
                           size_t offset,
                           size_t size,
                           void* serializePointer) override {
    DCHECK_LE(size + offset, size_);
    // Copy the data into the shared memory allocation.
    // In the case of buffer mapping, this is the mapped GPU memory which we
    // copy into client-visible shared memory.
    memcpy(static_cast<uint8_t*>(ptr_) + offset, data, size);
  }

 private:
  raw_ptr<void> ptr_;
  uint32_t size_;
};

class WriteHandleImpl
    : public dawn::wire::server::MemoryTransferService::WriteHandle {
 public:
  WriteHandleImpl(const void* ptr, uint32_t size)
      : WriteHandle(), ptr_(ptr), size_(size) {}

  ~WriteHandleImpl() override = default;

  // The offset is always absolute offset from start of buffer
  bool DeserializeDataUpdate(const void* deserialize_pointer,
                             size_t deserialize_size,
                             size_t offset,
                             size_t size) override {
    // Nothing is serialized because we're using shared memory.
    DCHECK_EQ(deserialize_size, 0u);
    DCHECK_LE(size + offset, size_);
    DCHECK(mTargetData);
    DCHECK(ptr_);

    // Copy from shared memory into the target buffer.
    // mTargetData will always be the starting address
    // of the backing buffer after the dawn side change.
    memcpy(static_cast<uint8_t*>(mTargetData) + offset,
           static_cast<const uint8_t*>(ptr_) + offset, size);
    return true;
  }

 private:
  raw_ptr<const void> ptr_;  // Pointer to client-visible shared memory.
  uint32_t size_;
};

}  // namespace

DawnServiceMemoryTransferService::DawnServiceMemoryTransferService(
    CommonDecoder* decoder)
    : dawn::wire::server::MemoryTransferService(), decoder_(decoder) {}

DawnServiceMemoryTransferService::~DawnServiceMemoryTransferService() = default;

bool DawnServiceMemoryTransferService::DeserializeReadHandle(
    const void* deserialize_pointer,
    size_t deserialize_size,
    ReadHandle** read_handle) {
  DCHECK(deserialize_pointer);
  DCHECK_EQ(deserialize_size, sizeof(MemoryTransferHandle));
  const volatile MemoryTransferHandle* handle =
      reinterpret_cast<const volatile MemoryTransferHandle*>(
          deserialize_pointer);

  uint32_t size = handle->size;
  int32_t shm_id = handle->shm_id;
  uint32_t shm_offset = handle->shm_offset;

  void* ptr = decoder_->GetAddressAndCheckSize(shm_id, shm_offset, size);
  if (ptr == nullptr) {
    return false;
  }

  DCHECK(read_handle);
  *read_handle = new ReadHandleImpl(ptr, size);

  return true;
}

bool DawnServiceMemoryTransferService::DeserializeWriteHandle(
    const void* deserialize_pointer,
    size_t deserialize_size,
    WriteHandle** write_handle) {
  DCHECK(deserialize_pointer);
  DCHECK_EQ(deserialize_size, sizeof(MemoryTransferHandle));
  const volatile MemoryTransferHandle* handle =
      reinterpret_cast<const volatile MemoryTransferHandle*>(
          deserialize_pointer);

  uint32_t size = handle->size;
  int32_t shm_id = handle->shm_id;
  uint32_t shm_offset = handle->shm_offset;

  void* ptr = decoder_->GetAddressAndCheckSize(shm_id, shm_offset, size);
  if (ptr == nullptr) {
    return false;
  }

  DCHECK(write_handle);
  *write_handle = new WriteHandleImpl(ptr, size);

  return true;
}

}  // namespace webgpu
}  // namespace gpu
