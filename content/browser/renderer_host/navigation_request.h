// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_NAVIGATION_REQUEST_H_
#define CONTENT_BROWSER_RENDERER_HOST_NAVIGATION_REQUEST_H_

#include <memory>

#include "base/callback.h"
#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "content/browser/initiator_csp_context.h"
#include "content/browser/loader/navigation_url_loader_delegate.h"
#include "content/browser/navigation_subresource_loader_params.h"
#include "content/browser/renderer_host/cross_origin_opener_policy_status.h"
#include "content/browser/renderer_host/navigation_controller_impl.h"
#include "content/browser/renderer_host/navigation_entry_impl.h"
#include "content/browser/renderer_host/navigation_throttle_runner.h"
#include "content/browser/site_instance_impl.h"
#include "content/browser/web_package/web_bundle_handle.h"
#include "content/common/content_export.h"
#include "content/common/navigation_params.h"
#include "content/common/navigation_params.mojom.h"
#include "content/public/browser/allow_service_worker_result.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/navigation_type.h"
#include "content/public/browser/peak_gpu_memory_tracker.h"
#include "content/public/browser/render_process_host_observer.h"
#include "content/public/common/impression.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "net/base/proxy_server.h"
#include "net/dns/public/resolve_error_info.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/network/public/cpp/origin_policy.h"
#include "services/network/public/mojom/blocked_by_response_reason.mojom-shared.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "services/network/public/mojom/web_sandbox_flags.mojom-shared.h"
#include "third_party/blink/public/common/loader/previews_state.h"

#if defined(OS_ANDROID)
#include "base/android/scoped_java_ref.h"
#include "content/browser/android/navigation_handle_proxy.h"
#endif

namespace base {
namespace trace_event {
class TracedValue;
}  // namespace trace_event
}  // namespace base

namespace network {
class ResourceRequestBody;
struct URLLoaderCompletionStatus;
}  // namespace network

struct FrameHostMsg_DidCommitProvisionalLoad_Params;

