// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ML_h
#define ML_h

#include "third_party/blink/renderer/core/execution_context/context_lifecycle_observer.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/bindings/trace_wrapper_member.h"
namespace blink {

class NavigatorML;
class NeuralNetworkContext;

class ML final : public ScriptWrappable, public ContextLifecycleObserver {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(ML);
  USING_PRE_FINALIZER(ML, Dispose);

 public:
  explicit ML(NavigatorML&);
  ~ML() override;

  NeuralNetworkContext* getNeuralNetworkContext();

  // ContextLifecycleObserver overrides.
  void ContextDestroyed(ExecutionContext*) override;

  // Interface required by garbage collection.
  void Trace(blink::Visitor*) override;

 private:
  void Dispose();

  TraceWrapperMember<NavigatorML> navigator_ml_;
  TraceWrapperMember<NeuralNetworkContext> nn_;
};

}  // namespace blink

#endif  // ML_h
