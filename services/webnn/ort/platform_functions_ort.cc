// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/ort/platform_functions_ort.h"

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/native_library.h"
#include "base/path_service.h"

namespace webnn::ort {

using OrtGetApiBaseProc = decltype(OrtGetApiBase)*;

PlatformFunctions::PlatformFunctions() {
  // First try to load onnxruntime.dll from the module folder. This enables
  // local testing using the latest redistributable onnxruntime.dll.
  base::ScopedNativeLibrary ort_library(
      base::LoadNativeLibrary(base::PathService::CheckedGet(base::DIR_MODULE)
                                  .Append(L"onnxruntime.dll"),
                              nullptr));

  if (!ort_library.is_valid()) {
    ort_library =
        base::ScopedNativeLibrary(base::LoadSystemLibrary(L"onnxruntime.dll"));
  }
  if (!ort_library.is_valid()) {
    LOG(ERROR) << "[WebNN] Failed to load onnxruntime.dll.";
    return;
  }

  OrtGetApiBaseProc ort_get_api_base_proc = reinterpret_cast<OrtGetApiBaseProc>(
      ort_library.GetFunctionPointer("OrtGetApiBase"));
  if (!ort_get_api_base_proc) {
    LOG(ERROR) << "[WebNN] Failed to get OrtGetApiBase function.";
    return;
  }

  // Request the API version matching the headers we are built against.
  const OrtApi* ort_api = ort_get_api_base_proc()->GetApi(ORT_API_VERSION);
  if (!ort_api) {
    LOG(ERROR) << "[WebNN] Failed to get OrtApi for API Version "
               << ORT_API_VERSION;
    return;
  }

  const OrtModelEditorApi* ort_model_editor_api = ort_api->GetModelEditorApi();
  if (!ort_model_editor_api) {
    LOG(ERROR) << "[WebNN] Failed to get OrtModelEditorApi.";
    return;
  }

  ort_library_ = std::move(ort_library);
  ort_api_ = ort_api;
  ort_model_editor_api_ = ort_model_editor_api;
}

PlatformFunctions::~PlatformFunctions() = default;

// static
PlatformFunctions* PlatformFunctions::GetInstance() {
  static base::NoDestructor<PlatformFunctions> instance;
  if (!instance->AllFunctionsLoaded()) {
    return nullptr;
  }
  return instance.get();
}

bool PlatformFunctions::AllFunctionsLoaded() {
  return ort_api_ && ort_model_editor_api_;
}

}  // namespace webnn::ort
