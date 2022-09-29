// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_builder.h"

#include <algorithm>
#include <memory>
#include <numeric>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_context_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_conv_2d_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_gemm_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_operand_descriptor.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_pool_2d_options.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_typed_array.h"
#include "third_party/blink/renderer/modules/ml/ml.h"
#include "third_party/blink/renderer/modules/ml/ml_context.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_operand.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_operator.h"

namespace blink {

class MLGraphBuilderTest : public testing::Test {
 public:
  MLGraphBuilderTest() = default;
  ~MLGraphBuilderTest() override = default;
};

MLGraphBuilder* CreateMLGraphBuilder(V8TestingScope& scope) {
  auto* ml = MakeGarbageCollected<ML>(scope.GetExecutionContext());
  auto* options = MLContextOptions::Create();
  options->setDevicePreference(V8MLDevicePreference::Enum::kAuto);
  options->setPowerPreference(V8MLPowerPreference::Enum::kAuto);
  auto* context = MakeGarbageCollected<MLContext>(
      options->devicePreference(), options->powerPreference(),
      options->modelFormat(), options->numThreads(), ml);
  auto* builder = MLGraphBuilder::Create(context);
  EXPECT_NE(builder, nullptr);
  return builder;
}

MLOperand* BuildInput(V8TestingScope& scope,
                      MLGraphBuilder* builder,
                      const String& name,
                      const Vector<uint32_t>& dimensions,
                      V8MLOperandType::Enum type) {
  auto* desc = MLOperandDescriptor::Create();
  desc->setDimensions(dimensions);
  desc->setType(type);
  auto* input = builder->input(name, desc, scope.GetExceptionState());
  EXPECT_NE(input, nullptr);
  EXPECT_EQ(input->Kind(), MLOperand::OperandKind::kInput);
  EXPECT_EQ(input->Type(), type);
  EXPECT_EQ(input->Dimensions(), dimensions);
  EXPECT_EQ(input->Name(), name);
  return input;
}

NotShared<DOMArrayBufferView> CreateDOMArrayBufferView(
    size_t size,
    V8MLOperandType::Enum type) {
  NotShared<DOMArrayBufferView> buffer_view;
  switch (type) {
    case V8MLOperandType::Enum::kFloat32: {
      auto* float32_array = blink::DOMFloat32Array::Create(size);
      buffer_view = NotShared<DOMArrayBufferView>(float32_array);
      break;
    }
    case V8MLOperandType::Enum::kInt32: {
      auto* int32_array = blink::DOMInt32Array::Create(size);
      buffer_view = NotShared<DOMArrayBufferView>(int32_array);
      break;
    }
    default:
      NOTREACHED();
  }
  CHECK(buffer_view.Get());
  return buffer_view;
}

MLOperand* BuildConstant(V8TestingScope& scope,
                         MLGraphBuilder* builder,
                         const Vector<uint32_t>& dimensions,
                         V8MLOperandType::Enum type) {
  auto* desc = MLOperandDescriptor::Create();
  desc->setDimensions(dimensions);
  desc->setType(type);
  size_t size = std::accumulate(dimensions.begin(), dimensions.end(), size_t(1),
                                std::multiplies<uint32_t>());
  NotShared<DOMArrayBufferView> buffer_view =
      CreateDOMArrayBufferView(size, type);
  auto* constant =
      builder->constant(desc, buffer_view, scope.GetExceptionState());
  EXPECT_NE(constant, nullptr);
  EXPECT_EQ(constant->Kind(), MLOperand::OperandKind::kConstant);
  EXPECT_EQ(constant->Type(), type);
  EXPECT_EQ(constant->Dimensions(), dimensions);
  EXPECT_EQ(constant->ArrayBufferView(), buffer_view.Get());
  return constant;
}

TEST_F(MLGraphBuilderTest, InputTest) {
  V8TestingScope scope;
  MLGraphBuilder* builder = CreateMLGraphBuilder(scope);
  {
    // Test building a 2-D input without errors.
    auto* desc = MLOperandDescriptor::Create();
    desc->setDimensions({3, 4});
    desc->setType(V8MLOperandType::Enum::kFloat32);
    auto* input = builder->input("input", desc, scope.GetExceptionState());
    EXPECT_NE(input, nullptr);
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kNoError);
  }
  {
    // Test throwing exception if the name is empty.
    auto* desc = MLOperandDescriptor::Create();
    desc->setDimensions({3, 4});
    desc->setType(V8MLOperandType::Enum::kFloat32);
    auto* input = builder->input("", desc, scope.GetExceptionState());
    EXPECT_EQ(input, nullptr);
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(), "The name is empty.");
  }
  {
    // Test throwing exception if a dimension size is 0.
    auto* desc = MLOperandDescriptor::Create();
    desc->setDimensions({3, 0});
    desc->setType(V8MLOperandType::Enum::kFloat32);
    auto* input = builder->input("input", desc, scope.GetExceptionState());
    EXPECT_EQ(input, nullptr);
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "Invalid operand descriptor: All dimensions should be positive");
  }
  {
    // Test throwing exception if the dimensions is empty.
    auto* desc = MLOperandDescriptor::Create();
    desc->setDimensions({});
    desc->setType(V8MLOperandType::Enum::kFloat32);
    auto* input = builder->input("input", desc, scope.GetExceptionState());
    EXPECT_EQ(input, nullptr);
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "Invalid operand descriptor: The dimensions is empty.");
  }
  {
    // Test throwing exception if the dimensions is too large.
    auto* desc = MLOperandDescriptor::Create();
    desc->setDimensions({2147483600, 102834, 2347816});
    desc->setType(V8MLOperandType::Enum::kFloat32);
    auto* input = builder->input("input", desc, scope.GetExceptionState());
    EXPECT_EQ(input, nullptr);
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(
        scope.GetExceptionState().Message(),
        "Invalid operand descriptor: The elements number of the dimensions is "
        "too large.");
  }
}