namespace content {

class AppCacheNavigationHandle;
class CrossOriginEmbedderPolicyReporter;
class WebBundleHandleTracker;
class WebBundleNavigationInfo;
class FrameNavigationEntry;
class FrameTreeNode;
class NavigationURLLoader;
class NavigationUIData;
class NavigatorDelegate;
class PrefetchedSignedExchangeCache;
class ServiceWorkerMainResourceHandle;
struct SubresourceLoaderParams;

// A UI thread object that owns a navigation request until it commits. It
// ensures the UI thread can start a navigation request in the
// ResourceDispatcherHost (that lives on the IO thread).
// TODO(clamy): Describe the interactions between the UI and IO thread during
// the navigation following its refactoring.
class CONTENT_EXPORT NavigationRequest
    : public NavigationHandle,
      public NavigationURLLoaderDelegate,
      public NavigationThrottleRunner::Delegate,
      private RenderProcessHostObserver,
      private network::mojom::CookieAccessObserver {
 public:
  // Keeps track of the various stages of a NavigationRequest.
  // To see what state transitions are allowed, see |SetState|.
  enum NavigationState {
    // Initial state.
    NOT_STARTED = 0,

    // Waiting for a BeginNavigation IPC from the renderer in a
    // browser-initiated navigation. If there is no live renderer when the
    // request is created, this stage is skipped.
    WAITING_FOR_RENDERER_RESPONSE,

    // TODO(zetamoo): Merge this state with WILL_START_REQUEST.
    // Temporary state where:
    //  - Before unload handlers have run and this navigation is allowed to
    //    start.
    //  - The navigation is still not visible to embedders (via
    //    NavigationHandle).
    WILL_START_NAVIGATION,

    // The navigation is visible to embedders (via NavigationHandle). Wait for
    // the NavigationThrottles to finish running the WillStartRequest event.
    // This is potentially asynchronous.
    WILL_START_REQUEST,

    // The request is being redirected. Wait for the NavigationThrottles to
    // finish running the WillRedirectRequest event. This is potentially
    // asynchronous.
    WILL_REDIRECT_REQUEST,

    // The response is being processed. Wait for the NavigationThrottles to
    // finish running the WillProcessResponse event. This is potentially
    // asynchronous.
    WILL_PROCESS_RESPONSE,

    // The response started on the IO thread and is ready to be committed.
    READY_TO_COMMIT,

    // The response has been committed. This is one of the two final states of
    // the request.
    DID_COMMIT,

    // The request is being canceled.
    CANCELING,

    // The request is failing. Wait for the NavigationThrottles to finish
    // running the WillFailRequest event. This is potentially asynchronous.
    WILL_FAIL_REQUEST,

    // The request failed on the IO thread and an error page should be
    // displayed. This is one of the two final states for the request.
    DID_COMMIT_ERROR_PAGE,
  };

  // The SiteInstance currently associated with the navigation. Note that the
  // final value will only be known when the response is received, or the
  // navigation fails, as server redirects can modify the SiteInstance to use
  // for the navigation.
  enum class AssociatedSiteInstanceType {
    NONE = 0,
    CURRENT,
    SPECULATIVE,
  };

  // Creates a request for a browser-initiated navigation.
  // Note: this is sometimes called for renderer-initiated navigations going
  // through the OpenURL path. |browser_initiated| should be false in that case.
  // TODO(clamy): Rename this function and consider merging it with
  // CreateRendererInitiated.
  static std::unique_ptr<NavigationRequest> CreateBrowserInitiated(
      FrameTreeNode* frame_tree_node,
      mojom::CommonNavigationParamsPtr common_params,
      mojom::CommitNavigationParamsPtr commit_params,
      bool browser_initiated,
      const GlobalFrameRoutingId& initiator_routing_id,
      const std::string& extra_headers,
      FrameNavigationEntry* frame_entry,
      NavigationEntryImpl* entry,
      const scoped_refptr<network::ResourceRequestBody>& post_body,
      std::unique_ptr<NavigationUIData> navigation_ui_data,
      const base::Optional<Impression>& impression);

  // Creates a request for a renderer-initiated navigation.
  // Note: |body| is sent to the IO thread when calling BeginNavigation, and
  // should no longer be manipulated afterwards on the UI thread.
  // TODO(clamy): see if ResourceRequestBody could be un-refcounted to avoid
  // threading subtleties.
  static std::unique_ptr<NavigationRequest> CreateRendererInitiated(
      FrameTreeNode* frame_tree_node,
      NavigationEntryImpl* entry,
      mojom::CommonNavigationParamsPtr common_params,
      mojom::BeginNavigationParamsPtr begin_params,
      int current_history_list_offset,
      int current_history_list_length,
      bool override_user_agent,
      scoped_refptr<network::SharedURLLoaderFactory> blob_url_loader_factory,
      mojo::PendingAssociatedRemote<mojom::NavigationClient> navigation_client,
      mojo::PendingRemote<blink::mojom::NavigationInitiator>
          navigation_initiator,
      scoped_refptr<PrefetchedSignedExchangeCache>
          prefetched_signed_exchange_cache,
      std::unique_ptr<WebBundleHandleTracker> web_bundle_handle_tracker);

  // Creates a request at commit time. This should only be used for
  // renderer-initiated same-document navigations, and navigations whose
  // original NavigationRequest has been destroyed by race-conditions.
  // TODO(clamy): Eventually, this should only be called for same-document
  // renderer-initiated navigations.
  static std::unique_ptr<NavigationRequest> CreateForCommit(
      FrameTreeNode* frame_tree_node,
      RenderFrameHostImpl* render_frame_host,
      const FrameHostMsg_DidCommitProvisionalLoad_Params& params,
      std::unique_ptr<CrossOriginEmbedderPolicyReporter> coep_reporter,
      bool is_same_document,
      std::unique_ptr<WebBundleNavigationInfo> web_bundle_navigation_info);

  static NavigationRequest* From(NavigationHandle* handle);

  // If |type| is a reload, returns the equivalent ReloadType. Otherwise returns
  // ReloadType::NONE.
  static ReloadType NavigationTypeToReloadType(mojom::NavigationType type);

  ~NavigationRequest() override;

  // Returns true if this request's URL matches |origin| and the request state
  // is at (or past) WILL_PROCESS_RESPONSE.
  bool HasCommittingOrigin(const url::Origin& origin);

  // Returns whether and how this navigation request is requesting opt-in
  // origin-isolation.
  enum class OptInIsolationCheckResult {
    NONE,          // no isolation requested
    HEADER,        // requested using the Origin-Isolation header
    ORIGIN_POLICY  // requested using origin policy
  };
  OptInIsolationCheckResult IsOptInIsolationRequested(const GURL& url);

  // The origin isolation end result is determined early in the lifecycle of a
  // NavigationRequest, but used late. In particular, we want to trigger use
  // counters and console warnings once navigation has committed.
  enum class OptInOriginIsolationEndResult {
    kNotRequestedAndNotIsolated,
    kNotRequestedButIsolated,
    kRequestedViaOriginPolicyButNotIsolated,
    kRequestedViaOriginPolicyAndIsolated,
    kRequestedViaHeaderButNotIsolated,
    kRequestedViaHeaderAndIsolated
  };
  void DetermineOriginIsolationEndResult(
      OptInIsolationCheckResult check_result);
  void ProcessOriginIsolationEndResult();

  // NavigationHandle implementation:
  int64_t GetNavigationId() override;
  ukm::SourceId GetNextPageUkmSourceId() override;
  const GURL& GetURL() override;
  SiteInstanceImpl* GetStartingSiteInstance() override;
  SiteInstanceImpl* GetSourceSiteInstance() override;
  bool IsInMainFrame() override;
  bool IsParentMainFrame() override;
  bool IsRendererInitiated() override;
  bool WasServerRedirect() override;
  const std::vector<GURL>& GetRedirectChain() override;
  int GetFrameTreeNodeId() override;
  RenderFrameHostImpl* GetParentFrame() override;
  base::TimeTicks NavigationStart() override;
  base::TimeTicks NavigationInputStart() override;
  const NavigationHandleTiming& GetNavigationHandleTiming() override;
  bool IsPost() override;
  const blink::mojom::Referrer& GetReferrer() override;
  void SetReferrer(blink::mojom::ReferrerPtr referrer) override;
  bool HasUserGesture() override;
  ui::PageTransition GetPageTransition() override;
  NavigationUIData* GetNavigationUIData() override;
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
  net::IPEndPoint GetSocketAddress() override;
  const net::HttpRequestHeaders& GetRequestHeaders() override;
  void RemoveRequestHeader(const std::string& header_name) override;
  void SetRequestHeader(const std::string& header_name,
                        const std::string& header_value) override;
  void SetCorsExemptRequestHeader(const std::string& header_name,
                                  const std::string& header_value) override;
  const net::HttpResponseHeaders* GetResponseHeaders() override;
  net::HttpResponseInfo::ConnectionInfo GetConnectionInfo() override;
  const base::Optional<net::SSLInfo>& GetSSLInfo() override;
  const base::Optional<net::AuthChallengeInfo>& GetAuthChallengeInfo() override;
  net::ResolveErrorInfo GetResolveErrorInfo() override;
  net::IsolationInfo GetIsolationInfo() override;
  void RegisterThrottleForTesting(
      std::unique_ptr<NavigationThrottle> navigation_throttle) override;
  bool IsDeferredForTesting() override;
  bool WasStartedFromContextMenu() override;
  const GURL& GetSearchableFormURL() override;
  const std::string& GetSearchableFormEncoding() override;
  ReloadType GetReloadType() override;
  RestoreType GetRestoreType() override;
  const GURL& GetBaseURLForDataURL() override;
  const GlobalRequestID& GetGlobalRequestID() override;
  bool IsDownload() override;
  bool IsFormSubmission() override;
  bool WasInitiatedByLinkClick() override;
  bool IsSignedExchangeInnerResponse() override;
  bool HasPrefetchedAlternativeSubresourceSignedExchange() override;
  bool WasResponseCached() override;
  const net::ProxyServer& GetProxyServer() override;
  const std::string& GetHrefTranslate() override;
  const base::Optional<Impression>& GetImpression() override;
  const GlobalFrameRoutingId& GetInitiatorRoutingId() override;
  const base::Optional<url::Origin>& GetInitiatorOrigin() override;
  bool IsSameProcess() override;
  NavigationEntry* GetNavigationEntry() override;
  int GetNavigationEntryOffset() override;
  void RegisterSubresourceOverride(
      blink::mojom::TransferrableURLLoaderPtr transferrable_loader) override;
  GlobalFrameRoutingId GetPreviousRenderFrameHostId() override;
  bool IsServedFromBackForwardCache() override;
  void SetIsOverridingUserAgent(bool override_ua) override;
  bool GetIsOverridingUserAgent() override;
  void SetSilentlyIgnoreErrors() override;

  // Called on the UI thread by the Navigator to start the navigation.
  // The NavigationRequest can be deleted while BeginNavigation() is called.
  void BeginNavigation();

  void ForceCSPForResponse(const std::string& csp);

  const mojom::CommonNavigationParams& common_params() const {
    return *common_params_;
  }

  const mojom::BeginNavigationParams* begin_params() const {
    return begin_params_.get();
  }

  const mojom::CommitNavigationParams& commit_params() const {
    return *commit_params_;
  }

  // Updates the navigation start time.
  void set_navigation_start_time(const base::TimeTicks& time) {
    common_params_->navigation_start = time;
  }

  bool did_same_site_proactive_browsing_instance_swap() {
    return did_same_site_proactive_browsing_instance_swap_;
  }

  void set_did_same_site_proactive_browsing_instance_swap(
      bool did_same_site_proactive_browsing_instance_swap) {
    did_same_site_proactive_browsing_instance_swap_ =
        did_same_site_proactive_browsing_instance_swap;
  }

  NavigationURLLoader* loader_for_testing() const { return loader_.get(); }

  NavigationState state() const { return state_; }

  FrameTreeNode* frame_tree_node() const { return frame_tree_node_; }

  SiteInstanceImpl* dest_site_instance() const {
    return dest_site_instance_.get();
  }

  bool is_view_source() const { return is_view_source_; }

  int bindings() const { return bindings_; }

  bool browser_initiated() const { return browser_initiated_; }

  bool from_begin_navigation() const { return from_begin_navigation_; }

  AssociatedSiteInstanceType associated_site_instance_type() const {
    return associated_site_instance_type_;
  }
  void set_associated_site_instance_type(AssociatedSiteInstanceType type) {
    associated_site_instance_type_ = type;
  }

  void set_was_discarded() { commit_params_->was_discarded = true; }

  void set_net_error(net::Error net_error) { net_error_ = net_error; }

  const std::string& GetMimeType() {
    return response_head_ ? response_head_->mime_type : base::EmptyString();
  }

  const network::mojom::URLResponseHead* response() {
    return response_head_.get();
  }

  void SetWaitingForRendererResponse();

  // Notifies the NavigatorDelegate the navigation started. This should be
  // called after any previous NavigationRequest for the FrameTreeNode has been
  // destroyed. |is_for_commit| should only be true when creating a
  // NavigationRequest at commit time (this happens for renderer-initiated
  // same-document navigations).
  void StartNavigation(bool is_for_commit);

  void set_on_start_checks_complete_closure_for_testing(
      base::OnceClosure closure) {
    on_start_checks_complete_closure_ = std::move(closure);
  }

  // Sets ID of the RenderProcessHost we expect the navigation to commit in.
  // This is used to inform the RenderProcessHost to expect a navigation to the
  // url we're navigating to.
  void SetExpectedProcess(RenderProcessHost* expected_process);

  // Updates the destination SiteInfo for this navigation. This is called on
  // redirects. |post_redirect_process| is the renderer process that should
  // handle the navigation following the redirect if it can be handled by an
  // existing RenderProcessHost. Otherwise, it should be null.
  // |is_coop_coep_cross_origin_isolated| &
  // |coop_coep_cross_origin_isolated_origin| is the new COOP/COEP info
  // extracted from the redirect response.
  void UpdateSiteInfo(
      bool is_coop_coep_cross_origin_isolated,
      const base::Optional<url::Origin>& coop_coep_cross_origin_isolated_origin,
      RenderProcessHost* post_redirect_process);

  int nav_entry_id() const { return nav_entry_id_; }

  bool was_set_overriding_user_agent_called() const {
    return was_set_overriding_user_agent_called_;
  }

  bool entry_overrides_ua() const { return entry_overrides_ua_; }

  // For automation driver-initiated navigations over the devtools protocol,
  // |devtools_navigation_token_| is used to tag the navigation. This navigation
  // token is then sent into the renderer and lands on the DocumentLoader. That
  // way subsequent Blink-level frame lifecycle events can be associated with
  // the concrete navigation.
  // - The value should not be sent back to the browser.
  // - The value on DocumentLoader may be generated in the renderer in some
  // cases, and thus shouldn't be trusted.
  // TODO(crbug.com/783506): Replace devtools navigation token with the generic
  // navigation token that can be passed from renderer to the browser.
  const base::UnguessableToken& devtools_navigation_token() const {
    return devtools_navigation_token_;
  }

  // Called on same-document navigation requests that need to be restarted as
  // cross-document navigations. This happens when a same-document commit fails
  // due to another navigation committing in the meantime.
  void ResetForCrossDocumentRestart();

  // If the navigation redirects cross-process or otherwise is forced to use a
  // different SiteInstance than anticipated (e.g., for switching between error
  // states), then reset any sensitive state that shouldn't carry over to the
  // new process.
  void ResetStateForSiteInstanceChange();

  // Lazily initializes and returns the mojo::NavigationClient interface used
  // for commit. Only used with PerNavigationMojoInterface enabled.
  mojom::NavigationClient* GetCommitNavigationClient();

  void set_transition(ui::PageTransition transition) {
    common_params_->transition = transition;
  }

  void set_has_user_gesture(bool has_user_gesture) {
    common_params_->has_user_gesture = has_user_gesture;
  }

  // Ignores any interface disconnect that might happen to the
  // navigation_client used to commit.
  void IgnoreCommitInterfaceDisconnection();

  // Resume and CancelDeferredNavigation must only be called by the
  // NavigationThrottle that is currently deferring the navigation.
  // |resuming_throttle| and |cancelling_throttle| are the throttles calling
  // these methods.
  void Resume(NavigationThrottle* resuming_throttle);
  void CancelDeferredNavigation(NavigationThrottle* cancelling_throttle,
                                NavigationThrottle::ThrottleCheckResult result);

  // Returns the underlying NavigationThrottleRunner for tests to manipulate.
  NavigationThrottleRunner* GetNavigationThrottleRunnerForTesting() {
    return throttle_runner_.get();
  }

  // Simulates renderer aborting navigation.
  void RendererAbortedNavigationForTesting();

  typedef base::OnceCallback<bool(NavigationThrottle::ThrottleCheckResult)>
      ThrottleChecksFinishedCallback;

  NavigationThrottle* GetDeferringThrottleForTesting() const {
    return throttle_runner_->GetDeferringThrottle();
  }

  // Called when the navigation was committed.
  // This will update the |state_|.
  // |navigation_entry_committed| indicates whether the navigation changed which
  // NavigationEntry is current.
  // |did_replace_entry| is true if the committed entry has replaced the
  // existing one. A non-user initiated redirect causes such replacement.
  void DidCommitNavigation(
      const FrameHostMsg_DidCommitProvisionalLoad_Params& params,
      bool navigation_entry_committed,
      bool did_replace_entry,
      const GURL& previous_url,
      NavigationType navigation_type);

  NavigationType navigation_type() const {
    DCHECK(state_ == DID_COMMIT || state_ == DID_COMMIT_ERROR_PAGE);
    return navigation_type_;
  }

#if defined(OS_ANDROID)
  // Returns a reference to |navigation_handle_| Java counterpart. It is used
  // by Java WebContentsObservers.
  base::android::ScopedJavaGlobalRef<jobject> java_navigation_handle() {
    return navigation_handle_proxy_->java_navigation_handle();
  }
#endif

  const std::string& post_commit_error_page_html() {
    return post_commit_error_page_html_;
  }

  void set_post_commit_error_page_html(
      const std::string& post_commit_error_page_html) {
    post_commit_error_page_html_ = post_commit_error_page_html;
  }

  void set_from_download_cross_origin_redirect(
      bool from_download_cross_origin_redirect) {
    from_download_cross_origin_redirect_ = from_download_cross_origin_redirect;
  }

  // This should be a private method. The only valid reason to be used
  // outside of the class constructor is in the case of an initial history
  // navigation in a subframe. This allows a browser-initiated NavigationRequest
  // to be canceled by the renderer.
  void SetNavigationClient(
      mojo::PendingAssociatedRemote<mojom::NavigationClient> navigation_client);

  // Whether the new document created by this navigation will be loaded from a
  // MHTML document. In this case, the navigation will commit in the main frame
  // process without needing any network requests.
  bool IsForMhtmlSubframe() const;

  std::unique_ptr<AppCacheNavigationHandle> TakeAppCacheHandle();

  AppCacheNavigationHandle* appcache_handle() const {
    return appcache_handle_.get();
  }

  void set_complete_callback_for_testing(
      ThrottleChecksFinishedCallback callback) {
    complete_callback_for_testing_ = std::move(callback);
  }

  // Sets the READY_TO_COMMIT -> DID_COMMIT timeout. Resets the timeout to the
  // default value if |timeout| is zero.
  static void SetCommitTimeoutForTesting(const base::TimeDelta& timeout);

  RenderFrameHostImpl* rfh_restored_from_back_forward_cache() {
    return rfh_restored_from_back_forward_cache_;
  }

  const WebBundleNavigationInfo* web_bundle_navigation_info() const {
    return web_bundle_navigation_info_.get();
  }

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
    return begin_params_->request_context_type;
  }

