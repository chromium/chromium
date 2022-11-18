// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_security_utils.h"

#include "base/command_line.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/origin_util.h"
#include "content/public/common/url_constants.h"

namespace content {
namespace service_worker_security_utils {

bool OriginCanRegisterServiceWorkerFromJavascript(const GURL& url) {
  // WebUI service workers are always registered in C++.
  if (url.SchemeIs(kChromeUIUntrustedScheme) || url.SchemeIs(kChromeUIScheme))
    return false;

  return OriginCanAccessServiceWorkers(url);
}

bool AllOriginsMatchAndCanAccessServiceWorkers(const std::vector<GURL>& urls) {
  // (A) Check if all origins can access service worker. Every URL must be
  // checked despite the same-origin check below in (B), because GetOrigin()
  // uses the inner URL for filesystem URLs so that https://foo/ and
  // filesystem:https://foo/ are considered equal, but filesystem URLs cannot
  // access service worker.
  for (const GURL& url : urls) {
    if (!OriginCanAccessServiceWorkers(url))
      return false;
  }

  // (B) Check if all origins are equal. Cross-origin access is permitted when
  // --disable-web-security is set.
  if (IsWebSecurityDisabled()) {
    return true;
  }
  const GURL& first = urls.front();
  for (const GURL& url : urls) {
    if (first.DeprecatedGetOriginAsURL() != url.DeprecatedGetOriginAsURL())
      return false;
  }
  return true;
}

bool IsWebSecurityDisabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableWebSecurity);
}

}  // namespace service_worker_security_utils
}  // namespace content
