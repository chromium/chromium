// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_PUBLIC_CPP_EXECUTION_PROVIDERS_INFO_H_
#define SERVICES_WEBNN_PUBLIC_CPP_EXECUTION_PROVIDERS_INFO_H_

#include <appmodel.h>

#include <string_view>

#include "base/containers/fixed_flat_map.h"
#include "base/memory/raw_span.h"
#include "base/strings/cstring_view.h"
#include "services/webnn/public/mojom/webnn_device.mojom.h"

namespace webnn {

// Specifies if an execution provider supports offline compilation and the
// related information needed for offline compilation.
struct OfflineCompilationSupport {
  // The supported device type.
  mojom::Device device_type;
  // Supported device IDs corresponding to the device type. An empty span means
  // all device IDs of `device_type` are supported.
  base::raw_span<const uint32_t> device_ids;
  // Libraries that must be preloaded before sandbox lockdown. This list should
  // only contain workarounds when compiling a graph has issues to load a
  // particular library.
  // TODO(crbug.com/529544314): Remove once EPs load all required libraries
  // internally by compiling a trivial graph.
  base::raw_span<const std::string_view> preload_libraries_workaround;
};

namespace internal {

inline constexpr OfflineCompilationSupport kOpenVINOOfflineCompilation[] = {
    {
        .device_type = mojom::Device::kNpu,
        .device_ids =
            (const uint32_t[]){
                0x643E,  // Lunarlake
                0xB03E,  // Pantherlake
                0xFD3E,  // Wildcatlake
            },
        .preload_libraries_workaround =
            (const std::string_view[]){
                // This library is loaded on a worker thread internally in the
                // NPU compiler, which triggers an ERROR_ACCESS_DENIED error
                // since the worker thread runs on a lockdown token. Preloading
                // this library before sandbox lockdown is a workaround for this
                // issue.
                "openvino_intel_npu_compiler.dll",
            },
    },
};

// The WebGPU EP is vendor-agnostic and backs a generic virtual GPU device, so
// it supports offline compilation for any GPU regardless of device ID. The
// default empty `device_ids` span expresses that "any device ID" support.
// Combined with the EP-level `vendor_id` of 0, this represents a
// vendor-agnostic generic virtual device, which is only registered when the
// environment is created with "allow_virtual_devices" enabled; device matching
// in the sandboxed Compiler process then resolves the target to that virtual
// device.
inline constexpr OfflineCompilationSupport kWebGpuOfflineCompilation[] = {
    {
        .device_type = mojom::Device::kGpu,
    },
};

inline constexpr OfflineCompilationSupport kNvTensorRTRTXOfflineCompilation[] =
    {
        {
            // The TensorRT-RTX EP only enumerates NVIDIA GPUs, so every GPU
            // device it exposes is supported. An empty `device_ids` span means
            // all device IDs for this device type qualify.
            .device_type = mojom::Device::kGpu,
        },
};

}  // namespace internal

inline constexpr std::string_view kCPUExecutionProvider =
    "CPUExecutionProvider";
inline constexpr std::string_view kDmlExecutionProvider =
    "DmlExecutionProvider";
inline constexpr std::string_view kMIGraphXExecutionProvider =
    "MIGraphXExecutionProvider";
inline constexpr std::string_view kNvTensorRTRTXExecutionProvider =
    "NvTensorRTRTXExecutionProvider";
inline constexpr std::string_view kOpenVINOExecutionProvider =
    "OpenVINOExecutionProvider";
inline constexpr std::string_view kQNNExecutionProvider =
    "QNNExecutionProvider";
inline constexpr std::string_view kVitisAIExecutionProvider =
    "VitisAIExecutionProvider";
inline constexpr std::string_view kWebGpuExecutionProvider =
    "WebGpuExecutionProvider";

// Describes the workarounds needed for execution provider limitations.
// TODO(crbug.com/428740146): Remove this struct once all the execution
// providers fix these issues.
struct EpWorkarounds {
  // TODO(crbug.com/429253567): Specify the minimum package version that
  // supports these features without requiring workarounds.

  // By default ONNX Resize op supports any axes, but some EPs may only support
  // NCHW layout. `ContextProperties.resample_2d_axes` will be updated to
  // respect this limit.
  bool resample2d_limit_to_nchw = false;

  // Whether the EP may report the NPU driver version in legacy concatenated
  // format (e.g., "1004404") instead of 4-part dot-separated format.
  bool npu_concatenated_driver_version = false;

