// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FRAME_HOST_NAVIGATION_HANDLE_IMPL_H_
#define CONTENT_BROWSER_FRAME_HOST_NAVIGATION_HANDLE_IMPL_H_

#include "content/public/browser/navigation_handle.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "content/browser/frame_host/frame_tree_node.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/common/content_export.h"
#include "content/public/browser/global_request_id.h"
#include "content/public/browser/navigation_data.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/navigation_type.h"
#include "content/public/browser/restore_type.h"
#include "third_party/blink/public/platform/modules/fetch/fetch_api_request.mojom.h"
#include "third_party/blink/public/platform/web_mixed_content_context_type.h"
#include "url/gurl.h"

struct FrameHostMsg_DidCommitProvisionalLoad_Params;

namespace content {

class AppCacheNavigationHandle;
class ChromeAppCacheService;
class NavigationUIData;
class NavigatorDelegate;
class ServiceWorkerContextWrapper;
class ServiceWorkerNavigationHandle;

// This class keeps track of a single navigation. It is created after the
// BeforeUnload for the navigation has run. It is then owned by the
// NavigationRequest until the navigation is ready to commit. The
// NavigationHandleImpl ownership is then transferred to the RenderFrameHost in
// which the navigation will commit. It is finaly destroyed when the navigation
// commits.
class CONTENT_EXPORT NavigationHandleImpl : public NavigationHandle {
 public:
  // If |redirect_chain| is empty, then the redirect chain will be created to
  // start with |url|. Otherwise |redirect_chain| is used as the starting point.
  // |navigation_start| comes from the CommonNavigationParams associated with
  // this navigation.
  static std::unique_ptr<NavigationHandleImpl> Create(
      const GURL& url,
      const std::vector<GURL>& redirect_chain,
      FrameTreeNode* frame_tree_node,
      bool is_renderer_initiated,
      bool is_same_document,
      base::TimeTicks navigation_start,
      int pending_nav_entry_id,
      bool started_from_context_menu,
      CSPDisposition should_check_main_world_csp,
      bool is_form_submission,
      std::unique_ptr<NavigationUIData> navigation_ui_data,
      const std::string& method = std::string(),
      net::HttpRequestHeaders request_headers = net::HttpRequestHeaders(),
      scoped_refptr<network::ResourceRequestBody> resource_request_body =
          nullptr,
      const Referrer& sanitized_referrer = content::Referrer(),
      bool has_user_gesture = false,
      ui::PageTransition transition = ui::PAGE_TRANSITION_LINK,
      bool is_external_protocol = false,
      blink::mojom::RequestContextType request_context_type =
          blink::mojom::RequestContextType::UNSPECIFIED,
      blink::WebMixedContentContextType mixed_content_context_type =
          blink::WebMixedContentContextType::kBlockable,
      base::TimeTicks input_start = base::TimeTicks());

  ~NavigationHandleImpl() override;

  // Used to track the state the navigation is currently in.
  enum State {
    INITIAL = 0,
    WILL_SEND_REQUEST,
    DEFERRING_START,
    WILL_REDIRECT_REQUEST,
    DEFERRING_REDIRECT,
    WILL_FAIL_REQUEST,
    DEFERRING_FAILURE,
    CANCELING,
    WILL_PROCESS_RESPONSE,
    DEFERRING_RESPONSE,
    READY_TO_COMMIT,
    DID_COMMIT,
    DID_COMMIT_ERROR_PAGE,
  };

