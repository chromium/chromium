// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ml/neural_network_context.h"

#include <utility>

#include "services/ml/public/mojom/constants.mojom-blink.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/modules/ml/model.h"
#include "third_party/blink/renderer/modules/ml/navigator_ml.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"

namespace blink {

NeuralNetworkContext::NeuralNetworkContext(NavigatorML* navigator_ml)
    : ContextLifecycleObserver(navigator_ml->GetDocument()) {
  navigator_ml->GetDocument()->GetFrame()->GetInterfaceProvider().GetInterface(
      mojo::MakeRequest(&neural_network_));
  neural_network_.set_connection_error_handler(WTF::Bind(
      &NeuralNetworkContext::OnConnectionError, WrapWeakPersistent(this)));
}

NeuralNetworkContext::~NeuralNetworkContext() = default;

ScriptPromise NeuralNetworkContext::createModel(ScriptState* script_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();
  if (!neural_network_) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNotSupportedError,
        "Neural Network service unavailable."));
    return promise;
  }

  requests_.insert(resolver);
  neural_network_->CreateModel(WTF::Bind(&NeuralNetworkContext::OnCreateModel,
                                         WrapPersistent(this),
                                         WrapPersistent(resolver)));
  return promise;
}

void NeuralNetworkContext::ContextDestroyed(ExecutionContext*) {
  Dispose();
}

void NeuralNetworkContext::Trace(blink::Visitor* visitor) {
  visitor->Trace(requests_);
  ScriptWrappable::Trace(visitor);
  ContextLifecycleObserver::Trace(visitor);
}

void NeuralNetworkContext::OnCreateModel(
    ScriptPromiseResolver* resolver,
    int32_t result_code,
    ml::mojom::blink::ModelInitParamsPtr init_params) {
  DCHECK(requests_.Contains(resolver));
  requests_.erase(resolver);

  if (result_code == ml::mojom::blink::NOT_ERROR) {
    resolver->Resolve(
        MakeGarbageCollected<Model>(std::move(init_params->model)));
  } else {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kInvalidStateError,
        "createModel fails: " + String::Number(result_code)));
  }
}

void NeuralNetworkContext::OnConnectionError() {
  for (const auto& request : requests_) {
    request->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNotSupportedError,
        "Neural Network is not implemented."));
  }
  requests_.clear();
  neural_network_.reset();
}

void NeuralNetworkContext::Dispose() {}

}  // namespace blink
