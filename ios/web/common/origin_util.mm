// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/common/origin_util.h"

#include "base/stl_util.h"
#include "net/base/url_util.h"
#include "url/gurl.h"
#include "url/url_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

bool IsOriginSecure(const GURL& url) {
  if (url.SchemeIsCryptographic() || url.SchemeIsFile())
    return true;

  // TODO(crbug.com/939077): Also consider inner origins of blob: URLs
  // (ideally, by deleting this function altogether and instead reusing
  // //services/network/public/cpp/is_potentially_trustworthy.h (possibly after
  // moving it to a location that can be consumed by //ios).
  if (url.SchemeIsFileSystem() && url.inner_url() &&
      IsOriginSecure(*url.inner_url())) {
    return true;
  }

  if (base::Contains(url::GetSecureSchemes(), url.scheme()))
    return true;

  if (net::IsLocalhost(url))
    return true;

  return false;
}

}  // namespace web
