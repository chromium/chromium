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

#if BUILDFLAG(IS_WIN)
// Configure the logging severity level of ONNX Runtime.
// Usage: --no-sandbox --enable-logging --webnn-ort-logging-level=VERBOSE
// Other severity levels could be "INFO", "WARNING", "ERROR" (default), and
// "FATAL".
inline constexpr char kWebNNOrtLoggingLevel[] = "webnn-ort-logging-level";
#endif  // BUILDFLAG(IS_WIN)

}  // namespace switches

#endif  // SERVICES_WEBNN_WEBNN_SWITCHES_H_
