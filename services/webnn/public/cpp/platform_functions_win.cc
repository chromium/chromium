// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/public/cpp/platform_functions_win.h"

#include <winerror.h>
#include <wrl.h>

#include "base/logging.h"
#include "base/native_library.h"
#include "base/path_service.h"
#include "base/strings/string_util_win.h"
#include "third_party/windows_app_sdk_headers/src/inc/abi/runtime/WindowsAppSDK-VersionInfo.h"

namespace webnn {

namespace {

constexpr PACKAGE_VERSION kWinAppRuntimePackageVersion = {
    .Major = WINDOWSAPPSDK_RUNTIME_VERSION_MAJOR,
    .Minor = WINDOWSAPPSDK_RUNTIME_VERSION_MINOR,
    .Build = WINDOWSAPPSDK_RUNTIME_VERSION_BUILD,
    .Revision = WINDOWSAPPSDK_RUNTIME_VERSION_REVISION,
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

base::FilePath GetPackagePath(const wchar_t* package_full_name) {
  uint32_t path_length = 0;

  // Get the required path length.
  LONG result =
      GetPackagePathByFullName(package_full_name, &path_length, nullptr);
  if (result != ERROR_INSUFFICIENT_BUFFER) {
    LOG(ERROR) << "[WebNN] Failed to get package path length for package: "
               << package_full_name << ". Error: "
               << logging::SystemErrorCodeToString(HRESULT_FROM_WIN32(result));
    return base::FilePath();
  }

  // Get the actual path.
  std::wstring path_buffer;
  result = GetPackagePathByFullName(package_full_name, &path_length,
                                    base::WriteInto(&path_buffer, path_length));
  if (result != ERROR_SUCCESS) {
    LOG(ERROR) << "[WebNN] Failed to get package path for package: "
               << package_full_name << ". Error: "
               << logging::SystemErrorCodeToString(HRESULT_FROM_WIN32(result));
    return base::FilePath();
  }

  return base::FilePath(path_buffer);
}

}  // namespace

PlatformFunctionsWin::PlatformFunctionsWin() {
  app_model_library_ = base::ScopedNativeLibrary(
      base::LoadSystemLibrary(L"KernelBase.dll", nullptr));
  if (!app_model_library_.is_valid()) {
    LOG(ERROR) << "[WebNN] Failed to load KernelBase.dll.";
    return;
  }

  // This function was introduced in Windows 11 (version 10.0.22000.0).
  // https://learn.microsoft.com/en-us/windows/win32/api/appmodel/nf-appmodel-trycreatepackagedependency#requirements
  try_create_package_dependency_proc_ =
      reinterpret_cast<TryCreatePackageDependencyProc>(
          app_model_library_.GetFunctionPointer("TryCreatePackageDependency"));
  if (!try_create_package_dependency_proc_) {
    LOG(ERROR) << "[WebNN] Failed to get TryCreatePackageDependency function "
                  "from KernelBase.dll.";
    return;
  }

  // This function was introduced in Windows 11 (version 10.0.22000.0).
  // https://learn.microsoft.com/en-us/windows/win32/api/appmodel/nf-appmodel-addpackagedependency#requirements
  add_package_dependency_proc_ = reinterpret_cast<AddPackageDependencyProc>(
      app_model_library_.GetFunctionPointer("AddPackageDependency"));
  if (!add_package_dependency_proc_) {
    LOG(ERROR) << "[WebNN] Failed to get AddPackageDependency function from "
                  "KernelBase.dll.";
  }
}

PlatformFunctionsWin::~PlatformFunctionsWin() = default;

// static
PlatformFunctionsWin* PlatformFunctionsWin::GetInstance() {
  static base::NoDestructor<PlatformFunctionsWin> instance;
  if (!instance->AllFunctionsLoaded()) {
    return nullptr;
  }
  return instance.get();
}

base::FilePath PlatformFunctionsWin::InitializePackageDependency(
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
               << " . Error: " << logging::SystemErrorCodeToString(hr);
    return base::FilePath();
  }

  PACKAGEDEPENDENCY_CONTEXT context{};
  ScopedWcharType package_full_name;
  hr = add_package_dependency_proc_(
      package_dependency_id.get(), /*rank=*/0,
      AddPackageDependencyOptions_PrependIfRankCollision, &context,
      ScopedWcharType::Receiver(package_full_name).get());
  if (FAILED(hr)) {
    LOG(ERROR) << "[WebNN] AddPackageDependency failed for package: "
               << package_family_name
               << ". Error: " << logging::SystemErrorCodeToString(hr);
    return base::FilePath();
  }

  return GetPackagePath(package_full_name.get());
}

base::FilePath
PlatformFunctionsWin::InitializeWinAppRuntimePackageDependency() {
  return InitializePackageDependency(
      WINDOWSAPPSDK_RUNTIME_PACKAGE_FRAMEWORK_PACKAGEFAMILYNAME_W,
      kWinAppRuntimePackageVersion);
}

bool PlatformFunctionsWin::AllFunctionsLoaded() {
  return try_create_package_dependency_proc_ && add_package_dependency_proc_;
}

}  // namespace webnn
