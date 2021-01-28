// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/client/dawn_client_serializer.h"

#include "base/numerics/checked_math.h"
#include "base/trace_event/trace_event.h"
#include "gpu/command_buffer/client/dawn_client_memory_transfer_service.h"
#include "gpu/command_buffer/client/shared_memory_limits.h"
#include "gpu/command_buffer/client/webgpu_cmd_helper.h"
#include "gpu/command_buffer/client/webgpu_implementation.h"

namespace gpu {
namespace webgpu {

DawnClientSerializer::DawnClientSerializer(
    DawnDeviceClientID device_client_id,
    WebGPUCmdHelper* helper,
    DawnClientMemoryTransferService* memory_transfer_service,
    std::unique_ptr<TransferBuffer> c2s_transfer_buffer)
    : device_client_id_(device_client_id),
      helper_(helper),
      memory_transfer_service_(memory_transfer_service),
      c2s_transfer_buffer_(std::move(c2s_transfer_buffer)),
      c2s_buffer_(helper_, c2s_transfer_buffer_.get()) {
  DCHECK(helper_);
  DCHECK(c2s_transfer_buffer_ && c2s_transfer_buffer_->HaveBuffer());

  const SharedMemoryLimits& limits = SharedMemoryLimits::ForWebGPUContext();
  c2s_buffer_default_size_ = limits.start_transfer_buffer_size;
  DCHECK_GT(c2s_buffer_default_size_, 0u);

  DCHECK(memory_transfer_service_);
  dawn_wire::WireClientDescriptor descriptor = {};
  descriptor.serializer = this;
  descriptor.memoryTransferService = memory_transfer_service_;
  wire_client_ = std::make_unique<dawn_wire::WireClient>(descriptor);
}

DawnClientSerializer::~DawnClientSerializer() {
  // Destroy the wire client before anything else because it might still call
  // GetCmdSpace so the rest of the serializer must still be valid.
  wire_client_ = nullptr;
}

// This function can only be called once for each DawnClientSerializer
// object (before any call of GetCmdSpace()).
void DawnClientSerializer::RequestDeviceCreation(
    uint32_t requested_adapter_id,
    const WGPUDeviceProperties& requested_device_properties) {
  DCHECK(!c2s_buffer_.valid());
  DCHECK_EQ(0u, c2s_put_offset_);

  size_t serialized_device_properties_size =
      dawn_wire::SerializedWGPUDevicePropertiesSize(
          &requested_device_properties);
  DCHECK_NE(0u, serialized_device_properties_size);

  DCHECK_LE(serialized_device_properties_size,
            c2s_transfer_buffer_->GetMaxSize());
  c2s_buffer_.Reset(serialized_device_properties_size);

  dawn_wire::SerializeWGPUDeviceProperties(
      &requested_device_properties,
      reinterpret_cast<char*>(c2s_buffer_.address()));

  helper_->RequestDevice(device_client_id_, requested_adapter_id,
                         c2s_buffer_.shm_id(), c2s_buffer_.offset(),
                         serialized_device_properties_size);
  c2s_buffer_.Release();

  helper_->Flush();
}

size_t DawnClientSerializer::GetMaximumAllocationSize() const {
  return c2s_transfer_buffer_->GetMaxSize();
}

void* DawnClientSerializer::GetCmdSpace(size_t size) {
  // Note: Dawn will never call this function with |size| >
  // GetMaximumAllocationSize().
  DCHECK_LE(size, GetMaximumAllocationSize());

  // The buffer size must be initialized before any commands are serialized.
  DCHECK_NE(c2s_buffer_default_size_, 0u);

  DCHECK_LE(c2s_put_offset_, c2s_buffer_.size());
  const bool overflows_remaining_space =
      size > static_cast<size_t>(c2s_buffer_.size() - c2s_put_offset_);

  if (LIKELY(c2s_buffer_.valid() && !overflows_remaining_space)) {
    // If the buffer is valid and has sufficient space, return the
    // pointer and increment the offset.
    uint8_t* ptr = static_cast<uint8_t*>(c2s_buffer_.address());
    ptr += c2s_put_offset_;

    c2s_put_offset_ += static_cast<uint32_t>(size);
    return ptr;
  }

  if (!c2s_transfer_buffer_) {
    // The serializer hit a fatal error and was disconnected.
    return nullptr;
  }

  // Otherwise, flush and reset the command stream.
  Flush();

  uint32_t allocation_size =
      std::max(c2s_buffer_default_size_, static_cast<uint32_t>(size));
  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("gpu.dawn"),
               "DawnClientSerializer::GetCmdSpace", "bytes", allocation_size);
  c2s_buffer_.Reset(allocation_size);

  if (!c2s_buffer_.valid() || c2s_buffer_.size() < size) {
    DLOG(ERROR) << "Dawn wire transfer buffer allocation failed";
    HandleGpuControlLostContext();
    return nullptr;
  }

  c2s_put_offset_ = size;
  return c2s_buffer_.address();
}

bool DawnClientSerializer::Flush() {
  if (c2s_buffer_.valid()) {
    TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("gpu.dawn"),
                 "DawnClientSerializer::Flush", "bytes", c2s_put_offset_);

    TRACE_EVENT_WITH_FLOW0(TRACE_DISABLED_BY_DEFAULT("gpu.dawn"),
                           "DawnCommands", TRACE_EVENT_FLAG_FLOW_OUT,
                           (static_cast<uint64_t>(c2s_buffer_.shm_id()) << 32) +
                               c2s_buffer_.offset());

    c2s_buffer_.Shrink(c2s_put_offset_);
    helper_->DawnCommands(device_client_id_, c2s_buffer_.shm_id(),
                          c2s_buffer_.offset(), c2s_put_offset_);
    c2s_put_offset_ = 0;
    c2s_buffer_.Release();
    client_awaiting_flush_ = false;
  }

  memory_transfer_service_->FreeHandles(helper_);
  return true;
}

void DawnClientSerializer::SetClientAwaitingFlush(bool awaiting_flush) {
  // If awaiting_flush is true, but the c2s_buffer_ is invalid (empty), that
  // means the last command right before this caused a flush. Another flush is
  // not needed.
  client_awaiting_flush_ = awaiting_flush && c2s_buffer_.valid();
}

void DawnClientSerializer::HandleGpuControlLostContext() {
  // Immediately forget pending commands.
  c2s_buffer_.Discard();
  c2s_transfer_buffer_ = nullptr;

  // Disconnect the wire client. WebGPU commands will become a noop, and the
  // device will receive a Lost event.
  // NOTE: This assumes single-threaded operation.
  wire_client_->Disconnect();
}

WGPUDevice DawnClientSerializer::GetDevice() const {
  return wire_client_->GetDevice();
}

ReservedTexture DawnClientSerializer::ReserveTexture() {
  dawn_wire::ReservedTexture reservation =
      wire_client_->ReserveTexture(GetDevice());
  return {reservation.texture, reservation.id, reservation.generation};
}

bool DawnClientSerializer::HandleCommands(const char* commands,
                                          size_t command_size) {
  return wire_client_->HandleCommands(commands, command_size);
}

}  // namespace webgpu
}  // namespace gpu
