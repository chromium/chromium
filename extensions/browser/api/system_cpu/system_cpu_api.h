// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef EXTENSIONS_BROWSER_API_SYSTEM_CPU_SYSTEM_CPU_API_H_
#define EXTENSIONS_BROWSER_API_SYSTEM_CPU_SYSTEM_CPU_API_H_

#include "extensions/browser/extension_function.h"
#include "extensions/common/api/system_cpu.h"

namespace extensions {

class SystemCpuGetInfoFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("system.cpu.getInfo", SYSTEM_CPU_GETINFO)

  SystemCpuGetInfoFunction() = default;
  SystemCpuGetInfoFunction(const SystemCpuGetInfoFunction&) = delete;
  SystemCpuGetInfoFunction& operator=(const SystemCpuGetInfoFunction&) = delete;

 private:
  ~SystemCpuGetInfoFunction() override = default;

  // ExtensionFunction:
  ResponseAction Run() override;

  void OnGetCpuInfoCompleted(bool success);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_SYSTEM_CPU_SYSTEM_CPU_API_H_
