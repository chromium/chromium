// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "third_party/blink/renderer/modules/ml/model.h"

#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"

#include "third_party/blink/renderer/modules/ml/compilation.h"
#include "third_party/blink/renderer/modules/ml/neural_network_context.h"

namespace blink {

Model::Model(ml::mojom::blink::ModelPtrInfo info) : is_finished_(false) {
  model_.Bind(std::move(info));
  model_.set_connection_error_handler(
      WTF::Bind(&Model::OnConnectionError, WrapWeakPersistent(this)));
  model_info_ = ml::mojom::blink::ModelInfo::New();
}

Model::~Model() {}

void Model::addOperand(const OperandOptions& options, ExceptionState& exception_state) {
  if (is_finished_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Model is finished.");
    return;
  }
  if (!options.hasType()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Operand type is missing.");
    return;
  }
  int32_t type = options.type();

  WTF::Vector<uint32_t> dimensions;
  float scale = 0;
  int32_t zeroPoint = 0;

  if (options.hasDimensions()) {
    dimensions = options.dimensions();
  }

  if (options.hasScale()) {
    scale = options.scale();
  }

  if (options.hasZeroPoint()) {
    zeroPoint = options.zeroPoint();
  }

  model_info_->operands.push_back(
      ml::mojom::blink::Operand::New(type, dimensions, scale, zeroPoint));
}

void Model::setOperandValue(uint32_t index,
                            MaybeShared<DOMArrayBufferView> data,
                            ExceptionState& exception_state) {
  if (is_finished_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Model is finished.");
    return;
  }

  if (index >= model_info_->operands.size()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Index is invalid.");
    return;
  }

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

  model_info_->values.push_back(ml::mojom::blink::OperandValueInfo::New(index, 0, 0));
  buffer_views_.push_back(data.View());
}

void Model::addOperation(int32_t type,
                         Vector<uint32_t>& inputs,
                         Vector<uint32_t>& outputs,
                         ExceptionState& exception_state) {
  if (is_finished_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Model is finished.");
    return;
  }
  for (size_t i = 0; i < inputs.size(); ++i) {
    if (inputs[i] > model_info_->operands.size()) {
      exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                        "Inputs is invalid.");
      return;
    }
  }
  for (size_t i = 0; i < outputs.size(); ++i) {
    if (outputs[i] > model_info_->operands.size()) {
      exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                        "Outputs is invalid.");
      return;
    }
  }
  model_info_->operations.push_back(
      ml::mojom::blink::Operation::New(type, inputs, outputs));
}

void Model::identifyInputsAndOutputs(Vector<uint32_t>& inputs,
                                     Vector<uint32_t>& outputs,
                                     ExceptionState& exception_state) {
  if (is_finished_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Model is finished.");
    return;
  }
  for (size_t i = 0; i < inputs.size(); ++i) {
    if (inputs[i] > model_info_->operands.size()) {
      exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                        "Inputs is invalid.");
      return;
    }
  }
  for (size_t i = 0; i < outputs.size(); ++i) {
    if (outputs[i] > model_info_->operands.size()) {
      exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                        "Outputs is invalid.");
      return;
    }
  }
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
  for (size_t i = 0; i < model_info_->values.size(); ++i) {
    DOMArrayBufferView* view = buffer_views_[i];
    total_byte_length += view->byteLength();
  }

  mojo::ScopedSharedBufferHandle memory = mojo::SharedBufferHandle::Create(total_byte_length);
  mojo::ScopedSharedBufferMapping mapping = memory->Map(total_byte_length);

  uint32_t offset = 0;
  for (size_t i = 0; i < model_info_->values.size(); ++i) {
    const ml::mojom::blink::OperandValueInfoPtr& value_info =  model_info_->values[i];
    DOMArrayBufferView* view = buffer_views_[i];
    uint32_t length = view->byteLength();
    value_info->offset = offset;
    value_info->length = length;
    uint8_t* base = static_cast<uint8_t*>(mapping.get()) + offset;
    memcpy(static_cast<void*>(base), view->BaseAddress(), length);
    offset += length;
  }

  model_info_->memory = memory->Clone(mojo::SharedBufferHandle::AccessMode::READ_ONLY);
  model_info_->memory_size = total_byte_length;
  model_->finish(std::move(model_info_),
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

  model_->createCompilation(
      WTF::Bind(&Model::OnCreateCompilation,
                WrapPersistent(this),
                WrapPersistent(resolver)));
  return promise;
}

void Model::OnCreateCompilation(
    ScriptPromiseResolver* resolver, int32_t result_code,
    ml::mojom::blink::CompilationInitParamsPtr init_params) {
  DCHECK(requests_.Contains(resolver));
  requests_.erase(resolver);

  if (result_code == ml::mojom::blink::NO_ERROR) {
    resolver->Resolve(new Compilation(std::move(init_params->compilation)));
  } else {
    String msg("createCompilation fails: ");
    msg.append(String::Number(result_code));
    resolver->Reject(
        DOMException::Create(DOMExceptionCode::kInvalidStateError, msg));
  }
}

void Model::OnResultCode(ScriptPromiseResolver* resolver, const String& operation_name, int32_t result_code) {
  DCHECK(requests_.Contains(resolver));
  requests_.erase(resolver);

  if (result_code == ml::mojom::blink::NO_ERROR) {
    resolver->Resolve(result_code);
  } else {
    String msg(operation_name);
    msg.append("fails: ");
    msg.append(String::Number(result_code));
    resolver->Reject(
        DOMException::Create(DOMExceptionCode::kInvalidStateError, msg));
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
