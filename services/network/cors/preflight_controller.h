// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_CORS_PREFLIGHT_CONTROLLER_H_
#define SERVICES_NETWORK_CORS_PREFLIGHT_CONTROLLER_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/component_export.h"
#include "base/containers/flat_set.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/types/strong_alias.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/cors/preflight_cache.h"
#include "services/network/cors/preflight_result.h"
#include "services/network/public/cpp/cors/cors_error_status.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom-forward.h"
#include "url/gurl.h"

namespace network {

class NetworkService;

namespace cors {

// A class to manage CORS-preflight, making a CORS-preflight request, checking
// its result, and owning a CORS-preflight cache.
class COMPONENT_EXPORT(NETWORK_SERVICE) PreflightController final {
 public:
  using CompletionCallback =
      base::OnceCallback<void(int net_error, base::Optional<CorsErrorStatus>)>;
  using WithTrustedHeaderClient =
      base::StrongAlias<class WithTrustedHeaderClientTag, bool>;
  // Creates a CORS-preflight ResourceRequest for a specified |request| for a
  // URL that is originally requested.
  static std::unique_ptr<ResourceRequest> CreatePreflightRequestForTesting(
      const ResourceRequest& request,
      bool tainted = false);
  // Creates a PreflightResult for a specified response parameters for testing.
  static std::unique_ptr<PreflightResult> CreatePreflightResultForTesting(
      const GURL& final_url,
      const mojom::URLResponseHead& head,
      const ResourceRequest& original_request,
      bool tainted,
      base::Optional<CorsErrorStatus>* detected_error_status);

  explicit PreflightController(NetworkService* network_service);
  ~PreflightController();

  // Determines if a CORS-preflight request is needed, and checks the cache, or
  // makes a preflight request if it is needed. A result will be notified
  // synchronously or asynchronously.
  void PerformPreflightCheck(
      CompletionCallback callback,
      const ResourceRequest& resource_request,
      WithTrustedHeaderClient with_trusted_header_client,
      bool tainted,
      const net::NetworkTrafficAnnotationTag& traffic_annotation,
      mojom::URLLoaderFactory* loader_factory,
      int32_t process_id,
      const net::IsolationInfo& isolation_info);

 private:
  class PreflightLoader;

  void RemoveLoader(PreflightLoader* loader);
  void AppendToCache(const url::Origin& origin,
                     const GURL& url,
                     const net::NetworkIsolationKey& network_isolation_key,
                     std::unique_ptr<PreflightResult> result);

  NetworkService* network_service() { return network_service_; }

  PreflightCache cache_;
  std::set<std::unique_ptr<PreflightLoader>, base::UniquePtrComparator>
      loaders_;

  NetworkService* const network_service_;

  DISALLOW_COPY_AND_ASSIGN(PreflightController);
};

}  // namespace cors

}  // namespace network

#endif  // SERVICES_NETWORK_CORS_PREFLIGHT_CONTROLLER_H_
