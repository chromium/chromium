// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/webnn_graph_builder_impl.h"

#include <memory>

#include "base/containers/span.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/notimplemented.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "services/webnn/error.h"
#include "services/webnn/public/cpp/operand_descriptor.h"
#include "services/webnn/public/cpp/webnn_types.h"
#include "services/webnn/public/mojom/features.mojom-features.h"
#include "services/webnn/public/mojom/webnn_tensor.mojom.h"
#include "services/webnn/scoped_sequence.h"
#include "services/webnn/webnn_constant_operand.h"
#include "services/webnn/webnn_context_impl.h"
#include "services/webnn/webnn_context_provider_impl.h"
#include "services/webnn/webnn_graph_impl.h"
#include "services/webnn/webnn_tensor_impl.h"
#include "services/webnn/webnn_test_environment.h"
#include "services/webnn/webnn_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/tokens/tokens.h"

namespace webnn {

namespace {

mojom::GraphInfoPtr BuildSimpleGraphInfo(
    mojo::AssociatedRemote<mojom::WebNNGraphBuilder>& graph_builder_remote) {
  // Build a simple graph.
  GraphInfoBuilder builder(graph_builder_remote);
  OperandId input_operand_id = builder.BuildInput(
      "input", /*dimensions=*/{2, 3}, OperandDataType::kFloat32);
  OperandId output_operand_id = builder.BuildOutput(
      "output", /*dimensions=*/{2, 3}, OperandDataType::kFloat32);
  builder.BuildClamp(input_operand_id, output_operand_id, /*min_value=*/0.0,
                     /*max_value=*/1.0);
  return builder.TakeGraphInfo();
}

// A fake WebNNGraph Mojo interface implementation that binds a pipe for
// computing graph message.
class FakeWebNNGraphImpl final : public WebNNGraphImpl {
 public:
  FakeWebNNGraphImpl(
      mojo::PendingAssociatedReceiver<mojom::WebNNGraph> receiver,
      base::WeakPtr<WebNNContextImpl> context,
      ComputeResourceInfo compute_resource_info)
      : WebNNGraphImpl(std::move(receiver),
                       std::move(context),
                       std::move(compute_resource_info),
                       /*devices=*/{}) {}

 private:
  ~FakeWebNNGraphImpl() override = default;

  void DispatchImpl(
      base::flat_map<std::string, scoped_refptr<WebNNTensorImpl>> named_inputs,
      base::flat_map<std::string, scoped_refptr<WebNNTensorImpl>> named_outputs)
      override {
    NOTIMPLEMENTED();
  }
};

// A fake WebNNContext Mojo interface implementation that binds a pipe for
// creating graph message.
class FakeWebNNContextImpl final : public WebNNContextImpl {
 public:
  FakeWebNNContextImpl(
      mojo::PendingReceiver<mojom::WebNNContext> receiver,
      base::WeakPtr<WebNNContextProviderImpl> context_provider,
      gpu::CommandBufferId command_buffer_id,
      std::unique_ptr<ScopedSequence> sequence,
      scoped_refptr<gpu::MemoryTracker> memory_tracker,
      scoped_refptr<base::SingleThreadTaskRunner> owning_task_runner,
      gpu::SharedImageManager* shared_image_manager,
      scoped_refptr<base::SingleThreadTaskRunner> main_task_runner)
      : WebNNContextImpl(std::move(receiver),
                         std::move(context_provider),
                         GetContextPropertiesForTesting(),
                         mojom::CreateContextOptions::New(),
                         mojo::ScopedDataPipeConsumerHandle(),
                         mojo::ScopedDataPipeProducerHandle(),
                         command_buffer_id,
                         std::move(sequence),
                         std::move(memory_tracker),
                         std::move(owning_task_runner),
                         shared_image_manager,
                         std::move(main_task_runner)) {}

  // WebNNContextImpl:
  base::WeakPtr<WebNNContextImpl> AsWeakPtr() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);
    return weak_factory_.GetWeakPtr();
  }

 private:
  ~FakeWebNNContextImpl() override = default;

