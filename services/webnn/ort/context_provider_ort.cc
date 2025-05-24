// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/ort/context_provider_ort.h"

#include "base/command_line.h"
#include "base/types/expected_macros.h"
#include "services/webnn/ort/context_impl_ort.h"
#include "services/webnn/ort/ort_status.h"
#include "services/webnn/ort/platform_functions_ort.h"
#include "services/webnn/ort/scoped_ort_types.h"
#include "services/webnn/webnn_switches.h"

namespace webnn::ort {

namespace {

// Helper function to convert a string to OrtLoggingLevel enum.
OrtLoggingLevel StringToOrtLoggingLevel(std::string_view logging_level) {
  if (logging_level == "VERBOSE") {
    return ORT_LOGGING_LEVEL_VERBOSE;
  } else if (logging_level == "INFO") {
    return ORT_LOGGING_LEVEL_INFO;
  } else if (logging_level == "WARNING") {
    return ORT_LOGGING_LEVEL_WARNING;
  } else if (logging_level == "ERROR") {
    return ORT_LOGGING_LEVEL_ERROR;
  } else if (logging_level == "FATAL") {
    return ORT_LOGGING_LEVEL_FATAL;
  }
  // Default to ERROR if the input is invalid.
  LOG(WARNING) << "[WebNN] Unrecognized logging level: " << logging_level
               << ". Default ERROR level will be used.";
  return ORT_LOGGING_LEVEL_ERROR;
}

}  // namespace

base::expected<std::unique_ptr<WebNNContextImpl>, mojom::ErrorPtr>
CreateContextFromOptions(mojom::CreateContextOptionsPtr options,
                         mojo::PendingReceiver<mojom::WebNNContext> receiver,
                         WebNNContextProviderImpl* context_provider) {
  auto* platform_functions = PlatformFunctions::GetInstance();
  if (!platform_functions) {
    return base::unexpected(mojom::Error::New(
        mojom::Error::Code::kNotSupportedError,
        "WebNN is not supported in this ONNX Runtime version."));
  }

  OrtLoggingLevel ort_logging_level = ORT_LOGGING_LEVEL_ERROR;
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kWebNNOrtLoggingLevel)) {
    std::string user_logging_level =
        base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            switches::kWebNNOrtLoggingLevel);
    ort_logging_level = StringToOrtLoggingLevel(user_logging_level);
  }

  // `OrtEnv` is reference counted. The first `CreateEnv()` will create the
  // `OrtEnv` instance. The following invocations return a reference to the
  // same instance. It is released upon the last reference is removed via
  // `ReleaseEnv()`.
  // Creating and releasing `OrtEnv` are protected by a lock internally, so it
  // is sequence bound.
  ScopedOrtEnv env;
  if (ORT_CALL_FAILED(platform_functions->ort_api()->CreateEnv(
          ort_logging_level, "WebNN", ScopedOrtEnv::Receiver(env).get()))) {
    return base::unexpected(
        mojom::Error::New(mojom::Error::Code::kNotSupportedError,
                          "Failed to create the ONNX Runtime environment."));
  }

  ASSIGN_OR_RETURN(scoped_refptr<SessionOptions> session_options,
                   SessionOptions::Create(options->device));
  return std::make_unique<ContextImplOrt>(std::move(receiver), context_provider,
                                          std::move(options), std::move(env),
                                          std::move(session_options));
}

}  // namespace webnn::ort
