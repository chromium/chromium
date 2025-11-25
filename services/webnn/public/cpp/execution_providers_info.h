// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_PUBLIC_CPP_EXECUTION_PROVIDERS_INFO_H_
#define SERVICES_WEBNN_PUBLIC_CPP_EXECUTION_PROVIDERS_INFO_H_

#include <appmodel.h>

#include "base/containers/fixed_flat_map.h"
#include "base/memory/raw_span.h"
#include "base/strings/cstring_view.h"

namespace webnn {

inline constexpr base::cstring_view kCpuExecutionProvider =
    "CPUExecutionProvider";
inline constexpr base::cstring_view kDmlExecutionProvider =
    "DmlExecutionProvider";
inline constexpr base::cstring_view kMIGraphXExecutionProvider =
    "MIGraphXExecutionProvider";
inline constexpr base::cstring_view kNvTensorRTRTXExecutionProvider =
    "NvTensorRTRTXExecutionProvider";
inline constexpr base::cstring_view kOpenVINOExecutionProvider =
    "OpenVINOExecutionProvider";
inline constexpr base::cstring_view kQNNExecutionProvider =
    "QNNExecutionProvider";
inline constexpr base::cstring_view kVitisAIExecutionProvider =
    "VitisAIExecutionProvider";
inline constexpr base::cstring_view kWebGpuExecutionProvider =
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
};

// The listed EPs must match the names of the histogram variants
// WebNNOrtExecutionProvider in
// tools/metrics/histograms/metadata/webnn/histograms.xml.
inline constexpr auto kKnownEPs = base::MakeFixedFlatMap<base::cstring_view,
                                                         EpInfo>({
    // AMD
    {
        kMIGraphXExecutionProvider,
        {
            .min_package_version =
                {
                    .Major = 1,
                    .Minor = 8,
                    .Build = 35,
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
                    .Major = 1,
                    .Minor = 8,
                    .Build = 14,
                    .Revision = 0,
                },
            .vendor_id = 0x10de,
            .enabled = true,
        },
    },
    // Intel
    {
        kOpenVINOExecutionProvider,
        {
            .min_package_version =
                {
                    .Major = 1,
                    .Minor = 8,
                    .Build = 15,
                    .Revision = 0,
                },
            .vendor_id = 0x8086,
            .enabled = true,
            .workarounds =
                {
                    .resample2d_limit_to_nchw = true,
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
        },
    },
    // Qualcomm
    {
        kQNNExecutionProvider,
        {
            .min_package_version =
                {
                    .Major = 1,
                    .Minor = 8,
                    .Build = 13,
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
                    .Build = 31,
                    .Revision = 0,
                },
            .vendor_id = 0x1022,
            .enabled = false,
        },
    },
});

}  // namespace webnn

#endif  // SERVICES_WEBNN_PUBLIC_CPP_EXECUTION_PROVIDERS_INFO_H_
