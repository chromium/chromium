// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/ort/ort_session_options.h"

#include <set>
#include <string_view>

#include "base/command_line.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "services/webnn/ort/environment.h"
#include "services/webnn/ort/logging.h"
#include "services/webnn/ort/ort_data_type.h"
#include "services/webnn/ort/ort_status.h"
#include "services/webnn/ort/platform_functions_ort.h"
#include "services/webnn/public/cpp/ep_device_info.h"
#include "services/webnn/public/cpp/execution_providers_info.h"
#include "services/webnn/public/cpp/webnn_trace.h"
#include "services/webnn/public/mojom/webnn_error.mojom.h"
#include "services/webnn/public/mojom/webnn_service_introspection.mojom.h"
#include "services/webnn/webnn_switches.h"
#include "third_party/windows_app_sdk_headers/src/inc/abi/winml/winml/onnxruntime_session_options_config_keys.h"

namespace webnn::ort {

namespace {

// Execution Provider selection delegate function that selects EPs based on
// the WebNN context options.
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

  // TODO(crbug.com/425487285): Select EPs based on WebNN power preference.
  const auto* context_options =
      static_cast<const mojom::CreateContextOptions*>(state);
  CHECK(context_options)
      << "CreateContextOptions must be provided in state parameter";
  OrtHardwareDeviceType device_type =
      ort::WebnnToOrtDeviceType(context_options->device);

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
    // Logs selected EP devices for the given device type.
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

std::optional<uint32_t> GetBatchedMatMulKDimensionLimit(
    const OrtEpDevice* first_selected_device) {
  const OrtApi* ort_api = PlatformFunctions::GetInstance()->ort_api();

  std::string_view ep_name = ort_api->EpDevice_EpName(first_selected_device);
  const auto iter = kKnownEPs.find(ep_name);
  if (iter == kKnownEPs.end()) {
    return std::nullopt;
  }

  OrtHardwareDeviceType hardware_device_type = ort_api->HardwareDevice_Type(
      ort_api->EpDevice_Device(first_selected_device));
  if (hardware_device_type != OrtHardwareDeviceType_NPU) {
    return std::nullopt;
  }

  return iter->second.workarounds.npu_batched_matmul_k_dimension_limit;
}

ScopedOrtSessionOptions CreateBaseSessionOptions(
    std::string_view primary_ep_name) {
  const OrtApi* ort_api = PlatformFunctions::GetInstance()->ort_api();
  ScopedOrtSessionOptions session_options;
  CHECK_STATUS(ort_api->CreateSessionOptions(
      ScopedOrtSessionOptions::Receiver(session_options).get()));

  // TODO(crbug.com/530292678): kWebNNOrtDumpModel is used for dumping either
  // the optimized ONNX model or the EP-specific IHV model. In the future, an EP
  // may support dumping both simultaneously. When that happens, we should
  // introduce a separate switch to allow users to control each dump target
  // independently.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kWebNNOrtDumpModel)) {
    base::FilePath dump_directory =
        base::CommandLine::ForCurrentProcess()->GetSwitchValuePath(
            switches::kWebNNOrtDumpModel);
    const auto ep_it = kKnownEPs.find(primary_ep_name);
    if (ep_it != kKnownEPs.end() &&
        !ep_it->second.model_dump_config_key.empty()) {
      // Currently, ORT's `SetOptimizedModelFilePath` can only dump the
      // ORT-optimized ONNX models, not the models that have been taken over
      // and compiled into an EP-specific format (e.g., OpenVINO). Due to this
      // limitation, dump such models via the EP's own session config entry
      // instead.
      CHECK_STATUS(ort_api->AddSessionConfigEntry(
          session_options.get(),
          /*config_key=*/ep_it->second.model_dump_config_key.c_str(),
          /*config_value=*/dump_directory.AsUTF8Unsafe().c_str()));
    } else {
      static uint64_t dump_count = 0;
      base::FilePath dump_path = dump_directory.AppendASCII(
          base::StrCat({"model", base::NumberToString(dump_count++), ".onnx"}));
      CHECK_STATUS(ort_api->SetOptimizedModelFilePath(
          session_options.get(), dump_path.value().c_str()));
    }
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

  return session_options;
}

}  // namespace

// static
base::expected<scoped_refptr<SessionOptions>, std::string>
SessionOptions::Create(mojom::CreateContextOptionsPtr context_options,
                       scoped_refptr<Environment> env) {
  ScopedTrace scoped_trace("SessionOptions::Create");

  base::span<const OrtEpDevice* const> registered_ep_devices =
      env->GetRegisteredEpDevices();
  std::vector<const OrtEpDevice*> selected_ep_devices =
      Environment::SelectEpDevices(
          registered_ep_devices,
          ort::WebnnToOrtDeviceType(context_options->device));
  if (selected_ep_devices.empty()) {
    return base::unexpected("No execution provider device available.");
  }
  const OrtEpDevice* first_selected_device = selected_ep_devices.front();

  scoped_trace.AddStep("Create session options");
  const OrtApi* ort_api = PlatformFunctions::GetInstance()->ort_api();
  ScopedOrtSessionOptions session_options =
      CreateBaseSessionOptions(ort_api->EpDevice_EpName(first_selected_device));

  // Apply required session configs for selected EPs.
  std::set<std::string_view> processed_ep_names;
  for (const auto* ep_device : selected_ep_devices) {
    CHECK(ep_device);
    std::string_view ep_name = ort_api->EpDevice_EpName(ep_device);
    // Skip if we've already processed this EP.
    if (processed_ep_names.contains(ep_name)) {
      continue;
    }
    processed_ep_names.insert(ep_name);

    const auto ep_it = kKnownEPs.find(ep_name);
    if (ep_it == kKnownEPs.end()) {
      continue;
    }
    for (const auto& [key, value] : ep_it->second.config_entries) {
      CHECK_STATUS(ort_api->AddSessionConfigEntry(session_options.get(),
                                                  key.c_str(), value.c_str()));
    }
  }

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kWebNNOrtDisableCpuFallback)) {
    CHECK_STATUS(ort_api->AddSessionConfigEntry(
        session_options.get(), kOrtSessionOptionsDisableCPUEPFallback, "1"));
  }

  return base::MakeRefCounted<SessionOptions>(
      base::PassKey<SessionOptions>(), std::move(session_options),
      std::move(env), selected_ep_devices.front(), std::move(context_options));
}

