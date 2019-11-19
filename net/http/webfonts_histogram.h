// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_WEBFONTS_HISTOGRAM_H_
#define NET_HTTP_WEBFONTS_HISTOGRAM_H_

#include <string>

#include "net/http/http_response_info.h"

namespace net {

// A collection of functions for histogram reporting about web fonts.
namespace web_fonts_histogram {

NET_EXPORT void MaybeRecordCacheStatus(
    HttpResponseInfo::CacheEntryStatus cache_status,
    const std::string& key);

}  // namespace web_fonts_histogram

}  // namespace net

#endif  // NET_HTTP_WEBFONTS_HISTOGRAM_H_
