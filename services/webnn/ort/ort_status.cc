// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/ort/ort_status.h"

#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "services/webnn/ort/platform_functions_ort.h"
#include "third_party/windows_app_sdk_headers/src/inc/abi/winml/winml/onnxruntime_c_api.h"

namespace webnn::ort {

namespace internal {

std::string OrtStatusErrorMessage(OrtStatus* status) {
  CHECK(status);

  constexpr char kOrtErrorCode[] = "[WebNN] ORT status error code: ";
  constexpr char kOrtErrorMessage[] = " error message: ";
  const OrtApi* ort_api = PlatformFunctions::GetInstance()->ort_api();
  return base::StrCat(
      {kOrtErrorCode,
       base::NumberToString(static_cast<int>(ort_api->GetErrorCode(status))),
       kOrtErrorMessage, ort_api->GetErrorMessage(status)});
}

}  // namespace internal

}  // namespace webnn::ort
