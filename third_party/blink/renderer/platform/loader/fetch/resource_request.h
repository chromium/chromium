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
#include "services/network/public/mojom/cors.mojom-blink.h"
#include "services/network/public/mojom/fetch_api.mojom-blink.h"
#include "services/network/public/mojom/request_context_frame_type.mojom-shared.h"
#include "third_party/blink/public/mojom/net/ip_address_space.mojom-blink.h"
#include "third_party/blink/public/platform/modules/fetch/fetch_api_request.mojom-shared.h"
#include "third_party/blink/public/platform/resource_request_blocked_reason.h"
#include "third_party/blink/public/platform/web_content_security_policy_struct.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_load_priority.h"
#include "third_party/blink/renderer/platform/network/http_header_map.h"
#include "third_party/blink/renderer/platform/network/http_names.h"
#include "third_party/blink/renderer/platform/network/http_parsers.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/referrer.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/ref_counted.h"
#include "third_party/blink/renderer/platform/wtf/time.h"

namespace blink {

class EncodedFormData;

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
  // make ResourceRequest WTF_MAKE_NONCOPYABLE. See crbug.com/787704.
  ResourceRequest(const ResourceRequest&);
  ResourceRequest& operator=(const ResourceRequest&);

  ~ResourceRequest();

  // Constructs a new ResourceRequest for a redirect from this instance.
  std::unique_ptr<ResourceRequest> CreateRedirectRequest(
      const KURL& new_url,
      const AtomicString& new_method,
      const KURL& new_site_for_cookies,
      const String& new_referrer,
      ReferrerPolicy new_referrer_policy,
      bool skip_service_worker) const;

  bool IsNull() const;

  const KURL& Url() const;
  void SetURL(const KURL&);

  void RemoveUserAndPassFromURL();

  mojom::FetchCacheMode GetCacheMode() const;
  void SetCacheMode(mojom::FetchCacheMode);

  base::TimeDelta TimeoutInterval() const;
  void SetTimeoutInterval(base::TimeDelta);

  const KURL& SiteForCookies() const;
  void SetSiteForCookies(const KURL&);

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

  const AtomicString& HttpMethod() const;
  void SetHTTPMethod(const AtomicString&);

  const HTTPHeaderMap& HttpHeaderFields() const;
  const AtomicString& HttpHeaderField(const AtomicString& name) const;
  void SetHTTPHeaderField(const AtomicString& name, const AtomicString& value);
  void AddHTTPHeaderField(const AtomicString& name, const AtomicString& value);
  void AddHTTPHeaderFields(const HTTPHeaderMap& header_fields);
  void ClearHTTPHeaderField(const AtomicString& name);

  const AtomicString& HttpContentType() const {
    return HttpHeaderField(HTTPNames::Content_Type);
  }
  void SetHTTPContentType(const AtomicString& http_content_type) {
    SetHTTPHeaderField(HTTPNames::Content_Type, http_content_type);
  }

  // TODO(domfarolino): Remove this once we stop storing the generated referrer
  // as a header, and instead use a separate member. See
  // https://crbug.com/850813.
  const AtomicString& HttpReferrer() const {
    return HttpHeaderField(HTTPNames::Referer);
  }
  void SetHTTPReferrer(const Referrer&);
  bool DidSetHTTPReferrer() const { return did_set_http_referrer_; }
  void ClearHTTPReferrer();

  void SetReferrerPolicy(ReferrerPolicy referrer_policy) {
    referrer_policy_ = referrer_policy;
  }
  ReferrerPolicy GetReferrerPolicy() const { return referrer_policy_; }

  void SetReferrerString(const String& referrer_string) {
    referrer_string_ = referrer_string;
  }
  const String& ReferrerString() const { return referrer_string_; }

  const AtomicString& HttpOrigin() const {
    return HttpHeaderField(HTTPNames::Origin);
  }
  void SetHTTPOrigin(const SecurityOrigin*);
  void ClearHTTPOrigin();
  void SetHTTPOriginIfNeeded(const SecurityOrigin*);
  void SetHTTPOriginToMatchReferrerIfNeeded();

  void SetHTTPUserAgent(const AtomicString& http_user_agent) {
    SetHTTPHeaderField(HTTPNames::User_Agent, http_user_agent);
  }
  void ClearHTTPUserAgent();

  void SetHTTPAccept(const AtomicString& http_accept) {
    SetHTTPHeaderField(HTTPNames::Accept, http_accept);
  }

  EncodedFormData* HttpBody() const;
  void SetHTTPBody(scoped_refptr<EncodedFormData>);

  bool AllowStoredCredentials() const;
  void SetAllowStoredCredentials(bool allow_credentials);

  // TODO(yhirano): Describe what Priority and IntraPriorityValue are.
  ResourceLoadPriority Priority() const;
  int IntraPriorityValue() const;
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

