// Copyright 2022 The Chromium Authors. All rights reserved.
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
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_operand_descriptor.h"
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

MLGraphBuilder* CreateMLGraphBuilder(V8TestingScope& v8_scope) {
  auto* ml = MakeGarbageCollected<ML>(v8_scope.GetExecutionContext());
  auto* options = MLContextOptions::Create();
  options->setDevicePreference(V8MLDevicePreference::Enum::kAuto);
  options->setPowerPreference(V8MLPowerPreference::Enum::kAuto);
  auto* context = MakeGarbageCollected<MLContext>(
      options->devicePreference(), options->powerPreference(),
      options->modelFormat(), options->numThreads(), ml);
  return MLGraphBuilder::Create(context);
}

MLOperand* CreateInput(
    V8TestingScope& v8_scope,
    MLGraphBuilder* graph_builder,
    const String& name,
    const Vector<uint32_t>& dimensions,
    V8MLOperandType::Enum type = V8MLOperandType::Enum::kFloat32) {
  MLOperandDescriptor* operand_desc = MLOperandDescriptor::Create();
  operand_desc->setDimensions(dimensions);
  operand_desc->setType(type);
  return graph_builder->input(name, operand_desc, v8_scope.GetExceptionState());
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
  return buffer_view;
}

MLOperand* CreateConstant(
    V8TestingScope& v8_scope,
    MLGraphBuilder* graph_builder,
    const Vector<uint32_t>& dimensions,
    V8MLOperandType::Enum type = V8MLOperandType::Enum::kFloat32) {
  MLOperandDescriptor* operand_desc = MLOperandDescriptor::Create();
  operand_desc->setDimensions(dimensions);
  operand_desc->setType(type);
  size_t size = std::accumulate(dimensions.begin(), dimensions.end(), size_t(1),
                                std::multiplies<uint32_t>());
  NotShared<DOMArrayBufferView> buffer_view =
      CreateDOMArrayBufferView(size, type);
  return graph_builder->constant(operand_desc, buffer_view,
                                 v8_scope.GetExceptionState());
}

void CheckDimensions(const Vector<uint32_t>& actual,
                     const Vector<uint32_t>& expected) {
  EXPECT_EQ(actual.size(), expected.size());
  for (wtf_size_t i = 0; i < actual.size(); ++i) {
    EXPECT_EQ(actual[i], expected[i]);
  }
}