  network::mojom::RequestDestination request_destination() const {
    return begin_params_->request_destination;
  }

  blink::WebMixedContentContextType mixed_content_context_type() const {
    return begin_params_->mixed_content_context_type;
  }

  // Returns true if the navigation was started by the Navigator by calling
  // BeginNavigation(), or if the request was created at commit time by calling
  // CreateForCommit().
  bool IsNavigationStarted() const;

  // Restart the navigation restoring the page from the back-forward cache
  // as a regular non-bfcached history navigation.
  //
  // The restart itself is asychronous as it's dangerous to restart navigation
  // with arbitrary state on the stack (another navigation might be starting,
  // so this function only posts the actual task to do all the work (see
  // RestartBackForwardCachedNavigationImpl);
  void RestartBackForwardCachedNavigation();

  std::unique_ptr<PeakGpuMemoryTracker> TakePeakGpuMemoryTracker();

  // Returns true for navigation responses to be rendered in a renderer process.
  // This excludes:
  //  - 204/205 navigation responses.
  //  - downloads.
  //
  // Must not be called before having received the response.
  bool response_should_be_rendered() const {
    DCHECK_GE(state_, WILL_PROCESS_RESPONSE);
    return response_should_be_rendered_;
  }

  const network::mojom::ClientSecurityStatePtr& client_security_state() const {
    return client_security_state_;
  }
  network::mojom::ClientSecurityStatePtr TakeClientSecurityState();

