// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/shared_storage/shared_storage_worklet_global_scope.h"

#include <stdint.h>

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/functional/callback.h"
#include "gin/converter.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/url_loader_factory.mojom-blink.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/shared_storage/module_script_downloader.h"
#include "third_party/blink/public/common/shared_storage/shared_storage_utils.h"
#include "third_party/blink/public/mojom/private_aggregation/private_aggregation_host.mojom-blink.h"
#include "third_party/blink/public/platform/cross_variant_mojo_util.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/unpacked_serialized_script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_no_argument_constructor.h"
#include "third_party/blink/renderer/bindings/core/v8/worker_or_worklet_script_controller.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_run_function_for_shared_storage_run_operation.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_run_function_for_shared_storage_select_url_operation.h"
#include "third_party/blink/renderer/core/context_features/context_feature_settings.h"
#include "third_party/blink/renderer/core/script/classic_script.h"
#include "third_party/blink/renderer/core/workers/global_scope_creation_params.h"
#include "third_party/blink/renderer/core/workers/threaded_messaging_proxy_base.h"
#include "third_party/blink/renderer/modules/crypto/crypto.h"
#include "third_party/blink/renderer/modules/shared_storage/private_aggregation.h"
#include "third_party/blink/renderer/modules/shared_storage/shared_storage.h"
#include "third_party/blink/renderer/modules/shared_storage/shared_storage_operation_definition.h"
#include "third_party/blink/renderer/modules/shared_storage/shared_storage_worklet_thread.h"
#include "third_party/blink/renderer/platform/bindings/callback_method_retriever.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-local-handle.h"
#include "v8/include/v8-primitive.h"
#include "v8/include/v8-value.h"

namespace blink {

namespace {

constexpr char kCannotDeserializeDataErrorMessage[] =
    "Cannot deserialize data.";

absl::optional<ScriptValue> Deserialize(
    v8::Isolate* isolate,
    ExecutionContext* execution_context,
    const BlinkCloneableMessage& serialized_data) {
  if (!serialized_data.message->CanDeserializeIn(execution_context)) {
    return absl::nullopt;
  }

  Member<UnpackedSerializedScriptValue> unpacked =
      SerializedScriptValue::Unpack(serialized_data.message);
  if (!unpacked) {
    return absl::nullopt;
  }

  return ScriptValue(isolate, unpacked->Deserialize(isolate));
}

// We try to use .stack property so that the error message contains a stack
// trace, but otherwise fallback to .toString().
String ExceptionToString(ScriptState* script_state,
                         v8::Local<v8::Value> exception) {
  v8::Isolate* isolate = script_state->GetIsolate();

  if (!exception.IsEmpty()) {
    v8::Local<v8::Context> context = script_state->GetContext();
    v8::Local<v8::Value> value =
        v8::TryCatch::StackTrace(context, exception).FromMaybe(exception);
    v8::Local<v8::String> value_string;
    if (value->ToString(context).ToLocal(&value_string)) {
      return String(gin::V8ToString(isolate, value_string));
    }
  }

  return "Unknown Failure";
}

struct UnresolvedSelectURLRequest final
    : public GarbageCollected<UnresolvedSelectURLRequest> {
  UnresolvedSelectURLRequest(size_t urls_size,
                             blink::mojom::blink::SharedStorageWorkletService::
                                 RunURLSelectionOperationCallback callback)
      : urls_size(urls_size), callback(std::move(callback)) {}
  ~UnresolvedSelectURLRequest() = default;

  void Trace(Visitor* visitor) const {}

  size_t urls_size;
  blink::mojom::blink::SharedStorageWorkletService::
      RunURLSelectionOperationCallback callback;
};

struct UnresolvedRunRequest final
    : public GarbageCollected<UnresolvedRunRequest> {
  explicit UnresolvedRunRequest(
      blink::mojom::blink::SharedStorageWorkletService::RunOperationCallback
          callback)
      : callback(std::move(callback)) {}
  ~UnresolvedRunRequest() = default;

  void Trace(Visitor* visitor) const {}

  blink::mojom::blink::SharedStorageWorkletService::RunOperationCallback
      callback;
};

class SelectURLResolutionSuccessCallback final
    : public ScriptFunction::Callable {
 public:
  explicit SelectURLResolutionSuccessCallback(
      UnresolvedSelectURLRequest* request)
      : request_(request) {}

  void Trace(Visitor* visitor) const final {
    visitor->Trace(request_);
    ScriptFunction::Callable::Trace(visitor);
  }

 private:
  ScriptValue Call(ScriptState* script_state, ScriptValue value) override {
    ScriptState::Scope scope(script_state);

    v8::Local<v8::Context> context = value.GetIsolate()->GetCurrentContext();
    v8::Local<v8::Value> v8_value = value.V8Value();

    v8::Local<v8::Uint32> v8_result_index;
    if (!v8_value->ToUint32(context).ToLocal(&v8_result_index)) {
      std::move(request_->callback)
          .Run(/*success=*/false,
               "Promise did not resolve to an uint32 number.",
               /*index=*/0);
    } else {
      uint32_t result_index = v8_result_index->Value();
      if (result_index >= request_->urls_size) {
        std::move(request_->callback)
            .Run(/*success=*/false,
                 "Promise resolved to a number outside the length of the input "
                 "urls.",
                 /*index=*/0);
      } else {
        std::move(request_->callback)
            .Run(/*success=*/true,
                 /*error_message=*/g_empty_string, result_index);
      }
    }

    return value;
  }

  Member<UnresolvedSelectURLRequest> request_;
};

class SelectURLResolutionFailureCallback final
    : public ScriptFunction::Callable {
 public:
  explicit SelectURLResolutionFailureCallback(
      UnresolvedSelectURLRequest* request)
      : request_(request) {}

  void Trace(Visitor* visitor) const final {
    visitor->Trace(request_);
    ScriptFunction::Callable::Trace(visitor);
  }

 private:
  ScriptValue Call(ScriptState* script_state, ScriptValue value) override {
    ScriptState::Scope scope(script_state);

    v8::Local<v8::Value> v8_value = value.V8Value();

    std::move(request_->callback)
        .Run(/*success=*/false, ExceptionToString(script_state, v8_value),
             /*index=*/0);

    return value;
  }

  Member<UnresolvedSelectURLRequest> request_;
};

class RunResolutionSuccessCallback final : public ScriptFunction::Callable {
 public:
  explicit RunResolutionSuccessCallback(UnresolvedRunRequest* request)
      : request_(request) {}

