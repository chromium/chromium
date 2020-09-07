// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/loader/network_utils.h"

#include "base/feature_list.h"
#include "build/build_config.h"
#include "net/net_buildflags.h"
#include "third_party/blink/public/common/features.h"

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

}  // namespace network_utils
}  // namespace blink