  // The unique child id (not PID) of the process from which this request
  // originated. In the case of out-of-process plugins, this allows to link back
  // the request to the plugin process (as it is processed through a render view
  // process).
  int GetPluginChildID() const { return plugin_child_id_; }
  void SetPluginChildID(int plugin_child_id) {
    plugin_child_id_ = plugin_child_id;
  }

  // Allows the request to be matched up with its app cache host.
  int AppCacheHostID() const { return app_cache_host_id_; }
  void SetAppCacheHostID(int id) { app_cache_host_id_ = id; }

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

  network::mojom::RequestContextFrameType GetFrameType() const {
    return frame_type_;
  }
  void SetFrameType(network::mojom::RequestContextFrameType frame_type) {
    frame_type_ = frame_type;
  }

  network::mojom::FetchRequestMode GetFetchRequestMode() const {
    return fetch_request_mode_;
  }
  void SetFetchRequestMode(network::mojom::FetchRequestMode mode) {
    fetch_request_mode_ = mode;
  }

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

  network::mojom::FetchCredentialsMode GetFetchCredentialsMode() const {
    return fetch_credentials_mode_;
  }
  void SetFetchCredentialsMode(network::mojom::FetchCredentialsMode mode) {
    fetch_credentials_mode_ = mode;
  }

  network::mojom::FetchRedirectMode GetFetchRedirectMode() const {
    return fetch_redirect_mode_;
  }
  void SetFetchRedirectMode(network::mojom::FetchRedirectMode redirect) {
    fetch_redirect_mode_ = redirect;
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

  bool WasDiscarded() const { return was_discarded_; }
  void SetWasDiscarded(bool was_discarded) { was_discarded_ = was_discarded; }

  // https://wicg.github.io/cors-rfc1918/#external-request
  bool IsExternalRequest() const { return is_external_request_; }
  void SetExternalRequestStateFromRequestorAddressSpace(mojom::IPAddressSpace);

  network::mojom::CORSPreflightPolicy CORSPreflightPolicy() const {
    return cors_preflight_policy_;
  }
  void SetCORSPreflightPolicy(network::mojom::CORSPreflightPolicy policy) {
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

  void SetNavigationStartTime(TimeTicks);
  TimeTicks NavigationStartTime() const { return navigation_start_; }

  void SetIsAdResource() { is_ad_resource_ = true; }
  bool IsAdResource() const { return is_ad_resource_; }

  void SetInitiatorCSP(const WebContentSecurityPolicyList& initiator_csp) {
    initiator_csp_ = initiator_csp;
  }
  const WebContentSecurityPolicyList& GetInitiatorCSP() const {
    return initiator_csp_;
  }

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

  void SetOriginPolicy(const String& policy) { origin_policy_ = policy; }
  const String& GetOriginPolicy() const { return origin_policy_; }

  void SetRequestedWith(const String& value) { requested_with_ = value; }
  const String& GetRequestedWith() const { return requested_with_; }

 private:
  using SharableExtraData =
      base::RefCountedData<std::unique_ptr<WebURLRequest::ExtraData>>;

  const CacheControlHeader& GetCacheControlHeader() const;

  bool NeedsHTTPOrigin() const;

  KURL url_;
  // TimeDelta::Max() represents the default timeout on platforms that have one.
  base::TimeDelta timeout_interval_;
  KURL site_for_cookies_;

  scoped_refptr<const SecurityOrigin> requestor_origin_;

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
  int plugin_child_id_;
  int app_cache_host_id_;
  WebURLRequest::PreviewsState previews_state_;
  scoped_refptr<SharableExtraData> sharable_extra_data_;
  mojom::RequestContextType request_context_;
  network::mojom::RequestContextFrameType frame_type_;
  network::mojom::FetchRequestMode fetch_request_mode_;
  mojom::FetchImportanceMode fetch_importance_mode_;
  network::mojom::FetchCredentialsMode fetch_credentials_mode_;
  network::mojom::FetchRedirectMode fetch_redirect_mode_;
  String fetch_integrity_;
  // TODO(domfarolino): Use AtomicString for referrer_string_ once
  // off-main-thread fetch is fully implemented and ResourceRequest never gets
  // transferred between threads. See https://crbug.com/706331.
  String referrer_string_;
  ReferrerPolicy referrer_policy_;
  bool did_set_http_referrer_;
  bool was_discarded_;
  bool is_external_request_;
  network::mojom::CORSPreflightPolicy cors_preflight_policy_;
  RedirectStatus redirect_status_;
  base::Optional<String> suggested_filename_;

  mutable CacheControlHeader cache_control_header_cache_;

  static base::TimeDelta default_timeout_interval_;

  TimeTicks navigation_start_;

  bool is_ad_resource_ = false;
  WebContentSecurityPolicyList initiator_csp_;

  bool upgrade_if_insecure_ = false;
  bool is_revalidating_ = false;

  bool is_automatic_upgrade_ = false;

  base::Optional<base::UnguessableToken> devtools_token_;
  String origin_policy_;
  String requested_with_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_RESOURCE_REQUEST_H_
