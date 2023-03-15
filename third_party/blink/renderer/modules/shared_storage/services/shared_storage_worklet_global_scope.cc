// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/shared_storage/services/shared_storage_worklet_global_scope.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "gin/converter.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/common/shared_storage/module_script_downloader.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/worker_or_worklet_script_controller.h"
#include "third_party/blink/renderer/core/script/classic_script.h"
#include "third_party/blink/renderer/core/workers/global_scope_creation_params.h"
#include "third_party/blink/renderer/core/workers/threaded_messaging_proxy_base.h"
#include "third_party/blink/renderer/modules/shared_storage/services/shared_storage_worklet_thread.h"

namespace blink {

// We try to use .stack property so that the error message contains a stack
// trace, but otherwise fallback to .toString().
std::string ExceptionToString(ScriptState* script_state,
                              v8::Local<v8::Value> exception) {
  v8::Isolate* isolate = script_state->GetIsolate();

  if (!exception.IsEmpty()) {
    v8::Local<v8::Context> context = script_state->GetContext();
    v8::Local<v8::Value> value =
        v8::TryCatch::StackTrace(context, exception).FromMaybe(exception);
    v8::Local<v8::String> value_string;
    if (value->ToString(context).ToLocal(&value_string)) {
      return gin::V8ToString(isolate, value_string);
    }
  }

  return "Unknown Failure";
}

SharedStorageWorkletGlobalScope::SharedStorageWorkletGlobalScope(
    std::unique_ptr<GlobalScopeCreationParams> creation_params,
    WorkerThread* thread)
    : WorkletGlobalScope(std::move(creation_params),
                         thread->GetWorkerReportingProxy(),
                         thread,
                         /*create_microtask_queue=*/true) {}

SharedStorageWorkletGlobalScope::~SharedStorageWorkletGlobalScope() = default;

void SharedStorageWorkletGlobalScope::BindSharedStorageWorkletService(
    mojo::PendingReceiver<mojom::SharedStorageWorkletService> receiver,
    base::OnceClosure disconnect_handler) {
  receiver_.Bind(std::move(receiver),
                 GetTaskRunner(blink::TaskType::kMiscPlatformAPI));

  // When `SharedStorageWorkletHost` is destroyed, the disconnect handler will
  // be called, and we rely on this explicit signal to clean up the worklet
  // environment.
  receiver_.set_disconnect_handler(std::move(disconnect_handler));
}

void SharedStorageWorkletGlobalScope::Trace(Visitor* visitor) const {
  visitor->Trace(receiver_);
  visitor->Trace(client_);
  visitor->Trace(private_aggregation_host_);
  WorkletGlobalScope::Trace(visitor);
}

void SharedStorageWorkletGlobalScope::Initialize(
    mojo::PendingAssociatedRemote<mojom::SharedStorageWorkletServiceClient>
        client,
    bool private_aggregation_permissions_policy_allowed,
    mojo::PendingRemote<mojom::PrivateAggregationHost> private_aggregation_host,
    const absl::optional<std::u16string>& embedder_context) {
  client_.Bind(std::move(client),
               GetTaskRunner(blink::TaskType::kMiscPlatformAPI));
  private_aggregation_permissions_policy_allowed_ =
      private_aggregation_permissions_policy_allowed;
  if (private_aggregation_host) {
    private_aggregation_host_.Bind(
        std::move(private_aggregation_host),
        GetTaskRunner(blink::TaskType::kMiscPlatformAPI));
  }
}

void SharedStorageWorkletGlobalScope::AddModule(
    mojo::PendingRemote<network::mojom::URLLoaderFactory>
        pending_url_loader_factory,
    const GURL& script_source_url,
    AddModuleCallback callback) {
  mojo::Remote<network::mojom::URLLoaderFactory> url_loader_factory(
      std::move(pending_url_loader_factory));

  module_script_downloader_ = std::make_unique<ModuleScriptDownloader>(
      url_loader_factory.get(), script_source_url,
      WTF::BindOnce(&SharedStorageWorkletGlobalScope::OnModuleScriptDownloaded,
                    WrapWeakPersistent(this), script_source_url,
                    std::move(callback)));
}

void SharedStorageWorkletGlobalScope::RunURLSelectionOperation(
    const std::string& name,
    const std::vector<GURL>& urls,
    const std::vector<uint8_t>& serialized_data,
    RunURLSelectionOperationCallback callback) {
  NOTIMPLEMENTED();

  std::move(callback).Run(
      /*success=*/false,
      /*error_message=*/"Not implemented",
      /*length=*/0);
}

void SharedStorageWorkletGlobalScope::RunOperation(
    const std::string& name,
    const std::vector<uint8_t>& serialized_data,
    RunOperationCallback callback) {
  NOTIMPLEMENTED();

  std::move(callback).Run(
      /*success=*/false,
      /*error_message=*/"Not implemented");
}

void SharedStorageWorkletGlobalScope::OnModuleScriptDownloaded(
    const GURL& script_source_url,
    mojom::SharedStorageWorkletService::AddModuleCallback callback,
    std::unique_ptr<std::string> response_body,
    std::string error_message) {
  module_script_downloader_.reset();

  if (!response_body) {
    std::move(callback).Run(false, std::string(error_message));
    return;
  }

  DCHECK(error_message.empty());

  ScriptState* script_state = ScriptController()->GetScriptState();
  DCHECK(script_state);

  // TODO(crbug.com/1419253): Using a classic script with the custom script
  // loader is tentative. Eventually, this should migrate to the blink-worklet's
  // script loading infrastructure.
  ClassicScript* worker_script = ClassicScript::Create(
      String(*response_body),
      /*source_url=*/KURL(String(script_source_url.spec().c_str())),
      /*base_url=*/KURL(), ScriptFetchOptions());

  v8::HandleScope handle_scope(script_state->GetIsolate());
  ScriptEvaluationResult result =
      worker_script->RunScriptOnScriptStateAndReturnValue(script_state);

  if (result.GetResultType() ==
      ScriptEvaluationResult::ResultType::kException) {
    v8::Local<v8::Value> exception = result.GetExceptionForWorklet();

    std::move(callback).Run(
        false, /*error_message=*/ExceptionToString(script_state, exception));
    return;
  } else if (result.GetResultType() !=
             ScriptEvaluationResult::ResultType::kSuccess) {
    std::move(callback).Run(false, /*error_message=*/"Internal Failure");
    return;
  }

  std::move(callback).Run(true, /*error_message=*/{});
}

}  // namespace blink
