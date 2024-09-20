// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_NAVIGATION_REQUEST_H_
#define CONTENT_BROWSER_RENDERER_HOST_NAVIGATION_REQUEST_H_

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/debug/crash_logging.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/memory/safe_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "content/browser/fenced_frame/fenced_frame_url_mapping.h"
#include "content/browser/loader/keep_alive_url_loader_service.h"
#include "content/browser/loader/navigation_url_loader_delegate.h"
#include "content/browser/loader/subresource_proxying_url_loader_service.h"
#include "content/browser/navigation_subresource_loader_params.h"
#include "content/browser/renderer_host/browsing_context_group_swap.h"
#include "content/browser/renderer_host/commit_deferring_condition_runner.h"
#include "content/browser/renderer_host/navigation_controller_impl.h"
#include "content/browser/renderer_host/navigation_policy_container_builder.h"
#include "content/browser/renderer_host/navigation_throttle_runner.h"
#include "content/browser/renderer_host/navigation_type.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/scoped_view_transition_resources.h"
#include "content/browser/security/coop/cross_origin_opener_policy_status.h"
#include "content/browser/site_instance_impl.h"
#include "content/browser/webui/web_ui_controller_factory_registry.h"
#include "content/browser/webui/web_ui_impl.h"
#include "content/common/content_export.h"
#include "content/common/navigation_client.mojom-forward.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/preloading_trigger_type.h"
#include "content/public/browser/render_process_host_observer.h"
#include "content/public/browser/weak_document_ptr.h"
#include "content/public/browser/web_ui_controller.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/simple_watcher.h"
#include "net/base/isolation_info.h"
#include "net/dns/public/resolve_error_info.h"
#include "net/http/http_connection_info.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/network/public/cpp/content_security_policy/csp_context.h"
#include "services/network/public/mojom/blocked_by_response_reason.mojom-shared.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "services/network/public/mojom/shared_dictionary_access_observer.mojom.h"
#include "services/network/public/mojom/trust_token_access_observer.mojom-shared.h"
#include "services/network/public/mojom/web_sandbox_flags.mojom-shared.h"
#include "third_party/blink/public/common/runtime_feature_state/runtime_feature_state_context.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/mojom/lcp_critical_path_predictor/lcp_critical_path_predictor.mojom.h"
#include "third_party/blink/public/mojom/loader/mixed_content.mojom-forward.h"
#include "third_party/blink/public/mojom/navigation/navigation_initiator_activation_and_ad_status.mojom.h"
#include "third_party/blink/public/mojom/navigation/navigation_params.mojom-forward.h"
#include "url/gurl.h"
#include "url/origin.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/scoped_java_ref.h"
#include "content/browser/android/navigation_handle_proxy.h"
#endif

namespace network {
struct URLLoaderCompletionStatus;
}  // namespace network

namespace ui {
class CompositorLock;
}  // namespace ui

namespace content {

class AgentClusterKey;
class CrossOriginEmbedderPolicyReporter;
class FrameTreeNode;
class NavigationUIData;
class NavigationURLLoader;
class NavigatorDelegate;
class PrefetchedSignedExchangeCache;
class PrerenderHostRegistry;
class RenderFrameHostCSPContext;
class ServiceWorkerMainResourceHandle;
class SubframeHistoryNavigationThrottle;

// The primary implementation of NavigationHandle.
//
// Lives from navigation start until the navigation has been committed.
class CONTENT_EXPORT NavigationRequest
    : public NavigationHandle,
      public NavigationURLLoaderDelegate,
      public NavigationThrottleRunner::Delegate,
      public CommitDeferringConditionRunner::Delegate,
      public FencedFrameURLMapping::MappingResultObserver,
      public mojom::NavigationRendererCancellationListener,
      private RenderProcessHostObserver,
      private network::mojom::CookieAccessObserver,
      private network::mojom::TrustTokenAccessObserver,
      private network::mojom::SharedDictionaryAccessObserver {
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
    // For navigations that have already committed synchronously in the renderer
    // (see |is_synchronous_renderer_commit_|), this will synchronously proceed
    // to DID_COMMIT directly without any waiting (or the navigation might not
    // commit in certain cases, and be cleared in this state). All other
    // navigations can only reach DID_COMMIT from READY_TO_COMMIT.
    WILL_START_REQUEST,

    // The request is being redirected. Wait for the NavigationThrottles to
    // finish running the WillRedirectRequest event. This is potentially
    // asynchronous.
    WILL_REDIRECT_REQUEST,

    // The response is being processed. Wait for the NavigationThrottles to
    // finish running the WillProcessResponse event. This is potentially
    // asynchronous.
    WILL_PROCESS_RESPONSE,

    // The navigation does not require a request/response. Wait only for
    // NavigationThrottles to finish before calling CommitNavigation(). This
    // will only be asynchronous if a throttle defers the navigation.
    WILL_COMMIT_WITHOUT_URL_LOADER,

    // The browser process has asked the renderer to commit the response
    // and is waiting for acknowledgement that it has been committed.
    READY_TO_COMMIT,

    // The response has been committed. This is one of the two final states of
    // the request.
    DID_COMMIT,

    // The request is being canceled.
    CANCELING,

    // The request is failing. Wait for the NavigationThrottles to finish
    // running the WillFailRequest event. This is potentially asynchronous.
    WILL_FAIL_REQUEST,

    // The request failed with a net error code and an error page should be
    // displayed. This is one of the two final states for the request.
    DID_COMMIT_ERROR_PAGE,
  };

  // The RenderFrameHost currently associated with the navigation. Note that the
  // final value will only be known when the response is received, or the
  // navigation fails, as server redirects can modify the RenderFrameHost to use
  // for the navigation.
  enum class AssociatedRenderFrameHostType {
    NONE = 0,
    // The navigation reuses the current RenderFrameHost.
    CURRENT,
    // The navigation uses a new RenderFrameHost, the speculative
    // RenderFrameHost.
    SPECULATIVE,
  };

  // This enum is used in UMA histograms, so existing values should neither be
  // reordered or removed.
  enum class OriginAgentClusterEndResult {
    // The first four enums are for use when OAC-by-default is disabled.
    kNotRequestedAndNotOriginKeyed,
    kNotRequestedButOriginKeyed,
    kRequestedButNotOriginKeyed,
    kRequestedAndOriginKeyed,
    // The remaining enums are for use when OAC-by-default is enabled.
    kExplicitlyNotRequestedAndNotOriginKeyed,
    kExplicitlyNotRequestedButOriginKeyed,
    kExplicitlyRequestedButNotOriginKeyed,
    kExplicitlyRequestedAndOriginKeyed,
    kNotExplicitlyRequestedButNotOriginKeyed,
    kNotExplicitlyRequestedAndOriginKeyed,
    kMaxValue = kNotExplicitlyRequestedAndOriginKeyed
  };

  // Creates a request for a browser-initiated navigation.
  static std::unique_ptr<NavigationRequest> CreateBrowserInitiated(
      FrameTreeNode* frame_tree_node,
      blink::mojom::CommonNavigationParamsPtr common_params,
      blink::mojom::CommitNavigationParamsPtr commit_params,
      bool was_opener_suppressed,
      const std::string& extra_headers,
      FrameNavigationEntry* frame_entry,
      NavigationEntryImpl* entry,
      bool is_form_submission,
      std::unique_ptr<NavigationUIData> navigation_ui_data,
      const std::optional<blink::Impression>& impression,
      bool is_pdf,
      bool is_embedder_initiated_fenced_frame_navigation = false,
      std::optional<std::u16string> embedder_shared_storage_context =
          std::nullopt);

  // Creates a request for either a browser-initiated navigation or a
  // renderer-initiated navigation.  Normally, renderer-initiated navigations
  // use CreateRendererInitiated(), but some legacy renderer-initiated
  // navigation paths, such as OpenURL, are stuck using this path instead;
  // these cases specify `browser_initiated` as false.
  //
  // Do NOT add more uses of this function.  Browser-initiated navigations
  // should use CreateBrowserInitiated() and renderer-initiated navigations
  // should use CreateRendererInitiated().
  //
  // TODO(crbug.com/40249771): Refactor the remaining uses of this function to
  // use either CreateBrowserInitiated() or CreateRendererInitiated() and then
  // remove this helper.
  static std::unique_ptr<NavigationRequest> Create(
      FrameTreeNode* frame_tree_node,
      blink::mojom::CommonNavigationParamsPtr common_params,
      blink::mojom::CommitNavigationParamsPtr commit_params,
      bool browser_initiated,
      bool was_opener_suppressed,
      const std::optional<blink::LocalFrameToken>& initiator_frame_token,
      int initiator_process_id,
      const std::string& extra_headers,
      FrameNavigationEntry* frame_entry,
      NavigationEntryImpl* entry,
      bool is_form_submission,
      std::unique_ptr<NavigationUIData> navigation_ui_data,
      const std::optional<blink::Impression>& impression,
      blink::mojom::NavigationInitiatorActivationAndAdStatus
          initiator_activation_and_ad_status,
      bool is_pdf,
      bool is_embedder_initiated_fenced_frame_navigation = false,
      bool is_container_initiated = false,
      bool has_rel_opener = false,
      net::StorageAccessApiStatus storage_access_api_status =
          net::StorageAccessApiStatus::kNone,
      std::optional<std::u16string> embedder_shared_storage_context =
          std::nullopt);

  // Creates a request for a renderer-initiated navigation.
  static std::unique_ptr<NavigationRequest> CreateRendererInitiated(
      FrameTreeNode* frame_tree_node,
      NavigationEntryImpl* entry,
      blink::mojom::CommonNavigationParamsPtr common_params,
      blink::mojom::BeginNavigationParamsPtr begin_params,
      int current_history_list_offset,
      int current_history_list_length,
      bool override_user_agent,
      scoped_refptr<network::SharedURLLoaderFactory> blob_url_loader_factory,
      mojo::PendingAssociatedRemote<mojom::NavigationClient> navigation_client,
      scoped_refptr<PrefetchedSignedExchangeCache>
          prefetched_signed_exchange_cache,
      mojo::PendingReceiver<mojom::NavigationRendererCancellationListener>
          renderer_cancellation_listener);

  // Creates a NavigationRequest for synchronous navigation that have committed
  // in the renderer process. Those are:
  // - same-document renderer-initiated navigations.
  // - synchronous about:blank navigations.
  //
  // TODO(clamy): Eventually, this should only be called for same-document
  // renderer-initiated navigations.
  static std::unique_ptr<NavigationRequest> CreateForSynchronousRendererCommit(
      FrameTreeNode* frame_tree_node,
      RenderFrameHostImpl* render_frame_host,
      bool is_same_document,
      const GURL& url,
      const url::Origin& origin,
      const std::optional<GURL>& initiator_base_url,
      const net::IsolationInfo& isolation_info_for_subresources,
      blink::mojom::ReferrerPtr referrer,
      const ui::PageTransition& transition,
      bool should_replace_current_entry,
      const std::string& method,
      bool has_transient_activation,
      bool is_overriding_user_agent,
      const std::vector<GURL>& redirects,
      const GURL& original_url,
      std::unique_ptr<CrossOriginEmbedderPolicyReporter> coep_reporter,
      int http_response_code);

  static NavigationRequest* From(NavigationHandle* handle);

  // If |type| is a reload, returns the equivalent ReloadType. Otherwise returns
  // ReloadType::NONE.
  static ReloadType NavigationTypeToReloadType(
      blink::mojom::NavigationType type);

  NavigationRequest(const NavigationRequest&) = delete;
  NavigationRequest& operator=(const NavigationRequest&) = delete;

  ~NavigationRequest() override;

  // Returns true if this request's URL matches |origin| and the request state
  // is at (or past) WILL_PROCESS_RESPONSE.
  bool HasCommittingOrigin(const url::Origin& origin);

  // Returns true if this navigation's COOP header implies that the destination
  // site of this navigation should be site-isolated.  In addition to checking
  // for eligible COOP header values, this function also verifies other
  // criteria, such as whether this feature is enabled on the device (e.g.,
  // above memory threshold) or whether the site is already isolated.
  bool ShouldRequestSiteIsolationForCOOP();

