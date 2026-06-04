// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/script_tools/model_context.h"

#include "base/metrics/histogram_functions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/public/web/web_script_tool_types.h"
#include "third_party/blink/renderer/bindings/core/v8/capture_source_location.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_model_context_get_tool_options.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_model_context_register_tool_options.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_model_context_tool.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_registered_tool.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_script_runner.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_tool_annotations.h"
#include "third_party/blink/renderer/core/dom/abort_signal.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/scoped_abort_state.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/events/web_mcp_event.h"
#include "third_party/blink/renderer/core/execution_context/agent.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/html/html_script_element.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/script_tools/script_tool_context.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/source_location.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/json/json_parser.h"
#include "third_party/blink/renderer/platform/json/json_values.h"
#include "third_party/blink/renderer/platform/scheduler/public/task_attribution_tracker.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/text/ascii_ctype.h"
#include "third_party/blink/renderer/platform/wtf/text/strcat.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

STATIC_ASSERT_ENUM(ScriptToolErrorCode::kInvalidToolName,
                   WebScriptToolErrorCode::kInvalidToolName);
STATIC_ASSERT_ENUM(ScriptToolErrorCode::kInvalidInputArguments,
                   WebScriptToolErrorCode::kInvalidInputArguments);
STATIC_ASSERT_ENUM(ScriptToolErrorCode::kMissingRequiredSubmitButton,
                   WebScriptToolErrorCode::kMissingRequiredSubmitButton);
STATIC_ASSERT_ENUM(ScriptToolErrorCode::kToolInvocationFailed,
                   WebScriptToolErrorCode::kToolInvocationFailed);
STATIC_ASSERT_ENUM(ScriptToolErrorCode::kToolCancelled,
                   WebScriptToolErrorCode::kToolCancelled);

namespace {

const char kPermissionPolicyNotEnabledError[] =
    "Access to the feature \"tools\" is disallowed by permissions policy.";

String ValidateAndStringifyObject(ScriptState* script_state,
                                  ExceptionState& exception_state,
                                  const ScriptObject& input) {
  v8::Local<v8::String> value;
  TryRethrowScope rethrow_scope(script_state->GetIsolate(), exception_state);
  if (!v8::JSON::Stringify(script_state->GetContext(), input.V8Object())
           .ToLocal(&value)) {
    CHECK(rethrow_scope.HasCaught());
    return String();
  }
  String result = ToBlinkString<String>(script_state->GetIsolate(), value,
                                        kDoNotExternalize);
  // JSON.stringify() can fail to produce a string in one of two ways:
  //   1. It can throw an exception (as with unserializable objects), which is
  //   handled by `rethrow_scope` above; or
  //   2. It can return the `undefined` JavaScript value, which stringifies as
  //   the String literal "undefined". We need to check for this, and consider
  //   it a failure.
  // This matches the semantics of
  // https://infra.spec.whatwg.org/#serialize-a-javascript-value-to-a-json-string,
  // which the spec uses in
  // https://webmachinelearning.github.io/webmcp/#dom-modelcontext-registertool.
  if (result == "undefined") {
    exception_state.ThrowTypeError(
        "invalid input schema: toJSON() returns undefined");
    return String();
  }

  return result;
}

bool IsValidToolName(const String& name) {
  if (name.empty() || name.length() > 128) {
    return false;
  }
  for (wtf_size_t i = 0; i < name.length(); ++i) {
    UChar c = name[i];
    if (!IsAsciiAlphanumeric(c) && c != '_' && c != '-' && c != '.') {
      return false;
    }
  }

  return true;
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

class ModelContext::ToolUnregisterAbortAlgorithm final
    : public AbortSignal::Algorithm {
 public:
  ToolUnregisterAbortAlgorithm(ModelContext* model_context,
                               const String& tool_name)
      : model_context_(model_context), tool_name_(tool_name) {}

  void Run() override { model_context_->UnregisterTool(tool_name_); }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(model_context_);
    AbortSignal::Algorithm::Trace(visitor);
  }

 private:
  Member<ModelContext> model_context_;
  String tool_name_;
};

class ModelContext::ToolFunctionFinishedCallback
    : public ThenCallable<IDLAny, ToolFunctionFinishedCallback> {
 public:
  explicit ToolFunctionFinishedCallback(
      ModelContext* model_context,
      const base::UnguessableToken& invocation_id,
      bool success)
      : model_context_(model_context),
        invocation_id_(invocation_id),
        success_(success) {}
  ~ToolFunctionFinishedCallback() override = default;

  void React(ScriptState* script_state, ScriptValue value) {
    if (success_) {
      std::optional<String> result;
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
      model_context_->OnToolExecuted(invocation_id_, *result);
    } else {
      V8ScriptRunner::ReportException(script_state->GetIsolate(),
                                      value.V8Value());
      model_context_->OnToolExecuted(
          invocation_id_,
          base::unexpected(std::make_pair(value, script_state)));
    }
  }

  void Trace(Visitor* visitor) const override {
    ThenCallable<IDLAny, ToolFunctionFinishedCallback>::Trace(visitor);
    visitor->Trace(model_context_);
  }

 private:
  Member<ModelContext> model_context_;
  const base::UnguessableToken invocation_id_;
  const bool success_;
};

ModelContext::ModelContext(
    Document& document,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : document_(document),
      task_runner_(std::move(task_runner)),
      script_tool_host_remote_(document.GetExecutionContext()),
      model_context_host_remote_(document.GetExecutionContext()),
      model_context_receiver_(this, document.GetExecutionContext()) {
  document.GetExecutionContext()->GetBrowserInterfaceBroker().GetInterface(
      model_context_host_remote_.BindNewPipeAndPassReceiver(task_runner_));
  model_context_host_remote_->BindModelContext(
      model_context_receiver_.BindNewPipeAndPassRemote(task_runner_));
}

void ModelContext::ForEachScriptTool(
    base::FunctionRef<void(const mojom::blink::ScriptTool&)> func) const {
  for (const ToolData* tool_data : ListTools()) {
    func(tool_data->ScriptTool());
  }
}

void ModelContext::registerTool(ScriptState* script_state,
                                ModelContextTool* tool,
                                ModelContextRegisterToolOptions* options,
                                ExceptionState& exception_state) {
  if (!document_->IsActive()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "The document is detached.");
    return;
  }

  if (!ExecutionContext::From(script_state)
           ->IsFeatureEnabled(
               network::mojom::PermissionsPolicyFeature::kTools)) {
    exception_state.ThrowSecurityError(kPermissionPolicyNotEnabledError);
    return;
  }

  if (tool_map_.find(tool->name()) != tool_map_.end()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Duplicate tool name");
    return;
  }

  if (!IsValidToolName(tool->name())) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Invalid tool name");
    return;
  }

