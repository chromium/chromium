// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ml/ml_model.h"

#include "base/numerics/safe_conversions.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_data_view.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_typed_array.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

namespace {

using ml::model_loader::mojom::blink::ComputeResult;
using ml::model_loader::mojom::blink::DataType;
using ml::model_loader::mojom::blink::Model;
using ml::model_loader::mojom::blink::ModelInfoPtr;

V8MLDataType::Enum ConvertMojoDataTypeToBlink(DataType tensor_type) {
  switch (tensor_type) {
    case DataType::kUnknown:
      return V8MLDataType::Enum::kUnknown;
    case DataType::kInt64:
      return V8MLDataType::Enum::kInt64;
    case DataType::kUint64:
      return V8MLDataType::Enum::kUint64;
    case DataType::kFloat64:
      return V8MLDataType::Enum::kFloat64;
    case DataType::kInt32:
      return V8MLDataType::Enum::kInt32;
    case DataType::kUint32:
      return V8MLDataType::Enum::kUint32;
    case DataType::kFloat32:
      return V8MLDataType::Enum::kFloat32;
    case DataType::kInt16:
      return V8MLDataType::Enum::kInt16;
    case DataType::kUint16:
      return V8MLDataType::Enum::kUint16;
    case DataType::kFloat16:
      return V8MLDataType::Enum::kFloat16;
    case DataType::kInt8:
      return V8MLDataType::Enum::kInt8;
    case DataType::kUint8:
      return V8MLDataType::Enum::kUint8;
    case DataType::kBool:
      return V8MLDataType::Enum::kBool;
  }
}

}  // namespace

MLModel::MLModel(ExecutionContext* context,
                 mojo::PendingRemote<Model> pending_remote,
                 ModelInfoPtr model_info)
    : remote_model_(context) {
  remote_model_.Bind(std::move(pending_remote),
                     context->GetTaskRunner(TaskType::kInternalDefault));

  // Stores the model info.
  input_tensor_name_to_info_ = std::move(model_info->input_tensor_info);
  output_tensor_name_to_info_ = std::move(model_info->output_tensor_info);
}

HeapVector<Member<MLTensorInfo>> MLModel::inputs(ScriptState* script_state) {
  HeapVector<Member<MLTensorInfo>> ret;
  for (const auto& name_info : input_tensor_name_to_info_) {
    auto* tensor_info = MLTensorInfo::Create();
    tensor_info->setName(name_info.key);
    tensor_info->setType(
        ConvertMojoDataTypeToBlink(name_info.value->data_type));
    tensor_info->setDimensions(name_info.value->dimensions);
    ret.push_back(tensor_info);
  }
  return ret;
}

HeapVector<Member<MLTensorInfo>> MLModel::outputs(ScriptState* script_state) {
  HeapVector<Member<MLTensorInfo>> ret;
  for (const auto& name_info : output_tensor_name_to_info_) {
    auto* tensor_info = MLTensorInfo::Create();
    tensor_info->setName(name_info.key);
    tensor_info->setType(
        ConvertMojoDataTypeToBlink(name_info.value->data_type));
    tensor_info->setDimensions(name_info.value->dimensions);
    ret.push_back(tensor_info);
  }
  return ret;
}

MLModel::~MLModel() = default;

ScriptPromise MLModel::compute(
    ScriptState* script_state,
    const HeapVector<std::pair<String, Member<MLTensor>>>& inputs,
    ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Invalid script state");
    return ScriptPromise();
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());
  ScriptPromise promise = resolver->Promise();

  // First verifies the sizes of inputs.
  if (input_tensor_name_to_info_.size() != inputs.size()) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kDataError,
        "The number of inputs doesn't match model's expectation."));
    return promise;
  }
  for (const auto& name_tensor : inputs) {
    auto iter = input_tensor_name_to_info_.find(name_tensor.first);
    if (iter == input_tensor_name_to_info_.end()) {
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kDataError,
          "There is unknown input: " + name_tensor.first));
      return promise;
    }
    if (iter->value->byte_size != name_tensor.second->data()->byteLength()) {
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kUnknownError, "Wrong input size."));
      return promise;
    }
  }
  // Fills the buffer with input tensors.
  HashMap<String, Vector<uint8_t>> input_mojo;

  for (const auto& name_tensor : inputs) {
    wtf_size_t size = base::checked_cast<wtf_size_t>(
        name_tensor.second->data()->byteLength());
    Vector<uint8_t> tensor(size);
    memcpy(tensor.data(), name_tensor.second->data()->BaseAddress(), size);

    input_mojo.insert(name_tensor.first, std::move(tensor));
  }

  remote_model_->Compute(
      std::move(input_mojo),
      WTF::BindOnce(&MLModel::OnComputeResult, WrapPersistent(this),
                    WrapPersistent(script_state), WrapPersistent(resolver)));

  return promise;
}

