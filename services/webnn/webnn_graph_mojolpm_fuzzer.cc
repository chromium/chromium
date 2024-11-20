// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>

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
#include "content/test/fuzzer/mojolpm_fuzzer_support.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/webnn/public/mojom/features.mojom-features.h"
#include "services/webnn/public/mojom/webnn_context.mojom.h"
#include "services/webnn/public/mojom/webnn_context_provider.mojom-mojolpm.h"
#include "services/webnn/public/mojom/webnn_context_provider.mojom.h"
#include "services/webnn/public/mojom/webnn_graph.mojom-mojolpm.h"
#include "services/webnn/public/mojom/webnn_graph.mojom.h"
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

void BuildGraph(webnn::mojom::GraphInfoPtr graph_info,
                webnn::mojom::CreateContextOptions::Device device) {
  mojo::Remote<webnn::mojom::WebNNContextProvider> webnn_provider_remote;
  mojo::Remote<webnn::mojom::WebNNContext> webnn_context_remote;
  mojo::AssociatedRemote<webnn::mojom::WebNNGraphBuilder>
      webnn_graph_builder_remote;

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

  base::test::TestFuture<webnn::mojom::CreateGraphResultPtr>
      create_graph_future;
  webnn_graph_builder_remote.set_disconnect_handler(
      base::BindLambdaForTesting([&] {
        create_graph_future.SetValue(webnn::mojom::CreateGraphResult::NewError(
            webnn::mojom::Error::New(webnn::mojom::Error::Code::kUnknownError,
                                     "Failed to create graph.")));
      }));

  webnn_graph_builder_remote->CreateGraph(std::move(graph_info),
                                          create_graph_future.GetCallback());
  ASSERT_TRUE(create_graph_future.Wait());
}

class WebnnGraphLPMFuzzer {
 public:
  explicit WebnnGraphLPMFuzzer(
      const services::fuzzing::webnn_graph::proto::Testcase& testcase)
      : testcase_(testcase) {}

  void NextAction() {
    const auto& action = testcase_->actions(action_index_);
    ++action_index_;
    const auto& create_graph = action.create_graph();
    auto graph_info_ptr = webnn::mojom::GraphInfo::New();
    mojolpm::FromProto(create_graph.graph_info(), graph_info_ptr);
    webnn::mojom::CreateContextOptions::Device device;
    mojolpm::FromProto(action.device(), device);
    BuildGraph(std::move(graph_info_ptr), device);
  }

  bool IsFinished() { return action_index_ >= testcase_->actions_size(); }

 private:
  const raw_ref<const services::fuzzing::webnn_graph::proto::Testcase>
      testcase_;
  int action_index_ = 0;
};

DEFINE_BINARY_PROTO_FUZZER(
    const services::fuzzing::webnn_graph::proto::Testcase& testcase) {
  WebnnGraphLPMFuzzer webnn_graph_fuzzer_instance(testcase);
  while (!webnn_graph_fuzzer_instance.IsFinished()) {
    webnn_graph_fuzzer_instance.NextAction();
  }
}

}  // namespace
