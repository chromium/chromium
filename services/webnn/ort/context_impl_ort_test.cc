// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/ort/context_impl_ort.h"

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/webnn/error.h"
#include "services/webnn/public/mojom/features.mojom-features.h"
#include "services/webnn/public/mojom/webnn_context.mojom.h"
#include "services/webnn/public/mojom/webnn_context_provider.mojom.h"
#include "services/webnn/public/mojom/webnn_error.mojom.h"
#include "services/webnn/public/mojom/webnn_graph.mojom.h"
#include "services/webnn/public/mojom/webnn_graph_builder.mojom.h"
#include "services/webnn/webnn_context_impl.h"
#include "services/webnn/webnn_context_provider_impl.h"
#include "services/webnn/webnn_graph_impl.h"
#include "services/webnn/webnn_tensor_impl.h"
#include "services/webnn/webnn_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace webnn::ort {

namespace {

template <typename T>
mojo_base::BigBuffer VectorToBigBuffer(const std::vector<T>& data) {
  return mojo_base::BigBuffer(
      base::as_bytes(base::span(data.data(), data.size())));
}

template <typename T>
std::vector<T> BigBufferToVector(mojo_base::BigBuffer big_buffer) {
  std::vector<T> data(big_buffer.size() / sizeof(T));
  memcpy(data.data(), big_buffer.data(), big_buffer.size());
  return data;
}

struct CreateTensorSuccess {
  mojo::AssociatedRemote<mojom::WebNNTensor> webnn_tensor_remote;
  blink::WebNNTensorToken webnn_tensor_handle;
};

CreateTensorSuccess CreateWebNNTensor(
    mojo::Remote<mojom::WebNNContext>& webnn_context_remote,
    OperandDataType data_type,
    std::vector<uint32_t> shape) {
  base::test::TestFuture<mojom::CreateTensorResultPtr> create_tensor_future;
  webnn_context_remote->CreateTensor(
      mojom::TensorInfo::New(
          *OperandDescriptor::Create(data_type, shape),
          MLTensorUsage{MLTensorUsageFlags::kWrite, MLTensorUsageFlags::kRead}),
      create_tensor_future.GetCallback());
  mojom::CreateTensorResultPtr create_tensor_result =
      create_tensor_future.Take();
  mojo::AssociatedRemote<mojom::WebNNTensor> webnn_tensor_remote;
  webnn_tensor_remote.Bind(
      std::move(create_tensor_result->get_success()->tensor_remote));
  return CreateTensorSuccess{
      std::move(webnn_tensor_remote),
      std::move(create_tensor_result->get_success()->tensor_handle)};
}

}  // namespace

class WebNNContextOrtImplTest : public testing::Test {
 public:
  void SetUp() override;

 protected:
  WebNNContextOrtImplTest()
      : scoped_feature_list_(
            webnn::mojom::features::kWebMachineLearningNeuralNetwork) {}
  ~WebNNContextOrtImplTest() override = default;

  bool CreateWebNNContext() {
    bool is_platform_supported = true;

    // Create the ContextImplOrt through context provider.
    base::test::TestFuture<mojom::CreateContextResultPtr> create_context_future;
    webnn_provider_remote_->CreateWebNNContext(
        mojom::CreateContextOptions::New(
            mojom::CreateContextOptions::Device::kCpu,
            mojom::CreateContextOptions::PowerPreference::kLowPower),
        create_context_future.GetCallback());
    auto create_context_result = create_context_future.Take();
    if (create_context_result->is_success()) {
      webnn_context_remote_.Bind(
          std::move(create_context_result->get_success()->context_remote));
    } else {
      is_platform_supported = create_context_result->get_error()->code !=
                              mojom::Error::Code::kNotSupportedError;
    }
    return is_platform_supported;
  }

 protected:
  mojo::Remote<mojom::WebNNContextProvider> webnn_provider_remote_;
  mojo::Remote<mojom::WebNNContext> webnn_context_remote_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::TaskEnvironment task_environment_;
};

void WebNNContextOrtImplTest::SetUp() {}

void VerifyFloatDataIsEqual(base::span<const float> data,
                            base::span<const float> expected_data) {
  EXPECT_THAT(data, testing::Pointwise(testing::FloatEq(), expected_data));
}

