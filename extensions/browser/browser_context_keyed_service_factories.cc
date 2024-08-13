// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/browser_context_keyed_service_factories.h"

#include "extensions/browser/core_browser_context_keyed_service_factories.h"
#include "extensions/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/browser/api/api_browser_context_keyed_service_factories.h"
#endif

namespace extensions {

void EnsureBrowserContextKeyedServiceFactoriesBuilt() {
  EnsureCoreBrowserContextKeyedServiceFactoriesBuilt();
// TODO(https://crbug.com/356905053): Remove this when APIs are compiled into
// the experimental desktop-android build.
#if BUILDFLAG(ENABLE_EXTENSIONS)
  EnsureApiBrowserContextKeyedServiceFactoriesBuilt();
#endif
}

}  // namespace extensions
