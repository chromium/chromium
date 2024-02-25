// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CONSTS_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CONSTS_H_

#include <stdint.h>

#include "base/time/time.h"

namespace content {

struct ServiceWorkerConsts {
  static constexpr char kBadMessageFromNonWindow[] =
      "The request message should not come from a non-window client.";

  static constexpr char kBadMessageGetRegistrationForReadyDuplicated[] =
      "There's already a completed or ongoing request to get the ready "
      "registration.";

  static constexpr char kBadMessageImproperOrigins[] =
      "Origins are not matching, or some cannot access service worker.";

  static constexpr char kBadMessageInvalidURL[] = "Some URLs are invalid.";

  static constexpr char kBadNavigationPreloadHeaderValue[] =
      "The navigation preload header value is invalid.";

  static constexpr char kDatabaseErrorMessage[] = "Failed to access storage.";

  static constexpr char kEnableNavigationPreloadErrorPrefix[] =
      "Failed to enable or disable navigation preload: ";

  static constexpr char kGetNavigationPreloadStateErrorPrefix[] =
      "Failed to get navigation preload state: ";

  static constexpr char kInvalidStateErrorMessage[] =
      "The object is in an invalid state.";

  static constexpr char kNoActiveWorkerErrorMessage[] =
      "The registration does not have an active worker.";

  static constexpr char kNoDocumentURLErrorMessage[] =
      "No URL is associated with the caller's document.";

  static constexpr char kSetNavigationPreloadHeaderErrorPrefix[] =
      "Failed to set navigation preload header: ";

  static constexpr char kShutdownErrorMessage[] =
      "The Service Worker system has shutdown.";

  static constexpr char kUpdateTimeoutErrorMesage[] =
      "Service worker self-update limit exceeded.";

  static constexpr char kUserDeniedPermissionMessage[] =
      "The user denied permission to use Service Worker.";

  // Constants for error messages.
  static constexpr char kServiceWorkerRegisterErrorPrefix[] =
      "Failed to register a ServiceWorker for scope ('%s') with script "
      "('%s'): ";

  static constexpr char kServiceWorkerUpdateErrorPrefix[] =
      "Failed to update a ServiceWorker for scope ('%s') with script ('%s'): ";

  static constexpr char kServiceWorkerUnregisterErrorPrefix[] =
      "Failed to unregister a ServiceWorkerRegistration: ";

  static constexpr char kServiceWorkerGetRegistrationErrorPrefix[] =
      "Failed to get a ServiceWorkerRegistration: ";

  static constexpr char kServiceWorkerGetRegistrationsErrorPrefix[] =
      "Failed to get ServiceWorkerRegistration objects: ";

  static constexpr char kServiceWorkerFetchScriptError[] =
      "An unknown error occurred when fetching the script.";

  static constexpr char kServiceWorkerBadHTTPResponseError[] =
      "A bad HTTP response code (%d) was received when fetching the script.";

  static constexpr char kServiceWorkerSSLError[] =
      "An SSL certificate error occurred when fetching the script.";

  static constexpr char kServiceWorkerBadMIMEError[] =
      "The script has an unsupported MIME type ('%s').";

  static constexpr char kServiceWorkerNoMIMEError[] =
      "The script does not have a MIME type.";

  static constexpr char kServiceWorkerRedirectError[] =
      "The script resource is behind a redirect, which is disallowed.";

  static constexpr char kServiceWorkerAllowed[] = "Service-Worker-Allowed";

  static constexpr char kServiceWorkerCopyScriptError[] =
      "An unknown error occurred when copying the script.";

  static constexpr char kServiceWorkerInvalidVersionError[] =
      "Service worker went to a bad state unexpectedly.";

  // Constants for invalid identifiers.
  static constexpr int kInvalidEmbeddedWorkerThreadId = -1;

  // The HTTP cache is bypassed for Service Worker scripts if the last network
  // fetch occurred over 24 hours ago.
  static constexpr auto kServiceWorkerScriptMaxCacheAge = base::Hours(24);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CONSTS_H_
