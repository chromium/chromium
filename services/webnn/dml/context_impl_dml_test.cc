// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/webnn/dml/adapter.h"
#include "services/webnn/dml/test_base.h"
#include "services/webnn/error.h"
#include "services/webnn/public/mojom/features.mojom-features.h"
#include "services/webnn/public/mojom/webnn_context.mojom.h"
#include "services/webnn/public/mojom/webnn_context_provider.mojom.h"
#include "services/webnn/public/mojom/webnn_error.mojom.h"
#include "services/webnn/public/mojom/webnn_graph.mojom.h"
#include "services/webnn/public/mojom/webnn_graph_builder.mojom.h"
#include "services/webnn/webnn_context_provider_impl.h"
#include "services/webnn/webnn_test_utils.h"

namespace webnn::dml {

class WebNNContextDMLImplTest : public TestBase {
 public:
  void SetUp() override;

 protected:
  WebNNContextDMLImplTest()
      : scoped_feature_list_(
            webnn::mojom::features::kWebMachineLearningNeuralNetwork) {}
  ~WebNNContextDMLImplTest() override = default;

  bool CreateWebNNContext(
      mojo::Remote<mojom::WebNNContextProvider>& webnn_provider_remote,
      mojo::Remote<mojom::WebNNContext>& webnn_context_remote) {
    bool is_platform_supported = true;

    // Create the ContextImplDml through context provider.
    base::test::TestFuture<mojom::CreateContextResultPtr> create_context_future;
    webnn_provider_remote->CreateWebNNContext(
        mojom::CreateContextOptions::New(
            mojom::CreateContextOptions::Device::kGpu,
            mojom::CreateContextOptions::PowerPreference::kDefault,
            /*thread_count_hint=*/0),
        create_context_future.GetCallback());
    auto create_context_result = create_context_future.Take();
    if (create_context_result->is_success()) {
      webnn_context_remote.Bind(
          std::move(create_context_result->get_success()->context_remote));
    } else {
      is_platform_supported = create_context_result->get_error()->code !=
                              mojom::Error::Code::kNotSupportedError;
    }
    return is_platform_supported;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

void WebNNContextDMLImplTest::SetUp() {
  SKIP_TEST_IF(!UseGPUInTests());
  Adapter::EnableDebugLayerForTesting();
  auto adapter_creation_result = Adapter::GetGpuInstanceForTesting();
  // If the adapter creation result has no value, it's most likely because
  // platform functions were not properly loaded.
  SKIP_TEST_IF(!adapter_creation_result.has_value());
  auto adapter = adapter_creation_result.value();
  // Graph compilation relies on IDMLDevice1::CompileGraph introduced in
  // DirectML version 1.2 or DML_FEATURE_LEVEL_2_1, so skip the tests if the
  // DirectML version doesn't support this feature.
  SKIP_TEST_IF(!adapter->IsDMLDeviceCompileGraphSupportedForTesting());
}

TEST_F(WebNNContextDMLImplTest, CreateGraphImplTest) {
  mojo::Remote<mojom::WebNNContextProvider> webnn_provider_remote;
  WebNNContextProviderImpl::CreateForTesting(
      webnn_provider_remote.BindNewPipeAndPassReceiver());

  mojo::Remote<mojom::WebNNContext> webnn_context_remote;
  SKIP_TEST_IF(
      !CreateWebNNContext(webnn_provider_remote, webnn_context_remote));

  ASSERT_TRUE(webnn_context_remote.is_bound());

  // Build a simple graph with relu operator.
  GraphInfoBuilder builder;
  uint64_t input_operand_id =
      builder.BuildInput("input", {1, 2, 3, 4}, OperandDataType::kFloat32);
  uint64_t output_operand_id =
      builder.BuildOutput("output", {1, 2, 3, 4}, OperandDataType::kFloat32);
  builder.BuildRelu(input_operand_id, output_operand_id);

  mojo::AssociatedRemote<mojom::WebNNGraphBuilder> graph_builder_remote;
  webnn_context_remote->CreateGraphBuilder(
      graph_builder_remote.BindNewEndpointAndPassReceiver());

  // The GraphImplDml should be built successfully.
  base::test::TestFuture<mojom::CreateGraphResultPtr> create_graph_future;
  graph_builder_remote->CreateGraph(builder.TakeGraphInfo(),
                                    create_graph_future.GetCallback());
  mojom::CreateGraphResultPtr create_graph_result = create_graph_future.Take();
  EXPECT_TRUE(create_graph_result->is_graph_remote());

  // Reset the remote to ensure `WebNNGraphImpl` is released.
  if (create_graph_result->is_graph_remote()) {
    create_graph_result->get_graph_remote().reset();
  }

  // Ensure `WebNNContextImpl::OnConnectionError()` is called and
  // `WebNNContextImpl` is released.
  webnn_context_remote.reset();
  webnn_provider_remote.reset();

  base::RunLoop().RunUntilIdle();
}

}  // namespace webnn::dml
