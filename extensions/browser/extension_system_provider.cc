// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_system_provider.h"

#include "components/keyed_service/content/browser_context_dependency_manager.h"

namespace extensions {

ExtensionSystemProvider::ExtensionSystemProvider(
    const char* name,
    BrowserContextDependencyManager* manager)
    : BrowserContextKeyedServiceFactory(name, manager) {}

ExtensionSystemProvider::~ExtensionSystemProvider() = default;

}  // namespace extensions