TEST_F(MLGraphBuilderTest, ConstantTest) {
  V8TestingScope scope;
  MLGraphBuilder* builder = CreateMLGraphBuilder(scope);
  {
    // Test building a 2-D constant without errors.
    auto* desc = MLOperandDescriptor::Create();
    desc->setDimensions({2, 3});
    desc->setType(V8MLOperandType::Enum::kFloat32);
    NotShared<DOMArrayBufferView> buffer_view =
        CreateDOMArrayBufferView(6, V8MLOperandType::Enum::kFloat32);
    auto* constant =
        builder->constant(desc, buffer_view, scope.GetExceptionState());
    EXPECT_NE(constant, nullptr);
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kNoError);
  }
  {
    // Test throwing exception if a dimension is 0.
    auto* desc = MLOperandDescriptor::Create();
    desc->setDimensions({2, 0});
    desc->setType(V8MLOperandType::Enum::kFloat32);
    NotShared<DOMArrayBufferView> buffer_view =
        CreateDOMArrayBufferView(6, V8MLOperandType::Enum::kFloat32);
    auto* constant =
        builder->constant(desc, buffer_view, scope.GetExceptionState());
    EXPECT_EQ(constant, nullptr);
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "Invalid operand descriptor: All dimensions should be positive");
  }
  {
    // Test throwing exception if buffer view type doesn't match the operand
    // type.
    auto* desc = MLOperandDescriptor::Create();
    desc->setDimensions({2, 3});
    desc->setType(V8MLOperandType::Enum::kInt32);
    NotShared<DOMArrayBufferView> buffer_view =
        CreateDOMArrayBufferView(6, V8MLOperandType::Enum::kFloat32);
    auto* constant =
        builder->constant(desc, buffer_view, scope.GetExceptionState());
    EXPECT_EQ(constant, nullptr);
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The buffer view type doesn't match the operand type.");
  }
  {
    // Test throwing exception if buffer view size is not expected.
    auto* desc = MLOperandDescriptor::Create();
    desc->setDimensions({2, 2});
    desc->setType(V8MLOperandType::Enum::kInt32);
    NotShared<DOMArrayBufferView> buffer_view =
        CreateDOMArrayBufferView(8, V8MLOperandType::Enum::kInt32);
    auto* constant =
        builder->constant(desc, buffer_view, scope.GetExceptionState());
    EXPECT_EQ(constant, nullptr);
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The buffer view byte length (32) doesn't match the expected "
              "byte length (16).");
  }
}

MLOperand* BuildConv2d(
    V8TestingScope& scope,
    MLGraphBuilder* builder,
    const MLOperand* input,
    const MLOperand* filter,
    const MLConv2dOptions* options = MLConv2dOptions::Create()) {
  auto* output =
      builder->conv2d(input, filter, options, scope.GetExceptionState());
  EXPECT_NE(output, nullptr);
  EXPECT_EQ(output->Kind(), MLOperand::OperandKind::kOutput);
  EXPECT_EQ(output->Type(), input->Type());
  auto* conv2d = output->Operator();
  EXPECT_NE(conv2d, nullptr);
  EXPECT_EQ(conv2d->Kind(), MLOperator::OperatorKind::kConv2d);
  EXPECT_EQ(conv2d->IsConnected(), true);
  EXPECT_NE(conv2d->Options(), nullptr);
  return output;
}

