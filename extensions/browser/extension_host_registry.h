// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_EXTENSION_HOST_REGISTRY_H_
#define EXTENSIONS_BROWSER_EXTENSION_HOST_REGISTRY_H_

#include <unordered_set>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "components/keyed_service/core/keyed_service.h"
#include "extensions/common/extension_id.h"

class BrowserContextKeyedServiceFactory;

namespace content {
class BrowserContext;
class RenderFrameHost;
}

namespace extensions {
class ExtensionHost;

// A class responsible for tracking ExtensionHosts and notifying observers of
// relevant changes.
// See also ProcessManager, which is responsible for more of the construction
// lifetime management of these hosts.
class ExtensionHostRegistry : public KeyedService {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Called when the RenderProcessHost for an ExtensionHost is ready.
    // In practice, this corresponds to "shortly after" the first render frame
    // is created in the host.
    // The `browser_context` is the context associated with that host (which
    // might be an incognito version of
    // ExtensionHostRegistry::browser_context_).
    virtual void OnExtensionHostRenderProcessReady(
        content::BrowserContext* browser_context,
        ExtensionHost* host) {}

    // Called when an ExtensionHost is destroyed. The `browser_context` is
    // the context associated with that host (which might be an incognito
    // version of ExtensionHostRegistry::browser_context_).
    virtual void OnExtensionHostDestroyed(
        content::BrowserContext* browser_context,
        ExtensionHost* host) {}

    // Called when an ExtensionHost completes its first load. The
    // `browser_context` is the context associated with that host (which might
    // be an incognito version of ExtensionHostRegistry::browser_context_).
    // Note: If you only need to observe a single ExtensionHost (that's already
    // created), prefer overriding
    // ExtensionHostObserver::OnExtensionHostDidStopFirstLoad().
    virtual void OnExtensionHostCompletedFirstLoad(
        content::BrowserContext* browser_context,
        ExtensionHost* host) {}

    // Called when a document element is first available in an ExtensionHost.
    // `browser_context` is the context associated with that host (which might
    // be an incognito version of ExtensionHostRegistry::browser_context_).
    // TODO(devlin): Do we really need both first load completed and document
    // element available notifications? This matches previous implementations,
    // but I'm not sure the distinction is relevant.
    virtual void OnExtensionHostDocumentElementAvailable(
        content::BrowserContext* browser_context,
        ExtensionHost* extension_host) {}

    // Called when an ExtensionHost's render process is terminated. Note that
    // this may be called multiple times for a single process termination, since
    // there may be multiple ExtensionHosts in the same process.
    // `browser_context` is the context associated with that host (which might
    // be an incognito version of ExtensionHostRegistry::browser_context_).
    virtual void OnExtensionHostRenderProcessGone(
        content::BrowserContext* browser_context,
        ExtensionHost* extension_host) {}

    // Called when `registry` is starting to shut down.
    virtual void OnExtensionHostRegistryShutdown(
        ExtensionHostRegistry* registry) {}
  };

  ExtensionHostRegistry();
  ExtensionHostRegistry(const ExtensionHostRegistry&) = delete;
  ExtensionHostRegistry& operator=(const ExtensionHostRegistry&) = delete;
  ~ExtensionHostRegistry() override;

  // Retrieves the ExtensionHostRegistry for a given `browser_context`.
  // NOTE: ExtensionHostRegistry is shared between on- and off-the-record
  // contexts. See also the comment
  // ExtensionHostRegistryFactory::GetBrowserContextToUse().
  static ExtensionHostRegistry* Get(content::BrowserContext* browser_context);

  // Retrieves the factory instance for the ExtensionHostRegistry.
  static BrowserContextKeyedServiceFactory* GetFactory();

  // Called when a new ExtensionHost is created, and starts tracking the host
  // in `extension_hosts_`.
  void ExtensionHostCreated(ExtensionHost* extension_host);

  // Called when an ExtensionHost's corresponding renderer process is ready, and
  // and notifies observers.
  void ExtensionHostRenderProcessReady(ExtensionHost* extension_host);

  // Called when an ExtensionHost completes its first load.
  void ExtensionHostCompletedFirstLoad(ExtensionHost* extension_host);

  // Called when an ExtensionHost has created a document element for its first
  // time.
  void ExtensionHostDocumentElementAvailable(ExtensionHost* extension_host);

  // Called when an ExtensionHost's render process is terminated.
  void ExtensionHostRenderProcessGone(ExtensionHost* extension_host);

  // Called when an ExtensionHost is destroyed. Stops tracking the host and
  // notifies observers.
  void ExtensionHostDestroyed(ExtensionHost* extension_host);

  // Returns the collection of ExtensionHosts associated with the specified
  // `extension_id`.
  // If performance ever becomes a consideration here, we can update the
  // storage in the registry to be an unordered_map split apart by extension.
  std::vector<ExtensionHost*> GetHostsForExtension(
      const ExtensionId& extension_id);

  // Returns the ExtensionHost for the given `render_frame_host`, if one exists.
  // `render_frame_host` must be the primary main frame host; we do this to
  // avoid returning an ExtensionHost for a non-extension frame within an
  // extension document.
  ExtensionHost* GetExtensionHostForPrimaryMainFrame(
      content::RenderFrameHost* render_frame_host);

  const std::unordered_set<raw_ptr<ExtensionHost, CtnExperimental>>&
  extension_hosts() {
    return extension_hosts_;
  }

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // KeyedService:
  void Shutdown() override;

 private:
  // The active set of ExtensionHosts.
  std::unordered_set<raw_ptr<ExtensionHost, CtnExperimental>> extension_hosts_;

  base::ObserverList<Observer> observers_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_EXTENSION_HOST_REGISTRY_H_
