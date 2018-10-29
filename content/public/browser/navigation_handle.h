// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_NAVIGATION_HANDLE_H_
#define CONTENT_PUBLIC_BROWSER_NAVIGATION_HANDLE_H_

#include <memory>
#include <string>

#include "content/common/content_export.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/reload_type.h"
#include "content/public/browser/restore_type.h"
#include "content/public/common/referrer.h"
#include "content/public/common/transferrable_url_loader.mojom.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_info.h"
#include "services/network/public/cpp/resource_request_body.h"
#include "ui/base/page_transition_types.h"

class GURL;

namespace net {
class HttpRequestHeaders;
class HttpResponseHeaders;
}  // namespace net

namespace content {
struct GlobalRequestID;
class NavigationData;
class NavigationThrottle;
class NavigationUIData;
class RenderFrameHost;
class SiteInstance;
class WebContents;

// A NavigationHandle tracks information related to a single navigation.
// NavigationHandles are provided to several WebContentsObserver methods to
// allow observers to track specific navigations. Observers should clear any
// references to a NavigationHandle at the time of
// WebContentsObserver::DidFinishNavigation, just before the handle is
// destroyed.
class CONTENT_EXPORT NavigationHandle {
 public:
  virtual ~NavigationHandle() {}

  // Parameters available at navigation start time -----------------------------
  //
  // These parameters are always available during the navigation. Note that
  // some may change during navigation (e.g. due to server redirects).

  // Get a unique ID for this navigation.
  virtual int64_t GetNavigationId() const = 0;

  // The URL the frame is navigating to. This may change during the navigation
  // when encountering a server redirect.
  // This URL may not be the same as the virtual URL returned from
  // WebContents::GetVisibleURL and WebContents::GetLastCommittedURL. For
  // example, viewing a page's source navigates to the URL of the page, but the
  // virtual URL is prefixed with "view-source:".
  virtual const GURL& GetURL() = 0;

  // Returns the SiteInstance that started the request.
  // If a frame in SiteInstance A navigates a frame in SiteInstance B to a URL
  // in SiteInstance C, then this returns B.
  virtual SiteInstance* GetStartingSiteInstance() = 0;

  // Whether the navigation is taking place in the main frame or in a subframe.
  // This remains constant over the navigation lifetime.
  virtual bool IsInMainFrame() = 0;

  // Whether the navigation is taking place in a frame that is a direct child
  // of the main frame. This remains constant over the navigation lifetime.
  virtual bool IsParentMainFrame() = 0;

  // Whether the navigation was initiated by the renderer process. Examples of
  // renderer-initiated navigations include:
  //  * <a> link click
  //  * changing window.location.href
  //  * redirect via the <meta http-equiv="refresh"> tag
  //  * using window.history.pushState
  //
  // This method returns false for browser-initiated navigations, including:
  //  * any navigation initiated from the omnibox
  //  * navigations via suggestions in browser UI
  //  * navigations via browser UI: Ctrl-R, refresh/forward/back/home buttons
  //  * using window.history.forward() or window.history.back()
  //  * any other "explicit" URL navigations, e.g. bookmarks
  virtual bool IsRendererInitiated() = 0;

  // Returns the FrameTreeNode ID for the frame in which the navigation is
  // performed. This ID is browser-global and uniquely identifies a frame that
  // hosts content. The identifier is fixed at the creation of the frame and
  // stays constant for the lifetime of the frame.
  virtual int GetFrameTreeNodeId() = 0;

  // Returns the RenderFrameHost for the parent frame, or nullptr if this
  // navigation is taking place in the main frame. This value will not change
  // during a navigation.
  virtual RenderFrameHost* GetParentFrame() = 0;

  // The WebContents the navigation is taking place in.
  WebContents* GetWebContents();

  // The time the navigation started, recorded either in the renderer or in the
  // browser process. Corresponds to Navigation Timing API.
  virtual base::TimeTicks NavigationStart() = 0;

  // The time the input leading to the navigation started. Will not be
  // set if unknown.
  virtual base::TimeTicks NavigationInputStart() = 0;

  // Whether or not the navigation was started within a context menu.
  virtual bool WasStartedFromContextMenu() const = 0;

  // Returns the URL and encoding of an INPUT field that corresponds to a
  // searchable form request.
  virtual const GURL& GetSearchableFormURL() = 0;
  virtual const std::string& GetSearchableFormEncoding() = 0;

  // Returns the reload type for this navigation. Note that renderer-initiated
  // reloads (via location.reload()) won't count as a reload and do return
  // ReloadType::NONE.
  virtual ReloadType GetReloadType() = 0;