TEST_F(MLGraphBuilderTest, Conv2dTest) {
  V8TestingScope scope;
  auto* builder = CreateMLGraphBuilder(scope);
  {
    // Test conv2d without padding.
    auto* input = BuildInput(scope, builder, "input", {1, 1, 5, 5},
                             V8MLOperandType::Enum::kFloat32);
    auto* filter = BuildConstant(scope, builder, {1, 1, 3, 3},
                                 V8MLOperandType::Enum::kFloat32);
    auto* output = BuildConv2d(scope, builder, input, filter);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 1, 3, 3}));
  }
  {
    // Test conv2d with padding=1.
    auto* input = BuildInput(scope, builder, "input", {1, 1, 5, 5},
                             V8MLOperandType::Enum::kFloat32);
    auto* filter = BuildConstant(scope, builder, {1, 1, 3, 3},
                                 V8MLOperandType::Enum::kFloat32);
    auto* options = MLConv2dOptions::Create();
    options->setPadding({1, 1, 1, 1});
    auto* output = BuildConv2d(scope, builder, input, filter, options);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 1, 5, 5}));
  }
  {
    // Test conv2d with autopad="same-lower".
    auto* input = BuildInput(scope, builder, "input", {1, 1, 5, 5},
                             V8MLOperandType::Enum::kFloat32);
    auto* filter = BuildConstant(scope, builder, {1, 1, 3, 3},
                                 V8MLOperandType::Enum::kFloat32);
    auto* options = MLConv2dOptions::Create();
    options->setAutoPad(V8MLAutoPad::Enum::kSameLower);
    auto* output = BuildConv2d(scope, builder, input, filter, options);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 1, 5, 5}));
  }
  {
    // Test conv2d with autopad="same-upper".
    auto* input = BuildInput(scope, builder, "input", {1, 1, 5, 5},
                             V8MLOperandType::Enum::kFloat32);
    auto* filter = BuildConstant(scope, builder, {1, 1, 3, 3},
                                 V8MLOperandType::Enum::kFloat32);
    auto* options = MLConv2dOptions::Create();
    options->setAutoPad(V8MLAutoPad::Enum::kSameUpper);
    auto* output = BuildConv2d(scope, builder, input, filter, options);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 1, 5, 5}));
  }
  {
    // Test conv2d with strides=2 and padding=1.
    auto* input = BuildInput(scope, builder, "input", {1, 1, 5, 5},
                             V8MLOperandType::Enum::kFloat32);
    auto* filter = BuildConstant(scope, builder, {1, 1, 3, 3},
                                 V8MLOperandType::Enum::kFloat32);
    auto* options = MLConv2dOptions::Create();
    options->setPadding({1, 1, 1, 1});
    options->setStrides({2, 2});
    auto* output = BuildConv2d(scope, builder, input, filter, options);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 1, 3, 3}));
  }
  {
    // Test conv2d with strides=2 and asymmetric padding.
    auto* input = BuildInput(scope, builder, "input", {1, 1, 5, 5},
                             V8MLOperandType::Enum::kFloat32);
    auto* filter = BuildConstant(scope, builder, {1, 1, 4, 2},
                                 V8MLOperandType::Enum::kFloat32);
    auto* options = MLConv2dOptions::Create();
    options->setPadding({1, 2, 0, 1});
    options->setStrides({2, 2});
    auto* output = BuildConv2d(scope, builder, input, filter, options);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 1, 3, 3}));
  }
  {
    // Test depthwise conv2d by setting groups to input channels.
    auto* input = BuildInput(scope, builder, "input", {1, 4, 2, 2},
                             V8MLOperandType::Enum::kFloat32);
    auto* filter = BuildConstant(scope, builder, {4, 1, 2, 2},
                                 V8MLOperandType::Enum::kFloat32);
    auto* options = MLConv2dOptions::Create();
    options->setGroups(4);
    auto* output = BuildConv2d(scope, builder, input, filter, options);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 4, 1, 1}));
  }
  {
    // Test depthwise conv2d with groups=4, inputLayout="nhwc" and
    // filterLayout="ihwo".
    auto* input = BuildInput(scope, builder, "input", {1, 2, 2, 4},
                             V8MLOperandType::Enum::kFloat32);
    auto* filter = BuildConstant(scope, builder, {1, 2, 2, 4},
                                 V8MLOperandType::Enum::kFloat32);
    auto* options = MLConv2dOptions::Create();
    options->setGroups(4);
    options->setInputLayout(V8MLInputOperandLayout::Enum::kNhwc);
    options->setFilterLayout(V8MLConv2dFilterOperandLayout::Enum::kIhwo);
    auto* output = BuildConv2d(scope, builder, input, filter, options);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 1, 1, 4}));
  }
  {
    // Test conv2d with dilations=4, inputLayout="nhwc" and
    // filterLayout="ihwo".
    auto* input = BuildInput(scope, builder, "input", {1, 65, 65, 1},
                             V8MLOperandType::Enum::kFloat32);
    auto* filter = BuildConstant(scope, builder, {1, 3, 3, 1},
                                 V8MLOperandType::Enum::kFloat32);
    auto* options = MLConv2dOptions::Create();
    options->setInputLayout(V8MLInputOperandLayout::Enum::kNhwc);
    options->setFilterLayout(V8MLConv2dFilterOperandLayout::Enum::kIhwo);
    options->setDilations({4, 4});
    auto* output = BuildConv2d(scope, builder, input, filter, options);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 57, 57, 1}));
  }
  {
    // Test conv2d with inputLayout="nchw" and filterLayout="oihw".
    auto* input = BuildInput(scope, builder, "input", {1, 2, 5, 5},
                             V8MLOperandType::Enum::kFloat32);
    auto* filter = BuildConstant(scope, builder, {1, 2, 3, 3},
                                 V8MLOperandType::Enum::kFloat32);
    auto* options = MLConv2dOptions::Create();
    options->setInputLayout(V8MLInputOperandLayout::Enum::kNchw);
    options->setFilterLayout(V8MLConv2dFilterOperandLayout::Enum::kOihw);
    auto* output = BuildConv2d(scope, builder, input, filter, options);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 1, 3, 3}));
  }
  {
    // Test conv2d with inputLayout="nchw" and filterLayout="hwio".
    auto* input = BuildInput(scope, builder, "input", {1, 2, 5, 5},
                             V8MLOperandType::Enum::kFloat32);
    auto* filter = BuildConstant(scope, builder, {3, 3, 2, 1},
                                 V8MLOperandType::Enum::kFloat32);
    auto* options = MLConv2dOptions::Create();
    options->setInputLayout(V8MLInputOperandLayout::Enum::kNchw);
    options->setFilterLayout(V8MLConv2dFilterOperandLayout::Enum::kHwio);
    auto* output = BuildConv2d(scope, builder, input, filter, options);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 1, 3, 3}));
  }
  {
    // Test conv2d with inputLayout="nchw" and filterLayout="ohwi".
    auto* input = BuildInput(scope, builder, "input", {1, 2, 5, 5},
                             V8MLOperandType::Enum::kFloat32);
    auto* filter = BuildConstant(scope, builder, {1, 3, 3, 2},
                                 V8MLOperandType::Enum::kFloat32);
    auto* options = MLConv2dOptions::Create();
    options->setInputLayout(V8MLInputOperandLayout::Enum::kNchw);
    options->setFilterLayout(V8MLConv2dFilterOperandLayout::Enum::kOhwi);
    auto* output = BuildConv2d(scope, builder, input, filter, options);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 1, 3, 3}));
  }
  {
    // Test conv2d with inputLayout="nchw" and filterLayout="ihwo".
    auto* input = BuildInput(scope, builder, "input", {1, 2, 5, 5},
                             V8MLOperandType::Enum::kFloat32);
    auto* filter = BuildConstant(scope, builder, {2, 3, 3, 1},
                                 V8MLOperandType::Enum::kFloat32);
    auto* options = MLConv2dOptions::Create();
    options->setInputLayout(V8MLInputOperandLayout::Enum::kNchw);
    options->setFilterLayout(V8MLConv2dFilterOperandLayout::Enum::kIhwo);
    auto* output = BuildConv2d(scope, builder, input, filter, options);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 1, 3, 3}));
  }
  {
    // Test conv2d with inputLayout="nhwc" and filterLayout="oihw".
    auto* input = BuildInput(scope, builder, "input", {1, 5, 5, 2},
                             V8MLOperandType::Enum::kFloat32);
    auto* filter = BuildConstant(scope, builder, {1, 2, 3, 3},
                                 V8MLOperandType::Enum::kFloat32);
    auto* options = MLConv2dOptions::Create();
    options->setInputLayout(V8MLInputOperandLayout::Enum::kNhwc);
    options->setFilterLayout(V8MLConv2dFilterOperandLayout::Enum::kOihw);
    auto* output = BuildConv2d(scope, builder, input, filter, options);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 3, 3, 1}));
  }
  {
    // Test conv2d with inputLayout="nhwc" and filterLayout="hwio".
    auto* input = BuildInput(scope, builder, "input", {1, 5, 5, 2},
                             V8MLOperandType::Enum::kFloat32);
    auto* filter = BuildConstant(scope, builder, {3, 3, 2, 1},
                                 V8MLOperandType::Enum::kFloat32);
    auto* options = MLConv2dOptions::Create();
    options->setInputLayout(V8MLInputOperandLayout::Enum::kNhwc);
    options->setFilterLayout(V8MLConv2dFilterOperandLayout::Enum::kHwio);
    auto* output = BuildConv2d(scope, builder, input, filter, options);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 3, 3, 1}));
  }
  {
    // Test conv2d with inputLayout="nhwc" and filterLayout="ohwi".
    auto* input = BuildInput(scope, builder, "input", {1, 5, 5, 2},
                             V8MLOperandType::Enum::kFloat32);
    auto* filter = BuildConstant(scope, builder, {1, 3, 3, 2},
                                 V8MLOperandType::Enum::kFloat32);
    auto* options = MLConv2dOptions::Create();
    options->setInputLayout(V8MLInputOperandLayout::Enum::kNhwc);
    options->setFilterLayout(V8MLConv2dFilterOperandLayout::Enum::kOhwi);
    auto* output = BuildConv2d(scope, builder, input, filter, options);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 3, 3, 1}));
  }
  {
    // Test conv2d with inputLayout="nhwc" and filterLayout="ihwo".
    auto* input = BuildInput(scope, builder, "input", {1, 5, 5, 2},
                             V8MLOperandType::Enum::kFloat32);
    auto* filter = BuildConstant(scope, builder, {2, 3, 3, 1},
                                 V8MLOperandType::Enum::kFloat32);
    auto* options = MLConv2dOptions::Create();
    options->setInputLayout(V8MLInputOperandLayout::Enum::kNhwc);
    options->setFilterLayout(V8MLConv2dFilterOperandLayout::Enum::kIhwo);
    auto* output = BuildConv2d(scope, builder, input, filter, options);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 3, 3, 1}));
  }
}

