// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_ORT_PLATFORM_FUNCTIONS_ORT_H_
#define SERVICES_WEBNN_ORT_PLATFORM_FUNCTIONS_ORT_H_

#include <windows.h>

#include <appmodel.h>

#include <optional>

#include "base/component_export.h"
#include "base/files/file_path.h"
#include "base/no_destructor.h"
#include "base/scoped_native_library.h"
#include "base/strings/cstring_view.h"
#include "third_party/onnxruntime_headers/src/include/onnxruntime/core/session/onnxruntime_c_api.h"

namespace webnn::ort {

class COMPONENT_EXPORT(WEBNN_SERVICE) PlatformFunctions {
 public:
  PlatformFunctions(const PlatformFunctions&) = delete;
  PlatformFunctions& operator=(const PlatformFunctions&) = delete;

  static PlatformFunctions* GetInstance();

  const OrtApi* ort_api() const { return ort_api_.get(); }
  const OrtModelEditorApi* ort_model_editor_api() const {
    return ort_model_editor_api_.get();
  }

  std::optional<base::FilePath> InitializePackageDependency(
      base::wcstring_view package_family_name,
      PACKAGE_VERSION min_version);

 private:
  friend class base::NoDestructor<PlatformFunctions>;

  PlatformFunctions();
  ~PlatformFunctions();

  bool AllFunctionsLoaded();

  // Library and functions for package dependency initialization.
  base::ScopedNativeLibrary app_model_library_;

  using TryCreatePackageDependencyProc = decltype(TryCreatePackageDependency)*;
  TryCreatePackageDependencyProc try_create_package_dependency_proc_ = nullptr;

  using AddPackageDependencyProc = decltype(AddPackageDependency)*;
  AddPackageDependencyProc add_package_dependency_proc_ = nullptr;

  base::ScopedNativeLibrary ort_library_;
  raw_ptr<const OrtApi> ort_api_ = nullptr;
  raw_ptr<const OrtModelEditorApi> ort_model_editor_api_ = nullptr;
};

}  // namespace webnn::ort

#endif  // SERVICES_WEBNN_ORT_PLATFORM_FUNCTIONS_ORT_H_
