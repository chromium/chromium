// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_registration_status.h"

#include <string>

#include "base/debug/crash_logging.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_error_type.mojom.h"

namespace content {

using blink::WebServiceWorkerError;

void GetServiceWorkerErrorTypeForRegistration(
    blink::ServiceWorkerStatusCode status,
    const std::string& status_message,
    blink::mojom::ServiceWorkerErrorType* out_error,
    std::string* out_message) {
  *out_error = blink::mojom::ServiceWorkerErrorType::kUnknown;
  if (!status_message.empty())
    *out_message = status_message;
  else
    *out_message = blink::ServiceWorkerStatusToString(status);
  switch (status) {
    case blink::ServiceWorkerStatusCode::kOk:
      DUMP_WILL_BE_NOTREACHED()
          << "Calling this when status == OK is not allowed";
      return;

    case blink::ServiceWorkerStatusCode::kErrorInstallWorkerFailed:
    case blink::ServiceWorkerStatusCode::kErrorProcessNotFound:
    case blink::ServiceWorkerStatusCode::kErrorRedundant:
    case blink::ServiceWorkerStatusCode::kErrorDisallowed:
    case blink::ServiceWorkerStatusCode::kErrorDiskCache:
      *out_error = blink::mojom::ServiceWorkerErrorType::kInstall;
      return;

    case blink::ServiceWorkerStatusCode::kErrorNotFound:
      *out_error = blink::mojom::ServiceWorkerErrorType::kNotFound;
      return;

      // kErrorStartWorkerFailed, kErrorNetwork, and kErrorSecurity are the
      // failures during starting a worker. kErrorStartWorkerFailed and
      // kErrorNetwork should result in TypeError per spec.
    case blink::ServiceWorkerStatusCode::kErrorStartWorkerFailed:
      *out_error = blink::mojom::ServiceWorkerErrorType::kType;
      return;

    case blink::ServiceWorkerStatusCode::kErrorNetwork:
      *out_error = blink::mojom::ServiceWorkerErrorType::kNetwork;
      return;

    case blink::ServiceWorkerStatusCode::kErrorSecurity:
      *out_error = blink::mojom::ServiceWorkerErrorType::kSecurity;
      return;

    case blink::ServiceWorkerStatusCode::kErrorScriptEvaluateFailed:
      *out_error = blink::mojom::ServiceWorkerErrorType::kScriptEvaluateFailed;
      return;

    case blink::ServiceWorkerStatusCode::kErrorTimeout:
      *out_error = blink::mojom::ServiceWorkerErrorType::kTimeout;
      return;

    case blink::ServiceWorkerStatusCode::kErrorAbort:
      *out_error = blink::mojom::ServiceWorkerErrorType::kAbort;
      return;

    case blink::ServiceWorkerStatusCode::kErrorStorageDataCorrupted:
      // In case of storage data corruption, `register()`, `getRegistration()`
      // or `getRegistrations()` fail. For that case, it might be fine to
      // just return promise error.
      // See: crbug.com/332136252
      return;

    case blink::ServiceWorkerStatusCode::kErrorActivateWorkerFailed:
    case blink::ServiceWorkerStatusCode::kErrorIpcFailed:
    case blink::ServiceWorkerStatusCode::kErrorFailed:
    case blink::ServiceWorkerStatusCode::kErrorExists:
    case blink::ServiceWorkerStatusCode::kErrorEventWaitUntilRejected:
    case blink::ServiceWorkerStatusCode::kErrorState:
    case blink::ServiceWorkerStatusCode::kErrorInvalidArguments:
    case blink::ServiceWorkerStatusCode::kErrorStorageDisconnected:
      // Unexpected, or should have bailed out before calling this, or we don't
      // have a corresponding blink error code yet.
      break;  // Fall through to NOTREACHED().
  }
  SCOPED_CRASH_KEY_NUMBER("GetSWErrTypeForReg", "status",
                          static_cast<uint32_t>(status));
  SCOPED_CRASH_KEY_STRING256("GetSWErrTypeForReg", "status_str",
                             blink::ServiceWorkerStatusToString(status));
  DUMP_WILL_BE_NOTREACHED()
      << "Got unexpected error code: " << static_cast<uint32_t>(status) << " "
      << blink::ServiceWorkerStatusToString(status);
}

}  // namespace content
