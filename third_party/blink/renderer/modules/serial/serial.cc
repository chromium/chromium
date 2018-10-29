// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/serial/serial.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/modules/event_target_modules_names.h"

namespace blink {

// static
Serial* Serial::Create(ExecutionContext& execution_context) {
  return new Serial(execution_context);
}

ExecutionContext* Serial::GetExecutionContext() const {
  return ContextLifecycleObserver::GetExecutionContext();
}

const AtomicString& Serial::InterfaceName() const {
  return EventTargetNames::Serial;
}

ScriptPromise Serial::getPorts(ScriptState* script_state) {
  return ScriptPromise::RejectWithDOMException(
      script_state, DOMException::Create(DOMExceptionCode::kNotSupportedError));
}

ScriptPromise Serial::requestPort(ScriptState* script_state,
                                  const SerialPortRequestOptions& options) {
  return ScriptPromise::RejectWithDOMException(
      script_state, DOMException::Create(DOMExceptionCode::kNotSupportedError));
}

void Serial::Trace(Visitor* visitor) {
  EventTargetWithInlineData::Trace(visitor);
  ContextLifecycleObserver::Trace(visitor);
}

Serial::Serial(ExecutionContext& execution_context)
    : ContextLifecycleObserver(&execution_context) {}

}  // namespace blink
