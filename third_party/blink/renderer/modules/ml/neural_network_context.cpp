// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/ml/NeuralNetworkContext.h"

#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/DOMException.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/ExecutionContext.h"
#include "core/frame/LocalDOMWindow.h"
#include "services/service_manager/public/cpp/interface_provider.h"

#include "modules/ml/NavigatorML.h"
#include "modules/ml/Model.h"

namespace blink {

NeuralNetworkContext::NeuralNetworkContext(NavigatorML* navigator_ml)
    : ContextLifecycleObserver(navigator_ml->GetDocument()) {
  navigator_ml->GetDocument()->GetFrame()->GetInterfaceProvider().GetInterface(
      mojo::MakeRequest(&neural_network_));
  neural_network_.set_connection_error_handler(
      WTF::Bind(&NeuralNetworkContext::OnConnectionError, WrapWeakPersistent(this)));
}

NeuralNetworkContext::~NeuralNetworkContext() {}

void NeuralNetworkContext::Dispose() {}

void NeuralNetworkContext::ContextDestroyed(ExecutionContext*) {
  Dispose();
}

ScriptPromise NeuralNetworkContext::createModel(ScriptState* script_state) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();
  if (!neural_network_) {
    resolver->Reject(DOMException::Create(
        kNotSupportedError, "Neural Network service unavailable."));
    return promise;
  }
  requests_.insert(resolver);

  neural_network_->createModel(
      WTF::Bind(&NeuralNetworkContext::OnCreateModel,
                WrapPersistent(this),
                WrapPersistent(resolver)));
  return promise;
}

void NeuralNetworkContext::OnCreateModel(
    ScriptPromiseResolver* resolver, int32_t result_code, ml::mojom::blink::ModelInitParamsPtr init_params) {
  DCHECK(requests_.Contains(resolver));
  requests_.erase(resolver);

  if (result_code == ml::mojom::blink::NO_ERROR) {
    resolver->Resolve(new Model(std::move(init_params->model)));
  } else {
    String msg("createModel fails: ");
    msg.append(String::Number(result_code));
    resolver->Reject(DOMException::Create(
                     kInvalidStateError, msg));
  }
}

void NeuralNetworkContext::Trace(blink::Visitor* visitor) {
  visitor->Trace(requests_);
  ScriptWrappable::Trace(visitor);
  ContextLifecycleObserver::Trace(visitor);
}

void NeuralNetworkContext::TraceWrappers(const ScriptWrappableVisitor* visitor) const {
}

void NeuralNetworkContext::OnConnectionError() {
  for (const auto& request : requests_) {
    request->Reject(DOMException::Create(kNotSupportedError,
                                         "Neural Network is not implemented."));
  }
  requests_.clear();
  neural_network_.reset();
}


}  // namespace blink
