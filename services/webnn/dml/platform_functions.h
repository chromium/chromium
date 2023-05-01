// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_DML_PLATFORM_FUNCTIONS_H_
#define SERVICES_WEBNN_DML_PLATFORM_FUNCTIONS_H_

#include <DirectML.h>
#include <d3d12.h>
#include <windows.h>

#include "base/no_destructor.h"
#include "base/scoped_native_library.h"

namespace webnn::dml {

class PlatformFunctions {
 public:
  PlatformFunctions(const PlatformFunctions&) = delete;
  PlatformFunctions& operator=(const PlatformFunctions&) = delete;

  static PlatformFunctions* GetInstance();

  using D3d12CreateDeviceProc = PFN_D3D12_CREATE_DEVICE;
  D3d12CreateDeviceProc d3d12_create_device_proc() const {
    return d3d12_create_device_proc_;
  }

  using DmlCreateDeviceProc = decltype(DMLCreateDevice)*;
  DmlCreateDeviceProc dml_create_device_proc() const {
    return dml_create_device_proc_;
  }

 private:
  friend class base::NoDestructor<PlatformFunctions>;
  PlatformFunctions();
  ~PlatformFunctions() = default;

  bool AllFunctionsLoaded();

  // D3D12
  base::ScopedNativeLibrary d3d12_library_;
  D3d12CreateDeviceProc d3d12_create_device_proc_;
  // DirectML
  base::ScopedNativeLibrary dml_library_;
  DmlCreateDeviceProc dml_create_device_proc_;
};

}  // namespace webnn::dml

#endif  // SERVICES_WEBNN_DML_PLATFORM_FUNCTIONS_H_
