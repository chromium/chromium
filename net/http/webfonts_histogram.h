// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_WEBFONTS_HISTOGRAM_H_
#define NET_HTTP_WEBFONTS_HISTOGRAM_H_

#include <string>

#include "net/http/http_response_info.h"

// A collection of functions for histogram reporting about web fonts.
namespace net::web_fonts_histogram {

NET_EXPORT void MaybeRecordCacheStatus(
    HttpResponseInfo::CacheEntryStatus cache_status,
    const std::string& key);

}  // namespace net::web_fonts_histogram

#endif  // NET_HTTP_WEBFONTS_HISTOGRAM_H_
