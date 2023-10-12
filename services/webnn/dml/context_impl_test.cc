// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/webnn_context_provider_impl.h"

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/webnn/dml/test_base.h"
#include "services/webnn/public/mojom/webnn_context_provider.mojom.h"
#include "services/webnn/public/mojom/webnn_graph.mojom.h"
#include "services/webnn/webnn_test_utils.h"

namespace webnn::dml {

class WebNNContextDMLImplTest : public TestBase {
 private:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(WebNNContextDMLImplTest, CreateGraphImplTest) {
  mojo::Remote<mojom::WebNNContextProvider> provider_remote;
  mojo::Remote<mojom::WebNNContext> webnn_context_remote;
  bool is_platform_supported = true;

  WebNNContextProviderImpl::Create(
      provider_remote.BindNewPipeAndPassReceiver());

  // Create the dml::ContextImpl through context provider.
  bool is_callback_called = false;
  base::RunLoop run_loop_create_context;
  auto options = mojom::CreateContextOptions::New();
  provider_remote->CreateWebNNContext(
      std::move(options),
      base::BindLambdaForTesting(
          [&](mojom::CreateContextResultPtr create_context_result) {
            if (create_context_result->is_context_remote()) {
              webnn_context_remote.Bind(
                  std::move(create_context_result->get_context_remote()));
            } else {
              is_platform_supported =
                  create_context_result->get_error()->error_code !=
                  mojom::Error::Code::kNotSupportedError;
            }
            is_callback_called = true;
            run_loop_create_context.Quit();
          }));
  run_loop_create_context.Run();
  EXPECT_TRUE(is_callback_called);

  // Remote is null when the platform is not supported which cannot be bound.
  SKIP_TEST_IF(!is_platform_supported);

  ASSERT_TRUE(webnn_context_remote.is_bound());

  // Build a simple graph with relu operator.
  GraphInfoBuilder builder;
  uint64_t input_operand_id = builder.BuildInput(
      "input", {1, 2, 3, 4}, mojom::Operand::DataType::kFloat32);
  uint64_t output_operand_id = builder.BuildOutput(
      "output", {1, 2, 3, 4}, mojom::Operand::DataType::kFloat32);
  builder.BuildRelu(input_operand_id, output_operand_id);

  // The dml::GraphImpl should be built successfully.
  base::RunLoop run_loop_create_graph;
  is_callback_called = false;
  webnn_context_remote->CreateGraph(
      builder.CloneGraphInfo(),
      base::BindLambdaForTesting(
          [&](mojom::CreateGraphResultPtr create_graph_result) {
            EXPECT_TRUE(create_graph_result->is_graph_remote());
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
