// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/ort/platform_functions_ort.h"

#include "base/base_paths_win.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/native_library.h"
#include "base/path_service.h"
#include "services/webnn/webnn_switches.h"

namespace webnn::ort {

PlatformFunctions::PlatformFunctions() {
  // If the switch `kWebNNOrtLibraryPath` is used, try to load onnxruntime.dll
  // from the specified path. Otherwise, try to load it from the module path.
  base::FilePath ort_library_path;
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kWebNNOrtLibraryPath)) {
    ort_library_path = base::CommandLine::ForCurrentProcess()
                           ->GetSwitchValuePath(switches::kWebNNOrtLibraryPath)
                           .Append(L"onnxruntime.dll");
  } else {
    ort_library_path = base::PathService::CheckedGet(base::DIR_MODULE)
                           .Append(L"onnxruntime.dll");
  }

  base::ScopedNativeLibrary ort_library(
      base::LoadNativeLibrary(ort_library_path, nullptr));
  if (!ort_library.is_valid()) {
    LOG(ERROR) << "[WebNN] Failed to load onnxruntime.dll from: "
               << ort_library_path;
    return;
  }

  OrtGetApiBaseProc ort_get_api_base_proc = reinterpret_cast<OrtGetApiBaseProc>(
      ort_library.GetFunctionPointer("OrtGetApiBase"));
  if (!ort_get_api_base_proc) {
    LOG(ERROR) << "[WebNN] Failed to get OrtGetApiBase function.";
    return;
  }

  // ORT_API_VERSION is defined in onnxruntime_c_api.h and must be passed to
  // `OrtApiBase::OrtApi()`.
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

  // ort
  ort_library_ = std::move(ort_library);
  ort_get_api_base_proc_ = std::move(ort_get_api_base_proc);
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

const OrtDmlApi* PlatformFunctions::ort_dml_api() {
  if (!ort_dml_api_.get()) {
    const OrtDmlApi* ort_dml_api;
    ort_api_->GetExecutionProviderApi(
        "DML", ORT_API_VERSION, reinterpret_cast<const void**>(&ort_dml_api));
    if (!ort_dml_api) {
      LOG(ERROR) << "[WebNN] Failed to get OrtDmlApi";
      return nullptr;
    }
    ort_dml_api_ = ort_dml_api;
  }
  return ort_dml_api_.get();
}

bool PlatformFunctions::AllFunctionsLoaded() {
  return ort_get_api_base_proc_ && ort_api_ && ort_model_editor_api_;
}

}  // namespace webnn::ort
