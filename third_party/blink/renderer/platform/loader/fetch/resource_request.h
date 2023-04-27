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

#include "base/containers/flat_set.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "net/cookies/site_for_cookies.h"
#include "net/filter/source_stream.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/network/public/mojom/attribution.mojom-blink.h"
#include "services/network/public/mojom/chunked_data_pipe_getter.mojom-blink-forward.h"
#include "services/network/public/mojom/cors.mojom-blink-forward.h"
#include "services/network/public/mojom/fetch_api.mojom-blink.h"
#include "services/network/public/mojom/ip_address_space.mojom-blink-forward.h"
#include "services/network/public/mojom/trust_tokens.mojom-blink.h"
#include "services/network/public/mojom/web_bundle_handle.mojom-blink.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink-forward.h"
#include "third_party/blink/public/platform/resource_request_blocked_reason.h"
#include "third_party/blink/public/platform/web_url_request_extra_data.h"
#include "third_party/blink/renderer/platform/loader/fetch/render_blocking_behavior.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_load_priority.h"
#include "third_party/blink/renderer/platform/network/http_header_map.h"
#include "third_party/blink/renderer/platform/network/http_names.h"
#include "third_party/blink/renderer/platform/network/http_parsers.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/ref_counted.h"

namespace blink {

class EncodedFormData;

// ResourceRequestHead represents request without request body.
// See ResourceRequest below to see what request is.
// ResourceRequestHead is implicitly copyable while ResourceRequest is not.
// TODO(yoichio) : Migrate existing ResourceRequest occurrence not using request
// body to ResourceRequestHead.
class PLATFORM_EXPORT ResourceRequestHead {
  DISALLOW_NEW();

 public:
  // TODO: Remove this enum from here since it is not used in this class anymore
  enum class RedirectStatus : uint8_t { kFollowedRedirect, kNoRedirect };

  struct RedirectInfo {
    // Original (first) url in the redirect chain.
    KURL original_url;

    // Previous url in the redirect chain.
    KURL previous_url;

    RedirectInfo() = delete;
    RedirectInfo(const KURL& original_url, const KURL& previous_url)
        : original_url(original_url), previous_url(previous_url) {}
  };

  struct PLATFORM_EXPORT WebBundleTokenParams {
    WebBundleTokenParams() = delete;
    WebBundleTokenParams(const WebBundleTokenParams& other);
    WebBundleTokenParams& operator=(const WebBundleTokenParams& other);

    WebBundleTokenParams(
        const KURL& bundle_url,
        const base::UnguessableToken& token,
        mojo::PendingRemote<network::mojom::blink::WebBundleHandle> handle);

    mojo::PendingRemote<network::mojom::blink::WebBundleHandle> CloneHandle()
        const;

    KURL bundle_url;
    base::UnguessableToken token;
    mojo::PendingRemote<network::mojom::blink::WebBundleHandle> handle;
  };

  ResourceRequestHead();
  explicit ResourceRequestHead(const KURL&);

  ResourceRequestHead(const ResourceRequestHead&);
  ResourceRequestHead& operator=(const ResourceRequestHead&);
  ResourceRequestHead(ResourceRequestHead&&);
  ResourceRequestHead& operator=(ResourceRequestHead&&);

  ~ResourceRequestHead();

  // Constructs a new ResourceRequest for a redirect from this instance.
  // Since body for a redirect request is kept and handled in the network
  // service, the returned instance here in blink side doesn't contain body.
  std::unique_ptr<ResourceRequest> CreateRedirectRequest(
      const KURL& new_url,
      const AtomicString& new_method,
      const net::SiteForCookies& new_site_for_cookies,
      const String& new_referrer,
      network::mojom::ReferrerPolicy new_referrer_policy,
      bool skip_service_worker) const;

  bool IsNull() const;

  const KURL& Url() const;
  void SetUrl(const KURL&);

  void RemoveUserAndPassFromURL();

  mojom::blink::FetchCacheMode GetCacheMode() const;
  void SetCacheMode(mojom::blink::FetchCacheMode);

  base::TimeDelta TimeoutInterval() const;
  void SetTimeoutInterval(base::TimeDelta);

  const net::SiteForCookies& SiteForCookies() const;
  void SetSiteForCookies(const net::SiteForCookies&);

  // Returns true if SiteForCookies() was set either via SetSiteForCookies or
  // CreateRedirectRequest.
  bool SiteForCookiesSet() const { return site_for_cookies_set_; }

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

