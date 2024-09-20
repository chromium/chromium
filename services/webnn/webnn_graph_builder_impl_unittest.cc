// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/webnn_graph_builder_impl.h"

#include <memory>

#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/notimplemented.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "services/webnn/error.h"
#include "services/webnn/public/mojom/features.mojom-features.h"
#include "services/webnn/webnn_constant_operand.h"
#include "services/webnn/webnn_context_impl.h"
#include "services/webnn/webnn_context_provider_impl.h"
#include "services/webnn/webnn_graph_impl.h"
#include "services/webnn/webnn_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace webnn {

namespace {

mojom::GraphInfoPtr BuildSimpleGraphInfo() {
  // Build a simple graph.
  GraphInfoBuilder builder;
  uint64_t input_operand_id = builder.BuildInput("input", /*dimensions=*/{2, 3},
                                                 OperandDataType::kFloat32);
  uint64_t output_operand_id = builder.BuildOutput(
      "output", /*dimensions=*/{2, 3}, OperandDataType::kFloat32);
  builder.BuildClamp(input_operand_id, output_operand_id, /*min_value=*/0.0,
                     /*max_value=*/1.0);
  EXPECT_TRUE(WebNNGraphBuilderImpl::IsValidForTesting(
      GetContextPropertiesForTesting(), builder.GetGraphInfo()));

  return builder.TakeGraphInfo();
}

// A fake WebNNGraph Mojo interface implementation that binds a pipe for
// computing graph message.
class FakeWebNNGraphImpl final : public WebNNGraphImpl {
 public:
  FakeWebNNGraphImpl(WebNNContextImpl* context,
                     ComputeResourceInfo compute_resource_info)
      : WebNNGraphImpl(context, std::move(compute_resource_info)) {}
  ~FakeWebNNGraphImpl() override = default;

 private:
  void ComputeImpl(base::flat_map<std::string, mojo_base::BigBuffer> inputs,
                   mojom::WebNNGraph::ComputeCallback callback) override {
    NOTIMPLEMENTED();
  }

  void DispatchImpl(
      const base::flat_map<std::string_view, WebNNTensorImpl*>& named_inputs,
      const base::flat_map<std::string_view, WebNNTensorImpl*>& named_outputs)
      override {
    NOTIMPLEMENTED();
  }
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
      /*constant_operands*/,
      CreateGraphImplCallback callback) override {
    // Asynchronously resolve `callback` so there's an opportunity for
    // subsequent messages to be (illegally) sent from the `WebNNGraphBuilder`
    // remote before it's disconnected.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](base::WeakPtr<WebNNContextImpl> context,
               WebNNGraphImpl::ComputeResourceInfo compute_resource_info,
               CreateGraphImplCallback callback) {
              CHECK(context);
              std::move(callback).Run(std::make_unique<FakeWebNNGraphImpl>(
                  context.get(), std::move(compute_resource_info)));
            },
            AsWeakPtr(), std::move(compute_resource_info),
            std::move(callback)));
  }

  void CreateTensorImpl(
      mojo::PendingAssociatedReceiver<mojom::WebNNTensor> receiver,
      mojom::TensorInfoPtr tensor_info,
      CreateTensorImplCallback callback) override {
    NOTIMPLEMENTED();
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

}  // namespace

class WebNNGraphBuilderImplTest : public testing::Test {
 public:
  WebNNGraphBuilderImplTest(const WebNNGraphBuilderImplTest&) = delete;
  WebNNGraphBuilderImplTest& operator=(const WebNNGraphBuilderImplTest&) =
      delete;

  void SetUp() override {
    WebNNContextProviderImpl::SetBackendForTesting(&backend_for_testing_);

    WebNNContextProviderImpl::CreateForTesting(
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

  mojo::Remote<mojom::WebNNContextProvider> provider_remote_;
  mojo::Remote<mojom::WebNNContext> webnn_context_;
  mojo::AssociatedRemote<mojom::WebNNGraphBuilder> graph_builder_remote_;
};

TEST_F(WebNNGraphBuilderImplTest, CreateGraph) {
  EXPECT_TRUE(graph_builder_remote().is_connected());

  base::test::TestFuture<mojom::CreateGraphResultPtr> create_graph_future;
  graph_builder_remote()->CreateGraph(BuildSimpleGraphInfo(),
                                      create_graph_future.GetCallback());
  mojom::CreateGraphResultPtr create_graph_result = create_graph_future.Take();
  EXPECT_TRUE(create_graph_result->is_graph_remote());

  // The remote should disconnect shortly after the future resolves since the
  // `WebNNGraphBuilder` is destroyed shortly after firing its `CreateGraph()`
  // callback.
  task_environment().RunUntilIdle();
  EXPECT_FALSE(graph_builder_remote().is_connected());
}

TEST_F(WebNNGraphBuilderImplTest, CreateGraphTwice) {
  base::test::TestFuture<mojom::CreateGraphResultPtr> create_graph_future;
  graph_builder_remote()->CreateGraph(BuildSimpleGraphInfo(),
                                      create_graph_future.GetCallback());

  // Don't wait for `create_graph_future` to resolve.

  mojo::test::BadMessageObserver bad_message_observer;
  graph_builder_remote()->CreateGraph(BuildSimpleGraphInfo(),
                                      base::DoNothing());
  EXPECT_EQ(bad_message_observer.WaitForBadMessage(),
            kBadMessageOnBuiltGraphBuilder);
}

}  // namespace webnn