enum class Pool2dKind { kAverage, kMax };

MLOperand* BuildPool2d(
    V8TestingScope& scope,
    MLGraphBuilder* builder,
    Pool2dKind kind,
    const MLOperand* input,
    const MLPool2dOptions* options = MLPool2dOptions::Create()) {
  MLOperand* output = nullptr;
  switch (kind) {
    case Pool2dKind::kAverage:
      output =
          builder->averagePool2d(input, options, scope.GetExceptionState());
      break;
    case Pool2dKind::kMax:
      output = builder->maxPool2d(input, options, scope.GetExceptionState());
      break;
  }
  EXPECT_NE(output, nullptr);
  EXPECT_EQ(output->Kind(), MLOperand::OperandKind::kOutput);
  EXPECT_EQ(output->Type(), input->Type());
  auto* pool2d = output->Operator();
  EXPECT_NE(pool2d, nullptr);
  switch (kind) {
    case Pool2dKind::kAverage:
      EXPECT_EQ(pool2d->Kind(), MLOperator::OperatorKind::kAveragePool2d);
      break;
    case Pool2dKind::kMax:
      EXPECT_EQ(pool2d->Kind(), MLOperator::OperatorKind::kMaxPool2d);
      break;
  }
  EXPECT_EQ(pool2d->IsConnected(), true);
  EXPECT_NE(pool2d->Options(), nullptr);
  return output;
}

