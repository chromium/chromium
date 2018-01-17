// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/ml/NeuralNetwork.h"

#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/DOMException.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/ExecutionContext.h"
#include "core/frame/LocalDOMWindow.h"

#include "modules/ml/NavigatorML.h"
#include "modules/ml/Model.h"

namespace blink {

NeuralNetwork::NeuralNetwork(NavigatorML* navigator_ml)
    : ContextLifecycleObserver(navigator_ml->GetDocument()),
      navigator_ml_(navigator_ml) {}

NeuralNetwork::~NeuralNetwork() {}

void NeuralNetwork::Dispose() {}

void NeuralNetwork::ContextDestroyed(ExecutionContext*) {
  Dispose();
}

Model* NeuralNetwork::createModel(ExceptionState& exception_state) {
  return new Model();
}


void NeuralNetwork::Trace(blink::Visitor* visitor) {
  visitor->Trace(navigator_ml_);
  ScriptWrappable::Trace(visitor);
  ContextLifecycleObserver::Trace(visitor);
}

void NeuralNetwork::TraceWrappers(const ScriptWrappableVisitor* visitor) const {
  visitor->TraceWrappers(navigator_ml_);
}


}  // namespace blink