  void CreateGraphImpl(
      mojo::PendingAssociatedReceiver<mojom::WebNNGraph> receiver,
      mojom::GraphInfoPtr graph_info,
      WebNNGraphImpl::ComputeResourceInfo compute_resource_info,
      base::flat_map<OperandId, std::unique_ptr<WebNNConstantOperand>>
      /*constant_operands*/,
      base::flat_map<OperandId, WebNNTensorImpl*>
      /*constant_tensor_operands*/,
      CreateGraphImplCallback callback) override {
    // Asynchronously resolve `callback` so there's an opportunity for
    // subsequent messages to be (illegally) sent from the `WebNNGraphBuilder`
    // remote before it's disconnected.
    scheduler_task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](mojo::PendingAssociatedReceiver<mojom::WebNNGraph> receiver,
               base::WeakPtr<WebNNContextImpl> context,
               WebNNGraphImpl::ComputeResourceInfo compute_resource_info,
               CreateGraphImplCallback callback) {
              CHECK(context);
              std::move(callback).Run(base::MakeRefCounted<FakeWebNNGraphImpl>(
                  std::move(receiver), std::move(context),
                  std::move(compute_resource_info)));
            },
            std::move(receiver), AsWeakPtr(), std::move(compute_resource_info),
            std::move(callback)));
  }

  base::expected<scoped_refptr<WebNNTensorImpl>, mojom::ErrorPtr>
  CreateTensorImpl(mojo::PendingAssociatedReceiver<mojom::WebNNTensor> receiver,
                   mojom::TensorInfoPtr tensor_info) override {
    return base::unexpected(mojom::Error::New(
        mojom::Error::Code::kNotSupportedError, "Not implemented"));
  }

  base::expected<scoped_refptr<WebNNTensorImpl>, mojom::ErrorPtr>
  CreateTensorFromSharedImageImpl(
      mojo::PendingAssociatedReceiver<mojom::WebNNTensor> receiver,
      mojom::TensorInfoPtr tensor_info,
      WebNNTensorImpl::RepresentationPtr representation) override {
    return base::unexpected(mojom::Error::New(
        mojom::Error::Code::kNotSupportedError, "Not implemented"));
  }

  base::WeakPtrFactory<FakeWebNNContextImpl> weak_factory_{this};
};

// Helper class to create the FakeWebNNContext that is intended to test
// the graph validation steps and computation resources.
class FakeWebNNBackend : public WebNNContextProviderImpl::BackendForTesting {
 public:
  std::unique_ptr<WebNNContextImpl, OnTaskRunnerDeleter> CreateWebNNContext(
      base::WeakPtr<WebNNContextProviderImpl> context_provider_impl,
      mojom::CreateContextOptionsPtr options,
      gpu::CommandBufferId command_buffer_id,
      std::unique_ptr<ScopedSequence> sequence,
      scoped_refptr<gpu::MemoryTracker> memory_tracker,
      scoped_refptr<base::SingleThreadTaskRunner> owning_task_runner,
      gpu::SharedImageManager* shared_image_manager,
      scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
      mojom::WebNNContextProvider::CreateWebNNContextCallback callback)
      override {
    mojo::PendingRemote<mojom::WebNNContext> remote;
    auto task_runner = owning_task_runner;
    std::unique_ptr<WebNNContextImpl, OnTaskRunnerDeleter> context_impl(
        new FakeWebNNContextImpl(
            remote.InitWithNewPipeAndPassReceiver(),
            std::move(context_provider_impl), command_buffer_id,
            std::move(sequence), std::move(memory_tracker),
            std::move(owning_task_runner), shared_image_manager,
            std::move(main_task_runner)),
        OnTaskRunnerDeleter(std::move(task_runner)));
    ContextProperties context_properties = context_impl->properties();
    // The receiver bound to FakeWebNNContext.
    auto success = mojom::CreateContextSuccess::New(
        std::move(remote), std::move(context_properties),
        context_impl->handle(), mojo::ScopedDataPipeProducerHandle(),
        mojo::ScopedDataPipeConsumerHandle());
    std::move(callback).Run(
        mojom::CreateContextResult::NewSuccess(std::move(success)));
    return context_impl;
  }
};

}  // namespace

class WebNNGraphBuilderImplTest : public testing::Test {
 public:
  WebNNGraphBuilderImplTest(const WebNNGraphBuilderImplTest&) = delete;
  WebNNGraphBuilderImplTest& operator=(const WebNNGraphBuilderImplTest&) =
      delete;