  if (!tool->description() || tool->description().empty()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Description is required");
    return;
  }

  String input_schema;
  if (tool->hasInputSchema()) {
    input_schema = ValidateAndStringifyObject(script_state, exception_state,
                                              tool->inputSchema());
    if (!input_schema) {
      // Exception already thrown by ValidateAndStringifyObject
      return;
    }
  }

  AbortSignal::AlgorithmHandle* abort_handle = nullptr;
  if (options && options->hasSignal()) {
    AbortSignal* signal = options->signal();
    if (signal->aborted()) {
      ExecutionContext::From(script_state)
          ->AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
              mojom::blink::ConsoleMessageSource::kJavaScript,
              mojom::blink::ConsoleMessageLevel::kWarning,
              "Tool '" + tool->name() +
                  "' was not registered because its AbortSignal was already "
                  "aborted."));
      return;
    }

    // Grab the `AlgorithmHandle` and tie its lifetime to `ToolData` farther
    // below.
    abort_handle = signal->AddAlgorithm(
        MakeGarbageCollected<ToolUnregisterAbortAlgorithm>(this, tool->name()));
  }

  auto script_tool = mojom::blink::ScriptTool::New();
  script_tool->name = tool->name();
  script_tool->description = tool->description();
  script_tool->input_schema = input_schema;
  // TODO(https://crbug.com/509568047): Stop setting these two members.
  script_tool->tool_owner_frame_token = document_->GetFrame()->GetFrameToken();
  script_tool->origin = document_->GetExecutionContext()->GetSecurityOrigin();

  Vector<scoped_refptr<const SecurityOrigin>> exposed_origins;
  if (options && options->hasExposedTo()) {
    for (const String& origin_str : options->exposedTo()) {
      scoped_refptr<const SecurityOrigin> origin =
          SecurityOrigin::CreateFromString(origin_str);
      if (origin->Protocol() != "https") {
        exception_state.ThrowSecurityError(
            "Only HTTPS origins are allowed in exposedTo list.");
        return;
      }
      exposed_origins.push_back(origin);
    }
  }
  script_tool->exposed_origins = std::move(exposed_origins);

  if (tool->hasAnnotations()) {
    script_tool->annotations = mojom::blink::ScriptToolAnnotations::New();
    CHECK(tool->annotations()->hasReadOnlyHint());
    script_tool->annotations->read_only = tool->annotations()->readOnlyHint();
    CHECK(tool->annotations()->hasUntrustedContentHint());
    script_tool->annotations->untrusted_content =
        tool->annotations()->untrustedContentHint();
  }

  auto* tool_data = MakeGarbageCollected<ToolData>(
      base::PassKey<ModelContext>(), std::move(script_tool),
      /*v8_tool_function=*/tool->execute(),
      CaptureSourceLocation(ExecutionContext::From(script_state)),
      abort_handle);

  tool_map_.insert(tool->name(), tool_data);
  model_context_host_remote_->RegisterScriptTool(
      tool_data->ScriptTool().Clone());
  probe::WebMCPToolAdded(document_, *tool_data);
  MaybeRecordToolCount();
}

