// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_builder_utils.h"

#include <numeric>

#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_operand_descriptor.h"
#include "third_party/blink/renderer/modules/ml/ml.h"
#include "third_party/blink/renderer/modules/ml/ml_context.h"

namespace blink {

MLGraphBuilder* CreateMLGraphBuilder(ExecutionContext* execution_context,
                                     ScriptState* script_state,
                                     ExceptionState& exception_state,
                                     MLContextOptions* options) {
  auto* ml = MakeGarbageCollected<ML>(execution_context);
  MLContext* ml_context =
      ml->createContextSync(script_state, options, exception_state);
  // createContextSync fails to create due to validation or invalid script
  // state.
  CHECK(ml_context);
  return MLGraphBuilder::Create(ml_context);
}

MLOperand* BuildInput(MLGraphBuilder* builder,
                      const String& name,
                      const Vector<uint32_t>& dimensions,
                      V8MLOperandType::Enum type,
                      ExceptionState& exception_state) {
  auto* desc = MLOperandDescriptor::Create();
  desc->setDimensions(dimensions);
  desc->setType(type);
  return builder->input(name, desc, exception_state);
}

NotShared<DOMArrayBufferView> CreateDOMArrayBufferView(
    size_t size,
    V8MLOperandType::Enum type) {
  NotShared<DOMArrayBufferView> buffer_view;
  switch (type) {
    case V8MLOperandType::Enum::kFloat32: {
      buffer_view = NotShared<DOMArrayBufferView>(
          blink::DOMFloat32Array::CreateOrNull(size));
      break;
    }
    case V8MLOperandType::Enum::kFloat16: {
      // Using Uint16Array for float16 is a workaround of WebNN spec issue:
      // https://github.com/webmachinelearning/webnn/issues/127
      buffer_view = NotShared<DOMArrayBufferView>(
          blink::DOMUint16Array::CreateOrNull(size));
      break;
    }
    case V8MLOperandType::Enum::kInt32: {
      buffer_view = NotShared<DOMArrayBufferView>(
          blink::DOMInt32Array::CreateOrNull(size));
      break;
    }
    case V8MLOperandType::Enum::kUint32: {
      buffer_view = NotShared<DOMArrayBufferView>(
          blink::DOMUint32Array::CreateOrNull(size));
      break;
    }
    case V8MLOperandType::Enum::kInt8: {
      buffer_view = NotShared<DOMArrayBufferView>(
          blink::DOMInt8Array::CreateOrNull(size));
      break;
    }
    case V8MLOperandType::Enum::kUint8: {
      buffer_view = NotShared<DOMArrayBufferView>(
          blink::DOMUint8Array::CreateOrNull(size));
      break;
    }
  }
  return buffer_view;
}

MLOperand* BuildConstant(
    MLGraphBuilder* builder,
    const Vector<uint32_t>& dimensions,
    V8MLOperandType::Enum type,
    ExceptionState& exception_state,
    absl::optional<NotShared<DOMArrayBufferView>> user_buffer_view) {
  auto* desc = MLOperandDescriptor::Create();
  desc->setDimensions(dimensions);
  desc->setType(type);
  size_t size = std::accumulate(dimensions.begin(), dimensions.end(), size_t(1),
                                std::multiplies<uint32_t>());

  NotShared<DOMArrayBufferView> buffer_view =
      user_buffer_view ? std::move(user_buffer_view.value())
                       : CreateDOMArrayBufferView(size, type);
  if (buffer_view.Get() == nullptr) {
    return nullptr;
  }
  return builder->constant(desc, buffer_view, exception_state);
}

}  // namespace blink
