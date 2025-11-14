// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/ort/platform_functions_ort.h"

#include <winerror.h>
#include <wrl.h>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/strings/cstring_view.h"
#include "base/strings/string_util_win.h"
#include "services/webnn/public/cpp/platform_functions_win.h"
#include "services/webnn/public/cpp/win_app_runtime_package_info.h"
#include "services/webnn/webnn_switches.h"

namespace webnn::ort {

namespace {

using OrtGetApiBaseProc = decltype(OrtGetApiBase)*;

constexpr base::wcstring_view kOnnxRuntimeLibraryName = L"onnxruntime.dll";

}  // namespace

PlatformFunctions::PlatformFunctions() {
  // If the switch `kWebNNOrtLibraryPathForTesting` is used, try to load ONNX
  // Runtime library from the specified path for testing development ORT build.
  // Otherwise, try to load it from the Windows ML package path.
  base::FilePath ort_library_path;
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kWebNNOrtLibraryPathForTesting)) {
    base::FilePath base_path =
        base::CommandLine::ForCurrentProcess()->GetSwitchValuePath(
            switches::kWebNNOrtLibraryPathForTesting);
    if (base_path.empty()) {
      LOG(ERROR) << "[WebNN] The specified ONNX Runtime library path is empty.";
      return;
    }
    ort_library_path = base_path.Append(kOnnxRuntimeLibraryName);
  } else {
    ort_library_path = InitializePackageDependency(
        kWinAppRuntimePackageFamilyName, kWinAppRuntimePackageMinVersion);
    if (ort_library_path.empty()) {
      return;
    }
    ort_library_path = ort_library_path.Append(kOnnxRuntimeLibraryName);
  }

  base::ScopedNativeLibrary ort_library = base::ScopedNativeLibrary(
      base::LoadNativeLibrary(ort_library_path, nullptr));
  if (!ort_library.is_valid()) {
    LOG(ERROR) << "[WebNN] Failed to load ONNX Runtime library from the path: "
               << ort_library_path;
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

base::FilePath PlatformFunctions::InitializePackageDependency(
    base::wcstring_view package_family_name,
    PACKAGE_VERSION min_version) {
  std::wstring dependency_id =
      TryCreatePackageDependencyForProcess(package_family_name, min_version);
  if (dependency_id.empty()) {
    LOG(ERROR) << "[WebNN] Failed to create package dependency for package: "
               << package_family_name;
    return base::FilePath();
  }

  base::FilePath package_path = AddPackageDependency(dependency_id);
  if (package_path.empty()) {
    LOG(ERROR) << "[WebNN] Failed to add package dependency for package: "
               << package_family_name;
  }
  return package_path;
}

}  // namespace webnn::ort
