// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/ort/ort_session_options.h"

#include <algorithm>
#include <vector>

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/strings/stringprintf.h"
#include "services/webnn/ort/environment.h"
#include "services/webnn/ort/ort_status.h"
#include "services/webnn/ort/platform_functions_ort.h"
#include "services/webnn/public/cpp/webnn_trace.h"
#include "services/webnn/public/mojom/webnn_device.mojom.h"
#include "services/webnn/public/mojom/webnn_error.mojom.h"
#include "services/webnn/webnn_switches.h"
#include "third_party/onnxruntime_headers/src/include/onnxruntime/core/session/onnxruntime_session_options_config_keys.h"

namespace webnn::ort {

namespace {

constexpr base::cstring_view kCpuExecutionProvider = "CPUExecutionProvider";

bool IsDefaultCpuEpDevice(const OrtEpDevice* device) {
  const OrtApi* ort_api = PlatformFunctions::GetInstance()->ort_api();

  return UNSAFE_BUFFERS(base::cstring_view(ort_api->EpDevice_EpName(device))) ==
         kCpuExecutionProvider;
}

// Select the first device of specified hardware device type from the sorted
// devices. Return nullptr if no such device is found.
// This behavior mimics the selection logic in ORT's provider_policy_context.cc:
// https://github.com/microsoft/onnxruntime/blob/main/onnxruntime/core/session/provider_policy_context.cc#L402-L444
template <OrtHardwareDeviceType DeviceType>
const OrtEpDevice* SelectFirstEpDeviceForDeviceType(
    base::span<const OrtEpDevice* const> sorted_devices) {
  const OrtApi* ort_api = PlatformFunctions::GetInstance()->ort_api();

  auto first_device = std::ranges::find_if(
      sorted_devices, [ort_api](const OrtEpDevice* device) {
        return ort_api->HardwareDevice_Type(ort_api->EpDevice_Device(device)) ==
               DeviceType;
      });

  if (first_device != sorted_devices.end()) {
    return *first_device;
  }

  return nullptr;
}

// Select the first CPU device and also append the default CPU EP device if the
// selected device is not the default one.
std::vector<const OrtEpDevice*> SelectEpDevicesForCpu(
    base::span<const OrtEpDevice* const> sorted_devices) {
  std::vector<const OrtEpDevice*> selected_devices;

  const OrtEpDevice* first_cpu =
      SelectFirstEpDeviceForDeviceType<OrtHardwareDeviceType_CPU>(
          sorted_devices);

  // Handle the rare case where no CPU EP device is available.
  if (!first_cpu) {
    LOG(ERROR) << "[WebNN] No CPU execution provider available.";
    return selected_devices;
  }

  selected_devices.push_back(first_cpu);

  // Add the default CPU EP device to ensure maximum coverage of opsets and
  // operators.
  if (!IsDefaultCpuEpDevice(first_cpu) &&
      IsDefaultCpuEpDevice(sorted_devices.back())) {
    selected_devices.push_back(sorted_devices.back());
  }

  return selected_devices;
}

// Select the first GPU device with CPU fallback.
std::vector<const OrtEpDevice*> SelectEpDevicesForGpu(
    base::span<const OrtEpDevice* const> sorted_devices) {
  std::vector<const OrtEpDevice*> selected_devices;

  const OrtEpDevice* first_gpu =
      SelectFirstEpDeviceForDeviceType<OrtHardwareDeviceType_GPU>(
          sorted_devices);

  if (first_gpu) {
    selected_devices.push_back(first_gpu);
  }

  std::vector<const OrtEpDevice*> cpu_fallback_devices =
      SelectEpDevicesForCpu(sorted_devices);
  selected_devices.insert(selected_devices.end(), cpu_fallback_devices.begin(),
                          cpu_fallback_devices.end());

  return selected_devices;
}

// Select the first NPU device with CPU fallback. If no NPU device is selected,
// delegate to GPU device selection logic which selects the first GPU device
// with CPU fallback.
std::vector<const OrtEpDevice*> SelectEpDevicesForNpu(
    base::span<const OrtEpDevice* const> sorted_devices) {
  const OrtEpDevice* first_npu =
      SelectFirstEpDeviceForDeviceType<OrtHardwareDeviceType_NPU>(
          sorted_devices);

  if (!first_npu) {
    return SelectEpDevicesForGpu(sorted_devices);
  }

  std::vector<const OrtEpDevice*> selected_devices;
  selected_devices.push_back(first_npu);

  std::vector<const OrtEpDevice*> cpu_fallback_devices =
      SelectEpDevicesForCpu(sorted_devices);
  selected_devices.insert(selected_devices.end(), cpu_fallback_devices.begin(),
                          cpu_fallback_devices.end());

  return selected_devices;
}

// Execution Provider selection delegate function that selects EPs based on
// WebNN device type.
// TODO(crbug.com/425487285): Select EPs based on WebNN power preference.
OrtStatus* EpSelectionPolicyDelegate(const OrtEpDevice** ep_devices,
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

  // The `sorted_devices` array is already pre-sorted by device type (NPU -> GPU
  // -> CPU), discrete GPU is preferred over integrated GPU, then by vendor
  // preference (matching platform vendor first). The default CPU EP device is
  // always placed last.
  // According to:
  // https://github.com/microsoft/onnxruntime/blob/f8c6262399e2c7e0a58cd494f0e58d4f4262dc43/onnxruntime/core/session/provider_policy_context.cc#L56
  //
  // TODO(crbug.com/439708046): Consolidate EP device selection logic into
  // reusable function with WebNN's own sorting.
  //
  // SAFETY: ORT guarantees that `ep_devices` is valid and contains
  // `num_devices` elements.
  base::span<const OrtEpDevice* const> sorted_devices =
      UNSAFE_BUFFERS(base::span(ep_devices, num_devices));
  std::vector<const OrtEpDevice*> selected_devices;
  switch (device_type) {
    case mojom::Device::kCpu:
      selected_devices = SelectEpDevicesForCpu(sorted_devices);
      break;
    case mojom::Device::kGpu:
      selected_devices = SelectEpDevicesForGpu(sorted_devices);
      break;
    case mojom::Device::kNpu:
      selected_devices = SelectEpDevicesForNpu(sorted_devices);
      break;
  }

  // ORT currently allows a maximum of 8 selected devices. The delegate
  // implementation here guarantees at most 3 EP devices will be selected.
  // According to:
  // https://github.com/microsoft/onnxruntime/blob/f8c6262399e2c7e0a58cd494f0e58d4f4262dc43/onnxruntime/core/session/provider_policy_context.cc#L159
  CHECK_LE(selected_devices.size(), max_selected)
      << "Selected device count (" << selected_devices.size()
      << ") exceeds maximum allowed (" << max_selected << ")";

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

  // Apply EP-specific configuration entries for the given device type.
  // TODO(crbug.com/439972928): Only apply configuration entries for EPs that
  // will be selected.
  std::vector<Environment::SessionConfigEntry> ep_config_entries =
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
