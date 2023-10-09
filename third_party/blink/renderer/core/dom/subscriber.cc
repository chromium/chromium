// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/subscriber.h"

#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_observer.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_observer_callback.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_observer_complete_callback.h"
#include "third_party/blink/renderer/core/dom/abort_signal.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"

namespace blink {

Subscriber::Subscriber(base::PassKey<Observable>,
                       ExecutionContext* execution_context,
                       Observer* observer)
    : ExecutionContextClient(execution_context),
      next_(observer->hasNext() ? observer->next() : nullptr),
      complete_(observer->hasComplete() ? observer->complete() : nullptr),
      error_(observer->hasError() ? observer->error() : nullptr),
      signal_(observer->hasSignal() ? observer->signal() : nullptr) {}

void Subscriber::next(ScriptValue value) {
  if (next_) {
    next_->InvokeAndReportException(nullptr, value);
  }
}

void Subscriber::complete() {
  if (complete_) {
    complete_->InvokeAndReportException(nullptr);
    CloseSubscription();
  }
}

void Subscriber::error(ScriptValue error) {
  if (error_) {
    error_->InvokeAndReportException(nullptr, error);
    CloseSubscription();
  }
}

void Subscriber::CloseSubscription() {
  // Reset all handlers, making it impossible to signal any more values to the
  // subscriber.
  next_ = nullptr;
  complete_ = nullptr;
  error_ = nullptr;
  signal_ = nullptr;

  // TODO(crbug.com/1485981): Implement tear-down semantics.
}

void Subscriber::Trace(Visitor* visitor) const {
  visitor->Trace(next_);
  visitor->Trace(complete_);
  visitor->Trace(error_);
  visitor->Trace(signal_);

  ScriptWrappable::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
}

}  // namespace blink