TEST_F(MLGraphBuilderTest, Pool2dTest) {
  V8TestingScope scope;
  auto* builder = CreateMLGraphBuilder(scope);
  const auto Pool2dKinds = {Pool2dKind::kAverage, Pool2dKind::kMax};
  for (const auto pool2d_kind : Pool2dKinds) {
    {
      // Test pool2d without windowDimensions.
      auto* input = BuildInput(scope, builder, "input", {1, 3, 4, 4},
                               V8MLOperandType::Enum::kFloat32);
      auto* output = BuildPool2d(scope, builder, pool2d_kind, input);
      EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 3, 1, 1}));
    }
    {
      // Test pool2d without padding.
      auto* input = BuildInput(scope, builder, "input", {1, 3, 4, 4},
                               V8MLOperandType::Enum::kFloat32);
      auto* options = MLPool2dOptions::Create();
      options->setWindowDimensions({3, 3});
      auto* output = BuildPool2d(scope, builder, pool2d_kind, input, options);
      EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 3, 2, 2}));
    }
    {
      // Test pool2d with padding=2.
      auto* input = BuildInput(scope, builder, "input", {1, 3, 5, 5},
                               V8MLOperandType::Enum::kFloat32);
      auto* options = MLPool2dOptions::Create();
      options->setWindowDimensions({5, 5});
      options->setPadding({2, 2, 2, 2});
      auto* output = BuildPool2d(scope, builder, pool2d_kind, input, options);
      EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 3, 5, 5}));
    }
    {
      // Test pool2d with autoPad="same-upper".
      auto* input = BuildInput(scope, builder, "input", {1, 3, 5, 5},
                               V8MLOperandType::Enum::kFloat32);
      auto* options = MLPool2dOptions::Create();
      options->setWindowDimensions({5, 5});
      options->setAutoPad(V8MLAutoPad::Enum::kSameUpper);
      auto* output = BuildPool2d(scope, builder, pool2d_kind, input, options);
      EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 3, 5, 5}));
    }
    {
      // Test pool2d with autoPad="same-lower".
      auto* input = BuildInput(scope, builder, "input", {1, 3, 5, 5},
                               V8MLOperandType::Enum::kFloat32);
      auto* options = MLPool2dOptions::Create();
      options->setWindowDimensions({5, 5});
      options->setAutoPad(V8MLAutoPad::Enum::kSameLower);
      auto* output = BuildPool2d(scope, builder, pool2d_kind, input, options);
      EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 3, 5, 5}));
    }
    {
      // Test pool2d with strides=2.
      auto* input = BuildInput(scope, builder, "input", {1, 3, 5, 5},
                               V8MLOperandType::Enum::kFloat32);
      auto* options = MLPool2dOptions::Create();
      options->setWindowDimensions({2, 2});
      options->setStrides({2, 2});
      auto* output = BuildPool2d(scope, builder, pool2d_kind, input, options);
      EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 3, 2, 2}));
    }
    {
      // Test pool2d with strides=2 and padding=1.
      auto* input = BuildInput(scope, builder, "input", {1, 3, 5, 5},
                               V8MLOperandType::Enum::kFloat32);
      auto* options = MLPool2dOptions::Create();
      options->setWindowDimensions({3, 3});
      options->setPadding({1, 1, 1, 1});
      options->setStrides({2, 2});
      auto* output = BuildPool2d(scope, builder, pool2d_kind, input, options);
      EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 3, 3, 3}));
    }
    {
      // Test pool2d with strides=2 and asymmetric padding.
      auto* input = BuildInput(scope, builder, "input", {1, 3, 7, 7},
                               V8MLOperandType::Enum::kFloat32);
      auto* options = MLPool2dOptions::Create();
      options->setWindowDimensions({4, 4});
      options->setPadding({2, 1, 2, 1});
      options->setStrides({2, 2});
      auto* output = BuildPool2d(scope, builder, pool2d_kind, input, options);
      EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 3, 4, 4}));
    }
    {
      // Test pool2d with strides=2, padding=1 and roundingType="floor".
      auto* input = BuildInput(scope, builder, "input", {1, 3, 7, 7},
                               V8MLOperandType::Enum::kFloat32);
      auto* options = MLPool2dOptions::Create();
      options->setWindowDimensions({4, 4});
      options->setPadding({1, 1, 1, 1});
      options->setStrides({2, 2});
      options->setRoundingType(V8MLRoundingType::Enum::kFloor);
      auto* output = BuildPool2d(scope, builder, pool2d_kind, input, options);
      EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 3, 3, 3}));
    }
    {
      // Test pool2d with strides=2, padding=1 and roundingType="ceil".
      auto* input = BuildInput(scope, builder, "input", {1, 3, 7, 7},
                               V8MLOperandType::Enum::kFloat32);
      auto* options = MLPool2dOptions::Create();
      options->setWindowDimensions({4, 4});
      options->setPadding({1, 1, 1, 1});
      options->setStrides({2, 2});
      options->setRoundingType(V8MLRoundingType::Enum::kCeil);
      auto* output = BuildPool2d(scope, builder, pool2d_kind, input, options);
      EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 3, 4, 4}));
    }
    {
      // Test pool2d with strides=2, padding=1 and outputSizes=[3, 3].
      auto* input = BuildInput(scope, builder, "input", {1, 3, 7, 7},
                               V8MLOperandType::Enum::kFloat32);
      auto* options = MLPool2dOptions::Create();
      options->setWindowDimensions({4, 4});
      options->setPadding({1, 1, 1, 1});
      options->setStrides({2, 2});
      options->setOutputSizes({3, 3});
      auto* output = BuildPool2d(scope, builder, pool2d_kind, input, options);
      EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 3, 3, 3}));
    }
    {
      // Test pool2d with strides=2, padding=1 and outputSizes=[4, 4].
      auto* input = BuildInput(scope, builder, "input", {1, 3, 7, 7},
                               V8MLOperandType::Enum::kFloat32);
      auto* options = MLPool2dOptions::Create();
      options->setWindowDimensions({4, 4});
      options->setPadding({1, 1, 1, 1});
      options->setStrides({2, 2});
      options->setOutputSizes({4, 4});
      auto* output = BuildPool2d(scope, builder, pool2d_kind, input, options);
      EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 3, 4, 4}));
    }
    {
      // Test pool2d with layout="nchw".
      auto* input = BuildInput(scope, builder, "input", {1, 2, 5, 5},
                               V8MLOperandType::Enum::kFloat32);
      auto* options = MLPool2dOptions::Create();
      options->setWindowDimensions({3, 3});
      options->setLayout(V8MLInputOperandLayout::Enum::kNchw);
      auto* output = BuildPool2d(scope, builder, pool2d_kind, input, options);
      EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 2, 3, 3}));
    }
    {
      // Test pool2d with layout="nhwc".
      auto* input = BuildInput(scope, builder, "input", {1, 5, 5, 2},
                               V8MLOperandType::Enum::kFloat32);
      auto* options = MLPool2dOptions::Create();
      options->setWindowDimensions({3, 3});
      options->setLayout(V8MLInputOperandLayout::Enum::kNhwc);
      auto* output = BuildPool2d(scope, builder, pool2d_kind, input, options);
      EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({1, 3, 3, 2}));
    }
  }
}

TEST_F(MLGraphBuilderTest, ReluTest) {
  V8TestingScope scope;
  auto* builder = CreateMLGraphBuilder(scope);
  {
    // Test building relu with float32 input.
    Vector<uint32_t> input_shape({3, 4, 5});
    auto* input = BuildInput(scope, builder, "input", input_shape,
                             V8MLOperandType::Enum::kFloat32);
    auto* output = builder->relu(input, scope.GetExceptionState());
    EXPECT_NE(output, nullptr);
    EXPECT_EQ(output->Kind(), MLOperand::OperandKind::kOutput);
    EXPECT_EQ(output->Type(), V8MLOperandType::Enum::kFloat32);
    EXPECT_EQ(output->Dimensions(), input_shape);
    const MLOperator* relu = output->Operator();
    EXPECT_NE(relu, nullptr);
    EXPECT_EQ(relu->Kind(), MLOperator::OperatorKind::kRelu);
    EXPECT_EQ(relu->IsConnected(), true);
    EXPECT_EQ(relu->Options(), nullptr);
  }
  {
    // Test building relu with int32 input.
    Vector<uint32_t> input_shape({3, 4, 5});
    auto* input = BuildInput(scope, builder, "input", input_shape,
                             V8MLOperandType::Enum::kInt32);
    auto* output = builder->relu(input, scope.GetExceptionState());
    EXPECT_NE(output, nullptr);
    EXPECT_EQ(output->Kind(), MLOperand::OperandKind::kOutput);
    EXPECT_EQ(output->Type(), V8MLOperandType::Enum::kInt32);
    EXPECT_EQ(output->Dimensions(), input_shape);
    const MLOperator* relu = output->Operator();
    EXPECT_NE(relu, nullptr);
    EXPECT_EQ(relu->Kind(), MLOperator::OperatorKind::kRelu);
    EXPECT_EQ(relu->IsConnected(), true);
    EXPECT_EQ(relu->Options(), nullptr);
  }
  {
    // Test building relu operator.
    auto* relu = builder->relu(scope.GetExceptionState());
    EXPECT_NE(relu, nullptr);
    EXPECT_EQ(relu->Kind(), MLOperator::OperatorKind::kRelu);
    EXPECT_EQ(relu->IsConnected(), false);
    EXPECT_EQ(relu->Options(), nullptr);
  }
}

