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
#include "third_party/blink/public/platform/modules/fetch/fetch_api_request.mojom-shared.h"
#include "third_party/blink/public/platform/referrer.mojom-shared.h"
#include "third_party/blink/public/platform/web_common.h"

namespace network {
namespace mojom {
enum class CORSPreflightPolicy : int32_t;
enum class FetchCredentialsMode : int32_t;
enum class FetchRedirectMode : int32_t;
enum class FetchRequestMode : int32_t;
enum class RequestContextFrameType : int32_t;
}  // namespace mojom
}  // namespace network

namespace blink {

namespace mojom {
enum class FetchCacheMode : int32_t;
}  // namespace mojom

class ResourceRequest;
class WebHTTPBody;
class WebHTTPHeaderVisitor;
class WebSecurityOrigin;
class WebString;
class WebURL;
struct WebContentSecurityPolicyList;

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
    kServerLoFiOn = 1 << 0,    // Request a Lo-Fi version of the resource
                               // from the server.
    kClientLoFiOn = 1 << 1,    // Request a Lo-Fi version of the resource
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
    kPreviewsStateLast = kLazyImageLoadDeferred
  };

  class ExtraData {
   public:
    virtual ~ExtraData() = default;
  };

  BLINK_PLATFORM_EXPORT ~WebURLRequest();
  BLINK_PLATFORM_EXPORT WebURLRequest();
  BLINK_PLATFORM_EXPORT WebURLRequest(const WebURLRequest&);
  BLINK_PLATFORM_EXPORT explicit WebURLRequest(const WebURL&);
  BLINK_PLATFORM_EXPORT WebURLRequest& operator=(const WebURLRequest&);

  BLINK_PLATFORM_EXPORT bool IsNull() const;

  BLINK_PLATFORM_EXPORT WebURL Url() const;
  BLINK_PLATFORM_EXPORT void SetURL(const WebURL&);

  // Used to implement third-party cookie blocking.
  BLINK_PLATFORM_EXPORT WebURL SiteForCookies() const;
  BLINK_PLATFORM_EXPORT void SetSiteForCookies(const WebURL&);

  // https://fetch.spec.whatwg.org/#concept-request-origin
  BLINK_PLATFORM_EXPORT WebSecurityOrigin RequestorOrigin() const;
  BLINK_PLATFORM_EXPORT void SetRequestorOrigin(const WebSecurityOrigin&);

  // Controls whether user name, password, and cookies may be sent with the
  // request.
  BLINK_PLATFORM_EXPORT bool AllowStoredCredentials() const;
  BLINK_PLATFORM_EXPORT void SetAllowStoredCredentials(bool);

  BLINK_PLATFORM_EXPORT mojom::FetchCacheMode GetCacheMode() const;
  BLINK_PLATFORM_EXPORT void SetCacheMode(mojom::FetchCacheMode);

  BLINK_PLATFORM_EXPORT base::TimeDelta TimeoutInterval() const;

  BLINK_PLATFORM_EXPORT WebString HttpMethod() const;
  BLINK_PLATFORM_EXPORT void SetHTTPMethod(const WebString&);

  BLINK_PLATFORM_EXPORT WebString HttpHeaderField(const WebString& name) const;
  // It's not possible to set the referrer header using this method. Use
  // SetHTTPReferrer instead.
  BLINK_PLATFORM_EXPORT void SetHTTPHeaderField(const WebString& name,
                                                const WebString& value);
  BLINK_PLATFORM_EXPORT void SetHTTPReferrer(const WebString& referrer,
                                             network::mojom::ReferrerPolicy);
  BLINK_PLATFORM_EXPORT void AddHTTPHeaderField(const WebString& name,
                                                const WebString& value);
  BLINK_PLATFORM_EXPORT void ClearHTTPHeaderField(const WebString& name);
  BLINK_PLATFORM_EXPORT void VisitHTTPHeaderFields(WebHTTPHeaderVisitor*) const;

  BLINK_PLATFORM_EXPORT WebHTTPBody HttpBody() const;
  BLINK_PLATFORM_EXPORT void SetHTTPBody(const WebHTTPBody&);

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

  BLINK_PLATFORM_EXPORT network::mojom::RequestContextFrameType GetFrameType()
      const;
  BLINK_PLATFORM_EXPORT void SetFrameType(
      network::mojom::RequestContextFrameType);

  BLINK_PLATFORM_EXPORT network::mojom::ReferrerPolicy GetReferrerPolicy()
      const;

  // Sets an HTTP origin header if it is empty and the HTTP method of the
  // request requires it.
  BLINK_PLATFORM_EXPORT void SetHTTPOriginIfNeeded(const WebSecurityOrigin&);

  // True if the request was user initiated.
  BLINK_PLATFORM_EXPORT bool HasUserGesture() const;
  BLINK_PLATFORM_EXPORT void SetHasUserGesture(bool);

  // A consumer controlled value intended to be used to identify the
  // requestor.
  BLINK_PLATFORM_EXPORT int RequestorID() const;
  BLINK_PLATFORM_EXPORT void SetRequestorID(int);

  // The unique child id (not PID) of the process from which this request
  // originated. In the case of out-of-process plugins, this allows to link back
  // the request to the plugin process (as it is processed through a render view
  // process).
  BLINK_PLATFORM_EXPORT int GetPluginChildID() const;
  BLINK_PLATFORM_EXPORT void SetPluginChildID(int);

  // Allows the request to be matched up with its app cache host.
  BLINK_PLATFORM_EXPORT int AppCacheHostID() const;
  BLINK_PLATFORM_EXPORT void SetAppCacheHostID(int);

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
  BLINK_PLATFORM_EXPORT network::mojom::FetchRequestMode GetFetchRequestMode()
      const;
  BLINK_PLATFORM_EXPORT void SetFetchRequestMode(
      network::mojom::FetchRequestMode);

  // The credentials mode which will be passed to the ServiceWorker.
  BLINK_PLATFORM_EXPORT network::mojom::FetchCredentialsMode
  GetFetchCredentialsMode() const;
  BLINK_PLATFORM_EXPORT void SetFetchCredentialsMode(
      network::mojom::FetchCredentialsMode);

  // The redirect mode which is used in Fetch API.
  BLINK_PLATFORM_EXPORT network::mojom::FetchRedirectMode GetFetchRedirectMode()
      const;
  BLINK_PLATFORM_EXPORT void SetFetchRedirectMode(
      network::mojom::FetchRedirectMode);

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

  BLINK_PLATFORM_EXPORT bool WasDiscarded() const;
  BLINK_PLATFORM_EXPORT void SetWasDiscarded(bool);

  // https://wicg.github.io/cors-rfc1918/#external-request
  BLINK_PLATFORM_EXPORT bool IsExternalRequest() const;

  BLINK_PLATFORM_EXPORT network::mojom::CORSPreflightPolicy
  GetCORSPreflightPolicy() const;

  BLINK_PLATFORM_EXPORT void SetNavigationStartTime(base::TimeTicks);

  // If this request was created from an anchor with a download attribute, this
  // is the value provided there.
  BLINK_PLATFORM_EXPORT base::Optional<WebString> GetSuggestedFilename() const;

  // Returns true if this request is tagged as an ad. This is done using various
  // heuristics so it is not expected to be 100% accurate.
  BLINK_PLATFORM_EXPORT bool IsAdResource() const;

  // This is the navigation relevant CSP to be used during request and response
  // checks.
  BLINK_PLATFORM_EXPORT const WebContentSecurityPolicyList& GetInitiatorCSP()
      const;

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

  // Set the applicable Origin Policy.
  BLINK_PLATFORM_EXPORT const WebString GetOriginPolicy() const;
  BLINK_PLATFORM_EXPORT void SetOriginPolicy(const WebString& policy);

  // Remembers 'X-Requested-With' header value. Blink should not set this header
  // value until CORS checks are done to avoid running checks even against
  // headers that are internally set.
  BLINK_PLATFORM_EXPORT const WebString GetRequestedWith() const;
  BLINK_PLATFORM_EXPORT void SetRequestedWith(const WebString&);

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
