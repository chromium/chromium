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

namespace webnn {

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

  // The maximum K dimension size of batched MatMul on certain NPU devices.
  // It is unnecessary to compute `|=` operation result across EP devices,
  // because there will be only one NPU device EP.
  std::optional<uint32_t> npu_batched_matmul_k_dimension_limit;

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
            .enabled = false,
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
                    // The OpenVINO NPU limits the batched MatMul K dimension
                    // size to 8192.
                    // For more details, see GraphBuilderOrt::AddMatMulOperation
                    // in src/services/webnn/graph_builder_ort.cc.
                    .npu_batched_matmul_k_dimension_limit = 8192,
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
});

}  // namespace webnn

#endif  // SERVICES_WEBNN_PUBLIC_CPP_EXECUTION_PROVIDERS_INFO_H_
