// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_BROWSER_CONTEXT_LIFETIME_TRACKER_H_
#define EXTENSIONS_BROWSER_BROWSER_CONTEXT_LIFETIME_TRACKER_H_

#include "extensions/browser/browser_context_lifetime_observer.h"

namespace content {
class BrowserContext;
}

namespace extensions {

// Lets //extensions code watch the lifecycle of a BrowserContext's
// off-the-record sibling -- functionality //content does not model
// generically, since what "off-the-record sibling" means is embedder-specific
// (e.g. a Chrome Profile and its incognito Profile). Obtained via
// ExtensionsBrowserClient::GetBrowserContextLifetimeTracker().
class BrowserContextLifetimeTracker {
 public:
  // Starts forwarding lifecycle events for `context` and its associated
  // off-the-record context (if any) to `observer` (see
  // BrowserContextLifetimeObserver). `context` must be an original
  // (non-off-the-record) context. `observer` must call StopObserving()
  // before it is destroyed.
  virtual void StartObserving(content::BrowserContext& context,
                              BrowserContextLifetimeObserver& observer) = 0;

  // Stops forwarding events to `observer`.
  virtual void StopObserving(BrowserContextLifetimeObserver& observer) = 0;

 protected:
  virtual ~BrowserContextLifetimeTracker() = default;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_BROWSER_CONTEXT_LIFETIME_TRACKER_H_
