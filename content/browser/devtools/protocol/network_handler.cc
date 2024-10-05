// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/network_handler.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string_view>
#include <utility>

#include "base/barrier_closure.h"
#include "base/base64.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/containers/queue.h"
#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/i18n/i18n_constants.h"
#include "base/i18n/icu_string_conversions.h"
#include "base/memory/raw_ptr.h"
#include "base/process/process_handle.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "content/browser/background_sync/background_sync_manager.h"
#include "content/browser/devtools/devtools_agent_host_impl.h"
#include "content/browser/devtools/devtools_io_context.h"
#include "content/browser/devtools/devtools_stream_file.h"
#include "content/browser/devtools/devtools_stream_pipe.h"
#include "content/browser/devtools/devtools_url_loader_interceptor.h"
#include "content/browser/devtools/protocol/devtools_network_resource_loader.h"
#include "content/browser/devtools/protocol/handler_helpers.h"
#include "content/browser/devtools/protocol/network.h"
#include "content/browser/devtools/protocol/page.h"
#include "content/browser/devtools/protocol/security.h"
#include "content/browser/devtools/render_frame_devtools_agent_host.h"
#include "content/browser/devtools/service_worker_devtools_agent_host.h"
#include "content/browser/devtools/service_worker_devtools_manager.h"
#include "content/browser/loader/url_loader_factory_utils.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/storage_partition_impl.h"
#include "content/browser/url_loader_factory_params_helper.h"
#include "content/browser/web_package/signed_exchange_envelope.h"
#include "content/browser/web_package/signed_exchange_error.h"
#include "content/common/web_package/signed_exchange_utils.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/browsing_data_remover.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/global_request_id.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/resource_context.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/base/host_port_pair.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/upload_bytes_element_reader.h"
#include "net/cert/ct_policy_status.h"
#include "net/cert/ct_sct_to_string.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_inclusion_status.h"
#include "net/cookies/cookie_partition_key.h"
#include "net/cookies/cookie_util.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/http/http_util.h"
#include "net/ssl/ssl_cipher_suite_names.h"
#include "net/ssl/ssl_connection_status_flags.h"
#include "net/url_request/referrer_policy.h"
#include "services/network/public/cpp/data_element.h"
#include "services/network/public/cpp/devtools_observer_util.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/mojom/client_security_state.mojom-shared.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "services/network/public/mojom/devtools_observer.mojom.h"
#include "services/network/public/mojom/http_raw_headers.mojom.h"
#include "services/network/public/mojom/service_worker_router_info.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/blink/public/common/loader/referrer_utils.h"
#include "third_party/blink/public/mojom/navigation/navigation_params.mojom.h"
#include "third_party/blink/public/platform/resource_request_blocked_reason.h"
#include "third_party/boringssl/src/include/openssl/ssl.h"
#include "url/third_party/mozilla/url_parse.h"

