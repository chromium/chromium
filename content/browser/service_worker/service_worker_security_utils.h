// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_SECURITY_UTILS_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_SECURITY_UTILS_H_

#include <vector>

#include "content/common/content_export.h"
#include "url/gurl.h"

namespace content {
namespace service_worker_security_utils {

// Returns true if |url| can register service workers from Javascript. This
// includes checking if |url| can access Service Workers.
CONTENT_EXPORT bool OriginCanRegisterServiceWorkerFromJavascript(
    const GURL& url);

// Returns true if all members of |urls| have the same origin, and
// OriginCanAccessServiceWorkers is true for this origin.
// If --disable-web-security is enabled, the same origin check is
// not performed.
CONTENT_EXPORT bool AllOriginsMatchAndCanAccessServiceWorkers(
    const std::vector<GURL>& urls);

CONTENT_EXPORT bool IsWebSecurityDisabled();

}  // namespace service_worker_security_utils
}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_SECURITY_UTILS_H_