  // NavigationHandle implementation:
  int64_t GetNavigationId() const override;
  ukm::SourceId GetNextPageUkmSourceId() override;
  const GURL& GetURL() override;
  SiteInstanceImpl* GetStartingSiteInstance() override;
  SiteInstanceImpl* GetSourceSiteInstance() override;
  bool IsInMainFrame() const override;
  bool IsInPrimaryMainFrame() const override;
  bool IsInOutermostMainFrame() override;
  bool IsInPrerenderedMainFrame() const override;
  bool IsPrerenderedPageActivation() const override;
  bool IsInFencedFrameTree() const override;
  FrameType GetNavigatingFrameType() const override;
  bool IsRendererInitiated() override;
  blink::mojom::NavigationInitiatorActivationAndAdStatus
  GetNavigationInitiatorActivationAndAdStatus() override;
  bool IsSameOrigin() override;
  bool WasServerRedirect() override;
  const std::vector<GURL>& GetRedirectChain() override;
  FrameTreeNodeId GetFrameTreeNodeId() override;
  RenderFrameHostImpl* GetParentFrame() override;
  RenderFrameHostImpl* GetParentFrameOrOuterDocument() override;
  base::TimeTicks NavigationStart() override;
  base::TimeTicks NavigationInputStart() override;
  const NavigationHandleTiming& GetNavigationHandleTiming() override;
  bool IsPost() override;
  std::string GetRequestMethod() override;
  const blink::mojom::Referrer& GetReferrer() override;
  void SetReferrer(blink::mojom::ReferrerPtr referrer) override;
  bool HasUserGesture() override;
  ui::PageTransition GetPageTransition() override;
  NavigationUIData* GetNavigationUIData() override;
  bool IsExternalProtocol() override;
  net::Error GetNetErrorCode() override;
  RenderFrameHostImpl* GetRenderFrameHost() const override;
  bool IsSameDocument() const override;
  bool IsHistory() const override;
  bool HasCommitted() const override;
  bool IsErrorPage() const override;
  bool HasSubframeNavigationEntryCommitted() override;
  bool DidReplaceEntry() override;
  bool ShouldUpdateHistory() override;
  const GURL& GetPreviousPrimaryMainFrameURL() override;
  net::IPEndPoint GetSocketAddress() override;
  const net::HttpRequestHeaders& GetRequestHeaders() override;
  void RemoveRequestHeader(const std::string& header_name) override;
  void SetRequestHeader(const std::string& header_name,
                        const std::string& header_value) override;
  void SetCorsExemptRequestHeader(const std::string& header_name,
                                  const std::string& header_value) override;
  void SetLCPPNavigationHint(
      const blink::mojom::LCPCriticalPathPredictorNavigationTimeHint& hint)
      override;
  const blink::mojom::LCPCriticalPathPredictorNavigationTimeHintPtr&
  GetLCPPNavigationHint() override;
  const net::HttpResponseHeaders* GetResponseHeaders() override;
  net::HttpConnectionInfo GetConnectionInfo() override;
  const std::optional<net::SSLInfo>& GetSSLInfo() override;
  const std::optional<net::AuthChallengeInfo>& GetAuthChallengeInfo() override;
  net::ResolveErrorInfo GetResolveErrorInfo() override;
  net::IsolationInfo GetIsolationInfo() override;
  void RegisterThrottleForTesting(
      std::unique_ptr<NavigationThrottle> navigation_throttle) override;
  bool IsDeferredForTesting() override;
  bool IsCommitDeferringConditionDeferredForTesting() override;
  CommitDeferringCondition* GetCommitDeferringConditionForTesting() override;
  bool WasStartedFromContextMenu() override;
  const GURL& GetSearchableFormURL() override;
  const std::string& GetSearchableFormEncoding() override;
  ReloadType GetReloadType() const override;
  RestoreType GetRestoreType() const override;
  const GURL& GetBaseURLForDataURL() override;
  const GlobalRequestID& GetGlobalRequestID() override;
  bool IsDownload() override;
  bool IsFormSubmission() override;
  bool WasInitiatedByLinkClick() override;
  bool IsSignedExchangeInnerResponse() override;
  bool HasPrefetchedAlternativeSubresourceSignedExchange() override;
  bool WasResponseCached() override;
  const std::string& GetHrefTranslate() override;
  const std::optional<blink::Impression>& GetImpression() override;
  const std::optional<blink::LocalFrameToken>& GetInitiatorFrameToken()
      override;
  int GetInitiatorProcessId() override;
  const std::optional<url::Origin>& GetInitiatorOrigin() override;
  const std::optional<GURL>& GetInitiatorBaseUrl() override;
  const std::vector<std::string>& GetDnsAliases() override;
  bool IsSameProcess() override;
  NavigationEntry* GetNavigationEntry() const override;
  int GetNavigationEntryOffset() override;
  void RegisterSubresourceOverride(
      blink::mojom::TransferrableURLLoaderPtr transferrable_loader) override;
  GlobalRenderFrameHostId GetPreviousRenderFrameHostId() override;
  int GetExpectedRenderProcessHostId() override;
  bool IsServedFromBackForwardCache() override;
  void SetIsOverridingUserAgent(bool override_ua) override;
  void SetSilentlyIgnoreErrors() override;
  void SetVisitedLinkSalt(uint64_t salt) override;
  network::mojom::WebSandboxFlags SandboxFlagsInitiator() override;
  network::mojom::WebSandboxFlags SandboxFlagsInherited() override;
  network::mojom::WebSandboxFlags SandboxFlagsToCommit() override;
  bool IsWaitingToCommit() override;
  bool WasResourceHintsReceived() override;
  bool IsPdf() override;
  void WriteIntoTrace(perfetto::TracedProto<TraceProto> context) const override;
  bool SetNavigationTimeout(base::TimeDelta timeout) override;
  void CancelNavigationTimeout() override;
  void SetAllowCookiesFromBrowser(bool allow_cookies_from_browser) override;
  void GetResponseBody(ResponseBodyCallback callback) override;
  PreloadingTriggerType GetPrerenderTriggerType() override;
  std::string GetPrerenderEmbedderHistogramSuffix() override;
#if BUILDFLAG(IS_ANDROID)
  const base::android::JavaRef<jobject>& GetJavaNavigationHandle() override;
#endif
  base::SafeRef<NavigationHandle> GetSafeRef() override;
  bool ExistingDocumentWasDiscarded() const override;
  blink::RuntimeFeatureStateContext& GetMutableRuntimeFeatureStateContext()
      override;
  void SetContentSettings(
      blink::mojom::RendererContentSettingsPtr content_settings) override;
  blink::mojom::RendererContentSettingsPtr GetContentSettingsForTesting()
      override;
  void SetIsAdTagged() override;
  std::optional<NavigationDiscardReason> GetNavigationDiscardReason() override;
  // NOTE: Read function comments in NavigationHandle before use!
  std::optional<url::Origin> GetOriginToCommit() override;
  // End of NavigationHandle implementation.

  // mojom::NavigationRendererCancellationListener implementation:
  void RendererCancellationWindowEnded() override;
  // End of mojom::NavigationRendererCancellationListener implementation.

  void RegisterCommitDeferringConditionForTesting(
      std::unique_ptr<CommitDeferringCondition> condition);

  // Called on the UI thread by the Navigator to start the navigation.
  // The NavigationRequest can be deleted while BeginNavigation() is called.
  void BeginNavigation();

  const blink::mojom::CommonNavigationParams& common_params() const {
    return *common_params_;
  }

  const blink::mojom::BeginNavigationParams& begin_params() const {
    return *begin_params_;
  }

  const blink::mojom::CommitNavigationParams& commit_params() const {
    return *commit_params_;
  }

  // Updates the navigation start time.
  void set_navigation_start_time(const base::TimeTicks& time) {
    common_params_->navigation_start = time;
  }

  void set_is_cross_site_cross_browsing_context_group(
      bool is_cross_site_cross_browsing_context_group) {
    commit_params_->is_cross_site_cross_browsing_context_group =
        is_cross_site_cross_browsing_context_group;
  }

  NavigationURLLoader* loader_for_testing() const { return loader_.get(); }

  NavigationState state() const { return state_; }

  FrameTreeNode* frame_tree_node() const { return frame_tree_node_; }

  bool is_synchronous_renderer_commit() const {
    return is_synchronous_renderer_commit_;
  }

  SiteInstanceImpl* dest_site_instance() const {
    return dest_site_instance_.get();
  }

  std::optional<BindingsPolicySet> bindings() const { return bindings_; }

  bool browser_initiated() const {
    return commit_params_->is_browser_initiated;
  }

  bool from_begin_navigation() const { return from_begin_navigation_; }

  AssociatedRenderFrameHostType GetAssociatedRFHType() const;

  void SetAssociatedRFHType(AssociatedRenderFrameHostType type);

  bool HasRenderFrameHost() const { return render_frame_host_.has_value(); }

  void set_was_discarded() { commit_params_->was_discarded = true; }

  void set_net_error(net::Error net_error) { net_error_ = net_error; }

  const std::string& GetMimeType() {
    return response_head_ ? response_head_->mime_type : base::EmptyString();
  }

  const network::mojom::URLResponseHead* response() {
    return response_head_.get();
  }

  const mojo::DataPipeConsumerHandle& response_body() {
    DCHECK_EQ(state_, WILL_PROCESS_RESPONSE);
    return response_body_.get();
  }

  mojo::ScopedDataPipeConsumerHandle& mutable_response_body_for_testing() {
    return response_body_;
  }

  void SetWaitingForRendererResponse();

  // Notifies the NavigatorDelegate the navigation started. This should be
  // called after any previous NavigationRequest for the FrameTreeNode has been
  // destroyed.
  void StartNavigation();

  void set_on_start_checks_complete_closure_for_testing(
      base::OnceClosure closure) {
    on_start_checks_complete_closure_ = std::move(closure);
  }

  // Updates the destination SiteInfo for this navigation. This is called on
  // redirects. |post_redirect_process| is the renderer process that should
  // handle the navigation following the redirect if it can be handled by an
  // existing RenderProcessHost. Otherwise, it should be null.
  void UpdateSiteInfo(RenderProcessHost* post_redirect_process);

  int nav_entry_id() const { return nav_entry_id_; }

  // For automation driver-initiated navigations over the devtools protocol,
  // |devtools_navigation_token_| is used to tag the navigation. This navigation
  // token is then sent into the renderer and lands on the DocumentLoader. That
  // way subsequent Blink-level frame lifecycle events can be associated with
  // the concrete navigation.
  // - The value should not be sent back to the browser.
  // - The value on DocumentLoader may be generated in the renderer in some
  // cases, and thus shouldn't be trusted.
  // TODO(crbug.com/40549185): Replace devtools navigation token with the
  // generic navigation token that can be passed from renderer to the browser.
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

  // If a navigation has been cancelled, and was initiated by the parent
  // document, report it with the appropriate ResourceTiming entry information.
  //
  // The ResourceTiming entry may not be sent if the current frame
  // does not have a parent, or if the navigation was cancelled before
  // a request was made.
  void MaybeAddResourceTimingEntryForCancelledNavigation();

  // Adds a resource timing entry to the parent in case of cancelled navigations
  // and failed <object> navigations.
  void AddResourceTimingEntryForFailedSubframeNavigation(
      const network::URLLoaderCompletionStatus& status);

  // Lazily initializes and returns the mojo::NavigationClient interface used
  // for commit.
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

  // Simulates renderer cancelling the navigation.
  void RendererRequestedNavigationCancellationForTesting();

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

  void DidCommitNavigation(const mojom::DidCommitProvisionalLoadParams& params,
                           bool navigation_entry_committed,
                           bool did_replace_entry,
                           const GURL& previous_main_frame_url);

  NavigationType navigation_type() const {
    DCHECK(state_ == DID_COMMIT || state_ == DID_COMMIT_ERROR_PAGE);
    return navigation_type_;
  }

  void set_navigation_type(NavigationType navigation_type) {
    navigation_type_ = navigation_type;
  }

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

  // Whether the navigation loads an MHTML document or a subframe of an MHTML
  // document.  The navigation might or might not be fullfilled from the MHTML
  // archive (see `is_mhtml_subframe_loaded_from_achive` in the NeedsUrlLoader
  // method).  The navigation will commit in the main frame process.
  bool IsMhtmlOrSubframe();

  // Whether this navigation navigates a subframe of an MHTML document.
  bool IsForMhtmlSubframe() const;

  // Whether the navigation has a non-OK net error code.
  // Note that this is different from IsErrorPage(), which only returns true if
  // the navigation has finished committing an error page. The net error code
  // can be non-OK before commit and also in cases that didn't result in the
  // navigation being committed (e.g. canceled navigations).
  virtual bool DidEncounterError() const;

  void set_begin_navigation_callback_for_testing(base::OnceClosure callback) {
    begin_navigation_callback_for_testing_ = std::move(callback);
  }

  void set_complete_callback_for_testing(
      ThrottleChecksFinishedCallback callback) {
    complete_callback_for_testing_ = std::move(callback);
  }

  network::mojom::URLLoaderClientEndpointsPtr&
  mutable_url_loader_client_endpoints_for_testing() {
    return url_loader_client_endpoints_;
  }

  void set_ready_to_commit_callback_for_testing(base::OnceClosure callback) {
    ready_to_commit_callback_for_testing_ = std::move(callback);
  }

  // Whether this navigation is queued, waiting for an existing pending commit
  // RenderFrameHost to finish navigating.
  bool IsQueued() const { return !!resume_commit_closure_; }

  void set_renderer_cancellation_window_ended_callback(
      base::OnceClosure callback) {
    DCHECK(!renderer_cancellation_window_ended());
    renderer_cancellation_window_ended_callback_ = std::move(callback);
  }

  bool renderer_cancellation_window_ended() const {
    return renderer_cancellation_window_ended_;
  }

  // Returns true if this navigation should wait for the renderer-initiated
  // navigation cancellation window to end before committing, and returns false
  // otherwise. See comment for `renderer_cancellation_listener_` for more
  // details.
  bool ShouldWaitForRendererCancellationWindowToEnd();

  // Sets the READY_TO_COMMIT -> DID_COMMIT timeout. Resets the timeout to the
  // default value if |timeout| is zero.
  static void SetCommitTimeoutForTesting(const base::TimeDelta& timeout);

  // Returns the `rfh_restored_from_back_forward_cache_` if the navigation is
  // a BFCache restore, or nullptr otherwise.
  RenderFrameHostImpl* GetRenderFrameHostRestoredFromBackForwardCache() const;

  // The NavigatorDelegate to notify/query for various navigation events. This
  // is always the WebContents.
  NavigatorDelegate* GetDelegate() const;

  blink::mojom::RequestContextType request_context_type() const {
    return begin_params_->request_context_type;
  }

  network::mojom::RequestDestination request_destination() const {
    return common_params_->request_destination;
  }

  blink::mojom::MixedContentContextType mixed_content_context_type() const {
    return begin_params_->mixed_content_context_type;
  }

  // Returns true if the navigation was started by the Navigator by calling
  // BeginNavigation(), or if the request was created at commit time by calling
  // CreateForCommit().
  bool IsNavigationStarted() const;

  std::unique_ptr<input::PeakGpuMemoryTracker> TakePeakGpuMemoryTracker();

  std::unique_ptr<NavigationEarlyHintsManager> TakeEarlyHintsManager();

  // Returns true for navigation responses to be rendered in a renderer process.
  // This excludes:
  //  - 204/205 navigation responses
  //  - Most downloads
  //  (Note: downloads with unsuccessful response codes will render an error
  //  page, causing this to return true.)
  //
  // Must not be called before having received the response. After
  // OnResponseStarted(), this is expected to be equivalent to
  // HasRenderFrameHost().
  bool response_should_be_rendered() const {
    DCHECK_GE(state_, WILL_PROCESS_RESPONSE);
    return response_should_be_rendered_;
  }

