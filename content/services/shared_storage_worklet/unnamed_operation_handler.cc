// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/shared_storage_worklet/unnamed_operation_handler.h"

#include "content/services/shared_storage_worklet/worklet_v8_helper.h"
#include "gin/arguments.h"
#include "gin/function_template.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-function.h"
#include "v8/include/v8-object.h"
#include "v8/include/v8-promise.h"
#include "v8/include/v8-value-serializer.h"

namespace shared_storage_worklet {

struct UnnamedOperationHandler::PendingRequest {
  explicit PendingRequest(
      mojom::SharedStorageWorkletService::RunOperationCallback callback);

  ~PendingRequest();

  mojom::SharedStorageWorkletService::RunOperationCallback callback;
};

UnnamedOperationHandler::PendingRequest::PendingRequest(
    mojom::SharedStorageWorkletService::RunOperationCallback callback)
    : callback(std::move(callback)) {}

UnnamedOperationHandler::PendingRequest::~PendingRequest() = default;

UnnamedOperationHandler::UnnamedOperationHandler(
    const std::map<std::string, v8::Global<v8::Function>>&
        operation_definition_map)
    : operation_definition_map_(operation_definition_map) {}

UnnamedOperationHandler::~UnnamedOperationHandler() = default;

void UnnamedOperationHandler::RunOperation(
    v8::Local<v8::Context> context,
    const std::string& name,
    const std::vector<uint8_t>& serialized_data,
    mojom::SharedStorageWorkletService::RunOperationCallback callback) {
  auto it = operation_definition_map_->find(name);
  if (it == operation_definition_map_->end()) {
    std::move(callback).Run(/*success=*/false, "Cannot find operation name.");
    return;
  }

  v8::Isolate* isolate = context->GetIsolate();

  v8::Context::Scope context_scope(context);

  v8::Local<v8::Function> run_function = it->second.Get(isolate);

  v8::Local<v8::Object> js_data;

  if (serialized_data.empty()) {
    js_data = v8::Object::New(isolate);
  } else {
    v8::ValueDeserializer deserializer(isolate, serialized_data.data(),
                                       serialized_data.size());
    v8::Local<v8::Value> value =
        deserializer.ReadValue(context).ToLocalChecked();
    js_data = value->ToObject(context).ToLocalChecked();
  }

  std::vector<v8::Local<v8::Value>> args{js_data};

  std::string error_message;
  v8::MaybeLocal<v8::Value> result = WorkletV8Helper::InvokeFunction(
      context, run_function, args, &error_message);

  if (result.IsEmpty()) {
    std::move(callback).Run(
        /*success=*/false, error_message);
    return;
  }

  if (!result.ToLocalChecked()->IsPromise()) {
    std::move(callback).Run(/*success=*/false,
                            "run() did not return a promise.");
    return;
  }

  v8::Local<v8::Promise> result_promise =
      result.ToLocalChecked().As<v8::Promise>();

  // If the promise is already completed, retrieve and handle the result
  // directly.
  if (result_promise->State() == v8::Promise::PromiseState::kFulfilled) {
    std::move(callback).Run(/*success=*/true, /*error_message=*/{});
    return;
  }

  if (result_promise->State() == v8::Promise::PromiseState::kRejected) {
    error_message = gin::V8ToString(
        isolate,
        result_promise->Result()->ToDetailString(context).ToLocalChecked());

    std::move(callback).Run(
        /*success=*/false, error_message);
    return;
  }

  // If the promise is pending, install callback functions that will be
  // triggered when it completes.
  auto pending_request = std::make_unique<PendingRequest>(std::move(callback));
  PendingRequest* pending_request_raw = pending_request.get();
  pending_requests_.emplace(pending_request_raw, std::move(pending_request));

  v8::Local<v8::Function> fulfilled_callback =
      gin::CreateFunctionTemplate(
          isolate, base::BindRepeating(
                       &UnnamedOperationHandler::OnPromiseFulfilled,
                       weak_ptr_factory_.GetWeakPtr(), pending_request_raw))
          ->GetFunction(context)
          .ToLocalChecked();

  v8::Local<v8::Function> rejected_callback =
      gin::CreateFunctionTemplate(
          isolate, base::BindRepeating(
                       &UnnamedOperationHandler::OnPromiseRejected,
                       weak_ptr_factory_.GetWeakPtr(), pending_request_raw))
          ->GetFunction(context)
          .ToLocalChecked();

  result_promise->Then(context, fulfilled_callback, rejected_callback)
      .ToLocalChecked();
}

void UnnamedOperationHandler::OnPromiseFulfilled(PendingRequest* request,
                                                 gin::Arguments* args) {
  std::move(request->callback).Run(/*success=*/true, /*error_message=*/{});
  pending_requests_.erase(request);
}

void UnnamedOperationHandler::OnPromiseRejected(PendingRequest* request,
                                                gin::Arguments* args) {
  std::string error_message;
  if (!args->GetNext(&error_message)) {
    std::move(request->callback)
        .Run(/*success=*/false,
             "Promise is rejected without an explicit error message.");
    pending_requests_.erase(request);
    return;
  }

  std::move(request->callback).Run(/*success=*/false, error_message);

  pending_requests_.erase(request);
}

}  // namespace shared_storage_worklet
