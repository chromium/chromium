// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/ort/platform_functions_ort.h"

#include <winerror.h>
#include <wrl.h>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/path_service.h"
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

// The PlatformFunctions instance is a process-lifetime object. Once
// constructed, it is never destroyed (the destructor is never called).
// static
PlatformFunctions* PlatformFunctions::g_instance_ = nullptr;

// static
bool PlatformFunctions::InitializeFromCommandLine() {
  CHECK(!g_instance_);

  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kWebNNOrtLibraryPathForTesting)) {
    return false;
  }

  base::FilePath base_path =
      base::CommandLine::ForCurrentProcess()->GetSwitchValuePath(
          switches::kWebNNOrtLibraryPathForTesting);
  if (base_path.empty()) {
    LOG(ERROR) << "[WebNN] The specified ONNX Runtime library path is empty.";
    return false;
  }

  return InitializeFromPath(base_path.Append(kOnnxRuntimeLibraryName));
}

// static
bool PlatformFunctions::InitializeWinML() {
  CHECK(!g_instance_);

  if (base::win::GetVersion() < kWinAppRuntimeSupportedMinVersion) {
    return false;
  }

  base::FilePath package_path = InitializePackageDependency(
      kWinAppRuntimePackageFamilyName, kWinAppRuntimePackageMinVersion);
  if (package_path.empty()) {
    return false;
  }

  return InitializeFromPath(package_path.Append(kOnnxRuntimeLibraryName));
}

// static
bool PlatformFunctions::EnsureInitialized() {
  // The static local with a lambda initializer guarantees thread-safe
  // one-time initialization per the C++ standard ([stmt.dcl]).
  static bool initialized = []() {
    if (InitializeFromCommandLine()) {
      return true;
    }
    return InitializeWinML();
  }();

  return initialized;
}

// static
PlatformFunctions* PlatformFunctions::GetInstance() {
  CHECK(g_instance_);
  return g_instance_;
}

// static
bool PlatformFunctions::InitializeFromPath(
    const base::FilePath& ort_library_path) {
  base::ScopedNativeLibrary ort_library = base::ScopedNativeLibrary(
      base::LoadNativeLibrary(ort_library_path, nullptr));
  if (!ort_library.is_valid()) {
    LOG(ERROR) << "[WebNN] Failed to load ONNX Runtime library from the path: "
               << ort_library_path;
    return false;
  }

  OrtGetApiBaseProc ort_get_api_base_proc = reinterpret_cast<OrtGetApiBaseProc>(
      ort_library.GetFunctionPointer("OrtGetApiBase"));
  if (!ort_get_api_base_proc) {
    LOG(ERROR) << "[WebNN] Failed to get OrtGetApiBase function.";
    return false;
  }

  // Request the API version matching the headers we are built against.
  const OrtApi* ort_api = ort_get_api_base_proc()->GetApi(ORT_API_VERSION);
  if (!ort_api) {
    LOG(ERROR) << "[WebNN] Failed to get OrtApi for API Version "
               << ORT_API_VERSION;
    return false;
  }

  const OrtModelEditorApi* ort_model_editor_api = ort_api->GetModelEditorApi();
  if (!ort_model_editor_api) {
    LOG(ERROR) << "[WebNN] Failed to get OrtModelEditorApi.";
    return false;
  }

  g_instance_ = new PlatformFunctions(std::move(ort_library), ort_api,
                                      ort_model_editor_api);
  return true;
}

PlatformFunctions::PlatformFunctions(
    base::ScopedNativeLibrary ort_library,
    const OrtApi* ort_api,
    const OrtModelEditorApi* ort_model_editor_api)
    : ort_library_(std::move(ort_library)),
      ort_api_(ort_api),
      ort_model_editor_api_(ort_model_editor_api) {}

PlatformFunctions::~PlatformFunctions() = default;

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