void ModelContext::UnregisterTool(const String& name) {
  auto it = tool_map_.find(name);
  if (it == tool_map_.end()) {
    return;
  }

  probe::WebMCPToolRemoved(document_, *it->value);
  tool_map_.erase(it);
  model_context_host_remote_->UnregisterScriptTool(name);
}

std::optional<ScriptToolDeclaration> ModelContext::GetScriptToolDeclaration(
    const String& name) const {
  auto it = tool_map_.find(name);
  if (it == tool_map_.end()) {
    return std::nullopt;
  }
  ScriptToolDeclaration declaration;
  const mojom::blink::ScriptTool& script_tool = it->value->ScriptTool();
  declaration.description = script_tool.description;
  declaration.input_schema = script_tool.input_schema;
  if (script_tool.annotations) {
    declaration.read_only = script_tool.annotations->read_only;
    declaration.untrusted_content = script_tool.annotations->untrusted_content;
  }
  return declaration;
}

void ModelContext::OnToolFailed(ScriptToolExecutedCallback callback,
                                const base::UnguessableToken& invocation_id,
                                ScriptToolError&& error) {
  probe::WebMCPToolFailed(document_, error, invocation_id,
                          /*exception=*/std::nullopt);
  task_runner_->PostTask(
      FROM_HERE,
      blink::BindOnce(std::move(callback), base::unexpected(std::move(error))));
}

