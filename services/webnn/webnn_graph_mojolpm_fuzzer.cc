// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>

#include "base/command_line.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/raw_ref.h"
#include "base/notreached.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/allow_check_is_test_for_testing.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "content/test/fuzzer/mojolpm_fuzzer_support.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/webnn/public/mojom/webnn_graph.mojom-mojolpm.h"
#include "services/webnn/public/mojom/webnn_graph.mojom.h"
#include "services/webnn/tflite/context_impl_tflite.h"
#include "services/webnn/tflite/graph_builder_tflite.h"
#include "services/webnn/webnn_constant_operand.h"
#include "services/webnn/webnn_context_impl.h"
#include "services/webnn/webnn_graph_builder_impl.h"
#include "services/webnn/webnn_graph_impl.h"
#include "services/webnn/webnn_graph_mojolpm_fuzzer.pb.h"
#include "third_party/libprotobuf-mutator/src/src/libfuzzer/libfuzzer_macro.h"

#if BUILDFLAG(IS_WIN)
#include "services/webnn/dml/adapter.h"
#include "services/webnn/dml/context_impl_dml.h"
#include "services/webnn/dml/graph_builder_dml.h"
#include "services/webnn/dml/graph_impl_dml.h"
#endif

#if BUILDFLAG(IS_POSIX)
#include "services/webnn/coreml/graph_builder_coreml.h"
#endif

namespace {
struct InitGlobals {
  InitGlobals() {
    mojo::core::Init();
    bool success = base::CommandLine::Init(0, nullptr);
    CHECK(success);

    TestTimeouts::Initialize();

    base::test::AllowCheckIsTestForTesting();

    task_environment = std::make_unique<base::test::TaskEnvironment>(
        base::test::TaskEnvironment::MainThreadType::DEFAULT,
        base::test::TaskEnvironment::TimeSource::MOCK_TIME);

#if BUILDFLAG(IS_WIN)
    auto adapter_creation_result =
        webnn::dml::Adapter::GetGpuInstanceForTesting();
    if (adapter_creation_result.has_value()) {
      adapter = adapter_creation_result.value();
    }
#endif
  }

  std::unique_ptr<base::test::TaskEnvironment> task_environment;
#if BUILDFLAG(IS_WIN)
  scoped_refptr<webnn::dml::Adapter> adapter;
#endif
};

InitGlobals* init_globals = new InitGlobals();


#if BUILDFLAG(IS_WIN)
scoped_refptr<webnn::dml::Adapter> GetAdapter() {
  return init_globals->adapter;
}
#endif

class WebnnGraphLPMFuzzer {
 public:
  explicit WebnnGraphLPMFuzzer(
      const services::fuzzing::webnn_graph::proto::Testcase& testcase)
      : testcase_(testcase) {}

  void NextAction() {
    const auto& action = testcase_->actions(action_index_);
    const auto& create_graph = action.create_graph();

#if BUILDFLAG(IS_POSIX)
    auto graph_info_ptr_coreml = webnn::mojom::GraphInfo::New();
    mojolpm::FromProto(create_graph.graph_info(), graph_info_ptr_coreml);
    auto coreml_properties =
        webnn::WebNNContextImpl::IntersectWithBaseProperties(
            webnn::coreml::GraphBuilderCoreml::GetContextProperties());
    if (webnn::WebNNGraphBuilderImpl::ValidateGraph(coreml_properties,
                                                    *graph_info_ptr_coreml)
            .has_value()) {
      // Test the Core ML graph builder.
      base::ScopedTempDir temp_dir;
      CHECK(temp_dir.CreateUniqueTempDir());

      auto constant_operands =
          webnn::WebNNGraphBuilderImpl::TakeConstants(*graph_info_ptr_coreml);
      auto coreml_graph_builder =
          webnn::coreml::GraphBuilderCoreml::CreateAndBuild(
              *graph_info_ptr_coreml, std::move(coreml_properties),
              constant_operands, temp_dir.GetPath());
    }
#endif

#if BUILDFLAG(IS_WIN)
    CHECK(GetAdapter());
    auto dml_properties = webnn::WebNNContextImpl::IntersectWithBaseProperties(
        webnn::dml::ContextImplDml::GetProperties(
            GetAdapter()->max_supported_feature_level()));

    auto graph_info_ptr_dml = webnn::mojom::GraphInfo::New();
    mojolpm::FromProto(create_graph.graph_info(), graph_info_ptr_dml);
    if (webnn::WebNNGraphBuilderImpl::ValidateGraph(dml_properties,
                                                    *graph_info_ptr_dml)
            .has_value()) {
      // Graph compilation relies on IDMLDevice1::CompileGraph introduced in
      // DirectML version 1.2 (DML_FEATURE_LEVEL_2_1).
      CHECK(GetAdapter()->IsDMLDeviceCompileGraphSupportedForTesting());

      auto constant_operands =
          webnn::WebNNGraphBuilderImpl::TakeConstants(*graph_info_ptr_dml);

      webnn::dml::GraphBuilderDml graph_builder(GetAdapter()->dml_device());
      std::unordered_map<uint64_t, uint32_t> constant_id_to_input_index_map;
      webnn::dml::GraphImplDml::GraphBufferBindingInfo
          graph_buffer_binding_info;
      auto create_operator_result =
          webnn::dml::GraphImplDml::CreateAndBuildInternal(
              dml_properties, GetAdapter(), graph_info_ptr_dml,
              constant_operands, graph_builder, constant_id_to_input_index_map,
              graph_buffer_binding_info);
      if (create_operator_result.has_value()) {
        auto dml_graph_builder = graph_builder.Compile(DML_EXECUTION_FLAG_NONE);
      }
    }
#endif

    auto tflite_properties =
        webnn::WebNNContextImpl::IntersectWithBaseProperties(
            webnn::tflite::GraphBuilderTflite::GetContextProperties());
    auto graph_info_ptr_tflite = webnn::mojom::GraphInfo::New();
    mojolpm::FromProto(create_graph.graph_info(), graph_info_ptr_tflite);
    if (webnn::WebNNGraphBuilderImpl::ValidateGraph(tflite_properties,
                                                    *graph_info_ptr_tflite)
            .has_value()) {
      // Test the TFLite graph builder.
      auto constant_operands =
          webnn::WebNNGraphBuilderImpl::TakeConstants(*graph_info_ptr_tflite);
      auto flatbuffer = webnn::tflite::GraphBuilderTflite::CreateAndBuild(
          std::move(tflite_properties), *graph_info_ptr_tflite,
          constant_operands);
    }
    ++action_index_;
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
