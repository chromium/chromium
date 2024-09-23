// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/browser_context_keyed_service_factories.h"

#include "extensions/browser/core_browser_context_keyed_service_factories.h"
#include "extensions/browser/api/api_browser_context_keyed_service_factories.h"

namespace extensions {

void EnsureBrowserContextKeyedServiceFactoriesBuilt() {
  EnsureCoreBrowserContextKeyedServiceFactoriesBuilt();
  EnsureApiBrowserContextKeyedServiceFactoriesBuilt();
}

}  // namespace extensions
