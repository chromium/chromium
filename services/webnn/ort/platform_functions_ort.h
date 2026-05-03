// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_ORT_PLATFORM_FUNCTIONS_ORT_H_
#define SERVICES_WEBNN_ORT_PLATFORM_FUNCTIONS_ORT_H_

#include <windows.h>

#include <appmodel.h>

#include "base/component_export.h"
#include "base/files/file_path.h"
#include "base/scoped_native_library.h"
#include "base/strings/cstring_view.h"
#include "third_party/windows_app_sdk_headers/src/inc/abi/winml/winml/onnxruntime_c_api.h"

namespace webnn::ort {

class COMPONENT_EXPORT(WEBNN_SERVICE) PlatformFunctions {
 public:
  PlatformFunctions(const PlatformFunctions&) = delete;
  PlatformFunctions& operator=(const PlatformFunctions&) = delete;
  ~PlatformFunctions();

  // Ensures PlatformFunctions has been initialized. Returns true if an instance
  // is available.
  static bool EnsureInitialized();

  // Returns the initialized instance. CHECKs that the instance has been
  // initialized via EnsureInitialized().
  static PlatformFunctions* GetInstance();

  const OrtApi* ort_api() const { return ort_api_.get(); }
  const OrtModelEditorApi* ort_model_editor_api() const {
    return ort_model_editor_api_.get();
  }

  static base::FilePath InitializePackageDependency(
      base::wcstring_view package_family_name,
      PACKAGE_VERSION min_version);

 private:
  PlatformFunctions(base::ScopedNativeLibrary ort_library,
                    const OrtApi* ort_api,
                    const OrtModelEditorApi* ort_model_editor_api);

  // Tries to load onnxruntime.dll from the path specified by the
  // `kWebNNOrtLibraryPathForTesting` command-line switch. Returns false if
  // the switch is not set or the library cannot be loaded.
  static bool InitializeFromCommandLine();

  // Tries to load onnxruntime.dll from the WinML package. Returns false if
  // the Windows version is too old or the WinML package cannot be loaded.
  static bool InitializeWinML();

  // Loads onnxruntime.dll from the given path, resolves APIs, and sets the
  // global instance. Returns true on success.
  static bool InitializeFromPath(const base::FilePath& ort_library_path);

  static PlatformFunctions* g_instance_;

  base::ScopedNativeLibrary ort_library_;
  raw_ptr<const OrtApi> ort_api_ = nullptr;
  raw_ptr<const OrtModelEditorApi> ort_model_editor_api_ = nullptr;
};

}  // namespace webnn::ort

#endif  // SERVICES_WEBNN_ORT_PLATFORM_FUNCTIONS_ORT_H_
