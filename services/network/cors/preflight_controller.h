// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_CORS_PREFLIGHT_CONTROLLER_H_
#define SERVICES_NETWORK_CORS_PREFLIGHT_CONTROLLER_H_

#include <memory>
#include <optional>

#include "base/component_export.h"
#include "base/containers/enum_set.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "base/types/strong_alias.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/cors/preflight_cache.h"
#include "services/network/cors/preflight_result.h"
#include "services/network/public/cpp/cors/cors_error_status.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/clear_data_filter.mojom-forward.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom-forward.h"
#include "url/gurl.h"

namespace net {
class NetLogWithSource;
}  // namespace net

namespace network {

class NetworkService;

namespace cors {

// Name of a histogram that records preflight errors (CorsError values).
extern const char kPreflightErrorHistogramName[];

// Name of a histogram that records suppressed preflight errors, aka warnings.
extern const char kPreflightWarningHistogramName[];

// Dictates how the PreflightController should treat PNA preflights.
//
// TODO(crbug.com/40204695): Remove this once enforcement is always on.
enum class PrivateNetworkAccessPreflightBehavior {
  // Enforce the presence of PNA headers for PNA preflights.
  kEnforce,

  // Check for PNA headers, but do not fail the request in case of error.
  // Instead, only report a warning to DevTools.
  kWarn,

  // Same as `kWarn`, also apply a short timeout to PNA preflights.
  kWarnWithTimeout,
};
// A class to manage CORS-preflight, making a CORS-preflight request, checking
// its result, and owning a CORS-preflight cache.
class COMPONENT_EXPORT(NETWORK_SERVICE) PreflightController final {
 public:
  // Indicate whether the current preflight is for CORS or PNA or both.
  enum class PreflightType {
    kMinValue = 0,

    kCors = kMinValue,
    kPrivateNetworkAccess = 1,

    kMaxValue = kPrivateNetworkAccess,
  };

  using PreflightMode =
      base::EnumSet<PreflightController::PreflightType,
                    PreflightController::PreflightType::kMinValue,
                    PreflightController::PreflightType::kMaxValue>;

  // Called with the result of `PerformPreflightCheck()`.
  //
  // `net_error` is the overall result of the operation.
  //
  // `cors_error_status` contains additional details about CORS-specific errors.
  // Invariant: `cors_error_status` is nullopt if `net_error` is neither
  // `net::ERR_FAILED` nor `net::OK`.
  // If `net_error` is `net::OK`, then `cors_error_status` may be non-nullopt to
  // indicate a warning-only error arose due to Private Network Access.
  // TODO(crbug.com/40204695): Once PNA preflights are always enforced,
  // stop populating `cors_error_status` when `net_error` is `net::OK`.
  //
  // `has_autorization_covered_by_wildcard` is true iff the request carries an
  // "authorization" header and that header is covered by the wildcard in the
  // preflight response.
  // TODO(crbug.com/40168475): Remove
  // `has_authorization_covered_by_wildcard` once the investigation is done.
  using CompletionCallback =
      base::OnceCallback<void(int net_error,
                              std::optional<CorsErrorStatus> cors_error_status,
                              bool has_authorization_covered_by_wildcard)>;

  using WithTrustedHeaderClient =
      base::StrongAlias<class WithTrustedHeaderClientTag, bool>;

  // TODO(crbug.com/40204695): Remove this once enforcement is always on.
  using EnforcePrivateNetworkAccessHeader =
      base::StrongAlias<class EnforcePrivateNetworkAccessHeaderTag, bool>;

  // Creates a CORS-preflight ResourceRequest for a specified `request` for a
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
      PrivateNetworkAccessPreflightBehavior private_network_access_behavior,
      std::optional<CorsErrorStatus>* detected_error_status);

  // Checks CORS aceess on the CORS-preflight response parameters for testing.
  static base::expected<void, CorsErrorStatus> CheckPreflightAccessForTesting(
      const GURL& response_url,
      const int response_status_code,
      const std::optional<std::string>& allow_origin_header,
      const std::optional<std::string>& allow_credentials_header,
      mojom::CredentialsMode actual_credentials_mode,
      const url::Origin& origin);

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
      NonWildcardRequestHeadersSupport non_wildcard_request_headers_support,
      PrivateNetworkAccessPreflightBehavior private_network_access_behavior,
      bool tainted,
      const net::NetworkTrafficAnnotationTag& traffic_annotation,
      mojom::URLLoaderFactory* loader_factory,
      const net::IsolationInfo& isolation_info,
      mojom::ClientSecurityStatePtr client_security_state,
      base::WeakPtr<mojo::Remote<mojom::DevToolsObserver>> devtools_observer,
      const net::NetLogWithSource& net_log,
      bool acam_preflight_spec_conformant,
      mojo::PendingRemote<mojom::URLLoaderNetworkServiceObserver>
          url_loader_network_service_observer,
      const PreflightMode& preflight_mode);

  // Clears the CORS preflight cache. The time range is always "all time" as
  // the preflight cache max age is capped to 2hrs. in Chrome.
  // It clears origins selectively when the url filter is not null, otherwise
  // clears all its contents.
  void ClearCorsPreflightCache(mojom::ClearDataFilterPtr url_filter);

  PreflightCache& GetPreflightCacheForTesting() { return cache_; }

 private:
  class PreflightLoader;

  void RemoveLoader(PreflightLoader* loader);
  void AppendToCache(const url::Origin& origin,
                     const GURL& url,
                     const net::NetworkIsolationKey& network_isolation_key,
                     mojom::IPAddressSpace target_ip_address_space,
                     std::unique_ptr<PreflightResult> result);

  NetworkService* network_service() { return network_service_; }

  PreflightCache cache_;
  std::set<std::unique_ptr<PreflightLoader>, base::UniquePtrComparator>
      loaders_;

  const raw_ptr<NetworkService> network_service_;
};

}  // namespace cors

}  // namespace network

#endif  // SERVICES_NETWORK_CORS_PREFLIGHT_CONTROLLER_H_