  bool ua_change_requires_reload() const { return ua_change_requires_reload_; }

  const network::mojom::ContentSecurityPolicy* required_csp() {
    return required_csp_.get();
  }
  void SetRequiredCSP(network::mojom::ContentSecurityPolicyPtr csp);
  network::mojom::ContentSecurityPolicyPtr TakeRequiredCSP();

  CrossOriginEmbedderPolicyReporter* coep_reporter() {
    return coep_reporter_.get();
  }

  std::unique_ptr<CrossOriginEmbedderPolicyReporter> TakeCoepReporter();

  // Returns UKM SourceId for the page we are navigating away from.
  // Equal to GetRenderFrameHost()->GetPageUkmSourceId() for subframe
  // and same-document navigations and to
  // RenderFrameHost::FromID(GetPreviousRenderFrameHostId())
  //     ->GetPageUkmSourceId() for main-frame cross-document navigations.
  ukm::SourceId GetPreviousPageUkmSourceId();

  void OnServiceWorkerAccessed(const GURL& scope,
                               AllowServiceWorkerResult allowed);

  // Take all cookie observers associated with this navigation.
  // Typically this is called when navigation commits to move these observers to
  // the committed document.
  std::vector<mojo::PendingReceiver<network::mojom::CookieAccessObserver>>
  TakeCookieObservers() WARN_UNUSED_RESULT;

  // The sandbox policy of the document to be loaded. This returns nullopt for
  // navigations that haven't reached the 'ReadyToCommit' stage yet. In
  // particular, this returns nullopt for same-document navigations.
  //
  // TODO(arthursonzogni): After RenderDocument, this can be computed and stored
  // directly into the RenderDocumentHost.
  base::Optional<network::mojom::WebSandboxFlags> SandboxFlagsToCommit();

  // Returns the coop status information relevant to the current navigation.
  CrossOriginOpenerPolicyStatus& coop_status() { return coop_status_; }

  // Whether the navigation was sent to be committed in a renderer by the
  // RenderFrameHost. This can either be for the commit of a successful
  // navigation or an error page.
  bool IsWaitingToCommit();

  // Returns true if |url| and |base_url| represent a WebView
  // loadDataWithBaseUrl navigation.
  static bool IsLoadDataWithBaseURL(const GURL& url, const GURL& base_url);

  // Returns true if |common_params| represents a WebView loadDataWithBaseUrl
  // navigation.
  static bool IsLoadDataWithBaseURL(
      const mojom::CommonNavigationParams& common_params);

  // Will calculate an *approximation* of the origin that this NavigationRequest
  // will commit.  (An "approximation", because sandboxing is not taken into
  // account - see https://crbug.com/1041376.  The approximation is still good
  // enough for |request_initiator_origin_lock| in
  // network::mojom::URLLoaderFactoryParams.)
  //
  // This method depends on GetRenderFrameHost() and therefore can only be
  // called after a response has been delivered for processing, or after the
  // navigation fails with an error page.
  //
  // TODO(lukasza, arthursonzogni): https://crbug.com/888079: Once the browser
  // process is able to calculate the exact origin to commit, the method below
  // should be renamed to something like GetOriginToCommit().
  url::Origin GetOriginForURLLoaderFactory();

  // Add information about this NavigationRequest to |traced_value| for
  // tracing purposes.
  void AsValueInto(base::trace_event::TracedValue* traced_value);

  mojo::ScopedDataPipeConsumerHandle& mutable_response_body_for_testing() {
    return response_body_;
  }

  // If this navigation fails with net::ERR_BLOCKED_BY_CLIENT, act as if it were
  // cancelled by the user and do not commit an error page.
  void SetSilentlyIgnoreBlockedByClient() {
    silently_ignore_blocked_by_client_ = true;
  }

  // Returns the current url from GetURL() packaged with other state required to
  // properly determine SiteInstances and process allocation.
  UrlInfo GetUrlInfo();

 private:
  friend class NavigationRequestTest;

  NavigationRequest(
      FrameTreeNode* frame_tree_node,
      mojom::CommonNavigationParamsPtr common_params,
      mojom::BeginNavigationParamsPtr begin_params,
      mojom::CommitNavigationParamsPtr commit_params,
      bool browser_initiated,
      bool from_begin_navigation,
      bool is_for_commit,
      const FrameNavigationEntry* frame_navigation_entry,
      NavigationEntryImpl* navitation_entry,
      std::unique_ptr<NavigationUIData> navigation_ui_data,
      mojo::PendingAssociatedRemote<mojom::NavigationClient> navigation_client,
      mojo::PendingRemote<blink::mojom::NavigationInitiator>
          navigation_initiator,
      RenderFrameHostImpl* rfh_restored_from_back_forward_cache,
      GlobalFrameRoutingId initiator_routing_id);

  // Checks if the response requests an isolated origin (using either origin
  // policy or the Origin-Isolation header), and if so opts in the origin to be
  // isolated.
  void CheckForIsolationOptIn(const GURL& url);

  // NavigationURLLoaderDelegate implementation.
  void OnRequestRedirected(
      const net::RedirectInfo& redirect_info,
      network::mojom::URLResponseHeadPtr response_head) override;
  void OnResponseStarted(
      network::mojom::URLLoaderClientEndpointsPtr url_loader_client_endpoints,
      network::mojom::URLResponseHeadPtr response_head,
      mojo::ScopedDataPipeConsumerHandle response_body,
      const GlobalRequestID& request_id,
      bool is_download,
      NavigationDownloadPolicy download_policy,
      base::Optional<SubresourceLoaderParams> subresource_loader_params)
      override;
  void OnRequestFailed(
      const network::URLLoaderCompletionStatus& status) override;
  void OnRequestStarted(base::TimeTicks timestamp) override;

  // To be called whenever a navigation request fails. If |skip_throttles| is
  // true, the registered NavigationThrottle(s) won't get a chance to intercept
  // NavigationThrottle::WillFailRequest. It should be used when a request
  // failed due to a throttle result itself. |error_page_content| is only used
  // when |skip_throttles| is true. If |collapse_frame| is true, the associated
  // frame tree node is collapsed.
  void OnRequestFailedInternal(
      const network::URLLoaderCompletionStatus& status,
      bool skip_throttles,
      const base::Optional<std::string>& error_page_content,
      bool collapse_frame);

