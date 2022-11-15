// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_CORE_EXTENSIONS_BROWSER_API_PROVIDER_H_
#define EXTENSIONS_BROWSER_API_CORE_EXTENSIONS_BROWSER_API_PROVIDER_H_

#include "extensions/browser/extensions_browser_api_provider.h"

namespace extensions {

class CoreExtensionsBrowserAPIProvider : public ExtensionsBrowserAPIProvider {
 public:
  CoreExtensionsBrowserAPIProvider();
  CoreExtensionsBrowserAPIProvider(const CoreExtensionsBrowserAPIProvider&) =
      delete;
  CoreExtensionsBrowserAPIProvider& operator=(
      const CoreExtensionsBrowserAPIProvider&) = delete;
  ~CoreExtensionsBrowserAPIProvider() override;

  void RegisterExtensionFunctions(ExtensionFunctionRegistry* registry) override;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_CORE_EXTENSIONS_BROWSER_API_PROVIDER_H_
