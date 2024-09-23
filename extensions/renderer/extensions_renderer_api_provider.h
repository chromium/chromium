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
class V8SchemaRegistry;

// An interface class that is owned by extensions::Dispatcher.
// ExtensionsRendererAPIProvider can be used to override and extend the behavior
// of the extensions system's renderer side from outside of extensions
// directories.
// NOTE: This class may be used on multiple threads. As such, ALL METHODS must
// be `const` and implementors must be careful about accessing any global state.
// See also comment in //extensions/renderer/dispatcher.cc.
class ExtensionsRendererAPIProvider {
 public:
  virtual ~ExtensionsRendererAPIProvider() = default;

  // Registers any native handlers to provide additional functionality for
  // native bindings. Called each time a new ScriptContext is created, since
  // native handlers are per-context.
  virtual void RegisterNativeHandlers(
      ModuleSystem* module_system,
      NativeExtensionBindingsSystem* bindings_system,
      V8SchemaRegistry* v8_schema_registry,
      ScriptContext* context) const = 0;

  // Registers any additional hooks associated with specific APIs to the API
  // bindings system. Called once per NativeExtensionBindingsSystem, which is
  // one-per-thread and re-used across ScriptContexts.
  virtual void AddBindingsSystemHooks(
      Dispatcher* dispatcher,
      NativeExtensionBindingsSystem* bindings_system) const = 0;

  // Includes additional source resources into the resource map.
  virtual void PopulateSourceMap(ResourceBundleSourceMap* source_map) const = 0;

  // Blink maintains an allowlist for custom element names. This method
  // provides the delegate the ability to add more names to that allowlist.
  virtual void EnableCustomElementAllowlist() const = 0;

  // Requires modules for defining WebView APIs within a ScriptContext's
  // ModuleSystem.
  virtual void RequireWebViewModules(ScriptContext* context) const = 0;
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_EXTENSIONS_RENDERER_API_PROVIDER_H_
