// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/dawn_object.h"

#include "gpu/command_buffer/client/webgpu_interface.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_device.h"
#include "third_party/blink/renderer/platform/bindings/microtask.h"

namespace blink {

DawnObjectBase::DawnObjectBase(
    scoped_refptr<DawnControlClientHolder> dawn_control_client)
    : dawn_control_client_(std::move(dawn_control_client)) {}

const scoped_refptr<DawnControlClientHolder>&
DawnObjectBase::GetDawnControlClient() const {
  return dawn_control_client_;
}

bool DawnObjectBase::IsDawnControlClientDestroyed() const {
  return dawn_control_client_->IsDestroyed();
}

gpu::webgpu::WebGPUInterface* DawnObjectBase::GetInterface() const {
  return dawn_control_client_->GetInterface();
}

const DawnProcTable& DawnObjectBase::GetProcs() const {
  return dawn_control_client_->GetProcs();
}

DawnDeviceClientSerializerHolder::DawnDeviceClientSerializerHolder(
    scoped_refptr<DawnControlClientHolder> dawn_control_client,
    uint64_t device_client_id)
    : dawn_control_client_(std::move(dawn_control_client)),
      device_client_id_(device_client_id) {}

DawnDeviceClientSerializerHolder::~DawnDeviceClientSerializerHolder() {
  if (dawn_control_client_->IsDestroyed()) {
    return;
  }
  dawn_control_client_->GetInterface()->RemoveDevice(device_client_id_);
}

const scoped_refptr<DawnControlClientHolder>&
DeviceTreeObject::GetDawnControlClient() const {
  return device_client_serializer_holder_->dawn_control_client_;
}

bool DeviceTreeObject::IsDawnControlClientDestroyed() const {
  return GetDawnControlClient()->IsDestroyed();
}
gpu::webgpu::WebGPUInterface* DeviceTreeObject::GetInterface() const {
  return GetDawnControlClient()->GetInterface();
}
const DawnProcTable& DeviceTreeObject::GetProcs() const {
  return GetDawnControlClient()->GetProcs();
}

uint64_t DeviceTreeObject::GetDeviceClientID() const {
  return device_client_serializer_holder_->device_client_id_;
}

void DeviceTreeObject::EnsureFlush() {
  bool needs_flush = false;
  GetInterface()->EnsureAwaitingFlush(
      device_client_serializer_holder_->device_client_id_, &needs_flush);
  if (!needs_flush) {
    // We've already enqueued a task to flush, or the command buffer
    // is empty. Do nothing.
    return;
  }
  Microtask::EnqueueMicrotask(WTF::Bind(
      [](scoped_refptr<DawnDeviceClientSerializerHolder> holder) {
        if (holder->dawn_control_client_->IsDestroyed()) {
          return;
        }
        holder->dawn_control_client_->GetInterface()->FlushAwaitingCommands(
            holder->device_client_id_);
      },
      device_client_serializer_holder_));
}

DawnObjectImpl::DawnObjectImpl(GPUDevice* device)
    : DeviceTreeObject(device->GetDeviceClientSerializerHolder()),
      device_(device) {}

DawnObjectImpl::~DawnObjectImpl() = default;

void DawnObjectImpl::Trace(Visitor* visitor) const {
  visitor->Trace(device_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
