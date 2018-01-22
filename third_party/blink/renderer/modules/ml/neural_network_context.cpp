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

#include "modules/ml/NavigatorML.h"
#include "modules/ml/Model.h"
#include "modules/ml/Compilation.h"
#include "modules/ml/Execution.h"

namespace blink {

NeuralNetworkContext::NeuralNetworkContext(NavigatorML* navigator_ml)
    : ContextLifecycleObserver(navigator_ml->GetDocument()),
      navigator_ml_(navigator_ml) {}

NeuralNetworkContext::~NeuralNetworkContext() {}

void NeuralNetworkContext::Dispose() {}

void NeuralNetworkContext::ContextDestroyed(ExecutionContext*) {
  Dispose();
}

Model* NeuralNetworkContext::createModel(ExceptionState& exception_state) {
  return new Model();
}

Compilation* NeuralNetworkContext::createCompilation(Model* model, ExceptionState& exception_state) {
  Compilation* compilation = new Compilation(navigator_ml_.Get());
  compilation->setModel(model, exception_state);
  return compilation;
}

Execution* NeuralNetworkContext::createExecution(Compilation* compilation, ExceptionState& exception_state) {
  Execution* execution = new Execution(navigator_ml_.Get());
  execution->setCompilation(compilation, exception_state);
  return execution;
}

void NeuralNetworkContext::Trace(blink::Visitor* visitor) {
  visitor->Trace(navigator_ml_);
  ScriptWrappable::Trace(visitor);
  ContextLifecycleObserver::Trace(visitor);
}

void NeuralNetworkContext::TraceWrappers(const ScriptWrappableVisitor* visitor) const {
  visitor->TraceWrappers(navigator_ml_);
}


}  // namespace blink