TEST_F(MLGraphBuilderTest, HardSwishTest) {
  V8TestingScope scope;
  MLGraphBuilder* builder = CreateMLGraphBuilder(scope);
  ASSERT_NE(nullptr, builder);
  {
    // Test building hard-swish with float32 input.
    auto* input = BuildInput(scope, builder, "input", {3, 4, 5},
                             V8MLOperandType::Enum::kFloat32);
    auto* output = builder->hardSwish(input, scope.GetExceptionState());
    EXPECT_NE(output, nullptr);
    EXPECT_EQ(output->Kind(), MLOperand::OperandKind::kOutput);
    EXPECT_EQ(output->Type(), V8MLOperandType::Enum::kFloat32);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({3, 4, 5}));
    auto* hard_swish = output->Operator();
    EXPECT_NE(hard_swish, nullptr);
    EXPECT_EQ(hard_swish->Kind(), MLOperator::OperatorKind::kHardSwish);
    EXPECT_EQ(hard_swish->IsConnected(), true);
    EXPECT_EQ(hard_swish->Options(), nullptr);
  }
  {
    // Test throwing exception when building hard-swish with int32 input.
    auto* input = BuildInput(scope, builder, "input", {3, 4, 5},
                             V8MLOperandType::Enum::kInt32);
    auto* output = builder->hardSwish(input, scope.GetExceptionState());
    EXPECT_EQ(output, nullptr);
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ("The input type must be one of the floating point types.",
              scope.GetExceptionState().Message());
  }
  {
    // Test building hard-swish as a standalone operator.
    auto* hard_swish = builder->hardSwish(scope.GetExceptionState());
    EXPECT_NE(hard_swish, nullptr);
    EXPECT_EQ(hard_swish->Kind(), MLOperator::OperatorKind::kHardSwish);
    EXPECT_EQ(hard_swish->IsConnected(), false);
    EXPECT_EQ(hard_swish->Options(), nullptr);
  }
}

MLOperand* BuildGemm(V8TestingScope& scope,
                     MLGraphBuilder* builder,
                     const MLOperand* a,
                     const MLOperand* b,
                     const MLGemmOptions* options = MLGemmOptions::Create()) {
  auto* output = builder->gemm(a, b, options, scope.GetExceptionState());
  EXPECT_NE(output, nullptr);
  EXPECT_EQ(output->Kind(), MLOperand::OperandKind::kOutput);
  EXPECT_EQ(output->Type(), a->Type());
  auto* gemm = output->Operator();
  EXPECT_NE(gemm, nullptr);
  EXPECT_EQ(gemm->Kind(), MLOperator::OperatorKind::kGemm);
  EXPECT_EQ(gemm->IsConnected(), true);
  EXPECT_NE(gemm->Options(), nullptr);
  return output;
}

