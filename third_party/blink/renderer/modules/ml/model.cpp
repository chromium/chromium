// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "third_party/blink/renderer/modules/ml/model.h"

#include <utility>

#include "mojo/public/cpp/bindings/interface_ptr.h"
#include "services/ml/public/mojom/constants.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/modules/ml/compilation.h"
#include "third_party/blink/renderer/modules/ml/neural_network_context.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

namespace {

bool InvalidState(const String& message, ExceptionState& exception_state) {
  exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                    message);
  return true;
}

bool InvalidAction(bool is_finished, ExceptionState& exception_state) {
  return is_finished ? InvalidState("Model is finished", exception_state)
                     : false;
}

bool InvalidInputOutput(wtf_size_t operands,
                        const Vector<uint32_t>& inputs,
                        const Vector<uint32_t>& outputs,
                        ExceptionState& exception_state) {
  for (wtf_size_t i = 0; i < inputs.size(); ++i) {
    if (inputs[i] >= operands)
      return InvalidState("Inputs is invalid.", exception_state);
  }

  for (wtf_size_t i = 0; i < outputs.size(); ++i) {
    if (outputs[i] >= operands)
      return InvalidState("Outputs is invalid.", exception_state);
  }

  return false;
}

bool InvalidOperand(const OperandOptions* options,
                    ExceptionState& exception_state) {
  if (!options->hasType())
    return InvalidState("Data type is invalid.", exception_state);

  switch (options->type()) {
    case NeuralNetworkContext::kFloat32:
    case NeuralNetworkContext::kInt32:
    case NeuralNetworkContext::kUint32:
      if (options->hasDimensions())
        return InvalidState("Data type is invalid.", exception_state);
      break;
    case NeuralNetworkContext::kTensorFloat32:
    case NeuralNetworkContext::kTensorInt32:
      if (!options->hasDimensions())
        return InvalidState("Data type is invalid.", exception_state);
      break;
    case NeuralNetworkContext::kTensorQuant8Asymm:
      if (!options->hasDimensions() || !options->hasScale() ||
          !options->hasZeroPoint() || options->scale() < 0 ||
          options->zeroPoint() < 0 || options->zeroPoint() > 255)
        return InvalidState("Data type is invalid.", exception_state);
      break;
    default:
      NOTREACHED();
  }
  return false;
}

bool InvalidOperandValue(
    wtf_size_t index,
    const WTF::Vector<ml::mojom::blink::OperandPtr>& operands,
    const DOMArrayBufferView* data,
    ExceptionState& exception_state) {
  if (index >= operands.size())
    return InvalidState("Data type is invalid.", exception_state);

  int32_t operand_type = operands[index]->type;
  WTF::ArrayBufferView::ViewType data_type = data->GetType();

  bool invalid = false;
  switch (operand_type) {
    case NeuralNetworkContext::kFloat32:
      if (data->byteLength() / data->TypeSize() > 1)
        invalid = true;
      FALLTHROUGH;
    case NeuralNetworkContext::kTensorFloat32:
      if (data_type != WTF::ArrayBufferView::kTypeFloat32)
        invalid = true;
      break;
    case NeuralNetworkContext::kInt32:
      if (data->byteLength() / data->TypeSize() > 1)
        invalid = true;
      FALLTHROUGH;
    case NeuralNetworkContext::kTensorInt32:
      if (data_type != WTF::ArrayBufferView::kTypeInt32)
        invalid = true;
      break;
    case NeuralNetworkContext::kUint32:
      if (data_type != WTF::ArrayBufferView::kTypeUint32 ||
          data->byteLength() / data->TypeSize() > 1)
        invalid = true;
      break;
    case NeuralNetworkContext::kTensorQuant8Asymm:
      if (data_type != WTF::ArrayBufferView::kTypeUint8)
        invalid = true;
      break;
    default:
      NOTREACHED();
  }

  return invalid ? InvalidState("Data type is invalid.", exception_state)
                 : false;
}

}  // namespace

Model::Model(ml::mojom::blink::ModelPtrInfo info) : is_finished_(false) {
  model_.Bind(std::move(info));
  model_.set_connection_error_handler(
      WTF::Bind(&Model::OnConnectionError, WrapWeakPersistent(this)));
  model_info_ = ml::mojom::blink::ModelInfo::New();
}

Model::~Model() = default;

void Model::addOperand(const OperandOptions* options,
                       ExceptionState& exception_state) {
  if (InvalidAction(is_finished_, exception_state) ||
      InvalidOperand(options, exception_state))
    return;

  model_info_->operands.push_back(ml::mojom::blink::Operand::New(
      options->type(),
      options->hasDimensions() ? options->dimensions()
                               : WTF::Vector<uint32_t>(),
      options->hasScale() ? options->scale() : 0,
      options->hasZeroPoint() ? options->zeroPoint() : 0));
}