namespace content {
namespace protocol {

namespace {

using GetCookiesCallback = Network::Backend::GetCookiesCallback;
using GetAllCookiesCallback = Network::Backend::GetAllCookiesCallback;
using SetCookieCallback = Network::Backend::SetCookieCallback;
using SetCookiesCallback = Network::Backend::SetCookiesCallback;
using DeleteCookiesCallback = Network::Backend::DeleteCookiesCallback;
using ClearBrowserCookiesCallback =
    Network::Backend::ClearBrowserCookiesCallback;

static constexpr char kInvalidCookieFields[] = "Invalid cookie fields";

Network::CertificateTransparencyCompliance SerializeCTPolicyCompliance(
    net::ct::CTPolicyCompliance ct_compliance) {
  switch (ct_compliance) {
    case net::ct::CTPolicyCompliance::CT_POLICY_COMPLIES_VIA_SCTS:
      return Network::CertificateTransparencyComplianceEnum::Compliant;
    case net::ct::CTPolicyCompliance::CT_POLICY_NOT_ENOUGH_SCTS:
    case net::ct::CTPolicyCompliance::CT_POLICY_NOT_DIVERSE_SCTS:
      return Network::CertificateTransparencyComplianceEnum::NotCompliant;
    case net::ct::CTPolicyCompliance::CT_POLICY_BUILD_NOT_TIMELY:
    case net::ct::CTPolicyCompliance::
        CT_POLICY_COMPLIANCE_DETAILS_NOT_AVAILABLE:
      return Network::CertificateTransparencyComplianceEnum::Unknown;
    case net::ct::CTPolicyCompliance::CT_POLICY_COUNT:
      NOTREACHED_IN_MIGRATION();
      return Network::CertificateTransparencyComplianceEnum::Unknown;
  }
  NOTREACHED_IN_MIGRATION();
  return Network::CertificateTransparencyComplianceEnum::Unknown;
}

namespace {
Network::CookiePriority BuildCookiePriority(net::CookiePriority priority) {
  switch (priority) {
    case net::CookiePriority::COOKIE_PRIORITY_HIGH:
      return Network::CookiePriorityEnum::High;
    case net::CookiePriority::COOKIE_PRIORITY_MEDIUM:
      return Network::CookiePriorityEnum::Medium;
    case net::CookiePriority::COOKIE_PRIORITY_LOW:
      return Network::CookiePriorityEnum::Low;
  }
}
Network::CookieSourceScheme BuildCookieSourceScheme(
    net::CookieSourceScheme scheme) {
  switch (scheme) {
    case net::CookieSourceScheme::kUnset:
      return Network::CookieSourceSchemeEnum::Unset;
    case net::CookieSourceScheme::kNonSecure:
      return Network::CookieSourceSchemeEnum::NonSecure;
    case net::CookieSourceScheme::kSecure:
      return Network::CookieSourceSchemeEnum::Secure;
  }
}
std::optional<Network::CookieSameSite> BuildCookieSameSite(
    net::CookieSameSite same_site) {
  switch (same_site) {
    case net::CookieSameSite::STRICT_MODE:
      return Network::CookieSameSiteEnum::Strict;
    case net::CookieSameSite::LAX_MODE:
      return Network::CookieSameSiteEnum::Lax;
    case net::CookieSameSite::NO_RESTRICTION:
      return Network::CookieSameSiteEnum::None;
    case net::CookieSameSite::UNSPECIFIED:
      return std::nullopt;
  }
}

std::unique_ptr<Network::CookiePartitionKey> BuildCookiePartitionKey(
    const std::string& top_level_site,
    bool has_cross_site_ancestor) {
  return Network::CookiePartitionKey::Create()
      .SetTopLevelSite(top_level_site)
      .SetHasCrossSiteAncestor(has_cross_site_ancestor)
      .Build();
}

}  // namespace

std::unique_ptr<Network::Cookie> BuildCookie(
    const net::CanonicalCookie& cookie) {
  std::unique_ptr<Network::Cookie> devtools_cookie =
      Network::Cookie::Create()
          .SetName(cookie.Name())
          .SetValue(cookie.Value())
          .SetDomain(cookie.Domain())
          .SetPath(cookie.Path())
          .SetExpires(cookie.ExpiryDate().is_null()
                          ? -1
                          : cookie.ExpiryDate().InSecondsFSinceUnixEpoch())
          .SetSize(cookie.Name().length() + cookie.Value().length())
          .SetHttpOnly(cookie.IsHttpOnly())
          .SetSecure(cookie.SecureAttribute())
          .SetSession(!cookie.IsPersistent())
          .SetPriority(BuildCookiePriority(cookie.Priority()))
          .SetSameParty(false)
          .SetSourceScheme(BuildCookieSourceScheme(cookie.SourceScheme()))
          .SetSourcePort(cookie.SourcePort())
          .Build();

  std::optional<Network::CookieSourceScheme> maybe_same_site =
      BuildCookieSameSite(cookie.SameSite());
  if (maybe_same_site) {
    devtools_cookie->SetSameSite(*maybe_same_site);
  }
  std::optional<net::CookiePartitionKey> partition_key = cookie.PartitionKey();
  if (partition_key) {
    if (partition_key->IsSerializeable()) {
      base::expected<net::CookiePartitionKey::SerializedCookiePartitionKey,
                     std::string>
          key_serialized_result =
              net::CookiePartitionKey::Serialize(partition_key);
      CHECK(key_serialized_result.has_value());
      devtools_cookie->SetPartitionKey(BuildCookiePartitionKey(
          key_serialized_result->TopLevelSite(),
          key_serialized_result->has_cross_site_ancestor()));
    } else {
      devtools_cookie->SetPartitionKeyOpaque(partition_key->site().opaque());
      // IsSerializeable may return false when the partition key's site is not
      // opaque since we introduced nonce-based cookie partitioning in
      // https://crrev.com/c/3285244.
      // TODO(1225444,1229638): Surface nonce-based cookie partition keys in
      // DevTools.
    }
  }
  return devtools_cookie;
}

class CookieRetrieverNetworkService
    : public base::RefCounted<CookieRetrieverNetworkService> {
 public:
  static void Retrieve(network::mojom::CookieManager* cookie_manager,
                       const std::vector<GURL>& urls,
                       const net::NetworkIsolationKey& network_isolation_key,
                       const net::SiteForCookies& site_for_cookies,
                       std::unique_ptr<GetCookiesCallback> callback) {
    scoped_refptr<CookieRetrieverNetworkService> self =
        new CookieRetrieverNetworkService(std::move(callback));
    net::CookieOptions cookie_options = net::CookieOptions::MakeAllInclusive();
    for (const auto& url : urls) {
      cookie_manager->GetCookieList(
          url, cookie_options,
          net::CookiePartitionKeyCollection::FromOptional(
              net::CookiePartitionKey::FromNetworkIsolationKey(
                  network_isolation_key, site_for_cookies,
                  net::SchemefulSite(url), /*main_frame_navigation=*/false)),
          base::BindOnce(&CookieRetrieverNetworkService::GotCookies, self));
    }
  }

 private:
  friend class base::RefCounted<CookieRetrieverNetworkService>;

  CookieRetrieverNetworkService(std::unique_ptr<GetCookiesCallback> callback)
      : callback_(std::move(callback)) {}

  void GotCookies(const net::CookieAccessResultList& cookies,
                  const net::CookieAccessResultList& excluded_cookies) {
    for (const auto& cookie_with_access_result : cookies) {
      const net::CanonicalCookie& cookie = cookie_with_access_result.cookie;
      // TODO (crbug.com/326605834) Once ancestor chain bit changes are
      // implemented update this method utilize the ancestor bit.
      base::expected<net::CookiePartitionKey::SerializedCookiePartitionKey,
                     std::string>
          serialized_partition_key =
              net::CookiePartitionKey::Serialize(cookie.PartitionKey());
      // We could be missing cookies that have unserializable partition key.
      // Reference the CookiePartitionKey::IsSerializable docs for more details.
      std::string key = base::StringPrintf(
          "%s::%s::%s::%d::%s", cookie.Name().c_str(), cookie.Domain().c_str(),
          cookie.Path().c_str(), cookie.SecureAttribute(),
          serialized_partition_key.has_value()
              ? serialized_partition_key->TopLevelSite().c_str()
              : serialized_partition_key.error().c_str());
      all_cookies_.emplace(std::move(key), cookie);
    }
  }

  ~CookieRetrieverNetworkService() {
    auto cookies = std::make_unique<Array<Network::Cookie>>();
    for (const auto& entry : all_cookies_)
      cookies->emplace_back(BuildCookie(entry.second));
    callback_->sendSuccess(std::move(cookies));
  }

  std::unique_ptr<GetCookiesCallback> callback_;
  std::unordered_map<std::string, net::CanonicalCookie> all_cookies_;
};

namespace {
std::vector<net::CanonicalCookie> FilterCookies(
    const std::vector<net::CanonicalCookie>& cookies,
    const std::string& name,
    const std::string& normalized_domain,
    const std::string& path,
    Maybe<Network::CookiePartitionKey> partition_key) {
  std::vector<net::CanonicalCookie> result;

  for (const auto& cookie : cookies) {
    if (cookie.Name() != name)
      continue;
    if (cookie.Domain() != normalized_domain)
      continue;
    if (!path.empty() && cookie.Path() != path)
      continue;

    if (cookie.PartitionKey().has_value() != partition_key.has_value()) {
      continue;
    }

    if (cookie.PartitionKey().has_value()) {
      base::expected<net::CookiePartitionKey::SerializedCookiePartitionKey,
                     std::string>
          serialized_result =
              net::CookiePartitionKey::Serialize(cookie.PartitionKey());

      if (!serialized_result.has_value() ||
          (serialized_result->TopLevelSite() !=
           partition_key->GetTopLevelSite())) {
        continue;
      }
      // TODO(crbug.com/328043119): Remove checks for
      // AncestorChainBitEnabledInPartitionedCookies after feature is removed.
      if (base::FeatureList::IsEnabled(
              net::features::kAncestorChainBitEnabledInPartitionedCookies) &&
          (serialized_result->has_cross_site_ancestor() !=
           partition_key->GetHasCrossSiteAncestor())) {
        continue;
      }
    }

    result.push_back(cookie);
  }

  return result;
}

void DeleteFilteredCookies(network::mojom::CookieManager* cookie_manager,
                           const std::string& name,
                           const std::string& normalized_domain,
                           const std::string& path,
                           Maybe<Network::CookiePartitionKey> partition_key,
                           std::unique_ptr<DeleteCookiesCallback> callback,
                           const std::vector<net::CanonicalCookie>& cookies) {
  std::vector<net::CanonicalCookie> filtered_list = FilterCookies(
      cookies, name, normalized_domain, path, std::move(partition_key));

  base::RepeatingClosure barrier_closure = base::BarrierClosure(
      filtered_list.size(),
      base::BindOnce(&DeleteCookiesCallback::sendSuccess, std::move(callback)));

  for (auto& cookie : filtered_list) {
    cookie_manager->DeleteCanonicalCookie(
        cookie, base::BindOnce([](base::RepeatingClosure callback,
                                  bool) { callback.Run(); },
                               barrier_closure));
  }
}

absl::variant<net::CookieSourceScheme, Response> GetSourceSchemeFromProtocol(
    const std::string& source_scheme) {
  if (source_scheme == Network::CookieSourceSchemeEnum::Unset) {
    return net::CookieSourceScheme::kUnset;
  } else if (source_scheme == Network::CookieSourceSchemeEnum::NonSecure) {
    return net::CookieSourceScheme::kNonSecure;
  } else if (source_scheme == Network::CookieSourceSchemeEnum::Secure) {
    return net::CookieSourceScheme::kSecure;
  }
  return Response::InvalidParams("Invalid cookie source scheme");
}

absl::variant<int, Response> GetCookieSourcePort(int source_port) {
  // Only {url::PORT_UNSPECIFIED, [1,65535]} are valid.
  if (source_port == url::PORT_UNSPECIFIED ||
      (source_port >= 1 && source_port <= 65535)) {
    return source_port;
  }

  return Response::InvalidParams("Invalid source port");
}

}  // namespace

absl::variant<std::unique_ptr<net::CanonicalCookie>, Response>
MakeCookieFromProtocolValues(
    const std::string& name,
    const std::string& value,
    const std::string& url_spec,
    const std::string& domain,
    const std::string& path,
    bool secure,
    bool http_only,
    const std::string& same_site,
    double expires,
    const std::string& priority,
    const Maybe<std::string>& source_scheme,
    const Maybe<int>& source_port,
    Maybe<Network::CookiePartitionKey>& partition_key) {
  std::string normalized_domain = domain;

  if (url_spec.empty() && domain.empty()) {
    return Response::InvalidParams(
        "At least one of the url or domain needs to be specified");
  }

  GURL source_url;
  if (!url_spec.empty()) {
    source_url = GURL(url_spec);
    if (!source_url.SchemeIsHTTPOrHTTPS())
      return Response::InvalidParams("URL must have scheme http or https");

    secure = secure || source_url.SchemeIsCryptographic();
    if (normalized_domain.empty())
      normalized_domain = source_url.host();
  }

  std::string url_host = normalized_domain;
  if (!normalized_domain.empty()) {
    // The value of |url_host| may have trickled down from a cookie domain,
    // where leading periods are legal. However, since we want to use it as a
    // URL host, we must the leading period if it exists.
    if (normalized_domain[0] == '.')
      url_host.erase(0, 1);
    // If there is no leading period, clear out |normalized_domain|, but keep
    // the value of |url_host|. CreateSanitizedCookie will determine the proper
    // domain from the URL we construct with |url_host|.
    else
      normalized_domain = "";
  }
  GURL url = GURL((secure ? "https://" : "http://") + url_host);

  base::Time expiration_date;
  if (expires >= 0) {
    expiration_date = expires ? base::Time::FromSecondsSinceUnixEpoch(expires)
                              : base::Time::UnixEpoch();
  }

  net::CookieSameSite css = net::CookieSameSite::UNSPECIFIED;
  if (same_site == Network::CookieSameSiteEnum::Lax)
    css = net::CookieSameSite::LAX_MODE;
  if (same_site == Network::CookieSameSiteEnum::Strict)
    css = net::CookieSameSite::STRICT_MODE;
  if (same_site == Network::CookieSameSiteEnum::None)
    css = net::CookieSameSite::NO_RESTRICTION;

  net::CookiePriority cp = net::CookiePriority::COOKIE_PRIORITY_MEDIUM;
  if (priority == Network::CookiePriorityEnum::High)
    cp = net::CookiePriority::COOKIE_PRIORITY_HIGH;
  else if (priority == Network::CookiePriorityEnum::Medium)
    cp = net::CookiePriority::COOKIE_PRIORITY_MEDIUM;
  else if (priority == Network::CookiePriorityEnum::Low)
    cp = net::CookiePriority::COOKIE_PRIORITY_LOW;

  std::optional<net::CookiePartitionKey> cookie_partition_key;
  if (partition_key.has_value() &&
      !partition_key.value().GetTopLevelSite().empty()) {
    base::expected<net::CookiePartitionKey, std::string>
        deserialized_partition_key =
            net::CookiePartitionKey::FromUntrustedInput(
                partition_key->GetTopLevelSite(),
                partition_key->GetHasCrossSiteAncestor());
    if (!deserialized_partition_key.has_value()) {
      return Response::InvalidParams(
          "Deserializing cookie partition key failed");
    }
    cookie_partition_key = deserialized_partition_key.value();
  }
  // TODO(crbug.com/40188414) Add Partitioned to DevTools cookie structures.
  std::unique_ptr<net::CanonicalCookie> cookie =
      net::CanonicalCookie::CreateSanitizedCookie(
          url, name, value, normalized_domain, path, base::Time(),
          expiration_date, base::Time(), secure, http_only, css, cp,
          cookie_partition_key, /*status=*/nullptr);

  if (!cookie)
    return Response::InvalidParams("Sanitizing cookie failed");

  // Update the cookie's sourceScheme unless it's undefined in which case we'll
  // keep the value that was implied from `url` via CreateSanitizedCookie.
  if (source_scheme.has_value()) {
    auto cookie_source_scheme_or_error =
        GetSourceSchemeFromProtocol(source_scheme.value());
    if (absl::holds_alternative<Response>(cookie_source_scheme_or_error)) {
      return absl::get<Response>(std::move(cookie_source_scheme_or_error));
    }
    net::CookieSourceScheme cookie_source_scheme =
        absl::get<net::CookieSourceScheme>(cookie_source_scheme_or_error);
    if (cookie->SecureAttribute() &&
        cookie_source_scheme == net::CookieSourceScheme::kNonSecure) {
      return Response::InvalidParams(
          "Secure attribute cannot be set for a cookie with an insecure source "
          "scheme");
    }

    cookie->SetSourceScheme(cookie_source_scheme);
  }

  // Update the cookie's port unless it's undefined in which case we'll
  // keep the value that was implied from `url` via CreateSanitizedCookie.
  if (source_port.has_value()) {
    auto cookie_source_port_or_error = GetCookieSourcePort(source_port.value());
    if (absl::holds_alternative<Response>(cookie_source_port_or_error)) {
      return absl::get<Response>(std::move(cookie_source_port_or_error));
    }
    int port_value = absl::get<int>(cookie_source_port_or_error);

    // If the url has a port specified it must match the source_port value.
    // Otherwise this set cookie request is considered malformed.
    // Note: Default port values (https: 443, http: 80) are ignored. They will
    // be treated as if they were not specified.
    if (source_url.has_port() && source_url.IntPort() != port_value) {
      return Response::InvalidParams(
          "Source port does not match the url's specified port");
    }

    cookie->SetSourcePort(port_value);
  }

  return cookie;
}

std::vector<GURL> ComputeCookieURLs(RenderFrameHostImpl* frame_host,
                                    Maybe<Array<String>>& protocol_urls) {
  std::vector<GURL> urls;

  if (protocol_urls.has_value()) {
    for (const std::string& url : protocol_urls.value()) {
      urls.emplace_back(url);
    }
  } else {
    base::queue<RenderFrameHostImpl*> queue;
    queue.push(frame_host);
    while (!queue.empty()) {
      RenderFrameHostImpl* node = queue.front();
      queue.pop();

      urls.push_back(node->GetLastCommittedURL());
      for (size_t i = 0; i < node->child_count(); ++i)
        queue.push(node->child_at(i)->current_frame_host());
    }
  }

  return urls;
}

String resourcePriority(net::RequestPriority priority) {
  switch (priority) {
    case net::MINIMUM_PRIORITY:
    case net::IDLE:
      return Network::ResourcePriorityEnum::VeryLow;
    case net::LOWEST:
      return Network::ResourcePriorityEnum::Low;
    case net::LOW:
      return Network::ResourcePriorityEnum::Medium;
    case net::MEDIUM:
      return Network::ResourcePriorityEnum::High;
    case net::HIGHEST:
      return Network::ResourcePriorityEnum::VeryHigh;
  }
  NOTREACHED_IN_MIGRATION();
  return Network::ResourcePriorityEnum::Medium;
}

String referrerPolicy(network::mojom::ReferrerPolicy referrer_policy) {
  switch (referrer_policy) {
    case network::mojom::ReferrerPolicy::kAlways:
      return Network::Request::ReferrerPolicyEnum::UnsafeUrl;
    case network::mojom::ReferrerPolicy::kDefault:
      return referrerPolicy(blink::ReferrerUtils::NetToMojoReferrerPolicy(
          blink::ReferrerUtils::GetDefaultNetReferrerPolicy()));
    case network::mojom::ReferrerPolicy::kNoReferrerWhenDowngrade:
      return Network::Request::ReferrerPolicyEnum::NoReferrerWhenDowngrade;
    case network::mojom::ReferrerPolicy::kNever:
      return Network::Request::ReferrerPolicyEnum::NoReferrer;
    case network::mojom::ReferrerPolicy::kOrigin:
      return Network::Request::ReferrerPolicyEnum::Origin;
    case network::mojom::ReferrerPolicy::kOriginWhenCrossOrigin:
      return Network::Request::ReferrerPolicyEnum::OriginWhenCrossOrigin;
    case network::mojom::ReferrerPolicy::kSameOrigin:
      return Network::Request::ReferrerPolicyEnum::SameOrigin;
    case network::mojom::ReferrerPolicy::kStrictOrigin:
      return Network::Request::ReferrerPolicyEnum::StrictOrigin;
    case network::mojom::ReferrerPolicy::kStrictOriginWhenCrossOrigin:
      return Network::Request::ReferrerPolicyEnum::StrictOriginWhenCrossOrigin;
  }
  NOTREACHED_IN_MIGRATION();
  return Network::Request::ReferrerPolicyEnum::NoReferrerWhenDowngrade;
}

String referrerPolicy(net::ReferrerPolicy referrer_policy) {
  return referrerPolicy(
      blink::ReferrerUtils::NetToMojoReferrerPolicy(referrer_policy));
}

String securityState(const GURL& url, const net::CertStatus& cert_status) {
  if (!url.SchemeIsCryptographic()) {
    // Some origins are considered secure even though they're not cryptographic,
    // so treat them as secure in the UI.
    if (network::IsUrlPotentiallyTrustworthy(url))
      return Security::SecurityStateEnum::Secure;
    return Security::SecurityStateEnum::Insecure;
  }
  if (net::IsCertStatusError(cert_status))
    return Security::SecurityStateEnum::Insecure;
  return Security::SecurityStateEnum::Secure;
}

std::optional<DevToolsURLLoaderInterceptor::InterceptionStage>
ToInterceptorStage(
    const protocol::Network::InterceptionStage& interceptor_stage) {
  if (interceptor_stage == protocol::Network::InterceptionStageEnum::Request) {
    return DevToolsURLLoaderInterceptor::kRequest;
  }
  if (interceptor_stage ==
      protocol::Network::InterceptionStageEnum::HeadersReceived) {
    return DevToolsURLLoaderInterceptor::kResponse;
  }
  return std::nullopt;
}

double timeDelta(base::TimeTicks time,
                 base::TimeTicks start,
                 double invalid_value = -1) {
  return time.is_null() ? invalid_value : (time - start).InMillisecondsF();
}

std::unique_ptr<Network::ResourceTiming> GetTiming(
    const net::LoadTimingInfo& load_timing) {
  if (load_timing.receive_headers_end.is_null())
    return nullptr;

  const base::TimeTicks kNullTicks;
  auto timing =
      Network::ResourceTiming::Create()
          .SetRequestTime((load_timing.request_start - kNullTicks).InSecondsF())
          .SetProxyStart(timeDelta(load_timing.proxy_resolve_start,
                                   load_timing.request_start))
          .SetProxyEnd(timeDelta(load_timing.proxy_resolve_end,
                                 load_timing.request_start))
          .SetDnsStart(timeDelta(load_timing.connect_timing.domain_lookup_start,
                                 load_timing.request_start))
          .SetDnsEnd(timeDelta(load_timing.connect_timing.domain_lookup_end,
                               load_timing.request_start))
          .SetConnectStart(timeDelta(load_timing.connect_timing.connect_start,
                                     load_timing.request_start))
          .SetConnectEnd(timeDelta(load_timing.connect_timing.connect_end,
                                   load_timing.request_start))
          .SetSslStart(timeDelta(load_timing.connect_timing.ssl_start,
                                 load_timing.request_start))
          .SetSslEnd(timeDelta(load_timing.connect_timing.ssl_end,
                               load_timing.request_start))
          .SetWorkerStart(-1)
          .SetWorkerReady(-1)
          .SetWorkerFetchStart(timeDelta(load_timing.service_worker_fetch_start,
                                         load_timing.request_start))
          .SetWorkerRespondWithSettled(
              timeDelta(load_timing.service_worker_respond_with_settled,
                        load_timing.request_start))
          .SetSendStart(
              timeDelta(load_timing.send_start, load_timing.request_start))
          .SetSendEnd(
              timeDelta(load_timing.send_end, load_timing.request_start))
          .SetPushStart(
              timeDelta(load_timing.push_start, load_timing.request_start, 0))
          .SetPushEnd(
              timeDelta(load_timing.push_end, load_timing.request_start, 0))
          .SetReceiveHeadersStart(timeDelta(load_timing.receive_headers_start,
                                            load_timing.request_start))
          .SetReceiveHeadersEnd(timeDelta(load_timing.receive_headers_end,
                                          load_timing.request_start))
          .Build();

  if (base::FeatureList::IsEnabled(
          blink::features::kServiceWorkerStaticRouterTimingInfo)) {
    if (!load_timing.service_worker_router_evaluation_start.is_null()) {
      timing->SetWorkerRouterEvaluationStart(
          timeDelta(load_timing.service_worker_router_evaluation_start,
                    load_timing.request_start));
    }

    if (!load_timing.service_worker_cache_lookup_start.is_null()) {
      timing->SetWorkerCacheLookupStart(
          timeDelta(load_timing.service_worker_cache_lookup_start,
                    load_timing.request_start));
    }
  }

  return timing;
}

std::unique_ptr<Network::ConnectTiming> GetConnectTiming(
    const base::TimeTicks timestamp) {
  const base::TimeTicks kNullTicks;
  return Network::ConnectTiming::Create()
      .SetRequestTime((timestamp - kNullTicks).InSecondsF())
      .Build();
}

std::unique_ptr<base::Value::Dict> GetRawHeaders(
    const std::vector<network::mojom::HttpRawHeaderPairPtr>& headers) {
  auto headers_dict = std::make_unique<base::Value::Dict>();
  for (const auto& header : headers) {
    std::string header_value;
    if (!base::ConvertToUtf8AndNormalize(header->value, base::kCodepageLatin1,
                                         &header_value)) {
      // For response headers, the encoding could be anything, so conversion
      // might fail; in that case this is the most useful thing we can do.
      header_value = header->value;
    }
    // TODO(crbug.com/40228605): Once there's an API to do this without
    // a double lookup, switch do doing so.
    base::Value* value = headers_dict->Find(header->key);
    if (value) {
      *value = base::Value(value->GetString() + '\n' + header_value);
    } else {
      headers_dict->Set(header->key, header_value);
    }
  }
  return headers_dict;
}

String GetProtocol(const GURL& url,
                   const network::mojom::URLResponseHeadDevToolsInfo& info) {
  std::string protocol = info.alpn_negotiated_protocol;
  if (protocol.empty() || protocol == "unknown") {
    if (info.was_fetched_via_spdy) {
      protocol = "h2";
    } else if (url.SchemeIsHTTPOrHTTPS()) {
      protocol = "http";
      if (info.headers) {
        if (info.headers->GetHttpVersion() == net::HttpVersion(0, 9))
          protocol = "http/0.9";
        else if (info.headers->GetHttpVersion() == net::HttpVersion(1, 0))
          protocol = "http/1.0";
        else if (info.headers->GetHttpVersion() == net::HttpVersion(1, 1))
          protocol = "http/1.1";
      }
    } else {
      protocol = url.scheme();
    }
  }
  return protocol;
}

bool GetPostData(
    const network::ResourceRequestBody& request_body,
    protocol::Array<protocol::Network::PostDataEntry>* data_entries,
    std::string* result) {
  const std::vector<network::DataElement>* elements = request_body.elements();
  if (elements->empty())
    return false;
  for (const auto& element : *elements) {
    // TODO(caseq): Also support blobs.
    if (element.type() != network::DataElement::Tag::kBytes)
      return false;
    base::span<const uint8_t> bytes =
        element.As<network::DataElementBytes>().bytes();
    auto data_entry = protocol::Network::PostDataEntry::Create().Build();
    data_entry->SetBytes(protocol::Binary::fromSpan(bytes));
    data_entries->push_back(std::move(data_entry));
    result->append(base::as_string_view(bytes));
  }
  return true;
}

String SignedExchangeErrorErrorFieldToString(SignedExchangeError::Field field) {
  switch (field) {
    case SignedExchangeError::Field::kSignatureSig:
      return Network::SignedExchangeErrorFieldEnum::SignatureSig;
    case SignedExchangeError::Field::kSignatureIintegrity:
      return Network::SignedExchangeErrorFieldEnum::SignatureIntegrity;
    case SignedExchangeError::Field::kSignatureCertUrl:
      return Network::SignedExchangeErrorFieldEnum::SignatureCertUrl;
    case SignedExchangeError::Field::kSignatureCertSha256:
      return Network::SignedExchangeErrorFieldEnum::SignatureCertSha256;
    case SignedExchangeError::Field::kSignatureValidityUrl:
      return Network::SignedExchangeErrorFieldEnum::SignatureValidityUrl;
    case SignedExchangeError::Field::kSignatureTimestamps:
      return Network::SignedExchangeErrorFieldEnum::SignatureTimestamps;
  }
  NOTREACHED_IN_MIGRATION();
  return "";
}

std::unique_ptr<Network::SignedExchangeError> BuildSignedExchangeError(
    const SignedExchangeError& error) {
  std::unique_ptr<Network::SignedExchangeError> signed_exchange_error =
      Network::SignedExchangeError::Create().SetMessage(error.message).Build();
  if (error.field) {
    signed_exchange_error->SetSignatureIndex(error.field->first);
    signed_exchange_error->SetErrorField(
        SignedExchangeErrorErrorFieldToString(error.field->second));
  }
  return signed_exchange_error;
}

std::unique_ptr<Array<Network::SignedExchangeError>> BuildSignedExchangeErrors(
    const std::vector<SignedExchangeError>& errors) {
  auto signed_exchange_errors =
      std::make_unique<protocol::Array<Network::SignedExchangeError>>();
  for (const auto& error : errors)
    signed_exchange_errors->emplace_back(BuildSignedExchangeError(error));
  return signed_exchange_errors;
}

std::unique_ptr<Array<Network::SetCookieBlockedReason>>
GetProtocolBlockedSetCookieReason(net::CookieInclusionStatus status) {
  std::unique_ptr<Array<Network::SetCookieBlockedReason>> blockedReasons =
      std::make_unique<Array<Network::SetCookieBlockedReason>>();
  if (status.HasExclusionReason(
          net::CookieInclusionStatus::EXCLUDE_SECURE_ONLY)) {
    blockedReasons->push_back(Network::SetCookieBlockedReasonEnum::SecureOnly);
  }
  if (status.HasExclusionReason(
          net::CookieInclusionStatus::EXCLUDE_SAMESITE_STRICT)) {
    if (status.HasSchemefulDowngradeWarning()) {
      blockedReasons->push_back(
          Network::SetCookieBlockedReasonEnum::SchemefulSameSiteStrict);
    } else {
      blockedReasons->push_back(
          Network::SetCookieBlockedReasonEnum::SameSiteStrict);
    }
  }
  if (status.HasExclusionReason(
          net::CookieInclusionStatus::EXCLUDE_SAMESITE_LAX)) {
    if (status.HasSchemefulDowngradeWarning()) {
      blockedReasons->push_back(
          Network::SetCookieBlockedReasonEnum::SchemefulSameSiteLax);
    } else {
      blockedReasons->push_back(
          Network::SetCookieBlockedReasonEnum::SameSiteLax);
    }
  }
  if (status.HasExclusionReason(
          net::CookieInclusionStatus::
              EXCLUDE_SAMESITE_UNSPECIFIED_TREATED_AS_LAX)) {
    if (status.HasSchemefulDowngradeWarning()) {
      blockedReasons->push_back(Network::SetCookieBlockedReasonEnum::
                                    SchemefulSameSiteUnspecifiedTreatedAsLax);
    } else {
      blockedReasons->push_back(
          Network::SetCookieBlockedReasonEnum::SameSiteUnspecifiedTreatedAsLax);
    }
  }
  if (status.HasExclusionReason(
          net::CookieInclusionStatus::EXCLUDE_SAMESITE_NONE_INSECURE)) {
    blockedReasons->push_back(
        Network::SetCookieBlockedReasonEnum::SameSiteNoneInsecure);
  }
  if (status.HasExclusionReason(
          net::CookieInclusionStatus::EXCLUDE_USER_PREFERENCES)) {
    blockedReasons->push_back(
        Network::SetCookieBlockedReasonEnum::UserPreferences);
  }
  if (status.HasExclusionReason(
          net::CookieInclusionStatus::
              EXCLUDE_THIRD_PARTY_BLOCKED_WITHIN_FIRST_PARTY_SET)) {
    blockedReasons->push_back(
        Network::SetCookieBlockedReasonEnum::ThirdPartyBlockedInFirstPartySet);
  }
  if (status.HasExclusionReason(
          net::CookieInclusionStatus::EXCLUDE_THIRD_PARTY_PHASEOUT)) {
    blockedReasons->push_back(
        Network::SetCookieBlockedReasonEnum::ThirdPartyPhaseout);
  }
  if (status.HasExclusionReason(
          net::CookieInclusionStatus::EXCLUDE_FAILURE_TO_STORE)) {
    blockedReasons->push_back(Network::SetCookieBlockedReasonEnum::SyntaxError);
  }
  if (status.HasExclusionReason(
          net::CookieInclusionStatus::EXCLUDE_NONCOOKIEABLE_SCHEME)) {
    blockedReasons->push_back(
        Network::SetCookieBlockedReasonEnum::SchemeNotSupported);
  }
  if (status.HasExclusionReason(
          net::CookieInclusionStatus::EXCLUDE_OVERWRITE_SECURE)) {
    blockedReasons->push_back(
        Network::SetCookieBlockedReasonEnum::OverwriteSecure);
  }
  if (status.HasExclusionReason(
          net::CookieInclusionStatus::EXCLUDE_INVALID_DOMAIN)) {
    blockedReasons->push_back(
        Network::SetCookieBlockedReasonEnum::InvalidDomain);
  }
  if (status.HasExclusionReason(
          net::CookieInclusionStatus::EXCLUDE_INVALID_PREFIX)) {
    blockedReasons->push_back(
        Network::SetCookieBlockedReasonEnum::InvalidPrefix);
  }
  if (status.HasExclusionReason(net::CookieInclusionStatus::
                                    EXCLUDE_NAME_VALUE_PAIR_EXCEEDS_MAX_SIZE)) {
    blockedReasons->push_back(
        Network::SetCookieBlockedReasonEnum::NameValuePairExceedsMaxSize);
  }
  if (status.HasExclusionReason(
          net::CookieInclusionStatus::EXCLUDE_DISALLOWED_CHARACTER)) {
    blockedReasons->push_back(
        Network::SetCookieBlockedReasonEnum::DisallowedCharacter);
  }
  if (status.HasExclusionReason(
          net::CookieInclusionStatus::EXCLUDE_UNKNOWN_ERROR)) {
    blockedReasons->push_back(
        Network::SetCookieBlockedReasonEnum::UnknownError);
  }
  if (status.HasExclusionReason(
          net::CookieInclusionStatus::EXCLUDE_NO_COOKIE_CONTENT)) {
    blockedReasons->push_back(
        Network::SetCookieBlockedReasonEnum::NoCookieContent);
  }

  return blockedReasons;
}

std::unique_ptr<Array<Network::CookieBlockedReason>>
GetProtocolBlockedCookieReason(net::CookieInclusionStatus status) {
  std::unique_ptr<Array<Network::CookieBlockedReason>> blockedReasons =
      std::make_unique<Array<Network::CookieBlockedReason>>();

  if (status.HasExclusionReason(
          net::CookieInclusionStatus::EXCLUDE_SECURE_ONLY)) {
    blockedReasons->push_back(Network::CookieBlockedReasonEnum::SecureOnly);
  }
  if (status.HasExclusionReason(
          net::CookieInclusionStatus::EXCLUDE_NOT_ON_PATH)) {
    blockedReasons->push_back(Network::CookieBlockedReasonEnum::NotOnPath);
  }
  if (status.HasExclusionReason(
          net::CookieInclusionStatus::EXCLUDE_DOMAIN_MISMATCH)) {
    blockedReasons->push_back(Network::CookieBlockedReasonEnum::DomainMismatch);
  }
  if (status.HasExclusionReason(
          net::CookieInclusionStatus::EXCLUDE_SAMESITE_STRICT)) {
    if (status.HasSchemefulDowngradeWarning()) {
      blockedReasons->push_back(
          Network::CookieBlockedReasonEnum::SchemefulSameSiteStrict);
    } else {
      blockedReasons->push_back(
          Network::CookieBlockedReasonEnum::SameSiteStrict);
    }
  }
  if (status.HasExclusionReason(
          net::CookieInclusionStatus::EXCLUDE_SAMESITE_LAX)) {
    if (status.HasSchemefulDowngradeWarning()) {
      blockedReasons->push_back(
          Network::CookieBlockedReasonEnum::SchemefulSameSiteLax);
    } else {
      blockedReasons->push_back(Network::CookieBlockedReasonEnum::SameSiteLax);
    }
  }
  if (status.HasExclusionReason(
          net::CookieInclusionStatus::
              EXCLUDE_SAMESITE_UNSPECIFIED_TREATED_AS_LAX)) {
    if (status.HasSchemefulDowngradeWarning()) {
      blockedReasons->push_back(Network::CookieBlockedReasonEnum::
                                    SchemefulSameSiteUnspecifiedTreatedAsLax);
    } else {
      blockedReasons->push_back(
          Network::CookieBlockedReasonEnum::SameSiteUnspecifiedTreatedAsLax);
    }
  }
  if (status.HasExclusionReason(
          net::CookieInclusionStatus::EXCLUDE_SAMESITE_NONE_INSECURE)) {
    blockedReasons->push_back(
        Network::CookieBlockedReasonEnum::SameSiteNoneInsecure);
  }
  if (status.HasExclusionReason(
          net::CookieInclusionStatus::EXCLUDE_USER_PREFERENCES)) {
    blockedReasons->push_back(
        Network::CookieBlockedReasonEnum::UserPreferences);
  }
  if (status.HasExclusionReason(
          net::CookieInclusionStatus::
              EXCLUDE_THIRD_PARTY_BLOCKED_WITHIN_FIRST_PARTY_SET)) {
    blockedReasons->push_back(
        Network::CookieBlockedReasonEnum::ThirdPartyBlockedInFirstPartySet);
  }
  if (status.HasExclusionReason(
          net::CookieInclusionStatus::EXCLUDE_THIRD_PARTY_PHASEOUT)) {
    blockedReasons->push_back(
        Network::CookieBlockedReasonEnum::ThirdPartyPhaseout);
  }
  if (status.HasExclusionReason(net::CookieInclusionStatus::
                                    EXCLUDE_NAME_VALUE_PAIR_EXCEEDS_MAX_SIZE)) {
    blockedReasons->push_back(
        Network::CookieBlockedReasonEnum::NameValuePairExceedsMaxSize);
  }
  if (status.HasExclusionReason(
          net::CookieInclusionStatus::EXCLUDE_UNKNOWN_ERROR)) {
    blockedReasons->push_back(Network::CookieBlockedReasonEnum::UnknownError);
  }

  return blockedReasons;
}

std::unique_ptr<Array<Network::BlockedSetCookieWithReason>>
BuildProtocolBlockedSetCookies(
    const net::CookieAndLineAccessResultList& net_list) {
  std::unique_ptr<Array<Network::BlockedSetCookieWithReason>> protocol_list =
      std::make_unique<Array<Network::BlockedSetCookieWithReason>>();

  for (const net::CookieAndLineWithAccessResult& cookie : net_list) {
    std::unique_ptr<Array<Network::SetCookieBlockedReason>> blocked_reasons =
        GetProtocolBlockedSetCookieReason(cookie.access_result.status);
    if (!blocked_reasons->size())
      continue;

    protocol_list->push_back(
        Network::BlockedSetCookieWithReason::Create()
            .SetBlockedReasons(std::move(blocked_reasons))
            .SetCookieLine(cookie.cookie_string)
            .SetCookie(cookie.cookie.has_value()
                           ? BuildCookie(cookie.cookie.value())
                           : nullptr)
            .Build());
  }
  return protocol_list;
}

Network::CookieExemptionReason GetProtocolCookieExemptionReason(
    net::CookieInclusionStatus status) {
  switch (status.exemption_reason()) {
    case net::CookieInclusionStatus::ExemptionReason::kNone:
      return Network::CookieExemptionReasonEnum::None;
    case net::CookieInclusionStatus::ExemptionReason::kUserSetting:
      return Network::CookieExemptionReasonEnum::UserSetting;
    case net::CookieInclusionStatus::ExemptionReason::k3PCDMetadata:
      return Network::CookieExemptionReasonEnum::TPCDMetadata;
    case net::CookieInclusionStatus::ExemptionReason::k3PCDDeprecationTrial:
      return Network::CookieExemptionReasonEnum::TPCDDeprecationTrial;
    case net::CookieInclusionStatus::ExemptionReason::
        kTopLevel3PCDDeprecationTrial:
      return Network::CookieExemptionReasonEnum::TopLevelTPCDDeprecationTrial;
    case net::CookieInclusionStatus::ExemptionReason::k3PCDHeuristics:
      return Network::CookieExemptionReasonEnum::TPCDHeuristics;
    case net::CookieInclusionStatus::ExemptionReason::kEnterprisePolicy:
      return Network::CookieExemptionReasonEnum::EnterprisePolicy;
    case net::CookieInclusionStatus::ExemptionReason::kStorageAccess:
      return Network::CookieExemptionReasonEnum::StorageAccess;
    case net::CookieInclusionStatus::ExemptionReason::kTopLevelStorageAccess:
      return Network::CookieExemptionReasonEnum::TopLevelStorageAccess;
    case net::CookieInclusionStatus::ExemptionReason::kScheme:
      return Network::CookieExemptionReasonEnum::Scheme;
  }
}

std::unique_ptr<Array<Network::ExemptedSetCookieWithReason>>
BuildProtocolExemptedSetCookies(
    const net::CookieAndLineAccessResultList& net_list) {
  std::unique_ptr<Array<Network::ExemptedSetCookieWithReason>> protocol_list =
      std::make_unique<Array<Network::ExemptedSetCookieWithReason>>();

  for (const auto& cookie : net_list) {
    Network::CookieExemptionReason exemption_reason =
        GetProtocolCookieExemptionReason(cookie.access_result.status);
    if (exemption_reason != Network::CookieExemptionReasonEnum::None) {
      // The exempted cookies are guaranteed to be valid.
      protocol_list->push_back(
          Network::ExemptedSetCookieWithReason::Create()
              .SetExemptionReason(std::move(exemption_reason))
              .SetCookieLine(cookie.cookie_string)
              .SetCookie(BuildCookie(cookie.cookie.value()))
              .Build());
    }
  }
  return protocol_list;
}

std::pair<std::unique_ptr<Array<Network::CookieBlockedReason>>,
          Network::CookieExemptionReason>
GetProtocolAssociatedCookie(net::CookieInclusionStatus status) {
  std::unique_ptr<Array<Network::CookieBlockedReason>> blocked_reasons =
      std::make_unique<Array<Network::CookieBlockedReason>>();
  blocked_reasons = GetProtocolBlockedCookieReason(status);

  Network::CookieExemptionReason exemption_reason =
      GetProtocolCookieExemptionReason(status);

  return std::make_pair(std::move(blocked_reasons),
                        std::move(exemption_reason));
}

std::unique_ptr<Array<Network::AssociatedCookie>>
BuildProtocolAssociatedCookies(const net::CookieAccessResultList& net_list) {
  auto protocol_list = std::make_unique<Array<Network::AssociatedCookie>>();

  for (const net::CookieWithAccessResult& cookie : net_list) {
    std::pair<std::unique_ptr<Array<Network::CookieBlockedReason>>,
              Network::CookieExemptionReason>
        cookie_with_reasons =
            GetProtocolAssociatedCookie(cookie.access_result.status);
    // Note that the condition below is not always true, as there might be
    // blocked reasons that we do not report.
    if (cookie_with_reasons.first->size() ||
        cookie.access_result.status.IsInclude()) {
      protocol_list->push_back(
          Network::AssociatedCookie::Create()
              .SetCookie(BuildCookie(cookie.cookie))
              .SetBlockedReasons(std::move(cookie_with_reasons.first))
              .SetExemptionReason(std::move(cookie_with_reasons.second))
              .Build());
    }
  }
  return protocol_list;
}

using SourceTypeEnum = net::SourceStream::SourceType;
namespace ContentEncodingEnum = protocol::Network::ContentEncodingEnum;
std::optional<SourceTypeEnum> SourceTypeFromProtocol(
    const protocol::Network::ContentEncoding& encoding) {
  if (ContentEncodingEnum::Gzip == encoding)
    return SourceTypeEnum::TYPE_GZIP;
  if (ContentEncodingEnum::Br == encoding)
    return SourceTypeEnum::TYPE_BROTLI;
  if (ContentEncodingEnum::Deflate == encoding)
    return SourceTypeEnum::TYPE_DEFLATE;
  if (ContentEncodingEnum::Zstd == encoding) {
    return SourceTypeEnum::TYPE_ZSTD;
  }
  return std::nullopt;
}

}  // namespace

class BackgroundSyncRestorer {
 public:
  BackgroundSyncRestorer(const std::string& host_id,
                         StoragePartition* storage_partition)
      : host_id_(host_id), storage_partition_(storage_partition) {
    SetServiceWorkerOfflineStatus(true);
  }