  void Trace(Visitor* visitor) const final {
    visitor->Trace(request_);
    ScriptFunction::Callable::Trace(visitor);
  }

 private:
  ScriptValue Call(ScriptState* script_state, ScriptValue value) override {
    std::move(request_->callback)
        .Run(/*success=*/true,
             /*error_message=*/g_empty_string);
    return value;
  }

  Member<UnresolvedRunRequest> request_;
};

class RunResolutionFailureCallback final : public ScriptFunction::Callable {
 public:
  explicit RunResolutionFailureCallback(UnresolvedRunRequest* request)
      : request_(request) {}

  void Trace(Visitor* visitor) const final {
    visitor->Trace(request_);
    ScriptFunction::Callable::Trace(visitor);
  }

 private:
  ScriptValue Call(ScriptState* script_state, ScriptValue value) override {
    ScriptState::Scope scope(script_state);

    v8::Local<v8::Value> v8_value = value.V8Value();

    std::move(request_->callback)
        .Run(/*success=*/false, ExceptionToString(script_state, v8_value));

    return value;
  }

  Member<UnresolvedRunRequest> request_;
};

}  // namespace

SharedStorageWorkletGlobalScope::SharedStorageWorkletGlobalScope(
    std::unique_ptr<GlobalScopeCreationParams> creation_params,
    WorkerThread* thread)
    : WorkletGlobalScope(std::move(creation_params),
                         thread->GetWorkerReportingProxy(),
                         thread) {
  ContextFeatureSettings::From(
      this, ContextFeatureSettings::CreationMode::kCreateIfNotExists)
      ->EnablePrivateAggregationInSharedStorage(
          ShouldDefinePrivateAggregationInSharedStorage());
}

SharedStorageWorkletGlobalScope::~SharedStorageWorkletGlobalScope() = default;

void SharedStorageWorkletGlobalScope::BindSharedStorageWorkletService(
    mojo::PendingReceiver<mojom::blink::SharedStorageWorkletService> receiver,
    base::OnceClosure disconnect_handler) {
  receiver_.Bind(std::move(receiver),
                 GetTaskRunner(blink::TaskType::kMiscPlatformAPI));

  // When `SharedStorageWorkletHost` is destroyed, the disconnect handler will
  // be called, and we rely on this explicit signal to clean up the worklet
  // environment.
  receiver_.set_disconnect_handler(std::move(disconnect_handler));
}

void SharedStorageWorkletGlobalScope::Register(
    const String& name,
    V8NoArgumentConstructor* operation_ctor,
    ExceptionState& exception_state) {
  if (name.empty()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kDataError,
                                      "Operation name cannot be empty.");
    return;
  }

