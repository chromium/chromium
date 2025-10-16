// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_NO_VARY_SEARCH_CACHE_TEST_UTILS_H_
#define NET_HTTP_NO_VARY_SEARCH_CACHE_TEST_UTILS_H_

// Utility functions that are useful when testing NoVarySearchCache.

#include <string_view>

#include "base/memory/scoped_refptr.h"

class GURL;

namespace net {

struct HttpRequestInfo;

class HttpResponseHeaders;
class NetworkIsolationKey;
class NoVarySearchCache;

namespace no_vary_search_cache_test_utils {

// Returns the URL "https://www.example.com/?{query}", or just
// "https://www.example.com/" if `query` is empty.
GURL CreateTestURL(std::string_view query = {});

// Creates an HttpRequestInfo object for CreateTestURL(query) as if it was a top
// frame navigation, with the fields used by GenerateCacheKeyRequest()
// initialized.
HttpRequestInfo CreateTestRequest(std::string_view query = {});

// Creates an HttpRequestInfo object for `url` as if it was a top frame
// navigation, with the fields used by GenerateCacheKeyRequest() initialized.
HttpRequestInfo CreateTestRequest(const GURL& url);

// Creates an HttpRequestInfo object for `url` and `nik` with the fields used
// by GenerateCacheKeyRequest() initialized.
HttpRequestInfo CreateTestRequest(const GURL& url,
                                  const NetworkIsolationKey& nik);

// Creates a response header object including the header "No-Vary-Search:
// {no_vary_search_value}".
scoped_refptr<HttpResponseHeaders> CreateTestHeaders(
    std::string_view no_vary_search_value);

// Inserts `query` into `cache` with a No-Vary-Search value of `no_vary_search`.
void Insert(NoVarySearchCache& cache,
            std::string_view query,
            std::string_view no_vary_search);

// Returns true if a URL matching `query` was found in `cache`. Marks the entry
// as recently used as a side-effect.
bool Exists(NoVarySearchCache& cache, std::string_view query);

// Erase an entry from `cache` matching `query` if one exists. Returns true if a
// query was erased.
bool Erase(NoVarySearchCache& cache, std::string_view query);

}  // namespace no_vary_search_cache_test_utils

}  // namespace net

#endif  // NET_HTTP_NO_VARY_SEARCH_CACHE_TEST_UTILS_H_
