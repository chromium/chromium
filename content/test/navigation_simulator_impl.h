// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_NAVIGATION_SIMULATOR_IMPL_H_
#define CONTENT_TEST_NAVIGATION_SIMULATOR_IMPL_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/optional.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/impression.h"
#include "content/public/test/navigation_simulator.h"
#include "content/test/test_render_frame_host.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "net/base/host_port_pair.h"
#include "net/base/ip_endpoint.h"
#include "net/dns/public/resolve_error_info.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/mojom/loader/referrer.mojom-forward.h"
#include "url/gurl.h"

struct FrameHostMsg_DidCommitProvisionalLoad_Params;

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

  static std::unique_ptr<NavigationSimulatorImpl> CreateBrowserInitiated(
      const GURL& original_url,
      WebContents* contents);

  static std::unique_ptr<NavigationSimulatorImpl> CreateHistoryNavigation(
      int offset,
      WebContents* web_contents);

  static std::unique_ptr<NavigationSimulatorImpl> CreateRendererInitiated(
      const GURL& original_url,
      RenderFrameHost* render_frame_host);

  static std::unique_ptr<NavigationSimulatorImpl> CreateFromPending(
      WebContents* contents);

  // Creates a NavigationSimulator for an already-started navigation happening
  // in |frame_tree_node|. Can be used to drive the navigation to completion.
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

  void SetInitiatorFrame(RenderFrameHost* initiator_frame_host) override;
  void SetTransition(ui::PageTransition transition) override;
  void SetHasUserGesture(bool has_user_gesture) override;
  void SetReloadType(ReloadType reload_type) override;
  void SetMethod(const std::string& method) override;
  void SetIsFormSubmission(bool is_form_submission) override;
  void SetWasInitiatedByLinkClick(bool was_initiated_by_link_click) override;
  void SetReferrer(blink::mojom::ReferrerPtr referrer) override;
  void SetSocketAddress(const net::IPEndPoint& remote_endpoint) override;
  void SetWasFetchedViaCache(bool was_fetched_via_cache) override;
  void SetIsSignedExchangeInnerResponse(
      bool is_signed_exchange_inner_response) override;
  void SetInterfaceProviderReceiver(
      mojo::PendingReceiver<service_manager::mojom::InterfaceProvider> receiver)
      override;
  void SetContentsMimeType(const std::string& contents_mime_type) override;
  void SetResponseHeaders(
      scoped_refptr<net::HttpResponseHeaders> response_headers) override;
  void SetAutoAdvance(bool auto_advance) override;
  void SetResolveErrorInfo(
      const net::ResolveErrorInfo& resolve_error_info) override;
  void SetSSLInfo(const net::SSLInfo& ssl_info) override;

  NavigationThrottle::ThrottleCheckResult GetLastThrottleCheckResult() override;
  NavigationRequest* GetNavigationHandle() override;
  content::GlobalRequestID GetGlobalRequestID() override;

  void SetKeepLoading(bool keep_loading) override;
  void StopLoading() override;
  void FailLoading(const GURL& url, int error_code) override;

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

  // Set DidCommit*Params history_list_was_cleared flag to |history_cleared|.
  void set_history_list_was_cleared(bool history_cleared);

  // Manually force the value of did_create_new_entry flag in DidCommit*Params
  // to |did_create_new_entry|.
  void set_did_create_new_entry(bool did_create_new_entry);

  // Manually force the value of should_replace_current_entry flag in
  // DidCommit*Params to |should_replace_current_entry|.
  void set_should_replace_current_entry(bool should_replace_current_entry) {
    should_replace_current_entry_ = should_replace_current_entry;
  }

  // Manually force the value of intended_as_new_entry flag in DidCommit*Params
  // to |intended_as_new_entry|.
  void set_intended_as_new_entry(bool intended_as_new_entry) {
    intended_as_new_entry_ = intended_as_new_entry;
  }

  void set_http_connection_info(net::HttpResponseInfo::ConnectionInfo info) {
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

  void set_page_state(const PageState& page_state) { page_state_ = page_state; }

  void set_origin(const url::Origin& origin) { origin_ = origin; }

  void set_impression(const Impression& impression) {
    impression_ = impression;
  }

  void SetIsPostWithId(int64_t post_id);

 private:
  NavigationSimulatorImpl(const GURL& original_url,
                          bool browser_initiated,
                          WebContentsImpl* web_contents,
                          TestRenderFrameHost* render_frame_host);

  // Adds a test navigation throttle to |request| which sanity checks various
  // callbacks have been properly called.
  void RegisterTestThrottle(NavigationRequest* request);

  // Initializes a NavigationSimulator from an existing NavigationRequest. This
  // should only be needed if a navigation was started without a valid
  // NavigationSimulator.
  void InitializeFromStartedRequest(NavigationRequest* request);

  // WebContentsObserver:
  void DidStartNavigation(NavigationHandle* navigation_handle) override;
  void DidRedirectNavigation(NavigationHandle* navigation_handle) override;
  void ReadyToCommitNavigation(NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(NavigationHandle* navigation_handle) override;

  void StartComplete();
  void RedirectComplete(int previous_num_will_redirect_request_called,
                        int previous_did_redirect_navigation_called);
  void ReadyToCommitComplete(bool ran_throttles);
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

  // This method will block waiting for throttle checks to complete if
  // |auto_advance_|. Otherwise will just set up state for checking the result
  // when the throttles end up finishing.
  void MaybeWaitForThrottleChecksComplete(base::OnceClosure complete_closure);

  // Sets |last_throttle_check_result_| and calls both the
  // |wait_closure_| and the |throttle_checks_complete_closure_|, if they are
  // set.
  bool OnThrottleChecksComplete(NavigationThrottle::ThrottleCheckResult result);

  // Helper method to set the OnThrottleChecksComplete callback on the
  // NavigationRequest.
  void PrepareCompleteCallbackOnRequest();

  // Check if the navigation corresponds to a same-document navigation.
  // Only use on renderer-initiated navigations.
  bool CheckIfSameDocument();

  // Infers from internal parameters whether the navigation created a new
  // entry.
  bool DidCreateNewEntry();

  // Set the navigation to be done towards the specified navigation controller
  // offset. Typically -1 for back navigations or 1 for forward navigations.
  void SetSessionHistoryOffset(int offset);

  // Build DidCommitProvisionalLoadParams to commit the ongoing navigation,
  // based on internal NavigationSimulator state and given parameters.
  std::unique_ptr<FrameHostMsg_DidCommitProvisionalLoad_Params>
  BuildDidCommitProvisionalLoadParams(bool same_document,
                                      bool failed_navigation);

  // Simulate the UnloadACK in the old RenderFrameHost if it was unloaded at the
  // commit time.
  void SimulateUnloadCompletionCallbackForPreviousFrameIfNeeded(
      RenderFrameHostImpl* previous_frame);

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
  WebContentsImpl* web_contents_;

  // The renderer associated with this navigation.
  // Note: this can initially be null for browser-initiated navigations.
  TestRenderFrameHost* render_frame_host_;

  FrameTreeNode* frame_tree_node_;

  // The NavigationRequest associated with this navigation.
  NavigationRequest* request_;

  // Note: additional parameters to modify the navigation should be properly
  // initialized (if needed) in InitializeFromStartedRequest.
  GURL original_url_;
  GURL navigation_url_;
  net::IPEndPoint remote_endpoint_;
  bool was_fetched_via_cache_ = false;
  bool is_signed_exchange_inner_response_ = false;
  std::string initial_method_;
  bool is_form_submission_ = false;
  bool was_initiated_by_link_click_ = false;
  bool browser_initiated_;
  bool same_document_ = false;
  TestRenderFrameHost::LoadingScenario loading_scenario_ =
      TestRenderFrameHost::LoadingScenario::kOther;
  blink::mojom::ReferrerPtr referrer_;
  RenderFrameHost* initiator_frame_host_ = nullptr;
  ui::PageTransition transition_;
  ReloadType reload_type_ = ReloadType::NONE;
  int session_history_offset_ = 0;
  bool has_user_gesture_ = true;
  mojo::PendingReceiver<service_manager::mojom::InterfaceProvider>
      interface_provider_receiver_;
  mojo::PendingReceiver<blink::mojom::BrowserInterfaceBroker>
      browser_interface_broker_receiver_;
  std::string contents_mime_type_;
  scoped_refptr<net::HttpResponseHeaders> response_headers_;
  network::mojom::CSPDisposition should_check_main_world_csp_ =
      network::mojom::CSPDisposition::CHECK;
  net::HttpResponseInfo::ConnectionInfo http_connection_info_ =
      net::HttpResponseInfo::CONNECTION_INFO_UNKNOWN;
  net::ResolveErrorInfo resolve_error_info_ = net::ResolveErrorInfo(net::OK);
  base::Optional<net::SSLInfo> ssl_info_;
  base::Optional<PageState> page_state_;
  base::Optional<url::Origin> origin_;
  base::Optional<Impression> impression_;
  int64_t post_id_ = -1;

  bool auto_advance_ = true;
  bool drop_unload_ack_ = false;
  bool block_invoking_before_unload_completed_callback_ = false;
  bool keep_loading_ = false;

  // Generic params structure used for fully customized browser initiated
  // navigation requests. Only valid if explicitely provided.
  NavigationController::LoadURLParams* load_url_params_;

  bool history_list_was_cleared_ = false;
  bool should_replace_current_entry_ = false;
  base::Optional<bool> did_create_new_entry_;
  base::Optional<bool> intended_as_new_entry_;
  bool was_aborted_ = false;

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
  base::Optional<NavigationThrottle::ThrottleCheckResult>
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

  // This member simply ensures that we do not disconnect
  // the NavigationClient interface, as it would be interpreted as a
  // cancellation coming from the renderer process side. This member interface
  // will never be bound.
  // Only used when PerNavigationMojoInterface is enabled.
  mojo::PendingAssociatedReceiver<mojom::NavigationClient>
      navigation_client_receiver_;

  base::WeakPtrFactory<NavigationSimulatorImpl> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_NAVIGATION_SIMULATOR_H_
