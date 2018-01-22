// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/ml/Model.h"
#include "bindings/modules/v8/array_buffer_view_or_double.h"

namespace blink {

Model::Model() {}

Model::~Model() {}

uint32_t Model::addOperand(const OperandOptions& options, ExceptionState& state) {
  // TODO: implement
  return 0;
}

void Model::setOperandValue(uint32_t index, const ArrayBufferViewOrDouble& data, ExceptionState& state) {
  // TODO: implement
}

void Model::addOperation(uint32_t type, Vector<uint32_t>& inputs, Vector<uint32_t>& outputs, ExceptionState& state) {
  // TODO: implement
}

void Model::identifyInputsAndOutputs(Vector<uint32_t>& inputs, Vector<uint32_t>& outputs, ExceptionState& state) {
  // TODO: implement
}

void Model::finish(ExceptionState& state) {
  // TODO: implement
}

ml::mojom::blink::ModelPtr Model::GetModelStruct() {
  ml::mojom::blink::ModelPtr model_struct = ml::mojom::blink::Model::New();
  // TODO: implement
  model_struct->buffer = mojo::SharedBufferHandle::Create(4);
  return model_struct;
}

void Model::Trace(blink::Visitor* visitor) {
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
