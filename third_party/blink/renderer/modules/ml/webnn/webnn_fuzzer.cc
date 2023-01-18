// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <numeric>

#include "testing/libfuzzer/proto/lpm_interface.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_arraybufferallowshared_arraybufferviewallowshared.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_context_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_conv_2d_options.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/modules/ml/ml.h"
#include "third_party/blink/renderer/modules/ml/ml_context.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_builder.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_operand.h"
#include "third_party/blink/renderer/modules/ml/webnn/webnn.pb.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/testing/blink_fuzzer_test_support.h"

namespace blink {

namespace {

MLGraphBuilder* CreateMLGraphBuilder() {
  auto page_holder = std::make_unique<DummyPageHolder>();
  auto* ml = MakeGarbageCollected<ML>(
      page_holder->GetFrame().DomWindow()->GetExecutionContext());
  auto* options = MLContextOptions::Create();
  auto* context = MakeGarbageCollected<MLContext>(
      options->devicePreference(), options->powerPreference(),
      options->modelFormat(), options->numThreads(), ml);
  return MLGraphBuilder::Create(context);
}

V8MLOperandType::Enum ToV8MLOperandType(::webnn_proto::OperandType type) {
  switch (type) {
    case ::webnn_proto::OperandType::FLOAT32:
      return V8MLOperandType::Enum::kFloat32;
    case ::webnn_proto::OperandType::FLOAT16:
      return V8MLOperandType::Enum::kFloat16;
    case ::webnn_proto::OperandType::INT32:
      return V8MLOperandType::Enum::kInt32;
    case ::webnn_proto::OperandType::UINT32:
      return V8MLOperandType::Enum::kUint32;
    case ::webnn_proto::OperandType::INT8:
      return V8MLOperandType::Enum::kInt8;
    case ::webnn_proto::OperandType::UINT8:
      return V8MLOperandType::Enum::kUint8;
    default:
      NOTREACHED();
  }
}

V8MLAutoPad::Enum ToV8MLAutoPad(::webnn_proto::MLAutoPad autopad) {
  switch (autopad) {
    case ::webnn_proto::MLAutoPad::EXPLICIT:
      return V8MLAutoPad::Enum::kExplicit;
    case ::webnn_proto::MLAutoPad::SAME_UPPER:
      return V8MLAutoPad::Enum::kSameUpper;
    case ::webnn_proto::MLAutoPad::SAME_LOWER:
      return V8MLAutoPad::Enum::kSameLower;
    default:
      NOTREACHED();
  }
}

V8MLInputOperandLayout::Enum ToV8MLInputOperandLayout(
    ::webnn_proto::MLInputOperandLayout inputLayout) {
  switch (inputLayout) {
    case ::webnn_proto::MLInputOperandLayout::NCHW:
      return V8MLInputOperandLayout::Enum::kNchw;
    case ::webnn_proto::MLInputOperandLayout::NHWC:
      return V8MLInputOperandLayout::Enum::kNhwc;
    default:
      NOTREACHED();
  }
}

V8MLConv2dFilterOperandLayout::Enum ToV8MLFilterOperandLayout(
    ::webnn_proto::MLConv2dFilterOperandLayout filterLayout) {
  switch (filterLayout) {
    case ::webnn_proto::MLConv2dFilterOperandLayout::HWIO:
      return V8MLConv2dFilterOperandLayout::Enum::kHwio;
    case ::webnn_proto::MLConv2dFilterOperandLayout::IHWO:
      return V8MLConv2dFilterOperandLayout::Enum::kIhwo;
    case ::webnn_proto::MLConv2dFilterOperandLayout::OHWI:
      return V8MLConv2dFilterOperandLayout::Enum::kOhwi;
    case ::webnn_proto::MLConv2dFilterOperandLayout::OIHW:
      return V8MLConv2dFilterOperandLayout::Enum::kOihw;
    default:
      NOTREACHED();
  }
}

template <typename T>
Vector<T> ToVector(const ::google::protobuf::RepeatedField<T>& inputs) {
  Vector<T> elements;
  for (auto i : inputs) {
    elements.push_back(i);
  }
  return elements;
}

void ProtobufToConv2dOptions(const webnn_proto::conv2dOptions& data,
                             MLConv2dOptions* options) {
  if (data.padding_size() > 0) {
    options->setPadding(ToVector<uint32_t>(data.padding()));
  }

  if (data.strides_size() > 0) {
    options->setStrides(ToVector<uint32_t>(data.strides()));
  }

  if (data.dilations_size() > 0) {
    options->setDilations(ToVector<uint32_t>(data.dilations()));
  }

  if (data.has_autopad()) {
    options->setAutoPad(ToV8MLAutoPad(data.autopad()));
  }

  if (data.has_groups()) {
    options->setGroups(data.groups());
  }

  if (data.has_inputlayout()) {
    options->setInputLayout(ToV8MLInputOperandLayout(data.inputlayout()));
  }

  if (data.has_filterlayout()) {
    options->setFilterLayout(ToV8MLFilterOperandLayout(data.filterlayout()));
  }
}

NotShared<DOMArrayBufferView> CreateDOMArrayBufferView(
    size_t size,
    V8MLOperandType::Enum type) {
  NotShared<DOMArrayBufferView> buffer_view;
  switch (type) {
    case V8MLOperandType::Enum::kFloat32: {
      auto* float32_array = blink::DOMFloat32Array::CreateOrNull(size);
      buffer_view = NotShared<DOMArrayBufferView>(float32_array);
      break;
    }
    case V8MLOperandType::Enum::kInt32: {
      auto* int32_array = blink::DOMInt32Array::CreateOrNull(size);
      buffer_view = NotShared<DOMArrayBufferView>(int32_array);
      break;
    }
    // Using Uint16Array for float16 is a workaround of WebNN spec issue:
    // https://github.com/webmachinelearning/webnn/issues/127
    case V8MLOperandType::Enum::kFloat16: {
      auto* uint16_array = blink::DOMInt16Array::CreateOrNull(size);
      buffer_view = NotShared<DOMArrayBufferView>(uint16_array);
      break;
    }
    case V8MLOperandType::Enum::kInt8: {
      auto* int8_array = blink::DOMInt8Array::CreateOrNull(size);
      buffer_view = NotShared<DOMArrayBufferView>(int8_array);
      break;
    }
    case V8MLOperandType::Enum::kUint32: {
      auto* uint32_array = blink::DOMUint32Array::CreateOrNull(size);
      buffer_view = NotShared<DOMArrayBufferView>(uint32_array);
      break;
    }
    case V8MLOperandType::Enum::kUint8: {
      auto* uint8_array = blink::DOMUint8Array::CreateOrNull(size);
      buffer_view = NotShared<DOMArrayBufferView>(uint8_array);
      break;
    }
    default:
      NOTREACHED();
  }
  return buffer_view;
}
}  // namespace

DEFINE_PROTO_FUZZER(const webnn_proto::webnn& webnn) {
  static BlinkFuzzerTestSupport test_support = BlinkFuzzerTestSupport();
  DummyExceptionStateForTesting exception_state;

  auto* builder = CreateMLGraphBuilder();
  auto* operand_desc = MLOperandDescriptor::Create();
  operand_desc->setDimensions(ToVector<uint32_t>(webnn.input_dimensions()));
  operand_desc->setType(ToV8MLOperandType(webnn.input_type()));
  auto* input = builder->input("input", operand_desc, exception_state);

  Vector<uint32_t> filter_dimensions =
      ToVector<uint32_t>(webnn.filter_dimensions());
  V8MLOperandType::Enum filter_type = ToV8MLOperandType(webnn.filter_type());
  auto* filter_desc = MLOperandDescriptor::Create();
  filter_desc->setDimensions(filter_dimensions);
  filter_desc->setType(filter_type);
  size_t filter_size =
      std::accumulate(filter_dimensions.begin(), filter_dimensions.end(),
                      size_t(1), std::multiplies<uint32_t>());
  NotShared<DOMArrayBufferView> filter_buffer =
      CreateDOMArrayBufferView(filter_size, filter_type);
  if (filter_buffer.Get() == nullptr) {
    return;
  }
  MLOperand* filter =
      builder->constant(filter_desc, filter_buffer, exception_state);

  MLConv2dOptions* conv2d_options = blink::MLConv2dOptions::Create();
  if (webnn.has_conv2d_options()) {
    ProtobufToConv2dOptions(webnn.conv2d_options(), conv2d_options);
  }
  if (input == nullptr || filter == nullptr) {
    return;
  }
  builder->conv2d(input, filter, conv2d_options, exception_state);

  V8PerIsolateData::MainThreadIsolate()->RequestGarbageCollectionForTesting(
      v8::Isolate::kFullGarbageCollection);
}

}  // namespace blink
