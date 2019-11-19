// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_DISPATCHER_DELEGATE_H_
#define EXTENSIONS_RENDERER_DISPATCHER_DELEGATE_H_

#include <set>
#include <string>

namespace extensions {
class Dispatcher;
class ModuleSystem;
class NativeExtensionBindingsSystem;
class ResourceBundleSourceMap;
class ScriptContext;

// Base class and default implementation for an extensions::Dispacher delegate.
// DispatcherDelegate can be used to override and extend the behavior of the
// extensions system's renderer side.
class DispatcherDelegate {
 public:
  virtual ~DispatcherDelegate() {}

  // Includes additional native handlers in a ScriptContext's ModuleSystem.
  virtual void RegisterNativeHandlers(
      Dispatcher* dispatcher,
      ModuleSystem* module_system,
      NativeExtensionBindingsSystem* bindings_system,
      ScriptContext* context) {}

  // Includes additional source resources into the resource map.
  virtual void PopulateSourceMap(ResourceBundleSourceMap* source_map) {}

  // Requires modules for defining <webview> within an extension context's
  // module system.
  virtual void RequireWebViewModules(ScriptContext* context);

  // Allows the delegate to respond to an updated set of active extensions in
  // the Dispatcher.
  virtual void OnActiveExtensionsUpdated(
      const std::set<std::string>& extension_ids) {}

  // Allows the delegate to add any additional custom bindings or types to the
  // native bindings system. This will only be called if --native-crx-bindings
  // is enabled.
  virtual void InitializeBindingsSystem(
      Dispatcher* dispatcher,
      NativeExtensionBindingsSystem* bindings_system) {}
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_DISPATCHER_DELEGATE_H_
