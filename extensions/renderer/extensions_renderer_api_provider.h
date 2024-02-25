// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_EXTENSIONS_RENDERER_API_PROVIDER_H_
#define EXTENSIONS_RENDERER_EXTENSIONS_RENDERER_API_PROVIDER_H_

namespace extensions {

class ScriptContext;
class ResourceBundleSourceMap;

// An interface class that is owned by extensions::Dispatcher.
// ExtensionsRendererAPIProvider can be used to override and extend the behavior
// of the extensions system's renderer side from outside of extensions
// directories.
class ExtensionsRendererAPIProvider {
 public:
  virtual ~ExtensionsRendererAPIProvider() = default;

  // Blink maintains an allowlist for custom element names. This method
  // provides the delegate the ability to add more names to that allowlist.
  virtual void EnableCustomElementAllowlist() = 0;

  // Includes additional source resources into the resource map.
  virtual void PopulateSourceMap(ResourceBundleSourceMap* source_map) = 0;

  // Requires modules for defining WebView APIs within a ScriptContext's
  // ModuleSystem.
  virtual bool RequireWebViewModules(ScriptContext* context) = 0;
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_EXTENSIONS_RENDERER_API_PROVIDER_H_