  // Helper to determine whether an error page for the provided error code
  // should stay in the current process.
  bool ShouldKeepErrorPageInCurrentProcess(int net_error);

  // Called when the NavigationThrottles have been checked by the
  // NavigationHandle.
  void OnStartChecksComplete(NavigationThrottle::ThrottleCheckResult result);
  void OnRedirectChecksComplete(NavigationThrottle::ThrottleCheckResult result);
  void OnFailureChecksComplete(NavigationThrottle::ThrottleCheckResult result);
  void OnWillProcessResponseChecksComplete(
      NavigationThrottle::ThrottleCheckResult result);

  // Called either by OnFailureChecksComplete() or OnRequestFailed() directly.
  // |error_page_content| contains the content of the error page (i.e. flattened
  // HTML, JS, CSS).
  void CommitErrorPage(const base::Optional<std::string>& error_page_content);

  // Have a RenderFrameHost commit the navigation. The NavigationRequest will
  // be destroyed after this call.
  void CommitNavigation();

  // Checks if the specified CSP context's relevant CSP directive
  // allows the navigation. This is called to perform the frame-src
  // and navigate-to checks.
  bool IsAllowedByCSPDirective(
      network::CSPContext* context,
      network::mojom::CSPDirectiveName directive,
      bool has_followed_redirect,
      bool url_upgraded_after_redirect,
      bool is_response_check,
      network::CSPContext::CheckCSPDisposition disposition);

  // Checks if CSP allows the navigation. This will check the frame-src and
  // navigate-to directives.
  // Returns net::OK if the checks pass, and net::ERR_ABORTED or
  // net::ERR_BLOCKED_BY_CSP depending on which checks fail.
  net::Error CheckCSPDirectives(
      RenderFrameHostImpl* parent,
      bool has_followed_redirect,
      bool url_upgraded_after_redirect,
      bool is_response_check,
      network::CSPContext::CheckCSPDisposition disposition);

  // Check whether a request should be allowed to continue or should be blocked
  // because it violates a CSP. This method can have two side effects:
  // - If a CSP is configured to send reports and the request violates the CSP,
  //   a report will be sent.
  // - The navigation request may be upgraded from HTTP to HTTPS if a CSP is
  //   configured to upgrade insecure requests.
  net::Error CheckContentSecurityPolicy(bool has_followed_redirect,
                                        bool url_upgraded_after_redirect,
                                        bool is_response_check);

  // Builds the parameters used to commit a navigation to a page that was
  // restored from the back-forward cache.
  std::unique_ptr<FrameHostMsg_DidCommitProvisionalLoad_Params>
  MakeDidCommitProvisionalLoadParamsForBFCache();

  // This enum describes the result of the credentialed subresource check for
  // the request.
  enum class CredentialedSubresourceCheckResult {
    ALLOW_REQUEST,
    BLOCK_REQUEST,
  };

  // Chrome blocks subresource requests whose URLs contain embedded credentials
  // (e.g. `https://user:pass@example.com/page.html`). Check whether the
  // request should be allowed to continue or should be blocked.
  CredentialedSubresourceCheckResult CheckCredentialedSubresource() const;

  // This enum describes the result of the legacy protocol check for
  // the request.
  enum class LegacyProtocolInSubresourceCheckResult {
    ALLOW_REQUEST,
    BLOCK_REQUEST,
  };

  // Block subresources requests that target "legacy" protocol (like "ftp") when
  // the main document is not served from a "legacy" protocol.
  LegacyProtocolInSubresourceCheckResult CheckLegacyProtocolInSubresource()
      const;

  // Block about:srcdoc navigation that aren't expected to happen. For instance,
  // main frame navigations or about:srcdoc#foo.
  enum class AboutSrcDocCheckResult {
    ALLOW_REQUEST,
    BLOCK_REQUEST,
  };
  AboutSrcDocCheckResult CheckAboutSrcDoc() const;

  // Called before a commit. Updates the history index and length held in
  // CommitNavigationParams. This is used to update this shared state with the
  // renderer process.
  void UpdateCommitNavigationParamsHistory();

  // Called when an ongoing renderer-initiated navigation is aborted.
  // Only used with PerNavigationMojoInterface enabled.
  void OnRendererAbortedNavigation();

  // Binds the given error_handler to be called when an interface disconnection
  // happens on the renderer side.
  // Only used with PerNavigationMojoInterface enabled.
  void HandleInterfaceDisconnection(
      mojo::AssociatedRemote<mojom::NavigationClient>*,
      base::OnceClosure error_handler);

  // When called, this NavigationRequest will no longer interpret the interface
  // disconnection on the renderer side as an AbortNavigation.
  // TODO(ahemery): remove this function when NavigationRequest properly handles
  // interface disconnection in all cases.
  // Only used with PerNavigationMojoInterface enabled.
  void IgnoreInterfaceDisconnection();

  // Inform the RenderProcessHost to no longer expect a navigation.
  void ResetExpectedProcess();

  // If this is a same-site main-frame navigation where we did a proactive
  // BrowsingInstance swap but we're reusing the old page's process, we need
  // to send the routing ID and the updated lifecycle state of the old page so
  // that we can run pagehide and visibilitychange handlers of the old page
  // when we commit the new page.
  void AddOldPageInfoToCommitParamsIfNeeded();

  // Compute the history offset of the new document compared to the current one.
  // See navigation_history_offset_ for more details.
  int EstimateHistoryOffset();

  // Record download related UseCounters when navigation is a download before
  // filtered by download_policy.
  void RecordDownloadUseCountersPrePolicyCheck(
      NavigationDownloadPolicy download_policy);

  // Record download related UseCounters when navigation is a download after
  // filtered by download_policy.
  void RecordDownloadUseCountersPostPolicyCheck();

  // NavigationThrottleRunner::Delegate:
  void OnNavigationEventProcessed(
      NavigationThrottleRunner::Event event,
      NavigationThrottle::ThrottleCheckResult result) override;

  void OnWillStartRequestProcessed(
      NavigationThrottle::ThrottleCheckResult result);
  void OnWillRedirectRequestProcessed(
      NavigationThrottle::ThrottleCheckResult result);
  void OnWillFailRequestProcessed(
      NavigationThrottle::ThrottleCheckResult result);
  void OnWillProcessResponseProcessed(
      NavigationThrottle::ThrottleCheckResult result);

  void CancelDeferredNavigationInternal(
      NavigationThrottle::ThrottleCheckResult result);

  // TODO(zetamoo): Remove the Will* methods and fold them into their callers.

  // Called when the URLRequest will start in the network stack.
  void WillStartRequest();

  // Called when the URLRequest will be redirected in the network stack.
  // This will also inform the delegate that the request was redirected.
  //
  // |post_redirect_process| is the renderer process we expect to use to commit
  // the navigation now that it has been redirected. It can be null if there is
  // no live process that can be used. In that case, a suitable renderer process
  // will be created at commit time.
  //
  // |is_coop_coep_cross_origin_isolated| &
  // |coop_coep_cross_origin_isolated_origin| is the new COOP/COEP info
  // extracted from the redirect response.
  void WillRedirectRequest(
      const GURL& new_referrer_url,
      bool is_coop_coep_cross_origin_isolated,
      const base::Optional<url::Origin>& coop_coep_cross_origin_isolated_origin,
      RenderProcessHost* post_redirect_process);

  // Called when the URLRequest will fail.
  void WillFailRequest();

