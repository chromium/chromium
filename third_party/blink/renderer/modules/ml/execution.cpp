// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/ml/Execution.h"

#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/DOMException.h"
#include "core/dom/Document.h"
#include "core/frame/LocalFrame.h"
#include "services/service_manager/public/cpp/interface_provider.h"

#include "modules/ml/NavigatorML.h"
#include "modules/ml/Compilation.h"

namespace blink {

Execution::Execution(NavigatorML* navigator_ml) {
  navigator_ml->GetDocument()->GetFrame()->GetInterfaceProvider().GetInterface(
      mojo::MakeRequest(&service_));
  service_.set_connection_error_handler(
      WTF::Bind(&Execution::OnConnectionError, WrapWeakPersistent(this)));
}

Execution::~Execution() {}

void Execution::setCompilation(Compilation* compilation, ExceptionState& exception_state) {
  compilation_id_ = compilation->GetID();
}

void Execution::setInput(uint32_t index, MaybeShared<DOMArrayBufferView> data, ExceptionState& exception_state) {
  // TODO: implement
}

void Execution::setOutput(uint32_t index, MaybeShared<DOMArrayBufferView> data, ExceptionState& exception_state) {
  // TODO: implement
}

ScriptPromise Execution::startCompute(ScriptState* script_state) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();
  if (!service_) {
    resolver->Reject(DOMException::Create(
        kNotSupportedError, "Compilation service unavailable."));
    return promise;
  }
  requests_.insert(resolver);
  // TODO: implement
  ml::mojom::blink::ComputeRequestPtr compute_request =
      ml::mojom::blink::ComputeRequest::New();
  compute_request->buffer = mojo::SharedBufferHandle::Create(4);
  service_->compute(
      compilation_id_,
      std::move(compute_request),
      WTF::Bind(&Execution::OnComputeDone, WrapPersistent(this),
                WrapPersistent(resolver)));
  return promise;
}

void Execution::OnComputeDone(ScriptPromiseResolver* resolver, int32_t result) {
  DCHECK(requests_.Contains(resolver));
  requests_.erase(resolver);

  if (result >= 0) {
    resolver->Resolve();
  } else {
    resolver->Reject(DOMException::Create(
                     kInvalidStateError, "Execution fails."));
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
  service_.reset();
}

}  // namespace blink