  if (operation_definition_map_.Contains(name)) {
    exception_state.ThrowDOMException(DOMExceptionCode::kDataError,
                                      "Operation name already registered.");
    return;
  }

  // If the result of Type(argument=prototype) is not Object, throw a TypeError.
  CallbackMethodRetriever retriever(operation_ctor);
  retriever.GetPrototypeObject(exception_state);
  if (exception_state.HadException()) {
    return;
  }

  v8::Local<v8::Function> v8_run =
      retriever.GetMethodOrThrow("run", exception_state);
  if (exception_state.HadException()) {
    return;
  }

  auto* operation_definition =
      MakeGarbageCollected<SharedStorageOperationDefinition>(
          ScriptController()->GetScriptState(), name, operation_ctor, v8_run);

  operation_definition_map_.Set(name, operation_definition);
}

void SharedStorageWorkletGlobalScope::OnConsoleApiMessage(
    mojom::ConsoleMessageLevel level,
    const String& message,
    SourceLocation* location) {
  client_->ConsoleLog(message);

  WorkerOrWorkletGlobalScope::OnConsoleApiMessage(level, message, location);
}

void SharedStorageWorkletGlobalScope::NotifyContextDestroyed() {
  if (private_aggregation_) {
    CHECK(ShouldDefinePrivateAggregationInSharedStorage());
    private_aggregation_->OnWorkletDestroyed();
  }

  WorkletGlobalScope::NotifyContextDestroyed();
}

bool SharedStorageWorkletGlobalScope::FeatureEnabled(
    OriginTrialFeature feature) const {
  // The shared storage worklet infrastructure doesn't yet support checking the
  // origin trial features. We'll go over each feature that can potentially be
  // checked (e.g. IDL attribute/interface exposures conditioned on
  // RuntimeEnabled=XXX), and replicate their status manually.

  // The worklet must have been created from a context eligible for shared
  // storage. It's okay to treat `kSharedStorageAPI` as enabled.
  if (feature == OriginTrialFeature::kSharedStorageAPI) {
    return true;
  }

  // For unknown features, return false to be on the safe side.
  return false;
}

void SharedStorageWorkletGlobalScope::Trace(Visitor* visitor) const {
  visitor->Trace(receiver_);
  visitor->Trace(shared_storage_);
  visitor->Trace(private_aggregation_);
  visitor->Trace(crypto_);
  visitor->Trace(operation_definition_map_);
  visitor->Trace(client_);
  WorkletGlobalScope::Trace(visitor);
  Supplementable<SharedStorageWorkletGlobalScope>::Trace(visitor);
}

void SharedStorageWorkletGlobalScope::Initialize(
    mojo::PendingAssociatedRemote<
        mojom::blink::SharedStorageWorkletServiceClient> client,
    bool private_aggregation_permissions_policy_allowed,
    const String& embedder_context) {
  client_.Bind(std::move(client),
               GetTaskRunner(blink::TaskType::kMiscPlatformAPI));

  private_aggregation_permissions_policy_allowed_ =
      private_aggregation_permissions_policy_allowed;

  embedder_context_ = embedder_context;
}

void SharedStorageWorkletGlobalScope::AddModule(
    mojo::PendingRemote<network::mojom::blink::URLLoaderFactory>
        pending_url_loader_factory,
    const KURL& script_source_url,
    AddModuleCallback callback) {
  mojo::Remote<network::mojom::URLLoaderFactory> url_loader_factory(
      CrossVariantMojoRemote<network::mojom::URLLoaderFactoryInterfaceBase>(
          std::move(pending_url_loader_factory)));

  module_script_downloader_ = std::make_unique<ModuleScriptDownloader>(
      url_loader_factory.get(), GURL(script_source_url),
      WTF::BindOnce(&SharedStorageWorkletGlobalScope::OnModuleScriptDownloaded,
                    WrapWeakPersistent(this), script_source_url,
                    std::move(callback)));
}

