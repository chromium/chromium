// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CONTEXT_CORE_OBSERVER_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CONTEXT_CORE_OBSERVER_H_

#include <stdint.h>
#include <string>

#include "base/functional/callback.h"
#include "base/time/time.h"
#include "content/browser/service_worker/service_worker_info.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/service_worker_context_observer.h"
#include "third_party/blink/public/common/service_worker/embedded_worker_status.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_client.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_container_type.mojom.h"
#include "url/gurl.h"

namespace blink {
class StorageKey;
}  // namespace blink

namespace content {

struct ConsoleMessage;

class ServiceWorkerContextCoreObserver {
 public:
  virtual void OnNewLiveRegistration(int64_t registration_id,
                                     const GURL& scope,
                                     const blink::StorageKey& key) {}
  virtual void OnNewLiveVersion(const ServiceWorkerVersionInfo& version_info) {}
  virtual void OnLiveVersionDestroyed(int64_t version_id) {}
  virtual void OnStarting(int64_t version_id) {}
  virtual void OnStarted(int64_t version_id,
                         const GURL& scope,
                         int process_id,
                         const GURL& script_url,
                         const blink::ServiceWorkerToken& token,
                         const blink::StorageKey& key) {}
  virtual void OnStopping(int64_t version_id) {}
  virtual void OnStopped(int64_t version_id) {}
  // Called when the context core is about to be deleted. After this is called,
  // method calls on this observer will be for a new context core, possibly
  // reusing version/registration IDs previously seen. So this method gives the
  // observer a chance to discard any state it has.
  virtual void OnDeleteAndStartOver() {}
  virtual void OnVersionStateChanged(int64_t version_id,
                                     const GURL& scope,
                                     const blink::StorageKey& key,
                                     ServiceWorkerVersion::Status status) {}
  virtual void OnVersionRouterRulesChanged(int64_t version_id,
                                           const std::string& router_rules) {}
  virtual void OnVersionDevToolsRoutingIdChanged(int64_t version_id,
                                                 int process_id,
                                                 int devtools_agent_route_id) {}
  virtual void OnMainScriptResponseSet(int64_t version_id,
                                       base::Time script_response_time,
                                       base::Time script_last_modified) {}
  virtual void OnErrorReported(
      int64_t version_id,
      const GURL& scope,
      const blink::StorageKey& key,
      const ServiceWorkerContextObserver::ErrorInfo& info) {}
  virtual void OnReportConsoleMessage(int64_t version_id,
                                      const GURL& scope,
                                      const blink::StorageKey& key,
                                      const ConsoleMessage& message) {}
  virtual void OnControlleeAdded(int64_t version_id,
                                 const std::string& uuid,
                                 const ServiceWorkerClientInfo& info) {}
  virtual void OnControlleeRemoved(int64_t version_id,
                                   const std::string& uuid) {}
  virtual void OnNoControllees(int64_t version_id,
                               const GURL& scope,
                               const blink::StorageKey& key) {}
  virtual void OnControlleeNavigationCommitted(
      int64_t version_id,
      const std::string& uuid,
      GlobalRenderFrameHostId render_frame_host_id) {}
  // Called when the ServiceWorkerContainer.register() promise is resolved.
  //
  // This is called before the service worker registration is persisted to
  // storage. The implementation cannot assume that the ServiceWorkerContextCore
  // will find the registration at this point.
  virtual void OnRegistrationCompleted(int64_t registration_id,
                                       const GURL& scope,
                                       const blink::StorageKey& key) {}
  // Called after a service worker registration is persisted to storage.
  //
  // This happens after OnRegistrationCompleted(). The implementation can assume
  // that ServiceWorkerContextCore will find the registration, and can safely
  // add user data to the registration.
  virtual void OnRegistrationStored(int64_t registration_id,
                                    const GURL& scope,
                                    const blink::StorageKey& key) {}

  // Called after a task has been posted to delete a registration from storage.
  // This is roughly equivalent to the same time that the promise for
  // unregister() would be resolved. This means the live
  // ServiceWorkerRegistration may still exist, and the deletion operator may
  // not yet have finished.
  virtual void OnRegistrationDeleted(int64_t registration_id,
                                     const GURL& scope,
                                     const blink::StorageKey& key) {}

  // Called after all registrations for `key` are deleted from storage. There
  // may still be live registrations for this origin in the kUninstalling or
  // kUninstalled state.
  //
  // This is called after OnRegistrationDeleted(). It is called once
  // ServiceWorkerRegistry gets confirmation that the delete operation finished.
  virtual void OnAllRegistrationsDeletedForStorageKey(
      const blink::StorageKey& key) {}

  // Notified when the storage corruption recovery is completed and all stored
  // data is wiped out.
  virtual void OnStorageWiped() {}

  // Called when a container host representing a client is execution ready. See
  // https://html.spec.whatwg.org/multipage/webappapis.html#concept-environment-execution-ready-flag
  virtual void OnClientIsExecutionReady(
      ukm::SourceId source_id,
      const GURL& url,
      blink::mojom::ServiceWorkerClientType type) {}

  // Called when a container host representing a client is destroyed.
  virtual void OnClientDestroyed(ukm::SourceId source_id,
                                 const GURL& url,
                                 blink::mojom::ServiceWorkerClientType type) {}

  // Called when a Service Worker opens a new window.
  virtual void OnWindowOpened(const GURL& script_url, const GURL& url) {}

  // Called when a Service Worker navigates an existing tab.
  virtual void OnClientNavigated(const GURL& script_url, const GURL& url) {}

 protected:
  virtual ~ServiceWorkerContextCoreObserver() {}
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CONTEXT_CORE_OBSERVER_H_
