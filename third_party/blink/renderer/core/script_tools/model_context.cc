// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/script_tools/model_context.h"

#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_annotations_dict.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

namespace {

String ValidateAndStringifyObject(ScriptState* script_state,
                                  const ScriptObject& input) {
  v8::Local<v8::String> value;
  if (!v8::JSON::Stringify(script_state->GetContext(), input.V8Object())
           .ToLocal(&value)) {
    return String();
  }
  return ToBlinkString<String>(script_state->GetIsolate(), value,
                               kDoNotExternalize);
}

ScriptObject JSONStringToScriptObject(ScriptState* script_state,
                                      const String& json_string) {
  v8::Local<v8::String> v8_json_string;
  if (!v8::String::NewFromUtf8(script_state->GetIsolate(),
                               json_string.Utf8().c_str())
           .ToLocal(&v8_json_string)) {
    return ScriptObject();
  }

  v8::Local<v8::Value> parsed_value;
  if (!v8::JSON::Parse(script_state->GetContext(), v8_json_string)
           .ToLocal(&parsed_value)) {
    return ScriptObject();
  }

  if (!parsed_value->IsObject()) {
    return ScriptObject();
  }

  v8::Local<v8::Object> v8_object = v8::Local<v8::Object>::Cast(parsed_value);
  return ScriptObject(script_state->GetIsolate(), v8_object);
}

}  // namespace

class ModelContext::ToolFunctionFinishedCallback
    : public ThenCallable<IDLAny, ToolFunctionFinishedCallback> {
 public:
  explicit ToolFunctionFinishedCallback(ModelContext* model_context,
                                        uint32_t execution_id,
                                        bool success)
      : model_context_(model_context),
        execution_id_(execution_id),
        success_(success) {}
  ~ToolFunctionFinishedCallback() override = default;

  void React(ScriptState* script_state, ScriptValue value) {
    std::optional<String> result;
    if (success_) {
      String temp;
      if (value.ToString(temp)) {
        result.emplace(std::move(temp));
      }
    }

    model_context_->OnToolExecuted(execution_id_, std::move(result));
  }

  void Trace(Visitor* visitor) const override {
    ThenCallable<IDLAny, ToolFunctionFinishedCallback>::Trace(visitor);
    visitor->Trace(model_context_);
  }

 private:
  Member<ModelContext> model_context_;
  const uint32_t execution_id_;
  const bool success_;
};

