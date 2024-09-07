/*
 * Copyright (C) 2003, 2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2009, 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"

#include <memory>

#include "base/unguessable_token.h"
#include "net/base/request_priority.h"
#include "services/network/public/mojom/ip_address_space.mojom-blink.h"
#include "services/network/public/mojom/web_bundle_handle.mojom-blink.h"
#include "third_party/blink/public/common/permissions_policy/permissions_policy.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom-blink.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/renderer/platform/network/encoded_form_data.h"
#include "third_party/blink/renderer/platform/network/http_names.h"
#include "third_party/blink/renderer/platform/network/network_utils.h"
#include "third_party/blink/renderer/platform/weborigin/referrer.h"

namespace blink {

ResourceRequestHead::WebBundleTokenParams&
ResourceRequestHead::WebBundleTokenParams::operator=(
    const WebBundleTokenParams& other) {
  bundle_url = other.bundle_url;
  token = other.token;
  handle = other.CloneHandle();
  return *this;
}

ResourceRequestHead::WebBundleTokenParams::WebBundleTokenParams(
    const WebBundleTokenParams& other) {
  *this = other;
}

ResourceRequestHead::WebBundleTokenParams::WebBundleTokenParams(
    const KURL& bundle_url,
    const base::UnguessableToken& web_bundle_token,
    mojo::PendingRemote<network::mojom::blink::WebBundleHandle>
        web_bundle_handle)
    : bundle_url(bundle_url),
      token(web_bundle_token),
      handle(std::move(web_bundle_handle)) {}

mojo::PendingRemote<network::mojom::blink::WebBundleHandle>
ResourceRequestHead::WebBundleTokenParams::CloneHandle() const {
  if (!handle)
    return mojo::NullRemote();
  mojo::Remote<network::mojom::blink::WebBundleHandle> remote(std::move(
      const_cast<mojo::PendingRemote<network::mojom::blink::WebBundleHandle>&>(
          handle)));
  mojo::PendingRemote<network::mojom::blink::WebBundleHandle> new_remote;
  remote->Clone(new_remote.InitWithNewPipeAndPassReceiver());
  const_cast<mojo::PendingRemote<network::mojom::blink::WebBundleHandle>&>(
      handle) = remote.Unbind();
  return new_remote;
}

const base::TimeDelta ResourceRequestHead::default_timeout_interval_ =
    base::TimeDelta::Max();

ResourceRequestHead::ResourceRequestHead() : ResourceRequestHead(NullURL()) {}

ResourceRequestHead::ResourceRequestHead(const KURL& url)
    : url_(url),
      timeout_interval_(default_timeout_interval_),
      http_method_(http_names::kGET),
      report_upload_progress_(false),
      has_user_gesture_(false),
      has_text_fragment_token_(false),
      download_to_blob_(false),
      use_stream_on_response_(false),
      keepalive_(false),
      browsing_topics_(false),
      ad_auction_headers_(false),
      shared_storage_writable_opted_in_(false),
      shared_storage_writable_eligible_(false),
      allow_stale_response_(false),
      skip_service_worker_(false),
      download_to_cache_only_(false),
      site_for_cookies_set_(false),
      is_form_submission_(false),
      priority_incremental_(net::kDefaultPriorityIncremental),
      is_ad_resource_(false),
      upgrade_if_insecure_(false),
      is_revalidating_(false),
      is_automatic_upgrade_(false),
      is_from_origin_dirty_style_sheet_(false),
      is_fetch_like_api_(false),
      is_fetch_later_api_(false),
      is_favicon_(false),
      prefetch_maybe_for_top_level_navigation_(false),
      shared_dictionary_writer_enabled_(false),
      requires_upgrade_for_loader_(false),
      cache_mode_(mojom::blink::FetchCacheMode::kDefault),
      initial_priority_(ResourceLoadPriority::kUnresolved),
      priority_(ResourceLoadPriority::kUnresolved),
      intra_priority_value_(0),
      request_context_(mojom::blink::RequestContextType::UNSPECIFIED),
      destination_(network::mojom::RequestDestination::kEmpty),
      mode_(network::mojom::RequestMode::kNoCors),
      fetch_priority_hint_(mojom::blink::FetchPriorityHint::kAuto),
      credentials_mode_(network::mojom::CredentialsMode::kInclude),
      redirect_mode_(network::mojom::RedirectMode::kFollow),
      referrer_string_(Referrer::ClientReferrerString()),
      referrer_policy_(network::mojom::ReferrerPolicy::kDefault),
      cors_preflight_policy_(
          network::mojom::CorsPreflightPolicy::kConsiderPreflight),
      target_address_space_(network::mojom::IPAddressSpace::kUnknown) {}

ResourceRequestHead::ResourceRequestHead(const ResourceRequestHead&) = default;

ResourceRequestHead& ResourceRequestHead::operator=(
    const ResourceRequestHead&) = default;

ResourceRequestHead::ResourceRequestHead(ResourceRequestHead&&) = default;

ResourceRequestHead& ResourceRequestHead::operator=(ResourceRequestHead&&) =
    default;

ResourceRequestHead::~ResourceRequestHead() = default;

ResourceRequestBody::ResourceRequestBody() : ResourceRequestBody(nullptr) {}

ResourceRequestBody::ResourceRequestBody(
    scoped_refptr<EncodedFormData> form_body)
    : form_body_(form_body) {}

ResourceRequestBody::ResourceRequestBody(
    mojo::PendingRemote<network::mojom::blink::ChunkedDataPipeGetter>
        stream_body)
    : stream_body_(std::move(stream_body)) {}

ResourceRequestBody::ResourceRequestBody(ResourceRequestBody&& src)
    : form_body_(std::move(src.form_body_)),
      stream_body_(std::move(src.stream_body_)) {}

ResourceRequestBody& ResourceRequestBody::operator=(ResourceRequestBody&& src) =
    default;

ResourceRequestBody::~ResourceRequestBody() = default;

void ResourceRequestBody::SetStreamBody(
    mojo::PendingRemote<network::mojom::blink::ChunkedDataPipeGetter>
        stream_body) {
  stream_body_ = std::move(stream_body);
}

ResourceRequest::ResourceRequest() : ResourceRequestHead(NullURL()) {}

ResourceRequest::ResourceRequest(const String& url_string)
    : ResourceRequestHead(KURL(url_string)) {}

ResourceRequest::ResourceRequest(const KURL& url) : ResourceRequestHead(url) {}

ResourceRequest::ResourceRequest(const ResourceRequestHead& head)
    : ResourceRequestHead(head) {}

ResourceRequest::ResourceRequest(ResourceRequest&&) = default;

ResourceRequest& ResourceRequest::operator=(ResourceRequest&&) = default;

ResourceRequest::~ResourceRequest() = default;

void ResourceRequest::CopyHeadFrom(const ResourceRequestHead& src) {
  this->ResourceRequestHead::operator=(src);
}

std::unique_ptr<ResourceRequest> ResourceRequestHead::CreateRedirectRequest(
    const KURL& new_url,
    const AtomicString& new_method,
    const net::SiteForCookies& new_site_for_cookies,
    const String& new_referrer,
    network::mojom::ReferrerPolicy new_referrer_policy,
    bool skip_service_worker) const {
  std::unique_ptr<ResourceRequest> request =
      std::make_unique<ResourceRequest>(new_url);
  request->SetRequestorOrigin(RequestorOrigin());
  request->SetIsolatedWorldOrigin(IsolatedWorldOrigin());
  request->SetHttpMethod(new_method);
  request->SetSiteForCookies(new_site_for_cookies);
  String referrer =
      new_referrer.empty() ? Referrer::NoReferrer() : String(new_referrer);
  request->SetReferrerString(referrer);
  request->SetReferrerPolicy(new_referrer_policy);
  request->SetSkipServiceWorker(skip_service_worker);
  request->redirect_info_ = RedirectInfo(
      redirect_info_ ? redirect_info_->original_url : Url(), Url());

  // Copy from parameters for |this|.
  request->SetDownloadToBlob(DownloadToBlob());
  request->SetUseStreamOnResponse(UseStreamOnResponse());
  request->SetRequestContext(GetRequestContext());
  request->SetMode(GetMode());
  request->SetTargetAddressSpace(GetTargetAddressSpace());
  request->SetCredentialsMode(GetCredentialsMode());
  request->SetKeepalive(GetKeepalive());
  request->SetBrowsingTopics(GetBrowsingTopics());
  request->SetAdAuctionHeaders(GetAdAuctionHeaders());
  request->SetSharedStorageWritableOptedIn(GetSharedStorageWritableOptedIn());
  request->SetPriority(Priority());
  request->SetPriorityIncremental(PriorityIncremental());

  request->SetCorsPreflightPolicy(CorsPreflightPolicy());
  if (IsAdResource())
    request->SetIsAdResource();
  request->SetUpgradeIfInsecure(UpgradeIfInsecure());
  request->SetIsAutomaticUpgrade(IsAutomaticUpgrade());
  request->SetRequestedWithHeader(GetRequestedWithHeader());
  request->SetClientDataHeader(GetClientDataHeader());
  request->SetPurposeHeader(GetPurposeHeader());
  request->SetUkmSourceId(GetUkmSourceId());
  request->SetInspectorId(InspectorId());
  request->SetFromOriginDirtyStyleSheet(IsFromOriginDirtyStyleSheet());
  request->SetRecursivePrefetchToken(RecursivePrefetchToken());
  request->SetFetchLikeAPI(IsFetchLikeAPI());
  request->SetFetchLaterAPI(IsFetchLaterAPI());
  request->SetFavicon(IsFavicon());
  request->SetAttributionReportingSupport(GetAttributionReportingSupport());
  request->SetAttributionReportingEligibility(
      GetAttributionReportingEligibility());
  request->SetAttributionReportingSrcToken(GetAttributionSrcToken());

  return request;
}

bool ResourceRequestHead::IsNull() const {
  return url_.IsNull();
}

const KURL& ResourceRequestHead::Url() const {
  return url_;
}

void ResourceRequestHead::SetUrl(const KURL& url) {
  // Loading consists of a number of phases. After cache lookup the url should
  // not change (otherwise checks would not be valid). This DCHECK verifies
  // that.
#if DCHECK_IS_ON()
  DCHECK(is_set_url_allowed_);
#endif
  url_ = url;
}

void ResourceRequestHead::RemoveUserAndPassFromURL() {
  if (url_.User().empty() && url_.Pass().empty())
    return;

  url_.SetUser(String());
  url_.SetPass(String());
}

mojom::blink::FetchCacheMode ResourceRequestHead::GetCacheMode() const {
  return cache_mode_;
}

void ResourceRequestHead::SetCacheMode(
    mojom::blink::FetchCacheMode cache_mode) {
  cache_mode_ = cache_mode;
}

base::TimeDelta ResourceRequestHead::TimeoutInterval() const {
  return timeout_interval_;
}

void ResourceRequestHead::SetTimeoutInterval(
    base::TimeDelta timout_interval_seconds) {
  timeout_interval_ = timout_interval_seconds;
}

const net::SiteForCookies& ResourceRequestHead::SiteForCookies() const {
  return site_for_cookies_;
}

void ResourceRequestHead::SetSiteForCookies(
    const net::SiteForCookies& site_for_cookies) {
  site_for_cookies_ = site_for_cookies;
  site_for_cookies_set_ = true;
}

const SecurityOrigin* ResourceRequestHead::TopFrameOrigin() const {
  return top_frame_origin_.get();
}

void ResourceRequestHead::SetTopFrameOrigin(
    scoped_refptr<const SecurityOrigin> origin) {
  top_frame_origin_ = std::move(origin);
}

const AtomicString& ResourceRequestHead::HttpMethod() const {
  return http_method_;
}

void ResourceRequestHead::SetHttpMethod(const AtomicString& http_method) {
  http_method_ = http_method;
}

const HTTPHeaderMap& ResourceRequestHead::HttpHeaderFields() const {
  return http_header_fields_;
}

const AtomicString& ResourceRequestHead::HttpHeaderField(
    const AtomicString& name) const {
  return http_header_fields_.Get(name);
}

void ResourceRequestHead::SetHttpHeaderField(const AtomicString& name,
                                             const AtomicString& value) {
  http_header_fields_.Set(name, value);
}

void ResourceRequestHead::SetHTTPOrigin(const SecurityOrigin* origin) {
  SetHttpHeaderField(http_names::kOrigin, origin->ToAtomicString());
}

void ResourceRequestHead::ClearHTTPOrigin() {
  http_header_fields_.Remove(http_names::kOrigin);
}

void ResourceRequestHead::SetHttpOriginIfNeeded(const SecurityOrigin* origin) {
  if (NeedsHTTPOrigin())
    SetHTTPOrigin(origin);
}

void ResourceRequestHead::SetHTTPOriginToMatchReferrerIfNeeded() {
  if (NeedsHTTPOrigin()) {
    SetHTTPOrigin(SecurityOrigin::CreateFromString(ReferrerString()).get());
  }
}

void ResourceRequestHead::ClearHTTPUserAgent() {
  http_header_fields_.Remove(http_names::kUserAgent);
}

void ResourceRequestBody::SetFormBody(
    scoped_refptr<EncodedFormData> form_body) {
  form_body_ = std::move(form_body);
}

const scoped_refptr<EncodedFormData>& ResourceRequest::HttpBody() const {
  return body_.FormBody();
}

void ResourceRequest::SetHttpBody(scoped_refptr<EncodedFormData> http_body) {
  body_.SetFormBody(std::move(http_body));
}

ResourceLoadPriority ResourceRequestHead::InitialPriority() const {
  return initial_priority_;
}

ResourceLoadPriority ResourceRequestHead::Priority() const {
  return priority_;
}

int ResourceRequestHead::IntraPriorityValue() const {
  return intra_priority_value_;
}

bool ResourceRequestHead::PriorityHasBeenSet() const {
  return priority_ != ResourceLoadPriority::kUnresolved;
}

void ResourceRequestHead::SetPriority(ResourceLoadPriority priority,
                                      int intra_priority_value) {
  if (!PriorityHasBeenSet())
    initial_priority_ = priority;
  priority_ = priority;
  intra_priority_value_ = intra_priority_value;
}

bool ResourceRequestHead::PriorityIncremental() const {
  return priority_incremental_;
}

void ResourceRequestHead::SetPriorityIncremental(bool priority_incremental) {
  priority_incremental_ = priority_incremental;
}

void ResourceRequestHead::AddHttpHeaderField(const AtomicString& name,
                                             const AtomicString& value) {
  HTTPHeaderMap::AddResult result = http_header_fields_.Add(name, value);
  if (!result.is_new_entry)
    result.stored_value->value = result.stored_value->value + ", " + value;
}

void ResourceRequestHead::AddHTTPHeaderFields(
    const HTTPHeaderMap& header_fields) {
  HTTPHeaderMap::const_iterator end = header_fields.end();
  for (HTTPHeaderMap::const_iterator it = header_fields.begin(); it != end;
       ++it)
    AddHttpHeaderField(it->key, it->value);
}

void ResourceRequestHead::ClearHttpHeaderField(const AtomicString& name) {
  http_header_fields_.Remove(name);
}

bool ResourceRequestHead::IsConditional() const {
  return (http_header_fields_.Contains(http_names::kIfMatch) ||
          http_header_fields_.Contains(http_names::kIfModifiedSince) ||
          http_header_fields_.Contains(http_names::kIfNoneMatch) ||
          http_header_fields_.Contains(http_names::kIfRange) ||
          http_header_fields_.Contains(http_names::kIfUnmodifiedSince));
}

void ResourceRequestHead::SetHasUserGesture(bool has_user_gesture) {
  has_user_gesture_ |= has_user_gesture;
}

void ResourceRequestHead::SetHasTextFragmentToken(
    bool has_text_fragment_token) {
  has_text_fragment_token_ = has_text_fragment_token;
}

bool ResourceRequestHead::CanDisplay(const KURL& url) const {
  if (RequestorOrigin()->CanDisplay(url))
    return true;

  if (IsolatedWorldOrigin() && IsolatedWorldOrigin()->CanDisplay(url))
    return true;

  return false;
}

const CacheControlHeader& ResourceRequestHead::GetCacheControlHeader() const {
  if (!cache_control_header_cache_.parsed) {
    cache_control_header_cache_ = ParseCacheControlDirectives(
        http_header_fields_.Get(http_names::kCacheControl),
        http_header_fields_.Get(http_names::kPragma));
  }
  return cache_control_header_cache_;
}

bool ResourceRequestHead::CacheControlContainsNoCache() const {
  return GetCacheControlHeader().contains_no_cache;
}

bool ResourceRequestHead::CacheControlContainsNoStore() const {
  return GetCacheControlHeader().contains_no_store;
}

bool ResourceRequestHead::HasCacheValidatorFields() const {
  return !http_header_fields_.Get(http_names::kLastModified).empty() ||
         !http_header_fields_.Get(http_names::kETag).empty();
}

bool ResourceRequestHead::NeedsHTTPOrigin() const {
  if (!HttpOrigin().empty())
    return false;  // Request already has an Origin header.

  // Don't send an Origin header for GET or HEAD to avoid privacy issues.
  // For example, if an intranet page has a hyperlink to an external web
  // site, we don't want to include the Origin of the request because it
  // will leak the internal host name. Similar privacy concerns have lead
  // to the widespread suppression of the Referer header at the network
  // layer.
  if (HttpMethod() == http_names::kGET || HttpMethod() == http_names::kHEAD)
    return false;

  // For non-GET and non-HEAD methods, always send an Origin header so the
  // server knows we support this feature.
  return true;
}

bool ResourceRequest::IsFeatureEnabledForSubresourceRequestAssumingOptIn(
    const PermissionsPolicy* policy,
    mojom::blink::PermissionsPolicyFeature feature,
    const url::Origin& origin) {
  if (!policy) {
    return false;
  }

  bool browsing_topics_opted_in =
      (feature == mojom::blink::PermissionsPolicyFeature::kBrowsingTopics ||
       feature == mojom::blink::PermissionsPolicyFeature::
                      kBrowsingTopicsBackwardCompatible) &&
      GetBrowsingTopics();
  bool shared_storage_opted_in =
      feature == mojom::blink::PermissionsPolicyFeature::kSharedStorage &&
      GetSharedStorageWritableOptedIn();

  if (!browsing_topics_opted_in && !shared_storage_opted_in) {
    return false;
  }

  return policy->IsFeatureEnabledForSubresourceRequestAssumingOptIn(feature,
                                                                    origin);
}

}  // namespace blink
