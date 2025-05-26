// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/ort/model_editor.h"

#include <array>
#include <memory>
#include <vector>

#include "base/containers/heap_array.h"
#include "base/strings/cstring_view.h"
#include "services/webnn/ort/scoped_ort_types.h"
#include "services/webnn/ort/test_base_ort.h"
#include "services/webnn/public/cpp/operand_descriptor.h"
#include "services/webnn/webnn_constant_operand.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace webnn::ort {

class WebNNOrtModelEditorTest : public TestBaseOrt {};

TEST_F(WebNNOrtModelEditorTest, AddAndGather) {
  ModelEditor model_editor;

  // Add an input.
  constexpr base::cstring_view input = "input";
  auto input_desc = OperandDescriptor::CreateForDeserialization(
      OperandDataType::kUint32, {4, 2, 4});
  ASSERT_TRUE(input_desc.has_value());
  model_editor.AddInput(input, input_desc.value());

  // Add two initializers.
  constexpr base::cstring_view add_initializer = "add_initializer";
  std::array<uint32_t, 32> add_initializer_data;  // 128 bytes
  add_initializer_data.fill(1);
  auto add_initializer_desc = OperandDescriptor::CreateForDeserialization(
      OperandDataType::kUint32, {4, 2, 4});
  ASSERT_TRUE(add_initializer_desc.has_value());
  auto add_initializer_operand = std::make_unique<WebNNConstantOperand>(
      std::move(add_initializer_desc.value()),
      base::HeapArray<uint8_t>::CopiedFrom(
          base::as_byte_span(add_initializer_data)));
  model_editor.AddInitializer(add_initializer,
                              std::move(add_initializer_operand));

  constexpr base::cstring_view gather_indices_initializer =
      "gather_indices_initializer";
  std::array<int64_t, 4> gather_indices_initializer_data = {0, 1, 0,
                                                            1};  // 32 bytes
  auto gather_indices_initializer_desc =
      OperandDescriptor::CreateForDeserialization(OperandDataType::kInt64, {4});
  ASSERT_TRUE(gather_indices_initializer_desc.has_value());
  auto gather_indices_initializer_operand =
      std::make_unique<WebNNConstantOperand>(
          std::move(gather_indices_initializer_desc.value()),
          base::HeapArray<uint8_t>::CopiedFrom(
              base::as_byte_span(gather_indices_initializer_data)));
  model_editor.AddInitializer(gather_indices_initializer,
                              std::move(gather_indices_initializer_operand));

  // Add Add node.
  constexpr base::cstring_view add_output = "add_output";
  std::array<const char*, 2> add_inputs = {input.c_str(),
                                           add_initializer.c_str()};
  std::array<const char*, 1> add_outputs = {add_output.c_str()};
  model_editor.AddNode(
      /*op_type=*/"Add", /*node_name=*/"add", add_inputs, add_outputs);

  // Add Gather node.
  int64_t axis = 1;
  std::array<ScopedOrtOpAttr, 1> gather_attrs = {
      model_editor.CreateAttribute(/*name=*/"axis", axis)};

  constexpr base::cstring_view output = "output";
  std::array<const char*, 2> gather_inputs = {
      add_output.c_str(), gather_indices_initializer.c_str()};
  std::array<const char*, 1> gather_outputs = {output.c_str()};
  model_editor.AddNode(
      /*op_type=*/"Gather", /*node_name=*/"gather", gather_inputs,
      gather_outputs, gather_attrs);

  // Add an output.
  model_editor.AddOutput(output, OperandDescriptor::CreateForDeserialization(
                                     OperandDataType::kUint32, {4, 4, 4})
                                     .value());

  std::unique_ptr<ModelEditor::ModelInfo> model_info =
      model_editor.BuildAndTakeModelInfo();
  ASSERT_NE(model_info, nullptr);

  // `add_initializer` data should be saved into `external_data` since it
  // reaches `kMinExternalDataSize`.
  EXPECT_EQ(model_info->external_data.size(), 1u);
  EXPECT_TRUE(model_info->model.is_valid());
}

TEST_F(WebNNOrtModelEditorTest, ReshapeToScalar) {
  ModelEditor model_editor;

  // Add an input.
  constexpr base::cstring_view input = "input";
  auto input_desc = OperandDescriptor::CreateForDeserialization(
      OperandDataType::kInt32, {1, 1, 1, 1});
  ASSERT_TRUE(input_desc.has_value());
  model_editor.AddInput(input, input_desc.value());

  // Add an initializer which is an empty tensor that represents an empty shape
  // of a scalar.
  constexpr base::cstring_view new_shape_initializer = "new_shape_initializer";
  model_editor.AddInitializer(new_shape_initializer,
                              ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT32,
                              /*shape=*/{0}, /*data=*/{});

  // Add Reshape node.
  constexpr base::cstring_view output = "output";
  std::array<const char*, 2> reshape_inputs = {input.c_str(),
                                               new_shape_initializer.c_str()};
  std::array<const char*, 1> reshape_outputs = {output.c_str()};
  model_editor.AddNode(
      /*op_type=*/"Reshape", /*node_name=*/"reshape", reshape_inputs,
      reshape_outputs);

  // Add an output.
  auto output_desc =
      OperandDescriptor::CreateForDeserialization(OperandDataType::kInt32, {});
  ASSERT_TRUE(output_desc.has_value());
  model_editor.AddOutput(output, output_desc.value());

  std::unique_ptr<ModelEditor::ModelInfo> model_info =
      model_editor.BuildAndTakeModelInfo();
  ASSERT_NE(model_info, nullptr);

  EXPECT_EQ(model_info->external_data.size(), 0u);
  EXPECT_TRUE(model_info->model.is_valid());
}

}  // namespace webnn::ort
