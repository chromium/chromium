// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/dml/platform_functions.h"

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/native_library.h"
#include "base/path_service.h"

namespace webnn::dml {

PlatformFunctions::PlatformFunctions() {
  // D3D12
  base::ScopedNativeLibrary d3d12_library(
      std::move(base::LoadSystemLibrary(L"D3D12.dll")));
  if (!d3d12_library.is_valid()) {
    DLOG(ERROR) << "Failed to load D3D12.dll.";
    return;
  }
  D3d12CreateDeviceProc d3d12_create_device_proc =
      reinterpret_cast<D3d12CreateDeviceProc>(
          d3d12_library.GetFunctionPointer("D3D12CreateDevice"));
  if (!d3d12_create_device_proc) {
    DLOG(ERROR) << "Failed to get D3D12CreateDevice function.";
    return;
  }

  D3d12GetDebugInterfaceProc d3d12_get_debug_interface_proc =
      reinterpret_cast<D3d12GetDebugInterfaceProc>(
          d3d12_library.GetFunctionPointer("D3D12GetDebugInterface"));
  if (!d3d12_get_debug_interface_proc) {
    DLOG(ERROR) << "Failed to get D3D12GetDebugInterface function.";
    return;
  }

  // First try to Load DirectML.dll from the module folder. It would enable
  // running unit tests which require DirectML feature level 4.0+ on Windows 10.
  base::ScopedNativeLibrary dml_library;
  base::FilePath module_path;
  if (base::PathService::Get(base::DIR_MODULE, &module_path)) {
    dml_library = base::ScopedNativeLibrary(
        base::LoadNativeLibrary(module_path.Append(L"directml.dll"), nullptr));
  }
  // If it failed to load from module folder, try to load from system folder.
  if (!dml_library.is_valid()) {
    dml_library =
        base::ScopedNativeLibrary(base::LoadSystemLibrary(L"directml.dll"));
  }
  if (!dml_library.is_valid()) {
    DLOG(ERROR) << "Failed to load directml.dll.";
    return;
  }
  DmlCreateDeviceProc dml_create_device_proc =
      reinterpret_cast<DmlCreateDeviceProc>(
          dml_library.GetFunctionPointer("DMLCreateDevice"));
  if (!dml_create_device_proc) {
    DLOG(ERROR) << "Failed to get DMLCreateDevice function.";
    return;
  }

  // D3D12
  d3d12_library_ = std::move(d3d12_library);
  d3d12_create_device_proc_ = std::move(d3d12_create_device_proc);
  d3d12_get_debug_interface_proc_ = std::move(d3d12_get_debug_interface_proc);
  // DirectML
  dml_library_ = std::move(dml_library);
  dml_create_device_proc_ = std::move(dml_create_device_proc);
}

// static
PlatformFunctions* PlatformFunctions::GetInstance() {
  static base::NoDestructor<PlatformFunctions> instance;
  if (!instance->AllFunctionsLoaded()) {
    DLOG(ERROR) << "Failed to load all platform functions.";
    return nullptr;
  }
  return instance.get();
}

bool PlatformFunctions::AllFunctionsLoaded() {
  return d3d12_create_device_proc_ && dml_create_device_proc_ &&
         d3d12_get_debug_interface_proc_;
}

}  // namespace webnn::dml
