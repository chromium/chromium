// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef EXTENSIONS_BROWSER_API_SYSTEM_CPU_SYSTEM_CPU_API_H_
#define EXTENSIONS_BROWSER_API_SYSTEM_CPU_SYSTEM_CPU_API_H_

#include "extensions/common/api/system_cpu.h"
#include "extensions/browser/extension_function.h"

namespace extensions {

class SystemCpuGetInfoFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("system.cpu.getInfo", SYSTEM_CPU_GETINFO)
  SystemCpuGetInfoFunction();

 private:
  ~SystemCpuGetInfoFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

  void OnGetCpuInfoCompleted(bool success);

  DISALLOW_COPY_AND_ASSIGN(SystemCpuGetInfoFunction);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_SYSTEM_CPU_SYSTEM_CPU_API_H_