  // Called when the URLRequest has delivered response headers and metadata.
  // |callback| will be called when all throttle checks have completed,
  // allowing the caller to cancel the navigation or let it proceed.
  // NavigationHandle will not call |callback| with a result of DEFER.
  // If the result is PROCEED, then 'ReadyToCommitNavigation' will be called
  // just before calling |callback|.
  void WillProcessResponse();

  // Checks for attempts to navigate to a page that is already referenced more
  // than once in the frame's ancestors.  This is a helper function used by
  // WillStartRequest and WillRedirectRequest to prevent the navigation.
  bool IsSelfReferentialURL();

  // RenderProcessHostObserver implementation.
  void RenderProcessExited(RenderProcessHost* host,
                           const ChildProcessTerminationInfo& info) override;
  void RenderProcessHostDestroyed(RenderProcessHost* host) override;

  // Updates navigation handle timings.
  void UpdateNavigationHandleTimingsOnResponseReceived(bool is_first_response);
  void UpdateNavigationHandleTimingsOnCommitSent();

  // Helper function that computes the SiteInfo for |common_params_.url|.
  // Note: |site_info_| should only be updated with the result of this function.
  SiteInfo GetSiteInfoForCommonParamsURL(
      bool is_coop_coep_cross_origin_isolated,
      const base::Optional<url::Origin>&
          coop_coep_cross_origin_isolated_origin);

  // Updates the state of the navigation handle after encountering a server
  // redirect.
  void UpdateStateFollowingRedirect(const GURL& new_referrer_url);

  // NeedsUrlLoader() returns true if the navigation needs to use the
  // NavigationURLLoader for loading the document.
  //
  // A few types of navigations don't make any network requests. They can be
  // committed immediately in BeginNavigation(). They self-contain the data
  // needed for commit:
  // - about:blank: The renderer already knows how to load the empty document.
  // - about:srcdoc: The data is stored in the iframe srcdoc attribute.
  // - same-document: Only the history and URL are updated, no new document.
  // - MHTML subframe: The data is in the archive, owned by the main frame.
  //
  // Note #1: Even though "data:" URLs don't generate actual network requests,
  // including within MHTML subframes, they are still handled by the network
  // stack. The reason is that a few of them can't always be handled otherwise.
  // For instance:
  //  - the ones resulting in downloads.
  //  - the "invalid" ones. An error page is generated instead.
  //  - the ones with an unsupported MIME type.
  //  - the ones targeting the top-level frame on Android.
  //
  // Note #2: Even though "javascript:" URL and RendererDebugURL fit very well
  // in this category, they don't use the NavigationRequest.
  //
  // Note #3: Navigations that do not use a URL loader also bypass
  //          NavigationThrottle.
  bool NeedsUrlLoader();

  // Returns whether the ready-to-commit navigation will yield a secure context.
  //
  // Helper for UpdateClientSecurityState().
  //
  // Implements the following algorithm:
  // https://w3c.github.io/webappsec-secure-contexts/#is-settings-object-contextually-secure
  bool IsWebSecureContext() const;

  // Updates |client_security_state_| during ReadyToCommitNavigation().
  //
  // Must not be called for same-document navigation requests nor for requests
  // served from the back-forward cache.
  void UpdateClientSecurityState();

  // Called when the navigation is ready to be committed. This will update the
  // |state_| and inform the delegate.
  void ReadyToCommitNavigation(bool is_error);

  // Called if READY_TO_COMMIT -> COMMIT state transition takes an unusually
  // long time.
  void OnCommitTimeout();

  // Called by the RenderProcessHost to handle the case when the process changed
  // its state of being blocked.
  void RenderProcessBlockedStateChanged(bool blocked);

  void StopCommitTimeout();
  void RestartCommitTimeout();

  std::vector<std::string> TakeRemovedRequestHeaders() {
    return std::move(removed_request_headers_);
  }

  net::HttpRequestHeaders TakeModifiedRequestHeaders() {
    return std::move(modified_request_headers_);
  }

  // Returns true if the contents of |common_params_| requires
  // |source_site_instance_| to be set. This is used to ensure that data: and
  // about:blank URLs with valid initiator origins always have
  // |source_site_instance_| set so that site isolation enforcements work
  // properly.
  bool RequiresInitiatorBasedSourceSiteInstance() const;

  // Sets |source_site_instance_| to a SiteInstance that is derived from
  // |common_params_->initiator_origin| and related to the |frame_tree_node_|'s
  // current SiteInstance. |source_site_instance_| is only set if it doesn't
  // already have a value and RequiresInitiatorBasedSourceSiteInstance() returns
  // true.
  void SetSourceSiteInstanceToInitiatorIfNeeded();

  // See RestartBackForwardCachedNavigation.
  void RestartBackForwardCachedNavigationImpl();

  void ForceEnableOriginTrials(const std::vector<std::string>& trials) override;

  void CreateCoepReporter(StoragePartition* storage_partition);

  base::Optional<network::mojom::BlockedByResponseReason> EnforceCOEP();

  bool IsOverridingUserAgent() const {
    return commit_params_->is_overriding_user_agent || entry_overrides_ua_;
  }

  // Returns the user-agent override, or an empty string if one isn't set.
  std::string GetUserAgentOverride();

  mojo::PendingRemote<network::mojom::CookieAccessObserver>
  CreateCookieAccessObserver();

  // network::mojom::CookieAccessObserver:
  void OnCookiesAccessed(
      network::mojom::CookieAccessDetailsPtr details) override;
  void Clone(mojo::PendingReceiver<network::mojom::CookieAccessObserver>
                 observer) override;

  // Convenience function to return the NavigationControllerImpl this
  // NavigationRequest is in.
  NavigationControllerImpl* GetNavigationController();

  // Compute the sandbox policy of the document to be loaded. Called once when
  // reaching the 'ReadyToCommit' stage.
  network::mojom::WebSandboxFlags ComputeSandboxFlagsToCommit();

  // DCHECK that tranistioning from the current state to |state| valid. This
  // does nothing in non-debug builds.
  void CheckStateTransition(NavigationState state) const;

  // Set |state_| to |state| and also DCHECK that this state transition is
  // valid.
  void SetState(NavigationState state);

  // When a navigation fails, one of two things can happen:
  // 1) An error page commits and replaces the old document.
  // 2) The navigation is canceled, and the previous document is kept.
  //
  // If appropriate, this applies (2), deletes |this|, and returns true.
  // In that case, the caller must immediately return.
  bool MaybeCancelFailedNavigation();

  // Never null. The pointee node owns this navigation request instance.
  FrameTreeNode* const frame_tree_node_;

  // Value of |is_for_commit| supplied to the constructor.
  const bool is_for_commit_;

  // Invariant: At least one of |loader_| or |render_frame_host_| is null.
  RenderFrameHostImpl* render_frame_host_ = nullptr;

  // Initialized on creation of the NavigationRequest. Sent to the renderer when
  // the navigation is ready to commit.
  // Note: When the navigation is ready to commit, the url in |common_params|
  // will be set to the final navigation url, obtained after following all
  // redirects.
  // Note: |common_params_| and |begin_params_| are not const as they can be
  // modified during redirects.
  // Note: |commit_params_| is not const because was_discarded will
  // be set in CreatedNavigationRequest.
  // Note: |browser_initiated_| and |common_params_| may be mutated by
  // ContentBrowserClient::OverrideNavigationParams at StartNavigation time
  // (i.e. before we actually kick off the navigation). |browser_initiated|
  // will always be true for history navigations, even if they began in the
  // renderer using the history API.
  mojom::CommonNavigationParamsPtr common_params_;
  mojom::BeginNavigationParamsPtr begin_params_;
  mojom::CommitNavigationParamsPtr commit_params_;
  bool browser_initiated_;

