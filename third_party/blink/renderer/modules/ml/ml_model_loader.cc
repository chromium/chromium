// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ml/ml_model_loader.h"

#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_data_type.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/modules/ml/ml_context.h"
#include "third_party/blink/renderer/modules/ml/ml_model.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

namespace {

using ml::model_loader::mojom::blink::CreateModelLoaderOptions;
using ml::model_loader::mojom::blink::CreateModelLoaderResult;
using ml::model_loader::mojom::blink::DataType;
using ml::model_loader::mojom::blink::DevicePreference;
using ml::model_loader::mojom::blink::LoadModelResult;
using ml::model_loader::mojom::blink::Model;
using ml::model_loader::mojom::blink::ModelFormat;
using ml::model_loader::mojom::blink::ModelInfoPtr;
using ml::model_loader::mojom::blink::ModelLoader;

// Returns the type size of different `DataType`s used in storing them. This
// function is only used in `CheckIOTensorByteSize`. Such a check is needed
// because we are using `DOMTypedArray` to store them in returning the compute
// results, whose storage units are the type sizes. Notice that we let the size
// of kUnknown be 1 to make it always passes the check. This is correct because
// in such case we use `DOMArrayBuffer` for the output, not the `DOMTypedArray`.
// A special case is `kFloat16`. Because there is no native support of float16
// in Javascript, we still use `DOMArrayBuffer` to store the computation result.
// But because its size is explicit, we returns 2 here.
unsigned GetTypeSizeInStorage(DataType tensor_type) {
  switch (tensor_type) {
    case DataType::kInt64:
    case DataType::kUint64:
    case DataType::kFloat64:
      return 8;
    case DataType::kInt32:
    case DataType::kUint32:
    case DataType::kFloat32:
      return 4;
    case DataType::kInt16:
    case DataType::kUint16:
    case DataType::kFloat16:
      return 2;
    case DataType::kInt8:
    case DataType::kUint8:
    case DataType::kBool:
    case DataType::kUnknown:
      return 1;
  }
}

// Sanity check on whether the input/output tensor byte sizes can be divided by
// the size of the data type.
bool CheckIOTensorByteSize(const ModelInfoPtr& model_info) {
  for (const auto& name_tensor_info : model_info->input_tensor_info) {
    if (name_tensor_info.value->byte_size %
            GetTypeSizeInStorage(name_tensor_info.value->data_type) !=
        0)
      return false;
  }
  for (const auto& name_tensor_info : model_info->output_tensor_info) {
    if (name_tensor_info.value->byte_size %
            GetTypeSizeInStorage(name_tensor_info.value->data_type) !=
        0)
      return false;
  }
  return true;
}

void OnRemoteModelLoad(ExecutionContext* execution_context,
                       ScriptPromiseResolver* resolver,
                       LoadModelResult result,
                       mojo::PendingRemote<Model> pending_remote,
                       ModelInfoPtr model_info) {
  switch (result) {
    case LoadModelResult::kUnknownError:
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kUnknownError, "Unknown error."));
      return;
    case LoadModelResult::kInvalidModel:
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kDataError, "Invalid input model."));
      return;
    case LoadModelResult::kNotSupported:
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotSupportedError, "Model can not be supported."));
      return;
    case LoadModelResult::kOk:
      if (!CheckIOTensorByteSize(model_info)) {
        resolver->Reject(MakeGarbageCollected<DOMException>(
            DOMExceptionCode::kDataError,
            "Invalid IO tensor buffer byte size."));
        pending_remote.reset();
        return;
      }
      auto* model = MakeGarbageCollected<MLModel>(
          execution_context, std::move(pending_remote), std::move(model_info));
      resolver->Resolve(model);
      return;
  }
}

ModelFormat ConvertBlinkModelFormatToMojo(
    const V8MLModelFormat& model_format_blink) {
  // Uses `switch` because it can help detect whether the enum cases are all
  // considered.
  switch (model_format_blink.AsEnum()) {
    case V8MLModelFormat::Enum::kTflite:
      return ModelFormat::kTfLite;
  }
}

