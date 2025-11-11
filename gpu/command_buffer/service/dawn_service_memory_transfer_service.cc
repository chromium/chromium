// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/dawn_service_memory_transfer_service.h"

#include "base/compiler_specific.h"
#include "base/memory/raw_span.h"
#include "gpu/command_buffer/common/dawn_memory_transfer_handle.h"
#include "gpu/command_buffer/service/command_buffer_service.h"
#include "gpu/command_buffer/service/common_decoder.h"

namespace gpu {
namespace webgpu {

namespace {

std::pair<scoped_refptr<gpu::Buffer>, base::raw_span<uint8_t>> GetHandleInfo(
    CommonDecoder* decoder,
    const void* deserialize_pointer,
    size_t deserialize_size) {
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
      decoder->command_buffer_service()->GetTransferBuffer(shm_id);
  if (buffer == nullptr) {
    return std::make_pair(std::move(buffer), base::raw_span<uint8_t>());
  }

  void* ptr = buffer->GetDataAddress(shm_offset, size);
  if (ptr == nullptr) {
    return std::make_pair(std::move(buffer), base::raw_span<uint8_t>());
  }

  // SAFETY: gpu::Buffer::GetDataAddress() will return a valid pointer only when
  // `shm_offset + size` is neither overflow nor greater than the total size of
  // the whole transfer buffer.
  auto buffer_data_view =
      UNSAFE_BUFFERS(base::raw_span<uint8_t>(static_cast<uint8_t*>(ptr), size));
  return std::make_pair(std::move(buffer), buffer_data_view);
}

class ReadHandleImpl
    : public dawn::wire::server::MemoryTransferService::ReadHandle {
 public:
  ReadHandleImpl(scoped_refptr<Buffer> buffer,
                 base::raw_span<uint8_t> buffer_data_view)
      : buffer_(std::move(buffer)), buffer_data_view_(buffer_data_view) {}

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
    CHECK_LE(offset, buffer_data_view_.size());
    CHECK_LE(size, buffer_data_view_.size() - offset);
    // Copy the data into the shared memory allocation.
    // In the case of buffer mapping, this is the mapped GPU memory which we
    // copy into client-visible shared memory.
    UNSAFE_TODO(memcpy(buffer_data_view_.data() + offset, data, size));
  }

 private:
  scoped_refptr<gpu::Buffer> buffer_;
  // Data view to client-visible shared memory owned by buffer_.
  base::raw_span<uint8_t> buffer_data_view_;
};

class WriteHandleImpl
    : public dawn::wire::server::MemoryTransferService::WriteHandle {
 public:
  WriteHandleImpl(scoped_refptr<Buffer> buffer,
                  base::raw_span<uint8_t> buffer_data_view)
      : buffer_(std::move(buffer)), buffer_data_view_(buffer_data_view) {}

  ~WriteHandleImpl() override = default;

  // The offset is always absolute offset from start of buffer
  bool DeserializeDataUpdate(const void* deserialize_pointer,
                             size_t deserialize_size,
                             size_t offset,
                             size_t size) override {
    // Nothing is serialized because we're using shared memory.
    DCHECK_EQ(deserialize_size, 0u);
    DCHECK(buffer_data_view_.data());

    auto targetData = GetTarget();
    DCHECK(targetData.data());

    if (offset > targetData.size() || size > targetData.size() - offset) {
      return false;
    }
    if (offset > buffer_data_view_.size() ||
        size > buffer_data_view_.size() - offset) {
      return false;
    }

    // Copy from shared memory into the target buffer.
    // `GetTarget()` will always return the starting address
    // of the backing buffer after the dawn side change.
    UNSAFE_TODO(memcpy(static_cast<uint8_t*>(targetData.data()) + offset,
                       buffer_data_view_.data() + offset, size));
    return true;
  }

  uint8_t* GetSourceData() const override { return buffer_data_view_.data(); }

 private:
  scoped_refptr<gpu::Buffer> buffer_;
  // Data view to client-visible shared memory owned by buffer_.
  base::raw_span<uint8_t> buffer_data_view_;
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
  auto [buffer, buffer_data_view] =
      GetHandleInfo(decoder_, deserialize_pointer, deserialize_size);
  if (buffer_data_view.data() == nullptr) {
    return false;
  }

  DCHECK(buffer);
  DCHECK(read_handle);
  *read_handle = new ReadHandleImpl(std::move(buffer), buffer_data_view);

  return true;
}

bool DawnServiceMemoryTransferService::DeserializeWriteHandle(
    const void* deserialize_pointer,
    size_t deserialize_size,
    WriteHandle** write_handle) {
  auto [buffer, buffer_data_view] =
      GetHandleInfo(decoder_, deserialize_pointer, deserialize_size);
  if (buffer_data_view.data() == nullptr) {
    return false;
  }

  DCHECK(buffer);
  DCHECK(write_handle);
  *write_handle = new WriteHandleImpl(std::move(buffer), buffer_data_view);

  return true;
}

}  // namespace webgpu
}  // namespace gpu
