// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/public/cpp/platform_functions_win.h"

#include <winerror.h>
#include <wrl.h>

#include "base/logging.h"
#include "base/scoped_generic.h"
#include "base/strings/string_util_win.h"
#include "services/webnn/public/cpp/win_app_runtime_package_info.h"

namespace webnn {

namespace {

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
  HMODULE kbase = ::GetModuleHandle(L"KernelBase.dll");

  // This function was introduced in Windows 11 (version 10.0.22000.0).
  // https://learn.microsoft.com/en-us/windows/win32/api/appmodel/nf-appmodel-trycreatepackagedependency#requirements
  try_create_package_dependency_proc_ =
      reinterpret_cast<TryCreatePackageDependencyProc>(
          ::GetProcAddress(kbase, "TryCreatePackageDependency"));
  if (!try_create_package_dependency_proc_) {
    LOG(ERROR) << "[WebNN] Failed to get TryCreatePackageDependency function "
                  "from KernelBase.dll.";
    return;
  }

  // This function was introduced in Windows 11 (version 10.0.22000.0).
  // https://learn.microsoft.com/en-us/windows/win32/api/appmodel/nf-appmodel-addpackagedependency#requirements
  add_package_dependency_proc_ = reinterpret_cast<AddPackageDependencyProc>(
      ::GetProcAddress(kbase, "AddPackageDependency"));
  if (!add_package_dependency_proc_) {
    LOG(ERROR) << "[WebNN] Failed to get AddPackageDependency function from "
                  "KernelBase.dll.";
  }

  // This function was introduced in Windows 11 (version 10.0.22000.0).
  // https://learn.microsoft.com/en-us/windows/win32/api/appmodel/nf-appmodel-deletepackagedependency#requirements
  delete_package_dependency_proc_ =
      reinterpret_cast<DeletePackageDependencyProc>(
          ::GetProcAddress(kbase, "DeletePackageDependency"));
  if (!delete_package_dependency_proc_) {
    LOG(ERROR) << "[WebNN] Failed to get DeletePackageDependency function from "
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

base::FilePath PlatformFunctionsWin::InitializePackageDependencyForProcess(
    base::wcstring_view package_family_name,
    PACKAGE_VERSION min_version) {
  std::wstring dependency_id = TryCreatePackageDependency(
      package_family_name, min_version, PackageDependencyLifetimeKind_Process,
      /*lifetime_artifact=*/nullptr);
  if (dependency_id.empty()) {
    return base::FilePath();
  }
  return AddPackageDependency(dependency_id);
}

std::wstring PlatformFunctionsWin::TryCreatePackageDependencyForFilePath(
    base::wcstring_view package_family_name,
    PACKAGE_VERSION min_version,
    const base::FilePath& file_path) {
  return TryCreatePackageDependency(package_family_name, min_version,
                                    PackageDependencyLifetimeKind_FilePath,
                                    file_path.value().c_str());
}

base::FilePath PlatformFunctionsWin::AddPackageDependency(
    base::wcstring_view dependency_id) {
  PACKAGEDEPENDENCY_CONTEXT context{};
  ScopedWcharType package_full_name;
  HRESULT hr = add_package_dependency_proc_(
      dependency_id.c_str(), /*rank=*/0,
      AddPackageDependencyOptions_PrependIfRankCollision, &context,
      ScopedWcharType::Receiver(package_full_name).get());
  if (FAILED(hr)) {
    LOG(WARNING) << "[WebNN] AddPackageDependency failed for dependency ID: "
                 << dependency_id
                 << " . Error: " << logging::SystemErrorCodeToString(hr);
    return base::FilePath();
  }
  return GetPackagePath(package_full_name.get());
}

bool PlatformFunctionsWin::DeletePackageDependency(
    base::wcstring_view dependency_id) {
  HRESULT hr = delete_package_dependency_proc_(dependency_id.c_str());
  if (FAILED(hr)) {
    LOG(WARNING) << "[WebNN] DeletePackageDependency failed for dependency ID: "
                 << dependency_id
                 << " . Error: " << logging::SystemErrorCodeToString(hr);
    return false;
  }
  return true;
}

std::wstring PlatformFunctionsWin::TryCreatePackageDependency(
    base::wcstring_view package_family_name,
    PACKAGE_VERSION min_version,
    PackageDependencyLifetimeKind lifetime_kind,
    const wchar_t* lifetime_artifact) {
  ScopedWcharType package_dependency_id;
  HRESULT hr = try_create_package_dependency_proc_(
      /*user=*/nullptr, package_family_name.c_str(), min_version,
      PackageDependencyProcessorArchitectures_None, lifetime_kind,
      lifetime_artifact, CreatePackageDependencyOptions_None,
      ScopedWcharType::Receiver(package_dependency_id).get());
  if (FAILED(hr)) {
    LOG(WARNING) << "[WebNN] TryCreatePackageDependency failed for package: "
                 << package_family_name
                 << " . Error: " << logging::SystemErrorCodeToString(hr);
    return {};
  }
  return package_dependency_id.get();
}

bool PlatformFunctionsWin::AllFunctionsLoaded() {
  return try_create_package_dependency_proc_ && add_package_dependency_proc_ &&
         delete_package_dependency_proc_;
}

}  // namespace webnn
