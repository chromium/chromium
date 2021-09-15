// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_EXTENSION_HOST_TEST_HELPER_H_
#define EXTENSIONS_BROWSER_EXTENSION_HOST_TEST_HELPER_H_

#include <set>

#include "base/callback.h"
#include "base/scoped_observation.h"
#include "extensions/browser/extension_host_registry.h"
#include "extensions/common/extension_id.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {
class BrowserContext;
}

namespace extensions {

// A helper class to wait for particular events associated with ExtensionHosts.
// TODO(devlin): Add enough bits and bobs to use this class within (or to
// replace) our background page-specific test helpers.
class ExtensionHostTestHelper : public ExtensionHostRegistry::Observer {
 public:
  // Constructs an ExtensionHostTestHelper restricted to a given
  // `extension_id`. Only hosts associated with the given `browser_context`
  // (or its off/on-the-record counterpart) are considered.
  ExtensionHostTestHelper(content::BrowserContext* browser_context,
                          ExtensionId extension_id);

  // As above, but observes *all* extension hosts for the given
  // `browser_context`. Prefer the above constructor when possible to eliminate
  // possibilities of observing an unrelated event.
  explicit ExtensionHostTestHelper(content::BrowserContext* browser_context);

  ExtensionHostTestHelper(const ExtensionHostTestHelper&) = delete;
  ExtensionHostTestHelper& operator=(const ExtensionHostTestHelper&) = delete;
  ~ExtensionHostTestHelper() override;

  // TODO(devlin): Add a restriction for type of ExtensionHost, e.g.
  // background, popup, etc.

  // Waits until an extension host matching the restrictions (if any) is
  // destroyed.
  void WaitForExtensionHostDestroyed() { WaitFor(HostEvent::kDestroyed); }

 private:
  // The different types of events this class can wait for.
  enum class HostEvent {
    // TODO(devlin): Add events here for created, load stopped, etc.
    kDestroyed,
  };

  // ExtensionHostRegistry::Observer:
  void OnExtensionHostDestroyed(content::BrowserContext* browser_context,
                                ExtensionHost* host) override;

  // Waits for the given `event` to happen. This may return immediately if the
  // event was already observed.
  void WaitFor(HostEvent event);

  // Called when an `event` has been seen, and quits an active run loop if
  // we're currently waiting on the event.
  void EventSeen(ExtensionHost* host, HostEvent event);

  // The event we're currently waiting for, if any.
  absl::optional<HostEvent> waiting_for_;

  // A closure to quit an active run loop, if we're waiting on a given event.
  base::OnceClosure quit_loop_;

  // The ID of the extension whose hosts this helper is watching, if it is
  // restricted to a given ID.
  const ExtensionId extension_id_;

  // The set of all events this helper has seen.
  std::set<HostEvent> observed_events_;

  base::ScopedObservation<ExtensionHostRegistry,
                          ExtensionHostRegistry::Observer>
      host_registry_observation_{this};
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_EXTENSION_HOST_TEST_HELPER_H_
