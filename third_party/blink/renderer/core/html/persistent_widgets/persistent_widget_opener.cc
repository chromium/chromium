// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/persistent_widgets/persistent_widget_opener.h"

#include "third_party/blink/renderer/core/event_target_names.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"

namespace blink {

// static
PersistentWidgetOpener* PersistentWidgetOpener::persistentWidgetOpener(
    LocalDOMWindow& window) {
  return nullptr;
}

PersistentWidgetOpener::PersistentWidgetOpener(
    ExecutionContext* execution_context)
    : execution_context_(execution_context) {}

PersistentWidgetOpener::~PersistentWidgetOpener() = default;

void PersistentWidgetOpener::postMessage(ScriptState* script_state,
                                         const ScriptValue& message,
                                         const String& target_origin,
                                         ExceptionState& exception_state) {
  // TODO(crbug.com/537818859): Implement this
}

void PersistentWidgetOpener::postMessage(
    ScriptState* script_state,
    const ScriptValue& message,
    const WindowPostMessageOptions* options,
    ExceptionState& exception_state) {
  // TODO(crbug.com/537818859): Implement this
}

const AtomicString& PersistentWidgetOpener::InterfaceName() const {
  return event_target_names::kPersistentWidgetOpener;
}

ExecutionContext* PersistentWidgetOpener::GetExecutionContext() const {
  return execution_context_.Get();
}

void PersistentWidgetOpener::Trace(Visitor* visitor) const {
  visitor->Trace(execution_context_);
  EventTarget::Trace(visitor);
}

}  // namespace blink
