// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/ml/Model.h"
#include "modules/ml/Compilation.h"

namespace blink {

Model::Model() {}

Model::~Model() {}

uint32_t Model::addOperand(const OperandOptions& options, ExceptionState& state) {
  return 0;
}

void Model::setOperandValue(uint32_t index, MaybeShared<DOMArrayBufferView> data, ExceptionState& state) {

}

void Model::addOperation(uint32_t type, Vector<uint32_t>& inputs, Vector<uint32_t>& outputs, ExceptionState& state) {

}

void Model::identifyInputsAndOutputs(Vector<uint32_t>& inputs, Vector<uint32_t>& outputs, ExceptionState& state) {

}

void Model::finish(ExceptionState& state) {

}

Compilation* Model::createCompilation() {
  return new Compilation;
}

void Model::Trace(blink::Visitor* visitor) {
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
