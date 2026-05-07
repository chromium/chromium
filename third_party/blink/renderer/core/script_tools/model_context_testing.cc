// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/script_tools/model_context_testing.h"

#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/abort_signal.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/script_tools/model_context.h"
#include "third_party/blink/renderer/core/script_tools/script_tool_types.h"

namespace blink {

ModelContextTesting::ModelContextTesting(ModelContext& model_context)
    : model_context_(model_context) {
  model_context_->SetToolChangeCallback(blink::BindRepeating(
      &ModelContextTesting::OnToolChange, WrapWeakPersistent(this)));
}

HeapVector<Member<RegisteredToolDeprecated>> ModelContextTesting::listTools() {
  HeapVector<Member<RegisteredToolDeprecated>> tools;
  model_context_->ForEachScriptTool(
      [&tools](const mojom::blink::ScriptTool& mojom_tool) {
        auto* tool = MakeGarbageCollected<RegisteredToolDeprecated>();
        tool->setName(mojom_tool.name);
        tool->setDescription(mojom_tool.description);
        tool->setInputSchema(mojom_tool.input_schema);
        tools.emplace_back(std::move(tool));
      });
  return tools;
}

ScriptPromise<IDLNullable<IDLString>> ModelContextTesting::executeTool(
    ScriptState* script_state,
    String tool_name,
    String input_arguments,
    const ExecuteToolOptions* options) {
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLNullable<IDLString>>>(
          script_state);

  ScriptPromise promise = resolver->Promise();

  auto callback =
      [](ScriptPromiseResolver<IDLNullable<IDLString>>* resolver,
         base::expected<String, ScriptToolError> result) {
        if (!resolver->GetScriptState() ||
            !resolver->GetScriptState()->ContextIsValid()) {
          return;
        }

        if (result.has_value()) {
          resolver->Resolve(result.value());
        } else {
          resolver->Reject(MakeGarbageCollected<DOMException>(
              DOMExceptionCode::kUnknownError,
              GetToolErrorMessage(result.error())));
        }
      };

  model_context_->ExecuteTool(
      /*invocation_id=*/base::UnguessableToken::Create(), tool_name,
      input_arguments, options->getSignalOr(nullptr),
      blink::BindOnce(callback, WrapPersistent(resolver)));

  return promise;
}

ScriptPromise<IDLString> ModelContextTesting::getCrossDocumentScriptToolResult(
    ScriptState* script_state) {
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLString>>(script_state);

  ScriptPromise promise = resolver->Promise();

  auto callback = [](ScriptPromiseResolver<IDLString>* resolver,
                     String result) {
    if (!resolver->GetScriptState() ||
        !resolver->GetScriptState()->ContextIsValid()) {
      return;
    }

    resolver->Resolve(result);
  };

  model_context_->GetCrossDocumentScriptToolResult(
      base::UnguessableToken::Create(),
      blink::BindOnce(callback, WrapPersistent(resolver)));

  return promise;
}

void ModelContextTesting::OnToolChange() {
  // This is a non-cancelable and non-bubbling event.
  DispatchEvent(*Event::Create(event_type_names::kToolchange));
}

const AtomicString& ModelContextTesting::InterfaceName() const {
  return event_type_names::kToolchange;
}

ExecutionContext* ModelContextTesting::GetExecutionContext() const {
  return model_context_->GetExecutionContext();
}

void ModelContextTesting::Trace(Visitor* visitor) const {
  EventTarget::Trace(visitor);
  visitor->Trace(model_context_);
}

}  // namespace blink
