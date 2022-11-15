// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/core_extensions_browser_api_provider.h"

#include "extensions/browser/api/generated_api_registration.h"

namespace extensions {

CoreExtensionsBrowserAPIProvider::CoreExtensionsBrowserAPIProvider() = default;
CoreExtensionsBrowserAPIProvider::~CoreExtensionsBrowserAPIProvider() = default;

void CoreExtensionsBrowserAPIProvider::RegisterExtensionFunctions(
    ExtensionFunctionRegistry* registry) {
  api::GeneratedFunctionRegistry::RegisterAll(registry);
}

}  // namespace extensions
