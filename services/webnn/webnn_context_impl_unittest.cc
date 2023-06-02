// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/webnn_context_provider_impl.h"

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/webnn/public/mojom/webnn_graph.mojom.h"
#include "services/webnn/public/mojom/webnn_service.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace webnn {

namespace {

mojom::GraphInfoPtr BuildSimpleGraph() {
  auto graph_info = mojom::GraphInfo::New();
  size_t operand_id = 0;
  // Create left mojo operand.
  auto lhs_operand = mojom::Operand::New();
  lhs_operand->data_type = mojom::Operand::DataType::kFloat32;
  lhs_operand->dimensions = std::vector<uint32_t>(3, 2);
  lhs_operand->name = std::move("lhs");
  lhs_operand->kind = mojom::Operand::Kind::kInput;
  size_t lhs_operand_id = ++operand_id;
  graph_info->id_to_operand_map[lhs_operand_id] = std::move(lhs_operand);
  graph_info->input_operands.push_back(lhs_operand_id);

  // Create right mojo operand.
  auto rhs_operand = mojom::Operand::New();
  rhs_operand->data_type = mojom::Operand::DataType::kFloat32;
  rhs_operand->dimensions = std::vector<uint32_t>(3, 2);
  rhs_operand->name = std::move("rhs");
  rhs_operand->kind = mojom::Operand::Kind::kInput;
  size_t rhs_operand_id = ++operand_id;
  graph_info->id_to_operand_map[rhs_operand_id] = std::move(rhs_operand);
  graph_info->input_operands.push_back(rhs_operand_id);

  // Create output mojo operand.
  auto output_operand = mojom::Operand::New();
  output_operand->data_type = mojom::Operand::DataType::kFloat32;
  output_operand->dimensions = std::vector<uint32_t>(3, 2);
  output_operand->name = std::move("output");
  output_operand->kind = mojom::Operand::Kind::kOutput;
  size_t output_operand_id = ++operand_id;
  graph_info->id_to_operand_map[output_operand_id] = std::move(output_operand);
  graph_info->output_operands.push_back(output_operand_id);

  // Create the add operation.
  auto operation = mojom::Operator::New();
  operation->kind = mojom::Operator::Kind::kAdd;
  operation->input_operands = {lhs_operand_id, rhs_operand_id};
  operation->output_operands = {output_operand_id};
  graph_info->operators.emplace_back(std::move(operation));
  return graph_info;
}

}  // namespace

class WebNNContextImplTest : public testing::Test {
 public:
  WebNNContextImplTest(const WebNNContextImplTest&) = delete;
  WebNNContextImplTest& operator=(const WebNNContextImplTest&) = delete;

 protected:
  WebNNContextImplTest() = default;
  ~WebNNContextImplTest() override = default;

 private:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(WebNNContextImplTest, CreateWebNNGraphTest) {
  mojo::Remote<mojom::WebNNContextProvider> provider_remote;
  mojo::Remote<mojom::WebNNContext> webnn_context_remote;

  WebNNContextProviderImpl::Create(
      provider_remote.BindNewPipeAndPassReceiver());

  bool is_callback_called = false;
  base::RunLoop run_loop_create_context;
  auto options = mojom::CreateContextOptions::New();
  provider_remote->CreateWebNNContext(
      std::move(options),
      base::BindLambdaForTesting(
          [&](mojom::CreateContextResult result,
              mojo::PendingRemote<mojom::WebNNContext> remote) {
#if BUILDFLAG(IS_WIN)
            EXPECT_EQ(result, mojom::CreateContextResult::kOk);
            webnn_context_remote.Bind(std::move(remote));
#else
            EXPECT_EQ(result, mojom::CreateContextResult::kNotSupported);
#endif
            is_callback_called = true;
            run_loop_create_context.Quit();
          }));
  run_loop_create_context.Run();
  EXPECT_TRUE(is_callback_called);

  if (!webnn_context_remote.is_bound()) {
    // Don't continue testing for unsupported platforms.
    return;
  }

  base::RunLoop run_loop_create_graph;
  is_callback_called = false;
  webnn_context_remote->CreateGraph(
      BuildSimpleGraph(),
      base::BindLambdaForTesting(
          [&](mojo::PendingRemote<mojom::WebNNGraph> remote) {
            EXPECT_TRUE(remote.is_valid());
            is_callback_called = true;
            run_loop_create_graph.Quit();
          }));
  run_loop_create_graph.Run();
  EXPECT_TRUE(is_callback_called);
}

}  // namespace webnn
