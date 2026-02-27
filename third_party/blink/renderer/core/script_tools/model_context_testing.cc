// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/script_tools/model_context_testing.h"

#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/abort_signal.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/script_tools/model_context.h"

namespace blink {

namespace {

String GetToolErrorMessage(WebDocument::ScriptToolError error) {
  if (!error.message.IsEmpty()) {
    return error.message;
  }
  String conversion;
  switch (error.code) {
    case WebDocument::ScriptToolError::kInvalidToolName:
      conversion = "Tool was not executed due to invalid name";
      break;
    case WebDocument::ScriptToolError::kInvalidInputArguments:
      conversion = "Tool was not executed due to invalid input arguments";
      break;
    case WebDocument::ScriptToolError::kMissingRequiredSubmitButton:
      conversion =
          "Tool was not executed due to missing required submit button";
      break;
    case WebDocument::ScriptToolError::kToolInvocationFailed:
      conversion =
          "Tool was executed but the invocation failed. For example, the "
          "script function threw an error";
      break;
    case WebDocument::ScriptToolError::kToolCancelled:
      conversion = "Tool was cancelled";
      break;
    default:
      NOTREACHED();
  }
  if (error.message.IsEmpty()) {
    return conversion;
  }
  return conversion + ": " + String(error.message);
}

}  // namespace

ModelContextTesting::ModelContextTesting(ModelContext& model_context)
    : model_context_(model_context) {
  model_context_->SetToolsChangedCallback(blink::BindRepeating(
      &ModelContextTesting::OnToolsChanged, WrapWeakPersistent(this)));
}

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
         base::expected<WebString, WebDocument::ScriptToolError> result) {
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
      tool_name, input_arguments, options->getSignalOr(nullptr),
      blink::BindOnce(callback, WrapPersistent(resolver)));

  return promise;
}

void ModelContextTesting::registerToolsChangedCallback(
    V8ToolsChangedCallback* callback) {
  tools_changed_callback_ = callback;
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
      blink::BindOnce(callback, WrapPersistent(resolver)));

  return promise;
}

void ModelContextTesting::OnToolsChanged() {
  // This is a non-cancelable and non-bubbling event.
  DispatchEvent(*Event::Create(event_type_names::kToolchange));

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

const AtomicString& ModelContextTesting::InterfaceName() const {
  return event_type_names::kToolchange;
}

ExecutionContext* ModelContextTesting::GetExecutionContext() const {
  return model_context_->GetExecutionContext();
}

void ModelContextTesting::Trace(Visitor* visitor) const {
  EventTarget::Trace(visitor);
  visitor->Trace(model_context_);
  visitor->Trace(tools_changed_callback_);
}

}  // namespace blink