  BackgroundSyncRestorer(const BackgroundSyncRestorer&) = delete;
  BackgroundSyncRestorer& operator=(const BackgroundSyncRestorer&) = delete;

  ~BackgroundSyncRestorer() { SetServiceWorkerOfflineStatus(false); }

  void SetStoragePartition(StoragePartition* storage_partition) {
    storage_partition_ = storage_partition;
  }

 private:
  void SetServiceWorkerOfflineStatus(bool offline) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    scoped_refptr<DevToolsAgentHost> host =
        DevToolsAgentHost::GetForId(host_id_);
    if (!host || !storage_partition_ ||
        host->GetType() != DevToolsAgentHost::kTypeServiceWorker) {
      return;
    }
    scoped_refptr<ServiceWorkerDevToolsAgentHost> service_worker_host =
        static_cast<ServiceWorkerDevToolsAgentHost*>(host.get());
    scoped_refptr<BackgroundSyncContextImpl> sync_context =
        static_cast<StoragePartitionImpl*>(storage_partition_)
            ->GetBackgroundSyncContext();
    if (offline) {
      auto* swcontext = static_cast<ServiceWorkerContextWrapper*>(
          storage_partition_->GetServiceWorkerContext());
      ServiceWorkerVersion* version =
          swcontext->GetLiveVersion(service_worker_host->version_id());
      if (!version)
        return;
      offline_sw_registration_id_ = version->registration_id();
    }
    if (offline_sw_registration_id_ ==
        blink::mojom::kInvalidServiceWorkerRegistrationId)
      return;
    sync_context->background_sync_manager()->EmulateServiceWorkerOffline(
        offline_sw_registration_id_, offline);
  }

