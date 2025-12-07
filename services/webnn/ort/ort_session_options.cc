// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/ort/ort_session_options.h"

#include <string_view>

#include "base/command_line.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "services/webnn/ort/environment.h"
#include "services/webnn/ort/logging.h"
#include "services/webnn/ort/ort_status.h"
#include "services/webnn/ort/platform_functions_ort.h"
#include "services/webnn/public/cpp/execution_providers_info.h"
#include "services/webnn/public/cpp/webnn_trace.h"
#include "services/webnn/public/mojom/webnn_device.mojom.h"
#include "services/webnn/public/mojom/webnn_error.mojom.h"
#include "services/webnn/webnn_switches.h"
#include "third_party/windows_app_sdk_headers/src/inc/abi/winml/winml/onnxruntime_session_options_config_keys.h"

namespace webnn::ort {

namespace {

// Execution Provider selection delegate function that selects EPs based on
// WebNN device type.
// TODO(crbug.com/425487285): Select EPs based on WebNN power preference.
OrtStatus* ORT_API_CALL
EpSelectionPolicyDelegate(const OrtEpDevice** ep_devices,
                          size_t num_devices,
                          const OrtKeyValuePairs* model_metadata,
                          const OrtKeyValuePairs* runtime_metadata,
                          const OrtEpDevice** selected,
                          size_t max_selected,
                          size_t* num_selected,
                          void* state) {
  // Early return if no devices available.
  if (num_devices == 0) {
    *num_selected = 0;
    return nullptr;
  }

  mojom::Device* device_type_ptr = static_cast<mojom::Device*>(state);
  CHECK(device_type_ptr) << "Device type must be provided in state parameter";
  mojom::Device device_type = *device_type_ptr;

  // SAFETY: ORT guarantees that `ep_devices` is valid and contains
  // `num_devices` elements.
  base::span<const OrtEpDevice* const> available_devices =
      UNSAFE_BUFFERS(base::span(ep_devices, num_devices));

  // ORT currently allows a maximum of 8 selected devices. The implementation
  // here guarantees at most 3 EP devices will be selected for WebNN.
  // According to:
  // https://github.com/microsoft/onnxruntime/blob/f8c6262399e2c7e0a58cd494f0e58d4f4262dc43/onnxruntime/core/session/provider_policy_context.cc#L159
  std::vector<const OrtEpDevice*> selected_devices =
      Environment::SelectEpDevices(available_devices, device_type);
  CHECK_LE(selected_devices.size(), max_selected)
      << "Selected device count (" << selected_devices.size()
      << ") exceeds maximum allowed (" << max_selected << ")";

  OrtLoggingLevel ort_logging_level = GetOrtLoggingLevel();
  if (ort_logging_level == ORT_LOGGING_LEVEL_VERBOSE ||
      ort_logging_level == ORT_LOGGING_LEVEL_INFO) {
    // Logs selected EP devices for the given WebNN device type.
    const OrtApi* ort_api = PlatformFunctions::GetInstance()->ort_api();
    LogEpDevices(ort_api, selected_devices, "Selected OrtEpDevice");
  }

  for (size_t i = 0; i < selected_devices.size(); ++i) {
    // SAFETY: ORT guarantees that `selected` is valid and contains
    // `max_selected` elements.
    UNSAFE_BUFFERS(selected[i]) = selected_devices[i];
  }

  *num_selected = selected_devices.size();

  return nullptr;
}

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
    mojom::Device device_type,
    scoped_refptr<Environment> env) {
  ScopedTrace scoped_trace("SessionOptions::Create");

  scoped_trace.AddStep("Create session options");
  const OrtApi* ort_api = PlatformFunctions::GetInstance()->ort_api();
  ScopedOrtSessionOptions session_options;
  CHECK_STATUS(ort_api->CreateSessionOptions(
      ScopedOrtSessionOptions::Receiver(session_options).get()));

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

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kWebNNOrtEnableProfiling)) {
    std::wstring profile_prefix =
        base::CommandLine::ForCurrentProcess()->GetSwitchValueNative(
            switches::kWebNNOrtEnableProfiling);
    if (profile_prefix.empty()) {
      profile_prefix = L"WebNNOrtProfile";
    }

    CHECK_STATUS(ort_api->EnableProfiling(session_options.get(),
                                          profile_prefix.c_str()));
  }

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kWebNNOrtDisableCpuFallback)) {
    CHECK_STATUS(ort_api->AddSessionConfigEntry(
        session_options.get(), kOrtSessionOptionsDisableCPUEPFallback, "1"));
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

  std::vector<SessionConfigEntry> ep_config_entries =
      env->GetEpConfigEntries(device_type);
  for (const auto& config_entry : ep_config_entries) {
    CHECK_STATUS(ort_api->AddSessionConfigEntry(
        session_options.get(),
        /*config_key=*/config_entry.key.c_str(),
        /*config_value=*/config_entry.value.c_str()));
  }

  return base::MakeRefCounted<SessionOptions>(
      base::PassKey<SessionOptions>(), std::move(session_options), device_type);
}

SessionOptions::SessionOptions(base::PassKey<SessionOptions>,
                               ScopedOrtSessionOptions session_options,
                               mojom::Device device_type)
    : session_options_(std::move(session_options)), device_type_(device_type) {
  CHECK(session_options_.get());

  const OrtApi* ort_api = PlatformFunctions::GetInstance()->ort_api();
  // SAFETY: Passing `&device_type_` is safe because the delegate is only called
  // synchronously during session creation, and `device_type_` is a member
  // variable of this SessionOptions object which outlives the session creation
  // process.
  // NOTE: `const_cast` is safe here because `EpSelectionPolicyDelegate` only
  // reads the `device_type_` value and never modifies it. The `void*` parameter
  // is a C API limitation that doesn't preserve const-correctness.
  CHECK_STATUS(ort_api->SessionOptionsSetEpSelectionPolicyDelegate(
      session_options_.get(), EpSelectionPolicyDelegate,
      const_cast<mojom::Device*>(&device_type_)));
}

SessionOptions::~SessionOptions() = default;

}  // namespace webnn::ort
