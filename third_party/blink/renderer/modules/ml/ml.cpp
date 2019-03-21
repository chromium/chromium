// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ml/ml.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/modules/ml/navigator_ml.h"
#include "third_party/blink/renderer/modules/ml/neural_network_context.h"

namespace blink {

ML::ML(NavigatorML& navigator_ml)
    : ContextLifecycleObserver(navigator_ml.GetDocument()),
      navigator_ml_(&navigator_ml) {}

ML::~ML() = default;

NeuralNetworkContext* ML::getNeuralNetworkContext() {
  if (!nn_)
    nn_ = MakeGarbageCollected<NeuralNetworkContext>(navigator_ml_.Get());

  return nn_.Get();
}

void ML::ContextDestroyed(ExecutionContext*) {
  Dispose();
}

void ML::Trace(blink::Visitor* visitor) {
  visitor->Trace(navigator_ml_);
  visitor->Trace(nn_);
  ScriptWrappable::Trace(visitor);
  ContextLifecycleObserver::Trace(visitor);
}

void ML::Dispose() {}

}  // namespace blink