void SharedStorageWorkletGlobalScope::RunURLSelectionOperation(
    const String& name,
    const Vector<KURL>& urls,
    BlinkCloneableMessage serialized_data,
    mojo::PendingRemote<mojom::blink::PrivateAggregationHost>
        private_aggregation_host,
    RunURLSelectionOperationCallback callback) {
  String error_message;
  SharedStorageOperationDefinition* operation_definition = nullptr;
  if (!PerformCommonOperationChecks(name, error_message,
                                    operation_definition)) {
    std::move(callback).Run(
        /*success=*/false, error_message,
        /*length=*/0);
    return;
  }

  base::OnceClosure operation_completion_cb =
      StartOperation(std::move(private_aggregation_host));
  RunURLSelectionOperationCallback combined_operation_completion_cb =
      std::move(callback).Then(std::move(operation_completion_cb));

  DCHECK(operation_definition);

  ScriptState* script_state = operation_definition->GetScriptState();
  ScriptState::Scope scope(script_state);

  v8::Isolate* isolate = script_state->GetIsolate();
  v8::TryCatch try_catch(isolate);
  try_catch.SetVerbose(true);

  TraceWrapperV8Reference<v8::Value> instance =
      operation_definition->GetInstance();
  V8RunFunctionForSharedStorageSelectURLOperation* registered_run_function =
      operation_definition->GetRunFunctionForSharedStorageSelectURLOperation();

  Vector<String> urls_param;
  base::ranges::transform(urls, std::back_inserter(urls_param),
                          [](const KURL& url) { return url.GetString(); });

  absl::optional<ScriptValue> data_param =
      Deserialize(isolate, /*execution_context=*/this, serialized_data);
  if (!data_param) {
    std::move(combined_operation_completion_cb)
        .Run(/*success=*/false, kCannotDeserializeDataErrorMessage,
             /*index=*/0);
    return;
  }

  v8::Maybe<ScriptPromise> result = registered_run_function->Invoke(
      instance.Get(isolate), urls_param, *data_param);

  if (try_catch.HasCaught()) {
    v8::Local<v8::Value> exception = try_catch.Exception();
    std::move(combined_operation_completion_cb)
        .Run(/*success=*/false, ExceptionToString(script_state, exception),
             /*index=*/0);
    return;
  }

  if (result.IsNothing()) {
    std::move(combined_operation_completion_cb)
        .Run(/*success=*/false, "Internal error.",
             /*index=*/0);
    return;
  }

  auto* unresolved_request = MakeGarbageCollected<UnresolvedSelectURLRequest>(
      urls.size(), std::move(combined_operation_completion_cb));

  ScriptPromise promise = result.FromJust();

  auto* success_callback = MakeGarbageCollected<ScriptFunction>(
      script_state, MakeGarbageCollected<SelectURLResolutionSuccessCallback>(
                        unresolved_request));
  auto* failure_callback = MakeGarbageCollected<ScriptFunction>(
      script_state, MakeGarbageCollected<SelectURLResolutionFailureCallback>(
                        unresolved_request));

  promise.Then(success_callback, failure_callback);
}

