// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_host_registry.h"

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/no_destructor.h"
#include "base/observer_list.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "content/public/browser/render_frame_host.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extensions_browser_client.h"

namespace extensions {

namespace {

class ExtensionHostRegistryFactory : public BrowserContextKeyedServiceFactory {
 public:
  ExtensionHostRegistryFactory();
  ExtensionHostRegistryFactory(const ExtensionHostRegistryFactory&) = delete;
  ExtensionHostRegistryFactory& operator=(const ExtensionHostRegistryFactory&) =
      delete;
  ~ExtensionHostRegistryFactory() override = default;

  ExtensionHostRegistry* GetForBrowserContext(content::BrowserContext* context);

 private:
  // BrowserContextKeyedServiceFactory:
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

ExtensionHostRegistryFactory::ExtensionHostRegistryFactory()
    : BrowserContextKeyedServiceFactory(
          "ExtensionHostRegistry",
          BrowserContextDependencyManager::GetInstance()) {}

ExtensionHostRegistry* ExtensionHostRegistryFactory::GetForBrowserContext(
    content::BrowserContext* browser_context) {
  return static_cast<ExtensionHostRegistry*>(
      GetServiceForBrowserContext(browser_context, /*create=*/true));
}

content::BrowserContext* ExtensionHostRegistryFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  // This seems like a service that should have its own instance in incognito
  // in order to better ensure there isn't any bleed-over from off-the-record
  // contexts. Unfortunately, other systems (I'm looking at you,
  // LazyBackgroundTaskQueue!) rely on this, and are set up to be redirect to
  // the original context. This makes it quite challenging to let this have its
  // own incognito context.
  return ExtensionsBrowserClient::Get()->GetContextRedirectedToOriginal(
      context, /*force_guest_profile=*/true);
}

KeyedService* ExtensionHostRegistryFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new ExtensionHostRegistry();
}

}  // namespace

ExtensionHostRegistry::ExtensionHostRegistry() = default;
ExtensionHostRegistry::~ExtensionHostRegistry() = default;

// static
ExtensionHostRegistry* ExtensionHostRegistry::Get(
    content::BrowserContext* browser_context) {
  return static_cast<ExtensionHostRegistryFactory*>(GetFactory())
      ->GetForBrowserContext(browser_context);
}

// static
BrowserContextKeyedServiceFactory* ExtensionHostRegistry::GetFactory() {
  static base::NoDestructor<ExtensionHostRegistryFactory> g_factory;
  return g_factory.get();
}

void ExtensionHostRegistry::ExtensionHostCreated(
    ExtensionHost* extension_host) {
  DCHECK(!base::Contains(extension_hosts_, extension_host));
  extension_hosts_.insert(extension_host);

  // Note: There's not currently any observer method corresponding to host
  // creation, because most systems and listeners care about the host being
  // at a certain state of readiness. This is just to start properly
  // tracking the host.
}

void ExtensionHostRegistry::ExtensionHostRenderProcessReady(
    ExtensionHost* extension_host) {
  DCHECK(base::Contains(extension_hosts_, extension_host));

  for (Observer& observer : observers_) {
    observer.OnExtensionHostRenderProcessReady(
        extension_host->browser_context(), extension_host);
  }
}

void ExtensionHostRegistry::ExtensionHostCompletedFirstLoad(
    ExtensionHost* extension_host) {
  DCHECK(base::Contains(extension_hosts_, extension_host));

  // TODO(devlin): This can unexpectedly fire when a renderer process is
  // terminating.  When a renderer process is terminated, it causes the
  // RenderFrameHostImpl to reset its loading state, which calls
  // DidStopLoading() if it was loading. Then, if the first load never
  // happened, ExtensionHost will fire the DidCompleteFirstLoad() notification.
  //
  // This is probably a behavioral bug. We should have ExtensionHost check
  // whether the renderer is still around or whether the load succeeded before
  // notifying observers, or at least indicate the success in the notification.

  for (Observer& observer : observers_) {
    observer.OnExtensionHostCompletedFirstLoad(
        extension_host->browser_context(), extension_host);
  }
}

void ExtensionHostRegistry::ExtensionHostDocumentElementAvailable(
    ExtensionHost* extension_host) {
  DCHECK(base::Contains(extension_hosts_, extension_host));

  for (Observer& observer : observers_) {
    observer.OnExtensionHostDocumentElementAvailable(
        extension_host->browser_context(), extension_host);
  }
}

void ExtensionHostRegistry::ExtensionHostRenderProcessGone(
    ExtensionHost* extension_host) {
  DCHECK(base::Contains(extension_hosts_, extension_host));

  for (Observer& observer : observers_) {
    observer.OnExtensionHostRenderProcessGone(extension_host->browser_context(),
                                              extension_host);
  }
}

void ExtensionHostRegistry::ExtensionHostDestroyed(
    ExtensionHost* extension_host) {
  DCHECK(base::Contains(extension_hosts_, extension_host));
  extension_hosts_.erase(extension_host);

  for (Observer& observer : observers_) {
    observer.OnExtensionHostDestroyed(extension_host->browser_context(),
                                      extension_host);
  }
}

std::vector<ExtensionHost*> ExtensionHostRegistry::GetHostsForExtension(
    const ExtensionId& extension_id) {
  std::vector<ExtensionHost*> hosts;
  for (ExtensionHost* host : extension_hosts_) {
    if (host->extension_id() == extension_id) {
      hosts.push_back(host);
    }
  }
  return hosts;
}

ExtensionHost* ExtensionHostRegistry::GetExtensionHostForPrimaryMainFrame(
    content::RenderFrameHost* render_frame_host) {
  DCHECK(render_frame_host->IsInPrimaryMainFrame())
      << "GetExtensionHostForPrimaryMainFrame() should only be called with "
      << "the primary main frame.";
  for (ExtensionHost* host : extension_hosts_) {
    if (host->main_frame_host() == render_frame_host) {
      return host;
    }
  }
  return nullptr;
}

void ExtensionHostRegistry::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void ExtensionHostRegistry::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void ExtensionHostRegistry::Shutdown() {
  for (Observer& observer : observers_) {
    observer.OnExtensionHostRegistryShutdown(this);
  }
}

}  // namespace extensions
