// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/ml/Execution.h"

#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/DOMException.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/ExecutionContext.h"
#include "core/frame/LocalDOMWindow.h"
#include "services/service_manager/public/cpp/interface_provider.h"

namespace blink {

Execution::Execution(ml::mojom::blink::ExecutionPtrInfo info) {
  execution_.Bind(std::move(info));
  execution_.set_connection_error_handler(
      WTF::Bind(&Execution::OnConnectionError, WrapWeakPersistent(this)));
}

Execution::~Execution() = default;

ScriptPromise Execution::setInput(ScriptState* script_state,
                                  uint32_t index,
                                  MaybeShared<DOMArrayBufferView> data) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();
  if (!execution_) {
    resolver->Reject(DOMException::Create(
        kNotSupportedError, "Execution service unavailable."));
    return promise;
  }
  requests_.insert(resolver);

  uint32_t length = data.View()->byteLength();
  mojo::ScopedSharedBufferHandle buffer = mojo::SharedBufferHandle::Create(length);
  mojo::ScopedSharedBufferMapping mapping = buffer->Map(length);
  memcpy(static_cast<void*>(mapping.get()), data.View()->BaseAddress(), length);

  execution_->setInput(index, std::move(buffer), length,
                       WTF::Bind(&Execution::OnResultCode, WrapPersistent(this),
                                 WrapPersistent(resolver), String("setInput")));
  return promise;
}

ScriptPromise Execution::setOutput(ScriptState* script_state,
                                   uint32_t index,
                                   MaybeShared<DOMArrayBufferView> data) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();
  if (!execution_) {
    resolver->Reject(DOMException::Create(
        kNotSupportedError, "Execution service unavailable."));
    return promise;
  }
  requests_.insert(resolver);

  uint32_t length = data.View()->byteLength();
  mojo::ScopedSharedBufferHandle buffer = mojo::SharedBufferHandle::Create(length);

  execution_->setOutput(index, std::move(buffer), length,
                        WTF::Bind(&Execution::OnResultCode, WrapPersistent(this),
                                  WrapPersistent(resolver), String("setOutput")));
  return promise;
}

ScriptPromise Execution::startCompute(ScriptState* script_state) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();
  if (!execution_) {
    resolver->Reject(DOMException::Create(
        kNotSupportedError, "Neural Network service unavailable."));
    return promise;
  }
  requests_.insert(resolver);

  execution_->startCompute(
      WTF::Bind(&Execution::OnResultCode, WrapPersistent(this),
                WrapPersistent(resolver), String("startCompute")));
  return promise;
}

void Execution::OnResultCode(ScriptPromiseResolver* resolver,
                             const String& operation_name,
                             int32_t result_code) {
  DCHECK(requests_.Contains(resolver));
  requests_.erase(resolver);

  if (result_code == ml::mojom::blink::NO_ERROR) {
    resolver->Resolve(result_code);
  } else {
    String msg(operation_name);
    msg.append("fails: ");
    msg.append(String::Number(result_code));
    resolver->Reject(DOMException::Create(
                     kInvalidStateError, msg));
  }
}

void Execution::Trace(blink::Visitor* visitor) {
  visitor->Trace(requests_);
  ScriptWrappable::Trace(visitor);
}

void Execution::OnConnectionError() {
  for (const auto& request : requests_) {
    request->Reject(DOMException::Create(kNotSupportedError,
                                         "Execution is not implemented."));
  }
  requests_.clear();
  execution_.reset();
}

}  // namespace blink
