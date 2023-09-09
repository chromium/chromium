// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_SERVICE_WORKER_CONTEXT_OBSERVER_H_
#define CONTENT_PUBLIC_BROWSER_SERVICE_WORKER_CONTEXT_OBSERVER_H_

#include <string>

#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/service_worker_client_info.h"
#include "url/gurl.h"

namespace content {
class ServiceWorkerContext;
struct ConsoleMessage;
struct ServiceWorkerRunningInfo;

class ServiceWorkerContextObserver {
 public:
  struct ErrorInfo {
    ErrorInfo(const std::u16string& message,
              int line,
              int column,
              const GURL& url)
        : error_message(message),
          line_number(line),
          column_number(column),
          source_url(url) {}
    ErrorInfo(const ErrorInfo& info) = default;
    const std::u16string error_message;
    const int line_number;
    const int column_number;
    const GURL source_url;
  };
  // Called when a service worker has been registered with scope |scope|.
  //
  // This is called when the ServiceWorkerContainer.register() promise is
  // resolved, which happens before the service worker registration is persisted
  // to disk.
  virtual void OnRegistrationCompleted(const GURL& scope) {}

  // Called after a service worker registration is persisted to storage with
  // registration ID |registration_id| and scope |scope|.
  //
  // This happens after OnRegistrationCompleted().
  virtual void OnRegistrationStored(int64_t registration_id,
                                    const GURL& scope) {}

  // Called when the service worker with id |version_id| changes status to
  // activated.
  virtual void OnVersionActivated(int64_t version_id, const GURL& scope) {}

  // Called when the service worker with id |version_id| changes status to
  // redundant.
  virtual void OnVersionRedundant(int64_t version_id, const GURL& scope) {}

  // Called when the service worker with id |version_id| is starting, started,
  // stopping, or stopped running.
  //
  // OnVersionStartedRunning()/OnVersionStoppedRunning(): called after a worker
  //   finishes starting/stopping or the version is destroyed before finishing
  //   stopping.
  // OnVersionStartingRunning()/OnVersionStoppingRunning(): called before a
  //   worker finishes starting/stopping.
  //
  // That is, a worker in the process of starting is not yet considered running,
  // and a worker in the process of starting/stopping is not yet considered
  // running/stopped -- even if it's executing JavaScript.
  //
  // TODO(minggang): Create a new observer to listen to the events when the
  // process of the service worker is allocated/released, instead of using the
  // running status of the embedded worker.
  virtual void OnVersionStartingRunning(int64_t version_id) {}
  virtual void OnVersionStartedRunning(
      int64_t version_id,
      const ServiceWorkerRunningInfo& running_info) {}
  virtual void OnVersionStoppingRunning(int64_t version_id) {}
  virtual void OnVersionStoppedRunning(int64_t version_id) {}

  // Called when the service worker with id |version_id| begins starting or
  // stopping running.
  //
  // These functions are currently called before a worker finishes
  // starting/stopping or the version is destroyed before finishing
  // stopping. That is, a worker in the process of starting/stopping is not yet
  // considered running/stopped, even if it's executing JavaScript.

  // Called when a controllee is added/removed for the service worker with id
  // |version_id|.
  virtual void OnControlleeAdded(int64_t version_id,
                                 const std::string& client_uuid,
                                 const ServiceWorkerClientInfo& client_info) {}
  virtual void OnControlleeRemoved(int64_t version_id,
                                   const std::string& client_uuid) {}

  // Called when there are no more controllees for the service worker with id
  // |version_id|.
  virtual void OnNoControllees(int64_t version_id, const GURL& scope) {}

  // Called when the navigation for a window client commits to a render frame
  // host. At this point, if there was a previous controllee attached to that
  // RenderFrameHost, it has already been removed and OnControlleeRemoved()
  // has been called.
  virtual void OnControlleeNavigationCommitted(
      int64_t version_id,
      const std::string& client_uuid,
      GlobalRenderFrameHostId render_frame_host_id) {}

  // Called when an error is reported for the service worker with id
  // |version_id|.
  virtual void OnErrorReported(int64_t version_id,
                               const GURL& scope,
                               const ErrorInfo& info) {}

  // Called when a console message is reported for the service worker with id
  // |version_id|.
  virtual void OnReportConsoleMessage(int64_t version_id,
                                      const GURL& scope,
                                      const ConsoleMessage& message) {}

  // Called when |context| is destroyed. Observers must no longer use |context|.
  virtual void OnDestruct(ServiceWorkerContext* context) {}

  // Called when a Service Worker opens a new window.
  // |script_url| is the URL of the service worker.
  // |url| is the destination URL.
  virtual void OnWindowOpened(const GURL& script_url, const GURL& url) {}

  // Called when a Service Worker navigates an existing tab.
  // |script_url| is the URL of the service worker.
  // |url| is the destination URL.
  virtual void OnClientNavigated(const GURL& script_url, const GURL& url) {}

 protected:
  virtual ~ServiceWorkerContextObserver() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_SERVICE_WORKER_CONTEXT_OBSERVER_H_