  std::string host_id_;
  raw_ptr<StoragePartition> storage_partition_;
  int64_t offline_sw_registration_id_ =
      blink::mojom::kInvalidServiceWorkerRegistrationId;
};

NetworkHandler::NetworkHandler(
    const std::string& host_id,
    const base::UnguessableToken& devtools_token,
    DevToolsIOContext* io_context,
    base::RepeatingClosure update_loader_factories_callback,
    DevToolsAgentHostClient* client)
    : DevToolsDomainHandler(Network::Metainfo::domainName),
      host_id_(host_id),
      devtools_token_(devtools_token),
      io_context_(io_context),
      client_(client),
      browser_context_(nullptr),
      storage_partition_(nullptr),
      host_(nullptr),
      enabled_(false),
#if BUILDFLAG(ENABLE_REPORTING)
      reporting_receiver_(this),
#endif  // BUILDFLAG(ENABLE_REPORTING)
      bypass_service_worker_(false),
      cache_disabled_(false),
      update_loader_factories_callback_(
          std::move(update_loader_factories_callback)) {
  DCHECK(io_context_);
  static bool have_configured_service_worker_context = false;
  if (have_configured_service_worker_context)
    return;
  have_configured_service_worker_context = true;
}

NetworkHandler::~NetworkHandler() = default;

// static
std::unique_ptr<Array<Network::Cookie>> NetworkHandler::BuildCookieArray(
    const std::vector<net::CanonicalCookie>& cookie_list) {
  auto cookies = std::make_unique<Array<Network::Cookie>>();

  for (const net::CanonicalCookie& cookie : cookie_list)
    cookies->emplace_back(BuildCookie(cookie));

  return cookies;
}

// static
net::Error NetworkHandler::NetErrorFromString(const std::string& error,
                                              bool* ok) {
  *ok = true;
  if (error == Network::ErrorReasonEnum::Failed)
    return net::ERR_FAILED;
  if (error == Network::ErrorReasonEnum::Aborted)
    return net::ERR_ABORTED;
  if (error == Network::ErrorReasonEnum::TimedOut)
    return net::ERR_TIMED_OUT;
  if (error == Network::ErrorReasonEnum::AccessDenied)
    return net::ERR_ACCESS_DENIED;
  if (error == Network::ErrorReasonEnum::ConnectionClosed)
    return net::ERR_CONNECTION_CLOSED;
  if (error == Network::ErrorReasonEnum::ConnectionReset)
    return net::ERR_CONNECTION_RESET;
  if (error == Network::ErrorReasonEnum::ConnectionRefused)
    return net::ERR_CONNECTION_REFUSED;
  if (error == Network::ErrorReasonEnum::ConnectionAborted)
    return net::ERR_CONNECTION_ABORTED;
  if (error == Network::ErrorReasonEnum::ConnectionFailed)
    return net::ERR_CONNECTION_FAILED;
  if (error == Network::ErrorReasonEnum::NameNotResolved)
    return net::ERR_NAME_NOT_RESOLVED;
  if (error == Network::ErrorReasonEnum::InternetDisconnected)
    return net::ERR_INTERNET_DISCONNECTED;
  if (error == Network::ErrorReasonEnum::AddressUnreachable)
    return net::ERR_ADDRESS_UNREACHABLE;
  if (error == Network::ErrorReasonEnum::BlockedByClient)
    return net::ERR_BLOCKED_BY_CLIENT;
  if (error == Network::ErrorReasonEnum::BlockedByResponse)
    return net::ERR_BLOCKED_BY_RESPONSE;
  *ok = false;
  return net::ERR_FAILED;
}

// static
String NetworkHandler::NetErrorToString(int net_error) {
  switch (net_error) {
    case net::ERR_ABORTED:
      return Network::ErrorReasonEnum::Aborted;
    case net::ERR_TIMED_OUT:
      return Network::ErrorReasonEnum::TimedOut;
    case net::ERR_ACCESS_DENIED:
      return Network::ErrorReasonEnum::AccessDenied;
    case net::ERR_CONNECTION_CLOSED:
      return Network::ErrorReasonEnum::ConnectionClosed;
    case net::ERR_CONNECTION_RESET:
      return Network::ErrorReasonEnum::ConnectionReset;
    case net::ERR_CONNECTION_REFUSED:
      return Network::ErrorReasonEnum::ConnectionRefused;
    case net::ERR_CONNECTION_ABORTED:
      return Network::ErrorReasonEnum::ConnectionAborted;
    case net::ERR_CONNECTION_FAILED:
      return Network::ErrorReasonEnum::ConnectionFailed;
    case net::ERR_NAME_NOT_RESOLVED:
      return Network::ErrorReasonEnum::NameNotResolved;
    case net::ERR_INTERNET_DISCONNECTED:
      return Network::ErrorReasonEnum::InternetDisconnected;
    case net::ERR_ADDRESS_UNREACHABLE:
      return Network::ErrorReasonEnum::AddressUnreachable;
    case net::ERR_BLOCKED_BY_CLIENT:
      return Network::ErrorReasonEnum::BlockedByClient;
    case net::ERR_BLOCKED_BY_RESPONSE:
      return Network::ErrorReasonEnum::BlockedByResponse;
    default:
      return Network::ErrorReasonEnum::Failed;
  }
}

// static
bool NetworkHandler::AddInterceptedResourceType(
    const std::string& resource_type,
    base::flat_set<blink::mojom::ResourceType>* intercepted_resource_types) {
  if (resource_type == protocol::Network::ResourceTypeEnum::Document) {
    intercepted_resource_types->insert(blink::mojom::ResourceType::kMainFrame);
    intercepted_resource_types->insert(blink::mojom::ResourceType::kSubFrame);
    return true;
  }
  if (resource_type == protocol::Network::ResourceTypeEnum::Stylesheet) {
    intercepted_resource_types->insert(blink::mojom::ResourceType::kStylesheet);
    return true;
  }
  if (resource_type == protocol::Network::ResourceTypeEnum::Image) {
    intercepted_resource_types->insert(blink::mojom::ResourceType::kImage);
    return true;
  }
  if (resource_type == protocol::Network::ResourceTypeEnum::Media) {
    intercepted_resource_types->insert(blink::mojom::ResourceType::kMedia);
    return true;
  }
  if (resource_type == protocol::Network::ResourceTypeEnum::Font) {
    intercepted_resource_types->insert(
        blink::mojom::ResourceType::kFontResource);
    return true;
  }
  if (resource_type == protocol::Network::ResourceTypeEnum::Script) {
    intercepted_resource_types->insert(blink::mojom::ResourceType::kScript);
    return true;
  }
  if (resource_type == protocol::Network::ResourceTypeEnum::XHR) {
    intercepted_resource_types->insert(blink::mojom::ResourceType::kXhr);
    return true;
  }
  if (resource_type == protocol::Network::ResourceTypeEnum::Fetch) {
    intercepted_resource_types->insert(blink::mojom::ResourceType::kPrefetch);
    return true;
  }
  if (resource_type ==
      protocol::Network::ResourceTypeEnum::CSPViolationReport) {
    intercepted_resource_types->insert(blink::mojom::ResourceType::kCspReport);
    return true;
  }
  if (resource_type == protocol::Network::ResourceTypeEnum::Ping) {
    intercepted_resource_types->insert(blink::mojom::ResourceType::kPing);
    return true;
  }
  if (resource_type == protocol::Network::ResourceTypeEnum::Other) {
    intercepted_resource_types->insert(
        blink::mojom::ResourceType::kSubResource);
    intercepted_resource_types->insert(blink::mojom::ResourceType::kObject);
    intercepted_resource_types->insert(blink::mojom::ResourceType::kWorker);
    intercepted_resource_types->insert(
        blink::mojom::ResourceType::kSharedWorker);
    intercepted_resource_types->insert(blink::mojom::ResourceType::kFavicon);
    intercepted_resource_types->insert(
        blink::mojom::ResourceType::kServiceWorker);
    intercepted_resource_types->insert(
        blink::mojom::ResourceType::kPluginResource);
    return true;
  }
  return false;
}

// static
const char* NetworkHandler::ResourceTypeToString(
    blink::mojom::ResourceType resource_type) {
  switch (resource_type) {
    case blink::mojom::ResourceType::kMainFrame:
      return protocol::Network::ResourceTypeEnum::Document;
    case blink::mojom::ResourceType::kSubFrame:
      return protocol::Network::ResourceTypeEnum::Document;
    case blink::mojom::ResourceType::kStylesheet:
      return protocol::Network::ResourceTypeEnum::Stylesheet;
    case blink::mojom::ResourceType::kScript:
      return protocol::Network::ResourceTypeEnum::Script;
    case blink::mojom::ResourceType::kImage:
      return protocol::Network::ResourceTypeEnum::Image;
    case blink::mojom::ResourceType::kFontResource:
      return protocol::Network::ResourceTypeEnum::Font;
    case blink::mojom::ResourceType::kSubResource:
      return protocol::Network::ResourceTypeEnum::Other;
    case blink::mojom::ResourceType::kObject:
      return protocol::Network::ResourceTypeEnum::Other;
    case blink::mojom::ResourceType::kMedia:
      return protocol::Network::ResourceTypeEnum::Media;
    case blink::mojom::ResourceType::kWorker:
      return protocol::Network::ResourceTypeEnum::Other;
    case blink::mojom::ResourceType::kSharedWorker:
      return protocol::Network::ResourceTypeEnum::Other;
    case blink::mojom::ResourceType::kPrefetch:
      return protocol::Network::ResourceTypeEnum::Fetch;
    case blink::mojom::ResourceType::kFavicon:
      return protocol::Network::ResourceTypeEnum::Other;
    case blink::mojom::ResourceType::kXhr:
      return protocol::Network::ResourceTypeEnum::XHR;
    case blink::mojom::ResourceType::kPing:
      return protocol::Network::ResourceTypeEnum::Ping;
    case blink::mojom::ResourceType::kServiceWorker:
      return protocol::Network::ResourceTypeEnum::Other;
    case blink::mojom::ResourceType::kCspReport:
      return protocol::Network::ResourceTypeEnum::CSPViolationReport;
    case blink::mojom::ResourceType::kPluginResource:
      return protocol::Network::ResourceTypeEnum::Other;
    default:
      return protocol::Network::ResourceTypeEnum::Other;
  }
}

// static
std::vector<NetworkHandler*> NetworkHandler::ForAgentHost(
    DevToolsAgentHostImpl* host) {
  return host->HandlersByName<NetworkHandler>(Network::Metainfo::domainName);
}

void NetworkHandler::Wire(UberDispatcher* dispatcher) {
  frontend_ = std::make_unique<Network::Frontend>(dispatcher->channel());
  Network::Dispatcher::wire(dispatcher, this);
}

void NetworkHandler::SetRenderer(int render_process_host_id,
                                 RenderFrameHostImpl* frame_host) {
  RenderProcessHost* process_host =
      RenderProcessHost::FromID(render_process_host_id);
  if (process_host) {
    storage_partition_ = process_host->GetStoragePartition();
    browser_context_ = process_host->GetBrowserContext();
  } else {
    storage_partition_ = nullptr;
    browser_context_ = nullptr;
  }
  host_ = frame_host;
  if (background_sync_restorer_)
    background_sync_restorer_->SetStoragePartition(storage_partition_);
}

Response NetworkHandler::Enable(Maybe<int> max_total_size,
                                Maybe<int> max_resource_size,
                                Maybe<int> max_post_data_size) {
  enabled_ = true;
  return Response::FallThrough();
}

Response NetworkHandler::Disable() {
  enabled_ = false;
  url_loader_interceptor_.reset();
  SetNetworkConditions(nullptr);
  extra_headers_.clear();
  ClearAcceptedEncodingsOverride();
  return Response::FallThrough();
}

#if BUILDFLAG(ENABLE_REPORTING)

namespace {

String BuildReportStatus(const net::ReportingReport::Status status) {
  switch (status) {
    case net::ReportingReport::Status::QUEUED:
      return protocol::Network::ReportStatusEnum::Queued;
    case net::ReportingReport::Status::PENDING:
      return protocol::Network::ReportStatusEnum::Pending;
    case net::ReportingReport::Status::DOOMED:
      return protocol::Network::ReportStatusEnum::MarkedForRemoval;
    case net::ReportingReport::Status::SUCCESS:
      return protocol::Network::ReportStatusEnum::Success;
  }
}

std::vector<GURL> ComputeReportingURLs(RenderFrameHostImpl* frame_host) {
  std::vector<GURL> urls;
  frame_host->ForEachRenderFrameHostWithAction(
      [frame_host, &urls](content::RenderFrameHostImpl* rfh) {
        if (rfh != frame_host && (rfh->is_local_root_subframe() ||
                                  &rfh->GetPage() != &frame_host->GetPage())) {
          return content::RenderFrameHost::FrameIterationAction::kSkipChildren;
        }
        urls.push_back(frame_host->GetLastCommittedURL());
        return content::RenderFrameHost::FrameIterationAction::kContinue;
      });
  return urls;
}

}  // namespace

std::unique_ptr<protocol::Network::ReportingApiReport>
NetworkHandler::BuildProtocolReport(const net::ReportingReport& report) {
  if (!host_) {
    return nullptr;
  }
  std::vector<GURL> reporting_filter_urls = ComputeReportingURLs(host_);
  if (base::Contains(reporting_filter_urls, report.url)) {
    return protocol::Network::ReportingApiReport::Create()
        .SetId(report.id.ToString())
        .SetInitiatorUrl(report.url.spec())
        .SetDestination(report.group)
        .SetType(report.type)
        .SetTimestamp(
            (report.queued - base::TimeTicks::UnixEpoch()).InSecondsF())
        .SetDepth(report.depth)
        .SetCompletedAttempts(report.attempts)
        .SetBody(std::make_unique<base::Value::Dict>(report.body.Clone()))
        .SetStatus(BuildReportStatus(report.status))
        .Build();
  }
  return nullptr;
}

void NetworkHandler::OnReportAdded(const net::ReportingReport& report) {
  auto protocol_report = BuildProtocolReport(report);
  if (protocol_report) {
    frontend_->ReportingApiReportAdded(std::move(protocol_report));
  }
}

void NetworkHandler::OnReportUpdated(const net::ReportingReport& report) {
  auto protocol_report = BuildProtocolReport(report);
  if (protocol_report) {
    frontend_->ReportingApiReportUpdated(std::move(protocol_report));
  }
}

std::unique_ptr<protocol::Network::ReportingApiEndpoint>
NetworkHandler::BuildProtocolEndpoint(const net::ReportingEndpoint& endpoint) {
  return protocol::Network::ReportingApiEndpoint::Create()
      .SetUrl(endpoint.info.url.spec())
      .SetGroupName(endpoint.group_key.group_name)
      .Build();
}

void NetworkHandler::OnEndpointsUpdatedForOrigin(
    const std::vector<::net::ReportingEndpoint>& endpoints) {
  if (!host_ || endpoints.empty()) {
    return;
  }
  // Endpoint should have an origin.
  DCHECK(endpoints[0].group_key.origin.has_value());
  url::Origin origin = endpoints[0].group_key.origin.value();
  DCHECK(base::ranges::all_of(endpoints, [&](auto const& endpoint) {
    // Endpoint should have an origin.
    DCHECK(endpoint.group_key.origin.has_value());
    return endpoint.group_key.origin.value() == origin;
  }));
  std::vector<GURL> reporting_filter_urls = ComputeReportingURLs(host_);

  // Only send protocol event if the origin of the updated endpoints matches
  // an origin in the local frame tree.
  if (base::ranges::any_of(reporting_filter_urls, [&](auto const& url) {
        return url::Origin::Create(url) == origin;
      })) {
    auto protocol_endpoints = std::make_unique<
        protocol::Array<protocol::Network::ReportingApiEndpoint>>();
    protocol_endpoints->reserve(endpoints.size());
    for (const auto& endpoint : endpoints) {
      protocol_endpoints->push_back(BuildProtocolEndpoint(endpoint));
    }
    frontend_->ReportingApiEndpointsChangedForOrigin(
        origin.Serialize(), std::move(protocol_endpoints));
  }
}

Response NetworkHandler::EnableReportingApi(const bool enable) {
  if (!storage_partition_ || !host_) {
    return Response::InternalError();
  }

  if (enable) {
    if (!reporting_receiver_.is_bound()) {
      mojo::PendingRemote<network::mojom::ReportingApiObserver> observer;
      reporting_receiver_.Bind(observer.InitWithNewPipeAndPassReceiver());
      storage_partition_->GetNetworkContext()->AddReportingApiObserver(
          std::move(observer));
    }
  } else {
    reporting_receiver_.reset();
  }
  return Response::Success();
}

#else
Response NetworkHandler::EnableReportingApi(const bool enable) {
  return Response::InternalError();
}
#endif  // BUILDFLAG(ENABLE_REPORTING)

Response NetworkHandler::SetCacheDisabled(bool cache_disabled) {
  cache_disabled_ = cache_disabled;
  return Response::FallThrough();
}

Response NetworkHandler::SetAcceptedEncodings(
    std::unique_ptr<Array<Network::ContentEncoding>> encodings) {
  std::set<net::SourceStream::SourceType> accepted_stream_types;
  for (auto encoding : *encodings) {
    auto type = SourceTypeFromProtocol(encoding);
    if (!type)
      return Response::InvalidParams("Unknown encoding type: " + encoding);
    accepted_stream_types.insert(type.value());
  }
  accepted_stream_types_ = std::move(accepted_stream_types);

  return Response::FallThrough();
}

Response NetworkHandler::ClearAcceptedEncodingsOverride() {
  accepted_stream_types_ = std::nullopt;
  return Response::FallThrough();
}

class DevtoolsClearCacheObserver
    : public content::BrowsingDataRemover::Observer {
 public:
  explicit DevtoolsClearCacheObserver(
      content::BrowsingDataRemover* remover,
      std::unique_ptr<NetworkHandler::ClearBrowserCacheCallback> callback)
      : remover_(remover), callback_(std::move(callback)) {
    remover_->AddObserver(this);
  }

  ~DevtoolsClearCacheObserver() override { remover_->RemoveObserver(this); }
  void OnBrowsingDataRemoverDone(uint64_t failed_data_types) override {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    callback_->sendSuccess();
    delete this;
  }

 private:
  raw_ptr<content::BrowsingDataRemover> remover_;
  std::unique_ptr<NetworkHandler::ClearBrowserCacheCallback> callback_;
};

void NetworkHandler::ClearBrowserCache(
    std::unique_ptr<ClearBrowserCacheCallback> callback) {
  if (!browser_context_) {
    callback->sendFailure(Response::InternalError());
    return;
  }
  content::BrowsingDataRemover* remover =
      browser_context_->GetBrowsingDataRemover();
  remover->RemoveAndReply(
      base::Time(), base::Time::Max(),
      content::BrowsingDataRemover::DATA_TYPE_CACHE,
      content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB,
      new DevtoolsClearCacheObserver(remover, std::move(callback)));
}

void NetworkHandler::ClearBrowserCookies(
    std::unique_ptr<ClearBrowserCookiesCallback> callback) {
  if (!storage_partition_) {
    callback->sendFailure(Response::InternalError());
    return;
  }

  storage_partition_->GetCookieManagerForBrowserProcess()->DeleteCookies(
      network::mojom::CookieDeletionFilter::New(),
      base::BindOnce([](std::unique_ptr<ClearBrowserCookiesCallback> callback,
                        uint32_t) { callback->sendSuccess(); },
                     std::move(callback)));
}

void NetworkHandler::GetCookies(Maybe<Array<String>> protocol_urls,
                                std::unique_ptr<GetCookiesCallback> callback) {
  if (!host_ || !storage_partition_) {
    callback->sendFailure(Response::InternalError());
    return;
  }
  std::vector<GURL> urls = ComputeCookieURLs(host_, protocol_urls);
  bool is_webui = host_ && host_->web_ui();

  urls.erase(std::remove_if(urls.begin(), urls.end(),
                            [=, this](const GURL& url) {
                              return !client_->MayAttachToURL(url, is_webui);
                            }),
             urls.end());

  CookieRetrieverNetworkService::Retrieve(
      storage_partition_->GetCookieManagerForBrowserProcess(), urls,
      host_->GetNetworkIsolationKey(), host_->ComputeSiteForCookies(),
      std::move(callback));
}

void NetworkHandler::GetAllCookies(
    std::unique_ptr<GetAllCookiesCallback> callback) {
  if (!storage_partition_) {
    callback->sendFailure(Response::InternalError());
    return;
  }
  storage_partition_->GetCookieManagerForBrowserProcess()->GetAllCookies(
      base::BindOnce(&NetworkHandler::GotAllCookies, weak_factory_.GetWeakPtr(),
                     std::move(callback)));
}

void NetworkHandler::GotAllCookies(
    std::unique_ptr<GetAllCookiesCallback> callback,
    const std::vector<net::CanonicalCookie>& cookies) {
  bool is_webui = host_ && host_->web_ui();
  std::vector<net::CanonicalCookie> filtered_cookies;
  for (const auto& cookie : cookies) {
    if (client_->MayAttachToURL(
            GURL(base::StrCat({url::kHttpsScheme, url::kStandardSchemeSeparator,
                               cookie.DomainWithoutDot()})),
            is_webui) &&
        client_->MayAttachToURL(
            GURL(base::StrCat({url::kHttpScheme, url::kStandardSchemeSeparator,
                               cookie.DomainWithoutDot()})),
            is_webui)) {
      filtered_cookies.emplace_back(std::move(cookie));
    }
  }
  callback->sendSuccess(NetworkHandler::BuildCookieArray(filtered_cookies));
}

void NetworkHandler::SetCookie(const std::string& name,
                               const std::string& value,
                               Maybe<std::string> url,
                               Maybe<std::string> domain,
                               Maybe<std::string> path,
                               Maybe<bool> secure,
                               Maybe<bool> http_only,
                               Maybe<std::string> same_site,
                               Maybe<double> expires,
                               Maybe<std::string> priority,
                               Maybe<bool> same_party,
                               Maybe<std::string> source_scheme,
                               Maybe<int> source_port,
                               Maybe<Network::CookiePartitionKey> partition_key,
                               std::unique_ptr<SetCookieCallback> callback) {
  if (!storage_partition_) {
    callback->sendFailure(Response::InternalError());
    return;
  }

  auto cookie_or_error = MakeCookieFromProtocolValues(
      name, value, url.value_or(""), domain.value_or(""), path.value_or(""),
      secure.value_or(false), http_only.value_or(false), same_site.value_or(""),
      expires.value_or(-1), priority.value_or(""), source_scheme, source_port,
      partition_key);

  if (absl::holds_alternative<Response>(cookie_or_error)) {
    callback->sendFailure(absl::get<Response>(std::move(cookie_or_error)));
    return;
  }
  std::unique_ptr<net::CanonicalCookie> cookie =
      absl::get<std::unique_ptr<net::CanonicalCookie>>(
          std::move(cookie_or_error));

  net::CookieOptions options;
  // Permit it to set a SameSite cookie if it wants to.
  options.set_same_site_cookie_context(
      net::CookieOptions::SameSiteCookieContext::MakeInclusive());
  options.set_include_httponly();
  storage_partition_->GetCookieManagerForBrowserProcess()->SetCanonicalCookie(
      *cookie, net::cookie_util::SimulatedCookieSource(*cookie, "https"),
      options,
      base::BindOnce(net::cookie_util::IsCookieAccessResultInclude)
          .Then(base::BindOnce(&SetCookieCallback::sendSuccess,
                               std::move(callback))));
}

// static
void NetworkHandler::SetCookies(
    StoragePartition* storage_partition,
    std::unique_ptr<protocol::Array<Network::CookieParam>> cookies,
    base::OnceCallback<void(bool)> callback) {
  std::vector<std::unique_ptr<net::CanonicalCookie>> net_cookies;
  for (const std::unique_ptr<Network::CookieParam>& cookie : *cookies) {
    // We need to pass Maybe<> to the function below, but we can't directly
    // get to the `cookie`'s Maybe<> so instead we recreate them.
    const Maybe<std::string> source_scheme =
        cookie->HasSourceScheme()
            ? Maybe<std::string>(cookie->GetSourceScheme(""))
            : Maybe<std::string>();
    const Maybe<int> source_port = cookie->HasSourcePort()
                                       ? Maybe<int>(cookie->GetSourcePort(0))
                                       : Maybe<int>();

    Maybe<Network::CookiePartitionKey> partition_key;
    if (cookie->HasPartitionKey()) {
      protocol::Network::CookiePartitionKey* key =
          cookie->GetPartitionKey(nullptr);
      if (key) {
        std::string site = key->GetTopLevelSite();
        if (!site.empty()) {
          partition_key =
              BuildCookiePartitionKey(site, key->GetHasCrossSiteAncestor());
        }
      }
    }

    auto net_cookie_or_error = MakeCookieFromProtocolValues(
        cookie->GetName(), cookie->GetValue(), cookie->GetUrl(""),
        cookie->GetDomain(""), cookie->GetPath(""), cookie->GetSecure(false),
        cookie->GetHttpOnly(false), cookie->GetSameSite(""),
        cookie->GetExpires(-1), cookie->GetPriority(""), source_scheme,
        source_port, partition_key);
    if (absl::holds_alternative<Response>(net_cookie_or_error)) {
      // TODO: Investiage whether we can report the error as a protocol error
      // (this might be a breaking CDP change).
      std::move(callback).Run(false);
      return;
    }
    net_cookies.push_back(absl::get<std::unique_ptr<net::CanonicalCookie>>(
        std::move(net_cookie_or_error)));
  }

  base::RepeatingClosure barrier_closure = base::BarrierClosure(
      net_cookies.size(), base::BindOnce(std::move(callback), true));

  auto* cookie_manager = storage_partition->GetCookieManagerForBrowserProcess();
  net::CookieOptions options;
  options.set_include_httponly();
  // Permit it to set a SameSite cookie if it wants to.
  options.set_same_site_cookie_context(
      net::CookieOptions::SameSiteCookieContext::MakeInclusive());
  for (const auto& cookie : net_cookies) {
    cookie_manager->SetCanonicalCookie(
        *cookie, net::cookie_util::SimulatedCookieSource(*cookie, "https"),
        options,
        base::BindOnce([](base::RepeatingClosure callback,
                          net::CookieAccessResult) { callback.Run(); },
                       barrier_closure));
  }
}

void NetworkHandler::SetCookies(
    std::unique_ptr<protocol::Array<Network::CookieParam>> cookies,
    std::unique_ptr<SetCookiesCallback> callback) {
  if (!storage_partition_) {
    callback->sendFailure(Response::InternalError());
    return;
  }

  NetworkHandler::SetCookies(
      storage_partition_, std::move(cookies),
      base::BindOnce(
          [](std::unique_ptr<SetCookiesCallback> callback, bool success) {
            if (success) {
              callback->sendSuccess();
            } else {
              callback->sendFailure(
                  Response::InvalidParams(kInvalidCookieFields));
            }
          },
          std::move(callback)));
}

void NetworkHandler::DeleteCookies(
    const std::string& name,
    Maybe<std::string> url_spec,
    Maybe<std::string> domain,
    Maybe<std::string> path,
    Maybe<Network::CookiePartitionKey> partition_key,
    std::unique_ptr<DeleteCookiesCallback> callback) {
  if (!storage_partition_) {
    callback->sendFailure(Response::InternalError());
    return;
  }
  if (!url_spec.has_value() && !domain.has_value()) {
    callback->sendFailure(Response::InvalidParams(
        "At least one of the url and domain needs to be specified"));
  }

  std::string normalized_domain = domain.value_or("");
  if (normalized_domain.empty()) {
    GURL url(url_spec.value_or(""));
    if (!url.SchemeIsHTTPOrHTTPS()) {
      callback->sendFailure(Response::InvalidParams(
          "An http or https url URL must be specified"));
      return;
    }
    normalized_domain = url.host();
  }

  auto* cookie_manager =
      storage_partition_->GetCookieManagerForBrowserProcess();

  cookie_manager->GetAllCookies(
      base::BindOnce(&DeleteFilteredCookies, base::Unretained(cookie_manager),
                     name, normalized_domain, path.value_or(""),
                     std::move(partition_key), std::move(callback)));
}

Response NetworkHandler::SetExtraHTTPHeaders(
    std::unique_ptr<protocol::Network::Headers> headers) {
  std::vector<std::pair<std::string, std::string>> new_headers;
  for (const auto entry : *headers) {
    if (!entry.second.is_string())
      return Response::InvalidParams("Invalid header value, string expected");
    if (!net::HttpUtil::IsValidHeaderName(entry.first))
      return Response::InvalidParams("Invalid header name");
    const std::string& value = entry.second.GetString();
    if (!net::HttpUtil::IsValidHeaderValue(value))
      return Response::InvalidParams("Invalid header value");
    new_headers.emplace_back(entry.first, value);
  }
  extra_headers_.swap(new_headers);
  return Response::FallThrough();
}

Response NetworkHandler::CanEmulateNetworkConditions(bool* result) {
  *result = true;
  return Response::Success();
}

Response NetworkHandler::EmulateNetworkConditions(
    bool offline,
    double latency,
    double download_throughput,
    double upload_throughput,
    Maybe<protocol::Network::ConnectionType>,
    Maybe<double> packet_loss,
    Maybe<int> packet_queue_length,
    Maybe<bool> packet_reordering) {
  network::mojom::NetworkConditionsPtr network_conditions;
  bool throttling_enabled = offline || latency > 0 || download_throughput > 0 ||
                            upload_throughput > 0;
  if (throttling_enabled) {
    network_conditions = network::mojom::NetworkConditions::New();
    network_conditions->offline = offline;
    network_conditions->latency = base::Milliseconds(latency);
    network_conditions->download_throughput = download_throughput;
    network_conditions->upload_throughput = upload_throughput;
    network_conditions->packet_loss = packet_loss.value_or(0.);
    network_conditions->packet_queue_length = packet_queue_length.value_or(0);
    network_conditions->packet_reordering = packet_reordering.value_or(false);
  }
  SetNetworkConditions(std::move(network_conditions));
  return Response::FallThrough();
}

Response NetworkHandler::SetBypassServiceWorker(bool bypass) {
  bypass_service_worker_ = bypass;
  return Response::FallThrough();
}

namespace {

std::unique_ptr<protocol::Network::SecurityDetails> BuildSecurityDetails(
    const net::SSLInfo& ssl_info) {
  // This function should be kept in sync with the corresponding function in
  // inspector_network_agent.cc in //third_party/blink.
  if (!ssl_info.cert)
    return nullptr;
  auto signed_certificate_timestamp_list =
      std::make_unique<protocol::Array<Network::SignedCertificateTimestamp>>();
  for (auto const& sct : ssl_info.signed_certificate_timestamps) {
    std::unique_ptr<protocol::Network::SignedCertificateTimestamp>
        signed_certificate_timestamp =
            Network::SignedCertificateTimestamp::Create()
                .SetStatus(net::ct::StatusToString(sct.status))
                .SetOrigin(net::ct::OriginToString(sct.sct->origin))
                .SetLogDescription(sct.sct->log_description)
                .SetLogId(base::HexEncode(sct.sct->log_id))
                .SetTimestamp((sct.sct->timestamp - base::Time::UnixEpoch())
                                  .InMillisecondsF())
                .SetHashAlgorithm(net::ct::HashAlgorithmToString(
                    sct.sct->signature.hash_algorithm))
                .SetSignatureAlgorithm(net::ct::SignatureAlgorithmToString(
                    sct.sct->signature.signature_algorithm))
                .SetSignatureData(
                    base::HexEncode(sct.sct->signature.signature_data))
                .Build();
    signed_certificate_timestamp_list->emplace_back(
        std::move(signed_certificate_timestamp));
  }
  std::vector<std::string> san_dns;
  std::vector<std::string> san_ip;
  ssl_info.cert->GetSubjectAltName(&san_dns, &san_ip);
  auto san_list = std::make_unique<protocol::Array<String>>(std::move(san_dns));
  for (const std::string& san : san_ip) {
    san_list->emplace_back(net::IPAddress(base::as_byte_span(san)).ToString());
  }

  const char* protocol = "";
  const char* key_exchange = "";
  const char* cipher = "";
  const char* mac = nullptr;

  int ssl_version =
      net::SSLConnectionStatusToVersion(ssl_info.connection_status);

  if (ssl_info.connection_status) {
    net::SSLVersionToString(&protocol, ssl_version);

    bool is_aead;
    bool is_tls13;
    uint16_t cipher_suite =
        net::SSLConnectionStatusToCipherSuite(ssl_info.connection_status);
    net::SSLCipherSuiteToStrings(&key_exchange, &cipher, &mac, &is_aead,
                                 &is_tls13, cipher_suite);
    if (key_exchange == nullptr) {
      DCHECK(is_tls13);
      key_exchange = "";
    }
  }

  std::unique_ptr<protocol::Network::SecurityDetails> security_details =
      protocol::Network::SecurityDetails::Create()
          .SetProtocol(protocol)
          .SetKeyExchange(key_exchange)
          .SetCipher(cipher)
          .SetSubjectName(ssl_info.cert->subject().common_name)
          .SetSanList(std::move(san_list))
          .SetIssuer(ssl_info.cert->issuer().common_name)
          .SetValidFrom(ssl_info.cert->valid_start().InSecondsFSinceUnixEpoch())
          .SetValidTo(ssl_info.cert->valid_expiry().InSecondsFSinceUnixEpoch())
          .SetCertificateId(0)  // Keep this in protocol for compatibility.
          .SetSignedCertificateTimestampList(
              std::move(signed_certificate_timestamp_list))
          .SetCertificateTransparencyCompliance(
              SerializeCTPolicyCompliance(ssl_info.ct_policy_compliance))
          .SetEncryptedClientHello(ssl_info.encrypted_client_hello)
          .Build();

  if (ssl_info.key_exchange_group != 0) {
    const char* key_exchange_group =
        SSL_get_curve_name(ssl_info.key_exchange_group);
    if (key_exchange_group)
      security_details->SetKeyExchangeGroup(key_exchange_group);
  }
  if (mac)
    security_details->SetMac(mac);
  if (ssl_info.peer_signature_algorithm != 0) {
    security_details->SetServerSignatureAlgorithm(
        ssl_info.peer_signature_algorithm);
  }

  return security_details;
}

std::unique_ptr<base::Value::Dict> BuildResponseHeaders(
    const net::HttpResponseHeaders* headers) {
  auto headers_dict = std::make_unique<base::Value::Dict>();
  if (!headers)
    return headers_dict;
  size_t iterator = 0;
  std::string name;
  std::string value;
  while (headers->EnumerateHeaderLines(&iterator, &name, &value)) {
    base::Value* header_value = headers_dict->Find(name);
    if (header_value)
      *header_value = base::Value(header_value->GetString() + '\n' + value);
    else
      headers_dict->Set(name, value);
  }
  return headers_dict;
}

std::unique_ptr<base::Value::Dict> BuildRequestHeaders(
    const net::HttpRequestHeaders& headers,
    const GURL& referrer) {
  auto headers_dict = std::make_unique<base::Value::Dict>();
  for (net::HttpRequestHeaders::Iterator it(headers); it.GetNext();)
    headers_dict->Set(it.name(), it.value());

  // This is normally added down the stack, so we have to fake it here.
  if (!referrer.is_empty())
    headers_dict->Set(net::HttpRequestHeaders::kReferer, referrer.spec());

  return headers_dict;
}

String BuildServiceWorkerResponseSource(
    const network::mojom::URLResponseHeadDevToolsInfo& info) {
  switch (info.service_worker_response_source) {
    case network::mojom::FetchResponseSource::kCacheStorage:
      return protocol::Network::ServiceWorkerResponseSourceEnum::CacheStorage;
    case network::mojom::FetchResponseSource::kHttpCache:
      return protocol::Network::ServiceWorkerResponseSourceEnum::HttpCache;
    case network::mojom::FetchResponseSource::kNetwork:
      return protocol::Network::ServiceWorkerResponseSourceEnum::Network;
    case network::mojom::FetchResponseSource::kUnspecified:
      return protocol::Network::ServiceWorkerResponseSourceEnum::FallbackCode;
  }
}

String BuildServiceWorkerRouterSourceType(
    const network::mojom::ServiceWorkerRouterSourceType& type) {
  switch (type) {
    case network::mojom::ServiceWorkerRouterSourceType::kNetwork:
      return protocol::Network::ServiceWorkerRouterSourceEnum::Network;
    case network::mojom::ServiceWorkerRouterSourceType::kRace:
      return protocol::Network::ServiceWorkerRouterSourceEnum::
          RaceNetworkAndFetchHandler;
    case network::mojom::ServiceWorkerRouterSourceType::kFetchEvent:
      return protocol::Network::ServiceWorkerRouterSourceEnum::FetchEvent;
    case network::mojom::ServiceWorkerRouterSourceType::kCache:
      return protocol::Network::ServiceWorkerRouterSourceEnum::Cache;
  }
}

String AlternateProtocolUsageToString(
    net::AlternateProtocolUsage alternate_protocol_usage) {
  switch (alternate_protocol_usage) {
    case net::AlternateProtocolUsage::ALTERNATE_PROTOCOL_USAGE_NO_RACE:
      return protocol::Network::AlternateProtocolUsageEnum::
          AlternativeJobWonWithoutRace;
    case net::AlternateProtocolUsage::ALTERNATE_PROTOCOL_USAGE_WON_RACE:
      return protocol::Network::AlternateProtocolUsageEnum::
          AlternativeJobWonRace;
    case net::AlternateProtocolUsage::
        ALTERNATE_PROTOCOL_USAGE_MAIN_JOB_WON_RACE:
      return protocol::Network::AlternateProtocolUsageEnum::MainJobWonRace;
    case net::AlternateProtocolUsage::ALTERNATE_PROTOCOL_USAGE_MAPPING_MISSING:
      return protocol::Network::AlternateProtocolUsageEnum::MappingMissing;
    case net::AlternateProtocolUsage::ALTERNATE_PROTOCOL_USAGE_BROKEN:
      return protocol::Network::AlternateProtocolUsageEnum::Broken;
    case net::AlternateProtocolUsage::
        ALTERNATE_PROTOCOL_USAGE_DNS_ALPN_H3_JOB_WON_WITHOUT_RACE:
      return protocol::Network::AlternateProtocolUsageEnum::
          DnsAlpnH3JobWonWithoutRace;
    case net::AlternateProtocolUsage::
        ALTERNATE_PROTOCOL_USAGE_DNS_ALPN_H3_JOB_WON_RACE:
      return protocol::Network::AlternateProtocolUsageEnum::DnsAlpnH3JobWonRace;
    case net::AlternateProtocolUsage::
        ALTERNATE_PROTOCOL_USAGE_UNSPECIFIED_REASON:
      return protocol::Network::AlternateProtocolUsageEnum::UnspecifiedReason;
    case net::AlternateProtocolUsage::ALTERNATE_PROTOCOL_USAGE_MAX:
      return protocol::Network::AlternateProtocolUsageEnum::UnspecifiedReason;
  }
  return protocol::Network::AlternateProtocolUsageEnum::UnspecifiedReason;
}

std::unique_ptr<Network::Response> BuildResponse(
    const GURL& url,
    const network::mojom::URLResponseHeadDevToolsInfo& info) {
  int status = 0;
  std::string status_text;
  if (info.headers) {
    status = info.headers->response_code();
    status_text = info.headers->GetStatusText();
  } else if (url.SchemeIs(url::kDataScheme)) {
    status = net::HTTP_OK;
    status_text = "OK";
  }

  std::string url_fragment;
  auto response =
      Network::Response::Create()
          .SetUrl(NetworkHandler::ExtractFragment(url, &url_fragment))
          .SetStatus(status)
          .SetStatusText(status_text)
          .SetHeaders(BuildResponseHeaders(info.headers.get()))
          .SetMimeType(info.mime_type)
          .SetCharset(info.charset)
          .SetConnectionReused(info.load_timing.socket_reused)
          .SetConnectionId(info.load_timing.socket_log_id)
          .SetSecurityState(securityState(url, info.cert_status))
          .SetEncodedDataLength(info.encoded_data_length)
          .SetTiming(GetTiming(info.load_timing))
          .SetFromDiskCache(!info.load_timing.request_start_time.is_null() &&
                            info.response_time <
                                info.load_timing.request_start_time)
          .Build();
  response->SetFromServiceWorker(info.was_fetched_via_service_worker);
  if (info.was_fetched_via_service_worker) {
    response->SetServiceWorkerResponseSource(
        BuildServiceWorkerResponseSource(info));
  }
  response->SetFromPrefetchCache(info.was_in_prefetch_cache);
  if (!info.response_time.is_null()) {
    response->SetResponseTime(
        info.response_time.InMillisecondsFSinceUnixEpochIgnoringNull());
  }
  if (!info.cache_storage_cache_name.empty()) {
    response->SetCacheStorageCacheName(info.cache_storage_cache_name);
  }
  if (!info.service_worker_router_info.is_null()) {
    auto service_worker_router_info =
        protocol::Network::ServiceWorkerRouterInfo::Create().Build();
    if (info.service_worker_router_info->rule_id_matched) {
      service_worker_router_info->SetRuleIdMatched(
          *info.service_worker_router_info->rule_id_matched);
    }

    if (info.service_worker_router_info->matched_source_type) {
      service_worker_router_info->SetMatchedSourceType(
          BuildServiceWorkerRouterSourceType(
              *info.service_worker_router_info->matched_source_type));
    }

    if (info.service_worker_router_info->actual_source_type) {
      service_worker_router_info->SetActualSourceType(
          BuildServiceWorkerRouterSourceType(
              *info.service_worker_router_info->actual_source_type));
    }

    response->SetServiceWorkerRouterInfo(std::move(service_worker_router_info));
  }

  response->SetProtocol(GetProtocol(url, info));
  if (info.alternate_protocol_usage !=
      net::AlternateProtocolUsage::
          ALTERNATE_PROTOCOL_USAGE_UNSPECIFIED_REASON) {
    response->SetAlternateProtocolUsage(
        AlternateProtocolUsageToString(info.alternate_protocol_usage));
  }
  response->SetRemoteIPAddress(
      net::HostPortPair::FromIPEndPoint(info.remote_endpoint).HostForURL());
  response->SetRemotePort(info.remote_endpoint.port());
  if (info.ssl_info.has_value())
    response->SetSecurityDetails(BuildSecurityDetails(*info.ssl_info));

  return response;
}

std::unique_ptr<Network::Response> BuildRedirectResponse(
    std::optional<std::pair<const GURL&,
                            const network::mojom::URLResponseHeadDevToolsInfo&>>
        redirect_info,
    bool& redirect_emitted_extra_info) {
  std::unique_ptr<Network::Response> redirect_response;
  if (redirect_info) {
    const auto& [previous_url, head] = *redirect_info;
    redirect_emitted_extra_info = head.emitted_extra_info;
    redirect_response = BuildResponse(previous_url, head);
  }
  return redirect_response;
}

String blockedReason(blink::ResourceRequestBlockedReason reason) {
  switch (reason) {
    case blink::ResourceRequestBlockedReason::kCSP:
      return protocol::Network::BlockedReasonEnum::Csp;
    case blink::ResourceRequestBlockedReason::kMixedContent:
      return protocol::Network::BlockedReasonEnum::MixedContent;
    case blink::ResourceRequestBlockedReason::kOrigin:
      return protocol::Network::BlockedReasonEnum::Origin;
    case blink::ResourceRequestBlockedReason::kInspector:
      return protocol::Network::BlockedReasonEnum::Inspector;
    case blink::ResourceRequestBlockedReason::kSubresourceFilter:
      return protocol::Network::BlockedReasonEnum::SubresourceFilter;
    case blink::ResourceRequestBlockedReason::kContentType:
      return protocol::Network::BlockedReasonEnum::ContentType;
    case blink::ResourceRequestBlockedReason::kOther:
      return protocol::Network::BlockedReasonEnum::Other;
    case blink::ResourceRequestBlockedReason::kCoepFrameResourceNeedsCoepHeader:
      return protocol::Network::BlockedReasonEnum::
          CoepFrameResourceNeedsCoepHeader;
    case blink::ResourceRequestBlockedReason::
        kCoopSandboxedIFrameCannotNavigateToCoopPage:
      return protocol::Network::BlockedReasonEnum::
          CoopSandboxedIframeCannotNavigateToCoopPage;
    case blink::ResourceRequestBlockedReason::kCorpNotSameOrigin:
      return protocol::Network::BlockedReasonEnum::CorpNotSameOrigin;
    case blink::ResourceRequestBlockedReason::
        kCorpNotSameOriginAfterDefaultedToSameOriginByCoep:
      return protocol::Network::BlockedReasonEnum::
          CorpNotSameOriginAfterDefaultedToSameOriginByCoep;
    case blink::ResourceRequestBlockedReason::
        kCorpNotSameOriginAfterDefaultedToSameOriginByDip:
      return protocol::Network::BlockedReasonEnum::
          CorpNotSameOriginAfterDefaultedToSameOriginByDip;
    case blink::ResourceRequestBlockedReason::
        kCorpNotSameOriginAfterDefaultedToSameOriginByCoepAndDip:
      return protocol::Network::BlockedReasonEnum::
          CorpNotSameOriginAfterDefaultedToSameOriginByCoep;
    case blink::ResourceRequestBlockedReason::kCorpNotSameSite:
      return protocol::Network::BlockedReasonEnum::CorpNotSameSite;
    case blink::ResourceRequestBlockedReason::kConversionRequest:
      // This is actually never reached, as the conversion request
      // is marked as successful and no blocking reason is reported.
      NOTREACHED_IN_MIGRATION();
      return protocol::Network::BlockedReasonEnum::Other;
  }
  NOTREACHED_IN_MIGRATION();
  return protocol::Network::BlockedReasonEnum::Other;
}

Maybe<String> GetBlockedReasonFor(
    const network::URLLoaderCompletionStatus& status) {
  if (status.blocked_by_response_reason) {
    switch (*status.blocked_by_response_reason) {
      case network::mojom::BlockedByResponseReason::
          kCoepFrameResourceNeedsCoepHeader:
        return {protocol::Network::BlockedReasonEnum::
                    CoepFrameResourceNeedsCoepHeader};
      case network::mojom::BlockedByResponseReason::
          kCoopSandboxedIFrameCannotNavigateToCoopPage:
        return {protocol::Network::BlockedReasonEnum::
                    CoopSandboxedIframeCannotNavigateToCoopPage};
      case network::mojom::BlockedByResponseReason::
          kCorpNotSameOriginAfterDefaultedToSameOriginByCoep:
        return {protocol::Network::BlockedReasonEnum::
                    CorpNotSameOriginAfterDefaultedToSameOriginByCoep};
      case network::mojom::BlockedByResponseReason::
          kCorpNotSameOriginAfterDefaultedToSameOriginByDip:
        return {protocol::Network::BlockedReasonEnum::
                    CorpNotSameOriginAfterDefaultedToSameOriginByDip};
      case network::mojom::BlockedByResponseReason::
          kCorpNotSameOriginAfterDefaultedToSameOriginByCoepAndDip:
        return {protocol::Network::BlockedReasonEnum::
                    CorpNotSameOriginAfterDefaultedToSameOriginByCoepAndDip};
      case network::mojom::BlockedByResponseReason::kCorpNotSameOrigin:
        return {protocol::Network::BlockedReasonEnum::CorpNotSameOrigin};
      case network::mojom::BlockedByResponseReason::kCorpNotSameSite:
        return {protocol::Network::BlockedReasonEnum::CorpNotSameSite};
    }
    NOTREACHED_IN_MIGRATION();
  }
  if (status.error_code != net::ERR_BLOCKED_BY_CLIENT &&
      status.error_code != net::ERR_BLOCKED_BY_RESPONSE)
    return Maybe<String>();

  if (status.extended_error_code <=
      static_cast<int>(blink::ResourceRequestBlockedReason::kMax)) {
    return blockedReason(static_cast<blink::ResourceRequestBlockedReason>(
        status.extended_error_code));
  }

  // TODO(karandeepb): Embedder would know how to interpret the
  // `status.extended_error_code` in this case. For now just return Other.
  return {protocol::Network::BlockedReasonEnum::Other};
}

String GetTrustTokenOperationType(
    network::mojom::TrustTokenOperationType operation) {
  switch (operation) {
    case network::mojom::TrustTokenOperationType::kIssuance:
      return protocol::Network::TrustTokenOperationTypeEnum::Issuance;
    case network::mojom::TrustTokenOperationType::kRedemption:
      return protocol::Network::TrustTokenOperationTypeEnum::Redemption;
    case network::mojom::TrustTokenOperationType::kSigning:
      return protocol::Network::TrustTokenOperationTypeEnum::Signing;
  }
}

String GetTrustTokenRefreshPolicy(
    network::mojom::TrustTokenRefreshPolicy policy) {
  switch (policy) {
    case network::mojom::TrustTokenRefreshPolicy::kUseCached:
      return protocol::Network::TrustTokenParams::RefreshPolicyEnum::UseCached;
    case network::mojom::TrustTokenRefreshPolicy::kRefresh:
      return protocol::Network::TrustTokenParams::RefreshPolicyEnum::Refresh;
  }
}

std::unique_ptr<protocol::Network::TrustTokenParams> BuildTrustTokenParams(
    const network::mojom::TrustTokenParams& params) {
  auto protocol_params =
      protocol::Network::TrustTokenParams::Create()
          .SetOperation(GetTrustTokenOperationType(params.operation))
          .SetRefreshPolicy(GetTrustTokenRefreshPolicy(params.refresh_policy))
          .Build();

  if (!params.issuers.empty()) {
    auto issuers = std::make_unique<protocol::Array<protocol::String>>();
    for (const auto& issuer : params.issuers) {
      issuers->push_back(issuer.Serialize());
    }
    protocol_params->SetIssuers(std::move(issuers));
  }

  return protocol_params;
}

}  // namespace

void NetworkHandler::PrefetchRequestWillBeSent(
    const std::string& request_id,
    const network::ResourceRequest& request,
    const GURL& initiator_url,
    Maybe<std::string> frame_token,
    base::TimeTicks timestamp,
    std::optional<std::pair<const GURL&,
                            const network::mojom::URLResponseHeadDevToolsInfo&>>
        redirect_info) {
  if (!enabled_)
    return;

  std::string url = request.url.is_valid() ? request.url.spec() : "";
  double current_ticks = timestamp.since_origin().InSecondsF();
  double current_wall_time = base::Time::Now().InSecondsFSinceUnixEpoch();
  auto initiator =
      Network::Initiator::Create()
          .SetType(Network::Initiator::TypeEnum::Script)
          .SetUrl(initiator_url.is_valid() ? initiator_url.spec() : "")
          .Build();

  bool redirect_emitted_extra_info = false;
  std::unique_ptr<Network::Response> redirect_response =
      BuildRedirectResponse(redirect_info, redirect_emitted_extra_info);

  auto request_info =
      Network::Request::Create()
          .SetUrl(url)
          .SetMethod(request.method)
          .SetHeaders(BuildRequestHeaders(request.headers, request.referrer))
          .SetInitialPriority(resourcePriority(request.priority))
          .SetReferrerPolicy(referrerPolicy(request.referrer_policy))
          .Build();

  frontend_->RequestWillBeSent(
      request_id, request_id, url, std::move(request_info), current_ticks,
      current_wall_time, std::move(initiator), redirect_emitted_extra_info,
      std::move(redirect_response),
      std::string(Network::ResourceTypeEnum::Prefetch), std::move(frame_token),
      request.has_user_gesture);
}

void NetworkHandler::NavigationRequestWillBeSent(
    const NavigationRequest& nav_request,
    base::TimeTicks timestamp) {
  if (!enabled_)
    return;

  const blink::mojom::CommonNavigationParams& common_params =
      nav_request.common_params();
  net::HttpRequestHeaders headers;
  headers.AddHeadersFromString(nav_request.begin_params().headers);

  std::unique_ptr<Network::Response> redirect_response;
  const blink::mojom::CommitNavigationParams& commit_params =
      nav_request.commit_params();
  bool redirect_emitted_extra_info = false;
  if (!commit_params.redirect_response.empty()) {
    const network::mojom::URLResponseHead& head =
        *commit_params.redirect_response.back();
    network::mojom::URLResponseHeadDevToolsInfoPtr head_info =
        network::ExtractDevToolsInfo(head);
    redirect_response =
        BuildResponse(commit_params.redirects.back(), *head_info);
    redirect_emitted_extra_info = head_info->emitted_extra_info;
  }
  std::string url_fragment;
  std::string url_without_fragment =
      ExtractFragment(common_params.url, &url_fragment);
  auto request =
      Network::Request::Create()
          .SetUrl(url_without_fragment)
          .SetMethod(common_params.method)
          .SetHeaders(BuildRequestHeaders(headers, common_params.referrer->url))
          .SetInitialPriority(resourcePriority(net::HIGHEST))
          .SetReferrerPolicy(referrerPolicy(common_params.referrer->policy))
          .Build();
  if (!url_fragment.empty())
    request->SetUrlFragment(url_fragment);

  if (common_params.post_data) {
    request->SetHasPostData(true);
    std::string post_data;
    auto data_entries =
        std::make_unique<protocol::Array<protocol::Network::PostDataEntry>>();
    if (GetPostData(*common_params.post_data, data_entries.get(), &post_data)) {
      if (!post_data.empty())
        request->SetPostData(post_data);
      if (data_entries->size())
        request->SetPostDataEntries(std::move(data_entries));
    }
  }
  // TODO(caseq): report potentially blockable types
  request->SetMixedContentType(Security::MixedContentTypeEnum::None);

  std::unique_ptr<Network::Initiator> initiator;
  const std::optional<base::Value::Dict>& initiator_optional =
      nav_request.begin_params().devtools_initiator;
  if (initiator_optional.has_value())
    crdtp::ConvertProtocolValue(initiator_optional.value(), &initiator);
  if (!initiator) {
    initiator = Network::Initiator::Create()
                    .SetType(Network::Initiator::TypeEnum::Other)
                    .Build();
  }
  std::string id = nav_request.devtools_navigation_token().ToString();
  double current_ticks = timestamp.since_origin().InSecondsF();
  double current_wall_time = base::Time::Now().InSecondsFSinceUnixEpoch();
  std::string frame_token = nav_request.frame_tree_node()
                                ->current_frame_host()
                                ->devtools_frame_token()
                                .ToString();

  const blink::mojom::BeginNavigationParams& begin_params =
      nav_request.begin_params();
  if (begin_params.trust_token_params) {
    request->SetTrustTokenParams(
        BuildTrustTokenParams(*begin_params.trust_token_params));
  }

  if (host_) {
    if (nav_request.frame_tree_node()->IsOutermostMainFrame()) {
      request->SetIsSameSite(true);
    } else {
      request->SetIsSameSite(
          host_->ComputeSiteForCookies().IsFirstParty(common_params.url));
    }
  }
  frontend_->RequestWillBeSent(
      id, id, url_without_fragment, std::move(request), current_ticks,
      current_wall_time, std::move(initiator), redirect_emitted_extra_info,
      std::move(redirect_response),
      std::string(Network::ResourceTypeEnum::Document), std::move(frame_token),
      common_params.has_user_gesture);
}

void NetworkHandler::FencedFrameReportRequestSent(
    const std::string& request_id,
    const network::ResourceRequest& request,
    const std::string& event_data,
    base::TimeTicks timestamp) {
  if (!enabled_) {
    return;
  }

  CHECK(request.url.is_valid());
  double current_ticks = timestamp.since_origin().InSecondsF();
  double current_wall_time = base::Time::Now().InSecondsFSinceUnixEpoch();
  auto initiator = Network::Initiator::Create()
                       .SetType(Network::Initiator::TypeEnum::Other)
                       .SetRequestId(request_id)
                       .Build();

  auto request_info =
      Network::Request::Create()
          .SetUrl(request.url.spec())
          .SetMethod(request.method)
          .SetHeaders(BuildRequestHeaders(request.headers, request.referrer))
          .SetInitialPriority(resourcePriority(request.priority))
          .SetReferrerPolicy(referrerPolicy(request.referrer_policy))
          .Build();

  if (!event_data.empty()) {
    request_info->SetHasPostData(true);
    request_info->SetPostData(event_data);
  }

  frontend_->RequestWillBeSent(
      request_id, request_id, request.url.spec(), std::move(request_info),
      current_ticks, current_wall_time, std::move(initiator),
      /*redirectHasExtraInfo=*/false, std::unique_ptr<Network::Response>(),
      std::string(Network::ResourceTypeEnum::Other),
      Maybe<std::string>() /* frame_id */, request.has_user_gesture);
}

void NetworkHandler::RequestSent(
    const std::string& request_id,
    const std::string& loader_id,
    const net::HttpRequestHeaders& request_headers,
    const network::mojom::URLRequestDevToolsInfo& request_info,
    const char* initiator_type,
    const std::optional<GURL>& initiator_url,
    const std::string& initiator_devtools_request_id,
    base::TimeTicks timestamp) {
  if (!enabled_)
    return;
  std::unique_ptr<Network::Initiator> initiator =
      Network::Initiator::Create().SetType(initiator_type).Build();
  if (initiator_url)
    initiator->SetUrl(initiator_url->spec());
  if (initiator_devtools_request_id.size())
    initiator->SetRequestId(initiator_devtools_request_id);
  std::string url_fragment;
  std::string url_without_fragment =
      ExtractFragment(request_info.url, &url_fragment);
  auto request_object =
      Network::Request::Create()
          .SetUrl(url_without_fragment)
          .SetMethod(request_info.method)
          .SetHeaders(BuildRequestHeaders(request_headers, GURL()))
          .SetInitialPriority(resourcePriority(request_info.priority))
          .SetReferrerPolicy(referrerPolicy(request_info.referrer_policy))
          .Build();
  if (!url_fragment.empty())
    request_object->SetUrlFragment(url_fragment);
  if (request_info.trust_token_params) {
    request_object->SetTrustTokenParams(
        BuildTrustTokenParams(*request_info.trust_token_params));
  }

  std::string resource_type = Network::ResourceTypeEnum::Other;
  if (request_info.resource_type ==
          static_cast<int>(blink::mojom::ResourceType::kWorker) ||
      request_info.resource_type ==
          static_cast<int>(blink::mojom::ResourceType::kSharedWorker) ||
      request_info.resource_type ==
          static_cast<int>(blink::mojom::ResourceType::kServiceWorker)) {
    resource_type = Network::ResourceTypeEnum::Script;
  }

  // TODO(crbug.com/40798984): Populate redirectHasExtraInfo instead of
  // just returning false.
  frontend_->RequestWillBeSent(
      request_id, loader_id, url_without_fragment, std::move(request_object),
      timestamp.since_origin().InSecondsF(),
      base::Time::Now().InSecondsFSinceUnixEpoch(), std::move(initiator),
      /*redirectHasExtraInfo=*/false, std::unique_ptr<Network::Response>(),
      resource_type, Maybe<std::string>() /* frame_id */,
      request_info.has_user_gesture);
}

namespace {
String BuildCorsError(network::mojom::CorsError cors_error) {
  switch (cors_error) {
    case network::mojom::CorsError::kDisallowedByMode:
      return protocol::Network::CorsErrorEnum::DisallowedByMode;

    case network::mojom::CorsError::kInvalidResponse:
      return protocol::Network::CorsErrorEnum::InvalidResponse;

    case network::mojom::CorsError::kWildcardOriginNotAllowed:
      return protocol::Network::CorsErrorEnum::WildcardOriginNotAllowed;

    case network::mojom::CorsError::kMissingAllowOriginHeader:
      return protocol::Network::CorsErrorEnum::MissingAllowOriginHeader;

    case network::mojom::CorsError::kMultipleAllowOriginValues:
      return protocol::Network::CorsErrorEnum::MultipleAllowOriginValues;

    case network::mojom::CorsError::kInvalidAllowOriginValue:
      return protocol::Network::CorsErrorEnum::InvalidAllowOriginValue;

    case network::mojom::CorsError::kAllowOriginMismatch:
      return protocol::Network::CorsErrorEnum::AllowOriginMismatch;

    case network::mojom::CorsError::kInvalidAllowCredentials:
      return protocol::Network::CorsErrorEnum::InvalidAllowCredentials;

    case network::mojom::CorsError::kCorsDisabledScheme:
      return protocol::Network::CorsErrorEnum::CorsDisabledScheme;

    case network::mojom::CorsError::kPreflightInvalidStatus:
      return protocol::Network::CorsErrorEnum::PreflightInvalidStatus;

    case network::mojom::CorsError::kPreflightDisallowedRedirect:
      return protocol::Network::CorsErrorEnum::PreflightDisallowedRedirect;

    case network::mojom::CorsError::kPreflightWildcardOriginNotAllowed:
      return protocol::Network::CorsErrorEnum::
          PreflightWildcardOriginNotAllowed;

    case network::mojom::CorsError::kPreflightMissingAllowOriginHeader:
      return protocol::Network::CorsErrorEnum::
          PreflightMissingAllowOriginHeader;

    case network::mojom::CorsError::kPreflightMultipleAllowOriginValues:
      return protocol::Network::CorsErrorEnum::
          PreflightMultipleAllowOriginValues;

    case network::mojom::CorsError::kPreflightInvalidAllowOriginValue:
      return protocol::Network::CorsErrorEnum::PreflightInvalidAllowOriginValue;

    case network::mojom::CorsError::kPreflightAllowOriginMismatch:
      return protocol::Network::CorsErrorEnum::PreflightAllowOriginMismatch;

    case network::mojom::CorsError::kPreflightInvalidAllowCredentials:
      return protocol::Network::CorsErrorEnum::PreflightInvalidAllowCredentials;

    case network::mojom::CorsError::kPreflightMissingAllowPrivateNetwork:
      return protocol::Network::CorsErrorEnum::
          PreflightMissingAllowPrivateNetwork;

    case network::mojom::CorsError::kPreflightInvalidAllowPrivateNetwork:
      return protocol::Network::CorsErrorEnum::
          PreflightInvalidAllowPrivateNetwork;

    case network::mojom::CorsError::kInvalidAllowMethodsPreflightResponse:
      return protocol::Network::CorsErrorEnum::
          InvalidAllowMethodsPreflightResponse;

    case network::mojom::CorsError::kInvalidAllowHeadersPreflightResponse:
      return protocol::Network::CorsErrorEnum::
          InvalidAllowHeadersPreflightResponse;

    case network::mojom::CorsError::kMethodDisallowedByPreflightResponse:
      return protocol::Network::CorsErrorEnum::
          MethodDisallowedByPreflightResponse;

    case network::mojom::CorsError::kHeaderDisallowedByPreflightResponse:
      return protocol::Network::CorsErrorEnum::
          HeaderDisallowedByPreflightResponse;

    case network::mojom::CorsError::kRedirectContainsCredentials:
      return protocol::Network::CorsErrorEnum::RedirectContainsCredentials;

    case network::mojom::CorsError::kInsecurePrivateNetwork:
      return protocol::Network::CorsErrorEnum::InsecurePrivateNetwork;

    case network::mojom::CorsError::kInvalidPrivateNetworkAccess:
      return protocol::Network::CorsErrorEnum::InvalidPrivateNetworkAccess;

    case network::mojom::CorsError::kUnexpectedPrivateNetworkAccess:
      return protocol::Network::CorsErrorEnum::UnexpectedPrivateNetworkAccess;

    case network::mojom::CorsError::kPreflightMissingPrivateNetworkAccessId:
      return protocol::Network::CorsErrorEnum::
          PreflightMissingPrivateNetworkAccessId;

    case network::mojom::CorsError::kPreflightMissingPrivateNetworkAccessName:
      return protocol::Network::CorsErrorEnum::
          PreflightMissingPrivateNetworkAccessName;

    case network::mojom::CorsError::kPrivateNetworkAccessPermissionUnavailable:
      return protocol::Network::CorsErrorEnum::
          PrivateNetworkAccessPermissionUnavailable;

    case network::mojom::CorsError::kPrivateNetworkAccessPermissionDenied:
      return protocol::Network::CorsErrorEnum::
          PrivateNetworkAccessPermissionDenied;
  }
}
}  // namespace

void NetworkHandler::ResponseReceived(
    const std::string& request_id,
    const std::string& loader_id,
    const GURL& url,
    const char* resource_type,
    const network::mojom::URLResponseHeadDevToolsInfo& head,
    Maybe<std::string> frame_id) {
  if (!enabled_)
    return;
  std::unique_ptr<Network::Response> response(BuildResponse(url, head));
  frontend_->ResponseReceived(
      request_id, loader_id,
      base::TimeTicks::Now().ToInternalValue() /
          static_cast<double>(base::Time::kMicrosecondsPerSecond),
      resource_type, std::move(response), head.emitted_extra_info,
      std::move(frame_id));
}

void NetworkHandler::LoadingComplete(
    const std::string& request_id,
    const char* resource_type,
    const network::URLLoaderCompletionStatus& status) {
  if (!enabled_)
    return;
  if (status.error_code != net::OK) {
    frontend_->LoadingFailed(
        request_id,
        base::TimeTicks::Now().ToInternalValue() /
            static_cast<double>(base::Time::kMicrosecondsPerSecond),
        resource_type, net::ErrorToString(status.error_code),
        status.error_code == net::Error::ERR_ABORTED,
        GetBlockedReasonFor(status),
        status.cors_error_status
            ? BuildCorsErrorStatus(*status.cors_error_status)
            : Maybe<protocol::Network::CorsErrorStatus>());
    return;
  }
  frontend_->LoadingFinished(
      request_id,
      status.completion_time.ToInternalValue() /
          static_cast<double>(base::Time::kMicrosecondsPerSecond),
      status.encoded_data_length);
}

void NetworkHandler::FetchKeepAliveRequestWillBeSent(
    const std::string& request_id,
    const network::ResourceRequest& request,
    const GURL& initiator_url,
    Maybe<std::string> frame_token,
    base::TimeTicks timestamp,
    std::optional<std::pair<const GURL&,
                            const network::mojom::URLResponseHeadDevToolsInfo&>>
        redirect_info) {
  if (!enabled_) {
    return;
  }

  std::string url = request.url.is_valid() ? request.url.spec() : "";
  double current_ticks = timestamp.since_origin().InSecondsF();
  double current_wall_time = base::Time::Now().InSecondsFSinceUnixEpoch();
  auto initiator =
      Network::Initiator::Create()
          .SetType(Network::Initiator::TypeEnum::Script)
          .SetUrl(initiator_url.is_valid() ? initiator_url.spec() : "")
          .Build();

  bool redirect_emitted_extra_info = false;
  std::unique_ptr<Network::Response> redirect_response =
      BuildRedirectResponse(redirect_info, redirect_emitted_extra_info);

  auto request_info =
      Network::Request::Create()
          .SetUrl(url)
          .SetMethod(request.method)
          .SetHeaders(BuildRequestHeaders(request.headers, request.referrer))
          .SetInitialPriority(resourcePriority(request.priority))
          .SetReferrerPolicy(referrerPolicy(request.referrer_policy))
          // A fetch keepalive request is categorized as blockable.
          // https://www.w3.org/TR/mixed-content/#category-blockable
          .SetMixedContentType(Security::MixedContentTypeEnum::Blockable)
          .Build();

  if (request.request_body) {
    request_info->SetHasPostData(true);
    std::string post_data;
    auto data_entries =
        std::make_unique<protocol::Array<protocol::Network::PostDataEntry>>();
    if (GetPostData(*request.request_body, data_entries.get(), &post_data)) {
      if (!post_data.empty()) {
        request_info->SetPostData(post_data);
      }
      if (data_entries->size()) {
        request_info->SetPostDataEntries(std::move(data_entries));
      }
    }
  }

  frontend_->RequestWillBeSent(
      request_id, request_id, url, std::move(request_info), current_ticks,
      current_wall_time, std::move(initiator), redirect_emitted_extra_info,
      std::move(redirect_response),
      std::string(Network::ResourceTypeEnum::Fetch), std::move(frame_token),
      request.has_user_gesture);
}

void NetworkHandler::OnSignedExchangeReceived(
    std::optional<const base::UnguessableToken> devtools_navigation_token,
    const GURL& outer_request_url,
    const network::mojom::URLResponseHead& outer_response,
    const std::optional<SignedExchangeEnvelope>& envelope,
    const scoped_refptr<net::X509Certificate>& certificate,
    const std::optional<net::SSLInfo>& ssl_info,
    const std::vector<SignedExchangeError>& errors) {
  if (!enabled_)
    return;
  network::mojom::URLResponseHeadDevToolsInfoPtr head_info =
      network::ExtractDevToolsInfo(outer_response);
  std::unique_ptr<Network::SignedExchangeInfo> signed_exchange_info =
      Network::SignedExchangeInfo::Create()
          .SetOuterResponse(BuildResponse(outer_request_url, *head_info))
          .Build();

  if (envelope) {
    auto headers_dict = std::make_unique<base::Value::Dict>();
    for (const auto& it : envelope->response_headers())
      headers_dict->Set(it.first, it.second);

    const SignedExchangeSignatureHeaderField::Signature& sig =
        envelope->signature();
    auto signatures =
        std::make_unique<protocol::Array<Network::SignedExchangeSignature>>();
    std::unique_ptr<Network::SignedExchangeSignature> signature =
        Network::SignedExchangeSignature::Create()
            .SetLabel(sig.label)
            .SetSignature(base::HexEncode(sig.sig))
            .SetIntegrity(sig.integrity)
            .SetCertUrl(sig.cert_url.spec())
            .SetValidityUrl(sig.validity_url.url.spec())
            .SetDate(sig.date)
            .SetExpires(sig.expires)
            .Build();
    if (sig.cert_sha256) {
      signature->SetCertSha256(base::HexEncode(sig.cert_sha256->data));
    }
    if (certificate) {
      auto encoded_certificates = std::make_unique<protocol::Array<String>>();
      encoded_certificates->emplace_back(
          base::Base64Encode(net::x509_util::CryptoBufferAsStringPiece(
              certificate->cert_buffer())));
      for (const auto& cert : certificate->intermediate_buffers()) {
        encoded_certificates->emplace_back(base::Base64Encode(
            net::x509_util::CryptoBufferAsStringPiece(cert.get())));
      }
      signature->SetCertificates(std::move(encoded_certificates));
    }
    signatures->emplace_back(std::move(signature));

    signed_exchange_info->SetHeader(
        Network::SignedExchangeHeader::Create()
            .SetRequestUrl(envelope->request_url().url.spec())
            .SetResponseCode(envelope->response_code())
            .SetResponseHeaders(std::move(headers_dict))
            .SetSignatures(std::move(signatures))
            .SetHeaderIntegrity(
                signed_exchange_utils::CreateHeaderIntegrityHashString(
                    envelope->ComputeHeaderIntegrity()))
            .Build());
  }
  if (ssl_info)
    signed_exchange_info->SetSecurityDetails(BuildSecurityDetails(*ssl_info));
  if (errors.size())
    signed_exchange_info->SetErrors(BuildSignedExchangeErrors(errors));

  frontend_->SignedExchangeReceived(
      devtools_navigation_token ? devtools_navigation_token->ToString() : "",
      std::move(signed_exchange_info));
}

DispatchResponse NetworkHandler::SetRequestInterception(
    std::unique_ptr<protocol::Array<protocol::Network::RequestPattern>>
        patterns) {
  if (patterns->empty()) {
    if (url_loader_interceptor_) {
      url_loader_interceptor_.reset();
      update_loader_factories_callback_.Run();
    }
    return Response::Success();
  }

  std::vector<DevToolsURLLoaderInterceptor::Pattern> interceptor_patterns;
  for (const std::unique_ptr<protocol::Network::RequestPattern>& pattern :
       *patterns) {
    base::flat_set<blink::mojom::ResourceType> resource_types;
    std::string resource_type = pattern->GetResourceType("");
    if (!resource_type.empty()) {
      if (!AddInterceptedResourceType(resource_type, &resource_types)) {
        return Response::InvalidParams(base::StringPrintf(
            "Cannot intercept resources of type '%s'", resource_type.c_str()));
      }
    }
    auto interception_stage = pattern->GetInterceptionStage(
        protocol::Network::InterceptionStageEnum::Request);
    auto stage = ToInterceptorStage(interception_stage);
    if (!stage.has_value()) {
      return Response::InvalidParams(base::StringPrintf(
          "Unsupported interception stage '%s'", interception_stage.c_str()));
    }
    interceptor_patterns.emplace_back(pattern->GetUrlPattern("*"),
                                      std::move(resource_types), stage.value());
  }

  if (!host_)
    return Response::InternalError();

  if (!url_loader_interceptor_) {
    url_loader_interceptor_ =
        std::make_unique<DevToolsURLLoaderInterceptor>(base::BindRepeating(
            &NetworkHandler::RequestIntercepted, weak_factory_.GetWeakPtr()));
    url_loader_interceptor_->SetPatterns(interceptor_patterns, true);
    update_loader_factories_callback_.Run();
  } else {
    url_loader_interceptor_->SetPatterns(interceptor_patterns, true);
  }
  return Response::Success();
}

void NetworkHandler::ContinueInterceptedRequest(
    const std::string& interception_id,
    Maybe<std::string> error_reason,
    Maybe<protocol::Binary> raw_response,
    Maybe<std::string> url,
    Maybe<std::string> method,
    Maybe<std::string> post_data,
    Maybe<protocol::Network::Headers> opt_headers,
    Maybe<protocol::Network::AuthChallengeResponse> auth_challenge_response,
    std::unique_ptr<ContinueInterceptedRequestCallback> callback) {
  scoped_refptr<net::HttpResponseHeaders> response_headers;
  scoped_refptr<base::RefCountedMemory> response_body;
  size_t body_offset = 0;

  if (raw_response.has_value()) {
    const protocol::Binary& raw = raw_response.value();

    std::string raw_headers;
    size_t header_size = net::HttpUtil::LocateEndOfHeaders(raw);
    if (header_size == std::string::npos) {
      LOG(WARNING) << "Can't find headers in raw response";
      header_size = 0;
    } else {
      raw_headers = net::HttpUtil::AssembleRawHeaders(std::string_view(
          reinterpret_cast<const char*>(raw.data()), header_size));
    }
    CHECK_LE(header_size, raw.size());
    response_headers =
        base::MakeRefCounted<net::HttpResponseHeaders>(std::move(raw_headers));
    response_body = raw.bytes();
    body_offset = header_size;
  }

  std::optional<net::Error> error;
  if (error_reason.has_value()) {
    bool ok;
    error = NetErrorFromString(error_reason.value(), &ok);
    if (!ok) {
      callback->sendFailure(Response::InvalidParams("Invalid errorReason."));
      return;
    }
  }

  std::unique_ptr<DevToolsURLLoaderInterceptor::Modifications::HeadersVector>
      override_headers;
  if (opt_headers.has_value()) {
    const base::Value::Dict& headers = opt_headers.value();
    override_headers = std::make_unique<
        DevToolsURLLoaderInterceptor::Modifications::HeadersVector>();
    for (const auto entry : headers) {
      std::string value;
      if (!entry.second.is_string()) {
        callback->sendFailure(Response::InvalidParams("Invalid header value"));
        return;
      }
      override_headers->emplace_back(entry.first, entry.second.GetString());
    }
  }
  using AuthChallengeResponse =
      DevToolsURLLoaderInterceptor::AuthChallengeResponse;
  std::unique_ptr<AuthChallengeResponse> override_auth;
  if (auth_challenge_response.has_value()) {
    std::string type = auth_challenge_response->GetResponse();
    if (type == Network::AuthChallengeResponse::ResponseEnum::Default) {
      override_auth = std::make_unique<AuthChallengeResponse>(
          AuthChallengeResponse::kDefault);
    } else if (type ==
               Network::AuthChallengeResponse::ResponseEnum::CancelAuth) {
      override_auth = std::make_unique<AuthChallengeResponse>(
          AuthChallengeResponse::kCancelAuth);
    } else if (type == Network::AuthChallengeResponse::ResponseEnum::
                           ProvideCredentials) {
      override_auth = std::make_unique<AuthChallengeResponse>(
          base::UTF8ToUTF16(auth_challenge_response->GetUsername("")),
          base::UTF8ToUTF16(auth_challenge_response->GetPassword("")));
    } else {
      callback->sendFailure(
          Response::InvalidParams("Unrecognized authChallengeResponse."));
      return;
    }
  }

  Maybe<protocol::Binary> post_data_bytes;
  if (post_data.has_value()) {
    post_data_bytes = protocol::Binary::fromString(post_data.value());
  }

  auto modifications =
      std::make_unique<DevToolsURLLoaderInterceptor::Modifications>(
          std::move(error), std::move(response_headers),
          std::move(response_body), body_offset, std::move(url),
          std::move(method), std::move(post_data_bytes),
          std::move(override_headers), std::move(override_auth));

  if (!url_loader_interceptor_)
    return;

  url_loader_interceptor_->ContinueInterceptedRequest(
      interception_id, std::move(modifications), std::move(callback));
}

void NetworkHandler::GetResponseBodyForInterception(
    const String& interception_id,
    std::unique_ptr<GetResponseBodyForInterceptionCallback> callback) {
  if (!url_loader_interceptor_)
    return;

  url_loader_interceptor_->GetResponseBody(interception_id,
                                           std::move(callback));
}

void NetworkHandler::BodyDataReceived(const String& request_id,
                                      const String& body,
                                      bool is_base64_encoded) {
  received_body_data_[request_id] = {body, is_base64_encoded};
}

void NetworkHandler::GetResponseBody(
    const String& request_id,
    std::unique_ptr<GetResponseBodyCallback> callback) {
  auto it = received_body_data_.find(request_id);
  if (it != received_body_data_.end()) {
    callback->sendSuccess(it->second.first, it->second.second);
  } else {
    callback->fallThrough();
  }
}

void NetworkHandler::TakeResponseBodyForInterceptionAsStream(
    const String& interception_id,
    std::unique_ptr<TakeResponseBodyForInterceptionAsStreamCallback> callback) {
  if (url_loader_interceptor_) {
    url_loader_interceptor_->TakeResponseBodyPipe(
        interception_id,
        base::BindOnce(&NetworkHandler::OnResponseBodyPipeTaken,
                       weak_factory_.GetWeakPtr(), std::move(callback)));
    return;
  }
  callback->sendFailure(Response::ServerError(
      "Network.takeResponseBodyForInterceptionAsStream is only "
      "currently supported with --enable-features=NetworkService"));
}

void NetworkHandler::OnResponseBodyPipeTaken(
    std::unique_ptr<TakeResponseBodyForInterceptionAsStreamCallback> callback,
    Response response,
    mojo::ScopedDataPipeConsumerHandle pipe,
    const std::string& mime_type) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_EQ(response.IsSuccess(), pipe.is_valid());
  if (!response.IsSuccess()) {
    callback->sendFailure(std::move(response));
    return;
  }
  // The pipe stream is owned only by io_context after we return.
  bool is_binary = !DevToolsIOContext::IsTextMimeType(mime_type);
  auto stream =
      DevToolsStreamPipe::Create(io_context_, std::move(pipe), is_binary);
  callback->sendSuccess(stream->handle());
}

