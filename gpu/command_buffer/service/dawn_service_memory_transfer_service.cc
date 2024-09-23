// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "gpu/command_buffer/service/dawn_service_memory_transfer_service.h"

#include "base/memory/raw_ptr.h"
#include "gpu/command_buffer/common/dawn_memory_transfer_handle.h"
#include "gpu/command_buffer/service/command_buffer_service.h"
#include "gpu/command_buffer/service/common_decoder.h"

namespace gpu {
namespace webgpu {

namespace {

class ReadHandleImpl
    : public dawn::wire::server::MemoryTransferService::ReadHandle {
 public:
  ReadHandleImpl(scoped_refptr<Buffer> buffer, void* ptr, uint32_t size)
      : buffer_(std::move(buffer)), ptr_(ptr), size_(size) {}

  ~ReadHandleImpl() override = default;

  size_t SizeOfSerializeDataUpdate(size_t offset, size_t size) override {
    // Nothing is serialized because we're using shared memory.
    return 0;
  }

  void SerializeDataUpdate(const void* data,
                           size_t offset,
                           size_t size,
                           void* serializePointer) override {
    // TODO(crbug.com/40061304): A compromised renderer could have a shared
    // memory size not large enough to fit the GPU buffer contents. Instead of
    // DCHECK, do a CHECK here to crash the release build. The crash is fine
    // since it is not reachable from normal behavior. WebGPU post-V1 will have
    // a refactored API.
    CHECK_LE(offset, size_);
    CHECK_LE(size, size_ - offset);
    // Copy the data into the shared memory allocation.
    // In the case of buffer mapping, this is the mapped GPU memory which we
    // copy into client-visible shared memory.
    memcpy(static_cast<uint8_t*>(ptr_) + offset, data, size);
  }

 private:
  scoped_refptr<gpu::Buffer> buffer_;
  // Pointer to client-visible shared memory owned by buffer_.
  raw_ptr<void> ptr_;
  uint32_t size_;
};

class WriteHandleImpl
    : public dawn::wire::server::MemoryTransferService::WriteHandle {
 public:
  WriteHandleImpl(scoped_refptr<Buffer> buffer, const void* ptr, uint32_t size)
      : buffer_(std::move(buffer)), ptr_(ptr), size_(size) {}

  ~WriteHandleImpl() override = default;

  // The offset is always absolute offset from start of buffer
  bool DeserializeDataUpdate(const void* deserialize_pointer,
                             size_t deserialize_size,
                             size_t offset,
                             size_t size) override {
    // Nothing is serialized because we're using shared memory.
    DCHECK_EQ(deserialize_size, 0u);
    DCHECK(mTargetData);
    DCHECK(ptr_);

    if (offset > mDataLength || size > mDataLength - offset) {
      return false;
    }
    if (offset > size_ || size > size_ - offset) {
      return false;
    }

    // Copy from shared memory into the target buffer.
    // mTargetData will always be the starting address
    // of the backing buffer after the dawn side change.
    memcpy(static_cast<uint8_t*>(mTargetData) + offset,
           static_cast<const uint8_t*>(ptr_) + offset, size);
    return true;
  }

 private:
  scoped_refptr<gpu::Buffer> buffer_;
  // Pointer to client-visible shared memory owned by buffer_.
  raw_ptr<const void> ptr_;
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
  // Use CHECK instead of DCHECK because the cast of the memory to
  // MemoryTransferHandle and subsequent reads won't be safe if deserialize_size
  // is too small.
  CHECK_EQ(deserialize_size, sizeof(MemoryTransferHandle));
  const volatile MemoryTransferHandle* handle =
      reinterpret_cast<const volatile MemoryTransferHandle*>(
          deserialize_pointer);

  uint32_t size = handle->size;
  int32_t shm_id = handle->shm_id;
  uint32_t shm_offset = handle->shm_offset;

  scoped_refptr<gpu::Buffer> buffer =
      decoder_->command_buffer_service()->GetTransferBuffer(shm_id);
  if (buffer == nullptr) {
    return false;
  }

  void* ptr = buffer->GetDataAddress(shm_offset, size);
  if (ptr == nullptr) {
    return false;
  }

  DCHECK(read_handle);
  *read_handle = new ReadHandleImpl(std::move(buffer), ptr, size);

  return true;
}

bool DawnServiceMemoryTransferService::DeserializeWriteHandle(
    const void* deserialize_pointer,
    size_t deserialize_size,
    WriteHandle** write_handle) {
  DCHECK(deserialize_pointer);
  // Use CHECK instead of DCHECK because the cast of the memory to
  // MemoryTransferHandle and subsequent reads won't be safe if deserialize_size
  // is too small.
  CHECK_EQ(deserialize_size, sizeof(MemoryTransferHandle));
  const volatile MemoryTransferHandle* handle =
      reinterpret_cast<const volatile MemoryTransferHandle*>(
          deserialize_pointer);

  uint32_t size = handle->size;
  int32_t shm_id = handle->shm_id;
  uint32_t shm_offset = handle->shm_offset;

  scoped_refptr<gpu::Buffer> buffer =
      decoder_->command_buffer_service()->GetTransferBuffer(shm_id);
  if (buffer == nullptr) {
    return false;
  }

  const void* ptr = buffer->GetDataAddress(shm_offset, size);
  if (ptr == nullptr) {
    return false;
  }

  DCHECK(write_handle);
  *write_handle = new WriteHandleImpl(std::move(buffer), ptr, size);

  return true;
}

}  // namespace webgpu
}  // namespace gpu
