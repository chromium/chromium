// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_CONSTANTS_H_
#define EXTENSIONS_BROWSER_API_CONSTANTS_H_

namespace extensions {

// Represents the extension API that performed a search redirect.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(ExtensionSearchRedirectedByApi)
enum class ExtensionSearchRedirectedByApi {
  kDeclarativeNetRequest = 0,
  kTabsUpdate = 1,
  kMaxValue = kTabsUpdate,
};
// LINT.ThenChange(//tools/metrics/histograms/enums.xml:ExtensionSearchRedirectedByApi)

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_CONSTANTS_H_