ModelContext::ModelContext(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : task_runner_(std::move(task_runner)) {}

void ModelContext::ForEachScriptTool(
    base::FunctionRef<void(const mojom::blink::ScriptTool&)> func) const {
  for (const auto& tool : tool_map_) {
    func(*tool.value->script_tool);
  }
}

void ModelContext::registerTool(ScriptState* script_state,
                                ToolRegistrationParams* params,
                                ExceptionState& exception_state) {
  if (!RegisterTool(script_state, params, exception_state)) {
    return;
  }
}

void ModelContext::unregisterTool(const String& tool_name,
                                  ExceptionState& exception_state) {
  auto it = tool_map_.find(tool_name);
  if (it == tool_map_.end()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Invalid tool name");
    return;
  }

  tool_map_.erase(it);
  OnToolsChanged();
}

void ModelContext::provideContext(ScriptState* script_state,
                                  ProvideContextParams* params,
                                  ExceptionState& exception_state) {
  auto prev_tool_map = std::move(tool_map_);

  for (auto tool : params->tools()) {
    if (!RegisterTool(script_state, tool, exception_state)) {
      tool_map_ = std::move(prev_tool_map);
      return;
    }
  }
}

void ModelContext::clearContext() {
  tool_map_.clear();
  OnToolsChanged();
}

void ModelContext::ExecuteTool(
    const String& name,
    const String& input_arguments,
    WebDocument::ScriptToolExecutedCallback tool_executed_cb) {
  auto it = tool_map_.find(name);

  if (it == tool_map_.end()) {
    task_runner_->PostTask(
        FROM_HERE,
        blink::BindOnce(
            std::move(tool_executed_cb),
            base::unexpected(WebDocument::ScriptToolError::kInvalidToolName)));
    return;
  }

  V8ToolFunction* tool_function = it->value->tool_function;

  ScriptState* script_state = tool_function->CallbackRelevantScriptState();
  ScriptState::Scope scope(script_state);

  auto script_object = JSONStringToScriptObject(script_state, input_arguments);
  ScriptValue script_value = script_object;
  if (script_value.IsEmpty()) {
    task_runner_->PostTask(
        FROM_HERE,
        blink::BindOnce(
            std::move(tool_executed_cb),
            base::unexpected(
                WebDocument::ScriptToolError::kInvalidInputArguments)));
    return;
  }

  v8::Maybe<ScriptPromise<IDLAny>> maybe_result =
      tool_function->Invoke(nullptr, {std::move(script_object)});

  // If the callback couldn't be run for some reason, treat it as an empty
  // promise rejected with an abort exception.
  ScriptPromise<IDLAny> result;
  if (maybe_result.IsNothing()) {
    result = ScriptPromise<IDLAny>::RejectWithDOMException(
        script_state, MakeGarbageCollected<DOMException>(
                          DOMExceptionCode::kAbortError, "Failure"));
  } else {
    result = maybe_result.FromJust();
  }

  uint32_t execution_id = ++next_execution_id_;
  pending_executions_.insert(execution_id, std::move(tool_executed_cb));

  result.Then(script_state,
              MakeGarbageCollected<ToolFunctionFinishedCallback>(
                  this, execution_id, true),
              MakeGarbageCollected<ToolFunctionFinishedCallback>(
                  this, execution_id, false));
}

bool ModelContext::RegisterTool(ScriptState* script_state,
                                ToolRegistrationParams* params,
                                ExceptionState& exception_state) {
  if (tool_map_.find(params->name()) != tool_map_.end()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Duplicate tool name");
    return false;
  }

  if (!params->name() || params->name().empty()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Tool name is required");
    return false;
  }

  if (!params->description() || params->description().empty()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Description is required");
    return false;
  }

  String input_schema;
  if (params->hasInputSchema()) {
    input_schema =
        ValidateAndStringifyObject(script_state, params->inputSchema());
    if (!input_schema) {
      exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                        "Invalid input schema");
      return false;
    }
  }

  auto* tool_data = MakeGarbageCollected<ToolData>();

  auto script_tool = mojom::blink::ScriptTool::New();
  script_tool->name = params->name();
  script_tool->description = params->description();
  script_tool->input_schema = input_schema;

  if (params->hasAnnotations()) {
    script_tool->annotations = mojom::blink::ScriptToolAnnotations::New();
    script_tool->annotations->read_only = params->annotations()->readOnlyHint();
  }

  tool_data->script_tool = std::move(script_tool);
  tool_data->tool_function = params->execute();

  tool_map_.insert(params->name(), std::move(tool_data));
  OnToolsChanged();
  return true;
}

void ModelContext::OnToolExecuted(uint32_t execution_id,
                                  std::optional<String> result) {
  auto it = pending_executions_.find(execution_id);
  CHECK(it != pending_executions_.end());

  if (result) {
    std::move(it->value).Run(*result);
  } else {
    std::move(it->value).Run(
        base::unexpected(WebDocument::ScriptToolError::kToolInvocationFailed));
  }
  pending_executions_.erase(it);
}

void ModelContext::OnToolsChanged() {
  if (tools_changed_closure_) {
    task_runner_->PostTask(FROM_HERE, *tools_changed_closure_);
  }
}

void ModelContext::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  visitor->Trace(tool_map_);
}

void ModelContext::ToolData::Trace(Visitor* visitor) const {
  visitor->Trace(tool_function);
}

}  // namespace blink