  void SetUp() override {
    WebNNContextProviderImpl::SetBackendForTesting(&backend_for_testing_);

    webnn_test_environment_.BindWebNNContextProvider(
        provider_remote_.BindNewPipeAndPassReceiver());

    base::test::TestFuture<mojom::CreateContextResultPtr> create_context_future;
    provider_remote_->CreateWebNNContext(mojom::CreateContextOptions::New(),
                                         create_context_future.GetCallback());
    mojom::CreateContextResultPtr create_context_result =
        create_context_future.Take();
    webnn_context_.Bind(
        std::move(create_context_result->get_success()->context_remote));

    webnn_context_->CreateGraphBuilder(
        graph_builder_remote_.BindNewEndpointAndPassReceiver());
  }
  void TearDown() override {
    WebNNContextProviderImpl::SetBackendForTesting(nullptr);
  }

  base::test::TaskEnvironment& task_environment() { return task_environment_; }

  mojo::AssociatedRemote<mojom::WebNNGraphBuilder>& graph_builder_remote() {
    return graph_builder_remote_;
  }

 protected:
  WebNNGraphBuilderImplTest()
      : scoped_feature_list_(
            webnn::mojom::features::kWebMachineLearningNeuralNetwork) {}
  ~WebNNGraphBuilderImplTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::TaskEnvironment task_environment_;

  FakeWebNNBackend backend_for_testing_;

  test::WebNNTestEnvironment webnn_test_environment_;
  mojo::Remote<mojom::WebNNContextProvider> provider_remote_;
  mojo::Remote<mojom::WebNNContext> webnn_context_;
  mojo::AssociatedRemote<mojom::WebNNGraphBuilder> graph_builder_remote_;
};

TEST_F(WebNNGraphBuilderImplTest, CreateGraph) {
  EXPECT_TRUE(graph_builder_remote().is_connected());

  mojom::GraphInfoPtr graph_info = BuildSimpleGraphInfo(graph_builder_remote());

  base::test::TestFuture<
      base::expected<mojom::CreateGraphSuccessPtr, mojom::ErrorPtr>>
      create_graph_future;
  graph_builder_remote()->CreateGraph(std::move(graph_info),
                                      create_graph_future.GetCallback());
  auto create_graph_result = create_graph_future.Take();
  EXPECT_TRUE(create_graph_result.has_value());

  // The remote should disconnect shortly after the future resolves since the
  // `WebNNGraphBuilder` is destroyed shortly after firing its `CreateGraph()`
  // callback.
  task_environment().RunUntilIdle();
  EXPECT_FALSE(graph_builder_remote().is_connected());
}

TEST_F(WebNNGraphBuilderImplTest, CreateGraphTwice) {
  mojom::GraphInfoPtr graph_info = BuildSimpleGraphInfo(graph_builder_remote());

  base::test::TestFuture<
      base::expected<mojom::CreateGraphSuccessPtr, mojom::ErrorPtr>>
      create_graph_future;
  graph_builder_remote()->CreateGraph(CloneGraphInfoForTesting(*graph_info),
                                      create_graph_future.GetCallback());

  // Don't wait for `create_graph_future` to resolve.

  mojo::test::BadMessageObserver bad_message_observer;
  graph_builder_remote()->CreateGraph(std::move(graph_info), base::DoNothing());
  EXPECT_EQ(bad_message_observer.WaitForBadMessage(),
            kBadMessageOnBuiltGraphBuilder);
}

TEST_F(WebNNGraphBuilderImplTest, CreateGraphWithConstant) {
  const std::array<float, 6> kConstantData{3.0, 4.0, 5.0, 6.0, 7.0, 8.0};

  GraphInfoBuilder builder(graph_builder_remote());
  OperandId constant_operand_id = builder.BuildConstant(
      /*dimensions=*/{2, 3}, OperandDataType::kFloat32,
      base::as_byte_span(base::allow_nonunique_obj, kConstantData));
  OperandId output_operand_id = builder.BuildOutput(
      "output", /*dimensions=*/{2, 3}, OperandDataType::kFloat32);
  builder.BuildClamp(constant_operand_id, output_operand_id, /*min_value=*/5.0,
                     /*max_value=*/7.0);
  EXPECT_TRUE(builder.IsValidGraphForTesting(GetContextPropertiesForTesting()));

  base::test::TestFuture<
      base::expected<mojom::CreateGraphSuccessPtr, mojom::ErrorPtr>>
      create_graph_future;
  graph_builder_remote()->CreateGraph(builder.TakeGraphInfo(),
                                      create_graph_future.GetCallback());
  auto create_graph_result = create_graph_future.Take();
  EXPECT_TRUE(create_graph_result.has_value());
}