// static
scoped_refptr<SessionOptions> SessionOptions::Create(
    const EpDeviceInfo& target_device,
    scoped_refptr<Environment> env) {
  const OrtApi* ort_api = PlatformFunctions::GetInstance()->ort_api();

  // Ensure the specified EP device is registered in the environment.
  const OrtEpDevice* target_ort_device = nullptr;
  base::span<const OrtEpDevice* const> registered_devices =
      env->GetRegisteredEpDevices();
  for (const auto* registered_device : registered_devices) {
    const OrtHardwareDevice* hardware_device =
        ort_api->EpDevice_Device(registered_device);
    std::string_view ep_name = ort_api->EpDevice_EpName(registered_device);
    OrtHardwareDeviceType hardware_device_type =
        ort_api->HardwareDevice_Type(hardware_device);
    uint32_t device_id = ort_api->HardwareDevice_DeviceId(hardware_device);

    if (target_device.ep_name == ep_name &&
        WebnnToOrtDeviceType(target_device.device_type) ==
            hardware_device_type &&
        target_device.device_id == device_id) {
      target_ort_device = registered_device;
      break;
    }
  }
  CHECK(target_ort_device) << "[WebNN] Target EP device not registered: "
                           << target_device.ep_name
                           << " with device type: " << target_device.device_type
                           << " and ID: 0x" << std::hex
                           << target_device.device_id;

  ScopedOrtSessionOptions session_options =
      CreateBaseSessionOptions(target_device.ep_name);

  // Disable CPU EP fallback to ensure the session will be created on the
  // expected EP device.
  CHECK_STATUS(ort_api->AddSessionConfigEntry(
      session_options.get(), kOrtSessionOptionsDisableCPUEPFallback, "1"));

  const auto ep_it = kKnownEPs.find(target_device.ep_name);
  if (ep_it != kKnownEPs.end()) {
    for (const auto& [key, value] : ep_it->second.config_entries) {
      CHECK_STATUS(ort_api->AddSessionConfigEntry(session_options.get(),
                                                  key.c_str(), value.c_str()));
    }
  }

  // Directly bind the target device to the session options, bypassing the
  // auto EP selection policy.
  CHECK_STATUS(ort_api->SessionOptionsAppendExecutionProvider_V2(
      session_options.get(), const_cast<OrtEnv*>(env->get()),
      &target_ort_device,
      /*num_ep_devices=*/1, /*ep_option_keys=*/nullptr,
      /*ep_option_vals=*/nullptr, /*num_ep_options=*/0));

  return base::MakeRefCounted<SessionOptions>(
      base::PassKey<SessionOptions>(), std::move(session_options),
      std::move(env), target_ort_device, /*context_options=*/nullptr);
}

SessionOptions::SessionOptions(base::PassKey<SessionOptions>,
                               ScopedOrtSessionOptions session_options,
                               scoped_refptr<Environment> env,
                               const OrtEpDevice* first_selected_device,
                               mojom::CreateContextOptionsPtr context_options)
    : session_options_(std::move(session_options)),
      env_(std::move(env)),
      first_selected_device_(first_selected_device),
      batched_matmul_k_dimension_limit_(
          GetBatchedMatMulKDimensionLimit(first_selected_device)),
      context_options_(std::move(context_options)) {
  // Set the EP selection policy delegate if `context_options_` is provided.
  if (context_options_) {
    const OrtApi* ort_api = PlatformFunctions::GetInstance()->ort_api();
    // SAFETY: Passing `session_options_.get()` and `context_options_.get()` is
    // safe because the delegate is only called synchronously during session
    // creation, and `session_options_` and `context_options_` are member
    // variables of this SessionOptions object which outlives the session
    // creation process.
    CHECK_STATUS(ort_api->SessionOptionsSetEpSelectionPolicyDelegate(
        session_options_.get(), EpSelectionPolicyDelegate,
        context_options_.get()));
  }
}

SessionOptions::~SessionOptions() = default;

}  // namespace webnn::ort