  // The chain of URLs seen during navigation redirects.  This should only
  // contain values if the mode is `RedirectMode::kNavigate`.
  const WTF::Vector<KURL>& NavigationRedirectChain() const {
    return navigation_redirect_chain_;
  }
  void SetNavigationRedirectChain(const WTF::Vector<KURL>& value) {
    navigation_redirect_chain_ = value;
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

  bool IsFormSubmission() const { return is_form_submission_; }
  void SetFormSubmission(bool is_form_submission) {
    is_form_submission_ = is_form_submission;
  }

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

  bool AllowStoredCredentials() const;
  void SetAllowStoredCredentials(bool allow_credentials);

  // The initial priority for the request.
  ResourceLoadPriority InitialPriority() const;

  // The current priority for the request (in case it was changed).
  ResourceLoadPriority Priority() const;

  // Sub-priority for ordering requests at the same priority level.
  // Used for visible images to load larger images before small.
  int IntraPriorityValue() const;
  bool PriorityHasBeenSet() const;
  void SetPriority(ResourceLoadPriority, int intra_priority_value = 0);

  bool IsConditional() const;

  // Incremental property of HTTP Extensible Priorities which specifies that
  // responses can be delivered concurrently if they are the same priority on
  // a connection that supports multiplexing (HTTP/3 primarily).
  // https://www.rfc-editor.org/rfc/rfc9218
  bool PriorityIncremental() const;
  void SetPriorityIncremental(bool);

  // Whether the associated ResourceHandleClient needs to be notified of
  // upload progress made for that resource.
  bool ReportUploadProgress() const { return report_upload_progress_; }
  void SetReportUploadProgress(bool report_upload_progress) {
    report_upload_progress_ = report_upload_progress;
  }

  // True if request was user initiated.
  bool HasUserGesture() const { return has_user_gesture_; }
  void SetHasUserGesture(bool);

  bool HasTextFragmentToken() const { return has_text_fragment_token_; }
  void SetHasTextFragmentToken(bool);

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

  // True if the request should be considered for computing and attaching the
  // topics headers.
  bool GetBrowsingTopics() const { return browsing_topics_; }
  void SetBrowsingTopics(bool browsing_topics) {
    browsing_topics_ = browsing_topics;
  }

  // True if service workers should not get events for the request.
  bool GetSkipServiceWorker() const { return skip_service_worker_; }
  void SetSkipServiceWorker(bool skip_service_worker) {
    skip_service_worker_ = skip_service_worker;
  }

  // Extra data associated with this request.
  const scoped_refptr<WebURLRequestExtraData>& GetURLRequestExtraData() const {
    return url_request_extra_data_;
  }
  void SetURLRequestExtraData(
      scoped_refptr<WebURLRequestExtraData> url_request_extra_data) {
    url_request_extra_data_ = std::move(url_request_extra_data);
  }

  bool IsDownloadToNetworkCacheOnly() const { return download_to_cache_only_; }

  void SetDownloadToNetworkCacheOnly(bool download_to_cache_only) {
    download_to_cache_only_ = download_to_cache_only;
  }

  mojom::blink::RequestContextType GetRequestContext() const {
    return request_context_;
  }
  void SetRequestContext(mojom::blink::RequestContextType context) {
    request_context_ = context;
  }

  network::mojom::RequestDestination GetRequestDestination() const {
    return destination_;
  }
  void SetRequestDestination(network::mojom::RequestDestination destination) {
    destination_ = destination;
  }

  network::mojom::RequestMode GetMode() const { return mode_; }
  void SetMode(network::mojom::RequestMode mode) { mode_ = mode; }

  network::mojom::IPAddressSpace GetTargetAddressSpace() const {
    return target_address_space_;
  }
  void SetTargetAddressSpace(
      network::mojom::IPAddressSpace target_address_space) {
    target_address_space_ = target_address_space;
  }

  // A resource request's fetch_priority_hint_ is a developer-set priority
  // hint that differs from priority_. It is used in
  // ResourceFetcher::ComputeLoadPriority to possibly influence the resolved
  // priority of a resource request.
  // This member exists both here and in FetchParameters, as opposed just in
  // the latter because the fetch() API creates a ResourceRequest object long
  // before its associaed FetchParameters, so this makes it easier to
  // communicate a fetch_priority_hint value down to the lower-level fetching
  // code.
  mojom::blink::FetchPriorityHint GetFetchPriorityHint() const {
    return fetch_priority_hint_;
  }
  void SetFetchPriorityHint(
      mojom::blink::FetchPriorityHint fetch_priority_hint) {
    fetch_priority_hint_ = fetch_priority_hint;
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

  bool CacheControlContainsNoCache() const;
  bool CacheControlContainsNoStore() const;
  bool HasCacheValidatorFields() const;

  network::mojom::CorsPreflightPolicy CorsPreflightPolicy() const {
    return cors_preflight_policy_;
  }
  void SetCorsPreflightPolicy(network::mojom::CorsPreflightPolicy policy) {
    cors_preflight_policy_ = policy;
  }

  const absl::optional<RedirectInfo>& GetRedirectInfo() const {
    return redirect_info_;
  }

  void SetSuggestedFilename(const absl::optional<String>& suggested_filename) {
    suggested_filename_ = suggested_filename;
  }
  const absl::optional<String>& GetSuggestedFilename() const {
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

  const absl::optional<base::UnguessableToken>& GetDevToolsToken() const {
    return devtools_token_;
  }
  void SetDevToolsToken(
      const absl::optional<base::UnguessableToken>& devtools_token) {
    devtools_token_ = devtools_token;
  }

  const scoped_refptr<
      base::RefCountedData<base::flat_set<net::SourceStream::SourceType>>>&
  GetDevToolsAcceptedStreamTypes() const {
    return devtools_accepted_stream_types_;
  }
  void SetDevToolsAcceptedStreamTypes(
      const scoped_refptr<
          base::RefCountedData<base::flat_set<net::SourceStream::SourceType>>>&
          types) {
    devtools_accepted_stream_types_ = types;
  }

  const absl::optional<String>& GetDevToolsId() const { return devtools_id_; }
  void SetDevToolsId(const absl::optional<String>& devtools_id) {
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

  // A V8 stack id string describing where the request was initiated. DevTools
  // can use this to display the initiator call stack when debugging a process
  // that later intercepts the request, e.g., in a service worker fetch event
  // handler.
  const absl::optional<String>& GetDevToolsStackId() const {
    return devtools_stack_id_;
  }
  void SetDevToolsStackId(const absl::optional<String>& devtools_stack_id) {
    devtools_stack_id_ = devtools_stack_id;
  }

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
      const absl::optional<base::UnguessableToken>& token) {
    recursive_prefetch_token_ = token;
  }
  const absl::optional<base::UnguessableToken>& RecursivePrefetchToken() const {
    return recursive_prefetch_token_;
  }

  void SetInspectorId(uint64_t inspector_id) { inspector_id_ = inspector_id; }
  uint64_t InspectorId() const { return inspector_id_; }

  bool IsFromOriginDirtyStyleSheet() const {
    return is_from_origin_dirty_style_sheet_;
  }
  void SetFromOriginDirtyStyleSheet(bool dirty) {
    is_from_origin_dirty_style_sheet_ = dirty;
  }

  bool IsFetchLikeAPI() const { return is_fetch_like_api_; }

  void SetFetchLikeAPI(bool enabled) { is_fetch_like_api_ = enabled; }

  bool IsFavicon() const { return is_favicon_; }

  void SetFavicon(bool enabled) { is_favicon_ = enabled; }

  bool PrefetchMaybeForTopLeveNavigation() const {
    return prefetch_maybe_for_top_level_navigation_;
  }
  void SetPrefetchMaybeForTopLevelNavigation(
      bool prefetch_maybe_for_top_level_navigation) {
    prefetch_maybe_for_top_level_navigation_ =
        prefetch_maybe_for_top_level_navigation;
  }

  const absl::optional<network::mojom::blink::TrustTokenParams>&
  TrustTokenParams() const {
    return trust_token_params_;
  }
  void SetTrustTokenParams(
      absl::optional<network::mojom::blink::TrustTokenParams> params) {
    trust_token_params_ = std::move(params);
  }

  // Whether either RequestorOrigin or IsolatedWorldOrigin can display the
  // |url|,
  bool CanDisplay(const KURL&) const;

  // The original destination of a request passed through by a service worker.
  network::mojom::RequestDestination GetOriginalDestination() const {
    return original_destination_;
  }
  void SetOriginalDestination(network::mojom::RequestDestination value) {
    original_destination_ = value;
  }

  const absl::optional<ResourceRequestHead::WebBundleTokenParams>&
  GetWebBundleTokenParams() const {
    return web_bundle_token_params_;
  }

  void SetWebBundleTokenParams(
      ResourceRequestHead::WebBundleTokenParams params) {
    web_bundle_token_params_ = params;
  }

  void SetRenderBlockingBehavior(RenderBlockingBehavior behavior) {
    render_blocking_behavior_ = behavior;
  }
  RenderBlockingBehavior GetRenderBlockingBehavior() const {
    return render_blocking_behavior_;
  }

  void SetHasStorageAccess(bool has_storage_access) {
    has_storage_access_ = has_storage_access;
  }
  bool GetHasStorageAccess() const { return has_storage_access_; }

  network::mojom::AttributionSupport GetAttributionReportingSupport() const {
    return attribution_reporting_support_;
  }

  void SetAttributionReportingSupport(
      network::mojom::AttributionSupport attribution_support) {
    attribution_reporting_support_ = attribution_support;
  }

  network::mojom::AttributionReportingEligibility
  GetAttributionReportingEligibility() const {
    return attribution_reporting_eligibility_;
  }

  void SetAttributionReportingEligibility(
      network::mojom::AttributionReportingEligibility eligibility) {
    attribution_reporting_eligibility_ = eligibility;
  }

 private:
  const CacheControlHeader& GetCacheControlHeader() const;

  bool NeedsHTTPOrigin() const;

  KURL url_;
  // base::TimeDelta::Max() represents the default timeout on platforms that
  // have one.
  base::TimeDelta timeout_interval_;
  net::SiteForCookies site_for_cookies_;
  scoped_refptr<const SecurityOrigin> top_frame_origin_;

  scoped_refptr<const SecurityOrigin> requestor_origin_;
  WTF::Vector<KURL> navigation_redirect_chain_;
  scoped_refptr<const SecurityOrigin> isolated_world_origin_;

  AtomicString http_method_;
  HTTPHeaderMap http_header_fields_;
  bool allow_stored_credentials_ : 1;
  bool report_upload_progress_ : 1;
  bool has_user_gesture_ : 1;
  bool has_text_fragment_token_ : 1;
  bool download_to_blob_ : 1;
  bool use_stream_on_response_ : 1;
  bool keepalive_ : 1;
  bool browsing_topics_ : 1;
  bool allow_stale_response_ : 1;
  mojom::blink::FetchCacheMode cache_mode_;
  bool skip_service_worker_ : 1;
  bool download_to_cache_only_ : 1;
  bool site_for_cookies_set_ : 1;
  bool is_form_submission_ : 1;
  bool priority_incremental_ : 1;
  ResourceLoadPriority initial_priority_;
  ResourceLoadPriority priority_;
  int intra_priority_value_;
  scoped_refptr<WebURLRequestExtraData> url_request_extra_data_;
  mojom::blink::RequestContextType request_context_;
  network::mojom::RequestDestination destination_;
  network::mojom::RequestMode mode_;
  mojom::blink::FetchPriorityHint fetch_priority_hint_;
  network::mojom::CredentialsMode credentials_mode_;
  network::mojom::RedirectMode redirect_mode_;
  String fetch_integrity_;
  String referrer_string_;
  network::mojom::ReferrerPolicy referrer_policy_;
  network::mojom::CorsPreflightPolicy cors_preflight_policy_;
  absl::optional<RedirectInfo> redirect_info_;
  absl::optional<network::mojom::blink::TrustTokenParams> trust_token_params_;
  network::mojom::IPAddressSpace target_address_space_;

  absl::optional<String> suggested_filename_;

  mutable CacheControlHeader cache_control_header_cache_;

  static const base::TimeDelta default_timeout_interval_;

  bool is_ad_resource_ = false;

  bool upgrade_if_insecure_ = false;
  bool is_revalidating_ = false;

  bool is_automatic_upgrade_ = false;

  absl::optional<base::UnguessableToken> devtools_token_;
  absl::optional<String> devtools_id_;
  String requested_with_header_;
  String client_data_header_;
  String purpose_header_;

  absl::optional<String> devtools_stack_id_;

  ukm::SourceId ukm_source_id_ = ukm::kInvalidSourceId;

  base::UnguessableToken fetch_window_id_;

  network::mojom::RequestDestination original_destination_ =
      network::mojom::RequestDestination::kEmpty;

  uint64_t inspector_id_ = 0;

  bool is_from_origin_dirty_style_sheet_ = false;

  bool is_fetch_like_api_ = false;

  bool is_favicon_ = false;

  // Currently this is only used when a prefetch request has `as=document`
  // specified. If true, and the request is cross-origin, the browser will cache
  // the request under the cross-origin's partition. Furthermore, its reuse from
  // the prefetch cache will be restricted to top-level-navigations.
  bool prefetch_maybe_for_top_level_navigation_ = false;

  // This is used when fetching preload header requests from cross-origin
  // prefetch responses. The browser process uses this token to ensure the
  // request is cached correctly.
  absl::optional<base::UnguessableToken> recursive_prefetch_token_;

  // This is used when fetching either a WebBundle or a subresrouce in the
  // WebBundle. The network process uses this token to associate the request to
  // the bundle.
  absl::optional<WebBundleTokenParams> web_bundle_token_params_;

  // Render blocking behavior of the resource. Used in maintaining correct
  // reporting for redirects.
  RenderBlockingBehavior render_blocking_behavior_ =
      RenderBlockingBehavior::kUnset;

  // If not null, the network service will not advertise any stream types
  // (via Accept-Encoding) that are not listed. Also, it will not attempt
  // decoding any non-listed stream types.
  // Instead of using absl::optional, we use scoped_refptr to reduce
  // blink memory footprint because the attribute is only used by DevTools
  // and we should keep the footprint minimal when DevTools is closed.
  scoped_refptr<
      base::RefCountedData<base::flat_set<net::SourceStream::SourceType>>>
      devtools_accepted_stream_types_;

  bool has_storage_access_ = false;

  network::mojom::AttributionSupport attribution_reporting_support_ =
      network::mojom::AttributionSupport::kWeb;

  network::mojom::AttributionReportingEligibility
      attribution_reporting_eligibility_ =
          network::mojom::AttributionReportingEligibility::kUnset;
};

class PLATFORM_EXPORT ResourceRequestBody {
 public:
  ResourceRequestBody();
  explicit ResourceRequestBody(scoped_refptr<EncodedFormData> form_body);
  explicit ResourceRequestBody(
      mojo::PendingRemote<network::mojom::blink::ChunkedDataPipeGetter>
          stream_body);
  ResourceRequestBody(const ResourceRequestBody&) = delete;
  ResourceRequestBody(ResourceRequestBody&&);

  ResourceRequestBody& operator=(const ResourceRequestBody&) = delete;
  ResourceRequestBody& operator=(ResourceRequestBody&&);

  ~ResourceRequestBody();

  bool IsEmpty() const { return !form_body_ && !stream_body_; }
  const scoped_refptr<EncodedFormData>& FormBody() const { return form_body_; }
  void SetFormBody(scoped_refptr<EncodedFormData>);

  mojo::PendingRemote<network::mojom::blink::ChunkedDataPipeGetter>
  TakeStreamBody() {
    return std::move(stream_body_);
  }
  const mojo::PendingRemote<network::mojom::blink::ChunkedDataPipeGetter>&
  StreamBody() const {
    return stream_body_;
  }
  void SetStreamBody(
      mojo::PendingRemote<network::mojom::blink::ChunkedDataPipeGetter>);

 private:
  scoped_refptr<EncodedFormData> form_body_;
  mojo::PendingRemote<network::mojom::blink::ChunkedDataPipeGetter>
      stream_body_;
};

// A ResourceRequest is a "request" object for ResourceLoader. Conceptually
// it is https://fetch.spec.whatwg.org/#concept-request, but it contains
// a lot of blink specific fields. WebURLRequest is the "public version"
// of this class and URLLoader needs it. See WebURLRequest and
// WrappedResourceRequest.
//
// This class is thread-bound. Do not copy/pass an instance across threads.
//
// Although request consists head and body, ResourceRequest is implemented by
// inheriting ResourceRequestHead in order to make it possible to use
// property accessors through both ResourceRequestHead and ResourceRequest while
// avoiding duplicate accessor definitions.
// For those who want to add a new property in request, please implement its
// member and accessors in ResourceRequestHead instead of ResourceRequest.
class PLATFORM_EXPORT ResourceRequest final : public ResourceRequestHead {
  USING_FAST_MALLOC(ResourceRequest);

 public:
  ResourceRequest();
  explicit ResourceRequest(const String& url_string);
  explicit ResourceRequest(const KURL&);
  explicit ResourceRequest(const ResourceRequestHead&);

  ResourceRequest(const ResourceRequest&) = delete;
  ResourceRequest(ResourceRequest&&);
  ResourceRequest& operator=(const ResourceRequest&) = delete;
  ResourceRequest& operator=(ResourceRequest&&);

  ~ResourceRequest();

  void CopyHeadFrom(const ResourceRequestHead&);

  const scoped_refptr<EncodedFormData>& HttpBody() const;
  void SetHttpBody(scoped_refptr<EncodedFormData>);

  ResourceRequestBody& MutableBody() { return body_; }

 private:
  ResourceRequestBody body_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_RESOURCE_REQUEST_H_
