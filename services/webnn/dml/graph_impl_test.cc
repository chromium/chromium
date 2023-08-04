// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <DirectML.h>
#include <wrl.h>

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "services/webnn/dml/adapter.h"
#include "services/webnn/dml/command_queue.h"
#include "services/webnn/dml/command_recorder.h"
#include "services/webnn/dml/graph_impl.h"
#include "services/webnn/dml/test_base.h"
#include "services/webnn/public/mojom/webnn_graph.mojom.h"
#include "services/webnn/webnn_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace webnn::dml {

class WebNNGraphDMLImplTest : public TestBase {
 public:
  void SetUp() override;

  bool CreateAndBuildGraph(const mojom::GraphInfoPtr& graph_info);

 protected:
  bool is_compile_graph_supported_ = true;
  base::test::SingleThreadTaskEnvironment task_environment_;
  scoped_refptr<Adapter> adapter_;
};

void WebNNGraphDMLImplTest::SetUp() {
  SKIP_TEST_IF(!UseGPUInTests());
  ASSERT_TRUE(InitializeGLDisplay());
  adapter_ = Adapter::GetInstance();
  ASSERT_NE(adapter_.get(), nullptr);
  IDMLDevice* dml_device = adapter_->dml_device();
  ASSERT_NE(dml_device, nullptr);
  // IDMLDevice1::CompileGraph will rely on IDMLDevice1 interface.
  ComPtr<IDMLDevice1> dml_device1;
  HRESULT hr = dml_device->QueryInterface(IID_PPV_ARGS(&dml_device1));
  if (FAILED(hr)) {
    DLOG(WARNING) << "Failed to query dml device1 : "
                  << logging::SystemErrorCodeToString(hr);
    is_compile_graph_supported_ = false;
  }
}

bool WebNNGraphDMLImplTest::CreateAndBuildGraph(
    const mojom::GraphInfoPtr& graph_info) {
  base::RunLoop build_graph_run_loop;
  bool result = false;
  GraphImpl::CreateAndBuild(
      adapter_->command_queue(), adapter_->dml_device(), graph_info,
      base::BindLambdaForTesting(
          [&](mojo::PendingRemote<mojom::WebNNGraph> remote) {
            result = remote.is_valid();
            build_graph_run_loop.Quit();
          }));
  build_graph_run_loop.Run();
  return result;
}

// Test building a DML graph with single operator relu.
TEST_F(WebNNGraphDMLImplTest, BuildSingleOperatorRelu) {
  // Build the mojom graph info.
  GraphInfoBuilder builder;
  uint64_t input_operand_id = builder.BuildInput(
      "input", {1, 2, 3, 4}, mojom::Operand::DataType::kFloat32);
  uint64_t output_operand_id = builder.BuildOutput(
      "output", {1, 2, 3, 4}, mojom::Operand::DataType::kFloat32);
  builder.BuildOperator(mojom::Operator::Kind::kRelu, {input_operand_id},
                        {output_operand_id});
  EXPECT_TRUE(CreateAndBuildGraph(builder.GetGraphInfo()));
}

// Test building a DML graph with two relu operators.
//    [input]
//       |
//      relu1
//       |
//      relu2
TEST_F(WebNNGraphDMLImplTest, BuildGraphWithTwoRelu) {
  SKIP_TEST_IF(!is_compile_graph_supported_);
  // Build the mojom graph info.
  GraphInfoBuilder builder;
  uint64_t input_operand_id = builder.BuildInput(
      "input", {1, 2, 3, 4}, mojom::Operand::DataType::kFloat32);
  uint64_t relu1_output_id =
      builder.BuildOperand({1, 2, 3, 4}, mojom::Operand::DataType::kFloat32);
  builder.BuildOperator(mojom::Operator::Kind::kRelu, {input_operand_id},
                        {relu1_output_id});
  uint64_t output_operand_id = builder.BuildOutput(
      "output", {1, 2, 3, 4}, mojom::Operand::DataType::kFloat32);
  builder.BuildOperator(mojom::Operator::Kind::kRelu, {relu1_output_id},
                        {output_operand_id});
  EXPECT_TRUE(CreateAndBuildGraph(builder.GetGraphInfo()));
}

}  // namespace webnn::dml