  // Returns the `ClientSecurityState` we should pass to
  // `NavigationURLLoaderFactory`.
  //
  // For subresource requests the `ClientSecurityState` is passed through
  // `URLLoaderFactoryParams`, since each request client has a dedicated
  // factory. That does not work for navigation requests because they all share
  // a common factory, so each request is tagged with a `ClientSecurityState` to
  // use instead.
  network::mojom::ClientSecurityStatePtr
  BuildClientSecurityStateForNavigationFetch();

  // Returns the `ClientSecurityState` to be used for subresource fetches by the
  // document we are navigating to.
  //
  // Must only be called after `ReadyToCommitNavigation()`.
  network::mojom::ClientSecurityStatePtr
  BuildClientSecurityStateForCommittedDocument();

  bool ua_change_requires_reload() const { return ua_change_requires_reload_; }

  void SetRequiredCSP(network::mojom::ContentSecurityPolicyPtr csp);
  network::mojom::ContentSecurityPolicyPtr TakeRequiredCSP();

  bool is_credentialless() const { return is_credentialless_; }

  // Returns a pointer to the policies copied from the navigation initiator.
  // Returns nullptr if this navigation had no initiator.
  const PolicyContainerPolicies* GetInitiatorPolicyContainerPolicies() const;

  // The DocumentToken that should be used for the document created as a result
  // of committing this navigation.
  // - must only be called for cross-document navigations
  // - must not be called before the navigation is ready to commit
  const blink::DocumentToken& GetDocumentToken() const;

  // Returns the policies of the new document being navigated to.
  //
  // Must only be called after ReadyToCommitNavigation().
  const PolicyContainerPolicies& GetPolicyContainerPolicies() const;

  // Creates a new policy container for Blink connected to this navigation's
  // PolicyContainerHost.
  //
  // Must only be called after ReadyToCommitNavigation().
  blink::mojom::PolicyContainerPtr CreatePolicyContainerForBlink();

  // Returns a new refptr to this navigation's PolicyContainerHost.
  //
  // Must only be called after ReadyToCommitNavigation().
  // It is invalid to call after `TakePolicyContainerHost()`.
  scoped_refptr<PolicyContainerHost> GetPolicyContainerHost();

  // Moves this navigation's PolicyContainerHost out of this instance.
  //
  // Must only be called after ReadyToCommitNavigation().
  scoped_refptr<PolicyContainerHost> TakePolicyContainerHost();

  CrossOriginEmbedderPolicyReporter* coep_reporter() {
    return coep_reporter_.get();
  }

  std::unique_ptr<CrossOriginEmbedderPolicyReporter> TakeCoepReporter();

  // Returns UKM SourceId for the page we are navigating away from.
  // Equal to GetRenderFrameHost()->GetPageUkmSourceId() for subframe
  // and same-document navigations and to
  // RenderFrameHost::FromID(GetPreviousRenderFrameHostId())
  //     ->GetPageUkmSourceId() for main-frame cross-document navigations.
  // Note: If this method is called on a prerender navigation, it will return
  // ukm::kInvalidSourceId due to our data collection policy.
  ukm::SourceId GetPreviousPageUkmSourceId();

  void OnServiceWorkerAccessed(const GURL& scope,
                               AllowServiceWorkerResult allowed);

  // Take all cookie observers associated with this navigation.
  // Typically this is called when navigation commits to move these observers to
  // the committed document.
  [[nodiscard]] std::vector<
      mojo::PendingReceiver<network::mojom::CookieAccessObserver>>
  TakeCookieObservers();

  // Take all Trust Token observers associated with this navigation.
  // Typically this is called when navigation commits to move these observers to
  // the committed document.
  [[nodiscard]] std::vector<
      mojo::PendingReceiver<network::mojom::TrustTokenAccessObserver>>
  TakeTrustTokenObservers();

  // Take all shared dictionary observers associated with this navigation.
  // Typically this is called when navigation commits to move these observers to
  // the committed document.
  [[nodiscard]] std::vector<
      mojo::PendingReceiver<network::mojom::SharedDictionaryAccessObserver>>
  TakeSharedDictionaryAccessObservers();

  // Returns the coop status information relevant to the current navigation.
  CrossOriginOpenerPolicyStatus& coop_status() { return coop_status_; }

  // Returns true if this is a NavigationRequest represents a WebView
  // loadDataWithBaseUrl navigation.
  bool IsLoadDataWithBaseURL() const;

  // Calculates the origin that this NavigationRequest may commit.
  //
  // GetTentativeOriginAtRequestTime must be called before the final HTTP
  // response is received (unlike GetOriginToCommit), but the returned origin
  // may differ from the final origin committed by this navigation (e.g. the
  // origin may change because of subsequent redirects, or because of CSP
  // headers in the final response; or because no commit may happen at all in
  // case of downloads or 204 responses). Prefer to use GetOriginToCommit if
  // possible.
  url::Origin GetTentativeOriginAtRequestTime();

  // Same as `GetOriginToCommit()`, except that includes information about how
  // the origin gets calculated, to help debug if the browser-side calculated
  // origin for this navigation differs from the origin calculated on the
  // renderer side.
  // TODO(crbug.com/40772732): Remove this.
  std::pair<std::optional<url::Origin>, std::string>
  GetOriginToCommitWithDebugInfo();

  // If this navigation fails with net::ERR_BLOCKED_BY_CLIENT, act as if it were
  // cancelled by the user and do not commit an error page.
  void SetSilentlyIgnoreBlockedByClient() {
    silently_ignore_blocked_by_client_ = true;
  }

  // Returns the current url from GetURL() packaged with other state required to
  // properly determine SiteInstances and process allocation.
  UrlInfo GetUrlInfo();

  bool is_overriding_user_agent() const {
    return commit_params_->is_overriding_user_agent;
  }

  // Returns the IsolationInfo that should be used to load subresources.
  const net::IsolationInfo& isolation_info_for_subresources() const {
    return isolation_info_for_subresources_;
  }

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
  // Note #3: Navigations that do not use a URL loader do not send the usual
  // set of callbacks to NavigationThrottle. Instead, they send a single
  // separate callback, WillCommitWithoutUrlLoader().
  bool NeedsUrlLoader();

  network::mojom::PrivateNetworkRequestPolicy private_network_request_policy()
      const {
    return private_network_request_policy_;
  }

  // Whether this navigation request waits for the result of beforeunload before
  // proceeding.
  bool IsWaitingForBeforeUnload();

  // Returns the original request url:
  // - If this navigation resulted in an error page, this will return the URL
  // of the page that failed to load.
  // - If this is navigation is triggered by loadDataWithBaseURL or related
  // functions, this will return the data URL (or data header, in case of
  // loadDataAsStringWithBaseURL).
  // - Otherwise, this will return the first URL in |redirect_chain_|. This
  // means if the navigation is started due to a client redirect, we will return
  // the URL of the page that initiated the client redirect. Otherwise we will
  // return the first destination URL for this navigation.
  // NOTE: This might result in a different value than original_url in
  // |commit_params_|, which is always set to the first destination URL for this
  // navigation.
  const GURL& GetOriginalRequestURL();

  // The previous main frame URL. This may be empty if there was no last
  // committed entry.
  const GURL& GetPreviousMainFrameURL() const;

  // This is the same as |NavigationHandle::IsServedFromBackForwardCache|, but
  // adds a const qualifier.
  bool IsServedFromBackForwardCache() const;

  // Whether this navigation is activating an existing page (e.g. served from
  // the BackForwardCache or Prerender)
  bool IsPageActivation() const override;

  // Returns whether the navigation type is a restore navigation.
  bool IsRestore() const;

  // Returns whether the navigation type is a reload navigation.
  bool IsReload() const;

  // Sets state pertaining to prerender activations. This is only called if
  // this navigation is a prerender activation.
  void SetPrerenderActivationNavigationState(
      std::unique_ptr<NavigationEntryImpl> prerender_navigation_entry,
      const blink::mojom::FrameReplicationState& replication_state);

  std::unique_ptr<NavigationEntryImpl> TakePrerenderNavigationEntry();

  // Returns value that is only valid for prerender activation navigations.
  const blink::mojom::FrameReplicationState&
  prerender_main_frame_replication_state() {
    return prerender_navigation_state_->prerender_main_frame_replication_state;
  }

  // Origin-keyed Agent Cluster (OAC) state needs to be tracked for origins that
  // opt-in or opt-out, so that a given origin is treated consistently within a
  // given BrowsingInstance. This ensures that foo.example.com will have the
  // same OAC state in a BrowsingInstance even if multiple documents in the
  // origin have different headers.
  //
  // This function ensures that this OAC state is tracked in cases where it is
  // not handled elsewhere. For example, OAC origins that end up in origin-keyed
  // processes already track their OAC state during SiteInstance creation.
  // However, SiteInstance does not track origin state in the following cases,
  // which this function handles:
  //
  // 1) If process isolation for OAC is disabled (e.g., on low-memory Android
  // devices), the origin will use a site-keyed SiteInstance that does not
  // record OAC state for the origin.
  //
  // 2) If the origin has no header but ends up with OAC-by-default (and
  // kOriginKeyedProcessesByDefault is not enabled), the origin will use a
  // site-keyed SiteInstance.
  //
  // 3) If the origin opts-out of OAC using a header, it will use a site-keyed
  // SiteInstance.
  //
  // 4)If the origin opts-in to OAC using a header, but it is first placed in a
  // speculative RenderFrameHost before the header is received, it creates a
  // SiteInfo with default isolation and an origin-keyed process (by default).
  // In this case, the origin was not tracked when the SiteInstance was created,
  // and needs to be tracked later when the opt-in header is observed.
  //
  // In all of these cases, this function updates the BrowsingInstance to keep
  // track of the OAC state for this NavigationRequest's origin.
  //
  // TODO(wjmaclean): Cases 1 and 2 will not be necessary once we use
  // origin-keyed SiteInstances within a site-keyed process, via
  // SiteInstanceGroup. Cases 3 and 4 will still be needed at that point, but
  // might become simpler.
  void AddOriginAgentClusterStateIfNecessary(
      const IsolationContext& isolation_context);

  // Store a console message, which will be sent to the final RenderFrameHost
  // immediately after requesting the navigation to commit.
  //
  // /!\ WARNING /!\: Beware of not leaking cross-origin information to a
  // potentially compromised renderer when using this method.
  void AddDeferredConsoleMessage(blink::mojom::ConsoleMessageLevel level,
                                 std::string message);

  bool is_deferred_on_fenced_frame_url_mapping_for_testing() const {
    return is_deferred_on_fenced_frame_url_mapping_;
  }

  base::WeakPtr<NavigationRequest> GetWeakPtr();

  bool is_running_potential_prerender_activation_checks() const {
    return is_running_potential_prerender_activation_checks_;
  }

  FrameTreeNodeId prerender_frame_tree_node_id() const {
    DCHECK(prerender_frame_tree_node_id_.has_value())
        << "Must be called after StartNavigation()";
    return prerender_frame_tree_node_id_.value();
  }

  const std::optional<FencedFrameProperties>& GetFencedFrameProperties() const {
    return fenced_frame_properties_;
  }

  // Compute and return the `FencedFrameProperties` that this
  // `NavigationRequest` acts under, i.e. the properties attached to this
  // `NavigationRequest` if present.
  // Otherwise, returns the fenced frame properties associated with the given
  // source. See `FrameTreeNode::GetFencedFrameProperties()` on how fenced
  // frame properties are obtained for different sources.
  // TODO(crbug.com/40060657): Once navigation support for urn::uuid in iframes
  // is deprecated, remove the parameter `node_source`.
  const std::optional<FencedFrameProperties>& ComputeFencedFrameProperties(
      FencedFramePropertiesNodeSource node_source =
          FencedFramePropertiesNodeSource::kClosestAncestor) const;

  const std::optional<base::UnguessableToken> ComputeFencedFrameNonce() const;

  void RenderFallbackContentForObjectTag();

  // Returns the vector of web features used during the navigation, whose
  // recording was delayed until the new document that used them commits.
  //
  // Empties this instance's vector.
  std::vector<blink::mojom::WebFeature> TakeWebFeaturesToLog();

  void set_subresource_proxying_url_loader_service_bind_context(
      base::WeakPtr<SubresourceProxyingURLLoaderService::BindContext>
          bind_context) {
    DCHECK(!subresource_proxying_url_loader_service_bind_context_);
    subresource_proxying_url_loader_service_bind_context_ = bind_context;
  }

  base::WeakPtr<KeepAliveURLLoaderService::FactoryContext>
  keep_alive_url_loader_factory_context() {
    return keep_alive_url_loader_factory_context_;
  }

  void set_keep_alive_url_loader_factory_context(
      base::WeakPtr<KeepAliveURLLoaderService::FactoryContext>
          factory_context) {
    DCHECK(!keep_alive_url_loader_factory_context_);
    keep_alive_url_loader_factory_context_ = factory_context;
  }

  void set_fetch_later_loader_factory_context(
      base::WeakPtr<KeepAliveURLLoaderService::FactoryContext>
          factory_context) {
    DCHECK(!fetch_later_loader_factory_context_);
    fetch_later_loader_factory_context_ = factory_context;
  }

  // Helper for logging crash keys related to a NavigationRequest (e.g.
  // "navigation_request_url", "navigation_request_initiator", and
  // "navigation_request_is_same_document").  The crash keys will be logged if a
  // ScopedCrashKeys instance exists when a crash or DumpWithoutCrashing
  // happens.
  class ScopedCrashKeys {
   public:
    explicit ScopedCrashKeys(NavigationRequest& navigation_request);
    ~ScopedCrashKeys();

    // No copy constructor and no copy assignment operator.
    ScopedCrashKeys(const ScopedCrashKeys&) = delete;
    ScopedCrashKeys& operator=(const ScopedCrashKeys&) = delete;

   private:
    url::debug::ScopedOriginCrashKey initiator_origin_;
    url::debug::ScopedUrlCrashKey url_;
    base::debug::ScopedCrashKeyString is_same_document_;
  };

