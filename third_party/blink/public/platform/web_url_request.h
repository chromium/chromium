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

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_URL_REQUEST_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_URL_REQUEST_H_

#include <memory>
#include "base/optional.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "third_party/blink/public/platform/web_common.h"
#include "ui/base/page_transition_types.h"

// TODO(crbug.com/922875): Need foo.mojom.shared-forward.h.
namespace network {
namespace mojom {
enum class CorsPreflightPolicy : int32_t;
enum class CredentialsMode : int32_t;
enum class RedirectMode : int32_t;
enum class ReferrerPolicy : int32_t;
enum class RequestMode : int32_t;
enum class RequestContextFrameType : int32_t;
}  // namespace mojom
}  // namespace network

namespace blink {

namespace mojom {
enum class FetchCacheMode : int32_t;
enum class RequestContextType : int32_t;
}  // namespace mojom

class ResourceRequest;
class WebHTTPBody;
class WebHTTPHeaderVisitor;
class WebSecurityOrigin;
class WebString;
class WebURL;

class WebURLRequest {
 public:
  // The enum values should remain synchronized with the enum
  // WebURLRequestPriority in tools/metrics/histograms.enums.xml.
  enum class Priority {
    kUnresolved = -1,
    kVeryLow,
    kLow,
    kMedium,
    kHigh,
    kVeryHigh,
    kLowest = kVeryLow,
    kHighest = kVeryHigh,
  };

  typedef int PreviewsState;

  // The Previews types which determines whether to request a Preview version of
  // the resource.
  enum PreviewsTypes {
    kPreviewsUnspecified = 0,  // Let the browser process decide whether or
                               // not to request Preview types.
    kServerLoFiOn_DEPRECATED =
        1 << 0,  // Request a Lo-Fi version of the resource
                 // from the server. Deprecated and should not be used.
    kClientLoFiOn = 1 << 1,          // Request a Lo-Fi version of the resource
                                     // from the client.
    kClientLoFiAutoReload = 1 << 2,  // Request the original version of the
                                     // resource after a decoding error occurred
                                     // when attempting to use Client Lo-Fi.
    kServerLitePageOn = 1 << 3,      // Request a Lite Page version of the
                                     // resource from the server.
    kPreviewsNoTransform = 1 << 4,   // Explicitly forbid Previews
                                     // transformations.
    kPreviewsOff = 1 << 5,  // Request a normal (non-Preview) version of
                            // the resource. Server transformations may
                            // still happen if the page is heavy.
    kNoScriptOn = 1 << 6,   // Request that script be disabled for page load.
    kResourceLoadingHintsOn = 1 << 7,  // Request that resource loading hints be
                                       // used during pageload.
    kOfflinePageOn = 1 << 8,
    kLitePageRedirectOn = 1 << 9,  // Allow the browser to redirect the resource
                                   // to a Lite Page server.
    kLazyImageLoadDeferred = 1 << 10,  // Request the placeholder version of an
                                       // image that was deferred by lazyload.
    kLazyImageAutoReload = 1 << 11,    // Request the full version of an image
                                       // that was previously fetched as a
                                       // placeholder by lazyload.
    kDeferAllScriptOn = 1 << 12,  // Request that script execution be deferred
                                  // until parsing completes.
    kSubresourceRedirectOn =
        1 << 13,  // Allow the subresources in the page to be redirected
                  // to serve better optimized resources.
    kPreviewsStateLast = kSubresourceRedirectOn
  };

  class ExtraData {
   public:
    void set_render_frame_id(int render_frame_id) {
      render_frame_id_ = render_frame_id;
    }
    void set_is_main_frame(bool is_main_frame) {
      is_main_frame_ = is_main_frame;
    }
    ui::PageTransition transition_type() const { return transition_type_; }
    void set_transition_type(ui::PageTransition transition_type) {
      transition_type_ = transition_type;
    }

    // The request is for a prefetch-only client (i.e. running NoStatePrefetch)
    // and should use LOAD_PREFETCH network flags.
    bool is_for_no_state_prefetch() const { return is_for_no_state_prefetch_; }
    void set_is_for_no_state_prefetch(bool prefetch) {
      is_for_no_state_prefetch_ = prefetch;
    }

    // true if the request originated from within a service worker e.g. due to
    // a fetch() in the service worker script.
    void set_originated_from_service_worker(
        bool originated_from_service_worker) {
      originated_from_service_worker_ = originated_from_service_worker;
    }

