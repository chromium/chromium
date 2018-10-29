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
#include "third_party/blink/public/platform/web_http_body.h"
#include "third_party/blink/public/platform/web_http_header_visitor.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/network/encoded_form_data.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/allocator.h"
#include "third_party/blink/renderer/platform/wtf/noncopyable.h"

namespace blink {

// The purpose of this struct is to permit allocating a ResourceRequest on the
// heap, which is otherwise disallowed by DISALLOW_NEW annotation on
// ResourceRequest.
// TODO(keishi): Replace with GCWrapper<ResourceRequest>
struct WebURLRequest::ResourceRequestContainer {
  ResourceRequestContainer() = default;
  explicit ResourceRequestContainer(const ResourceRequest& r)
      : resource_request(r) {}

  ResourceRequest resource_request;
};

WebURLRequest::~WebURLRequest() = default;

WebURLRequest::WebURLRequest()
    : owned_resource_request_(new ResourceRequestContainer()),
      resource_request_(&owned_resource_request_->resource_request) {}

WebURLRequest::WebURLRequest(const WebURLRequest& r)
    : owned_resource_request_(
          new ResourceRequestContainer(*r.resource_request_)),
      resource_request_(&owned_resource_request_->resource_request) {}

WebURLRequest::WebURLRequest(const WebURL& url) : WebURLRequest() {
  SetURL(url);
}

WebURLRequest& WebURLRequest::operator=(const WebURLRequest& r) {
  // Copying subclasses that have different m_resourceRequest ownership
  // semantics via this operator is just not supported.
  DCHECK(owned_resource_request_);
  DCHECK(resource_request_);
  if (&r != this)
    *resource_request_ = *r.resource_request_;
  return *this;
}

bool WebURLRequest::IsNull() const {
  return resource_request_->IsNull();
}

WebURL WebURLRequest::Url() const {
  return resource_request_->Url();
}

void WebURLRequest::SetURL(const WebURL& url) {
  resource_request_->SetURL(url);
}

WebURL WebURLRequest::SiteForCookies() const {
  return resource_request_->SiteForCookies();
}

void WebURLRequest::SetSiteForCookies(const WebURL& site_for_cookies) {
  resource_request_->SetSiteForCookies(site_for_cookies);
}

WebSecurityOrigin WebURLRequest::RequestorOrigin() const {
  return resource_request_->RequestorOrigin();
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

void WebURLRequest::SetHTTPMethod(const WebString& http_method) {
  resource_request_->SetHTTPMethod(http_method);
}

WebString WebURLRequest::HttpHeaderField(const WebString& name) const {
  return resource_request_->HttpHeaderField(name);
}

void WebURLRequest::SetHTTPHeaderField(const WebString& name,
                                       const WebString& value) {
  CHECK(!DeprecatedEqualIgnoringCase(name, "referer"));
  resource_request_->SetHTTPHeaderField(name, value);
}

void WebURLRequest::SetHTTPReferrer(
    const WebString& web_referrer,
    network::mojom::ReferrerPolicy referrer_policy) {
  // WebString doesn't have the distinction between empty and null. We use
  // the null WTFString for referrer.
  DCHECK_EQ(Referrer::NoReferrer(), String());
  String referrer =
      web_referrer.IsEmpty() ? Referrer::NoReferrer() : String(web_referrer);
  // TODO(domfarolino): Stop storing ResourceRequest's generated referrer as a
  // header and instead use a separate member. See https://crbug.com/850813.
  resource_request_->SetHTTPReferrer(
      Referrer(referrer, static_cast<ReferrerPolicy>(referrer_policy)));
}

void WebURLRequest::AddHTTPHeaderField(const WebString& name,
                                       const WebString& value) {
  resource_request_->AddHTTPHeaderField(name, value);
}

void WebURLRequest::ClearHTTPHeaderField(const WebString& name) {
  resource_request_->ClearHTTPHeaderField(name);
}

void WebURLRequest::VisitHTTPHeaderFields(WebHTTPHeaderVisitor* visitor) const {
  const HTTPHeaderMap& map = resource_request_->HttpHeaderFields();
  for (HTTPHeaderMap::const_iterator it = map.begin(); it != map.end(); ++it)
    visitor->VisitHeader(it->key, it->value);
}

WebHTTPBody WebURLRequest::HttpBody() const {
  return WebHTTPBody(resource_request_->HttpBody());
}

void WebURLRequest::SetHTTPBody(const WebHTTPBody& http_body) {
  resource_request_->SetHTTPBody(http_body);
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

mojom::RequestContextType WebURLRequest::GetRequestContext() const {
  return resource_request_->GetRequestContext();
}

network::mojom::RequestContextFrameType WebURLRequest::GetFrameType() const {
  return resource_request_->GetFrameType();
}

network::mojom::ReferrerPolicy WebURLRequest::GetReferrerPolicy() const {
  return static_cast<network::mojom::ReferrerPolicy>(
      resource_request_->GetReferrerPolicy());
}

void WebURLRequest::SetHTTPOriginIfNeeded(const WebSecurityOrigin& origin) {
  resource_request_->SetHTTPOriginIfNeeded(origin.Get());
}

bool WebURLRequest::HasUserGesture() const {
  return resource_request_->HasUserGesture();
}

void WebURLRequest::SetHasUserGesture(bool has_user_gesture) {
  resource_request_->SetHasUserGesture(has_user_gesture);
}

void WebURLRequest::SetRequestContext(
    mojom::RequestContextType request_context) {
  resource_request_->SetRequestContext(request_context);
}

void WebURLRequest::SetFrameType(
    network::mojom::RequestContextFrameType frame_type) {
  resource_request_->SetFrameType(frame_type);
}

int WebURLRequest::RequestorID() const {
  return resource_request_->RequestorID();
}

void WebURLRequest::SetRequestorID(int requestor_id) {
  resource_request_->SetRequestorID(requestor_id);
}

int WebURLRequest::GetPluginChildID() const {
  return resource_request_->GetPluginChildID();
}

void WebURLRequest::SetPluginChildID(int plugin_child_id) {
  resource_request_->SetPluginChildID(plugin_child_id);
}

int WebURLRequest::AppCacheHostID() const {
  return resource_request_->AppCacheHostID();
}

void WebURLRequest::SetAppCacheHostID(int app_cache_host_id) {
  resource_request_->SetAppCacheHostID(app_cache_host_id);
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

network::mojom::FetchRequestMode WebURLRequest::GetFetchRequestMode() const {
  return resource_request_->GetFetchRequestMode();
}

void WebURLRequest::SetFetchRequestMode(network::mojom::FetchRequestMode mode) {
  return resource_request_->SetFetchRequestMode(mode);
}

network::mojom::FetchCredentialsMode WebURLRequest::GetFetchCredentialsMode()
    const {
  return resource_request_->GetFetchCredentialsMode();
}

void WebURLRequest::SetFetchCredentialsMode(
    network::mojom::FetchCredentialsMode mode) {
  return resource_request_->SetFetchCredentialsMode(mode);
}

network::mojom::FetchRedirectMode WebURLRequest::GetFetchRedirectMode() const {
  return resource_request_->GetFetchRedirectMode();
}

void WebURLRequest::SetFetchRedirectMode(
    network::mojom::FetchRedirectMode redirect) {
  return resource_request_->SetFetchRedirectMode(redirect);
}

WebString WebURLRequest::GetFetchIntegrity() const {
  return resource_request_->GetFetchIntegrity();
}

void WebURLRequest::SetFetchIntegrity(const WebString& integrity) {
  return resource_request_->SetFetchIntegrity(integrity);
}

WebURLRequest::PreviewsState WebURLRequest::GetPreviewsState() const {
  return resource_request_->GetPreviewsState();
}

void WebURLRequest::SetPreviewsState(
    WebURLRequest::PreviewsState previews_state) {
  return resource_request_->SetPreviewsState(previews_state);
}

WebURLRequest::ExtraData* WebURLRequest::GetExtraData() const {
  return resource_request_->GetExtraData();
}

void WebURLRequest::SetExtraData(std::unique_ptr<ExtraData> extra_data) {
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

bool WebURLRequest::WasDiscarded() const {
  return resource_request_->WasDiscarded();
}
void WebURLRequest::SetWasDiscarded(bool was_discarded) {
  resource_request_->SetWasDiscarded(was_discarded);
}

bool WebURLRequest::IsExternalRequest() const {
  return resource_request_->IsExternalRequest();
}

network::mojom::CORSPreflightPolicy WebURLRequest::GetCORSPreflightPolicy()
    const {
  return resource_request_->CORSPreflightPolicy();
}

void WebURLRequest::SetNavigationStartTime(
    base::TimeTicks navigation_start_seconds) {
  resource_request_->SetNavigationStartTime(navigation_start_seconds);
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

const WebContentSecurityPolicyList& WebURLRequest::GetInitiatorCSP() const {
  return resource_request_->GetInitiatorCSP();
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

const WebString WebURLRequest::GetOriginPolicy() const {
  return resource_request_->GetOriginPolicy();
}

void WebURLRequest::SetOriginPolicy(const WebString& policy) {
  resource_request_->SetOriginPolicy(policy);
}

const WebString WebURLRequest::GetRequestedWith() const {
  return resource_request_->GetRequestedWith();
}

void WebURLRequest::SetRequestedWith(const WebString& value) {
  resource_request_->SetRequestedWith(value);
}

const ResourceRequest& WebURLRequest::ToResourceRequest() const {
  DCHECK(resource_request_);
  return *resource_request_;
}

WebURLRequest::WebURLRequest(ResourceRequest& r) : resource_request_(&r) {}

}  // namespace blink