// static
std::string NetworkHandler::ExtractFragment(const GURL& url,
                                            std::string* fragment) {
  if (!url.has_ref()) {
    *fragment = std::string();
    return url.spec();
  }
  *fragment = "#" + url.ref();
  GURL::Replacements replacements;
  replacements.ClearRef();
  return url.ReplaceComponents(replacements).spec();
}

// static
std::unique_ptr<Network::Request>
NetworkHandler::CreateRequestFromResourceRequest(
    const network::ResourceRequest& request,
    const std::string& cookie_line,
    std::vector<base::expected<std::vector<uint8_t>, std::string>>
        request_bodies) {
  std::unique_ptr<base::Value::Dict> headers_dict =
      BuildRequestHeaders(request.headers, request.referrer);
  if (!cookie_line.empty())
    headers_dict->Set(net::HttpRequestHeaders::kCookie, cookie_line);

  std::string url_fragment;
  std::unique_ptr<protocol::Network::Request> request_object =
      Network::Request::Create()
          .SetUrl(ExtractFragment(request.url, &url_fragment))
          .SetMethod(request.method)
          .SetHeaders(std::move(headers_dict))
          .SetInitialPriority(resourcePriority(request.priority))
          .SetReferrerPolicy(referrerPolicy(request.referrer_policy))
          .Build();
  if (!url_fragment.empty())
    request_object->SetUrlFragment(url_fragment);
  if (!request_bodies.empty()) {
    std::string post_data;
    auto data_entries =
        std::make_unique<protocol::Array<protocol::Network::PostDataEntry>>();

    for (auto& body : request_bodies) {
      // TODO(caseq): post_data is deprecated, remove.
      auto entry = protocol::Network::PostDataEntry::Create().Build();
      if (body.has_value()) {
        post_data.append(reinterpret_cast<const char*>(body->data()),
                         body->size());
        entry->SetBytes(protocol::Binary::fromVector(*std::move(body)));
      }
      data_entries->push_back(std::move(entry));
    }
    if (!post_data.empty()) {
      request_object->SetPostData(std::move(post_data));
    }
    request_object->SetPostDataEntries(std::move(data_entries));
    request_object->SetHasPostData(true);
  }
  return request_object;
}