    // Determines whether SameSite cookies will be attached to the request
    // even when the request looks cross-site.
    bool attach_same_site_cookies() const { return attach_same_site_cookies_; }
    void set_attach_same_site_cookies(bool attach) {
      attach_same_site_cookies_ = attach;
    }

    virtual ~ExtraData() = default;

   protected:
    BLINK_PLATFORM_EXPORT ExtraData();

    int render_frame_id_;
    bool is_main_frame_ = false;
    ui::PageTransition transition_type_ = ui::PAGE_TRANSITION_LINK;
    bool is_for_no_state_prefetch_ = false;
    bool originated_from_service_worker_ = false;
    bool attach_same_site_cookies_ = false;
  };

  BLINK_PLATFORM_EXPORT ~WebURLRequest();
  BLINK_PLATFORM_EXPORT WebURLRequest();
  BLINK_PLATFORM_EXPORT WebURLRequest(const WebURLRequest&);
  BLINK_PLATFORM_EXPORT explicit WebURLRequest(const WebURL&);
  BLINK_PLATFORM_EXPORT WebURLRequest& operator=(const WebURLRequest&);

  BLINK_PLATFORM_EXPORT bool IsNull() const;

  BLINK_PLATFORM_EXPORT WebURL Url() const;
  BLINK_PLATFORM_EXPORT void SetUrl(const WebURL&);

  // Used to implement third-party cookie blocking.
  BLINK_PLATFORM_EXPORT WebURL SiteForCookies() const;
  BLINK_PLATFORM_EXPORT void SetSiteForCookies(const WebURL&);

  BLINK_PLATFORM_EXPORT base::Optional<WebSecurityOrigin> TopFrameOrigin()
      const;
  BLINK_PLATFORM_EXPORT void SetTopFrameOrigin(const WebSecurityOrigin&);

  // https://fetch.spec.whatwg.org/#concept-request-origin
  BLINK_PLATFORM_EXPORT WebSecurityOrigin RequestorOrigin() const;
  BLINK_PLATFORM_EXPORT void SetRequestorOrigin(const WebSecurityOrigin&);

  // The origin of the isolated world - set if this is a fetch/XHR initiated by
  // an isolated world.
  BLINK_PLATFORM_EXPORT WebSecurityOrigin IsolatedWorldOrigin() const;

  // Controls whether user name, password, and cookies may be sent with the
  // request.
  BLINK_PLATFORM_EXPORT bool AllowStoredCredentials() const;
  BLINK_PLATFORM_EXPORT void SetAllowStoredCredentials(bool);

  BLINK_PLATFORM_EXPORT mojom::FetchCacheMode GetCacheMode() const;
  BLINK_PLATFORM_EXPORT void SetCacheMode(mojom::FetchCacheMode);

  BLINK_PLATFORM_EXPORT base::TimeDelta TimeoutInterval() const;

  BLINK_PLATFORM_EXPORT WebString HttpMethod() const;
  BLINK_PLATFORM_EXPORT void SetHttpMethod(const WebString&);

  BLINK_PLATFORM_EXPORT WebString HttpHeaderField(const WebString& name) const;
  // It's not possible to set the referrer header using this method. Use
  // SetHttpReferrer instead.
  BLINK_PLATFORM_EXPORT void SetHttpHeaderField(const WebString& name,
                                                const WebString& value);
  BLINK_PLATFORM_EXPORT void SetHttpReferrer(const WebString& referrer,
                                             network::mojom::ReferrerPolicy);
  BLINK_PLATFORM_EXPORT void AddHttpHeaderField(const WebString& name,
                                                const WebString& value);
  BLINK_PLATFORM_EXPORT void ClearHttpHeaderField(const WebString& name);
  BLINK_PLATFORM_EXPORT void VisitHttpHeaderFields(WebHTTPHeaderVisitor*) const;

  BLINK_PLATFORM_EXPORT WebHTTPBody HttpBody() const;
  BLINK_PLATFORM_EXPORT void SetHttpBody(const WebHTTPBody&);

  BLINK_PLATFORM_EXPORT WebHTTPBody AttachedCredential() const;
  BLINK_PLATFORM_EXPORT void SetAttachedCredential(const WebHTTPBody&);

