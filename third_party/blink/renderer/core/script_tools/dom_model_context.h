// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SCRIPT_TOOLS_DOM_MODEL_CONTEXT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SCRIPT_TOOLS_DOM_MODEL_CONTEXT_H_

#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/script_tools/model_context.h"

namespace blink {

class Document;
class ExceptionState;
class ExecuteToolOptions;
class ModelContextGetToolOptions;
class ModelContextRegisterToolOptions;
class ModelContextTool;
class Tool;
class V8ToolExecuteCallback;

// Implements the `ModelContext` Web IDL interface (`document.modelContext`)
class CORE_EXPORT DOMModelContext final : public EventTarget {
  DEFINE_WRAPPERTYPEINFO();

 public:
  DOMModelContext(Document& document, ModelContext& model_context);

  DEFINE_ATTRIBUTE_EVENT_LISTENER(toolchange, kToolchange)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(toolactivated, kToolactivated)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(toolcancel, kToolcancel)

  ScriptPromise<IDLUndefined> registerTool(
      ScriptState* state,
      ModelContextTool* tool,
      ModelContextRegisterToolOptions* options);
  ScriptPromise<IDLSequence<Tool>> getTools(
      ScriptState* script_state,
      const ModelContextGetToolOptions* options = nullptr);
  ScriptPromise<IDLNullable<IDLString>> executeTool(ScriptState* script_state,
                                                    Tool* tool);
  ScriptPromise<IDLNullable<IDLString>> executeTool(
      ScriptState* script_state,
      Tool* tool,
      ScriptObject input_object,
      const ExecuteToolOptions* options = nullptr);
  ScriptPromise<IDLUndefined> resumeTool(ScriptState* script_state,
                                         const String& token,
                                         V8ToolExecuteCallback* callback);
  void waitForToolsUntil(ScriptState* script_state,
                         ScriptPromise<IDLAny> promise,
                         ExceptionState& exception_state);

  void Trace(Visitor*) const override;

 private:
  // EventTarget:
  const AtomicString& InterfaceName() const override;
  ExecutionContext* GetExecutionContext() const override;

  Member<Document> document_;
  Member<ModelContext> model_context_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_SCRIPT_TOOLS_DOM_MODEL_CONTEXT_H_
