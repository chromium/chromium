// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_CORS_PREFLIGHT_CONTROLLER_H_
#define SERVICES_NETWORK_CORS_PREFLIGHT_CONTROLLER_H_

#include <memory>

#include "base/callback.h"
#include "base/component_export.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/macros.h"
#include "base/types/strong_alias.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/cors/preflight_cache.h"
#include "services/network/cors/preflight_result.h"
#include "services/network/public/cpp/cors/cors_error_status.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom-forward.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace net {
class NetLogWithSource;
}  // namespace net

namespace network {

class NetworkService;

namespace cors {

// A class to manage CORS-preflight, making a CORS-preflight request, checking
// its result, and owning a CORS-preflight cache.
class COMPONENT_EXPORT(NETWORK_SERVICE) PreflightController final {
 public:
  using CompletionCallback = base::OnceCallback<
      void(int net_error, absl::optional<CorsErrorStatus>, bool)>;
  using WithTrustedHeaderClient =
      base::StrongAlias<class WithTrustedHeaderClientTag, bool>;
  using WithNonWildcardRequestHeadersSupport =
      PreflightResult::WithNonWildcardRequestHeadersSupport;
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
      absl::optional<CorsErrorStatus>* detected_error_status);
  // Checks CORS aceess on the CORS-preflight response parameters for testing.
  static absl::optional<CorsErrorStatus> CheckPreflightAccessForTesting(
      const GURL& response_url,
      const int response_status_code,
      const absl::optional<std::string>& allow_origin_header,
      const absl::optional<std::string>& allow_credentials_header,
      mojom::CredentialsMode actual_credentials_mode,
      const url::Origin& origin);
  // Checks errors for the currently experimental
  // "Access-Control-Allow-External" header for testing.
  static absl::optional<CorsErrorStatus> CheckExternalPreflightForTesting(
      const absl::optional<std::string>& allow_external);

  explicit PreflightController(NetworkService* network_service);

  PreflightController(const PreflightController&) = delete;
  PreflightController& operator=(const PreflightController&) = delete;

  ~PreflightController();

  // Determines if a CORS-preflight request is needed, and checks the cache, or
  // makes a preflight request if it is needed. A result will be notified
  // synchronously or asynchronously.
  void PerformPreflightCheck(
      CompletionCallback callback,
      const ResourceRequest& resource_request,
      WithTrustedHeaderClient with_trusted_header_client,
      WithNonWildcardRequestHeadersSupport
          with_non_wildcard_request_headers_support,
      bool tainted,
      const net::NetworkTrafficAnnotationTag& traffic_annotation,
      mojom::URLLoaderFactory* loader_factory,
      const net::IsolationInfo& isolation_info,
      mojo::PendingRemote<mojom::DevToolsObserver> devtools_observer,
      const net::NetLogWithSource& net_log);

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
};

}  // namespace cors

}  // namespace network

#endif  // SERVICES_NETWORK_CORS_PREFLIGHT_CONTROLLER_H_
