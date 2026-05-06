// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ai/language_model_create_client.h"

#include "base/feature_list.h"
#include "base/task/sequenced_task_runner.h"
#include "base/values.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_create_monitor_callback.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_language_model_message.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_language_model_message_content.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_language_model_message_type.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_language_model_tool_declaration.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_language_model_message_value.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_language_model_prompt.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_languagemodelmessagecontentsequence_string.h"
#include "third_party/blink/renderer/core/dom/quota_exceeded_error.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/modules/ai/ai_features.h"
#include "third_party/blink/renderer/modules/ai/ai_interface_proxy.h"
#include "third_party/blink/renderer/modules/ai/ai_utils.h"
#include "third_party/blink/renderer/modules/ai/language_model_prompt_builder.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/text/strcat.h"

namespace blink {

namespace {

// Result of tool validation and conversion.
// On success: mojo_tools is populated, error_message and exception are empty.
// On failure: error_message or exception is set, mojo_tools is empty.
struct ValidationResult {
  String error_message;            // Non-empty if validation failed.
  v8::Local<v8::Value> exception;  // Non-empty if exception was thrown.
  Vector<mojom::blink::AILanguageModelToolDeclarationPtr>
      mojo_tools;  // Converted tools if successful.
};

// Gets a property from a V8 object. On failure, sets result.exception and
// returns false.
bool GetSchemaProperty(v8::Local<v8::Object> schema,
                       v8::Local<v8::Context> context,
                       v8::Local<v8::String> key,
                       v8::TryCatch& try_catch,
                       v8::Local<v8::Value>* out_value,
                       ValidationResult& result) {
  if (!schema->Get(context, key).ToLocal(out_value)) {
    DCHECK(try_catch.HasCaught());
    result.exception = try_catch.Exception();
    try_catch.Reset();
    return false;
  }
  return true;
}

// Validates tool declarations and converts to Mojo format. If an exception is
// thrown during validation (e.g., from custom toJSON), it will be returned in
// the exception field.
// Note: Caller must enter a ScriptState::Scope before calling this function
// so that any v8::Local handles in the returned ValidationResult remain valid.
ValidationResult ValidateAndConvertToolDeclarations(
    ScriptState* script_state,
    const HeapVector<Member<LanguageModelToolDeclaration>>& tools,
    bool has_tool_call_in_expected_outputs) {
  ValidationResult result;
  if (!has_tool_call_in_expected_outputs) {
    result.error_message =
        "When tools are provided, expectedOutputs must include {type: "
        "\"tool-call\"}.";
    return result;
  }

  v8::Isolate* isolate = script_state->GetIsolate();
  v8::Local<v8::Context> context = script_state->GetContext();

  HashSet<String> tool_names;

  for (const auto& tool : tools) {
    // Validate name is not empty.
    if (tool->name().empty()) {
      result.error_message = "Tool name cannot be empty.";
      return result;
    }

    // Validate description is not empty.
    if (tool->description().empty()) {
      result.error_message =
          StrCat({"Tool description cannot be empty for tool: ", tool->name()});
      return result;
    }

    // Check for duplicate names.
    if (tool_names.Contains(tool->name())) {
      result.error_message = StrCat({"Duplicate tool name: ", tool->name()});
      return result;
    }
    tool_names.insert(tool->name());

    // Validate inputSchema is an object and not null.
    const ScriptObject& input_schema_obj = tool->inputSchema();
    v8::Local<v8::Object> schema_v8 = input_schema_obj.V8Object();

    // Use TryCatch for all V8 operations on the schema, as property access
    // can throw (e.g., Proxy traps or custom getters).
    v8::TryCatch try_catch(isolate);

    // Check if "type" property exists and equals "object".
    v8::Local<v8::String> type_key = V8AtomicString(isolate, "type");
    v8::Local<v8::Value> type_value;
    if (!GetSchemaProperty(schema_v8, context, type_key, try_catch, &type_value,
                           result)) {
      return result;
    }
    if (!type_value->IsString()) {
      result.error_message =
          StrCat({"Tool inputSchema must have a 'type' property that is "
                  "a string for tool: ",
                  tool->name()});
      return result;
    }

    // Compare against "object" string.
    v8::Local<v8::String> expected_type = V8AtomicString(isolate, "object");
    if (!type_value.As<v8::String>()->StringEquals(expected_type)) {
      result.error_message =
          StrCat({"Tool inputSchema 'type' must be 'object' for tool: ",
                  tool->name()});
      return result;
    }

    // Validate "properties" if present.
    v8::Local<v8::String> properties_key =
        V8AtomicString(isolate, "properties");
    v8::Local<v8::Value> properties_value;
    if (!GetSchemaProperty(schema_v8, context, properties_key, try_catch,
                           &properties_value, result)) {
      return result;
    }
    if (!properties_value->IsUndefined() && !properties_value->IsObject()) {
      result.error_message = StrCat(
          {"Tool inputSchema 'properties' must be an object if present for "
           "tool: ",
           tool->name()});
      return result;
    }

    // Validate "required" if present.
    v8::Local<v8::String> required_key = V8AtomicString(isolate, "required");
    v8::Local<v8::Value> required_value;
    if (!GetSchemaProperty(schema_v8, context, required_key, try_catch,
                           &required_value, result)) {
      return result;
    }
    if (!required_value->IsUndefined() && !required_value->IsArray()) {
      result.error_message =
          StrCat({"Tool inputSchema 'required' must be an array if present for "
                  "tool: ",
                  tool->name()});
      return result;
    }

    // Stringify checks for serialization issues that FromV8Value doesn't:
    // it invokes toJSON() methods and propagates V8 exceptions from
    // getters/Proxy traps (FromV8Value swallows them internally).
    v8::MaybeLocal<v8::String> maybe_json =
        v8::JSON::Stringify(context, schema_v8);
    if (try_catch.HasCaught()) {
      result.exception = try_catch.Exception();
      try_catch.Reset();
      return result;
    }

    // If no exception but empty result, it's a built-in error.
    if (maybe_json.IsEmpty()) {
      result.error_message = StrCat(
          {"Failed to serialize inputSchema for tool: ", tool->name(),
           ". The schema may contain circular references or non-serializable "
           "values."});
      return result;
    }

    // Convert inputSchema from V8 to base::Value using Platform API.
    std::unique_ptr<WebV8ValueConverter> converter =
        Platform::Current()->CreateWebV8ValueConverter();

    std::unique_ptr<base::Value> converted_value =
        converter->FromV8Value(schema_v8, context);

    // V8ValueConverter returns nullptr for unsupported types, or a valid
    // base::Value that may contain Type::NONE for circular references or
    // unsupported types nested within the structure.
    if (!converted_value || ContainsNoneType(*converted_value)) {
      result.error_message = StrCat(
          {"Failed to serialize inputSchema for tool: ", tool->name(),
           ". The schema may contain circular references or non-serializable "
           "types (functions, BigInt, etc.)."});
      return result;
    }

    // Conversion: Create mojo tool declaration.
    auto mojo_tool = mojom::blink::AILanguageModelToolDeclaration::New();
    mojo_tool->name = tool->name();
    mojo_tool->description = tool->description();
    mojo_tool->input_schema = std::move(*converted_value).TakeDict();
    result.mojo_tools.push_back(std::move(mojo_tool));
  }

  return result;  // Empty error_message and exception means valid.
}

}  // namespace

LanguageModelCreateClient::LanguageModelCreateClient(
    ScriptPromiseResolver<LanguageModel>* resolver,
    LanguageModelCreateOptions* options,
    mojom::blink::AILanguageModelSamplingParamsPtr resolved_sampling_params,
    std::optional<mojom::blink::AILanguageModelSamplingMode> sampling_mode)
    : ExecutionContextClient(
          ExecutionContext::From(resolver->GetScriptState())),
      AIContextObserver(resolver->GetScriptState(),
                        this,
                        resolver,
                        options->getSignalOr(nullptr)),
      receiver_(this, GetExecutionContext()),
      options_(options),
      resolved_sampling_params_(std::move(resolved_sampling_params)),
      sampling_mode_(sampling_mode),
      task_runner_(
          GetExecutionContext()->GetTaskRunner(TaskType::kInternalDefault)) {
  HeapMojoRemote<mojom::blink::AIManager>& ai_manager_remote =
      AIInterfaceProxy::GetAIManagerRemote(GetExecutionContext());

  if (options->hasMonitor()) {
    monitor_ = MakeGarbageCollected<CreateMonitor>(
        GetExecutionContext(), options->getSignalOr(nullptr), task_runner_);
    // If an exception is thrown, don't initiate the model download.
    // `AICreateMonitorCallback`'s `Invoke` will automatically reject the
    // promise with the thrown exception.
    if (options->monitor()->Invoke(nullptr, monitor_).IsNothing()) {
      return;
    }
    ai_manager_remote->AddModelDownloadProgressObserver(monitor_->BindRemote());
  }

  LanguageModel::ExecuteAvailability(
      ai_manager_remote, options, resolved_sampling_params_.Clone(),
      BindOnce(&LanguageModelCreateClient::Create, WrapPersistent(this)));
}

LanguageModelCreateClient::~LanguageModelCreateClient() = default;

void LanguageModelCreateClient::Trace(Visitor* visitor) const {
  ExecutionContextClient::Trace(visitor);
  AIContextObserver::Trace(visitor);
  visitor->Trace(receiver_);
  visitor->Trace(options_);
  visitor->Trace(monitor_);
}

void LanguageModelCreateClient::Create(
    mojom::blink::ModelAvailabilityCheckResult result) {
  // Abort may have been triggered by `OnDownloadProgressUpdate`.
  if (!GetResolver()) {
    return;
  }

  Availability availability = ConvertModelAvailabilityCheckResult(result);
  if (availability == Availability::kUnavailable) {
    GetResolver()->RejectWithDOMException(
        DOMExceptionCode::kNotSupportedError,
        ConvertModelAvailabilityCheckResultToDebugString(result));
    return;
  }

  ScriptState* script_state = GetScriptState();
  LocalDOMWindow* window = LocalDOMWindow::From(script_state);

  // Prompt APIs are only available within window and extension worker
  // contexts by default. User activation is not consumed by workers,
  // as they lack the ability to do so.
  if (window && RequiresUserActivation(availability) &&
      !MeetsUserActivationRequirements(window)) {
    GetResolver()->RejectWithDOMException(
        DOMExceptionCode::kNotAllowedError,
        kExceptionMessageUserActivationRequired);
    return;
  }

  // TODO(crbug.com/476192657): Process initialPrompts after getting real info.
  auto info = blink::mojom::blink::AILanguageModelInstanceInfo::New();
  info->input_types = {mojom::blink::AILanguageModelPromptType::kText};
  info->sampling_mode = sampling_mode_;
  Vector<mojom::blink::AILanguageModelExpectedPtr> expected_in, expected_out;
  if (options_->hasExpectedInputs()) {
    expected_in = ToMojoExpectations(options_->expectedInputs());
    for (const auto& expected : expected_in) {
      // Reject kToolCall in expectedInputs - tool calls are model outputs, not
      // inputs. Tool responses should be used to send results back.
      // TODO(crbug.com/422803232): Maybe allow kToolCall expectedInputs.
      if (expected->type ==
          mojom::blink::AILanguageModelPromptType::kToolCall) {
        GetResolver()->Reject(DOMException::Create(
            kExceptionMessageUnableToCreateSession,
            DOMException::GetErrorName(DOMExceptionCode::kNotSupportedError)));
        return;
      }
      // Reject kToolResponse without AIPromptAPIToolUse runtime feature
      // enabled.
      if (expected->type ==
              mojom::blink::AILanguageModelPromptType::kToolResponse &&
          !RuntimeEnabledFeatures::AIPromptAPIToolUseEnabled(
              GetExecutionContext())) {
        GetResolver()->Reject(DOMException::Create(
            kExceptionMessageUnableToCreateSession,
            DOMException::GetErrorName(DOMExceptionCode::kNotSupportedError)));
        return;
      }
      // Reject multimodal types when feature is not enabled.
      if ((expected->type == mojom::blink::AILanguageModelPromptType::kImage ||
           expected->type == mojom::blink::AILanguageModelPromptType::kAudio) &&
          !RuntimeEnabledFeatures::AIPromptAPIMultimodalInputEnabled(
              GetExecutionContext())) {
        GetResolver()->Reject(DOMException::Create(
            kExceptionMessageUnableToCreateSession,
            DOMException::GetErrorName(DOMExceptionCode::kNotSupportedError)));
        return;
      }
      // TODO(crbug.com/417817645): Check model capabilities before conversion.
      if (!info->input_types->Contains(expected->type)) {
        info->input_types->push_back(expected->type);
      }
    }
  }

  bool has_tool_call_in_expected_outputs = false;
  if (options_->hasExpectedOutputs()) {
    expected_out = ToMojoExpectations(options_->expectedOutputs());
    for (const auto& expected : expected_out) {
      // Reject when `expected` has tool types without AIPromptAPIToolUse
      // runtime feature enabled.
      if (expected->type ==
              mojom::blink::AILanguageModelPromptType::kToolCall &&
          !RuntimeEnabledFeatures::AIPromptAPIToolUseEnabled(
              GetExecutionContext())) {
        GetResolver()->Reject(DOMException::Create(
            kExceptionMessageUnableToCreateSession,
            DOMException::GetErrorName(DOMExceptionCode::kNotSupportedError)));
        return;
      }
      // Reject other non-text output types (currently unsupported).
      if (expected->type != mojom::blink::AILanguageModelPromptType::kText &&
          expected->type !=
              mojom::blink::AILanguageModelPromptType::kToolCall) {
        GetResolver()->Reject(DOMException::Create(
            kExceptionMessageUnableToCreateSession,
            DOMException::GetErrorName(DOMExceptionCode::kNotSupportedError)));
        return;
      }
      // Track if "tool-call" type is present.
      if (expected->type ==
          mojom::blink::AILanguageModelPromptType::kToolCall) {
        has_tool_call_in_expected_outputs = true;
      }
    }
  }

  // Validate tools if provided, before processing initialPrompts.
  if (options_->hasTools() && !options_->tools().empty()) {
    // Check if the AIPromptAPIToolUse feature is enabled.
    if (!RuntimeEnabledFeatures::AIPromptAPIToolUseEnabled(
            GetExecutionContext())) {
      GetResolver()->Reject(DOMException::Create(
          "Tool use feature is not enabled",
          DOMException::GetErrorName(DOMExceptionCode::kNotSupportedError)));
      return;
    }

    // Enter V8 scope here so returned v8::Local handles in ValidationResult
    // remain valid until after Reject() is called.
    ScriptState::Scope scope(script_state);

    // Validate tool declarations and convert to Mojo in a single pass.
    ValidationResult validation_result = ValidateAndConvertToolDeclarations(
        GetScriptState(), options_->tools(), has_tool_call_in_expected_outputs);
    if (!validation_result.exception.IsEmpty()) {
      GetResolver()->Reject(validation_result.exception);
      return;
    }

    if (!validation_result.error_message.empty()) {
      GetResolver()->RejectWithTypeError(validation_result.error_message);
      return;
    }

    converted_tools_ = std::move(validation_result.mojo_tools);
  }

  if (!options_->hasInitialPrompts() || options_->initialPrompts().empty()) {
    OnInitialPromptsResolved(std::move(expected_in), std::move(expected_out),
                             /*initial_prompts=*/{});
    return;
  }

  for (const auto& message : options_->initialPrompts()) {
    if (message->role() == V8LanguageModelMessageRole::Enum::kSystem &&
        message != options_->initialPrompts().front()) {
      // Only the first prompt supports the `system` role.
      GetResolver()->RejectWithTypeError(
          kExceptionMessagePromptWithSystemRoleIsNotTheFirst);
      return;
    }
    if (message->prefix()) {
      GetResolver()->Reject(DOMException::Create(
          "initialPrompts cannot specify an assistant response prefix.",
          DOMException::GetErrorName(DOMExceptionCode::kSyntaxError)));
      return;
    }
  }

  ConvertPromptInputsToMojo(
      GetScriptState(), options_->getSignalOr(nullptr),
      MakeGarbageCollected<V8LanguageModelPrompt>(options_->initialPrompts()),
      info, /*json_schema=*/"",
      BindOnce(&LanguageModelCreateClient::OnInitialPromptsResolved,
               WrapPersistent(this), std::move(expected_in),
               std::move(expected_out)),
      BindOnce(&LanguageModelCreateClient::OnInitialPromptsRejected,
               WrapPersistent(this)));
}

void LanguageModelCreateClient::OnResult(
    mojo::PendingRemote<mojom::blink::AILanguageModel> pending_remote,
    mojom::blink::AILanguageModelInstanceInfoPtr info) {
  if (!GetResolver()) {
    return;
  }
  if (pending_remote && monitor_) {
    // Ensure that a download completion event is sent.
    monitor_->OnDownloadProgressUpdate(0, kNormalizedDownloadProgressMax);

    // Abort may have been triggered by `OnDownloadProgressUpdate`.
    if (!this->GetResolver()) {
      return;
    }

    // Ensure that a download completion event is sent.
    monitor_->OnDownloadProgressUpdate(kNormalizedDownloadProgressMax,
                                       kNormalizedDownloadProgressMax);

    // Abort may have been triggered by `OnDownloadProgressUpdate`.
    if (!this->GetResolver()) {
      return;
    }
  }

  CHECK(info);
  if (GetExecutionContext() && pending_remote) {
    info->sampling_mode = sampling_mode_;
    GetResolver()->Resolve(MakeGarbageCollected<LanguageModel>(
        GetExecutionContext(), std::move(pending_remote), task_runner_,
        std::move(info)));
  } else {
    GetResolver()->RejectWithDOMException(
        DOMExceptionCode::kInvalidStateError,
        kExceptionMessageUnableToCreateSession);
  }
  Cleanup();
}

void LanguageModelCreateClient::OnError(
    mojom::blink::AIManagerCreateClientError error,
    mojom::blink::QuotaErrorInfoPtr quota_error_info) {
  if (!GetResolver()) {
    return;
  }

  using mojom::blink::AIManagerCreateClientError;

  switch (error) {
    case AIManagerCreateClientError::kUnableToCreateSession:
    case AIManagerCreateClientError::kUnableToCalculateTokenSize: {
      GetResolver()->RejectWithDOMException(
          DOMExceptionCode::kInvalidStateError,
          kExceptionMessageUnableToCreateSession);
      break;
    }
    case AIManagerCreateClientError::kInitialInputTooLarge: {
      CHECK(quota_error_info);
      QuotaExceededError::Reject(
          GetResolver(), kExceptionMessageInputTooLarge,
          static_cast<double>(quota_error_info->quota),
          static_cast<double>(quota_error_info->requested));
      break;
    }
    case AIManagerCreateClientError::kUnsupportedLanguage: {
      GetResolver()->RejectWithDOMException(
          DOMExceptionCode::kNotSupportedError,
          kExceptionMessageUnsupportedLanguages);
      break;
    }
    case AIManagerCreateClientError::kIncompatiblePreferenceOptions: {
      GetResolver()->RejectWithDOMException(
          DOMExceptionCode::kNotSupportedError,
          kExceptionMessageIncompatiblePreferenceOptions);
      break;
    }
  }
  Cleanup();
}

void LanguageModelCreateClient::OnConnectionError() {
  OnError(mojom::blink::AIManagerCreateClientError::kUnableToCreateSession,
          /*quota_error_info=*/nullptr);
}

void LanguageModelCreateClient::ResetReceiver() {
  receiver_.reset();
}

void LanguageModelCreateClient::OnInitialPromptsResolved(
    Vector<mojom::blink::AILanguageModelExpectedPtr> expected_inputs,
    Vector<mojom::blink::AILanguageModelExpectedPtr> expected_outputs,
    Vector<mojom::blink::AILanguageModelPromptPtr> initial_prompts) {
  if (!GetResolver()) {
    return;
  }

  mojo::PendingRemote<mojom::blink::AIManagerCreateLanguageModelClient>
      client_remote;
  receiver_.Bind(client_remote.InitWithNewPipeAndPassReceiver(), task_runner_);
  receiver_.set_disconnect_handler(BindOnce(
      &LanguageModelCreateClient::OnConnectionError, WrapWeakPersistent(this)));
  HeapMojoRemote<mojom::blink::AIManager>& ai_manager_remote =
      AIInterfaceProxy::GetAIManagerRemote(GetExecutionContext());

  ai_manager_remote->CreateLanguageModel(
      std::move(client_remote),
      mojom::blink::AILanguageModelCreateOptions::New(
          resolved_sampling_params_.Clone(), std::move(initial_prompts),
          std::move(expected_inputs), std::move(expected_outputs),
          std::move(converted_tools_), sampling_mode_));
}

void LanguageModelCreateClient::OnInitialPromptsRejected(
    const ScriptValue& error) {
  if (GetResolver()) {
    GetResolver()->Reject(error);
  }
}

}  // namespace blink
