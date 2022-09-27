// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/bindings/extensions_registry.h"

namespace blink {

// static
ExtensionsRegistry& ExtensionsRegistry::GetInstance() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(ExtensionsRegistry, instance, ());
  return instance;
}

void ExtensionsRegistry::RegisterBlinkExtensionInstallCallback(
    InstallExtensionFuncType callback) {
  install_funcs_.push_back(callback);
}

void ExtensionsRegistry::InstallExtensions(ScriptState* script_state) {
  for (auto install_func : install_funcs_) {
    install_func(script_state);
  }
}

}  // namespace blink
