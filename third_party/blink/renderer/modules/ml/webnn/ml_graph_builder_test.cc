// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <memory>

#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_builder.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_context_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_operand_descriptor.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_typed_array.h"
#include "third_party/blink/renderer/modules/ml/ml.h"
#include "third_party/blink/renderer/modules/ml/ml_context.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_operand.h"

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
}  // namespace blink