  // NavigationHandle implementation:
  int64_t GetNavigationId() const override;
  const GURL& GetURL() override;
  SiteInstance* GetStartingSiteInstance() override;
  bool IsInMainFrame() override;
  bool IsParentMainFrame() override;
  bool IsRendererInitiated() override;
  bool WasServerRedirect() override;
  const std::vector<GURL>& GetRedirectChain() override;
  int GetFrameTreeNodeId() override;
  RenderFrameHostImpl* GetParentFrame() override;
  base::TimeTicks NavigationStart() override;
  base::TimeTicks NavigationInputStart() override;
  bool IsPost() override;
  const scoped_refptr<network::ResourceRequestBody>& GetResourceRequestBody()
      override;
  const Referrer& GetReferrer() override;
  bool HasUserGesture() override;
  ui::PageTransition GetPageTransition() override;
  const NavigationUIData* GetNavigationUIData() override;
  bool IsExternalProtocol() override;
  net::Error GetNetErrorCode() override;
  RenderFrameHostImpl* GetRenderFrameHost() override;
  bool IsSameDocument() override;
  bool HasCommitted() override;
  bool IsErrorPage() override;
  bool HasSubframeNavigationEntryCommitted() override;
  bool DidReplaceEntry() override;
  bool ShouldUpdateHistory() override;
  const GURL& GetPreviousURL() override;
  net::HostPortPair GetSocketAddress() override;
  const net::HttpRequestHeaders& GetRequestHeaders() override;
  const net::HttpResponseHeaders* GetResponseHeaders() override;
  net::HttpResponseInfo::ConnectionInfo GetConnectionInfo() override;
  const net::SSLInfo& GetSSLInfo() override;
  void RegisterThrottleForTesting(
      std::unique_ptr<NavigationThrottle> navigation_throttle) override;
  NavigationThrottle::ThrottleCheckResult CallWillStartRequestForTesting()
      override;
  NavigationThrottle::ThrottleCheckResult CallWillRedirectRequestForTesting(
      const GURL& new_url,
      bool new_method_is_post,
      const GURL& new_referrer_url,
      bool new_is_external_protocol) override;
  NavigationThrottle::ThrottleCheckResult CallWillFailRequestForTesting(
      RenderFrameHost* render_frame_host,
      base::Optional<net::SSLInfo> ssl_info) override;
  NavigationThrottle::ThrottleCheckResult CallWillProcessResponseForTesting(
      RenderFrameHost* render_frame_host,
      const std::string& raw_response_header,
      bool was_cached,
      const net::ProxyServer& proxy_server) override;
  void CallDidCommitNavigationForTesting(const GURL& url) override;
  void CallResumeForTesting() override;
  bool IsDeferredForTesting() override;
  bool WasStartedFromContextMenu() const override;
  const GURL& GetSearchableFormURL() override;
  const std::string& GetSearchableFormEncoding() override;
  ReloadType GetReloadType() override;
  RestoreType GetRestoreType() override;
  const GURL& GetBaseURLForDataURL() override;
  const GlobalRequestID& GetGlobalRequestID() override;
  bool IsDownload() override;
  bool IsFormSubmission() override;
  bool IsSignedExchangeInnerResponse() override;
  bool WasResponseCached() override;
  const net::ProxyServer& GetProxyServer() override;

  const std::string& origin_policy() const { return origin_policy_; }
  void set_origin_policy(const std::string& origin_policy) {
    origin_policy_ = origin_policy;
  }

  // Resume and CancelDeferredNavigation must only be called by the
  // NavigationThrottle that is currently deferring the navigation.
  // |resuming_throttle| and |cancelling_throttle| are the throttles calling
  // these methods.
  void Resume(NavigationThrottle* resuming_throttle);
  void CancelDeferredNavigation(NavigationThrottle* cancelling_throttle,
                                NavigationThrottle::ThrottleCheckResult result);

  NavigationData* GetNavigationData() override;
  void RegisterSubresourceOverride(
      mojom::TransferrableURLLoaderPtr transferrable_loader) override;

  // Used in tests.
  State state_for_testing() const { return state_; }

  // The NavigatorDelegate to notify/query for various navigation events.
  // Normally this is the WebContents, except if this NavigationHandle was
  // created during a navigation to an interstitial page. In this case it will
  // be the InterstitialPage itself.
  //
  // Note: due to the interstitial navigation case, all calls that can possibly
  // expose the NavigationHandle to code outside of content/ MUST go though the
  // NavigatorDelegate. In particular, the ContentBrowserClient should not be
  // called directly from the NavigationHandle code. Thus, these calls will not
  // expose the NavigationHandle when navigating to an InterstitialPage.
  NavigatorDelegate* GetDelegate() const;

