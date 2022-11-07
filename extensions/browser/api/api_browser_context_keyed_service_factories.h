// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_API_BROWSER_CONTEXT_KEYED_SERVICE_FACTORIES_H_
#define EXTENSIONS_BROWSER_API_API_BROWSER_CONTEXT_KEYED_SERVICE_FACTORIES_H_

namespace extensions {

// Ensures the existence of any BrowserContextKeyedServiceFactory provided by
// the APIs at the //extensions layer.
void EnsureApiBrowserContextKeyedServiceFactoriesBuilt();

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_API_BROWSER_CONTEXT_KEYED_SERVICE_FACTORIES_H_
