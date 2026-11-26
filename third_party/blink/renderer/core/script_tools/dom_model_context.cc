// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/script_tools/dom_model_context.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/event_target_names.h"

namespace blink {

DOMModelContext::DOMModelContext(Document& document,
                                 ModelContext& model_context)
    : document_(document), model_context_(model_context) {}

ScriptPromise<IDLUndefined> DOMModelContext::registerTool(
    ScriptState* state,
    ModelContextTool* tool,
    ModelContextRegisterToolOptions* options) {
  return model_context_->registerTool(state, tool, options);
}

ScriptPromise<IDLSequence<Tool>> DOMModelContext::getTools(
    ScriptState* script_state,
    const ModelContextGetToolOptions* options) {
  return model_context_->getTools(script_state, options);
}

ScriptPromise<IDLNullable<IDLString>> DOMModelContext::executeTool(
    ScriptState* script_state,
    Tool* tool) {
  return model_context_->executeTool(script_state, tool);
}

ScriptPromise<IDLNullable<IDLString>> DOMModelContext::executeTool(
    ScriptState* script_state,
    Tool* tool,
    ScriptObject input_object,
    const ExecuteToolOptions* options) {
  return model_context_->executeTool(script_state, tool, input_object, options);
}

ScriptPromise<IDLUndefined> DOMModelContext::resumeTool(
    ScriptState* script_state,
    const String& token,
    V8ToolExecuteCallback* callback) {
  return model_context_->resumeTool(script_state, token, callback);
}

void DOMModelContext::waitForToolsUntil(ScriptState* script_state,
                                        ScriptPromise<IDLAny> promise,
                                        ExceptionState& exception_state) {
  model_context_->waitForToolsUntil(script_state, promise, exception_state);
}

const AtomicString& DOMModelContext::InterfaceName() const {
  return event_target_names::kModelContext;
}

ExecutionContext* DOMModelContext::GetExecutionContext() const {
  return document_->GetExecutionContext();
}

void DOMModelContext::Trace(Visitor* visitor) const {
  visitor->Trace(document_);
  visitor->Trace(model_context_);
  EventTarget::Trace(visitor);
}

}  // namespace blink
