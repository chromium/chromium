// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "third_party/blink/renderer/modules/ml/model.h"

#include <utility>

#include "mojo/public/cpp/bindings/interface_ptr.h"
#include "services/ml/public/interfaces/constants.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/modules/ml/compilation.h"
#include "third_party/blink/renderer/modules/ml/neural_network_context.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

namespace {
bool InvalidState(bool is_finished,
                  const String& message,
                  ExceptionState& exception_state) {
  String error_message = is_finished ? "Model is finished." : "";
  if (message.IsEmpty() && error_message.IsEmpty())
    return false;

  exception_state.ThrowDOMException(
      DOMExceptionCode::kInvalidStateError,
      error_message.IsEmpty() ? message : error_message);

  return true;
}

bool InValidParameters(bool is_finished,
                       const Vector<uint32_t>& inputs,
                       const Vector<uint32_t>& outputs,
                       size_t operands,
                       ExceptionState& exception_state) {
  if (InvalidState(is_finished, "", exception_state))
    return true;

  for (size_t i = 0; i < inputs.size(); ++i) {
    if (inputs[i] > operands)
      return InvalidState(false, "Inputs is invalid.", exception_state);
  }

  for (size_t i = 0; i < outputs.size(); ++i) {
    if (outputs[i] > operands)
      return InvalidState(false, "Outputs is invalid.", exception_state);
  }

  return false;
}

}  // namespace

Model::Model(ml::mojom::blink::ModelPtrInfo info) : is_finished_(false) {
  model_.Bind(std::move(info));
  model_.set_connection_error_handler(
      WTF::Bind(&Model::OnConnectionError, WrapWeakPersistent(this)));
  model_info_ = ml::mojom::blink::ModelInfo::New();
}

Model::~Model() = default;

void Model::addOperand(const OperandOptions& options,
                       ExceptionState& exception_state) {
  if (InvalidState(is_finished_,
                   !options.hasType() ? "Operand type is missing." : "",
                   exception_state))
    return;

  model_info_->operands.push_back(ml::mojom::blink::Operand::New(
      options.type(),
      options.hasDimensions() ? options.dimensions() : WTF::Vector<uint32_t>(),
      options.hasScale() ? options.scale() : 0,
      options.hasZeroPoint() ? options.zeroPoint() : 0));
}

void Model::setOperandValue(uint32_t index,
                            MaybeShared<DOMArrayBufferView> data,
                            ExceptionState& exception_state) {
  if (InvalidState(
          is_finished_,
          index >= model_info_->operands.size() ? "Index is invalid." : "",
          exception_state))
    return;

  const ml::mojom::blink::OperandPtr& operand =
      model_info_->operands[index];

  WTF::ArrayBufferView::ViewType view_type = data.View()->GetType();
  if (view_type == WTF::ArrayBufferView::kTypeFloat32 &&
      !(operand->type == NeuralNetworkContext::kFloat32 ||
        operand->type == NeuralNetworkContext::kTensorFloat32)) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Data type is invalid.");
    return;
  }

  if (view_type == WTF::ArrayBufferView::kTypeInt32 &&
      !(operand->type == NeuralNetworkContext::kInt32 ||
        operand->type == NeuralNetworkContext::kTensorInt32)) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Data type is invalid.");
    return;
  }

  if (view_type == WTF::ArrayBufferView::kTypeUint32 &&
      (operand->type != NeuralNetworkContext::kUint32)) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Data type is invalid.");
    return;
  }

  if (view_type == WTF::ArrayBufferView::kTypeUint8 &&
      (operand->type != NeuralNetworkContext::kTensorQuant8Asymm)) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Data type is invalid.");
    return;
  }

  model_info_->values.push_back(
      ml::mojom::blink::OperandValueInfo::New(index, 0, 0));
  buffer_views_.push_back(data.View());
}