  // Returns the restore type for this navigation. RestoreType::NONE is returned
  // if the navigation is not a restore.
  virtual RestoreType GetRestoreType() = 0;

  // Used for specifying a base URL for pages loaded via data URLs.
  virtual const GURL& GetBaseURLForDataURL() = 0;

  // Whether the navigation is done using HTTP POST method. This may change
  // during the navigation (e.g. after encountering a server redirect).
  //
  // Note: page and frame navigations can only be done using HTTP POST or HTTP
  // GET methods (and using other, scheme-specific protocols for non-http(s) URI
  // schemes like data: or file:).  Therefore //content public API exposes only
  // |bool IsPost()| as opposed to |const std::string& GetMethod()| method.
  virtual bool IsPost() = 0;

  // Returns the POST body associated with this navigation. This will be null
  // for GET and/or other non-POST requests (or if a response to a POST request
  // was a redirect that changed the method to GET - for example 302).
  virtual const scoped_refptr<network::ResourceRequestBody>&
  GetResourceRequestBody() = 0;

  // Returns a sanitized version of the referrer for this request.
  virtual const Referrer& GetReferrer() = 0;

  // Whether the navigation was initiated by a user gesture. Note that this
  // will return false for browser-initiated navigations.
  // TODO(clamy): when PlzNavigate launches, this should return true for
  // browser-initiated navigations.
  virtual bool HasUserGesture() = 0;

  // Returns the page transition type.
  virtual ui::PageTransition GetPageTransition() = 0;

  // Returns the NavigationUIData associated with the navigation.
  virtual const NavigationUIData* GetNavigationUIData() = 0;

  // Whether the target URL cannot be handled by the browser's internal protocol
  // handlers.
  virtual bool IsExternalProtocol() = 0;

  // Navigation control flow --------------------------------------------------

  // The net error code if an error happened prior to commit. Otherwise it will
  // be net::OK.
  virtual net::Error GetNetErrorCode() = 0;

  // Returns the RenderFrameHost this navigation is committing in.  The
  // RenderFrameHost returned will be the final host for the navigation.  This
  // can only be accessed after a response has been delivered for processing,
  // or after the navigation fails with an error page.
  virtual RenderFrameHost* GetRenderFrameHost() = 0;

  // Whether the navigation happened without changing document. Examples of
  // same document navigations are:
  // * reference fragment navigations
  // * pushState/replaceState
  // * same page history navigation
  virtual bool IsSameDocument() = 0;

  // Whether the navigation has encountered a server redirect or not.
  virtual bool WasServerRedirect() = 0;

  // Lists the redirects that occurred on the way to the current page. The
  // current page is the last one in the list (so even when there's no redirect,
  // there will be one entry in the list).
  virtual const std::vector<GURL>& GetRedirectChain() = 0;

  // Whether the navigation has committed. Navigations that end up being
  // downloads or return 204/205 response codes do not commit (i.e. the
  // WebContents stays at the existing URL).
  // This returns true for either successful commits or error pages that
  // replace the previous page (distinguished by |IsErrorPage|), and false for
  // errors that leave the user on the previous page.
  virtual bool HasCommitted() = 0;

  // Whether the navigation resulted in an error page.
  // Note that if an error page reloads, this will return true even though
  // GetNetErrorCode will be net::OK.
  virtual bool IsErrorPage() = 0;

  // Not all committed subframe navigations (i.e., !IsInMainFrame &&
  // HasCommitted) end up causing a change of the current NavigationEntry. For
  // example, some users of NavigationHandle may want to ignore the initial
  // commit in a newly added subframe or location.replace events in subframes
  // (e.g., ads), while still reacting to user actions like link clicks and
  // back/forward in subframes.  Such users should check if this method returns
  // true before proceeding.
  // Note: it's only valid to call this method for subframes for which
  // HasCommitted returns true.
  virtual bool HasSubframeNavigationEntryCommitted() = 0;

  // True if the committed entry has replaced the existing one. A non-user
  // initiated redirect causes such replacement.
  virtual bool DidReplaceEntry() = 0;

  // Returns true if the browser history should be updated. Otherwise only
  // the session history will be updated. E.g., on unreachable urls.
  virtual bool ShouldUpdateHistory() = 0;

  // The previous main frame URL that the user was on. This may be empty if
  // there was no last committed entry.
  virtual const GURL& GetPreviousURL() = 0;

  // Returns the remote address of the socket which fetched this resource.
  virtual net::HostPortPair GetSocketAddress() = 0;

  // Returns the headers used for this request.
  virtual const net::HttpRequestHeaders& GetRequestHeaders() = 0;

