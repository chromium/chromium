// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_BROWSER_CONTEXT_KEYED_SERVICE_FACTORIES_H_
#define EXTENSIONS_BROWSER_BROWSER_CONTEXT_KEYED_SERVICE_FACTORIES_H_

namespace extensions {

// Ensures the existence of any BrowserContextKeyedServiceFactory provided by
// the core extensions code or //extensions layer APIs.
void EnsureBrowserContextKeyedServiceFactoriesBuilt();

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_BROWSER_CONTEXT_KEYED_SERVICE_FACTORIES_H_
