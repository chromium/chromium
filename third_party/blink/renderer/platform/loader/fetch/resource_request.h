/*
 * Copyright (C) 2003, 2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2006 Samuel Weinig <sam.weinig@gmail.com>
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

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_RESOURCE_REQUEST_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_RESOURCE_REQUEST_H_

#include <memory>

#include "base/macros.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/network/public/mojom/cors.mojom-blink-forward.h"
#include "services/network/public/mojom/fetch_api.mojom-blink-forward.h"
#include "services/network/public/mojom/ip_address_space.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink-forward.h"
#include "third_party/blink/public/platform/resource_request_blocked_reason.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_load_priority.h"
#include "third_party/blink/renderer/platform/network/http_header_map.h"
#include "third_party/blink/renderer/platform/network/http_names.h"
#include "third_party/blink/renderer/platform/network/http_parsers.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/ref_counted.h"

namespace blink {

class EncodedFormData;
struct Referrer;

// A ResourceRequest is a "request" object for ResourceLoader. Conceptually
// it is https://fetch.spec.whatwg.org/#concept-request, but it contains
// a lot of blink specific fields. WebURLRequest is the "public version"
// of this class and WebURLLoader needs it. See WebURLRequest and
// WrappedResourceRequest.
//
// This class is thread-bound. Do not copy/pass an instance across threads.
class PLATFORM_EXPORT ResourceRequest final {
  USING_FAST_MALLOC(ResourceRequest);

 public:
  enum class RedirectStatus : uint8_t { kFollowedRedirect, kNoRedirect };
  ResourceRequest();
  explicit ResourceRequest(const String& url_string);
  explicit ResourceRequest(const KURL&);

  // TODO(toyoshim): Use std::unique_ptr as much as possible, and hopefully
  // make ResourceRequest DISALLOW_COPY_AND_ASSIGN. See crbug.com/787704.
  ResourceRequest(const ResourceRequest&);
  ResourceRequest& operator=(const ResourceRequest&);

  ~ResourceRequest();

  // Constructs a new ResourceRequest for a redirect from this instance.
  std::unique_ptr<ResourceRequest> CreateRedirectRequest(
      const KURL& new_url,
      const AtomicString& new_method,
      const KURL& new_site_for_cookies,
      const String& new_referrer,
      network::mojom::ReferrerPolicy new_referrer_policy,
      bool skip_service_worker) const;

  bool IsNull() const;

  const KURL& Url() const;
  void SetUrl(const KURL&);

  // ThreadableLoader sometimes breaks redirect chains into separate Resource
  // and ResourceRequests. The ResourceTiming API needs the initial URL for the
  // name attribute of PerformanceResourceTiming entries. This property
  // remembers the initial URL for that purpose. Note that it can return a null
  // URL. In that case, use Url() instead.
  const KURL& GetInitialUrlForResourceTiming() const;
  void SetInitialUrlForResourceTiming(const KURL&);

  void RemoveUserAndPassFromURL();

  mojom::FetchCacheMode GetCacheMode() const;
  void SetCacheMode(mojom::FetchCacheMode);

  base::TimeDelta TimeoutInterval() const;
  void SetTimeoutInterval(base::TimeDelta);

  const KURL& SiteForCookies() const;
  void SetSiteForCookies(const KURL&);

  const SecurityOrigin* TopFrameOrigin() const;
  void SetTopFrameOrigin(scoped_refptr<const SecurityOrigin>);

  // The origin of the request, specified at
  // https://fetch.spec.whatwg.org/#concept-request-origin. This origin can be
  // null upon request, corresponding to the "client" value in the spec. In that
  // case client's origin will be set when requesting. See
  // ResourceFetcher::RequestResource.
  const scoped_refptr<const SecurityOrigin>& RequestorOrigin() const {
    return requestor_origin_;
  }
  void SetRequestorOrigin(scoped_refptr<const SecurityOrigin> origin) {
    requestor_origin_ = std::move(origin);
  }

  // The origin of the isolated world - set if this is a fetch/XHR initiated by
  // an isolated world.
  const scoped_refptr<const SecurityOrigin>& IsolatedWorldOrigin() const {
    return isolated_world_origin_;
  }
  void SetIsolatedWorldOrigin(scoped_refptr<const SecurityOrigin> origin) {
    isolated_world_origin_ = std::move(origin);
  }

  const AtomicString& HttpMethod() const;
  void SetHttpMethod(const AtomicString&);

  const HTTPHeaderMap& HttpHeaderFields() const;
  const AtomicString& HttpHeaderField(const AtomicString& name) const;
  void SetHttpHeaderField(const AtomicString& name, const AtomicString& value);
  void AddHttpHeaderField(const AtomicString& name, const AtomicString& value);
  void AddHTTPHeaderFields(const HTTPHeaderMap& header_fields);
  void ClearHttpHeaderField(const AtomicString& name);

  const AtomicString& HttpContentType() const {
    return HttpHeaderField(http_names::kContentType);
  }
  void SetHTTPContentType(const AtomicString& http_content_type) {
    SetHttpHeaderField(http_names::kContentType, http_content_type);
  }

  // TODO(domfarolino): Remove this once we stop storing the generated referrer
  // as a header, and instead use a separate member. See
  // https://crbug.com/850813.
  const AtomicString& HttpReferrer() const {
    return HttpHeaderField(http_names::kReferer);
  }
  void SetHttpReferrer(const Referrer&);
  bool DidSetHttpReferrer() const { return did_set_http_referrer_; }
  void ClearHTTPReferrer();

  void SetReferrerPolicy(network::mojom::ReferrerPolicy referrer_policy) {
    referrer_policy_ = referrer_policy;
  }
  network::mojom::ReferrerPolicy GetReferrerPolicy() const {
    return referrer_policy_;
  }

  void SetReferrerString(const String& referrer_string) {
    referrer_string_ = referrer_string;
  }
  const String& ReferrerString() const { return referrer_string_; }

  const AtomicString& HttpOrigin() const {
    return HttpHeaderField(http_names::kOrigin);
  }
  void SetHTTPOrigin(const SecurityOrigin*);
  void ClearHTTPOrigin();
  void SetHttpOriginIfNeeded(const SecurityOrigin*);
  void SetHTTPOriginToMatchReferrerIfNeeded();

  void SetHTTPUserAgent(const AtomicString& http_user_agent) {
    SetHttpHeaderField(http_names::kUserAgent, http_user_agent);
  }
  void ClearHTTPUserAgent();

  void SetHTTPAccept(const AtomicString& http_accept) {
    SetHttpHeaderField(http_names::kAccept, http_accept);
  }

  EncodedFormData* HttpBody() const;
  void SetHttpBody(scoped_refptr<EncodedFormData>);

  bool AllowStoredCredentials() const;
  void SetAllowStoredCredentials(bool allow_credentials);

  // TODO(yhirano): Describe what Priority and IntraPriorityValue are.
  ResourceLoadPriority Priority() const;
  int IntraPriorityValue() const;
  bool PriorityHasBeenSet() const;
  void SetPriority(ResourceLoadPriority, int intra_priority_value = 0);

  bool IsConditional() const;

  // Whether the associated ResourceHandleClient needs to be notified of
  // upload progress made for that resource.
  bool ReportUploadProgress() const { return report_upload_progress_; }
  void SetReportUploadProgress(bool report_upload_progress) {
    report_upload_progress_ = report_upload_progress;
  }

  // Whether actual headers being sent/received should be collected and reported
  // for the request.
  bool ReportRawHeaders() const { return report_raw_headers_; }
  void SetReportRawHeaders(bool report_raw_headers) {
    report_raw_headers_ = report_raw_headers;
  }

  // Allows the request to be matched up with its requestor.
  int RequestorID() const { return requestor_id_; }
  void SetRequestorID(int requestor_id) { requestor_id_ = requestor_id; }

  // True if request was user initiated.
  bool HasUserGesture() const { return has_user_gesture_; }
  void SetHasUserGesture(bool);

  // True if request shuold be downloaded to blob.
  bool DownloadToBlob() const { return download_to_blob_; }
  void SetDownloadToBlob(bool download_to_blob) {
    download_to_blob_ = download_to_blob;
  }

  // True if the requestor wants to receive a response body as
  // WebDataConsumerHandle.
  bool UseStreamOnResponse() const { return use_stream_on_response_; }
  void SetUseStreamOnResponse(bool use_stream_on_response) {
    use_stream_on_response_ = use_stream_on_response;
  }

  // True if the request can work after the fetch group is terminated.
  bool GetKeepalive() const { return keepalive_; }
  void SetKeepalive(bool keepalive) { keepalive_ = keepalive; }

  // True if service workers should not get events for the request.
  bool GetSkipServiceWorker() const { return skip_service_worker_; }
  void SetSkipServiceWorker(bool skip_service_worker) {
    skip_service_worker_ = skip_service_worker;
  }

  // True if corresponding AppCache group should be resetted.
  bool ShouldResetAppCache() const { return should_reset_app_cache_; }
  void SetShouldResetAppCache(bool should_reset_app_cache) {
    should_reset_app_cache_ = should_reset_app_cache;
  }

  // Extra data associated with this request.
  WebURLRequest::ExtraData* GetExtraData() const {
    return sharable_extra_data_ ? sharable_extra_data_->data.get() : nullptr;
  }
  void SetExtraData(std::unique_ptr<WebURLRequest::ExtraData> extra_data) {
    if (extra_data) {
      sharable_extra_data_ =
          base::MakeRefCounted<SharableExtraData>(std::move(extra_data));
    } else {
      sharable_extra_data_ = nullptr;
    }
  }

  bool IsDownloadToNetworkCacheOnly() const { return download_to_cache_only_; }

  void SetDownloadToNetworkCacheOnly(bool download_to_cache_only) {
    download_to_cache_only_ = download_to_cache_only;
  }

  mojom::RequestContextType GetRequestContext() const {
    return request_context_;
  }
  void SetRequestContext(mojom::RequestContextType context) {
    request_context_ = context;
  }

  network::mojom::RequestMode GetMode() const { return mode_; }
  void SetMode(network::mojom::RequestMode mode) { mode_ = mode; }

  // A resource request's fetch_importance_mode_ is a developer-set priority
  // hint that differs from priority_. It is used in
  // ResourceFetcher::ComputeLoadPriority to possibly influence the resolved
  // priority of a resource request.
  // This member exists both here and in FetchParameters, as opposed just in
  // the latter because the fetch() API creates a ResourceRequest object long
  // before its associaed FetchParameters, so this makes it easier to
  // communicate an importance value down to the lower-level fetching code.
  mojom::FetchImportanceMode GetFetchImportanceMode() const {
    return fetch_importance_mode_;
  }
  void SetFetchImportanceMode(mojom::FetchImportanceMode mode) {
    fetch_importance_mode_ = mode;
  }

  network::mojom::CredentialsMode GetCredentialsMode() const {
    return credentials_mode_;
  }
  void SetCredentialsMode(network::mojom::CredentialsMode mode) {
    credentials_mode_ = mode;
  }

  network::mojom::RedirectMode GetRedirectMode() const {
    return redirect_mode_;
  }
  void SetRedirectMode(network::mojom::RedirectMode redirect) {
    redirect_mode_ = redirect;
  }

  const String& GetFetchIntegrity() const { return fetch_integrity_; }
  void SetFetchIntegrity(const String& integrity) {
    fetch_integrity_ = integrity;
  }

  WebURLRequest::PreviewsState GetPreviewsState() const {
    return previews_state_;
  }
  void SetPreviewsState(WebURLRequest::PreviewsState previews_state) {
    previews_state_ = previews_state;
  }

  bool CacheControlContainsNoCache() const;
  bool CacheControlContainsNoStore() const;
  bool HasCacheValidatorFields() const;

  // https://wicg.github.io/cors-rfc1918/#external-request
  bool IsExternalRequest() const { return is_external_request_; }
  void SetExternalRequestStateFromRequestorAddressSpace(
      network::mojom::IPAddressSpace);

  network::mojom::CorsPreflightPolicy CorsPreflightPolicy() const {
    return cors_preflight_policy_;
  }
  void SetCorsPreflightPolicy(network::mojom::CorsPreflightPolicy policy) {
    cors_preflight_policy_ = policy;
  }

  void SetRedirectStatus(RedirectStatus status) { redirect_status_ = status; }
  RedirectStatus GetRedirectStatus() const { return redirect_status_; }

  void SetSuggestedFilename(const base::Optional<String>& suggested_filename) {
    suggested_filename_ = suggested_filename;
  }
  const base::Optional<String>& GetSuggestedFilename() const {
    return suggested_filename_;
  }

  void SetIsAdResource() { is_ad_resource_ = true; }
  bool IsAdResource() const { return is_ad_resource_; }

  void SetUpgradeIfInsecure(bool upgrade_if_insecure) {
    upgrade_if_insecure_ = upgrade_if_insecure;
  }
  bool UpgradeIfInsecure() const { return upgrade_if_insecure_; }

  bool IsRevalidating() const { return is_revalidating_; }
  void SetIsRevalidating(bool value) { is_revalidating_ = value; }
  void SetIsAutomaticUpgrade(bool is_automatic_upgrade) {
    is_automatic_upgrade_ = is_automatic_upgrade;
  }
  bool IsAutomaticUpgrade() const { return is_automatic_upgrade_; }

  void SetAllowStaleResponse(bool value) { allow_stale_response_ = value; }
  bool AllowsStaleResponse() const { return allow_stale_response_; }

  const base::Optional<base::UnguessableToken>& GetDevToolsToken() const {
    return devtools_token_;
  }
  void SetDevToolsToken(
      const base::Optional<base::UnguessableToken>& devtools_token) {
    devtools_token_ = devtools_token;
  }

  const base::Optional<String>& GetDevToolsId() const { return devtools_id_; }
  void SetDevToolsId(const base::Optional<String>& devtools_id) {
    devtools_id_ = devtools_id;
  }

  void SetRequestedWithHeader(const String& value) {
    requested_with_header_ = value;
  }
  const String& GetRequestedWithHeader() const {
    return requested_with_header_;
  }

  void SetClientDataHeader(const String& value) { client_data_header_ = value; }
  const String& GetClientDataHeader() const { return client_data_header_; }

  void SetPurposeHeader(const String& value) { purpose_header_ = value; }
  const String& GetPurposeHeader() const { return purpose_header_; }

  void SetUkmSourceId(ukm::SourceId ukm_source_id) {
    ukm_source_id_ = ukm_source_id;
  }
  ukm::SourceId GetUkmSourceId() const { return ukm_source_id_; }

  // https://fetch.spec.whatwg.org/#concept-request-window
  // See network::ResourceRequest::fetch_window_id for details.
  void SetFetchWindowId(const base::UnguessableToken& id) {
    fetch_window_id_ = id;
  }
  const base::UnguessableToken& GetFetchWindowId() const {
    return fetch_window_id_;
  }

  void SetRecursivePrefetchToken(
      const base::Optional<base::UnguessableToken>& token) {
    recursive_prefetch_token_ = token;
  }
  const base::Optional<base::UnguessableToken>& RecursivePrefetchToken() const {
    return recursive_prefetch_token_;
  }

  void SetInspectorId(uint64_t inspector_id) { inspector_id_ = inspector_id; }
  uint64_t InspectorId() const { return inspector_id_; }

  // Temporary for metrics. True if the request was initiated by a stylesheet
  // that is not origin-clean:
  // https://drafts.csswg.org/cssom-1/#concept-css-style-sheet-origin-clean-flag
  //
  // TODO(crbug.com/898497): Remove this when there is enough data.
  bool IsFromOriginDirtyStyleSheet() const {
    return is_from_origin_dirty_style_sheet_;
  }
  void SetFromOriginDirtyStyleSheet(bool dirty) {
    is_from_origin_dirty_style_sheet_ = dirty;
  }

  bool IsSignedExchangePrefetchCacheEnabled() const {
    return is_signed_exchange_prefetch_cache_enabled_;
  }
  void SetSignedExchangePrefetchCacheEnabled(bool enabled) {
    is_signed_exchange_prefetch_cache_enabled_ = enabled;
  }

  bool PrefetchMaybeForTopLeveNavigation() const {
    return prefetch_maybe_for_top_level_navigation_;
  }
  void SetPrefetchMaybeForTopLevelNavigation(
      bool prefetch_maybe_for_top_level_navigation) {
    prefetch_maybe_for_top_level_navigation_ =
        prefetch_maybe_for_top_level_navigation;
  }

  // Whether either RequestorOrigin or IsolatedWorldOrigin can display the
  // |url|,
  bool CanDisplay(const KURL&) const;

 private:
  using SharableExtraData =
      base::RefCountedData<std::unique_ptr<WebURLRequest::ExtraData>>;

  const CacheControlHeader& GetCacheControlHeader() const;

  bool NeedsHTTPOrigin() const;

  KURL url_;
  // TODO(yoav): initial_url_for_resource_timing_ is a stop-gap only needed
  // until Out-of-Blink CORS lands: https://crbug.com/736308
  KURL initial_url_for_resource_timing_;
  // base::TimeDelta::Max() represents the default timeout on platforms that
  // have one.
  base::TimeDelta timeout_interval_;
  KURL site_for_cookies_;
  scoped_refptr<const SecurityOrigin> top_frame_origin_;

  scoped_refptr<const SecurityOrigin> requestor_origin_;
  scoped_refptr<const SecurityOrigin> isolated_world_origin_;

  AtomicString http_method_;
  HTTPHeaderMap http_header_fields_;
  scoped_refptr<EncodedFormData> http_body_;
  bool allow_stored_credentials_ : 1;
  bool report_upload_progress_ : 1;
  bool report_raw_headers_ : 1;
  bool has_user_gesture_ : 1;
  bool download_to_blob_ : 1;
  bool use_stream_on_response_ : 1;
  bool keepalive_ : 1;
  bool should_reset_app_cache_ : 1;
  bool allow_stale_response_ : 1;
  mojom::FetchCacheMode cache_mode_;
  bool skip_service_worker_ : 1;
  bool download_to_cache_only_ : 1;
  ResourceLoadPriority priority_;
  int intra_priority_value_;
  int requestor_id_;
  WebURLRequest::PreviewsState previews_state_;
  scoped_refptr<SharableExtraData> sharable_extra_data_;
  mojom::RequestContextType request_context_;
  network::mojom::RequestMode mode_;
  mojom::FetchImportanceMode fetch_importance_mode_;
  network::mojom::CredentialsMode credentials_mode_;
  network::mojom::RedirectMode redirect_mode_;
  String fetch_integrity_;
  // TODO(domfarolino): Use AtomicString for referrer_string_ once
  // off-main-thread fetch is fully implemented and ResourceRequest never gets
  // transferred between threads. See https://crbug.com/706331.
  String referrer_string_;
  network::mojom::ReferrerPolicy referrer_policy_;
  bool did_set_http_referrer_;
  bool is_external_request_;
  network::mojom::CorsPreflightPolicy cors_preflight_policy_;
  RedirectStatus redirect_status_;

  base::Optional<String> suggested_filename_;

  mutable CacheControlHeader cache_control_header_cache_;

  static const base::TimeDelta default_timeout_interval_;

  bool is_ad_resource_ = false;

  bool upgrade_if_insecure_ = false;
  bool is_revalidating_ = false;

  bool is_automatic_upgrade_ = false;

  base::Optional<base::UnguessableToken> devtools_token_;
  base::Optional<String> devtools_id_;
  String requested_with_header_;
  String client_data_header_;
  String purpose_header_;

  ukm::SourceId ukm_source_id_ = ukm::kInvalidSourceId;

  base::UnguessableToken fetch_window_id_;

  uint64_t inspector_id_ = 0;

  bool is_from_origin_dirty_style_sheet_ = false;

  bool is_signed_exchange_prefetch_cache_enabled_ = false;

  // Currently this is only used when a prefetch request has `as=document`
  // specified. If true, and the request is cross-origin, the browser will cache
  // the request under the cross-origin's partition. Furthermore, its reuse from
  // the prefetch cache will be restricted to top-level-navigations.
  bool prefetch_maybe_for_top_level_navigation_ = false;

  // This is used when fetching preload header requests from cross-origin
  // prefetch responses. The browser process uses this token to ensure the
  // request is cached correctly.
  base::Optional<base::UnguessableToken> recursive_prefetch_token_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_RESOURCE_REQUEST_H_