  blink::mojom::RequestContextType request_context_type() const {
    DCHECK_GE(state_, WILL_SEND_REQUEST);
    return request_context_type_;
  }

  blink::WebMixedContentContextType mixed_content_context_type() const {
    DCHECK_GE(state_, WILL_SEND_REQUEST);
    return mixed_content_context_type_;
  }

  // Get the unique id from the NavigationEntry associated with this
  // NavigationHandle. Note that a synchronous, renderer-initiated navigation
  // will not have a NavigationEntry associated with it, and this will return 0.
  int pending_nav_entry_id() const { return pending_nav_entry_id_; }

  void set_net_error_code(net::Error net_error_code) {
    net_error_code_ = net_error_code;
  }

  // Updates the RenderFrameHost that is about to commit the navigation. This
  // is used during transfer navigations.
  void set_render_frame_host(RenderFrameHostImpl* render_frame_host) {
    render_frame_host_ = render_frame_host;
  }

  void InitServiceWorkerHandle(
      ServiceWorkerContextWrapper* service_worker_context);
  ServiceWorkerNavigationHandle* service_worker_handle() const {
    return service_worker_handle_.get();
  }

  void InitAppCacheHandle(ChromeAppCacheService* appcache_service);
  AppCacheNavigationHandle* appcache_handle() const {
    return appcache_handle_.get();
  }

  typedef base::Callback<void(NavigationThrottle::ThrottleCheckResult)>
      ThrottleChecksFinishedCallback;

  // Called when the URLRequest will start in the network stack.  |callback|
  // will be called when all throttle checks have completed. This will allow
  // the caller to cancel the navigation or let it proceed.
  void WillStartRequest(const ThrottleChecksFinishedCallback& callback);

  // Updates the state of the navigation handle after encountering a server
  // redirect.
  void UpdateStateFollowingRedirect(
      const GURL& new_url,
      const std::string& new_method,
      const GURL& new_referrer_url,
      bool new_is_external_protocol,
      scoped_refptr<net::HttpResponseHeaders> response_headers,
      net::HttpResponseInfo::ConnectionInfo connection_info,
      const ThrottleChecksFinishedCallback& callback);

  // Called when the URLRequest will be redirected in the network stack.
  // |callback| will be called when all throttles check have completed. This
  // will allow the caller to cancel the navigation or let it proceed.
  // This will also inform the delegate that the request was redirected.
  //
  // |post_redirect_process| is the renderer process we expect to
  // use to commit the navigation now that it has been redirected. It can be
  // null if there is no live process that can be used. In that case, a suitable
  // renderer process will be created at commit time.
  void WillRedirectRequest(
      const GURL& new_url,
      const std::string& new_method,
      const GURL& new_referrer_url,
      bool new_is_external_protocol,
      scoped_refptr<net::HttpResponseHeaders> response_headers,
      net::HttpResponseInfo::ConnectionInfo connection_info,
      RenderProcessHost* post_redirect_process,
      const ThrottleChecksFinishedCallback& callback);

  // Called when the URLRequest will fail. |render_frame_host| corresponds to
  // the RenderFrameHost in which the error page will load. |callback| will be
  // called when all throttles check have completed. This will allow the caller
  // to explicitly cancel the navigation (with a custom error code and/or
  // custom error page HTML) or let the failure proceed as normal.
  void WillFailRequest(RenderFrameHostImpl* render_frame_host,
                       base::Optional<net::SSLInfo> ssl_info,
                       const ThrottleChecksFinishedCallback& callback);