bool NetworkHandler::MaybeCreateProxyForInterception(
    int process_id,
    StoragePartition* storage_partition,
    const base::UnguessableToken& frame_token,
    bool is_navigation,
    bool is_download,
    network::mojom::URLLoaderFactoryOverride* intercepting_factory) {
  return url_loader_interceptor_ &&
         url_loader_interceptor_->CreateProxyForInterception(
             process_id, storage_partition, frame_token, is_navigation,
             is_download, intercepting_factory);
}

void NetworkHandler::ApplyOverrides(
    net::HttpRequestHeaders* headers,
    bool* skip_service_worker,
    bool* disable_cache,
    std::optional<std::vector<net::SourceStream::SourceType>>*
        accepted_stream_types) {
  for (auto& entry : extra_headers_)
    headers->SetHeader(entry.first, entry.second);
  *skip_service_worker |= bypass_service_worker_;
  *disable_cache |= cache_disabled_;
  if (!accepted_stream_types_)
    return;
  if (!*accepted_stream_types)
    *accepted_stream_types = std::vector<net::SourceStream::SourceType>();
  (*accepted_stream_types)
      ->insert((*accepted_stream_types)->end(), accepted_stream_types_->begin(),
               accepted_stream_types_->end());
}

void NetworkHandler::RequestIntercepted(
    std::unique_ptr<InterceptedRequestInfo> info) {
  protocol::Maybe<protocol::Network::ErrorReason> error_reason;
  if (info->response_error_code < 0)
    error_reason = NetErrorToString(info->response_error_code);

  Maybe<int> status_code;
  Maybe<protocol::Network::Headers> response_headers;
  if (info->response_headers) {
    status_code = info->response_headers->response_code();
    response_headers = BuildResponseHeaders(info->response_headers.get());
  }

  std::unique_ptr<protocol::Network::AuthChallenge> auth_challenge;
  if (info->auth_challenge) {
    auth_challenge =
        protocol::Network::AuthChallenge::Create()
            .SetSource(info->auth_challenge->is_proxy
                           ? Network::AuthChallenge::SourceEnum::Proxy
                           : Network::AuthChallenge::SourceEnum::Server)
            .SetOrigin(info->auth_challenge->challenger.Serialize())
            .SetScheme(info->auth_challenge->scheme)
            .SetRealm(info->auth_challenge->realm)
            .Build();
  }

  frontend_->RequestIntercepted(
      info->interception_id, std::move(info->network_request),
      info->frame_id.ToString(), ResourceTypeToString(info->resource_type),
      info->is_navigation, std::move(info->is_download),
      std::move(info->redirect_url), std::move(auth_challenge),
      std::move(error_reason), std::move(status_code),
      std::move(response_headers), std::move(info->renderer_request_id));
}

