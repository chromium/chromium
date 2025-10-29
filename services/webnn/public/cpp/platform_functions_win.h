// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_PUBLIC_CPP_PLATFORM_FUNCTIONS_WIN_H_
#define SERVICES_WEBNN_PUBLIC_CPP_PLATFORM_FUNCTIONS_WIN_H_

#include <windows.h>

#include <appmodel.h>

#include "base/component_export.h"
#include "base/files/file_path.h"
#include "base/no_destructor.h"
#include "base/scoped_native_library.h"
#include "base/strings/cstring_view.h"

namespace webnn {

class COMPONENT_EXPORT(WEBNN_PUBLIC_CPP) PlatformFunctionsWin {
 public:
  PlatformFunctionsWin(const PlatformFunctionsWin&) = delete;
  PlatformFunctionsWin& operator=(const PlatformFunctionsWin&) = delete;

  static PlatformFunctionsWin* GetInstance();

  base::FilePath InitializePackageDependency(
      base::wcstring_view package_family_name,
      PACKAGE_VERSION min_version);

  base::FilePath InitializeWinAppRuntimePackageDependency();

 private:
  friend class base::NoDestructor<PlatformFunctionsWin>;

  PlatformFunctionsWin();
  ~PlatformFunctionsWin();

  bool AllFunctionsLoaded();

  // Library and functions for package dependency initialization.
  base::ScopedNativeLibrary app_model_library_;

  using TryCreatePackageDependencyProc = decltype(TryCreatePackageDependency)*;
  TryCreatePackageDependencyProc try_create_package_dependency_proc_ = nullptr;

  using AddPackageDependencyProc = decltype(AddPackageDependency)*;
  AddPackageDependencyProc add_package_dependency_proc_ = nullptr;
};

}  // namespace webnn

#endif  // SERVICES_WEBNN_PUBLIC_CPP_PLATFORM_FUNCTIONS_WIN_H_
