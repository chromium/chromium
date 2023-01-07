// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_PROCESS_UTIL_H_
#define EXTENSIONS_BROWSER_PROCESS_UTIL_H_

namespace content {
class BrowserContext;
}

namespace extensions {
class Extension;

namespace process_util {

enum class PersistentBackgroundPageState {
  // The extension doesn't have a persistent background page.
  kInvalid,
  // The background page isn't ready yet.
  kNotReady,
  // The background page is "ready"; in practice, this corresponds to the
  // document element being available in the background page's ExtensionHost.
  kReady,
};

// Returns the state of the persistent background page (if any) for the given
// `extension`.
PersistentBackgroundPageState GetPersistentBackgroundPageState(
    const Extension& extension,
    content::BrowserContext* browser_context);

}  // namespace process_util
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_PROCESS_UTIL_H_
