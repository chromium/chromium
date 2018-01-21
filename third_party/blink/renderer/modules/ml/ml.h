// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ML_h
#define ML_h

#include "bindings/core/v8/ScriptPromise.h"
#include "platform/bindings/ScriptWrappable.h"
#include "core/dom/ContextLifecycleObserver.h"

namespace blink {

class NavigatorML;
class NeuralNetworkContext;

class ML final : public ScriptWrappable,
                 public ContextLifecycleObserver {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(ML);
  USING_PRE_FINALIZER(ML, Dispose);

 public:
  ML(NavigatorML*);
  ~ML() override;

  NeuralNetworkContext* getNeuralNetworkContext();

  // ContextLifecycleObserver overrides.
  void ContextDestroyed(ExecutionContext*) override;
  void Dispose();

  // Interface required by garbage collection.
  void Trace(blink::Visitor*) override;
  void TraceWrappers(const ScriptWrappableVisitor*) const override;

 private:
  TraceWrapperMember<NavigatorML> navigator_ml_;
  TraceWrapperMember<NeuralNetworkContext> nn_;
};

}  // namespace blink

#endif  // ML_h