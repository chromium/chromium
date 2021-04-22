// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/url_util.h"

#include "base/feature_list.h"
#include "build/build_config.h"
#include "net/net_buildflags.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace network {

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

}  // namespace network
