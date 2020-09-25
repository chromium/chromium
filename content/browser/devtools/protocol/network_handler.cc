// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/network_handler.h"

#include <stddef.h>
#include <stdint.h>
#include <memory>

#include "base/barrier_closure.h"
#include "base/base64.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/containers/queue.h"
#include "base/i18n/i18n_constants.h"
#include "base/i18n/icu_string_conversions.h"
#include "base/json/json_reader.h"
#include "base/process/process_handle.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/task/post_task.h"
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
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/storage_partition_impl.h"
#include "content/browser/url_loader_factory_params_helper.h"
#include "content/browser/web_package/signed_exchange_envelope.h"
#include "content/browser/web_package/signed_exchange_error.h"
#include "content/common/navigation_params.h"
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
#include "net/cookies/cookie_util.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/http/http_util.h"
#include "net/ssl/ssl_cipher_suite_names.h"
#include "net/ssl/ssl_connection_status_flags.h"
#include "net/url_request/referrer_policy.h"
#include "services/network/public/cpp/data_element.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/http_raw_request_response_info.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/blink/public/common/loader/network_utils.h"
#include "third_party/blink/public/common/loader/referrer_utils.h"
#include "third_party/blink/public/platform/resource_request_blocked_reason.h"
#include "third_party/boringssl/src/include/openssl/ssl.h"

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

const char kInvalidCookieFields[] = "Invalid cookie fields";

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
      NOTREACHED();
      return Network::CertificateTransparencyComplianceEnum::Unknown;
  }
  NOTREACHED();
  return Network::CertificateTransparencyComplianceEnum::Unknown;
}

std::unique_ptr<Network::Cookie> BuildCookie(
    const net::CanonicalCookie& cookie) {
  String cp;
  switch (cookie.Priority()) {
    case net::CookiePriority::COOKIE_PRIORITY_HIGH:
      cp = Network::CookiePriorityEnum::High;
      break;
    case net::CookiePriority::COOKIE_PRIORITY_MEDIUM:
      cp = Network::CookiePriorityEnum::Medium;
      break;
    case net::CookiePriority::COOKIE_PRIORITY_LOW:
      cp = Network::CookiePriorityEnum::Low;
      break;
  }

  std::unique_ptr<Network::Cookie> devtools_cookie =
      Network::Cookie::Create()
          .SetName(cookie.Name())
          .SetValue(cookie.Value())
          .SetDomain(cookie.Domain())
          .SetPath(cookie.Path())
          .SetExpires(cookie.ExpiryDate().is_null()
                          ? -1
                          : cookie.ExpiryDate().ToDoubleT())
          .SetSize(cookie.Name().length() + cookie.Value().length())
          .SetHttpOnly(cookie.IsHttpOnly())
          .SetSecure(cookie.IsSecure())
          .SetSession(!cookie.IsPersistent())
          .SetPriority(cp)
          .Build();

  switch (cookie.SameSite()) {
    case net::CookieSameSite::STRICT_MODE:
      devtools_cookie->SetSameSite(Network::CookieSameSiteEnum::Strict);
      break;
    case net::CookieSameSite::LAX_MODE:
      devtools_cookie->SetSameSite(Network::CookieSameSiteEnum::Lax);
      break;
    case net::CookieSameSite::NO_RESTRICTION:
      devtools_cookie->SetSameSite(Network::CookieSameSiteEnum::None);
      break;
    case net::CookieSameSite::UNSPECIFIED:
      break;
  }
  return devtools_cookie;
}