  // Controls whether upload progress events are generated when a request
  // has a body.
  BLINK_PLATFORM_EXPORT bool ReportUploadProgress() const;
  BLINK_PLATFORM_EXPORT void SetReportUploadProgress(bool);

  // Controls whether actual headers sent and received for request are
  // collected and reported.
  BLINK_PLATFORM_EXPORT bool ReportRawHeaders() const;
  BLINK_PLATFORM_EXPORT void SetReportRawHeaders(bool);

  BLINK_PLATFORM_EXPORT mojom::RequestContextType GetRequestContext() const;
  BLINK_PLATFORM_EXPORT void SetRequestContext(mojom::RequestContextType);

  BLINK_PLATFORM_EXPORT network::mojom::ReferrerPolicy GetReferrerPolicy()
      const;

  // Sets an HTTP origin header if it is empty and the HTTP method of the
  // request requires it.
  BLINK_PLATFORM_EXPORT void SetHttpOriginIfNeeded(const WebSecurityOrigin&);

  // True if the request was user initiated.
  BLINK_PLATFORM_EXPORT bool HasUserGesture() const;
  BLINK_PLATFORM_EXPORT void SetHasUserGesture(bool);

  // A consumer controlled value intended to be used to identify the
  // requestor.
  BLINK_PLATFORM_EXPORT int RequestorID() const;
  BLINK_PLATFORM_EXPORT void SetRequestorID(int);

  // If true, the client expects to receive the raw response pipe. Similar to
  // UseStreamOnResponse but the stream will be a mojo DataPipe rather than a
  // WebDataConsumerHandle.
  // If the request is fetched synchronously the response will instead be piped
  // to a blob if this flag is set to true.
  BLINK_PLATFORM_EXPORT bool PassResponsePipeToClient() const;

  // True if the requestor wants to receive the response body as a stream.
  BLINK_PLATFORM_EXPORT bool UseStreamOnResponse() const;
  BLINK_PLATFORM_EXPORT void SetUseStreamOnResponse(bool);

  // True if the request can work after the fetch group is terminated.
  BLINK_PLATFORM_EXPORT bool GetKeepalive() const;
  BLINK_PLATFORM_EXPORT void SetKeepalive(bool);

  // True if the service workers should not get events for the request.
  BLINK_PLATFORM_EXPORT bool GetSkipServiceWorker() const;
  BLINK_PLATFORM_EXPORT void SetSkipServiceWorker(bool);

  // True if corresponding AppCache group should be resetted.
  BLINK_PLATFORM_EXPORT bool ShouldResetAppCache() const;
  BLINK_PLATFORM_EXPORT void SetShouldResetAppCache(bool);

  // The request mode which will be passed to the ServiceWorker.
  BLINK_PLATFORM_EXPORT network::mojom::RequestMode GetMode() const;
  BLINK_PLATFORM_EXPORT void SetMode(network::mojom::RequestMode);

  // The credentials mode which will be passed to the ServiceWorker.
  BLINK_PLATFORM_EXPORT network::mojom::CredentialsMode GetCredentialsMode()
      const;
  BLINK_PLATFORM_EXPORT void SetCredentialsMode(
      network::mojom::CredentialsMode);

  // The redirect mode which is used in Fetch API.
  BLINK_PLATFORM_EXPORT network::mojom::RedirectMode GetRedirectMode() const;
  BLINK_PLATFORM_EXPORT void SetRedirectMode(network::mojom::RedirectMode);

  // The integrity which is used in Fetch API.
  BLINK_PLATFORM_EXPORT WebString GetFetchIntegrity() const;
  BLINK_PLATFORM_EXPORT void SetFetchIntegrity(const WebString&);

  // The PreviewsState which determines whether to request a Preview version of
  // the resource. The PreviewsState is a bitmask of potentially several
  // Previews optimizations.
  BLINK_PLATFORM_EXPORT PreviewsState GetPreviewsState() const;
  BLINK_PLATFORM_EXPORT void SetPreviewsState(PreviewsState);

  // Extra data associated with the underlying resource request. Resource
  // requests can be copied. If non-null, each copy of a resource requests
  // holds a pointer to the extra data, and the extra data pointer will be
  // deleted when the last resource request is destroyed. Setting the extra
  // data pointer will cause the underlying resource request to be
  // dissociated from any existing non-null extra data pointer.
  BLINK_PLATFORM_EXPORT ExtraData* GetExtraData() const;
  BLINK_PLATFORM_EXPORT void SetExtraData(std::unique_ptr<ExtraData>);

