// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_BROWSER_CONTEXT_LIFETIME_OBSERVER_H_
#define EXTENSIONS_BROWSER_BROWSER_CONTEXT_LIFETIME_OBSERVER_H_

namespace content {
class BrowserContext;
}

namespace extensions {

// Observes the lifetime of a primary content::BrowserContext and its
// off-the-record sibling (see BrowserContextLifetimeTracker::
// StartObserving()).
class BrowserContextLifetimeObserver {
 public:
  // Called when the off-the-record sibling of the primary context being
  // observed is created (possibly synchronously from StartObserving(), if
  // one already exists at that point).
  virtual void OnRelatedOffTheRecordBrowserContextCreated(
      content::BrowserContext& off_the_record_context) {}

  // Called just before the off-the-record sibling is destroyed. If the
  // primary context is being destroyed too, this is called before
  // OnPrimaryBrowserContextDestroyed().
  virtual void OnRelatedOffTheRecordBrowserContextDestroyed(
      content::BrowserContext& off_the_record_context) {}

  // Called just before the primary context itself is destroyed. No further
  // notifications will follow.
  virtual void OnPrimaryBrowserContextDestroyed(
      content::BrowserContext& primary_context) {}

 protected:
  virtual ~BrowserContextLifetimeObserver() = default;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_BROWSER_CONTEXT_LIFETIME_OBSERVER_H_
