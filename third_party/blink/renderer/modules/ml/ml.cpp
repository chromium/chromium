// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ml/ml.h"

#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"

#include "third_party/blink/renderer/modules/ml/navigator_ml.h"
#include "third_party/blink/renderer/modules/ml/neural_network_context.h"

namespace blink {

ML::ML(NavigatorML* navigator_ml)
    : ContextLifecycleObserver(navigator_ml->GetDocument()),
      navigator_ml_(navigator_ml) {}

ML::~ML() {}

void ML::Dispose() {}

void ML::ContextDestroyed(ExecutionContext*) {
  Dispose();
}

NeuralNetworkContext* ML::getNeuralNetworkContext() {
  if (!nn_) {
    nn_ = new NeuralNetworkContext(navigator_ml_.Get());
  }
  return nn_.Get();
}

void ML::Trace(blink::Visitor* visitor) {
  visitor->Trace(navigator_ml_);
  visitor->Trace(nn_);
  ScriptWrappable::Trace(visitor);
  ContextLifecycleObserver::Trace(visitor);
}

}  // namespace blink
