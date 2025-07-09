// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/ort/platform_functions_ort.h"

#include <winerror.h>
#include <wrl.h>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/native_library.h"
#include "base/path_service.h"
#include "base/strings/string_util_win.h"
#include "services/webnn/webnn_switches.h"

namespace webnn::ort {

namespace {

using OrtGetApiBaseProc = decltype(OrtGetApiBase)*;

constexpr base::wcstring_view kWindowsMLPackageFamilyName =
    L"Microsoft.WindowsMLRuntime.0.3_8wekyb3d8bbwe";

constexpr base::wcstring_view kOnnxRuntimeLibraryName = L"onnxruntime.dll";

constexpr PACKAGE_VERSION kWindowsMLPackageVersion = {
    .Major = 0,
    .Minor = 0,
    .Build = 0,
    .Revision = 0,
};

struct ScopedWcharTypeTraits {
  static wchar_t* InvalidValue() { return nullptr; }
  static void Free(wchar_t* value) {
    if (value) {
      ::HeapFree(GetProcessHeap(), 0, value);
    }
  }
};
using ScopedWcharType = base::ScopedGeneric<wchar_t*, ScopedWcharTypeTraits>;

std::optional<base::FilePath> GetPackagePath(const wchar_t* package_full_name) {
  uint32_t path_length = 0;

  // Get the required path length.
  int32_t result =
      GetPackagePathByFullName(package_full_name, &path_length, nullptr);
  if (result != ERROR_INSUFFICIENT_BUFFER) {
    LOG(ERROR) << "[WebNN] Failed to get package path length for package: "
               << package_full_name << ". Error: "
               << logging::SystemErrorCodeToString(HRESULT_FROM_WIN32(result));
    return std::nullopt;
  }

  // Get the actual path.
  std::wstring path_buffer;
  result = GetPackagePathByFullName(package_full_name, &path_length,
                                    base::WriteInto(&path_buffer, path_length));
  if (result != ERROR_SUCCESS) {
    LOG(ERROR) << "[WebNN] Failed to get package path for package: "
               << package_full_name << ". Error: "
               << logging::SystemErrorCodeToString(HRESULT_FROM_WIN32(result));
    return std::nullopt;
  }

  return base::FilePath(path_buffer);
}

}  // namespace

PlatformFunctions::PlatformFunctions() {
  // KernelBase should always be present on Win10+ machines.
  app_model_library_ = base::ScopedNativeLibrary(
      base::LoadSystemLibrary(L"KernelBase.dll", nullptr));
  CHECK(app_model_library_.is_valid());

  try_create_package_dependency_proc_ =
      reinterpret_cast<TryCreatePackageDependencyProc>(
          app_model_library_.GetFunctionPointer("TryCreatePackageDependency"));
  if (!try_create_package_dependency_proc_) {
    LOG(ERROR) << "[WebNN] Failed to get TryCreatePackageDependency function "
                  "from KernelBase.dll.";
    return;
  }

  add_package_dependency_proc_ = reinterpret_cast<AddPackageDependencyProc>(
      app_model_library_.GetFunctionPointer("AddPackageDependency"));
  if (!add_package_dependency_proc_) {
    LOG(ERROR) << "[WebNN] Failed to get AddPackageDependency function from "
                  "KernelBase.dll.";
    return;
  }

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
    // Initialize Windows ML.
    std::optional<base::FilePath> windows_ml_package_path =
        InitializePackageDependency(kWindowsMLPackageFamilyName,
                                    kWindowsMLPackageVersion);
    if (!windows_ml_package_path) {
      LOG(ERROR) << "[WebNN] Failed to initialize Windows ML and get the "
                    "package path.";
      return;
    }
    ort_library_path = windows_ml_package_path->Append(kOnnxRuntimeLibraryName);
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

PlatformFunctions::~PlatformFunctions() = default;

// static
PlatformFunctions* PlatformFunctions::GetInstance() {
  static base::NoDestructor<PlatformFunctions> instance;
  if (!instance->AllFunctionsLoaded()) {
    return nullptr;
  }
  return instance.get();
}

std::optional<base::FilePath> PlatformFunctions::InitializePackageDependency(
    base::wcstring_view package_family_name,
    PACKAGE_VERSION min_version) {
  ScopedWcharType package_dependency_id;
  HRESULT hr = try_create_package_dependency_proc_(
      /*user=*/nullptr, package_family_name.c_str(), min_version,
      PackageDependencyProcessorArchitectures_None,
      PackageDependencyLifetimeKind_Process,
      /*lifetimeArtifact=*/nullptr, CreatePackageDependencyOptions_None,
      ScopedWcharType::Receiver(package_dependency_id).get());
  if (FAILED(hr)) {
    LOG(ERROR) << "[WebNN] TryCreatePackageDependency failed for package: "
               << package_family_name
               << ". Error: " << logging::SystemErrorCodeToString(hr);
    return std::nullopt;
  }

  PACKAGEDEPENDENCY_CONTEXT context{};
  ScopedWcharType package_full_name;
  hr = add_package_dependency_proc_(
      package_dependency_id.get(), /*rank=*/0,
      AddPackageDependencyOptions_PrependIfRankCollision, &context,
      ScopedWcharType::Receiver(package_full_name).get());
  if (FAILED(hr)) {
    LOG(ERROR) << "[WebNN] AddPackageDependency failed for package: "
               << package_full_name.get()
               << ". Error: " << logging::SystemErrorCodeToString(hr);
    return std::nullopt;
  }

  return GetPackagePath(package_full_name.get());
}

bool PlatformFunctions::AllFunctionsLoaded() {
  return ort_api_ && ort_model_editor_api_ &&
         try_create_package_dependency_proc_ && add_package_dependency_proc_;
}

}  // namespace webnn::ort
