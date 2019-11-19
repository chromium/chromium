// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_consts.h"

namespace content {

const char ServiceWorkerConsts::kBadMessageFromNonWindow[] =
    "The request message should not come from a non-window client.";

const char ServiceWorkerConsts::kBadMessageGetRegistrationForReadyDuplicated[] =
    "There's already a completed or ongoing request to get the ready "
    "registration.";

const char ServiceWorkerConsts::kBadMessageImproperOrigins[] =
    "Origins are not matching, or some cannot access service worker.";

const char ServiceWorkerConsts::kBadMessageInvalidURL[] =
    "Some URLs are invalid.";

const char ServiceWorkerConsts::kBadNavigationPreloadHeaderValue[] =
    "The navigation preload header value is invalid.";

const char ServiceWorkerConsts::kDatabaseErrorMessage[] =
    "Failed to access storage.";

const char ServiceWorkerConsts::kEnableNavigationPreloadErrorPrefix[] =
    "Failed to enable or disable navigation preload: ";

const char ServiceWorkerConsts::kGetNavigationPreloadStateErrorPrefix[] =
    "Failed to get navigation preload state: ";

const char ServiceWorkerConsts::kInvalidStateErrorMessage[] =
    "The object is in an invalid state.";

const char ServiceWorkerConsts::kNoActiveWorkerErrorMessage[] =
    "The registration does not have an active worker.";

const char ServiceWorkerConsts::kNoDocumentURLErrorMessage[] =
    "No URL is associated with the caller's document.";

const char ServiceWorkerConsts::kSetNavigationPreloadHeaderErrorPrefix[] =
    "Failed to set navigation preload header: ";

const char ServiceWorkerConsts::kShutdownErrorMessage[] =
    "The Service Worker system has shutdown.";

const char ServiceWorkerConsts::kUpdateTimeoutErrorMesage[] =
    "Service worker self-update limit exceeded.";

const char ServiceWorkerConsts::kUserDeniedPermissionMessage[] =
    "The user denied permission to use Service Worker.";

const char ServiceWorkerConsts::kServiceWorkerRegisterErrorPrefix[] =
    "Failed to register a ServiceWorker for scope ('%s') with script ('%s'): ";

const char ServiceWorkerConsts::kServiceWorkerUpdateErrorPrefix[] =
    "Failed to update a ServiceWorker for scope ('%s') with script ('%s'): ";

const char ServiceWorkerConsts::kServiceWorkerUnregisterErrorPrefix[] =
    "Failed to unregister a ServiceWorkerRegistration: ";

const char ServiceWorkerConsts::kServiceWorkerGetRegistrationErrorPrefix[] =
    "Failed to get a ServiceWorkerRegistration: ";

const char ServiceWorkerConsts::kServiceWorkerGetRegistrationsErrorPrefix[] =
    "Failed to get ServiceWorkerRegistration objects: ";

const char ServiceWorkerConsts::kServiceWorkerFetchScriptError[] =
    "An unknown error occurred when fetching the script.";

const char ServiceWorkerConsts::kServiceWorkerBadHTTPResponseError[] =
    "A bad HTTP response code (%d) was received when fetching the script.";

const char ServiceWorkerConsts::kServiceWorkerSSLError[] =
    "An SSL certificate error occurred when fetching the script.";

const char ServiceWorkerConsts::kServiceWorkerBadMIMEError[] =
    "The script has an unsupported MIME type ('%s').";

const char ServiceWorkerConsts::kServiceWorkerNoMIMEError[] =
    "The script does not have a MIME type.";

const char ServiceWorkerConsts::kServiceWorkerRedirectError[] =
    "The script resource is behind a redirect, which is disallowed.";

const char ServiceWorkerConsts::kServiceWorkerAllowed[] =
    "Service-Worker-Allowed";

const char ServiceWorkerConsts::kServiceWorkerCopyScriptError[] =
    "An unknown error occurred when copying the script.";

const char ServiceWorkerConsts::kServiceWorkerInvalidVersionError[] =
    "Service worker went to a bad state unexpectedly.";

const int ServiceWorkerConsts::kInvalidEmbeddedWorkerThreadId = -1;

const int64_t ServiceWorkerConsts::kInvalidServiceWorkerResourceId = -1;

// static
constexpr base::TimeDelta ServiceWorkerConsts::kServiceWorkerScriptMaxCacheAge;

}  // namespace content