void Model::addOperation(int32_t type,
                         Vector<uint32_t>& inputs,
                         Vector<uint32_t>& outputs,
                         ExceptionState& exception_state) {
  if (InValidParameters(is_finished_, inputs, outputs,
                        model_info_->operands.size(), exception_state))
    return;

  model_info_->operations.push_back(
      ml::mojom::blink::Operation::New(type, inputs, outputs));
}

void Model::identifyInputsAndOutputs(Vector<uint32_t>& inputs,
                                     Vector<uint32_t>& outputs,
                                     ExceptionState& exception_state) {
  if (InValidParameters(is_finished_, inputs, outputs,
                        model_info_->operands.size(), exception_state))
    return;

  model_info_->inputs = inputs;
  model_info_->outputs = outputs;
}

ScriptPromise Model::finish(ScriptState* script_state) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();
  if (is_finished_) {
    resolver->Reject(DOMException::Create(DOMExceptionCode::kNotSupportedError,
                                          "Model is finished."));
    return promise;
  }
  if (!model_) {
    resolver->Reject(DOMException::Create(DOMExceptionCode::kNotSupportedError,
                                          "Model service unavailable."));
    return promise;
  }

  requests_.insert(resolver);

  uint32_t total_byte_length = 0;
  for (size_t i = 0; i < model_info_->values.size(); ++i)
    total_byte_length += buffer_views_[i]->byteLength();

  mojo::ScopedSharedBufferHandle memory =
      mojo::SharedBufferHandle::Create(total_byte_length);
  mojo::ScopedSharedBufferMapping mapping = memory->Map(total_byte_length);

  uint32_t offset = 0;
  for (size_t i = 0; i < model_info_->values.size(); ++i) {
    const ml::mojom::blink::OperandValueInfoPtr& value_info =
        model_info_->values[i];
    DOMArrayBufferView* view = buffer_views_[i];
    uint32_t length = view->byteLength();
    value_info->offset = offset;
    value_info->length = length;
    uint8_t* base = static_cast<uint8_t*>(mapping.get()) + offset;
    memcpy(static_cast<void*>(base), view->BaseAddress(), length);
    offset += length;
  }

  model_info_->memory =
      memory->Clone(mojo::SharedBufferHandle::AccessMode::READ_ONLY);
  model_info_->memory_size = total_byte_length;
  model_->Finish(std::move(model_info_),
                 WTF::Bind(&Model::OnResultCode, WrapPersistent(this),
                           WrapPersistent(resolver), String("finish")));
  is_finished_ = true;
  return promise;
}

ScriptPromise Model::createCompilation(ScriptState* script_state) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();
  if (!is_finished_) {
    resolver->Reject(DOMException::Create(DOMExceptionCode::kNotSupportedError,
                                          "Model is not finished."));
    return promise;
  }
  if (!model_) {
    resolver->Reject(DOMException::Create(DOMExceptionCode::kNotSupportedError,
                                          "Model service unavailable."));
    return promise;
  }
  requests_.insert(resolver);

  model_->CreateCompilation(WTF::Bind(&Model::OnCreateCompilation,
                                      WrapPersistent(this),
                                      WrapPersistent(resolver)));
  return promise;
}

void Model::OnCreateCompilation(
    ScriptPromiseResolver* resolver, int32_t result_code,
    ml::mojom::blink::CompilationInitParamsPtr init_params) {
  DCHECK(requests_.Contains(resolver));
  requests_.erase(resolver);

  if (result_code == ml::mojom::blink::NOT_ERROR) {
    resolver->Resolve(new Compilation(std::move(init_params->compilation)));
  } else {
    resolver->Reject(DOMException::Create(
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
    resolver->Reject(
        DOMException::Create(DOMExceptionCode::kInvalidStateError,
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
    request->Reject(DOMException::Create(DOMExceptionCode::kNotSupportedError,
                                         "Model is not implemented."));
  }
  requests_.clear();
  model_.reset();
}

}  // namespace blink
