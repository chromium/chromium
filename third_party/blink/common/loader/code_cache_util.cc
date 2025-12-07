// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/loader/code_cache_util.h"

#include "base/strings/strcat.h"
#include "net/base/url_util.h"
#include "url/gurl.h"

namespace blink {

std::string UrlToCodeCacheKey(const GURL& url) {
  return base::StrCat(
      {// Add a prefix so that the key can't be parsed as a valid URL.
       kCodeCacheKeyPrefix,
       // Remove reference, username and password sections of the URL.
       net::SimplifyUrlForRequest(url).spec()});
}

}  // namespace blink
