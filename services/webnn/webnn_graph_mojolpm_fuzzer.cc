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

base::test::TaskEnvironment& GetEnvironment() {
  return *init_globals->task_environment;
}

#if BUILDFLAG(IS_WIN)
scoped_refptr<webnn::dml::Adapter> GetAdapter() {
  return init_globals->adapter;
}
#endif

webnn::mojom::GraphInfoPtr CloneGraphInfo(
    const webnn::mojom::GraphInfo& graph_info) {
  webnn::mojom::GraphInfoPtr cloned_graph_info = webnn::mojom::GraphInfo::New();

  cloned_graph_info->id_to_operand_map.reserve(
      graph_info.id_to_operand_map.size());
  for (auto& [operand_id, operand_info] : graph_info.id_to_operand_map) {
    cloned_graph_info->id_to_operand_map[operand_id] = operand_info.Clone();
  }
  cloned_graph_info->input_operands = graph_info.input_operands;
  cloned_graph_info->output_operands = graph_info.output_operands;

  cloned_graph_info->operations.reserve(graph_info.operations.size());
  for (auto& operation : graph_info.operations) {
    cloned_graph_info->operations.push_back(operation.Clone());
  }

  cloned_graph_info->constant_id_to_buffer_map.reserve(
      graph_info.constant_id_to_buffer_map.size());
  for (const auto& [constant_id, buffer] :
       graph_info.constant_id_to_buffer_map) {
    cloned_graph_info->constant_id_to_buffer_map[constant_id] = buffer.Clone();
  }
  return cloned_graph_info;
}

scoped_refptr<base::SingleThreadTaskRunner> GetFuzzerTaskRunner() {
  return GetEnvironment().GetMainThreadTaskRunner();
}

class WebnnGraphLPMFuzzer {
 public:
  explicit WebnnGraphLPMFuzzer(
      const services::fuzzing::webnn_graph::proto::Testcase& testcase)
      : testcase_(testcase) {}

  void NextAction() {
    const auto& action = testcase_->actions(action_index_);
    const auto& create_graph = action.create_graph();

    auto graph_info_ptr = webnn::mojom::GraphInfo::New();
    mojolpm::FromProto(create_graph.graph_info(), graph_info_ptr);

#if BUILDFLAG(IS_POSIX)
    auto coreml_properties =
        webnn::WebNNContextImpl::IntersectWithBaseProperties(
            webnn::coreml::GraphBuilderCoreml::GetContextProperties());
    if (webnn::WebNNGraphBuilderImpl::ValidateGraph(coreml_properties,
                                                    *graph_info_ptr)
            .has_value()) {
      // Test the Core ML graph builder.
      base::ScopedTempDir temp_dir;
      CHECK(temp_dir.CreateUniqueTempDir());

      auto cloned_graph_info_ptr = CloneGraphInfo(*graph_info_ptr);
      auto constant_operands =
          webnn::WebNNGraphBuilderImpl::TakeConstants(*cloned_graph_info_ptr);
      auto coreml_graph_builder =
          webnn::coreml::GraphBuilderCoreml::CreateAndBuild(
              *cloned_graph_info_ptr, std::move(coreml_properties),
              constant_operands, temp_dir.GetPath());
    }
#endif

#if BUILDFLAG(IS_WIN)
    CHECK(GetAdapter());
    auto dml_properties = webnn::WebNNContextImpl::IntersectWithBaseProperties(
        webnn::dml::ContextImplDml::GetProperties(
            GetAdapter()->max_supported_feature_level()));
    if (webnn::WebNNGraphBuilderImpl::ValidateGraph(dml_properties,
                                                    *graph_info_ptr)
            .has_value()) {
      // Graph compilation relies on IDMLDevice1::CompileGraph introduced in
      // DirectML version 1.2 (DML_FEATURE_LEVEL_2_1).
      CHECK(GetAdapter()->IsDMLDeviceCompileGraphSupportedForTesting());

      auto cloned_graph_info_ptr = CloneGraphInfo(*graph_info_ptr);
      auto constant_operands =
          webnn::WebNNGraphBuilderImpl::TakeConstants(*cloned_graph_info_ptr);

      webnn::dml::GraphBuilderDml graph_builder(GetAdapter()->dml_device());
      std::unordered_map<uint64_t, uint32_t> constant_id_to_input_index_map;
      webnn::dml::GraphImplDml::GraphBufferBindingInfo
          graph_buffer_binding_info;
      auto create_operator_result =
          webnn::dml::GraphImplDml::CreateAndBuildInternal(
              dml_properties, GetAdapter(), cloned_graph_info_ptr,
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
    if (webnn::WebNNGraphBuilderImpl::ValidateGraph(tflite_properties,
                                                    *graph_info_ptr)
            .has_value()) {
      // Test the TFLite graph builder.
      //
      // No need to clone `graph_info_ptr` since this is the last use.
      auto constant_operands =
          webnn::WebNNGraphBuilderImpl::TakeConstants(*graph_info_ptr);
      auto flatbuffer = webnn::tflite::GraphBuilderTflite::CreateAndBuild(
          std::move(tflite_properties), *graph_info_ptr, constant_operands);
    }

    ++action_index_;
  }

  bool IsFinished() { return action_index_ >= testcase_->actions_size(); }

 private:
  const raw_ref<const services::fuzzing::webnn_graph::proto::Testcase>
      testcase_;
  int action_index_ = 0;
};

void NextAction(WebnnGraphLPMFuzzer* testcase,
                base::OnceClosure fuzzer_run_loop) {
  if (!testcase->IsFinished()) {
    testcase->NextAction();
    GetFuzzerTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(NextAction, base::Unretained(testcase),
                                  std::move(fuzzer_run_loop)));
  } else {
    std::move(fuzzer_run_loop).Run();
  }
}

void RunTestcase(WebnnGraphLPMFuzzer* testcase) {
  base::RunLoop fuzzer_run_loop;
  GetFuzzerTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(NextAction, base::Unretained(testcase),
                                fuzzer_run_loop.QuitClosure()));
  // Make sure that all callbacks have completed.
  constexpr base::TimeDelta kTimeout = base::Seconds(5);
  GetEnvironment().FastForwardBy(kTimeout);
  fuzzer_run_loop.Run();
}

DEFINE_BINARY_PROTO_FUZZER(
    const services::fuzzing::webnn_graph::proto::Testcase& testcase) {
  if (!testcase.actions_size()) {
    return;
  }

  WebnnGraphLPMFuzzer webnn_graph_fuzzer_instance(testcase);
  base::RunLoop main_run_loop;

  GetFuzzerTaskRunner()->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(RunTestcase,
                     base::Unretained(&webnn_graph_fuzzer_instance)),
      main_run_loop.QuitClosure());
  main_run_loop.Run();
}

}  // namespace
