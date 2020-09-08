// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/loader/network_utils.h"

#include "base/feature_list.h"
#include "build/build_config.h"
#include "net/net_buildflags.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "third_party/blink/public/common/features.h"
#include "url/url_constants.h"

namespace blink {
namespace network_utils {

bool AlwaysAccessNetwork(
    const scoped_refptr<net::HttpResponseHeaders>& headers) {
  if (!headers)
    return false;

  // RFC 2616, section 14.9.
  return headers->HasHeaderValue("cache-control", "no-cache") ||
         headers->HasHeaderValue("cache-control", "no-store") ||
         headers->HasHeaderValue("pragma", "no-cache") ||
         headers->HasHeaderValue("vary", "*");
}

bool IsURLHandledByNetworkService(const GURL& url) {
  if (url.SchemeIsHTTPOrHTTPS() || url.SchemeIsWSOrWSS())
    return true;
#if !BUILDFLAG(DISABLE_FTP_SUPPORT)
  if (url.SchemeIs(url::kFtpScheme) &&
      base::FeatureList::IsEnabled(features::kFtpProtocol))
    return true;
#endif
  return false;
}

bool IsOriginSecure(const GURL& url) {
  // TODO(lukasza): data: URLs (and opaque origins associated with them) should
  // be considered insecure according to
  // https://www.w3.org/TR/powerful-features/#is-url-trustworthy.
  // Unfortunately, changing this behavior of NetworkUtils::IsOriginSecure
  // breaks quite a few tests for now (e.g. considering data: insecure makes us
  // think that https + data = mixed content), so fixing this is postponed to a
  // follow-up CL.  WIP CL @ https://crrev.com/c/1505897.
  if (url.SchemeIs(url::kDataScheme))
    return true;

  return network::IsUrlPotentiallyTrustworthy(url);
}

}  // namespace network_utils
}  // namespace blink
