// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/script_tools/model_context.h"

#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/renderer/bindings/core/v8/capture_source_location.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_annotations_dict.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_script_runner.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_tool_function.h"
#include "third_party/blink/renderer/core/dom/abort_signal.h"
#include "third_party/blink/renderer/core/dom/scoped_abort_state.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/events/web_mcp_event.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/html/html_script_element.h"
#include "third_party/blink/renderer/platform/bindings/source_location.h"
#include "third_party/blink/renderer/platform/json/json_parser.h"
#include "third_party/blink/renderer/platform/json/json_values.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

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

String ComputeScriptToolResult(const Document& document) {
  StringBuilder builder;
  builder.Append("[");

  bool first = true;
  for (HTMLScriptElement& script_element :
       Traversal<HTMLScriptElement>::DescendantsOf(document)) {
    if (static_cast<ScriptElementBase&>(script_element).TypeAttributeValue() !=
        "application/ld+json") {
      continue;
    }

    const String& json_raw = script_element.textContent();
    if (json_raw.empty()) {
      continue;
    }

    JSONParseError error;
    std::unique_ptr<JSONValue> parsed_json =
        ParseJSONWithCommentsDeprecated(json_raw, &error);
    if (!parsed_json) {
      LOG(ERROR) << "JSON parsing failed : " << error.message;
      continue;
    }

    if (!first) {
      builder.Append(",");
    }

    builder.Append(parsed_json->ToJSONString());
    first = false;
  }

  builder.Append("]");
  return builder.ToString();
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
      if (value.IsObject()) {
        v8::Local<v8::String> json_string;
        if (v8::JSON::Stringify(script_state->GetContext(), value.V8Value())
                .ToLocal(&json_string)) {
          result = ToBlinkString<String>(script_state->GetIsolate(),
                                         json_string, kDoNotExternalize);
        }
      }

      if (!result) {
        String temp;
        if (value.ToString(temp)) {
          result.emplace(std::move(temp));
        }
      }

      if (!result || result->empty()) {
        result = "Operation succeeded";
      }
    } else {
      V8ScriptRunner::ReportException(script_state->GetIsolate(),
                                      value.V8Value());
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
    Document& document,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : document_(document),
      task_runner_(std::move(task_runner)),
      script_tool_host_remote_(document.GetExecutionContext()) {}

void ModelContext::ForEachScriptTool(
    base::FunctionRef<void(const mojom::blink::ScriptTool&)> func) const {
  for (const ToolData* tool_data : ListTools()) {
    func(tool_data->ScriptTool());
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

void ModelContext::SetScriptToolDeclaration(
    const String& name,
    WebDocument::ScriptToolDeclaration* tool_declaration) const {
  auto it = tool_map_.find(name);
  if (it != tool_map_.end()) {
    const mojom::blink::ScriptTool& script_tool = it->value->ScriptTool();
    tool_declaration->description = script_tool.description;
    tool_declaration->input_schema = script_tool.input_schema;
    if (script_tool.annotations) {
      tool_declaration->read_only = script_tool.annotations->read_only;
    }
  }
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

std::optional<uint32_t> ModelContext::ExecuteTool(
    const String& name,
    const String& input_arguments,
    AbortSignal* signal,
    ScriptToolExecutedCallback tool_executed_cb) {
  auto it = tool_map_.find(name);

  if (it == tool_map_.end()) {
    task_runner_->PostTask(
        FROM_HERE,
        blink::BindOnce(std::move(tool_executed_cb),
                        base::unexpected(WebDocument::ScriptToolError(
                            WebDocument::ScriptToolError::kInvalidToolName,
                            String("Tool not found: " + name)))));
    return std::nullopt;
  }

  std::optional<uint32_t> execution_id;
  if (V8ToolFunction* v8_tool_function = it->value->GetV8ToolFunction()) {
    execution_id = ExecuteV8Tool(v8_tool_function, name, input_arguments,
                                 signal, std::move(tool_executed_cb));
  } else {
    // TODO(479598776): Add support for tracking execution of
    // declarative tools, so that they can be cancelled.
    // TODO(481899636): Add signal support for declarative tools.
    ExecuteDeclarativeTool(it->value->DeclarativeTool(), input_arguments,
                           std::move(tool_executed_cb));
  }

  // Fire the `toolactivate` event *after* activating the tool, but potentially
  // *before* the tool call finishes. Importantly, if the tool is a declarative
  // WebMCP tool, the form will be filled out synchronously above in
  // ExecuteDeclarativeTool(), so by the time the event is fired, the form will
  // be populated.
  if (LocalDOMWindow* window = document_->domWindow()) {
    // This is a synchronous, non-cancelable event.
    window->DispatchEvent(
        *WebMCPEvent::Create(event_type_names::kToolactivated, name));
  }

  return execution_id;
}

void ModelContext::CancelTool(uint32_t execution_id) {
  auto it = pending_executions_.find(execution_id);
  if (it == pending_executions_.end()) {
    return;
  }
  String tool_name = it->value.tool_name;

  if (LocalDOMWindow* window = document_->domWindow()) {
    // This is a synchronous, non-cancelable event. Note that this can re-enter
    // JavaScript and modify `pending_executions_`.
    window->DispatchEvent(
        *WebMCPEvent::Create(event_type_names::kToolcancel, tool_name));
  }

  // The pending_executions_ map might have been rehashed during DispatchEvent.
  auto pending_execution = pending_executions_.find(execution_id);
  if (pending_execution == pending_executions_.end()) {
    return;
  }
  task_runner_->PostTask(
      FROM_HERE,
      blink::BindOnce(std::move(pending_execution->value.callback),
                      base::unexpected(WebDocument::ScriptToolError(
                          WebDocument::ScriptToolError::kToolCancelled))));
  pending_executions_.erase(pending_execution);
}

void ModelContext::GetCrossDocumentScriptToolResult(
    CrossDocumentScriptToolResultCallback result_callback) {
  if (document_->HasFinishedParsing()) {
    std::move(result_callback).Run(ComputeScriptToolResult(*document_));
    return;
  }

  cross_document_result_callbacks_.push_back(std::move(result_callback));
}

void ModelContext::DidFinishParsing() {
  if (cross_document_result_callbacks_.empty()) {
    return;
  }

  auto result = ComputeScriptToolResult(*document_);
  for (auto& callback : cross_document_result_callbacks_) {
    std::move(callback).Run(result);
  }
  cross_document_result_callbacks_.clear();
}

// This overload is used for declaratively-created WebMCP tools. It passes
// the input argument JSON string to the corresponding <form> object, and
// submits the form. The result comes back one of two ways:
//   - if the form `submit` event is not preventDefaulted, then the browser
//     marks the navigation as coming from an agent-initiated submission. The
//     renderer for the navigated page will then look for a <script> with the
//     agent response type, and pass its contents back to OnToolExecuted().
//   - if the form `submit` event is preventDefaulted, and the
//     respondWith() function is called on the event, the passed Promise
//     will contain the response, once it resolves. (If the event is prevented,
//     but respondWith() isn't called, an error is reported back to the agent.)
void ModelContext::ExecuteDeclarativeTool(
    DeclarativeWebMCPTool* tool,
    const String& input_arguments,
    ScriptToolExecutedCallback tool_executed_cb) {
  tool->ExecuteTool(
      input_arguments,
      blink::BindOnce(
          [](ScriptToolExecutedCallback tool_executed_cb,
             base::expected<String, WebDocument::ScriptToolError> result) {
            std::move(tool_executed_cb).Run(result);
          },
          std::move(tool_executed_cb)));
}

// This overload is used for JS-provided tool functions. It converts the input
// argument string to a JSON object, calls the function, receives a Promise,
// waits for the promise to resolve, JSON-stringifies the result, and passes
// it to OnToolExecuted().
std::optional<uint32_t> ModelContext::ExecuteV8Tool(
    V8ToolFunction* tool_function,
    const String& name,
    const String& input_arguments,
    AbortSignal* signal,
    ScriptToolExecutedCallback tool_executed_cb) {
  UseCounter::Count(document_, WebFeature::kModelContextExecuteTool);
  ScriptState* script_state = tool_function->CallbackRelevantScriptState();
  ScriptState::Scope scope(script_state);
  v8::TryCatch try_catch(script_state->GetIsolate());

  auto script_object = JSONStringToScriptObject(script_state, input_arguments);
  ScriptValue script_value = script_object;

  if (try_catch.HasCaught() || script_value.IsEmpty()) {
    task_runner_->PostTask(
        FROM_HERE, blink::BindOnce(
                       std::move(tool_executed_cb),
                       base::unexpected(WebDocument::ScriptToolError(
                           WebDocument::ScriptToolError::kInvalidInputArguments,
                           "Failed to parse input arguments"))));
    return std::nullopt;
  }

  ScriptPromise<IDLAny> result;
  if (signal && signal->aborted()) {
    result = ScriptPromise<IDLAny>::Reject(script_state,
                                           signal->reason(script_state));
  } else {
    v8::Maybe<ScriptPromise<IDLAny>> maybe_result =
        tool_function->Invoke(nullptr, {std::move(script_object)});

    // If the callback couldn't be run for some reason, treat it as an empty
    // promise rejected with an abort exception.
    if (maybe_result.IsNothing()) {
      result = ScriptPromise<IDLAny>::RejectWithDOMException(
          script_state, MakeGarbageCollected<DOMException>(
                            DOMExceptionCode::kAbortError, "Failure"));
    } else {
      result = maybe_result.FromJust();
    }
  }

  uint32_t execution_id = ++next_execution_id_;

  // Use blink::ScopedAbortState to manage the abort algorithm lifecycle.
  // The state is wrapped in a unique_ptr and passed to the cleanup callback
  // to ensure the abort algorithm is unregistered when the tool finishes.
  std::unique_ptr<ScopedAbortState> scoped_abort_state;
  if (signal && !signal->aborted()) {
    auto callback = blink::BindOnce(&ModelContext::CancelTool,
                                    WrapWeakPersistent(this), execution_id);
    auto* handle = signal->AddAlgorithm(std::move(callback));
    scoped_abort_state = std::make_unique<ScopedAbortState>(signal, handle);
  }

  auto callback_wrapper = blink::BindOnce(
      [](ScriptToolExecutedCallback inner_cb,
         std::unique_ptr<ScopedAbortState> scoped_abort_state,
         base::expected<WebString, WebDocument::ScriptToolError> result) {
        // ScopedAbortState is destroyed here, unregistering the algorithm.
        std::move(inner_cb).Run(result);
      },
      std::move(tool_executed_cb), std::move(scoped_abort_state));
  pending_executions_.insert(
      execution_id, PendingExecution{.tool_name = name,
                                     .callback = std::move(callback_wrapper)});

  result.Then(script_state,
              MakeGarbageCollected<ToolFunctionFinishedCallback>(
                  this, execution_id, true),
              MakeGarbageCollected<ToolFunctionFinishedCallback>(
                  this, execution_id, false));
  return execution_id;
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

  auto script_tool = mojom::blink::ScriptTool::New();
  script_tool->name = params->name();
  script_tool->description = params->description();
  script_tool->input_schema = input_schema;

  if (params->hasAnnotations()) {
    script_tool->annotations = mojom::blink::ScriptToolAnnotations::New();
    script_tool->annotations->read_only = params->annotations()->readOnlyHint();
  }

  auto* tool_data = MakeGarbageCollected<ToolData>(
      base::PassKey<ModelContext>(), std::move(script_tool),
      /*v8_tool_function=*/params->execute(),
      CaptureSourceLocation(ExecutionContext::From(script_state)));

  tool_map_.insert(params->name(), tool_data);
  OnToolsChanged();
  UseCounter::Count(document_, WebFeature::kModelContextRegisterTool);
  return true;
}

void ModelContext::RegisterDeclarativeTool(
    String name,
    String description,
    DeclarativeWebMCPTool* declarative_tool) {
  auto script_tool = mojom::blink::ScriptTool::New();
  script_tool->name = name;
  script_tool->description = description;
  script_tool->input_schema = "{}";  // For now

  auto* tool_data = MakeGarbageCollected<ToolData>(
      base::PassKey<ModelContext>(), std::move(script_tool), declarative_tool);
  tool_map_.insert(name, std::move(tool_data));
  OnToolsChanged();
  UseCounter::Count(document_,
                    WebFeature::kModelContextRegisterDeclarativeTool);
}

void ModelContext::OnToolExecuted(uint32_t execution_id,
                                  std::optional<String> result) {
  auto it = pending_executions_.find(execution_id);
  if (it == pending_executions_.end()) {
    return;
  }

  if (result) {
    std::move(it->value.callback).Run(*result);
  } else {
    std::move(it->value.callback)
        .Run(base::unexpected(WebDocument::ScriptToolError(
            WebDocument::ScriptToolError::kToolInvocationFailed)));
  }
  pending_executions_.erase(it);
}

void ModelContext::OnToolsChanged() {
  if (tools_changed_closure_) {
    task_runner_->PostTask(FROM_HERE, *tools_changed_closure_);
  }
}

void ModelContext::PauseExecution() {
  if (!script_tool_host_remote_.is_bound()) {
    document_->GetExecutionContext()->GetBrowserInterfaceBroker().GetInterface(
        script_tool_host_remote_.BindNewPipeAndPassReceiver(task_runner_));
  }
  script_tool_host_remote_->PauseExecution();
}

HeapVector<Member<const ModelContext::ToolData>> ModelContext::ListTools()
    const {
  HeapVector<Member<const ToolData>> tools;
  tools.ReserveInitialCapacity(tool_map_.size());

  for (const auto& entry : tool_map_) {
    ToolData* tool_data = entry.value;
    CHECK(tool_data);
    // Always update the input schema of declarative tools,
    // since the DOM might have changed.
    tool_data->RefreshDeclarativeInputSchema();
    tools.push_back(tool_data);
  }

  std::sort(tools.begin(), tools.end(),
            [](const ToolData* a, const ToolData* b) {
              return CodeUnitCompareLessThan(a->Name(), b->Name());
            });

  return tools;
}

ExecutionContext* ModelContext::GetExecutionContext() const {
  return document_ ? document_->GetExecutionContext() : nullptr;
}

void ModelContext::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  visitor->Trace(tool_map_);
  visitor->Trace(document_);
  visitor->Trace(script_tool_host_remote_);
}

const String& ModelContext::ToolData::Name() const {
  return script_tool_->name;
}

SourceLocation* ModelContext::ToolData::GetSourceLocation() const {
  return source_location_;
}

Element* ModelContext::ToolData::BackingFormElement() const {
  return declarative_tool_ ? declarative_tool_->FormElement() : nullptr;
}

void ModelContext::ToolData::Trace(Visitor* visitor) const {
  visitor->Trace(v8_tool_function_);
  visitor->Trace(declarative_tool_);
  visitor->Trace(source_location_);
}

void ModelContext::ToolData::RefreshDeclarativeInputSchema() {
  if (declarative_tool_) {
    script_tool_->input_schema = declarative_tool_->ComputeInputSchema();
  }
}

}  // namespace blink
