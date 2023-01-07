// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/url_util.h"

#include "url/gurl.h"

namespace network {

bool IsURLHandledByNetworkService(const GURL& url) {
  return (url.SchemeIsHTTPOrHTTPS() || url.SchemeIsWSOrWSS());
}

}  // namespace network