  // Prerender2:
  void set_prerender_trigger_type(PreloadingTriggerType type) {
    DCHECK(!prerender_trigger_type_.has_value());
    prerender_trigger_type_ = type;
  }
  void set_prerender_embedder_histogram_suffix(const std::string& suffix) {
    prerender_embedder_histogram_suffix_ = suffix;
  }

  // Used in tests to indicate this navigation should force a BrowsingInstance
  // swap.
  void set_force_new_browsing_instance(bool force_new_browsing_instance) {
    force_new_browsing_instance_ = force_new_browsing_instance;
  }

  // When this returns true, it indicates this navigation should force a
  // BrowsingInstance swap. Used only in tests.
  bool force_new_browsing_instance() { return force_new_browsing_instance_; }

  const scoped_refptr<NavigationOrDocumentHandle>&
  navigation_or_document_handle() {
    return navigation_or_document_handle_;
  }

  const std::pair<std::optional<url::Origin>, std::string>&
  browser_side_origin_to_commit_with_debug_info() {
    return browser_side_origin_to_commit_with_debug_info_;
  }

  // Initializes state which is passed from the old Document to the new Document
  // for a ViewTransition.
  void SetViewTransitionState(
      std::unique_ptr<ScopedViewTransitionResources> resources,
      blink::ViewTransitionState view_transition_state);

  // Returns a const reference to a blink::RuntimeFeatureStateContext (RFSC)
  // object. Once the commit params are sent to the renderer we no longer allow
  // write access to the RFSC, but read access is still available.
  //
  // Note: This method has another
  // version, `GetMutableRuntimeFeatureStateContext()`, accessible via
  // NavigationHandle and will return a mutable reference to the RFSC.
  const blink::RuntimeFeatureStateContext& GetRuntimeFeatureStateContext();

  BrowsingContextGroupSwap browsing_context_group_swap() const {
    return browsing_context_group_swap_;
  }

  // If the navigation fails before commit and |pending_navigation_api_key_| has
  // been set, then the renderer will be notified of the pre-commit failure and
  // provide |pending_navigation_api_key_| so that the Navigation API can fire
  // events and reject promises.
  // This should only be set if this request's frame initiated the navigation,
  // because only the initiating frame has outstanding promises to reject.
  void set_pending_navigation_api_key(std::optional<std::string> key) {
    pending_navigation_api_key_ = key;
  }

  // Returns the navigation token for this request if this NavigationRequest
  // is for a history navigation, and it might be cancelled via the navigate
  // event. In that case, any associated subframe history navigations will be
  // deferred until this navigation commits.
  // This may only be called if this is a main-frame same-document navigation.
  // Will return a valid token if canceling traversals via the navigate event is
  // enabled, this is a history navigation, a navigate event is registered in
  // the renderer, and the frame has met the user activation requirements to be
  // allowed to cancel the navigation.
  // This token will then be provided to the subframe NavigationRequests via
  // set_main_frame_same_document_history_token().
  std::optional<base::UnguessableToken>
  GetNavigationTokenForDeferringSubframes();

  // For subframe NavigationRequests, these set and return the main frame's
  // NavigationRequest token, in the case that the main frame returns it from
  // GetNavigationTokenForDeferringSubframes(). Note that by the time
  // `main_frame_same_document_history_token()` is called, the NavigationRequest
  // represented by that token may have already finished and been deleted, so
  // any attempt to lookup based on this token must null-check the request.
  void set_main_frame_same_document_history_token(
      std::optional<base::UnguessableToken> token) {
    main_frame_same_document_navigation_token_ = token;
  }
  std::optional<base::UnguessableToken>
  main_frame_same_document_history_token() {
    return main_frame_same_document_navigation_token_;
  }

  // When a SubframeHistoryNavigationThrottle is created, it registers itself
  // with the NavigationRequest for the main frame. If the main frame
  // NavigationRequest commits, then these throttles will be resumed in
  // UnblockPendingSubframeNavigationRequestsIfNeeded().
  // If it is canceled instead, these throttles will all be canceled.
  // Takes a WeakPtr because `throttle` is owned by another NavigationRequest,
  // and that request may be canceled outside of our control, in which case
  // `throttle` will be destroyed.
  void AddDeferredSubframeNavigationThrottle(
      base::WeakPtr<SubframeHistoryNavigationThrottle> throttle);

  std::unique_ptr<RenderFrameHostImpl::CookieChangeListener>
  TakeCookieChangeListener() {
    return std::move(cookie_change_listener_);
  }

  // Returns true if there is a speculative RFH that has a pending commit
  // cross-document navigation, and this NavigationRequest is not a pending
  // commit NavigationRequest itself. This means that this navigation should be
  // queued (i.e. wait for the pending commit navigation to finish committing),
  // before continuing and creating a new speculative RFH to commit in, so that
  // it won't cause the existing pending commit RFH to be deleted. This function
  // should only be called for navigations that are owned by the FrameTreeNode
  // (i.e. it hasn't moved to the RenderFrameHost that it will commit in yet),
  // as only those navigations can be queued.
  bool ShouldQueueDueToExistingPendingCommitRFH() const;

  void set_resume_commit_closure(base::OnceClosure closure) {
    resume_commit_closure_ = std::move(closure);
  }

  // Records metrics for `GetFrameHostForNavigation()` attempts that failed due
  // to this `NavigationRequest` being in the pending commit state.
  // `commit_attempt` should be true if `GetFrameHostForNavigation()` failed
  // when trying to get a RenderFrameHost when committing a navigation.
  void RecordMetricsForBlockedGetFrameHostAttempt(bool commit_attempt);

  // Creates a WebUI object for this navigation and saves it in `web_ui_`. Later
  // on, the WebUI created will be moved to `frame_host` (if `frame_host` is
  // null, it means a RenderFrameHost has not been picked for the navigation).
  void CreateWebUIIfNeeded(RenderFrameHostImpl* frame_host);

  bool HasWebUI() { return !!web_ui_; }

  WebUIImpl* web_ui() { return web_ui_.get(); }

  std::unique_ptr<WebUIImpl> TakeWebUI() {
    CHECK(HasWebUI());
    return std::move(web_ui_);
  }

  bool shared_storage_writable_eligible() const {
    return shared_storage_writable_eligible_;
  }

  enum ErrorPageProcess {
    kNotErrorPage,
    kPostCommitErrorPage,
    kCurrentProcess,
    kDestinationProcess,
    kIsolatedProcess
  };
  // Helper to determine whether a navigation is committing an error page and
  // should stay in the current process (kCurrentProcess), the destination
  // URL's process (kDestinationProcess), an isolated process
  // (kIsolatedProcess), or is a post-commit error page that does not have any
  // specific process requirements and goes through the "normal navigation"
  // path. Returns kNotErrorPage if the navigation is not anerror page
  // navigation.
  ErrorPageProcess ComputeErrorPageProcess();

  // This describes the reason for performing an early RenderFrameHost swap, if
  // any. This enum is used in UMA histograms, so existing values should be
  // neither reordered nor removed.
  enum class EarlyRenderFrameHostSwapType {
    kNone = 0,
    kInitialFrame = 1,
    kCrashedFrame = 2,
    kNavigationTransition = 3,  // DEPRECATED
    kMaxValue = kNavigationTransition,
  };

  // Remember if this navigation triggered an early swap of a speculative
  // RenderFrameHost to become a current RenderFrameHost prior to the
  // navigation commit, and if so, what triggered it.
  void set_early_render_frame_host_swap_type(
      EarlyRenderFrameHostSwapType type) {
    early_render_frame_host_swap_type_ = type;
  }
  EarlyRenderFrameHostSwapType early_render_frame_host_swap_type() const {
    return early_render_frame_host_swap_type_;
  }

  void set_previous_render_frame_host_id(GlobalRenderFrameHostId id) {
    previous_render_frame_host_id_ = id;
  }

  // Returns true if URL Loader has been created and hasn't been reset yet for
  // this navigation.
  bool HasLoader() const;

  // Notifies that an IPC will be sent to the old Document's renderer to
  // dispatch the `pageswap` event. Returns the parameters which should be
  // used for the event if this is a same-origin navigation.
  blink::mojom::PageSwapEventParamsPtr WillDispatchPageSwap();

  // Returns true if this navigation is eligible for dispatching a `pageswap`
  // event on the old Document and the event has not been dispatched already.
  bool ShouldDispatchPageSwapEvent() const;

  // Returns true if there have been any cross-origin redirects in the
  // navigation's lifetime. A redirect is cross-origin if the redirect url is
  // cross-origin with "previous URL" of the navigation. previous URL here is
  // either the initial URL (which the navigation started with) or the URL from
  // the last redirect.
  //
  // Note: This will be false if the initial url is cross-origin and there are
  // no redirects.
  bool did_encounter_cross_origin_redirect() const {
    return did_encounter_cross_origin_redirect_;
  }

  // Determines whether this navigation request was initiated by an animated
  // transition.
  void set_was_initiated_by_animated_transition() {
    was_initiated_by_animated_transition_ = true;
  }
  bool was_initiated_by_animated_transition() const {
    return was_initiated_by_animated_transition_;
  }

  void set_navigation_discard_reason(
      NavigationDiscardReason navigation_discard_reason) {
    CHECK(!navigation_discard_reason_.has_value());
    navigation_discard_reason_ = navigation_discard_reason;
  }

  // Returns the type of this navigation (e.g. history, browser-initiated, etc)
  // to set as a discard reason on another navigation that is being discarded
  // because this navigation is taking its place in the FrameTreeNode.
  NavigationDiscardReason GetTypeForNavigationDiscardReason();

  void set_force_no_https_upgrade() { force_no_https_upgrade_ = true; }

 private:
  friend class NavigationRequestTest;

  struct ConsoleMessage {
    blink::mojom::ConsoleMessageLevel level;
    std::string message;
  };

  NavigationRequest(
      FrameTreeNode* frame_tree_node,
      blink::mojom::CommonNavigationParamsPtr common_params,
      blink::mojom::BeginNavigationParamsPtr begin_params,
      blink::mojom::CommitNavigationParamsPtr commit_params,
      bool browser_initiated,
      bool from_begin_navigation,
      bool is_synchronous_renderer_commit,
      const FrameNavigationEntry* frame_navigation_entry,
      NavigationEntryImpl* navitation_entry,
      std::unique_ptr<NavigationUIData> navigation_ui_data,
      scoped_refptr<network::SharedURLLoaderFactory> blob_url_loader_factory,
      mojo::PendingAssociatedRemote<mojom::NavigationClient> navigation_client,
      scoped_refptr<PrefetchedSignedExchangeCache>
          prefetched_signed_exchange_cache,
      std::optional<base::SafeRef<RenderFrameHostImpl>>
          rfh_restored_from_back_forward_cache,
      int initiator_process_id,
      bool was_opener_suppressed,
      bool is_pdf,
      bool is_embedder_initiated_fenced_frame_navigation = false,
      mojo::PendingReceiver<mojom::NavigationRendererCancellationListener>
          renderer_cancellation_listener = mojo::NullReceiver(),
      std::optional<std::u16string> embedder_shared_storage_context =
          std::nullopt);

  // Checks if this navigation may activate a prerendered page. If it's
  // possible, schedules to start running CommitDeferringConditions for
  // prerendered page activation and returns true.
  bool MaybeStartPrerenderingActivationChecks();

  // Called from OnCommitDeferringConditionChecksComplete() if this request is
  // activating a prerendered page.
  void OnPrerenderingActivationChecksComplete(
      CommitDeferringCondition::NavigationType navigation_type,
      std::optional<FrameTreeNodeId> candidate_prerender_frame_tree_node_id);

  // Get the `FencedFrameURLMapping` associated with the current page.
  FencedFrameURLMapping& GetFencedFrameURLMap();

  // True if this is a fenced frame navigation to an urn:uuid.
  bool NeedFencedFrameURLMapping();

  // FencedFrameURLMapping::MappingResultObserver implementation.
  // Called from `FencedFrameURLMapping` when the mapping decision is made, and
  // resume the deferred navigation.
  void OnFencedFrameURLMappingComplete(
      const std::optional<FencedFrameProperties>& properties) override;

  // Called from BeginNavigation(), OnPrerenderingActivationChecksComplete(),
  // or OnFencedFrameURLMappingComplete().
  void BeginNavigationImpl();

  // Checks if the response requests an isolated origin via the
  // Origin-Agent-Cluster header, and if so opts in the origin to be isolated.
  void CheckForIsolationOptIn(const GURL& url);

  // Returns whether this navigation request is requesting opt-in
  // origin-isolation.
  bool IsOriginAgentClusterOptInRequested();

  // Returns whether this navigation request is requesting opt-out from
  // origin-isolation. Always returns false if
  // AreOriginAgentClustersEnabledByDefault() is false.
  bool IsOriginAgentClusterOptOutRequested();

  // Returns whether this NavigationRequest should use an origin-keyed agent
  // cluster, specifically in cases where no Origin-Agent-Cluster header has
  // been observed, either because no response has yet been received or because
  // it had no such header. (Returns false if the header is observed.)
  //
  // Note that an origin-keyed process may be used if this returns true, if
  // kOriginKeyedProcessesByDefault is enabled.
  bool IsIsolationImplied();

  // The Origin-Agent-Cluster end result is determined early in the lifecycle of
  // a NavigationRequest, but used late. In particular, we want to trigger use
  // counters and console warnings once navigation has committed.
  void DetermineOriginAgentClusterEndResult();
  void ProcessOriginAgentClusterEndResult();

  void PopulateDocumentTokenForCrossDocumentNavigation();

  // NavigationURLLoaderDelegate implementation.
  void OnRequestRedirected(
      const net::RedirectInfo& redirect_info,
      const net::NetworkAnonymizationKey& network_anonymization_key,
      network::mojom::URLResponseHeadPtr response_head) override;
  void OnResponseStarted(
      network::mojom::URLLoaderClientEndpointsPtr url_loader_client_endpoints,
      network::mojom::URLResponseHeadPtr response_head,
      mojo::ScopedDataPipeConsumerHandle response_body,
      GlobalRequestID request_id,
      bool is_download,
      net::NetworkAnonymizationKey network_anonymization_key,
      SubresourceLoaderParams subresource_loader_params,
      EarlyHints early_hints) override;
  void OnRequestFailed(
      const network::URLLoaderCompletionStatus& status) override;
  std::optional<NavigationEarlyHintsManagerParams>
  CreateNavigationEarlyHintsManagerParams(
      const network::mojom::EarlyHints& early_hints) override;

