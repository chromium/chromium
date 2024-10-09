// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/webnn_graph_impl.h"

#include <cmath>
#include <limits>

#include "base/containers/contains.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_associated_receiver.h"
#include "mojo/public/cpp/system/functions.h"
#include "services/webnn/error.h"
#include "services/webnn/public/cpp/ml_tensor_usage.h"
#include "services/webnn/public/cpp/operand_descriptor.h"
#include "services/webnn/public/cpp/supported_data_types.h"
#include "services/webnn/public/cpp/webnn_errors.h"
#include "services/webnn/public/mojom/features.mojom-features.h"
#include "services/webnn/public/mojom/webnn_context_provider.mojom.h"
#include "services/webnn/public/mojom/webnn_graph.mojom.h"
#include "services/webnn/public/mojom/webnn_graph_builder.mojom.h"
#include "services/webnn/public/mojom/webnn_tensor.mojom.h"
#include "services/webnn/webnn_constant_operand.h"
#include "services/webnn/webnn_context_impl.h"
#include "services/webnn/webnn_context_provider_impl.h"
#include "services/webnn/webnn_graph_builder_impl.h"
#include "services/webnn/webnn_tensor_impl.h"
#include "services/webnn/webnn_test_utils.h"
#include "services/webnn/webnn_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace webnn {

namespace {

// A fake WebNNGraph Mojo interface implementation that binds a pipe for
// computing graph message.
class FakeWebNNGraphImpl final : public WebNNGraphImpl {
 public:
  FakeWebNNGraphImpl(WebNNContextImpl* context,
                     ComputeResourceInfo compute_resource_info)
      : WebNNGraphImpl(context, std::move(compute_resource_info)) {}
  ~FakeWebNNGraphImpl() override = default;

  static void CreateAndBuild(
      WebNNContextImpl* context,
      const mojom::GraphInfo& graph_info,
      ComputeResourceInfo compute_resource_info,
      WebNNContextImpl::CreateGraphImplCallback callback) {
    std::move(callback).Run(std::make_unique<FakeWebNNGraphImpl>(
        context, std::move(compute_resource_info)));
  }

 private:
  // Return the `kOk` result for testing the validation of inputs and outputs in
  // `WebNNGraphImpl::Compute()` function.
  void ComputeImpl(base::flat_map<std::string, mojo_base::BigBuffer> inputs,
                   mojom::WebNNGraph::ComputeCallback callback) override {
    base::flat_map<std::string, mojo_base::BigBuffer> named_outputs;
    std::move(callback).Run(
        mojom::ComputeResult::NewNamedOutputs(std::move(named_outputs)));
  }

  // Return nothing for testing the validation of inputs and outputs in
  // `WebNNGraphImpl::Dispatch()` function.
  void DispatchImpl(
      const base::flat_map<std::string_view, WebNNTensorImpl*>& named_inputs,
      const base::flat_map<std::string_view, WebNNTensorImpl*>& named_outputs)
      override {}
};

// A fake WebNNTensor Mojo interface implementation that binds a pipe for
// tensor creation message.
class FakeWebNNTensorImpl final : public WebNNTensorImpl {
 public:
  FakeWebNNTensorImpl(
      mojo::PendingAssociatedReceiver<mojom::WebNNTensor> receiver,
      WebNNContextImpl* context,
      mojom::TensorInfoPtr tensor_info)
      : WebNNTensorImpl(std::move(receiver), context, std::move(tensor_info)) {}
  ~FakeWebNNTensorImpl() override = default;

 private:
  // Read/write nothing for testing the validation of inputs and outputs in
  // `WebNNGraphImpl::Dispatch()` function.
  void ReadTensorImpl(ReadTensorCallback callback) override {}
  void WriteTensorImpl(mojo_base::BigBuffer src_buffer) override {}
};

// A fake WebNNContext Mojo interface implementation that binds a pipe for
// creating graph message.
class FakeWebNNContextImpl final : public WebNNContextImpl {
 public:
  FakeWebNNContextImpl(mojo::PendingReceiver<mojom::WebNNContext> receiver,
                       WebNNContextProviderImpl* context_provider)
      : WebNNContextImpl(std::move(receiver),
                         context_provider,
                         GetContextPropertiesForTesting(),
                         mojom::CreateContextOptions::New()) {}
  ~FakeWebNNContextImpl() override = default;

  // WebNNContextImpl:
  base::WeakPtr<WebNNContextImpl> AsWeakPtr() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return weak_factory_.GetWeakPtr();
  }

 private:
  void CreateGraphImpl(
      mojom::GraphInfoPtr graph_info,
      WebNNGraphImpl::ComputeResourceInfo compute_resource_info,
      base::flat_map<uint64_t, std::unique_ptr<WebNNConstantOperand>>
          constant_operands,
      CreateGraphImplCallback callback) override {
    FakeWebNNGraphImpl::CreateAndBuild(this, *graph_info,
                                       std::move(compute_resource_info),
                                       std::move(callback));
  }

  void CreateTensorImpl(
      mojo::PendingAssociatedReceiver<mojom::WebNNTensor> receiver,
      mojom::TensorInfoPtr tensor_info,
      CreateTensorImplCallback callback) override {
    std::move(callback).Run(std::make_unique<FakeWebNNTensorImpl>(
        std::move(receiver), this, std::move(tensor_info)));
  }

  base::WeakPtrFactory<FakeWebNNContextImpl> weak_factory_{this};
};

// Helper class to create the FakeWebNNContext that is intended to test
// the graph validation steps and computation resources.
class FakeWebNNBackend : public WebNNContextProviderImpl::BackendForTesting {
 public:
  std::unique_ptr<WebNNContextImpl> CreateWebNNContext(
      WebNNContextProviderImpl* context_provider_impl,
      mojom::CreateContextOptionsPtr options,
      mojom::WebNNContextProvider::CreateWebNNContextCallback callback)
      override {
    mojo::PendingRemote<mojom::WebNNContext> remote;
    auto context_impl = std::make_unique<FakeWebNNContextImpl>(
        remote.InitWithNewPipeAndPassReceiver(), context_provider_impl);
    ContextProperties context_properties = context_impl->properties();
    // The receiver bound to FakeWebNNContext.
    auto success = mojom::CreateContextSuccess::New(
        std::move(remote), std::move(context_properties),
        context_impl->handle());
    std::move(callback).Run(
        mojom::CreateContextResult::NewSuccess(std::move(success)));
    return context_impl;
  }
};

bool ValidateInputsForComputing(
    mojom::GraphInfoPtr graph_info,
    base::flat_map<std::string, mojo_base::BigBuffer> inputs) {
  // Creates WebNN Context mojo interface with the provider.
  mojo::Remote<mojom::WebNNContextProvider> provider_remote;
  WebNNContextProviderImpl::CreateForTesting(
      provider_remote.BindNewPipeAndPassReceiver());

  base::test::TestFuture<mojom::CreateContextResultPtr> create_context_future;
  provider_remote->CreateWebNNContext(mojom::CreateContextOptions::New(),
                                      create_context_future.GetCallback());
  mojom::CreateContextResultPtr create_context_result =
      create_context_future.Take();
  mojo::Remote<mojom::WebNNContext> webnn_context;
  webnn_context.Bind(
      std::move(create_context_result->get_success()->context_remote));

  mojo::AssociatedRemote<mojom::WebNNGraphBuilder> graph_builder_remote;
  webnn_context->CreateGraphBuilder(
      graph_builder_remote.BindNewEndpointAndPassReceiver());

  // Creates WebNN Graph mojo interface with the graph information which is
  // validated before compiling.
  base::test::TestFuture<mojom::CreateGraphResultPtr> create_graph_future;
  graph_builder_remote->CreateGraph(std::move(graph_info),
                                    create_graph_future.GetCallback());
  mojom::CreateGraphResultPtr create_graph_result = create_graph_future.Take();
  mojo::AssociatedRemote<mojom::WebNNGraph> webnn_graph;
  webnn_graph.Bind(std::move(create_graph_result->get_graph_remote()));

  // Validate the inputs in the `Compute` function.
  bool valid = true;
  // Set up the error handler for bad mojo messages.
  mojo::SetDefaultProcessErrorHandler(
      base::BindLambdaForTesting([&](const std::string& error_message) {
        EXPECT_EQ(error_message,
                  "The inputs for computation don't match the built graph's "
                  "expectation.");
        valid = false;
      }));

  base::test::TestFuture<mojom::ComputeResultPtr> compute_future;
  webnn_graph->Compute(std::move(inputs), compute_future.GetCallback());
  EXPECT_TRUE(compute_future.Wait());

  mojo::SetDefaultProcessErrorHandler(base::NullCallback());
  return valid;
}

struct CreateTensorSuccess {
  std::optional<mojo::AssociatedRemote<mojom::WebNNTensor>> webnn_tensor;
  blink::WebNNTensorToken webnn_handle;
};

CreateTensorSuccess CreateWebNNTensor(
    mojo::Remote<mojom::WebNNContext>& webnn_context,
    OperandDataType data_type,
    std::vector<uint32_t> shape) {
  base::test::TestFuture<mojom::CreateTensorResultPtr> create_tensor_future;
  webnn_context->CreateTensor(
      mojom::TensorInfo::New(*OperandDescriptor::Create(data_type, shape),
                             MLTensorUsage()),
      create_tensor_future.GetCallback());
  mojom::CreateTensorResultPtr create_tensor_result =
      create_tensor_future.Take();
  mojo::AssociatedRemote<mojom::WebNNTensor> webnn_tensor;
  webnn_tensor.Bind(
      std::move(create_tensor_result->get_success()->tensor_remote));
  return CreateTensorSuccess{
      std::move(webnn_tensor),
      std::move(create_tensor_result->get_success()->tensor_handle)};
}

mojo::Remote<mojom::WebNNContext> CreateWebNNContext(
    mojo::Remote<mojom::WebNNContextProvider>& webnn_context_provider) {
  base::test::TestFuture<mojom::CreateContextResultPtr> create_context_future;
  webnn_context_provider->CreateWebNNContext(
      mojom::CreateContextOptions::New(), create_context_future.GetCallback());
  mojom::CreateContextResultPtr create_context_result =
      create_context_future.Take();
  mojo::Remote<mojom::WebNNContext> webnn_context;
  webnn_context.Bind(
      std::move(create_context_result->get_success()->context_remote));
  return webnn_context;
}

// Converts inputs and outputs to MLTensor then dispatches them.
bool ValidateDispatch(
    mojo::Remote<mojom::WebNNContext>& webnn_context,
    mojom::GraphInfoPtr graph_info,
    base::flat_map<std::string, CreateTensorSuccess> inputs,
    base::flat_map<std::string, CreateTensorSuccess> outputs) {
  mojo::AssociatedRemote<mojom::WebNNGraphBuilder> graph_builder_remote;
  webnn_context->CreateGraphBuilder(
      graph_builder_remote.BindNewEndpointAndPassReceiver());

  // Creates WebNN Graph mojo interface with the graph information which is
  // validated before compiling.
  base::test::TestFuture<mojom::CreateGraphResultPtr> create_graph_future;
  graph_builder_remote->CreateGraph(std::move(graph_info),
                                    create_graph_future.GetCallback());
  mojom::CreateGraphResultPtr create_graph_result = create_graph_future.Take();
  mojo::AssociatedRemote<mojom::WebNNGraph> webnn_graph;
  webnn_graph.Bind(std::move(create_graph_result->get_graph_remote()));

  // Validate the inputs in the `Dispatch` function.
  bool valid = true;
  // Set up the error handler for bad mojo messages.
  mojo::SetDefaultProcessErrorHandler(
      base::BindLambdaForTesting([&](const std::string& error_message) {
        EXPECT_EQ(error_message, kBadMessageInvalidTensor);
        valid = false;
      }));

  // Assign tensors for the inputs.
  base::flat_map<std::string, blink::WebNNTensorToken> dispatch_inputs;
  for (const auto& [name, tensor_info] : inputs) {
    dispatch_inputs.emplace(name, tensor_info.webnn_handle);
  }

  // Assign tensors for the outputs.
  base::flat_map<std::string, blink::WebNNTensorToken> dispatch_outputs;
  for (const auto& [name, tensor_info] : outputs) {
    dispatch_outputs.emplace(name, tensor_info.webnn_handle);
  }

  // Ensure CreateTensor messages have a chance to finish before calling
  // Dispatch().
  webnn_context.FlushForTesting();
  webnn_graph->Dispatch(dispatch_inputs, dispatch_outputs);

  // Ensure Dispatch message has a chance to finish before removing the error
  // handler.
  webnn_graph.FlushForTesting();
  mojo::SetDefaultProcessErrorHandler(base::NullCallback());
  return valid;
}

OperandDataType kAllOperandDataTypes[] = {
    OperandDataType::kFloat32, OperandDataType::kFloat16,
    OperandDataType::kInt32,   OperandDataType::kInt8,
    OperandDataType::kUint8,
};

}  // namespace

class WebNNGraphImplTest : public testing::Test {
 public:
  WebNNGraphImplTest(const WebNNGraphImplTest&) = delete;
  WebNNGraphImplTest& operator=(const WebNNGraphImplTest&) = delete;

  void SetUp() override {
    WebNNContextProviderImpl::SetBackendForTesting(&backend_for_testing_);
  }
  void TearDown() override {
    WebNNContextProviderImpl::SetBackendForTesting(nullptr);
  }

 protected:
  WebNNGraphImplTest()
      : scoped_feature_list_(
            webnn::mojom::features::kWebMachineLearningNeuralNetwork) {}
  ~WebNNGraphImplTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::TaskEnvironment task_environment_;

  FakeWebNNBackend backend_for_testing_;
};

struct OperandInfo {
  OperandDataType type;
  std::vector<uint32_t> dimensions;
};

struct ArgMinMaxTester {
  mojom::ArgMinMax::Kind kind;
  OperandInfo input;
  uint32_t axis;
  bool keep_dimensions = false;
  OperandInfo output;
  bool expected;

  void Test() {
    auto context_properties = GetContextPropertiesForTesting();

    // Build the graph with mojo type.
    GraphInfoBuilder builder;
    uint64_t input_operand_id =
        builder.BuildInput("input", input.dimensions, input.type);
    uint64_t output_operand_id =
        builder.BuildOutput("output", output.dimensions, output.type);
    builder.BuildArgMinMax(kind, input_operand_id, output_operand_id, axis,
                           keep_dimensions);

    EXPECT_EQ(WebNNGraphBuilderImpl::IsValidForTesting(context_properties,
                                                       builder.GetGraphInfo()),
              expected);
  }
};

TEST_F(WebNNGraphImplTest, ArgMinMaxTest) {
  const auto ArgMinMaxKinds = {mojom::ArgMinMax_Kind::kMin,
                               mojom::ArgMinMax_Kind::kMax};
  for (const auto kind : ArgMinMaxKinds) {
    {
      // Test argMinMax operator with axis = 0 and keep_dimensions = true.
      ArgMinMaxTester{.kind = kind,
                      .input = {.type = OperandDataType::kFloat32,
                                .dimensions = {2, 3, 4, 5}},
                      .axis = 0,
                      .keep_dimensions = true,
                      .output = {.type = OperandDataType::kInt32,
                                 .dimensions = {1, 3, 4, 5}},
                      .expected = true}
          .Test();
    }
    {
      // Test argMinMax operator with axis = 1 and keep_dimensions = false.
      ArgMinMaxTester{
          .kind = kind,
          .input = {.type = OperandDataType::kFloat16,
                    .dimensions = {2, 3, 4, 5}},
          .axis = 1,
          .keep_dimensions = false,
          .output = {.type = OperandDataType::kInt32, .dimensions = {2, 4, 5}},
          .expected = true}
          .Test();
    }
    {
      // Test the invalid graph when axis is greater than or equal to input
      // rank.
      ArgMinMaxTester{.kind = kind,
                      .input = {.type = OperandDataType::kFloat32,
                                .dimensions = {2, 3, 4, 5}},
                      .axis = 4,
                      .keep_dimensions = true,
                      .output = {.type = OperandDataType::kInt32,
                                 .dimensions = {2, 3, 4, 1}},
                      .expected = false}
          .Test();
    }
    {
      // Test the invalid graph when the output data type is not support.
      ArgMinMaxTester{.kind = kind,
                      .input = {.type = OperandDataType::kFloat32,
                                .dimensions = {2, 3, 4, 5}},
                      .axis = 0,
                      .keep_dimensions = true,
                      .output = {.type = OperandDataType::kFloat32,
                                 .dimensions = {1, 3, 4, 5}},
                      .expected = false}
          .Test();
    }
    {
      // Test the invalid graph when the output shape is incorrect.
      ArgMinMaxTester{.kind = kind,
                      .input = {.type = OperandDataType::kFloat32,
                                .dimensions = {2, 3, 4, 5}},
                      .axis = 0,
                      .keep_dimensions = false,
                      .output = {.type = OperandDataType::kInt32,
                                 .dimensions = {1, 3, 4, 5}},
                      .expected = false}
          .Test();
    }
    {
      // Test the invalid graph when the input and output are same operand.
      auto context_properties = GetContextPropertiesForTesting();
      GraphInfoBuilder builder;
      uint64_t input_operand_id =
          builder.BuildInput("input", {2, 3, 4, 5}, OperandDataType::kInt32);
      builder.BuildArgMinMax(kind, input_operand_id, input_operand_id,
                             /*axis=*/0,
                             /*keep_dimensions=*/true);
      EXPECT_FALSE(WebNNGraphBuilderImpl::IsValidForTesting(
          context_properties, builder.GetGraphInfo()));
    }
  }
}

struct ClampTester {
  OperandInfo input;
  struct ClampAttributes {
    float min_value;
    float max_value;
  };
  ClampAttributes attributes;
  OperandInfo output;
  bool expected;

  void Test() {
    auto context_properties = GetContextPropertiesForTesting();

    // Build the graph with mojo type.
    GraphInfoBuilder builder;
    uint64_t input_operand_id =
        builder.BuildInput("input", input.dimensions, input.type);
    uint64_t output_operand_id =
        builder.BuildOutput("output", output.dimensions, output.type);
    builder.BuildClamp(input_operand_id, output_operand_id,
                       attributes.min_value, attributes.max_value);
    EXPECT_EQ(WebNNGraphBuilderImpl::IsValidForTesting(context_properties,
                                                       builder.GetGraphInfo()),
              expected);
  }
};

