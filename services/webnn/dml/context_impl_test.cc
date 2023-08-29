// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/webnn_context_provider_impl.h"

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/webnn/dml/test_base.h"
#include "services/webnn/public/mojom/webnn_graph.mojom.h"
#include "services/webnn/public/mojom/webnn_service.mojom.h"
#include "services/webnn/webnn_test_utils.h"

namespace webnn::dml {

class WebNNContextDMLImplTest : public TestBase {
 private:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(WebNNContextDMLImplTest, CreateGraphImplTest) {
  mojo::Remote<mojom::WebNNContextProvider> provider_remote;
  mojo::Remote<mojom::WebNNContext> webnn_context_remote;
  mojom::CreateContextResult webnn_context_result =
      mojom::CreateContextResult::kOk;

  WebNNContextProviderImpl::Create(
      provider_remote.BindNewPipeAndPassReceiver());

  // Create the dml::ContextImpl through context provider.
  bool is_callback_called = false;
  base::RunLoop run_loop_create_context;
  auto options = mojom::CreateContextOptions::New();
  provider_remote->CreateWebNNContext(
      std::move(options),
      base::BindLambdaForTesting(
          [&](mojom::CreateContextResult result,
              mojo::PendingRemote<mojom::WebNNContext> remote) {
            if (result == mojom::CreateContextResult::kOk) {
              webnn_context_remote.Bind(std::move(remote));
            }
            webnn_context_result = result;
            is_callback_called = true;
            run_loop_create_context.Quit();
          }));
  run_loop_create_context.Run();
  EXPECT_TRUE(is_callback_called);

  // Remote is null when result is Unsupported which cannot be bound.
  SKIP_TEST_IF(webnn_context_result ==
               mojom::CreateContextResult::kNotSupported);

  EXPECT_EQ(webnn_context_result, mojom::CreateContextResult::kOk);
  ASSERT_TRUE(webnn_context_remote.is_bound());

  // Build a simple graph with relu operator.
  GraphInfoBuilder builder;
  uint64_t input_operand_id = builder.BuildInput(
      "input", {1, 2, 3, 4}, mojom::Operand::DataType::kFloat32);
  uint64_t output_operand_id = builder.BuildOutput(
      "output", {1, 2, 3, 4}, mojom::Operand::DataType::kFloat32);
  builder.BuildOperator(mojom::Operator::Kind::kRelu, {input_operand_id},
                        {output_operand_id});

  // The dml::GraphImpl should be built successfully.
  base::RunLoop run_loop_create_graph;
  is_callback_called = false;
  webnn_context_remote->CreateGraph(
      builder.CloneGraphInfo(),
      base::BindLambdaForTesting(
          [&](mojo::PendingRemote<mojom::WebNNGraph> remote) {
            EXPECT_TRUE(remote.is_valid());
            is_callback_called = true;
            run_loop_create_graph.Quit();
          }));
  run_loop_create_graph.Run();
  EXPECT_TRUE(is_callback_called);

  // Ensure `WebNNContextImpl::OnConnectionError()` is called and
  // `WebNNContextImpl` is released.
  webnn_context_remote.reset();
  base::RunLoop().RunUntilIdle();
}

}  // namespace webnn::dml
