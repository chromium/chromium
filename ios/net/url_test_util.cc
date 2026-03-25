// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/net/url_test_util.h"

#include "base/strings/strcat.h"
#include "url/gurl.h"

namespace net {

std::string GetContentAndFragmentForUrl(const GURL& url) {
  return base::StrCat(
      {url.GetContentPiece(), url.has_ref() ? "#" : "", url.ref()});
}

}  // namespace net
