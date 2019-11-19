// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_SERVICE_WORKER_CONTEXT_OBSERVER_H_
#define CONTENT_PUBLIC_BROWSER_SERVICE_WORKER_CONTEXT_OBSERVER_H_

#include "url/gurl.h"

namespace content {
class ServiceWorkerContext;
struct ConsoleMessage;
struct ServiceWorkerRunningInfo;

class ServiceWorkerContextObserver {
 public:
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

  // Called when the service worker with id |version_id| starts or stops
  // running.
  //
  // These functions are currently only called after a worker finishes
  // starting/stopping or the version is destroyed before finishing
  // stopping. That is, a worker in the process of starting is not yet
  // considered running, even if it's executing JavaScript.
  //
  // TODO(minggang): Create a new observer to listen to the events when the
  // process of the service worker is allocated/released, instead of using the
  // running status of the embedded worker.
  virtual void OnVersionStartedRunning(
      ServiceWorkerContext* context,
      int64_t version_id,
      const ServiceWorkerRunningInfo& running_info) {}
  virtual void OnVersionStoppedRunning(ServiceWorkerContext* context,
                                       int64_t version_id) {}

  // Called when there are no more controllees for the service worker with id
  // |version_id|.
  virtual void OnNoControllees(int64_t version_id, const GURL& scope) {}

  // Called when a console message is reported for the service worker with id
  // |version_id|.
  virtual void OnReportConsoleMessage(int64_t version_id,
                                      const ConsoleMessage& message) {}

  // Called when |context| is destroyed. Observers must no longer use |context|.
  virtual void OnDestruct(ServiceWorkerContext* context) {}

 protected:
  virtual ~ServiceWorkerContextObserver() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_SERVICE_WORKER_CONTEXT_OBSERVER_H_
