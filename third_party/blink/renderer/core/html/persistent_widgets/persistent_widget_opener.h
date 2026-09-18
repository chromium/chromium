// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_PERSISTENT_WIDGETS_PERSISTENT_WIDGET_OPENER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_PERSISTENT_WIDGETS_PERSISTENT_WIDGET_OPENER_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

class ExceptionState;
class ExecutionContext;
class LocalDOMWindow;
class ScriptObject;
class ScriptValue;
class WindowPostMessageOptions;

class CORE_EXPORT PersistentWidgetOpener : public EventTarget {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static PersistentWidgetOpener* persistentWidgetOpener(LocalDOMWindow& window);

  explicit PersistentWidgetOpener(ExecutionContext* execution_context);
  ~PersistentWidgetOpener() override;

  void postMessage(ScriptState* script_state,
                   const ScriptValue& message,
                   const String& target_origin,
                   ExceptionState& exception_state);
  void postMessage(ScriptState* script_state,
                   const ScriptValue& message,
                   const WindowPostMessageOptions* options,
                   ExceptionState& exception_state);

  // EventTarget implementation:
  const AtomicString& InterfaceName() const override;
  ExecutionContext* GetExecutionContext() const override;

  void Trace(Visitor* visitor) const override;

 private:
  Member<ExecutionContext> execution_context_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_PERSISTENT_WIDGETS_PERSISTENT_WIDGET_OPENER_H_
