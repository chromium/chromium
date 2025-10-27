// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_ORT_LOGGING_H_
#define SERVICES_WEBNN_ORT_LOGGING_H_

#include <string_view>

#include "base/containers/span.h"
#include "third_party/windows_app_sdk_headers/src/inc/abi/winml/winml/onnxruntime_c_api.h"

namespace webnn::ort {

// Returns the ORT logging level specified by the `kWebNNOrtLoggingLevel` switch
// if valid; defaults to ERROR level if unset or unrecognized.
OrtLoggingLevel GetOrtLoggingLevel();

// Helper to log detailed info for a list of EP devices.
void LogEpDevices(const OrtApi* ort_api,
                  base::span<const OrtEpDevice* const> ep_devices,
                  std::string_view prefix);

}  // namespace webnn::ort

#endif  // SERVICES_WEBNN_ORT_LOGGING_H_
