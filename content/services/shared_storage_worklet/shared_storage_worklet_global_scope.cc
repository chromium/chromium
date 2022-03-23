// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/shared_storage_worklet/shared_storage_worklet_global_scope.h"

#include <memory>
#include <string>
#include <utility>

#include "base/threading/thread_task_runner_handle.h"
#include "content/services/shared_storage_worklet/console.h"
#include "content/services/shared_storage_worklet/module_script_downloader.h"
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

SharedStorageWorkletGlobalScope::SharedStorageWorkletGlobalScope() = default;
SharedStorageWorkletGlobalScope::~SharedStorageWorkletGlobalScope() = default;

void SharedStorageWorkletGlobalScope::AddModule(
    mojo::PendingRemote<network::mojom::URLLoaderFactory>
        pending_url_loader_factory,
    mojom::SharedStorageWorkletServiceClient* client,
    const GURL& script_source_url,
    mojom::SharedStorageWorkletService::AddModuleCallback callback) {
  mojo::Remote<network::mojom::URLLoaderFactory> url_loader_factory(
      std::move(pending_url_loader_factory));

  module_script_downloader_ = std::make_unique<ModuleScriptDownloader>(
      url_loader_factory.get(), script_source_url,
      base::BindOnce(&SharedStorageWorkletGlobalScope::OnModuleScriptDownloaded,
                     weak_ptr_factory_.GetWeakPtr(), client, script_source_url,
                     std::move(callback)));
}

void SharedStorageWorkletGlobalScope::OnModuleScriptDownloaded(
    mojom::SharedStorageWorkletServiceClient* client,
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
      base::ThreadTaskRunnerHandle::Get(), gin::IsolateHolder::kSingleThread,
      gin::IsolateHolder::IsolateType::kBlinkWorkerThread);

  WorkletV8Helper::HandleScope scope(Isolate());
  global_context_.Reset(Isolate(), v8::Context::New(Isolate()));

  v8::Local<v8::Context> context = LocalContext();
  v8::Context::Scope context_scope(context);

  v8::Local<v8::Object> global = context->Global();

  url_selection_operation_handler_ =
      std::make_unique<UrlSelectionOperationHandler>();

  unnamed_operation_handler_ = std::make_unique<UnnamedOperationHandler>();

  console_ = std::make_unique<Console>(client);
  global
      ->Set(context, gin::StringToSymbol(Isolate(), "console"),
            console_->GetWrapper(Isolate()).ToLocalChecked())
      .Check();

  global
      ->Set(
          context,
          gin::StringToSymbol(Isolate(), "registerURLSelectionOperation"),
          gin::CreateFunctionTemplate(
              Isolate(), base::BindRepeating(&SharedStorageWorkletGlobalScope::
                                                 RegisterURLSelectionOperation,
                                             weak_ptr_factory_.GetWeakPtr()))
              ->GetFunction(context)
              .ToLocalChecked())
      .Check();

  global
      ->Set(context, gin::StringToSymbol(Isolate(), "registerOperation"),
            gin::CreateFunctionTemplate(
                Isolate(),
                base::BindRepeating(
                    &SharedStorageWorkletGlobalScope::RegisterOperation,
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

void SharedStorageWorkletGlobalScope::RegisterURLSelectionOperation(
    gin::Arguments* args) {
  url_selection_operation_handler_->RegisterOperation(args);
}

void SharedStorageWorkletGlobalScope::RegisterOperation(gin::Arguments* args) {
  unnamed_operation_handler_->RegisterOperation(args);
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

v8::Isolate* SharedStorageWorkletGlobalScope::Isolate() {
  return isolate_holder_->isolate();
}

v8::Local<v8::Context> SharedStorageWorkletGlobalScope::LocalContext() {
  return global_context_.Get(Isolate());
}

}  // namespace shared_storage_worklet
