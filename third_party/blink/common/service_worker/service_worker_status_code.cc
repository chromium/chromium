// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/service_worker/service_worker_status_code.h"

#include "base/notreached.h"

namespace blink {

const char* ServiceWorkerStatusToString(ServiceWorkerStatusCode status) {
  switch (status) {
    case ServiceWorkerStatusCode::kOk:
      return "Operation has succeeded";
    case ServiceWorkerStatusCode::kErrorFailed:
      return "Operation has failed (unknown reason)";
    case ServiceWorkerStatusCode::kErrorAbort:
      return "Operation has been aborted";
    case ServiceWorkerStatusCode::kErrorProcessNotFound:
      return "Could not find a renderer process to run a service worker";
    case ServiceWorkerStatusCode::kErrorNotFound:
      return "Not found";
    case ServiceWorkerStatusCode::kErrorExists:
      return "Already exists";
    case ServiceWorkerStatusCode::kErrorStartWorkerFailed:
      return "ServiceWorker cannot be started";
    case ServiceWorkerStatusCode::kErrorInstallWorkerFailed:
      return "ServiceWorker failed to install";
    case ServiceWorkerStatusCode::kErrorActivateWorkerFailed:
      return "ServiceWorker failed to activate";
    case ServiceWorkerStatusCode::kErrorIpcFailed:
      return "IPC connection was closed or IPC error has occurred";
    case ServiceWorkerStatusCode::kErrorNetwork:
      return "Operation failed by network issue";
    case ServiceWorkerStatusCode::kErrorSecurity:
      return "Operation failed by security issue";
    case ServiceWorkerStatusCode::kErrorEventWaitUntilRejected:
      return "ServiceWorker failed to handle event (event.waitUntil "
             "Promise rejected)";
    case ServiceWorkerStatusCode::kErrorState:
      return "The ServiceWorker state was not valid";
    case ServiceWorkerStatusCode::kErrorTimeout:
      return "The ServiceWorker timed out";
    case ServiceWorkerStatusCode::kErrorScriptEvaluateFailed:
      return "ServiceWorker script evaluation failed";
    case ServiceWorkerStatusCode::kErrorDiskCache:
      return "Disk cache error";
    case ServiceWorkerStatusCode::kErrorRedundant:
      return "Redundant worker";
    case ServiceWorkerStatusCode::kErrorDisallowed:
      return "Worker disallowed";
    case ServiceWorkerStatusCode::kErrorInvalidArguments:
      return "Invalid arguments";
    case ServiceWorkerStatusCode::kErrorStorageDisconnected:
      return "Storage operation error";
    case ServiceWorkerStatusCode::kErrorStorageDataCorrupted:
      return "Storage data corrupted";
  }
  NOTREACHED_IN_MIGRATION();
  return "";
}

}  // namespace blink