TEST_F(WebNNContextOrtImplTest, CreateWriteReadTensorTest) {
  WebNNContextProviderImpl::CreateForTesting(
      webnn_provider_remote_.BindNewPipeAndPassReceiver());
  ASSERT_TRUE(CreateWebNNContext());

  ASSERT_TRUE(webnn_context_remote_.is_bound());

  OperandDataType data_type = OperandDataType::kFloat32;
  std::vector<uint32_t> shape = {1, 1, 2, 4};
  std::vector<float> input_data({-1.0, -2.0, -3.0, -4.0, 5.0, 6.0, 7.0, 8.0});

  // Create tensor
  CreateTensorSuccess input_tensor =
      CreateWebNNTensor(webnn_context_remote_, data_type, shape);

  // Write tensor
  input_tensor.webnn_tensor_remote->WriteTensor(VectorToBigBuffer(input_data));

  // Read tensor
  base::test::TestFuture<mojom::ReadTensorResultPtr> future;
  input_tensor.webnn_tensor_remote->ReadTensor(future.GetCallback());
  mojom::ReadTensorResultPtr result = future.Take();
  ASSERT_FALSE(result->is_error());

  std::vector<float> output_data =
      BigBufferToVector<float>(std::move(result->get_buffer()));
  VerifyFloatDataIsEqual(output_data, input_data);

  webnn_context_remote_.reset();
  webnn_provider_remote_.reset();
  EXPECT_TRUE(base::test::RunUntil([&]() { return true; }));
}

TEST_F(WebNNContextOrtImplTest, DispatchGraphWithReluTest) {
  WebNNContextProviderImpl::CreateForTesting(
      webnn_provider_remote_.BindNewPipeAndPassReceiver());
  ASSERT_TRUE(CreateWebNNContext());

  ASSERT_TRUE(webnn_context_remote_.is_bound());

  OperandDataType data_type = OperandDataType::kFloat32;
  std::vector<uint32_t> shape = {1, 1, 2, 4};
  std::vector<float> input_data({-1.0, -2.0, -3.0, -4.0, 5.0, 6.0, 7.0, 8.0});
  std::vector<float> expected_output_data({0, 0, 0, 0, 5.0, 6.0, 7.0, 8.0});

  // Build a simple graph with relu operator.
  mojo::AssociatedRemote<mojom::WebNNGraphBuilder> graph_builder_remote;
  webnn_context_remote_->CreateGraphBuilder(
      graph_builder_remote.BindNewEndpointAndPassReceiver());
  GraphInfoBuilder builder(graph_builder_remote);

  uint64_t input_operand_id =
      builder.BuildInput("input", shape, OperandDataType::kFloat32);
  uint64_t output_operand_id =
      builder.BuildOutput("output", shape, OperandDataType::kFloat32);
  builder.BuildRelu(input_operand_id, output_operand_id);

  // The GraphImplOrt should be built successfully.
  base::test::TestFuture<mojom::CreateGraphResultPtr> create_graph_future;
  graph_builder_remote->CreateGraph(builder.TakeGraphInfo(),
                                    create_graph_future.GetCallback());
  mojom::CreateGraphResultPtr create_graph_result = create_graph_future.Take();
  EXPECT_TRUE(create_graph_result->is_graph_remote());
  mojo::AssociatedRemote<mojom::WebNNGraph> webnn_graph_remote;
  webnn_graph_remote.Bind(std::move(create_graph_result->get_graph_remote()));

  // Create tensor
  CreateTensorSuccess input_tensor =
      CreateWebNNTensor(webnn_context_remote_, data_type, shape);
  CreateTensorSuccess output_tensor =
      CreateWebNNTensor(webnn_context_remote_, data_type, shape);

  // Write tensor
  input_tensor.webnn_tensor_remote->WriteTensor(VectorToBigBuffer(input_data));

  // Dispatch
  base::flat_map<std::string, blink::WebNNTensorToken> named_inputs;
  named_inputs.emplace("input", input_tensor.webnn_tensor_handle);
  base::flat_map<std::string, blink::WebNNTensorToken> named_outputs;
  named_outputs.emplace("output", output_tensor.webnn_tensor_handle);

  webnn_graph_remote->Dispatch(named_inputs, named_outputs);

  // Read tensor
  base::test::TestFuture<mojom::ReadTensorResultPtr> future;
  output_tensor.webnn_tensor_remote->ReadTensor(future.GetCallback());
  mojom::ReadTensorResultPtr result = future.Take();
  ASSERT_FALSE(result->is_error());

  std::vector<float> output_data =
      BigBufferToVector<float>(std::move(result->get_buffer()));
  VerifyFloatDataIsEqual(output_data, expected_output_data);

  webnn_graph_remote.reset();
  graph_builder_remote.reset();
  webnn_context_remote_.reset();
  webnn_provider_remote_.reset();
  EXPECT_TRUE(base::test::RunUntil([&]() { return true; }));
}

}  // namespace webnn::ort
