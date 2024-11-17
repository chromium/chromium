// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_builder_test_utils.h"

#include <numeric>

#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_tester.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_context.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_operand_descriptor.h"
#include "third_party/blink/renderer/modules/ml/ml.h"
#include "third_party/blink/renderer/modules/ml/ml_context.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_builder.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_operand.h"

namespace blink {

MLOperand* BuildInput(ScriptState* script_state,
                      MLGraphBuilder* builder,
                      const String& name,
                      const Vector<uint32_t>& dimensions,
                      V8MLOperandDataType::Enum data_type,
                      ExceptionState& exception_state) {
  auto* desc = MLOperandDescriptor::Create();
  desc->setShape(dimensions);
  desc->setDataType(data_type);
  return builder->input(script_state, name, desc, exception_state);
}

NotShared<DOMArrayBufferView> CreateDOMArrayBufferView(
    size_t size,
    V8MLOperandDataType::Enum data_type) {
  NotShared<DOMArrayBufferView> buffer_view;
  switch (data_type) {
    case V8MLOperandDataType::Enum::kFloat32: {
      buffer_view = NotShared<DOMArrayBufferView>(
          blink::DOMFloat32Array::CreateOrNull(size));
      break;
    }
    case V8MLOperandDataType::Enum::kFloat16: {
      // Using Uint16Array for float16 is a workaround of WebNN spec issue:
      // https://github.com/webmachinelearning/webnn/issues/127
      buffer_view = NotShared<DOMArrayBufferView>(
          blink::DOMUint16Array::CreateOrNull(size));
      break;
    }
    case V8MLOperandDataType::Enum::kInt32: {
      buffer_view = NotShared<DOMArrayBufferView>(
          blink::DOMInt32Array::CreateOrNull(size));
      break;
    }
    case V8MLOperandDataType::Enum::kUint32: {
      buffer_view = NotShared<DOMArrayBufferView>(
          blink::DOMUint32Array::CreateOrNull(size));
      break;
    }
    case V8MLOperandDataType::Enum::kInt64: {
      buffer_view = NotShared<DOMArrayBufferView>(
          blink::DOMBigInt64Array::CreateOrNull(size));
      break;
    }
    case V8MLOperandDataType::Enum::kUint64: {
      buffer_view = NotShared<DOMArrayBufferView>(
          blink::DOMBigUint64Array::CreateOrNull(size));
      break;
    }
    case V8MLOperandDataType::Enum::kInt8: {
      buffer_view = NotShared<DOMArrayBufferView>(
          blink::DOMInt8Array::CreateOrNull(size));
      break;
    }
    case V8MLOperandDataType::Enum::kUint8: {
      buffer_view = NotShared<DOMArrayBufferView>(
          blink::DOMUint8Array::CreateOrNull(size));
      break;
    }
    // Using DOMUint8Array for int4/uint4 is a workaround since
    // TypedArray doesn't support int4/uint4.
    case V8MLOperandDataType::Enum::kInt4:
    case V8MLOperandDataType::Enum::kUint4: {
      buffer_view = NotShared<DOMArrayBufferView>(
          blink::DOMUint8Array::CreateOrNull(std::ceil(size / 2)));
      break;
    }
  }
  return buffer_view;
}

MLOperand* BuildConstant(
    ScriptState* script_state,
    MLGraphBuilder* builder,
    const Vector<uint32_t>& dimensions,
    V8MLOperandDataType::Enum data_type,
    ExceptionState& exception_state,
    std::optional<NotShared<DOMArrayBufferView>> user_buffer_view) {
  auto* desc = MLOperandDescriptor::Create();
  desc->setShape(dimensions);
  desc->setDataType(data_type);
  size_t size = std::accumulate(dimensions.begin(), dimensions.end(), size_t(1),
                                std::multiplies<uint32_t>());

  NotShared<DOMArrayBufferView> buffer_view =
      user_buffer_view ? std::move(user_buffer_view.value())
                       : CreateDOMArrayBufferView(size, data_type);
  if (buffer_view.Get() == nullptr) {
    return nullptr;
  }
  return builder->constant(script_state, desc, buffer_view, exception_state);
}

}  // namespace blink
