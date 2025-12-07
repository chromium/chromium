// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/public/cpp/platform_functions_win.h"

#include <winerror.h>
#include <wrl.h>

#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
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
    return base::FilePath();
  }

  // Get the actual path.
  std::wstring path_buffer;
  result = GetPackagePathByFullName(package_full_name, &path_length,
                                    base::WriteInto(&path_buffer, path_length));
  if (result != ERROR_SUCCESS) {
    return base::FilePath();
  }

  return base::FilePath(path_buffer);
}

// This class provides functions for managing the package dependencies of
// Windows Store apps.
class PlatformFunctionsWin {
 public:
  ~PlatformFunctionsWin() = delete;
  PlatformFunctionsWin(const PlatformFunctionsWin&) = delete;
  PlatformFunctionsWin& operator=(const PlatformFunctionsWin&) = delete;

  static PlatformFunctionsWin* GetInstance() {
    static base::NoDestructor<PlatformFunctionsWin> instance;
    return instance.get();
  }

  std::wstring TryCreatePackageDependency(
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
      base::UmaHistogramSparse(
          "WebNN.ORT.TryCreatePackageDependency.ErrorResult", hr);
      return {};
    }
    return package_dependency_id.get();
  }

  base::FilePath AddPackageDependency(base::wcstring_view dependency_id) {
    PACKAGEDEPENDENCY_CONTEXT context{};
    ScopedWcharType package_full_name;
    HRESULT hr = add_package_dependency_proc_(
        dependency_id.c_str(), /*rank=*/0,
        AddPackageDependencyOptions_PrependIfRankCollision, &context,
        ScopedWcharType::Receiver(package_full_name).get());
    if (FAILED(hr)) {
      base::UmaHistogramSparse("WebNN.ORT.AddPackageDependency.ErrorResult",
                               hr);
      return base::FilePath();
    }
    return GetPackagePath(package_full_name.get());
  }

  bool DeletePackageDependency(base::wcstring_view dependency_id) {
    HRESULT hr = delete_package_dependency_proc_(dependency_id.c_str());
    if (FAILED(hr)) {
      base::UmaHistogramSparse("WebNN.ORT.DeletePackageDependency.ErrorResult",
                               hr);
      return false;
    }
    return true;
  }

 private:
  friend class base::NoDestructor<PlatformFunctionsWin>;

  PlatformFunctionsWin() {
    HMODULE kbase = ::GetModuleHandle(L"KernelBase.dll");

    // This function was introduced in Windows 11 Version 21H2
    // https://learn.microsoft.com/en-us/windows/win32/api/appmodel/nf-appmodel-trycreatepackagedependency#requirements
    try_create_package_dependency_proc_ =
        reinterpret_cast<TryCreatePackageDependencyProc>(
            ::GetProcAddress(kbase, "TryCreatePackageDependency"));
    CHECK(try_create_package_dependency_proc_);

    // This function was introduced in Windows 11 Version 21H2
    // https://learn.microsoft.com/en-us/windows/win32/api/appmodel/nf-appmodel-addpackagedependency#requirements
    add_package_dependency_proc_ = reinterpret_cast<AddPackageDependencyProc>(
        ::GetProcAddress(kbase, "AddPackageDependency"));
    CHECK(add_package_dependency_proc_);

    // This function was introduced in Windows 11 Version 21H2
    // https://learn.microsoft.com/en-us/windows/win32/api/appmodel/nf-appmodel-deletepackagedependency#requirements
    delete_package_dependency_proc_ =
        reinterpret_cast<DeletePackageDependencyProc>(
            ::GetProcAddress(kbase, "DeletePackageDependency"));
    CHECK(delete_package_dependency_proc_);
  }

  using TryCreatePackageDependencyProc =
      decltype(::TryCreatePackageDependency)*;
  TryCreatePackageDependencyProc try_create_package_dependency_proc_ = nullptr;

  using AddPackageDependencyProc = decltype(::AddPackageDependency)*;
  AddPackageDependencyProc add_package_dependency_proc_ = nullptr;

  using DeletePackageDependencyProc = decltype(::DeletePackageDependency)*;
  DeletePackageDependencyProc delete_package_dependency_proc_ = nullptr;
};

}  // namespace

base::FilePath InitializePackageDependencyForProcess(
    base::wcstring_view package_family_name,
    PACKAGE_VERSION min_version) {
  std::wstring dependency_id =
      TryCreatePackageDependencyForProcess(package_family_name, min_version);
  if (dependency_id.empty()) {
    return base::FilePath();
  }
  return AddPackageDependency(dependency_id);
}

std::wstring TryCreatePackageDependencyForFilePath(
    base::wcstring_view package_family_name,
    PACKAGE_VERSION min_version,
    const base::FilePath& file_path) {
  auto* platform_functions = PlatformFunctionsWin::GetInstance();
  return platform_functions->TryCreatePackageDependency(
      package_family_name, min_version, PackageDependencyLifetimeKind_FilePath,
      file_path.value().c_str());
}

std::wstring TryCreatePackageDependencyForProcess(
    base::wcstring_view package_family_name,
    PACKAGE_VERSION min_version) {
  auto* platform_functions = PlatformFunctionsWin::GetInstance();
  return platform_functions->TryCreatePackageDependency(
      package_family_name, min_version, PackageDependencyLifetimeKind_Process,
      /*lifetime_artifact=*/nullptr);
}

base::FilePath AddPackageDependency(base::wcstring_view dependency_id) {
  auto* platform_functions = PlatformFunctionsWin::GetInstance();
  return platform_functions->AddPackageDependency(dependency_id);
}

bool DeletePackageDependency(base::wcstring_view dependency_id) {
  auto* platform_functions = PlatformFunctionsWin::GetInstance();
  return platform_functions->DeletePackageDependency(dependency_id);
}

}  // namespace webnn
