// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/origin_util.h"

#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/stl_util.h"
#include "base/strings/pattern.h"
#include "content/common/url_schemes.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "url/gurl.h"
#include "url/url_util.h"

namespace content {

bool IsOriginSecure(const GURL& url) {
  // TODO(lukasza): data: URLs (and opaque origins associated with them) should
  // be considered insecure according to
  // https://www.w3.org/TR/powerful-features/#is-url-trustworthy.
  // Unfortunately, changing this behavior of content::IsOriginSecure breaks
  // quite a few tests for now (e.g. considering data: insecure makes us think
  // that https + data = mixed content), so fixing this is postponed to a
  // follow-up CL.  WIP CL @ https://crrev.com/c/1505897.
  if (url.SchemeIs(url::kDataScheme))
    return true;

  return network::IsUrlPotentiallyTrustworthy(url);
}

bool OriginCanAccessServiceWorkers(const GURL& url) {
  if (url.SchemeIsHTTPOrHTTPS() && IsOriginSecure(url))
    return true;

  if (base::Contains(GetServiceWorkerSchemes(), url.scheme())) {
    return true;
  }

  return false;
}

bool IsPotentiallyTrustworthyOrigin(const url::Origin& origin) {
  return network::IsOriginPotentiallyTrustworthy(origin);
}

}  // namespace content
