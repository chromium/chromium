// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CONTEXT_CORE_OBSERVER_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CONTEXT_CORE_OBSERVER_H_

#include <stdint.h>
#include <string>

#include "base/callback.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "content/browser/service_worker/service_worker_info.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_provider_type.mojom.h"
#include "url/gurl.h"

namespace content {

enum class EmbeddedWorkerStatus;

struct ConsoleMessage;

class ServiceWorkerContextCoreObserver {
 public:
  struct ErrorInfo {
    ErrorInfo(const base::string16& message,
              int line,
              int column,
              const GURL& url)
        : error_message(message),
          line_number(line),
          column_number(column),
          source_url(url) {}
    ErrorInfo(const ErrorInfo& info)
        : error_message(info.error_message),
          line_number(info.line_number),
          column_number(info.column_number),
          source_url(info.source_url) {}
    const base::string16 error_message;
    const int line_number;
    const int column_number;
    const GURL source_url;
  };
  virtual void OnNewLiveRegistration(int64_t registration_id,
                                     const GURL& scope) {}
  virtual void OnNewLiveVersion(const ServiceWorkerVersionInfo& version_info) {}
  virtual void OnStarting(int64_t version_id) {}
  virtual void OnStarted(int64_t version_id,
                         const GURL& scope,
                         int process_id,
                         const GURL& script_url) {}
  virtual void OnStopping(int64_t version_id) {}
  virtual void OnStopped(int64_t version_id) {}
  // Called when the context core is about to be deleted. After this is called,
  // method calls on this observer will be for a new context core, possibly
  // reusing version/registration IDs previously seen. So this method gives the
  // observer a chance to discard any state it has.
  virtual void OnDeleteAndStartOver() {}
  virtual void OnVersionStateChanged(int64_t version_id,
                                     const GURL& scope,
                                     ServiceWorkerVersion::Status status) {}
  virtual void OnVersionDevToolsRoutingIdChanged(int64_t version_id,
                                                 int process_id,
                                                 int devtools_agent_route_id) {}
  virtual void OnMainScriptHttpResponseInfoSet(
      int64_t version_id,
      base::Time script_response_time,
      base::Time script_last_modified) {}
  virtual void OnErrorReported(int64_t version_id, const ErrorInfo& info) {}
  virtual void OnReportConsoleMessage(int64_t version_id,
                                      const ConsoleMessage& message) {}
  virtual void OnControlleeAdded(int64_t version_id,
                                 const GURL& scope,
                                 const std::string& uuid,
                                 const ServiceWorkerClientInfo& info) {}
  virtual void OnControlleeRemoved(int64_t version_id,
                                   const GURL& scope,
                                   const std::string& uuid) {}
  virtual void OnNoControllees(int64_t version_id, const GURL& scope) {}
  // Called when the ServiceWorkerContainer.register() promise is resolved.
  //
  // This is called before the service worker registration is persisted to
  // storage. The implementation cannot assume that the ServiceWorkerContextCore
  // will find the registration at this point.
  virtual void OnRegistrationCompleted(int64_t registration_id,
                                       const GURL& scope) {}
  // Called after a service worker registration is persisted to storage.
  //
  // This happens after OnRegistrationCompleted(). The implementation can assume
  // that ServiceWorkerContextCore will find the registration, and can safely
  // add user data to the registration.
  virtual void OnRegistrationStored(int64_t registration_id,
                                    const GURL& scope) {}
  virtual void OnRegistrationDeleted(int64_t registration_id,
                                     const GURL& scope) {}

  // Notified when the storage corruption recovery is completed and all stored
  // data is wiped out.
  virtual void OnStorageWiped() {}

 protected:
  virtual ~ServiceWorkerContextCoreObserver() {}
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CONTEXT_CORE_OBSERVER_H_
