// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/dawn_service_memory_transfer_service.h"

#include <dawn/dawn_proc_table.h>
#include <dawn/webgpu.h>

#include "base/compiler_specific.h"
#include "base/memory/raw_span.h"
#include "gpu/command_buffer/common/dawn_memory_transfer_handle.h"
#include "gpu/command_buffer/service/command_buffer_service.h"
#include "gpu/command_buffer/service/common_decoder.h"

namespace gpu {
namespace webgpu {

namespace {

struct HandleInfo {
  scoped_refptr<gpu::Buffer> buffer;
  base::raw_span<std::byte> data;
  TransferBufferType type = TransferBufferType::kShared;
};

HandleInfo GetHandleInfo(CommonDecoder* decoder,
                         base::span<const std::byte> deserialize_data_bytes) {
  if (deserialize_data_bytes.size() != sizeof(MemoryTransferHandle)) {
    return {};
  }

  MemoryTransferHandle handle;
  base::byte_span_from_ref(handle).copy_from(
      base::as_bytes(deserialize_data_bytes));

  scoped_refptr<gpu::Buffer> buffer =
      decoder->command_buffer_service()->GetTransferBuffer(handle.shm_id);
  if (buffer == nullptr) {
    return {};
  }

  std::span<std::byte> data = base::subtle::reinterpret_span<std::byte>(
      buffer->GetSpanData(handle.shm_offset, handle.size));
  return {std::move(buffer), data, handle.type};
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
  void SerializeDataUpdate(std::span<volatile std::byte> serialize_data,
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

 protected:
  scoped_refptr<gpu::Buffer> buffer_;
  // Data view to client-visible shared memory owned by buffer_.
  base::raw_span<std::byte> buffer_data_view_;
};

// A memory handle backed by a dedicated shared memory transfer buffer that can
// be imported directly into Dawn as SharedBufferMemory. Falls back to the
// copy-through-shared-memory path of MemoryHandleImpl when direct wrapping is
// unavailable.
class MemoryHandleWithSharedMemoryImpl : public MemoryHandleImpl {
 public:
  using MemoryHandleImpl::MemoryHandleImpl;

  ~MemoryHandleWithSharedMemoryImpl() override { ReleaseWGPUObjects(); }

  WGPUBuffer TryWrapInBuffer(const DawnProcTable* procs,
                             WGPUDevice device,
                             const WGPUBufferDescriptor* descriptor) override {
    if (descriptor->size > buffer_data_view_.size()) {
      return nullptr;
    }

    DCHECK(procs);
    procs_ = procs;

    if (!procs_->deviceHasFeature(
            device, WGPUFeatureName_SharedBufferMemoryHostPointer)) {
      ReleaseWGPUObjects();
      return nullptr;
    }

    // `buffer_` is allocated with `TransferBufferType::kDedicated` so it owns
    // the entire shared memory region.
    const base::UnsafeSharedMemoryRegion& region =
        buffer_->backing()->shared_memory_region();
    if (!region.IsValid()) {
      ReleaseWGPUObjects();
      return nullptr;
    }

    WGPUSharedBufferMemoryHostPointerDescriptor host_pointer_desc = {};
    host_pointer_desc.chain.sType =
        WGPUSType_SharedBufferMemoryHostPointerDescriptor;
    host_pointer_desc.pointer = buffer_data_view_.data();
    host_pointer_desc.size = region.GetSize();
    // We need to keep the underlying `gpu::Buffer` alive until the shared
    // buffer memory is destroyed, so we pass a reference to the `gpu::Buffer`
    // in the dispose callback when creating `shared_buffer_memory_`.
    host_pointer_desc.disposeCallbackInfo.mode =
        WGPUCallbackMode_AllowSpontaneous;
    host_pointer_desc.disposeCallbackInfo.userdata1 =
        new scoped_refptr<gpu::Buffer>(buffer_);
    host_pointer_desc.disposeCallbackInfo.callback =
        [](WGPUCallbackStatus, void* userdata1, void* userdata2) {
          delete static_cast<scoped_refptr<gpu::Buffer>*>(userdata1);
        };

    WGPUSharedBufferMemoryDescriptor desc = {};
    desc.nextInChain = &host_pointer_desc.chain;

    shared_buffer_memory_ =
        procs_->deviceImportSharedBufferMemory(device, &desc);
    if (shared_buffer_memory_ == nullptr) {
      ReleaseWGPUObjects();
      return nullptr;
    }

    wgpu_buffer_ = procs_->sharedBufferMemoryCreateBuffer(shared_buffer_memory_,
                                                          descriptor);
    if (wgpu_buffer_ == nullptr) {
      ReleaseWGPUObjects();
      return nullptr;
    }

    // BeginAccess only returns success/failure with no way to retrieve the
    // underlying reason, so wrap it in a validation error scope and discard
    // the result. This just prevents the error from reaching the device's
    // uncaptured error callback.
    procs_->devicePushErrorScope(device, WGPUErrorFilter_Validation);

    WGPUSharedBufferMemoryBeginAccessDescriptor begin_access_desc = {};
    begin_access_desc.initialized = true;
    begin_access_desc.fenceCount = 0;
    WGPUStatus status = procs_->sharedBufferMemoryBeginAccess(
        shared_buffer_memory_, wgpu_buffer_, &begin_access_desc);

    procs_->devicePopErrorScope(
        device, {nullptr, WGPUCallbackMode_AllowSpontaneous,
                 [](WGPUPopErrorScopeStatus pop_status, WGPUErrorType type,
                    WGPUStringView message, void*, void*) {},
                 nullptr, nullptr});

    if (status != WGPUStatus_Success) {
      procs_->bufferDestroy(wgpu_buffer_);
      ReleaseWGPUObjects();
      return nullptr;
    }

    // Remember the buffer so EndAccess can be paired with this BeginAccess when
    // this handle is released.
    return wgpu_buffer_;
  }

 private:
  void ReleaseWGPUObjects() {
    if (shared_buffer_memory_ != nullptr) {
      DCHECK(procs_);
      if (wgpu_buffer_ != nullptr) {
        WGPUSharedBufferMemoryEndAccessState end_state = {};
        procs_->sharedBufferMemoryEndAccess(shared_buffer_memory_, wgpu_buffer_,
                                            &end_state);
        procs_->bufferDestroy(wgpu_buffer_);
        wgpu_buffer_ = nullptr;
      }
      procs_->sharedBufferMemoryRelease(shared_buffer_memory_);
      shared_buffer_memory_ = nullptr;
    }
    procs_ = nullptr;
  }

  WGPUSharedBufferMemory shared_buffer_memory_ = nullptr;
  WGPUBuffer wgpu_buffer_ = nullptr;
  raw_ptr<const DawnProcTable> procs_ = nullptr;
};

}  // namespace

DawnServiceMemoryTransferService::DawnServiceMemoryTransferService(
    CommonDecoder* decoder)
    : dawn::wire::server::MemoryTransferService(), decoder_(decoder) {}

DawnServiceMemoryTransferService::~DawnServiceMemoryTransferService() = default;

std::unique_ptr<dawn::wire::server::MemoryTransferService::MemoryHandle>
DawnServiceMemoryTransferService::DeserializeMemoryHandle(
    std::span<const std::byte> creation_data) {
  HandleInfo info = GetHandleInfo(decoder_, creation_data);
  if (info.data.data() == nullptr) {
    return nullptr;
  }
  DCHECK(info.buffer);

  if (info.type == TransferBufferType::kDedicated) {
    return std::make_unique<MemoryHandleWithSharedMemoryImpl>(
        std::move(info.buffer), info.data);
  }
  return std::make_unique<MemoryHandleImpl>(std::move(info.buffer), info.data);
}

}  // namespace webgpu
}  // namespace gpu