  // Called when the URLRequest has delivered response headers and metadata.
  // |callback| will be called when all throttle checks have completed,
  // allowing the caller to cancel the navigation or let it proceed.
  // NavigationHandle will not call |callback| with a result of DEFER.
  // If the result is PROCEED, then 'ReadyToCommitNavigation' will be called
  // with |render_frame_host| and |response_headers| just before calling
  // |callback|.
  void WillProcessResponse(
      RenderFrameHostImpl* render_frame_host,
      scoped_refptr<net::HttpResponseHeaders> response_headers,
      net::HttpResponseInfo::ConnectionInfo connection_info,
      const net::HostPortPair& socket_address,
      const net::SSLInfo& ssl_info,
      const GlobalRequestID& request_id,
      bool should_replace_current_entry,
      bool is_download,
      bool is_stream,
      bool is_signed_exchange_inner_response,
      bool was_cached,
      const ThrottleChecksFinishedCallback& callback);

  // Returns the FrameTreeNode this navigation is happening in.
  FrameTreeNode* frame_tree_node() { return frame_tree_node_; }

  // Called when the navigation is ready to be committed in
  // |render_frame_host|. This will update the |state_| and inform the
  // delegate.
  void ReadyToCommitNavigation(RenderFrameHostImpl* render_frame_host,
                               bool is_error);

  // Called when the navigation was committed in |render_frame_host|. This will
  // update the |state_|.
  // |navigation_entry_committed| indicates whether the navigation changed which
  // NavigationEntry is current.
  // |did_replace_entry| is true if the committed entry has replaced the
  // existing one. A non-user initiated redirect causes such replacement.
  void DidCommitNavigation(
      const FrameHostMsg_DidCommitProvisionalLoad_Params& params,
      bool navigation_entry_committed,
      bool did_replace_entry,
      const GURL& previous_url,
      NavigationType navigation_type,
      RenderFrameHostImpl* render_frame_host);

  // Called during commit. Takes ownership of the embedder's NavigationData
  // instance. This NavigationData may have been cloned prior to being added
  // here.
  void set_navigation_data(std::unique_ptr<NavigationData> navigation_data) {
    navigation_data_ = std::move(navigation_data);
  }

  NavigationUIData* navigation_ui_data() const {
    return navigation_ui_data_.get();
  }

  const GURL& base_url() { return base_url_; }

  void set_searchable_form_url(const GURL& url) { searchable_form_url_ = url; }
  void set_searchable_form_encoding(const std::string& encoding) {
    searchable_form_encoding_ = encoding;
  }

  NavigationType navigation_type() {
    DCHECK_GE(state_, DID_COMMIT);
    return navigation_type_;
  }

  void set_response_headers_for_testing(
      scoped_refptr<net::HttpResponseHeaders> response_headers) {
    response_headers_ = response_headers;
  }

  void set_complete_callback_for_testing(
      const ThrottleChecksFinishedCallback& callback) {
    complete_callback_for_testing_ = callback;
  }

  CSPDisposition should_check_main_world_csp() const {
    return should_check_main_world_csp_;
  }

  const SourceLocation& source_location() const { return source_location_; }
  void set_source_location(const SourceLocation& source_location) {
    source_location_ = source_location;
  }

  void set_proxy_server(const net::ProxyServer& proxy_server) {
    proxy_server_ = proxy_server;
  }

  // Sets ID of the RenderProcessHost we expect the navigation to commit in.
  // This is used to inform the RenderProcessHost to expect a navigation to the
  // url we're navigating to.
  void SetExpectedProcess(RenderProcessHost* expected_process);

  NavigationThrottle* GetDeferringThrottleForTesting() const {
    return GetDeferringThrottle();
  }

  // Sets the READY_TO_COMMIT -> DID_COMMIT timeout.  Resets the timeout to the
  // default value if |timeout| is zero.
  static void SetCommitTimeoutForTesting(const base::TimeDelta& timeout);

 private:
  friend class NavigationHandleImplTest;

