// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/ort/utils_ort.h"

#include "base/strings/strcat.h"
#include "services/webnn/ort/platform_functions_ort.h"
#include "services/webnn/public/cpp/webnn_errors.h"

namespace webnn::ort {

namespace {

const char kBackendName[] = "Ort: ";

}  // namespace

const OrtApi* GetOrtApi() {
  PlatformFunctions* platform_functions = PlatformFunctions::GetInstance();
  CHECK(platform_functions);
  return platform_functions->ort_api();
}

mojom::ErrorPtr CreateError(mojom::Error::Code error_code,
                            const std::string& error_message,
                            std::string_view label) {
  LOG(ERROR) << "[WebNN] CreateError: " << error_message;
  if (!label.empty()) {
    return mojom::Error::New(
        error_code, base::StrCat({kBackendName, GetErrorLabelPrefix(label),
                                  error_message}));
  }
  return mojom::Error::New(error_code, kBackendName + error_message);
}

}  // namespace webnn::ort
