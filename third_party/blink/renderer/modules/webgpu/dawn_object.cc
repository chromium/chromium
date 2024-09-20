// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/dawn_object.h"

#include "base/numerics/checked_math.h"
#include "gpu/command_buffer/client/webgpu_interface.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_device.h"

namespace blink {

// DawnObjectBase

DawnObjectBase::DawnObjectBase(
    scoped_refptr<DawnControlClientHolder> dawn_control_client,
    const String& label)
    : dawn_control_client_(std::move(dawn_control_client)), label_(label) {}

const scoped_refptr<DawnControlClientHolder>&
DawnObjectBase::GetDawnControlClient() const {
  return dawn_control_client_;
}

void DawnObjectBase::setLabel(const String& value) {
  label_ = value;
  setLabelImpl(value);
}

void DawnObjectBase::EnsureFlush(scheduler::EventLoop& event_loop) {
  dawn_control_client_->EnsureFlush(event_loop);
}

void DawnObjectBase::FlushNow() {
  dawn_control_client_->Flush();
}

// DawnObjectImpl

DawnObjectImpl::DawnObjectImpl(GPUDevice* device, const String& label)
    : DawnObjectBase(device->GetDawnControlClient(), label), device_(device) {}

DawnObjectImpl::~DawnObjectImpl() = default;

const wgpu::Device& DawnObjectImpl::GetDeviceHandle() const {
  return device_->GetHandle();
}

void DawnObjectImpl::Trace(Visitor* visitor) const {
  visitor->Trace(device_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
