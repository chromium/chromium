// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>
#include <random>

#include "base/command_line.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/raw_ref.h"
#include "base/notreached.h"
#include "base/test/allow_check_is_test_for_testing.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/test/test_timeouts.h"
#include "base/types/fixed_array.h"
#include "base/types/zip.h"
#include "content/test/fuzzer/mojolpm_fuzzer_support.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/webnn/public/cpp/ml_tensor_usage.h"
#include "services/webnn/public/mojom/features.mojom-features.h"
#include "services/webnn/public/mojom/webnn_context.mojom.h"
#include "services/webnn/public/mojom/webnn_context_provider.mojom-mojolpm.h"
#include "services/webnn/public/mojom/webnn_context_provider.mojom.h"
#include "services/webnn/public/mojom/webnn_graph.mojom-mojolpm.h"
#include "services/webnn/public/mojom/webnn_graph.mojom.h"
#include "services/webnn/public/mojom/webnn_tensor.mojom.h"
#include "services/webnn/webnn_context_impl.h"
#include "services/webnn/webnn_context_provider_impl.h"
#include "services/webnn/webnn_graph_builder_impl.h"
#include "services/webnn/webnn_graph_impl.h"
#include "services/webnn/webnn_graph_mojolpm_fuzzer.pb.h"
#include "third_party/libprotobuf-mutator/src/src/libfuzzer/libfuzzer_macro.h"

namespace {

struct InitGlobals {
  InitGlobals()
      : scoped_feature_list_(
            webnn::mojom::features::kWebMachineLearningNeuralNetwork) {
    mojo::core::Init();
    bool success = base::CommandLine::Init(0, nullptr);
    CHECK(success);

    TestTimeouts::Initialize();

    base::test::AllowCheckIsTestForTesting();

    task_environment = std::make_unique<base::test::TaskEnvironment>(
        base::test::TaskEnvironment::MainThreadType::DEFAULT,
        base::test::TaskEnvironment::TimeSource::MOCK_TIME);
  }

