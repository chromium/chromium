// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_EXTENSIONS_RENDERER_API_PROVIDER_H_
#define EXTENSIONS_RENDERER_EXTENSIONS_RENDERER_API_PROVIDER_H_

namespace extensions {

class Dispatcher;
class ModuleSystem;
class NativeExtensionBindingsSystem;
class ScriptContext;
class ResourceBundleSourceMap;

// An interface class that is owned by extensions::Dispatcher.
// ExtensionsRendererAPIProvider can be used to override and extend the behavior
// of the extensions system's renderer side from outside of extensions
// directories.
class ExtensionsRendererAPIProvider {
 public:
  virtual ~ExtensionsRendererAPIProvider() = default;

  // Registers any native handlers to provide additional functionality for
  // native bindings. Called each time a new ScriptContext is created, since
  // native handlers are per-context.
  virtual void RegisterNativeHandlers(
      ModuleSystem* module_system,
      NativeExtensionBindingsSystem* bindings_system,
      ScriptContext* context) = 0;

  // Registers any additional hooks associated with specific APIs to the API
  // bindings system. Called once per NativeExtensionBindingsSystem, which is
  // one-per-thread and re-used across ScriptContexts.
  virtual void AddBindingsSystemHooks(
      Dispatcher* dispatcher,
      NativeExtensionBindingsSystem* bindings_system) = 0;

  // Includes additional source resources into the resource map.
  virtual void PopulateSourceMap(ResourceBundleSourceMap* source_map) = 0;

  // Blink maintains an allowlist for custom element names. This method
  // provides the delegate the ability to add more names to that allowlist.
  virtual void EnableCustomElementAllowlist() = 0;

  // Requires modules for defining WebView APIs within a ScriptContext's
  // ModuleSystem.
  virtual void RequireWebViewModules(ScriptContext* context) = 0;
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_EXTENSIONS_RENDERER_API_PROVIDER_H_