void MLModel::Trace(Visitor* visitor) const {
  visitor->Trace(remote_model_);

  ScriptWrappable::Trace(visitor);
}

void MLModel::OnComputeResult(
    ScriptState* script_state,
    ScriptPromiseResolver* resolver,
    ComputeResult result,
    const absl::optional<HashMap<String, Vector<uint8_t>>>& outputs) {
  if (result != ComputeResult::kOk || !outputs.has_value()) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kOperationError,
        "Failed to obtain the computation result."));
    return;
  }

  if (outputs.value().size() != output_tensor_name_to_info_.size()) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kUnknownError,
        "The number of output tensors of computation does't match the model's "
        "expectation."));
    return;
  }

  for (const auto& name_tensor : outputs.value()) {
    auto iter = output_tensor_name_to_info_.find(name_tensor.key);
    if (iter == output_tensor_name_to_info_.end()) {
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kUnknownError,
          "There is an unknown output tensor in the computation result: " +
              name_tensor.key));
      return;
    }
    if (name_tensor.value.size() != iter->value->byte_size) {
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kUnknownError,
          "The output tensor size does not match model's expectation: " +
              name_tensor.key));
      return;
    }
  }

  HeapVector<std::pair<String, Member<MLTensor>>> outputs_blink;
  for (const auto& name_output : outputs.value()) {
    // Already checked above that `iter` can not be `end()`.
    auto iter = output_tensor_name_to_info_.find(name_output.key);
    DCHECK(iter != output_tensor_name_to_info_.end());
    auto* blink_tensor = MLTensor::Create();

#define WEBML_SET_TYPED_OUTPUTS_BLINK(type_name)                              \
  {                                                                           \
    auto* typed_array = DOM##type_name##Array::Create(                        \
        name_output.value.size() / sizeof(DOM##type_name##Array::ValueType)); \
    /* Notice that we use the byte length of `typed_array` instead of the */  \
    /* length of `name_output.value` because in theory, the former can be */  \
    /* smaller (e.g. when `name_output.value.size() % type_size != 0.`*/      \
    memcpy(typed_array->Data(), name_output.value.data(),                     \
           typed_array->byteLength());                                        \
    blink_tensor->setData(NotShared<DOMArrayBufferView>(typed_array));        \
    blink_tensor->setDimensions(iter->value->dimensions);                     \
    outputs_blink.emplace_back(name_output.key, std::move(blink_tensor));     \
  }

    switch (iter->value->data_type) {
      case DataType::kInt64:
        WEBML_SET_TYPED_OUTPUTS_BLINK(BigInt64);
        break;
      case DataType::kUint64:
        WEBML_SET_TYPED_OUTPUTS_BLINK(BigUint64);
        break;
      case DataType::kFloat64:
        WEBML_SET_TYPED_OUTPUTS_BLINK(Float64);
        break;
      case DataType::kInt32:
        WEBML_SET_TYPED_OUTPUTS_BLINK(Int32);
        break;
      case DataType::kUint32:
        WEBML_SET_TYPED_OUTPUTS_BLINK(Uint32);
        break;
      case DataType::kFloat32:
        WEBML_SET_TYPED_OUTPUTS_BLINK(Float32);
        break;
      case DataType::kInt16:
        WEBML_SET_TYPED_OUTPUTS_BLINK(Int16);
        break;
      case DataType::kUint16:
        WEBML_SET_TYPED_OUTPUTS_BLINK(Uint16);
        break;
      case DataType::kInt8:
        WEBML_SET_TYPED_OUTPUTS_BLINK(Int8);
        break;
      case DataType::kUint8:
        WEBML_SET_TYPED_OUTPUTS_BLINK(Uint8);
        break;
      case DataType::kFloat16:
        // There is no DomTypedArray for float16. So we use ArrayBuffer.
      case DataType::kBool:
        // There is no DomTypedArray for bool. So we use ArrayBuffer.
      case DataType::kUnknown: {
        auto* dom_buffer = DOMArrayBuffer::Create(name_output.value.data(),
                                                  name_output.value.size());
        blink_tensor->setData(NotShared<DOMArrayBufferView>(
            DOMDataView::Create(dom_buffer, 0, name_output.value.size())));
        blink_tensor->setDimensions(iter->value->dimensions);
        outputs_blink.emplace_back(name_output.key, std::move(blink_tensor));
        break;
      }
    }
#undef WEBML_SET_TYPED_OUTPUTS_BLINK
  }

  resolver->Resolve(std::move(outputs_blink));
}

}  // namespace blink
