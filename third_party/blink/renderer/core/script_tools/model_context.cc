// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/script_tools/model_context.h"

#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "third_party/blink/public/common/script_tools/script_tool_utils.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/public/web/web_script_tool_types.h"
#include "third_party/blink/renderer/bindings/core/v8/capture_source_location.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_execute_tool_options.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_model_context_get_tool_options.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_model_context_register_tool_options.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_model_context_tool.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_script_runner.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_tool_annotations.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_tool_execute_callback.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_tool_execute_callback_options.h"
#include "third_party/blink/renderer/core/dom/abort_controller.h"
#include "third_party/blink/renderer/core/dom/abort_signal.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/scoped_abort_state.h"
#include "third_party/blink/renderer/core/event_target_names.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/events/tool_activated_event.h"
#include "third_party/blink/renderer/core/events/tool_cancel_event.h"
#include "third_party/blink/renderer/core/execution_context/agent.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/html/html_script_element.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/script_tools/model_context_supplement.h"
#include "third_party/blink/renderer/core/script_tools/model_context_tool_invocation.h"
#include "third_party/blink/renderer/core/script_tools/script_tool_context.h"
#include "third_party/blink/renderer/core/script_tools/tool.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/source_location.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/json/json_parser.h"
#include "third_party/blink/renderer/platform/json/json_values.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
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

const char kPermissionPolicyNotEnabledErrorMessage[] =
    "Access to the feature \"tools\" is disallowed by permissions policy.";
const char kInactiveDocumentErrorMessage[] = "The document is not active.";
const char kTargetFrameNotAccessibleErrorMessage[] =
    "Target frame is not accessible.";
const char kNotFoundErrorMessage[] =
    "The requested tool was not found or is not accessible.";
const char kDataErrorMessage[] = "Failed to parse tool input arguments.";
const char kOperationErrorMessage[] = "Tool execution failed.";
const char kAbortErrorMessage[] = "Tool execution aborted.";

// `input` must be a non-empty, non-null object.
String ValidateAndStringifyValue(ScriptState* script_state,
                                 ExceptionState& exception_state,
                                 const ScriptObject& input,
                                 const String& value_name) {
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
    exception_state.ThrowTypeError("invalid " + value_name +
                                   ": toJSON() returns undefined");
    return String();
  }

  return result;
}

ScriptObject JSONStringToScriptObject(ScriptState* script_state,
                                      const String& json_string) {
  v8::Local<v8::String> v8_json_string =
      V8String(script_state->GetIsolate(), json_string);

  v8::Local<v8::Value> parsed_value;
  if (!v8::JSON::Parse(script_state->GetContext(), v8_json_string)
           .ToLocal(&parsed_value)) {
    return ScriptObject();
  }

  // `v8::JSON::Parse()` parses any valid JSON primitive, which means
  // `parsed_value` could be a Number, a String, etc. We want to explicitly
  // reject non-objects. Note that arrays are considered objects by v8 and
  // ECMAScript; see
  // `external/wpt/webmcp/imperative/object-arguments.https.html`.
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
                               const base::UnguessableToken& tool_id,
                               ScriptPromiseResolverBase* resolver,
                               AbortSignal* signal)
      : model_context_(model_context),
        tool_id_(tool_id),
        resolver_(resolver),
        signal_(signal) {}

  void Run() override {
    model_context_->UnregisterImperativeTool(tool_id_);
    resolver_->Reject(signal_->reason(resolver_->GetScriptState()));
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(model_context_);
    visitor->Trace(resolver_);
    visitor->Trace(signal_);
    AbortSignal::Algorithm::Trace(visitor);
  }

 private:
  Member<ModelContext> model_context_;
  // The specific tool this algorithm was created for. Keyed by id rather than
  // by name, so that this can only ever unregister that exact tool.
  const base::UnguessableToken tool_id_;
  // Never null. The `ScriptPromiseResolverBase` that `this` must reject when
  // `signal_` is aborted (as notified by `Run()` above).
  Member<ScriptPromiseResolverBase> resolver_;
  // Never null. We have to store the `signal_` that `this` is associated with
  // in order to get the abort reason.
  Member<AbortSignal> signal_;
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
        v8::TryCatch try_catch(script_state->GetIsolate());
        v8::Local<v8::String> json_string;
        if (v8::JSON::Stringify(script_state->GetContext(), value.V8Value())
                .ToLocal(&json_string)) {
          result = ToBlinkString<String>(script_state->GetIsolate(),
                                         json_string, kDoNotExternalize);
        } else {
          CHECK(try_catch.HasCaught());
          HandleFailure(script_state, ScriptValue(script_state->GetIsolate(),
                                                  try_catch.Exception()));
          return;
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
      HandleFailure(script_state, value);
    }
  }

  void Trace(Visitor* visitor) const override {
    ThenCallable<IDLAny, ToolFunctionFinishedCallback>::Trace(visitor);
    visitor->Trace(model_context_);
  }

 private:
  void HandleFailure(ScriptState* script_state, ScriptValue value) {
    v8::Isolate* isolate = script_state->GetIsolate();
    v8::Local<v8::Message> message =
        v8::Exception::CreateMessage(isolate, value.V8Value());
    String message_text = ToCoreStringWithNullCheck(isolate, message->Get());
    if (message_text.empty()) {
      message_text = "Unknown error";
    }
    SourceLocation* location = CaptureSourceLocation(
        isolate, message, model_context_->GetExecutionContext());

    model_context_->GetExecutionContext()->AddConsoleMessage(
        MakeGarbageCollected<ConsoleMessage>(
            mojom::blink::ConsoleMessageSource::kJavaScript,
            mojom::blink::ConsoleMessageLevel::kError,
            "WebMCP tool execution failed: " + message_text, location));

    model_context_->OnToolExecuted(
        invocation_id_, base::unexpected(std::make_pair(value, script_state)));
  }

  Member<ModelContext> model_context_;
  const base::UnguessableToken invocation_id_;
  const bool success_;
};

