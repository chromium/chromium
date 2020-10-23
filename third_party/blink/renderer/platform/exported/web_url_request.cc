/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/public/platform/web_url_request.h"

#include <memory>

#include "base/time/time.h"
#include "net/base/load_flags.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/optional_trust_token_params.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/public/platform/url_conversion.h"
#include "third_party/blink/public/platform/web_http_body.h"
#include "third_party/blink/public/platform/web_http_header_visitor.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/loader/fetch/trust_token_params_conversion.h"
#include "third_party/blink/renderer/platform/network/encoded_form_data.h"
#include "third_party/blink/renderer/platform/weborigin/referrer.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

using blink::mojom::FetchCacheMode;

namespace blink {

// This is complementary to ConvertNetPriorityToWebKitPriority, defined in
// service_worker_context_client.cc.
net::RequestPriority ConvertWebKitPriorityToNetPriority(
    WebURLRequest::Priority priority) {
  switch (priority) {
    case WebURLRequest::Priority::kVeryHigh:
      return net::HIGHEST;

    case WebURLRequest::Priority::kHigh:
      return net::MEDIUM;

    case WebURLRequest::Priority::kMedium:
      return net::LOW;

    case WebURLRequest::Priority::kLow:
      return net::LOWEST;

    case WebURLRequest::Priority::kVeryLow:
      return net::IDLE;

    case WebURLRequest::Priority::kUnresolved:
    default:
      NOTREACHED();
      return net::LOW;
  }
}

WebURLRequest::ExtraData::ExtraData() : render_frame_id_(MSG_ROUTING_NONE) {}

WebURLRequest::~WebURLRequest() = default;

WebURLRequest::WebURLRequest()
    : owned_resource_request_(std::make_unique<ResourceRequest>()),
      resource_request_(owned_resource_request_.get()) {}

WebURLRequest::WebURLRequest(WebURLRequest&& src) {
  *this = std::move(src);
}

WebURLRequest& WebURLRequest::operator=(WebURLRequest&& src) {
  if (this == &src) {
    return *this;
  }
  if (src.owned_resource_request_) {
    owned_resource_request_ = std::move(src.owned_resource_request_);
    resource_request_ = owned_resource_request_.get();
  } else {
    owned_resource_request_ = std::make_unique<ResourceRequest>();
    resource_request_ = owned_resource_request_.get();
    CopyFrom(src);
  }
  src.resource_request_ = nullptr;
  return *this;
}

WebURLRequest::WebURLRequest(const WebURL& url) : WebURLRequest() {
  SetUrl(url);
}

void WebURLRequest::CopyFrom(const WebURLRequest& r) {
  // Copying subclasses that have different m_resourceRequest ownership
  // semantics via this operator is just not supported.
  DCHECK(owned_resource_request_);
  DCHECK_EQ(owned_resource_request_.get(), resource_request_);
  DCHECK(owned_resource_request_->IsNull());
  DCHECK(this != &r);
  resource_request_->CopyHeadFrom(*r.resource_request_);
  resource_request_->SetHttpBody(r.resource_request_->HttpBody());
}

bool WebURLRequest::IsNull() const {
  return resource_request_->IsNull();
}

WebURL WebURLRequest::Url() const {
  return resource_request_->Url();
}

void WebURLRequest::SetUrl(const WebURL& url) {
  resource_request_->SetUrl(url);
}

const net::SiteForCookies& WebURLRequest::SiteForCookies() const {
  return resource_request_->SiteForCookies();
}

void WebURLRequest::SetSiteForCookies(
    const net::SiteForCookies& site_for_cookies) {
  resource_request_->SetSiteForCookies(site_for_cookies);
}

base::Optional<WebSecurityOrigin> WebURLRequest::TopFrameOrigin() const {
  const SecurityOrigin* origin = resource_request_->TopFrameOrigin();
  return origin ? base::Optional<WebSecurityOrigin>(origin)
                : base::Optional<WebSecurityOrigin>();
}

void WebURLRequest::SetTopFrameOrigin(const WebSecurityOrigin& origin) {
  resource_request_->SetTopFrameOrigin(origin);
}

WebSecurityOrigin WebURLRequest::RequestorOrigin() const {
  return resource_request_->RequestorOrigin();
}

WebSecurityOrigin WebURLRequest::IsolatedWorldOrigin() const {
  return resource_request_->IsolatedWorldOrigin();
}

void WebURLRequest::SetRequestorOrigin(
    const WebSecurityOrigin& requestor_origin) {
  resource_request_->SetRequestorOrigin(requestor_origin);
}

bool WebURLRequest::AllowStoredCredentials() const {
  return resource_request_->AllowStoredCredentials();
}

void WebURLRequest::SetAllowStoredCredentials(bool allow_stored_credentials) {
  resource_request_->SetAllowStoredCredentials(allow_stored_credentials);
}

mojom::FetchCacheMode WebURLRequest::GetCacheMode() const {
  return resource_request_->GetCacheMode();
}

void WebURLRequest::SetCacheMode(mojom::FetchCacheMode cache_mode) {
  resource_request_->SetCacheMode(cache_mode);
}

base::TimeDelta WebURLRequest::TimeoutInterval() const {
  return resource_request_->TimeoutInterval();
}

WebString WebURLRequest::HttpMethod() const {
  return resource_request_->HttpMethod();
}

void WebURLRequest::SetHttpMethod(const WebString& http_method) {
  resource_request_->SetHttpMethod(http_method);
}

WebString WebURLRequest::HttpHeaderField(const WebString& name) const {
  return resource_request_->HttpHeaderField(name);
}

void WebURLRequest::SetHttpHeaderField(const WebString& name,
                                       const WebString& value) {
  CHECK(!EqualIgnoringASCIICase(name, "referer"));
  resource_request_->SetHttpHeaderField(name, value);
}

void WebURLRequest::AddHttpHeaderField(const WebString& name,
                                       const WebString& value) {
  resource_request_->AddHttpHeaderField(name, value);
}

void WebURLRequest::ClearHttpHeaderField(const WebString& name) {
  resource_request_->ClearHttpHeaderField(name);
}

void WebURLRequest::VisitHttpHeaderFields(WebHTTPHeaderVisitor* visitor) const {
  const HTTPHeaderMap& map = resource_request_->HttpHeaderFields();
  for (HTTPHeaderMap::const_iterator it = map.begin(); it != map.end(); ++it)
    visitor->VisitHeader(it->key, it->value);
}

WebHTTPBody WebURLRequest::HttpBody() const {
  return WebHTTPBody(resource_request_->HttpBody());
}

void WebURLRequest::SetHttpBody(const WebHTTPBody& http_body) {
  resource_request_->SetHttpBody(http_body);
}

bool WebURLRequest::ReportUploadProgress() const {
  return resource_request_->ReportUploadProgress();
}

void WebURLRequest::SetReportUploadProgress(bool report_upload_progress) {
  resource_request_->SetReportUploadProgress(report_upload_progress);
}

void WebURLRequest::SetReportRawHeaders(bool report_raw_headers) {
  resource_request_->SetReportRawHeaders(report_raw_headers);
}

bool WebURLRequest::ReportRawHeaders() const {
  return resource_request_->ReportRawHeaders();
}

mojom::blink::RequestContextType WebURLRequest::GetRequestContext() const {
  return resource_request_->GetRequestContext();
}

network::mojom::RequestDestination WebURLRequest::GetRequestDestination()
    const {
  return resource_request_->GetRequestDestination();
}

void WebURLRequest::SetReferrerString(const WebString& referrer) {
  resource_request_->SetReferrerString(referrer);
}

void WebURLRequest::SetReferrerPolicy(
    network::mojom::ReferrerPolicy referrer_policy) {
  resource_request_->SetReferrerPolicy(referrer_policy);
}

WebString WebURLRequest::ReferrerString() const {
  return resource_request_->ReferrerString();
}

network::mojom::ReferrerPolicy WebURLRequest::GetReferrerPolicy() const {
  return resource_request_->GetReferrerPolicy();
}

void WebURLRequest::SetHttpOriginIfNeeded(const WebSecurityOrigin& origin) {
  resource_request_->SetHttpOriginIfNeeded(origin.Get());
}

bool WebURLRequest::HasUserGesture() const {
  return resource_request_->HasUserGesture();
}

bool WebURLRequest::HasTextFragmentToken() const {
  return resource_request_->HasTextFragmentToken();
}

void WebURLRequest::SetHasUserGesture(bool has_user_gesture) {
  resource_request_->SetHasUserGesture(has_user_gesture);
}

void WebURLRequest::SetRequestContext(
    mojom::blink::RequestContextType request_context) {
  resource_request_->SetRequestContext(request_context);
}

void WebURLRequest::SetRequestDestination(
    network::mojom::RequestDestination destination) {
  resource_request_->SetRequestDestination(destination);
}

int WebURLRequest::RequestorID() const {
  return resource_request_->RequestorID();
}

void WebURLRequest::SetRequestorID(int requestor_id) {
  resource_request_->SetRequestorID(requestor_id);
}

bool WebURLRequest::PassResponsePipeToClient() const {
  return resource_request_->DownloadToBlob();
}

bool WebURLRequest::UseStreamOnResponse() const {
  return resource_request_->UseStreamOnResponse();
}

void WebURLRequest::SetUseStreamOnResponse(bool use_stream_on_response) {
  resource_request_->SetUseStreamOnResponse(use_stream_on_response);
}

bool WebURLRequest::GetKeepalive() const {
  return resource_request_->GetKeepalive();
}

void WebURLRequest::SetKeepalive(bool keepalive) {
  resource_request_->SetKeepalive(keepalive);
}

bool WebURLRequest::GetSkipServiceWorker() const {
  return resource_request_->GetSkipServiceWorker();
}

void WebURLRequest::SetSkipServiceWorker(bool skip_service_worker) {
  resource_request_->SetSkipServiceWorker(skip_service_worker);
}

bool WebURLRequest::ShouldResetAppCache() const {
  return resource_request_->ShouldResetAppCache();
}

void WebURLRequest::SetShouldResetAppCache(bool set_should_reset_app_cache) {
  resource_request_->SetShouldResetAppCache(set_should_reset_app_cache);
}

network::mojom::RequestMode WebURLRequest::GetMode() const {
  return resource_request_->GetMode();
}

void WebURLRequest::SetMode(network::mojom::RequestMode mode) {
  return resource_request_->SetMode(mode);
}

network::mojom::CredentialsMode WebURLRequest::GetCredentialsMode() const {
  return resource_request_->GetCredentialsMode();
}

void WebURLRequest::SetCredentialsMode(network::mojom::CredentialsMode mode) {
  return resource_request_->SetCredentialsMode(mode);
}

network::mojom::RedirectMode WebURLRequest::GetRedirectMode() const {
  return resource_request_->GetRedirectMode();
}

void WebURLRequest::SetRedirectMode(network::mojom::RedirectMode redirect) {
  return resource_request_->SetRedirectMode(redirect);
}

WebString WebURLRequest::GetFetchIntegrity() const {
  return resource_request_->GetFetchIntegrity();
}

void WebURLRequest::SetFetchIntegrity(const WebString& integrity) {
  return resource_request_->SetFetchIntegrity(integrity);
}

PreviewsState WebURLRequest::GetPreviewsState() const {
  return resource_request_->GetPreviewsState();
}

void WebURLRequest::SetPreviewsState(PreviewsState previews_state) {
  return resource_request_->SetPreviewsState(previews_state);
}

const scoped_refptr<WebURLRequest::ExtraData>& WebURLRequest::GetExtraData()
    const {
  return resource_request_->GetExtraData();
}

void WebURLRequest::SetExtraData(scoped_refptr<ExtraData> extra_data) {
  resource_request_->SetExtraData(std::move(extra_data));
}

bool WebURLRequest::IsDownloadToNetworkCacheOnly() const {
  return resource_request_->IsDownloadToNetworkCacheOnly();
}

void WebURLRequest::SetDownloadToNetworkCacheOnly(bool download_to_cache_only) {
  resource_request_->SetDownloadToNetworkCacheOnly(download_to_cache_only);
}

ResourceRequest& WebURLRequest::ToMutableResourceRequest() {
  DCHECK(resource_request_);
  return *resource_request_;
}

WebURLRequest::Priority WebURLRequest::GetPriority() const {
  return static_cast<WebURLRequest::Priority>(resource_request_->Priority());
}

void WebURLRequest::SetPriority(WebURLRequest::Priority priority) {
  resource_request_->SetPriority(static_cast<ResourceLoadPriority>(priority));
}

bool WebURLRequest::IsExternalRequest() const {
  return resource_request_->IsExternalRequest();
}

network::mojom::CorsPreflightPolicy WebURLRequest::GetCorsPreflightPolicy()
    const {
  return resource_request_->CorsPreflightPolicy();
}

base::Optional<WebString> WebURLRequest::GetSuggestedFilename() const {
  if (!resource_request_->GetSuggestedFilename().has_value())
    return base::Optional<WebString>();
  return static_cast<WebString>(
      resource_request_->GetSuggestedFilename().value());
}

bool WebURLRequest::IsAdResource() const {
  return resource_request_->IsAdResource();
}

void WebURLRequest::SetUpgradeIfInsecure(bool upgrade_if_insecure) {
  resource_request_->SetUpgradeIfInsecure(upgrade_if_insecure);
}

bool WebURLRequest::UpgradeIfInsecure() const {
  return resource_request_->UpgradeIfInsecure();
}

bool WebURLRequest::SupportsAsyncRevalidation() const {
  return resource_request_->AllowsStaleResponse();
}

bool WebURLRequest::IsRevalidating() const {
  return resource_request_->IsRevalidating();
}

const base::Optional<base::UnguessableToken>& WebURLRequest::GetDevToolsToken()
    const {
  return resource_request_->GetDevToolsToken();
}

const WebString WebURLRequest::GetRequestedWithHeader() const {
  return resource_request_->GetRequestedWithHeader();
}

void WebURLRequest::SetRequestedWithHeader(const WebString& value) {
  resource_request_->SetRequestedWithHeader(value);
}

const WebString WebURLRequest::GetPurposeHeader() const {
  return resource_request_->GetPurposeHeader();
}

const base::UnguessableToken& WebURLRequest::GetFetchWindowId() const {
  return resource_request_->GetFetchWindowId();
}
void WebURLRequest::SetFetchWindowId(const base::UnguessableToken& id) {
  resource_request_->SetFetchWindowId(id);
}

int WebURLRequest::GetLoadFlagsForWebUrlRequest() const {
  int load_flags = net::LOAD_NORMAL;

  switch (resource_request_->GetCacheMode()) {
    case FetchCacheMode::kNoStore:
      load_flags |= net::LOAD_DISABLE_CACHE;
      break;
    case FetchCacheMode::kValidateCache:
      load_flags |= net::LOAD_VALIDATE_CACHE;
      break;
    case FetchCacheMode::kBypassCache:
      load_flags |= net::LOAD_BYPASS_CACHE;
      break;
    case FetchCacheMode::kForceCache:
      load_flags |= net::LOAD_SKIP_CACHE_VALIDATION;
      break;
    case FetchCacheMode::kOnlyIfCached:
      load_flags |= net::LOAD_ONLY_FROM_CACHE | net::LOAD_SKIP_CACHE_VALIDATION;
      break;
    case FetchCacheMode::kUnspecifiedOnlyIfCachedStrict:
      load_flags |= net::LOAD_ONLY_FROM_CACHE;
      break;
    case FetchCacheMode::kDefault:
      break;
    case FetchCacheMode::kUnspecifiedForceCacheMiss:
      load_flags |= net::LOAD_ONLY_FROM_CACHE | net::LOAD_BYPASS_CACHE;
      break;
  }

  if (resource_request_->GetRequestContext() ==
      blink::mojom::blink::RequestContextType::PREFETCH)
    load_flags |= net::LOAD_PREFETCH;

  if (resource_request_->GetExtraData()) {
    if (resource_request_->GetExtraData()->is_for_no_state_prefetch())
      load_flags |= net::LOAD_PREFETCH;
  }
  if (resource_request_->AllowsStaleResponse()) {
    load_flags |= net::LOAD_SUPPORT_ASYNC_REVALIDATION;
  }
  if (resource_request_->PrefetchMaybeForTopLeveNavigation()) {
    DCHECK_EQ(resource_request_->GetRequestContext(),
              blink::mojom::blink::RequestContextType::PREFETCH);
    if (!resource_request_->RequestorOrigin()->IsSameOriginWith(
            SecurityOrigin::Create(resource_request_->Url()).get())) {
      load_flags |= net::LOAD_RESTRICTED_PREFETCH;
    }
  }

  return load_flags;
}

const ResourceRequest& WebURLRequest::ToResourceRequest() const {
  DCHECK(resource_request_);
  return *resource_request_;
}

base::Optional<WebString> WebURLRequest::GetDevToolsId() const {
  return resource_request_->GetDevToolsId();
}

bool WebURLRequest::IsFromOriginDirtyStyleSheet() const {
  return resource_request_->IsFromOriginDirtyStyleSheet();
}

bool WebURLRequest::IsSignedExchangePrefetchCacheEnabled() const {
  return resource_request_->IsSignedExchangePrefetchCacheEnabled();
}

base::Optional<base::UnguessableToken> WebURLRequest::RecursivePrefetchToken()
    const {
  return resource_request_->RecursivePrefetchToken();
}

network::OptionalTrustTokenParams WebURLRequest::TrustTokenParams() const {
  return ConvertTrustTokenParams(resource_request_->TrustTokenParams());
}

WebURLRequest::WebURLRequest(ResourceRequest& r) : resource_request_(&r) {}

}  // namespace blink