class Conv2dTester {
 public:
  Conv2dTester& SetOperandType(V8MLOperandType::Enum type) {
    type_ = type;
    return *this;
  }
  Conv2dTester& SetInputShape(const Vector<uint32_t>& input_shape) {
    input_shape_ = input_shape;
    return *this;
  }
  Conv2dTester& SetFilterShape(const Vector<uint32_t>& filter_shape) {
    filter_shape_ = filter_shape;
    return *this;
  }
  Conv2dTester& SetOptionsPadding(const Vector<int32_t>& padding) {
    padding_ = padding;
    return *this;
  }
  Conv2dTester& SetOptionsStrides(const Vector<int32_t>& strides) {
    strides_ = strides;
    return *this;
  }
  Conv2dTester& SetOptionsDilations(const Vector<int32_t>& dilations) {
    dilations_ = dilations;
    return *this;
  }
  Conv2dTester& SetOptionsAutoPad(V8MLAutoPad::Enum auto_pad) {
    auto_pad_ = auto_pad;
    return *this;
  }
  Conv2dTester& SetOptionsGroups(int32_t groups) {
    groups_ = groups;
    return *this;
  }
  Conv2dTester& SetOptionsInputLayout(
      V8MLInputOperandLayout::Enum input_layout) {
    input_layout_ = input_layout;
    return *this;
  }
  Conv2dTester& SetOptionsFilterLayout(
      V8MLConv2dFilterOperandLayout::Enum filter_layout) {
    filter_layout_ = filter_layout;
    return *this;
  }
  void ExpectOutputShape(const Vector<uint32_t>& expected_output_shape) {
    V8TestingScope v8_scope;
    MLGraphBuilder* graph_builder = CreateMLGraphBuilder(v8_scope);
    ASSERT_NE(nullptr, graph_builder);
    MLOperand* input =
        CreateInput(v8_scope, graph_builder, "input", input_shape_, type_);
    ASSERT_NE(input, nullptr);
    MLOperand* filter =
        CreateConstant(v8_scope, graph_builder, filter_shape_, type_);
    ASSERT_NE(filter, nullptr);

    MLConv2dOptions* options = MLConv2dOptions::Create();
    if (padding_) {
      options->setPadding(padding_.value());
    }
    if (strides_) {
      options->setStrides(strides_.value());
    }
    if (dilations_) {
      options->setDilations(dilations_.value());
    }
    if (auto_pad_) {
      options->setAutoPad(auto_pad_.value());
    }
    if (groups_) {
      options->setGroups(groups_.value());
    }
    if (input_layout_) {
      options->setInputLayout(input_layout_.value());
    }
    if (filter_layout_) {
      options->setFilterLayout(filter_layout_.value());
    }
    MLOperand* output = graph_builder->conv2d(input, filter, options,
                                              v8_scope.GetExceptionState());
    EXPECT_NE(output, nullptr);
    EXPECT_EQ(output->Kind(), MLOperand::OperandKind::kOutput);
    EXPECT_EQ(output->Type(), type_);
    CheckDimensions(output->Dimensions(), expected_output_shape);
    const MLOperator* conv2d = output->Operator();
    EXPECT_NE(conv2d, nullptr);
    EXPECT_EQ(conv2d->Kind(), MLOperator::OperatorKind::kConv2d);
    EXPECT_EQ(conv2d->IsConnected(), true);
    EXPECT_NE(conv2d->Options(), nullptr);
  }

 private:
  V8MLOperandType::Enum type_{V8MLOperandType::Enum::kFloat32};
  Vector<uint32_t> input_shape_;
  Vector<uint32_t> filter_shape_;
  absl::optional<Vector<int32_t>> padding_;
  absl::optional<Vector<int32_t>> strides_;
  absl::optional<Vector<int32_t>> dilations_;
  absl::optional<V8MLAutoPad::Enum> auto_pad_;
  absl::optional<int32_t> groups_;
  absl::optional<V8MLInputOperandLayout::Enum> input_layout_;
  absl::optional<V8MLConv2dFilterOperandLayout::Enum> filter_layout_;
};

TEST_F(MLGraphBuilderTest, InputValidationTest) {
  V8TestingScope v8_scope;
  MLGraphBuilder* graph_builder = CreateMLGraphBuilder(v8_scope);
  ASSERT_NE(nullptr, graph_builder);
  {
    MLOperand* input = CreateInput(v8_scope, graph_builder, "input", {3, 4},
                                   V8MLOperandType::Enum::kInt32);
    EXPECT_NE(input, nullptr);
    EXPECT_EQ(v8_scope.GetExceptionState().Code(),
              ToExceptionCode(DOMExceptionCode::kNoError));
  }
  {
    MLOperand* input = CreateInput(v8_scope, graph_builder, "", {3, 4});
    EXPECT_EQ(input, nullptr);
    EXPECT_EQ(v8_scope.GetExceptionState().Code(),
              ToExceptionCode(DOMExceptionCode::kDataError));
    EXPECT_EQ("The name is empty.", v8_scope.GetExceptionState().Message());
  }
  {
    MLOperand* input = CreateInput(v8_scope, graph_builder, "input", {3, 0});
    EXPECT_EQ(input, nullptr);
    EXPECT_EQ(v8_scope.GetExceptionState().Code(),
              ToExceptionCode(DOMExceptionCode::kDataError));
    EXPECT_EQ("Invalid operand descriptor: All dimensions should be positive",
              v8_scope.GetExceptionState().Message());
  }
  {
    MLOperand* input = CreateInput(v8_scope, graph_builder, "input", {});
    EXPECT_EQ(input, nullptr);
    EXPECT_EQ(v8_scope.GetExceptionState().Code(),
              ToExceptionCode(DOMExceptionCode::kDataError));
    EXPECT_EQ("Invalid operand descriptor: The dimensions is empty.",
              v8_scope.GetExceptionState().Message());
  }
  {
    MLOperand* input = CreateInput(v8_scope, graph_builder, "input",
                                   {2147483600, 102834, 2347816});
    EXPECT_EQ(input, nullptr);
    EXPECT_EQ(v8_scope.GetExceptionState().Code(),
              ToExceptionCode(DOMExceptionCode::kDataError));
    EXPECT_EQ(
        "Invalid operand descriptor: The elements number of the dimensions is "
        "too large.",
        v8_scope.GetExceptionState().Message());
  }
}

