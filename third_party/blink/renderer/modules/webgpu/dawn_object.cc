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

void DawnObjectBase::setLabel(ScriptState* script_state,
                              const ScriptValue value,
                              ExceptionState& exception_state) {
  v8::Local<v8::Value> v8_value = value.V8Value();
  if (v8_value->IsString()) {
    setLabel(ToCoreString(v8::Local<v8::String>::Cast(v8_value)));
  } else if (v8_value->IsUndefined()) {
    setLabel(String());
  } else {
    exception_state.ThrowTypeError(
        "'label' is not of type 'string' or undefined");
  }
}

void DawnObjectBase::setLabel(const String& value) {
  // TODO: Relay label changes to Dawn
  label_ = value;
  setLabelImpl(value);
}

void DawnObjectBase::EnsureFlush() {
  bool needs_flush = false;
  auto context_provider = GetContextProviderWeakPtr();
  if (UNLIKELY(!context_provider))
    return;
  context_provider->ContextProvider()->WebGPUInterface()->EnsureAwaitingFlush(
      &needs_flush);
  if (!needs_flush) {
    // We've already enqueued a task to flush, or the command buffer
    // is empty. Do nothing.
    return;
  }
  Microtask::EnqueueMicrotask(WTF::Bind(
      [](scoped_refptr<DawnControlClientHolder> dawn_control_client) {
        if (auto context_provider =
                dawn_control_client->GetContextProviderWeakPtr()) {
          context_provider->ContextProvider()
              ->WebGPUInterface()
              ->FlushAwaitingCommands();
        }
      },
      dawn_control_client_));
}

// Flush commands up until now on this object's parent device immediately.
void DawnObjectBase::FlushNow() {
  auto context_provider = GetContextProviderWeakPtr();
  if (LIKELY(context_provider)) {
    context_provider->ContextProvider()->WebGPUInterface()->FlushCommands();
  }
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