DevicePreference ConvertBlinkDevicePreferenceToMojo(
    const V8MLDevicePreference& device_preference_blink) {
  switch (device_preference_blink.AsEnum()) {
    case V8MLDevicePreference::Enum::kAuto:
      return DevicePreference::kAuto;
    case V8MLDevicePreference::Enum::kCpu:
      return DevicePreference::kCpu;
    case V8MLDevicePreference::Enum::kGpu:
      return DevicePreference::kGpu;
  }
}

}  // namespace

MLModelLoader::MLModelLoader(ExecutionContext* execution_context,
                             MLContext* ml_context)
    : ml_context_(ml_context), remote_loader_(execution_context) {}

// static
MLModelLoader* MLModelLoader::Create(ScriptState* script_state,
                                     MLContext* ml_context,
                                     ExceptionState& exception_state) {
  if (!script_state->ContextIsValid())
    return nullptr;

  // ml_context is a required input and can not be null.

  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  return MakeGarbageCollected<MLModelLoader>(execution_context, ml_context);
}

MLModelLoader::~MLModelLoader() = default;

ScriptPromise MLModelLoader::load(ScriptState* script_state,
                                  DOMArrayBuffer* buffer,
                                  ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Invalid script state");
    return ScriptPromise();
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());
  ScriptPromise promise = resolver->Promise();

  auto* execution_context = ExecutionContext::From(script_state);
  Load(script_state, buffer,
       WTF::BindOnce(&OnRemoteModelLoad, WrapPersistent(execution_context),
                     WrapPersistent(resolver)));

  return promise;
}

void MLModelLoader::Trace(Visitor* visitor) const {
  visitor->Trace(ml_context_);
  visitor->Trace(remote_loader_);

  ScriptWrappable::Trace(visitor);
}

void MLModelLoader::Load(ScriptState* script_state,
                         DOMArrayBuffer* buffer,
                         ModelLoadedCallback callback) {
  if (ml_context_->GetML() == nullptr) {
    std::move(callback).Run(LoadModelResult::kUnknownError, mojo::NullRemote(),
                            nullptr);
  } else if (buffer == nullptr) {
    std::move(callback).Run(LoadModelResult::kInvalidModel, mojo::NullRemote(),
                            nullptr);
  } else {
    if (!remote_loader_.is_bound()) {
      // Needs to bootstrap the mojo connection first.
      auto options_mojo = CreateModelLoaderOptions::New();

      options_mojo->num_threads = ml_context_->GetNumThreads();
      options_mojo->model_format =
          ConvertBlinkModelFormatToMojo(ml_context_->GetModelFormat());
      options_mojo->device_preference = ConvertBlinkDevicePreferenceToMojo(
          ml_context_->GetDevicePreference());

      ml_context_->GetML()->CreateModelLoader(
          script_state, std::move(options_mojo),
          WTF::BindOnce(&MLModelLoader::OnRemoteLoaderCreated,
                        WrapPersistent(this), WrapPersistent(script_state),
                        WrapPersistent(buffer), std::move(callback)));
    } else {
      // Directly use `remote_loader_`.
      remote_loader_->Load(
          base::make_span(static_cast<const uint8_t*>(buffer->Data()),
                          buffer->ByteLength()),
          std::move(callback));
    }
  }
}

void MLModelLoader::OnRemoteLoaderCreated(
    ScriptState* script_state,
    DOMArrayBuffer* buffer,
    ModelLoadedCallback callback,
    CreateModelLoaderResult result,
    mojo::PendingRemote<ModelLoader> pending_remote) {
  switch (result) {
    case CreateModelLoaderResult::kUnknownError: {
      std::move(callback).Run(LoadModelResult::kUnknownError,
                              mojo::NullRemote(), nullptr);
      return;
    }
    case CreateModelLoaderResult::kNotSupported: {
      std::move(callback).Run(LoadModelResult::kNotSupported,
                              mojo::NullRemote(), nullptr);
      return;
    }
    case CreateModelLoaderResult::kOk: {
      auto* execution_context = ExecutionContext::From(script_state);

      remote_loader_.Bind(
          std::move(pending_remote),
          execution_context->GetTaskRunner(TaskType::kInternalDefault));

      remote_loader_->Load(
          base::make_span(static_cast<const uint8_t*>(buffer->Data()),
                          buffer->ByteLength()),
          std::move(callback));
      return;
    }
  }
}

}  // namespace blink
