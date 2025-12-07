// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/no_vary_search_cache_test_utils.h"

#include <utility>

#include "net/base/load_flags.h"
#include "net/base/network_isolation_key.h"
#include "net/base/schemeful_site.h"
#include "net/http/http_request_info.h"
#include "net/http/http_response_headers.h"
#include "net/http/no_vary_search_cache.h"
#include "url/gurl.h"

namespace net::no_vary_search_cache_test_utils {

GURL CreateTestURL(std::string_view query) {
  GURL url("https://example.com/");
  if (query.empty()) {
    return url;
  }

  GURL::Replacements replacements;
  replacements.SetQueryStr(query);
  return url.ReplaceComponents(replacements);
}

HttpRequestInfo CreateTestRequest(std::string_view query) {
  return CreateTestRequest(CreateTestURL(query));
}

HttpRequestInfo CreateTestRequest(const GURL& url) {
  SchemefulSite site(url);
  return CreateTestRequest(url, NetworkIsolationKey(site, site));
}

HttpRequestInfo CreateTestRequest(const GURL& url,
                                  const NetworkIsolationKey& nik) {
  // Only fill in the fields that GenerateCacheKeyForRequest() looks at.
  HttpRequestInfo request;
  request.url = url;
  request.network_isolation_key = nik;
  request.is_subframe_document_resource = false;
  request.is_main_frame_navigation = true;
  CHECK(!request.upload_data_stream);
  request.load_flags = LOAD_NORMAL;
  CHECK(!request.initiator);
  return request;
}

scoped_refptr<HttpResponseHeaders> CreateTestHeaders(
    std::string_view no_vary_search_value) {
  return HttpResponseHeaders::Builder({1, 1}, "200 OK")
      .AddHeader("No-Vary-Search", no_vary_search_value)
      .Build();
}

void Insert(NoVarySearchCache& cache,
            std::string_view query,
            std::string_view no_vary_search) {
  auto headers = CreateTestHeaders(no_vary_search);
  cache.MaybeInsert(CreateTestRequest(query), *headers);
}

bool Exists(NoVarySearchCache& cache, std::string_view query) {
  return cache.Lookup(CreateTestRequest(query)).has_value();
}

bool Erase(NoVarySearchCache& cache, std::string_view query) {
  auto result = cache.Lookup(CreateTestRequest(query));
  if (!result) {
    return false;
  }
  cache.Erase(std::move(result->erase_handle));
  return true;
}

}  // namespace net::no_vary_search_cache_test_utils
