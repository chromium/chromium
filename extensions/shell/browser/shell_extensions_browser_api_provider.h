// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_SHELL_BROWSER_SHELL_EXTENSIONS_BROWSER_API_PROVIDER_H_
#define EXTENSIONS_SHELL_BROWSER_SHELL_EXTENSIONS_BROWSER_API_PROVIDER_H_

#include "extensions/browser/extensions_browser_api_provider.h"

namespace extensions {

class ShellExtensionsBrowserAPIProvider : public ExtensionsBrowserAPIProvider {
 public:
  ShellExtensionsBrowserAPIProvider();
  ShellExtensionsBrowserAPIProvider(const ShellExtensionsBrowserAPIProvider&) =
      delete;
  ShellExtensionsBrowserAPIProvider& operator=(
      const ShellExtensionsBrowserAPIProvider&) = delete;
  ~ShellExtensionsBrowserAPIProvider() override;

  void RegisterExtensionFunctions(ExtensionFunctionRegistry* registry) override;
};

}  // namespace extensions

#endif  // EXTENSIONS_SHELL_BROWSER_SHELL_EXTENSIONS_BROWSER_API_PROVIDER_H_
