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

std::pair<scoped_refptr<gpu::Buffer>, base::span<std::byte>> GetHandleInfo(
    CommonDecoder* decoder,
    base::span<const std::byte> deserialize_data_bytes) {
  if (deserialize_data_bytes.size() != sizeof(MemoryTransferHandle)) {
    return {nullptr, {}};
  }

  MemoryTransferHandle handle;
  base::byte_span_from_ref(handle).copy_from(
      base::as_bytes(deserialize_data_bytes));

  scoped_refptr<gpu::Buffer> buffer =
      decoder->command_buffer_service()->GetTransferBuffer(handle.shm_id);
  if (buffer == nullptr) {
    return {nullptr, {}};
  }

  std::span<std::byte> data = base::subtle::reinterpret_span<std::byte>(
      buffer->GetSpanData(handle.shm_offset, handle.size));
  return {std::move(buffer), data};
}

class MemoryHandleImpl
    : public dawn::wire::server::MemoryTransferService::MemoryHandle {
 public:
  MemoryHandleImpl(scoped_refptr<Buffer> buffer,
                   base::raw_span<std::byte> buffer_data_view)
      : buffer_(std::move(buffer)), buffer_data_view_(buffer_data_view) {}

  ~MemoryHandleImpl() override = default;

  std::span<std::byte> GetSource() const override { return buffer_data_view_; }

  size_t GetSerializeDataUpdateSize(size_t offset, size_t size) const override {
    // Nothing is serialized because we're using shared memory.
    return 0;
  }
  void SerializeDataUpdate(std::span<std::byte> serialize_data,
                           size_t offset,
                           size_t size,
                           std::span<const std::byte> data) const override {
    DCHECK(serialize_data.size() == GetSerializeDataUpdateSize(offset, size));
    DCHECK(data.size() == size);
    // TODO(crbug.com/526518083): A compromised renderer could have a shared
    // memory size not large enough to fit the GPU buffer contents. Instead of
    // DCHECK, do a CHECK here to crash the release build. Add to
    // dawn::wire::server the validation that offset + size fits in the
    // MemoryHandle.
    CHECK_LE(offset, buffer_data_view_.size());
    CHECK_LE(size, buffer_data_view_.size() - offset);
    // Copy the data into the shared memory allocation.
    // In the case of buffer mapping, this is the mapped GPU memory which we
    // copy into client-visible shared memory.
    buffer_data_view_.subspan(offset, size).copy_from(data);
  }

  bool DeserializeDataUpdate(std::span<const std::byte> deserialize_data,
                             size_t offset,
                             size_t size,
                             std::span<std::byte> target) override {
    // Nothing is serialized because we're using shared memory.
    DCHECK(deserialize_data.empty());
    DCHECK(target.size() == size);

    if (offset > buffer_data_view_.size() ||
        size > buffer_data_view_.size() - offset || size > target.size()) {
      return false;
    }

    base::span<std::byte> dest = target.first(size);
    dest.copy_from(buffer_data_view_.subspan(offset, size));
    return true;
  }

 private:
  scoped_refptr<gpu::Buffer> buffer_;
  // Data view to client-visible shared memory owned by buffer_.
  base::raw_span<std::byte> buffer_data_view_;
};

}  // namespace

DawnServiceMemoryTransferService::DawnServiceMemoryTransferService(
    CommonDecoder* decoder)
    : dawn::wire::server::MemoryTransferService(), decoder_(decoder) {}

DawnServiceMemoryTransferService::~DawnServiceMemoryTransferService() = default;

std::unique_ptr<dawn::wire::server::MemoryTransferService::MemoryHandle>
DawnServiceMemoryTransferService::DeserializeMemoryHandle(
    std::span<const std::byte> creation_data) {
  auto [buffer, buffer_data_view] = GetHandleInfo(decoder_, creation_data);
  if (buffer_data_view.data() == nullptr) {
    return nullptr;
  }
  DCHECK(buffer);

  return std::make_unique<MemoryHandleImpl>(std::move(buffer),
                                            buffer_data_view);
}

}  // namespace webgpu
}  // namespace gpu
