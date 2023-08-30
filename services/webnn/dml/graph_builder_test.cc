// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <DirectML.h>
#include <d3d11.h>
#include <wrl.h>

#include "base/logging.h"
#include "services/webnn/dml/adapter.h"
#include "services/webnn/dml/graph_builder.h"
#include "services/webnn/dml/tensor_desc.h"
#include "services/webnn/dml/test_base.h"
#include "services/webnn/dml/utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/gl_angle_util_win.h"

namespace webnn::dml {

class WebNNGraphBuilderTest : public TestBase {
 public:
  void SetUp() override;

 protected:
  ComPtr<IDMLDevice> dml_device_;
  bool is_compile_graph_supported_ = true;
};

void WebNNGraphBuilderTest::SetUp() {
  SKIP_TEST_IF(!UseGPUInTests());
  ASSERT_TRUE(InitializeGLDisplay());
  Adapter::EnableDebugLayerForTesting();
  scoped_refptr<Adapter> adapter = Adapter::GetInstanceForTesting();
  ASSERT_NE(adapter.get(), nullptr);
  dml_device_ = adapter->dml_device();
  ASSERT_NE(dml_device_.Get(), nullptr);
  is_compile_graph_supported_ =
      adapter->IsDMLDeviceCompileGraphSupportedForTesting();
}

// Test creating an invalid operator node with inconsistent tensor dimensions.
TEST_F(WebNNGraphBuilderTest, CreateInvalidOperator) {
  GraphBuilder graph_builder(dml_device_);

  TensorDesc input_tensor_desc(DML_TENSOR_DATA_TYPE_FLOAT32, {1, 2, 3, 4});
  TensorDesc output_tensor_desc(DML_TENSOR_DATA_TYPE_FLOAT32, {1, 2, 3});
  NodeInfo input_node = graph_builder.CreateInputNode();
  ASSERT_EQ(input_node.type, NodeInfo::Type::kInput);
  NodeOutputInfo input =
      graph_builder.CreateNodeOutput(input_node, input_tensor_desc);

  DML_ACTIVATION_RELU_OPERATOR_DESC invalid_operator_desc{
      .InputTensor = &input_tensor_desc.GetDMLTensorDesc(),
      .OutputTensor = &output_tensor_desc.GetDMLTensorDesc()};

  NodeInfo invalid_node = graph_builder.CreateOperatorNode(
      DML_OPERATOR_ACTIVATION_RELU, &invalid_operator_desc, {input});
  EXPECT_EQ(invalid_node.type, NodeInfo::Type::kInvalid);
}

// Test building a DML graph with single operator relu.
TEST_F(WebNNGraphBuilderTest, BuildSingleOperatorRelu) {
  GraphBuilder graph_builder(dml_device_);

  TensorDesc input_tensor_desc(DML_TENSOR_DATA_TYPE_FLOAT32, {1, 2, 3, 4});
  NodeInfo input_node = graph_builder.CreateInputNode();
  EXPECT_EQ(input_node.type, NodeInfo::Type::kInput);
  NodeOutputInfo input =
      graph_builder.CreateNodeOutput(input_node, input_tensor_desc);

  DML_ACTIVATION_RELU_OPERATOR_DESC relu_operator_desc{
      .InputTensor = &input_tensor_desc.GetDMLTensorDesc(),
      .OutputTensor = &input_tensor_desc.GetDMLTensorDesc()};

  NodeInfo relu_node = graph_builder.CreateOperatorNode(
      DML_OPERATOR_ACTIVATION_RELU, &relu_operator_desc, {input});
  EXPECT_EQ(relu_node.type, NodeInfo::Type::kOperator);
  NodeOutputInfo output =
      graph_builder.CreateNodeOutput(relu_node, input_tensor_desc);

  EXPECT_NE(graph_builder.Compile({output}, DML_EXECUTION_FLAG_NONE).Get(),
            nullptr);
}

// Test building a DML graph with single operator conv2d which has multiple
// inputs.
TEST_F(WebNNGraphBuilderTest, BuildSingleOperatorConv2d) {
  GraphBuilder graph_builder(dml_device_);

  TensorDesc input_tensor_desc(DML_TENSOR_DATA_TYPE_FLOAT32, {1, 1, 3, 3});
  NodeInfo input_node = graph_builder.CreateInputNode();
  EXPECT_EQ(input_node.type, NodeInfo::Type::kInput);
  NodeOutputInfo input =
      graph_builder.CreateNodeOutput(input_node, input_tensor_desc);

  TensorDesc filter_tensor_desc(DML_TENSOR_DATA_TYPE_FLOAT32,
                                DML_TENSOR_FLAG_OWNED_BY_DML, {1, 1, 2, 2});
  NodeInfo filter_node = graph_builder.CreateInputNode();
  EXPECT_EQ(filter_node.type, NodeInfo::Type::kInput);
  NodeOutputInfo filter =
      graph_builder.CreateNodeOutput(filter_node, filter_tensor_desc);

  TensorDesc output_tensor_desc(DML_TENSOR_DATA_TYPE_FLOAT32, {1, 1, 2, 2});
  const std::vector<uint32_t> strides({1, 1});
  const std::vector<uint32_t> dilations({1, 1});
  const std::vector<uint32_t> start_padding({0, 0});
  const std::vector<uint32_t> end_padding({0, 0});
  const std::vector<uint32_t> output_padding({0, 0});
  DML_CONVOLUTION_OPERATOR_DESC conv_operator_desc{
      .InputTensor = &input_tensor_desc.GetDMLTensorDesc(),
      .FilterTensor = &filter_tensor_desc.GetDMLTensorDesc(),
      .BiasTensor = nullptr,
      .OutputTensor = &output_tensor_desc.GetDMLTensorDesc(),
      .Mode = DML_CONVOLUTION_MODE_CROSS_CORRELATION,
      .Direction = DML_CONVOLUTION_DIRECTION_FORWARD,
      .DimensionCount = 2,
      .Strides = strides.data(),
      .Dilations = dilations.data(),
      .StartPadding = start_padding.data(),
      .EndPadding = end_padding.data(),
      .OutputPadding = output_padding.data(),
      .GroupCount = 1,
      .FusedActivation = nullptr};

  NodeInfo conv_node = graph_builder.CreateOperatorNode(
      DML_OPERATOR_CONVOLUTION, &conv_operator_desc, {input, filter});
  EXPECT_EQ(conv_node.type, NodeInfo::Type::kOperator);

  NodeOutputInfo output =
      graph_builder.CreateNodeOutput(conv_node, output_tensor_desc);

  EXPECT_NE(graph_builder.Compile({output}, DML_EXECUTION_FLAG_NONE).Get(),
            nullptr);
}

// Test building a DML graph with single operator split which has multiple
// outputs.
TEST_F(WebNNGraphBuilderTest, BuildSingleOperatorSplit) {
  GraphBuilder graph_builder(dml_device_);

  TensorDesc input_tensor_desc(DML_TENSOR_DATA_TYPE_FLOAT32, {1, 2, 6, 4});
  NodeInfo input_node = graph_builder.CreateInputNode();
  EXPECT_EQ(input_node.type, NodeInfo::Type::kInput);
  NodeOutputInfo input =
      graph_builder.CreateNodeOutput(input_node, input_tensor_desc);

  TensorDesc output_tensor_desc0(DML_TENSOR_DATA_TYPE_FLOAT32,
                                 std::vector<uint32_t>{1, 2, 1, 4});
  TensorDesc output_tensor_desc1(DML_TENSOR_DATA_TYPE_FLOAT32,
                                 std::vector<uint32_t>{1, 2, 2, 4});
  TensorDesc output_tensor_desc2(DML_TENSOR_DATA_TYPE_FLOAT32,
                                 std::vector<uint32_t>{1, 2, 3, 4});
  std::vector<DML_TENSOR_DESC> output_tensor_descs;
  uint32_t output_count = 3;
  output_tensor_descs.reserve(output_count);
  output_tensor_descs.push_back(output_tensor_desc0.GetDMLTensorDesc());
  output_tensor_descs.push_back(output_tensor_desc1.GetDMLTensorDesc());
  output_tensor_descs.push_back(output_tensor_desc2.GetDMLTensorDesc());

  DML_SPLIT_OPERATOR_DESC split_operator_desc = {
      .InputTensor = &input_tensor_desc.GetDMLTensorDesc(),
      .OutputCount = output_count,
      .OutputTensors = output_tensor_descs.data(),
      .Axis = 2};

  NodeInfo split_node = graph_builder.CreateOperatorNode(
      DML_OPERATOR_SPLIT, &split_operator_desc, {input});
  EXPECT_EQ(split_node.type, NodeInfo::Type::kOperator);
  NodeOutputInfo output0 =
      graph_builder.CreateNodeOutput(split_node, output_tensor_desc0, 0);
  NodeOutputInfo output1 =
      graph_builder.CreateNodeOutput(split_node, output_tensor_desc1, 1);
  NodeOutputInfo output2 =
      graph_builder.CreateNodeOutput(split_node, output_tensor_desc2, 2);

  EXPECT_NE(graph_builder
                .Compile({output0, output1, output2}, DML_EXECUTION_FLAG_NONE)
                .Get(),
            nullptr);
}

// Test building a DML graph with two operators: relu and conv2d.
//   [input] [filter]
//       |      /
//      relu   /
//        \   /
//       conv2d
TEST_F(WebNNGraphBuilderTest, BuildGraphWithReluAndConv2d) {
  SKIP_TEST_IF(!is_compile_graph_supported_);
  GraphBuilder graph_builder(dml_device_);

  TensorDesc input_tensor_desc(DML_TENSOR_DATA_TYPE_FLOAT32, {1, 1, 3, 3});
  NodeInfo input_node = graph_builder.CreateInputNode();
  EXPECT_EQ(input_node.type, NodeInfo::Type::kInput);
  NodeOutputInfo input =
      graph_builder.CreateNodeOutput(input_node, input_tensor_desc);

  TensorDesc filter_tensor_desc(DML_TENSOR_DATA_TYPE_FLOAT32,
                                DML_TENSOR_FLAG_OWNED_BY_DML, {1, 1, 2, 2});
  NodeInfo filter_node = graph_builder.CreateInputNode();
  EXPECT_EQ(filter_node.type, NodeInfo::Type::kInput);
  NodeOutputInfo filter =
      graph_builder.CreateNodeOutput(filter_node, filter_tensor_desc);

  DML_ACTIVATION_RELU_OPERATOR_DESC relu_operator_desc{
      .InputTensor = &input_tensor_desc.GetDMLTensorDesc(),
      .OutputTensor = &input_tensor_desc.GetDMLTensorDesc()};

  NodeInfo relu_node = graph_builder.CreateOperatorNode(
      DML_OPERATOR_ACTIVATION_RELU, &relu_operator_desc, {input});
  EXPECT_EQ(relu_node.type, NodeInfo::Type::kOperator);
  NodeOutputInfo relu_output =
      graph_builder.CreateNodeOutput(relu_node, input_tensor_desc);

  TensorDesc output_tensor_desc(DML_TENSOR_DATA_TYPE_FLOAT32, {1, 1, 2, 2});
  const std::vector<uint32_t> strides({1, 1});
  const std::vector<uint32_t> dilations({1, 1});
  const std::vector<uint32_t> start_padding({0, 0});
  const std::vector<uint32_t> end_padding({0, 0});
  const std::vector<uint32_t> output_padding({0, 0});
  DML_CONVOLUTION_OPERATOR_DESC conv_operator_desc{
      .InputTensor = &input_tensor_desc.GetDMLTensorDesc(),
      .FilterTensor = &filter_tensor_desc.GetDMLTensorDesc(),
      .BiasTensor = nullptr,
      .OutputTensor = &output_tensor_desc.GetDMLTensorDesc(),
      .Mode = DML_CONVOLUTION_MODE_CROSS_CORRELATION,
      .Direction = DML_CONVOLUTION_DIRECTION_FORWARD,
      .DimensionCount = 2,
      .Strides = strides.data(),
      .Dilations = dilations.data(),
      .StartPadding = start_padding.data(),
      .EndPadding = end_padding.data(),
      .OutputPadding = output_padding.data(),
      .GroupCount = 1,
      .FusedActivation = nullptr};

  NodeInfo conv_node = graph_builder.CreateOperatorNode(
      DML_OPERATOR_CONVOLUTION, &conv_operator_desc, {relu_output, filter});
  EXPECT_EQ(conv_node.type, NodeInfo::Type::kOperator);

  NodeOutputInfo output =
      graph_builder.CreateNodeOutput(conv_node, output_tensor_desc);

  EXPECT_NE(graph_builder.Compile({output}, DML_EXECUTION_FLAG_NONE).Get(),
            nullptr);
}

}  // namespace webnn::dml