void NetworkHandler::SetNetworkConditions(
    network::mojom::NetworkConditionsPtr conditions) {
  if (!storage_partition_)
    return;
  network::mojom::NetworkContext* context =
      storage_partition_->GetNetworkContext();
  bool offline = conditions ? conditions->offline : false;

  if (!devtools_token_.is_empty())
    context->SetNetworkConditions(devtools_token_, std::move(conditions));

  if (offline == !!background_sync_restorer_)
    return;
  background_sync_restorer_.reset(
      offline ? new BackgroundSyncRestorer(host_id_, storage_partition_)
              : nullptr);
}

namespace {
protocol::Network::CrossOriginOpenerPolicyValue
makeCrossOriginOpenerPolicyValue(
    network::mojom::CrossOriginOpenerPolicyValue value) {
  switch (value) {
    case network::mojom::CrossOriginOpenerPolicyValue::kSameOrigin:
      return protocol::Network::CrossOriginOpenerPolicyValueEnum::SameOrigin;
    case network::mojom::CrossOriginOpenerPolicyValue::kSameOriginAllowPopups:
      return protocol::Network::CrossOriginOpenerPolicyValueEnum::
          SameOriginAllowPopups;
    case network::mojom::CrossOriginOpenerPolicyValue::kUnsafeNone:
      return protocol::Network::CrossOriginOpenerPolicyValueEnum::UnsafeNone;
    case network::mojom::CrossOriginOpenerPolicyValue::kSameOriginPlusCoep:
      return protocol::Network::CrossOriginOpenerPolicyValueEnum::
          SameOriginPlusCoep;
    case network::mojom::CrossOriginOpenerPolicyValue::kRestrictProperties:
      return protocol::Network::CrossOriginOpenerPolicyValueEnum::
          RestrictProperties;
    case network::mojom::CrossOriginOpenerPolicyValue::
        kRestrictPropertiesPlusCoep:
      return protocol::Network::CrossOriginOpenerPolicyValueEnum::
          RestrictPropertiesPlusCoep;
    case network::mojom::CrossOriginOpenerPolicyValue::kNoopenerAllowPopups:
      return protocol::Network::CrossOriginOpenerPolicyValueEnum::
          NoopenerAllowPopups;
  }
}
protocol::Network::CrossOriginEmbedderPolicyValue
makeCrossOriginEmbedderPolicyValue(
    network::mojom::CrossOriginEmbedderPolicyValue value) {
  switch (value) {
    case network::mojom::CrossOriginEmbedderPolicyValue::kNone:
      return protocol::Network::CrossOriginEmbedderPolicyValueEnum::None;
    case network::mojom::CrossOriginEmbedderPolicyValue::kCredentialless:
      return protocol::Network::CrossOriginEmbedderPolicyValueEnum::
          Credentialless;
    case network::mojom::CrossOriginEmbedderPolicyValue::kRequireCorp:
      return protocol::Network::CrossOriginEmbedderPolicyValueEnum::RequireCorp;
  }
}
protocol::Network::ContentSecurityPolicySource makeContentSecurityPolicySource(
    network::mojom::ContentSecurityPolicySource source) {
  switch (source) {
    case network::mojom::ContentSecurityPolicySource::kHTTP:
      return protocol::Network::ContentSecurityPolicySourceEnum::HTTP;
    case network::mojom::ContentSecurityPolicySource::kMeta:
      return protocol::Network::ContentSecurityPolicySourceEnum::Meta;
  }
}
std::unique_ptr<protocol::Network::CrossOriginOpenerPolicyStatus>
makeCrossOriginOpenerPolicyStatus(
    const network::CrossOriginOpenerPolicy& coop) {
  auto protocol_coop =
      protocol::Network::CrossOriginOpenerPolicyStatus::Create()
          .SetValue(makeCrossOriginOpenerPolicyValue(coop.value))
          .SetReportOnlyValue(
              makeCrossOriginOpenerPolicyValue(coop.report_only_value))
          .Build();

  if (coop.reporting_endpoint)
    protocol_coop->SetReportingEndpoint(*coop.reporting_endpoint);
  if (coop.report_only_reporting_endpoint) {
    protocol_coop->SetReportOnlyReportingEndpoint(
        *coop.report_only_reporting_endpoint);
  }
  return protocol_coop;
}
std::unique_ptr<protocol::Network::CrossOriginEmbedderPolicyStatus>
makeCrossOriginEmbedderPolicyStatus(
    const network::CrossOriginEmbedderPolicy& coep) {
  auto protocol_coep =
      protocol::Network::CrossOriginEmbedderPolicyStatus::Create()
          .SetValue(makeCrossOriginEmbedderPolicyValue(coep.value))
          .SetReportOnlyValue(
              makeCrossOriginEmbedderPolicyValue(coep.report_only_value))
          .Build();

  if (coep.reporting_endpoint)
    protocol_coep->SetReportingEndpoint(*coep.reporting_endpoint);
  if (coep.report_only_reporting_endpoint) {
    protocol_coep->SetReportOnlyReportingEndpoint(
        *coep.report_only_reporting_endpoint);
  }
  return protocol_coep;
}
std::unique_ptr<protocol::Array<protocol::Network::ContentSecurityPolicyStatus>>
makeContentSecurityPolicyStatus(
    const std::vector<network::mojom::ContentSecurityPolicyHeader>&
        csp_headers) {
  auto csp_status = std::make_unique<
      protocol::Array<protocol::Network::ContentSecurityPolicyStatus>>();
  for (const auto& csp_header : csp_headers) {
    auto csp_status_component =
        protocol::Network::ContentSecurityPolicyStatus::Create()
            .SetEffectiveDirectives(csp_header.header_value)
            .SetIsEnforced(csp_header.type ==
                           network::mojom::ContentSecurityPolicyType::kEnforce)
            .SetSource(makeContentSecurityPolicySource(csp_header.source))
            .Build();
    csp_status->emplace_back(std::move(csp_status_component));
  }
  return csp_status;
}
}  // namespace

