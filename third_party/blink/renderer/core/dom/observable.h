// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_OBSERVABLE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_OBSERVABLE_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

class ExecutionContext;
class Observer;
class ScriptState;
class V8SubscribeCallback;

// Implementation of the DOM `Observable` API. See
// https://github.com/WICG/observable and
// https://docs.google.com/document/d/1NEobxgiQO-fTSocxJBqcOOOVZRmXcTFg9Iqrhebb7bg/edit.
class CORE_EXPORT Observable final : public ScriptWrappable,
                                     public ExecutionContextClient {
  DEFINE_WRAPPERTYPEINFO();

 public:
  // Called by v8 bindings.
  static Observable* Create(ScriptState*, V8SubscribeCallback*);

  Observable(ExecutionContext*, V8SubscribeCallback*);

  // API methods:
  void subscribe(ScriptState*, Observer*);

  void Trace(Visitor*) const override;

 private:
  // This gets called when the `subscribe` method is invoked. When this callback
  // is run, errors are caught and "reported":
  // https://html.spec.whatwg.org/C#report-the-exception.
  const Member<V8SubscribeCallback> subscribe_callback_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_OBSERVABLE_H_