class CookieRetrieverNetworkService
    : public base::RefCounted<CookieRetrieverNetworkService> {
 public:
  static void Retrieve(network::mojom::CookieManager* cookie_manager,
                       const std::vector<GURL> urls,
                       std::unique_ptr<GetCookiesCallback> callback) {
    scoped_refptr<CookieRetrieverNetworkService> self =
        new CookieRetrieverNetworkService(std::move(callback));
    net::CookieOptions cookie_options = net::CookieOptions::MakeAllInclusive();
    for (const auto& url : urls) {
      cookie_manager->GetCookieList(
          url, cookie_options,
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
      std::string key = base::StringPrintf(
          "%s::%s::%s::%d", cookie.Name().c_str(), cookie.Domain().c_str(),
          cookie.Path().c_str(), cookie.IsSecure());
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

std::vector<net::CanonicalCookie> FilterCookies(
    const std::vector<net::CanonicalCookie>& cookies,
    const std::string& name,
    const std::string& normalized_domain,
    const std::string& path) {
  std::vector<net::CanonicalCookie> result;

  for (const auto& cookie : cookies) {
    if (cookie.Name() != name)
      continue;
    if (cookie.Domain() != normalized_domain)
      continue;
    if (!path.empty() && cookie.Path() != path)
      continue;
    result.push_back(cookie);
  }

  return result;
}

void DeleteFilteredCookies(network::mojom::CookieManager* cookie_manager,
                           const std::string& name,
                           const std::string& normalized_domain,
                           const std::string& path,
                           std::unique_ptr<DeleteCookiesCallback> callback,
                           const std::vector<net::CanonicalCookie>& cookies) {
  std::vector<net::CanonicalCookie> filtered_list =
      FilterCookies(cookies, name, normalized_domain, path);

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

std::unique_ptr<net::CanonicalCookie> MakeCookieFromProtocolValues(
    const std::string& name,
    const std::string& value,
    const std::string& url_spec,
    const std::string& domain,
    const std::string& path,
    bool secure,
    bool http_only,
    const std::string& same_site,
    double expires,
    const std::string& priority) {
  std::string normalized_domain = domain;

  if (url_spec.empty() && domain.empty())
    return nullptr;

  if (!url_spec.empty()) {
    GURL source_url = GURL(url_spec);
    if (!source_url.SchemeIsHTTPOrHTTPS())
      return nullptr;

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
    expiration_date =
        expires ? base::Time::FromDoubleT(expires) : base::Time::UnixEpoch();
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

  return net::CanonicalCookie::CreateSanitizedCookie(
      url, name, value, normalized_domain, path, base::Time(), expiration_date,
      base::Time(), secure, http_only, css, cp);
}

std::vector<GURL> ComputeCookieURLs(RenderFrameHostImpl* frame_host,
                                    Maybe<Array<String>>& protocol_urls) {
  std::vector<GURL> urls;

  if (protocol_urls.isJust()) {
    for (const std::string& url : *protocol_urls.fromJust())
      urls.emplace_back(url);
  } else {
    base::queue<FrameTreeNode*> queue;
    queue.push(frame_host->frame_tree_node());
    while (!queue.empty()) {
      FrameTreeNode* node = queue.front();
      queue.pop();

      urls.push_back(node->current_url());
      for (size_t i = 0; i < node->child_count(); ++i)
        queue.push(node->child_at(i));
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
  NOTREACHED();
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
  NOTREACHED();
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
    if (blink::network_utils::IsOriginSecure(url))
      return Security::SecurityStateEnum::Secure;
    return Security::SecurityStateEnum::Insecure;
  }
  if (net::IsCertStatusError(cert_status))
    return Security::SecurityStateEnum::Insecure;
  return Security::SecurityStateEnum::Secure;
}

DevToolsURLLoaderInterceptor::InterceptionStage ToInterceptorStage(
    const protocol::Network::InterceptionStage& interceptor_stage) {
  if (interceptor_stage == protocol::Network::InterceptionStageEnum::Request)
    return DevToolsURLLoaderInterceptor::REQUEST;
  if (interceptor_stage ==
      protocol::Network::InterceptionStageEnum::HeadersReceived)
    return DevToolsURLLoaderInterceptor::RESPONSE;
  NOTREACHED();
  return DevToolsURLLoaderInterceptor::REQUEST;
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
  return Network::ResourceTiming::Create()
      .SetRequestTime((load_timing.request_start - kNullTicks).InSecondsF())
      .SetProxyStart(
          timeDelta(load_timing.proxy_resolve_start, load_timing.request_start))
      .SetProxyEnd(
          timeDelta(load_timing.proxy_resolve_end, load_timing.request_start))
      .SetDnsStart(timeDelta(load_timing.connect_timing.dns_start,
                             load_timing.request_start))
      .SetDnsEnd(timeDelta(load_timing.connect_timing.dns_end,
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
      .SetSendEnd(timeDelta(load_timing.send_end, load_timing.request_start))
      .SetPushStart(
          timeDelta(load_timing.push_start, load_timing.request_start, 0))
      .SetPushEnd(timeDelta(load_timing.push_end, load_timing.request_start, 0))
      .SetReceiveHeadersEnd(
          timeDelta(load_timing.receive_headers_end, load_timing.request_start))
      .Build();
}

std::unique_ptr<Object> GetRawHeaders(
    const std::vector<network::mojom::HttpRawHeaderPairPtr>& headers) {
  std::unique_ptr<DictionaryValue> headers_dict(DictionaryValue::create());
  for (const auto& header : headers) {
    std::string value;
    bool merge_with_another = headers_dict->getString(header->key, &value);
    std::string header_value;
    if (!base::ConvertToUtf8AndNormalize(header->value, base::kCodepageLatin1,
                                         &header_value)) {
      // For response headers, the encoding could be anything, so conversion
      // might fail; in that case this is the most useful thing we can do.
      header_value = header->value;
    }
    headers_dict->setString(header->key, merge_with_another
                                             ? value + '\n' + header_value
                                             : header_value);
  }
  return Object::fromValue(headers_dict.get(), nullptr);
}

String GetProtocol(const GURL& url,
                   const network::mojom::URLResponseHead& info) {
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
    if (element.type() != network::mojom::DataElementType::kBytes)
      return false;
    auto bytes = protocol::Binary::fromSpan(
        reinterpret_cast<const uint8_t*>(element.bytes()), element.length());
    auto data_entry = protocol::Network::PostDataEntry::Create().Build();
    data_entry->SetBytes(std::move(bytes));
    data_entries->push_back(std::move(data_entry));
    result->append(element.bytes(), element.length());
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
  NOTREACHED();
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
    blockedReasons->push_back(
        Network::SetCookieBlockedReasonEnum::SameSiteStrict);
  }
  if (status.HasExclusionReason(
          net::CookieInclusionStatus::EXCLUDE_SAMESITE_LAX)) {
    blockedReasons->push_back(Network::SetCookieBlockedReasonEnum::SameSiteLax);
  }
  if (status.HasExclusionReason(
          net::CookieInclusionStatus::
              EXCLUDE_SAMESITE_UNSPECIFIED_TREATED_AS_LAX)) {
    blockedReasons->push_back(
        Network::SetCookieBlockedReasonEnum::SameSiteUnspecifiedTreatedAsLax);
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
  if (status.HasExclusionReason(
          net::CookieInclusionStatus::EXCLUDE_UNKNOWN_ERROR)) {
    blockedReasons->push_back(
        Network::SetCookieBlockedReasonEnum::UnknownError);
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
    blockedReasons->push_back(Network::CookieBlockedReasonEnum::SameSiteStrict);
  }
  if (status.HasExclusionReason(
          net::CookieInclusionStatus::EXCLUDE_SAMESITE_LAX)) {
    blockedReasons->push_back(Network::CookieBlockedReasonEnum::SameSiteLax);
  }
  if (status.HasExclusionReason(
          net::CookieInclusionStatus::
              EXCLUDE_SAMESITE_UNSPECIFIED_TREATED_AS_LAX)) {
    blockedReasons->push_back(
        Network::CookieBlockedReasonEnum::SameSiteUnspecifiedTreatedAsLax);
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

std::unique_ptr<Array<Network::BlockedCookieWithReason>>
BuildProtocolAssociatedCookies(const net::CookieAccessResultList& net_list) {
  auto protocol_list =
      std::make_unique<Array<Network::BlockedCookieWithReason>>();

  for (const net::CookieWithAccessResult& cookie : net_list) {
    std::unique_ptr<Array<Network::CookieBlockedReason>> blocked_reasons =
        GetProtocolBlockedCookieReason(cookie.access_result.status);
    // Note that the condition below is not always true,
    // as there might be blocked reasons that we do not report.
    if (blocked_reasons->size() || cookie.access_result.status.IsInclude()) {
      protocol_list->push_back(
          Network::BlockedCookieWithReason::Create()
              .SetBlockedReasons(std::move(blocked_reasons))
              .SetCookie(BuildCookie(cookie.cookie))
              .Build());
    }
  }
  return protocol_list;
}

}  // namespace

class BackgroundSyncRestorer {
 public:
  BackgroundSyncRestorer(const std::string& host_id,
                         StoragePartition* storage_partition)
      : host_id_(host_id),
        storage_partition_(storage_partition),
        offline_sw_registration_id_(
            new int64_t(blink::mojom::kInvalidServiceWorkerRegistrationId)) {
    SetServiceWorkerOfflineStatus(true);
  }

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
      RunOrPostTaskOnThread(
          FROM_HERE, ServiceWorkerContext::GetCoreThreadId(),
          base::BindOnce(
              &SetServiceWorkerOfflineOnServiceWorkerCoreThread, sync_context,
              base::WrapRefCounted(static_cast<ServiceWorkerContextWrapper*>(
                  storage_partition_->GetServiceWorkerContext())),
              service_worker_host->version_id(),
              offline_sw_registration_id_.get()));
    } else {
      RunOrPostTaskOnThread(
          FROM_HERE, ServiceWorkerContext::GetCoreThreadId(),
          base::BindOnce(&SetServiceWorkerOnlineOnServiceWorkerCoreThread,
                         sync_context, offline_sw_registration_id_.get()));
    }
  }

  static void SetServiceWorkerOfflineOnServiceWorkerCoreThread(
      scoped_refptr<BackgroundSyncContextImpl> sync_context,
      scoped_refptr<ServiceWorkerContextWrapper> swcontext,
      int64_t version_id,
      int64_t* offline_sw_registration_id) {
    DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
    ServiceWorkerVersion* version = swcontext.get()->GetLiveVersion(version_id);
    if (!version)
      return;
    int64_t registration_id = version->registration_id();
    *offline_sw_registration_id = registration_id;
    if (registration_id == blink::mojom::kInvalidServiceWorkerRegistrationId)
      return;
    sync_context->background_sync_manager()->EmulateServiceWorkerOffline(
        registration_id, true);
  }

  static void SetServiceWorkerOnlineOnServiceWorkerCoreThread(
      scoped_refptr<BackgroundSyncContextImpl> sync_context,
      int64_t* offline_sw_registration_id) {
    DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
    if (*offline_sw_registration_id ==
        blink::mojom::kInvalidServiceWorkerRegistrationId) {
      return;
    }
    sync_context->background_sync_manager()->EmulateServiceWorkerOffline(
        *offline_sw_registration_id, false);
  }

  std::string host_id_;
  StoragePartition* storage_partition_;
  std::unique_ptr<int64_t, content::BrowserThread::DeleteOnIOThread>
      offline_sw_registration_id_;

  DISALLOW_COPY_AND_ASSIGN(BackgroundSyncRestorer);
};

NetworkHandler::NetworkHandler(
    const std::string& host_id,
    const base::UnguessableToken& devtools_token,
    DevToolsIOContext* io_context,
    base::RepeatingClosure update_loader_factories_callback)
    : DevToolsDomainHandler(Network::Metainfo::domainName),
      host_id_(host_id),
      devtools_token_(devtools_token),
      io_context_(io_context),
      browser_context_(nullptr),
      storage_partition_(nullptr),
      host_(nullptr),
      enabled_(false),
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
  frontend_.reset(new Network::Frontend(dispatcher->channel()));
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
  if (background_sync_restorer_ && storage_partition_)
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
  return Response::FallThrough();
}

Response NetworkHandler::SetCacheDisabled(bool cache_disabled) {
  cache_disabled_ = cache_disabled;
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
  content::BrowsingDataRemover* remover_;
  std::unique_ptr<NetworkHandler::ClearBrowserCacheCallback> callback_;
};

void NetworkHandler::ClearBrowserCache(
    std::unique_ptr<ClearBrowserCacheCallback> callback) {
  if (!browser_context_) {
    callback->sendFailure(Response::InternalError());
    return;
  }
  content::BrowsingDataRemover* remover =
      content::BrowserContext::GetBrowsingDataRemover(browser_context_);
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

  CookieRetrieverNetworkService::Retrieve(
      storage_partition_->GetCookieManagerForBrowserProcess(), urls,
      std::move(callback));
}

void NetworkHandler::GetAllCookies(
    std::unique_ptr<GetAllCookiesCallback> callback) {
  if (!storage_partition_) {
    callback->sendFailure(Response::InternalError());
    return;
  }
  storage_partition_->GetCookieManagerForBrowserProcess()->GetAllCookies(
      base::BindOnce(
          [](std::unique_ptr<GetAllCookiesCallback> callback,
             const std::vector<net::CanonicalCookie>& cookies) {
            callback->sendSuccess(NetworkHandler::BuildCookieArray(cookies));
          },
          std::move(callback)));
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
                               std::unique_ptr<SetCookieCallback> callback) {
  if (!storage_partition_) {
    callback->sendFailure(Response::InternalError());
    return;
  }

  if (!url.isJust() && !domain.isJust()) {
    callback->sendFailure(Response::InvalidParams(
        "At least one of the url and domain needs to be specified"));
  }

  std::unique_ptr<net::CanonicalCookie> cookie = MakeCookieFromProtocolValues(
      name, value, url.fromMaybe(""), domain.fromMaybe(""), path.fromMaybe(""),
      secure.fromMaybe(false), http_only.fromMaybe(false),
      same_site.fromMaybe(""), expires.fromMaybe(-1), priority.fromMaybe(""));

  if (!cookie) {
    callback->sendFailure(Response::InvalidParams(kInvalidCookieFields));
    return;
  }

  net::CookieOptions options;
  // Permit it to set a SameSite cookie if it wants to.
  options.set_same_site_cookie_context(
      net::CookieOptions::SameSiteCookieContext::MakeInclusive());
  options.set_include_httponly();
  storage_partition_->GetCookieManagerForBrowserProcess()->SetCanonicalCookie(
      *cookie, net::cookie_util::SimulatedCookieSource(*cookie, "https"),
      options,
      net::cookie_util::AdaptCookieAccessResultToBool(base::BindOnce(
          &SetCookieCallback::sendSuccess, std::move(callback))));
}

// static
void NetworkHandler::SetCookies(
    StoragePartition* storage_partition,
    std::unique_ptr<protocol::Array<Network::CookieParam>> cookies,
    base::OnceCallback<void(bool)> callback) {
  std::vector<std::unique_ptr<net::CanonicalCookie>> net_cookies;
  for (const std::unique_ptr<Network::CookieParam>& cookie : *cookies) {
    std::unique_ptr<net::CanonicalCookie> net_cookie =
        MakeCookieFromProtocolValues(
            cookie->GetName(), cookie->GetValue(), cookie->GetUrl(""),
            cookie->GetDomain(""), cookie->GetPath(""),
            cookie->GetSecure(false), cookie->GetHttpOnly(false),
            cookie->GetSameSite(""), cookie->GetExpires(-1),
            cookie->GetPriority(""));
    if (!net_cookie) {
      std::move(callback).Run(false);
      return;
    }
    net_cookies.push_back(std::move(net_cookie));
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
    std::unique_ptr<DeleteCookiesCallback> callback) {
  if (!storage_partition_) {
    callback->sendFailure(Response::InternalError());
    return;
  }
  if (!url_spec.isJust() && !domain.isJust()) {
    callback->sendFailure(Response::InvalidParams(
        "At least one of the url and domain needs to be specified"));
  }

  std::string normalized_domain = domain.fromMaybe("");
  if (normalized_domain.empty()) {
    GURL url(url_spec.fromMaybe(""));
    if (!url.SchemeIsHTTPOrHTTPS()) {
      callback->sendFailure(Response::InvalidParams(
          "An http or https url URL must be specified"));
      return;
    }
    normalized_domain = url.host();
  }

  auto* cookie_manager =
      storage_partition_->GetCookieManagerForBrowserProcess();

  cookie_manager->GetAllCookies(base::BindOnce(
      &DeleteFilteredCookies, base::Unretained(cookie_manager), name,
      normalized_domain, path.fromMaybe(""), std::move(callback)));
}

Response NetworkHandler::SetExtraHTTPHeaders(
    std::unique_ptr<protocol::Network::Headers> headers) {
  std::vector<std::pair<std::string, std::string>> new_headers;
  std::unique_ptr<protocol::DictionaryValue> object = headers->toValue();
  for (size_t i = 0; i < object->size(); ++i) {
    auto entry = object->at(i);
    std::string value;
    if (!entry.second->asString(&value))
      return Response::InvalidParams("Invalid header value, string expected");
    if (!net::HttpUtil::IsValidHeaderName(entry.first))
      return Response::InvalidParams("Invalid header name");
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
    Maybe<protocol::Network::ConnectionType>) {
  network::mojom::NetworkConditionsPtr network_conditions;
  bool throttling_enabled = offline || latency > 0 || download_throughput > 0 ||
                            upload_throughput > 0;
  if (throttling_enabled) {
    network_conditions = network::mojom::NetworkConditions::New();
    network_conditions->offline = offline;
    network_conditions->latency = base::TimeDelta::FromMilliseconds(latency);
    network_conditions->download_throughput = download_throughput;
    network_conditions->upload_throughput = upload_throughput;
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
                .SetLogId(base::HexEncode(sct.sct->log_id.c_str(),
                                          sct.sct->log_id.length()))
                .SetTimestamp((sct.sct->timestamp - base::Time::UnixEpoch())
                                  .InMillisecondsF())
                .SetHashAlgorithm(net::ct::HashAlgorithmToString(
                    sct.sct->signature.hash_algorithm))
                .SetSignatureAlgorithm(net::ct::SignatureAlgorithmToString(
                    sct.sct->signature.signature_algorithm))
                .SetSignatureData(
                    base::HexEncode(sct.sct->signature.signature_data.c_str(),
                                    sct.sct->signature.signature_data.length()))
                .Build();
    signed_certificate_timestamp_list->emplace_back(
        std::move(signed_certificate_timestamp));
  }
  std::vector<std::string> san_dns;
  std::vector<std::string> san_ip;
  ssl_info.cert->GetSubjectAltName(&san_dns, &san_ip);
  auto san_list = std::make_unique<protocol::Array<String>>(std::move(san_dns));
  for (const std::string& san : san_ip) {
    san_list->emplace_back(
        net::IPAddress(reinterpret_cast<const uint8_t*>(san.data()), san.size())
            .ToString());
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
          .SetValidFrom(ssl_info.cert->valid_start().ToDoubleT())
          .SetValidTo(ssl_info.cert->valid_expiry().ToDoubleT())
          .SetCertificateId(0)  // Keep this in protocol for compatability.
          .SetSignedCertificateTimestampList(
              std::move(signed_certificate_timestamp_list))
          .SetCertificateTransparencyCompliance(
              SerializeCTPolicyCompliance(ssl_info.ct_policy_compliance))
          .Build();

  if (ssl_info.key_exchange_group != 0) {
    const char* key_exchange_group =
        SSL_get_curve_name(ssl_info.key_exchange_group);
    if (key_exchange_group)
      security_details->SetKeyExchangeGroup(key_exchange_group);
  }
  if (mac)
    security_details->SetMac(mac);

  return security_details;
}

std::unique_ptr<protocol::Object> BuildResponseHeaders(
    scoped_refptr<net::HttpResponseHeaders> headers) {
  auto headers_dict = DictionaryValue::create();
  if (!headers)
    return std::make_unique<protocol::Object>(std::move(headers_dict));
  size_t iterator = 0;
  std::string name;
  std::string value;
  while (headers->EnumerateHeaderLines(&iterator, &name, &value)) {
    std::string old_value;
    bool merge_with_another = headers_dict->getString(name, &old_value);
    headers_dict->setString(
        name, merge_with_another ? old_value + '\n' + value : value);
  }
  return std::make_unique<protocol::Object>(std::move(headers_dict));
}

String BuildServiceWorkerResponseSource(
    const network::mojom::URLResponseHead& info) {
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

std::unique_ptr<Network::Response> BuildResponse(
    const GURL& url,
    const network::mojom::URLResponseHead& info) {
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
          .SetHeaders(BuildResponseHeaders(info.headers))
          .SetMimeType(info.mime_type)
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
    response->SetResponseTime(info.response_time.ToJsTimeIgnoringNull());
  }
  if (!info.cache_storage_cache_name.empty()) {
    response->SetCacheStorageCacheName(info.cache_storage_cache_name);
  }

  auto* raw_info = info.raw_request_response_info.get();
  if (raw_info) {
    if (raw_info->http_status_code) {
      response->SetStatus(raw_info->http_status_code);
      response->SetStatusText(raw_info->http_status_text);
    }
    if (raw_info->request_headers.size()) {
      response->SetRequestHeaders(GetRawHeaders(raw_info->request_headers));
    }
    if (!raw_info->request_headers_text.empty()) {
      response->SetRequestHeadersText(raw_info->request_headers_text);
    }
    if (raw_info->response_headers.size())
      response->SetHeaders(GetRawHeaders(raw_info->response_headers));
    if (!raw_info->response_headers_text.empty())
      response->SetHeadersText(raw_info->response_headers_text);
  }
  response->SetProtocol(GetProtocol(url, info));
  response->SetRemoteIPAddress(
      net::HostPortPair::FromIPEndPoint(info.remote_endpoint).HostForURL());
  response->SetRemotePort(info.remote_endpoint.port());
  if (info.ssl_info.has_value())
    response->SetSecurityDetails(BuildSecurityDetails(*info.ssl_info));

  return response;
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
    case blink::ResourceRequestBlockedReason::kCollapsedByClient:
      return protocol::Network::BlockedReasonEnum::CollapsedByClient;
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
    case blink::ResourceRequestBlockedReason::kCorpNotSameSite:
      return protocol::Network::BlockedReasonEnum::CorpNotSameSite;
  }
  NOTREACHED();
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
      case network::mojom::BlockedByResponseReason::kCorpNotSameOrigin:
        return {protocol::Network::BlockedReasonEnum::CorpNotSameOrigin};
      case network::mojom::BlockedByResponseReason::kCorpNotSameSite:
        return {protocol::Network::BlockedReasonEnum::CorpNotSameSite};
    }
    NOTREACHED();
  }
  if (status.error_code != net::ERR_BLOCKED_BY_CLIENT &&
      status.error_code != net::ERR_BLOCKED_BY_RESPONSE)
    return Maybe<String>();
  return blockedReason(static_cast<blink::ResourceRequestBlockedReason>(
      status.extended_error_code));
}
}  // namespace

void NetworkHandler::NavigationRequestWillBeSent(
    const NavigationRequest& nav_request,
    base::TimeTicks timestamp) {
  if (!enabled_)
    return;

  net::HttpRequestHeaders headers;
  headers.AddHeadersFromString(nav_request.begin_params()->headers);
  std::unique_ptr<DictionaryValue> headers_dict(DictionaryValue::create());
  for (net::HttpRequestHeaders::Iterator it(headers); it.GetNext();)
    headers_dict->setString(it.name(), it.value());

  const mojom::CommonNavigationParams& common_params =
      nav_request.common_params();
  GURL referrer = common_params.referrer->url;
  // This is normally added down the stack, so we have to fake it here.
  if (!referrer.is_empty())
    headers_dict->setString(net::HttpRequestHeaders::kReferer, referrer.spec());

  std::unique_ptr<Network::Response> redirect_response;
  const mojom::CommitNavigationParams& commit_params =
      nav_request.commit_params();
  if (!commit_params.redirect_response.empty()) {
    redirect_response = BuildResponse(commit_params.redirects.back(),
                                      *commit_params.redirect_response.back());
  }
  std::string url_fragment;
  std::string url_without_fragment =
      ExtractFragment(common_params.url, &url_fragment);
  auto request =
      Network::Request::Create()
          .SetUrl(url_without_fragment)
          .SetMethod(common_params.method)
          .SetHeaders(Object::fromValue(headers_dict.get(), nullptr))
          .SetInitialPriority(resourcePriority(net::HIGHEST))
          .SetReferrerPolicy(referrerPolicy(common_params.referrer->policy))
          .Build();
  if (!url_fragment.empty())
    request->SetUrlFragment(url_fragment);

  if (common_params.post_data) {
    std::string post_data;
    auto data_entries =
        std::make_unique<protocol::Array<protocol::Network::PostDataEntry>>();
    if (GetPostData(*common_params.post_data, data_entries.get(), &post_data)) {
      if (!post_data.empty())
        request->SetPostData(post_data);
      if (data_entries->size())
        request->SetPostDataEntries(std::move(data_entries));
      request->SetHasPostData(true);
    }
  }
  // TODO(caseq): report potentially blockable types
  request->SetMixedContentType(Security::MixedContentTypeEnum::None);

  std::unique_ptr<Network::Initiator> initiator;
  const base::Optional<base::Value>& initiator_optional =
      nav_request.begin_params()->devtools_initiator;
  if (initiator_optional.has_value()) {
    initiator = protocol::ValueTypeConverter<Network::Initiator>::FromValue(
        *toProtocolValue(&initiator_optional.value(), 1000));
  }
  if (!initiator) {
    initiator = Network::Initiator::Create()
                    .SetType(Network::Initiator::TypeEnum::Other)
                    .Build();
  }
  std::string id = nav_request.devtools_navigation_token().ToString();
  double current_ticks = timestamp.since_origin().InSecondsF();
  double current_wall_time = base::Time::Now().ToDoubleT();
  std::string frame_token =
      nav_request.frame_tree_node()->devtools_frame_token().ToString();
  frontend_->RequestWillBeSent(
      id, id, url_without_fragment, std::move(request), current_ticks,
      current_wall_time, std::move(initiator), std::move(redirect_response),
      std::string(Network::ResourceTypeEnum::Document), std::move(frame_token),
      common_params.has_user_gesture);
}

void NetworkHandler::RequestSent(const std::string& request_id,
                                 const std::string& loader_id,
                                 const network::ResourceRequest& request,
                                 const char* initiator_type,
                                 const base::Optional<GURL>& initiator_url,
                                 base::TimeTicks timestamp) {
  if (!enabled_)
    return;
  std::unique_ptr<DictionaryValue> headers_dict(DictionaryValue::create());
  for (net::HttpRequestHeaders::Iterator it(request.headers); it.GetNext();)
    headers_dict->setString(it.name(), it.value());
  std::unique_ptr<Network::Initiator> initiator =
      Network::Initiator::Create().SetType(initiator_type).Build();
  if (initiator_url)
    initiator->SetUrl(initiator_url->spec());
  std::string url_fragment;
  std::string url_without_fragment =
      ExtractFragment(request.url, &url_fragment);
  auto request_object =
      Network::Request::Create()
          .SetUrl(url_without_fragment)
          .SetMethod(request.method)
          .SetHeaders(Object::fromValue(headers_dict.get(), nullptr))
          .SetInitialPriority(resourcePriority(request.priority))
          .SetReferrerPolicy(referrerPolicy(request.referrer_policy))
          .Build();
  if (!url_fragment.empty())
    request_object->SetUrlFragment(url_fragment);
  frontend_->RequestWillBeSent(
      request_id, loader_id, url_without_fragment, std::move(request_object),
      timestamp.since_origin().InSecondsF(), base::Time::Now().ToDoubleT(),
      std::move(initiator), std::unique_ptr<Network::Response>(),
      std::string(Network::ResourceTypeEnum::Other),
      Maybe<std::string>() /* frame_id */, request.has_user_gesture);
}

void NetworkHandler::ResponseReceived(
    const std::string& request_id,
    const std::string& loader_id,
    const GURL& url,
    const char* resource_type,
    const network::mojom::URLResponseHead& head,
    Maybe<std::string> frame_id) {
  if (!enabled_)
    return;
  std::unique_ptr<Network::Response> response(BuildResponse(url, head));
  frontend_->ResponseReceived(
      request_id, loader_id,
      base::TimeTicks::Now().ToInternalValue() /
          static_cast<double>(base::Time::kMicrosecondsPerSecond),
      resource_type, std::move(response), std::move(frame_id));
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
        GetBlockedReasonFor(status));
    return;
  }
  frontend_->LoadingFinished(
      request_id,
      status.completion_time.ToInternalValue() /
          static_cast<double>(base::Time::kMicrosecondsPerSecond),
      status.encoded_data_length);
}

void NetworkHandler::OnSignedExchangeReceived(
    base::Optional<const base::UnguessableToken> devtools_navigation_token,
    const GURL& outer_request_url,
    const network::mojom::URLResponseHead& outer_response,
    const base::Optional<SignedExchangeEnvelope>& envelope,
    const scoped_refptr<net::X509Certificate>& certificate,
    const base::Optional<net::SSLInfo>& ssl_info,
    const std::vector<SignedExchangeError>& errors) {
  if (!enabled_)
    return;
  std::unique_ptr<Network::SignedExchangeInfo> signed_exchange_info =
      Network::SignedExchangeInfo::Create()
          .SetOuterResponse(BuildResponse(outer_request_url, outer_response))
          .Build();

  if (envelope) {
    std::unique_ptr<DictionaryValue> headers_dict(DictionaryValue::create());
    for (const auto& it : envelope->response_headers())
      headers_dict->setString(it.first, it.second);

    const SignedExchangeSignatureHeaderField::Signature& sig =
        envelope->signature();
    auto signatures =
        std::make_unique<protocol::Array<Network::SignedExchangeSignature>>();
    std::unique_ptr<Network::SignedExchangeSignature> signature =
        Network::SignedExchangeSignature::Create()
            .SetLabel(sig.label)
            .SetSignature(base::HexEncode(sig.sig.data(), sig.sig.size()))
            .SetIntegrity(sig.integrity)
            .SetCertUrl(sig.cert_url.spec())
            .SetValidityUrl(sig.validity_url.url.spec())
            .SetDate(sig.date)
            .SetExpires(sig.expires)
            .Build();
    if (sig.cert_sha256) {
      signature->SetCertSha256(base::HexEncode(sig.cert_sha256->data,
                                               sizeof(sig.cert_sha256->data)));
    }
    if (certificate) {
      auto encoded_certificates = std::make_unique<protocol::Array<String>>();
      encoded_certificates->emplace_back();
      base::Base64Encode(
          net::x509_util::CryptoBufferAsStringPiece(certificate->cert_buffer()),
          &encoded_certificates->back());
      for (const auto& cert : certificate->intermediate_buffers()) {
        encoded_certificates->emplace_back();
        base::Base64Encode(
            net::x509_util::CryptoBufferAsStringPiece(cert.get()),
            &encoded_certificates->back());
      }
      signature->SetCertificates(std::move(encoded_certificates));
    }
    signatures->emplace_back(std::move(signature));

    signed_exchange_info->SetHeader(
        Network::SignedExchangeHeader::Create()
            .SetRequestUrl(envelope->request_url().url.spec())
            .SetResponseCode(envelope->response_code())
            .SetResponseHeaders(Object::fromValue(headers_dict.get(), nullptr))
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
    interceptor_patterns.emplace_back(
        pattern->GetUrlPattern("*"), std::move(resource_types),
        ToInterceptorStage(pattern->GetInterceptionStage(
            protocol::Network::InterceptionStageEnum::Request)));
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

  if (raw_response.isJust()) {
    const protocol::Binary& raw = raw_response.fromJust();

    std::string raw_headers;
    size_t header_size = net::HttpUtil::LocateEndOfHeaders(
        reinterpret_cast<const char*>(raw.data()), raw.size());
    if (header_size == std::string::npos) {
      LOG(WARNING) << "Can't find headers in raw response";
      header_size = 0;
    } else {
      raw_headers = net::HttpUtil::AssembleRawHeaders(base::StringPiece(
          reinterpret_cast<const char*>(raw.data()), header_size));
    }
    CHECK_LE(header_size, raw.size());
    response_headers =
        base::MakeRefCounted<net::HttpResponseHeaders>(std::move(raw_headers));
    response_body = raw.bytes();
    body_offset = header_size;
  }

  base::Optional<net::Error> error;
  if (error_reason.isJust()) {
    bool ok;
    error = NetErrorFromString(error_reason.fromJust(), &ok);
    if (!ok) {
      callback->sendFailure(Response::InvalidParams("Invalid errorReason."));
      return;
    }
  }

  std::unique_ptr<DevToolsURLLoaderInterceptor::Modifications::HeadersVector>
      override_headers;
  if (opt_headers.isJust()) {
    std::unique_ptr<protocol::DictionaryValue> headers =
        opt_headers.fromJust()->toValue();
    override_headers = std::make_unique<
        DevToolsURLLoaderInterceptor::Modifications::HeadersVector>();
    for (size_t i = 0; i < headers->size(); ++i) {
      const protocol::DictionaryValue::Entry& entry = headers->at(i);
      std::string value;
      if (!entry.second->asString(&value)) {
        callback->sendFailure(Response::InvalidParams("Invalid header value"));
        return;
      }
      override_headers->emplace_back(entry.first, value);
    }
  }
  using AuthChallengeResponse =
      DevToolsURLLoaderInterceptor::AuthChallengeResponse;
  std::unique_ptr<AuthChallengeResponse> override_auth;
  if (auth_challenge_response.isJust()) {
    std::string type = auth_challenge_response.fromJust()->GetResponse();
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
          base::UTF8ToUTF16(
              auth_challenge_response.fromJust()->GetUsername("")),
          base::UTF8ToUTF16(
              auth_challenge_response.fromJust()->GetPassword("")));
    } else {
      callback->sendFailure(
          Response::InvalidParams("Unrecognized authChallengeResponse."));
      return;
    }
  }

  Maybe<protocol::Binary> post_data_bytes;
  if (post_data.isJust())
    post_data_bytes = protocol::Binary::fromString(post_data.fromJust());

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
    const std::string& cookie_line) {
  std::unique_ptr<DictionaryValue> headers_dict(DictionaryValue::create());
  for (net::HttpRequestHeaders::Iterator it(request.headers); it.GetNext();)
    headers_dict->setString(it.name(), it.value());
  if (request.referrer.is_valid()) {
    headers_dict->setString(net::HttpRequestHeaders::kReferer,
                            request.referrer.spec());
  }
  if (!cookie_line.empty())
    headers_dict->setString(net::HttpRequestHeaders::kCookie, cookie_line);

  std::string url_fragment;
  std::unique_ptr<protocol::Network::Request> request_object =
      Network::Request::Create()
          .SetUrl(ExtractFragment(request.url, &url_fragment))
          .SetMethod(request.method)
          .SetHeaders(Object::fromValue(headers_dict.get(), nullptr))
          .SetInitialPriority(resourcePriority(request.priority))
          .SetReferrerPolicy(referrerPolicy(request.referrer_policy))
          .Build();
  if (!url_fragment.empty())
    request_object->SetUrlFragment(url_fragment);
  if (request.request_body) {
    std::string post_data;
    auto data_entries =
        std::make_unique<protocol::Array<protocol::Network::PostDataEntry>>();
    if (GetPostData(*request.request_body, data_entries.get(), &post_data)) {
      if (!post_data.empty())
        request_object->SetPostData(std::move(post_data));
      if (data_entries->size())
        request_object->SetPostDataEntries(std::move(data_entries));
      request_object->SetHasPostData(true);
    }
  }
  return request_object;
}

bool NetworkHandler::MaybeCreateProxyForInterception(
    RenderProcessHost* rph,
    const base::UnguessableToken& frame_token,
    bool is_navigation,
    bool is_download,
    network::mojom::URLLoaderFactoryOverride* intercepting_factory) {
  return url_loader_interceptor_ &&
         url_loader_interceptor_->CreateProxyForInterception(
             rph, frame_token, is_navigation, is_download,
             intercepting_factory);
}

void NetworkHandler::ApplyOverrides(net::HttpRequestHeaders* headers,
                                    bool* skip_service_worker,
                                    bool* disable_cache) {
  for (auto& entry : extra_headers_)
    headers->SetHeader(entry.first, entry.second);
  *skip_service_worker |= bypass_service_worker_;
  *disable_cache |= cache_disabled_;
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
    response_headers = BuildResponseHeaders(info->response_headers);
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
  }
}
protocol::Network::CrossOriginEmbedderPolicyValue
makeCrossOriginEmbedderPolicyValue(
    network::mojom::CrossOriginEmbedderPolicyValue value) {
  switch (value) {
    case network::mojom::CrossOriginEmbedderPolicyValue::kNone:
      return protocol::Network::CrossOriginEmbedderPolicyValueEnum::None;
    case network::mojom::CrossOriginEmbedderPolicyValue::kRequireCorp:
      return protocol::Network::CrossOriginEmbedderPolicyValueEnum::RequireCorp;
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
makeCrossOriginOpenerEmbedderStatus(
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
}  // namespace

DispatchResponse NetworkHandler::GetSecurityIsolationStatus(
    Maybe<String> frame_id,
    std::unique_ptr<protocol::Network::SecurityIsolationStatus>* out_info) {
  if (!frame_id.isJust() || !host_) {
    // TODO(sigurds): Support for workers.
    return Response::InvalidParams("Currently only frames are supported");
  }
  FrameTreeNode* frame_tree_node = FrameTreeNodeFromDevToolsFrameToken(
      host_->frame_tree_node(), frame_id.takeJust());
  if (!frame_tree_node) {
    return Response::InvalidParams("No frame with given id found");
  }
  RenderFrameHostImpl* rfhi = frame_tree_node->current_frame_host();

  auto coep =
      makeCrossOriginOpenerEmbedderStatus(rfhi->cross_origin_embedder_policy());
  auto coop =
      makeCrossOriginOpenerPolicyStatus(rfhi->cross_origin_opener_policy());

  *out_info = protocol::Network::SecurityIsolationStatus::Create()
                  .SetCoep(std::move(coep))
                  .SetCoop(std::move(coop))
                  .Build();
  return Response::Success();
}

void NetworkHandler::OnRequestWillBeSentExtraInfo(
    const std::string& devtools_request_id,
    const net::CookieAccessResultList& request_cookie_list,
    const std::vector<network::mojom::HttpRawHeaderPairPtr>& request_headers) {
  if (!enabled_)
    return;

  frontend_->RequestWillBeSentExtraInfo(
      devtools_request_id, BuildProtocolAssociatedCookies(request_cookie_list),
      GetRawHeaders(request_headers));
}

void NetworkHandler::OnResponseReceivedExtraInfo(
    const std::string& devtools_request_id,
    const net::CookieAndLineAccessResultList& response_cookie_list,
    const std::vector<network::mojom::HttpRawHeaderPairPtr>& response_headers,
    const base::Optional<std::string>& response_headers_text) {
  if (!enabled_)
    return;

  frontend_->ResponseReceivedExtraInfo(
      devtools_request_id, BuildProtocolBlockedSetCookies(response_cookie_list),
      GetRawHeaders(response_headers),
      response_headers_text.has_value() ? response_headers_text.value()
                                        : Maybe<String>());
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
    std::unique_ptr<protocol::DictionaryValue> headers_object =
        protocol::DictionaryValue::create();
    size_t iterator = 0;
    std::string name;
    std::string value;
    // TODO(chromium:1069378): This probably needs to handle duplicate header
    // names correctly by folding them.
    while (rh->EnumerateHeaderLines(&iterator, &name, &value)) {
      headers_object->setString(name, value);
    }
    protocol::ErrorSupport errors;
    result->SetHeaders(
        protocol::Network::Headers::fromValue(headers_object.get(), &errors));
  }

  callback->sendSuccess(std::move(result));
  loaders_.erase(it);
}

namespace {

mojo::PendingRemote<network::mojom::URLLoaderFactory>
CreateNetworkFactoryForDevTools(
    RenderProcessHost* host,
    network::mojom::URLLoaderFactoryParamsPtr params) {
  if (!host || !params) {
    // Return an invalid remote by default.
    return {};
  }

  // Don't allow trust token redemption.
  params->trust_token_redemption_policy =
      network::mojom::TrustTokenRedemptionPolicy::kForbid;
  // Let DevTools fetch resources without CORS and CORB. Source maps are valid
  // JSON and would otherwise require a CORS fetch + correct response headers.
  // See BUG(chromium:1076435) for more context.
  params->is_corb_enabled = false;

  mojo::PendingRemote<network::mojom::URLLoaderFactory> remote;
  host->CreateURLLoaderFactory(remote.InitWithNewPipeAndPassReceiver(),
                               std::move(params));
  return remote;
}
}  // namespace

void NetworkHandler::LoadNetworkResource(
    const String& frame_id,
    const String& url,
    std::unique_ptr<protocol::Network::LoadNetworkResourceOptions> options,
    std::unique_ptr<LoadNetworkResourceCallback> callback) {
  GURL gurl(url);
  const bool is_gurl_valid = gurl.is_valid() && gurl.SchemeIsHTTPOrHTTPS();
  if (!is_gurl_valid) {
    callback->sendFailure(Response::InvalidParams(
        "The url must be valid and have scheme http or https"));
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
    FrameTreeNode* node =
        FrameTreeNodeFromDevToolsFrameToken(host_->frame_tree_node(), frame_id);
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
        mojo::Clone(frame->last_committed_client_security_state()),
        /**coep_reporter=*/mojo::NullRemote(), frame->GetProcess(),
        network::mojom::TrustTokenRedemptionPolicy::kForbid,
        "NetworkHandler::LoadNetworkResource");

    auto factory =
        CreateNetworkFactoryForDevTools(frame->GetProcess(), std::move(params));
    url_loader_factory.Bind(std::move(factory));
    auto loader = DevToolsNetworkResourceLoader::Create(
        std::move(url_loader_factory), std::move(gurl),
        frame->GetLastCommittedOrigin(), frame->ComputeSiteForCookies(),
        caching, include_credentials, frame->GetRoutingID(),
        std::move(complete_callback));
    loaders_.emplace(std::move(loader), std::move(callback));
    return;
  }
  scoped_refptr<DevToolsAgentHostImpl> host =
      DevToolsAgentHostImpl::GetForId(host_id_);
  if (host) {
    // TODO(sigurds): Support dedicated workers.
    auto info = host->CreateNetworkFactoryParamsForDevTools();
    auto factory = CreateNetworkFactoryForDevTools(
        host->GetProcessHost(), std::move(info.factory_params));
    if (factory.is_valid()) {
      url_loader_factory.Bind(std::move(factory));
      auto loader = DevToolsNetworkResourceLoader::Create(
          std::move(url_loader_factory), std::move(gurl),
          std::move(info.origin), std::move(info.site_for_cookies), caching,
          include_credentials, MSG_ROUTING_NONE, std::move(complete_callback));
      loaders_.emplace(std::move(loader), std::move(callback));
      return;
    }
  }
  callback->sendFailure(Response::ServerError("Target not supported"));
}

}  // namespace protocol
}  // namespace content