TEST_F(MLGraphBuilderTest, GemmTest) {
  V8TestingScope scope;
  MLGraphBuilder* builder = CreateMLGraphBuilder(scope);
  ASSERT_NE(nullptr, builder);
  {
    // Test building gemm with default option.
    auto* a = BuildInput(scope, builder, "a", {2, 3},
                         V8MLOperandType::Enum::kFloat32);
    auto* b = BuildInput(scope, builder, "b", {3, 4},
                         V8MLOperandType::Enum::kFloat32);
    auto* options = MLGemmOptions::Create();
    EXPECT_FALSE(options->hasC());
    EXPECT_TRUE(options->hasAlpha());
    EXPECT_EQ(options->alpha(), 1);
    EXPECT_TRUE(options->hasBeta());
    EXPECT_EQ(options->beta(), 1);
    EXPECT_TRUE(options->hasATranspose());
    EXPECT_EQ(options->aTranspose(), false);
    EXPECT_TRUE(options->hasBTranspose());
    EXPECT_EQ(options->bTranspose(), false);
    auto* output = BuildGemm(scope, builder, a, b, options);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({2, 4}));
  }
  {
    // Test building gemm with two matrices - {2, 3} and {2, 4} that can't be
    // multiplied together due to incompatible dimensions.
    auto* a = BuildInput(scope, builder, "a", {2, 3},
                         V8MLOperandType::Enum::kFloat32);
    auto* b = BuildInput(scope, builder, "b", {2, 4},
                         V8MLOperandType::Enum::kFloat32);
    auto* options = MLGemmOptions::Create();
    auto* output = builder->gemm(a, b, options, scope.GetExceptionState());
    EXPECT_EQ(output, nullptr);
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(
        scope.GetExceptionState().Message(),
        "The number of columns (3) in the first matrix isn't equal to the "
        "number of rows (2) in the second matrix.");
  }
  {
    // Test building gemm with aTranspose = true.
    // Transposed a_dimensions would be {3, 2} and it's compatible with
    // b_dimensions {2, 4}.
    auto* a = BuildInput(scope, builder, "a", {2, 3},
                         V8MLOperandType::Enum::kFloat32);
    auto* b = BuildInput(scope, builder, "b", {2, 4},
                         V8MLOperandType::Enum::kFloat32);
    auto* options = MLGemmOptions::Create();
    options->setATranspose(true);
    auto* output = BuildGemm(scope, builder, a, b, options);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({3, 4}));
  }
  {
    // Test building gemm with aTranspose = true.
    // Transposed a_dimensions would be {3, 2} and it can't be multiplied with
    // b_dimensions {3, 4}.
    auto* a = BuildInput(scope, builder, "a", {2, 3},
                         V8MLOperandType::Enum::kFloat32);
    auto* b = BuildInput(scope, builder, "b", {3, 4},
                         V8MLOperandType::Enum::kFloat32);
    auto* options = MLGemmOptions::Create();
    options->setATranspose(true);
    auto* output = builder->gemm(a, b, options, scope.GetExceptionState());
    EXPECT_EQ(output, nullptr);
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(
        scope.GetExceptionState().Message(),
        "The number of columns (2) in the transposed first matrix isn't equal "
        "to the number of rows (3) in the second matrix.");
  }
  {
    // Test building gemm with bTranspose = true.
    // Transposed b_dimensions would be {3, 4} and it's compatible with
    // a_dimensions {2, 3}.
    auto* a = BuildInput(scope, builder, "a", {2, 3},
                         V8MLOperandType::Enum::kFloat32);
    auto* b = BuildInput(scope, builder, "b", {4, 3},
                         V8MLOperandType::Enum::kFloat32);
    auto* options = MLGemmOptions::Create();
    options->setBTranspose(true);
    auto* output = BuildGemm(scope, builder, a, b, options);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({2, 4}));
  }
  {
    // Test building gemm with bTranspose = true.
    // Transposed b_dimensions would be {4, 3} and it's incompatible with
    // a_dimensions {2, 3}.
    auto* a = BuildInput(scope, builder, "a", {2, 3},
                         V8MLOperandType::Enum::kFloat32);
    auto* b = BuildInput(scope, builder, "b", {3, 4},
                         V8MLOperandType::Enum::kFloat32);
    auto* options = MLGemmOptions::Create();
    options->setBTranspose(true);
    auto* output = builder->gemm(a, b, options, scope.GetExceptionState());
    EXPECT_EQ(output, nullptr);
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(
        scope.GetExceptionState().Message(),
        "The number of columns (3) in the first matrix isn't equal to the "
        "number of rows (4) in the transposed second matrix.");
  }
  {
    // Test building gemm with a_dimensions = {2, 3, 1}.
    // Test throwing an error due to input_a is not a 2-D tensor.
    auto* a = BuildInput(scope, builder, "a", {2, 3, 1},
                         V8MLOperandType::Enum::kFloat32);
    auto* b = BuildInput(scope, builder, "b", {2, 4},
                         V8MLOperandType::Enum::kFloat32);
    auto* options = MLGemmOptions::Create();
    auto* output = builder->gemm(a, b, options, scope.GetExceptionState());
    EXPECT_EQ(output, nullptr);
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The first input must be a 2-D tensor.");
  }
  {
    // Test building gemm with two mismatching input types.
    auto* a = BuildInput(scope, builder, "a", {2, 3},
                         V8MLOperandType::Enum::kFloat32);
    auto* b =
        BuildInput(scope, builder, "b", {3, 4}, V8MLOperandType::Enum::kInt32);
    auto* options = MLGemmOptions::Create();
    auto* output = builder->gemm(a, b, options, scope.GetExceptionState());
    EXPECT_EQ(output, nullptr);
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The types of first two inputs don't match.");
  }
  {
    // Test building gemm with setting optional input C.
    // The output dimensions of a * b would be {2, 4} and
    // c_dimensions {4} is able to broadcast to {2, 4}.
    auto* a = BuildInput(scope, builder, "a", {2, 3},
                         V8MLOperandType::Enum::kFloat32);
    auto* b = BuildInput(scope, builder, "b", {3, 4},
                         V8MLOperandType::Enum::kFloat32);
    auto* options = MLGemmOptions::Create();
    auto* c =
        BuildInput(scope, builder, "c", {4}, V8MLOperandType::Enum::kFloat32);
    options->setC(c);
    auto* output = BuildGemm(scope, builder, a, b, options);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({2, 4}));
  }
  {
    // Test building gemm with aTranspose = true, bTranspose = true.
    // The output dimensions of a * b would be {2, 4} and
    // c_dimension {2, 3} is incompatible with {2, 4}.
    auto* a = BuildInput(scope, builder, "a", {2, 3},
                         V8MLOperandType::Enum::kFloat32);
    auto* b = BuildInput(scope, builder, "b", {3, 4},
                         V8MLOperandType::Enum::kFloat32);
    auto* options = MLGemmOptions::Create();
    auto* c = BuildInput(scope, builder, "a", {2, 3},
                         V8MLOperandType::Enum::kFloat32);
    options->setC(c);
    auto* output = builder->gemm(a, b, options, scope.GetExceptionState());
    EXPECT_EQ(output, nullptr);
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The third input tensor isn't unidirectionally broadcastable to "
              "the output tensor.");
  }
  {
    // Test building gemm with aTranspose = true, bTranspose = true.
    // Set optional input C with type = int32 and it mismatches with input
    // type float32.
    auto* a = BuildInput(scope, builder, "a", {3, 2},
                         V8MLOperandType::Enum::kFloat32);
    auto* b = BuildInput(scope, builder, "b", {4, 3},
                         V8MLOperandType::Enum::kFloat32);
    auto* options = MLGemmOptions::Create();
    auto* c =
        BuildInput(scope, builder, "c", {2, 4}, V8MLOperandType::Enum::kInt32);
    options->setC(c);
    options->setATranspose(true);
    options->setBTranspose(true);
    auto* output = builder->gemm(a, b, options, scope.GetExceptionState());
    EXPECT_EQ(output, nullptr);
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kDataError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The third input type doesn't match other inputs' type.");
  }
  {
    // Test building gemm with aTranspose = true, bTranspose = true.
    // Set optional input C with dimensions = {2, 3, 4} and an error should be
    // thrown since c_dimensions is not a 2-D tensor.
    auto* a = BuildInput(scope, builder, "a", {3, 2},
                         V8MLOperandType::Enum::kFloat32);
    auto* b = BuildInput(scope, builder, "b", {4, 3},
                         V8MLOperandType::Enum::kFloat32);
    auto* options = MLGemmOptions::Create();
    auto* c = BuildInput(scope, builder, "c", {2, 3, 4},
                         V8MLOperandType::Enum::kFloat32);
    options->setC(c);
    options->setATranspose(true);
    options->setBTranspose(true);
    auto* output = builder->gemm(a, b, options, scope.GetExceptionState());
    EXPECT_EQ(output, nullptr);
    EXPECT_EQ(ToExceptionCode(DOMExceptionCode::kDataError),
              scope.GetExceptionState().Code());
    EXPECT_EQ(
        scope.GetExceptionState().Message(),
        "The third input tensor should be either a scalar or a 2-D tensor.");
  }
}