TEST_F(WebNNGraphImplTest, ClampTest) {
  {
    // Test clamp operator with both the minimum and maximum values.
    ClampTester{
        .input = {.type = OperandDataType::kInt8, .dimensions = {3, 4}},
        .attributes = {.min_value = 0.0, .max_value = 6.0},
        .output = {.type = OperandDataType::kInt8, .dimensions = {3, 4}},
        .expected = true}
        .Test();
  }
  {
    // Test clamp operator with the min value is infinite.
    ClampTester{
        .input = {.type = OperandDataType::kInt32, .dimensions = {2, 3, 4}},
        .attributes = {.min_value = static_cast<float>(-1.0 / 0.0),
                       .max_value = 3.0},
        .output = {.type = OperandDataType::kInt32, .dimensions = {2, 3, 4}},
        .expected = true}
        .Test();
  }
  {
    // Test clamp operator with the max value is infinite.
    ClampTester{
        .input = {.type = OperandDataType::kInt32, .dimensions = {2, 3, 4}},
        .attributes = {.min_value = 0.0,
                       .max_value = static_cast<float>(1.0 / 0.0)},
        .output = {.type = OperandDataType::kInt32, .dimensions = {2, 3, 4}},
        .expected = true}
        .Test();
  }
  {
    // Test the invalid graph when max value = 0 and min value = 0.
    ClampTester{.input = {.type = OperandDataType::kFloat32,
                          .dimensions = {1, 2, 2, 7}},
                .output = {.type = OperandDataType::kFloat32,
                           .dimensions = {1, 2, 2, 7}},
                .expected = false}
        .Test();
  }
  {
    // Test the invalid graph when the max value is less than the min value.
    ClampTester{
        .input = {.type = OperandDataType::kFloat32, .dimensions = {4, 2}},
        .attributes = {.min_value = 7.0, .max_value = 3.0},
        .output = {.type = OperandDataType::kFloat32, .dimensions = {4, 2}},
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph when the min value is NAN.
    ClampTester{
        .input = {.type = OperandDataType::kInt32, .dimensions = {2, 3, 4}},
        .attributes = {.min_value = NAN, .max_value = 3.0},
        .output = {.type = OperandDataType::kInt32, .dimensions = {2, 3, 4}},
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph when the max value is NAN.
    ClampTester{
        .input = {.type = OperandDataType::kInt32, .dimensions = {2, 3, 4}},
        .attributes = {.min_value = 0.0, .max_value = NAN},
        .output = {.type = OperandDataType::kInt32, .dimensions = {2, 3, 4}},
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph for the output shapes are not expected.
    ClampTester{
        .input = {.type = OperandDataType::kFloat32, .dimensions = {4, 2}},
        .output = {.type = OperandDataType::kFloat32, .dimensions = {2}},
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph for output types don't match.
    ClampTester{.input = {.type = OperandDataType::kFloat32, .dimensions = {2}},
                .output = {.type = OperandDataType::kInt32, .dimensions = {2}},
                .expected = false}
        .Test();
  }
}

struct HardSigmoidTester {
  OperandInfo input;
  std::optional<float> alpha;
  std::optional<float> beta;
  OperandInfo output;
  bool expected;

  void Test() {
    auto context_properties = GetContextPropertiesForTesting();

    // Build the graph with mojo type.
    GraphInfoBuilder builder;
    uint64_t input_operand_id =
        builder.BuildInput("input", input.dimensions, input.type);
    uint64_t output_operand_id =
        builder.BuildOutput("output", output.dimensions, output.type);
    builder.BuildHardSigmoid(input_operand_id, output_operand_id, alpha, beta);
    EXPECT_EQ(WebNNGraphBuilderImpl::IsValidForTesting(context_properties,
                                                       builder.GetGraphInfo()),
              expected);
  }
};

TEST_F(WebNNGraphImplTest, HardSigmoidTest) {
  {
    // Test hardSigmoid operator with default alpha and beta values.
    HardSigmoidTester{
        .input = {.type = OperandDataType::kFloat32, .dimensions = {3, 4}},
        .output = {.type = OperandDataType::kFloat32, .dimensions = {3, 4}},
        .expected = true}
        .Test();
  }
  {
    // Test the invalid graph when the alpha value is NAN.
    HardSigmoidTester{
        .input = {.type = OperandDataType::kFloat32, .dimensions = {2, 3, 4}},
        .alpha = NAN,
        .beta = 0.5,
        .output = {.type = OperandDataType::kFloat32, .dimensions = {2, 3, 4}},
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph when the beta value is NAN.
    HardSigmoidTester{
        .input = {.type = OperandDataType::kFloat16, .dimensions = {2, 3, 4}},
        .alpha = 1.0,
        .beta = NAN,
        .output = {.type = OperandDataType::kFloat16, .dimensions = {2, 3, 4}},
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph for the output shapes are not expected.
    HardSigmoidTester{
        .input = {.type = OperandDataType::kFloat32, .dimensions = {4, 2}},
        .output = {.type = OperandDataType::kFloat32, .dimensions = {2}},
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph for output types don't match.
    HardSigmoidTester{
        .input = {.type = OperandDataType::kFloat32, .dimensions = {2}},
        .output = {.type = OperandDataType::kInt32, .dimensions = {2}},
        .expected = false}
        .Test();
  }
}

struct BatchNormalizationTester {
  OperandInfo input;
  OperandInfo mean;
  OperandInfo variance;
  std::optional<OperandInfo> scale;
  std::optional<OperandInfo> bias;
  struct BatchNormalizationAttributes {
    std::optional<uint64_t> scale_operand_id;
    std::optional<uint64_t> bias_operand_id;
    uint32_t axis = 1;
    float epsilon = 1e-5;
  };
  BatchNormalizationAttributes attributes;
  OperandInfo output;
  bool expected;

  void Test() {
    auto context_properties = GetContextPropertiesForTesting();

    // Build the graph with mojo type.
    GraphInfoBuilder builder;
    uint64_t input_operand_id =
        builder.BuildInput("input", input.dimensions, input.type);
    uint64_t mean_operand_id =
        builder.BuildInput("mean", mean.dimensions, mean.type);
    uint64_t variance_operand_id =
        builder.BuildInput("variance", variance.dimensions, variance.type);
    uint64_t output_operand_id =
        builder.BuildOutput("output", output.dimensions, output.type);

    if (scale) {
      attributes.scale_operand_id =
          builder.BuildInput("scale", scale->dimensions, scale->type);
    }
    if (bias) {
      attributes.bias_operand_id =
          builder.BuildInput("bias", bias->dimensions, bias->type);
    }
    builder.BuildBatchNormalization(input_operand_id, mean_operand_id,
                                    variance_operand_id, output_operand_id,
                                    std::move(attributes));
    EXPECT_EQ(WebNNGraphBuilderImpl::IsValidForTesting(context_properties,
                                                       builder.GetGraphInfo()),
              expected);
  }
};

TEST_F(WebNNGraphImplTest, BatchNormalizationTest) {
  {
    // Test building batchNormalization with default option.
    BatchNormalizationTester{
        .input = {.type = OperandDataType::kFloat32,
                  .dimensions = {1, 2, 3, 3}},
        .mean = {.type = OperandDataType::kFloat32, .dimensions = {2}},
        .variance = {.type = OperandDataType::kFloat32, .dimensions = {2}},
        .output = {.type = OperandDataType::kFloat32,
                   .dimensions = {1, 2, 3, 3}},
        .expected = true}
        .Test();
  }
  {
    // Test building batchNormalization with axis = 3.
    BatchNormalizationTester{
        .input = {.type = OperandDataType::kFloat32,
                  .dimensions = {1, 2, 3, 3}},
        .mean = {.type = OperandDataType::kFloat32, .dimensions = {3}},
        .variance = {.type = OperandDataType::kFloat32, .dimensions = {3}},
        .attributes = {.axis = 3},
        .output = {.type = OperandDataType::kFloat32,
                   .dimensions = {1, 2, 3, 3}},
        .expected = true}
        .Test();
  }
  {
    // Test building batchNormalization with setting optional bias and scale.
    BatchNormalizationTester{
        .input = {.type = OperandDataType::kFloat32,
                  .dimensions = {1, 2, 3, 3}},
        .mean = {.type = OperandDataType::kFloat32, .dimensions = {2}},
        .variance = {.type = OperandDataType::kFloat32, .dimensions = {2}},
        .scale =
            OperandInfo{.type = OperandDataType::kFloat32, .dimensions = {2}},
        .bias =
            OperandInfo{.type = OperandDataType::kFloat32, .dimensions = {2}},
        .output = {.type = OperandDataType::kFloat32,
                   .dimensions = {1, 2, 3, 3}},
        .expected = true}
        .Test();
  }
  {
    // Test building batchNormalization when input data type and mean data
    // type mismatched.
    BatchNormalizationTester{
        .input = {.type = OperandDataType::kFloat32,
                  .dimensions = {1, 2, 3, 3}},
        .mean = {.type = OperandDataType::kInt32, .dimensions = {2}},
        .variance = {.type = OperandDataType::kFloat32, .dimensions = {2}},
        .output = {.type = OperandDataType::kFloat32,
                   .dimensions = {1, 2, 3, 3}},
        .expected = false}
        .Test();
  }
  {
    // Test building batchNormalization when the size of mean is not equal to
    // the size of the input dimension denoted by axis.
    BatchNormalizationTester{
        .input = {.type = OperandDataType::kFloat32,
                  .dimensions = {1, 2, 3, 3}},
        .mean = {.type = OperandDataType::kFloat32, .dimensions = {3}},
        .variance = {.type = OperandDataType::kFloat32, .dimensions = {2}},
        .output = {.type = OperandDataType::kFloat32,
                   .dimensions = {1, 2, 3, 3}},
        .expected = false}
        .Test();
  }
  {
    // Test building batchNormalization when input data type and variance data
    // type mismatched.
    BatchNormalizationTester{
        .input = {.type = OperandDataType::kFloat32,
                  .dimensions = {1, 2, 3, 3}},
        .mean = {.type = OperandDataType::kInt32, .dimensions = {2}},
        .variance = {.type = OperandDataType::kFloat32, .dimensions = {2}},
        .output = {.type = OperandDataType::kFloat32,
                   .dimensions = {1, 2, 3, 3}},
        .expected = false}
        .Test();
  }
  {
    // Test building batchNormalization when the size of variance is not equal
    // to the size of the input dimension denoted by axis.
    BatchNormalizationTester{
        .input = {.type = OperandDataType::kFloat32,
                  .dimensions = {1, 2, 3, 3}},
        .mean = {.type = OperandDataType::kFloat32, .dimensions = {2}},
        .variance = {.type = OperandDataType::kFloat32, .dimensions = {1}},
        .output = {.type = OperandDataType::kFloat32,
                   .dimensions = {1, 2, 3, 3}},
        .expected = false}
        .Test();
  }
  {
    // Test building batchNormalization when input data is not floating point
    // type.
    BatchNormalizationTester{
        .input = {.type = OperandDataType::kInt32, .dimensions = {1, 2, 3, 3}},
        .mean = {.type = OperandDataType::kInt32, .dimensions = {2}},
        .variance = {.type = OperandDataType::kInt32, .dimensions = {2}},
        .output = {.type = OperandDataType::kFloat32,
                   .dimensions = {1, 2, 3, 3}},
        .expected = false}
        .Test();
  }
  {
    // Test building batchNormalization when axis is out of range [0, N-1].
    BatchNormalizationTester{
        .input = {.type = OperandDataType::kFloat32,
                  .dimensions = {1, 2, 3, 3}},
        .mean = {.type = OperandDataType::kFloat32, .dimensions = {3}},
        .variance = {.type = OperandDataType::kFloat32, .dimensions = {3}},
        .attributes = {.axis = 4},
        .output = {.type = OperandDataType::kFloat32,
                   .dimensions = {1, 2, 3, 3}},
        .expected = false}
        .Test();
  }
  {
    // Test batchNormalization when input data type and scale data type
    // mismatched.
    BatchNormalizationTester{
        .input = {.type = OperandDataType::kFloat32,
                  .dimensions = {1, 2, 3, 3}},
        .mean = {.type = OperandDataType::kFloat32, .dimensions = {2}},
        .variance = {.type = OperandDataType::kFloat32, .dimensions = {2}},
        .scale =
            OperandInfo{.type = OperandDataType::kInt32, .dimensions = {2}},
        .output = {.type = OperandDataType::kFloat32,
                   .dimensions = {1, 2, 3, 3}},
        .expected = false}
        .Test();
  }
  {
    // Test building batchNormalization when the size of scale is not equal
    // to the size of the input dimension denoted by axis.
    BatchNormalizationTester{
        .input = {.type = OperandDataType::kFloat32,
                  .dimensions = {1, 2, 3, 3}},
        .mean = {.type = OperandDataType::kFloat32, .dimensions = {2}},
        .variance = {.type = OperandDataType::kFloat32, .dimensions = {2}},
        .scale =
            OperandInfo{.type = OperandDataType::kFloat32, .dimensions = {3}},
        .output = {.type = OperandDataType::kFloat32,
                   .dimensions = {1, 2, 3, 3}},
        .expected = false}
        .Test();
  }
  {
    // Test batchNormalization when input data type and bias data type
    // mismatched.
    BatchNormalizationTester{
        .input = {.type = OperandDataType::kFloat32,
                  .dimensions = {1, 2, 3, 3}},
        .mean = {.type = OperandDataType::kFloat32, .dimensions = {2}},
        .variance = {.type = OperandDataType::kFloat32, .dimensions = {2}},
        .bias = OperandInfo{.type = OperandDataType::kInt32, .dimensions = {2}},
        .output = {.type = OperandDataType::kFloat32,
                   .dimensions = {1, 2, 3, 3}},
        .expected = false}
        .Test();
  }
  {
    // Test building batchNormalization when the size of bias is not equal
    // to the size of the input dimension denoted by axis.
    BatchNormalizationTester{
        .input = {.type = OperandDataType::kFloat32,
                  .dimensions = {1, 2, 3, 3}},
        .mean = {.type = OperandDataType::kFloat32, .dimensions = {2}},
        .variance = {.type = OperandDataType::kFloat32, .dimensions = {2}},
        .bias =
            OperandInfo{.type = OperandDataType::kFloat32, .dimensions = {3}},
        .output = {.type = OperandDataType::kFloat32,
                   .dimensions = {1, 2, 3, 3}},
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph for output type is not the same as input type.
    BatchNormalizationTester{
        .input = {.type = OperandDataType::kFloat32,
                  .dimensions = {1, 2, 3, 3}},
        .mean = {.type = OperandDataType::kFloat32, .dimensions = {2}},
        .variance = {.type = OperandDataType::kFloat32, .dimensions = {2}},
        .bias =
            OperandInfo{.type = OperandDataType::kFloat32, .dimensions = {3}},
        .output = {.type = OperandDataType::kInt32, .dimensions = {1, 2, 3, 3}},
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph for output shape is not the same as input shape.
    BatchNormalizationTester{
        .input = {.type = OperandDataType::kFloat32,
                  .dimensions = {1, 2, 3, 3}},
        .mean = {.type = OperandDataType::kFloat32, .dimensions = {2}},
        .variance = {.type = OperandDataType::kFloat32, .dimensions = {2}},
        .bias =
            OperandInfo{.type = OperandDataType::kFloat32, .dimensions = {3}},
        .output = {.type = OperandDataType::kFloat32,
                   .dimensions = {1, 1, 3, 3}},
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph for input operand == output operand.
    auto context_properties = GetContextPropertiesForTesting();
    GraphInfoBuilder builder;
    uint64_t input_operand_id =
        builder.BuildInput("input", {1, 2, 3, 4}, OperandDataType::kFloat32);
    uint64_t mean_operand_id =
        builder.BuildInput("mean", {2}, OperandDataType::kFloat32);
    uint64_t variance_operand_id =
        builder.BuildInput("variance", {2}, OperandDataType::kFloat32);
    builder.BuildBatchNormalization(
        input_operand_id, mean_operand_id, variance_operand_id,
        input_operand_id,
        BatchNormalizationTester::BatchNormalizationAttributes{});
    EXPECT_FALSE(WebNNGraphBuilderImpl::IsValidForTesting(
        context_properties, builder.GetGraphInfo()));
  }
  {
    // Test the invalid graph for mean operand == output operand.
    auto context_properties = GetContextPropertiesForTesting();
    GraphInfoBuilder builder;
    uint64_t input_operand_id =
        builder.BuildInput("input", {1, 2, 3, 4}, OperandDataType::kFloat32);
    uint64_t mean_operand_id =
        builder.BuildInput("mean", {2}, OperandDataType::kFloat32);
    uint64_t variance_operand_id =
        builder.BuildInput("variance", {2}, OperandDataType::kFloat32);
    builder.BuildBatchNormalization(
        input_operand_id, mean_operand_id, variance_operand_id, mean_operand_id,
        BatchNormalizationTester::BatchNormalizationAttributes{});
    EXPECT_FALSE(WebNNGraphBuilderImpl::IsValidForTesting(
        context_properties, builder.GetGraphInfo()));
  }
  {
    // Test the invalid graph for variance operand == output operand.
    auto context_properties = GetContextPropertiesForTesting();
    GraphInfoBuilder builder;
    uint64_t input_operand_id =
        builder.BuildInput("input", {1, 2, 3, 4}, OperandDataType::kFloat32);
    uint64_t mean_operand_id =
        builder.BuildInput("mean", {2}, OperandDataType::kFloat32);
    uint64_t variance_operand_id =
        builder.BuildInput("variance", {2}, OperandDataType::kFloat32);
    builder.BuildBatchNormalization(
        input_operand_id, mean_operand_id, variance_operand_id,
        variance_operand_id,
        BatchNormalizationTester::BatchNormalizationAttributes{});
    EXPECT_FALSE(WebNNGraphBuilderImpl::IsValidForTesting(
        context_properties, builder.GetGraphInfo()));
  }
}

struct ConcatTester {
  std::vector<OperandInfo> inputs;
  uint32_t axis;
  OperandInfo output;
  bool expected;

  void Test() {
    auto context_properties = GetContextPropertiesForTesting();

    // Build the graph with mojo type.
    GraphInfoBuilder builder;
    std::vector<uint64_t> input_operand_ids;
    input_operand_ids.reserve(inputs.size());
    for (size_t i = 0; i < inputs.size(); ++i) {
      input_operand_ids.push_back(
          builder.BuildInput(base::StringPrintf("input%zu", i),
                             inputs[i].dimensions, inputs[i].type));
    }
    uint64_t output_operand_id =
        builder.BuildOutput("output", output.dimensions, output.type);
    builder.BuildConcat(std::move(input_operand_ids), output_operand_id, axis);
    EXPECT_EQ(WebNNGraphBuilderImpl::IsValidForTesting(context_properties,
                                                       builder.GetGraphInfo()),
              expected);
  }
};

TEST_F(WebNNGraphImplTest, ConcatTest) {
  {
    // Test concat operator with three inputs.
    ConcatTester{
        .inputs =
            {{.type = OperandDataType::kFloat32, .dimensions = {3, 1, 5, 6}},
             {.type = OperandDataType::kFloat32, .dimensions = {3, 2, 5, 6}},
             {.type = OperandDataType::kFloat32, .dimensions = {3, 3, 5, 6}}},
        .axis = 1,
        .output = {.type = OperandDataType::kFloat32,
                   .dimensions = {3, 6, 5, 6}},
        .expected = true}
        .Test();
  }
  {
    // Test concat operator when the input is the same as output.
    ConcatTester{.inputs = {{.type = OperandDataType::kFloat32,
                             .dimensions = {3, 1, 5, 6}}},
                 .axis = 1,
                 .output = {.type = OperandDataType::kFloat32,
                            .dimensions = {3, 1, 5, 6}},
                 .expected = true}
        .Test();
  }
  {
    // Test concat operator with empty inputs.
    ConcatTester{.inputs = {},
                 .axis = 0,
                 .output = {.type = OperandDataType::kInt32, .dimensions = {1}},
                 .expected = false}
        .Test();
  }
  {
    // Test concat operator when the inputs' datatypes don't match each
    // other.
    ConcatTester{.inputs = {{.type = OperandDataType::kFloat32,
                             .dimensions = {3, 1, 5, 6}},
                            {.type = OperandDataType::kInt32,
                             .dimensions = {3, 2, 5, 6}}},
                 .axis = 1,
                 .output = {.type = OperandDataType::kFloat32,
                            .dimensions = {3, 3, 5, 6}},
                 .expected = false}
        .Test();
  }
  {
    // Test concat operator when the inputs can not be concatenated.
    ConcatTester{
        .inputs = {{.type = OperandDataType::kFloat32, .dimensions = {3, 1, 5}},
                   {.type = OperandDataType::kFloat32,
                    .dimensions = {3, 2, 5, 6}}},
        .axis = 1,
        .output = {.type = OperandDataType::kFloat32, .dimensions = {3, 3, 5}},
        .expected = false}
        .Test();
  }
  {
    // Test concat operator when the axis is equal to or greater than the
    // size of dimension.
    ConcatTester{.inputs = {{.type = OperandDataType::kFloat32,
                             .dimensions = {3, 1, 5, 6}},
                            {.type = OperandDataType::kFloat32,
                             .dimensions = {3, 1, 5, 6}}},
                 .axis = 4,
                 .output = {.type = OperandDataType::kFloat32,
                            .dimensions = {3, 1, 5, 12}},
                 .expected = false}
        .Test();
  }
  {
    // Test concat operator when the inputs have other axes with different
    // sizes except on the axis.
    ConcatTester{.inputs = {{.type = OperandDataType::kFloat32,
                             .dimensions = {3, 1, 5, 6}},
                            {.type = OperandDataType::kFloat32,
                             .dimensions = {3, 1, 5, 1}}},
                 .axis = 1,
                 .output = {.type = OperandDataType::kFloat32,
                            .dimensions = {3, 2, 5, 7}},
                 .expected = false}
        .Test();
  }
  {
    // Test concat operator when the output datatype doesn't match the
    // inputs' datatypes.
    ConcatTester{
        .inputs = {{.type = OperandDataType::kFloat32,
                    .dimensions = {3, 1, 5, 6}},
                   {.type = OperandDataType::kFloat32,
                    .dimensions = {3, 2, 5, 6}}},
        .axis = 1,
        .output = {.type = OperandDataType::kInt32, .dimensions = {3, 3, 5, 6}},
        .expected = false}
        .Test();
  }
  {
    // Test concat operator when the output dimension is incorrect.
    ConcatTester{
        .inputs = {{.type = OperandDataType::kFloat32, .dimensions = {3, 1, 2}},
                   {.type = OperandDataType::kFloat32,
                    .dimensions = {1, 1, 2}}},
        .axis = 0,
        .output = {.type = OperandDataType::kFloat32, .dimensions = {5, 1, 2}},
        .expected = false}
        .Test();
  }
}

struct Conv2dTester {
  mojom::Conv2d::Kind type;
  OperandInfo input;
  OperandInfo filter;
  struct Conv2dAttributes {
    std::vector<uint32_t> padding = {0, 0, 0, 0};
    std::vector<uint32_t> strides = {1, 1};
    std::vector<uint32_t> dilations = {1, 1};
    uint32_t groups = 1;
    InputOperandLayout input_layout = InputOperandLayout::kNchw;
    std::optional<OperandInfo> bias;
  };
  Conv2dAttributes attributes;
  OperandInfo output;
  bool expected;

  void Test() {
    auto context_properties = GetContextPropertiesForTesting();
    // Override the default input layout to exercise all the validation cases.
    context_properties.input_operand_layout = attributes.input_layout;

    // Build the graph with mojo type.
    GraphInfoBuilder builder;
    uint64_t input_operand_id =
        builder.BuildInput("input", input.dimensions, input.type);
    uint64_t filter_operand_id =
        builder.BuildInput("filter", filter.dimensions, filter.type);

    std::optional<uint64_t> bias_operand_id;
    if (attributes.bias) {
      bias_operand_id = builder.BuildInput("bias", attributes.bias->dimensions,
                                           attributes.bias->type);
    }

    uint64_t output_operand_id =
        builder.BuildOutput("output", output.dimensions, output.type);
    builder.BuildConv2d(type, input_operand_id, filter_operand_id,
                        output_operand_id, std::move(attributes),
                        bias_operand_id);
    EXPECT_EQ(WebNNGraphBuilderImpl::IsValidForTesting(context_properties,
                                                       builder.GetGraphInfo()),
              expected);
  }
};

TEST_F(WebNNGraphImplTest, Conv2dTest) {
  {
    // Test conv2d with default attributes.
    Conv2dTester{.type = mojom::Conv2d::Kind::kDirect,
                 .input = {.type = OperandDataType::kFloat32,
                           .dimensions = {1, 1, 5, 5}},
                 .filter = {.type = OperandDataType::kFloat32,
                            .dimensions = {1, 1, 3, 3}},
                 .output = {.type = OperandDataType::kFloat32,
                            .dimensions = {1, 1, 3, 3}},
                 .expected = true}
        .Test();
  }
  {
    // Test conv2d for same upper or lower padding.
    Conv2dTester{.type = mojom::Conv2d::Kind::kDirect,
                 .input = {.type = OperandDataType::kFloat16,
                           .dimensions = {1, 1, 5, 5}},
                 .filter = {.type = OperandDataType::kFloat16,
                            .dimensions = {1, 1, 3, 3}},
                 .attributes = {.padding = {1, 1, 1, 1}},
                 .output = {.type = OperandDataType::kFloat16,
                            .dimensions = {1, 1, 5, 5}},
                 .expected = true}
        .Test();
  }
  {
    // Test conv2d with strides=2 and padding=1.
    Conv2dTester{.type = mojom::Conv2d::Kind::kDirect,
                 .input = {.type = OperandDataType::kFloat16,
                           .dimensions = {1, 1, 5, 5}},
                 .filter = {.type = OperandDataType::kFloat16,
                            .dimensions = {1, 1, 3, 3}},
                 .attributes = {.padding = {1, 1, 1, 1}, .strides = {2, 2}},
                 .output = {.type = OperandDataType::kFloat16,
                            .dimensions = {1, 1, 3, 3}},
                 .expected = true}
        .Test();
  }
  {
    // Test depthwise conv2d by setting groups to input channels.
    Conv2dTester{.type = mojom::Conv2d::Kind::kDirect,
                 .input = {.type = OperandDataType::kFloat16,
                           .dimensions = {1, 4, 2, 2}},
                 .filter = {.type = OperandDataType::kFloat16,
                            .dimensions = {4, 1, 2, 2}},
                 .attributes = {.groups = 4},
                 .output = {.type = OperandDataType::kFloat16,
                            .dimensions = {1, 4, 1, 1}},
                 .expected = true}
        .Test();
  }
  {
    // Test conv2d with inputLayout="nchw" and filterLayout="oihw".
    Conv2dTester{.type = mojom::Conv2d::Kind::kDirect,
                 .input = {.type = OperandDataType::kFloat16,
                           .dimensions = {1, 2, 5, 5}},
                 .filter = {.type = OperandDataType::kFloat16,
                            .dimensions = {1, 2, 3, 3}},
                 .attributes = {.input_layout = InputOperandLayout::kNchw},
                 .output = {.type = OperandDataType::kFloat16,
                            .dimensions = {1, 1, 3, 3}},
                 .expected = true}
        .Test();
  }
  {
    // Test the invalid graph when the input is not a 4-D tensor.
    Conv2dTester{
        .type = mojom::Conv2d::Kind::kDirect,
        .input = {.type = OperandDataType::kFloat32, .dimensions = {1, 5, 5}},
        .filter = {.type = OperandDataType::kFloat32,
                   .dimensions = {1, 1, 3, 3}},
        .output = {.type = OperandDataType::kFloat32,
                   .dimensions = {1, 1, 3, 3}},
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph when the input data type is not floating point.
    Conv2dTester{
        .type = mojom::Conv2d::Kind::kDirect,
        .input = {.type = OperandDataType::kInt8, .dimensions = {1, 1, 5, 5}},
        .filter = {.type = OperandDataType::kInt8, .dimensions = {1, 1, 3, 3}},
        .output = {.type = OperandDataType::kInt8, .dimensions = {1, 1, 3, 3}},
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph when the filter is not a 4-D tensor.
    Conv2dTester{
        .type = mojom::Conv2d::Kind::kDirect,
        .input = {.type = OperandDataType::kFloat32,
                  .dimensions = {1, 1, 5, 5}},
        .filter = {.type = OperandDataType::kFloat32, .dimensions = {1, 3, 3}},
        .output = {.type = OperandDataType::kFloat32,
                   .dimensions = {1, 1, 3, 3}},
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph when the filter type doesn't match the input
    // type.
    Conv2dTester{
        .type = mojom::Conv2d::Kind::kDirect,
        .input = {.type = OperandDataType::kFloat32,
                  .dimensions = {1, 1, 5, 5}},
        .filter = {.type = OperandDataType::kInt32, .dimensions = {1, 1, 3, 3}},
        .output = {.type = OperandDataType::kFloat32,
                   .dimensions = {1, 1, 3, 3}},
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph when the bias type doesn't match input type.
    Conv2dTester{
        .type = mojom::Conv2d::Kind::kDirect,
        .input = {.type = OperandDataType::kFloat32,
                  .dimensions = {1, 1, 5, 5}},
        .filter = {.type = OperandDataType::kFloat32,
                   .dimensions = {1, 1, 3, 3}},
        .attributes = {.bias = OperandInfo{.type = OperandDataType::kInt32,
                                           .dimensions = {1}}},
        .output = {.type = OperandDataType::kFloat32,
                   .dimensions = {1, 1, 3, 3}},
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph when the bias shape is not equal to
    // [output_channels].
    Conv2dTester{
        .type = mojom::Conv2d::Kind::kDirect,
        .input = {.type = OperandDataType::kFloat32,
                  .dimensions = {1, 1, 5, 5}},
        .filter = {.type = OperandDataType::kFloat32,
                   .dimensions = {1, 1, 3, 3}},
        .attributes = {.bias = OperandInfo{.type = OperandDataType::kFloat32,
                                           .dimensions = {2}}},
        .output = {.type = OperandDataType::kFloat32,
                   .dimensions = {1, 1, 3, 3}},
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph when the number of filter input channels
    // doesn't match the result of input channels divided by groups
    Conv2dTester{
        .type = mojom::Conv2d::Kind::kDirect,
        .input = {.type = OperandDataType::kFloat32,
                  .dimensions = {1, 1, 5, 5}},
        .filter = {.type = OperandDataType::kFloat32,
                   .dimensions = {1, 1, 3, 3}},
        .attributes = {.groups = 3},
        .output = {.type = OperandDataType::kInt32, .dimensions = {1, 1, 3, 3}},
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph for the output shapes are not expected.
    Conv2dTester{.type = mojom::Conv2d::Kind::kDirect,
                 .input = {.type = OperandDataType::kFloat32,
                           .dimensions = {1, 1, 5, 5}},
                 .filter = {.type = OperandDataType::kFloat32,
                            .dimensions = {1, 1, 3, 3}},
                 .output = {.type = OperandDataType::kFloat32,
                            .dimensions = {1, 2, 1, 1}},
                 .expected = false}
        .Test();
  }
  {
    // Test the invalid graph for output types don't match.
    Conv2dTester{.type = mojom::Conv2d::Kind::kDirect,
                 .input = {.type = OperandDataType::kFloat32,
                           .dimensions = {1, 1, 5, 5}},
                 .filter = {.type = OperandDataType::kFloat32,
                            .dimensions = {1, 1, 3, 3}},
                 .output = {.type = OperandDataType::kFloat16,
                            .dimensions = {1, 1, 3, 3}},
                 .expected = false}
        .Test();
  }
  {
    // Test the invalid graph for input operand == output operand.
    auto context_properties = GetContextPropertiesForTesting();
    GraphInfoBuilder builder;
    uint64_t input_operand_id =
        builder.BuildInput("input", {1, 1, 5, 5}, OperandDataType::kFloat32);
    uint64_t filter_operand_id =
        builder.BuildInput("filter", {1, 1, 3, 3}, OperandDataType::kFloat32);

    builder.BuildConv2d(mojom::Conv2d::Kind::kDirect, input_operand_id,
                        filter_operand_id, input_operand_id,
                        Conv2dTester::Conv2dAttributes{}, std::nullopt);

    EXPECT_FALSE(WebNNGraphBuilderImpl::IsValidForTesting(
        context_properties, builder.GetGraphInfo()));
  }
  {
    // Test the invalid graph for filter operand == output operand.
    auto context_properties = GetContextPropertiesForTesting();
    GraphInfoBuilder builder;
    uint64_t input_operand_id =
        builder.BuildInput("input", {1, 1, 5, 5}, OperandDataType::kFloat32);
    uint64_t filter_operand_id =
        builder.BuildInput("filter", {1, 1, 3, 3}, OperandDataType::kFloat32);

    builder.BuildConv2d(mojom::Conv2d::Kind::kDirect, input_operand_id,
                        filter_operand_id, filter_operand_id,
                        Conv2dTester::Conv2dAttributes{}, std::nullopt);

    EXPECT_FALSE(WebNNGraphBuilderImpl::IsValidForTesting(
        context_properties, builder.GetGraphInfo()));
  }
}

TEST_F(WebNNGraphImplTest, ConvTranspose2dTest) {
  {
    // Test convTranspose2d with default attributes.
    Conv2dTester{.type = mojom::Conv2d::Kind::kTransposed,
                 .input = {.type = OperandDataType::kFloat32,
                           .dimensions = {1, 1, 3, 3}},
                 .filter = {.type = OperandDataType::kFloat32,
                            .dimensions = {1, 1, 3, 3}},
                 .output = {.type = OperandDataType::kFloat32,
                            .dimensions = {1, 1, 5, 5}},
                 .expected = true}
        .Test();
  }
  {
    // Test convTranspose2d with input_layout = kChannelsLast.
    Conv2dTester{.type = mojom::Conv2d::Kind::kTransposed,
                 .input = {.type = OperandDataType::kFloat32,
                           .dimensions = {1, 3, 3, 1}},
                 .filter = {.type = OperandDataType::kFloat32,
                            .dimensions = {1, 3, 3, 1}},
                 .attributes = {.input_layout = InputOperandLayout::kNhwc},
                 .output = {.type = OperandDataType::kFloat32,
                            .dimensions = {1, 5, 5, 1}},
                 .expected = true}
        .Test();
  }
  {
    // Test convTranspose2d with padding = [1, 1, 1, 1].
    Conv2dTester{.type = mojom::Conv2d::Kind::kTransposed,
                 .input = {.type = OperandDataType::kFloat32,
                           .dimensions = {1, 1, 5, 5}},
                 .filter = {.type = OperandDataType::kFloat32,
                            .dimensions = {1, 1, 3, 3}},
                 .attributes = {.padding = {1, 1, 1, 1}},
                 .output = {.type = OperandDataType::kFloat32,
                            .dimensions = {1, 1, 5, 5}},
                 .expected = true}
        .Test();
  }
  {
    // Test convTranspose2d with strides = [2, 2].
    Conv2dTester{.type = mojom::Conv2d::Kind::kTransposed,
                 .input = {.type = OperandDataType::kFloat32,
                           .dimensions = {1, 1, 3, 3}},
                 .filter = {.type = OperandDataType::kFloat32,
                            .dimensions = {1, 2, 3, 3}},
                 .attributes = {.strides = {2, 2}},
                 .output = {.type = OperandDataType::kFloat32,
                            .dimensions = {1, 2, 7, 7}},
                 .expected = true}
        .Test();
  }
  {
    // Test convTranspose2d with strides = [2, 2] and padding = [1, 1, 1,
    // 1].
    Conv2dTester{.type = mojom::Conv2d::Kind::kTransposed,
                 .input = {.type = OperandDataType::kFloat32,
                           .dimensions = {1, 1, 3, 3}},
                 .filter = {.type = OperandDataType::kFloat32,
                            .dimensions = {1, 1, 3, 3}},
                 .attributes = {.padding = {1, 1, 1, 1}, .strides = {2, 2}},
                 .output = {.type = OperandDataType::kFloat32,
                            .dimensions = {1, 1, 5, 5}},
                 .expected = true}
        .Test();
  }
  {
    // Test convTranspose2d with group = 3.
    Conv2dTester{.type = mojom::Conv2d::Kind::kTransposed,
                 .input = {.type = OperandDataType::kFloat32,
                           .dimensions = {1, 1, 3, 3}},
                 .filter = {.type = OperandDataType::kFloat32,
                            .dimensions = {1, 1, 3, 3}},
                 .attributes = {.groups = 3},
                 .output = {.type = OperandDataType::kFloat32,
                            .dimensions = {1, 3, 5, 5}},
                 .expected = true}
        .Test();
  }
  {
    // Test the invalid graph for output types don't match.
    Conv2dTester{
        .type = mojom::Conv2d::Kind::kTransposed,
        .input = {.type = OperandDataType::kFloat32,
                  .dimensions = {1, 1, 5, 5}},
        .filter = {.type = OperandDataType::kFloat32,
                   .dimensions = {1, 1, 3, 3}},
        .output = {.type = OperandDataType::kInt32, .dimensions = {1, 1, 3, 3}},
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph for the input is not a 4-D tensor.
    Conv2dTester{
        .type = mojom::Conv2d::Kind::kTransposed,
        .input = {.type = OperandDataType::kFloat32, .dimensions = {1, 3, 3}},
        .filter = {.type = OperandDataType::kFloat32,
                   .dimensions = {1, 1, 3, 3}},
        .output = {.type = OperandDataType::kInt32, .dimensions = {1, 1, 5, 5}},
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph for the filter is not a 4-D tensor.
    Conv2dTester{
        .type = mojom::Conv2d::Kind::kTransposed,
        .input = {.type = OperandDataType::kFloat32,
                  .dimensions = {1, 1, 3, 3}},
        .filter = {.type = OperandDataType::kFloat32, .dimensions = {1, 3, 3}},
        .output = {.type = OperandDataType::kInt32, .dimensions = {1, 1, 5, 5}},
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph when the number of input channels is not equal
    // to the number of filter input channels.
    Conv2dTester{.type = mojom::Conv2d::Kind::kTransposed,
                 .input = {.type = OperandDataType::kFloat32,
                           .dimensions = {1, 1, 3, 3}},
                 .filter = {.type = OperandDataType::kFloat32,
                            .dimensions = {3, 1, 3, 3}},
                 .attributes = {.groups = 3},
                 .output = {.type = OperandDataType::kFloat32,
                            .dimensions = {1, 3, 5, 5}},
                 .expected = false}
        .Test();
  }
  {
    // Test the invalid graph when the number of output channels doesn't
    // match the result of filter output channels multiplied by groups
    Conv2dTester{
        .type = mojom::Conv2d::Kind::kTransposed,
        .input = {.type = OperandDataType::kFloat32,
                  .dimensions = {1, 1, 3, 3}},
        .filter = {.type = OperandDataType::kFloat32,
                   .dimensions = {1, 1, 3, 3}},
        .attributes = {.groups = 3},
        .output = {.type = OperandDataType::kInt32, .dimensions = {1, 1, 5, 5}},
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph when the filter type doesn't match the input
    // type.
    Conv2dTester{
        .type = mojom::Conv2d::Kind::kTransposed,
        .input = {.type = OperandDataType::kFloat32,
                  .dimensions = {1, 1, 3, 3}},
        .filter = {.type = OperandDataType::kInt32, .dimensions = {1, 1, 3, 3}},
        .output = {.type = OperandDataType::kFloat32,
                   .dimensions = {1, 1, 5, 5}},
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph when the bias type doesn't match input type.
    Conv2dTester{
        .type = mojom::Conv2d::Kind::kTransposed,
        .input = {.type = OperandDataType::kFloat32,
                  .dimensions = {1, 1, 3, 3}},
        .filter = {.type = OperandDataType::kFloat32,
                   .dimensions = {1, 1, 3, 3}},
        .attributes = {.bias = OperandInfo{.type = OperandDataType::kInt32,
                                           .dimensions = {1}}},
        .output = {.type = OperandDataType::kFloat32,
                   .dimensions = {1, 1, 5, 5}},
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph when the bias shape is not equal to
    // [output_channels].
    Conv2dTester{
        .type = mojom::Conv2d::Kind::kTransposed,
        .input = {.type = OperandDataType::kFloat32,
                  .dimensions = {1, 1, 3, 3}},
        .filter = {.type = OperandDataType::kFloat32,
                   .dimensions = {1, 1, 3, 3}},
        .attributes = {.bias = OperandInfo{.type = OperandDataType::kFloat32,
                                           .dimensions = {2}}},
        .output = {.type = OperandDataType::kFloat32,
                   .dimensions = {1, 1, 5, 5}},
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph for input operand == output operand.
    auto context_properties = GetContextPropertiesForTesting();
    GraphInfoBuilder builder;
    uint64_t input_operand_id =
        builder.BuildInput("input", {1, 1, 3, 3}, OperandDataType::kFloat32);
    uint64_t filter_operand_id =
        builder.BuildInput("filter", {1, 1, 3, 3}, OperandDataType::kFloat32);

    builder.BuildConv2d(mojom::Conv2d::Kind::kTransposed, input_operand_id,
                        filter_operand_id, input_operand_id,
                        Conv2dTester::Conv2dAttributes{}, std::nullopt);

    EXPECT_FALSE(WebNNGraphBuilderImpl::IsValidForTesting(
        context_properties, builder.GetGraphInfo()));
  }
  {
    // Test the invalid graph for filter operand == output operand.
    auto context_properties = GetContextPropertiesForTesting();
    GraphInfoBuilder builder;
    uint64_t input_operand_id =
        builder.BuildInput("input", {1, 1, 3, 3}, OperandDataType::kFloat32);
    uint64_t filter_operand_id =
        builder.BuildInput("filter", {1, 1, 3, 3}, OperandDataType::kFloat32);

    builder.BuildConv2d(mojom::Conv2d::Kind::kTransposed, input_operand_id,
                        filter_operand_id, filter_operand_id,
                        Conv2dTester::Conv2dAttributes{}, std::nullopt);

    EXPECT_FALSE(WebNNGraphBuilderImpl::IsValidForTesting(
        context_properties, builder.GetGraphInfo()));
  }
}

struct CumulativeSumTester {
  OperandInfo input;
  uint32_t axis;
  std::optional<bool> exclusive;
  std::optional<bool> reversed;
  OperandInfo output;
  bool expected;

  void Test() {
    auto context_properties = GetContextPropertiesForTesting();

    // Build the graph with mojo type.
    GraphInfoBuilder builder;
    uint64_t input_operand_id =
        builder.BuildInput("input", input.dimensions, input.type);
    uint64_t output_operand_id =
        builder.BuildOutput("output", output.dimensions, output.type);
    builder.BuildCumulativeSum(input_operand_id, output_operand_id, axis,
                               exclusive, reversed);
    EXPECT_EQ(WebNNGraphBuilderImpl::IsValidForTesting(context_properties,
                                                       builder.GetGraphInfo()),
              expected);
  }
};

TEST_F(WebNNGraphImplTest, CumulativeSumTeste) {
  {
    // Test cumulativeSum operator with default exclusive and reversed values.
    CumulativeSumTester{
        .input = {.type = OperandDataType::kFloat32, .dimensions = {3, 4}},
        .axis = 0,
        .output = {.type = OperandDataType::kFloat32, .dimensions = {3, 4}},
        .expected = true}
        .Test();
  }
  {
    // Test cumulativeSum operator with exclusive and reversed.
    CumulativeSumTester{.input = {.type = OperandDataType::kFloat32,
                                  .dimensions = {1, 2, 3, 4}},
                        .axis = 0,
                        .exclusive = true,
                        .reversed = true,
                        .output = {.type = OperandDataType::kFloat32,
                                   .dimensions = {1, 2, 3, 4}},
                        .expected = true}
        .Test();
  }
  {
    // Test cumulativeSum operator with axis=2.
    CumulativeSumTester{
        .input = {.type = OperandDataType::kFloat32, .dimensions = {2, 3, 4}},
        .axis = 2,
        .output = {.type = OperandDataType::kFloat32, .dimensions = {2, 3, 4}},
        .expected = true}
        .Test();
  }
  {
    // Test the invalid graph when the input is a scalar.
    CumulativeSumTester{
        .input = {.type = OperandDataType::kFloat32, .dimensions = {}},
        .axis = 0,
        .output = {.type = OperandDataType::kFloat16, .dimensions = {}},
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph with an invalid axis.
    CumulativeSumTester{
        .input = {.type = OperandDataType::kFloat32, .dimensions = {2, 3, 4}},
        .axis = 3,
        .output = {.type = OperandDataType::kFloat16, .dimensions = {2, 3, 4}},
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph when output type doesn't match input type.
    CumulativeSumTester{
        .input = {.type = OperandDataType::kFloat32, .dimensions = {2, 3, 4}},
        .axis = 2,
        .output = {.type = OperandDataType::kFloat16, .dimensions = {2, 3, 4}},
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph for input operand == output operand.
    auto context_properties = GetContextPropertiesForTesting();
    GraphInfoBuilder builder;
    uint32_t axis = 0;
    bool exclusive = false;
    bool reversed = false;
    uint64_t input_operand_id =
        builder.BuildInput("input", {1, 1, 3, 3}, OperandDataType::kFloat32);
    builder.BuildCumulativeSum(input_operand_id, input_operand_id, axis,
                               exclusive, reversed);
    EXPECT_FALSE(WebNNGraphBuilderImpl::IsValidForTesting(
        context_properties, builder.GetGraphInfo()));
  }
}

struct DequantizeLinearTester {
  OperandInfo input;
  OperandInfo scale;
  OperandInfo zero_point;
  OperandInfo output;
  bool expected;

  void Test() {
    auto context_properties = GetContextPropertiesForTesting();

    // Build the graph with mojo type.
    GraphInfoBuilder builder;
    uint64_t input_operand_id =
        builder.BuildInput("input", input.dimensions, input.type);
    uint64_t scale_operand_id =
        builder.BuildInput("scale", scale.dimensions, scale.type);
    uint64_t zero_point_operand_id = builder.BuildInput(
        "zero_point", zero_point.dimensions, zero_point.type);
    uint64_t output_operand_id =
        builder.BuildOutput("output", output.dimensions, output.type);
    builder.BuildDequantizeLinear(input_operand_id, scale_operand_id,
                                  zero_point_operand_id, output_operand_id);
    EXPECT_EQ(WebNNGraphBuilderImpl::IsValidForTesting(context_properties,
                                                       builder.GetGraphInfo()),
              expected);
  }
};

TEST_F(WebNNGraphImplTest, DequantizeLinearTest) {
  {
    // Test dequantizeLinear operator when the input, the scale and the
    // zero_point have the same shape.
    DequantizeLinearTester{
        .input = {.type = OperandDataType::kInt8, .dimensions = {3, 2, 5}},
        .scale = {.type = OperandDataType::kFloat32, .dimensions = {3, 2, 5}},
        .zero_point = {.type = OperandDataType::kInt8, .dimensions = {3, 2, 5}},
        .output = {.type = OperandDataType::kFloat32, .dimensions = {3, 2, 5}},
        .expected = true}
        .Test();
  }
  {
    // Test dequantizeLinear operator with a broadcastable scale.
    DequantizeLinearTester{
        .input = {.type = OperandDataType::kInt8, .dimensions = {3, 2, 5}},
        .scale = {.type = OperandDataType::kFloat32, .dimensions = {5}},
        .zero_point = {.type = OperandDataType::kInt8, .dimensions = {3, 2, 5}},
        .output = {.type = OperandDataType::kFloat32, .dimensions = {3, 2, 5}},
        .expected = true}
        .Test();
  }
  {
    // Test dequantizeLinear operator with a broadcastable scale.
    DequantizeLinearTester{
        .input = {.type = OperandDataType::kInt8, .dimensions = {3, 2, 5}},
        .scale = {.type = OperandDataType::kFloat32, .dimensions = {3, 1, 1}},
        .zero_point = {.type = OperandDataType::kInt8, .dimensions = {3, 2, 5}},
        .output = {.type = OperandDataType::kFloat32, .dimensions = {3, 2, 5}},
        .expected = true}
        .Test();
  }
  {
    // Test dequantizeLinear operator with a broadcastable zeroPoint.
    DequantizeLinearTester{
        .input = {.type = OperandDataType::kInt8, .dimensions = {3, 2, 5}},
        .scale = {.type = OperandDataType::kFloat32, .dimensions = {3, 2, 5}},
        .zero_point = {.type = OperandDataType::kInt8, .dimensions = {5}},
        .output = {.type = OperandDataType::kFloat32, .dimensions = {3, 2, 5}},
        .expected = true}
        .Test();
  }
  {
    // Test the invalid graph with an invalid scale.
    DequantizeLinearTester{
        .input = {.type = OperandDataType::kInt8, .dimensions = {3, 2, 5}},
        .scale = {.type = OperandDataType::kFloat32, .dimensions = {2}},
        .zero_point = {.type = OperandDataType::kInt8, .dimensions = {3, 2, 5}},
        .output = {.type = OperandDataType::kFloat32, .dimensions = {3, 2, 5}},
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph with an invalid zero_point.
    DequantizeLinearTester{
        .input = {.type = OperandDataType::kInt8, .dimensions = {3, 2, 5}},
        .scale = {.type = OperandDataType::kFloat32, .dimensions = {3, 2, 5}},
        .zero_point = {.type = OperandDataType::kInt8, .dimensions = {2}},
        .output = {.type = OperandDataType::kFloat32, .dimensions = {3, 2, 5}},
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph when the zero_point datatype doesn't match the
    // input's datatype.
    DequantizeLinearTester{
        .input = {.type = OperandDataType::kInt8, .dimensions = {3, 2, 5}},
        .scale = {.type = OperandDataType::kFloat32, .dimensions = {3, 2, 5}},
        .zero_point = {.type = OperandDataType::kUint8,
                       .dimensions = {3, 2, 5}},
        .output = {.type = OperandDataType::kFloat32, .dimensions = {3, 2, 5}},
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph when the output datatype doesn't match the
    // scale's datatype.
    DequantizeLinearTester{
        .input = {.type = OperandDataType::kInt8, .dimensions = {3, 2, 5}},
        .scale = {.type = OperandDataType::kFloat32, .dimensions = {3, 2, 5}},
        .zero_point = {.type = OperandDataType::kInt8, .dimensions = {3, 2, 5}},
        .output = {.type = OperandDataType::kFloat16, .dimensions = {3, 2, 5}},
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph for the output shapes are not expected.
    DequantizeLinearTester{
        .input = {.type = OperandDataType::kInt8, .dimensions = {3, 2, 5}},
        .scale = {.type = OperandDataType::kFloat32, .dimensions = {5}},
        .zero_point = {.type = OperandDataType::kInt8, .dimensions = {5}},
        .output = {.type = OperandDataType::kFloat16, .dimensions = {5}},
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph when the input is as same as output.
    auto context_properties = GetContextPropertiesForTesting();
    GraphInfoBuilder builder;
    uint64_t input_operand_id =
        builder.BuildInput("input", {2, 3}, OperandDataType::kInt8);
    uint64_t scale_operand_id =
        builder.BuildInput("scale", {2, 3}, OperandDataType::kFloat32);
    uint64_t zero_point_operand_id =
        builder.BuildInput("zero_point", {2, 3}, OperandDataType::kInt8);
    builder.BuildDequantizeLinear(input_operand_id, scale_operand_id,
                                  zero_point_operand_id, input_operand_id);
    EXPECT_FALSE(WebNNGraphBuilderImpl::IsValidForTesting(
        context_properties, builder.GetGraphInfo()));
  }
  {
    // Test the invalid graph when the scale is as same as output.
    auto context_properties = GetContextPropertiesForTesting();
    GraphInfoBuilder builder;
    uint64_t input_operand_id =
        builder.BuildInput("input", {2, 3}, OperandDataType::kInt8);
    uint64_t scale_operand_id =
        builder.BuildInput("scale", {2, 3}, OperandDataType::kFloat32);
    uint64_t zero_point_operand_id =
        builder.BuildInput("zero_point", {2, 3}, OperandDataType::kInt8);
    builder.BuildDequantizeLinear(input_operand_id, scale_operand_id,
                                  zero_point_operand_id, scale_operand_id);
    EXPECT_FALSE(WebNNGraphBuilderImpl::IsValidForTesting(
        context_properties, builder.GetGraphInfo()));
  }
  {
    // Test the invalid graph when the zeroPoint is as same as output.
    auto context_properties = GetContextPropertiesForTesting();
    GraphInfoBuilder builder;
    uint64_t input_operand_id =
        builder.BuildInput("input", {2, 3}, OperandDataType::kInt8);
    uint64_t scale_operand_id =
        builder.BuildInput("scale", {2, 3}, OperandDataType::kFloat32);
    uint64_t zero_point_operand_id =
        builder.BuildInput("zero_point", {2, 3}, OperandDataType::kInt8);
    builder.BuildDequantizeLinear(input_operand_id, scale_operand_id,
                                  zero_point_operand_id, zero_point_operand_id);
    EXPECT_FALSE(WebNNGraphBuilderImpl::IsValidForTesting(
        context_properties, builder.GetGraphInfo()));
  }
}

struct ElementWiseBinaryTester {
  mojom::ElementWiseBinary::Kind kind;
  OperandInfo lhs;
  OperandInfo rhs;
  OperandInfo output;
  bool expected;

  static constexpr std::array<mojom::ElementWiseBinary::Kind, 15>
      kAllBinaryOps = {
          mojom::ElementWiseBinary::Kind::kAdd,
          mojom::ElementWiseBinary::Kind::kSub,
          mojom::ElementWiseBinary::Kind::kMul,
          mojom::ElementWiseBinary::Kind::kDiv,
          mojom::ElementWiseBinary::Kind::kPow,
          mojom::ElementWiseBinary::Kind::kMax,
          mojom::ElementWiseBinary::Kind::kMin,
          mojom::ElementWiseBinary::Kind::kEqual,
          mojom::ElementWiseBinary::Kind::kGreater,
          mojom::ElementWiseBinary::Kind::kGreaterOrEqual,
          mojom::ElementWiseBinary::Kind::kLesser,
          mojom::ElementWiseBinary::Kind::kLesserOrEqual,
          mojom::ElementWiseBinary::Kind::kLogicalAnd,
          mojom::ElementWiseBinary::Kind::kLogicalOr,
          mojom::ElementWiseBinary::Kind::kLogicalXor,
  };

  static OperandDataType GetValidInputType(mojom::ElementWiseBinary::Kind op) {
    switch (op) {
      case mojom::ElementWiseBinary::Kind::kAdd:
      case mojom::ElementWiseBinary::Kind::kSub:
      case mojom::ElementWiseBinary::Kind::kMul:
      case mojom::ElementWiseBinary::Kind::kDiv:
      case mojom::ElementWiseBinary::Kind::kPow:
      case mojom::ElementWiseBinary::Kind::kMax:
      case mojom::ElementWiseBinary::Kind::kMin:
      case mojom::ElementWiseBinary::Kind::kEqual:
      case mojom::ElementWiseBinary::Kind::kGreater:
      case mojom::ElementWiseBinary::Kind::kGreaterOrEqual:
      case mojom::ElementWiseBinary::Kind::kLesser:
      case mojom::ElementWiseBinary::Kind::kLesserOrEqual:
        return OperandDataType::kFloat32;
      case mojom::ElementWiseBinary::Kind::kLogicalAnd:
      case mojom::ElementWiseBinary::Kind::kLogicalOr:
      case mojom::ElementWiseBinary::Kind::kLogicalXor:
        return OperandDataType::kUint8;
    }
  }

  static OperandDataType GetValidOutputType(mojom::ElementWiseBinary::Kind op) {
    switch (op) {
      case mojom::ElementWiseBinary::Kind::kAdd:
      case mojom::ElementWiseBinary::Kind::kSub:
      case mojom::ElementWiseBinary::Kind::kMul:
      case mojom::ElementWiseBinary::Kind::kDiv:
      case mojom::ElementWiseBinary::Kind::kPow:
      case mojom::ElementWiseBinary::Kind::kMax:
      case mojom::ElementWiseBinary::Kind::kMin:
        return OperandDataType::kFloat32;
      case mojom::ElementWiseBinary::Kind::kEqual:
      case mojom::ElementWiseBinary::Kind::kGreater:
      case mojom::ElementWiseBinary::Kind::kGreaterOrEqual:
      case mojom::ElementWiseBinary::Kind::kLesser:
      case mojom::ElementWiseBinary::Kind::kLesserOrEqual:
      case mojom::ElementWiseBinary::Kind::kLogicalAnd:
      case mojom::ElementWiseBinary::Kind::kLogicalOr:
      case mojom::ElementWiseBinary::Kind::kLogicalXor:
        return OperandDataType::kUint8;
    }
  }

  void Test() {
    auto context_properties = GetContextPropertiesForTesting();

    // Build the graph with mojo type.
    GraphInfoBuilder builder;
    uint64_t lhs_operand_id =
        builder.BuildInput("lhs", lhs.dimensions, lhs.type);
    uint64_t rhs_operand_id =
        builder.BuildInput("rhs", rhs.dimensions, rhs.type);
    uint64_t output_operand_id =
        builder.BuildOutput("output", output.dimensions, output.type);
    builder.BuildElementWiseBinary(kind, lhs_operand_id, rhs_operand_id,
                                   output_operand_id);
    EXPECT_EQ(WebNNGraphBuilderImpl::IsValidForTesting(context_properties,
                                                       builder.GetGraphInfo()),
              expected);
  }
};

TEST_F(WebNNGraphImplTest, ElementWiseBinaryTest) {
  for (const auto& op : ElementWiseBinaryTester::kAllBinaryOps) {
    const OperandDataType valid_input_type =
        ElementWiseBinaryTester::GetValidInputType(op);
    const OperandDataType valid_output_type =
        ElementWiseBinaryTester::GetValidOutputType(op);

  // Testing building with two input dimensions - {8, 1, 6, 1} and {7, 1, 5}.
  // Both the a and b dimensions have axes with length one that are expanded to
  // a larger size during the broadcast operation.
  // a_dimensions     (4d) 8 * 1 * 6 * 1
  // b_dimensions     (3d)     7 * 1 * 5
  // output_dimenions (4d) 8 * 7 * 6 * 5
    {
      ElementWiseBinaryTester{
          .kind = op,
          .lhs = {.type = valid_input_type, .dimensions = {8, 1, 6, 1}},
          .rhs = {.type = valid_input_type, .dimensions = {7, 1, 5}},
          .output = {.type = valid_output_type, .dimensions = {8, 7, 6, 5}},
          .expected = true}
          .Test();
    }

    // Testing building with two input dimensions - {4, 2, 1} and {4}.
    // a_dimensions     (3d) 4 * 2 * 1
    // b_dimensions     (1d)         4
    // output_dimenions (3d) 4 * 2 * 4
    {
      ElementWiseBinaryTester{
          .kind = op,
          .lhs = {.type = valid_input_type, .dimensions = {4, 2, 1}},
          .rhs = {.type = valid_input_type, .dimensions = {4}},
          .output = {.type = valid_output_type, .dimensions = {4, 2, 4}},
          .expected = true}
          .Test();
    }

    // Test the invalid graph for the input shapes are not broadcastable.
    {
      ElementWiseBinaryTester{
          .kind = op,
          .lhs = {.type = valid_input_type, .dimensions = {4, 2}},
          .rhs = {.type = valid_input_type, .dimensions = {4}},
          .output = {.type = valid_output_type, .dimensions = {4, 2}},
          .expected = false}
          .Test();
    }

    // Test the invalid graph for the output shapes are not expected.
    {
      ElementWiseBinaryTester{
          .kind = op,
          .lhs = {.type = valid_input_type, .dimensions = {4, 2}},
          .rhs = {.type = valid_input_type, .dimensions = {4, 2}},
          .output = {.type = valid_output_type, .dimensions = {2}},
          .expected = false}
          .Test();
    }

    // Test the invalid graph for input types don't match.
    {
      ElementWiseBinaryTester{
          .kind = op,
          .lhs = {.type = valid_input_type, .dimensions = {2}},
          .rhs = {.type = OperandDataType::kInt64, .dimensions = {2}},
          .output = {.type = valid_output_type, .dimensions = {2}},
          .expected = false}
          .Test();
    }

    // Test the invalid graph for output types don't match.
    {
      ElementWiseBinaryTester{
          .kind = op,
          .lhs = {.type = valid_input_type, .dimensions = {2}},
          .rhs = {.type = valid_input_type, .dimensions = {2}},
          .output = {.type = OperandDataType::kInt64, .dimensions = {2}},
          .expected = false}
          .Test();
    }
  }
}

struct ElementWiseUnaryTester {
  mojom::ElementWiseUnary::Kind kind;
  OperandInfo input;
  OperandInfo output;
  bool expected;

  void Test() {
    auto context_properties = GetContextPropertiesForTesting();

    // Build the graph with mojo type.
    GraphInfoBuilder builder;
    uint64_t input_operand_id =
        builder.BuildInput("input", input.dimensions, input.type);
    uint64_t output_operand_id =
        builder.BuildOutput("output", output.dimensions, output.type);
    builder.BuildElementWiseUnary(kind, input_operand_id, output_operand_id);
    EXPECT_EQ(WebNNGraphBuilderImpl::IsValidForTesting(context_properties,
                                                       builder.GetGraphInfo()),
              expected);
  }
};

// Test the data type support for element-wise unary operators.
// The data type support is defined in the first parameter of the tuple
// as a std::pair of mojom::ElementWiseUnary::Kind and array of
// datatypes supported by the operator.
class ElementWiseUnaryDataTypeFixture
    : public testing::TestWithParam<
          std::tuple<std::pair<mojom::ElementWiseUnary::Kind,
                               std::vector<OperandDataType>>,
                     OperandDataType,
                     OperandDataType>> {
 public:
  // Populate meaningful test suffixes.
  struct PrintToStringParamName {
    template <class ParamType>
    std::string operator()(
        const testing::TestParamInfo<ParamType>& info) const {
      std::string test_name =
          base::StrCat({OpKindToString(std::get<0>(info.param).first), "_",
                        DataTypeToString(std::get<1>(info.param)), "_",
                        DataTypeToString(std::get<2>(info.param))});
      return test_name;
    }
  };

  void TestDataTypeSupportWithDimensions(
      const std::vector<uint32_t>& dimensions) {
    auto [operator_trait, inputDataType, outputDataType] = GetParam();
    const mojom::ElementWiseUnary::Kind& kind = operator_trait.first;
    // Some operators support dissimilar input and output data types.
    const std::set<mojom::ElementWiseUnary::Kind>
        kOperatorsWithDissimilarDatatypeSupport = {
            mojom::ElementWiseUnary::Kind::kCast};

    // Check if data types match, or if the operator supports mismatch.
    // Check if the data type is supported by the operator.
    const bool expected =
        (inputDataType == outputDataType ||
         kOperatorsWithDissimilarDatatypeSupport.contains(kind)) &&
        base::Contains(operator_trait.second, inputDataType);

    ElementWiseUnaryTester{
        .kind = kind,
        .input = {.type = inputDataType, .dimensions = dimensions},
        .output = {.type = outputDataType, .dimensions = dimensions},
        .expected = expected}
        .Test();
  }
};

TEST_P(ElementWiseUnaryDataTypeFixture, TestUnaryOperandDataTypeSupport) {
  TestDataTypeSupportWithDimensions(std::vector<uint32_t>{1, 2, 3, 1});
}

TEST_P(ElementWiseUnaryDataTypeFixture, TestUnaryOperandScalarDataTypeSupport) {
  TestDataTypeSupportWithDimensions(std::vector<uint32_t>{});
}

INSTANTIATE_TEST_SUITE_P(
    WebNNGraphImplTest,
    ElementWiseUnaryDataTypeFixture,
    ::testing::Combine(
        ::testing::ValuesIn({
            std::make_pair(mojom::ElementWiseUnary::Kind::kLogicalNot,
                           std::vector<OperandDataType>{
                               OperandDataType::kUint8}),
            std::make_pair(
                mojom::ElementWiseUnary::Kind::kIdentity,
                std::vector<OperandDataType>(kAllOperandDataTypes,
                                             std::end(kAllOperandDataTypes))),
            std::make_pair(mojom::ElementWiseUnary::Kind::kSqrt,
                           std::vector<OperandDataType>{
                               OperandDataType::kFloat16,
                               OperandDataType::kFloat32}),
            std::make_pair(mojom::ElementWiseUnary::Kind::kErf,
                           std::vector<OperandDataType>{
                               OperandDataType::kFloat16,
                               OperandDataType::kFloat32}),
            std::make_pair(mojom::ElementWiseUnary::Kind::kReciprocal,
                           std::vector<OperandDataType>{
                               OperandDataType::kFloat16,
                               OperandDataType::kFloat32}),
            std::make_pair(
                mojom::ElementWiseUnary::Kind::kCast,
                std::vector<OperandDataType>(kAllOperandDataTypes,
                                             std::end(kAllOperandDataTypes))),
        }),
        ::testing::ValuesIn(kAllOperandDataTypes),
        ::testing::ValuesIn(kAllOperandDataTypes)),
    ElementWiseUnaryDataTypeFixture::PrintToStringParamName());

TEST_F(WebNNGraphImplTest, ElementWiseUnaryTest) {
  {
    // Test building element-wise abs.
    ElementWiseUnaryTester{
        .kind = mojom::ElementWiseUnary::Kind::kAbs,
        .input = {.type = OperandDataType::kFloat32, .dimensions = {1}},
        .output = {.type = OperandDataType::kFloat32, .dimensions = {1}},
        .expected = true}
        .Test();
  }
  {
    // Test building element-wise ceil.
    ElementWiseUnaryTester{
        .kind = mojom::ElementWiseUnary::Kind::kCeil,
        .input = {.type = OperandDataType::kFloat16, .dimensions = {1}},
        .output = {.type = OperandDataType::kFloat16, .dimensions = {1}},
        .expected = true}
        .Test();
  }
  {
    // Test building element-wise cos.
    ElementWiseUnaryTester{
        .kind = mojom::ElementWiseUnary::Kind::kCos,
        .input = {.type = OperandDataType::kFloat32, .dimensions = {1, 2}},
        .output = {.type = OperandDataType::kFloat32, .dimensions = {1, 2}},
        .expected = true}
        .Test();
  }
  {
    // Test building element-wise exp.
    ElementWiseUnaryTester{
        .kind = mojom::ElementWiseUnary::Kind::kExp,
        .input = {.type = OperandDataType::kFloat16, .dimensions = {1, 2}},
        .output = {.type = OperandDataType::kFloat16, .dimensions = {1, 2}},
        .expected = true}
        .Test();
  }
  {
    // Test building element-wise floor.
    ElementWiseUnaryTester{
        .kind = mojom::ElementWiseUnary::Kind::kFloor,
        .input = {.type = OperandDataType::kFloat32, .dimensions = {1, 2, 3}},
        .output = {.type = OperandDataType::kFloat32, .dimensions = {1, 2, 3}},
        .expected = true}
        .Test();
  }
  {
    // Test building element-wise log.
    ElementWiseUnaryTester{
        .kind = mojom::ElementWiseUnary::Kind::kLog,
        .input = {.type = OperandDataType::kFloat16, .dimensions = {1, 2, 3}},
        .output = {.type = OperandDataType::kFloat16, .dimensions = {1, 2, 3}},
        .expected = true}
        .Test();
  }
  {
    // Test building element-wise neg.
    ElementWiseUnaryTester{.kind = mojom::ElementWiseUnary::Kind::kNeg,
                           .input = {.type = OperandDataType::kFloat32,
                                     .dimensions = {1, 2, 3, 4}},
                           .output = {.type = OperandDataType::kFloat32,
                                      .dimensions = {1, 2, 3, 4}},
                           .expected = true}
        .Test();
  }
  {
    // Test building element-wise sin.
    ElementWiseUnaryTester{.kind = mojom::ElementWiseUnary::Kind::kSin,
                           .input = {.type = OperandDataType::kFloat16,
                                     .dimensions = {1, 2, 3, 4}},
                           .output = {.type = OperandDataType::kFloat16,
                                      .dimensions = {1, 2, 3, 4}},
                           .expected = true}
        .Test();
  }
  {
    // Test building element-wise tan.
    ElementWiseUnaryTester{.kind = mojom::ElementWiseUnary::Kind::kTan,
                           .input = {.type = OperandDataType::kFloat32,
                                     .dimensions = {1, 2, 3, 4, 5}},
                           .output = {.type = OperandDataType::kFloat32,
                                      .dimensions = {1, 2, 3, 4, 5}},
                           .expected = true}
        .Test();
  }
  {
    // Test the invalid element-wise abs graph for the input with
    // unsupported data type.
    ElementWiseUnaryTester{
        .kind = mojom::ElementWiseUnary::Kind::kAbs,
        .input = {.type = OperandDataType::kUint32, .dimensions = {1, 2, 3, 4}},
        .output = {.type = OperandDataType::kUint32,
                   .dimensions = {1, 2, 3, 4}},
        .expected = false}
        .Test();
  }
  {
    // Test the invalid element-wise neg graph for the input with
    // unsupported data type.
    ElementWiseUnaryTester{
        .kind = mojom::ElementWiseUnary::Kind::kNeg,
        .input = {.type = OperandDataType::kUint8, .dimensions = {1, 2, 3, 4}},
        .output = {.type = OperandDataType::kUint8, .dimensions = {1, 2, 3, 4}},
        .expected = false}
        .Test();
  }
  {
    // Test the invalid element-wise ceil graph for the input with
    // unsupported data type.
    ElementWiseUnaryTester{
        .kind = mojom::ElementWiseUnary::Kind::kCeil,
        .input = {.type = OperandDataType::kUint32, .dimensions = {1, 2, 3, 4}},
        .output = {.type = OperandDataType::kUint32,
                   .dimensions = {1, 2, 3, 4}},
        .expected = false}
        .Test();
  }
  {
    // Test the invalid element-wise cos graph for the input with
    // unsupported data type.
    ElementWiseUnaryTester{
        .kind = mojom::ElementWiseUnary::Kind::kCos,
        .input = {.type = OperandDataType::kUint32, .dimensions = {1, 2, 3, 4}},
        .output = {.type = OperandDataType::kUint32,
                   .dimensions = {1, 2, 3, 4}},
        .expected = false}
        .Test();
  }
  {
    // Test the invalid element-wise exp graph for the input with
    // unsupported data type.
    ElementWiseUnaryTester{
        .kind = mojom::ElementWiseUnary::Kind::kExp,
        .input = {.type = OperandDataType::kUint8, .dimensions = {1, 2, 3, 4}},
        .output = {.type = OperandDataType::kUint8, .dimensions = {1, 2, 3, 4}},
        .expected = false}
        .Test();
  }
  {
    // Test the invalid element-wise floor graph for the input with
    // unsupported data type.
    ElementWiseUnaryTester{
        .kind = mojom::ElementWiseUnary::Kind::kFloor,
        .input = {.type = OperandDataType::kInt8, .dimensions = {1, 2, 3, 4}},
        .output = {.type = OperandDataType::kInt8, .dimensions = {1, 2, 3, 4}},
        .expected = false}
        .Test();
  }
  {
    // Test the invalid element-wise log graph for the input with
    // unsupported data type.
    ElementWiseUnaryTester{
        .kind = mojom::ElementWiseUnary::Kind::kLog,
        .input = {.type = OperandDataType::kInt32, .dimensions = {1, 2, 3, 4}},
        .output = {.type = OperandDataType::kInt32, .dimensions = {1, 2, 3, 4}},
        .expected = false}
        .Test();
  }
  {
    // Test the invalid element-wise sin graph for the input with
    // unsupported data type.
    ElementWiseUnaryTester{
        .kind = mojom::ElementWiseUnary::Kind::kSin,
        .input = {.type = OperandDataType::kUint32, .dimensions = {1, 2, 3, 4}},
        .output = {.type = OperandDataType::kUint32,
                   .dimensions = {1, 2, 3, 4}},
        .expected = false}
        .Test();
  }
  {
    // Test the invalid element-wise tan graph for the input with
    // unsupported data type.
    ElementWiseUnaryTester{
        .kind = mojom::ElementWiseUnary::Kind::kTan,
        .input = {.type = OperandDataType::kUint32, .dimensions = {1, 2, 3, 4}},
        .output = {.type = OperandDataType::kUint32,
                   .dimensions = {1, 2, 3, 4}},
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph for the input and output shapes don't match.
    ElementWiseUnaryTester{.kind = mojom::ElementWiseUnary::Kind::kAbs,
                           .input = {.type = OperandDataType::kFloat32,
                                     .dimensions = {1, 2, 3, 4}},
                           .output = {.type = OperandDataType::kFloat32,
                                      .dimensions = {1, 2, 3, 4, 5}},
                           .expected = false}
        .Test();
  }
  {
    // Test the invalid graph for output type don't match.
    ElementWiseUnaryTester{.kind = mojom::ElementWiseUnary::Kind::kCeil,
                           .input = {.type = OperandDataType::kFloat32,
                                     .dimensions = {1, 2, 3, 4}},
                           .output = {.type = OperandDataType::kFloat16,
                                      .dimensions = {1, 2, 3, 4}},
                           .expected = false}
        .Test();
  }
  // Test case for cast where dimensions don't match
  {
    ElementWiseUnaryTester{
        .kind = mojom::ElementWiseUnary::Kind::kCast,
        .input = {.type = OperandDataType::kUint8, .dimensions = {1, 2, 3, 1}},
        .output = {.type = OperandDataType::kInt8, .dimensions = {1, 2, 3, 2}},
        .expected = false}
        .Test();
  }
}

struct EluTester {
  OperandInfo input;
  OperandInfo output;
  float alpha = 1.0;
  bool expected;

  void Test() {
    auto context_properties = GetContextPropertiesForTesting();

    // Build the graph with mojo type.
    GraphInfoBuilder builder;
    uint64_t input_operand_id =
        builder.BuildInput("input", input.dimensions, input.type);
    uint64_t output_operand_id =
        builder.BuildOutput("output", output.dimensions, output.type);
    builder.BuildElu(input_operand_id, output_operand_id, alpha);

    EXPECT_EQ(WebNNGraphBuilderImpl::IsValidForTesting(context_properties,
                                                       builder.GetGraphInfo()),
              expected);
  }
};

TEST_F(WebNNGraphImplTest, EluTest) {
  {
    // Test elu operator for 2-D tensor with float32 input.
    EluTester{
        .input = {.type = OperandDataType::kFloat32, .dimensions = {2, 6}},
        .output = {.type = OperandDataType::kFloat32, .dimensions = {2, 6}},
        .expected = true}
        .Test();
  }
  {
    // Test the invalid graph when the alpha is less than or equal to 0.
    EluTester{.input = {.type = OperandDataType::kFloat32, .dimensions = {2}},
              .output = {.type = OperandDataType::kFloat32, .dimensions = {2}},
              .alpha = 0,
              .expected = false}
        .Test();
  }
  {
    // Test the invalid graph when the alpha is NAN.
    EluTester{.input = {.type = OperandDataType::kFloat32, .dimensions = {2}},
              .output = {.type = OperandDataType::kFloat32, .dimensions = {2}},
              .alpha = NAN,
              .expected = false}
        .Test();
  }
  {
    // Test the invalid graph for the output shapes are not as expected.
    EluTester{
        .input = {.type = OperandDataType::kFloat32, .dimensions = {4, 2}},
        .output = {.type = OperandDataType::kFloat32, .dimensions = {2}},
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph for output data types which don't match.
    EluTester{.input = {.type = OperandDataType::kFloat32, .dimensions = {2}},
              .output = {.type = OperandDataType::kInt32, .dimensions = {2}},
              .expected = false}
        .Test();
  }
  {
    // Test the invalid graph when the input data type is not floating
    // point.
    EluTester{.input = {.type = OperandDataType::kInt32, .dimensions = {2}},
              .output = {.type = OperandDataType::kInt32, .dimensions = {2}},
              .expected = false}
        .Test();
  }
  {
    // Test the invalid graph when the input is as same as output.
    auto context_properties = GetContextPropertiesForTesting();
    GraphInfoBuilder builder;
    uint64_t input_operand_id =
        builder.BuildInput("input", {2}, OperandDataType::kFloat32);
    builder.BuildElu(input_operand_id, input_operand_id, /*alpha*/ 1.0);
    EXPECT_FALSE(WebNNGraphBuilderImpl::IsValidForTesting(
        context_properties, builder.GetGraphInfo()));
  }
}

struct ExpandTester {
  OperandInfo input;
  OperandInfo output;
  bool expected;

  void Test() {
    auto context_properties = GetContextPropertiesForTesting();

    // Build the graph with mojo type.
    GraphInfoBuilder builder;
    uint64_t input_operand_id =
        builder.BuildInput("input", input.dimensions, input.type);
    uint64_t output_operand_id =
        builder.BuildOutput("output", output.dimensions, output.type);
    builder.BuildExpand(input_operand_id, output_operand_id);

    EXPECT_EQ(WebNNGraphBuilderImpl::IsValidForTesting(context_properties,
                                                       builder.GetGraphInfo()),
              expected);
  }
};

TEST_F(WebNNGraphImplTest, ExpandTest) {
  {
    // Test building expand with the output shapes that are the same as
    // input.
    ExpandTester{
        .input = {.type = OperandDataType::kFloat32, .dimensions = {2, 6}},
        .output = {.type = OperandDataType::kFloat32, .dimensions = {2, 6}},
        .expected = true}
        .Test();
  }
  {
    // Test building expand with the output shapes that are broadcastable.
    ExpandTester{
        .input = {.type = OperandDataType::kInt32, .dimensions = {3, 1, 5}},
        .output = {.type = OperandDataType::kInt32, .dimensions = {3, 4, 5}},
        .expected = true}
        .Test();
  }
  {
    // Test building expand with the output shapes that are broadcastable
    // and the number of output shapes larger than input.
    ExpandTester{
        .input = {.type = OperandDataType::kInt32, .dimensions = {2, 5}},
        .output = {.type = OperandDataType::kInt32, .dimensions = {3, 2, 5}},
        .expected = true}
        .Test();
  }
  {
    // Test the invalid graph when the input shapes are not the same as
    // output shape and not broadcastable.
    ExpandTester{
        .input = {.type = OperandDataType::kFloat32, .dimensions = {3, 6, 2}},
        .output = {.type = OperandDataType::kFloat32, .dimensions = {4, 3, 5}},
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph when the input shapes are not broadcastable.
    ExpandTester{
        .input = {.type = OperandDataType::kInt32, .dimensions = {5}},
        .output = {.type = OperandDataType::kInt32, .dimensions = {5, 4}},
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph for output data types which don't match.
    ExpandTester{
        .input = {.type = OperandDataType::kFloat32, .dimensions = {2}},
        .output = {.type = OperandDataType::kInt32, .dimensions = {2}},
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph when the input is as same as output.
    auto context_properties = GetContextPropertiesForTesting();
    GraphInfoBuilder builder;
    uint64_t input_operand_id =
        builder.BuildInput("input", {2}, OperandDataType::kFloat32);
    builder.BuildExpand(input_operand_id, input_operand_id);
    EXPECT_FALSE(WebNNGraphBuilderImpl::IsValidForTesting(
        context_properties, builder.GetGraphInfo()));
  }
}

struct GatherAttributes {
  OperandInfo indices;
  uint32_t axis;
};

struct GatherTester {
  OperandInfo input;
  GatherAttributes attributes;
  OperandInfo output;
  bool expected;

  void Test() {
    auto context_properties = GetContextPropertiesForTesting();

    // Build the graph with mojo type.
    GraphInfoBuilder builder;
    uint64_t input_operand_id =
        builder.BuildInput("input", input.dimensions, input.type);
    uint64_t indices_operand_id = builder.BuildInput(
        "indices", attributes.indices.dimensions, attributes.indices.type);
    uint64_t output_operand_id =
        builder.BuildOutput("output", output.dimensions, output.type);
    builder.BuildGather(input_operand_id, indices_operand_id, output_operand_id,
                        attributes.axis);
    EXPECT_EQ(WebNNGraphBuilderImpl::IsValidForTesting(context_properties,
                                                       builder.GetGraphInfo()),
              expected);
  }
};

TEST_F(WebNNGraphImplTest, GatherTest) {
  {
    // Test gather operator with 3-D input and 2-D indices.
    GatherTester{
        .input = {.type = OperandDataType::kFloat32, .dimensions = {3, 4, 5}},
        .attributes = {.indices = {.type = OperandDataType::kUint32,
                                   .dimensions = {6, 7}},
                       .axis = 1},
        .output = {.type = OperandDataType::kFloat32,
                   .dimensions = {3, 6, 7, 5}},
        .expected = true}
        .Test();
  }
  {
    // Test the invalid graph for the axis is too large.
    GatherTester{
        .input = {.type = OperandDataType::kFloat16, .dimensions = {3, 4, 5}},
        .attributes = {.indices = {.type = OperandDataType::kUint32,
                                   .dimensions = {6, 7}},
                       .axis = 3},
        .output = {.type = OperandDataType::kFloat16,
                   .dimensions = {3, 4, 5, 6, 7}},
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph for the indices data type is floating point.
    GatherTester{
        .input = {.type = OperandDataType::kFloat32, .dimensions = {3, 4, 5}},
        .attributes = {.indices = {.type = OperandDataType::kFloat16,
                                   .dimensions = {6, 7}},
                       .axis = 1},
        .output = {.type = OperandDataType::kFloat32,
                   .dimensions = {3, 6, 7, 5}},
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph for the indices data type is not one of uint32,
    // int32 or int64.
    GatherTester{
        .input = {.type = OperandDataType::kFloat32, .dimensions = {3, 4, 5}},
        .attributes = {.indices = {.type = OperandDataType::kFloat32,
                                   .dimensions = {6, 7}},
                       .axis = 1},
        .output = {.type = OperandDataType::kFloat32,
                   .dimensions = {3, 6, 7, 5}},
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph for the output shapes are not expected.
    GatherTester{
        .input = {.type = OperandDataType::kFloat32, .dimensions = {3, 4, 5}},
        .attributes = {.indices = {.type = OperandDataType::kUint32,
                                   .dimensions = {6, 7}},
                       .axis = 1},
        .output = {.type = OperandDataType::kFloat32,
                   .dimensions = {3, 4, 6, 7, 5}},
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph for output types don't match.
    GatherTester{
        .input = {.type = OperandDataType::kFloat32, .dimensions = {3, 4, 5}},
        .attributes = {.indices = {.type = OperandDataType::kUint32,
                                   .dimensions = {6, 7}},
                       .axis = 1},
        .output = {.type = OperandDataType::kFloat16,
                   .dimensions = {3, 6, 7, 5}},
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph when the output is as same as the input.
    auto context_properties = GetContextPropertiesForTesting();
    GraphInfoBuilder builder;
    uint64_t input_operand_id =
        builder.BuildInput("input", {2, 3}, OperandDataType::kFloat32);
    uint64_t indices_operand_id =
        builder.BuildInput("indices", {2}, OperandDataType::kUint32);
    builder.BuildGather(input_operand_id, indices_operand_id, input_operand_id,
                        /*axis*/ 0);
    EXPECT_FALSE(WebNNGraphBuilderImpl::IsValidForTesting(
        context_properties, builder.GetGraphInfo()));
  }
  {
    // Test the invalid graph when the output is as same as the indices.
    auto context_properties = GetContextPropertiesForTesting();
    GraphInfoBuilder builder;
    uint64_t input_operand_id =
        builder.BuildInput("input", {3}, OperandDataType::kUint32);
    uint64_t indices_operand_id =
        builder.BuildInput("indices", {3}, OperandDataType::kUint32);
    builder.BuildGather(input_operand_id, indices_operand_id,
                        indices_operand_id, /*axis*/ 0);
    EXPECT_FALSE(WebNNGraphBuilderImpl::IsValidForTesting(
        context_properties, builder.GetGraphInfo()));
  }
}

struct GatherElementsTester {
  OperandInfo input;
  GatherAttributes attributes;
  OperandInfo output;
  bool expected;

  void Test() {
    auto context_properties = GetContextPropertiesForTesting();

    // Build the graph with mojo type.
    GraphInfoBuilder builder;
    uint64_t input_operand_id =
        builder.BuildInput("input", input.dimensions, input.type);
    uint64_t indices_operand_id = builder.BuildInput(
        "indices", attributes.indices.dimensions, attributes.indices.type);
    uint64_t output_operand_id =
        builder.BuildOutput("output", output.dimensions, output.type);
    builder.BuildGatherElements(input_operand_id, indices_operand_id,
                                output_operand_id, attributes.axis);
    EXPECT_EQ(WebNNGraphBuilderImpl::IsValidForTesting(context_properties,
                                                       builder.GetGraphInfo()),
              expected);
  }
};

TEST_F(WebNNGraphImplTest, GatherElementsTest) {
  {
    // Test gatherElements with 4-D input and indices.
    GatherElementsTester{
        .input = {.type = OperandDataType::kFloat32,
                  .dimensions = {3, 4, 5, 6}},
        .attributes = {.indices = {.type = OperandDataType::kUint32,
                                   .dimensions = {3, 4, 2, 6}},
                       .axis = 2},
        .output = {.type = OperandDataType::kFloat32,
                   .dimensions = {3, 4, 2, 6}},
        .expected = true}
        .Test();
  }
  {
    // Test the invalid graph for the axis is greater than the rank of input.
    GatherElementsTester{
        .input = {.type = OperandDataType::kFloat32, .dimensions = {3, 4, 5}},
        .attributes = {.indices = {.type = OperandDataType::kUint32,
                                   .dimensions = {3, 4, 5}},
                       .axis = 3},
        .output = {.type = OperandDataType::kFloat32, .dimensions = {3, 4, 5}},
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph for indices has incorrect rank.
    GatherElementsTester{
        .input = {.type = OperandDataType::kFloat32, .dimensions = {3, 4, 5}},
        .attributes = {.indices = {.type = OperandDataType::kUint32,
                                   .dimensions = {3, 4}},
                       .axis = 2},
        .output = {.type = OperandDataType::kFloat32, .dimensions = {3, 4}},
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph for indices has incorrect shape.
    GatherElementsTester{
        .input = {.type = OperandDataType::kFloat32, .dimensions = {3, 4, 5}},
        .attributes = {.indices = {.type = OperandDataType::kUint32,
                                   .dimensions = {3, 3, 5}},
                       .axis = 2},
        .output = {.type = OperandDataType::kFloat32, .dimensions = {3, 3, 5}},
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph for indices data type is floating point.
    GatherElementsTester{
        .input = {.type = OperandDataType::kFloat32, .dimensions = {3, 4, 5}},
        .attributes = {.indices = {.type = OperandDataType::kFloat16,
                                   .dimensions = {3, 4, 5}},
                       .axis = 0},
        .output = {.type = OperandDataType::kFloat32, .dimensions = {3, 4, 5}},
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph for output shapes are not expected.
    GatherElementsTester{
        .input = {.type = OperandDataType::kFloat32, .dimensions = {3, 4, 5}},
        .attributes = {.indices = {.type = OperandDataType::kUint32,
                                   .dimensions = {3, 1, 5}},
                       .axis = 1},
        .output = {.type = OperandDataType::kFloat32, .dimensions = {3, 4, 5}},
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph for output types don't match.
    GatherElementsTester{
        .input = {.type = OperandDataType::kFloat32, .dimensions = {3, 4, 5}},
        .attributes = {.indices = {.type = OperandDataType::kUint32,
                                   .dimensions = {3, 1, 5}},
                       .axis = 1},
        .output = {.type = OperandDataType::kFloat16, .dimensions = {3, 1, 5}},
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph when the output is as same as the input.
    auto context_properties = GetContextPropertiesForTesting();
    GraphInfoBuilder builder;
    uint64_t input_operand_id =
        builder.BuildInput("input", {2, 3}, OperandDataType::kFloat32);
    uint64_t indices_operand_id =
        builder.BuildInput("indices", {2, 3}, OperandDataType::kUint32);
    builder.BuildGatherElements(input_operand_id, indices_operand_id,
                                input_operand_id,
                                /*axis=*/0);
    EXPECT_FALSE(WebNNGraphBuilderImpl::IsValidForTesting(
        context_properties, builder.GetGraphInfo()));
  }
  {
    // Test the invalid graph when the output is as same as the indices.
    auto context_properties = GetContextPropertiesForTesting();
    GraphInfoBuilder builder;
    uint64_t input_operand_id =
        builder.BuildInput("input", {3}, OperandDataType::kUint32);
    uint64_t indices_operand_id =
        builder.BuildInput("indices", {3}, OperandDataType::kUint32);
    builder.BuildGatherElements(input_operand_id, indices_operand_id,
                                indices_operand_id, /*axis=*/0);
    EXPECT_FALSE(WebNNGraphBuilderImpl::IsValidForTesting(
        context_properties, builder.GetGraphInfo()));
  }
}

struct GatherNDTester {
  OperandInfo input;
  OperandInfo indices;
  OperandInfo output;
  bool expected;

  void Test() {
    auto context_properties = GetContextPropertiesForTesting();

    // Build the graph with mojo type.
    GraphInfoBuilder builder;
    uint64_t input_operand_id =
        builder.BuildInput("input", input.dimensions, input.type);
    uint64_t indices_operand_id =
        builder.BuildInput("indices", indices.dimensions, indices.type);
    uint64_t output_operand_id =
        builder.BuildOutput("output", output.dimensions, output.type);
    builder.BuildGatherND(input_operand_id, indices_operand_id,
                          output_operand_id);
    EXPECT_EQ(WebNNGraphBuilderImpl::IsValidForTesting(context_properties,
                                                       builder.GetGraphInfo()),
              expected);
  }
};

TEST_F(WebNNGraphImplTest, GatherNDTest) {
  {
    // Test gatherND with 4-D input 3-D indices.
    GatherNDTester{
        .input = {.type = OperandDataType::kFloat32,
                  .dimensions = {3, 4, 5, 6}},
        .indices = {.type = OperandDataType::kUint32, .dimensions = {3, 7, 2}},
        .output = {.type = OperandDataType::kFloat32,
                   .dimensions = {3, 7, 5, 6}},
        .expected = true}
        .Test();
  }
  {
    // Test the invalid graph for the input is a scalar.
    GatherNDTester{
        .input = {.type = OperandDataType::kFloat32, .dimensions = {}},
        .indices = {.type = OperandDataType::kUint32, .dimensions = {1, 2}},
        .output = {.type = OperandDataType::kFloat32, .dimensions = {}},
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph for the indices is a scalar.
    GatherNDTester{
        .input = {.type = OperandDataType::kFloat32,
                  .dimensions = {1, 2, 3, 4, 5}},
        .indices = {.type = OperandDataType::kUint32, .dimensions = {}},
        .output = {.type = OperandDataType::kFloat32,
                   .dimensions = {1, 2, 3, 4, 5}},
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph for indices.shape[-1] is greater than the input
    // rank.
    GatherNDTester{
        .input = {.type = OperandDataType::kFloat32, .dimensions = {1, 2, 3}},
        .indices = {.type = OperandDataType::kUint32, .dimensions = {1, 4}},
        .output = {.type = OperandDataType::kFloat32, .dimensions = {1, 2, 3}},
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph for output shapes are not expected.
    GatherNDTester{
        .input = {.type = OperandDataType::kFloat32,
                  .dimensions = {1, 2, 3, 4}},
        .indices = {.type = OperandDataType::kUint32, .dimensions = {1, 1}},
        .output = {.type = OperandDataType::kFloat32,
                   .dimensions = {1, 1, 3, 4}},
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph for output types don't match.
    GatherNDTester{
        .input = {.type = OperandDataType::kFloat32,
                  .dimensions = {1, 2, 3, 4}},
        .indices = {.type = OperandDataType::kUint32, .dimensions = {1, 1}},
        .output = {.type = OperandDataType::kFloat16,
                   .dimensions = {1, 2, 3, 4}},
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph when the output is as same as the input.
    auto context_properties = GetContextPropertiesForTesting();
    GraphInfoBuilder builder;
    uint64_t input_operand_id =
        builder.BuildInput("input", {2, 3}, OperandDataType::kUint32);
    uint64_t indices_operand_id =
        builder.BuildInput("indices", {2, 1}, OperandDataType::kUint32);
    builder.BuildGatherND(input_operand_id, indices_operand_id,
                          input_operand_id);
    EXPECT_FALSE(WebNNGraphBuilderImpl::IsValidForTesting(
        context_properties, builder.GetGraphInfo()));
  }
  {
    // Test the invalid graph when the output is as same as the indices.
    auto context_properties = GetContextPropertiesForTesting();
    GraphInfoBuilder builder;
    uint64_t input_operand_id =
        builder.BuildInput("input", {2, 1}, OperandDataType::kUint32);
    uint64_t indices_operand_id =
        builder.BuildInput("indices", {2, 1}, OperandDataType::kUint32);
    builder.BuildGatherND(input_operand_id, indices_operand_id,
                          indices_operand_id);
    EXPECT_FALSE(WebNNGraphBuilderImpl::IsValidForTesting(
        context_properties, builder.GetGraphInfo()));
  }
}

struct GeluTester {
  OperandInfo input;
  OperandInfo output;
  bool expected;

  void Test() {
    auto context_properties = GetContextPropertiesForTesting();

    // Build the graph with mojo type.
    GraphInfoBuilder builder;
    uint64_t input_operand_id =
        builder.BuildInput("input", input.dimensions, input.type);
    uint64_t output_operand_id =
        builder.BuildOutput("output", output.dimensions, output.type);
    builder.BuildGelu(input_operand_id, output_operand_id);
    EXPECT_EQ(WebNNGraphBuilderImpl::IsValidForTesting(context_properties,
                                                       builder.GetGraphInfo()),
              expected);
  }
};

TEST_F(WebNNGraphImplTest, GeluTest) {
  {
    // Test gelu operator for 3-D tensor with float32 input.
    GeluTester{
        .input = {.type = OperandDataType::kFloat32, .dimensions = {2, 6, 4}},
        .output = {.type = OperandDataType::kFloat32, .dimensions = {2, 6, 4}},
        .expected = true}
        .Test();
  }
  {
    // Test the invalid graph when the input has data type int32.
    GeluTester{.input = {.type = OperandDataType::kInt32, .dimensions = {}},
               .output = {.type = OperandDataType::kInt32, .dimensions = {}},
               .expected = false}
        .Test();
  }
  {
    // Test the invalid graph for the output shapes are not expected.
    GeluTester{
        .input = {.type = OperandDataType::kFloat32, .dimensions = {4, 2}},
        .output = {.type = OperandDataType::kFloat32, .dimensions = {2}},
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph for output types don't match.
    GeluTester{.input = {.type = OperandDataType::kFloat32, .dimensions = {2}},
               .output = {.type = OperandDataType::kInt32, .dimensions = {2}},
               .expected = false}
        .Test();
  }
  {
    // Test the invalid graph when the input has the same id as the output.
    auto context_properties = GetContextPropertiesForTesting();
    GraphInfoBuilder builder;
    uint64_t input_operand_id =
        builder.BuildInput("input", {1}, OperandDataType::kFloat16);
    builder.BuildGelu(input_operand_id, input_operand_id);
    EXPECT_FALSE(WebNNGraphBuilderImpl::IsValidForTesting(
        context_properties, builder.GetGraphInfo()));
  }
}

struct GemmTester {
  OperandInfo a;
  OperandInfo b;
  std::optional<OperandInfo> c;
  struct GemmAttributes {
    std::optional<uint64_t> c_operand_id;
    float alpha = 1.0;
    float beta = 1.0;
    bool a_transpose = false;
    bool b_transpose = false;
  };
  GemmAttributes attributes;
  OperandInfo output;
  bool expected;

  void Test() {
    auto context_properties = GetContextPropertiesForTesting();

    // Build the graph with mojo type.
    GraphInfoBuilder builder;
    uint64_t a_operand_id = builder.BuildInput("a", a.dimensions, a.type);
    uint64_t b_operand_id = builder.BuildInput("b", b.dimensions, b.type);
    uint64_t output_operand_id =
        builder.BuildOutput("output", output.dimensions, output.type);

    if (c) {
      attributes.c_operand_id = builder.BuildInput("c", c->dimensions, c->type);
    }
    builder.BuildGemm(a_operand_id, b_operand_id, output_operand_id,
                      std::move(attributes));
    EXPECT_EQ(WebNNGraphBuilderImpl::IsValidForTesting(context_properties,
                                                       builder.GetGraphInfo()),
              expected);
  }
};

TEST_F(WebNNGraphImplTest, GemmTest) {
  {
    // Test building gemm with default option.
    GemmTester{
        .a = {.type = OperandDataType::kFloat32, .dimensions = {2, 3}},
        .b = {.type = OperandDataType::kFloat32, .dimensions = {3, 4}},
        .output = {.type = OperandDataType::kFloat32, .dimensions = {2, 4}},
        .expected = true}
        .Test();
  }
  {
    // Test building gemm with aTranspose = true.
    // Transposed a_dimensions would be {3, 2} and it's compatible with
    // b_dimensions {2, 4}.
    GemmTester{
        .a = {.type = OperandDataType::kFloat32, .dimensions = {2, 3}},
        .b = {.type = OperandDataType::kFloat32, .dimensions = {2, 4}},
        .attributes = {.a_transpose = true},
        .output = {.type = OperandDataType::kFloat32, .dimensions = {3, 4}},
        .expected = true}
        .Test();
  }
  {
    // Test building gemm with bTranspose = true.
    // Transposed b_dimensions would be {3, 4} and it's compatible with
    // a_dimensions {2, 3}.
    GemmTester{
        .a = {.type = OperandDataType::kFloat32, .dimensions = {2, 3}},
        .b = {.type = OperandDataType::kFloat32, .dimensions = {4, 3}},
        .attributes = {.b_transpose = true},
        .output = {.type = OperandDataType::kFloat32, .dimensions = {2, 4}},
        .expected = true}
        .Test();
  }
  {
    // Test building gemm with setting optional input C.
    // The output dimensions of a * b would be {2, 4} and c_dimensions {4}
    // is able to broadcast to {2, 4}.
    GemmTester{
        .a = {.type = OperandDataType::kFloat32, .dimensions = {2, 3}},
        .b = {.type = OperandDataType::kFloat32, .dimensions = {3, 4}},
        .c = OperandInfo{.type = OperandDataType::kFloat32, .dimensions = {4}},
        .output = {.type = OperandDataType::kFloat32, .dimensions = {2, 4}},
        .expected = true}
        .Test();
  }
  {
    // Test building gemm with two matrices - {2, 3} and {2, 4} that can't
    // be multiplied together due to incompatible dimensions.
    GemmTester{
        .a = {.type = OperandDataType::kFloat32, .dimensions = {2, 3}},
        .b = {.type = OperandDataType::kFloat32, .dimensions = {2, 4}},
        .output = {.type = OperandDataType::kFloat32, .dimensions = {3, 4}},
        .expected = false}
        .Test();
  }
  {
    // Test building gemm with aTranspose = true, bTranspose = true.
    // The output dimensions of a * b would be {2, 4} and c_dimension {2, 3}
    // is incompatible with {2, 4}.
    GemmTester{
        .a = {.type = OperandDataType::kFloat32, .dimensions = {2, 3}},
        .b = {.type = OperandDataType::kFloat32, .dimensions = {3, 4}},
        .c = OperandInfo{.type = OperandDataType::kFloat32,
                         .dimensions = {2, 3}},
        .output = {.type = OperandDataType::kFloat32, .dimensions = {2, 4}},
        .expected = false}
        .Test();
  }
  {
    // Test building gemm with aTranspose = true, bTranspose = true.
    // Set optional input C with type = int32 and it mismatches with input
    // type float32.
    GemmTester{
        .a = {.type = OperandDataType::kFloat32, .dimensions = {3, 2}},
        .b = {.type = OperandDataType::kFloat32, .dimensions = {4, 3}},
        .c = OperandInfo{.type = OperandDataType::kInt32, .dimensions = {2, 4}},
        .attributes = {.a_transpose = true, .b_transpose = true},
        .output = {.type = OperandDataType::kFloat32, .dimensions = {2, 4}},
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph if the input is not floating point.
    GemmTester{
        .a = {.type = OperandDataType::kInt32, .dimensions = {2, 3}},
        .b = {.type = OperandDataType::kInt32, .dimensions = {3, 4}},
        .output = {.type = OperandDataType::kInt32, .dimensions = {2, 4}},
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph for the output shapes are not expected.
    GemmTester{
        .a = {.type = OperandDataType::kFloat32, .dimensions = {2, 3}},
        .b = {.type = OperandDataType::kInt32, .dimensions = {3, 4}},
        .output = {.type = OperandDataType::kFloat32, .dimensions = {3, 4}},
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph for output types don't match.
    GemmTester{
        .a = {.type = OperandDataType::kFloat32, .dimensions = {2, 3}},
        .b = {.type = OperandDataType::kInt32, .dimensions = {3, 4}},
        .output = {.type = OperandDataType::kFloat32, .dimensions = {2, 4}},
        .expected = false}
        .Test();
  }
}

struct GruTester {
  struct GruAttributes {
    std::optional<uint64_t> bias_operand_id;
    std::optional<uint64_t> recurrent_bias_operand_id;
    std::optional<uint64_t> initial_hidden_state_operand_id;
    bool reset_after = true;
    bool return_sequence = false;
    mojom::RecurrentNetworkDirection direction =
        mojom::RecurrentNetworkDirection::kForward;
    mojom::GruWeightLayout layout = mojom::GruWeightLayout::kZrn;
    std::vector<mojom::RecurrentNetworkActivation> activations = {
        mojom::RecurrentNetworkActivation::kSigmoid,
        mojom::RecurrentNetworkActivation::kTanh};
  };

  OperandInfo input;
  OperandInfo weight;
  OperandInfo recurrent_weight;
  uint32_t steps;
  uint32_t hidden_size;
  std::optional<OperandInfo> bias;
  std::optional<OperandInfo> recurrent_bias;
  std::optional<OperandInfo> initial_hidden_state;
  GruAttributes attributes;
  std::vector<OperandInfo> outputs;
  bool expected;

  void Test() {
    auto context_properties = GetContextPropertiesForTesting();

    // Build the graph with mojo type.
    GraphInfoBuilder builder;
    uint64_t input_operand_id =
        builder.BuildInput("input", input.dimensions, input.type);
    uint64_t weight_operand_id =
        builder.BuildInput("weight", weight.dimensions, weight.type);
    uint64_t recurrent_weight_operand_id = builder.BuildInput(
        "recurrentWeight", recurrent_weight.dimensions, recurrent_weight.type);

    std::vector<uint64_t> output_operand_ids;
    output_operand_ids.reserve(outputs.size());
    for (size_t i = 0; i < outputs.size(); ++i) {
      output_operand_ids.push_back(
          builder.BuildOutput(base::StringPrintf("output%zu", i),
                              outputs[i].dimensions, outputs[i].type));
    }

    if (bias.has_value()) {
      attributes.bias_operand_id =
          builder.BuildInput("bias", bias->dimensions, bias->type);
    }
    if (recurrent_bias.has_value()) {
      attributes.recurrent_bias_operand_id = builder.BuildInput(
          "recurrentBias", recurrent_bias->dimensions, recurrent_bias->type);
    }
    if (initial_hidden_state.has_value()) {
      attributes.initial_hidden_state_operand_id = builder.BuildInput(
          "initialHiddenState", initial_hidden_state->dimensions,
          initial_hidden_state->type);
    }

    builder.BuildGru(input_operand_id, weight_operand_id,
                     recurrent_weight_operand_id, std::move(output_operand_ids),
                     steps, hidden_size, std::move(attributes));
    EXPECT_EQ(WebNNGraphBuilderImpl::IsValidForTesting(context_properties,
                                                       builder.GetGraphInfo()),
              expected);
  }
};

TEST_F(WebNNGraphImplTest, GruTest) {
  {
    // Test the gru operator.
    uint32_t steps = 2;
    uint32_t batch_size = 1;
    uint32_t input_size = 3;
    uint32_t hidden_size = 4;
    uint32_t num_directions = 2;
    GruTester{
        .input = {.type = OperandDataType::kFloat32,
                  .dimensions = {steps, batch_size, input_size}},
        .weight = {.type = OperandDataType::kFloat32,
                   .dimensions = {num_directions, 3 * hidden_size, input_size}},
        .recurrent_weight = {.type = OperandDataType::kFloat32,
                             .dimensions = {num_directions, 3 * hidden_size,
                                            hidden_size}},
        .steps = steps,
        .hidden_size = hidden_size,
        .bias = OperandInfo{.type = OperandDataType::kFloat32,
                            .dimensions = {num_directions, 3 * hidden_size}},
        .recurrent_bias =
            OperandInfo{.type = OperandDataType::kFloat32,
                        .dimensions = {num_directions, 3 * hidden_size}},
        .initial_hidden_state =
            OperandInfo{
                .type = OperandDataType::kFloat32,
                .dimensions = {num_directions, batch_size, hidden_size}},
        .attributes = {.reset_after = true,
                       .return_sequence = true,
                       .direction = mojom::RecurrentNetworkDirection::kBoth},
        .outputs = {{.type = OperandDataType::kFloat32,
                     .dimensions = {num_directions, batch_size, hidden_size}},
                    {.type = OperandDataType::kFloat32,
                     .dimensions = {steps, num_directions, batch_size,
                                    hidden_size}}},
        .expected = true}
        .Test();
  }
  {
    // Test the invalid graph when the shape of weight is incorrect.
    uint32_t steps = 2;
    uint32_t batch_size = 1;
    uint32_t input_size = 3;
    uint32_t hidden_size = 4;
    uint32_t num_directions = 1;
    GruTester{
        .input = {.type = OperandDataType::kFloat32,
                  .dimensions = {steps, batch_size, input_size}},
        .weight = {.type = OperandDataType::kFloat32,
                   .dimensions = {num_directions, 4 * hidden_size, input_size}},
        .recurrent_weight = {.type = OperandDataType::kFloat32,
                             .dimensions = {num_directions, 3 * hidden_size,
                                            hidden_size}},
        .steps = steps,
        .hidden_size = hidden_size,
        .outputs = {{.type = OperandDataType::kFloat32,
                     .dimensions = {num_directions, batch_size, hidden_size}}},
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph when the the number of activation is not 2.
    uint32_t steps = 2;
    uint32_t batch_size = 1;
    uint32_t input_size = 3;
    uint32_t hidden_size = 4;
    uint32_t num_directions = 1;
    GruTester{
        .input = {.type = OperandDataType::kFloat32,
                  .dimensions = {steps, batch_size, input_size}},
        .weight = {.type = OperandDataType::kFloat32,
                   .dimensions = {num_directions, 3 * hidden_size, input_size}},
        .recurrent_weight = {.type = OperandDataType::kFloat32,
                             .dimensions = {num_directions, 3 * hidden_size,
                                            hidden_size}},
        .steps = steps,
        .hidden_size = hidden_size,
        .attributes =
            {.direction = mojom::RecurrentNetworkDirection::kBackward,
             .activations = {mojom::RecurrentNetworkActivation::kSigmoid,
                             mojom::RecurrentNetworkActivation::kTanh,
                             mojom::RecurrentNetworkActivation::kTanh}},
        .outputs = {{.type = OperandDataType::kFloat32,
                     .dimensions = {num_directions, batch_size, hidden_size}}},
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph when the output shape is incorrect.
    uint32_t steps = 2;
    uint32_t batch_size = 1;
    uint32_t input_size = 3;
    uint32_t hidden_size = 4;
    uint32_t num_directions = 1;
    GruTester{
        .input = {.type = OperandDataType::kFloat32,
                  .dimensions = {steps, batch_size, input_size}},
        .weight = {.type = OperandDataType::kFloat32,
                   .dimensions = {num_directions, 3 * hidden_size, input_size}},
        .recurrent_weight = {.type = OperandDataType::kFloat32,
                             .dimensions = {num_directions, 3 * hidden_size,
                                            hidden_size}},
        .steps = steps,
        .hidden_size = hidden_size,
        .outputs = {{.type = OperandDataType::kFloat32,
                     .dimensions = {num_directions, batch_size,
                                    3 * hidden_size}}},
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph when the output number is incorrect.
    uint32_t steps = 2;
    uint32_t batch_size = 1;
    uint32_t input_size = 3;
    uint32_t hidden_size = 4;
    uint32_t num_directions = 1;
    GruTester{
        .input = {.type = OperandDataType::kFloat32,
                  .dimensions = {steps, batch_size, input_size}},
        .weight = {.type = OperandDataType::kFloat32,
                   .dimensions = {num_directions, 3 * hidden_size, input_size}},
        .recurrent_weight = {.type = OperandDataType::kFloat32,
                             .dimensions = {num_directions, 3 * hidden_size,
                                            hidden_size}},
        .steps = steps,
        .hidden_size = hidden_size,
        .outputs = {{.type = OperandDataType::kFloat32,
                     .dimensions = {num_directions, batch_size, hidden_size}},
                    {.type = OperandDataType::kFloat32,
                     .dimensions = {steps, num_directions, batch_size,
                                    hidden_size}}},
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph when the initial hidden state has the same id as
    // one of the outputs.
    uint32_t steps = 2;
    uint32_t batch_size = 1;
    uint32_t input_size = 3;
    uint32_t hidden_size = 4;
    uint32_t num_directions = 1;

    auto context_properties = GetContextPropertiesForTesting();
    GraphInfoBuilder builder;
    uint64_t input_operand_id = builder.BuildInput(
        "input", {steps, batch_size, input_size}, OperandDataType::kFloat32);
    uint64_t weight_operand_id = builder.BuildInput(
        "weight", {num_directions, 3 * hidden_size, input_size},
        OperandDataType::kFloat32);
    uint64_t recurrent_weight_operand_id = builder.BuildInput(
        "recurrentWeight", {num_directions, 3 * hidden_size, hidden_size},
        OperandDataType::kFloat32);

    uint64_t initial_hidden_state_operand_id = builder.BuildInput(
        "initialHiddenState", {num_directions, batch_size, hidden_size},
        OperandDataType::kFloat32);

    builder.BuildGru(
        input_operand_id, weight_operand_id, recurrent_weight_operand_id,
        {initial_hidden_state_operand_id}, steps, hidden_size,
        GruTester::GruAttributes{.initial_hidden_state_operand_id =
                                     initial_hidden_state_operand_id});
    EXPECT_FALSE(WebNNGraphBuilderImpl::IsValidForTesting(
        context_properties, builder.GetGraphInfo()));
  }
}

struct GruCellTester {
  struct GruCellAttributes {
    std::optional<uint64_t> bias_operand_id;
    std::optional<uint64_t> recurrent_bias_operand_id;
    bool reset_after = true;
    mojom::GruWeightLayout layout = mojom::GruWeightLayout::kZrn;
    std::vector<mojom::RecurrentNetworkActivation> activations = {
        mojom::RecurrentNetworkActivation::kSigmoid,
        mojom::RecurrentNetworkActivation::kTanh};
  };

  OperandInfo input;
  OperandInfo weight;
  OperandInfo recurrent_weight;
  OperandInfo hidden_state;
  uint32_t hidden_size;
  std::optional<OperandInfo> bias;
  std::optional<OperandInfo> recurrent_bias;
  GruCellAttributes attributes;
  OperandInfo output;
  bool expected;

  void Test() {
    auto context_properties = GetContextPropertiesForTesting();

    // Build the graph with mojo type.
    GraphInfoBuilder builder;
    uint64_t input_operand_id =
        builder.BuildInput("input", input.dimensions, input.type);
    uint64_t weight_operand_id =
        builder.BuildInput("weight", weight.dimensions, weight.type);
    uint64_t recurrent_weight_operand_id = builder.BuildInput(
        "recurrentWeight", recurrent_weight.dimensions, recurrent_weight.type);
    uint64_t hidden_state_operand_id = builder.BuildInput(
        "hiddenState", hidden_state.dimensions, hidden_state.type);

    if (bias.has_value()) {
      attributes.bias_operand_id =
          builder.BuildInput("bias", bias->dimensions, bias->type);
    }
    if (recurrent_bias.has_value()) {
      attributes.recurrent_bias_operand_id = builder.BuildInput(
          "recurrentBias", recurrent_bias->dimensions, recurrent_bias->type);
    }

    uint64_t output_operand_id =
        builder.BuildOutput("output", output.dimensions, output.type);

    builder.BuildGruCell(input_operand_id, weight_operand_id,
                         recurrent_weight_operand_id, hidden_state_operand_id,
                         output_operand_id, hidden_size, std::move(attributes));
    EXPECT_EQ(WebNNGraphBuilderImpl::IsValidForTesting(context_properties,
                                                       builder.GetGraphInfo()),
              expected);
  }
};

TEST_F(WebNNGraphImplTest, GruCellTest) {
  uint32_t batch_size = 2;
  uint32_t input_size = 4;
  uint32_t hidden_size = 6;

  OperandInfo valid_input = {.type = OperandDataType::kFloat32,
                             .dimensions = {batch_size, input_size}};
  OperandInfo valid_weight = {.type = OperandDataType::kFloat32,
                              .dimensions = {3 * hidden_size, input_size}};
  OperandInfo valid_recurrent_weight = {
      .type = OperandDataType::kFloat32,
      .dimensions = {3 * hidden_size, hidden_size}};
  OperandInfo valid_hidden_state = {.type = OperandDataType::kFloat32,
                                    .dimensions = {batch_size, hidden_size}};
  OperandInfo valid_bias = {.type = OperandDataType::kFloat32,
                            .dimensions = {3 * hidden_size}};
  OperandInfo valid_recurrent_bias = {.type = OperandDataType::kFloat32,
                                      .dimensions = {3 * hidden_size}};
  OperandInfo valid_output = {.type = OperandDataType::kFloat32,
                              .dimensions = {batch_size, hidden_size}};

  {
    // Test the valid gruCell operator.
    GruCellTester{.input = valid_input,
                  .weight = valid_weight,
                  .recurrent_weight = valid_recurrent_weight,
                  .hidden_state = valid_hidden_state,
                  .hidden_size = hidden_size,
                  .bias = valid_bias,
                  .recurrent_bias = valid_recurrent_bias,
                  .attributes = {.reset_after = true},
                  .output = valid_output,
                  .expected = true}
        .Test();
  }
  {
    // Test the invalid graph when the data type of the input is incorrect.
    GruCellTester{.input = {.type = OperandDataType::kInt8,
                            .dimensions = {batch_size, input_size}},
                  .weight = valid_weight,
                  .recurrent_weight = valid_recurrent_weight,
                  .hidden_state = valid_hidden_state,
                  .hidden_size = hidden_size,
                  .bias = valid_bias,
                  .recurrent_bias = valid_recurrent_bias,
                  .attributes = {.reset_after = true},
                  .output = valid_output,
                  .expected = false}
        .Test();
  }
  {
    // Test the invalid graph when the shape of the input is incorrect.
    GruCellTester{.input = {.type = OperandDataType::kFloat32,
                            .dimensions = {1, input_size}},
                  .weight = valid_weight,
                  .recurrent_weight = valid_recurrent_weight,
                  .hidden_state = valid_hidden_state,
                  .hidden_size = hidden_size,
                  .bias = valid_bias,
                  .recurrent_bias = valid_recurrent_bias,
                  .attributes = {.reset_after = true},
                  .output = valid_output,
                  .expected = false}
        .Test();
  }
  {
    // Test the invalid graph when the rank of the input is incorrect.
    GruCellTester{.input = {.type = OperandDataType::kFloat32,
                            .dimensions = {input_size}},
                  .weight = valid_weight,
                  .recurrent_weight = valid_recurrent_weight,
                  .hidden_state = valid_hidden_state,
                  .hidden_size = hidden_size,
                  .bias = valid_bias,
                  .recurrent_bias = valid_recurrent_bias,
                  .attributes = {.reset_after = true},
                  .output = valid_output,
                  .expected = false}
        .Test();
  }
  {
    // Test the invalid graph when the data type of the weight is incorrect.
    GruCellTester{.input = valid_input,
                  .weight = {.type = OperandDataType::kInt8,
                             .dimensions = {3 * hidden_size, input_size}},
                  .recurrent_weight = valid_recurrent_weight,
                  .hidden_state = valid_hidden_state,
                  .hidden_size = hidden_size,
                  .bias = valid_bias,
                  .recurrent_bias = valid_recurrent_bias,
                  .attributes = {.reset_after = true},
                  .output = valid_output,
                  .expected = false}
        .Test();
  }
  {
    // Test the invalid graph when the shape of the weight is incorrect.
    GruCellTester{.input = valid_input,
                  .weight = {.type = OperandDataType::kFloat32,
                             .dimensions = {4 * hidden_size, input_size}},
                  .recurrent_weight = valid_recurrent_weight,
                  .hidden_state = valid_hidden_state,
                  .hidden_size = hidden_size,
                  .bias = valid_bias,
                  .recurrent_bias = valid_recurrent_bias,
                  .attributes = {.reset_after = true},
                  .output = valid_output,
                  .expected = false}
        .Test();
  }
  {
    // Test the invalid graph when the rank of the weight is incorrect.
    GruCellTester{.input = valid_input,
                  .weight = {.type = OperandDataType::kFloat32,
                             .dimensions = {3 * hidden_size}},
                  .recurrent_weight = valid_recurrent_weight,
                  .hidden_state = valid_hidden_state,
                  .hidden_size = hidden_size,
                  .bias = valid_bias,
                  .recurrent_bias = valid_recurrent_bias,
                  .attributes = {.reset_after = true},
                  .output = valid_output,
                  .expected = false}
        .Test();
  }
  {
    // Test the invalid graph when the data type of the recurrent weight is
    // incorrect.
    GruCellTester{
        .input = valid_input,
        .weight = valid_weight,
        .recurrent_weight = {.type = OperandDataType::kInt8,
                             .dimensions = {3 * hidden_size, hidden_size}},
        .hidden_state = valid_hidden_state,
        .hidden_size = hidden_size,
        .bias = valid_bias,
        .recurrent_bias = valid_recurrent_bias,
        .attributes = {.reset_after = true},
        .output = valid_output,
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph when the shape of the recurrent weight is
    // incorrect.
    GruCellTester{
        .input = valid_input,
        .weight = valid_weight,
        .recurrent_weight = {.type = OperandDataType::kFloat32,
                             .dimensions = {3 * hidden_size, input_size}},
        .hidden_state = valid_hidden_state,
        .hidden_size = hidden_size,
        .bias = valid_bias,
        .recurrent_bias = valid_recurrent_bias,
        .attributes = {.reset_after = true},
        .output = valid_output,
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph when the rank of the recurrent weight is
    // incorrect.
    GruCellTester{.input = valid_input,
                  .weight = valid_weight,
                  .recurrent_weight = {.type = OperandDataType::kFloat32,
                                       .dimensions = {3 * hidden_size}},
                  .hidden_state = valid_hidden_state,
                  .hidden_size = hidden_size,
                  .bias = valid_bias,
                  .recurrent_bias = valid_recurrent_bias,
                  .attributes = {.reset_after = true},
                  .output = valid_output,
                  .expected = false}
        .Test();
  }
  {
    // Test the invalid graph when the hidden_size is incorrect.
    GruCellTester{.input = valid_input,
                  .weight = valid_weight,
                  .recurrent_weight = valid_recurrent_weight,
                  .hidden_state = valid_hidden_state,
                  .hidden_size = 1000,
                  .bias = valid_bias,
                  .recurrent_bias = valid_recurrent_bias,
                  .attributes = {.reset_after = true},
                  .output = valid_output,
                  .expected = false}
        .Test();
  }
  {
    // Test the invalid graph when the data type of the bias is incorrect.
    GruCellTester{.input = valid_input,
                  .weight = valid_weight,
                  .recurrent_weight = valid_recurrent_weight,
                  .hidden_state = valid_hidden_state,
                  .hidden_size = hidden_size,
                  .bias = OperandInfo{.type = OperandDataType::kUint8,
                                      .dimensions = {3 * hidden_size}},
                  .recurrent_bias = valid_recurrent_bias,
                  .attributes = {.reset_after = true},
                  .output = valid_output,
                  .expected = false}
        .Test();
  }
  {
    // Test the invalid graph when the shape of the bias is incorrect.
    GruCellTester{.input = valid_input,
                  .weight = valid_weight,
                  .recurrent_weight = valid_recurrent_weight,
                  .hidden_state = valid_hidden_state,
                  .hidden_size = hidden_size,
                  .bias = OperandInfo{.type = OperandDataType::kFloat32,
                                      .dimensions = {4 * hidden_size}},
                  .recurrent_bias = valid_recurrent_bias,
                  .attributes = {.reset_after = true},
                  .output = valid_output,
                  .expected = false}
        .Test();
  }
  {
    // Test the invalid graph when the rank of the bias is incorrect.
    GruCellTester{
        .input = valid_input,
        .weight = valid_weight,
        .recurrent_weight = valid_recurrent_weight,
        .hidden_state = valid_hidden_state,
        .hidden_size = hidden_size,
        .bias = OperandInfo{.type = OperandDataType::kFloat32,
                            .dimensions = {3 * hidden_size, hidden_size}},
        .recurrent_bias = valid_recurrent_bias,
        .attributes = {.reset_after = true},
        .output = valid_output,
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph when the data type of the recurrent bias is
    // incorrect.
    GruCellTester{
        .input = valid_input,
        .weight = valid_weight,
        .recurrent_weight = valid_recurrent_weight,
        .hidden_state = valid_hidden_state,
        .hidden_size = hidden_size,
        .bias = valid_bias,
        .recurrent_bias = OperandInfo{.type = OperandDataType::kUint8,
                                      .dimensions = {3 * hidden_size}},
        .attributes = {.reset_after = true},
        .output = valid_output,
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph when the shape of the recurrent bias is incorrect.
    GruCellTester{
        .input = valid_input,
        .weight = valid_weight,
        .recurrent_weight = valid_recurrent_weight,
        .hidden_state = valid_hidden_state,
        .hidden_size = hidden_size,
        .bias = valid_bias,
        .recurrent_bias = OperandInfo{.type = OperandDataType::kFloat32,
                                      .dimensions = {4 * hidden_size}},
        .attributes = {.reset_after = true},
        .output = valid_output,
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph when the rank of the recurrent bias is incorrect.
    GruCellTester{.input = valid_input,
                  .weight = valid_weight,
                  .recurrent_weight = valid_recurrent_weight,
                  .hidden_state = valid_hidden_state,
                  .hidden_size = hidden_size,
                  .bias = valid_bias,
                  .recurrent_bias =
                      OperandInfo{.type = OperandDataType::kFloat32,
                                  .dimensions = {3 * hidden_size, hidden_size}},
                  .attributes = {.reset_after = true},
                  .output = valid_output,
                  .expected = false}
        .Test();
  }
  {
    // Test the invalid graph when the the number of activation is not 2.
    GruCellTester{
        .input = valid_input,
        .weight = valid_weight,
        .recurrent_weight = valid_recurrent_weight,
        .hidden_state = valid_hidden_state,
        .hidden_size = hidden_size,
        .attributes =
            {.activations = {mojom::RecurrentNetworkActivation::kSigmoid,
                             mojom::RecurrentNetworkActivation::kTanh,
                             mojom::RecurrentNetworkActivation::kTanh}},
        .output = valid_output,
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph when the output data type is incorrect.
    GruCellTester{.input = valid_input,
                  .weight = valid_weight,
                  .recurrent_weight = valid_recurrent_weight,
                  .hidden_state = valid_hidden_state,
                  .hidden_size = hidden_size,
                  .bias = valid_bias,
                  .recurrent_bias = valid_recurrent_bias,
                  .attributes = {.reset_after = true},
                  .output = {.type = OperandDataType::kInt32,
                             .dimensions = {batch_size, hidden_size}},
                  .expected = false}
        .Test();
  }
  {
    // Test the invalid graph when the output shape is incorrect.
    GruCellTester{.input = valid_input,
                  .weight = valid_weight,
                  .recurrent_weight = valid_recurrent_weight,
                  .hidden_state = valid_hidden_state,
                  .hidden_size = hidden_size,
                  .bias = valid_bias,
                  .recurrent_bias = valid_recurrent_bias,
                  .attributes = {.reset_after = true},
                  .output = {.type = OperandDataType::kFloat32,
                             .dimensions = {batch_size, 3 * hidden_size}},
                  .expected = false}
        .Test();
  }
  {
    // Test the invalid graph when the output rank is incorrect.
    GruCellTester{.input = valid_input,
                  .weight = valid_weight,
                  .recurrent_weight = valid_recurrent_weight,
                  .hidden_state = valid_hidden_state,
                  .hidden_size = hidden_size,
                  .bias = valid_bias,
                  .recurrent_bias = valid_recurrent_bias,
                  .attributes = {.reset_after = true},
                  .output = {.type = OperandDataType::kFloat32,
                             .dimensions = {hidden_size}},
                  .expected = false}
        .Test();
  }
  {
    // Test the invalid graph when the hidden state has the same id as the
    // output.
    auto context_properties = GetContextPropertiesForTesting();
    GraphInfoBuilder builder;
    uint64_t input_operand_id = builder.BuildInput(
        "input", {batch_size, input_size}, OperandDataType::kFloat32);
    uint64_t weight_operand_id = builder.BuildInput(
        "weight", {3 * hidden_size, input_size}, OperandDataType::kFloat32);
    uint64_t recurrent_weight_operand_id =
        builder.BuildInput("recurrentWeight", {3 * hidden_size, hidden_size},
                           OperandDataType::kFloat32);

    uint64_t hidden_state_operand_id = builder.BuildInput(
        "hiddenState", {batch_size, hidden_size}, OperandDataType::kFloat32);

    builder.BuildGruCell(input_operand_id, weight_operand_id,
                         recurrent_weight_operand_id, hidden_state_operand_id,
                         hidden_state_operand_id, hidden_size,
                         GruCellTester::GruCellAttributes{.reset_after = true});
    EXPECT_FALSE(WebNNGraphBuilderImpl::IsValidForTesting(
        context_properties, builder.GetGraphInfo()));
  }
}

struct InstanceNormalizationTester {
  OperandInfo input;
  std::optional<OperandInfo> scale;
  std::optional<OperandInfo> bias;
  struct InstanceNormalizationAttributes {
    std::optional<uint64_t> scale_operand_id;
    std::optional<uint64_t> bias_operand_id;
    mojom::InputOperandLayout layout =
        mojom::InputOperandLayout::kChannelsFirst;
    float epsilon = 1e-5;
  };
  InstanceNormalizationAttributes attributes;
  OperandInfo output;
  bool expected;

  void Test() {
    auto context_properties = GetContextPropertiesForTesting();

    // Build the graph with mojo type.
    GraphInfoBuilder builder;
    uint64_t input_operand_id =
        builder.BuildInput("input", input.dimensions, input.type);
    uint64_t output_operand_id =
        builder.BuildOutput("output", output.dimensions, output.type);

    if (scale) {
      attributes.scale_operand_id =
          builder.BuildInput("scale", scale->dimensions, scale->type);
    }
    if (bias) {
      attributes.bias_operand_id =
          builder.BuildInput("bias", bias->dimensions, bias->type);
    }
    builder.BuildInstanceNormalization(input_operand_id, output_operand_id,
                                       std::move(attributes));
    EXPECT_EQ(WebNNGraphBuilderImpl::IsValidForTesting(context_properties,
                                                       builder.GetGraphInfo()),
              expected);
  }
};

TEST_F(WebNNGraphImplTest, InstanceNormalizationTest) {
  {
    // Test building instanceNormalization with default option.
    InstanceNormalizationTester{.input = {.type = OperandDataType::kFloat32,
                                          .dimensions = {1, 2, 3, 3}},
                                .output = {.type = OperandDataType::kFloat32,
                                           .dimensions = {1, 2, 3, 3}},
                                .expected = true}
        .Test();
  }
  {
    // Test building instanceNormalization with layout = kChannelsLast.
    InstanceNormalizationTester{
        .input = {.type = OperandDataType::kFloat32,
                  .dimensions = {1, 2, 3, 3}},
        .scale =
            OperandInfo{.type = OperandDataType::kFloat32, .dimensions = {3}},
        .bias =
            OperandInfo{.type = OperandDataType::kFloat32, .dimensions = {3}},
        .attributes = {.layout = mojom::InputOperandLayout::kChannelsLast},
        .output = {.type = OperandDataType::kFloat32,
                   .dimensions = {1, 2, 3, 3}},
        .expected = true}
        .Test();
  }
  {
    // Test building instanceNormalization with default layout = kChannelsFirst.
    InstanceNormalizationTester{
        .input = {.type = OperandDataType::kFloat32,
                  .dimensions = {1, 2, 3, 3}},
        .scale =
            OperandInfo{.type = OperandDataType::kFloat32, .dimensions = {2}},
        .bias =
            OperandInfo{.type = OperandDataType::kFloat32, .dimensions = {2}},
        .output = {.type = OperandDataType::kFloat32,
                   .dimensions = {1, 2, 3, 3}},
        .expected = true}
        .Test();
  }
  {
    // Test instanceNormalization when input data type and scale data type
    // mismatched.
    InstanceNormalizationTester{
        .input = {.type = OperandDataType::kFloat32,
                  .dimensions = {1, 2, 3, 3}},
        .scale =
            OperandInfo{.type = OperandDataType::kInt32, .dimensions = {2}},
        .output = {.type = OperandDataType::kFloat32,
                   .dimensions = {1, 2, 3, 3}},
        .expected = false}
        .Test();
  }
  {
    // Test building instanceNormalization when the size of scale is not equal
    // to the size of the feature dimension of the input.
    InstanceNormalizationTester{
        .input = {.type = OperandDataType::kFloat32,
                  .dimensions = {1, 2, 3, 3}},
        .scale =
            OperandInfo{.type = OperandDataType::kFloat32, .dimensions = {3}},
        .output = {.type = OperandDataType::kFloat32,
                   .dimensions = {1, 2, 3, 3}},
        .expected = false}
        .Test();
  }
  {
    // Test instanceNormalization when input data type and bias data type
    // mismatched.
    InstanceNormalizationTester{
        .input = {.type = OperandDataType::kFloat32,
                  .dimensions = {1, 2, 3, 3}},
        .bias = OperandInfo{.type = OperandDataType::kInt32, .dimensions = {2}},
        .output = {.type = OperandDataType::kFloat32,
                   .dimensions = {1, 2, 3, 3}},
        .expected = false}
        .Test();
  }
  {
    // Test building instanceNormalization when the size of bias is not equal
    // to the size of the feature dimension of the input.
    InstanceNormalizationTester{
        .input = {.type = OperandDataType::kFloat32,
                  .dimensions = {1, 2, 3, 3}},
        .bias =
            OperandInfo{.type = OperandDataType::kFloat32, .dimensions = {2}},
        .attributes = {.layout = mojom::InputOperandLayout::kChannelsLast},
        .output = {.type = OperandDataType::kFloat32,
                   .dimensions = {1, 2, 3, 3}},
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph for output type is not the same as input type.
    InstanceNormalizationTester{
        .input = {.type = OperandDataType::kFloat32,
                  .dimensions = {1, 2, 3, 3}},
        .output = {.type = OperandDataType::kInt32, .dimensions = {1, 2, 3, 3}},
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph for output shape is not the same as input shape.
    InstanceNormalizationTester{.input = {.type = OperandDataType::kFloat32,
                                          .dimensions = {1, 2, 3, 3}},
                                .output = {.type = OperandDataType::kFloat32,
                                           .dimensions = {1, 1, 3, 3}},
                                .expected = false}
        .Test();
  }
  {
    // Test the invalid graph for input is not a 4-D tensor.
    InstanceNormalizationTester{
        .input = {.type = OperandDataType::kFloat32, .dimensions = {1, 2, 3}},
        .output = {.type = OperandDataType::kFloat32, .dimensions = {1, 2, 3}},
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph for input operand == output operand.
    auto context_properties = GetContextPropertiesForTesting();
    GraphInfoBuilder builder;
    uint64_t input_operand_id =
        builder.BuildInput("input", {1, 2, 3, 4}, OperandDataType::kFloat32);
    builder.BuildInstanceNormalization(
        input_operand_id, input_operand_id,
        InstanceNormalizationTester::InstanceNormalizationAttributes{});
    EXPECT_FALSE(WebNNGraphBuilderImpl::IsValidForTesting(
        context_properties, builder.GetGraphInfo()));
  }
  {
    // Test the invalid graph when the output is the same as the scale.
    auto context_properties = GetContextPropertiesForTesting();
    GraphInfoBuilder builder;
    uint64_t input_operand_id =
        builder.BuildInput("input", {1, 2, 3, 4}, OperandDataType::kFloat32);
    uint64_t scale_operand_id =
        builder.BuildInput("scale", {2}, OperandDataType::kFloat32);

    InstanceNormalizationTester::InstanceNormalizationAttributes attributes;
    attributes.scale_operand_id = scale_operand_id;

    builder.BuildInstanceNormalization(input_operand_id, scale_operand_id,
                                       std::move(attributes));
    EXPECT_FALSE(WebNNGraphBuilderImpl::IsValidForTesting(
        context_properties, builder.GetGraphInfo()));
  }
  {
    // Test the invalid graph when the output is the same as the bias.
    auto context_properties = GetContextPropertiesForTesting();
    GraphInfoBuilder builder;
    uint64_t input_operand_id =
        builder.BuildInput("input", {1, 2, 3, 4}, OperandDataType::kFloat32);
    uint64_t bias_operand_id =
        builder.BuildInput("bias", {2}, OperandDataType::kFloat32);

    InstanceNormalizationTester::InstanceNormalizationAttributes attributes;
    attributes.bias_operand_id = bias_operand_id;

    builder.BuildInstanceNormalization(input_operand_id, bias_operand_id,
                                       std::move(attributes));
    EXPECT_FALSE(WebNNGraphBuilderImpl::IsValidForTesting(
        context_properties, builder.GetGraphInfo()));
  }
}

struct LayerNormalizationTester {
  OperandInfo input;
  std::optional<OperandInfo> scale;
  std::optional<OperandInfo> bias;
  struct LayerNormalizationAttributes {
    std::optional<uint64_t> scale_operand_id;
    std::optional<uint64_t> bias_operand_id;
    std::vector<uint32_t> axes;
    float epsilon = 1e-5;
  };
  LayerNormalizationAttributes attributes;
  OperandInfo output;
  bool expected;

  void Test() {
    auto context_properties = GetContextPropertiesForTesting();

    // Build the graph with mojo type.
    GraphInfoBuilder builder;
    uint64_t input_operand_id =
        builder.BuildInput("input", input.dimensions, input.type);
    uint64_t output_operand_id =
        builder.BuildOutput("output", output.dimensions, output.type);

    if (scale.has_value()) {
      attributes.scale_operand_id =
          builder.BuildInput("scale", scale->dimensions, scale->type);
    }
    if (bias.has_value()) {
      attributes.bias_operand_id =
          builder.BuildInput("bias", bias->dimensions, bias->type);
    }
    builder.BuildLayerNormalization(input_operand_id, output_operand_id,
                                    std::move(attributes));
    EXPECT_EQ(WebNNGraphBuilderImpl::IsValidForTesting(context_properties,
                                                       builder.GetGraphInfo()),
              expected);
  }
};

TEST_F(WebNNGraphImplTest, LayerNormalizationTest) {
  {
    // Test building layerNormalization with default option for scalar input.
    LayerNormalizationTester{
        .input = {.type = OperandDataType::kFloat32, .dimensions = {}},
        .attributes = {.axes = {}},
        .output = {.type = OperandDataType::kFloat32, .dimensions = {}},
        .expected = true}
        .Test();
  }
  {
    // Test building layerNormalization with 4-D input.
    LayerNormalizationTester{
        .input = {.type = OperandDataType::kFloat16,
                  .dimensions = {1, 2, 3, 4}},
        .scale = OperandInfo{.type = OperandDataType::kFloat16,
                             .dimensions = {3, 4}},
        .bias = OperandInfo{.type = OperandDataType::kFloat16,
                            .dimensions = {3, 4}},
        .attributes = {.axes = {2, 3}},
        .output = {.type = OperandDataType::kFloat16,
                   .dimensions = {1, 2, 3, 4}},
        .expected = true}
        .Test();
  }
  {
    // Test the invalid graph when the input is a scalar and the axes is not
    // empty.
    LayerNormalizationTester{
        .input = {.type = OperandDataType::kFloat32, .dimensions = {}},
        .attributes = {.axes = {0}},
        .output = {.type = OperandDataType::kFloat32, .dimensions = {}},
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph for the input data type is int64.
    LayerNormalizationTester{
        .input = {.type = OperandDataType::kInt64, .dimensions = {1}},
        .attributes = {.axes = {}},
        .output = {.type = OperandDataType::kInt64, .dimensions = {1}},
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph for the axes have duplications.
    LayerNormalizationTester{
        .input = {.type = OperandDataType::kFloat32, .dimensions = {1, 2}},
        .attributes = {.axes = {0, 0}},
        .output = {.type = OperandDataType::kFloat32, .dimensions = {1, 2}},
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph for the axis is greater than the input rank.
    LayerNormalizationTester{
        .input = {.type = OperandDataType::kFloat32, .dimensions = {1, 2}},
        .attributes = {.axes = {2}},
        .output = {.type = OperandDataType::kFloat32, .dimensions = {1, 2}},
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph for the bias type doesn't match the input type.
    LayerNormalizationTester{
        .input = {.type = OperandDataType::kFloat16,
                  .dimensions = {1, 2, 3, 4}},
        .bias = OperandInfo{.type = OperandDataType::kFloat32,
                            .dimensions = {3, 4}},
        .attributes = {.axes = {2, 3}},
        .output = {.type = OperandDataType::kFloat16,
                   .dimensions = {1, 2, 3, 4}},
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph for the scale shape doesn't match the reduction
    // dimensions.
    LayerNormalizationTester{
        .input = {.type = OperandDataType::kFloat16,
                  .dimensions = {1, 2, 3, 4}},
        .scale = OperandInfo{.type = OperandDataType::kFloat16,
                             .dimensions = {2, 3}},
        .attributes = {.axes = {2, 3}},
        .output = {.type = OperandDataType::kFloat16,
                   .dimensions = {1, 2, 3, 4}},
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph for the output shapes are not expected.
    LayerNormalizationTester{.input = {.type = OperandDataType::kFloat16,
                                       .dimensions = {1, 2, 3, 4}},
                             .attributes = {.axes = {}},
                             .output = {.type = OperandDataType::kFloat16,
                                        .dimensions = {1, 2, 3, 3}},
                             .expected = false}
        .Test();
  }
  {
    // Test the invalid graph for the output type doesn't match the input type.
    LayerNormalizationTester{.input = {.type = OperandDataType::kFloat16,
                                       .dimensions = {1, 2, 3, 4}},
                             .attributes = {.axes = {}},
                             .output = {.type = OperandDataType::kFloat32,
                                        .dimensions = {1, 2, 3, 4}},
                             .expected = false}
        .Test();
  }
  {
    // Test the invalid graph when the output is the same as the input.
    auto context_properties = GetContextPropertiesForTesting();
    GraphInfoBuilder builder;
    uint64_t input_operand_id =
        builder.BuildInput("input", {1, 2, 3, 4}, OperandDataType::kFloat32);
    builder.BuildLayerNormalization(
        input_operand_id, input_operand_id,
        LayerNormalizationTester::LayerNormalizationAttributes{});
    EXPECT_FALSE(WebNNGraphBuilderImpl::IsValidForTesting(
        context_properties, builder.GetGraphInfo()));
  }
  {
    // Test the invalid graph when the output is the same as the scale.
    auto context_properties = GetContextPropertiesForTesting();
    GraphInfoBuilder builder;
    uint64_t input_operand_id =
        builder.BuildInput("input", {1, 2, 3, 4}, OperandDataType::kFloat32);
    uint64_t scale_operand_id =
        builder.BuildInput("scale", {1, 2, 3, 4}, OperandDataType::kFloat32);

    LayerNormalizationTester::LayerNormalizationAttributes attributes;
    attributes.scale_operand_id = scale_operand_id;
    attributes.axes = {0, 1, 2, 3};

    builder.BuildLayerNormalization(input_operand_id, scale_operand_id,
                                    std::move(attributes));
    EXPECT_FALSE(WebNNGraphBuilderImpl::IsValidForTesting(
        context_properties, builder.GetGraphInfo()));
  }
  {
    // Test the invalid graph when the output is the same as the bias.
    auto context_properties = GetContextPropertiesForTesting();
    GraphInfoBuilder builder;
    uint64_t input_operand_id =
        builder.BuildInput("input", {1, 2, 3, 4}, OperandDataType::kFloat32);
    uint64_t bias_operand_id =
        builder.BuildInput("bias", {1, 2, 3, 4}, OperandDataType::kFloat32);

    LayerNormalizationTester::LayerNormalizationAttributes attributes;
    attributes.bias_operand_id = bias_operand_id;
    attributes.axes = {0, 1, 2, 3};

    builder.BuildLayerNormalization(input_operand_id, bias_operand_id,
                                    std::move(attributes));
    EXPECT_FALSE(WebNNGraphBuilderImpl::IsValidForTesting(
        context_properties, builder.GetGraphInfo()));
  }
}

struct LstmTester {
  struct LstmAttributes {
    std::optional<uint64_t> bias_operand_id;
    std::optional<uint64_t> recurrent_bias_operand_id;
    std::optional<uint64_t> peephole_weight_operand_id;
    std::optional<uint64_t> initial_hidden_state_operand_id;
    std::optional<uint64_t> initial_cell_state_operand_id;
    bool return_sequence = false;
    mojom::RecurrentNetworkDirection direction =
        mojom::RecurrentNetworkDirection::kForward;
    mojom::LstmWeightLayout layout = mojom::LstmWeightLayout::kIofg;
    std::vector<mojom::RecurrentNetworkActivation> activations = {
        mojom::RecurrentNetworkActivation::kSigmoid,
        mojom::RecurrentNetworkActivation::kTanh,
        mojom::RecurrentNetworkActivation::kTanh};
  };

  OperandInfo input;
  OperandInfo weight;
  OperandInfo recurrent_weight;
  uint32_t steps;
  uint32_t hidden_size;
  std::optional<OperandInfo> bias;
  std::optional<OperandInfo> recurrent_bias;
  std::optional<OperandInfo> peephole_weight;
  std::optional<OperandInfo> initial_hidden_state;
  std::optional<OperandInfo> initial_cell_state;
  LstmAttributes attributes;
  std::vector<OperandInfo> outputs;
  bool expected;

  void Test() {
    auto context_properties = GetContextPropertiesForTesting();

    // Build the graph with mojo type.
    GraphInfoBuilder builder;
    uint64_t input_operand_id =
        builder.BuildInput("input", input.dimensions, input.type);
    uint64_t weight_operand_id =
        builder.BuildInput("weight", weight.dimensions, weight.type);
    uint64_t recurrent_weight_operand_id = builder.BuildInput(
        "recurrentWeight", recurrent_weight.dimensions, recurrent_weight.type);

    std::vector<uint64_t> output_operand_ids;
    output_operand_ids.reserve(outputs.size());
    for (size_t i = 0; i < outputs.size(); ++i) {
      output_operand_ids.push_back(
          builder.BuildOutput(base::StringPrintf("output%zu", i),
                              outputs[i].dimensions, outputs[i].type));
    }

    if (bias.has_value()) {
      attributes.bias_operand_id =
          builder.BuildInput("bias", bias->dimensions, bias->type);
    }
    if (recurrent_bias.has_value()) {
      attributes.recurrent_bias_operand_id = builder.BuildInput(
          "recurrentBias", recurrent_bias->dimensions, recurrent_bias->type);
    }
    if (peephole_weight.has_value()) {
      attributes.peephole_weight_operand_id = builder.BuildInput(
          "peepholeWeight", peephole_weight->dimensions, peephole_weight->type);
    }
    if (initial_hidden_state.has_value()) {
      attributes.initial_hidden_state_operand_id = builder.BuildInput(
          "initialHiddenState", initial_hidden_state->dimensions,
          initial_hidden_state->type);
    }
    if (initial_cell_state.has_value()) {
      attributes.initial_cell_state_operand_id =
          builder.BuildInput("initialCellState", initial_cell_state->dimensions,
                             initial_cell_state->type);
    }

    builder.BuildLstm(input_operand_id, weight_operand_id,
                      recurrent_weight_operand_id,
                      std::move(output_operand_ids), steps, hidden_size,
                      std::move(attributes));
    EXPECT_EQ(WebNNGraphBuilderImpl::IsValidForTesting(context_properties,
                                                       builder.GetGraphInfo()),
              expected);
  }
};

TEST_F(WebNNGraphImplTest, LstmTest) {
  {
    // Test the lstm operator.
    uint32_t steps = 2;
    uint32_t batch_size = 1;
    uint32_t input_size = 3;
    uint32_t hidden_size = 4;
    uint32_t direction_count = 2;
    LstmTester{
        .input = {.type = OperandDataType::kFloat32,
                  .dimensions = {steps, batch_size, input_size}},
        .weight = {.type = OperandDataType::kFloat32,
                   .dimensions = {direction_count, 4 * hidden_size,
                                  input_size}},
        .recurrent_weight = {.type = OperandDataType::kFloat32,
                             .dimensions = {direction_count, 4 * hidden_size,
                                            hidden_size}},
        .steps = steps,
        .hidden_size = hidden_size,
        .bias = OperandInfo{.type = OperandDataType::kFloat32,
                            .dimensions = {direction_count, 4 * hidden_size}},
        .recurrent_bias =
            OperandInfo{.type = OperandDataType::kFloat32,
                        .dimensions = {direction_count, 4 * hidden_size}},
        .peephole_weight =
            OperandInfo{.type = OperandDataType::kFloat32,
                        .dimensions = {direction_count, 3 * hidden_size}},
        .initial_hidden_state =
            OperandInfo{
                .type = OperandDataType::kFloat32,
                .dimensions = {direction_count, batch_size, hidden_size}},
        .initial_cell_state =
            OperandInfo{
                .type = OperandDataType::kFloat32,
                .dimensions = {direction_count, batch_size, hidden_size}},
        .attributes = {.return_sequence = true,
                       .direction = mojom::RecurrentNetworkDirection::kBoth},
        .outputs = {{.type = OperandDataType::kFloat32,
                     .dimensions = {direction_count, batch_size, hidden_size}},
                    {.type = OperandDataType::kFloat32,
                     .dimensions = {direction_count, batch_size, hidden_size}},
                    {.type = OperandDataType::kFloat32,
                     .dimensions = {steps, direction_count, batch_size,
                                    hidden_size}}},
        .expected = true}
        .Test();
  }
  {
    // Test the invalid graph when the shape of weight is incorrect.
    uint32_t steps = 2;
    uint32_t batch_size = 1;
    uint32_t input_size = 3;
    uint32_t hidden_size = 4;
    uint32_t direction_count = 1;
    LstmTester{
        .input = {.type = OperandDataType::kFloat32,
                  .dimensions = {steps, batch_size, input_size}},
        .weight = {.type = OperandDataType::kFloat32,
                   .dimensions = {direction_count, 4 * hidden_size, 1000}},
        .recurrent_weight = {.type = OperandDataType::kFloat32,
                             .dimensions = {direction_count, 4 * hidden_size,
                                            hidden_size}},
        .steps = steps,
        .hidden_size = hidden_size,
        .outputs = {{.type = OperandDataType::kFloat32,
                     .dimensions = {direction_count, batch_size, hidden_size}},
                    {.type = OperandDataType::kFloat32,
                     .dimensions = {direction_count, batch_size, hidden_size}}},
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph when the output is incorrect.
    uint32_t steps = 2;
    uint32_t batch_size = 1;
    uint32_t input_size = 3;
    uint32_t hidden_size = 4;
    uint32_t direction_count = 1;
    LstmTester{
        .input = {.type = OperandDataType::kFloat32,
                  .dimensions = {steps, batch_size, input_size}},
        .weight = {.type = OperandDataType::kFloat32,
                   .dimensions = {direction_count, 4 * hidden_size,
                                  input_size}},
        .recurrent_weight = {.type = OperandDataType::kFloat32,
                             .dimensions = {direction_count, 4 * hidden_size,
                                            hidden_size}},
        .steps = steps,
        .hidden_size = hidden_size,
        .outputs = {{.type = OperandDataType::kFloat32,
                     .dimensions = {direction_count, batch_size, hidden_size}},
                    {.type = OperandDataType::kFloat32,
                     .dimensions = {direction_count, batch_size, 1000}}},
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph when the recurrent weight has the same id as
    // one of the outputs.
    uint32_t steps = 2;
    uint32_t batch_size = 16;
    uint32_t input_size = 3;
    uint32_t hidden_size = 4;
    uint32_t direction_count = 1;

    auto context_properties = GetContextPropertiesForTesting();
    GraphInfoBuilder builder;
    uint64_t input_operand_id = builder.BuildInput(
        "input", {steps, batch_size, input_size}, OperandDataType::kFloat32);
    uint64_t weight_operand_id = builder.BuildInput(
        "weight", {direction_count, 4 * hidden_size, input_size},
        OperandDataType::kFloat32);
    uint64_t recurrent_weight_operand_id = builder.BuildInput(
        "recurrentWeight", {direction_count, 4 * hidden_size, hidden_size},
        OperandDataType::kFloat32);

    uint64_t output_operand_id = builder.BuildOutput(
        "output", {direction_count, batch_size, hidden_size},
        OperandDataType::kFloat32);
    builder.BuildLstm(input_operand_id, weight_operand_id,
                      recurrent_weight_operand_id,
                      {output_operand_id, recurrent_weight_operand_id}, steps,
                      hidden_size, LstmTester::LstmAttributes{});
    EXPECT_FALSE(WebNNGraphBuilderImpl::IsValidForTesting(
        context_properties, builder.GetGraphInfo()));
  }
  {
    // Test the invalid graph when the initial cell state has the same id as
    // one of the outputs.
    uint32_t steps = 2;
    uint32_t batch_size = 1;
    uint32_t input_size = 3;
    uint32_t hidden_size = 4;
    uint32_t direction_count = 1;

    auto context_properties = GetContextPropertiesForTesting();
    GraphInfoBuilder builder;
    uint64_t input_operand_id = builder.BuildInput(
        "input", {steps, batch_size, input_size}, OperandDataType::kFloat32);
    uint64_t weight_operand_id = builder.BuildInput(
        "weight", {direction_count, 4 * hidden_size, input_size},
        OperandDataType::kFloat32);
    uint64_t recurrent_weight_operand_id = builder.BuildInput(
        "recurrentWeight", {direction_count, 4 * hidden_size, hidden_size},
        OperandDataType::kFloat32);

    uint64_t initial_cell_state_operand_id = builder.BuildInput(
        "initialCellState", {direction_count, batch_size, hidden_size},
        OperandDataType::kFloat32);
    uint64_t output_operand_id = builder.BuildOutput(
        "output", {direction_count, batch_size, hidden_size},
        OperandDataType::kFloat32);

    builder.BuildLstm(
        input_operand_id, weight_operand_id, recurrent_weight_operand_id,
        {initial_cell_state_operand_id, output_operand_id}, steps, hidden_size,
        LstmTester::LstmAttributes{.initial_cell_state_operand_id =
                                       initial_cell_state_operand_id});
    EXPECT_FALSE(WebNNGraphBuilderImpl::IsValidForTesting(
        context_properties, builder.GetGraphInfo()));
  }
}

struct LstmCellTester {
  struct LstmCellAttributes {
    std::optional<uint64_t> bias_operand_id;
    std::optional<uint64_t> recurrent_bias_operand_id;
    std::optional<uint64_t> peephole_weight_operand_id;
    mojom::LstmWeightLayout layout = mojom::LstmWeightLayout::kIofg;
    std::vector<mojom::RecurrentNetworkActivation> activations = {
        mojom::RecurrentNetworkActivation::kSigmoid,
        mojom::RecurrentNetworkActivation::kTanh,
        mojom::RecurrentNetworkActivation::kTanh};
  };

  OperandInfo input;
  OperandInfo weight;
  OperandInfo recurrent_weight;
  OperandInfo hidden_state;
  OperandInfo cell_state;
  uint32_t hidden_size;
  std::optional<OperandInfo> bias;
  std::optional<OperandInfo> recurrent_bias;
  std::optional<OperandInfo> peephole_weight;
  LstmCellAttributes attributes;
  std::vector<OperandInfo> outputs;
  bool expected;

  void Test() {
    auto context_properties = GetContextPropertiesForTesting();

    // Build the graph with mojo type.
    GraphInfoBuilder builder;
    uint64_t input_operand_id =
        builder.BuildInput("input", input.dimensions, input.type);
    uint64_t weight_operand_id =
        builder.BuildInput("weight", weight.dimensions, weight.type);
    uint64_t recurrent_weight_operand_id = builder.BuildInput(
        "recurrentWeight", recurrent_weight.dimensions, recurrent_weight.type);
    uint64_t hidden_state_operand_id = builder.BuildInput(
        "hiddenState", hidden_state.dimensions, hidden_state.type);
    uint64_t cell_state_operand_id =
        builder.BuildInput("cellState", cell_state.dimensions, cell_state.type);

    std::vector<uint64_t> output_operand_ids;
    output_operand_ids.reserve(outputs.size());
    for (size_t i = 0; i < outputs.size(); ++i) {
      output_operand_ids.push_back(
          builder.BuildOutput(base::StringPrintf("output%zu", i),
                              outputs[i].dimensions, outputs[i].type));
    }

    if (bias.has_value()) {
      attributes.bias_operand_id =
          builder.BuildInput("bias", bias->dimensions, bias->type);
    }
    if (recurrent_bias.has_value()) {
      attributes.recurrent_bias_operand_id = builder.BuildInput(
          "recurrentBias", recurrent_bias->dimensions, recurrent_bias->type);
    }
    if (peephole_weight.has_value()) {
      attributes.peephole_weight_operand_id = builder.BuildInput(
          "peepholeWeight", peephole_weight->dimensions, peephole_weight->type);
    }

    builder.BuildLstmCell(input_operand_id, weight_operand_id,
                          recurrent_weight_operand_id, hidden_state_operand_id,
                          cell_state_operand_id, std::move(output_operand_ids),
                          hidden_size, std::move(attributes));
    EXPECT_EQ(WebNNGraphBuilderImpl::IsValidForTesting(context_properties,
                                                       builder.GetGraphInfo()),
              expected);
  }
};

TEST_F(WebNNGraphImplTest, LstmCellTest) {
  uint32_t batch_size = 15;
  uint32_t input_size = 12;
  uint32_t hidden_size = 20;

  OperandInfo valid_input = {.type = OperandDataType::kFloat32,
                             .dimensions = {batch_size, input_size}};
  OperandInfo valid_weight = {.type = OperandDataType::kFloat32,
                              .dimensions = {4 * hidden_size, input_size}};
  OperandInfo valid_recurrent_weight = {
      .type = OperandDataType::kFloat32,
      .dimensions = {4 * hidden_size, hidden_size}};
  OperandInfo valid_hidden_state = {.type = OperandDataType::kFloat32,
                                    .dimensions = {batch_size, hidden_size}};
  OperandInfo valid_cell_state = {.type = OperandDataType::kFloat32,
                                  .dimensions = {batch_size, hidden_size}};
  OperandInfo valid_bias = {.type = OperandDataType::kFloat32,
                            .dimensions = {4 * hidden_size}};
  OperandInfo valid_recurrent_bias = {.type = OperandDataType::kFloat32,
                                      .dimensions = {4 * hidden_size}};
  OperandInfo valid_peephole_weight = {.type = OperandDataType::kFloat32,
                                       .dimensions = {3 * hidden_size}};
  std::vector<OperandInfo> valid_outputs = {
      {.type = OperandDataType::kFloat32,
       .dimensions = {batch_size, hidden_size}},
      {.type = OperandDataType::kFloat32,
       .dimensions = {batch_size, hidden_size}}};
  {
    // Test a valid lstmCell operator.
    LstmCellTester{.input = valid_input,
                   .weight = valid_weight,
                   .recurrent_weight = valid_recurrent_weight,
                   .hidden_state = valid_hidden_state,
                   .cell_state = valid_cell_state,
                   .hidden_size = hidden_size,
                   .bias = valid_bias,
                   .recurrent_bias = valid_recurrent_bias,
                   .peephole_weight = valid_peephole_weight,
                   .outputs = valid_outputs,
                   .expected = true}
        .Test();
  }
  {
    // Test the invalid graph when the data type of the input is not one of the
    // floating point types.
    LstmCellTester{.input = {.type = OperandDataType::kUint32,
                             .dimensions = {batch_size, input_size}},
                   .weight = valid_weight,
                   .recurrent_weight = valid_recurrent_weight,
                   .hidden_state = valid_hidden_state,
                   .cell_state = valid_cell_state,
                   .hidden_size = hidden_size,
                   .outputs = valid_outputs,
                   .expected = false}
        .Test();
  }
  {
    // Test the invalid graph when the data type of the weight is incorrect.
    LstmCellTester{.input = valid_input,
                   .weight = {.type = OperandDataType::kFloat16,
                              .dimensions = {4 * hidden_size, input_size}},
                   .recurrent_weight = valid_recurrent_weight,
                   .hidden_state = valid_hidden_state,
                   .cell_state = valid_cell_state,
                   .hidden_size = hidden_size,
                   .outputs = valid_outputs,
                   .expected = false}
        .Test();
  }
  {
    // Test the invalid graph when the rank of the recurrent weight is
    // incorrect.
    LstmCellTester{.input = valid_input,
                   .weight = valid_weight,
                   .recurrent_weight = {.type = OperandDataType::kFloat32,
                                        .dimensions = {4 * hidden_size}},
                   .hidden_state = valid_hidden_state,
                   .cell_state = valid_cell_state,
                   .hidden_size = hidden_size,
                   .outputs = valid_outputs,
                   .expected = false}
        .Test();
  }
  {
    // Test the invalid graph when the shape of the hidden state is incorrect.
    LstmCellTester{.input = valid_input,
                   .weight = valid_weight,
                   .recurrent_weight = valid_recurrent_weight,
                   .hidden_state = {.type = OperandDataType::kFloat32,
                                    .dimensions = {batch_size, 1000}},
                   .cell_state = valid_cell_state,
                   .hidden_size = hidden_size,
                   .outputs = valid_outputs,
                   .expected = false}
        .Test();
  }
  {
    // Test the invalid graph when the rank of the cell state is incorrect.
    LstmCellTester{
        .input = valid_input,
        .weight = valid_weight,
        .recurrent_weight = valid_recurrent_weight,
        .hidden_state = valid_hidden_state,
        .cell_state = {.type = OperandDataType::kFloat32,
                       .dimensions = {batch_size, hidden_size, 1000}},
        .hidden_size = hidden_size,
        .outputs = valid_outputs,
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph when the data type of the bias incorrect.
    LstmCellTester{.input = valid_input,
                   .weight = valid_weight,
                   .recurrent_weight = valid_recurrent_weight,
                   .hidden_state = valid_hidden_state,
                   .cell_state = valid_cell_state,
                   .hidden_size = hidden_size,
                   .bias = OperandInfo{.type = OperandDataType::kUint32,
                                       .dimensions = {4 * hidden_size}},
                   .outputs = valid_outputs,
                   .expected = false}
        .Test();
  }
  {
    // Test the invalid graph when the shape of the recurrent bias is incorrect.
    LstmCellTester{
        .input = valid_input,
        .weight = valid_weight,
        .recurrent_weight = valid_recurrent_weight,
        .hidden_state = valid_hidden_state,
        .cell_state = valid_cell_state,
        .hidden_size = hidden_size,
        .recurrent_bias = OperandInfo{.type = OperandDataType::kFloat32,
                                      .dimensions = {1000}},
        .outputs = valid_outputs,
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph when the data type of the peephole weight is
    // incorrect.
    LstmCellTester{
        .input = valid_input,
        .weight = valid_weight,
        .recurrent_weight = valid_recurrent_weight,
        .hidden_state = valid_hidden_state,
        .cell_state = valid_cell_state,
        .hidden_size = hidden_size,
        .peephole_weight = OperandInfo{.type = OperandDataType::kInt64,
                                       .dimensions = {3 * hidden_size}},
        .outputs = valid_outputs,
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph when the output data type is incorrect.
    LstmCellTester{.input = valid_input,
                   .weight = valid_weight,
                   .recurrent_weight = valid_recurrent_weight,
                   .hidden_state = valid_hidden_state,
                   .cell_state = valid_cell_state,
                   .hidden_size = hidden_size,
                   .outputs = {{.type = OperandDataType::kInt8,
                                .dimensions = {batch_size, hidden_size}},
                               {.type = OperandDataType::kInt8,
                                .dimensions = {batch_size, hidden_size}}},
                   .expected = false}
        .Test();
  }
  {
    // Test the invalid graph when the cell state has the same id as
    // one of the outputs.
    auto context_properties = GetContextPropertiesForTesting();
    GraphInfoBuilder builder;
    uint64_t input_operand_id = builder.BuildInput(
        "input", {batch_size, input_size}, OperandDataType::kFloat32);
    uint64_t weight_operand_id = builder.BuildInput(
        "weight", {4 * hidden_size, input_size}, OperandDataType::kFloat32);
    uint64_t recurrent_weight_operand_id =
        builder.BuildInput("recurrentWeight", {4 * hidden_size, hidden_size},
                           OperandDataType::kFloat32);
    uint64_t hidden_state_operand_id = builder.BuildInput(
        "hiddenState", {batch_size, hidden_size}, OperandDataType::kFloat32);
    uint64_t cell_state_operand_id = builder.BuildInput(
        "cellState", {batch_size, hidden_size}, OperandDataType::kFloat32);
    uint64_t output_operand_id = builder.BuildOutput(
        "output", {batch_size, hidden_size}, OperandDataType::kFloat32);

    builder.BuildLstmCell(input_operand_id, weight_operand_id,
                          recurrent_weight_operand_id, hidden_state_operand_id,
                          cell_state_operand_id,
                          {cell_state_operand_id, output_operand_id},
                          hidden_size, LstmTester::LstmAttributes{});
    EXPECT_FALSE(WebNNGraphBuilderImpl::IsValidForTesting(
        context_properties, builder.GetGraphInfo()));
  }
}

struct MatmulTester {
  OperandInfo a;
  OperandInfo b;
  OperandInfo output;
  bool expected;

  void Test() {
    auto context_properties = GetContextPropertiesForTesting();

    // Build the graph with mojo type.
    GraphInfoBuilder builder;
    uint64_t a_operand_id = builder.BuildInput("a", a.dimensions, a.type);
    uint64_t b_operand_id = builder.BuildInput("b", b.dimensions, b.type);
    uint64_t output_operand_id =
        builder.BuildOutput("output", output.dimensions, output.type);

    builder.BuildMatmul(a_operand_id, b_operand_id, output_operand_id);
    EXPECT_EQ(WebNNGraphBuilderImpl::IsValidForTesting(context_properties,
                                                       builder.GetGraphInfo()),
              expected);
  }
};

TEST_F(WebNNGraphImplTest, MatmulTest) {
  {
    // Test building matmul with 2-D * 2-D.
    MatmulTester{
        .a = {.type = OperandDataType::kFloat32, .dimensions = {2, 3}},
        .b = {.type = OperandDataType::kFloat32, .dimensions = {3, 4}},
        .output = {.type = OperandDataType::kFloat32, .dimensions = {2, 4}},
        .expected = true}
        .Test();
  }
  {
    // Test building matmul with 2-D * 4-D.
    MatmulTester{
        .a = {.type = OperandDataType::kFloat32, .dimensions = {2, 3}},
        .b = {.type = OperandDataType::kFloat32, .dimensions = {2, 3, 3, 4}},
        .output = {.type = OperandDataType::kFloat32,
                   .dimensions = {2, 3, 2, 4}},
        .expected = true}
        .Test();
  }
  {
    // Test building matmul with 3-D * 4-D using broadcasting.
    MatmulTester{
        .a = {.type = OperandDataType::kFloat32, .dimensions = {2, 2, 3}},
        .b = {.type = OperandDataType::kFloat32, .dimensions = {3, 1, 3, 4}},
        .output = {.type = OperandDataType::kFloat32,
                   .dimensions = {3, 2, 2, 4}},
        .expected = true}
        .Test();
  }
  {
    // Test the invalid graph for one input rank is smaller than 2.
    MatmulTester{
        .a = {.type = OperandDataType::kFloat32, .dimensions = {3}},
        .b = {.type = OperandDataType::kFloat32, .dimensions = {3, 4}},
        .output = {.type = OperandDataType::kFloat32, .dimensions = {3, 4}},
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph for the number of columns in first matrix
    // mismatches with the number of rows in second matrix.
    MatmulTester{
        .a = {.type = OperandDataType::kFloat32, .dimensions = {3, 2}},
        .b = {.type = OperandDataType::kFloat32, .dimensions = {3, 4}},
        .output = {.type = OperandDataType::kFloat32, .dimensions = {3, 4}},
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph for the input shapes are not broadcastable.
    MatmulTester{
        .a = {.type = OperandDataType::kFloat32, .dimensions = {3, 2, 3}},
        .b = {.type = OperandDataType::kFloat32, .dimensions = {2, 3, 4}},
        .output = {.type = OperandDataType::kFloat32, .dimensions = {3, 4}},
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph if the input is not floating point.
    MatmulTester{
        .a = {.type = OperandDataType::kUint8, .dimensions = {2, 3}},
        .b = {.type = OperandDataType::kUint8, .dimensions = {3, 4}},
        .output = {.type = OperandDataType::kUint8, .dimensions = {2, 4}},
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph for the output shapes are not expected.
    MatmulTester{
        .a = {.type = OperandDataType::kFloat32, .dimensions = {2, 3}},
        .b = {.type = OperandDataType::kFloat32, .dimensions = {3, 4}},
        .output = {.type = OperandDataType::kFloat32, .dimensions = {3, 4}},
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph for input types are not same.
    MatmulTester{
        .a = {.type = OperandDataType::kFloat32, .dimensions = {2, 3}},
        .b = {.type = OperandDataType::kInt32, .dimensions = {3, 4}},
        .output = {.type = OperandDataType::kFloat32, .dimensions = {2, 4}},
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph for output type is not the same as input type.
    MatmulTester{
        .a = {.type = OperandDataType::kFloat32, .dimensions = {2, 3}},
        .b = {.type = OperandDataType::kFloat32, .dimensions = {3, 4}},
        .output = {.type = OperandDataType::kInt32, .dimensions = {2, 4}},
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph when the output is as same as one input.
    auto context_properties = GetContextPropertiesForTesting();
    GraphInfoBuilder builder;
    uint64_t a_operand_id =
        builder.BuildInput("a", {2, 3}, OperandDataType::kFloat32);
    uint64_t b_operand_id =
        builder.BuildInput("b", {3, 4}, OperandDataType::kFloat32);
    builder.BuildMatmul(a_operand_id, b_operand_id, a_operand_id);
    EXPECT_FALSE(WebNNGraphBuilderImpl::IsValidForTesting(
        context_properties, builder.GetGraphInfo()));
  }
}

struct PadTester {
  OperandInfo input;
  std::vector<uint32_t> beginning_padding;
  std::vector<uint32_t> ending_padding;
  mojom::PaddingMode::Tag mode = mojom::PaddingMode::Tag::kConstant;
  float value = 0;
  OperandInfo output;
  bool expected;

  void Test() {
    auto context_properties = GetContextPropertiesForTesting();

    // Build the graph with mojo type.
    GraphInfoBuilder builder;
    uint64_t input_operand_id =
        builder.BuildInput("input", input.dimensions, input.type);
    uint64_t output_operand_id =
        builder.BuildOutput("output", output.dimensions, output.type);
    builder.BuildPad(input_operand_id, output_operand_id, beginning_padding,
                     ending_padding, mode, value);
    EXPECT_EQ(WebNNGraphBuilderImpl::IsValidForTesting(context_properties,
                                                       builder.GetGraphInfo()),
              expected);
  }
};

TEST_F(WebNNGraphImplTest, PadTest) {
  {
    // Test pad with default options, beginningPadding = {1, 2} and
    // endingPadding = {1, 2}.
    PadTester{
        .input = {.type = OperandDataType::kFloat32, .dimensions = {2, 3}},
        .beginning_padding = {1, 2},
        .ending_padding = {1, 2},
        .output = {.type = OperandDataType::kFloat32, .dimensions = {4, 7}},
        .expected = true}
        .Test();
  }
  {
    // Test pad with mode = "edge", beginningPadding = {1, 2} and
    // endingPadding = {1, 2}.
    PadTester{
        .input = {.type = OperandDataType::kFloat32, .dimensions = {2, 3}},
        .beginning_padding = {1, 2},
        .ending_padding = {1, 2},
        .mode = mojom::PaddingMode::Tag::kEdge,
        .output = {.type = OperandDataType::kFloat32, .dimensions = {4, 7}},
        .expected = true}
        .Test();
  }
  {
    // Test pad with value = 1, beginningPadding = {1, 2} and
    // endingPadding = {1, 2}.
    PadTester{
        .input = {.type = OperandDataType::kFloat32, .dimensions = {2, 3}},
        .beginning_padding = {1, 2},
        .ending_padding = {1, 2},
        .value = 1,
        .output = {.type = OperandDataType::kFloat32, .dimensions = {4, 7}},
        .expected = true}
        .Test();
  }
  {
    // Test the invalid graph when the length of beginningPadding is not
    // equal to the input rank.
    PadTester{
        .input = {.type = OperandDataType::kFloat32, .dimensions = {2, 3}},
        .beginning_padding = {1},
        .ending_padding = {1, 2},
        .output = {.type = OperandDataType::kFloat32, .dimensions = {4, 7}},
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph when the length of endingPadding is not equal
    // to the input rank.
    PadTester{
        .input = {.type = OperandDataType::kFloat32, .dimensions = {2, 3}},
        .beginning_padding = {1, 0},
        .ending_padding = {1, 2, 0},
        .output = {.type = OperandDataType::kFloat32, .dimensions = {4, 7}},
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph when the input is as same as output.
    auto context_properties = GetContextPropertiesForTesting();
    GraphInfoBuilder builder;
    uint64_t input_operand_id =
        builder.BuildInput("input", {2, 3}, OperandDataType::kFloat32);
    builder.BuildPad(input_operand_id, input_operand_id, {1, 1}, {1, 1},
                     mojom::PaddingMode::Tag::kConstant, 0);
    EXPECT_FALSE(WebNNGraphBuilderImpl::IsValidForTesting(
        context_properties, builder.GetGraphInfo()));
  }
}

struct Pool2dTester {
  OperandInfo input;
  struct Pool2dAttributes {
    std::vector<uint32_t> window_dimensions;
    std::vector<uint32_t> padding = {0, 0, 0, 0};
    std::vector<uint32_t> strides = {1, 1};
    std::vector<uint32_t> dilations = {1, 1};
    InputOperandLayout layout = InputOperandLayout::kNchw;
  };
  Pool2dAttributes attributes;
  OperandInfo output;
  bool expected;

  void Test() {
    Test(mojom::Pool2d::Kind::kAveragePool2d);
    Test(mojom::Pool2d::Kind::kL2Pool2d);
    Test(mojom::Pool2d::Kind::kMaxPool2d);
  }

  void Test(mojom::Pool2d::Kind kind) {
    auto context_properties = GetContextPropertiesForTesting();
    context_properties.input_operand_layout = attributes.layout;

    // Build the graph with mojo type.
    GraphInfoBuilder builder;
    uint64_t input_operand_id =
        builder.BuildInput("input", input.dimensions, input.type);
    uint64_t output_operand_id =
        builder.BuildOutput("output", output.dimensions, output.type);
    builder.BuildPool2d(kind, input_operand_id, output_operand_id,
                        std::move(attributes));
    EXPECT_EQ(WebNNGraphBuilderImpl::IsValidForTesting(context_properties,
                                                       builder.GetGraphInfo()),
              expected);
  }
};

TEST_F(WebNNGraphImplTest, Pool2dTest) {
  {
    // Test pool2d with default attributes.
    Pool2dTester{.input = {.type = OperandDataType::kFloat32,
                           .dimensions = {1, 3, 4, 4}},
                 .attributes = {.window_dimensions = {1, 1}, .strides = {1, 1}},
                 .output = {.type = OperandDataType::kFloat32,
                            .dimensions = {1, 3, 4, 4}},
                 .expected = true}
        .Test();
  }
  {
    // Test pool2d with window dimensions.
    Pool2dTester{.input = {.type = OperandDataType::kFloat32,
                           .dimensions = {1, 3, 5, 5}},
                 .attributes = {.window_dimensions = {2, 2}, .strides = {2, 2}},
                 .output = {.type = OperandDataType::kFloat32,
                            .dimensions = {1, 3, 3, 3}},
                 .expected = true}
        .Test();
  }
  {
    // Test pool2d with strides=2, padding=1 and floor rounding.
    Pool2dTester{.input = {.type = OperandDataType::kFloat16,
                           .dimensions = {1, 3, 7, 7}},
                 .attributes = {.window_dimensions = {4, 4},
                                .padding = {1, 1, 1, 1},
                                .strides = {2, 2}},
                 .output = {.type = OperandDataType::kFloat16,
                            .dimensions = {1, 3, 3, 3}},
                 .expected = true}
        .Test();
  }
  {
    // Test pool2d with strides=2, padding=1 and ceil rounding.
    Pool2dTester{.input = {.type = OperandDataType::kFloat32,
                           .dimensions = {1, 3, 7, 7}},
                 .attributes = {.window_dimensions = {4, 4},
                                .padding = {1, 1, 1, 1},
                                .strides = {2, 2}},
                 .output = {.type = OperandDataType::kFloat32,
                            .dimensions = {1, 3, 4, 4}},
                 .expected = true}
        .Test();
  }
  {
    // Test pool2d with layout="nhwc".
    Pool2dTester{.input = {.type = OperandDataType::kFloat16,
                           .dimensions = {1, 5, 5, 2}},
                 .attributes = {.window_dimensions = {3, 3},
                                .strides = {1, 1},
                                .layout = InputOperandLayout::kNhwc},
                 .output = {.type = OperandDataType::kFloat16,
                            .dimensions = {1, 3, 3, 2}},
                 .expected = true}
        .Test();
  }
  {
    // Test the invalid graph when the input is not a 4-D tensor.
    Pool2dTester{
        .input = {.type = OperandDataType::kFloat32, .dimensions = {3, 5, 5}},
        .attributes = {.window_dimensions = {5, 5},
                       .padding = {2, 2, 2, 2},
                       .strides = {1, 1}},
        .output = {.type = OperandDataType::kFloat32, .dimensions = {3, 5, 5}},
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph when window dimensions are 0.
    Pool2dTester{.input = {.type = OperandDataType::kFloat32,
                           .dimensions = {1, 3, 4, 4}},
                 .attributes = {.window_dimensions = {0, 0}, .strides = {1, 1}},
                 .output = {.type = OperandDataType::kFloat32,
                            .dimensions = {1, 3, 4, 4}},
                 .expected = false}
        .Test();
  }
  {
    // Test the invalid graph when strides are 0.
    Pool2dTester{.input = {.type = OperandDataType::kFloat32,
                           .dimensions = {1, 3, 4, 4}},
                 .attributes = {.window_dimensions = {1, 1}, .strides = {0, 0}},
                 .output = {.type = OperandDataType::kFloat32,
                            .dimensions = {1, 3, 4, 4}},
                 .expected = false}
        .Test();
  }
  {
    // Test the invalid graph when dilations are 0.
    Pool2dTester{.input = {.type = OperandDataType::kFloat32,
                           .dimensions = {1, 3, 4, 4}},
                 .attributes = {.window_dimensions = {1, 1},
                                .strides = {1, 1},
                                .dilations = {0, 0}},
                 .output = {.type = OperandDataType::kFloat32,
                            .dimensions = {1, 3, 4, 4}},
                 .expected = false}
        .Test();
  }
  {
    // Test the invalid graph for the output shapes are not expected.
    Pool2dTester{.input = {.type = OperandDataType::kFloat32,
                           .dimensions = {1, 3, 4, 4}},
                 .attributes = {.window_dimensions = {4, 4}, .strides = {1, 1}},
                 .output = {.type = OperandDataType::kFloat32,
                            .dimensions = {1, 2, 1, 1}},
                 .expected = false}
        .Test();
  }
  {
    // Test the invalid graph for output types don't match.
    Pool2dTester{.input = {.type = OperandDataType::kFloat32,
                           .dimensions = {1, 3, 4, 4}},
                 .attributes = {.window_dimensions = {4, 4}, .strides = {1, 1}},
                 .output = {.type = OperandDataType::kFloat16,
                            .dimensions = {1, 3, 1, 1}},
                 .expected = false}
        .Test();
  }
  {
    // Test the invalid graph if the input data type is not floating point for
    // averagePool2d.
    Pool2dTester{
        .input = {.type = OperandDataType::kInt32, .dimensions = {1, 3, 4, 4}},
        .attributes = {.window_dimensions = {4, 4}, .strides = {1, 1}},
        .output = {.type = OperandDataType::kInt32, .dimensions = {1, 3, 1, 1}},
        .expected = false}
        .Test(mojom::Pool2d::Kind::kAveragePool2d);
  }
  {
    // Test the invalid graph if the input data type is not floating point for
    // l2Pool2d.
    Pool2dTester{
        .input = {.type = OperandDataType::kInt8, .dimensions = {1, 3, 4, 4}},
        .attributes = {.window_dimensions = {4, 4}, .strides = {1, 1}},
        .output = {.type = OperandDataType::kInt8, .dimensions = {1, 3, 1, 1}},
        .expected = false}
        .Test(mojom::Pool2d::Kind::kL2Pool2d);
  }
}

struct PreluTester {
  OperandInfo input;
  OperandInfo slope;
  OperandInfo output;
  bool expected;

  void Test() {
    auto context_properties = GetContextPropertiesForTesting();

    // Build the graph with mojo type.
    GraphInfoBuilder builder;
    uint64_t input_operand_id =
        builder.BuildInput("input", input.dimensions, input.type);
    uint64_t slope_operand_id =
        builder.BuildInput("slope", slope.dimensions, slope.type);
    uint64_t output_operand_id =
        builder.BuildOutput("output", output.dimensions, output.type);
    builder.BuildPrelu(input_operand_id, slope_operand_id, output_operand_id);
    EXPECT_EQ(WebNNGraphBuilderImpl::IsValidForTesting(context_properties,
                                                       builder.GetGraphInfo()),
              expected);
  }
};

TEST_F(WebNNGraphImplTest, PreluTest) {
  {
    // Test prelu operator when the input and the slope have the same shape.
    PreluTester{
        .input = {.type = OperandDataType::kFloat32, .dimensions = {3, 2, 5}},
        .slope = {.type = OperandDataType::kFloat32, .dimensions = {3, 2, 5}},
        .output = {.type = OperandDataType::kFloat32, .dimensions = {3, 2, 5}},
        .expected = true}
        .Test();
  }
  {
    // Test prelu operator with a broadcastable slope.
    PreluTester{
        .input = {.type = OperandDataType::kFloat32, .dimensions = {3, 2, 5}},
        .slope = {.type = OperandDataType::kFloat32, .dimensions = {3, 1, 5}},
        .output = {.type = OperandDataType::kFloat32, .dimensions = {3, 2, 5}},
        .expected = true}
        .Test();
  }
  {
    // Test the invalid graph with an invalid slope.
    PreluTester{
        .input = {.type = OperandDataType::kFloat32, .dimensions = {3, 2, 5}},
        .slope = {.type = OperandDataType::kFloat32, .dimensions = {3, 5}},
        .output = {.type = OperandDataType::kFloat32, .dimensions = {3, 2, 5}},
        .expected = false}
        .Test();
  }
  {
    // Test prelu operator with input data type and slope data type = int32.
    PreluTester{
        .input = {.type = OperandDataType::kInt32, .dimensions = {3, 2, 5}},
        .slope = {.type = OperandDataType::kInt32, .dimensions = {3, 2, 5}},
        .output = {.type = OperandDataType::kInt32, .dimensions = {3, 2, 5}},
        .expected = true}
        .Test();
  }
  {
    // Test prelu operator with input data type and slope data type = float16.
    PreluTester{
        .input = {.type = OperandDataType::kFloat16, .dimensions = {3, 2, 5}},
        .slope = {.type = OperandDataType::kFloat16, .dimensions = {3, 2, 5}},
        .output = {.type = OperandDataType::kFloat16, .dimensions = {3, 2, 5}},
        .expected = true}
        .Test();
  }
  {
    // Test prelu operator with input data type and slope data type = int8.
    PreluTester{
        .input = {.type = OperandDataType::kInt8, .dimensions = {3, 2, 5}},
        .slope = {.type = OperandDataType::kInt8, .dimensions = {3, 2, 5}},
        .output = {.type = OperandDataType::kInt8, .dimensions = {3, 2, 5}},
        .expected = true}
        .Test();
  }
  {
    // Test the invalid graph when the slope datatype doesn't match the
    // input's datatype.
    PreluTester{
        .input = {.type = OperandDataType::kFloat16, .dimensions = {3, 2, 5}},
        .slope = {.type = OperandDataType::kFloat32, .dimensions = {3, 2, 5}},
        .output = {.type = OperandDataType::kFloat16, .dimensions = {3, 2, 5}},
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph when the input data type and slope data type =
    // uint32.
    PreluTester{
        .input = {.type = OperandDataType::kUint32, .dimensions = {3, 2, 5}},
        .slope = {.type = OperandDataType::kUint32, .dimensions = {3, 2, 5}},
        .output = {.type = OperandDataType::kUint32, .dimensions = {3, 2, 5}},
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph when the output datatype doesn't match the
    // input's datatype.
    PreluTester{
        .input = {.type = OperandDataType::kFloat16, .dimensions = {3, 2, 5}},
        .slope = {.type = OperandDataType::kFloat16, .dimensions = {3, 2, 5}},
        .output = {.type = OperandDataType::kFloat32, .dimensions = {3, 2, 5}},
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph for the output shapes are not expected.
    PreluTester{
        .input = {.type = OperandDataType::kFloat16, .dimensions = {3, 2, 5}},
        .slope = {.type = OperandDataType::kFloat16, .dimensions = {3, 2, 5}},
        .output = {.type = OperandDataType::kFloat16, .dimensions = {3, 2, 6}},
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph when the input is as same as output.
    auto context_properties = GetContextPropertiesForTesting();
    GraphInfoBuilder builder;
    uint64_t input_operand_id =
        builder.BuildInput("input", {2, 3}, OperandDataType::kFloat32);
    uint64_t slope_operand_id =
        builder.BuildInput("slope", {2, 3}, OperandDataType::kFloat32);
    builder.BuildPrelu(input_operand_id, slope_operand_id, input_operand_id);
    EXPECT_FALSE(WebNNGraphBuilderImpl::IsValidForTesting(
        context_properties, builder.GetGraphInfo()));
  }
  {
    // Test the invalid graph when the slope is as same as output.
    auto context_properties = GetContextPropertiesForTesting();
    GraphInfoBuilder builder;
    uint64_t input_operand_id =
        builder.BuildInput("input", {2, 3}, OperandDataType::kFloat32);
    uint64_t output_operand_id =
        builder.BuildOutput("output", {2, 3}, OperandDataType::kFloat32);
    builder.BuildPrelu(input_operand_id, output_operand_id, output_operand_id);
    EXPECT_FALSE(WebNNGraphBuilderImpl::IsValidForTesting(
        context_properties, builder.GetGraphInfo()));
  }
}

struct QuantizeLinearTester {
  OperandInfo input;
  OperandInfo scale;
  OperandInfo zero_point;
  OperandInfo output;
  bool expected;

  void Test() {
    auto context_properties = GetContextPropertiesForTesting();

    // Build the graph with mojo type.
    GraphInfoBuilder builder;
    uint64_t input_operand_id =
        builder.BuildInput("input", input.dimensions, input.type);
    uint64_t scale_operand_id =
        builder.BuildInput("scale", scale.dimensions, scale.type);
    uint64_t zero_point_operand_id = builder.BuildInput(
        "zero_point", zero_point.dimensions, zero_point.type);
    uint64_t output_operand_id =
        builder.BuildOutput("output", output.dimensions, output.type);
    builder.BuildQuantizeLinear(input_operand_id, scale_operand_id,
                                zero_point_operand_id, output_operand_id);
    EXPECT_EQ(WebNNGraphBuilderImpl::IsValidForTesting(context_properties,
                                                       builder.GetGraphInfo()),
              expected);
  }
};

TEST_F(WebNNGraphImplTest, QuantizeLinearTest) {
  {
    // Test quantizeLinear operator when the input, the scale and the zero_point
    // have the same shape.
    QuantizeLinearTester{
        .input = {.type = OperandDataType::kFloat32, .dimensions = {3, 2, 5}},
        .scale = {.type = OperandDataType::kFloat32, .dimensions = {3, 2, 5}},
        .zero_point = {.type = OperandDataType::kInt8, .dimensions = {3, 2, 5}},
        .output = {.type = OperandDataType::kInt8, .dimensions = {3, 2, 5}},
        .expected = true}
        .Test();
  }
  {
    // Test quantizeLinear operator with a broadcastable scale.
    QuantizeLinearTester{
        .input = {.type = OperandDataType::kFloat32, .dimensions = {3, 2, 5}},
        .scale = {.type = OperandDataType::kFloat32, .dimensions = {5}},
        .zero_point = {.type = OperandDataType::kInt8, .dimensions = {3, 2, 5}},
        .output = {.type = OperandDataType::kInt8, .dimensions = {3, 2, 5}},
        .expected = true}
        .Test();
  }
  {
    // Test quantizeLinear operator with a broadcastable zeroPoint.
    QuantizeLinearTester{
        .input = {.type = OperandDataType::kFloat32, .dimensions = {3, 2, 5}},
        .scale = {.type = OperandDataType::kFloat32, .dimensions = {3, 2, 5}},
        .zero_point = {.type = OperandDataType::kInt8, .dimensions = {5}},
        .output = {.type = OperandDataType::kInt8, .dimensions = {3, 2, 5}},
        .expected = true}
        .Test();
  }
  {
    // Test quantizeLinear operator with a broadcastable zeroPoint.
    QuantizeLinearTester{
        .input = {.type = OperandDataType::kFloat32, .dimensions = {3, 2, 5}},
        .scale = {.type = OperandDataType::kFloat32, .dimensions = {3, 2, 5}},
        .zero_point = {.type = OperandDataType::kInt8, .dimensions = {3, 1, 1}},
        .output = {.type = OperandDataType::kInt8, .dimensions = {3, 2, 5}},
        .expected = true}
        .Test();
  }
  {
    // Test the invalid graph with an invalid scale.
    QuantizeLinearTester{
        .input = {.type = OperandDataType::kFloat32, .dimensions = {3, 2, 5}},
        .scale = {.type = OperandDataType::kFloat32, .dimensions = {3, 5}},
        .zero_point = {.type = OperandDataType::kInt8, .dimensions = {5}},
        .output = {.type = OperandDataType::kInt8, .dimensions = {3, 2, 5}},
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph with an invalid zero_point.
    QuantizeLinearTester{
        .input = {.type = OperandDataType::kFloat32, .dimensions = {3, 2, 5}},
        .scale = {.type = OperandDataType::kFloat32, .dimensions = {5}},
        .zero_point = {.type = OperandDataType::kInt8, .dimensions = {2}},
        .output = {.type = OperandDataType::kInt8, .dimensions = {3, 2, 5}},
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph when the scale datatype doesn't match the
    // input's datatype.
    QuantizeLinearTester{
        .input = {.type = OperandDataType::kFloat32, .dimensions = {3, 2, 5}},
        .scale = {.type = OperandDataType::kFloat16, .dimensions = {5}},
        .zero_point = {.type = OperandDataType::kInt8, .dimensions = {5}},
        .output = {.type = OperandDataType::kInt8, .dimensions = {3, 2, 5}},
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph when the output datatype doesn't match the
    // zero_point's datatype.
    QuantizeLinearTester{
        .input = {.type = OperandDataType::kFloat32, .dimensions = {3, 2, 5}},
        .scale = {.type = OperandDataType::kFloat32, .dimensions = {5}},
        .zero_point = {.type = OperandDataType::kInt8, .dimensions = {5}},
        .output = {.type = OperandDataType::kUint8, .dimensions = {3, 2, 5}},
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph for the output shapes are not expected.
    QuantizeLinearTester{
        .input = {.type = OperandDataType::kFloat32, .dimensions = {3, 2, 5}},
        .scale = {.type = OperandDataType::kFloat32, .dimensions = {5}},
        .zero_point = {.type = OperandDataType::kInt8, .dimensions = {5}},
        .output = {.type = OperandDataType::kUint8, .dimensions = {5}},
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph when the input is as same as output.
    auto context_properties = GetContextPropertiesForTesting();
    GraphInfoBuilder builder;
    uint64_t input_operand_id =
        builder.BuildInput("input", {2, 3}, OperandDataType::kFloat32);
    uint64_t scale_operand_id =
        builder.BuildInput("scale", {2, 3}, OperandDataType::kFloat32);
    uint64_t zero_point_operand_id =
        builder.BuildInput("zero_point", {2, 3}, OperandDataType::kInt8);
    builder.BuildQuantizeLinear(input_operand_id, scale_operand_id,
                                zero_point_operand_id, input_operand_id);
    EXPECT_FALSE(WebNNGraphBuilderImpl::IsValidForTesting(
        context_properties, builder.GetGraphInfo()));
  }
  {
    // Test the invalid graph when the scale is as same as output.
    auto context_properties = GetContextPropertiesForTesting();
    GraphInfoBuilder builder;
    uint64_t input_operand_id =
        builder.BuildInput("input", {2, 3}, OperandDataType::kFloat32);
    uint64_t scale_operand_id =
        builder.BuildInput("scale", {2, 3}, OperandDataType::kFloat32);
    uint64_t zero_point_operand_id =
        builder.BuildInput("zero_point", {2, 3}, OperandDataType::kInt8);
    builder.BuildQuantizeLinear(input_operand_id, scale_operand_id,
                                zero_point_operand_id, scale_operand_id);
    EXPECT_FALSE(WebNNGraphBuilderImpl::IsValidForTesting(
        context_properties, builder.GetGraphInfo()));
  }
  {
    // Test the invalid graph when the zeroPoint is as same as output.
    auto context_properties = GetContextPropertiesForTesting();
    GraphInfoBuilder builder;
    uint64_t input_operand_id =
        builder.BuildInput("input", {2, 3}, OperandDataType::kFloat32);
    uint64_t scale_operand_id =
        builder.BuildInput("scale", {2, 3}, OperandDataType::kFloat32);
    uint64_t zero_point_operand_id =
        builder.BuildInput("zero_point", {2, 3}, OperandDataType::kInt8);
    builder.BuildQuantizeLinear(input_operand_id, scale_operand_id,
                                zero_point_operand_id, zero_point_operand_id);
    EXPECT_FALSE(WebNNGraphBuilderImpl::IsValidForTesting(
        context_properties, builder.GetGraphInfo()));
  }
}

struct ReduceTester {
  mojom::Reduce::Kind kind;
  OperandInfo input;
  std::vector<uint32_t> axes;
  bool keep_dimensions = false;
  OperandInfo output;
  bool expected;

  void Test() {
    auto context_properties = GetContextPropertiesForTesting();

    // Build the graph with mojo type.
    GraphInfoBuilder builder;
    uint64_t input_operand_id =
        builder.BuildInput("input", input.dimensions, input.type);
    uint64_t output_operand_id =
        builder.BuildOutput("output", output.dimensions, output.type);
    builder.BuildReduce(kind, input_operand_id, output_operand_id, axes,
                        keep_dimensions);

    EXPECT_EQ(WebNNGraphBuilderImpl::IsValidForTesting(context_properties,
                                                       builder.GetGraphInfo()),
              expected);
  }
};

TEST_F(WebNNGraphImplTest, ReduceTest) {
  {
    // Test reduce operator with axes = {0, 2} and keep_dimensions = true.
    ReduceTester{.kind = mojom::Reduce::Kind::kL1,
                 .input = {.type = OperandDataType::kFloat32,
                           .dimensions = {2, 3, 4, 5}},
                 .axes = {0, 2},
                 .keep_dimensions = true,
                 .output = {.type = OperandDataType::kFloat32,
                            .dimensions = {1, 3, 1, 5}},
                 .expected = true}
        .Test();
  }
  {
    // Test reduceL1 operator with input_data_type = int32.
    ReduceTester{
        .kind = mojom::Reduce::Kind::kL1,
        .input = {.type = OperandDataType::kInt32, .dimensions = {2, 3, 4, 5}},
        .axes = {0, 2},
        .keep_dimensions = true,
        .output = {.type = OperandDataType::kInt32, .dimensions = {1, 3, 1, 5}},
        .expected = true}
        .Test();
  }
  {
    // Test reduce operator with axes = {2} and keep_dimensions = false.
    ReduceTester{
        .kind = mojom::Reduce::Kind::kL2,
        .input = {.type = OperandDataType::kFloat32,
                  .dimensions = {2, 3, 4, 5}},
        .axes = {2},
        .keep_dimensions = false,
        .output = {.type = OperandDataType::kFloat32, .dimensions = {2, 3, 5}},
        .expected = true}
        .Test();
  }
  {
    ReduceTester{
        .kind = mojom::Reduce::Kind::kMin,
        .input = {.type = OperandDataType::kFloat32,
                  .dimensions = {2, 3, 4, 5}},
        .axes = {0, 1, 2, 3},
        .output = {.type = OperandDataType::kFloat32, .dimensions = {}},
        .expected = true}
        .Test();
  }
  // Test reduceMin with input_data_type = int64.
  {
    ReduceTester{
        .kind = mojom::Reduce::Kind::kMin,
        .input = {.type = OperandDataType::kInt64, .dimensions = {2, 3, 4, 5}},
        .axes = {0, 1, 2, 3},
        .output = {.type = OperandDataType::kInt64, .dimensions = {}},
        .expected = true}
        .Test();
  }
  // Test reduceSum with input_data_type = int64.
  {
    ReduceTester{
        .kind = mojom::Reduce::Kind::kSum,
        .input = {.type = OperandDataType::kInt64, .dimensions = {2, 3, 4, 5}},
        .axes = {0, 1, 2, 3},
        .output = {.type = OperandDataType::kInt64, .dimensions = {}},
        .expected = true}
        .Test();
  }
  {
    // Test reduce operator with empty axes = {}.
    ReduceTester{.kind = mojom::Reduce::Kind::kMin,
                 .input = {.type = OperandDataType::kFloat32,
                           .dimensions = {2, 3, 4, 5}},
                 .axes = {},
                 .output = {.type = OperandDataType::kFloat32,
                            .dimensions = {2, 3, 4, 5}},
                 .expected = true}
        .Test();
  }
  {
    // Test the invalid graph when the rank of axes is larger than the
    // input rank.
    ReduceTester{
        .kind = mojom::Reduce::Kind::kMax,
        .input = {.type = OperandDataType::kFloat32, .dimensions = {2, 3}},
        .axes = {0, 1, 2},
        .keep_dimensions = false,
        .output = {.type = OperandDataType::kFloat32, .dimensions = {2, 3}},
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph when the axes contains duplicate values.
    ReduceTester{
        .kind = mojom::Reduce::Kind::kMean,
        .input = {.type = OperandDataType::kFloat32, .dimensions = {2, 3}},
        .axes = {1, 1},
        .keep_dimensions = false,
        .output = {.type = OperandDataType::kFloat32, .dimensions = {2, 3}},
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph when one value in axes is greater than
    // input_rank - 1.
    ReduceTester{
        .kind = mojom::Reduce::Kind::kSum,
        .input = {.type = OperandDataType::kFloat32, .dimensions = {2, 3}},
        .axes = {2},
        .keep_dimensions = false,
        .output = {.type = OperandDataType::kFloat32, .dimensions = {2, 3}},
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph for output shapes are not expected.
    ReduceTester{
        .kind = mojom::Reduce::Kind::kProduct,
        .input = {.type = OperandDataType::kFloat32, .dimensions = {2, 3}},
        .axes = {0},
        .keep_dimensions = false,
        .output = {.type = OperandDataType::kFloat32, .dimensions = {1, 3}},
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph for output types don't match.
    ReduceTester{
        .kind = mojom::Reduce::Kind::kLogSum,
        .input = {.type = OperandDataType::kFloat32, .dimensions = {2, 3}},
        .axes = {0},
        .keep_dimensions = false,
        .output = {.type = OperandDataType::kFloat16, .dimensions = {3}},
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph for the input type is not one of float types
    // for reduceLogSum.
    ReduceTester{
        .kind = mojom::Reduce::Kind::kLogSum,
        .input = {.type = OperandDataType::kInt32, .dimensions = {2, 3}},
        .axes = {0},
        .keep_dimensions = false,
        .output = {.type = OperandDataType::kInt32, .dimensions = {3}},
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph for the input type is not one of float types
    // for reduceLogSumExp.
    ReduceTester{
        .kind = mojom::Reduce::Kind::kLogSumExp,
        .input = {.type = OperandDataType::kInt32, .dimensions = {2, 3}},
        .axes = {0},
        .keep_dimensions = false,
        .output = {.type = OperandDataType::kInt32, .dimensions = {3}},
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph for the input type is not one of float types
    // for reduceL2.
    ReduceTester{
        .kind = mojom::Reduce::Kind::kL2,
        .input = {.type = OperandDataType::kInt32, .dimensions = {2, 3}},
        .axes = {0},
        .keep_dimensions = false,
        .output = {.type = OperandDataType::kInt32, .dimensions = {3}},
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph for the input type is not one of float types
    // for reduceMean.
    ReduceTester{
        .kind = mojom::Reduce::Kind::kMean,
        .input = {.type = OperandDataType::kInt32, .dimensions = {2, 3}},
        .axes = {0},
        .keep_dimensions = false,
        .output = {.type = OperandDataType::kInt32, .dimensions = {3}},
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph for the input type is not one of {float32,
    // float16, int32, uint32, int64, uint64} types for reduceProduce.
    ReduceTester{
        .kind = mojom::Reduce::Kind::kProduct,
        .input = {.type = OperandDataType::kInt8, .dimensions = {2, 3}},
        .axes = {0},
        .keep_dimensions = false,
        .output = {.type = OperandDataType::kInt8, .dimensions = {3}},
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph for the input type is not one of {float32,
    // float16, int32, uint32, int64, uint64} types for reduceL1.
    ReduceTester{
        .kind = mojom::Reduce::Kind::kL1,
        .input = {.type = OperandDataType::kUint8, .dimensions = {2, 3}},
        .axes = {0},
        .keep_dimensions = false,
        .output = {.type = OperandDataType::kUint8, .dimensions = {3}},
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph for the input type is not one of {float32,
    // float16, int32, uint32, int64, uint64} types for reduceSum.
    ReduceTester{
        .kind = mojom::Reduce::Kind::kSum,
        .input = {.type = OperandDataType::kUint8, .dimensions = {2, 3}},
        .axes = {0},
        .keep_dimensions = false,
        .output = {.type = OperandDataType::kUint8, .dimensions = {3}},
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph for the input type is not one of {float32,
    // float16, int32, uint32, int64, uint64} types for reduceSumSquare.
    ReduceTester{
        .kind = mojom::Reduce::Kind::kSumSquare,
        .input = {.type = OperandDataType::kInt8, .dimensions = {2, 3}},
        .axes = {0},
        .keep_dimensions = false,
        .output = {.type = OperandDataType::kInt8, .dimensions = {3}},
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph for the input type and the output type are not
    // same.
    ReduceTester{
        .kind = mojom::Reduce::Kind::kLogSum,
        .input = {.type = OperandDataType::kFloat32, .dimensions = {2, 3}},
        .axes = {0},
        .keep_dimensions = false,
        .output = {.type = OperandDataType::kInt32, .dimensions = {3}},
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph when the input is as same as output.
    auto context_properties = GetContextPropertiesForTesting();
    GraphInfoBuilder builder;
    uint64_t input_operand_id =
        builder.BuildInput("input", {2, 3}, OperandDataType::kFloat32);
    builder.BuildReduce(mojom::Reduce::Kind::kSumSquare, input_operand_id,
                        input_operand_id, {0}, false);
    EXPECT_FALSE(WebNNGraphBuilderImpl::IsValidForTesting(
        context_properties, builder.GetGraphInfo()));
  }
}

struct ReluTester {
  OperandInfo input;
  OperandInfo output;
  bool expected;

  void Test() {
    auto context_properties = GetContextPropertiesForTesting();

    // Build the graph with mojo type.
    GraphInfoBuilder builder;
    uint64_t input_operand_id =
        builder.BuildInput("input", input.dimensions, input.type);
    uint64_t output_operand_id =
        builder.BuildOutput("output", output.dimensions, output.type);
    builder.BuildRelu(input_operand_id, output_operand_id);
    EXPECT_EQ(WebNNGraphBuilderImpl::IsValidForTesting(context_properties,
                                                       builder.GetGraphInfo()),
              expected);
  }
};

TEST_F(WebNNGraphImplTest, ReluTest) {
  {
    // Test relu operator for 3-D tensor with float32 input.
    ReluTester{
        .input = {.type = OperandDataType::kFloat32, .dimensions = {2, 6, 4}},
        .output = {.type = OperandDataType::kFloat32, .dimensions = {2, 6, 4}},
        .expected = true}
        .Test();
  }
  {
    // Test relu operator for 4-D tensor with int32 input.
    ReluTester{.input = {.type = OperandDataType::kFloat32,
                         .dimensions = {1, 5, 3, 7}},
               .output = {.type = OperandDataType::kFloat32,
                          .dimensions = {1, 5, 3, 7}},
               .expected = true}
        .Test();
  }
  {
    // Test the invalid graph if the data type is not supported.
    ReluTester{
        .input = {.type = OperandDataType::kUint32, .dimensions = {4, 2}},
        .output = {.type = OperandDataType::kUint32, .dimensions = {4, 2}},
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph for the output shapes are not expected.
    ReluTester{
        .input = {.type = OperandDataType::kFloat32, .dimensions = {4, 2}},
        .output = {.type = OperandDataType::kFloat32, .dimensions = {2}},
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph for output types don't match.
    ReluTester{.input = {.type = OperandDataType::kFloat32, .dimensions = {2}},
               .output = {.type = OperandDataType::kInt32, .dimensions = {2}},
               .expected = false}
        .Test();
  }
}

struct Resample2dTester {
  OperandInfo input;
  struct Resample2dAttributes {
    mojom::Resample2d::InterpolationMode mode =
        mojom::Resample2d::InterpolationMode::kNearestNeighbor;
    std::optional<std::vector<float>> scales;
    std::vector<uint32_t> axes = {2, 3};
  };
  Resample2dAttributes attributes;
  OperandInfo output;
  bool expected;

  void Test() {
    auto context_properties = GetContextPropertiesForTesting();

    // Build the graph with mojo type.
    GraphInfoBuilder builder;
    uint64_t input_operand_id =
        builder.BuildInput("input", input.dimensions, input.type);
    uint64_t output_operand_id =
        builder.BuildOutput("output", output.dimensions, output.type);
    builder.BuildResample2d(input_operand_id, output_operand_id, attributes);
    EXPECT_EQ(WebNNGraphBuilderImpl::IsValidForTesting(context_properties,
                                                       builder.GetGraphInfo()),
              expected);
  }
};

TEST_F(WebNNGraphImplTest, Resample2dTest) {
  {
    // Test resample2d with "NearestNeighbor" mode and axes = [2, 3].
    Resample2dTester{.input = {.type = OperandDataType::kFloat32,
                               .dimensions = {1, 1, 2, 4}},
                     .output = {.type = OperandDataType::kFloat32,
                                .dimensions = {1, 1, 2, 4}},
                     .expected = true}
        .Test();
  }
  {
    // Test resample2d with "Linear" mode, axes = [1, 2] and explicit scales
    // = [2, 2], input_data_type = float32.
    Resample2dTester{
        .input = {.type = OperandDataType::kFloat32,
                  .dimensions = {1, 2, 4, 1}},
        .attributes = {.mode = mojom::Resample2d::InterpolationMode::kLinear,
                       .scales = std::vector<float>{2, 2},
                       .axes = {1, 2}},
        .output = {.type = OperandDataType::kFloat32,
                   .dimensions = {1, 4, 8, 1}},
        .expected = true}
        .Test();
  }
  {
    // Test resample2d with "Linear" mode, axes = [1, 2] and explicit scales
    // = [2, 2], input_data_type = float16.
    Resample2dTester{
        .input = {.type = OperandDataType::kFloat16,
                  .dimensions = {1, 2, 4, 1}},
        .attributes = {.mode = mojom::Resample2d::InterpolationMode::kLinear,
                       .scales = std::vector<float>{2, 2},
                       .axes = {1, 2}},
        .output = {.type = OperandDataType::kFloat16,
                   .dimensions = {1, 4, 8, 1}},
        .expected = true}
        .Test();
  }
  {
    // Test resample2d with "Linear" mode, axes = [1, 2] and explicit scales
    // = [2, 2.2] which is not exactly output dimensions / input dimensions.
    Resample2dTester{
        .input = {.type = OperandDataType::kFloat32,
                  .dimensions = {1, 2, 4, 1}},
        .attributes = {.mode = mojom::Resample2d::InterpolationMode::kLinear,
                       .scales = std::vector<float>{2, 2.2},
                       .axes = {1, 2}},
        .output = {.type = OperandDataType::kFloat32,
                   .dimensions = {1, 4, 8, 1}},
        .expected = true}
        .Test();
  }
  {
    // Test the invalid graph for output types don't match.
    Resample2dTester{.input = {.type = OperandDataType::kFloat32,
                               .dimensions = {1, 1, 2, 4}},
                     .output = {.type = OperandDataType::kFloat16,
                                .dimensions = {1, 1, 4, 8}},
                     .expected = false}
        .Test();
  }
  {
    // Test the invalid graph if the input is not floating point.
    Resample2dTester{
        .input = {.type = OperandDataType::kInt32, .dimensions = {1, 1, 2, 4}},
        .output = {.type = OperandDataType::kInt32, .dimensions = {1, 1, 4, 8}},
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph for input is not a 4-D tensor.
    Resample2dTester{
        .input = {.type = OperandDataType::kFloat32, .dimensions = {1, 1, 2}},
        .output = {.type = OperandDataType::kFloat32,
                   .dimensions = {1, 1, 2, 4}},
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph for output is not a 4-D tensor.
    Resample2dTester{
        .input = {.type = OperandDataType::kFloat32,
                  .dimensions = {1, 1, 2, 4}},
        .output = {.type = OperandDataType::kFloat32, .dimensions = {1, 1, 2}},
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph for output dimensions that don't match the
    // calculated dimensions by scales.
    Resample2dTester{
        .input = {.type = OperandDataType::kFloat32,
                  .dimensions = {1, 2, 4, 1}},
        .attributes = {.mode = mojom::Resample2d::InterpolationMode::kLinear,
                       .scales = std::vector<float>{2, 2},
                       .axes = {1, 2}},
        .output = {.type = OperandDataType::kFloat32,
                   .dimensions = {1, 5, 8, 1}},
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph when the scale height is too large.
    Resample2dTester{
        .input = {.type = OperandDataType::kFloat32,
                  .dimensions = {1, 1, 34902, 23243}},
        .attributes = {.mode = mojom::Resample2d::InterpolationMode::kLinear,
                       .scales = std::vector<float>{232433, 4}},
        .output = {.type = OperandDataType::kFloat32,
                   .dimensions = {1, 1, 2, 4}},
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph when the scale height is too small.
    Resample2dTester{
        .input = {.type = OperandDataType::kFloat32,
                  .dimensions = {1, 1, 2, 4}},
        .attributes = {.mode = mojom::Resample2d::InterpolationMode::kLinear,
                       .scales = std::vector<float>{0.02, 0.8}},
        .output = {.type = OperandDataType::kFloat32,
                   .dimensions = {1, 1, 2, 4}},
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph when the scale width is too large.
    Resample2dTester{
        .input = {.type = OperandDataType::kFloat32,
                  .dimensions = {1, 1, 34902, 23243}},
        .attributes = {.mode = mojom::Resample2d::InterpolationMode::kLinear,
                       .scales = std::vector<float>{20, 434324}},
        .output = {.type = OperandDataType::kFloat32,
                   .dimensions = {1, 1, 2, 4}},
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph when the scale width is too small.
    Resample2dTester{
        .input = {.type = OperandDataType::kFloat32,
                  .dimensions = {1, 1, 2, 4}},
        .attributes = {.mode = mojom::Resample2d::InterpolationMode::kLinear,
                       .scales = std::vector<float>{0.7, 0.1}},
        .output = {.type = OperandDataType::kFloat32,
                   .dimensions = {1, 1, 2, 4}},
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph when the scales are negative.
    Resample2dTester{.input = {.type = OperandDataType::kFloat32,
                               .dimensions = {1, 1, 2, 4}},
                     .attributes{.scales = std::vector<float>{1.0, -2.0}},
                     .output = {.type = OperandDataType::kFloat32,
                                .dimensions = {1, 2, 4, 4}},
                     .expected = false}
        .Test();
  }
  // Test when the dimensions of the input tensor to which
  // the interpolation algorithm applies are not two consecutive dimensions.
  {
    // With axes = [1, 3].
    Resample2dTester{.input = {.type = OperandDataType::kFloat32,
                               .dimensions = {1, 1, 2, 4}},
                     .attributes = {.axes = {1, 3}},
                     .output = {.type = OperandDataType::kFloat32,
                                .dimensions = {1, 2, 2, 8}},
                     .expected = true}
        .Test();
  }
  {
    // With axes = [1, 2, 3]
    Resample2dTester{.input = {.type = OperandDataType::kFloat32,
                               .dimensions = {1, 1, 2, 4}},
                     .attributes = {.axes = {1, 2, 3}},
                     .output = {.type = OperandDataType::kFloat32,
                                .dimensions = {1, 2, 4, 8}},
                     .expected = false}
        .Test();
  }
  // Test the invalid graph when the dimension of output doesn't equal to
  // the dimension of input except along the axes.
  {
    // With explicit scales.
    Resample2dTester{
        .input = {.type = OperandDataType::kFloat32,
                  .dimensions = {1, 1, 2, 4}},
        .attributes = {.scales = std::vector<float>{2, 2}, .axes = {2, 3}},
        .output = {.type = OperandDataType::kFloat32,
                   .dimensions = {1, 2, 4, 8}},
        .expected = false}
        .Test();
  }
  {
    // Without explicit scales.
    Resample2dTester{.input = {.type = OperandDataType::kFloat32,
                               .dimensions = {1, 1, 2, 4}},
                     .attributes = {.axes = {2, 3}},
                     .output = {.type = OperandDataType::kFloat32,
                                .dimensions = {1, 2, 4, 8}},
                     .expected = false}
        .Test();
  }
  {
    // Test the invalid graph when the input is as same as output.
    auto context_properties = GetContextPropertiesForTesting();
    GraphInfoBuilder builder;
    uint64_t input_operand_id =
        builder.BuildInput("input", {1, 1, 2, 4}, OperandDataType::kFloat32);
    builder.BuildResample2d(input_operand_id, input_operand_id,
                            Resample2dTester::Resample2dAttributes{});

    EXPECT_FALSE(WebNNGraphBuilderImpl::IsValidForTesting(
        context_properties, builder.GetGraphInfo()));
  }
}

struct ReshapeTester {
  OperandInfo input;
  OperandInfo output;
  bool expected;

  void Test() {
    auto context_properties = GetContextPropertiesForTesting();

    // Build the graph with mojo type.
    GraphInfoBuilder builder;
    uint64_t input_operand_id =
        builder.BuildInput("input", input.dimensions, input.type);
    uint64_t output_operand_id =
        builder.BuildOutput("output", output.dimensions, output.type);
    builder.BuildReshape(input_operand_id, output_operand_id);
    EXPECT_EQ(WebNNGraphBuilderImpl::IsValidForTesting(context_properties,
                                                       builder.GetGraphInfo()),
              expected);
  }
};

TEST_F(WebNNGraphImplTest, ReshapeTest) {
  {
    // Test reshape operator from 2-D tensor to 1-D tensor.
    ReshapeTester{
        .input = {.type = OperandDataType::kFloat32, .dimensions = {2, 4}},
        .output = {.type = OperandDataType::kFloat32, .dimensions = {8}},
        .expected = true}
        .Test();
  }
  {
    // Test reshape operator from 4-D tensor to 2-D tensor.
    ReshapeTester{
        .input = {.type = OperandDataType::kInt32, .dimensions = {1, 3, 2, 1}},
        .output = {.type = OperandDataType::kInt32, .dimensions = {1, 6}},
        .expected = true}
        .Test();
  }
  {
    // Test the invalid graph when the number of input elements are not
    // equal to the number of output elements.
    ReshapeTester{
        .input = {.type = OperandDataType::kFloat32, .dimensions = {2, 3, 4}},
        .output = {.type = OperandDataType::kInt32, .dimensions = {3, 5}},
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph for output types don't match.
    ReshapeTester{
        .input = {.type = OperandDataType::kFloat32, .dimensions = {2}},
        .output = {.type = OperandDataType::kInt32, .dimensions = {2}},
        .expected = false}
        .Test();
  }
}

struct ScatterNDTester {
  OperandInfo input;
  OperandInfo indices;
  OperandInfo updates;
  OperandInfo output;
  bool expected;

  void Test() {
    auto context_properties = GetContextPropertiesForTesting();

    // Build the graph with mojo type.
    GraphInfoBuilder builder;
    uint64_t input_operand_id =
        builder.BuildInput("input", input.dimensions, input.type);
    uint64_t indices_operand_id =
        builder.BuildInput("indices", indices.dimensions, indices.type);
    uint64_t updates_operand_id =
        builder.BuildInput("updates", updates.dimensions, updates.type);
    uint64_t output_operand_id =
        builder.BuildOutput("output", output.dimensions, output.type);
    builder.BuildScatterND(input_operand_id, indices_operand_id,
                           updates_operand_id, output_operand_id);
    EXPECT_EQ(WebNNGraphBuilderImpl::IsValidForTesting(context_properties,
                                                       builder.GetGraphInfo()),
              expected);
  }
};

TEST_F(WebNNGraphImplTest, ScatterNDTest) {
  {
    // Test a valid scatterND with 3-D input, 2-D indices and 3-D updates.
    ScatterNDTester{
        .input = {.type = OperandDataType::kFloat32, .dimensions = {4, 4, 4}},
        .indices = {.type = OperandDataType::kUint32, .dimensions = {2, 1}},
        .updates = {.type = OperandDataType::kFloat32, .dimensions = {2, 4, 4}},
        .output = {.type = OperandDataType::kFloat32, .dimensions = {4, 4, 4}},
        .expected = true}
        .Test();
  }
  {
    // Test an invalid scatterND that the updates tensor data type is not the
    // same as input data type.
    ScatterNDTester{
        .input = {.type = OperandDataType::kFloat32, .dimensions = {4, 4, 4}},
        .indices = {.type = OperandDataType::kUint32, .dimensions = {2, 1}},
        .updates = {.type = OperandDataType::kFloat16, .dimensions = {2, 4, 4}},
        .output = {.type = OperandDataType::kFloat32, .dimensions = {4, 4, 4}},
        .expected = false}
        .Test();
  }
  {
    // Test an invalid scatterND with scalar input tensor.
    ScatterNDTester{
        .input = {.type = OperandDataType::kFloat32, .dimensions = {}},
        .indices = {.type = OperandDataType::kUint32, .dimensions = {2, 1}},
        .updates = {.type = OperandDataType::kFloat32, .dimensions = {2, 4, 4}},
        .output = {.type = OperandDataType::kFloat32, .dimensions = {4, 4, 4}},
        .expected = false}
        .Test();
  }
  {
    // Test an invalid scatterND with scalar indices tensor.
    ScatterNDTester{
        .input = {.type = OperandDataType::kFloat32, .dimensions = {4, 4, 4}},
        .indices = {.type = OperandDataType::kUint32, .dimensions = {}},
        .updates = {.type = OperandDataType::kFloat32, .dimensions = {2, 4, 4}},
        .output = {.type = OperandDataType::kFloat32, .dimensions = {4, 4, 4}},
        .expected = false}
        .Test();
  }
  {
    // Test an invalid scatterND that the size of last dimension of indices
    // tensor is greater than input rank.
    ScatterNDTester{
        .input = {.type = OperandDataType::kFloat32, .dimensions = {4, 4, 4}},
        .indices = {.type = OperandDataType::kUint32, .dimensions = {2, 4}},
        .updates = {.type = OperandDataType::kFloat32, .dimensions = {2, 4, 4}},
        .output = {.type = OperandDataType::kFloat32, .dimensions = {4, 4, 4}},
        .expected = false}
        .Test();
  }
  {
    // Test an invalid scatterND whose updates tensor shape is invalid.
    ScatterNDTester{
        .input = {.type = OperandDataType::kFloat32, .dimensions = {4, 4, 4}},
        .indices = {.type = OperandDataType::kUint32, .dimensions = {2, 1}},
        // Updates tensor shape should be [2, 4, 4].
        .updates = {.type = OperandDataType::kFloat32, .dimensions = {2, 3, 4}},
        .output = {.type = OperandDataType::kFloat32, .dimensions = {4, 4, 4}},
        .expected = false}
        .Test();
  }
  {
    // Test an invalid scatterND whose output shape is not the same as input.
    ScatterNDTester{
        .input = {.type = OperandDataType::kFloat32, .dimensions = {4, 4, 4}},
        .indices = {.type = OperandDataType::kUint32, .dimensions = {2, 1}},
        .updates = {.type = OperandDataType::kFloat32, .dimensions = {2, 4, 4}},
        .output = {.type = OperandDataType::kFloat32, .dimensions = {2, 4, 4}},
        .expected = false}
        .Test();
  }
  {
    // Test an invalid scatterND whose output data type is not the same as
    // input.
    ScatterNDTester{
        .input = {.type = OperandDataType::kFloat32, .dimensions = {4, 4, 4}},
        .indices = {.type = OperandDataType::kUint32, .dimensions = {2, 1}},
        .updates = {.type = OperandDataType::kFloat32, .dimensions = {2, 4, 4}},
        .output = {.type = OperandDataType::kFloat16, .dimensions = {4, 4, 4}},
        .expected = false}
        .Test();
  }
  {
    // Test an invalid scatterND where the output is the same as the input.
    auto context_properties = GetContextPropertiesForTesting();
    GraphInfoBuilder builder;
    uint64_t input_operand_id =
        builder.BuildInput("input", {4, 4, 4}, OperandDataType::kFloat32);
    uint64_t indices_operand_id =
        builder.BuildInput("indices", {2, 1}, OperandDataType::kUint32);
    uint64_t updates_operand_id =
        builder.BuildInput("updates", {2, 4, 4}, OperandDataType::kFloat32);
    builder.BuildScatterND(input_operand_id, indices_operand_id,
                           updates_operand_id, input_operand_id);
    EXPECT_FALSE(WebNNGraphBuilderImpl::IsValidForTesting(
        context_properties, builder.GetGraphInfo()));
  }
}

struct SliceTester {
  struct SliceAttributes {
    std::vector<uint32_t> starts;
    std::vector<uint32_t> sizes;
  };

  OperandInfo input;
  SliceAttributes attributes;
  OperandInfo output;

  bool expected;

  void Test() {
    auto context_properties = GetContextPropertiesForTesting();

    // Build the graph with mojo type.
    GraphInfoBuilder builder;
    uint64_t input_operand_id =
        builder.BuildInput("input", input.dimensions, input.type);
    uint64_t output_operand_id =
        builder.BuildOutput("output", output.dimensions, output.type);
    builder.BuildSlice(input_operand_id, output_operand_id,
                       std::move(attributes.starts),
                       std::move(attributes.sizes));
    EXPECT_EQ(WebNNGraphBuilderImpl::IsValidForTesting(context_properties,
                                                       builder.GetGraphInfo()),
              expected);
  }
};

TEST_F(WebNNGraphImplTest, SliceTest) {
  {
    // Test slice with output dimensions equal to input dimensions.
    SliceTester{
        .input = {.type = OperandDataType::kFloat32, .dimensions = {4, 4}},
        .attributes = {.starts = {0, 0}, .sizes = {4, 4}},
        .output = {.type = OperandDataType::kFloat32, .dimensions = {4, 4}},
        .expected = true}
        .Test();
  }
  {
    // Test 4x4 2-D Tensor to 2x2 slice
    SliceTester{
        .input = {.type = OperandDataType::kFloat32, .dimensions = {4, 4}},
        .attributes = {.starts = {0, 0}, .sizes = {2, 2}},
        .output = {.type = OperandDataType::kFloat32, .dimensions = {2, 2}},
        .expected = true}
        .Test();
  }
  {
    // Test 4x4 2-D Tensor to 2x2 slice with offsets
    SliceTester{
        .input = {.type = OperandDataType::kFloat32, .dimensions = {4, 4}},
        .attributes = {.starts = {2, 2}, .sizes = {2, 2}},
        .output = {.type = OperandDataType::kFloat32, .dimensions = {2, 2}},
        .expected = true}
        .Test();
  }
  {
    // Test that going out-of-bounds of the input tensor fails.
    SliceTester{
        .input = {.type = OperandDataType::kFloat32, .dimensions = {2, 2}},
        .attributes = {.starts = {1, 0}, .sizes = {2, 2}},
        .output = {.type = OperandDataType::kFloat32, .dimensions = {2, 2}},
        .expected = false}
        .Test();
  }
  {
    // Test that mismatched output dimensions and size attribute will fail.
    SliceTester{
        .input = {.type = OperandDataType::kFloat32, .dimensions = {2, 2}},
        .attributes = {.starts = {0, 0}, .sizes = {1, 1}},
        .output = {.type = OperandDataType::kFloat32, .dimensions = {2, 1}},
        .expected = false}
        .Test();
  }
  {
    // Test that using size zero will result in failure.
    SliceTester{
        .input = {.type = OperandDataType::kFloat32, .dimensions = {2, 2}},
        .attributes = {.starts = {0, 0}, .sizes = {0, 1}},
        .output = {.type = OperandDataType::kFloat32, .dimensions = {1}},
        .expected = false}
        .Test();
  }
  {
    // Test that having starts and sizes lengths not equal to the input rank
    // will fail.
    SliceTester{
        .input = {.type = OperandDataType::kFloat32, .dimensions = {4, 4}},
        .attributes = {.starts = {0}, .sizes = {4}},
        .output = {.type = OperandDataType::kFloat32, .dimensions = {4, 4}},
        .expected = false}
        .Test();
  }
  {
    // Test that input data type not equal to the output data type will
    // fail.
    SliceTester{
        .input = {.type = OperandDataType::kFloat16, .dimensions = {4, 4}},
        .attributes = {.starts = {0, 0}, .sizes = {4, 4}},
        .output = {.type = OperandDataType::kFloat32, .dimensions = {4, 4}},
        .expected = false}
        .Test();
  }
}

enum class FloatingPointUnaryKind {
  kHardSwish,
  kLeakyRelu,
  kLinear,
  kSigmoid,
  kTanh
};

struct FloatingPointUnaryTester {
  OperandInfo input;
  OperandInfo output;
  bool expected;

  void Test() {
    Test(FloatingPointUnaryKind::kHardSwish);
    Test(FloatingPointUnaryKind::kLeakyRelu);
    Test(FloatingPointUnaryKind::kLinear);
    Test(FloatingPointUnaryKind::kSigmoid);
    Test(FloatingPointUnaryKind::kTanh);
  }

  void Test(FloatingPointUnaryKind kind) {
    auto context_properties = GetContextPropertiesForTesting();

    // Build the graph with mojo type.
    GraphInfoBuilder builder;
    uint64_t input_operand_id =
        builder.BuildInput("input", input.dimensions, input.type);
    uint64_t output_operand_id =
        builder.BuildOutput("output", output.dimensions, output.type);
    switch (kind) {
      case FloatingPointUnaryKind::kHardSwish:
        builder.BuildHardSwish(input_operand_id, output_operand_id);
        break;
      case FloatingPointUnaryKind::kLeakyRelu:
        builder.BuildLeakyRelu(input_operand_id, output_operand_id,
                               /*alpha*/ 1.0);
        break;
      case FloatingPointUnaryKind::kLinear:
        builder.BuildLinear(input_operand_id, output_operand_id,
                            /*alpha*/ 1.0, /*beta*/ 0.0);
        break;
      case FloatingPointUnaryKind::kSigmoid:
        builder.BuildSigmoid(input_operand_id, output_operand_id);
        break;
      case FloatingPointUnaryKind::kTanh:
        builder.BuildTanh(input_operand_id, output_operand_id);
        break;
    }
    EXPECT_EQ(WebNNGraphBuilderImpl::IsValidForTesting(context_properties,
                                                       builder.GetGraphInfo()),
              expected);
  }
};

TEST_F(WebNNGraphImplTest, FloatingPointUnaryTest) {
  {
    // Test the operator for 2-D tensor with float32 input.
    FloatingPointUnaryTester{
        .input = {.type = OperandDataType::kFloat32, .dimensions = {2, 6}},
        .output = {.type = OperandDataType::kFloat32, .dimensions = {2, 6}},
        .expected = true}
        .Test();
  }
  {
    // Test the invalid graph for the output shapes are not as expected.
    FloatingPointUnaryTester{
        .input = {.type = OperandDataType::kFloat32, .dimensions = {4, 2}},
        .output = {.type = OperandDataType::kFloat32, .dimensions = {2}},
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph for output data types which don't match.
    FloatingPointUnaryTester{
        .input = {.type = OperandDataType::kFloat32, .dimensions = {2}},
        .output = {.type = OperandDataType::kInt32, .dimensions = {2}},
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph when the input data type is not floating
    // point.
    FloatingPointUnaryTester{
        .input = {.type = OperandDataType::kInt32, .dimensions = {2}},
        .output = {.type = OperandDataType::kInt32, .dimensions = {2}},
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph for leaky relu when the input is as same as
    // output.
    auto context_properties = GetContextPropertiesForTesting();
    GraphInfoBuilder builder;
    uint64_t input_operand_id =
        builder.BuildInput("input", {2}, OperandDataType::kFloat32);
    builder.BuildLeakyRelu(input_operand_id, input_operand_id,
                           /*alpha*/ 1.0);

    EXPECT_FALSE(WebNNGraphBuilderImpl::IsValidForTesting(
        context_properties, builder.GetGraphInfo()));
  }
  {
    // Test the invalid graph for leaky relu when alpha is NAN.
    auto context_properties = GetContextPropertiesForTesting();
    GraphInfoBuilder builder;
    uint64_t input_operand_id =
        builder.BuildInput("input", {2}, OperandDataType::kFloat32);
    uint64_t output_operand_id =
        builder.BuildOutput("output", {2}, OperandDataType::kFloat32);
    builder.BuildLeakyRelu(input_operand_id, output_operand_id,
                           /*alpha*/ NAN);

    EXPECT_FALSE(WebNNGraphBuilderImpl::IsValidForTesting(
        context_properties, builder.GetGraphInfo()));
  }
  {
    // Test the invalid graph for linear when the input is as same as output.
    auto context_properties = GetContextPropertiesForTesting();
    GraphInfoBuilder builder;
    uint64_t input_operand_id =
        builder.BuildInput("input", {2}, OperandDataType::kFloat32);
    builder.BuildLinear(input_operand_id, input_operand_id,
                        /*alpha*/ 1.0, /*beta*/ 0.0);

    EXPECT_FALSE(WebNNGraphBuilderImpl::IsValidForTesting(
        context_properties, builder.GetGraphInfo()));
  }
  {
    // Test the invalid graph for linear when alpha is NAN.
    auto context_properties = GetContextPropertiesForTesting();
    GraphInfoBuilder builder;
    uint64_t input_operand_id =
        builder.BuildInput("input", {2}, OperandDataType::kFloat32);
    uint64_t output_operand_id =
        builder.BuildOutput("output", {2}, OperandDataType::kFloat32);
    builder.BuildLinear(input_operand_id, output_operand_id,
                        /*alpha*/ NAN, /*beta*/ 0.0);

    EXPECT_FALSE(WebNNGraphBuilderImpl::IsValidForTesting(
        context_properties, builder.GetGraphInfo()));
  }
  {
    // Test the invalid graph for linear when beta is NAN.
    auto context_properties = GetContextPropertiesForTesting();
    GraphInfoBuilder builder;
    uint64_t input_operand_id =
        builder.BuildInput("input", {2}, OperandDataType::kFloat32);
    uint64_t output_operand_id =
        builder.BuildOutput("output", {2}, OperandDataType::kFloat32);
    builder.BuildLinear(input_operand_id, output_operand_id,
                        /*alpha*/ 1.0, /*beta*/ NAN);

    EXPECT_FALSE(WebNNGraphBuilderImpl::IsValidForTesting(
        context_properties, builder.GetGraphInfo()));
  }
  {
    // Test the invalid graph for sigmoid when the input is as same as
    // output.
    auto context_properties = GetContextPropertiesForTesting();
    GraphInfoBuilder builder;
    uint64_t input_operand_id =
        builder.BuildInput("input", {2}, OperandDataType::kFloat32);
    builder.BuildSigmoid(input_operand_id, input_operand_id);

    EXPECT_FALSE(WebNNGraphBuilderImpl::IsValidForTesting(
        context_properties, builder.GetGraphInfo()));
  }
  {
    // Test the invalid graph for tanh when the input is as same as output.
    auto context_properties = GetContextPropertiesForTesting();
    GraphInfoBuilder builder;
    uint64_t input_operand_id =
        builder.BuildInput("input", {2}, OperandDataType::kFloat32);
    builder.BuildTanh(input_operand_id, input_operand_id);

    EXPECT_FALSE(WebNNGraphBuilderImpl::IsValidForTesting(
        context_properties, builder.GetGraphInfo()));
  }
}

struct SoftmaxTester {
  OperandInfo input;
  OperandInfo output;
  uint32_t axis;
  bool expected;

  void Test() {
    auto context_properties = GetContextPropertiesForTesting();

    // Build the graph with mojo type.
    GraphInfoBuilder builder;
    uint64_t input_operand_id =
        builder.BuildInput("input", input.dimensions, input.type);
    uint64_t output_operand_id =
        builder.BuildOutput("output", output.dimensions, output.type);
    builder.BuildSoftmax(input_operand_id, output_operand_id, axis);
    EXPECT_EQ(WebNNGraphBuilderImpl::IsValidForTesting(context_properties,
                                                       builder.GetGraphInfo()),
              expected);
  }
};

TEST_F(WebNNGraphImplTest, SoftmaxTest) {
  {
    // Test softmax operator for input operand with [2, 2] dimensions.
    SoftmaxTester{
        .input = {.type = OperandDataType::kFloat32, .dimensions = {2, 2}},
        .output = {.type = OperandDataType::kFloat32, .dimensions = {2, 2}},
        .axis = 1,
        .expected = true}
        .Test();
  }
  {
    // Test softmax operator for input operand with [1, 1, 4, 2] dimensions.
    SoftmaxTester{.input = {.type = OperandDataType::kFloat32,
                            .dimensions = {1, 1, 4, 2}},
                  .output = {.type = OperandDataType::kFloat32,
                             .dimensions = {1, 1, 4, 2}},
                  .axis = 3,
                  .expected = true}
        .Test();
  }
  {
    // Test the invalid graph when building softmax with int32 input.
    SoftmaxTester{
        .input = {.type = OperandDataType::kInt32, .dimensions = {2, 3}},
        .output = {.type = OperandDataType::kInt32, .dimensions = {2, 3}},
        .axis = 1,
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph for axis is not less than the input rank.
    SoftmaxTester{
        .input = {.type = OperandDataType::kFloat32, .dimensions = {2, 5}},
        .output = {.type = OperandDataType::kFloat32, .dimensions = {2, 5}},
        .axis = 2,
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph for the output shapes are not expected.
    SoftmaxTester{
        .input = {.type = OperandDataType::kFloat32, .dimensions = {4, 2}},
        .output = {.type = OperandDataType::kFloat32, .dimensions = {2}},
        .axis = 1,
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph for output types don't match.
    SoftmaxTester{
        .input = {.type = OperandDataType::kFloat32, .dimensions = {2, 5}},
        .output = {.type = OperandDataType::kFloat16, .dimensions = {2, 5}},
        .axis = 1,
        .expected = false}
        .Test();
  }
}

struct SoftplusTester {
  OperandInfo input;
  OperandInfo output;
  bool expected;

  void Test() {
    auto context_properties = GetContextPropertiesForTesting();

    // Build the graph with mojo type.
    GraphInfoBuilder builder;
    uint64_t input_operand_id =
        builder.BuildInput("input", input.dimensions, input.type);
    uint64_t output_operand_id =
        builder.BuildOutput("output", output.dimensions, output.type);
    builder.BuildSoftplus(input_operand_id, output_operand_id);
    EXPECT_EQ(WebNNGraphBuilderImpl::IsValidForTesting(context_properties,
                                                       builder.GetGraphInfo()),
              expected);
  }
};

TEST_F(WebNNGraphImplTest, SoftplusTest) {
  {
    // Test softplus operator.
    SoftplusTester{
        .input = {.type = OperandDataType::kFloat32, .dimensions = {2, 2}},
        .output = {.type = OperandDataType::kFloat32, .dimensions = {2, 2}},
        .expected = true}
        .Test();
  }
  {
    // Test the invalid graph for invalid data type.
    SoftplusTester{
        .input = {.type = OperandDataType::kInt32, .dimensions = {4, 2}},
        .output = {.type = OperandDataType::kInt32, .dimensions = {4, 2}},
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph for the output shapes are not expected.
    SoftplusTester{
        .input = {.type = OperandDataType::kFloat32, .dimensions = {4, 2}},
        .output = {.type = OperandDataType::kFloat32, .dimensions = {2}},
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph for output types don't match.
    SoftplusTester{
        .input = {.type = OperandDataType::kFloat32, .dimensions = {2, 5}},
        .output = {.type = OperandDataType::kFloat16, .dimensions = {2, 5}},
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph for input operand == output operand.
    auto context_properties = GetContextPropertiesForTesting();
    GraphInfoBuilder builder;
    uint64_t input_operand_id =
        builder.BuildInput("input", {4, 6}, OperandDataType::kFloat32);
    builder.BuildSoftplus(input_operand_id, input_operand_id);
    EXPECT_FALSE(WebNNGraphBuilderImpl::IsValidForTesting(
        context_properties, builder.GetGraphInfo()));
  }
}

struct SoftsignTester {
  OperandInfo input;
  OperandInfo output;
  bool expected;

  void Test() {
    auto context_properties = GetContextPropertiesForTesting();

    // Build the graph with mojo type.
    GraphInfoBuilder builder;
    uint64_t input_operand_id =
        builder.BuildInput("input", input.dimensions, input.type);
    uint64_t output_operand_id =
        builder.BuildOutput("output", output.dimensions, output.type);
    builder.BuildSoftsign(input_operand_id, output_operand_id);
    EXPECT_EQ(WebNNGraphBuilderImpl::IsValidForTesting(context_properties,
                                                       builder.GetGraphInfo()),
              expected);
  }
};

TEST_F(WebNNGraphImplTest, SoftsignTest) {
  {
    // Test softsign operator with input dimensions = [2, 4] and data type
    // float32.
    SoftsignTester{
        .input = {.type = OperandDataType::kFloat32, .dimensions = {2, 4}},
        .output = {.type = OperandDataType::kFloat32, .dimensions = {2, 4}},
        .expected = true}
        .Test();
  }
  {
    // Test the invalid graph for invalid data type.
    SoftsignTester{
        .input = {.type = OperandDataType::kInt32, .dimensions = {4, 2}},
        .output = {.type = OperandDataType::kInt32, .dimensions = {4, 2}},
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph for the output shapes are not expected.
    SoftsignTester{
        .input = {.type = OperandDataType::kFloat32, .dimensions = {4, 2}},
        .output = {.type = OperandDataType::kFloat32, .dimensions = {2}},
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph for output types don't match.
    SoftsignTester{
        .input = {.type = OperandDataType::kFloat32, .dimensions = {2, 5}},
        .output = {.type = OperandDataType::kFloat16, .dimensions = {2, 5}},
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph for input operand == output operand.
    auto context_properties = GetContextPropertiesForTesting();
    GraphInfoBuilder builder;
    uint64_t input_operand_id =
        builder.BuildInput("input", {4, 6}, OperandDataType::kFloat32);
    builder.BuildSoftsign(input_operand_id, input_operand_id);
    EXPECT_FALSE(WebNNGraphBuilderImpl::IsValidForTesting(
        context_properties, builder.GetGraphInfo()));
  }
}

struct SplitTester {
  OperandInfo input;
  std::vector<OperandInfo> outputs;
  uint32_t axis = 0;
  bool expected;

  void Test() {
    auto context_properties = GetContextPropertiesForTesting();

    // Build the graph with mojo type.
    GraphInfoBuilder builder;
    uint64_t input_operand_id =
        builder.BuildInput("input", input.dimensions, input.type);

    std::vector<uint64_t> output_operand_ids;
    for (size_t i = 0; i < outputs.size(); ++i) {
      output_operand_ids.push_back(
          builder.BuildOutput("output" + base::NumberToString(i),
                              outputs[i].dimensions, outputs[i].type));
    }
    builder.BuildSplit(input_operand_id, output_operand_ids, axis);
    EXPECT_EQ(WebNNGraphBuilderImpl::IsValidForTesting(context_properties,
                                                       builder.GetGraphInfo()),
              expected);
  }
};

TEST_F(WebNNGraphImplTest, ValidateSplitTest) {
  using OperandDataType::kFloat32;
  {
    // Tests default axis split.
    SplitTester{.input = {.type = kFloat32, .dimensions = {2, 2}},
                .outputs = {{.type = kFloat32, .dimensions = {1, 2}},
                            {.type = kFloat32, .dimensions = {1, 2}}},
                .expected = true}
        .Test();
  }
  {
    // Tests axis=1 split.
    SplitTester{.input = {.type = kFloat32, .dimensions = {2, 2}},
                .outputs = {{.type = kFloat32, .dimensions = {2, 1}},
                            {.type = kFloat32, .dimensions = {2, 1}}},
                .axis = 1,
                .expected = true}
        .Test();
  }
  {
    // Tests for an invalid graph where not all output types match the input
    // type.
    SplitTester{
        .input = {.type = kFloat32, .dimensions = {2, 2}},
        .outputs = {{.type = kFloat32, .dimensions = {1, 2}},
                    {.type = OperandDataType::kFloat16, .dimensions = {1, 2}}},
        .expected = false}
        .Test();
  }
  {
    // Tests for an invalid graph where the sum of the splits is less than
    // the input tensor size.
    SplitTester{.input = {.type = kFloat32, .dimensions = {2, 6}},
                .outputs = {{.type = kFloat32, .dimensions = {2, 1}},
                            {.type = kFloat32, .dimensions = {2, 2}},
                            {.type = kFloat32, .dimensions = {2, 2}}},
                .axis = 1,
                .expected = false}
        .Test();
  }
  {
    // Tests for an invalid graph where the sum of the splits is greater
    // than the input tensor size.
    SplitTester{.input = {.type = kFloat32, .dimensions = {2, 6}},
                .outputs = {{.type = kFloat32, .dimensions = {2, 1}},
                            {.type = kFloat32, .dimensions = {2, 2}},
                            {.type = kFloat32, .dimensions = {2, 4}}},
                .axis = 1,
                .expected = false}
        .Test();
  }
  {
    // Tests for an invalid graph where specified axis is greater then the
    // rank of the input tensor
    SplitTester{.input = {.type = kFloat32, .dimensions = {2, 2}},
                .outputs = {{.type = kFloat32, .dimensions = {1, 2}},
                            {.type = kFloat32, .dimensions = {1, 2}}},
                .axis = 2,
                .expected = false}
        .Test();
  }
  {
    // Tests for an invalid graph where a split as specified along multiple
    // axis.
    SplitTester{.input = {.type = kFloat32, .dimensions = {4, 6}},
                .outputs = {{.type = kFloat32, .dimensions = {1, 2}},
                            {.type = kFloat32, .dimensions = {2, 3}},
                            {.type = kFloat32, .dimensions = {1, 1}}},
                .expected = false}
        .Test();
  }
  {
    auto context_properties = GetContextPropertiesForTesting();
    GraphInfoBuilder builder;
    uint64_t input_operand_id = builder.BuildInput("input", {4, 6}, kFloat32);

    builder.BuildSplit(input_operand_id, {input_operand_id}, 0);
    builder.BuildSplit(input_operand_id,
                       {builder.BuildOutput("output", {4, 6}, kFloat32)}, 0);
    EXPECT_FALSE(WebNNGraphBuilderImpl::IsValidForTesting(
        context_properties, builder.GetGraphInfo()));
  }
}

struct TileTester {
  OperandInfo input;
  std::vector<uint32_t> repetitions;
  OperandInfo output;
  bool expected;

  void Test() {
    auto context_properties = GetContextPropertiesForTesting();

    // Build the graph with mojo type.
    GraphInfoBuilder builder;
    uint64_t input_operand_id =
        builder.BuildInput("input", input.dimensions, input.type);
    uint64_t output_operand_id =
        builder.BuildOutput("output", output.dimensions, output.type);
    builder.BuildTile(input_operand_id, output_operand_id,
                      std::move(repetitions));
    EXPECT_EQ(WebNNGraphBuilderImpl::IsValidForTesting(context_properties,
                                                       builder.GetGraphInfo()),
              expected);
  }
};

TEST_F(WebNNGraphImplTest, TileTest) {
  {
    // Test tile operator with repetitions [2, 3, 1, 2].
    TileTester{.input = {.type = OperandDataType::kFloat32,
                         .dimensions = {1, 2, 3, 4}},
               .repetitions = {2, 3, 1, 2},
               .output = {.type = OperandDataType::kFloat32,
                          .dimensions = {2, 6, 3, 8}},
               .expected = true}
        .Test();
  }
  {
    // Test the invalid graph when the repetitions array is empty.
    TileTester{
        .input = {.type = OperandDataType::kFloat32, .dimensions = {1, 2, 3}},
        .repetitions = {},
        .output = {.type = OperandDataType::kFloat32, .dimensions = {1, 2, 3}},
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph when the rank of repetitions is larger than
    // the input rank.
    TileTester{
        .input = {.type = OperandDataType::kFloat32, .dimensions = {1, 2, 3}},
        .repetitions = {1, 1, 2, 2},
        .output = {.type = OperandDataType::kFloat32,
                   .dimensions = {1, 2, 3, 3}},
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph when the repetitions contain zero value.
    TileTester{.input = {.type = OperandDataType::kFloat32,
                         .dimensions = {1, 2, 3, 4}},
               .repetitions = {0, 1, 2, 2},
               .output = {.type = OperandDataType::kFloat32,
                          .dimensions = {1, 2, 3, 3}},
               .expected = false}
        .Test();
  }
  {
    // Test the invalid graph when any value in repetitions causes tiled
    // dimension size overflow.
    TileTester{.input = {.type = OperandDataType::kFloat32,
                         .dimensions = {1, 2, 34902, 4}},
               .repetitions = {1, 1, 232433, 2},
               .output = {.type = OperandDataType::kFloat32,
                          .dimensions = {1, 2, 2, 3}},
               .expected = false}
        .Test();
  }
  {
    // Test the invalid graph for output shapes are not expected.
    TileTester{
        .input = {.type = OperandDataType::kFloat32,
                  .dimensions = {1, 2, 3, 4}},
        .repetitions = {2, 1, 2, 3},
        .output = {.type = OperandDataType::kFloat32, .dimensions = {1, 2, 3}},
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph for output types don't match.
    TileTester{.input = {.type = OperandDataType::kFloat32,
                         .dimensions = {1, 2, 3, 4}},
               .repetitions = {0, 1, 2, 3},
               .output = {.type = OperandDataType::kFloat16,
                          .dimensions = {1, 2, 3, 4}},
               .expected = false}
        .Test();
  }
  {
    // Test the invalid graph for input operand == output operand.
    auto context_properties = GetContextPropertiesForTesting();
    GraphInfoBuilder builder;
    uint64_t input_operand_id =
        builder.BuildInput("input", {4, 6}, OperandDataType::kFloat32);
    builder.BuildTile(input_operand_id, input_operand_id,
                      std::vector<uint32_t>{1, 2});
    EXPECT_FALSE(WebNNGraphBuilderImpl::IsValidForTesting(
        context_properties, builder.GetGraphInfo()));
  }
}

struct TransposeTester {
  OperandInfo input;
  std::vector<uint32_t> permutation;
  OperandInfo output;
  bool expected;

  void Test() {
    auto context_properties = GetContextPropertiesForTesting();

    // Build the graph with mojo type.
    GraphInfoBuilder builder;
    uint64_t input_operand_id =
        builder.BuildInput("input", input.dimensions, input.type);
    uint64_t output_operand_id =
        builder.BuildOutput("output", output.dimensions, output.type);
    builder.BuildTranspose(input_operand_id, output_operand_id,
                           std::move(permutation));
    EXPECT_EQ(WebNNGraphBuilderImpl::IsValidForTesting(context_properties,
                                                       builder.GetGraphInfo()),
              expected);
  }
};

TEST_F(WebNNGraphImplTest, TransposeTest) {
  {
    // Test transpose operator with permutation [2, 3, 1, 0].
    TransposeTester{.input = {.type = OperandDataType::kFloat32,
                              .dimensions = {1, 2, 3, 4}},
                    .permutation = {2, 3, 1, 0},
                    .output = {.type = OperandDataType::kFloat32,
                               .dimensions = {3, 4, 2, 1}},
                    .expected = true}
        .Test();
  }
  {
    // Test the invalid graph when the rank of permutation is larger than
    // the input rank.
    TransposeTester{
        .input = {.type = OperandDataType::kFloat32, .dimensions = {1, 2, 3}},
        .permutation = {0, 1, 2, 2},
        .output = {.type = OperandDataType::kFloat32,
                   .dimensions = {1, 2, 3, 3}},
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph when the permutation contains duplicate
    // values.
    TransposeTester{.input = {.type = OperandDataType::kFloat32,
                              .dimensions = {1, 2, 3, 4}},
                    .permutation = {0, 1, 2, 2},
                    .output = {.type = OperandDataType::kFloat32,
                               .dimensions = {1, 2, 3, 3}},
                    .expected = false}
        .Test();
  }
  {
    // Test the invalid graph when one value in permutation is greater than
    // input_rank - 1.
    TransposeTester{.input = {.type = OperandDataType::kFloat16,
                              .dimensions = {1, 2, 3, 4}},
                    .permutation = {0, 1, 2, 4},
                    .output = {.type = OperandDataType::kFloat16,
                               .dimensions = {1, 2, 3, 4}},
                    .expected = false}
        .Test();
  }
  {
    // Test the invalid graph for output shapes are not expected.
    TransposeTester{
        .input = {.type = OperandDataType::kFloat32,
                  .dimensions = {1, 2, 3, 4}},
        .permutation = {0, 1, 2, 3},
        .output = {.type = OperandDataType::kFloat32, .dimensions = {1, 2, 3}},
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph for output types don't match.
    TransposeTester{.input = {.type = OperandDataType::kFloat32,
                              .dimensions = {1, 2, 3, 4}},
                    .permutation = {0, 1, 2, 3},
                    .output = {.type = OperandDataType::kFloat16,
                               .dimensions = {1, 2, 3, 4}},
                    .expected = false}
        .Test();
  }
}

struct TriangularTester {
  OperandInfo input;
  bool upper = true;
  int32_t diagonal = 0;
  OperandInfo output;
  bool expected;

  void Test() {
    auto context_properties = GetContextPropertiesForTesting();

    // Build the graph with mojo type.
    GraphInfoBuilder builder;
    uint64_t input_operand_id =
        builder.BuildInput("input", input.dimensions, input.type);
    uint64_t output_operand_id =
        builder.BuildOutput("output", output.dimensions, output.type);
    builder.BuildTriangular(input_operand_id, output_operand_id, upper,
                            diagonal);
    EXPECT_EQ(WebNNGraphBuilderImpl::IsValidForTesting(context_properties,
                                                       builder.GetGraphInfo()),
              expected);
  }
};

TEST_F(WebNNGraphImplTest, TriangularTest) {
  {
    // Test triangular operator with upper = true and diagonal = 2.
    TriangularTester{
        .input = {.type = OperandDataType::kFloat32, .dimensions = {2, 2}},
        .upper = true,
        .diagonal = 2,
        .output = {.type = OperandDataType::kFloat32, .dimensions = {2, 2}},
        .expected = true}
        .Test();
  }
  {
    // Test the invalid graph for the output shapes are not expected.
    TriangularTester{
        .input = {.type = OperandDataType::kFloat32, .dimensions = {4, 2}},
        .output = {.type = OperandDataType::kFloat32, .dimensions = {2}},
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph for output types don't match.
    TriangularTester{
        .input = {.type = OperandDataType::kFloat32, .dimensions = {2, 5}},
        .output = {.type = OperandDataType::kFloat16, .dimensions = {2, 5}},
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph for input operand == output operand.
    auto context_properties = GetContextPropertiesForTesting();
    GraphInfoBuilder builder;
    uint64_t input_operand_id =
        builder.BuildInput("input", {4, 6}, OperandDataType::kFloat32);

    builder.BuildTriangular(input_operand_id, input_operand_id,
                            /*upper*/ true, /*diagonal*/ -1);
    EXPECT_FALSE(WebNNGraphBuilderImpl::IsValidForTesting(
        context_properties, builder.GetGraphInfo()));
  }
}

struct WhereTester {
  OperandInfo condition;
  OperandInfo true_value;
  OperandInfo false_value;
  OperandInfo output;
  bool expected;

  void Test() {
    auto context_properties = GetContextPropertiesForTesting();

    // Build the graph with mojo type.
    GraphInfoBuilder builder;
    uint64_t condition_operand_id =
        builder.BuildInput("condition", condition.dimensions, condition.type);
    uint64_t true_value_operand_id = builder.BuildInput(
        "true_value", true_value.dimensions, true_value.type);
    uint64_t false_value_operand_id = builder.BuildInput(
        "false_value", false_value.dimensions, false_value.type);
    uint64_t output_operand_id =
        builder.BuildOutput("output", output.dimensions, output.type);
    builder.BuildWhere(condition_operand_id, true_value_operand_id,
                       false_value_operand_id, output_operand_id);
    EXPECT_EQ(WebNNGraphBuilderImpl::IsValidForTesting(context_properties,
                                                       builder.GetGraphInfo()),
              expected);
  }
};

TEST_F(WebNNGraphImplTest, WhereTest) {
  {
    // Test the invalid graph when the condition data type is not uint8.
    WhereTester{
        .condition = {.type = OperandDataType::kFloat32, .dimensions = {2, 4}},
        .true_value = {.type = OperandDataType::kFloat32, .dimensions = {2, 4}},
        .false_value = {.type = OperandDataType::kFloat32,
                        .dimensions = {2, 4}},
        .output = {.type = OperandDataType::kFloat32, .dimensions = {2, 4}},
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph when the the data types of true_value and
    // false_value don't match.
    WhereTester{
        .condition = {.type = OperandDataType::kUint8, .dimensions = {2, 4}},
        .true_value = {.type = OperandDataType::kFloat32, .dimensions = {2, 4}},
        .false_value = {.type = OperandDataType::kFloat16,
                        .dimensions = {2, 4}},
        .output = {.type = OperandDataType::kFloat32, .dimensions = {2, 4}},
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph when the the data types of output and
    // true_value don't match.
    WhereTester{
        .condition = {.type = OperandDataType::kUint8, .dimensions = {2, 4}},
        .true_value = {.type = OperandDataType::kFloat32, .dimensions = {2, 4}},
        .false_value = {.type = OperandDataType::kFloat32,
                        .dimensions = {2, 4}},
        .output = {.type = OperandDataType::kFloat16, .dimensions = {2, 4}},
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph when the the shape of output is wrong.
    WhereTester{
        .condition = {.type = OperandDataType::kUint8, .dimensions = {2, 4}},
        .true_value = {.type = OperandDataType::kFloat32, .dimensions = {2, 4}},
        .false_value = {.type = OperandDataType::kFloat32,
                        .dimensions = {2, 4}},
        .output = {.type = OperandDataType::kFloat32, .dimensions = {2, 5}},
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph when the shapes of true_value and false_value
    // are not broadcastable.
    WhereTester{
        .condition = {.type = OperandDataType::kUint8, .dimensions = {2, 4}},
        .true_value = {.type = OperandDataType::kFloat32, .dimensions = {2, 4}},
        .false_value = {.type = OperandDataType::kFloat32,
                        .dimensions = {2, 3}},
        .output = {.type = OperandDataType::kFloat32, .dimensions = {2, 4}},
        .expected = false}
        .Test();
  }
  {
    // Test the invalid graph when the condition shape is not broadcastable.
    WhereTester{
        .condition = {.type = OperandDataType::kUint8, .dimensions = {2, 4}},
        .true_value = {.type = OperandDataType::kFloat32, .dimensions = {2, 3}},
        .false_value = {.type = OperandDataType::kFloat32,
                        .dimensions = {2, 1}},
        .output = {.type = OperandDataType::kFloat32, .dimensions = {2, 4}},
        .expected = false}
        .Test();
  }
  {
    // Test where with 2-D condition, 2-D true_value and 2-D false_value using
    // broadcast.
    WhereTester{
        .condition = {.type = OperandDataType::kUint8, .dimensions = {2, 1}},
        .true_value = {.type = OperandDataType::kFloat32, .dimensions = {2, 4}},
        .false_value = {.type = OperandDataType::kFloat32,
                        .dimensions = {2, 4}},
        .output = {.type = OperandDataType::kFloat32, .dimensions = {2, 4}},
        .expected = true}
        .Test();
  }
  {
    // Test where with 2-D condition, 2-D true_value and 3-D false_value using
    // broadcast.
    WhereTester{
        .condition = {.type = OperandDataType::kUint8, .dimensions = {1, 4}},
        .true_value = {.type = OperandDataType::kFloat32, .dimensions = {3, 4}},
        .false_value = {.type = OperandDataType::kFloat32,
                        .dimensions = {2, 3, 4}},
        .output = {.type = OperandDataType::kFloat32, .dimensions = {2, 3, 4}},
        .expected = true}
        .Test();
  }
  {
    // Test where with 3-D condition, 3-D true_value and 3-D false_value using
    // broadcast.
    WhereTester{
        .condition = {.type = OperandDataType::kUint8, .dimensions = {2, 1, 4}},
        .true_value = {.type = OperandDataType::kFloat32,
                       .dimensions = {2, 3, 4}},
        .false_value = {.type = OperandDataType::kFloat32,
                        .dimensions = {1, 4}},
        .output = {.type = OperandDataType::kFloat32, .dimensions = {2, 3, 4}},
        .expected = true}
        .Test();
  }
  {
    // Test where with 4-D condition, 3-D true_value and 2-D false_value using
    // broadcast.
    WhereTester{.condition = {.type = OperandDataType::kUint8,
                              .dimensions = {2, 3, 4, 5}},
                .true_value = {.type = OperandDataType::kFloat32,
                               .dimensions = {3, 4, 5}},
                .false_value = {.type = OperandDataType::kFloat32,
                                .dimensions = {4, 5}},
                .output = {.type = OperandDataType::kFloat32,
                           .dimensions = {2, 3, 4, 5}},
                .expected = true}
        .Test();
  }
  {
    // Test the invalid graph when the condition is as same as output.
    auto context_properties = GetContextPropertiesForTesting();
    GraphInfoBuilder builder;
    uint64_t condition_operand_id =
        builder.BuildInput("condition", {2, 4}, OperandDataType::kUint8);
    uint64_t true_value_operand_id =
        builder.BuildInput("true_value", {2, 4}, OperandDataType::kFloat32);
    uint64_t false_value_operand_id =
        builder.BuildInput("false_value", {2, 4}, OperandDataType::kFloat32);
    builder.BuildWhere(condition_operand_id, true_value_operand_id,
                       false_value_operand_id, condition_operand_id);
    EXPECT_FALSE(WebNNGraphBuilderImpl::IsValidForTesting(
        context_properties, builder.GetGraphInfo()));
  }
  {
    // Test the invalid graph when the true_value is as same as output.
    auto context_properties = GetContextPropertiesForTesting();
    GraphInfoBuilder builder;
    uint64_t condition_operand_id =
        builder.BuildInput("condition", {2, 4}, OperandDataType::kUint8);
    uint64_t true_value_operand_id =
        builder.BuildInput("true_value", {2, 4}, OperandDataType::kFloat32);
    uint64_t false_value_operand_id =
        builder.BuildInput("false_value", {2, 4}, OperandDataType::kFloat32);
    builder.BuildWhere(condition_operand_id, true_value_operand_id,
                       false_value_operand_id, true_value_operand_id);
    EXPECT_FALSE(WebNNGraphBuilderImpl::IsValidForTesting(
        context_properties, builder.GetGraphInfo()));
  }
  {
    // Test the invalid graph when the false_value is as same as output.
    auto context_properties = GetContextPropertiesForTesting();
    GraphInfoBuilder builder;
    uint64_t condition_operand_id =
        builder.BuildInput("condition", {2, 4}, OperandDataType::kUint8);
    uint64_t true_value_operand_id =
        builder.BuildInput("true_value", {2, 4}, OperandDataType::kFloat32);
    uint64_t false_value_operand_id =
        builder.BuildInput("false_value", {2, 4}, OperandDataType::kFloat32);
    builder.BuildWhere(condition_operand_id, true_value_operand_id,
                       false_value_operand_id, false_value_operand_id);
    EXPECT_FALSE(WebNNGraphBuilderImpl::IsValidForTesting(
        context_properties, builder.GetGraphInfo()));
  }
}

TEST_F(WebNNGraphImplTest, ValidateInputsTest) {
  auto context_properties = GetContextPropertiesForTesting();
  const std::vector<uint32_t> dimensions = {3, 5};
  const size_t byte_length = 15;
  // Build the graph with mojo type.
  GraphInfoBuilder builder;
  uint64_t lhs_operand_id =
      builder.BuildInput("lhs", dimensions, OperandDataType::kUint8);
  uint64_t rhs_operand_id =
      builder.BuildInput("rhs", dimensions, OperandDataType::kUint8);
  uint64_t output_operand_id =
      builder.BuildOutput("output", dimensions, OperandDataType::kUint8);
  builder.BuildElementWiseBinary(mojom::ElementWiseBinary::Kind::kAdd,
                                 lhs_operand_id, rhs_operand_id,
                                 output_operand_id);
  EXPECT_TRUE(WebNNGraphBuilderImpl::IsValidForTesting(context_properties,
                                                       builder.GetGraphInfo()));

  {
    // Validate the inputs match the expected.
    base::flat_map<std::string, mojo_base::BigBuffer> inputs;
    inputs["lhs"] = std::vector<uint8_t>(byte_length);
    inputs["rhs"] = std::vector<uint8_t>(byte_length);
    EXPECT_TRUE(ValidateInputsForComputing(builder.CloneGraphInfo(),
                                           std::move(inputs)));
  }
  {
    // Test the invalid inputs for invalid input size.
    base::flat_map<std::string, mojo_base::BigBuffer> inputs;
    inputs["lhs"] = std::vector<uint8_t>(byte_length);
    EXPECT_FALSE(ValidateInputsForComputing(builder.CloneGraphInfo(),
                                            std::move(inputs)));
  }
  {
    // Test the invalid inputs for invalid input name.
    base::flat_map<std::string, mojo_base::BigBuffer> inputs;
    inputs["a_different_input_name"] = std::vector<uint8_t>(byte_length);
    inputs["rhs"] = std::vector<uint8_t>(byte_length);
    EXPECT_FALSE(ValidateInputsForComputing(builder.CloneGraphInfo(),
                                            std::move(inputs)));
  }
  {
    // Test the invalid inputs for invalid first input byte length.
    base::flat_map<std::string, mojo_base::BigBuffer> inputs;
    inputs["lhs"] = std::vector<uint8_t>(20);
    inputs["rhs"] = std::vector<uint8_t>(byte_length);
    EXPECT_FALSE(ValidateInputsForComputing(builder.CloneGraphInfo(),
                                            std::move(inputs)));
  }
  {
    // Test the invalid inputs for invalid second input byte length.
    base::flat_map<std::string, mojo_base::BigBuffer> inputs;
    inputs["lhs"] = std::vector<uint8_t>(byte_length);
    inputs["rhs"] = std::vector<uint8_t>(20);
    EXPECT_FALSE(ValidateInputsForComputing(builder.CloneGraphInfo(),
                                            std::move(inputs)));
  }
}

TEST_F(WebNNGraphImplTest, ValidateDispatchTest) {
  auto context_properties = GetContextPropertiesForTesting();
  // TODO(crbug.com/325598628): De-dup these data type constants.
  const OperandDataType kMojoDataType = OperandDataType::kUint8;
  const OperandDataType kDataType = OperandDataType::kUint8;
  const std::vector<uint32_t> kShape = {3, 5};
  // Build the graph with mojo type.
  GraphInfoBuilder builder;
  const uint64_t lhs_operand_id =
      builder.BuildInput("lhs", kShape, kMojoDataType);
  const uint64_t rhs_operand_id =
      builder.BuildInput("rhs", kShape, kMojoDataType);
  const uint64_t output_1_operand_id =
      builder.BuildOutput("output1", kShape, kMojoDataType);
  builder.BuildElementWiseBinary(mojom::ElementWiseBinary::Kind::kAdd,
                                 lhs_operand_id, rhs_operand_id,
                                 output_1_operand_id);
  const uint64_t output_2_operand_id =
      builder.BuildOutput("output2", kShape, kMojoDataType);
  builder.BuildElementWiseBinary(mojom::ElementWiseBinary::Kind::kAdd,
                                 lhs_operand_id, rhs_operand_id,
                                 output_2_operand_id);
  EXPECT_TRUE(WebNNGraphBuilderImpl::IsValidForTesting(context_properties,
                                                       builder.GetGraphInfo()));

  mojo::Remote<mojom::WebNNContextProvider> provider_remote;
  WebNNContextProviderImpl::CreateForTesting(
      provider_remote.BindNewPipeAndPassReceiver());

  {
    // Validate the inputs match the expected.
    mojo::Remote<mojom::WebNNContext> webnn_context =
        CreateWebNNContext(provider_remote);
    base::flat_map<std::string, CreateTensorSuccess> inputs;
    inputs["lhs"] = CreateWebNNTensor(webnn_context, kDataType, kShape);
    inputs["rhs"] = CreateWebNNTensor(webnn_context, kDataType, kShape);
    base::flat_map<std::string, CreateTensorSuccess> outputs;
    outputs["output1"] = CreateWebNNTensor(webnn_context, kDataType, kShape);
    outputs["output2"] = CreateWebNNTensor(webnn_context, kDataType, kShape);
    EXPECT_TRUE(ValidateDispatch(webnn_context, builder.CloneGraphInfo(),
                                 std::move(inputs), std::move(outputs)));
  }
  {
    // Test the invalid inputs for invalid input size.
    mojo::Remote<mojom::WebNNContext> webnn_context =
        CreateWebNNContext(provider_remote);
    base::flat_map<std::string, CreateTensorSuccess> inputs;
    inputs["lhs"] = CreateWebNNTensor(webnn_context, kDataType, kShape);
    base::flat_map<std::string, CreateTensorSuccess> outputs;
    outputs["output1"] = CreateWebNNTensor(webnn_context, kDataType, kShape);
    outputs["output2"] = CreateWebNNTensor(webnn_context, kDataType, kShape);
    EXPECT_FALSE(ValidateDispatch(webnn_context, builder.CloneGraphInfo(),
                                  std::move(inputs), std::move(outputs)));
  }
  {
    // Test the invalid outputs for invalid output size.
    mojo::Remote<mojom::WebNNContext> webnn_context =
        CreateWebNNContext(provider_remote);
    base::flat_map<std::string, CreateTensorSuccess> inputs;
    inputs["lhs"] = CreateWebNNTensor(webnn_context, kDataType, kShape);
    inputs["rhs"] = CreateWebNNTensor(webnn_context, kDataType, kShape);
    base::flat_map<std::string, CreateTensorSuccess> outputs;
    outputs["output1"] = CreateWebNNTensor(webnn_context, kDataType, kShape);
    outputs["output2"] = CreateWebNNTensor(webnn_context, kDataType, kShape);
    outputs["a_different_output_name"] =
        CreateWebNNTensor(webnn_context, kDataType, kShape);
    EXPECT_FALSE(ValidateDispatch(webnn_context, builder.CloneGraphInfo(),
                                  std::move(inputs), std::move(outputs)));
  }
  {
    // Test the invalid inputs for invalid input name.
    mojo::Remote<mojom::WebNNContext> webnn_context =
        CreateWebNNContext(provider_remote);
    base::flat_map<std::string, CreateTensorSuccess> inputs;
    inputs["a_different_input_name"] =
        CreateWebNNTensor(webnn_context, kDataType, kShape);
    inputs["rhs"] = CreateWebNNTensor(webnn_context, kDataType, kShape);
    base::flat_map<std::string, CreateTensorSuccess> outputs;
    outputs["output1"] = CreateWebNNTensor(webnn_context, kDataType, kShape);
    outputs["output2"] = CreateWebNNTensor(webnn_context, kDataType, kShape);
    EXPECT_FALSE(ValidateDispatch(webnn_context, builder.CloneGraphInfo(),
                                  std::move(inputs), std::move(outputs)));
  }
  {
    // Test the invalid outputs for invalid input name.
    mojo::Remote<mojom::WebNNContext> webnn_context =
        CreateWebNNContext(provider_remote);
    base::flat_map<std::string, CreateTensorSuccess> inputs;
    inputs["lhs"] = CreateWebNNTensor(webnn_context, kDataType, kShape);
    inputs["rhs"] = CreateWebNNTensor(webnn_context, kDataType, kShape);
    base::flat_map<std::string, CreateTensorSuccess> outputs;
    outputs["a_different_output_name"] =
        CreateWebNNTensor(webnn_context, kDataType, kShape);
    outputs["output2"] = CreateWebNNTensor(webnn_context, kDataType, kShape);
    EXPECT_FALSE(ValidateDispatch(webnn_context, builder.CloneGraphInfo(),
                                  std::move(inputs), std::move(outputs)));
  }
  {
    // Test the invalid inputs for invalid first input shape.
    mojo::Remote<mojom::WebNNContext> webnn_context =
        CreateWebNNContext(provider_remote);
    base::flat_map<std::string, CreateTensorSuccess> inputs;
    inputs["lhs"] = CreateWebNNTensor(webnn_context, kDataType, {2, 5});
    inputs["rhs"] = CreateWebNNTensor(webnn_context, kDataType, kShape);
    base::flat_map<std::string, CreateTensorSuccess> outputs;
    outputs["output1"] = CreateWebNNTensor(webnn_context, kDataType, kShape);
    outputs["output2"] = CreateWebNNTensor(webnn_context, kDataType, kShape);
    EXPECT_FALSE(ValidateDispatch(webnn_context, builder.CloneGraphInfo(),
                                  std::move(inputs), std::move(outputs)));
  }
  {
    // Test the invalid inputs for invalid first input data type.
    mojo::Remote<mojom::WebNNContext> webnn_context =
        CreateWebNNContext(provider_remote);
    base::flat_map<std::string, CreateTensorSuccess> inputs;
    inputs["lhs"] =
        CreateWebNNTensor(webnn_context, OperandDataType::kInt8, kShape);
    inputs["rhs"] = CreateWebNNTensor(webnn_context, kDataType, kShape);
    base::flat_map<std::string, CreateTensorSuccess> outputs;
    outputs["output1"] = CreateWebNNTensor(webnn_context, kDataType, kShape);
    outputs["output2"] = CreateWebNNTensor(webnn_context, kDataType, kShape);
    EXPECT_FALSE(ValidateDispatch(webnn_context, builder.CloneGraphInfo(),
                                  std::move(inputs), std::move(outputs)));
  }
  {
    // Test the invalid outputs for invalid first output shape.
    mojo::Remote<mojom::WebNNContext> webnn_context =
        CreateWebNNContext(provider_remote);
    base::flat_map<std::string, CreateTensorSuccess> inputs;
    inputs["lhs"] = CreateWebNNTensor(webnn_context, kDataType, kShape);
    inputs["rhs"] = CreateWebNNTensor(webnn_context, kDataType, kShape);
    base::flat_map<std::string, CreateTensorSuccess> outputs;
    outputs["output1"] = CreateWebNNTensor(webnn_context, kDataType, {3, 4});
    outputs["output2"] = CreateWebNNTensor(webnn_context, kDataType, kShape);
    EXPECT_FALSE(ValidateDispatch(webnn_context, builder.CloneGraphInfo(),
                                  std::move(inputs), std::move(outputs)));
  }
  {
    // Test the invalid inputs for invalid second input data type.
    mojo::Remote<mojom::WebNNContext> webnn_context =
        CreateWebNNContext(provider_remote);
    base::flat_map<std::string, CreateTensorSuccess> inputs;
    inputs["lhs"] = CreateWebNNTensor(webnn_context, kDataType, kShape);
    inputs["rhs"] =
        CreateWebNNTensor(webnn_context, OperandDataType::kInt32, kShape);
    base::flat_map<std::string, CreateTensorSuccess> outputs;
    outputs["output1"] = CreateWebNNTensor(webnn_context, kDataType, kShape);
    outputs["output2"] = CreateWebNNTensor(webnn_context, kDataType, kShape);
    EXPECT_FALSE(ValidateDispatch(webnn_context, builder.CloneGraphInfo(),
                                  std::move(inputs), std::move(outputs)));
  }
  {
    // Test the invalid outputs for invalid second output shape.
    mojo::Remote<mojom::WebNNContext> webnn_context =
        CreateWebNNContext(provider_remote);
    base::flat_map<std::string, CreateTensorSuccess> inputs;
    inputs["lhs"] = CreateWebNNTensor(webnn_context, kDataType, kShape);
    inputs["rhs"] = CreateWebNNTensor(webnn_context, kDataType, kShape);
    base::flat_map<std::string, CreateTensorSuccess> outputs;
    outputs["output1"] = CreateWebNNTensor(webnn_context, kDataType, kShape);
    outputs["output2"] = CreateWebNNTensor(webnn_context, kDataType, {2, 5});
    EXPECT_FALSE(ValidateDispatch(webnn_context, builder.CloneGraphInfo(),
                                  std::move(inputs), std::move(outputs)));
  }
  {
    // Test the inputs using the same tensor more than once.
    mojo::Remote<mojom::WebNNContext> webnn_context =
        CreateWebNNContext(provider_remote);
    base::flat_map<std::string, CreateTensorSuccess> inputs;
    inputs["lhs"] = CreateWebNNTensor(webnn_context, kDataType, kShape);
    inputs["rhs"] = {/*webnn_tensor=*/std::nullopt, inputs["lhs"].webnn_handle};
    base::flat_map<std::string, CreateTensorSuccess> outputs;
    outputs["output1"] = CreateWebNNTensor(webnn_context, kDataType, kShape);
    outputs["output2"] = CreateWebNNTensor(webnn_context, kDataType, kShape);
    EXPECT_TRUE(ValidateDispatch(webnn_context, builder.CloneGraphInfo(),
                                 std::move(inputs), std::move(outputs)));
  }
  {
    // Test the invalid outputs when using the same tensor more than once.
    mojo::Remote<mojom::WebNNContext> webnn_context =
        CreateWebNNContext(provider_remote);
    base::flat_map<std::string, CreateTensorSuccess> inputs;
    inputs["lhs"] = CreateWebNNTensor(webnn_context, kDataType, kShape);
    inputs["rhs"] = CreateWebNNTensor(webnn_context, kDataType, kShape);
    base::flat_map<std::string, CreateTensorSuccess> outputs;
    outputs["output1"] = CreateWebNNTensor(webnn_context, kDataType, kShape);
    outputs["output2"] = {/*webnn_tensor=*/std::nullopt,
                          outputs["output1"].webnn_handle};
    EXPECT_FALSE(ValidateDispatch(webnn_context, builder.CloneGraphInfo(),
                                  std::move(inputs), std::move(outputs)));
  }
  {
    // Test the inputs and outputs are invalid when using the same tensor.
    mojo::Remote<mojom::WebNNContext> webnn_context =
        CreateWebNNContext(provider_remote);
    base::flat_map<std::string, CreateTensorSuccess> inputs;
    inputs["lhs"] = CreateWebNNTensor(webnn_context, kDataType, kShape);
    inputs["rhs"] = CreateWebNNTensor(webnn_context, kDataType, kShape);
    base::flat_map<std::string, CreateTensorSuccess> outputs;
    outputs["output1"] = {/*webnn_tensor=*/std::nullopt,
                          inputs["lhs"].webnn_handle};
    outputs["output2"] = CreateWebNNTensor(webnn_context, kDataType, kShape);
    EXPECT_FALSE(ValidateDispatch(webnn_context, builder.CloneGraphInfo(),
                                  std::move(inputs), std::move(outputs)));
  }
  {
    // Test the inputs are invalid when using a invalid tensor.
    mojo::Remote<mojom::WebNNContext> webnn_context =
        CreateWebNNContext(provider_remote);
    base::flat_map<std::string, CreateTensorSuccess> inputs;
    inputs["lhs"] = {/*webnn_tensor=*/std::nullopt};
    inputs["rhs"] = CreateWebNNTensor(webnn_context, kDataType, kShape);
    base::flat_map<std::string, CreateTensorSuccess> outputs;
    outputs["output1"] = CreateWebNNTensor(webnn_context, kDataType, kShape);
    outputs["output2"] = CreateWebNNTensor(webnn_context, kDataType, kShape);
    EXPECT_FALSE(ValidateDispatch(webnn_context, builder.CloneGraphInfo(),
                                  std::move(inputs), std::move(outputs)));
  }
  {
    // Test the outputs are invalid when using a invalid tensor.
    mojo::Remote<mojom::WebNNContext> webnn_context =
        CreateWebNNContext(provider_remote);
    base::flat_map<std::string, CreateTensorSuccess> inputs;
    inputs["lhs"] = CreateWebNNTensor(webnn_context, kDataType, kShape);
    inputs["rhs"] = CreateWebNNTensor(webnn_context, kDataType, kShape);
    base::flat_map<std::string, CreateTensorSuccess> outputs;
    outputs["output1"] = CreateWebNNTensor(webnn_context, kDataType, kShape);
    outputs["output2"] = {/*webnn_tensor=*/std::nullopt};
    EXPECT_FALSE(ValidateDispatch(webnn_context, builder.CloneGraphInfo(),
                                  std::move(inputs), std::move(outputs)));
  }
}

struct ConstantOperandTester {
  std::vector<uint8_t> values;
  bool expected;

  void Test() {
    auto context_properties = GetContextPropertiesForTesting();

    const std::vector<uint32_t> dimensions = {3, 5};
    // Build the graph with mojo type.
    GraphInfoBuilder builder;
    uint64_t lhs_operand_id =
        builder.BuildInput("lhs", dimensions, OperandDataType::kUint8);
    uint64_t rhs_operand_id =
        builder.BuildConstant(dimensions, OperandDataType::kUint8, values);
    uint64_t output_operand_id =
        builder.BuildOutput("output", dimensions, OperandDataType::kUint8);
    builder.BuildElementWiseBinary(mojom::ElementWiseBinary::Kind::kAdd,
                                   lhs_operand_id, rhs_operand_id,
                                   output_operand_id);
    EXPECT_EQ(WebNNGraphBuilderImpl::IsValidForTesting(context_properties,
                                                       builder.GetGraphInfo()),
              expected);
  }
};

TEST_F(WebNNGraphImplTest, ValidateConstantOperandTest) {
  {
    // Test valid constant data.
    ConstantOperandTester{.values = std::vector<uint8_t>(15), .expected = true}
        .Test();
  }
  {
    // Test the invalid graph for the byte length of constant data doesn't
    // match the graph's expected.
    ConstantOperandTester{.values = std::vector<uint8_t>(10), .expected = false}
        .Test();
  }
}

// Test building a graph with two inputs and two constant in the following
// topology.
//    [input_a] [constant_a] [input_b] [constant_b]
//           \    /                \    /
//            gemm                  gemm
//                \                /
//                       gemm
TEST_F(WebNNGraphImplTest, BuildMultipleInputsAppendingConstants) {
  auto context_properties = GetContextPropertiesForTesting();
  // Build the mojom graph info.
  GraphInfoBuilder builder;
  // The graph outputs are built first, and then inputs / constants.
  uint64_t output_operand_id =
      builder.BuildOutput("output", {2, 2}, OperandDataType::kFloat32);
  uint64_t input_a_operand_id =
      builder.BuildInput("input_a", {2, 2}, OperandDataType::kFloat32);
  std::vector<float> constant_data = {5.0, 6.0, 7.0, 8.0};
  uint64_t constant_a_operand_id = builder.BuildConstant(
      {2, 2}, OperandDataType::kFloat32, base::as_byte_span(constant_data));

  uint64_t intermediate_1_operand_id =
      builder.BuildIntermediateOperand({2, 2}, OperandDataType::kFloat32);
  builder.BuildGemm(input_a_operand_id, constant_a_operand_id,
                    intermediate_1_operand_id, GemmTester::GemmAttributes());

  uint64_t input_b_operand_id =
      builder.BuildInput("input_b", {2, 2}, OperandDataType::kFloat32);
  uint64_t constant_b_operand_id = builder.BuildConstant(
      {2, 2}, OperandDataType::kFloat32, base::as_byte_span(constant_data));
  uint64_t intermediate_2_operand_id =
      builder.BuildIntermediateOperand({2, 2}, OperandDataType::kFloat32);
  builder.BuildGemm(input_b_operand_id, constant_b_operand_id,
                    intermediate_2_operand_id, GemmTester::GemmAttributes());
  builder.BuildGemm(intermediate_1_operand_id, intermediate_2_operand_id,
                    output_operand_id, GemmTester::GemmAttributes());
  EXPECT_TRUE(WebNNGraphBuilderImpl::IsValidForTesting(context_properties,
                                                       builder.GetGraphInfo()));
}

// Test building a graph with two inputs and two constant in the following
// topology.
//    [constant_a] [input_a] [constant_b] [input_b]
//           \    /                \    /
//            gemm                  gemm
//                \                /
//                       gemm
TEST_F(WebNNGraphImplTest, BuildMultipleConstantsAppendingInputs) {
  auto context_properties = GetContextPropertiesForTesting();
  // Build the mojom graph info.
  GraphInfoBuilder builder;
  // The graph outputs are built first, and then inputs / constants.
  uint64_t output_operand_id =
      builder.BuildOutput("output", {2, 2}, OperandDataType::kFloat32);
  std::vector<float> constant_data = {5.0, 6.0, 7.0, 8.0};
  uint64_t constant_a_operand_id = builder.BuildConstant(
      {2, 2}, OperandDataType::kFloat32, base::as_byte_span(constant_data));
  uint64_t input_a_operand_id =
      builder.BuildInput("input_a", {2, 2}, OperandDataType::kFloat32);
  uint64_t intermediate_1_operand_id =
      builder.BuildIntermediateOperand({2, 2}, OperandDataType::kFloat32);
  builder.BuildGemm(constant_a_operand_id, input_a_operand_id,
                    intermediate_1_operand_id, GemmTester::GemmAttributes());

  uint64_t input_b_operand_id =
      builder.BuildInput("input_b", {2, 2}, OperandDataType::kFloat32);
  uint64_t constant_b_operand_id = builder.BuildConstant(
      {2, 2}, OperandDataType::kFloat32, base::as_byte_span(constant_data));
  uint64_t intermediate_2_operand_id =
      builder.BuildIntermediateOperand({2, 2}, OperandDataType::kFloat32);
  builder.BuildGemm(constant_b_operand_id, input_b_operand_id,
                    intermediate_2_operand_id, GemmTester::GemmAttributes());

  builder.BuildGemm(intermediate_1_operand_id, intermediate_2_operand_id,
                    output_operand_id, GemmTester::GemmAttributes());
  EXPECT_TRUE(WebNNGraphBuilderImpl::IsValidForTesting(context_properties,
                                                       builder.GetGraphInfo()));
}

TEST_F(WebNNGraphImplTest, BuildOperationWithNonexistentInputs) {
  auto context_properties = GetContextPropertiesForTesting();
  GraphInfoBuilder builder;
  uint64_t input_operand_id =
      builder.BuildInput("input_a", {2, 2}, OperandDataType::kFloat32);

  uint64_t intermediate_operand_id =
      builder.BuildIntermediateOperand({2, 2}, OperandDataType::kFloat32);
  uint64_t output_operand_id =
      builder.BuildOutput("output", {2, 2}, OperandDataType::kUint8);
  builder.BuildRelu(intermediate_operand_id, output_operand_id);
  builder.BuildRelu(input_operand_id, intermediate_operand_id);
  EXPECT_FALSE(WebNNGraphBuilderImpl::IsValidForTesting(
      context_properties, builder.GetGraphInfo()));
}

}  // namespace webnn
