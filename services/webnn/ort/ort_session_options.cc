// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/ort/ort_session_options.h"

#include "base/command_line.h"
#include "base/strings/stringprintf.h"
#include "services/webnn/ort/ort_status.h"
#include "services/webnn/ort/platform_functions_ort.h"
#include "services/webnn/public/cpp/webnn_trace.h"
#include "services/webnn/public/mojom/webnn_device.mojom.h"
#include "services/webnn/public/mojom/webnn_error.mojom.h"
#include "services/webnn/webnn_switches.h"
#include "third_party/onnxruntime_headers/src/include/onnxruntime/core/session/onnxruntime_session_options_config_keys.h"

namespace webnn::ort {

namespace {

// Helper function to convert a string to GraphOptimizationLevel enum. Return
// nullopt for invalid input to let ORT decide the optimization level.
std::optional<GraphOptimizationLevel> StringToOrtGraphOptimizationLevel(
    std::string_view graph_optimization_level) {
  if (graph_optimization_level == "DISABLE_ALL") {
    return ORT_DISABLE_ALL;
  } else if (graph_optimization_level == "BASIC") {
    return ORT_ENABLE_BASIC;
  } else if (graph_optimization_level == "EXTENDED") {
    return ORT_ENABLE_EXTENDED;
  } else if (graph_optimization_level == "ALL") {
    return ORT_ENABLE_ALL;
  }

  LOG(WARNING) << "[WebNN] Unrecognized graph optimization level: "
               << graph_optimization_level
               << ". Supported values: DISABLE_ALL, BASIC, EXTENDED, ALL. "
               << "Letting ORT decide the optimization level.";
  return std::nullopt;
}

}  // namespace

// static
scoped_refptr<SessionOptions> SessionOptions::Create(
    mojom::Device device_type) {
  ScopedTrace scoped_trace("SessionOptions::Create");

  scoped_trace.AddStep("Create session options");
  const OrtApi* ort_api = PlatformFunctions::GetInstance()->ort_api();
  ScopedOrtSessionOptions session_options;
  CHECK_STATUS(ort_api->CreateSessionOptions(
      ScopedOrtSessionOptions::Receiver(session_options).get()));

  // TODO(crbug.com/425487285): Map WebNN power preference to ORT auto EP
  // selection policy
  OrtExecutionProviderDevicePolicy device_policy;
  switch (device_type) {
    case mojom::Device::kCpu:
      device_policy = OrtExecutionProviderDevicePolicy::
          OrtExecutionProviderDevicePolicy_PREFER_CPU;
      break;
    case mojom::Device::kGpu:
      device_policy = OrtExecutionProviderDevicePolicy::
          OrtExecutionProviderDevicePolicy_PREFER_GPU;
      break;
    case mojom::Device::kNpu:
      device_policy = OrtExecutionProviderDevicePolicy::
          OrtExecutionProviderDevicePolicy_PREFER_NPU;
      break;
  }
  CHECK_STATUS(ort_api->SessionOptionsSetEpSelectionPolicy(
      session_options.get(), device_policy));

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kWebNNOrtDumpModel)) {
    static uint64_t dump_count = 0;
    base::FilePath dump_directory =
        base::CommandLine::ForCurrentProcess()->GetSwitchValuePath(
            switches::kWebNNOrtDumpModel);
    base::FilePath dump_path = dump_directory.AppendASCII(
        base::StringPrintf("model%d.onnx", dump_count++));
    CHECK_STATUS(ort_api->SetOptimizedModelFilePath(session_options.get(),
                                                    dump_path.value().c_str()));
  }

  // Enable strict shape type inference check. All inconsistencies encountered
  // will expose errors during session creation. For example, if the graph
  // output shape set by WebNN is different from ONNX shape inference result,
  // the session creation will fail.
  CHECK_STATUS(ort_api->AddSessionConfigEntry(
      session_options.get(),
      /*config_key=*/kOrtSessionOptionsConfigStrictShapeTypeInference,
      /*config_value=*/"1"));

  // Only set graph optimization level if user provides a valid input.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kWebNNOrtGraphOptimizationLevel)) {
    std::string user_graph_optimization_level =
        base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            switches::kWebNNOrtGraphOptimizationLevel);
    std::optional<GraphOptimizationLevel> ort_graph_optimization_level =
        StringToOrtGraphOptimizationLevel(user_graph_optimization_level);
    if (ort_graph_optimization_level) {
      CHECK_STATUS(ort_api->SetSessionGraphOptimizationLevel(
          session_options.get(), ort_graph_optimization_level.value()));
    }
  }

  return base::MakeRefCounted<SessionOptions>(base::PassKey<SessionOptions>(),
                                              std::move(session_options));
}

SessionOptions::SessionOptions(base::PassKey<SessionOptions>,
                               ScopedOrtSessionOptions session_options)
    : session_options_(std::move(session_options)) {
  CHECK(session_options_.get());
}

SessionOptions::~SessionOptions() = default;

}  // namespace webnn::ort
