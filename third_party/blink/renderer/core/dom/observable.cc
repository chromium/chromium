// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/observable.h"

#include "base/types/pass_key.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_subscribe_callback.h"
#include "third_party/blink/renderer/core/dom/subscriber.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

using PassKey = base::PassKey<Observable>;

// static
Observable* Observable::Create(ScriptState* script_state,
                               V8SubscribeCallback* subscribe_callback) {
  return MakeGarbageCollected<Observable>(ExecutionContext::From(script_state),
                                          subscribe_callback);
}

Observable::Observable(ExecutionContext* execution_context,
                       V8SubscribeCallback* subscribe_callback)
    : ExecutionContextClient(execution_context),
      subscribe_callback_(subscribe_callback) {
  DCHECK(subscribe_callback);
  DCHECK(RuntimeEnabledFeatures::ObservableAPIEnabled(execution_context));
}

void Observable::subscribe(Observer* observer) {
  // Build and initialize a `Subscriber` with a dictionary of `Observer`
  // callbacks.
  Subscriber* subscriber = MakeGarbageCollected<Subscriber>(
      PassKey(), GetExecutionContext(), observer);

  DCHECK(subscribe_callback_);
  // Exceptions are "reported", per
  // https://html.spec.whatwg.org/C#report-the-exception, and do not interrupt
  // the ordinary control flow here.
  subscribe_callback_->InvokeAndReportException(nullptr, subscriber);
}

void Observable::Trace(Visitor* visitor) const {
  visitor->Trace(subscribe_callback_);

  ScriptWrappable::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
}

}  // namespace blink
