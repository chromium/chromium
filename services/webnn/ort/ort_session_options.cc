// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/ort/ort_session_options.h"

#include "services/webnn/ort/ort_status.h"
#include "services/webnn/ort/platform_functions_ort.h"
#include "services/webnn/public/cpp/webnn_trace.h"
#include "services/webnn/public/mojom/webnn_device.mojom.h"
#include "services/webnn/public/mojom/webnn_error.mojom.h"
#include "third_party/onnxruntime_headers/src/include/onnxruntime/core/session/onnxruntime_session_options_config_keys.h"

namespace webnn::ort {

// static
base::expected<scoped_refptr<SessionOptions>, mojom::ErrorPtr>
SessionOptions::Create(mojom::Device device_type) {
  ScopedTrace scoped_trace("SessionOptions::Create");

  if (device_type != mojom::Device::kCpu) {
    return base::unexpected(mojom::Error::New(
        mojom::Error::Code::kNotSupportedError,
        "The ONNX Runtime backend only supports CPU device type currently."));
  }

  scoped_trace.AddStep("Create session options");
  const OrtApi* ort_api = PlatformFunctions::GetInstance()->ort_api();
  ScopedOrtSessionOptions session_options;
  CHECK_STATUS(ort_api->CreateSessionOptions(
      ScopedOrtSessionOptions::Receiver(session_options).get()));
  // TODO(crbug.com/416539420): Add a switch to dump model once ORT backend can
  // build a model.

  // Enable strict shape type inference check. All inconsistencies encountered
  // will expose errors during session creation. For example, if the graph
  // output shape set by WebNN is different from ONNX shape inference result,
  // the session creation will fail.
  CHECK_STATUS(ort_api->AddSessionConfigEntry(
      session_options.get(),
      /*config_key=*/kOrtSessionOptionsConfigStrictShapeTypeInference,
      /*config_value=*/"1"));

  // Use CPU EP by default.
  //
  // TODO(crbug.com/412841630): Investigate how to apply layout optimizations
  // (ORT_ENABLE_ALL).
  // https://onnxruntime.ai/docs/performance/model-optimizations/graph-optimizations.html#layout-optimizations
  // TODO(crbug.com/416543902): Add a switch to test different optimization
  // levels at runtime.
  CHECK_STATUS(ort_api->SetSessionGraphOptimizationLevel(
      session_options.get(), GraphOptimizationLevel::ORT_ENABLE_BASIC));

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
