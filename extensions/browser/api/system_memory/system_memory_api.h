// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_SYSTEM_MEMORY_SYSTEM_MEMORY_API_H_
#define EXTENSIONS_BROWSER_API_SYSTEM_MEMORY_SYSTEM_MEMORY_API_H_

#include "extensions/browser/extension_function.h"
#include "extensions/common/api/system_memory.h"

namespace extensions {

class SystemMemoryGetInfoFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("system.memory.getInfo", SYSTEM_MEMORY_GETINFO)
  SystemMemoryGetInfoFunction();

 private:
  ~SystemMemoryGetInfoFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

  void OnGetMemoryInfoCompleted(bool success);

  DISALLOW_COPY_AND_ASSIGN(SystemMemoryGetInfoFunction);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_SYSTEM_MEMORY_SYSTEM_MEMORY_API_H_
