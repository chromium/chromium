// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CONSTS_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CONSTS_H_

#include <stdint.h>

#include "base/time/time.h"
#include "content/common/content_export.h"

namespace content {

struct CONTENT_EXPORT ServiceWorkerConsts {
  static const char kBadMessageFromNonWindow[];
  static const char kBadMessageGetRegistrationForReadyDuplicated[];
  static const char kBadMessageImproperOrigins[];
  static const char kBadMessageInvalidURL[];
  static const char kBadNavigationPreloadHeaderValue[];
  static const char kDatabaseErrorMessage[];
  static const char kEnableNavigationPreloadErrorPrefix[];
  static const char kGetNavigationPreloadStateErrorPrefix[];
  static const char kInvalidStateErrorMessage[];
  static const char kNoActiveWorkerErrorMessage[];
  static const char kNoDocumentURLErrorMessage[];
  static const char kSetNavigationPreloadHeaderErrorPrefix[];
  static const char kShutdownErrorMessage[];
  static const char kUpdateTimeoutErrorMesage[];
  static const char kUserDeniedPermissionMessage[];

  // Constants for error messages.
  static const char kServiceWorkerRegisterErrorPrefix[];
  static const char kServiceWorkerUpdateErrorPrefix[];
  static const char kServiceWorkerUnregisterErrorPrefix[];
  static const char kServiceWorkerGetRegistrationErrorPrefix[];
  static const char kServiceWorkerGetRegistrationsErrorPrefix[];
  static const char kServiceWorkerFetchScriptError[];
  static const char kServiceWorkerBadHTTPResponseError[];
  static const char kServiceWorkerSSLError[];
  static const char kServiceWorkerBadMIMEError[];
  static const char kServiceWorkerNoMIMEError[];
  static const char kServiceWorkerRedirectError[];
  static const char kServiceWorkerAllowed[];
  static const char kServiceWorkerCopyScriptError[];
  static const char kServiceWorkerInvalidVersionError[];

  // Constants for invalid identifiers.
  static const int kInvalidEmbeddedWorkerThreadId;
  static const int64_t kInvalidServiceWorkerResourceId;

  // The HTTP cache is bypassed for Service Worker scripts if the last network
  // fetch occurred over 24 hours ago.
  static constexpr base::TimeDelta kServiceWorkerScriptMaxCacheAge =
      base::TimeDelta::FromHours(24);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CONSTS_H_
