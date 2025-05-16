// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_WEBNN_SWITCHES_H_
#define SERVICES_WEBNN_WEBNN_SWITCHES_H_

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

#if BUILDFLAG(WEBNN_USE_ORT)
// Save the generated ONNX model file to the folder specified by
// --webnn-ort-dump-model. Note, the folder needs to be accessible from the
// GPU process sandbox or --no-sandbox must be used.
// Usage: --no-sandbox --webnn-ort-dump-model=./OnnxModels
inline constexpr char kWebNNOrtDumpModel[] = "webnn-ort-dump-model";

// Enable model caching for OV EP with cache folder specified by
// --webnn-ort-use-ov-model-cache. Note, the folder needs to be accessible from
// the GPU process sandbox or --no-sandbox must be used. Usage: --no-sandbox
// --webnn-ort-use-ov-model-cache=./ModelCache
// This switch doesn't work if --webnn-ort-dump-model is enabled.
inline constexpr char kWebNNOrtUseOVModelCache[] =
    "webnn-ort-use-ov-model-cache";

// Configure the logging severity level of ONNX Runtime.
// Usage: --no-sandbox --enable-logging --webnn-ort-logging-level=VERBOSE
// Other severity levels could be "INFO", "WARNING" (default), "ERROR" and
// "FATAL".
inline constexpr char kWebNNOrtLoggingLevel[] = "webnn-ort-logging-level";

// For testing within GPU sandbox, developers need to copy ONNX Runtime DLLs,
// OpenVINO EP DLL and OpenVINO DLLs into a folder under "Program Files" and
// specify it with this switch.
// Usage: --webnn-ort-library-path="C:\Program Files\ONNXRuntime-OVEP"
//
// For testing non-Microsoft-signed DLLs within GPU sandbox, you need also
// append "--allow-third-party-modules".
// Usage: --webnn-ort-library-path="C:\Program Files\ONNXRuntime-OVEP"
// --allow-third-party-modules
inline constexpr char kWebNNOrtLibraryPath[] = "webnn-ort-library-path";

// Configure the ort graph optimization level of ONNX Runtime.
// Usage: --webnn-ort-graph-optimization-level=ALL
// Other levels could be "DISABLE_ALL", "BASIC" and "EXTENDED".
inline constexpr char kWebNNOrtGraphOptimizationLevel[] =
    "webnn-ort-graph-optimization-level";
#endif  // BUILDFLAG(WEBNN_USE_ORT)

}  // namespace switches

#endif  // SERVICES_WEBNN_WEBNN_SWITCHES_H_