TEST_F(WebNNGraphBuilderImplTest, CreatePendingConstantOnBuiltGraph) {
  mojom::GraphInfoPtr graph_info = BuildSimpleGraphInfo(graph_builder_remote());

  base::test::TestFuture<
      base::expected<mojom::CreateGraphSuccessPtr, mojom::ErrorPtr>>
      create_graph_future;
  graph_builder_remote()->CreateGraph(CloneGraphInfoForTesting(*graph_info),
                                      create_graph_future.GetCallback());

  // Don't wait for `create_graph_future` to resolve.

  const std::array<float, 6> kConstantData{3.0, 4.0, 5.0, 6.0, 7.0, 8.0};

  mojo::test::BadMessageObserver bad_message_observer;
  graph_builder_remote()->CreatePendingConstant(
      blink::WebNNPendingConstantToken(), OperandDataType::kFloat32,
      mojo_base::BigBuffer(
          base::as_byte_span(base::allow_nonunique_obj, kConstantData)));
  EXPECT_EQ(bad_message_observer.WaitForBadMessage(),
            kBadMessageOnBuiltGraphBuilder);
}

TEST_F(WebNNGraphBuilderImplTest, CreateInvalidPendingConstantDuplicate) {
  const std::array<float, 6> kConstantData{3.0, 4.0, 5.0, 6.0, 7.0, 8.0};

  blink::WebNNPendingConstantToken token;

  graph_builder_remote()->CreatePendingConstant(
      token, OperandDataType::kFloat32,
      mojo_base::BigBuffer(
          base::as_byte_span(base::allow_nonunique_obj, kConstantData)));

  // Create another pending constant with the same token.
  mojo::test::BadMessageObserver bad_message_observer;
  graph_builder_remote()->CreatePendingConstant(
      token, OperandDataType::kFloat32,
      mojo_base::BigBuffer(
          base::as_byte_span(base::allow_nonunique_obj, kConstantData)));
  EXPECT_EQ(bad_message_observer.WaitForBadMessage(),
            kBadMessageInvalidPendingConstant);
}

TEST_F(WebNNGraphBuilderImplTest, CreateInvalidPendingConstantEmpty) {
  mojo::test::BadMessageObserver bad_message_observer;
  graph_builder_remote()->CreatePendingConstant(
      blink::WebNNPendingConstantToken(), OperandDataType::kFloat32,
      // Data buffer cannot be empty.
      mojo_base::BigBuffer(0));
  EXPECT_EQ(bad_message_observer.WaitForBadMessage(),
            kBadMessageInvalidPendingConstant);
}

TEST_F(WebNNGraphBuilderImplTest, CreateInvalidPendingConstantBadType) {
  mojo::test::BadMessageObserver bad_message_observer;
  graph_builder_remote()->CreatePendingConstant(
      blink::WebNNPendingConstantToken(), OperandDataType::kFloat32,
      // The size of the data buffer must be a multiple of the 4 since the data
      // type has 4-byte elements.
      mojo_base::BigBuffer(6));
  EXPECT_EQ(bad_message_observer.WaitForBadMessage(),
            kBadMessageInvalidPendingConstant);
}

TEST_F(WebNNGraphBuilderImplTest, CreateInvalidGraphForTensorByteLengthLimit) {
  const std::vector<uint32_t> large_tensor_shape = {
      base::checked_cast<uint32_t>(std::numeric_limits<int32_t>::max() / 4), 2};

  GraphInfoBuilder builder(graph_builder_remote());
  OperandId input_operand_id = builder.BuildInput("input", large_tensor_shape,
                                                  OperandDataType::kFloat32);
  OperandId output_operand_id = builder.BuildOutput(
      "output", large_tensor_shape, OperandDataType::kFloat32);
  builder.BuildClamp(input_operand_id, output_operand_id, /*min_value=*/0.0,
                     /*max_value=*/1.0);
  EXPECT_FALSE(
      builder.IsValidGraphForTesting(GetContextPropertiesForTesting()));
}

}  // namespace webnn
