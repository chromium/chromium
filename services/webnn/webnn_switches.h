// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_WEBNN_SWITCHES_H_
#define SERVICES_WEBNN_WEBNN_SWITCHES_H_

#include "base/containers/span.h"
#include "build/build_config.h"
#include "services/webnn/buildflags.h"

namespace switches {

#if BUILDFLAG(IS_MAC)
// Copy the generated Core ML model to the folder specified
// by --webnn-coreml-dump-model, note: folder needs to be accessible from
// the GPU sandbox or use --no-sandbox.
// Usage: --no-sandbox --webnn-coreml-dump-model=/tmp/CoreMLModels
inline constexpr char kWebNNCoreMlDumpModel[] = "webnn-coreml-dump-model";
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(WEBNN_USE_TFLITE)
// Save the generated TFLite model file to the folder specified by
// --webnn-tflite-dump-model. Note, the folder needs to be accessible from the
// GPU process sandbox or --no-sandbox must be used.
// Usage: --no-sandbox --webnn-tflite-dump-model=/tmp/tflite_models
inline constexpr char kWebNNTfliteDumpModel[] = "webnn-tflite-dump-model";
#endif  // BUILDFLAG(WEBNN_USE_TFLITE)

#if BUILDFLAG(IS_WIN)
// Configure the logging severity level of ONNX Runtime.
// Usage: --webnn-ort-logging-level=VERBOSE
// Other severity levels could be "INFO", "WARNING", "ERROR" (default), and
// "FATAL".
inline constexpr char kWebNNOrtLoggingLevel[] = "webnn-ort-logging-level";
// Set the folder specified by --webnn-ort-dump-model for ONNX Runtime to save
// optimized ONNX model after graph level transformations.
// Usage: --no-sandbox --webnn-ort-dump-model=/tmp/ort_models
inline constexpr char kWebNNOrtDumpModel[] = "webnn-ort-dump-model";
// Force onnxruntime.dll to be loaded from a location specified by the switch
// for testing development ORT build. This switch is not to be used in shipping
// scenarios and is ignored by default.
// Usage: --webnn-ort-library-path-for-testing="C:\Program Files\ONNXRuntime"
// --allow-third-party-modules
inline constexpr char kWebNNOrtLibraryPathForTesting[] =
    "webnn-ort-library-path-for-testing";
// Specify the ORT EP name and library path pair via this switch for testing
// development EP builds. Libraries of the ORT EP specified by the EP name
// are forced to be loaded from the specified path. This switch is not to be
// used in shipping scenarios and is ignored by default.
// The value should be in the format <ep_name>?<ep_library_path>.
// Usage:
// --webnn-ort-ep-library-path-for-testing=OpenVINOExecutionProvider?"C:\Program
// Files\ONNXRuntime-EP\onnxruntime_providers_openvino_plugin.dll"
// --allow-third-party-modules
inline constexpr char kWebNNOrtEpLibraryPathForTesting[] =
    "webnn-ort-ep-library-path-for-testing";

// Force ONNX Runtime to only select one specific execution provider (EP) device
// which matches the given EP name, hardware vendor id and hardware device id
// for WebNN during the ORT session creation stage.
// Notes:
// 1. Available EP devices can be queried in logs when the
// `webnn-ort-logging-level` is set to `VERBOSE` or `INFO`. If no matching
// device is found, the ORT session creation fails.
// 2. The CPU fallback EP device is added implicitly in ORT by default unless
// disabled explicitly by `webnn-ort-disable-cpu-fallback` switch.
//
// The value should be in the format:
// <ep_name>,<hardware_vendor_id>,<hardware_device_id>
// Please note that all the entries must be provided in order, and both
// hardware_vendor_id and hardware_device_id are hexadecimal strings.
//
// Usage: For example, specifying an Intel GPU device using OpenVINO EP:
// --webnn-ort-ep-device=OpenVINOExecutionProvider,0x8086,0x4680
inline constexpr char kWebNNOrtEpDevice[] = "webnn-ort-ep-device";

// Enable all execution providers, ignoring the enabled flag blocklist.
inline constexpr char kWebNNOrtIgnoreEpBlocklist[] =
    "webnn-ort-ignore-ep-blocklist";

// Configure the graph optimization level of ONNX Runtime.
// Usage: --webnn-ort-graph-optimization-level=DISABLE_ALL
// Other levels could be "BASIC", "EXTENDED" and "ALL".
inline constexpr char kWebNNOrtGraphOptimizationLevel[] =
    "webnn-ort-graph-optimization-level";

// This switch allows us to collect ORT profile data for performance analysis.
// The profile data file is generated in Chrome's folder with a fixed naming
// format "prefix_date_time.json". The prefix can be provided by user or use
// "WebNNOrtProfile" as default.
// Usage: --no-sandbox --webnn-ort-enable-profiling="WebNNOrtOvCpuProfile"
inline constexpr char kWebNNOrtEnableProfiling[] = "webnn-ort-enable-profiling";

// This switch allows us to disable the fallback to default ORT CPU EP which is
// enabled by default.
// When this switch is set:
// 1. ORT session creation fails if non-CPU EPs cannot fully support all graph
// nodes.
// 2. Disables OpenVINO EP internal CPU fallback if NPU model compilation fails
// (for debugging).
// Usage: --webnn-ort-disable-cpu-fallback
inline constexpr char kWebNNOrtDisableCpuFallback[] =
    "webnn-ort-disable-cpu-fallback";
#endif  // BUILDFLAG(IS_WIN)

extern base::span<const char* const> GetWebNNSwitchesCopiedFromGpuProcessHost();
// extern const base::span<const char* const>
//     kWebNNSwitchesCopiedFromGpuProcessHost;

}  // namespace switches

#endif  // SERVICES_WEBNN_WEBNN_SWITCHES_H_
