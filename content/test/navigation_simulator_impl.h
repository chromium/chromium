// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_NAVIGATION_SIMULATOR_IMPL_H_
#define CONTENT_TEST_NAVIGATION_SIMULATOR_IMPL_H_

#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/navigation_simulator.h"
#include "content/test/test_render_frame_host.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "net/base/host_port_pair.h"
#include "net/base/ip_endpoint.h"
#include "net/base/load_flags.h"
#include "net/dns/public/resolve_error_info.h"
#include "net/http/http_connection_info.h"
#include "third_party/blink/public/common/navigation/impression.h"
#include "third_party/blink/public/mojom/loader/mixed_content.mojom.h"
#include "third_party/blink/public/mojom/loader/referrer.mojom-forward.h"
#include "url/gurl.h"

namespace content {

class FrameTreeNode;
class NavigationRequest;
class TestRenderFrameHost;
class WebContentsImpl;

namespace mojom {
class NavigationClient;
}

class NavigationSimulatorImpl : public NavigationSimulator,
                                public WebContentsObserver {
 public:
  ~NavigationSimulatorImpl() override;

  // TODO(crbug.com/40150370): Remove `original_url` as it's not used.
  static std::unique_ptr<NavigationSimulatorImpl> CreateBrowserInitiated(
      const GURL& original_url,
      WebContents* contents);

  static std::unique_ptr<NavigationSimulatorImpl> CreateHistoryNavigation(
      int offset,
      WebContents* web_contents,
      bool is_renderer_initiated);

  // TODO(crbug.com/40150370): Remove `original_url` as it's not used.
  static std::unique_ptr<NavigationSimulatorImpl> CreateRendererInitiated(
      const GURL& original_url,
      RenderFrameHost* render_frame_host);

  static std::unique_ptr<NavigationSimulatorImpl> CreateFromPending(
      NavigationController& controller);

  // Creates a NavigationSimulator for an already-started navigation happening
  // in `frame_tree_node`. Can be used to drive the navigation to completion.
  static std::unique_ptr<NavigationSimulatorImpl> CreateFromPendingInFrame(
      FrameTreeNode* frame_tree_node);

  // NavigationSimulator implementation.
  void Start() override;
  void Redirect(const GURL& new_url) override;
  void ReadyToCommit() override;
  void Commit() override;
  void AbortCommit() override;
  void AbortFromRenderer() override;
  void Fail(int error_code) override;
  void CommitErrorPage() override;
  void CommitSameDocument() override;
  RenderFrameHost* GetFinalRenderFrameHost() override;
  void Wait() override;
  bool IsDeferred() override;
  bool HasFailed() override;
  void SetInitiatorFrame(RenderFrameHost* initiator_frame_host) override;
  void SetTransition(ui::PageTransition transition) override;
  void SetHasUserGesture(bool has_user_gesture) override;
  void SetNavigationInputStart(base::TimeTicks navigation_input_start) override;
  void SetNavigationStart(base::TimeTicks navigation_start) override;
  void SetReloadType(ReloadType reload_type) override;
  void SetMethod(const std::string& method) override;
  void SetIsFormSubmission(bool is_form_submission) override;
  void SetReferrer(blink::mojom::ReferrerPtr referrer) override;
  void SetSocketAddress(const net::IPEndPoint& remote_endpoint) override;
  void SetWasFetchedViaCache(bool was_fetched_via_cache) override;
  void SetIsSignedExchangeInnerResponse(
      bool is_signed_exchange_inner_response) override;
  void SetPermissionsPolicyHeader(
      blink::ParsedPermissionsPolicy permissions_policy_header) override;
  void SetContentsMimeType(const std::string& contents_mime_type) override;
  void SetRedirectHeaders(
      scoped_refptr<net::HttpResponseHeaders> redirect_headers) override;
  void SetResponseHeaders(
      scoped_refptr<net::HttpResponseHeaders> response_headers) override;
  void SetResponseBody(
      mojo::ScopedDataPipeConsumerHandle response_body) override;
  void SetAutoAdvance(bool auto_advance) override;
  void SetResolveErrorInfo(
      const net::ResolveErrorInfo& resolve_error_info) override;
  void SetSSLInfo(const net::SSLInfo& ssl_info) override;
  void SetResponseDnsAliases(std::vector<std::string> aliases) override;
  void SetEarlyHintsPreloadLinkHeaderReceived(bool received) override;

  NavigationThrottle::ThrottleCheckResult GetLastThrottleCheckResult() override;
  NavigationRequest* GetNavigationHandle() override;
  content::GlobalRequestID GetGlobalRequestID() override;

  void SetKeepLoading(bool keep_loading) override;
  void StopLoading() override;

  // Additional utilities usable only inside content/.

  // This will do the very beginning of a navigation but stop before the
  // beforeunload event response. Will leave the Simulator in a
  // WAITING_BEFORE_UNLOAD state. We do not wait for beforeunload event when
  // starting renderer-side, use solely for browser initiated navigations.
  void BrowserInitiatedStartAndWaitBeforeUnload();

  // Set LoadURLParams and make browser initiated navigations use
  // LoadURLWithParams instead of LoadURL.
  void SetLoadURLParams(NavigationController::LoadURLParams* load_url_params);
  void set_should_check_main_world_csp(
      network::mojom::CSPDisposition disposition) {
    should_check_main_world_csp_ = disposition;
  }

  // Set DidCommit*Params history_list_was_cleared flag to `history_cleared`.
  void set_history_list_was_cleared(bool history_cleared);

  // Manually force the value of should_replace_current_entry flag in
  // DidCommit*Params to `should_replace_current_entry`.
  void set_should_replace_current_entry(bool should_replace_current_entry) {
    should_replace_current_entry_ = should_replace_current_entry;
  }

  void set_http_connection_info(net::HttpConnectionInfo info) {
    http_connection_info_ = info;
  }

  // Whether to drop the swap out ack of the previous RenderFrameHost during
  // cross-process navigations. By default this is false, set to true if you
  // want the old RenderFrameHost to be left in a pending swap out state.
  void set_drop_unload_ack(bool drop_unload_ack) {
    drop_unload_ack_ = drop_unload_ack;
  }

  // Whether to drop the BeforeUnloadCompleted of the current RenderFrameHost at
  // the beginning of a browser-initiated navigation. By default this is false,
  // set to true if you want to simulate the BeforeUnloadCompleted manually.
  void set_block_invoking_before_unload_completed_callback(
      bool block_invoking_before_unload_completed_callback) {
    block_invoking_before_unload_completed_callback_ =
        block_invoking_before_unload_completed_callback;
  }

  void set_page_state(const blink::PageState& page_state) {
    page_state_ = page_state;
  }

  void set_origin(const url::Origin& origin) { origin_ = origin; }

  void set_impression(const blink::Impression& impression) {
    impression_ = impression;
  }

  void set_skip_service_worker(bool skip_service_worker) {
    skip_service_worker_ = skip_service_worker;
  }

  void set_initiator_origin(
      const std::optional<url::Origin>& initiator_origin) {
    initiator_origin_ = initiator_origin;
  }

  void set_request_headers(const std::string& headers) { headers_ = headers; }

  void set_load_flags(int load_flags) { load_flags_ = load_flags; }

  void set_mixed_content_context_type(
      blink::mojom::MixedContentContextType mixed_content_context_type) {
    mixed_content_context_type_ = mixed_content_context_type;
  }

  void set_searchable_form_url(const GURL& searchable_form_url) {
    searchable_form_url_ = searchable_form_url;
  }

  void set_searchable_form_encoding(
      const std::string& searchable_form_encoding) {
    searchable_form_encoding_ = searchable_form_encoding;
  }

  void set_href_translate(const std::string& href_translate) {
    href_translate_ = href_translate;
  }

  void set_request_context_type(
      blink::mojom::RequestContextType request_context_type) {
    request_context_type_ = request_context_type;
  }

  void set_insecure_request_policy(
      blink::mojom::InsecureRequestPolicy insecure_request_policy) {
    insecure_request_policy_ = insecure_request_policy;
  }

  void set_insecure_navigations_set(
      const std::vector<uint32_t> insecure_navigations_set) {
    insecure_navigations_set_ = insecure_navigations_set;
  }

  void set_has_potentially_trustworthy_unique_origin(
      bool has_potentially_trustworthy_unique_origin) {
    has_potentially_trustworthy_unique_origin_ =
        has_potentially_trustworthy_unique_origin;
  }

  void set_supports_loading_mode_header(std::string value) {
    supports_loading_mode_header_ = value;
  }

  void set_post_id(int64_t post_id) { post_id_ = post_id; }

  void set_response_postprocess_hook(
      base::RepeatingCallback<void(network::mojom::URLResponseHead&)> hook) {
    response_postprocess_hook_ = std::move(hook);
  }

 private:
  NavigationSimulatorImpl(const GURL& original_url,
                          bool browser_initiated,
                          WebContentsImpl* web_contents,
                          TestRenderFrameHost* render_frame_host);

  // Adds a test navigation throttle to `request_` which sanity checks various
  // callbacks have been properly called.
  void RegisterTestThrottle();

  // Initializes a NavigationSimulator from an existing NavigationRequest. This
  // should only be needed if a navigation was started without a valid
  // NavigationSimulator.
  void InitializeFromStartedRequest(NavigationRequest* request);

  // WebContentsObserver:
  void RenderFrameDeleted(RenderFrameHost* render_frame_host) override;
  void DidStartNavigation(NavigationHandle* navigation_handle) override;
  void DidRedirectNavigation(NavigationHandle* navigation_handle) override;
  void ReadyToCommitNavigation(NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(NavigationHandle* navigation_handle) override;

  void StartComplete();
  void RedirectComplete(int previous_num_will_redirect_request_called,
                        int previous_did_redirect_navigation_called);
  void WillProcessResponseComplete();
  void ReadyToCommitComplete();
  void FailComplete(int error_code);

  void OnWillStartRequest();
  void OnWillRedirectRequest();
  void OnWillFailRequest();
  void OnWillProcessResponse();

  // Simulates a browser-initiated navigation starting. Returns false if the
  // navigation failed synchronously.
  bool SimulateBrowserInitiatedStart();

  // Simulates a renderer-initiated navigation starting. Returns false if the
  // navigation failed synchronously.
  bool SimulateRendererInitiatedStart();

  // This method will block waiting for the navigation to reach the next
  // NavigationThrottle phase of the navigation to complete
  // (StartRequest|Redirect|Failed|ProcessResponse) if `auto_advance_`. This
  // waits until *after* throttle checks are run (if the navigation requires
  // throttle checks).  If !`auto_advance_` this will just set up state for
  // checking the result when the throttles end up finishing.
  void MaybeWaitForThrottleChecksComplete(base::OnceClosure complete_closure);

  // Like above but blocks waiting for the ReadyToCommit checks to complete.
  void MaybeWaitForReadyToCommitCheckComplete();

  // Sets `last_throttle_check_result_` and calls both the `wait_closure_` and
  // the `throttle_checks_complete_closure_`, if they are set.
  bool OnThrottleChecksComplete(NavigationThrottle::ThrottleCheckResult result);

  // Helper method to set the OnThrottleChecksComplete callback on the
  // NavigationRequest.
  void PrepareCompleteCallbackOnRequest();

  // Infers from internal parameters whether the navigation created a new
  // entry.
  bool DidCreateNewEntry(bool same_document, bool should_replace_current_entry);

  // Set the navigation to be done towards the specified navigation controller
  // offset. Typically -1 for back navigations or 1 for forward navigations.
  void SetSessionHistoryOffset(int offset);

  // Build DidCommitProvisionalLoadParams to commit the ongoing navigation,
  // based on internal NavigationSimulator state and given parameters.
  mojom::DidCommitProvisionalLoadParamsPtr BuildDidCommitProvisionalLoadParams(
      bool same_document,
      bool failed_navigation,
      int last_http_status_code);

  // Simulate the UnloadACK in the old RenderFrameHost if it was unloaded at the
  // commit time.
  void SimulateUnloadCompletionCallbackForPreviousFrameIfNeeded(
      RenderFrameHostImpl* previous_frame);

  // Certain navigations can skip throttle checks:
  // - same-document navigations
  // - about:blank navigations
  // - navigations not handled by the network stack
  // - page activations like prerendering and back-forward cache.
  bool NeedsThrottleChecks() const;

  // Whether the navigation performs CommitDeferringCondition checks before
  // committing. i.e. if it goes through the full
  // WillStartRequest->WillProcessResponse->etc.->Commit phases. This includes
  // all navigations that require throttle checks plus page activations like
  // prerendering/BFCache.
  bool NeedsPreCommitChecks() const;

  enum State {
    INITIALIZATION,
    WAITING_BEFORE_UNLOAD,
    STARTED,
    READY_TO_COMMIT,
    FAILED,
    FINISHED,
  };

  State state_ = INITIALIZATION;

  // The WebContents in which the navigation is taking place.
  // IMPORTANT: Because NavigationSimulator is used outside content/ where we
  // sometimes use WebContentsImpl and not TestWebContents, this cannot be
  // assumed to cast properly to TestWebContents.
  raw_ptr<WebContentsImpl, DanglingUntriaged> web_contents_;

  // The renderer associated with this navigation.
  // Note: this can initially be null for browser-initiated navigations.
  raw_ptr<TestRenderFrameHost, AcrossTasksDanglingUntriaged> render_frame_host_;

  raw_ptr<FrameTreeNode, AcrossTasksDanglingUntriaged> frame_tree_node_;

  // The NavigationRequest associated with this navigation.
  raw_ptr<NavigationRequest> request_;

  // Note: additional parameters to modify the navigation should be properly
  // initialized (if needed) in InitializeFromStartedRequest.
  GURL navigation_url_;
  net::IPEndPoint remote_endpoint_;
  bool was_fetched_via_cache_ = false;
  bool is_signed_exchange_inner_response_ = false;
  std::string initial_method_;
  bool is_form_submission_ = false;
  bool browser_initiated_;
  bool same_document_ = false;
  TestRenderFrameHost::LoadingScenario loading_scenario_ =
      TestRenderFrameHost::LoadingScenario::kOther;
  blink::mojom::ReferrerPtr referrer_;
  raw_ptr<RenderFrameHost> initiator_frame_host_ = nullptr;
  ui::PageTransition transition_;
  ReloadType reload_type_ = ReloadType::NONE;
  int session_history_offset_ = 0;
  bool has_user_gesture_ = true;
  base::TimeTicks navigation_input_start_;
  base::TimeTicks navigation_start_;
  mojo::PendingReceiver<blink::mojom::BrowserInterfaceBroker>
      browser_interface_broker_receiver_;
  std::string contents_mime_type_;
  scoped_refptr<net::HttpResponseHeaders> redirect_headers_;
  scoped_refptr<net::HttpResponseHeaders> response_headers_;
  blink::ParsedPermissionsPolicy permissions_policy_header_;
  mojo::ScopedDataPipeConsumerHandle response_body_;
  network::mojom::CSPDisposition should_check_main_world_csp_ =
      network::mojom::CSPDisposition::CHECK;
  net::HttpConnectionInfo http_connection_info_ =
      net::HttpConnectionInfo::kUNKNOWN;
  net::ResolveErrorInfo resolve_error_info_ = net::ResolveErrorInfo(net::OK);
  std::optional<net::SSLInfo> ssl_info_;
  std::optional<blink::PageState> page_state_;
  std::optional<url::Origin> origin_;
  std::optional<blink::Impression> impression_;
  int64_t post_id_ = -1;
  bool skip_service_worker_ = false;
  std::optional<url::Origin> initiator_origin_;
  std::string headers_;
  int load_flags_ = net::LOAD_NORMAL;
  blink::mojom::MixedContentContextType mixed_content_context_type_ =
      blink::mojom::MixedContentContextType::kBlockable;
  GURL searchable_form_url_;
  std::string searchable_form_encoding_;
  std::string href_translate_;
  blink::mojom::RequestContextType request_context_type_ =
      blink::mojom::RequestContextType::LOCATION;
  blink::mojom::InsecureRequestPolicy insecure_request_policy_ =
      blink::mojom::InsecureRequestPolicy::kLeaveInsecureRequestsAlone;
  std::vector<uint32_t> insecure_navigations_set_;
  bool has_potentially_trustworthy_unique_origin_ = false;

  // Any DNS aliases, as read from CNAME records, for the request URL that
  // would be in the network::mojom::URLResponseHead. The alias chain order
  // is preserved in reverse, from canonical name (i.e. address record name)
  // through to query name.
  std::vector<std::string> response_dns_aliases_;

  bool auto_advance_ = true;
  bool drop_unload_ack_ = false;
  bool block_invoking_before_unload_completed_callback_ = false;
  bool keep_loading_ = false;

  // Generic params structure used for fully customized browser initiated
  // navigation requests. Only valid if explicitely provided.
  raw_ptr<NavigationController::LoadURLParams> load_url_params_;

  bool history_list_was_cleared_ = false;
  bool should_replace_current_entry_ = false;
  bool was_aborted_prior_to_ready_to_commit_ = false;

  bool early_hints_preload_link_header_received_ = false;

  std::string supports_loading_mode_header_;

  // A hook that can be used to tweak the response before it is processed.
  // Called for both redirect and final responses.
  base::RepeatingCallback<void(network::mojom::URLResponseHead&)>
      response_postprocess_hook_;

  std::optional<bool> was_prerendered_page_activation_;

  // These are used to sanity check the content/public/ API calls emitted as
  // part of the navigation.
  int num_did_start_navigation_called_ = 0;
  int num_will_start_request_called_ = 0;
  int num_will_redirect_request_called_ = 0;
  int num_will_fail_request_called_ = 0;
  int num_did_redirect_navigation_called_ = 0;
  int num_will_process_response_called_ = 0;
  int num_ready_to_commit_called_ = 0;
  int num_did_finish_navigation_called_ = 0;

  // Holds the last ThrottleCheckResult calculated by the navigation's
  // throttles. Will be unset before WillStartRequest is finished. Will be unset
  // while throttles are being run, but before they finish.
  std::optional<NavigationThrottle::ThrottleCheckResult>
      last_throttle_check_result_;

  // GlobalRequestID for the associated NavigationHandle. Only valid after
  // WillProcessResponse has been invoked on the NavigationHandle.
  content::GlobalRequestID request_id_;

  // Closure that is set when MaybeWaitForThrottleChecksComplete is called.
  // Called in OnThrottleChecksComplete.
  base::OnceClosure throttle_checks_complete_closure_;

  // Closure that is called in OnThrottleChecksComplete if we are waiting on the
  // result. Calling this will quit the nested run loop.
  base::OnceClosure wait_closure_;

  // Closure that is called when DidStartNavigation is called.
  base::OnceClosure did_start_navigation_closure_;

  // This member simply ensures that we do not disconnect the NavigationClient
  // interface, as it would be interpreted as a cancellation coming from the
  // renderer process side. This member interface will never be bound.
  mojo::PendingAssociatedReceiver<mojom::NavigationClient>
      navigation_client_receiver_;

  base::WeakPtrFactory<NavigationSimulatorImpl> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_TEST_NAVIGATION_SIMULATOR_IMPL_H_
