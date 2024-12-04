// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/ort/platform_functions_ort.h"

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/native_library.h"
#include "base/path_service.h"

namespace webnn::ort {

PlatformFunctions::PlatformFunctions() {
  // First try to Load DirectML.dll from the module folder. It would enable
  // running unit tests which require DirectML feature level 4.0+ on Windows 10.
  base::ScopedNativeLibrary ort_library =
      base::ScopedNativeLibrary(base::LoadSystemLibrary(L"onnxruntime.dll"));

  if (!ort_library.is_valid()) {
    LOG(ERROR) << "[WebNN] Failed to load onnxruntime.dll.";
    return;
  }

  OrtGetApiBaseProc ort_get_api_base_proc = reinterpret_cast<OrtGetApiBaseProc>(
      ort_library.GetFunctionPointer("OrtGetApiBase"));
  if (!ort_get_api_base_proc) {
    LOG(ERROR) << "[WebNN] Failed to get OrtGetApiBase function.";
    return;
  }

  // ort
  ort_library_ = std::move(ort_library);
  ort_get_api_base_proc_ = std::move(ort_get_api_base_proc);
}

PlatformFunctions::~PlatformFunctions() = default;

// static
PlatformFunctions* PlatformFunctions::GetInstance() {
  static base::NoDestructor<PlatformFunctions> instance;
  return instance.get();
}

}  // namespace webnn::ort