  // Stores the NavigationUIData for this navigation until the NavigationHandle
  // is created. This can be null if the embedded did not provide a
  // NavigationUIData at the beginning of the navigation.
  std::unique_ptr<NavigationUIData> navigation_ui_data_;

  // URLLoaderFactory to facilitate loading blob URLs.
  scoped_refptr<network::SharedURLLoaderFactory> blob_url_loader_factory_;

  NavigationState state_;
  bool is_navigation_started_ = false;

  std::unique_ptr<NavigationURLLoader> loader_;

#if defined(OS_ANDROID)
  // For each C++ NavigationHandle, there is a Java counterpart. It is the JNI
  // bridge in between the two.
  std::unique_ptr<NavigationHandleProxy> navigation_handle_proxy_;
#endif

  // These next items are used in browser-initiated navigations to store
  // information from the NavigationEntryImpl that is required after request
  // creation time.
  scoped_refptr<SiteInstanceImpl> source_site_instance_;
  scoped_refptr<SiteInstanceImpl> dest_site_instance_;
  const RestoreType restore_type_;
  const ReloadType reload_type_;
  const int nav_entry_id_;
  bool is_view_source_;
  int bindings_;
  bool entry_overrides_ua_ = false;

  // Set to true if SetIsOverridingUserAgent() is called.
  bool was_set_overriding_user_agent_called_ = false;

  scoped_refptr<SiteInstanceImpl> starting_site_instance_;

  // Whether the navigation should be sent to a renderer a process. This is
  // true, except for 204/205 responses and downloads.
  bool response_should_be_rendered_;

  // The type of SiteInstance associated with this navigation.
  AssociatedSiteInstanceType associated_site_instance_type_;

  // Stores the SiteInstance created on redirects to check if there is an
  // existing RenderProcessHost that can commit the navigation so that the
  // renderer process is not deleted while the navigation is ongoing. If the
  // SiteInstance was a brand new SiteInstance, it is not stored.
  scoped_refptr<SiteInstance> speculative_site_instance_;

  // Whether the NavigationRequest was created after receiving a BeginNavigation
  // IPC. When true, main frame navigations should not commit in a different
  // process (unless asked by the content/ embedder). When true, the renderer
  // process expects to be notified if the navigation is aborted.
  const bool from_begin_navigation_;

  // Holds objects received from OnResponseStarted while the WillProcessResponse
  // checks are performed by the NavigationHandle. Once the checks have been
  // completed, these objects will be used to continue the navigation.
  network::mojom::URLResponseHeadPtr response_head_;
  mojo::ScopedDataPipeConsumerHandle response_body_;
  network::mojom::URLLoaderClientEndpointsPtr url_loader_client_endpoints_;
  base::Optional<net::SSLInfo> ssl_info_;
  base::Optional<net::AuthChallengeInfo> auth_challenge_info_;
  bool is_download_ = false;
  GlobalRequestID request_id_;

  // Holds information for the navigation while the WillFailRequest
  // checks are performed by the NavigationHandle.
  bool has_stale_copy_in_cache_;
  net::Error net_error_ = net::OK;
  // Detailed host resolution error information. The error code in
  // |resolve_error_info_.error| should be consistent with (but not necessarily
  // the same as) |net_error_|. In the case of a host resolution error, for
  // example, |net_error_| should be ERR_NAME_NOT_RESOLVED while
  // |resolve_error_info_.error| may give a more detailed error such as
  // ERR_DNS_TIMED_OUT.
  net::ResolveErrorInfo resolve_error_info_;

  // Identifies in which RenderProcessHost this navigation is expected to
  // commit.
  int expected_render_process_host_id_;

  // The SiteInfo of this navigation, as obtained from
  // SiteInstanceImpl::ComputeSiteInfo().
  SiteInfo site_info_;

  const std::unique_ptr<InitiatorCSPContext> initiator_csp_context_;

  base::OnceClosure on_start_checks_complete_closure_;

  // Used in the network service world to pass the subressource loader params
  // to the renderer. Used by AppCache and ServiceWorker, and
  // SignedExchangeSubresourcePrefetch.
  base::Optional<SubresourceLoaderParams> subresource_loader_params_;

  // See comment on accessor.
  const base::UnguessableToken devtools_navigation_token_;

  base::Optional<std::vector<blink::mojom::TransferrableURLLoaderPtr>>
      subresource_overrides_;

  // The NavigationClient interface for that requested this navigation in the
  // case of a renderer initiated navigation. It is expected to be bound until
  // this navigation commits or is canceled.
  mojo::AssociatedRemote<mojom::NavigationClient> request_navigation_client_;

  // The NavigationClient interface used to commit the navigation. For now, this
  // is only used for same-site renderer-initiated navigation.
  // TODO(clamy, ahemery): Extend to all types of navigation.
  // Only valid when PerNavigationMojoInterface is enabled.
  mojo::AssociatedRemote<mojom::NavigationClient> commit_navigation_client_;

  // If set, any redirects to HTTP for this navigation will be upgraded to
  // HTTPS. This is used only on subframe navigations, when
  // upgrade-insecure-requests is set as a CSP policy.
  bool upgrade_if_insecure_ = false;

  // The offset of the new document in the history.
  // See NavigationHandle::GetNavigationEntryOffset() for details.
  int navigation_entry_offset_ = 0;

  // Owns the NavigationThrottles associated with this navigation, and is
  // responsible for notifying them about the various navigation events.
  std::unique_ptr<NavigationThrottleRunner> throttle_runner_;

  // Indicates whether the navigation changed which NavigationEntry is current.
  bool subframe_entry_committed_ = false;

  // True if the committed entry has replaced the existing one.
  // A non-user initiated redirect causes such replacement.
  bool did_replace_entry_ = false;

  // Set to false if we want to update the session history but not update the
  // browser history. E.g., on unreachable urls.
  bool should_update_history_ = false;

  // The previous main frame URL that the user was on. This may be empty if
  // there was no last committed entry.
  GURL previous_url_;

  // The type of navigation that just occurred. Note that not all types of
  // navigations in the enum are valid here, since some of them don't actually
  // cause a "commit" and won't generate this notification.
  NavigationType navigation_type_ = NAVIGATION_TYPE_UNKNOWN;

  // The chain of redirects, including client-side redirect and the current URL.
  // TODO(zetamoo): Try to improve redirect tracking during navigation.
  std::vector<GURL> redirect_chain_;

  // TODO(zetamoo): Try to remove this by always sanitizing the referrer in
  // common_params_.
  blink::mojom::ReferrerPtr sanitized_referrer_;

  bool was_redirected_ = false;

  // Whether this navigation was triggered by a x-origin redirect following a
  // prior (most likely <a download>) download attempt.
  bool from_download_cross_origin_redirect_ = false;

  // Used when SignedExchangeSubresourcePrefetch is enabled to hold the
  // prefetched signed exchanges. This is shared with the navigation initiator's
  // RenderFrameHostImpl. This also means that only the navigations that were
  // directly initiated by the frame that made the prefetches could use the
  // prefetched resources, which is a different behavior from regular prefetches
  // (where all prefetched resources are stored and shared in http cache).
  scoped_refptr<PrefetchedSignedExchangeCache>
      prefetched_signed_exchange_cache_;