bool ModelContext::ExecuteTool(const base::UnguessableToken& invocation_id,
                               const String& name,
                               const String& input_arguments,
                               AbortSignal* signal,
                               ScriptToolExecutedCallback tool_executed_cb) {
  probe::WebMCPToolExecuted(document_, name, input_arguments, invocation_id);

  auto it = tool_map_.find(name);
  if (it == tool_map_.end()) {
    OnToolFailed(std::move(tool_executed_cb), invocation_id,
                 ScriptToolError(ScriptToolErrorCode::kInvalidToolName,
                                 String("Tool not found: " + name)));
    return false;
  }

  bool success = true;
  if (V8ToolExecuteCallback* v8_tool_function =
          it->value->GetV8ToolExecuteCallback()) {
    success =
        ExecuteV8Tool(v8_tool_function, invocation_id, name, input_arguments,
                      signal, std::move(tool_executed_cb));
  } else {
    // TODO(481899636): Add signal support for declarative tools.
    ExecuteDeclarativeTool(it->value->DeclarativeTool(), invocation_id,
                           input_arguments, std::move(tool_executed_cb));
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

  return success;
}

bool ModelContext::CancelTool(const base::UnguessableToken& invocation_id) {
  auto it = pending_executions_.find(String(invocation_id.ToString()));
  if (it == pending_executions_.end()) {
    return false;
  }
  String tool_name = it->value.tool_name;

  if (LocalDOMWindow* window = document_->domWindow()) {
    // This is a synchronous, non-cancelable event. Note that this can re-enter
    // JavaScript and modify `pending_executions_`.
    window->DispatchEvent(
        *WebMCPEvent::Create(event_type_names::kToolcancel, tool_name));
  }

  // The pending_executions_ map might have been rehashed during DispatchEvent.
  auto pending_execution =
      pending_executions_.find(String(invocation_id.ToString()));
  if (pending_execution == pending_executions_.end()) {
    return false;
  }
  OnToolFailed(std::move(pending_execution->value.callback), invocation_id,
               ScriptToolError(ScriptToolErrorCode::kToolCancelled));
  pending_executions_.erase(pending_execution);
  return true;
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
    const base::UnguessableToken& invocation_id,
    const String& input_arguments,
    ScriptToolExecutedCallback tool_executed_cb) {
  // TODO(479598776): Add support for tracking execution of
  // declarative tools in pending_executions_, so that they can be cancelled.
  std::optional<scheduler::TaskAttributionTracker::TaskScope> task_scope;
  if (auto* tracker = document_->GetAgent().isolate()
                          ? scheduler::TaskAttributionTracker::From(
                                document_->GetAgent().isolate())
                          : nullptr) {
    task_scope = tracker->SetTaskStateVariable(
        MakeGarbageCollected<ScriptToolContext>(invocation_id));
  }
  tool->ExecuteTool(
      invocation_id, input_arguments,
      blink::BindOnce(
          [](Document* document, base::UnguessableToken invocation_id,
             ScriptToolExecutedCallback tool_executed_cb,
             base::expected<String, ScriptToolError> result) {
            if (result.has_value()) {
              // A null string indicates a cross-document navigation, in which
              // case we don't want to emit a toolResponded event here. The
              // new document will handle the response.
              if (!result->IsNull()) {
                probe::WebMCPToolResponded(document, *result, invocation_id);
              }
            } else {
              probe::WebMCPToolFailed(document, result.error(), invocation_id,
                                      /*exception=*/std::nullopt);
            }
            std::move(tool_executed_cb).Run(result);
          },
          WrapWeakPersistent(document_.Get()), invocation_id,
          std::move(tool_executed_cb)));
}

// This overload is used for JS-provided tool functions. It converts the input
// argument string to a JSON object, calls the function, receives a Promise,
// waits for the promise to resolve, JSON-stringifies the result, and passes
// it to OnToolExecuted().
bool ModelContext::ExecuteV8Tool(V8ToolExecuteCallback* tool_function,
                                 const base::UnguessableToken& invocation_id,
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
    OnToolFailed(std::move(tool_executed_cb), invocation_id,
                 ScriptToolError(ScriptToolErrorCode::kInvalidInputArguments,
                                 "Failed to parse input arguments"));
    return false;
  }

  ScriptPromise<IDLAny> result;
  if (signal && signal->aborted()) {
    result = ScriptPromise<IDLAny>::Reject(script_state,
                                           signal->reason(script_state));
  } else {
    std::optional<scheduler::TaskAttributionTracker::TaskScope> task_scope;
    if (auto* tracker = scheduler::TaskAttributionTracker::From(
            script_state->GetIsolate())) {
      task_scope = tracker->SetTaskStateVariable(
          MakeGarbageCollected<ScriptToolContext>(invocation_id));
    }
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

  // Use blink::ScopedAbortState to manage the abort algorithm lifecycle.
  // The state is wrapped in a unique_ptr and passed to the cleanup callback
  // to ensure the abort algorithm is unregistered when the tool finishes.
  std::unique_ptr<ScopedAbortState> scoped_abort_state;
  if (signal && !signal->aborted()) {
    auto callback =
        blink::BindOnce(base::IgnoreResult(&ModelContext::CancelTool),
                        WrapWeakPersistent(this), invocation_id);
    auto* handle = signal->AddAlgorithm(std::move(callback));
    scoped_abort_state = std::make_unique<ScopedAbortState>(signal, handle);
  }

  auto callback_wrapper = blink::BindOnce(
      [](ScriptToolExecutedCallback inner_cb,
         std::unique_ptr<ScopedAbortState> scoped_abort_state,
         base::expected<String, ScriptToolError> result) {
        // ScopedAbortState is destroyed here, unregistering the algorithm.
        std::move(inner_cb).Run(result);
      },
      std::move(tool_executed_cb), std::move(scoped_abort_state));
  pending_executions_.insert(
      String(invocation_id.ToString()),
      PendingExecution{.tool_name = name,
                       .callback = std::move(callback_wrapper),
                       .invocation_id = invocation_id});

  result.Then(script_state,
              MakeGarbageCollected<ToolFunctionFinishedCallback>(
                  this, invocation_id, true),
              MakeGarbageCollected<ToolFunctionFinishedCallback>(
                  this, invocation_id, false));
  return true;
}

void ModelContext::RegisterDeclarativeTool(
    String name,
    String description,
    DeclarativeWebMCPTool* declarative_tool) {
  if (!document_->GetExecutionContext()->IsFeatureEnabled(
          network::mojom::PermissionsPolicyFeature::kTools)) {
    // TODO(crbug.com/507724727) Surface an error if the `tools` permission
    // policy is not enabled
    return;
  }

  // TODO(https://crbug.com/509983792): Surface an error if the tool's name is
  // not valid.
  UseCounter::Count(document_,
                    WebFeature::kModelContextRegisterDeclarativeTool);

  auto script_tool = mojom::blink::ScriptTool::New();
  script_tool->name = name;
  script_tool->description = description;
  script_tool->input_schema = declarative_tool->ComputeInputSchema();
  // TODO(https://crbug.com/509568047): Stop setting these two members.
  script_tool->tool_owner_frame_token = document_->GetFrame()->GetFrameToken();
  script_tool->origin = document_->GetExecutionContext()->GetSecurityOrigin();

  auto* tool_data = MakeGarbageCollected<ToolData>(
      base::PassKey<ModelContext>(), std::move(script_tool), declarative_tool);

  tool_map_.insert(name, tool_data);
  model_context_host_remote_->RegisterScriptTool(
      tool_data->ScriptTool().Clone());
  probe::WebMCPToolAdded(document_, *tool_data);
  MaybeRecordToolCount();
}

void ModelContext::OnToolExecuted(
    const base::UnguessableToken& invocation_id,
    base::expected<String, std::pair<ScriptValue, ScriptState*>> result) {
  auto it = pending_executions_.find(String(invocation_id.ToString()));
  if (it == pending_executions_.end()) {
    return;
  }

  if (result.has_value()) {
    probe::WebMCPToolResponded(document_, result.value(), invocation_id);
    std::move(it->value.callback).Run(result.value());
  } else {
    ScriptToolError error(ScriptToolErrorCode::kToolInvocationFailed);
    probe::WebMCPToolFailed(document_, error, invocation_id, result.error());
    std::move(it->value.callback).Run(base::unexpected(error));
  }
  pending_executions_.erase(it);
}

void ModelContext::MaybeRecordToolCount() {
  if (!will_record_tool_count_) {
    will_record_tool_count_ = true;
    task_runner_->PostDelayedTask(
        FROM_HERE,
        blink::BindOnce(
            [](ModelContext* context) {
              if (context) {
                base::UmaHistogramCounts100(
                    "Blink.ModelContext.DelayedToolCount",
                    context->tool_map_.size());
              }
            },
            WrapWeakPersistent(this)),
        base::Seconds(10));
  }
}

void ModelContext::PauseExecution() {
  if (!script_tool_host_remote_.is_bound()) {
    document_->GetExecutionContext()->GetBrowserInterfaceBroker().GetInterface(
        script_tool_host_remote_.BindNewPipeAndPassReceiver(task_runner_));
  }
  script_tool_host_remote_->PauseExecution();
}

void ModelContext::NotifyToolChange() {
  if (tool_change_closure_) {
    (*tool_change_closure_).Run();
  }

  // The above closure fires the `toolchange` on the `ModelContextTesting`
  // interface. Even if it detaches the document, it is still safe to dispatch
  // the event on `this` as well.
  DispatchEvent(*Event::Create(event_type_names::kToolchange));
}

void ModelContext::ExecuteScriptTool(const String& name,
                                     const String& input_arguments,
                                     ExecuteScriptToolCallback callback) {
  // TODO(http://b/485810761): Pass `invocation_id` up from the browser, instead
  // of generating it in the renderer.
  ExecuteTool(
      /*invocation_id=*/base::UnguessableToken::Create(), name, input_arguments,
      /*signal=*/nullptr,
      blink::BindOnce(
          [](ExecuteScriptToolCallback callback,
             base::expected<String, ScriptToolError> result) {
            if (result.has_value()) {
              std::move(callback).Run(result.value(), true);
            } else {
              std::move(callback).Run(GetToolErrorMessage(result.error()),
                                      false);
            }
          },
          std::move(callback)));
}

HeapVector<Member<const ToolData>> ModelContext::ListTools() const {
  HeapVector<Member<const ToolData>> tools;
  tools.ReserveInitialCapacity(tool_map_.size());

  for (const auto& entry : tool_map_) {
    ToolData* tool_data = entry.value;
    CHECK(tool_data);
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

const AtomicString& ModelContext::InterfaceName() const {
  DEFINE_STATIC_LOCAL(AtomicString, name, ("ModelContext"));
  return name;
}

ScriptPromise<IDLSequence<RegisteredTool>> ModelContext::getTools(
    ScriptState* script_state,
    const ModelContextGetToolOptions* options) {
  if (!document_->IsActive()) {
    return ScriptPromise<IDLSequence<RegisteredTool>>::RejectWithDOMException(
        script_state,
        MakeGarbageCollected<DOMException>(DOMExceptionCode::kInvalidStateError,
                                           "The document is not active."));
  }

  Vector<scoped_refptr<const SecurityOrigin>> from_origins;
  if (options && options->hasFromOrigins()) {
    for (const String& origin_str : options->fromOrigins()) {
      scoped_refptr<const SecurityOrigin> origin =
          SecurityOrigin::CreateFromString(origin_str);
      if (!origin->IsPotentiallyTrustworthy()) {
        return ScriptPromise<IDLSequence<RegisteredTool>>::
            RejectWithDOMException(script_state,
                                   MakeGarbageCollected<DOMException>(
                                       DOMExceptionCode::kSecurityError,
                                       "Only secure origins are allowed in the "
                                       "fromOrigins list."));
      }
      from_origins.push_back(origin);
    }
  }

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLSequence<RegisteredTool>>>(
          script_state);
  ScriptPromise promise = resolver->Promise();

  if (!ExecutionContext::From(script_state)
           ->IsFeatureEnabled(
               network::mojom::PermissionsPolicyFeature::kTools)) {
    resolver->RejectWithSecurityError(kPermissionPolicyNotEnabledError,
                                      kPermissionPolicyNotEnabledError);
    return promise;
  }

  model_context_host_remote_->GetScriptTools(
      std::move(from_origins),
      blink::BindOnce(&ModelContext::OnGetScriptToolsCompleted,
                      WrapWeakPersistent(this), WrapPersistent(resolver)));

  return promise;
}

void ModelContext::OnGetScriptToolsCompleted(
    ScriptPromiseResolver<IDLSequence<RegisteredTool>>* resolver,
    Vector<mojom::blink::ScriptToolPtr> tools) {
  HeapVector<Member<RegisteredTool>> registered_tools;
  registered_tools.ReserveInitialCapacity(tools.size());

  for (const auto& t : tools) {
    auto* result = RegisteredTool::Create();
    result->setName(t->name);
    result->setDescription(t->description);
    if (!t->input_schema.IsNull()) {
      result->setInputSchema(t->input_schema);
    }
    if (t->annotations) {
      auto* annotations = ToolAnnotations::Create();
      annotations->setReadOnlyHint(t->annotations->read_only);
      annotations->setUntrustedContentHint(t->annotations->untrusted_content);
      result->setAnnotations(annotations);
    }

    Frame* frame = Frame::ResolveFrame(t->tool_owner_frame_token);
    // If we can't resolve the token into a concrete frame, that means the
    // document could have been discarded by the time the response IPC comes
    // back to the renderer. In that case, the tool is unusable from our
    // perspective, so exclude it from `registered_tools`.
    if (!frame) {
      continue;
    }

    result->setWindow(frame->DomWindow());
    result->setOrigin(t->origin->ToString());
    registered_tools.push_back(result);
  }

  resolver->Resolve(registered_tools);
}

ScriptPromise<IDLNullable<IDLString>> ModelContext::executeTool(
    ScriptState* script_state,
    RegisteredTool* tool,
    String input_arguments,
    const ExecuteToolOptions* options) {
  if (!document_->IsActive()) {
    return ScriptPromise<IDLNullable<IDLString>>::RejectWithDOMException(
        script_state,
        MakeGarbageCollected<DOMException>(DOMExceptionCode::kInvalidStateError,
                                           "The document is not active."));
  }

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLNullable<IDLString>>>(
          script_state);
  ScriptPromise promise = resolver->Promise();

  if (!ExecutionContext::From(script_state)
           ->IsFeatureEnabled(
               network::mojom::PermissionsPolicyFeature::kTools)) {
    resolver->RejectWithSecurityError(kPermissionPolicyNotEnabledError,
                                      kPermissionPolicyNotEnabledError);
    return promise;
  }

  DOMWindow* window = tool->window();
  // `window` is always non-null, but its frame might be missing if the document
  // was detached or discarded in the gap between tool retrieval and tool
  // execution.
  CHECK(window);

  Frame* target_frame = window->GetFrame();
  if (!target_frame) {
    resolver->RejectWithDOMException(DOMExceptionCode::kInvalidStateError,
                                     "Target frame is detached.");
    return promise;
  }
  blink::FrameToken frame_token = target_frame->GetFrameToken();

  // Because the document is active, we know `local_frame` is non-null.
  LocalFrame* local_frame = document_->GetFrame();
  CHECK(local_frame);

  std::unique_ptr<ScopedAbortState> scoped_abort_state;
  if (options && options->hasSignal()) {
    AbortSignal* signal = options->signal();
    if (signal->aborted()) {
      resolver->RejectWithDOMException(DOMExceptionCode::kAbortError,
                                       "Execution cancelled.");
      return promise;
    }

    auto* handle = signal->AddAlgorithm(BindOnce(
        [](ScriptPromiseResolver<IDLNullable<IDLString>>* resolver,
           ScriptState* script_state, AbortSignal* signal) {
          if (resolver->GetScriptState() &&
              resolver->GetScriptState()->ContextIsValid()) {
            resolver->Reject(signal->reason(script_state));
          }
        },
        WrapPersistent(resolver), WrapPersistent(script_state),
        WrapPersistent(signal)));

    scoped_abort_state = std::make_unique<ScopedAbortState>(signal, handle);
  }

  model_context_host_remote_->ExecuteRemoteScriptTool(
      frame_token, SecurityOrigin::CreateFromString(tool->origin()),
      tool->name(), input_arguments,
      blink::BindOnce(
          [](ModelContext* self,
             ScriptPromiseResolver<IDLNullable<IDLString>>* resolver,
             std::unique_ptr<ScopedAbortState> abort_state,
             const String& result, bool success) {
            if (self) {
              self->OnExecuteScriptToolCompleted(resolver, result, success);
            }
          },
          WrapWeakPersistent(this), WrapPersistent(resolver),
          std::move(scoped_abort_state)));
  return promise;
}

void ModelContext::OnExecuteScriptToolCompleted(
    ScriptPromiseResolver<IDLNullable<IDLString>>* resolver,
    const String& result,
    bool success) {
  // For the execution result to have been received from the browser process
  // over mojo, the frame/Document that sent the execution must not be detached,
  // and the same goes for the `resolver` that is tied to those objects.
  CHECK(resolver->GetScriptState() &&
        resolver->GetScriptState()->ContextIsValid());

  // `result` is either the result or the error string, so we can use it
  // unconditionally below.
  if (success) {
    resolver->Resolve(result);
  } else {
    // TODO(https://crbug.com/509555636): Support more granular execution error
    // reasons.
    resolver->RejectWithDOMException(DOMExceptionCode::kUnknownError, result);
  }
}

void ModelContext::Trace(Visitor* visitor) const {
  EventTarget::Trace(visitor);
  visitor->Trace(tool_map_);
  visitor->Trace(document_);
  visitor->Trace(script_tool_host_remote_);
  visitor->Trace(model_context_host_remote_);
  visitor->Trace(model_context_receiver_);
}

const String& ToolData::Name() const {
  return script_tool_->name;
}

SourceLocation* ToolData::GetSourceLocation() const {
  return source_location_;
}

Element* ToolData::BackingFormElement() const {
  return declarative_tool_ ? declarative_tool_->FormElement() : nullptr;
}

void ToolData::Trace(Visitor* visitor) const {
  visitor->Trace(v8_tool_function_);
  visitor->Trace(declarative_tool_);
  visitor->Trace(source_location_);
  visitor->Trace(abort_algorithm_handle_);
}

bool ToolData::RefreshDeclarativeInputSchema() {
  if (declarative_tool_) {
    String new_schema = declarative_tool_->ComputeInputSchema();
    if (new_schema != script_tool_->input_schema) {
      script_tool_->input_schema = new_schema;
      return true;
    }
  }
  return false;
}

}  // namespace blink