DispatchResponse NetworkHandler::GetSecurityIsolationStatus(
    Maybe<String> frame_id,
    std::unique_ptr<protocol::Network::SecurityIsolationStatus>* out_info) {
  scoped_refptr<DevToolsAgentHostImpl> host =
      DevToolsAgentHostImpl::GetForId(host_id_);
  std::string id = frame_id.value_or("");
  auto maybe_coep = host->cross_origin_embedder_policy(id);
  auto maybe_coop = host->cross_origin_opener_policy(id);
  auto maybe_csp = host->content_security_policy(id);
  auto status = protocol::Network::SecurityIsolationStatus::Create().Build();
  if (maybe_coep) {
    status->SetCoep(makeCrossOriginEmbedderPolicyStatus(*maybe_coep));
  }
  if (maybe_coop) {
    status->SetCoop(makeCrossOriginOpenerPolicyStatus(*maybe_coop));
  }
  if (maybe_csp) {
    status->SetCsp(makeContentSecurityPolicyStatus(*maybe_csp));
  }
  *out_info = std::move(status);
  return Response::Success();
}

void NetworkHandler::OnRequestWillBeSentExtraInfo(
    const std::string& devtools_request_id,
    const net::CookieAccessResultList& request_cookie_list,
    const std::vector<network::mojom::HttpRawHeaderPairPtr>& request_headers,
    const base::TimeTicks timestamp,
    const network::mojom::ClientSecurityStatePtr& security_state,
    const network::mojom::OtherPartitionInfoPtr& other_partition_info) {
  if (!enabled_)
    return;

  frontend_->RequestWillBeSentExtraInfo(
      devtools_request_id, BuildProtocolAssociatedCookies(request_cookie_list),
      GetRawHeaders(request_headers), GetConnectTiming(timestamp),
      MaybeBuildClientSecurityState(security_state),
      other_partition_info
          ? Maybe<bool>(
                other_partition_info->site_has_cookie_in_other_partition)
          : Maybe<bool>());
}

void NetworkHandler::OnResponseReceivedExtraInfo(
    const std::string& devtools_request_id,
    const net::CookieAndLineAccessResultList& response_cookie_list,
    const std::vector<network::mojom::HttpRawHeaderPairPtr>& response_headers,
    const std::optional<std::string>& response_headers_text,
    network::mojom::IPAddressSpace resource_address_space,
    int32_t http_status_code,
    const std::optional<net::CookiePartitionKey>& cookie_partition_key) {
  if (!enabled_)
    return;

  Maybe<Network::CookiePartitionKey> frontend_partition_key;
  // TODO (crbug.com/326605834) Once ancestor chain bit changes are implemented
  // update this method utilize the ancestor bit.
  if (cookie_partition_key) {
    base::expected<net::CookiePartitionKey::SerializedCookiePartitionKey,
                   std::string>
        serialized_result =
            net::CookiePartitionKey::Serialize(cookie_partition_key);
    if (serialized_result.has_value()) {
      frontend_partition_key =
          BuildCookiePartitionKey(serialized_result->TopLevelSite(),
                                  serialized_result->has_cross_site_ancestor());
    }
  }

  frontend_->ResponseReceivedExtraInfo(
      devtools_request_id, BuildProtocolBlockedSetCookies(response_cookie_list),
      GetRawHeaders(response_headers),
      BuildIpAddressSpace(resource_address_space), http_status_code,
      response_headers_text.has_value() ? response_headers_text.value()
                                        : Maybe<String>(),
      std::move(frontend_partition_key),
      cookie_partition_key
          ? Maybe<bool>(!cookie_partition_key->IsSerializeable())
          : Maybe<bool>(),
      BuildProtocolExemptedSetCookies(response_cookie_list));
}

void NetworkHandler::OnResponseReceivedEarlyHints(
    const std::string& devtools_request_id,
    const std::vector<network::mojom::HttpRawHeaderPairPtr>& response_headers) {
  if (!enabled_) {
    return;
  }

  frontend_->ResponseReceivedEarlyHints(devtools_request_id,
                                        GetRawHeaders(response_headers));
}

void NetworkHandler::OnLoadNetworkResourceFinished(
    DevToolsNetworkResourceLoader* loader,
    const net::HttpResponseHeaders* rh,
    bool success,
    int net_error,
    std::string content) {
  auto it = loaders_.find(loader);
  CHECK(it != loaders_.end());
  auto callback = std::move(it->second);
  auto result = Network::LoadNetworkResourcePageResult::Create()
                    .SetSuccess(success)
                    .Build();

  if (net_error != net::OK) {
    result->SetNetError(net_error);
    result->SetNetErrorName(net::ErrorToString(net_error));
  }

  if (success) {
    bool is_binary = true;
    std::string mime_type;
    if (rh && rh->GetMimeType(&mime_type)) {
      is_binary = !DevToolsIOContext::IsTextMimeType(mime_type);
    }
    // TODO(sigurds): Use the data-pipe from the network loader.
    scoped_refptr<DevToolsStreamFile> stream =
        DevToolsStreamFile::Create(io_context_, is_binary);
    stream->Append(std::make_unique<std::string>(std::move(content)));
    result->SetStream(stream->handle());
  }

  if (rh) {
    result->SetHttpStatusCode(rh->response_code());
    result->SetHeaders(BuildResponseHeaders(rh));
  }

  callback->sendSuccess(std::move(result));
  loaders_.erase(it);
}

namespace {

mojo::PendingRemote<network::mojom::URLLoaderFactory>
CreateNetworkFactoryForDevTools(
    std::string_view scheme,
    RenderProcessHost* host,
    int routing_id,
    const url::Origin& origin,
    network::mojom::URLLoaderFactoryParamsPtr params) {
  if (!host || !params) {
    // Return an invalid remote by default.
    return {};
  }

  // Don't allow trust token redemption.
  params->trust_token_redemption_policy =
      network::mojom::TrustTokenOperationPolicyVerdict::kForbid;
  // Don't allow trust token issuance.
  params->trust_token_issuance_policy =
      network::mojom::TrustTokenOperationPolicyVerdict::kForbid;
  // Let DevTools fetch resources without CORS and ORB. Source maps are valid
  // JSON and would otherwise require a CORS fetch + correct response headers.
  // See BUG(chromium:1076435) for more context.
  params->is_orb_enabled = false;

  if (scheme == url::kHttpScheme || scheme == url::kHttpsScheme) {
    return url_loader_factory::CreatePendingRemote(
        ContentBrowserClient::URLLoaderFactoryType::kDevTools,
        url_loader_factory::TerminalParams::ForNetworkContext(
            host->GetStoragePartition()->GetNetworkContext(),
            std::move(params)));
  }

  if (scheme != url::kFileScheme) {
    ContentBrowserClient::NonNetworkURLLoaderFactoryMap factories;
    GetContentClient()
        ->browser()
        ->RegisterNonNetworkSubresourceURLLoaderFactories(
            host->GetID(), routing_id, origin, &factories);
    auto i = factories.find(std::string(scheme));
    if (i == factories.end()) {
      return {};
    }
    return std::move(i->second);
  }
  return {};
}
}  // namespace

void NetworkHandler::LoadNetworkResource(
    Maybe<protocol::String> maybe_frame_id,
    const String& url,
    std::unique_ptr<protocol::Network::LoadNetworkResourceOptions> options,
    std::unique_ptr<LoadNetworkResourceCallback> callback) {
  GURL gurl(url);
  const bool is_gurl_valid = gurl.is_valid();
  if (!is_gurl_valid) {
    callback->sendFailure(Response::InvalidParams("The url must be valid"));
    return;
  }

  if (gurl.SchemeIs(url::kFileScheme) && !client_->MayReadLocalFiles()) {
    callback->sendFailure(Response::InvalidParams("Unsupported URL scheme"));
    return;
  }

  const DevToolsNetworkResourceLoader::Caching caching =
      options->GetDisableCache()
          ? DevToolsNetworkResourceLoader::Caching::kBypass
          : DevToolsNetworkResourceLoader::Caching::kDefault;
  const DevToolsNetworkResourceLoader::Credentials include_credentials =
      options->GetIncludeCredentials()
          ? DevToolsNetworkResourceLoader::Credentials::kInclude
          : DevToolsNetworkResourceLoader::Credentials::kSameSite;
  DevToolsNetworkResourceLoader::CompletionCallback complete_callback =
      base::BindOnce(&NetworkHandler::OnLoadNetworkResourceFinished,
                     base::Unretained(this));

  mojo::Remote<network::mojom::URLLoaderFactory> url_loader_factory;
  if (host_) {
    if (!maybe_frame_id.has_value()) {
      callback->sendFailure(Response::InvalidParams(
          "Parameter frameId must be provided for frame targets"));
      return;
    }
    FrameTreeNode* node = FrameTreeNodeFromDevToolsFrameToken(
        host_->frame_tree_node(), maybe_frame_id.value());
    RenderFrameHostImpl* frame = node ? node->current_frame_host() : nullptr;
    if (!frame) {
      callback->sendFailure(Response::InvalidParams("Frame not found"));
      return;
    }
    // Don't allow fetching resources for frames goverened by different
    // DevToolsAgentHosts.
    if (GetFrameTreeNodeAncestor(node) !=
        GetFrameTreeNodeAncestor(host_->frame_tree_node())) {
      callback->sendFailure(
          Response::InvalidParams("Frame not under control of agent host"));
      return;
    }

    auto params = URLLoaderFactoryParamsHelper::CreateForFrame(
        frame, frame->GetLastCommittedOrigin(),
        frame->GetIsolationInfoForSubresources(),
        frame->BuildClientSecurityState(),
        /**coep_reporter=*/mojo::NullRemote(), frame->GetProcess(),
        network::mojom::TrustTokenOperationPolicyVerdict::kForbid,
        network::mojom::TrustTokenOperationPolicyVerdict::kForbid,
        frame->GetCookieSettingOverrides(),
        "NetworkHandler::LoadNetworkResource");

    auto factory = CreateNetworkFactoryForDevTools(
        gurl.scheme(), frame->GetProcess(), frame->GetRoutingID(),
        frame->GetLastCommittedOrigin(), std::move(params));
    if (!factory.is_valid()) {
      callback->sendFailure(Response::InvalidParams("Unsupported URL scheme"));
      return;
    }

    url_loader_factory.Bind(std::move(factory));
    auto loader = DevToolsNetworkResourceLoader::Create(
        std::move(url_loader_factory), std::move(gurl),
        frame->GetLastCommittedOrigin(), frame->ComputeSiteForCookies(),
        caching, include_credentials, std::move(complete_callback));
    loaders_.emplace(std::move(loader), std::move(callback));
    return;
  }
  scoped_refptr<DevToolsAgentHostImpl> host =
      DevToolsAgentHostImpl::GetForId(host_id_);
  if (host) {
    // TODO(sigurds): Support dedicated workers.
    auto info = host->CreateNetworkFactoryParamsForDevTools();
    auto factory = CreateNetworkFactoryForDevTools(
        gurl.scheme(), host->GetProcessHost(), MSG_ROUTING_NONE, info.origin,
        std::move(info.factory_params));
    if (factory.is_valid()) {
      url_loader_factory.Bind(std::move(factory));
      auto loader = DevToolsNetworkResourceLoader::Create(
          std::move(url_loader_factory), std::move(gurl),
          std::move(info.origin), std::move(info.site_for_cookies), caching,
          include_credentials, std::move(complete_callback));
      loaders_.emplace(std::move(loader), std::move(callback));
      return;
    }
  }
  callback->sendFailure(Response::ServerError("Target not supported"));
}

namespace {

String GetTrustTokenOperationStatus(
    network::mojom::TrustTokenOperationStatus status) {
  switch (status) {
    case network::mojom::TrustTokenOperationStatus::kOk:
      return protocol::Network::TrustTokenOperationDone::StatusEnum::Ok;
    case network::mojom::TrustTokenOperationStatus::kInvalidArgument:
      return protocol::Network::TrustTokenOperationDone::StatusEnum::
          InvalidArgument;
    case network::mojom::TrustTokenOperationStatus::kMissingIssuerKeys:
      return protocol::Network::TrustTokenOperationDone::StatusEnum::
          MissingIssuerKeys;
    case network::mojom::TrustTokenOperationStatus::kFailedPrecondition:
      return protocol::Network::TrustTokenOperationDone::StatusEnum::
          FailedPrecondition;
    case network::mojom::TrustTokenOperationStatus::kResourceExhausted:
      return protocol::Network::TrustTokenOperationDone::StatusEnum::
          ResourceExhausted;
    case network::mojom::TrustTokenOperationStatus::kAlreadyExists:
      return protocol::Network::TrustTokenOperationDone::StatusEnum::
          AlreadyExists;
    case network::mojom::TrustTokenOperationStatus::kResourceLimited:
      return protocol::Network::TrustTokenOperationDone::StatusEnum::
          ResourceLimited;
    case network::mojom::TrustTokenOperationStatus::kUnauthorized:
      return protocol::Network::TrustTokenOperationDone::StatusEnum::
          Unauthorized;
    case network::mojom::TrustTokenOperationStatus::kBadResponse:
      return protocol::Network::TrustTokenOperationDone::StatusEnum::
          BadResponse;
    case network::mojom::TrustTokenOperationStatus::kInternalError:
      return protocol::Network::TrustTokenOperationDone::StatusEnum::
          InternalError;
    case network::mojom::TrustTokenOperationStatus::kUnknownError:
      return protocol::Network::TrustTokenOperationDone::StatusEnum::
          UnknownError;
    case network::mojom::TrustTokenOperationStatus::
        kOperationSuccessfullyFulfilledLocally:
      return protocol::Network::TrustTokenOperationDone::StatusEnum::
          FulfilledLocally;
  }
}

}  // namespace

void NetworkHandler::OnTrustTokenOperationDone(
    const std::string& devtools_request_id,
    const network::mojom::TrustTokenOperationResult& result) {
  if (!enabled_)
    return;

  Maybe<String> top_level_origin;
  if (result.top_level_origin) {
    top_level_origin = result.top_level_origin->Serialize();
  }
  Maybe<String> issuer;
  if (result.issuer) {
    issuer = result.issuer->Serialize();
  }

  frontend()->TrustTokenOperationDone(
      GetTrustTokenOperationStatus(result.status),
      GetTrustTokenOperationType(result.operation), devtools_request_id,
      std::move(top_level_origin), std::move(issuer),
      result.issued_token_count);
}

void NetworkHandler::OnSubresourceWebBundleMetadata(
    const std::string& devtools_request_id,
    const std::vector<GURL>& urls) {
  if (!enabled_)
    return;

  auto new_urls = std::make_unique<protocol::Array<protocol::String>>();
  for (const auto& url : urls) {
    new_urls->push_back(url.spec());
  }
  frontend()->SubresourceWebBundleMetadataReceived(devtools_request_id,
                                                   std::move(new_urls));
}

void NetworkHandler::OnSubresourceWebBundleMetadataError(
    const std::string& devtools_request_id,
    const std::string& error_message) {
  if (!enabled_)
    return;

  frontend()->SubresourceWebBundleMetadataError(devtools_request_id,
                                                error_message);
}

void NetworkHandler::OnSubresourceWebBundleInnerResponse(
    const std::string& inner_request_devtools_id,
    const GURL& url,
    const std::optional<std::string>& bundle_request_devtools_id) {
  if (!enabled_)
    return;

  frontend()->SubresourceWebBundleInnerResponseParsed(
      inner_request_devtools_id, url.spec(),
      bundle_request_devtools_id.has_value()
          ? Maybe<std::string>(*bundle_request_devtools_id)
          : Maybe<std::string>());
}

void NetworkHandler::OnSubresourceWebBundleInnerResponseError(
    const std::string& inner_request_devtools_id,
    const GURL& url,
    const std::string& error_message,
    const std::optional<std::string>& bundle_request_devtools_id) {
  if (!enabled_)
    return;

  frontend()->SubresourceWebBundleInnerResponseError(
      inner_request_devtools_id, url.spec(), error_message,
      bundle_request_devtools_id.has_value()
          ? Maybe<std::string>(*bundle_request_devtools_id)
          : Maybe<std::string>());
}

void NetworkHandler::OnPolicyContainerHostUpdated() {
  if (!enabled_) {
    return;
  }
  frontend()->PolicyUpdated();
}

String NetworkHandler::BuildPrivateNetworkRequestPolicy(
    network::mojom::PrivateNetworkRequestPolicy policy) {
  switch (policy) {
    case network::mojom::PrivateNetworkRequestPolicy::kAllow:
      return protocol::Network::PrivateNetworkRequestPolicyEnum::Allow;
    case network::mojom::PrivateNetworkRequestPolicy::kBlock:
      // TODO(crbug.com/40154414): Fix this.
      return protocol::Network::PrivateNetworkRequestPolicyEnum::
          BlockFromInsecureToMorePrivate;
    case network::mojom::PrivateNetworkRequestPolicy::kWarn:
      // TODO(crbug.com/40154414): Fix this.
      return protocol::Network::PrivateNetworkRequestPolicyEnum::
          WarnFromInsecureToMorePrivate;
    case network::mojom::PrivateNetworkRequestPolicy::kPreflightBlock:
      return protocol::Network::PrivateNetworkRequestPolicyEnum::PreflightBlock;
    case network::mojom::PrivateNetworkRequestPolicy::kPreflightWarn:
      return protocol::Network::PrivateNetworkRequestPolicyEnum::PreflightWarn;
  }
}

String NetworkHandler::BuildIpAddressSpace(
    network::mojom::IPAddressSpace space) {
  switch (space) {
    case network::mojom::IPAddressSpace::kLocal:
      return protocol::Network::IPAddressSpaceEnum::Local;
    case network::mojom::IPAddressSpace::kPrivate:
      return protocol::Network::IPAddressSpaceEnum::Private;
    case network::mojom::IPAddressSpace::kPublic:
      return protocol::Network::IPAddressSpaceEnum::Public;
    case network::mojom::IPAddressSpace::kUnknown:
      return protocol::Network::IPAddressSpaceEnum::Unknown;
  }
}

std::unique_ptr<protocol::Network::ClientSecurityState>
NetworkHandler::MaybeBuildClientSecurityState(
    const network::mojom::ClientSecurityStatePtr& state) {
  return state ? protocol::Network::ClientSecurityState::Create()
                     .SetPrivateNetworkRequestPolicy(
                         BuildPrivateNetworkRequestPolicy(
                             state->private_network_request_policy))
                     .SetInitiatorIPAddressSpace(
                         BuildIpAddressSpace(state->ip_address_space))
                     .SetInitiatorIsSecureContext(state->is_web_secure_context)
                     .Build()
               : nullptr;
}

std::unique_ptr<protocol::Network::CorsErrorStatus>
NetworkHandler::BuildCorsErrorStatus(const network::CorsErrorStatus& status) {
  return protocol::Network::CorsErrorStatus::Create()
      .SetCorsError(BuildCorsError(status.cors_error))
      .SetFailedParameter(status.failed_parameter)
      .Build();
}

}  // namespace protocol
}  // namespace content