  // Returns the response headers for the request, or nullptr if there aren't
  // any response headers or they have not been received yet. The response
  // headers may change during the navigation (e.g. after encountering a server
  // redirect). The headers returned should not be modified, as modifications
  // will not be reflected in the network stack.
  virtual const net::HttpResponseHeaders* GetResponseHeaders() = 0;

  // Returns the connection info for the request, the default value is
  // CONNECTION_INFO_UNKNOWN if there hasn't been a response (or redirect)
  // yet. The connection info may change during the navigation (e.g. after
  // encountering a server redirect).
  virtual net::HttpResponseInfo::ConnectionInfo GetConnectionInfo() = 0;

  // Returns the SSLInfo for a request that succeeded or failed due to a
  // certificate error. In the case of other request failures or of a non-secure
  // scheme, returns an empty object.
  virtual const net::SSLInfo& GetSSLInfo() = 0;

  // Returns the ID of the URLRequest associated with this navigation. Can only
  // be called from NavigationThrottle::WillProcessResponse and
  // WebContentsObserver::ReadyToCommitNavigation.
  // In the case of transfer navigations, this is the ID of the first request
  // made. The transferred request's ID will not be tracked by the
  // NavigationHandle.
  virtual const GlobalRequestID& GetGlobalRequestID() = 0;

  // Returns true if this navigation resulted in a download. Returns false if
  // this navigation did not result in a download, or if download status is not
  // yet known for this navigation.  Download status is determined for a
  // navigation when processing final (post redirect) HTTP response headers.
  virtual bool IsDownload() = 0;

  // Returns true if this navigation was initiated by a form submission.
  virtual bool IsFormSubmission() = 0;

  // Returns true if the target is an inner response of a signed exchange.
  virtual bool IsSignedExchangeInnerResponse() = 0;

  // Returns true if the navigation response was cached.
  virtual bool WasResponseCached() = 0;

  // Returns the proxy server used for this navigation, if any.
  virtual const net::ProxyServer& GetProxyServer() = 0;

  // Testing methods ----------------------------------------------------------
  //
  // The following methods should be used exclusively for writing unit tests.

  static std::unique_ptr<NavigationHandle> CreateNavigationHandleForTesting(
      const GURL& url,
      RenderFrameHost* render_frame_host,
      bool committed = false,
      net::Error error = net::OK,
      bool is_same_document = false,
      bool is_post = false,
      ui::PageTransition transition = ui::PAGE_TRANSITION_LINK,
      bool is_form_submission = false);

  // Registers a NavigationThrottle for tests. The throttle can
  // modify the request, pause the request or cancel the request. This will
  // take ownership of the NavigationThrottle.
  // Note: in non-test cases, NavigationThrottles should not be added directly
  // but returned by the implementation of
  // ContentBrowserClient::CreateThrottlesForNavigation. This ensures proper
  // ordering of the throttles.
  virtual void RegisterThrottleForTesting(
      std::unique_ptr<NavigationThrottle> navigation_throttle) = 0;

  // Simulates the network request starting.
  virtual NavigationThrottle::ThrottleCheckResult
  CallWillStartRequestForTesting() = 0;

  // Simulates the network request being redirected.
  virtual NavigationThrottle::ThrottleCheckResult
  CallWillRedirectRequestForTesting(const GURL& new_url,
                                    bool new_method_is_post,
                                    const GURL& new_referrer_url,
                                    bool new_is_external_protocol) = 0;

  // Simulates the network request failing.
  virtual NavigationThrottle::ThrottleCheckResult CallWillFailRequestForTesting(
      RenderFrameHost* render_frame_host,
      base::Optional<net::SSLInfo> ssl_info) = 0;

  // Simulates the reception of the network response.
  virtual NavigationThrottle::ThrottleCheckResult
  CallWillProcessResponseForTesting(RenderFrameHost* render_frame_host,
                                    const std::string& raw_response_headers,
                                    bool was_cached,
                                    const net::ProxyServer& proxy_server) = 0;

  // Simulates the navigation being committed.
  virtual void CallDidCommitNavigationForTesting(const GURL& url) = 0;

  // Simulates the navigation resuming. Most callers should just let the
  // deferring NavigationThrottle do the resuming.
  virtual void CallResumeForTesting() = 0;

  // Returns whether this navigation is currently deferred.
  virtual bool IsDeferredForTesting() = 0;

  // The NavigationData that the embedder returned from
  // ResourceDispatcherHostDelegate::GetNavigationData during commit. This will
  // be a clone of the NavigationData.
  virtual NavigationData* GetNavigationData() = 0;

  virtual void RegisterSubresourceOverride(
      mojom::TransferrableURLLoaderPtr transferrable_loader) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_NAVIGATION_HANDLE_H_