  NavigationHandleImpl(
      const GURL& url,
      const std::vector<GURL>& redirect_chain,
      FrameTreeNode* frame_tree_node,
      bool is_renderer_initiated,
      bool is_same_document,
      base::TimeTicks navigation_start,
      int pending_nav_entry_id,
      bool started_from_context_menu,
      CSPDisposition should_check_main_world_csp,
      bool is_form_submission,
      std::unique_ptr<NavigationUIData> navigation_ui_data,
      const std::string& method,
      net::HttpRequestHeaders request_headers,
      scoped_refptr<network::ResourceRequestBody> resource_request_body,
      const Referrer& sanitized_referrer,
      bool has_user_gesture,
      ui::PageTransition transition,
      bool is_external_protocol,
      blink::mojom::RequestContextType request_context_type,
      blink::WebMixedContentContextType mixed_content_context_type,
      base::TimeTicks input_start);

  NavigationThrottle::ThrottleCheckResult CheckWillStartRequest();
  NavigationThrottle::ThrottleCheckResult CheckWillRedirectRequest();
  NavigationThrottle::ThrottleCheckResult CheckWillFailRequest();
  NavigationThrottle::ThrottleCheckResult CheckWillProcessResponse();

  void ResumeInternal();
  void CancelDeferredNavigationInternal(
      NavigationThrottle::ThrottleCheckResult result);

  // Helper function to run and reset the |complete_callback_|. This marks the
  // end of a round of NavigationThrottleChecks.
  void RunCompleteCallback(NavigationThrottle::ThrottleCheckResult result);

  // Used in tests.
  State state() const { return state_; }

  // Populates |throttles_| with the throttles for this navigation.
  void RegisterNavigationThrottles();

  // Takes ownership of |throttle| (if any) and appends it to |throttles_|.
  void AddThrottle(std::unique_ptr<NavigationThrottle> throttle);

  // Checks for attempts to navigate to a page that is already referenced more
  // than once in the frame's ancestors.  This is a helper function used by
  // WillStartRequest and WillRedirectRequest to prevent the navigation.
  bool IsSelfReferentialURL();

  // Updates the destination site URL for this navigation. This is called on
  // redirects. |post_redirect_process| is the renderer process that should
  // handle the navigation following the redirect if it can be handled by an
  // existing RenderProcessHost. Otherwise, it should be null.
  void UpdateSiteURL(RenderProcessHost* post_redirect_process);

  // Returns the throttle that is currently deferring the navigation (i.e. the
  // throttle at index |next_index_ -1|). If the handle is not deferred, returns
  // nullptr;
  NavigationThrottle* GetDeferringThrottle() const;

  // Called if READY_TO_COMMIT -> COMMIT state transition takes an unusually
  // long time.
  void OnCommitTimeout();

  void RestartCommitTimeout();

  // See NavigationHandle for a description of those member variables.
  GURL url_;
  scoped_refptr<SiteInstance> starting_site_instance_;
  Referrer sanitized_referrer_;
  bool has_user_gesture_;
  ui::PageTransition transition_;
  bool is_external_protocol_;
  net::Error net_error_code_;
  RenderFrameHostImpl* render_frame_host_;
  const bool is_renderer_initiated_;
  const bool is_same_document_;
  bool was_redirected_;
  bool did_replace_entry_;
  bool should_update_history_;
  bool subframe_entry_committed_;
  scoped_refptr<net::HttpResponseHeaders> response_headers_;
  net::HttpResponseInfo::ConnectionInfo connection_info_;
  net::SSLInfo ssl_info_;

  // The original url of the navigation. This may differ from |url_| if the
  // navigation encounters redirects.
  const GURL original_url_;

  // The site URL of this navigation, as obtained from SiteInstance::GetSiteURL.
  GURL site_url_;

  // The HTTP method used for the navigation.
  std::string method_;

  // The headers used for the request.
  net::HttpRequestHeaders request_headers_;

  // The POST body associated with this navigation.  This will be null for GET
  // and/or other non-POST requests (or if a response to a POST request was a
  // redirect that changed the method to GET - for example 302).
  scoped_refptr<network::ResourceRequestBody> resource_request_body_;

  // The state the navigation is in.
  State state_;

  // The FrameTreeNode this navigation is happening in.
  FrameTreeNode* frame_tree_node_;

  // A list of Throttles registered for this navigation.
  std::vector<std::unique_ptr<NavigationThrottle>> throttles_;