TEST_F(MLGraphBuilderTest, ConstantValidationTest) {
  V8TestingScope v8_scope;
  MLGraphBuilder* graph_builder = CreateMLGraphBuilder(v8_scope);
  ASSERT_NE(nullptr, graph_builder);
  MLOperandDescriptor* operand_desc = MLOperandDescriptor::Create();
  {
    operand_desc->setDimensions({2, 3});
    operand_desc->setType(V8MLOperandType::Enum::kFloat32);
    NotShared<DOMArrayBufferView> buffer_view =
        CreateDOMArrayBufferView(6, V8MLOperandType::Enum::kFloat32);
    MLOperand* constant = graph_builder->constant(operand_desc, buffer_view,
                                                  v8_scope.GetExceptionState());
    EXPECT_NE(constant, nullptr);
    EXPECT_EQ(v8_scope.GetExceptionState().Code(),
              ToExceptionCode(DOMExceptionCode::kNoError));
  }
  {
    operand_desc->setDimensions({2, 0});
    operand_desc->setType(V8MLOperandType::Enum::kFloat32);
    NotShared<DOMArrayBufferView> buffer_view =
        CreateDOMArrayBufferView(6, V8MLOperandType::Enum::kFloat32);
    MLOperand* constant = graph_builder->constant(operand_desc, buffer_view,
                                                  v8_scope.GetExceptionState());
    EXPECT_EQ(constant, nullptr);
    EXPECT_EQ(v8_scope.GetExceptionState().Code(),
              ToExceptionCode(DOMExceptionCode::kDataError));
    EXPECT_EQ("Invalid operand descriptor: All dimensions should be positive",
              v8_scope.GetExceptionState().Message());
  }
  {
    operand_desc->setDimensions({2, 3});
    operand_desc->setType(V8MLOperandType::Enum::kInt32);
    NotShared<DOMArrayBufferView> buffer_view =
        CreateDOMArrayBufferView(6, V8MLOperandType::Enum::kFloat32);
    MLOperand* constant = graph_builder->constant(operand_desc, buffer_view,
                                                  v8_scope.GetExceptionState());
    EXPECT_EQ(constant, nullptr);
    EXPECT_EQ(v8_scope.GetExceptionState().Code(),
              ToExceptionCode(DOMExceptionCode::kDataError));
    EXPECT_EQ("The buffer view type doesn't match the operand type.",
              v8_scope.GetExceptionState().Message());
  }
  {
    operand_desc->setDimensions({2, 2});
    operand_desc->setType(V8MLOperandType::Enum::kInt32);
    NotShared<DOMArrayBufferView> buffer_view =
        CreateDOMArrayBufferView(8, V8MLOperandType::Enum::kInt32);
    MLOperand* constant = graph_builder->constant(operand_desc, buffer_view,
                                                  v8_scope.GetExceptionState());
    EXPECT_EQ(constant, nullptr);
    EXPECT_EQ(v8_scope.GetExceptionState().Code(),
              ToExceptionCode(DOMExceptionCode::kDataError));
    String msg = String("The buffer view byte length") + String(" (32) ") +
                 String("doesn't match the expected byte length") +
                 String(" (16).");
    EXPECT_EQ(msg, v8_scope.GetExceptionState().Message());
  }
}

