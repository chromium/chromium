// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_builder.h"

#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_clamp_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_conv_2d_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_gemm_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_operand_descriptor.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_pool_2d_options.h"
#include "third_party/blink/renderer/modules/ml/ml_context.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_operand.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_operator.h"

namespace blink {

// static
MLGraphBuilder* MLGraphBuilder::Create(MLContext* context) {
  return MakeGarbageCollected<MLGraphBuilder>(context);
}

MLGraphBuilder::MLGraphBuilder(MLContext* context) : ml_context_(context) {}

MLGraphBuilder::~MLGraphBuilder() = default;

void MLGraphBuilder::Trace(Visitor* visitor) const {
  visitor->Trace(ml_context_);
  ScriptWrappable::Trace(visitor);
}

MLOperand* MLGraphBuilder::input(String name, const MLOperandDescriptor* desc) {
  // TODO(crbug.com/1273291): Implement this on operating systems to access
  // hardware acceleration.
  NOTIMPLEMENTED();
  return MakeGarbageCollected<MLOperand>(this);
}

MLOperand* MLGraphBuilder::constant(const MLOperandDescriptor* desc,
                                    NotShared<DOMArrayBufferView> buffer_view) {
  // TODO(crbug.com/1273291): Implement this on operating systems to access
  // hardware acceleration.
  NOTIMPLEMENTED();
  return MakeGarbageCollected<MLOperand>(this);
}

MLOperand* MLGraphBuilder::clamp(const MLOperand* input,
                                 const MLClampOptions* options) {
  // TODO(crbug.com/1273291): Implement this on operating systems to access
  // hardware acceleration.
  NOTIMPLEMENTED();
  return MakeGarbageCollected<MLOperand>(this);
}

MLOperator* MLGraphBuilder::clamp(const MLClampOptions* options) {
  // TODO(crbug.com/1273291): Implement this on operating systems to access
  // hardware acceleration.
  NOTIMPLEMENTED();
  return MakeGarbageCollected<MLOperator>(this);
}

MLOperand* MLGraphBuilder::conv2d(const MLOperand* input,
                                  const MLOperand* filter,
                                  const MLConv2dOptions* options) {
  // TODO(crbug.com/1273291): Implement this on operating systems to access
  // hardware acceleration.
  NOTIMPLEMENTED();
  return MakeGarbageCollected<MLOperand>(this);
}

MLOperand* MLGraphBuilder::add(const MLOperand* a, const MLOperand* b) {
  // TODO(crbug.com/1273291): Implement this on operating systems to access
  // hardware acceleration.
  NOTIMPLEMENTED();
  return MakeGarbageCollected<MLOperand>(this);
}

MLOperand* MLGraphBuilder::gemm(const MLOperand* a,
                                const MLOperand* b,
                                const MLGemmOptions* options) {
  // TODO(crbug.com/1273291): Implement this on operating systems to access
  // hardware acceleration.
  NOTIMPLEMENTED();
  return MakeGarbageCollected<MLOperand>(this);
}

MLOperand* MLGraphBuilder::averagePool2d(const MLOperand* input,
                                         const MLPool2dOptions* options) {
  // TODO(crbug.com/1273291): Implement this on operating systems to access
  // hardware acceleration.
  NOTIMPLEMENTED();
  return MakeGarbageCollected<MLOperand>(this);
}

MLOperand* MLGraphBuilder::reshape(const MLOperand* input,
                                   const Vector<int32_t>& new_shape) {
  // TODO(crbug.com/1273291): Implement this on operating systems to access
  // hardware acceleration.
  NOTIMPLEMENTED();
  return MakeGarbageCollected<MLOperand>(this);
}

MLOperand* MLGraphBuilder::softmax(const MLOperand* input) {
  // TODO(crbug.com/1273291): Implement this on operating systems to access
  // hardware acceleration.
  NOTIMPLEMENTED();
  return MakeGarbageCollected<MLOperand>(this);
}

}  // namespace blink
