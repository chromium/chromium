// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_SYSTEM_MEMORY_SYSTEM_MEMORY_API_H_
#define EXTENSIONS_BROWSER_API_SYSTEM_MEMORY_SYSTEM_MEMORY_API_H_

#include "extensions/browser/extension_function.h"

namespace extensions {

class SystemMemoryGetInfoFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("system.memory.getInfo", SYSTEM_MEMORY_GETINFO)

  SystemMemoryGetInfoFunction() = default;
  SystemMemoryGetInfoFunction(const SystemMemoryGetInfoFunction&) = delete;
  SystemMemoryGetInfoFunction& operator=(const SystemMemoryGetInfoFunction&) =
      delete;

 private:
  ~SystemMemoryGetInfoFunction() override = default;

  // ExtensionFunction:
  ResponseAction Run() override;

  void OnGetMemoryInfoCompleted(bool success);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_SYSTEM_MEMORY_SYSTEM_MEMORY_API_H_
