// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_CACHE_UTIL_H_
#define NET_HTTP_HTTP_CACHE_UTIL_H_

namespace net {

class HttpRequestHeaders;

namespace http_cache_util {

// Determines cache-related load flags based on the provided HTTP request
// headers.
//
// This function inspects `extra_headers` for patterns implying specific cache
// behaviors (e.g., "Cache-Control: no-cache", "If-Match"). It can return
// flags like LOAD_DISABLE_CACHE, LOAD_BYPASS_CACHE, or LOAD_VALIDATE_CACHE.
//
// Returns an int representing the determined load flags , or 0 (LOAD_NORMAL) if
// no special cache-related headers are found.
int GetLoadFlagsForExtraHeaders(const HttpRequestHeaders& extra_headers);

}  // namespace http_cache_util
}  // namespace net

#endif  // NET_HTTP_HTTP_CACHE_UTIL_H_
