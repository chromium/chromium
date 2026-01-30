// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_POLYFILL_UTIL_H_
#define EXTENSIONS_RENDERER_POLYFILL_UTIL_H_

namespace extensions {

class Extension;

// Returns true if the `browser` namespace and polyfill support feature should
// be enabled for the given `extension`.
bool IsExtensionBrowserNamespaceAndPolyfillSupportEnabledForExtension(
    const Extension* extension);

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_POLYFILL_UTIL_H_