  // The index of the next throttle to check.
  size_t next_index_;

  // The time this navigation started.
  const base::TimeTicks navigation_start_;

  // The time the input event that lead to this navigation started.
  // Currently available only if the navigation was initiated by
  // the user clicking a link in the renderer.
  const base::TimeTicks input_start_;

  // The time this naviagtion was ready to commit.
  base::TimeTicks ready_to_commit_time_;

  // Timer for detecting an unexpectedly long time to commit a navigation.
  base::OneShotTimer commit_timeout_timer_;

  // The unique id of the corresponding NavigationEntry.
  int pending_nav_entry_id_;

  // The fetch request context type.
  blink::mojom::RequestContextType request_context_type_;

  // The mixed content context type for potential mixed content checks.
  blink::WebMixedContentContextType mixed_content_context_type_;

  // This callback will be run when all throttle checks have been performed. Be
  // careful about relying on it as the member may be removed as part of the
  // PlzNavigate refactoring.
  ThrottleChecksFinishedCallback complete_callback_;

  // This test-only callback will be run when all throttle checks have been
  // performed.
  // TODO(clamy): Revisit the unit test architecture when PlzNavigate ships.
  ThrottleChecksFinishedCallback complete_callback_for_testing_;

  // Manages the lifetime of a pre-created ServiceWorkerProviderHost until a
  // corresponding ServiceWorkerNetworkProvider is created in the renderer.
  std::unique_ptr<ServiceWorkerNavigationHandle> service_worker_handle_;

  // Manages the lifetime of a pre-created AppCacheHost until a browser side
  // navigation is ready to be committed, i.e we have a renderer process ready
  // to service the navigation request.
  std::unique_ptr<AppCacheNavigationHandle> appcache_handle_;

  // Embedder data from the IO thread tied to this navigation.
  std::unique_ptr<NavigationData> navigation_data_;

  // Embedder data from the UI thread tied to this navigation.
  std::unique_ptr<NavigationUIData> navigation_ui_data_;

  // The unique id to identify this to navigation with.
  int64_t navigation_id_;

  // The id of the URLRequest tied to this navigation.
  GlobalRequestID request_id_;

  // The chain of redirects.
  std::vector<GURL> redirect_chain_;

  // Stores the reload type, or NONE if it's not a reload.
  ReloadType reload_type_;

  // Stores the restore type, or NONE it it's not a restore.
  RestoreType restore_type_;

  GURL searchable_form_url_;
  std::string searchable_form_encoding_;

  GURL previous_url_;
  GURL base_url_;
  GURL base_url_for_data_url_;
  net::HostPortPair socket_address_;
  NavigationType navigation_type_;

  // Whether or not the CSP of the main world should apply. When the navigation
  // is initiated from a content script in an isolated world, the CSP defined
  // in the main world should not apply.
  CSPDisposition should_check_main_world_csp_;

  // Information about the JavaScript that started the navigation. For
  // navigations initiated by Javascript.
  SourceLocation source_location_;

  // Used to inform a RenderProcessHost that we expect this navigation to commit
  // in it.
  int expected_render_process_host_id_;

  // The origin policy that applies to this navigation. Empty if none applies.
  std::string origin_policy_;

  // Whether or not the navigation results from the submission of a form.
  bool is_form_submission_;

  // Whether the current NavigationEntry should be replaced upon commit.
  bool should_replace_current_entry_;

  // Whether the navigation ended up being a download or a stream.
  bool is_download_;
  bool is_stream_;

  // True if the target is an inner response of a signed exchange.
  bool is_signed_exchange_inner_response_;

  // Whether the response was cached.
  bool was_cached_;

  // Which proxy server was used for this navigation, if any.
  net::ProxyServer proxy_server_;

  // False by default unless the navigation started within a context menu.
  bool started_from_context_menu_;

  // Set in ReadyToCommitNavigation.
  bool is_same_process_;

  base::WeakPtrFactory<NavigationHandleImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(NavigationHandleImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_FRAME_HOST_NAVIGATION_HANDLE_IMPL_H_
