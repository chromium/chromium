// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_EXTENSIONS_BROWSER_API_PROVIDER_H_
#define EXTENSIONS_BROWSER_EXTENSIONS_BROWSER_API_PROVIDER_H_

class ExtensionFunctionRegistry;
namespace extensions {

// A class to provide browser-side, API-specific knowledge to the extensions
// system. This allows for composition of multiple providers, so that we can
// easily add or subtract features in different configurations.
class ExtensionsBrowserAPIProvider {
 public:
  ExtensionsBrowserAPIProvider() = default;
  ExtensionsBrowserAPIProvider(const ExtensionsBrowserAPIProvider&) = delete;
  ExtensionsBrowserAPIProvider& operator=(const ExtensionsBrowserAPIProvider&) =
      delete;
  virtual ~ExtensionsBrowserAPIProvider() = default;

  // Registers any API functions in the given |registry|.
  virtual void RegisterExtensionFunctions(
      ExtensionFunctionRegistry* registry) = 0;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_EXTENSIONS_BROWSER_API_PROVIDER_H_
