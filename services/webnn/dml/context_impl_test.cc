// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/test_future.h"
#include "services/webnn/webnn_context_provider_impl.h"

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/ml/webnn/features.mojom-features.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/webnn/dml/test_base.h"
#include "services/webnn/public/mojom/webnn_context_provider.mojom.h"
#include "services/webnn/public/mojom/webnn_error.mojom.h"
#include "services/webnn/public/mojom/webnn_graph.mojom.h"
#include "services/webnn/webnn_test_utils.h"

namespace webnn::dml {

class WebNNContextDMLImplTest : public TestBase {
 private:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(WebNNContextDMLImplTest, CreateGraphImplTest) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      webnn::mojom::features::kWebMachineLearningNeuralNetwork);

  mojo::Remote<mojom::WebNNContextProvider> provider_remote;
  mojo::Remote<mojom::WebNNContext> webnn_context_remote;
  bool is_platform_supported = true;

  WebNNContextProviderImpl::Create(
      provider_remote.BindNewPipeAndPassReceiver());

  // Create the dml::ContextImpl through context provider.
  base::test::TestFuture<mojom::CreateContextResultPtr> create_context_future;
  provider_remote->CreateWebNNContext(mojom::CreateContextOptions::New(),
                                      create_context_future.GetCallback());
  mojom::CreateContextResultPtr create_context_result =
      create_context_future.Take();
  if (create_context_result->is_context_remote()) {
    webnn_context_remote.Bind(
        std::move(create_context_result->get_context_remote()));
  } else {
    is_platform_supported = create_context_result->get_error()->code !=
                            mojom::Error::Code::kNotSupportedError;
  }

  // Remote is null when the platform is not supported which cannot be
  // bound.
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
  base::test::TestFuture<mojom::CreateGraphResultPtr> create_graph_future;
  webnn_context_remote->CreateGraph(builder.CloneGraphInfo(),
                                    create_graph_future.GetCallback());
  mojom::CreateGraphResultPtr create_graph_result = create_graph_future.Take();
  EXPECT_TRUE(create_graph_result->is_graph_remote());

  // Ensure `WebNNContextImpl::OnConnectionError()` is called and
  // `WebNNContextImpl` is released.
  webnn_context_remote.reset();
  base::RunLoop().RunUntilIdle();
}

}  // namespace webnn::dml