  // Selecting a `RenderFrameHost` to commit a navigation may occasionally fail.
  // When this happens, the navigation will bind a closure to continue the
  // navigation and assign it to `resume_commit_closure_`. This closure may run
  // even when it is still not possible to proceed; see the comment on the
  // `resume_commit_closure_` field for the full details.

  // Corresponds to navigations committing from `OnResponseStarted()`:
  void SelectFrameHostForOnResponseStarted(
      network::mojom::URLLoaderClientEndpointsPtr url_loader_client_endpoints,
      bool is_download,
      SubresourceLoaderParams subresource_loader_params);
  void SelectFrameHostForOnRequestFailedInternal(
      bool exists_in_cache,
      bool skip_throttles,
      const std::optional<std::string>& error_page_content);
  void SelectFrameHostForCrossDocumentNavigationWithNoUrlLoader();

  // To be called whenever a navigation request fails. If |skip_throttles| is
  // true, the registered NavigationThrottle(s) won't get a chance to intercept
  // NavigationThrottle::WillFailRequest. It should be used when a request
  // failed due to a throttle result itself. |error_page_content| is only used
  // when |skip_throttles| is true. If |collapse_frame| is true, the associated
  // frame tree node is collapsed.
  void OnRequestFailedInternal(
      const network::URLLoaderCompletionStatus& status,
      bool skip_throttles,
      const std::optional<std::string>& error_page_content,
      bool collapse_frame);

  // Called when the NavigationThrottles have been checked by the
  // NavigationHandle.
  void OnStartChecksComplete(NavigationThrottle::ThrottleCheckResult result);
  void OnRedirectChecksComplete(NavigationThrottle::ThrottleCheckResult result);
  void OnFailureChecksComplete(NavigationThrottle::ThrottleCheckResult result);
  void OnWillProcessResponseChecksComplete(
      NavigationThrottle::ThrottleCheckResult result);
  void OnWillCommitWithoutUrlLoaderChecksComplete(
      NavigationThrottle::ThrottleCheckResult result);

  // Runs CommitDeferringConditions.
  //
  // For prerendered page activation, this is called at the beginning of the
  // navigation (i.e., in BeginNavigation()). This is because activating a
  // prerendered page must be an atomic, synchronous operation so there is no
  // chance for the prerender to be cancelled during the operation. The
  // CommitDeferringConditions are asynchronous, so they run at the beginning
  // of navigation. Once they finish, the atomic activation sequence runs.
  void RunCommitDeferringConditions();

  // Similar to the NavigationThrottle checks above but this is called from
  // CommitDeferringConditionRunner rather than NavigationThrottles and is
  // invoked after all throttle checks and commit checks have completed and the
  // navigation can proceed to commit.
  void OnCommitDeferringConditionChecksComplete(
      CommitDeferringCondition::NavigationType navigation_type,
      std::optional<FrameTreeNodeId> candidate_prerender_frame_tree_node_id)
      override;

  // Called either by OnFailureChecksComplete() or OnRequestFailed() directly.
  // |error_page_content| contains the content of the error page (i.e. flattened
  // HTML, JS, CSS).
  void CommitErrorPage(const std::optional<std::string>& error_page_content);

  // Have a RenderFrameHost commit the navigation. The NavigationRequest will
  // be destroyed sometime after this call, typically after the renderer has
  // informed the browser process that the commit has finished.
  void CommitNavigation();

  // Commits the navigation to an existing page (back-forward cache navigation
  // or prerender activation). NavigationRequest will be destroyed after this
  // call.
  void CommitPageActivation();

  // Checks if the specified CSP context's relevant CSP directive
  // allows the navigation. This is called to perform the frame-src check.
  bool IsAllowedByCSPDirective(
      const std::vector<network::mojom::ContentSecurityPolicyPtr>& policies,
      network::CSPContext* context,
      network::mojom::CSPDirectiveName directive,
      bool has_followed_redirect,
      bool url_upgraded_after_redirect,
      bool is_opaque_fenced_frame,
      network::CSPContext::CheckCSPDisposition disposition);

