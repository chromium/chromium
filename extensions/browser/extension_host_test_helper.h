// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_EXTENSION_HOST_TEST_HELPER_H_
#define EXTENSIONS_BROWSER_EXTENSION_HOST_TEST_HELPER_H_

#include <map>
#include <optional>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "extensions/browser/extension_host_registry.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/mojom/view_type.mojom.h"

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

  // Restricts this class to only observing ExtensionHosts of the specified
  // `view_type`. Other extension hosts matching the event (even from the same
  // extension and browser context) will be ignored. This allows tests to wait
  // for, e.g., a background page or popup host event to happen.
  void RestrictToType(mojom::ViewType view_type);

  // Restricts this class to only observing the specified `host`.
  void RestrictToHost(const ExtensionHost* host);

  // Waits for an ExtensionHost matching the restrictions (if any) to fire the
  // corresponding notification.
  // NOTE: These WaitFor() methods can return null if the host has already been
  // destroyed (which can happen if the host was closed before this method was
  // called or if the host is destroyed synchronously from creation), before
  // the run loop is quit.
  ExtensionHost* WaitForRenderProcessReady() {
    return WaitFor(HostEvent::kRenderProcessReady);
  }
  ExtensionHost* WaitForDocumentElementAvailable() {
    return WaitFor(HostEvent::kDocumentElementAvailable);
  }
  ExtensionHost* WaitForHostCompletedFirstLoad() {
    return WaitFor(HostEvent::kCompletedFirstLoad);
  }
  // NOTE: No return because the ExtensionHost is *always* (obviously)
  // destroyed by the time this returns.
  void WaitForHostDestroyed() { WaitFor(HostEvent::kDestroyed); }
  // Technically, the host can outlive the render process, but it's unlikely to
  // be for long. Similar to above, avoid returning the host object.
  void WaitForRenderProcessGone() { WaitFor(HostEvent::kRenderProcessGone); }

 private:
  // The different types of events this class can wait for.
  enum class HostEvent {
    kRenderProcessReady,
    kDocumentElementAvailable,
    kCompletedFirstLoad,
    kDestroyed,
    kRenderProcessGone,
  };

  // ExtensionHostRegistry::Observer:
  void OnExtensionHostRenderProcessReady(
      content::BrowserContext* browser_context,
      ExtensionHost* host) override;
  void OnExtensionHostDocumentElementAvailable(
      content::BrowserContext* browser_context,
      ExtensionHost* host) override;
  void OnExtensionHostCompletedFirstLoad(
      content::BrowserContext* browser_context,
      ExtensionHost* host) override;
  void OnExtensionHostDestroyed(content::BrowserContext* browser_context,
                                ExtensionHost* host) override;
  void OnExtensionHostRenderProcessGone(
      content::BrowserContext* browser_context,
      ExtensionHost* host) override;

  // Waits for the given `event` to happen. This may return immediately if the
  // event was already observed. Returns the ExtensionHost corresponding to the
  // event if the host is still valid (it may not be, if it has already been
  // destroyed).
  ExtensionHost* WaitFor(HostEvent event);

  // Called when an `event` has been seen, and quits an active run loop if
  // we're currently waiting on the event.
  void EventSeen(ExtensionHost* host, HostEvent event);

  // The event we're currently waiting for, if any.
  std::optional<HostEvent> waiting_for_;

  // A closure to quit an active run loop, if we're waiting on a given event.
  base::OnceClosure quit_loop_;

  // The associated browser context.
  const raw_ptr<content::BrowserContext, AcrossTasksDanglingUntriaged>
      browser_context_;

  // The ID of the extension whose hosts this helper is watching, if it is
  // restricted to a given ID.
  const ExtensionId extension_id_;

  // The specific type of host this helper is waiting on, if any (nullopt
  // implies waiting on any kind of ExtensionHost).
  std::optional<mojom::ViewType> restrict_to_type_;

  // The specific host this helper is waiting on, if any (null implies
  // waiting on any host).
  raw_ptr<const ExtensionHost, AcrossTasksDanglingUntriaged> restrict_to_host_ =
      nullptr;

  // The set of all events this helper has seen and their corresponding
  // ExtensionHosts. ExtensionHosts are nulled out when they are destroyed, but
  // the events stay in the map.
  std::map<HostEvent, raw_ptr<ExtensionHost, CtnExperimental>> observed_events_;

  base::ScopedObservation<ExtensionHostRegistry,
                          ExtensionHostRegistry::Observer>
      host_registry_observation_{this};
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_EXTENSION_HOST_TEST_HELPER_H_