void SharedStorageWorkletGlobalScope::RunOperation(
    const String& name,
    BlinkCloneableMessage serialized_data,
    mojo::PendingRemote<mojom::blink::PrivateAggregationHost>
        private_aggregation_host,
    RunOperationCallback callback) {
  String error_message;
  SharedStorageOperationDefinition* operation_definition = nullptr;
  if (!PerformCommonOperationChecks(name, error_message,
                                    operation_definition)) {
    std::move(callback).Run(
        /*success=*/false, error_message);
    return;
  }

  base::OnceClosure operation_completion_cb =
      StartOperation(std::move(private_aggregation_host));
  mojom::blink::SharedStorageWorkletService::RunOperationCallback
      combined_operation_completion_cb =
          std::move(callback).Then(std::move(operation_completion_cb));

  DCHECK(operation_definition);

  ScriptState* script_state = operation_definition->GetScriptState();
  ScriptState::Scope scope(script_state);

  v8::Isolate* isolate = script_state->GetIsolate();
  v8::TryCatch try_catch(isolate);
  try_catch.SetVerbose(true);

  TraceWrapperV8Reference<v8::Value> instance =
      operation_definition->GetInstance();
  V8RunFunctionForSharedStorageRunOperation* registered_run_function =
      operation_definition->GetRunFunctionForSharedStorageRunOperation();

  absl::optional<ScriptValue> data_param =
      Deserialize(isolate, /*execution_context=*/this, serialized_data);
  if (!data_param) {
    std::move(combined_operation_completion_cb)
        .Run(/*success=*/false, kCannotDeserializeDataErrorMessage);
    return;
  }

  v8::Maybe<ScriptPromise> result =
      registered_run_function->Invoke(instance.Get(isolate), *data_param);

  if (try_catch.HasCaught()) {
    v8::Local<v8::Value> exception = try_catch.Exception();
    std::move(combined_operation_completion_cb)
        .Run(/*success=*/false, ExceptionToString(script_state, exception));
    return;
  }

  if (result.IsNothing()) {
    std::move(combined_operation_completion_cb)
        .Run(/*success=*/false, "Internal error.");
    return;
  }

  auto* unresolved_request = MakeGarbageCollected<UnresolvedRunRequest>(
      std::move(combined_operation_completion_cb));

  ScriptPromise promise = result.FromJust();

  auto* success_callback = MakeGarbageCollected<ScriptFunction>(
      script_state,
      MakeGarbageCollected<RunResolutionSuccessCallback>(unresolved_request));
  auto* failure_callback = MakeGarbageCollected<ScriptFunction>(
      script_state,
      MakeGarbageCollected<RunResolutionFailureCallback>(unresolved_request));

  promise.Then(success_callback, failure_callback);
}

SharedStorage* SharedStorageWorkletGlobalScope::sharedStorage(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  if (!add_module_finished_) {
    CHECK(!shared_storage_);

    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotAllowedError,
        "sharedStorage cannot be accessed during addModule().");

    return nullptr;
  }

  // As long as `addModule()` has finished, it should be fine to expose
  // `sharedStorage`: on the browser side, we already enforce that `addModule()`
  // can only be called once, so there's no way to expose the storage data to
  // the associated `Document`.
  if (!shared_storage_) {
    shared_storage_ = MakeGarbageCollected<SharedStorage>();
  }

  return shared_storage_.Get();
}

PrivateAggregation* SharedStorageWorkletGlobalScope::privateAggregation(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  CHECK(ShouldDefinePrivateAggregationInSharedStorage());

  if (!add_module_finished_) {
    CHECK(!private_aggregation_);

    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotAllowedError,
        "privateAggregation cannot be accessed during addModule().");

    return nullptr;
  }

  return GetOrCreatePrivateAggregation();
}

Crypto* SharedStorageWorkletGlobalScope::crypto(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  if (!crypto_) {
    crypto_ = MakeGarbageCollected<Crypto>();
  }

  return crypto_.Get();
}

// Returns the unique ID for the currently running operation.
int64_t SharedStorageWorkletGlobalScope::GetCurrentOperationId() {
  ScriptState* script_state = ScriptController()->GetScriptState();
  DCHECK(script_state);

  v8::Local<v8::Context> context = script_state->GetContext();

  v8::Local<v8::Value> data = context->GetContinuationPreservedEmbedderData();
  return data.As<v8::BigInt>()->Int64Value();
}

