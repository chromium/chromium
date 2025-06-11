// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/ort/platform_functions_ort.h"

#include <windows.h>

#include <appmodel.h>
#include <winerror.h>
#include <wrl.h>

#include <optional>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/native_library.h"
#include "base/path_service.h"
#include "base/strings/string_util_win.h"

namespace webnn::ort {

namespace {

using OrtGetApiBaseProc = decltype(OrtGetApiBase)*;
using TryCreatePackageDependencyProc = decltype(TryCreatePackageDependency)*;
using AddPackageDependencyProc = decltype(AddPackageDependency)*;

constexpr int kMinVersionMajor = 0;
constexpr int kMinVersionMinor = 0;
constexpr int kMinVersionBuild = 0;
constexpr int kMinVersionRevision = 0;
constexpr wchar_t kWindowsMLPackageFamilyName[] =
    L"Microsoft.WindowsMLRuntime.0.3_8wekyb3d8bbwe";

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
    LOG(ERROR) << "Failed to get package path length for package: "
               << package_full_name << ". Error: "
               << logging::SystemErrorCodeToString(HRESULT_FROM_WIN32(result));
    return std::nullopt;
  }

  // Get the actual path.
  std::wstring path_buffer;
  result = GetPackagePathByFullName(package_full_name, &path_length,
                                    base::WriteInto(&path_buffer, path_length));
  if (result != ERROR_SUCCESS) {
    LOG(ERROR) << "Failed to get package path for package: "
               << package_full_name << ". Error: "
               << logging::SystemErrorCodeToString(HRESULT_FROM_WIN32(result));
    return std::nullopt;
  }

  return base::FilePath(path_buffer);
}

std::optional<base::FilePath> InitializeWindowsML() {
  // KernelBase should always be present on Win10+ machines.
  base::ScopedNativeLibrary app_model_library(
      base::LoadSystemLibrary(L"KernelBase.dll", nullptr));
  CHECK(app_model_library.is_valid());

  TryCreatePackageDependencyProc try_create_package_dependency_proc =
      reinterpret_cast<TryCreatePackageDependencyProc>(
          app_model_library.GetFunctionPointer("TryCreatePackageDependency"));
  AddPackageDependencyProc add_package_dependency_proc =
      reinterpret_cast<AddPackageDependencyProc>(
          app_model_library.GetFunctionPointer("AddPackageDependency"));
  if (!try_create_package_dependency_proc || !add_package_dependency_proc) {
    LOG(ERROR) << "Failed to get TryCreatePackageDependency and "
                  "AddPackageDependency functions from KernelBase.dll.";
    return std::nullopt;
  }

  PACKAGE_VERSION min_version = {.Major = kMinVersionMajor,
                                 .Minor = kMinVersionMinor,
                                 .Build = kMinVersionBuild,
                                 .Revision = kMinVersionRevision};
  ScopedWcharType package_dependency_id;
  HRESULT hr = try_create_package_dependency_proc(
      /*user=*/nullptr, kWindowsMLPackageFamilyName, min_version,
      PackageDependencyProcessorArchitectures_None,
      PackageDependencyLifetimeKind_Process,
      /*lifetimeArtifact=*/nullptr, CreatePackageDependencyOptions_None,
      ScopedWcharType::Receiver(package_dependency_id).get());
  if (FAILED(hr)) {
    LOG(ERROR) << "TryCreatePackageDependency failed for package: "
               << kWindowsMLPackageFamilyName
               << ". Error: " << logging::SystemErrorCodeToString(hr);
    return std::nullopt;
  }

  PACKAGEDEPENDENCY_CONTEXT context{};
  ScopedWcharType package_full_name;
  hr = add_package_dependency_proc(
      package_dependency_id.get(), /*rank=*/0,
      AddPackageDependencyOptions_PrependIfRankCollision, &context,
      ScopedWcharType::Receiver(package_full_name).get());
  if (FAILED(hr)) {
    LOG(ERROR) << "AddPackageDependency failed for package: "
               << package_full_name.get()
               << ". Error: " << logging::SystemErrorCodeToString(hr);
    return std::nullopt;
  }

  return GetPackagePath(package_full_name.get());
}

}  // namespace

PlatformFunctions::PlatformFunctions() {
  // First try to load onnxruntime.dll from the module folder. This enables
  // local testing using the latest redistributable onnxruntime.dll.
  base::ScopedNativeLibrary ort_library(
      base::LoadNativeLibrary(base::PathService::CheckedGet(base::DIR_MODULE)
                                  .Append(L"onnxruntime.dll"),
                              nullptr));

  // If it failed to load from module folder, try to load from the Windows ML
  // package.
  if (!ort_library.is_valid()) {
    // Initialize Windows ML.
    std::optional<base::FilePath> package_path = InitializeWindowsML();
    if (!package_path) {
      LOG(ERROR) << "Failed to initialize Windows ML and get the package path.";
      return;
    }

    ort_library = base::ScopedNativeLibrary(base::LoadNativeLibrary(
        package_path->Append(L"onnxruntime.dll"), nullptr));
    if (!ort_library.is_valid()) {
      LOG(ERROR) << "[WebNN] Failed to load onnxruntime.dll from package path: "
                 << package_path->value();
      return;
    }
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