  // The request is downloaded to the network cache, but not rendered or
  // executed.
  BLINK_PLATFORM_EXPORT bool IsDownloadToNetworkCacheOnly() const;
  BLINK_PLATFORM_EXPORT void SetDownloadToNetworkCacheOnly(bool);

  BLINK_PLATFORM_EXPORT Priority GetPriority() const;
  BLINK_PLATFORM_EXPORT void SetPriority(Priority);

  // https://wicg.github.io/cors-rfc1918/#external-request
  BLINK_PLATFORM_EXPORT bool IsExternalRequest() const;

  BLINK_PLATFORM_EXPORT network::mojom::CorsPreflightPolicy
  GetCorsPreflightPolicy() const;

  // If this request was created from an anchor with a download attribute, this
  // is the value provided there.
  BLINK_PLATFORM_EXPORT base::Optional<WebString> GetSuggestedFilename() const;

  // Returns true if this request is tagged as an ad. This is done using various
  // heuristics so it is not expected to be 100% accurate.
  BLINK_PLATFORM_EXPORT bool IsAdResource() const;

  // Should be set to true if this request (including redirects) should be
  // upgraded to HTTPS due to an Upgrade-Insecure-Requests requirement.
  BLINK_PLATFORM_EXPORT void SetUpgradeIfInsecure(bool);

  // Returns true if request (including redirects) should be upgraded to HTTPS
  // due to an Upgrade-Insecure-Requests requirement.
  BLINK_PLATFORM_EXPORT bool UpgradeIfInsecure() const;

  BLINK_PLATFORM_EXPORT bool SupportsAsyncRevalidation() const;

  // Returns true when the request is for revalidation.
  BLINK_PLATFORM_EXPORT bool IsRevalidating() const;

  // Returns the DevTools ID to throttle the network request.
  BLINK_PLATFORM_EXPORT const base::Optional<base::UnguessableToken>&
  GetDevToolsToken() const;

  // Remembers 'X-Requested-With' header value. Blink should not set this header
  // value until CORS checks are done to avoid running checks even against
  // headers that are internally set.
  BLINK_PLATFORM_EXPORT const WebString GetRequestedWithHeader() const;
  BLINK_PLATFORM_EXPORT void SetRequestedWithHeader(const WebString&);

  // Remembers 'Purpose' header value. Blink should not set this header value
  // until CORS checks are done to avoid running checks even against headers
  // that are internally set.
  BLINK_PLATFORM_EXPORT const WebString GetPurposeHeader() const;

  // https://fetch.spec.whatwg.org/#concept-request-window
  // See network::ResourceRequest::fetch_window_id for details.
  BLINK_PLATFORM_EXPORT const base::UnguessableToken& GetFetchWindowId() const;
  BLINK_PLATFORM_EXPORT void SetFetchWindowId(const base::UnguessableToken&);

  BLINK_PLATFORM_EXPORT base::Optional<WebString> GetDevToolsId() const;

  BLINK_PLATFORM_EXPORT int GetLoadFlagsForWebUrlRequest() const;

  BLINK_PLATFORM_EXPORT bool IsFromOriginDirtyStyleSheet() const;

  BLINK_PLATFORM_EXPORT bool IsSignedExchangePrefetchCacheEnabled() const;

  BLINK_PLATFORM_EXPORT base::Optional<base::UnguessableToken>
  RecursivePrefetchToken() const;

#if INSIDE_BLINK
  BLINK_PLATFORM_EXPORT ResourceRequest& ToMutableResourceRequest();
  BLINK_PLATFORM_EXPORT const ResourceRequest& ToResourceRequest() const;

 protected:
  // Permit subclasses to set arbitrary ResourceRequest pointer as
  // |resource_request_|. |owned_resource_request_| is not set in this case.
  BLINK_PLATFORM_EXPORT explicit WebURLRequest(ResourceRequest&);
#endif

 private:
  struct ResourceRequestContainer;

  // If this instance owns a ResourceRequest then |owned_resource_request_|
  // is non-null and |resource_request_| points to the ResourceRequest
  // instance it contains.
  std::unique_ptr<ResourceRequestContainer> owned_resource_request_;

  // Should never be null.
  ResourceRequest* resource_request_;
};

}  // namespace blink

#endif
