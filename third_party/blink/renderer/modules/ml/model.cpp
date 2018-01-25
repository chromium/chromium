// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/ExceptionMessages.h"
#include "bindings/core/v8/ExceptionState.h"
#include "modules/ml/Model.h"
#include "modules/ml/NeuralNetworkContext.h"

namespace blink {

Model::Model() : is_finished_(false) {}

Model::~Model() {}

uint32_t Model::addOperand(const OperandOptions& options,
                           ExceptionState& exception_state) {
  if (is_finished_) {
    exception_state.ThrowDOMException(kInvalidStateError,
                                      "Model has been finished.");
  }

  // TODO: validate the options

  Operand operand;
  operand.type = options.type();
  if (options.hasDimensions()) {
    operand.dimensions = options.dimensions();
  }
  if (options.hasScale()) {
    operand.scale = options.scale();
  }
  if (options.hasZeroPoint()) {
    operand.zeroPoint = options.zeroPoint();
  }
  uint32_t index = operands_.size();
  operands_.push_back(operand);
  return index;
}

void Model::setOperandValue(uint32_t index,
                            MaybeShared<DOMArrayBufferView> data,
                            ExceptionState& exception_state) {
  if (is_finished_) {
    exception_state.ThrowDOMException(kInvalidStateError,
                                      "Model has been finished.");
  }

  if (index > operands_.size()) {
    exception_state.ThrowDOMException(kInvalidStateError,
                                      "Index is invalid.");
  }

  const Operand& operand = operands_[index];

  WTF::ArrayBufferView::ViewType view_type = data.View()->GetType();
  if (view_type == WTF::ArrayBufferView::kTypeFloat32 &&
      !(operand.type == NeuralNetworkContext::kFloat32 ||
        operand.type == NeuralNetworkContext::kTensorFloat32)) {
    exception_state.ThrowDOMException(kInvalidStateError,
                                      "Data type is invalid.");
  }

  if (view_type == WTF::ArrayBufferView::kTypeInt32 &&
      !(operand.type == NeuralNetworkContext::kInt32 ||
        operand.type == NeuralNetworkContext::kTensorInt32)) {
    exception_state.ThrowDOMException(kInvalidStateError,
                                      "Data type is invalid.");
  }

  if (view_type == WTF::ArrayBufferView::kTypeUint32 &&
      (operand.type != NeuralNetworkContext::kUint32)) {
    exception_state.ThrowDOMException(kInvalidStateError,
                                      "Data type is invalid.");
  }

  if (view_type == WTF::ArrayBufferView::kTypeUint8 &&
      (operand.type != NeuralNetworkContext::kTensorQuant8Asymm)) {
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
    if (inputs[i] > operands_.size()) {
      exception_state.ThrowDOMException(kInvalidStateError,
                                        "Inputs is invalid.");
    }
  }
  for (size_t i = 0; i < outputs.size(); ++i) {
    if (outputs[i] > operands_.size()) {
      exception_state.ThrowDOMException(kInvalidStateError,
                                        "Outputs is invalid.");
    }
  }
  Operation operation;
  operation.type = type;
  operation.inputs = inputs;
  operation.outputs = outputs;

  operations_.push_back(operation);
}

void Model::identifyInputsAndOutputs(Vector<uint32_t>& inputs,
                                     Vector<uint32_t>& outputs,
                                     ExceptionState& exception_state) {
  if (is_finished_) {
    exception_state.ThrowDOMException(kInvalidStateError,
                                      "Model has been finished.");
  }
  for (size_t i = 0; i < inputs.size(); ++i) {
    if (inputs[i] > operands_.size()) {
      exception_state.ThrowDOMException(kInvalidStateError,
                                        "Inputs is invalid.");
    }
  }
  for (size_t i = 0; i < outputs.size(); ++i) {
    if (outputs[i] > operands_.size()) {
      exception_state.ThrowDOMException(kInvalidStateError,
                                        "Outputs is invalid.");
    }
  }
  inputs_ = inputs;
  outputs_ = outputs;
}

void Model::finish(ExceptionState& exception_state) {
  if (is_finished_) {
    exception_state.ThrowDOMException(kInvalidStateError,
                                      "Model has been finished.");
  }

  is_finished_ = true;
}

void Model::Trace(blink::Visitor* visitor) {
  visitor->Trace(buffer_views_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
