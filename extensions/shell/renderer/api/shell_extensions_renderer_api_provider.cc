// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/shell/renderer/api/shell_extensions_renderer_api_provider.h"

#include "extensions/renderer/module_system.h"
#include "extensions/renderer/script_context.h"

namespace extensions {

void ShellExtensionsRendererAPIProvider::RegisterNativeHandlers(
    ModuleSystem* module_system,
    NativeExtensionBindingsSystem* bindings_system,
    V8SchemaRegistry* v8_schema_registry,
    ScriptContext* context) const {}

void ShellExtensionsRendererAPIProvider::AddBindingsSystemHooks(
    Dispatcher* dispatcher,
    NativeExtensionBindingsSystem* bindings_system) const {}

void ShellExtensionsRendererAPIProvider::PopulateSourceMap(
    ResourceBundleSourceMap* source_map) const {}

void ShellExtensionsRendererAPIProvider::EnableCustomElementAllowlist() const {}

void ShellExtensionsRendererAPIProvider::RequireWebViewModules(
    ScriptContext* context) const {
  context->module_system()->Require("extensionsWebViewElement");
}

}  // namespace extensions
