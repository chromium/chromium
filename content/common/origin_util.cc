// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/origin_util.h"

#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/stl_util.h"
#include "base/strings/pattern.h"
#include "content/common/url_schemes.h"
#include "net/base/url_util.h"
#include "url/gurl.h"
#include "url/url_util.h"

namespace {

// This function partially reflects the result from SecurityOrigin::isUnique,
// not its actual implementation. It takes into account how
// SecurityOrigin::create might return unique origins for URLs whose schemes are
// included in SchemeRegistry::shouldTreatURLSchemeAsNoAccess.
bool IsOriginUnique(const url::Origin& origin) {
  return origin.opaque() ||
         base::ContainsValue(url::GetNoAccessSchemes(), origin.scheme());
}

bool IsWhitelistedSecureOrigin(const url::Origin& origin) {
  if (base::ContainsValue(content::GetSecureOriginsAndPatterns(),
                          origin.Serialize()))
    return true;
  for (const auto& origin_or_pattern : content::GetSecureOriginsAndPatterns()) {
    if (base::MatchPattern(origin.host(), origin_or_pattern))
      return true;
  }
  return false;
}

}  // namespace

namespace content {

bool IsOriginSecure(const GURL& url) {
  if (url.SchemeIsCryptographic() || url.SchemeIsFile())
    return true;

  if (url.SchemeIsFileSystem() && url.inner_url() &&
      IsOriginSecure(*url.inner_url())) {
    return true;
  }

  if (net::IsLocalhost(url))
    return true;

  if (base::ContainsValue(url::GetSecureSchemes(), url.scheme()))
    return true;

  return IsWhitelistedSecureOrigin(url::Origin::Create(url));
}

bool OriginCanAccessServiceWorkers(const GURL& url) {
  if (url.SchemeIsHTTPOrHTTPS() && IsOriginSecure(url))
    return true;

  if (base::ContainsValue(GetServiceWorkerSchemes(), url.scheme())) {
    return true;
  }

  return false;
}

bool IsPotentiallyTrustworthyOrigin(const url::Origin& origin) {
  // Note: Considering this mirrors SecurityOrigin::isPotentiallyTrustworthy, it
  // assumes m_isUniqueOriginPotentiallyTrustworthy is set to false. This
  // implementation follows Blink's default behavior but in the renderer it can
  // be changed per instance by calls to
  // SecurityOrigin::setUniqueOriginIsPotentiallyTrustworthy.
  if (IsOriginUnique(origin))
    return false;

  if (base::ContainsValue(url::GetSecureSchemes(), origin.scheme()) ||
      base::ContainsValue(url::GetLocalSchemes(), origin.scheme()) ||
      net::IsLocalhost(origin.GetURL())) {
    return true;
  }

  return IsWhitelistedSecureOrigin(origin);
}

}  // namespace content