  // Checks if CSP allows the navigation. This will check the frame-src and
  // fenced-frame-src directives. Returns net::OK if the checks
  // pass, and net::ERR_ABORTED or net::ERR_BLOCKED_BY_CSP depending on which
  // checks fail.
  net::Error CheckCSPDirectives(
      RenderFrameHostCSPContext parent_context,
      const PolicyContainerPolicies* parent_policies,
      RenderFrameHostCSPContext initiator_context,
      const PolicyContainerPolicies* initiator_policies,
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
  mojom::DidCommitProvisionalLoadParamsPtr
  MakeDidCommitProvisionalLoadParamsForBFCacheRestore();

  // Builds the parameters used to commit a navigation to a prerendered page
  // that was activated.
  mojom::DidCommitProvisionalLoadParamsPtr
  MakeDidCommitProvisionalLoadParamsForPrerenderActivation();

  // Builds generic activation parameters used to commit a navigation to a page.
  mojom::DidCommitProvisionalLoadParamsPtr
  MakeDidCommitProvisionalLoadParamsForActivation();

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

  // Block about:srcdoc navigation that aren't expected to happen. For instance,
  // main frame navigations or about:srcdoc#foo.
  enum class AboutSrcDocCheckResult {
    ALLOW_REQUEST,
    BLOCK_REQUEST,
  };
  AboutSrcDocCheckResult CheckAboutSrcDoc() const;

  // When the embedder requires the use of Content Security Policy via Embedded
  // Enforcement, framed documents must either:
  // 1. Use the 'allow-csp-from' header to opt-into enforcement.
  // 2. Enforce its own CSP that subsumes the required CSP.
  // Framed documents that fail to do either of these will be blocked.
  //
  // See:
  // - https://w3c.github.io/webappsec-cspee/#required-csp-header
  // - https://w3c.github.io/webappsec-cspee/#allow-csp-from-header
  //
  // SetupCSPEmbeddedEnforcement() retrieve the iframe 'csp' attribute applying.
  // CheckCSPEmbeddedEnforcement() inspects the response headers. It decides if
  // the 'csp' attribute should be installed into the child. This might also
  // block it and display an error page instead.
  void SetupCSPEmbeddedEnforcement();
  enum class CSPEmbeddedEnforcementResult {
    ALLOW_RESPONSE,
    BLOCK_RESPONSE,
  };
  CSPEmbeddedEnforcementResult CheckCSPEmbeddedEnforcement();

  // Called before a commit. Updates the history index and length held in
  // CommitNavigationParams. This is used to update this shared state with the
  // renderer process.
  void UpdateHistoryParamsInCommitNavigationParams();

  // The disconnect handler for the NavigationClient Mojo interface; used as a
  // signal to potentially cancel navigations, e.g. when the renderer replaces
  // an existing NavigationClient connection with a new one or when the renderer
  // process crashes.
  void OnNavigationClientDisconnected(uint32_t reason,
                                      const std::string& description);

  // Binds the given error_handler to be called when an interface disconnection
  // happens on the renderer side.
  void HandleInterfaceDisconnection(
      mojo::AssociatedRemote<mojom::NavigationClient>&);

  // When called, this NavigationRequest will no longer interpret the interface
  // disconnection on the renderer side as an AbortNavigation.
  // TODO(crbug.com/40276805): remove this function when
  // NavigationRequest properly handles interface disconnection in all cases.
  void IgnoreInterfaceDisconnection();

  // Sets ID of the RenderProcessHost we expect the navigation to commit in.
  // This is used to inform the RenderProcessHost to expect a navigation to the
  // url we're navigating to.
  void SetExpectedProcess(RenderProcessHost* expected_process);

  // Inform the RenderProcessHost to no longer expect a navigation.
  void ResetExpectedProcess();

  // If this is a same-site main-frame navigation where we did a proactive
  // BrowsingInstance swap but we're reusing the old page's process, we need
  // to send the routing ID and the updated lifecycle state of the old page so
  // that we can run pagehide and visibilitychange handlers of the old page
  // when we commit the new page.
  void AddOldPageInfoToCommitParamsIfNeeded();

  // Record download related UseCounters when navigation is a download before
  // filtered by download_policy.
  void RecordDownloadUseCountersPrePolicyCheck();

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
  void OnWillCommitWithoutUrlLoaderProcessed(
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
  void WillRedirectRequest(const GURL& new_referrer_url,
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

  // Called when no URLRequest will be needed to perform this navigation, just
  // before commit.
  void WillCommitWithoutUrlLoader();

  // Checks for attempts to navigate to a page that is already referenced more
  // than once in the frame's ancestors.  This is a helper function used by
  // WillStartRequest and WillRedirectRequest to prevent the navigation.
  bool IsSelfReferentialURL();

  // RenderProcessHostObserver implementation.
  void RenderProcessExited(RenderProcessHost* host,
                           const ChildProcessTerminationInfo& info) override;
  void RenderProcessHostDestroyed(RenderProcessHost* host) override;

  // Updates navigation handle timings.
  void UpdateNavigationHandleTimingsOnResponseReceived(bool is_redirect,
                                                       bool is_first_response);
  void UpdateNavigationHandleTimingsOnCommitSent();

  // Helper function that computes the SiteInfo for |common_params_.url|.
  // Note: |site_info_| should only be updated with the result of this function.
  SiteInfo GetSiteInfoForCommonParamsURL();

  // Updates the state of the navigation handle after encountering a server
  // redirect.
  void UpdateStateFollowingRedirect(const GURL& new_referrer_url);

  // Updates |private_network_request_policy_| for ReadyToCommitNavigation().
  //
  // Must not be called for same-document navigation requests nor for requests
  // served from the back-forward cache or from prerendered pages.
  void UpdatePrivateNetworkRequestPolicy();

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

  void ForceEnableOriginTrials(const std::vector<std::string>& trials) override;

  void CreateCoepReporter(StoragePartition* storage_partition);

  // [spec]: https://html.spec.whatwg.org/C/#obtain-an-embedder-policy
  //
  // Returns the CrossOriginEmbedderPolicy for the document, which is inherited
  // or retrieved from response headers.
  network::CrossOriginEmbedderPolicy ComputeCrossOriginEmbedderPolicy();

  // [spec]:
  // https://html.spec.whatwg.org/C/#check-a-navigation-response's-adherence-to-its-embedder-policy
  //
  // Return whether the response's COEP is compatible with its parent's COEP. It
  // also sends COEP reports if needed.
  bool CheckResponseAdherenceToCoep(const GURL& url);

  std::optional<network::mojom::BlockedByResponseReason> EnforceCOEP();

  // Check the COOP value of the page is compatible with the COEP value of each
  // of its documents. COOP:kSameOriginPlusCoep is incompatible with COEP:kNone.
  // If they aren't, this returns false and emits a crash report.
  bool CoopCoepSanityCheck();

  // Checks that, given an origin to be committed, all of the permissions
  // policies that a fenced frame requires to be enabled are enabled. If not, it
  // logs a console message and returns false.
  bool CheckPermissionsPoliciesForFencedFrames(const url::Origin&);

  // Helper function that determines if a given required permissions policy
  // feature is properly enabled for a given origin to be committed.
  bool IsFencedFrameRequiredPolicyFeatureAllowed(
      const url::Origin&,
      const blink::mojom::PermissionsPolicyFeature feature);

  // Returns the user-agent override, or an empty string if one isn't set.
  std::string GetUserAgentOverride();

  mojo::PendingRemote<network::mojom::CookieAccessObserver>
  CreateCookieAccessObserver();

  // network::mojom::CookieAccessObserver:
  void OnCookiesAccessed(std::vector<network::mojom::CookieAccessDetailsPtr>
                             details_vector) override;
  void Clone(mojo::PendingReceiver<network::mojom::CookieAccessObserver>
                 observer) override;

  mojo::PendingRemote<network::mojom::TrustTokenAccessObserver>
  CreateTrustTokenAccessObserver();

  // network::mojom::TrustTokenAccessObserver:
  void OnTrustTokensAccessed(
      network::mojom::TrustTokenAccessDetailsPtr details) override;
  void Clone(mojo::PendingReceiver<network::mojom::TrustTokenAccessObserver>
                 observer) override;

  mojo::PendingRemote<network::mojom::SharedDictionaryAccessObserver>
  CreateSharedDictionaryAccessObserver();

  // network::mojom::SharedDictionaryAccessObserver:
  void OnSharedDictionaryAccessed(
      network::mojom::SharedDictionaryAccessDetailsPtr details) override;
  void Clone(
      mojo::PendingReceiver<network::mojom::SharedDictionaryAccessObserver>
          observer) override;

  // Convenience function to return the NavigationControllerImpl this
  // NavigationRequest is in.
  NavigationControllerImpl* GetNavigationController() const;

  // Convenience function to return the PrerenderHostRegistry this
  // NavigationRequest can be associated with.
  PrerenderHostRegistry& GetPrerenderHostRegistry();

  // Returns the RenderFrameHost of the initiator document, iff there is such
  // a document and its RenderFrameHost has not committed a different document
  // since this navigation started. Otherwise returns nullptr.
  RenderFrameHostImpl* GetInitiatorDocumentRenderFrameHost();

  // Records the appropriate kAddressSpace* WebFeature for the response we just
  // received on the initiator document, if possible.
  void RecordAddressSpaceFeature();

  // Computes the PolicyContainerPolicies and the sandbox flags to use for
  // committing a regular document.
  // Called when the response to commit is known.
  void ComputePoliciesToCommit();

  // Computes the PolicyContainerPolicies and the sandbox flags to use for
  // committing an error document.
  //
  // Note:
  // |ComputePoliciesToCommit()| can be followed by
  // |ComputePoliciesToCommitForErrorPage()|. This happens when the decision to
  // commit an error document happens after receiving the regular document's
  // response.
  void ComputePoliciesToCommitForError();

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

  // Called just after a navigation commits (also in case of error): it
  // sends all console messages to the final RenderFrameHost.
  void SendDeferredConsoleMessages();

  bool ShouldRenderFallbackContentForResponse(
      const net::HttpResponseHeaders& response_head) const;

  // Whether this is a same-URL navigation that should replace the current entry
  // or not. Called when the navigation just started.
  bool ShouldReplaceCurrentEntryForSameUrlNavigation() const;

  // Whether this navigation happens on the initial empty document or initial
  // NavigationEntry, and thus should replace the current entry. Called when the
  // navigation just started.
  bool ShouldReplaceCurrentEntryForNavigationFromInitialEmptyDocumentOrEntry()
      const;

  // Whether a failed navigation should replace the current entry or not. Called
  // when an error page is about to be committed.
  bool ShouldReplaceCurrentEntryForFailedNavigation() const;

  // Calculates the origin that this NavigationRequest may commit. See also the
  // comment of GetOriginToCommit(). Performs calculation without information
  // from RenderFrameHostImpl (e.g. CSPs are ignored). Should be used only in
  // situations where the final frame host hasn't been determined but the origin
  // is needed to create URLLoaderFactory.
  url::Origin GetOriginForURLLoaderFactoryBeforeResponse(
      network::mojom::WebSandboxFlags sandbox_flags);

  // Superset of GetOriginForURLLoaderFactoryBeforeResponse(). Calculates
  // the origin with information from the final frame host. Can be called only
  // after the final response is received or ready.
  std::optional<url::Origin> GetOriginForURLLoaderFactoryAfterResponse();

  // These functions are the same as their non-WithDebugInfo counterparts,
  // except that they include information about how the origin gets calculated,
  // to help debug if the browser-side calculated origin for this navigation
  // differs from the origin calculated on the renderer side.
  // TODO(crbug.com/40772732): Remove this.
  std::pair<url::Origin, std::string>
  GetOriginForURLLoaderFactoryBeforeResponseWithDebugInfo(
      network::mojom::WebSandboxFlags sandbox_flags);
  std::pair<std::optional<url::Origin>, std::string>
  GetOriginForURLLoaderFactoryAfterResponseWithDebugInfo();

  // Computes the CrossOriginIsolationKey to use for committing the navigation.
  // A nullopt result means that either the cross-origin isolation status of the
  // request cannot be determined because we do not have final headers for the
  // navigation yet, or that the navigation is not cross-origin isolated.
  std::optional<AgentClusterKey::CrossOriginIsolationKey>
  ComputeCrossOriginIsolationKey();

  // Computes the web-exposed isolation information based on `coop_status_` and
  // current `frame_tree_node_` info.
  // If the return result is nullopt, it means that the WebExposedIsolationInfo
  // is not relevant or unknown. This can happen for example when we do not have
  // a network response yet, or when going to an "about:blank" page.
  std::optional<WebExposedIsolationInfo> ComputeWebExposedIsolationInfo();

  // Computes whether the navigation is for a document that should live in a
  // BrowsingInstance only containing other documents with the same COOP value
  // set by the same origin. This is the case if this document or its top-level
  // document sets COOP: same-origin or COOP: restrict-properties. If it is a
  // top-level document, simply return its origin, otherwise inherit the
  // top-level document value.
  //
  // If the return value is nullopt, it indicates that neither COOP: same-origin
  // nor COOP: restrict-properties were used for this document or for its parent
  // in the case of a subframe.
  std::optional<url::Origin> ComputeCommonCoopOrigin();

  // Assign an invalid frame tree node id to `prerender_frame_tree_node_id_`.
  // Called as soon as when we are certain that this navigation won't activate a
  // prerendered page. This is needed because `IsPrerenderedPageActivation()`,
  // which may be called at any point after BeginNavigation(), will assume that
  // 'prerender_frame_tree_node_id_' has an value assigned.
  void MaybeAssignInvalidPrerenderFrameTreeNodeId();

  // The NavigationDownloadPolicy is currently fully computed by the renderer
  // process. It is left empty for browser side initiated navigation. This is a
  // problem. This function is an incomplete attempt to start computing it from
  // the browser process instead.
  // TODO(crbug.com/40249217): Complete the implementation the browser
  // side implementation.
  void ComputeDownloadPolicy();

  blink::NavigationDownloadPolicy& download_policy() {
    return common_params_->download_policy;
  }

  // Called on FrameTreeNode's queued NavigationRequest (if any) when another
  // NavigationRequest associated with the same FrameTreeNode is destroyed and
  // the queued NavigationRequest can be resumed. Will post a task to run the
  // `resume_commit_closure_` asynchronously.
  void PostResumeCommitTask();

  // See https://crbug.com/1412365
  void CheckSoftNavigationHeuristicsInvariants();

  // Used to resume any SubframeHistoryNavigationThrottles in this FrameTree
  // when this NavigationRequest commits.
  // `subframe_history_navigation_throttles_` will only be populated if this
  // IsInMainFrame().
  void UnblockPendingSubframeNavigationRequestsIfNeeded();

  // If this request is a same-origin cross-document traversal (i.e., session
  // history navigation), this will send a message to the renderer to have it
  // fire the navigate event. Normally, the renderer fires the navigate event at
  // navigation start for cross-document navigations, before sending the
  // BeginNavigation to the browser. That doesn't work for traversals, because
  // the renderer don't know which frame(s) will navigate. This is called after
  // beforeunload events fire and after any navigation start throttles have
  // resumed, so we know the navigation is proceeding. The navigate event can't
  // cancel a cross-document traversal, so it can be sent in parallel, instead
  // of blocking and waiting for the result.
  void MaybeDispatchNavigateEventForCrossDocumentTraversal();

  // Returns if we should add/reset the `CookieChangeListener` for the current
  // navigation.
  bool ShouldAddCookieChangeListener();

  // Returns the `StoragePartition` based on the config from the `site_info_`.
  StoragePartition* GetStoragePartitionWithCurrentSiteInfo();

  // Passes the response body contents to the original caller using the stored
  // callback once the body has been successfully read from its corresponding
  // data pipe.
  void OnResponseBodyReady(MojoResult result);

  // Helper to record early RenderFrameHost swap metrics at the end of a
  // navigation.
  void RecordEarlyRenderFrameHostSwapMetrics();

  // Helpers for GetTentativeOriginAtRequestTime and GetOriginToCommit.
  std::pair<url::Origin, std::string>
  GetOriginForURLLoaderFactoryUncheckedWithDebugInfo();
  url::Origin GetOriginForURLLoaderFactoryUnchecked();

  void MaybeRecordTraceEventsAndHistograms();

  void ResetViewTransitionState();

  // This check is to prevent a race condition where a parent fenced frame
  // initiates a nested fenced frame navigation right before the entire frame
  // tree has network access disabled. If such navigation is allowed to commit,
  // the navigated fenced frame will have network access. This allows parent
  // fenced frame to communicate cross-site data into child fenced frame, which
  // is bad. So we need to disable navigations when both the embedder and nested
  // frames have already disabled network.
  bool IsDisabledEmbedderInitiatedFencedFrameNavigation();

  // Sets the expected process to the process of the current associated RFH.
  void SetExpectedProcessIfAssociated();

  // Sets the Document-Isolation-Policy header to a default value in unsecure
  // contexts or if DocumentIsolationPolicy is not supported.
  void SanitizeDocumentIsolationPolicyHeader();

  // Never null. The pointee node owns this navigation request instance.
  // This field is not a raw_ptr because of incompatibilities with tracing
  // (TRACE_EVENT*), perfetto::TracedDictionary::Add and gmock/EXPECT_THAT.
  RAW_PTR_EXCLUSION FrameTreeNode* const frame_tree_node_;

  // Used for short-lived NavigationRequest created at DidCommit time for the
  // purpose of committing navigation that were not driven by the browser
  // process. This is used in only two cases:
  //  - same-document navigation initiated by the renderer process.
  //  - the synchronous about:blank navigation.
  const bool is_synchronous_renderer_commit_;

  // The RenderFrameHost that this navigation intends to commit in. The value
  // will be set when we know the final RenderFrameHost that the navigation will
  // commit in (i.e. when we receive the final network response for most
  // navigations). Note that currently this can be reset to std::nullopt for
  // cross-document restarts and some failed navigations.
  // TODO(crbug.com/40063115): Don't reset this on failed navigations,
  // and ensure the NavigationRequest doesn't outlive the `render_frame_host_`
  // picked for failed Back/Forward Cache restores.
  // Invariant: At least one of |loader_| or |render_frame_host_| is
  // null/std::nullopt.
  std::optional<
      base::SafeRef<RenderFrameHostImpl, base::SafeRefDanglingUntriaged>>
      render_frame_host_;

  // Initialized on creation of the NavigationRequest. Sent to the renderer when
  // the navigation is ready to commit.
  // Note: When the navigation is ready to commit, the url in |common_params|
  // will be set to the final navigation url, obtained after following all
  // redirects.
  //
  // Note: |common_params_| and |begin_params_| are not const as they can be
  // modified during redirects.
  //
  // Note: |commit_params_| is not const because was_discarded will
  // be set in CreatedNavigationRequest.
  //
  // Note: |commit_params_->is_browser_initiated| and |common_params_| may be
  // mutated by ContentBrowserClient::OverrideNavigationParams at construction
  // time (i.e. before we actually kick off the navigation).
  blink::mojom::CommonNavigationParamsPtr common_params_;
  blink::mojom::BeginNavigationParamsPtr begin_params_;
  blink::mojom::CommitNavigationParamsPtr commit_params_;
  bool same_origin_ = false;
  // This member is calculated at ReadyToCommit time. It is used to compare
  // against renderer calculated origin and browser calculated one at commit
  // time.
  std::pair<std::optional<url::Origin>, std::string>
      browser_side_origin_to_commit_with_debug_info_;

  // Stores the NavigationUIData for this navigation until the NavigationHandle
  // is created. This can be null if the embedded did not provide a
  // NavigationUIData at the beginning of the navigation.
  std::unique_ptr<NavigationUIData> navigation_ui_data_;

  // URLLoaderFactory to facilitate loading blob URLs.
  const scoped_refptr<network::SharedURLLoaderFactory> blob_url_loader_factory_;

  // A bundle of URLLoaderFactory to facilitate loading subresources.
  // It is shared between prefetch, topics, keep-alive, and fetchLater.
  scoped_refptr<network::SharedURLLoaderFactory>
      subresource_proxying_factory_bundle_ = nullptr;

  NavigationState state_ = NOT_STARTED;
  bool is_navigation_started_ = false;

  // Manages the lifetime of a pre-created ServiceWorkerContainerHost until a
  // corresponding container is created in the renderer. This must be destroyed
  // after `loader_` to avoid dangling pointers, since `loader_` can have a
  // raw_ptr to this object.
  std::unique_ptr<ServiceWorkerMainResourceHandle> service_worker_handle_;

  std::unique_ptr<NavigationURLLoader> loader_;

  bool navigation_visible_to_embedder_ = false;
#if BUILDFLAG(IS_ANDROID)
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
  std::optional<BindingsPolicySet> bindings_;

  scoped_refptr<SiteInstanceImpl> starting_site_instance_;

  // Whether the navigation should be sent to a renderer a process. This is
  // true, except for 204/205 responses and downloads.
  bool response_should_be_rendered_ = true;

  // Whether devtools overrides were applied on the User-Agent request header.
  bool devtools_user_agent_override_ = false;

  // Whether devtools overrides were applied on the Accept-Language request
  // header.
  bool devtools_accept_language_override_ = false;

  // The type of RenderFrameHost associated with this navigation.
  AssociatedRenderFrameHostType associated_rfh_type_ =
      AssociatedRenderFrameHostType::NONE;

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
  std::optional<net::SSLInfo> ssl_info_;
  std::optional<net::AuthChallengeInfo> auth_challenge_info_;
  bool is_download_ = false;
  GlobalRequestID request_id_;
  std::unique_ptr<NavigationEarlyHintsManager> early_hints_manager_;

  // Holds information for the navigation while the WillFailRequest
  // checks are performed by the NavigationHandle.
  bool has_stale_copy_in_cache_ = false;
  net::Error net_error_ = net::OK;
  int extended_error_code_ = 0;

  // Detailed host resolution error information. The error code in
  // |resolve_error_info_.error| should be consistent with (but not necessarily
  // the same as) |net_error_|. In the case of a host resolution error, for
  // example, |net_error_| should be ERR_NAME_NOT_RESOLVED while
  // |resolve_error_info_.error| may give a more detailed error such as
  // ERR_DNS_TIMED_OUT.
  net::ResolveErrorInfo resolve_error_info_;

  // Identifies in which RenderProcessHost this navigation is expected to
  // commit.
  int expected_render_process_host_id_ = ChildProcessHost::kInvalidUniqueID;

  // The SiteInfo of this navigation, as obtained from
  // SiteInstanceImpl::ComputeSiteInfo().
  SiteInfo site_info_;

  base::OnceClosure on_start_checks_complete_closure_;

  // Used in the network service world to pass the subressource loader params
  // to the renderer. Used by ServiceWorker and
  // SignedExchangeSubresourcePrefetch.
  SubresourceLoaderParams subresource_loader_params_;

  // DocumentToken to use for the newly-committed document in a cross-document
  // navigation. Currently set immediately before sending CommitNavigation to
  // the renderer. In the future, this may be populated earlier to allow lookup
  // of a navigation request by the document that it may create, similar to how
  // `NavigationOrDocumentHandle` behaves.
  std::optional<blink::DocumentToken> document_token_;

  // See comment on accessor.
  const base::UnguessableToken devtools_navigation_token_ =
      base::UnguessableToken::Create();

  std::optional<std::vector<blink::mojom::TransferrableURLLoaderPtr>>
      subresource_overrides_;

  // The NavigationClient interface for that requested this navigation in the
  // case of a renderer initiated navigation. It is expected to be bound until
  // this navigation commits or is canceled.
  mojo::AssociatedRemote<mojom::NavigationClient> request_navigation_client_;

  // The NavigationClient interface used to commit the navigation. For now, this
  // is only used for same-site renderer-initiated navigation.
  // TODO(crbug.com/40276805): Extend to all types of navigation.
  mojo::AssociatedRemote<mojom::NavigationClient> commit_navigation_client_;

  // If set, any redirects to HTTP for this navigation will be upgraded to
  // HTTPS. This is used only on subframe navigations, when
  // upgrade-insecure-requests is set as a CSP policy.
  bool upgrade_if_insecure_ = false;

  // The offset of the new document in the history.
  const int navigation_entry_offset_ = 0;

  // Owns the NavigationThrottles associated with this navigation, and is
  // responsible for notifying them about the various navigation events.
  std::unique_ptr<NavigationThrottleRunner> throttle_runner_;

  // Once the navigation has passed all throttle checks the navigation will
  // commit. However, we may need to defer the commit until certain conditions
  // are met. CommitDeferringConditionRunner is responsible for deferring a
  // commit if needed and resuming it, by calling
  // OnCommitDeferringConditionChecksComplete, once all checks passed.
  //
  // For prerendered page activation, it doesn't run the NavigationThrottles and
  // run the CommitDeferringConditionRunner at the beginning of
  // BeginNavigation(). See the comment on RunCommitDeferringConditions() for
  // details.
  std::unique_ptr<CommitDeferringConditionRunner> commit_deferrer_;

  // Indicates whether the navigation changed which NavigationEntry is current.
  bool subframe_entry_committed_ = false;

  // True if the committed entry has replaced the existing one.
  // A non-user initiated redirect causes such replacement.
  bool did_replace_entry_ = false;

  // Set to false if we want to update the session history but not update the
  // browser history. E.g., on unreachable urls or navigations in non-primary
  // frame trees.
  bool should_update_history_ = false;

  // The previous main frame URL that the user was on. This may be empty if
  // there was no last committed entry.
  GURL previous_main_frame_url_;

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

  // Used to hold prefetched signed exchanges. This is shared with the
  // navigation initiator's RenderFrameHostImpl. This also means that only the
  // navigations that were directly initiated by the frame that made the
  // prefetches could use the prefetched resources, which is a different
  // behavior from regular prefetches (where all prefetched resources are
  // stored and shared in http cache).
  scoped_refptr<PrefetchedSignedExchangeCache>
      prefetched_signed_exchange_cache_;

  // Timing information of loading for the navigation. Used for recording UMAs.
  NavigationHandleTiming navigation_handle_timing_;

  // The time this navigation was ready to commit.
  base::TimeTicks ready_to_commit_time_;

  // The time BeginNavigation() was called.
  base::TimeTicks begin_navigation_time_;

  // The time OnResponseStarted() was called.
  base::TimeTicks receive_response_time_;

  // The first `fetchStart` time. This is different from the
  // `first_request_start_time` in `NavigationHandleTiming` since the
  // `first_fetch_start_time_` is the first time when the browser is
  // ready to fetch using an HTTP request, whereas the `requestStart` is
  // when the browser has obtained a connection and is ready to send the
  // HTTP request.
  base::TimeTicks first_fetch_start_time_;

  // The time when the final (which might also be the first) headers are
  // received.
  base::TimeTicks final_receive_headers_end_time_;

  // The time WillStartRequest() was called.
  base::TimeTicks will_start_request_time_;

  // Set in ReadyToCommitNavigation.
  bool is_same_process_ = true;

  // If set, starting the navigation will immediately result in an error page
  // with this html as content and |net_error| as the network error.
  std::string post_commit_error_page_html_;

  // This test-only callback will be run when BeginNavigation() is called.
  base::OnceClosure begin_navigation_callback_for_testing_;

  // This test-only callback will be run when all throttle checks have been
  // performed. If the callback returns true, On*ChecksComplete functions are
  // skipped, and only the test callback is being performed.
  // TODO(clamy): Revisit the unit test architecture.
  ThrottleChecksFinishedCallback complete_callback_for_testing_;

  // Test-only callback. Called when we're ready to call CommitNavigation.
  // Unlike above, this is informational only; it does not affect the request.
  base::OnceClosure ready_to_commit_callback_for_testing_;

  // Unique id that identifies the navigation for which this NavigationRequest
  // is created.
  const int64_t navigation_id_ = ++unique_id_counter_;
  // static member for generating the unique id above.
  static int64_t unique_id_counter_;

  // Timer for detecting an unexpectedly long time to commit a navigation.
  base::OneShotTimer commit_timeout_timer_;

  base::CallbackListSubscription
      render_process_blocked_state_changed_subscription_;

  // The headers used for the request. The value of this comes from
  // |begin_params_->headers|. If not set, it needs to be calculated.
  std::optional<net::HttpRequestHeaders> request_headers_;

  // Used to update the request's headers. When modified during the navigation
  // start, the headers will be applied to the initial network request. When
  // modified during a redirect, the headers will be applied to the redirected
  // request.
  net::HttpRequestHeaders modified_request_headers_;

  net::HttpRequestHeaders cors_exempt_request_headers_;

  // Set of headers to remove during the redirect phase. This can only be
  // modified during the redirect phase.
  std::vector<std::string> removed_request_headers_;

  // The RenderFrameHost that is being restored from the back/forward cache.
  // This can be null if this navigation is not restoring a page from the
  // back/forward cache.
  std::optional<base::SafeRef<RenderFrameHostImpl>>
      rfh_restored_from_back_forward_cache_;

  // Whether the navigation is for restoring a page from the back/forward cache
  // or not.
  const bool is_back_forward_cache_restore_;

  // These are set to the values from the FrameNavigationEntry this
  // NavigationRequest is associated with (if any).
  int64_t frame_entry_item_sequence_number_ = -1;
  int64_t frame_entry_document_sequence_number_ = -1;

  // If non-empty, it represents the IsolationInfo explicitly asked to be used
  // for this NavigationRequest.
  std::optional<net::IsolationInfo> isolation_info_;

  // This is used to store the `RenderFrameHostManager::current_frame_host()` id
  // when the navigation commits and about to potentially change the current
  // RenderFrameHost. The ID (if set) refers to the RFH of the document that was
  // replaced when this navigation committed, while the ID saved in
  // `current_render_frame_host_id_at_construction_` below refers to the RFH of
  // the document that was there when this navigation was started. The two might
  // be different, since other navigations could commit and change the current
  // RFH before this navigation commits, which can happen with navigation
  // queueing or early RFH swap.
  GlobalRenderFrameHostId previous_render_frame_host_id_;

  // This is used to store the `RenderFrameHostManager::current_frame_host()` id
  // at request creation time. See also the comment for
  // `previous_render_frame_host_id_` above on how these two IDs may differ.
  const GlobalRenderFrameHostId current_render_frame_host_id_at_construction_;

  // Frame token of the frame host that initiated the navigation, derived from
  // |begin_params().initiator_frame_token|. This is best effort: it is only
  // defined for some renderer-initiated navigations (e.g., not drag and drop).
  // The frame with the corresponding frame token may have been deleted before
  // the navigation begins. This parameter is defined if and only if
  // |initiator_process_id_| below is.
  const std::optional<blink::LocalFrameToken> initiator_frame_token_;

  // ID of the renderer process of the frame host that initiated the navigation.
  // This is defined if and only if |initiator_frame_token_| above is, and it is
  // only valid in conjunction with it.
  const int initiator_process_id_ = ChildProcessHost::kInvalidUniqueID;

  // The initiator Document's token, if it is present when this
  // NavigationRequest was created.
  std::optional<blink::DocumentToken> initiator_document_token_;

  // The sandbox flags of the navigation's initiator, if any.
  // WebSandboxFlags::kNone otherwise.
  const network::mojom::WebSandboxFlags sandbox_flags_initiator_;

  // Whether a navigation in a new window had the opener suppressed. False if
  // the navigation is not in a new window. Can only be true for renderer
  // initiated navigations which use `CreateBrowserInitiated()`.
  const bool was_opener_suppressed_ = false;

  // This tracks a connection between the current pending entry and this
  // request, such that the pending entry can be discarded if no requests are
  // left referencing it.
  std::unique_ptr<NavigationControllerImpl::PendingEntryRef> pending_entry_ref_;

  // Used only by DCHECK.
  // True if the NavigationThrottles are running an event, the request then can
  // be cancelled for deferring.
  bool processing_navigation_throttle_ = false;

  // Holds the required CSP for this navigation. This will be moved into
  // the RenderFrameHost at DidCommitNavigation time.
  network::mojom::ContentSecurityPolicyPtr required_csp_;

  // Whether the document loaded by this navigation will be committed inside an
  // iframe credentialless. Documents loaded inside credentialless iframes get
  // partitioned storage and use a transient NetworkAnonymizationKey.
  const bool is_credentialless_;

  // Non-nullopt from construction until |TakePolicyContainerHost()| is called.
  std::optional<NavigationPolicyContainerBuilder> policy_container_builder_;

  std::unique_ptr<CrossOriginEmbedderPolicyReporter> coep_reporter_;

  std::unique_ptr<input::PeakGpuMemoryTracker> loading_mem_tracker_;

  // Structure tracking the effects of the CrossOriginOpenerPolicy on this
  // navigation.
  CrossOriginOpenerPolicyStatus coop_status_{this};

#if DCHECK_IS_ON()
  bool is_safe_to_delete_ = true;
#endif

  // UKM source associated with the page we are navigated away from.
  const ukm::SourceId previous_page_ukm_source_id_;

  // If true, changes to the user-agent override require a reload. If false, a
  // reload is not necessary.
  bool ua_change_requires_reload_ = true;

  // Controls whether or not an error page is displayed on error. If set to
  // true, an error will be treated as if the user simply cancelled the
  // navigation.
  bool silently_ignore_errors_ = false;

  // Similar but only suppresses the error page when the error code is
  // net::ERR_BLOCKED_BY_CLIENT.
  bool silently_ignore_blocked_by_client_ = false;

  // Whether the navigation loads an MHTML document or a subframe of an MHTML
  // document.  The navigation might or might not be fullfilled from the MHTML
  // archive (see `is_mhtml_subframe_loaded_from_achive` in the NeedsUrlLoader
  // method).
  bool is_mhtml_or_subframe_ = false;

  // True when at least one preload Link header was received via an Early Hints
  // response. This is set only for a main frame navigation.
  bool was_resource_hints_received_ = false;

  // Set to true when this navigation has created parameters for
  // NavigationEarlyHintsManager. Used to check whether cross origin redirects
  // happened after Early Hints responses are received.
  bool did_create_early_hints_manager_params_ = false;

  // Set to true when an Early Hints response was received before cross origin
  // redirects during navigation.
  bool did_receive_early_hints_before_cross_origin_redirect_ = false;

  // Observers listening to cookie access notifications for the network requests
  // made by this navigation.
  mojo::ReceiverSet<network::mojom::CookieAccessObserver> cookie_observers_;

  // Observers listening to Trust Token access notifications for the network
  // requests made by this navigation.
  mojo::ReceiverSet<network::mojom::TrustTokenAccessObserver>
      trust_token_observers_;

  // Observers listening to shared dictionary access notifications for the
  // network requests made by this navigation.
  mojo::ReceiverSet<network::mojom::SharedDictionaryAccessObserver>
      shared_dictionary_observers_;

  OriginAgentClusterEndResult origin_agent_cluster_end_result_ =
      OriginAgentClusterEndResult::kNotRequestedAndNotOriginKeyed;

  net::IsolationInfo isolation_info_for_subresources_;

  // Set while CommitDeferringConditions are running for prerendered page
  // activation. This is needed as PrerenderHost hasn't been reserved and
  // prerender_frame_tree_node_id() is not available yet while they are
  // running.
  bool is_running_potential_prerender_activation_checks_ = false;

  // Set to true before the fenced frame url mapping. Reset to false when the
  // mapping finishes. If the initial mapping state of the urn:uuid is pending,
  // the mapping will finish asynchronously; otherwise, the mapping will finish
  // synchronously.
  bool is_deferred_on_fenced_frame_url_mapping_ = false;

  // The start time of fenced frame url mapping.
  base::TimeTicks fenced_frame_url_mapping_start_time_;

  // The root frame tree node id of the prerendered page. This will be a valid
  // FrameTreeNodeId value when this navigation will activate a prerendered
  // page. For all other navigations this will be an invalid FrameTreeNodeId. We
  // only know whether this is the case when BeginNavigation is called so the
  // optional will be empty until then and callers must not query its value
  // before it's been computed.
  std::optional<FrameTreeNodeId> prerender_frame_tree_node_id_;

  // Contains state pertaining to a prerender activation. This is only used if
  // this navigation is a prerender activation.
  struct PrerenderActivationNavigationState {
    PrerenderActivationNavigationState();
    ~PrerenderActivationNavigationState();

    // Used to store a cloned NavigationEntry for activating a prerendered page.
    // |prerender_navigation_entry| is cloned and stored in NavigationRequest
    // when the prerendered page is transferred to the target FrameTree and is
    // consumed when NavigationController needs a new entry to commit.
    std::unique_ptr<NavigationEntryImpl> prerender_navigation_entry;

    // Used to store the FrameReplicationState for the prerendered page prior to
    // activation. Value is to be used to populate
    // DidCommitProvisionalLoadParams values and to verify the replication state
    // after activation.
    blink::mojom::FrameReplicationState prerender_main_frame_replication_state;
  };

  std::optional<PrerenderActivationNavigationState> prerender_navigation_state_;

  // The following fields that constitute the ClientSecurityState. This
  // state is used to take security decisions about the request, and later on
  // when passed to the RenderFrameHostImpl, about the fetching of subresources.
  //
  // They have some default values and get updated via inheritance or network
  // responses/redirects. Finally they get passed down to the
  // RenderFrameHostImpl at commit time.
  //
  // The policy to apply to private network requests for subresources of the
  // document we are navigating to. Influenced by the document's policy
  // container, origin, and `ContentBrowserClient`.
  network::mojom::PrivateNetworkRequestPolicy private_network_request_policy_ =
      network::mojom::PrivateNetworkRequestPolicy::kWarn;

  // The list of web features that were used by the new document during
  // navigation. These can only be logged once the document commits, so they are
  // held in this vector until then.
  std::vector<blink::mojom::WebFeature> web_features_to_log_;

  // Messages to be printed on the console in the target RenderFrameHost of this
  // NavigationRequest.
  std::vector<ConsoleMessage> console_messages_;

  // Indicates that this navigation is for PDF content in a renderer. On
  // Android, this can only be true when a PDF NativePage is created for
  // a main frame navigation.
  bool is_pdf_ = false;

  // Indicates that this navigation is an embedder-initiated navigation of a
  // fenced frame root. That is to say, the navigation is caused by a `src`
  // attribute mutation on the <fencedframe> element, which cannot be performed
  // from inside the fenced frame tree.
  // TODO(crbug.com/40202462): Make this `const` again once ShadowDOM is gone.
  bool is_embedder_initiated_fenced_frame_navigation_ = false;

  // On every embedder-initiated navigation of a fenced frame, i.e.
  // `is_embedder_initiated_fenced_frame_navigation_`, we reinitialize
  // the fenced frame properties with the default `FencedFrameProperties(url)`
  // constructor, which gives the fenced frame a fresh partition nonce.
  //
  // If the embedder-initiated navigation is to a urn:uuid, we overwrite
  // the default properties stored in this `NavigationRequest` with the
  // `FencedFrameProperties` bound to that urn:uuid, in
  // `NavigationRequest::OnFencedFrameURLMappingComplete`.
  //
  // For certain actions related to the pending `NavigationRequest` (rather
  // than the existing fenced frame document), e.g. partitioned network
  // requests for the pending navigation, we use the pending
  // `FencedFrameProperties`.
  //
  // If the navigation commits, this new set of fenced frame properties will be
  // stored in the fenced frame root FrameTreeNode in
  // `NavigationRequest::DidCommitNavigation`.
  //
  // If the navigation doesn't commit (e.g. an HTTP 204 response), the fenced
  // frame properties will not be stored in the fenced frame root.
  std::optional<FencedFrameProperties> fenced_frame_properties_;

  // For fenced frames, any contextual string that was written by the embedder
  // via `blink::FencedFrameConfig::setSharedStorageContext()` to be later
  // retrieved only inside an eligible shared storage worklet in the fenced
  // frame via `sharedStorage.context`. absl:nullopt if this request is not for
  // a fenced frame or if the context string wasn't set prior to this
  // navigation.
  std::optional<std::u16string> embedder_shared_storage_context_;

  // Prerender2:
  // The type to trigger prerendering. The value is valid only when Prerender2
  // is enabled.
  std::optional<PreloadingTriggerType> prerender_trigger_type_;
  // The suffix of a prerender embedder. This value is valid only when
  // PreloadingTriggerType is kEmbedder. Only used for metrics.
  std::string prerender_embedder_histogram_suffix_;

  // Prevents the compositor from requesting main frame updates early in
  // navigation.
  std::unique_ptr<ui::CompositorLock> compositor_lock_;

  // This navigation request should swap browsing instances as part of a test
  // reset.
  bool force_new_browsing_instance_ = false;

  // Whether the ongoing navigation resource request is eligible for topics
  // calculation. This is set before the initial request and each subsequent
  // redirect. If `topics_eligible_` is true, the request headers will contain
  // the "Sec-Browsing-Topics" header, and if the corresponding response headers
  // contain "Observe-Browsing-Topics: ?1", a topic observation will be stored.
  bool topics_eligible_ = false;

  // Whether this navigation request is an iframe navigation for which the
  // adAuctionHeaders attribute is set. Only requests with this attribute may be
  // eligible for ad auction headerse, but not all requests with this attribute
  // are eligible. `ad_auction_headers_eligible_`, below, indicates whether or
  // not this request is eligible.
  const bool has_ad_auction_headers_attribute_ = false;

  // Whether the ongoing navigation resource request should have its Ad Auction
  // response headers examined for interception. This is set before the initial
  // request for iframe navigations that provide the `adAuctionHeaders`
  // attribute. On redirect or error, this is always set to false. If this is
  // set to true, the request headers will contain the "Sec-Ad-Auction-Fetch:
  // ?1" header, and several response headers will be intercepted. See
  // content/browser/interest_group/ad_auction_headers_util.h for more details.
  bool ad_auction_headers_eligible_ = false;

  // Whether or not the original request (without considering redirects or
  // permissions policy) opted-in to write to shared storage from response
  // headers. See https://github.com/WICG/shared-storage#from-response-headers
  bool shared_storage_writable_opted_in_ = false;

  // Whether or not the current request is eligible to shared storage from
  // response headers. See
  // https://github.com/WICG/shared-storage#from-response-headers
  bool shared_storage_writable_eligible_ = false;

  // A WeakPtr for the BindContext associated with the browser routing loader
  // factory for the committing document. This will be set in
  // `CommitNavigation()`, and can become null if the corresponding factory is
  // destroyed. Upon `DidCommitNavigation()`,
  // `subresource_proxying_url_loader_service_bind_context_` will be notified
  // with the committed document.
  base::WeakPtr<SubresourceProxyingURLLoaderService::BindContext>
      subresource_proxying_url_loader_service_bind_context_;

  // A WeakPtr for the FactoryContext associated with the browser fetch
  // keepalive loader factory for the committing document.
  // This field will be set in `CommitNavigation()`, and can become null if the
  // corresponding factory is destroyed.
  // Upon `DidCommitNavigation()`, this field will be notified with the
  // committed document.
  base::WeakPtr<KeepAliveURLLoaderService::FactoryContext>
      keep_alive_url_loader_factory_context_;
  // A WeakPtr for the FactoryContext associated with the browser fetchlater
  // loader factory for the committing document.
  // See also `keep_alive_url_loader_factory_context_` for the timing to update.
  base::WeakPtr<KeepAliveURLLoaderService::FactoryContext>
      fetch_later_loader_factory_context_;

  scoped_refptr<NavigationOrDocumentHandle> navigation_or_document_handle_;

  // Exposes getters and setters for Blink Runtime-Enabled Features to the
  // browser process. Any feature set using the RuntimeFeatureStateContext
  // before navigation commit will be communicated back to the renderer process.
  // NOTE: these feature changes will apply to the "to-be-created" document.
  blink::RuntimeFeatureStateContext runtime_feature_state_context_;

  // Renderer-initiated navigations can be canceled until the JS task that
  // started the navigation finishes. See RendererCancellationThrottle for more
  // details. The window of time in which the renderer can cancel the navigation
  // is called the "cancellation window" and the navigation can't commit until
  // the cancellation window ended, which `renderer_cancellation_listener_`
  // listens to. `renderer_cancellation_window_ended_` is true if the
  // cancellation window had ended. If
  // `renderer_cancellation_window_ended_callback_` is set, the navigation is
  // being deferred by RendererCancellationThrottle to wait for the cancellation
  // window to finish or for the navigation to get canceled. If it is set when
  // the cancellation window ended, the callback will be run, to resume the
  // navigation.
  mojo::Receiver<mojom::NavigationRendererCancellationListener>
      renderer_cancellation_listener_{this};
  bool renderer_cancellation_window_ended_ = false;
  base::OnceClosure renderer_cancellation_window_ended_callback_;

  // Whether a Cookie header added to this request should not be overwritten by
  // the network service.
  bool allow_cookies_from_browser_ = false;

  // If the browser has asked the renderer to commit the navigation in a
  // speculative RenderFrameHost, but the renderer has not yet responded, a
  // subsequent navigation request will be queued when it is about to pick its
  // final RenderFrameHost, to avoid deleting the previous navigation's pending
  // commit RenderFrameHost. A queued navigation should populate this field with
  // a closure that resumes committing the navigation when run.
  //
  // The closure should always be bound with a `WeakPtr` receiver. To avoid
  // weird reentrancy bugs, it will be run as a non-nested posted task, which
  // means the original NavigationRequest could already be deleted by the
  // time the closure runs.
  base::OnceClosure resume_commit_closure_;

  // Metrics for measuring the impact of navigation queueing. Note that while
  // `resume_commit_closure_` is set on the navigation that was *blocked*, these
  // metrics are set on the NavigationRequest that is *blocking*.
  struct PendingCommitMetrics {
    // When this `NavigationRequest` caused its speculative RenderFrameHost to
    // enter pending commit.
    base::TimeTicks start_time;
    // How many `GetFrameHostForNavigation()` calls failed in total.
    int blocked_count = 0;
    // How many `GetFrameHostForNavigation()` calls failed when trying to assign
    // a final `RenderFrameHost` for commit.
    int blocked_commit_count = 0;
  };
  PendingCommitMetrics pending_commit_metrics_;

  // Records whether the new document will commit inside another BrowsingContext
  // group as a result of this navigation, and for what reason. Deciding whether
  // to clear the window name and to clear the proxies are based on this value.
  //
  // It is created with a default no-swap value, and is set within
  // RenderFrameHostManager::GetSiteInstanceForNavigation(). It is generally set
  // more than once, first for a speculative computation before receiving
  // headers, then for each redirect, and finally once a definitve response has
  // been received. It might also never be set if the navigation does not go
  // through SiteInstance selection, such as for a renderer initiated
  // same-document navigation.
  BrowsingContextGroupSwap browsing_context_group_swap_ =
      BrowsingContextGroupSwap::CreateDefault();

  // See `set_pending_navigation_api_key()` for context.
  std::optional<std::string> pending_navigation_api_key_;

  // If this NavigationRequest is for a main-frame same-document back/forward
  // navigation, any subframe NavigationRequests are deferred until the renderer
  // has a chance to fire a navigate event. If the navigate
  // event allows the navigation to proceed,
  // UnblockPendingSubframeNavigationRequestsIfNeeded() will resume these
  // requests
  std::vector<base::WeakPtr<SubframeHistoryNavigationThrottle>>
      subframe_history_navigation_throttles_;

  // If this NavigationRequest is in a subframe and part of a history traversal,
  // and the main frame is performing a same-document navigation, this token
  // may be set if there is a possibility that JS in the main frame will cancel
  // the history traversal via the navigate event. In that case, this token is
  // used to look up the main frame's NavigationRequest so that it can be passed
  // SubframeHistoryNavigationThrottle can defer this request until the main
  // frame commits.
  std::optional<base::UnguessableToken>
      main_frame_same_document_navigation_token_;

  // The listener that receives cookie change events and maintains cookie change
  // information for the domain of the URL that this `NavigationRequest` is
  // navigating to. The listener will observe all the cookie changes starting
  // from the navigation/redirection, and it will be moved to the
  // `RenderFrameHostImpl` when the navigation is committed and continues
  // observing until the destruction of the document.
  // See `RenderFrameHostImpl::CookieChangeListener`.
  std::unique_ptr<RenderFrameHostImpl::CookieChangeListener>
      cookie_change_listener_;

  // LCP Critical Path Predictor managed hint data those were already available
  // at the time of navigation. The hint is passed along to the renderer process
  // on commit along with the other navigation params.
  blink::mojom::LCPCriticalPathPredictorNavigationTimeHint lcpp_hint_;

  // The WebUI object to be used for this navigation. When a RenderFrameHost has
  // been picked for the navigation, the WebUI object will be moved to be owned
  // by the RenderFrameHost.
  std::unique_ptr<WebUIImpl> web_ui_;

  // Returns whether this navigation is currently deferred.
  bool IsDeferred();

  // The watcher used to asynchronously read the response body from the data
  // pipe. Once the response body is read, it is passed to the original caller
  // using the stored callback. This is instantiated when a response body is
  // requested and destroyed upon returning the response body.
  std::unique_ptr<mojo::SimpleWatcher> response_body_watcher_;
  ResponseBodyCallback response_body_callback_;
  // Used to confirm that any NavigationThrottle that calls `GetResponseBody()`
  // becomes deferred.
  bool was_get_response_body_called_ = false;

  // Used to prevent re-entrancy into `Resume()`.
  bool is_resuming_ = false;

  EarlyRenderFrameHostSwapType early_render_frame_host_swap_type_ =
      EarlyRenderFrameHostSwapType::kNone;

  // Whether the embedder indicated this navigation is being used for
  // advertising porpoises.
  bool is_ad_tagged_ = false;

  // This is the origin to commit value calculated at request time for data: URL
  // navigations. It is stored so that the opaque origin nonce can be maintained
  // across a navigation. This value is used when a speculative SiteInstance
  // is created so the site URL of the navigation can match the initiator
  // origin. We store the tentative origin to commit value, since we need it
  // before ready to commit time, which is when the regular origin to commit
  // value is available.
  std::optional<url::Origin> tentative_data_origin_to_commit_;

  // `pageswap` can be fired at different stages of the navigation lifecycle:
  // - ready to commit if this navigation is associated with a ViewTransition.
  // - unload old document if there is no ViewTransition opt-in.
  // This tracks whether the pageswap event has been fired for this
  // navigation.
  bool did_fire_page_swap_ = false;

  // Set if there has been any cross-origin redirects in the lifetime of this
  // request.
  bool did_encounter_cross_origin_redirect_ = false;

  // A scoped reference on the ViewTransition resources generated for this
  // navigation. This is set after we received the cached results from the old
  // Document's renderer. If the navigation commits, the resources are
  // transferred to the new Document's view. If the navigation finishes without
  // committing, the resources are destroyed with this request.
  std::unique_ptr<ScopedViewTransitionResources> view_transition_resources_;

  // If true, this means that this navigation request was initiated by an
  // animated transition.
  bool was_initiated_by_animated_transition_ = false;

  // If the navigation is cancelled/discarded before it commits, the reason
  // for cancellation will be saved.
  std::optional<NavigationDiscardReason> navigation_discard_reason_;

  // If true, HTTPS Upgrades will be disabled on this navigation request.
  bool force_no_https_upgrade_ = false;

  // The initial request method of the request, before any redirects.
  std::string request_method_;

  base::WeakPtrFactory<NavigationRequest> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_NAVIGATION_REQUEST_H_
