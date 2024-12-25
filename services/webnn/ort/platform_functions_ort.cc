// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/ort/platform_functions_ort.h"

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/native_library.h"
#include "base/path_service.h"
#include "third_party/onnx/proto/onnx.pb.h"

namespace webnn::ort {

PlatformFunctions::PlatformFunctions() {
  // First try to Load onnxruntime.dll from the module folder.
  base::ScopedNativeLibrary ort_library;
  base::FilePath module_path;
  if (base::PathService::Get(base::DIR_MODULE, &module_path)) {
    ort_library = base::ScopedNativeLibrary(base::LoadNativeLibrary(
        module_path.Append(L"onnxruntime.dll"), nullptr));
  }
  // Dll from system folder doesn't support OrtGraph API.
  // if (!ort_library.is_valid()) {
  //   ort_library =
  //       base::ScopedNativeLibrary(base::LoadSystemLibrary(L"onnxruntime.dll"));
  // }
  if (!ort_library.is_valid()) {
    LOG(ERROR) << "[WebNN] Failed to load onnxruntime.dll.";
    return;
  }

  // TODO: Consider checking the version of the loaded onnxruntime.dll before
  // loading all these functions to avoid runtime crashes.
  OrtGetApiBaseProc ort_get_api_base_proc = reinterpret_cast<OrtGetApiBaseProc>(
      ort_library.GetFunctionPointer("OrtGetApiBase"));
  if (!ort_get_api_base_proc) {
    LOG(ERROR) << "[WebNN] Failed to get OrtGetApiBase function.";
    return;
  }

  const OrtApi* ort_api =
      ort_get_api_base_proc()->GetApi(onnx::Version::IR_VERSION);
  if (!ort_api) {
    LOG(ERROR) << "[WebNN] Failed to get OrtApi.";
    return;
  }

  const OrtGraphApi* ort_graph_api = ort_api->GetGraphApi();
  if (!ort_graph_api) {
    LOG(ERROR) << "[WebNN] Failed to get OrtGraphApi.";
    return;
  }

  // ort
  ort_library_ = std::move(ort_library);
  ort_get_api_base_proc_ = std::move(ort_get_api_base_proc);
  ort_api_ = ort_api;
  ort_graph_api_ = ort_graph_api;
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
  return ort_get_api_base_proc_ && ort_api_ && ort_graph_api_;
}

}  // namespace webnn::ort