void SharedStorageWorkletGlobalScope::OnModuleScriptDownloaded(
    const KURL& script_source_url,
    mojom::blink::SharedStorageWorkletService::AddModuleCallback callback,
    std::unique_ptr<std::string> response_body,
    std::string error_message) {
  module_script_downloader_.reset();

  mojom::blink::SharedStorageWorkletService::AddModuleCallback
      add_module_finished_callback = std::move(callback).Then(WTF::BindOnce(
          &SharedStorageWorkletGlobalScope::RecordAddModuleFinished,
          WrapPersistent(this)));

  if (!response_body) {
    std::move(add_module_finished_callback)
        .Run(false, String(error_message.c_str()));
    return;
  }

  DCHECK(error_message.empty());

  if (!ScriptController()) {
    std::move(add_module_finished_callback)
        .Run(false, /*error_message=*/"Worklet is being destroyed.");
    return;
  }

  ScriptState* script_state = ScriptController()->GetScriptState();
  DCHECK(script_state);

  // TODO(crbug.com/1419253): Using a classic script with the custom script
  // loader is tentative. Eventually, this should migrate to the blink-worklet's
  // script loading infrastructure.
  ClassicScript* worker_script =
      ClassicScript::Create(String(*response_body),
                            /*source_url=*/script_source_url,
                            /*base_url=*/KURL(), ScriptFetchOptions());

  v8::HandleScope handle_scope(script_state->GetIsolate());
  ScriptEvaluationResult result =
      worker_script->RunScriptOnScriptStateAndReturnValue(script_state);

  if (result.GetResultType() ==
      ScriptEvaluationResult::ResultType::kException) {
    v8::Local<v8::Value> exception = result.GetExceptionForWorklet();

    std::move(add_module_finished_callback)
        .Run(false,
             /*error_message=*/ExceptionToString(script_state, exception));
    return;
  } else if (result.GetResultType() !=
             ScriptEvaluationResult::ResultType::kSuccess) {
    std::move(add_module_finished_callback)
        .Run(false, /*error_message=*/"Internal Failure");
    return;
  }

  std::move(add_module_finished_callback)
      .Run(true, /*error_message=*/g_empty_string);
}

void SharedStorageWorkletGlobalScope::RecordAddModuleFinished() {
  add_module_finished_ = true;
}

bool SharedStorageWorkletGlobalScope::PerformCommonOperationChecks(
    const String& operation_name,
    String& error_message,
    SharedStorageOperationDefinition*& operation_definition) {
  DCHECK(error_message.empty());
  DCHECK_EQ(operation_definition, nullptr);

  if (!add_module_finished_) {
    // TODO(http://crbug/1249581): if this operation comes while fetching the
    // module script, we might want to queue the operation to be handled later
    // after addModule completes.
    error_message = "The module script hasn't been loaded.";
    return false;
  }

  auto it = operation_definition_map_.find(operation_name);
  if (it == operation_definition_map_.end()) {
    error_message = "Cannot find operation name.";
    return false;
  }

  operation_definition = it->value;

  ScriptState* script_state = operation_definition->GetScriptState();

  ScriptState::Scope scope(script_state);

  TraceWrapperV8Reference<v8::Value> instance =
      operation_definition->GetInstance();
  if (instance.IsEmpty()) {
    error_message = "Internal error.";
    return false;
  }

  return true;
}

base::OnceClosure SharedStorageWorkletGlobalScope::StartOperation(
    mojo::PendingRemote<mojom::blink::PrivateAggregationHost>
        private_aggregation_host) {
  CHECK(add_module_finished_);
  CHECK_EQ(!!private_aggregation_host,
           ShouldDefinePrivateAggregationInSharedStorage());

  int64_t operation_id = operation_counter_++;

  ScriptState* script_state = ScriptController()->GetScriptState();
  DCHECK(script_state);

  v8::HandleScope handle_scope(script_state->GetIsolate());
  v8::Local<v8::Context> context = script_state->GetContext();

  context->SetContinuationPreservedEmbedderData(
      v8::BigInt::New(context->GetIsolate(), operation_id));

  if (ShouldDefinePrivateAggregationInSharedStorage()) {
    GetOrCreatePrivateAggregation()->OnOperationStarted(
        operation_id, std::move(private_aggregation_host));
  }

  return WTF::BindOnce(&SharedStorageWorkletGlobalScope::FinishOperation,
                       WrapPersistent(this), operation_id);
}

void SharedStorageWorkletGlobalScope::FinishOperation(int64_t operation_id) {
  if (ShouldDefinePrivateAggregationInSharedStorage()) {
    CHECK(private_aggregation_);
    private_aggregation_->OnOperationFinished(operation_id);
  }
}

PrivateAggregation*
SharedStorageWorkletGlobalScope::GetOrCreatePrivateAggregation() {
  CHECK(ShouldDefinePrivateAggregationInSharedStorage());
  CHECK(add_module_finished_);

  if (!private_aggregation_) {
    private_aggregation_ = MakeGarbageCollected<PrivateAggregation>(this);
  }

  return private_aggregation_.Get();
}

}  // namespace blink
