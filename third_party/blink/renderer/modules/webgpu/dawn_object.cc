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

gpu::webgpu::WebGPUInterface* DawnObjectBase::GetInterface() const {
  return dawn_control_client_->GetInterface();
}

const DawnProcTable& DawnObjectBase::GetProcs() const {
  return dawn_control_client_->GetProcs();
}

void DawnObjectBase::EnsureFlush() {
  bool needs_flush = false;
  GetInterface()->EnsureAwaitingFlush(&needs_flush);
  if (!needs_flush) {
    // We've already enqueued a task to flush, or the command buffer
    // is empty. Do nothing.
    return;
  }
  Microtask::EnqueueMicrotask(WTF::Bind(
      [](scoped_refptr<DawnControlClientHolder> dawn_control_client) {
        dawn_control_client->GetInterface()->FlushAwaitingCommands();
      },
      dawn_control_client_));
}

// Flush commands up until now on this object's parent device immediately.
void DawnObjectBase::FlushNow() {
  GetInterface()->FlushCommands();
}

DawnObjectImpl::DawnObjectImpl(GPUDevice* device)
    : DawnObjectBase(device->GetDawnControlClient()), device_(device) {}

DawnObjectImpl::~DawnObjectImpl() = default;

WGPUDevice DawnObjectImpl::GetDeviceHandle() {
  return device_->GetHandle();
}

void DawnObjectImpl::Trace(Visitor* visitor) const {
  visitor->Trace(device_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