  // Tracks navigations within a Web Bundle file. Used when WebBundles feature
  // is enabled or TrustableWebBundleFileUrl switch is set.
  std::unique_ptr<WebBundleHandleTracker> web_bundle_handle_tracker_;

  // Timing information of loading for the navigation. Used for recording UMAs.
  std::unique_ptr<NavigationHandleTiming> navigation_handle_timing_;

  // The time this navigation was ready to commit.
  base::TimeTicks ready_to_commit_time_;

  // Manages the lifetime of a pre-created AppCacheHost until a browser side
  // navigation is ready to be committed, i.e we have a renderer process ready
  // to service the navigation request.
  std::unique_ptr<AppCacheNavigationHandle> appcache_handle_;

  // Set in ReadyToCommitNavigation.
  bool is_same_process_ = true;

  // If set, starting the navigation will immediately result in an error page
  // with this html as content and |net_error| as the network error.
  std::string post_commit_error_page_html_;

  // This test-only callback will be run when all throttle checks have been
  // performed. If the callback returns true, On*ChecksComplete functions are
  // skipped, and only the test callback is being performed.
  // TODO(clamy): Revisit the unit test architecture.
  ThrottleChecksFinishedCallback complete_callback_for_testing_;

  // The instance to process the Web Bundle that's bound to this request.
  // Used to navigate to the main resource URL of the Web Bundle, and
  // load it from the corresponding entry.
  // This is created in OnStartChecksComplete() and passed to the
  // RenderFrameHostImpl in CommitNavigation().
  std::unique_ptr<WebBundleHandle> web_bundle_handle_;

  // Keeps the Web Bundle related information when |this| is for a navigation
  // within a Web Bundle file. Used when WebBundle feature is enabled or
  // TrustableWebBundleFileUrl switch is set.
  // For navigations to Web Bundle file, this is cloned from
  // |web_bundle_handle_| in CommitNavigation(), and is passed to
  // FrameNavigationEntry for the navigation. And for history (back / forward)
  // navigations within the Web Bundle file, this is cloned from the
  // FrameNavigationEntry and is used to create a WebBundleHandle.
  std::unique_ptr<WebBundleNavigationInfo> web_bundle_navigation_info_;

  // Which proxy server was used for this navigation, if any.
  net::ProxyServer proxy_server_;

  // Unique id that identifies the navigation for which this NavigationRequest
  // is created.
  const int64_t navigation_id_ = ++unique_id_counter_;
  // static member for generating the unique id above.
  static int64_t unique_id_counter_;

  // Manages the lifetime of a pre-created ServiceWorkerContainerHost until a
  // corresponding container is created in the renderer.
  std::unique_ptr<ServiceWorkerMainResourceHandle> service_worker_handle_;

  // Timer for detecting an unexpectedly long time to commit a navigation.
  base::OneShotTimer commit_timeout_timer_;

  // The subscription to the notification of the changing of the render
  // process's blocked state.
  std::unique_ptr<
      RenderProcessHost::BlockStateChangedCallbackList::Subscription>
      render_process_blocked_state_changed_subscription_;

  // The headers used for the request. The value of this comes from
  // |begin_params_->headers|. If not set, it needs to be calculated.
  base::Optional<net::HttpRequestHeaders> request_headers_;

  // Used to update the request's headers. When modified during the navigation
  // start, the headers will be applied to the initial network request. When
  // modified during a redirect, the headers will be applied to the redirected
  // request.
  net::HttpRequestHeaders modified_request_headers_;

  net::HttpRequestHeaders cors_exempt_request_headers_;

  // Set of headers to remove during the redirect phase. This can only be
  // modified during the redirect phase.
  std::vector<std::string> removed_request_headers_;

  // The RenderFrameHost that was restored from the back-forward cache. This
  // will be null except for navigations that are restoring a page from the
  // back-forward cache.
  RenderFrameHostImpl* const rfh_restored_from_back_forward_cache_;

  // These are set to the values from the FrameNavigationEntry this
  // NavigationRequest is associated with (if any).
  int64_t frame_entry_item_sequence_number_ = -1;
  int64_t frame_entry_document_sequence_number_ = -1;

  // If non-empty, it represents the IsolationInfo explicitly asked to be used
  // for this NavigationRequest.
  base::Optional<net::IsolationInfo> isolation_info_;

  // This is used to store the current_frame_host id at request creation time.
  const GlobalFrameRoutingId previous_render_frame_host_id_;

  // Routing id of the frame host that initiated the navigation, derived from
  // |begin_params()->initiator_routing_id|. This is best effort: it is only
  // defined for some renderer-initiated navigations (e.g., not drag and drop).
  // The frame with the corresponding routing ID may have been deleted before
  // the navigation begins.
  const GlobalFrameRoutingId initiator_routing_id_;

  // This tracks a connection between the current pending entry and this
  // request, such that the pending entry can be discarded if no requests are
  // left referencing it.
  std::unique_ptr<NavigationControllerImpl::PendingEntryRef> pending_entry_ref_;

  // Used only by DCHECK.
  // True if the NavigationThrottles are running an event, the request then can
  // be cancelled for deferring.
  bool processing_navigation_throttle_ = false;

  // Used only by (D)CHECK.
  // True if we are restarting this navigation request as RenderFrameHost was
  // evicted.
  bool restarting_back_forward_cached_navigation_ = false;

  // Holds a set of values needed to enforce several WebPlatform security APIs
  // at the network request level.
  network::mojom::ClientSecurityStatePtr client_security_state_;

  // Holds the required CSP for this navigation. This will be moved into
  // the RenderFrameHost at DidCommitNavigation time.
  network::mojom::ContentSecurityPolicyPtr required_csp_;

  std::unique_ptr<CrossOriginEmbedderPolicyReporter> coep_reporter_;

  std::unique_ptr<PeakGpuMemoryTracker> loading_mem_tracker_ = nullptr;

  // Structure tracking the effects of the CrossOriginOpenerPolicy on this
  // navigation.
  CrossOriginOpenerPolicyStatus coop_status_;

#if DCHECK_IS_ON()
  bool is_safe_to_delete_ = true;
#endif

  // UKM source associated with the page we are navigated away from.
  const ukm::SourceId previous_page_ukm_source_id_;

  // If true, changes to the user-agent override require a reload. If false, a
  // reload is not necessary.
  bool ua_change_requires_reload_ = true;

  // Whether we're doing a same-site proactive BrowsingInstance swap for this
  // navigation.
  bool did_same_site_proactive_browsing_instance_swap_ = false;

  // Controls whether or not an error page is displayed on error. If set to
  // true, an error will be treated as if the user simply cancelled the
  // navigation.
  bool silently_ignore_errors_ = false;

  // Similar but only suppresses the error page when the error code is
  // net::ERR_BLOCKED_BY_CLIENT.
  bool silently_ignore_blocked_by_client_ = false;

  // Observers listening to cookie access notifications for the network requests
  // made by this navigation.
  mojo::ReceiverSet<network::mojom::CookieAccessObserver> cookie_observers_;

  // The sandbox flags of the document to be loaded. This is computed at
  // 'ReadyToCommit' time.
  base::Optional<network::mojom::WebSandboxFlags> sandbox_flags_to_commit_;

  OptInOriginIsolationEndResult origin_isolation_end_result_ =
      OptInOriginIsolationEndResult::kNotRequestedAndNotIsolated;

  base::WeakPtrFactory<NavigationRequest> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(NavigationRequest);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_NAVIGATION_REQUEST_H_