void Model::setOperandValue(uint32_t index,
                            MaybeShared<DOMArrayBufferView> data,
                            ExceptionState& exception_state) {
  if (InvalidAction(is_finished_, exception_state) ||
      InvalidOperandValue(index, model_info_->operands, data.View(),
                          exception_state))
    return;

  WTF::String index_str = WTF::String::Number(index);
  model_info_->values.insert(
      index_str, ml::mojom::blink::OperandValueInfo::New(index, 0, 0));
  buffer_views_.insert(index_str, data.View());
}

void Model::addOperation(int32_t type,
                         Vector<uint32_t>& inputs,
                         Vector<uint32_t>& outputs,
                         ExceptionState& exception_state) {
  if (InvalidAction(is_finished_, exception_state) ||
      InvalidInputOutput(model_info_->operands.size(), inputs, outputs,
                         exception_state))
    return;

  model_info_->operations.push_back(
      ml::mojom::blink::Operation::New(type, inputs, outputs));
}

void Model::identifyInputsAndOutputs(Vector<uint32_t>& inputs,
                                     Vector<uint32_t>& outputs,
                                     ExceptionState& exception_state) {
  if (InvalidAction(is_finished_, exception_state) ||
      InvalidInputOutput(model_info_->operands.size(), inputs, outputs,
                         exception_state))
    return;

  model_info_->inputs = inputs;
  model_info_->outputs = outputs;
}

ScriptPromise Model::finish(ScriptState* script_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();
  if (is_finished_) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNotSupportedError, "Model is finished."));
    return promise;
  }
  if (!model_) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNotSupportedError, "Model service unavailable."));
    return promise;
  }

  requests_.insert(resolver);

  uint32_t total_byte_length = 0;
  for (HeapHashMap<WTF::String, Member<DOMArrayBufferView>>::const_iterator
           itr = buffer_views_.begin();
       itr != buffer_views_.end(); ++itr)
    total_byte_length += itr->value->byteLength();

  memory_ = mojo::SharedBufferHandle::Create(total_byte_length);
  mojo::ScopedSharedBufferMapping mapping = memory_->Map(total_byte_length);

  uint32_t offset = 0;

  for (WTF::HashMap<WTF::String,
                    ml::mojom::blink::OperandValueInfoPtr>::const_iterator itr =
           model_info_->values.begin();
       itr != model_info_->values.end(); ++itr) {
    const ml::mojom::blink::OperandValueInfoPtr& value_info = itr->value;
    DOMArrayBufferView* view =
        buffer_views_.at(WTF::String::Number(value_info->index));
    uint32_t length = view->byteLength();
    value_info->offset = offset;
    value_info->length = length;
    uint8_t* base = static_cast<uint8_t*>(mapping.get()) + offset;
    memcpy(static_cast<void*>(base), view->BaseAddress(), length);
    offset += length;
  }

  model_info_->memory =
      memory_->Clone(mojo::SharedBufferHandle::AccessMode::READ_ONLY);
  model_info_->memory_size = total_byte_length;
  model_->Finish(std::move(model_info_),
                 WTF::Bind(&Model::OnResultCode, WrapPersistent(this),
                           WrapPersistent(resolver), String("finish")));
  is_finished_ = true;
  return promise;
}

ScriptPromise Model::createCompilation(ScriptState* script_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();
  if (!is_finished_) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNotSupportedError, "Model is not finished."));
    return promise;
  }
  if (!model_) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNotSupportedError, "Model service unavailable."));
    return promise;
  }
  requests_.insert(resolver);

  model_->CreateCompilation(WTF::Bind(&Model::OnCreateCompilation,
                                      WrapPersistent(this),
                                      WrapPersistent(resolver)));
  return promise;
}

void Model::OnCreateCompilation(
    ScriptPromiseResolver* resolver,
    int32_t result_code,
    ml::mojom::blink::CompilationInitParamsPtr init_params) {
  DCHECK(requests_.Contains(resolver));
  requests_.erase(resolver);

  if (result_code == ml::mojom::blink::NOT_ERROR) {
    resolver->Resolve(
        MakeGarbageCollected<Compilation>(std::move(init_params->compilation)));
  } else {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kInvalidStateError,
        "createCompilation fails: " + String::Number(result_code)));
  }
}

void Model::OnResultCode(ScriptPromiseResolver* resolver,
                         const String& operation_name,
                         int32_t result_code) {
  DCHECK(requests_.Contains(resolver));
  requests_.erase(resolver);

  if (result_code == ml::mojom::blink::NOT_ERROR) {
    resolver->Resolve(result_code);
  } else {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kInvalidStateError,
        "fails: " + String::Number(result_code)));
  }
}

void Model::Trace(blink::Visitor* visitor) {
  visitor->Trace(requests_);
  visitor->Trace(buffer_views_);
  ScriptWrappable::Trace(visitor);
}

void Model::OnConnectionError() {
  for (const auto& request : requests_) {
    request->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNotSupportedError, "Model is not implemented."));
  }
  requests_.clear();
  model_.reset();
}

}  // namespace blink