  EpWorkarounds& operator|=(const EpWorkarounds& other) {
    resample2d_limit_to_nchw |= other.resample2d_limit_to_nchw;
    return *this;
  }
};

// A key-value pair for session configuration needed by some execution
// providers.
struct SessionConfigEntry {
  base::cstring_view key;
  base::cstring_view value;
};

struct EpInfo {
  PACKAGE_VERSION min_package_version;
  // Represents the vendor id of the hardware device used by the execution
  // provider.
  uint32_t vendor_id;
  // Controls whether the execution provider is enabled or not. Disabled
  // execution providers can be enabled all at once via the
  // --webnn-ort-ignore-ep-blocklist command line switch.
  bool enabled;
  EpWorkarounds workarounds;
  base::raw_span<const SessionConfigEntry> config_entries;
  // The minimum driver versions required by the NPU device for this EP to work.
  // Empty value means no version check is needed (default allow).
  std::string_view min_npu_driver_version;
  // Optional session config key for dumping models. When set,
  // `SetOptimizedModelFilePath` is not used; instead, the dump path is passed
  // via this config entry. Empty value means the EP uses the default
  // `SetOptimizedModelFilePath` approach.
  base::cstring_view model_dump_config_key;
  // Environment config keys naming the device this EP must compile for,
  // supplying what it would otherwise query from a driver the Compiler process
  // cannot reach. They go on the environment rather than into provider options
  // because the EP reads them while enumerating devices, before any session
  // exists. Values are opaque to WebNN. Empty for an EP that resolves the
  // target from the hardware it runs on.
  //
  // Both are spelled without the "ep_factory.<ep_name>." prefix ORT reserves
  // for entries addressed to a particular execution provider, and must be set
  // together: an architecture without a device to attribute it to, or a device
  // without the architecture that decides what gets built, is not useful.
  base::cstring_view target_architecture_env_config_key;
  // Key whose value is the PCI device id from
  // `OrtApi::HardwareDevice_DeviceId()`, written as a hexadecimal string.
  base::cstring_view hardware_device_id_env_config_key;
  // The information of offline compilation support.
  base::raw_span<const OfflineCompilationSupport> offline_compilation_support;
};

// The listed EPs must match the names of the histogram variants
// WebNNOrtExecutionProvider in
// tools/metrics/histograms/metadata/webnn/histograms.xml.
inline constexpr auto kKnownEPs = base::MakeFixedFlatMap<std::string_view,
                                                         EpInfo>({
    // AMD
    {
        kMIGraphXExecutionProvider,
        {
            .min_package_version =
                {
                    .Major = 1,
                    .Minor = 8,
                    .Build = 53,
                    .Revision = 0,
                },
            .vendor_id = 0x1002,
            .enabled = false,
        },
    },
    // NVidia
    {
        kNvTensorRTRTXExecutionProvider,
        {
            .min_package_version =
                {
                    .Major = 0,
                    .Minor = 0,
                    .Build = 26,
                    .Revision = 0,
                },
            .vendor_id = 0x10de,
            // Kept disabled until the EP can compile inside the
            // Win32k-locked-down Compiler process. `min_package_version` must
            // be raised to the first package that does before enabling.
            .enabled = false,
            .target_architecture_env_config_key = "nv_target_sm",
            .hardware_device_id_env_config_key = "nv_hardware_device_id",
            .offline_compilation_support =
                internal::kNvTensorRTRTXOfflineCompilation,
        },
    },
    // Intel
    {
        kOpenVINOExecutionProvider,
        {
            // The package version 1.8.69.0 maps to the EP
            // version 1.3.0+b130ce1.
            .min_package_version =
                {
                    .Major = 1,
                    .Minor = 8,
                    .Build = 69,
                    .Revision = 0,
                },
            .vendor_id = 0x8086,
            .enabled = true,
            .workarounds =
                {
                    .resample2d_limit_to_nchw = true,
                    // The OpenVINO EP currently reports NPU driver versions in
                    // legacy concatenated format (e.g., "1004404").
                    .npu_concatenated_driver_version = true,
                },
            // OpenVINO EP configuration. Keys and values must align with the
            // ORT OpenVINO EP implementation. See:
            // https://github.com/microsoft/onnxruntime/blob/f46113d7b11af3fa0b3918029e442c3a14265522/onnxruntime/core/providers/openvino/openvino_provider_factory.cc#L459
            // and
            // https://onnxruntime.ai/docs/execution-providers/OpenVINO-ExecutionProvider.html#summary-of-options.
            //
            // To get more accurate inference results, WebNN requires the
            // accuracy execution mode on OpenVINO GPU/NPU to avoid lowering the
            // execution accuracy for performance reasons, maintain original
            // model precision (f32→f32, f16→f16) and disable dynamic
            // quantization. See:
            // https://docs.openvino.ai/2025/openvino-workflow/running-inference/optimize-inference/precision-control.html.
            //
            // On OpenVINO GPU, the default `fp16` precision specified by
            // `INFERENCE_PRECISION_HINT` can override the `ACCURACY` mode set
            // by `EXECUTION_MODE_HINT`. To improve robustness and ensure
            // accurate inference results, we explicitly set
            // `INFERENCE_PRECISION_HINT` to `dynamic`.
            .config_entries =
                (const SessionConfigEntry[]){
                    {.key = "ep.openvinoexecutionprovider.load_config",
                     .value = R"({
                            "GPU": {
                                "EXECUTION_MODE_HINT": "ACCURACY",
                                "INFERENCE_PRECISION_HINT": "dynamic"
                            },
                            "NPU": {
                                "EXECUTION_MODE_HINT": "ACCURACY"
                            }
                        })"},
                },
            // The minimum NPU driver version in 4-part dot-separated format.
            .min_npu_driver_version = "32.0.100.4404",
            // `SetOptimizedModelFilePath` does not work for the OpenVINO EP.
            // Dump models via its own session config entry.
            .model_dump_config_key =
                "ep.openvinoexecutionprovider.dump_subgraphs",
            .offline_compilation_support =
                internal::kOpenVINOOfflineCompilation,
        },
    },
    // Qualcomm
    {
        kQNNExecutionProvider,
        {
            .min_package_version =
                {
                    .Major = 2,
                    .Minor = 2420,
                    .Build = 40,
                    .Revision = 0,
                },
            .vendor_id = 0x4d4f4351,
            .enabled = false,
        },
    },
    // AMD
    {
        kVitisAIExecutionProvider,
        {
            .min_package_version =
                {
                    .Major = 1,
                    .Minor = 8,
                    .Build = 57,
                    .Revision = 0,
                },
            .vendor_id = 0x1022,
            .enabled = false,
        },
    },
    {
        kWebGpuExecutionProvider,
        {
            .min_package_version =
                {
                    .Major = 0,
                    .Minor = 2,
                    .Build = 0,
                    .Revision = 26194,
                },
            // The WebGPU EP is vendor-agnostic, so the vendor ID is set to 0
            // to match ORT's convention.
            // https://github.com/microsoft/onnxruntime/blob/a91b0b4/onnxruntime/core/providers/webgpu/ep/factory.cc#L66
            .vendor_id = 0,
            .enabled = false,
            // The WebGPU EP does not enable int64 ops by default. WebNN
            // requires int64 support, so enable it explicitly. Key and value
            // must align with the ORT WebGPU EP implementation. See:
            // https://github.com/microsoft/onnxruntime/blob/47faa11b035d53c49f3f93e815d004e616d360ca/onnxruntime/core/providers/webgpu/webgpu_provider_options.h#L18
            //
            // The WebGPU EP disables robust buffer access by default in
            // release builds (for performance). Enable it explicitly so
            // out-of-bounds accesses in generated shaders are
            // clamped/bounds-checked by the WebGPU runtime instead of reading
            // or writing arbitrary GPU memory. WebNN runs untrusted web
            // content, so this is enabled to harden Chrome's security. Key and
            // value must align with the ORT WebGPU EP implementation. See:
            // https://github.com/microsoft/onnxruntime/blob/4576ea193c1c322115d2a75bb88026310add77c0/onnxruntime/core/providers/webgpu/webgpu_provider_options.h#L37
            .config_entries =
                (const SessionConfigEntry[]){
                    {.key = "ep.webgpuexecutionprovider.enableInt64",
                     .value = "1"},
                    {.key = "ep.webgpuexecutionprovider.enableRobustness",
                     .value = "1"},
                },
            .offline_compilation_support = internal::kWebGpuOfflineCompilation,
        },
    },
});

}  // namespace webnn

#endif  // SERVICES_WEBNN_PUBLIC_CPP_EXECUTION_PROVIDERS_INFO_H_