TEST_F(MLGraphBuilderTest, Conv2dOutputShapeTest) {
  {
    // Test conv2d without padding.
    Conv2dTester()
        .SetInputShape({1, 1, 5, 5})
        .SetFilterShape({1, 1, 3, 3})
        .ExpectOutputShape({1, 1, 3, 3});
  }
  {
    // Test conv2d with padding=1.
    Conv2dTester()
        .SetInputShape({1, 1, 5, 5})
        .SetFilterShape({1, 1, 3, 3})
        .SetOptionsPadding({1, 1, 1, 1})
        .ExpectOutputShape({1, 1, 5, 5});
  }
  {
    // Test conv2d with autopad="same-lower".
    Conv2dTester()
        .SetInputShape({1, 1, 5, 5})
        .SetFilterShape({1, 1, 3, 3})
        .SetOptionsAutoPad(V8MLAutoPad::Enum::kSameLower)
        .ExpectOutputShape({1, 1, 5, 5});
  }
  {
    // Test conv2d with autopad="same-upper".
    Conv2dTester()
        .SetInputShape({1, 1, 5, 5})
        .SetFilterShape({1, 1, 3, 3})
        .SetOptionsAutoPad(V8MLAutoPad::Enum::kSameUpper)
        .ExpectOutputShape({1, 1, 5, 5});
  }
  {
    // Test conv2d with strides=2 and padding=1.
    Conv2dTester()
        .SetInputShape({1, 1, 5, 5})
        .SetFilterShape({1, 1, 3, 3})
        .SetOptionsPadding({1, 1, 1, 1})
        .SetOptionsStrides({2, 2})
        .ExpectOutputShape({1, 1, 3, 3});
  }
  {
    // Test conv2d with strides=2 and asymmetric padding.
    Conv2dTester()
        .SetInputShape({1, 1, 5, 5})
        .SetFilterShape({1, 1, 4, 2})
        .SetOptionsPadding({1, 2, 0, 1})
        .SetOptionsStrides({2, 2})
        .ExpectOutputShape({1, 1, 3, 3});
  }
  {
    // Test depthwise conv2d by setting groups to input channels.
    Conv2dTester()
        .SetInputShape({1, 4, 2, 2})
        .SetFilterShape({4, 1, 2, 2})
        .SetOptionsGroups(4)
        .ExpectOutputShape({1, 4, 1, 1});
  }
  {
    // Test depthwise conv2d with groups=4, inputLayout="nhwc" and
    // filterLayout="ihwo".
    Conv2dTester()
        .SetInputShape({1, 2, 2, 4})
        .SetFilterShape({1, 2, 2, 4})
        .SetOptionsGroups(4)
        .SetOptionsInputLayout(V8MLInputOperandLayout::Enum::kNhwc)
        .SetOptionsFilterLayout(V8MLConv2dFilterOperandLayout::Enum::kIhwo)
        .ExpectOutputShape({1, 1, 1, 4});
  }
  {
    // Test conv2d with dilations=4, inputLayout="nhwc" and
    // filterLayout="ihwo".
    Conv2dTester()
        .SetInputShape({1, 65, 65, 1})
        .SetFilterShape({1, 3, 3, 1})
        .SetOptionsInputLayout(V8MLInputOperandLayout::Enum::kNhwc)
        .SetOptionsFilterLayout(V8MLConv2dFilterOperandLayout::Enum::kIhwo)
        .SetOptionsDilations({4, 4})
        .ExpectOutputShape({1, 57, 57, 1});
  }
  {
    // Test conv2d with inputLayout="nchw" and filterLayout="oihw".
    Conv2dTester()
        .SetInputShape({1, 2, 5, 5})
        .SetFilterShape({1, 2, 3, 3})
        .SetOptionsInputLayout(V8MLInputOperandLayout::Enum::kNchw)
        .SetOptionsFilterLayout(V8MLConv2dFilterOperandLayout::Enum::kOihw)
        .ExpectOutputShape({1, 1, 3, 3});
  }
  {
    // Test conv2d with inputLayout="nchw" and filterLayout="hwio".
    Conv2dTester()
        .SetInputShape({1, 2, 5, 5})
        .SetFilterShape({3, 3, 2, 1})
        .SetOptionsInputLayout(V8MLInputOperandLayout::Enum::kNchw)
        .SetOptionsFilterLayout(V8MLConv2dFilterOperandLayout::Enum::kHwio)
        .ExpectOutputShape({1, 1, 3, 3});
  }
  {
    // Test conv2d with inputLayout="nchw" and filterLayout="ohwi".
    Conv2dTester()
        .SetInputShape({1, 2, 5, 5})
        .SetFilterShape({1, 3, 3, 2})
        .SetOptionsInputLayout(V8MLInputOperandLayout::Enum::kNchw)
        .SetOptionsFilterLayout(V8MLConv2dFilterOperandLayout::Enum::kOhwi)
        .ExpectOutputShape({1, 1, 3, 3});
  }
  {
    // Test conv2d with inputLayout="nchw" and filterLayout="ihwo".
    Conv2dTester()
        .SetInputShape({1, 2, 5, 5})
        .SetFilterShape({2, 3, 3, 1})
        .SetOptionsInputLayout(V8MLInputOperandLayout::Enum::kNchw)
        .SetOptionsFilterLayout(V8MLConv2dFilterOperandLayout::Enum::kIhwo)
        .ExpectOutputShape({1, 1, 3, 3});
  }
  {
    // Test conv2d with inputLayout="nhwc" and filterLayout="oihw".
    Conv2dTester()
        .SetInputShape({1, 5, 5, 2})
        .SetFilterShape({1, 2, 3, 3})
        .SetOptionsInputLayout(V8MLInputOperandLayout::Enum::kNhwc)
        .SetOptionsFilterLayout(V8MLConv2dFilterOperandLayout::Enum::kOihw)
        .ExpectOutputShape({1, 3, 3, 1});
  }
  {
    // Test conv2d with inputLayout="nhwc" and filterLayout="hwio".
    Conv2dTester()
        .SetInputShape({1, 5, 5, 2})
        .SetFilterShape({3, 3, 2, 1})
        .SetOptionsInputLayout(V8MLInputOperandLayout::Enum::kNhwc)
        .SetOptionsFilterLayout(V8MLConv2dFilterOperandLayout::Enum::kHwio)
        .ExpectOutputShape({1, 3, 3, 1});
  }
  {
    // Test conv2d with inputLayout="nchw" and filterLayout="ohwi".
    Conv2dTester()
        .SetInputShape({1, 5, 5, 2})
        .SetFilterShape({1, 3, 3, 2})
        .SetOptionsInputLayout(V8MLInputOperandLayout::Enum::kNhwc)
        .SetOptionsFilterLayout(V8MLConv2dFilterOperandLayout::Enum::kOhwi)
        .ExpectOutputShape({1, 3, 3, 1});
  }
  {
    // Test conv2d with inputLayout="nchw" and filterLayout="ihwo".
    Conv2dTester()
        .SetInputShape({1, 5, 5, 2})
        .SetFilterShape({2, 3, 3, 1})
        .SetOptionsInputLayout(V8MLInputOperandLayout::Enum::kNhwc)
        .SetOptionsFilterLayout(V8MLConv2dFilterOperandLayout::Enum::kIhwo)
        .ExpectOutputShape({1, 3, 3, 1});
  }
}

}  // namespace blink
