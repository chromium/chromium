// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/script_tools/model_context_testing.h"

#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/script_tools/model_context.h"

namespace blink {

ModelContextTesting::ModelContextTesting(ModelContext* model_context)
    : model_context_(model_context) {}

HeapVector<Member<RegisteredTool>> ModelContextTesting::listTools() {
  HeapVector<Member<RegisteredTool>> tools;
  model_context_->ForEachScriptTool(
      [&tools](const mojom::blink::ScriptTool& mojom_tool) {
        auto* tool = MakeGarbageCollected<RegisteredTool>();
        tool->setName(mojom_tool.name);
        tool->setDescription(mojom_tool.description);
        tool->setInputSchema(mojom_tool.input_schema);
        tools.emplace_back(std::move(tool));
      });
  return tools;
}

ScriptPromise<IDLString> ModelContextTesting::executeTool(
    ScriptState* script_state,
    String tool_name,
    String input_arguments) {
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLString>>(script_state);

  ScriptPromise promise = resolver->Promise();

  auto callback =
      [](ScriptPromiseResolver<IDLString>* resolver,
         base::expected<WebString, WebDocument::ScriptToolError> result) {
        if (!resolver->GetScriptState() ||
            !resolver->GetScriptState()->ContextIsValid()) {
          return;
        }

        if (result.has_value()) {
          resolver->Resolve(result.value());
        } else {
          resolver->Reject(MakeGarbageCollected<DOMException>(
              DOMExceptionCode::kUnknownError, "Error executing tool."));
        }
      };

  model_context_->ExecuteTool(
      tool_name, input_arguments,
      blink::BindOnce(callback, WrapPersistent(resolver)));

  return promise;
}

void ModelContextTesting::registerToolsChangedCallback(
    V8ToolsChangedCallback* callback) {
  if (!callback) {
    tools_changed_callback_ = nullptr;
    model_context_->SetToolsChangedCallback(std::nullopt);
    return;
  }

  tools_changed_callback_ = callback;
  model_context_->SetToolsChangedCallback(blink::BindRepeating(
      &ModelContextTesting::OnToolsChanged, WrapWeakPersistent(this)));
}

void ModelContextTesting::OnToolsChanged() {
  if (!tools_changed_callback_) {
    return;
  }

  ScriptState* script_state =
      tools_changed_callback_->CallbackRelevantScriptState();
  if (!script_state || !script_state->ContextIsValid()) {
    return;
  }

  ScriptState::Scope scope(script_state);
  static_cast<void>(tools_changed_callback_->Invoke(nullptr));
}

void ModelContextTesting::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  visitor->Trace(model_context_);
  visitor->Trace(tools_changed_callback_);
}

}  // namespace blink