  std::unique_ptr<base::test::TaskEnvironment> task_environment;
  base::test::ScopedFeatureList scoped_feature_list_;
};

InitGlobals* init_globals = new InitGlobals();

mojo_base::BigBuffer GenerateBytes(uint32_t seed, size_t byte_size) {
  // base::RandBytes doesn't allow seeding, so use std::mt19937 for
  // reproducible test data.
  std::mt19937 generator(seed);
  std::uniform_int_distribution<uint8_t> distribution(0, 255);

  mojo_base::BigBuffer buffer(byte_size);
  for (auto& data : buffer) {
    data = distribution(generator);
  }
  return buffer;
}

void BuildGraph(const mojolpm::webnn::mojom::GraphInfo& graph_info_proto,
                webnn::mojom::Device device,
                uint32_t seed) {
  mojo::Remote<webnn::mojom::WebNNContextProvider> webnn_provider_remote;
  mojo::Remote<webnn::mojom::WebNNContext> webnn_context_remote;
  mojo::AssociatedRemote<webnn::mojom::WebNNGraphBuilder>
      webnn_graph_builder_remote;
  mojo::AssociatedRemote<webnn::mojom::WebNNGraph> webnn_graph_remote;

  webnn::WebNNContextProviderImpl::CreateForTesting(
      webnn_provider_remote.BindNewPipeAndPassReceiver());

  // Create the ContextImpl through context provider.
  base::test::TestFuture<webnn::mojom::CreateContextResultPtr>
      create_context_future;
  webnn_provider_remote->CreateWebNNContext(
      webnn::mojom::CreateContextOptions::New(
          device,
          webnn::mojom::CreateContextOptions::PowerPreference::kDefault),
      create_context_future.GetCallback());
  webnn::mojom::CreateContextResultPtr create_context_result =
      create_context_future.Take();
  if (!create_context_result->is_success()) {
    return;
  }

  webnn_context_remote.Bind(
      std::move(create_context_result->get_success()->context_remote));

  EXPECT_TRUE(webnn_context_remote.is_bound());

  // Create the GraphBuilder through the context.
  webnn_context_remote->CreateGraphBuilder(
      webnn_graph_builder_remote.BindNewEndpointAndPassReceiver());

  base::test::TestFuture<base::expected<webnn::mojom::CreateGraphSuccessPtr,
                                        webnn::mojom::ErrorPtr>>
      create_graph_future;
  webnn_graph_builder_remote.set_disconnect_handler(
      base::BindLambdaForTesting([&] {
        create_graph_future.SetValue(base::unexpected(
            webnn::mojom::Error::New(webnn::mojom::Error::Code::kUnknownError,
                                     "Failed to create graph.")));
      }));

  auto graph_info = webnn::mojom::GraphInfo::New();
  mojolpm::FromProto(graph_info_proto, graph_info);
  webnn_graph_builder_remote->CreateGraph(std::move(graph_info),
                                          create_graph_future.GetCallback());
  auto create_graph_result = create_graph_future.Take();
  if (!create_graph_result.has_value()) {
    return;
  }
  webnn_graph_remote.Bind(std::move(create_graph_result.value()->graph_remote));

  // Get graph_info again for tensor operations.
  graph_info = webnn::mojom::GraphInfo::New();
  mojolpm::FromProto(graph_info_proto, graph_info);

  // Create input tensors.
  base::FixedArray<mojo::AssociatedRemote<webnn::mojom::WebNNTensor>>
      input_remotes(graph_info->input_operands.size());

  std::vector<std::pair<std::string, blink::WebNNTensorToken>>
      named_input_handles;
  named_input_handles.reserve(graph_info->input_operands.size());

  for (auto [operand_id, remote] :
       base::zip(graph_info->input_operands, input_remotes)) {
    const webnn::mojom::Operand& operand =
        *graph_info->operands.at(operand_id.value());
    EXPECT_TRUE(operand.name.has_value());

    auto tensor_info = webnn::mojom::TensorInfo::New(
        operand.descriptor,
        webnn::MLTensorUsage{webnn::MLTensorUsageFlags::kWrite});

    base::test::TestFuture<webnn::mojom::CreateTensorResultPtr>
        create_tensor_future;
    webnn_context_remote->CreateTensor(std::move(tensor_info),
                                       mojo_base::BigBuffer(0),
                                       create_tensor_future.GetCallback());
    webnn::mojom::CreateTensorResultPtr create_tensor_result =
        create_tensor_future.Take();
    if (!create_tensor_result->is_success()) {
      return;
    }
    remote.Bind(std::move(create_tensor_result->get_success()->tensor_remote));

    named_input_handles.emplace_back(
        *operand.name, create_tensor_result->get_success()->tensor_handle);
    remote->WriteTensor(
        GenerateBytes(seed, operand.descriptor.PackedByteLength()));
  }

  // Create output tensors.
  base::FixedArray<mojo::AssociatedRemote<webnn::mojom::WebNNTensor>>
      output_remotes(graph_info->output_operands.size());

  std::vector<std::pair<std::string, blink::WebNNTensorToken>>
      named_output_handles;
  named_output_handles.reserve(graph_info->output_operands.size());

  for (auto&& [operand_id, remote] :
       base::zip(graph_info->output_operands, output_remotes)) {
    const webnn::mojom::Operand& operand =
        *graph_info->operands.at(operand_id.value());
    EXPECT_TRUE(operand.name.has_value());

    auto tensor_info = webnn::mojom::TensorInfo::New(
        operand.descriptor,
        webnn::MLTensorUsage{webnn::MLTensorUsageFlags::kRead});

    base::test::TestFuture<webnn::mojom::CreateTensorResultPtr>
        create_tensor_future;
    webnn_context_remote->CreateTensor(std::move(tensor_info),
                                       mojo_base::BigBuffer(0),
                                       create_tensor_future.GetCallback());
    webnn::mojom::CreateTensorResultPtr create_tensor_result =
        create_tensor_future.Take();
    if (!create_tensor_result->is_success()) {
      return;
    }
    remote.Bind(std::move(create_tensor_result->get_success()->tensor_remote));

    named_output_handles.emplace_back(
        *operand.name, create_tensor_result->get_success()->tensor_handle);
  }

  webnn_graph_remote->Dispatch(named_input_handles, named_output_handles);

  // Wait for reading all output data.
  for (auto& remote : output_remotes) {
    base::test::TestFuture<webnn::mojom::ReadTensorResultPtr>
        read_tensor_future;
    remote->ReadTensor(read_tensor_future.GetCallback());
    EXPECT_TRUE(read_tensor_future.Wait());
  }
}

class WebnnGraphLPMFuzzer {
 public:
  explicit WebnnGraphLPMFuzzer(
      const services::fuzzing::webnn_graph::proto::Testcase& testcase)
      : testcase_(testcase) {
    webnn::WebNNContextProviderImpl::CreateForTesting(
        provider_remote_.BindNewPipeAndPassReceiver());

    base::test::TestFuture<webnn::mojom::CreateContextResultPtr>
        create_context_future;
    provider_remote_->CreateWebNNContext(
        webnn::mojom::CreateContextOptions::New(),
        create_context_future.GetCallback());
    webnn::mojom::CreateContextResultPtr create_context_result =
        create_context_future.Take();
    webnn_context_.Bind(
        std::move(create_context_result->get_success()->context_remote));
  }

  void NextAction() {
    const auto& action = testcase_->actions(action_index_);
    ++action_index_;
    const auto& create_graph = action.create_graph();

    webnn::mojom::Device device;
    mojolpm::FromProto(action.device(), device);
    BuildGraph(create_graph.graph_info(), device,
               testcase_->seed_for_input_data());
  }

  // Cap the number of actions at 100 to avoid timeouts.
  bool IsFinished() {
    return action_index_ > 100 || action_index_ >= testcase_->actions_size();
  }

 private:
  const raw_ref<const services::fuzzing::webnn_graph::proto::Testcase>
      testcase_;
  int action_index_ = 0;

  mojo::Remote<webnn::mojom::WebNNContextProvider> provider_remote_;
  mojo::Remote<webnn::mojom::WebNNContext> webnn_context_;
};

DEFINE_BINARY_PROTO_FUZZER(
    const services::fuzzing::webnn_graph::proto::Testcase& testcase) {
  WebnnGraphLPMFuzzer webnn_graph_fuzzer_instance(testcase);
  while (!webnn_graph_fuzzer_instance.IsFinished()) {
    webnn_graph_fuzzer_instance.NextAction();
  }
}

}  // namespace
