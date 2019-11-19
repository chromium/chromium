// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_CORS_PREFLIGHT_CACHE_H_
#define SERVICES_NETWORK_PUBLIC_CPP_CORS_PREFLIGHT_CACHE_H_

#include <map>
#include <memory>
#include <string>

#include "base/component_export.h"
#include "base/macros.h"
#include "net/http/http_request_headers.h"
#include "services/network/public/cpp/cors/preflight_result.h"
#include "services/network/public/mojom/fetch_api.mojom-shared.h"

class GURL;

namespace network {

namespace cors {

// A class to implement CORS-preflight cache that is defined in the fetch spec,
// https://fetch.spec.whatwg.org/#concept-cache.
// TODO(toyoshim): We may consider to clear all cached entries when users'
// network configuration is changed.
class COMPONENT_EXPORT(NETWORK_CPP) PreflightCache final {
 public:
  PreflightCache();
  ~PreflightCache();

  // Appends new |preflight_result| entry to the cache for a specified |origin|
  // and |url|.
  void AppendEntry(const std::string& origin,
                   const GURL& url,
                   std::unique_ptr<PreflightResult> preflight_result);

  // Consults with cached results, and decides if we can skip CORS-preflight or
  // not.
  bool CheckIfRequestCanSkipPreflight(const std::string& origin,
                                      const GURL& url,
                                      mojom::CredentialsMode credentials_mode,
                                      const std::string& method,
                                      const net::HttpRequestHeaders& headers,
                                      bool is_revalidating);

  // Counts cached entries for testing.
  size_t CountEntriesForTesting() const;

  // Purges one cache entry if number of entries is larger than |max_entries|
  // for testing.
  void MayPurgeForTesting(size_t max_entries, size_t purge_unit);

 private:
  void MayPurge(size_t max_entries, size_t purge_unit);

  // A map for caching. This is accessed by a pair of origin and url strings
  // to find a cached entry.
  std::map<std::pair<std::string /* origin */, std::string /* url */>,
           std::unique_ptr<PreflightResult>>
      cache_;

  DISALLOW_COPY_AND_ASSIGN(PreflightCache);
};

}  // namespace cors

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_CORS_PREFLIGHT_CACHE_H_
