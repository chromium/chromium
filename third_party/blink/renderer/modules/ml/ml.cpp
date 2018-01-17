// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/ml/ML.h"

#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/DOMException.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/ExecutionContext.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/LocalFrame.h"

#include "modules/ml/NeuralNetwork.h"
#include "modules/ml/NavigatorML.h"

namespace blink {

ML::ML(NavigatorML* navigator_ml)
    : ContextLifecycleObserver(navigator_ml->GetDocument()),
      navigator_ml_(navigator_ml) {}

ML::~ML() {}

void ML::Dispose() {}

void ML::ContextDestroyed(ExecutionContext*) {
  Dispose();
}

NeuralNetwork* ML::nn() {
  if (!nn_) {
    nn_ = new NeuralNetwork(navigator_ml_.Get());
  }
  return nn_.Get();
}

void ML::Trace(blink::Visitor* visitor) {
  visitor->Trace(navigator_ml_);
  visitor->Trace(nn_);
  ScriptWrappable::Trace(visitor);
  ContextLifecycleObserver::Trace(visitor);
}

void ML::TraceWrappers(const ScriptWrappableVisitor* visitor) const {
  visitor->TraceWrappers(navigator_ml_);
  visitor->TraceWrappers(nn_);
}

}  // namespace blink
