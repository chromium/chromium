// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/ExceptionMessages.h"
#include "bindings/core/v8/ExceptionState.h"
#include "modules/ml/Model.h"
#include "modules/ml/NeuralNetworkContext.h"

namespace blink {

Model::Model() : is_finished_(false) {
  mojo_model_ = ml::mojom::blink::Model::New();
}

Model::~Model() {}

uint32_t Model::addOperand(const OperandOptions& options,
                           ExceptionState& exception_state) {
  if (is_finished_) {
    exception_state.ThrowDOMException(kInvalidStateError,
                                      "Model has been finished.");
  }

  // TODO: validate the options

  ml::mojom::blink::OperandPtr operand =
      ml::mojom::blink::Operand::New();
  
  operand->type = options.type();
  if (options.hasDimensions()) {
    operand->dimensions = options.dimensions();
  }
  if (options.hasScale()) {
    operand->scale = options.scale();
  }
  if (options.hasZeroPoint()) {
    operand->zeroPoint = options.zeroPoint();
  }

  operand->bufferInfo =
      ml::mojom::blink::BufferInfo::New(0, 0);

  uint32_t index = mojo_model_->operands.size();
  mojo_model_->operands.push_back(std::move(operand));

  return index;
}

void Model::setOperandValue(uint32_t index,
                            MaybeShared<DOMArrayBufferView> data,
                            ExceptionState& exception_state) {
  if (is_finished_) {
    exception_state.ThrowDOMException(kInvalidStateError,
                                      "Model has been finished.");
  }

  if (index > mojo_model_->operands.size()) {
    exception_state.ThrowDOMException(kInvalidStateError,
                                      "Index is invalid.");
  }

  const ml::mojom::blink::OperandPtr& operand =
      mojo_model_->operands[index];

  WTF::ArrayBufferView::ViewType view_type = data.View()->GetType();
  if (view_type == WTF::ArrayBufferView::kTypeFloat32 &&
      !(operand->type == NeuralNetworkContext::kFloat32 ||
        operand->type == NeuralNetworkContext::kTensorFloat32)) {
    exception_state.ThrowDOMException(kInvalidStateError,
                                      "Data type is invalid.");
  }

  if (view_type == WTF::ArrayBufferView::kTypeInt32 &&
      !(operand->type == NeuralNetworkContext::kInt32 ||
        operand->type == NeuralNetworkContext::kTensorInt32)) {
    exception_state.ThrowDOMException(kInvalidStateError,
                                      "Data type is invalid.");
  }

  if (view_type == WTF::ArrayBufferView::kTypeUint32 &&
      (operand->type != NeuralNetworkContext::kUint32)) {
    exception_state.ThrowDOMException(kInvalidStateError,
                                      "Data type is invalid.");
  }

  if (view_type == WTF::ArrayBufferView::kTypeUint8 &&
      (operand->type != NeuralNetworkContext::kTensorQuant8Asymm)) {
    exception_state.ThrowDOMException(kInvalidStateError,
                                      "Data type is invalid.");
  }

  buffer_view_indexes_.push_back(index);
  buffer_views_.push_back(data.View());
}

void Model::addOperation(int32_t type,
                         Vector<uint32_t>& inputs,
                         Vector<uint32_t>& outputs,
                         ExceptionState& exception_state) {
  if (is_finished_) {
    exception_state.ThrowDOMException(kInvalidStateError,
                                      "Model has been finished.");
  }
  for (size_t i = 0; i < inputs.size(); ++i) {
    if (inputs[i] > mojo_model_->operands.size()) {
      exception_state.ThrowDOMException(kInvalidStateError,
                                        "Inputs is invalid.");
    }
  }
  for (size_t i = 0; i < outputs.size(); ++i) {
    if (outputs[i] > mojo_model_->operands.size()) {
      exception_state.ThrowDOMException(kInvalidStateError,
                                        "Outputs is invalid.");
    }
  }
  ml::mojom::blink::OperationPtr operation =
      ml::mojom::blink::Operation::New(type, inputs, outputs);
  mojo_model_->operations.push_back(std::move(operation));
}

void Model::identifyInputsAndOutputs(Vector<uint32_t>& inputs,
                                     Vector<uint32_t>& outputs,
                                     ExceptionState& exception_state) {
  if (is_finished_) {
    exception_state.ThrowDOMException(kInvalidStateError,
                                      "Model has been finished.");
  }
  for (size_t i = 0; i < inputs.size(); ++i) {
    if (inputs[i] > mojo_model_->operands.size()) {
      exception_state.ThrowDOMException(kInvalidStateError,
                                        "Inputs is invalid.");
    }
  }
  for (size_t i = 0; i < outputs.size(); ++i) {
    if (outputs[i] > mojo_model_->operands.size()) {
      exception_state.ThrowDOMException(kInvalidStateError,
                                        "Outputs is invalid.");
    }
  }
  mojo_model_->inputs = inputs;
  mojo_model_->outputs = outputs;
}

void Model::finish(ExceptionState& exception_state) {
  if (is_finished_) {
    exception_state.ThrowDOMException(kInvalidStateError,
                                      "Model has been finished.");
  }

  uint32_t total_byte_length = 0;
  for (size_t i = 0; i < buffer_view_indexes_.size(); ++i) {
    DOMArrayBufferView* view = buffer_views_[i];
    total_byte_length += view->byteLength();
  }

  mojo_model_->buffer = mojo::SharedBufferHandle::Create(total_byte_length);
  mojo::ScopedSharedBufferMapping mapping = mojo_model_->buffer->Map(total_byte_length);

  uint32_t offset = 0;
  for (size_t i = 0; i < buffer_view_indexes_.size(); ++i) {
    uint32_t index = buffer_view_indexes_[i];
    DOMArrayBufferView* view = buffer_views_[i];
    uint32_t length = view->byteLength();
    const ml::mojom::blink::OperandPtr& operand =
        mojo_model_->operands[index];
    operand->bufferInfo->offset = offset;
    operand->bufferInfo->length = length;
    uint8_t* base = static_cast<uint8_t*>(mapping.get()) + offset;
    memcpy(static_cast<void*>(base), view->BaseAddress(), length);
    offset += length;
  }

  is_finished_ = true;
}

void Model::Trace(blink::Visitor* visitor) {
  visitor->Trace(buffer_views_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