// static
bool ModelContext::IsDeclarativeWebMCPEnabled(const ExecutionContext* context) {
  return RuntimeEnabledFeatures::DeclarativeWebMCPEnabled(context) &&
         RuntimeEnabledFeatures::WebMCPEnabled(context);
}

ModelContext::ModelContext(Document& document)
    : document_(document),
      task_runner_(document.GetTaskRunner(TaskType::kUserInteraction)),
      script_tool_host_remote_(document.GetExecutionContext()),
      model_context_host_remote_(document.GetExecutionContext()),
      model_context_receiver_(this, document.GetExecutionContext()) {
  // Documents without a frame can share another document's execution context,
  // but must not bind to that document's browser host.
  if (!document.GetFrame()) {
    return;
  }
  auto* execution_context = document.GetExecutionContext();
  CHECK(execution_context);
  execution_context->GetBrowserInterfaceBroker().GetInterface(
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

ScriptPromise<IDLUndefined> ModelContext::registerTool(
    ScriptState* script_state,
    ModelContextTool* tool,
    ModelContextRegisterToolOptions* options) {
  if (!document_->IsActive()) {
    return ScriptPromise<IDLUndefined>::RejectWithDOMException(
        script_state,
        MakeGarbageCollected<DOMException>(DOMExceptionCode::kInvalidStateError,
                                           kInactiveDocumentErrorMessage));
  }

  if (!ExecutionContext::From(script_state)
           ->IsFeatureEnabled(
               network::mojom::PermissionsPolicyFeature::kTools)) {
    return ScriptPromise<IDLUndefined>::RejectWithDOMException(
        script_state, MakeGarbageCollected<DOMException>(
                          DOMExceptionCode::kNotAllowedError,
                          kPermissionPolicyNotEnabledErrorMessage));
  }

  if (FindToolDataByName(tool->name())) {
    return ScriptPromise<IDLUndefined>::RejectWithDOMException(
        script_state,
        MakeGarbageCollected<DOMException>(DOMExceptionCode::kInvalidStateError,
                                           "Duplicate tool name"));
  }

  if (!IsValidScriptToolName(tool->name().Utf8())) {
    return ScriptPromise<IDLUndefined>::RejectWithDOMException(
        script_state,
        MakeGarbageCollected<DOMException>(DOMExceptionCode::kInvalidStateError,
                                           "Invalid tool name"));
  }

  if (!tool->description() || tool->description().empty()) {
    return ScriptPromise<IDLUndefined>::RejectWithDOMException(
        script_state,
        MakeGarbageCollected<DOMException>(DOMExceptionCode::kInvalidStateError,
                                           "Description is required"));
  }

  String input_schema;
  if (tool->hasInputSchema()) {
    input_schema = ValidateAndStringifyValue(
        script_state, PassThroughException(script_state->GetIsolate()),
        tool->inputSchema(), "input schema");
    if (!input_schema) {
      // Exception already thrown by ValidateAndStringifyValue.
      return EmptyPromise();
    }
  }

  // Serializing `inputSchema` above can run script (e.g., a `toJSON()` method)
  // that makes the document inactive, for example by removing its iframe. The
  // rest of this method requires an active document (e.g., it uses
  // `model_context_host_remote_`), so check again.
  if (!document_->IsActive()) {
    return ScriptPromise<IDLUndefined>::RejectWithDOMException(
        script_state,
        MakeGarbageCollected<DOMException>(DOMExceptionCode::kInvalidStateError,
                                           kInactiveDocumentErrorMessage));
  }

  // Likewise, that script could have registered a tool with the same name, so
  // check for a duplicate again. Otherwise, the browser would receive two tools
  // with the same name from this document, and kill the renderer.
  if (FindToolDataByName(tool->name())) {
    return ScriptPromise<IDLUndefined>::RejectWithDOMException(
        script_state,
        MakeGarbageCollected<DOMException>(DOMExceptionCode::kInvalidStateError,
                                           "Duplicate tool name"));
  }

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(script_state);
  ScriptPromise promise = resolver->Promise();

  if (options && options->hasSignal() && options->signal()->aborted()) {
    resolver->Reject(options->signal()->reason(script_state));
    return promise;
  }

  Vector<scoped_refptr<const SecurityOrigin>> exposed_origins;
  if (options && options->hasExposedTo()) {
    for (const String& origin_str : options->exposedTo()) {
      scoped_refptr<const SecurityOrigin> origin =
          SecurityOrigin::CreateFromString(origin_str);
      if (!origin->IsPotentiallyTrustworthy()) {
        resolver->RejectWithDOMException(
            DOMExceptionCode::kSecurityError,
            "Only secure origins are allowed in the exposedTo list.");
        return promise;
      }
      exposed_origins.push_back(origin);
    }
  }

  // Mint the tool's identity up-front, so that the unregistration abort
  // algorithm below can be bound to this specific tool.
  const base::UnguessableToken tool_id = base::UnguessableToken::Create();

  AbortSignal::AlgorithmHandle* abort_handle = nullptr;
  if (options && options->hasSignal()) {
    AbortSignal* signal = options->signal();
    CHECK(!signal->aborted());

    // Grab the `AlgorithmHandle` and tie its lifetime to `ToolData` farther
    // below.
    abort_handle =
        signal->AddAlgorithm(MakeGarbageCollected<ToolUnregisterAbortAlgorithm>(
            this, tool_id, resolver, signal));
  }

  auto script_tool = mojom::blink::ScriptTool::New();
  script_tool->tool_id = tool_id;
  script_tool->name = tool->name();
  // If `tool` is not provided, the null string fallback is treated as a
  // nullable member by mojo.
  script_tool->title = tool->hasTitle() ? tool->title() : String();
  script_tool->description = tool->description();
  script_tool->input_schema = input_schema;
  script_tool->exposed_origins = std::move(exposed_origins);

  if (tool->hasAnnotations()) {
    script_tool->annotations = mojom::blink::ScriptToolAnnotations::New();
    CHECK(tool->annotations()->hasReadOnlyHint());
    script_tool->annotations->read_only = tool->annotations()->readOnlyHint();
    CHECK(tool->annotations()->hasUntrustedContentHint());
    script_tool->annotations->untrusted_content =
        tool->annotations()->untrustedContentHint();
    CHECK(tool->annotations()->hasConsequentialHint());
    script_tool->annotations->consequential =
        tool->annotations()->consequentialHint();
    CHECK(tool->annotations()->hasDebugging());
    script_tool->annotations->debugging = tool->annotations()->debugging();
  }

  auto* tool_data = MakeGarbageCollected<ToolData>(
      base::PassKey<ModelContext>(), std::move(script_tool),
      /*v8_tool_function=*/tool->execute(),
      CaptureSourceLocation(ExecutionContext::From(script_state)),
      abort_handle);

  tool_map_.insert(tool_id, tool_data);
  model_context_host_remote_->RegisterScriptTool(
      tool_data->ScriptTool().Clone(),
      blink::BindOnce(
          [](ScriptPromiseResolver<IDLUndefined>* resolver) {
            resolver->Resolve();
          },
          WrapPersistent(resolver)));

  probe::WebMCPToolAdded(document_, *tool_data);
  MaybeRecordToolCount();

  return promise;
}

void ModelContext::UnregisterImperativeTool(
    const base::UnguessableToken& tool_id) {
  // An imperative tool's `AbortSignal` aborts at most once, and nothing else
  // unregisters imperative tools, so the tool must still be registered.
  auto it = tool_map_.find(tool_id);
  CHECK(it != tool_map_.end());
  CHECK(!it->value->DeclarativeTool());

  probe::WebMCPToolRemoved(document_, *it->value);
  tool_map_.erase(it);
  // An inactive document (e.g., one whose frame was removed) has no browser
  // host to notify: its `model_context_host_remote_` is reset when its
  // execution context is destroyed. This is reachable from script, e.g., by
  // aborting a tool's `AbortSignal` after the registering document's iframe is
  // removed.
  if (document_->IsActive()) {
    model_context_host_remote_->UnregisterScriptTool(tool_id);
  }
}

void ModelContext::UnregisterDeclarativeTool(
    const base::UnguessableToken& tool_id) {
  // A form only holds onto a tool that was successfully registered with the
  // `ModelContext` of its document, and unregisters it from that same
  // `ModelContext`, so the tool must still be registered.
  auto it = tool_map_.find(tool_id);
  CHECK(it != tool_map_.end());
  CHECK(it->value->DeclarativeTool());

  probe::WebMCPToolRemoved(document_, *it->value);
  tool_map_.erase(it);
  // An inactive document (e.g., one whose frame was removed) has no browser
  // host to notify: its `model_context_host_remote_` is reset when its
  // execution context is destroyed. This is reachable when a form is removed
  // from a removed iframe's document, e.g., to be adopted into another
  // document.
  if (document_->IsActive()) {
    model_context_host_remote_->UnregisterScriptTool(tool_id);
  }
}

std::optional<ScriptToolDeclaration> ModelContext::GetScriptToolDeclaration(
    const String& name) const {
  ToolData* tool_data = FindToolDataByName(name);
  if (!tool_data) {
    return std::nullopt;
  }
  ScriptToolDeclaration declaration;
  const mojom::blink::ScriptTool& script_tool = tool_data->ScriptTool();
  declaration.description = script_tool.description;
  declaration.input_schema = script_tool.input_schema;
  if (script_tool.annotations) {
    declaration.read_only = script_tool.annotations->read_only;
    declaration.untrusted_content = script_tool.annotations->untrusted_content;
    declaration.consequential = script_tool.annotations->consequential;
    declaration.debugging = script_tool.annotations->debugging;
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

bool ModelContext::ExecuteToolByName(
    base::UnguessableToken invocation_id,
    const String& name,
    const String& input_arguments,
    ScriptToolExecutedCallback tool_executed_cb) {
  if (ToolData* tool_data = FindToolDataByName(name)) {
    return ExecuteTool(invocation_id, tool_data->Id(), input_arguments,
                       std::move(tool_executed_cb));
  }
  probe::WebMCPToolExecuted(document_, name, input_arguments, invocation_id);
  OnToolFailed(std::move(tool_executed_cb), invocation_id,
               ScriptToolError(ScriptToolErrorCode::kInvalidToolName,
                               StrCat({"Tool not found: ", name})));
  return false;
}

bool ModelContext::ExecuteTool(base::UnguessableToken invocation_id,
                               const base::UnguessableToken& tool_id,
                               const String& input_arguments,
                               ScriptToolExecutedCallback tool_executed_cb) {
  CHECK(document_->GetFrame());
  // Creating the AbortController via the main world's ScriptState is safe even
  // when tool callbacks execute in an isolated world, since DOM objects belong
  // to the shared ExecutionContext and V8 wrappers are created on demand in the
  // callback's world.
  ScriptState* script_state = ToScriptStateForMainWorld(document_->GetFrame());
  CHECK(script_state->ContextIsValid());
  AbortController* abort_controller = nullptr;
  {
    ScriptState::Scope scope(script_state);
    abort_controller = AbortController::Create(script_state);
  }
  CHECK(abort_controller);

  // Look the tool's name up before the probe below, so that DevTools reports
  // the name rather than the (non-web-exposed) id. An empty token is never a
  // valid key (ids are always minted at registration), and Blink's `HashMap`
  // disallows looking up the empty value, so check for it separately.
  auto it = tool_id.is_empty() ? tool_map_.end() : tool_map_.find(tool_id);
  if (it == tool_map_.end()) {
    // There's no name to report for a tool we can't find, so the probe falls
    // back to the id. DevTools is trusted with the id, but the error message
    // below is surfaced to the `executeTool()` caller, and the id is not
    // web-exposed, so keep the message generic.
    //
    // TODO(https://crbug.com/536063275): Once DevTools' `toolInvoked` event
    // carries a dedicated tool ID, report the ID there instead of in place of
    // the name.
    probe::WebMCPToolExecuted(document_, String::FromUtf8(tool_id.ToString()),
                              input_arguments, invocation_id);
    OnToolFailed(std::move(tool_executed_cb), invocation_id,
                 ScriptToolError(ScriptToolErrorCode::kInvalidToolName,
                                 "The tool is no longer registered."));
    return false;
  }

  const String name = it->value->Name();
  probe::WebMCPToolExecuted(document_, name, input_arguments, invocation_id);

  bool success = true;
  if (V8ToolExecuteCallback* v8_tool_function =
          it->value->GetV8ToolExecuteCallback()) {
    success = ExecuteV8Tool(v8_tool_function, invocation_id, tool_id, name,
                            input_arguments, abort_controller,
                            std::move(tool_executed_cb));
  } else {
    ExecuteDeclarativeTool(it->value->DeclarativeTool(), invocation_id, tool_id,
                           input_arguments, abort_controller,
                           std::move(tool_executed_cb));
  }

  // Fire the `toolactivated` event *after* activating the tool, but potentially
  // *before* the tool call finishes. Importantly, if the tool is a declarative
  // WebMCP tool, the form will be filled out synchronously above in
  // ExecuteDeclarativeTool(), so by the time the event is fired, the form will
  // be populated.
  // This is a synchronous, non-cancelable event.
  DispatchEvent(
      *ToolActivatedEvent::Create(event_type_names::kToolactivated, name));

  return success;
}

bool ModelContext::CancelTool(base::UnguessableToken invocation_id) {
  CHECK(document_->IsActive());

  // It's possible for `invocation_id` to not match any pending execution, for
  // example, if the tool execution finishes before a tool caller's signal to
  // cancel execution comes in.
  auto it = pending_executions_.find(invocation_id);
  if (it == pending_executions_.end()) {
    return false;
  }
  String tool_name = it->value.tool_name;
  base::UnguessableToken tool_id = it->value.tool_id;

  // It's possible that `invocation_id` matches an existing ID in
  // `pending_executions_`, but that `tool_id` is not found in `tool_map_`.
  // This can only happen for imperative tools. Consider this race:
  //   1. The caller document of an imperative tool aborts its execution. This
  //      sends an IPC to this method.
  //   2. Before this method runs, the tool is unregistered. Imperative tool
  //      unregistration synchronously removes the tool from `tool_map_`,
  //      but leaves pending executions in `pending_executions_`. Pending
  //      executions are cleaned up asynchronously in an IPC sent by the
  //      unregistration flow.
  //   3. This method finally runs, which finds a pending execution, but no
  //      registered tool. The imperative path below can proceed OK!
  //
  // Note that this kind of `tool_id` and `pending_executions_` desync cannot
  // happen for declarative tools. Whenever a declarative tool is unregistered,
  // it immediately invokes its `done_callback_` which removes the entry from
  // `pending_executions_`, and removes the tool from `tool_map_` at the same
  // time.
  auto tool_it = tool_map_.find(tool_id);
  if (tool_it != tool_map_.end() && tool_it->value->DeclarativeTool()) {
    // For declarative tools, we can forcefully cancel the internal state
    // machine and make the declarative tool "resolve" with an error. This will
    // synchronously run the `HTMLFormMcpTool::done_callback_`, which handles
    // removing the execution from `pending_executions_`.
    tool_it->value->DeclarativeTool()->CancelTool();
    CHECK(!pending_executions_.Contains(invocation_id));
  } else {
    // For imperative tools, we cannot forcefully reject the Promise that the
    // tool returned. Instead, we remove the pending execution, manually run its
    // callback to notify the browser of cancellation, and abort the signal that
    // the tool execution function is observing. When the tool's returned
    // Promise eventually settles, it will be safely ignored.
    auto pending_execution = pending_executions_.Take(invocation_id);
    CHECK(pending_execution.abort_controller);
    CHECK(pending_execution.callback);

    ScriptState* script_state =
        ToScriptStateForMainWorld(document_->GetFrame());
    if (script_state && script_state->ContextIsValid()) {
      ScriptState::Scope scope(script_state);
      pending_execution.abort_controller->abort(script_state);
    }

    // This invokes a DevTools probe which can enter script and detach the
    // document.
    OnToolFailed(std::move(pending_execution.callback), invocation_id,
                 ScriptToolError(ScriptToolErrorCode::kToolCancelled));
  }

  // Dispatch the synchronous toolcancel event for both types of tools.
  DispatchEvent(
      *ToolCancelEvent::Create(event_type_names::kToolcancel, tool_name));

  return true;
}

void ModelContext::GetCrossDocumentScriptToolResult(
    const base::UnguessableToken& invocation_id,
    CrossDocumentScriptToolResultCallback result_callback) {
  if (document_->HasFinishedParsing()) {
    String result = ComputeScriptToolResult(*document_);
    probe::WebMCPToolResponded(document_, result, invocation_id);
    std::move(result_callback).Run(result);
    return;
  }

  cross_document_result_callbacks_.push_back(blink::BindOnce(
      [](CrossDocumentScriptToolResultCallback original_callback,
         const base::UnguessableToken& invocation_id, Document* document,
         String result) {
        probe::WebMCPToolResponded(document, result, invocation_id);
        std::move(original_callback).Run(result);
      },
      std::move(result_callback), invocation_id,
      WrapPersistent(document_.Get())));
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
    base::UnguessableToken invocation_id,
    const base::UnguessableToken& tool_id,
    const String& input_arguments,
    AbortController* abort_controller,
    ScriptToolExecutedCallback tool_executed_cb) {
  CHECK(abort_controller);

  std::optional<scheduler::TaskAttributionTracker::TaskScope> task_scope;
  if (auto* tracker = document_->GetAgent().isolate()
                          ? scheduler::TaskAttributionTracker::From(
                                document_->GetAgent().isolate())
                          : nullptr) {
    task_scope = tracker->SetTaskStateVariable(
        MakeGarbageCollected<ScriptToolContext>(invocation_id));
  }

  pending_executions_.insert(invocation_id,
                             PendingExecution{
                                 .tool_name = tool->ToolName(),
                                 .tool_id = tool_id,
                                 .callback = std::move(tool_executed_cb),
                                 .abort_controller = abort_controller,
                             });

  tool->ExecuteTool(
      invocation_id, input_arguments,
      blink::BindOnce(
          [](Document* document, base::UnguessableToken invocation_id,
             base::expected<String, ScriptToolError> result) {
            CHECK(document);
            ModelContext* model_context =
                ModelContextSupplement::modelContext(*document);
            CHECK(model_context);
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
            auto it = model_context->pending_executions_.find(invocation_id);
            if (it != model_context->pending_executions_.end()) {
              std::move(it->value.callback).Run(result);
              model_context->pending_executions_.erase(it);
            }
          },
          WrapWeakPersistent(document_.Get()), invocation_id));
}

// This overload is used for JS-provided tool functions. It converts the input
// argument string to a JSON object, calls the function, receives a Promise,
// waits for the promise to resolve, JSON-stringifies the result, and passes
// it to OnToolExecuted().
bool ModelContext::ExecuteV8Tool(V8ToolExecuteCallback* tool_function,
                                 base::UnguessableToken invocation_id,
                                 const base::UnguessableToken& tool_id,
                                 const String& name,
                                 const String& input_arguments,
                                 AbortController* abort_controller,
                                 ScriptToolExecutedCallback tool_executed_cb) {
  UseCounter::Count(document_, WebFeature::kModelContextExecuteImperativeTool);
  CHECK(abort_controller);

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

  std::optional<scheduler::TaskAttributionTracker::TaskScope> task_scope;
  if (auto* tracker =
          scheduler::TaskAttributionTracker::From(script_state->GetIsolate())) {
    task_scope = tracker->SetTaskStateVariable(
        MakeGarbageCollected<ScriptToolContext>(invocation_id));
  }

  // Insert into `pending_executions_` before invoking `tool_function` so that
  // synchronous calls to `options.invocation.requestToken()` inside the
  // callback can find the active execution.
  pending_executions_.insert(invocation_id,
                             PendingExecution{
                                 .tool_name = name,
                                 .tool_id = tool_id,
                                 .callback = std::move(tool_executed_cb),
                                 .abort_controller = abort_controller,
                             });

  ToolExecuteCallbackOptions* execute_options =
      ToolExecuteCallbackOptions::Create();
  execute_options->setSignal(abort_controller->signal());
  if (RuntimeEnabledFeatures::WebMCPContinuationsEnabled(
          GetExecutionContext())) {
    execute_options->setInvocation(
        MakeGarbageCollected<ModelContextToolInvocation>(this, invocation_id));
  }
  v8::Maybe<ScriptPromise<IDLAny>> maybe_result =
      tool_function->Invoke(nullptr, std::move(script_object), execute_options);

  ScriptPromise<IDLAny> result;
  // If the callback couldn't be run for some reason, treat it as an empty
  // promise rejected with an abort exception.
  if (maybe_result.IsNothing()) {
    result = ScriptPromise<IDLAny>::RejectWithDOMException(
        script_state, MakeGarbageCollected<DOMException>(
                          DOMExceptionCode::kAbortError, kAbortErrorMessage));
  } else {
    result = maybe_result.FromJust();
  }

  result.Then(script_state,
              MakeGarbageCollected<ToolFunctionFinishedCallback>(
                  this, invocation_id, true),
              MakeGarbageCollected<ToolFunctionFinishedCallback>(
                  this, invocation_id, false));
  return true;
}

bool ModelContext::RegisterDeclarativeTool(
    DeclarativeWebMCPTool* declarative_tool) {
  // The form code must ensure the document has a frame before calling this.
  // Frameless documents do not have a bound ModelContextHost.
  if (!document_->GetExecutionContext()->IsFeatureEnabled(
          network::mojom::PermissionsPolicyFeature::kTools)) {
    // TODO(crbug.com/507724727) Surface an error if the `tools` permission
    // policy is not enabled
    return false;
  }

  if (FindToolDataByName(declarative_tool->ToolName())) {
    return false;
  }

  if (!IsValidScriptToolName(declarative_tool->ToolName().Utf8())) {
    return false;
  }
  UseCounter::Count(document_,
                    WebFeature::kModelContextRegisterDeclarativeTool);

  const base::UnguessableToken& tool_id = declarative_tool->ToolId();

  auto script_tool = mojom::blink::ScriptTool::New();
  script_tool->tool_id = tool_id;
  script_tool->name = declarative_tool->ToolName();
  script_tool->description = declarative_tool->ToolDescription();
  script_tool->title = declarative_tool->ToolTitle();
  script_tool->input_schema = declarative_tool->ComputeInputSchema();

  auto* tool_data = MakeGarbageCollected<ToolData>(
      base::PassKey<ModelContext>(), std::move(script_tool), declarative_tool);

  tool_map_.insert(tool_id, tool_data);
  model_context_host_remote_->RegisterScriptTool(
      tool_data->ScriptTool().Clone(), base::DoNothing());
  probe::WebMCPToolAdded(document_, *tool_data);
  MaybeRecordToolCount();
  return true;
}

void ModelContext::OnToolExecuted(
    const base::UnguessableToken& invocation_id,
    base::expected<String, std::pair<ScriptValue, ScriptState*>> result) {
  auto pending_execution = pending_executions_.Take(invocation_id);
  if (pending_execution.callback.is_null()) {
    return;
  }

  if (result.has_value()) {
    probe::WebMCPToolResponded(document_, result.value(), invocation_id);
    std::move(pending_execution.callback).Run(result.value());
  } else {
    ScriptToolError error(ScriptToolErrorCode::kToolInvocationFailed);
    probe::WebMCPToolFailed(document_, error, invocation_id, result.error());
    std::move(pending_execution.callback).Run(base::unexpected(error));
  }
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
  // The form code must ensure the document is active before calling this.
  // Pausing execution requires a browser connection for the document's frame.
  if (!script_tool_host_remote_.is_bound()) {
    document_->GetExecutionContext()->GetBrowserInterfaceBroker().GetInterface(
        script_tool_host_remote_.BindNewPipeAndPassReceiver(task_runner_));
  }
  script_tool_host_remote_->PauseExecution();
}

void ModelContext::NotifyToolChange() {
  DispatchEvent(*Event::Create(event_type_names::kToolchange));
}

void ModelContext::ExecuteScriptTool(
    const base::UnguessableToken& invocation_id,
    const base::UnguessableToken& tool_id,
    const String& input_arguments,
    ExecuteScriptToolCallback callback) {
  ExecuteTool(invocation_id, tool_id, input_arguments,
              blink::BindOnce(
                  [](ExecuteScriptToolCallback callback,
                     base::expected<String, ScriptToolError> result) {
                    if (result.has_value()) {
                      std::move(callback).Run(result.value(), std::nullopt);
                    } else {
                      std::move(callback).Run(String(), result.error().code);
                    }
                  },
                  std::move(callback)));
}

void ModelContext::CancelScriptTool(
    const base::UnguessableToken& invocation_id) {
  CancelTool(invocation_id);
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

ToolData* ModelContext::FindToolDataByName(const String& name) const {
  for (const auto& entry : tool_map_) {
    if (entry.value->Name() == name) {
      return entry.value.Get();
    }
  }
  return nullptr;
}

ExecutionContext* ModelContext::GetExecutionContext() const {
  return document_ ? document_->GetExecutionContext() : nullptr;
}

const AtomicString& ModelContext::InterfaceName() const {
  DEFINE_STATIC_LOCAL(AtomicString, name, ("ModelContext"));
  return name;
}

ScriptPromise<IDLSequence<Tool>> ModelContext::getTools(
    ScriptState* script_state,
    const ModelContextGetToolOptions* options) {
  if (!document_->IsActive()) {
    return ScriptPromise<IDLSequence<Tool>>::RejectWithDOMException(
        script_state,
        MakeGarbageCollected<DOMException>(DOMExceptionCode::kInvalidStateError,
                                           kInactiveDocumentErrorMessage));
  }

  if (!ExecutionContext::From(script_state)
           ->IsFeatureEnabled(
               network::mojom::PermissionsPolicyFeature::kTools)) {
    return ScriptPromise<IDLSequence<Tool>>::RejectWithDOMException(
        script_state, MakeGarbageCollected<DOMException>(
                          DOMExceptionCode::kNotAllowedError,
                          kPermissionPolicyNotEnabledErrorMessage));
  }

  Vector<scoped_refptr<const SecurityOrigin>> from_origins;
  if (options && options->hasFromOrigins()) {
    for (const String& origin_str : options->fromOrigins()) {
      scoped_refptr<const SecurityOrigin> origin =
          SecurityOrigin::CreateFromString(origin_str);
      if (!origin->IsPotentiallyTrustworthy()) {
        return ScriptPromise<IDLSequence<Tool>>::RejectWithDOMException(
            script_state, MakeGarbageCollected<DOMException>(
                              DOMExceptionCode::kSecurityError,
                              "Only secure origins are allowed in the "
                              "fromOrigins list."));
      }
      from_origins.push_back(origin);
    }
  }

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLSequence<Tool>>>(
          script_state);
  ScriptPromise promise = resolver->Promise();

  model_context_host_remote_->GetScriptTools(
      std::move(from_origins),
      blink::BindOnce(&ModelContext::OnGetScriptToolsCompleted,
                      WrapWeakPersistent(this), WrapPersistent(resolver)));

  return promise;
}

void ModelContext::OnGetScriptToolsCompleted(
    ScriptPromiseResolver<IDLSequence<Tool>>* resolver,
    Vector<mojom::blink::ExposedScriptToolPtr> tools) {
  HeapVector<Member<Tool>> result_tools;
  result_tools.ReserveInitialCapacity(tools.size());

  // Parsing each tool's input schema, and handing the parsed value to `Tool`,
  // both require the resolver's context to be entered.
  ScriptState* script_state = resolver->GetScriptState();
  ScriptState::Scope scope(script_state);

  for (auto& exposed_tool : tools) {
    mojom::blink::ScriptToolPtr& t = exposed_tool->tool;
    Frame* frame = Frame::ResolveFrame(exposed_tool->tool_owner_frame_token);
    // If we can't resolve the token into a concrete frame, that means the
    // document could have been discarded by the time the response IPC comes
    // back to the renderer. In that case, the tool is unusable from our
    // perspective, so exclude it from `result_tools`.
    if (!frame) {
      continue;
    }

    ScriptValue input_schema;
    if (!t->input_schema.IsNull()) {
      v8::TryCatch try_catch(script_state->GetIsolate());
      auto script_object =
          JSONStringToScriptObject(script_state, t->input_schema);
      input_schema = script_object;
      if (try_catch.HasCaught() || input_schema.IsEmpty()) {
        continue;
      }
    }

    result_tools.push_back(MakeGarbageCollected<Tool>(
        frame->DomWindow(), std::move(exposed_tool->origin), std::move(t),
        input_schema));
  }

  resolver->Resolve(result_tools);
}

ScriptPromise<IDLNullable<IDLString>> ModelContext::executeTool(
    ScriptState* script_state,
    Tool* tool,
    ScriptObject input_object,
    const ExecuteToolOptions* options) {
  if (!document_->IsActive()) {
    return ScriptPromise<IDLNullable<IDLString>>::RejectWithDOMException(
        script_state,
        MakeGarbageCollected<DOMException>(DOMExceptionCode::kInvalidStateError,
                                           kInactiveDocumentErrorMessage));
  }

  if (!ExecutionContext::From(script_state)
           ->IsFeatureEnabled(
               network::mojom::PermissionsPolicyFeature::kTools)) {
    return ScriptPromise<IDLNullable<IDLString>>::RejectWithDOMException(
        script_state, MakeGarbageCollected<DOMException>(
                          DOMExceptionCode::kNotAllowedError,
                          kPermissionPolicyNotEnabledErrorMessage));
  }

  KURL expected_url(NullUrl(), tool->origin());
  if (!expected_url.IsValid()) {
    return ScriptPromise<IDLNullable<IDLString>>::RejectWithDOMException(
        script_state,
        MakeGarbageCollected<DOMException>(DOMExceptionCode::kNotSupportedError,
                                           "The provided origin is invalid."));
  }

  scoped_refptr<SecurityOrigin> expected_target_origin =
      SecurityOrigin::Create(expected_url);
  if (expected_target_origin->IsOpaque()) {
    return ScriptPromise<IDLNullable<IDLString>>::RejectWithDOMException(
        script_state, MakeGarbageCollected<DOMException>(
                          DOMExceptionCode::kNotSupportedError,
                          "Cannot execute tools that live in a document with "
                          "an opaque origin."));
  }

  // Per Web IDL, an omitted or explicitly-undefined optional argument is
  // "missing", which is represented by `IsEmpty()`. We treat a "missing"
  // optional input object as the empty object.
  v8::Isolate* isolate = script_state->GetIsolate();
  if (input_object.IsEmpty()) {
    input_object = ScriptObject(isolate, v8::Object::New(isolate));
  }
  String input_arguments =
      ValidateAndStringifyValue(script_state, PassThroughException(isolate),
                                input_object, "input object");
  if (!input_arguments) {
    // Exception already thrown by ValidateAndStringifyValue.
    return EmptyPromise();
  }

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLNullable<IDLString>>>(
          script_state);
  ScriptPromise promise = resolver->Promise();

  // `window` is null if it was garbage collected, and its frame is missing if
  // the window was disconnected, i.e., the registering document was detached or
  // discarded in the gap between tool retrieval and tool execution.
  DOMWindow* window = tool->window();
  Frame* target_frame = window ? window->GetFrame() : nullptr;
  if (!target_frame) {
    resolver->RejectWithDOMException(DOMExceptionCode::kInvalidStateError,
                                     kTargetFrameNotAccessibleErrorMessage);
    return promise;
  }
  blink::FrameToken frame_token = target_frame->GetFrameToken();

  // Because the document is active, we know `local_frame` is non-null.
  LocalFrame* local_frame = document_->GetFrame();
  CHECK(local_frame);

  base::UnguessableToken invocation_id = base::UnguessableToken::Create();

  std::unique_ptr<ScopedAbortState> scoped_abort_state;
  if (options && options->hasSignal()) {
    AbortSignal* signal = options->signal();
    if (signal->aborted()) {
      resolver->Reject(signal->reason(resolver->GetScriptState()));
      return promise;
    }

    auto* handle = signal->AddAlgorithm(blink::BindOnce(
        [](ModelContext* self,
           ScriptPromiseResolver<IDLNullable<IDLString>>* resolver,
           ScriptState* script_state, AbortSignal* signal,
           const base::UnguessableToken& invocation_id) {
          if (resolver->GetScriptState() &&
              resolver->GetScriptState()->ContextIsValid()) {
            resolver->Reject(signal->reason(script_state));
          }
          if (self && self->model_context_host_remote_.is_bound()) {
            self->model_context_host_remote_->CancelRemoteScriptTool(
                invocation_id);
          }
        },
        WrapWeakPersistent(this), WrapPersistent(resolver),
        WrapPersistent(script_state), WrapPersistent(signal), invocation_id));

    scoped_abort_state = std::make_unique<ScopedAbortState>(signal, handle);
  }

  model_context_host_remote_->ExecuteRemoteScriptTool(
      invocation_id, tool->ToolId(), frame_token, expected_target_origin,
      input_arguments,
      blink::BindOnce(&ModelContext::OnExecuteScriptToolCompleted,
                      WrapWeakPersistent(this), WrapPersistent(resolver),
                      std::move(scoped_abort_state)));
  return promise;
}

void ModelContext::OnExecuteScriptToolCompleted(
    ScriptPromiseResolver<IDLNullable<IDLString>>* resolver,
    std::unique_ptr<ScopedAbortState> abort_state,
    const String& result,
    std::optional<mojom::blink::ExecuteRemoteScriptToolError> error) {
  // For the execution result to have been received from the browser process
  // over mojo, the frame/Document that sent the execution must not be detached,
  // and the same goes for the `resolver` that is tied to those objects.
  CHECK(resolver->GetScriptState() &&
        resolver->GetScriptState()->ContextIsValid());

  if (!error.has_value()) {
    resolver->Resolve(result);
    return;
  }

  switch (*error) {
    case mojom::blink::ExecuteRemoteScriptToolError::kInvalidStateError:
      resolver->RejectWithDOMException(DOMExceptionCode::kInvalidStateError,
                                       kTargetFrameNotAccessibleErrorMessage);
      break;
    case mojom::blink::ExecuteRemoteScriptToolError::kNotFoundError:
      resolver->RejectWithDOMException(DOMExceptionCode::kNotFoundError,
                                       kNotFoundErrorMessage);
      break;
    case mojom::blink::ExecuteRemoteScriptToolError::kDataError:
      resolver->RejectWithDOMException(DOMExceptionCode::kDataError,
                                       kDataErrorMessage);
      break;
    case mojom::blink::ExecuteRemoteScriptToolError::kOperationError:
      resolver->RejectWithDOMException(DOMExceptionCode::kOperationError,
                                       kOperationErrorMessage);
      break;
    case mojom::blink::ExecuteRemoteScriptToolError::kAbortError:
      resolver->RejectWithDOMException(DOMExceptionCode::kAbortError,
                                       kAbortErrorMessage);
      break;
  }
}

ScriptPromise<IDLString> ModelContext::RequestContinuationToken(
    base::PassKey<ModelContextToolInvocation>,
    ScriptState* script_state,
    const base::UnguessableToken& invocation_id,
    bool& did_request_token) {
  if (!document_->IsActive()) {
    return ScriptPromise<IDLString>::RejectWithDOMException(
        script_state,
        MakeGarbageCollected<DOMException>(DOMExceptionCode::kInvalidStateError,
                                           kInactiveDocumentErrorMessage));
  }

  // The `kTools` permission has already been checked for the execution context
  // associated with `script_state`.
  CHECK(
      ExecutionContext::From(script_state)
          ->IsFeatureEnabled(network::mojom::PermissionsPolicyFeature::kTools));

  auto it = pending_executions_.find(invocation_id);
  if (it == pending_executions_.end() || did_request_token) {
    return ScriptPromise<IDLString>::RejectWithDOMException(
        script_state,
        MakeGarbageCollected<DOMException>(
            DOMExceptionCode::kInvalidStateError,
            "Cannot request a continuation token for a tool that isn't running "
            "or more than once per invocation."));
  }

  did_request_token = true;

  return ScriptPromise<IDLString>::RejectWithDOMException(
      script_state, MakeGarbageCollected<DOMException>(
                        DOMExceptionCode::kNotSupportedError,
                        "Continuation tokens are not yet implemented."));
}

ScriptPromise<IDLUndefined> ModelContext::resumeTool(
    ScriptState* script_state,
    const String& token_str,
    V8ToolExecuteCallback* callback) {
  if (!document_->IsActive()) {
    return ScriptPromise<IDLUndefined>::RejectWithDOMException(
        script_state,
        MakeGarbageCollected<DOMException>(DOMExceptionCode::kInvalidStateError,
                                           kInactiveDocumentErrorMessage));
  }

  if (!ExecutionContext::From(script_state)
           ->IsFeatureEnabled(
               network::mojom::PermissionsPolicyFeature::kTools)) {
    return ScriptPromise<IDLUndefined>::RejectWithDOMException(
        script_state, MakeGarbageCollected<DOMException>(
                          DOMExceptionCode::kNotAllowedError,
                          kPermissionPolicyNotEnabledErrorMessage));
  }

  std::optional<base::UnguessableToken> token =
      base::UnguessableToken::DeserializeFromString(token_str.Utf8());
  if (!token.has_value()) {
    return ScriptPromise<IDLUndefined>::RejectWithDOMException(
        script_state, MakeGarbageCollected<DOMException>(
                          DOMExceptionCode::kInvalidAccessError,
                          "Invalid or already redeemed continuation token."));
  }

  return ScriptPromise<IDLUndefined>::RejectWithDOMException(
      script_state, MakeGarbageCollected<DOMException>(
                        DOMExceptionCode::kInvalidAccessError,
                        "Continuation token could not be redeemed."));
}

void ModelContext::PendingExecution::Trace(Visitor* visitor) const {
  visitor->Trace(abort_controller);
}

void ModelContext::Trace(Visitor* visitor) const {
  EventTarget::Trace(visitor);
  visitor->Trace(tool_map_);
  visitor->Trace(pending_executions_);
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