enum class ElementWiseBinaryKind { kAdd, kSub, kMul, kDiv, kMin, kMax };

MLOperand* BuildElementWiseBinary(V8TestingScope& scope,
                                  MLGraphBuilder* builder,
                                  ElementWiseBinaryKind kind,
                                  const MLOperand* a,
                                  const MLOperand* b) {
  MLOperand* output = nullptr;
  switch (kind) {
    case ElementWiseBinaryKind::kAdd:
      output = builder->add(a, b, scope.GetExceptionState());
      break;
    case ElementWiseBinaryKind::kSub:
      output = builder->sub(a, b, scope.GetExceptionState());
      break;
    case ElementWiseBinaryKind::kMul:
      output = builder->mul(a, b, scope.GetExceptionState());
      break;
    case ElementWiseBinaryKind::kDiv:
      output = builder->div(a, b, scope.GetExceptionState());
      break;
    case ElementWiseBinaryKind::kMin:
      output = builder->min(a, b, scope.GetExceptionState());
      break;
    case ElementWiseBinaryKind::kMax:
      output = builder->max(a, b, scope.GetExceptionState());
      break;
  }
  EXPECT_NE(output, nullptr);
  EXPECT_EQ(output->Kind(), MLOperand::OperandKind::kOutput);
  EXPECT_EQ(output->Type(), a->Type());
  auto* op = output->Operator();
  EXPECT_NE(op, nullptr);
  switch (kind) {
    case ElementWiseBinaryKind::kAdd:
      EXPECT_EQ(op->Kind(), MLOperator::OperatorKind::kAdd);
      break;
    case ElementWiseBinaryKind::kSub:
      EXPECT_EQ(op->Kind(), MLOperator::OperatorKind::kSub);
      break;
    case ElementWiseBinaryKind::kMul:
      EXPECT_EQ(op->Kind(), MLOperator::OperatorKind::kMul);
      break;
    case ElementWiseBinaryKind::kDiv:
      EXPECT_EQ(op->Kind(), MLOperator::OperatorKind::kDiv);
      break;
    case ElementWiseBinaryKind::kMin:
      EXPECT_EQ(op->Kind(), MLOperator::OperatorKind::kMin);
      break;
    case ElementWiseBinaryKind::kMax:
      EXPECT_EQ(op->Kind(), MLOperator::OperatorKind::kMax);
      break;
  }
  EXPECT_EQ(op->IsConnected(), true);
  return output;
}

TEST_F(MLGraphBuilderTest, ElementWiseBinaryTest) {
  V8TestingScope scope;
  MLGraphBuilder* builder = CreateMLGraphBuilder(scope);
  {
    // Testing building add with two input dimensions - {8, 1, 6, 1} and {7, 1,
    // 5}. Both the a and b dimensions have axes with length one that are
    // expanded to a larger size during the broadcast operation.
    // a_dimensions     (4d) 8 * 1 * 6 * 1
    // b_dimensions     (3d)     7 * 1 * 5
    // output_dimenions (4d) 8 * 7 * 6 * 5
    auto* a = BuildInput(scope, builder, "a", {8, 1, 6, 1},
                         V8MLOperandType::Enum::kFloat32);
    auto* b = BuildInput(scope, builder, "b", {7, 1, 5},
                         V8MLOperandType::Enum::kFloat32);
    auto* output = BuildElementWiseBinary(scope, builder,
                                          ElementWiseBinaryKind::kAdd, a, b);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({8, 7, 6, 5}));
  }
  {
    // Testing building add with two input dimensions - {4, 2, 1} and {4}.
    // a_dimensions     (3d) 4 * 2 * 1
    // b_dimensions     (1d)         4
    // output_dimenions (3d) 4 * 2 * 4
    auto* a = BuildInput(scope, builder, "a", {4, 2, 1},
                         V8MLOperandType::Enum::kFloat32);
    auto* b =
        BuildInput(scope, builder, "b", {4}, V8MLOperandType::Enum::kFloat32);
    auto* output = BuildElementWiseBinary(scope, builder,
                                          ElementWiseBinaryKind::kAdd, a, b);
    EXPECT_EQ(output->Dimensions(), Vector<uint32_t>({4, 2, 4}));
  }
  {
    // Test throwing exception when the input shapes are not broadcastable.
    auto* a = BuildInput(scope, builder, "a", {4, 2},
                         V8MLOperandType::Enum::kFloat32);
    auto* b =
        BuildInput(scope, builder, "b", {4}, V8MLOperandType::Enum::kFloat32);
    auto* output = builder->sub(a, b, scope.GetExceptionState());
    EXPECT_EQ(output, nullptr);
    EXPECT_EQ(ToExceptionCode(DOMExceptionCode::kDataError),
              scope.GetExceptionState().Code());
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The input shapes are not broadcastable.");
  }
  {
    // Test throwing exception when the input types don't match.
    auto* a = BuildInput(scope, builder, "a", {4, 2},
                         V8MLOperandType::Enum::kFloat32);
    auto* b =
        BuildInput(scope, builder, "b", {1}, V8MLOperandType::Enum::kInt32);
    auto* output = builder->max(a, b, scope.GetExceptionState());
    EXPECT_EQ(output, nullptr);
    EXPECT_EQ(ToExceptionCode(DOMExceptionCode::kDataError),
              scope.GetExceptionState().Code());
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "The input types don't match.");
  }
}

}  // namespace blink
