// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_CORE_BROWSER_CONTEXT_KEYED_SERVICE_FACTORIES_H_
#define EXTENSIONS_BROWSER_CORE_BROWSER_CONTEXT_KEYED_SERVICE_FACTORIES_H_

namespace extensions {

// Ensures the existence of any BrowserContextKeyedServiceFactory provided by
// the core extensions code.
void EnsureCoreBrowserContextKeyedServiceFactoriesBuilt();

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_CORE_BROWSER_CONTEXT_KEYED_SERVICE_FACTORIES_H_
