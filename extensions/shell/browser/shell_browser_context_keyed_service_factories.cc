// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/shell/browser/shell_browser_context_keyed_service_factories.h"

#include "extensions/shell/browser/shell_extension_system_factory.h"

namespace extensions {
namespace shell {

void EnsureBrowserContextKeyedServiceFactoriesBuilt() {
  ShellExtensionSystemFactory::GetInstance();
}

}  // namespace shell
}  // namespace extensions
