// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/native_library.h"
#include "base/path_service.h"
#include "build/build_config.h"
#include "services/on_device_model/on_device_model_service.h"

#if defined(ENABLE_ML_INTERNAL)
#include "services/on_device_model/ml/chrome_ml.h"  // nogncheck
#endif

namespace on_device_model {

// static
bool OnDeviceModelService::PreSandboxInit() {
#if defined(ENABLE_ML_INTERNAL)
  // Ensure the library is loaded before the sandbox is initialized.
  if (!ml::ChromeML::Get()) {
    LOG(ERROR) << "Unable to load ChromeML.";
    return false;
  }
#endif

#if defined(DAWN_USE_BUILT_DXC) && BUILDFLAG(IS_WIN)
  base::FilePath module_path;
  if (base::PathService::Get(base::DIR_MODULE, &module_path)) {
    // Preload DXC requirements if enabled.
    base::LoadNativeLibrary(module_path.Append(L"dxil.dll"), nullptr);
    base::LoadNativeLibrary(module_path.Append(L"dxcompiler.dll"), nullptr);
  }
#endif

  return true;
}

}  // namespace on_device_model
