// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_CORS_PREFLIGHT_CACHE_H_
#define SERVICES_NETWORK_CORS_PREFLIGHT_CACHE_H_

#include <map>
#include <memory>
#include <string>
#include <tuple>

#include "base/component_export.h"
#include "net/base/network_isolation_key.h"
#include "net/http/http_request_headers.h"
#include "services/network/cors/preflight_result.h"
#include "services/network/public/mojom/clear_data_filter.mojom-forward.h"
#include "services/network/public/mojom/fetch_api.mojom-shared.h"
#include "url/origin.h"

class GURL;

namespace net {
class NetLogWithSource;
}  // namespace net

namespace network {

namespace cors {

// A class to implement CORS-preflight cache that is defined in the fetch spec,
// https://fetch.spec.whatwg.org/#concept-cache.
// TODO(toyoshim): We may consider to clear all cached entries when users'
// network configuration is changed.
class COMPONENT_EXPORT(NETWORK_SERVICE) PreflightCache final {
 public:
  PreflightCache();

  PreflightCache(const PreflightCache&) = delete;
  PreflightCache& operator=(const PreflightCache&) = delete;

  ~PreflightCache();

  // Appends new `preflight_result` entry to the cache for a specified `origin`
  // and `url`.
  void AppendEntry(const url::Origin& origin,
                   const GURL& url,
                   const net::NetworkIsolationKey& network_isolation_key,
                   mojom::IPAddressSpace target_ip_address_space,
                   std::unique_ptr<PreflightResult> preflight_result);

  // Consults with cached results, and decides if we can skip CORS-preflight or
  // not.
  bool CheckIfRequestCanSkipPreflight(
      const url::Origin& origin,
      const GURL& url,
      const net::NetworkIsolationKey& network_isolation_key,
      mojom::IPAddressSpace target_ip_address_space,
      mojom::CredentialsMode credentials_mode,
      const std::string& method,
      const net::HttpRequestHeaders& headers,
      bool is_revalidating,
      const net::NetLogWithSource& net_log,
      bool acam_preflight_spec_conformant);

  void ClearCache(mojom::ClearDataFilterPtr url_filter);

  // Counts cached entries for testing.
  size_t CountEntriesForTesting() const;

  bool DoesEntryExistForTesting(
      const url::Origin& origin,
      const std::string& url,
      const net::NetworkIsolationKey& network_isolation_key,
      mojom::IPAddressSpace target_ip_address_space);

  // Purges one cache entry if number of entries is larger than
  // `max_entries` for testing.
  void MayPurgeForTesting(size_t max_entries, size_t purge_unit);

 private:
  void MayPurge(size_t max_entries, size_t purge_unit);

  // A map for caching. This is accessed by a tuple of origin,
  // url string, and NetworkIsolationKey to find a cached entry.
  std::map<std::tuple<url::Origin /* origin */,
                      std::string /* url */,
                      net::NetworkIsolationKey /* NIK */,
                      mojom::IPAddressSpace /* target_ip_address_space */>,
           std::unique_ptr<PreflightResult>>
      cache_;
};

}  // namespace cors

}  // namespace network

#endif  // SERVICES_NETWORK_CORS_PREFLIGHT_CACHE_H_
