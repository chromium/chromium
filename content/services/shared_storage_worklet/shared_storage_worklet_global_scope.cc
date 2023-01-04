// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/shared_storage_worklet/shared_storage_worklet_global_scope.h"

#include <memory>
#include <string>
#include <utility>

#include "base/task/single_thread_task_runner.h"
#include "content/common/private_aggregation_host.mojom.h"
#include "content/services/shared_storage_worklet/console.h"
#include "content/services/shared_storage_worklet/module_script_downloader.h"
#include "content/services/shared_storage_worklet/private_aggregation.h"
#include "content/services/shared_storage_worklet/shared_storage.h"
#include "content/services/shared_storage_worklet/unnamed_operation_handler.h"
#include "content/services/shared_storage_worklet/url_selection_operation_handler.h"
#include "content/services/shared_storage_worklet/worklet_v8_helper.h"
#include "gin/arguments.h"
#include "gin/function_template.h"
#include "gin/public/isolate_holder.h"
#include "gin/v8_initializer.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-function.h"
#include "v8/include/v8-initialization.h"
#include "v8/include/v8-object.h"

namespace shared_storage_worklet {

namespace {

// Initialize V8 (and gin).
void InitV8() {
#if defined(V8_USE_EXTERNAL_STARTUP_DATA)
  gin::V8Initializer::LoadV8Snapshot();
#endif

  gin::IsolateHolder::Initialize(gin::IsolateHolder::kNonStrictMode,
                                 gin::ArrayBufferAllocator::SharedInstance());
}

}  // namespace

SharedStorageWorkletGlobalScope::SharedStorageWorkletGlobalScope(
    bool private_aggregation_permissions_policy_allowed)
    : private_aggregation_permissions_policy_allowed_(
          private_aggregation_permissions_policy_allowed) {}

SharedStorageWorkletGlobalScope::~SharedStorageWorkletGlobalScope() = default;

void SharedStorageWorkletGlobalScope::AddModule(
    mojo::PendingRemote<network::mojom::URLLoaderFactory>
        pending_url_loader_factory,
    mojom::SharedStorageWorkletServiceClient* client,
    content::mojom::PrivateAggregationHost* private_aggregation_host,
    const GURL& script_source_url,
    mojom::SharedStorageWorkletService::AddModuleCallback callback) {
  mojo::Remote<network::mojom::URLLoaderFactory> url_loader_factory(
      std::move(pending_url_loader_factory));

  module_script_downloader_ = std::make_unique<ModuleScriptDownloader>(
      url_loader_factory.get(), script_source_url,
      base::BindOnce(&SharedStorageWorkletGlobalScope::OnModuleScriptDownloaded,
                     weak_ptr_factory_.GetWeakPtr(), client,
                     private_aggregation_host, script_source_url,
                     std::move(callback)));
}

void SharedStorageWorkletGlobalScope::OnModuleScriptDownloaded(
    mojom::SharedStorageWorkletServiceClient* client,
    content::mojom::PrivateAggregationHost* private_aggregation_host,
    const GURL& script_source_url,
    mojom::SharedStorageWorkletService::AddModuleCallback callback,
    std::unique_ptr<std::string> response_body,
    std::string error_message) {
  module_script_downloader_.reset();

  if (!response_body) {
    std::move(callback).Run(false, error_message);
    return;
  }

  DCHECK(error_message.empty());

  // Now the module script is downloaded, initialize the worklet environment.
  InitV8();

  // TODO(yaoxia): we may need a new IsolateType here. For now, set it to
  // `kBlinkWorkerThread` even though it's not technically for blink worker:
  // this is the best approximate type and is the only type that allows multiple
  // isolates in one process (see CanHaveMultipleIsolates(isolate_type) in
  // gin/v8_isolate_memory_dump_provider.cc).
  isolate_holder_ = std::make_unique<gin::IsolateHolder>(
      base::SingleThreadTaskRunner::GetCurrentDefault(),
      gin::IsolateHolder::kSingleThread,
      gin::IsolateHolder::IsolateType::kBlinkWorkerThread);

  WorkletV8Helper::HandleScope scope(Isolate());
  global_context_.Reset(Isolate(), v8::Context::New(Isolate()));

  v8::Local<v8::Context> context = LocalContext();
  v8::Context::Scope context_scope(context);

  v8::Local<v8::Object> global = context->Global();

  url_selection_operation_handler_ =
      std::make_unique<UrlSelectionOperationHandler>(operation_definition_map_);

  unnamed_operation_handler_ =
      std::make_unique<UnnamedOperationHandler>(operation_definition_map_);

  console_ = std::make_unique<Console>(client);
  global
      ->Set(context, gin::StringToSymbol(Isolate(), "console"),
            console_->GetWrapper(Isolate()).ToLocalChecked())
      .Check();

  if (private_aggregation_host) {
    private_aggregation_ = std::make_unique<PrivateAggregation>(
        *client, private_aggregation_permissions_policy_allowed_,
        *private_aggregation_host);
    global
        ->Set(context, gin::StringToSymbol(Isolate(), "privateAggregation"),
              private_aggregation_->GetWrapper(Isolate()).ToLocalChecked())
        .Check();
  }

  global
      ->Set(context, gin::StringToSymbol(Isolate(), "register"),
            gin::CreateFunctionTemplate(
                Isolate(),
                base::BindRepeating(&SharedStorageWorkletGlobalScope::Register,
                                    weak_ptr_factory_.GetWeakPtr()))
                ->GetFunction(context)
                .ToLocalChecked())
      .Check();

  // Execute the module script.
  v8::MaybeLocal<v8::Value> result = WorkletV8Helper::CompileAndRunScript(
      context, *response_body, script_source_url, &error_message);

  if (result.IsEmpty()) {
    std::move(callback).Run(false, error_message);
    return;
  }

  DCHECK(error_message.empty());

  // After the module script execution, create and expose the shared storage
  // object.
  shared_storage_ = std::make_unique<SharedStorage>(client);
  context->Global()
      ->Set(context, gin::StringToSymbol(Isolate(), "sharedStorage"),
            shared_storage_->GetWrapper(Isolate()).ToLocalChecked())
      .Check();

  std::move(callback).Run(true, {});
}

void SharedStorageWorkletGlobalScope::RunURLSelectionOperation(
    const std::string& name,
    const std::vector<GURL>& urls,
    const std::vector<uint8_t>& serialized_data,
    mojom::SharedStorageWorkletService::RunURLSelectionOperationCallback
        callback) {
  if (!isolate_holder_) {
    // TODO(yaoxia): if this operation comes while fetching the module script,
    // we might want to queue the operation to be handled later after addModule
    // completes. http://crbug/1249581
    std::move(callback).Run(
        /*success=*/false,
        /*error_message=*/"The module script hasn't been loaded.",
        /*length=*/0);
    return;
  }

  WorkletV8Helper::HandleScope scope(Isolate());
  url_selection_operation_handler_->RunOperation(
      LocalContext(), name, urls, serialized_data, std::move(callback));
}

void SharedStorageWorkletGlobalScope::RunOperation(
    const std::string& name,
    const std::vector<uint8_t>& serialized_data,
    mojom::SharedStorageWorkletService::RunOperationCallback callback) {
  if (!isolate_holder_) {
    // TODO(yaoxia): if this operation comes while fetching the module script,
    // we might want to queue the operation to be handled later after addModule
    // completes. http://crbug/1249581
    std::move(callback).Run(
        /*success=*/false,
        /*error_message=*/"The module script hasn't been loaded.");
    return;
  }

  WorkletV8Helper::HandleScope scope(Isolate());
  unnamed_operation_handler_->RunOperation(
      LocalContext(), name, serialized_data, std::move(callback));
}

void SharedStorageWorkletGlobalScope::Register(gin::Arguments* args) {
  std::string name;
  if (!args->GetNext(&name)) {
    args->ThrowTypeError("Missing \"name\" argument in operation registration");
    return;
  }

  if (name.empty()) {
    args->ThrowTypeError("Operation name cannot be empty");
    return;
  }

  if (operation_definition_map_.count(name)) {
    args->ThrowTypeError("Operation name already registered");
    return;
  }

  v8::Local<v8::Object> class_definition;
  if (!args->GetNext(&class_definition)) {
    args->ThrowTypeError(
        "Missing class name argument in operation registration");
    return;
  }

  if (!class_definition->IsConstructor()) {
    args->ThrowTypeError("Unexpected class argument: not a constructor");
    return;
  }

  v8::Isolate* isolate = args->isolate();
  v8::Local<v8::Context> context = args->GetHolderCreationContext();

  v8::Local<v8::Value> class_prototype =
      class_definition->Get(context, gin::StringToV8(isolate, "prototype"))
          .ToLocalChecked();

  if (!class_prototype->IsObject()) {
    args->ThrowTypeError("Unexpected class prototype: not an object");
    return;
  }

  v8::Local<v8::Value> run_function =
      class_prototype.As<v8::Object>()
          ->Get(context, gin::StringToV8(isolate, "run"))
          .ToLocalChecked();

  if (run_function->IsUndefined() || !run_function->IsFunction()) {
    args->ThrowTypeError("Missing \"run()\" function in the class");
    return;
  }

  operation_definition_map_.emplace(
      name, v8::Global<v8::Function>(isolate, run_function.As<v8::Function>()));
}

v8::Isolate* SharedStorageWorkletGlobalScope::Isolate() {
  return isolate_holder_->isolate();
}

v8::Local<v8::Context> SharedStorageWorkletGlobalScope::LocalContext() {
  return global_context_.Get(Isolate());
}

}  // namespace shared_storage_worklet
