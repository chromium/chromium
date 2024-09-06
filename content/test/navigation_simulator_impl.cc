// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/navigation_simulator_impl.h"

#include <utility>

#include "base/debug/stack_trace.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "content/browser/renderer_host/back_forward_cache_metrics.h"
#include "content/browser/renderer_host/debug_urls.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/navigation_entry_impl.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/content_navigation_policy.h"
#include "content/common/features.h"
#include "content/common/navigation_params_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_utils.h"
#include "content/test/test_navigation_url_loader.h"
#include "content/test/test_render_frame_host.h"
#include "content/test/test_web_contents.h"
#include "ipc/ipc_message.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/base/load_flags.h"
#include "net/storage_access_api/status.h"
#include "net/url_request/redirect_info.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/blink/public/common/chrome_debug_urls.h"
#include "third_party/blink/public/common/navigation/navigation_params.h"
#include "third_party/blink/public/mojom/loader/mixed_content.mojom.h"

namespace content {

namespace {

class NavigationThrottleCallbackRunner : public NavigationThrottle {
 public:
  NavigationThrottleCallbackRunner(
      NavigationHandle* handle,
      base::OnceClosure on_will_start_request,
      const base::RepeatingClosure& on_will_redirect_request,
      base::OnceClosure on_will_fail_request,
      base::OnceClosure on_will_process_response)
      : NavigationThrottle(handle),
        on_will_start_request_(std::move(on_will_start_request)),
        on_will_redirect_request_(on_will_redirect_request),
        on_will_fail_request_(std::move(on_will_fail_request)),
        on_will_process_response_(std::move(on_will_process_response)) {}

  NavigationThrottle::ThrottleCheckResult WillStartRequest() override {
    std::move(on_will_start_request_).Run();
    return NavigationThrottle::PROCEED;
  }

  NavigationThrottle::ThrottleCheckResult WillRedirectRequest() override {
    on_will_redirect_request_.Run();
    return NavigationThrottle::PROCEED;
  }

  NavigationThrottle::ThrottleCheckResult WillFailRequest() override {
    std::move(on_will_fail_request_).Run();
    return NavigationThrottle::PROCEED;
  }

  NavigationThrottle::ThrottleCheckResult WillProcessResponse() override {
    std::move(on_will_process_response_).Run();
    return NavigationThrottle::PROCEED;
  }

  const char* GetNameForLogging() override {
    return "NavigationThrottleCallbackRunner";
  }

 private:
  base::OnceClosure on_will_start_request_;
  base::RepeatingClosure on_will_redirect_request_;
  base::OnceClosure on_will_fail_request_;
  base::OnceClosure on_will_process_response_;
};

int64_t g_unique_identifier = 0;

FrameTreeNode* GetFrameTreeNodeForPendingEntry(
    NavigationControllerImpl& controller) {
  NavigationEntryImpl* pending_entry = controller.GetPendingEntry();
  FrameTreeNodeId frame_tree_node_id = pending_entry->frame_tree_node_id();
  FrameTree& frame_tree = controller.frame_tree();
  if (frame_tree_node_id.is_null()) {
    return frame_tree.root();
  }
  return frame_tree.FindByID(frame_tree_node_id);
}

}  // namespace

// static
RenderFrameHost* NavigationSimulator::NavigateAndCommitFromBrowser(
    WebContents* web_contents,
    const GURL& url) {
  auto simulator =
      NavigationSimulatorImpl::CreateBrowserInitiated(url, web_contents);
  simulator->Commit();
  return simulator->GetFinalRenderFrameHost();
}

// static
RenderFrameHost* NavigationSimulator::Reload(WebContents* web_contents) {
  NavigationEntry* entry =
      web_contents->GetController().GetLastCommittedEntry();
  CHECK(entry);
  auto simulator = NavigationSimulatorImpl::CreateBrowserInitiated(
      entry->GetURL(), web_contents);
  simulator->SetReloadType(ReloadType::NORMAL);
  simulator->Commit();
  return simulator->GetFinalRenderFrameHost();
}

// static
RenderFrameHost* NavigationSimulator::GoBack(WebContents* web_contents) {
  return GoToOffset(web_contents, -1);
}

// static
RenderFrameHost* NavigationSimulator::GoForward(WebContents* web_contents) {
  return GoToOffset(web_contents, 1);
}

// static
RenderFrameHost* NavigationSimulator::GoToOffset(WebContents* web_contents,
                                                 int offset) {
  auto simulator = NavigationSimulatorImpl::CreateHistoryNavigation(
      offset, web_contents, false /* is_renderer_initiated */);
  simulator->Commit();
  return simulator->GetFinalRenderFrameHost();
}

// static
RenderFrameHost* NavigationSimulator::NavigateAndCommitFromDocument(
    const GURL& original_url,
    RenderFrameHost* render_frame_host) {
  auto simulator = NavigationSimulator::CreateRendererInitiated(
      original_url, render_frame_host);
  simulator->Commit();
  return simulator->GetFinalRenderFrameHost();
}

// static
RenderFrameHost* NavigationSimulator::NavigateAndFailFromBrowser(
    WebContents* web_contents,
    const GURL& url,
    int net_error_code) {
  auto simulator =
      NavigationSimulator::CreateBrowserInitiated(url, web_contents);
  simulator->Fail(net_error_code);
  if (net_error_code == net::ERR_ABORTED)
    return nullptr;
  simulator->CommitErrorPage();
  return simulator->GetFinalRenderFrameHost();
}

// static
RenderFrameHost* NavigationSimulator::ReloadAndFail(WebContents* web_contents,
                                                    int net_error_code) {
  NavigationEntry* entry =
      web_contents->GetController().GetLastCommittedEntry();
  CHECK(entry);
  auto simulator = NavigationSimulator::CreateBrowserInitiated(entry->GetURL(),
                                                               web_contents);
  simulator->SetReloadType(ReloadType::NORMAL);
  simulator->Fail(net_error_code);
  if (net_error_code == net::ERR_ABORTED)
    return nullptr;
  simulator->CommitErrorPage();
  return simulator->GetFinalRenderFrameHost();
}

// static
RenderFrameHost* NavigationSimulator::GoBackAndFail(WebContents* web_contents,
                                                    int net_error_code) {
  return GoToOffsetAndFail(web_contents, -1, net_error_code);
}

// static
RenderFrameHost* NavigationSimulator::GoToOffsetAndFail(
    WebContents* web_contents,
    int offset,
    int net_error_code) {
  auto simulator = NavigationSimulator::CreateHistoryNavigation(
      offset, web_contents, false /* is_renderer_initiated */);
  simulator->Fail(net_error_code);
  if (net_error_code == net::ERR_ABORTED)
    return nullptr;
  simulator->CommitErrorPage();
  return simulator->GetFinalRenderFrameHost();
}

// static
RenderFrameHost* NavigationSimulator::NavigateAndFailFromDocument(
    const GURL& original_url,
    int net_error_code,
    RenderFrameHost* render_frame_host) {
  auto simulator = NavigationSimulator::CreateRendererInitiated(
      original_url, render_frame_host);
  simulator->Fail(net_error_code);
  if (net_error_code == net::ERR_ABORTED)
    return nullptr;
  simulator->CommitErrorPage();
  return simulator->GetFinalRenderFrameHost();
}

// static
std::unique_ptr<NavigationSimulator>
NavigationSimulator::CreateBrowserInitiated(const GURL& original_url,
                                            WebContents* web_contents) {
  return NavigationSimulatorImpl::CreateBrowserInitiated(original_url,
                                                         web_contents);
}

// static
std::unique_ptr<NavigationSimulatorImpl>
NavigationSimulatorImpl::CreateBrowserInitiated(const GURL& original_url,
                                                WebContents* web_contents) {
  return std::unique_ptr<NavigationSimulatorImpl>(new NavigationSimulatorImpl(
      original_url, true /* browser_initiated */,
      static_cast<WebContentsImpl*>(web_contents), nullptr));
}

// static
std::unique_ptr<NavigationSimulator>
NavigationSimulator::CreateHistoryNavigation(int offset,
                                             WebContents* web_contents,
                                             bool is_renderer_initiated) {
  return NavigationSimulatorImpl::CreateHistoryNavigation(
      offset, web_contents, is_renderer_initiated);
}

// static
std::unique_ptr<NavigationSimulatorImpl>
NavigationSimulatorImpl::CreateHistoryNavigation(int offset,
                                                 WebContents* web_contents,
                                                 bool is_renderer_initiated) {
  std::unique_ptr<NavigationSimulatorImpl> simulator = nullptr;
  if (is_renderer_initiated) {
    simulator = NavigationSimulatorImpl::CreateRendererInitiated(
        GURL(), web_contents->GetPrimaryMainFrame());
  } else {
    simulator =
        NavigationSimulatorImpl::CreateBrowserInitiated(GURL(), web_contents);
  }
  simulator->SetSessionHistoryOffset(offset);
  return simulator;
}

// static
std::unique_ptr<NavigationSimulator>
NavigationSimulator::CreateRendererInitiated(
    const GURL& original_url,
    RenderFrameHost* render_frame_host) {
  return NavigationSimulatorImpl::CreateRendererInitiated(original_url,
                                                          render_frame_host);
}

// static
std::unique_ptr<NavigationSimulatorImpl>
NavigationSimulatorImpl::CreateRendererInitiated(
    const GURL& original_url,
    RenderFrameHost* render_frame_host) {
  std::unique_ptr<NavigationSimulatorImpl> sim =
      std::unique_ptr<NavigationSimulatorImpl>(new NavigationSimulatorImpl(
          original_url, false /* browser_initiated */,
          static_cast<WebContentsImpl*>(
              WebContents::FromRenderFrameHost(render_frame_host)),
          static_cast<TestRenderFrameHost*>(render_frame_host)));

  sim->SetInitiatorFrame(render_frame_host);

  if (render_frame_host->IsNestedWithinFencedFrame()) {
    sim->set_supports_loading_mode_header("fenced-frame");
    sim->SetTransition(ui::PAGE_TRANSITION_AUTO_SUBFRAME);
    // Set should_replace_current_entry to true, to pass the DidCommitParams
    // check that expects the initial NavigationEntry to always be replaced.
    sim->set_should_replace_current_entry(true);
  }
  return sim;
}

// static
std::unique_ptr<NavigationSimulator> NavigationSimulator::CreateFromPending(
    NavigationController& controller) {
  return NavigationSimulatorImpl::CreateFromPending(controller);
}

// static
std::unique_ptr<NavigationSimulatorImpl>
NavigationSimulatorImpl::CreateFromPending(NavigationController& controller) {
  FrameTreeNode* frame_tree_node = GetFrameTreeNodeForPendingEntry(
      static_cast<NavigationControllerImpl&>(controller));
  return NavigationSimulatorImpl::CreateFromPendingInFrame(frame_tree_node);
}

// static
std::unique_ptr<NavigationSimulatorImpl>
NavigationSimulatorImpl::CreateFromPendingInFrame(
    FrameTreeNode* frame_tree_node) {
  CHECK(frame_tree_node);
  TestRenderFrameHost* test_frame_host =
      static_cast<TestRenderFrameHost*>(frame_tree_node->current_frame_host());
  CHECK(test_frame_host);
  NavigationRequest* request = frame_tree_node->navigation_request();
  // It is possible to not have a NavigationRequest in the frame tree node if
  // it did not go to the network (such as about:blank). In that case it is
  // already in the RenderFrameHost.
  if (!request)
    request = test_frame_host->navigation_requests().begin()->second.get();
  CHECK(request);

  // Simulate the BeforeUnload completion callback if needed.
  if (request->state() == NavigationRequest::WAITING_FOR_RENDERER_RESPONSE)
    test_frame_host->SimulateBeforeUnloadCompleted(true /* proceed */);

  auto simulator = base::WrapUnique(new NavigationSimulatorImpl(
      GURL(), request->browser_initiated(),
      WebContentsImpl::FromFrameTreeNode(frame_tree_node), test_frame_host));
  simulator->frame_tree_node_ = frame_tree_node;
  simulator->InitializeFromStartedRequest(request);
  return simulator;
}

NavigationSimulatorImpl::NavigationSimulatorImpl(
    const GURL& original_url,
    bool browser_initiated,
    WebContentsImpl* web_contents,
    TestRenderFrameHost* render_frame_host)
    : WebContentsObserver(web_contents),
      web_contents_(web_contents),
      render_frame_host_(render_frame_host),
      frame_tree_node_(
          render_frame_host
              ? render_frame_host->frame_tree_node()
              : web_contents->GetPrimaryMainFrame()->frame_tree_node()),
      request_(nullptr),
      navigation_url_(original_url),
      initial_method_("GET"),
      browser_initiated_(browser_initiated),
      referrer_(blink::mojom::Referrer::New()),
      transition_(browser_initiated ? ui::PAGE_TRANSITION_TYPED
                                    : ui::PAGE_TRANSITION_LINK),
      contents_mime_type_("text/html"),
      load_url_params_(nullptr) {
  net::IPAddress address;
  CHECK(address.AssignFromIPLiteral("2001:db8::1"));
  remote_endpoint_ = net::IPEndPoint(address, 80);

  // For renderer-initiated navigation, the RenderFrame must be initialized. Do
  // it if it hasn't happened yet.
  if (!browser_initiated)
    render_frame_host->InitializeRenderFrameIfNeeded();

  if (render_frame_host && render_frame_host->GetParent()) {
    transition_ = ui::PAGE_TRANSITION_MANUAL_SUBFRAME;
  }

  browser_interface_broker_receiver_ =
      mojo::PendingRemote<blink::mojom::BrowserInterfaceBroker>()
          .InitWithNewPipeAndPassReceiver();
}

NavigationSimulatorImpl::~NavigationSimulatorImpl() {}

void NavigationSimulatorImpl::InitializeFromStartedRequest(
    NavigationRequest* request) {
  CHECK(request);
  request_ = request;
  CHECK(request_->IsNavigationStarted());
  CHECK_EQ(web_contents_, request_->GetDelegate());
  CHECK(render_frame_host_);
  CHECK_EQ(frame_tree_node_, request_->frame_tree_node());
  state_ = STARTED;
  navigation_url_ = request_->GetURL();
  // |remote_endpoint_| cannot be inferred from the request.
  // |initial_method_| cannot be set after the request has started.
  browser_initiated_ = request_->browser_initiated();
  // |same_document_| should always be false here.
  referrer_ = request_->common_params().referrer.Clone();
  transition_ = request_->GetPageTransition();
  if (!request_->IsInMainFrame()) {
    // Subframe transitions always start as MANUAL_SUBFRAME, and the final
    // value will be calculated at DidCommit time (see
    // BuildDidCommitProvisionalLoadParams()).
    transition_ = ui::PAGE_TRANSITION_MANUAL_SUBFRAME;
  }
  // |reload_type_| cannot be set after the request has started.
  // |session_history_offset_| cannot be set after the request has started.
  has_user_gesture_ = request_->HasUserGesture();
  // |contents_mime_type_| cannot be inferred from the request.

  skip_service_worker_ = request_->begin_params().skip_service_worker;

  if (!browser_initiated_ && request_->GetInitiatorFrameToken().has_value()) {
    SetInitiatorFrame(RenderFrameHostImpl::FromFrameToken(
        request_->GetInitiatorProcessId(),
        request_->GetInitiatorFrameToken().value()));
  }

  // Add a throttle to count NavigationThrottle calls count. This should be
  // skipped if DidStartNavigation() has been called as the same setup is done
  // there.
  if (num_did_start_navigation_called_ == 0) {
    num_did_start_navigation_called_++;
    RegisterTestThrottle();
    PrepareCompleteCallbackOnRequest();
  }
}

void NavigationSimulatorImpl::RegisterTestThrottle() {
  DCHECK(request_);

  // Page activating navigations don't run throttles so we don't need to
  // register it in that case.
  if (request_->IsPageActivation())
    return;

  request_->RegisterThrottleForTesting(
      std::make_unique<NavigationThrottleCallbackRunner>(
          request_,
          base::BindOnce(&NavigationSimulatorImpl::OnWillStartRequest,
                         weak_factory_.GetWeakPtr()),
          base::BindRepeating(&NavigationSimulatorImpl::OnWillRedirectRequest,
                              weak_factory_.GetWeakPtr()),
          base::BindOnce(&NavigationSimulatorImpl::OnWillFailRequest,
                         weak_factory_.GetWeakPtr()),
          base::BindOnce(&NavigationSimulatorImpl::OnWillProcessResponse,
                         weak_factory_.GetWeakPtr())));
}

void NavigationSimulatorImpl::Start() {
  CHECK(state_ == INITIALIZATION || state_ == WAITING_BEFORE_UNLOAD)
      << "NavigationSimulatorImpl::Start should only be called once.";

  if (browser_initiated_) {
    if (!SimulateBrowserInitiatedStart())
      return;
  } else {
    if (!SimulateRendererInitiatedStart())
      return;
  }
  state_ = STARTED;

  CHECK(request_);
  if (blink::IsRendererDebugURL(navigation_url_))
    return;
  if (session_history_offset_)
    return;

  if (!NeedsThrottleChecks())
    return;

  MaybeWaitForThrottleChecksComplete(base::BindOnce(
      &NavigationSimulatorImpl::StartComplete, weak_factory_.GetWeakPtr()));
}

void NavigationSimulatorImpl::StartComplete() {
  CHECK_EQ(1, num_did_start_navigation_called_);
  if (GetLastThrottleCheckResult().action() == NavigationThrottle::PROCEED) {
    CHECK_EQ(1, num_will_start_request_called_);
  } else {
    state_ = FAILED;
  }
}

void NavigationSimulatorImpl::Redirect(const GURL& new_url) {
  CHECK_LE(state_, STARTED) << "NavigationSimulatorImpl::Redirect should be "
                               "called before Fail or Commit";
  CHECK_EQ(0, num_did_finish_navigation_called_)
      << "NavigationSimulatorImpl::Redirect cannot be called after the "
         "navigation has finished";

  if (state_ < STARTED) {
    Start();
    if (state_ == FAILED)
      return;
  }

  navigation_url_ = new_url;
  int previous_num_will_redirect_request_called =
      num_will_redirect_request_called_;
  int previous_did_redirect_navigation_called =
      num_did_redirect_navigation_called_;

  PrepareCompleteCallbackOnRequest();
  NavigationRequest* request = frame_tree_node_->navigation_request();
  CHECK(request) << "Trying to redirect a navigation that does not go to the "
                    "network stack.";

  TestNavigationURLLoader* url_loader =
      static_cast<TestNavigationURLLoader*>(request->loader_for_testing());
  CHECK(url_loader);

  net::RedirectInfo redirect_info;
  redirect_info.status_code = 302;
  redirect_info.new_method = "GET";
  redirect_info.new_url = new_url;
  redirect_info.new_site_for_cookies = net::SiteForCookies::FromUrl(new_url);
  redirect_info.new_referrer = referrer_->url.spec();
  redirect_info.new_referrer_policy =
      Referrer::ReferrerPolicyForUrlRequest(referrer_->policy);
  auto response = network::mojom::URLResponseHead::New();
  response->connection_info = http_connection_info_;
  response->ssl_info = ssl_info_;

  response->headers =
      base::MakeRefCounted<net::HttpResponseHeaders>(std::string());
  if (redirect_headers_) {
    response->headers = redirect_headers_;
    redirect_headers_ = nullptr;
  }

  if (response_postprocess_hook_) {
    response_postprocess_hook_.Run(*response);
  }
  url_loader->CallOnRequestRedirected(redirect_info, std::move(response));

  MaybeWaitForThrottleChecksComplete(base::BindOnce(
      &NavigationSimulatorImpl::RedirectComplete, weak_factory_.GetWeakPtr(),
      previous_num_will_redirect_request_called,
      previous_did_redirect_navigation_called));
}

void NavigationSimulatorImpl::RedirectComplete(
    int previous_num_will_redirect_request_called,
    int previous_did_redirect_navigation_called) {
  if (GetLastThrottleCheckResult().action() == NavigationThrottle::PROCEED) {
    CHECK_EQ(previous_num_will_redirect_request_called + 1,
             num_will_redirect_request_called_);
    CHECK_EQ(previous_did_redirect_navigation_called + 1,
             num_did_redirect_navigation_called_);
  } else {
    state_ = FAILED;
  }
}

void NavigationSimulatorImpl::ReadyToCommit() {
  CHECK_LE(state_, STARTED)
      << "NavigationSimulatorImpl::ReadyToCommit can only "
         "be called once, and cannot be called after "
         "NavigationSimulatorImpl::Fail";
  CHECK_EQ(0, num_did_finish_navigation_called_)
      << "NavigationSimulatorImpl::ReadyToCommit cannot be called after the "
         "navigation has finished";

  if (state_ < STARTED) {
    if (block_invoking_before_unload_completed_callback_ &&
        state_ == WAITING_BEFORE_UNLOAD) {
      // The user should have simulated the BeforeUnloadCompleted by themselves.
      // Finish the initialization and skip the Start simulation.
      InitializeFromStartedRequest(request_);
    } else {
      Start();

      // If a request is blocked, we may not have a request. In this case, mark
      // as failed and exit.
      if (!request_) {
        state_ = FAILED;
        return;
      }

      // For prerendered page activation, CommitDeferringConditions
      // asynchronously run before the navigation starts. Wait here until all
      // the conditions run.
      if (request_->is_running_potential_prerender_activation_checks()) {
        base::RunLoop run_loop;
        did_start_navigation_closure_ = run_loop.QuitClosure();
        run_loop.Run();
        DCHECK(was_prerendered_page_activation_.value());
      }

      // The navigation has failed or already finished for prerendered page
      // activation.
      if (state_ == FAILED || state_ == FINISHED)
        return;
    }
  }

  CHECK_EQ(nullptr, redirect_headers_)
      << "Redirect headers were set but never used in a Redirect call";

  if (!response_headers_) {
    response_headers_ =
        base::MakeRefCounted<net::HttpResponseHeaders>(std::string());
  }

  response_headers_->SetHeader("Content-Type", contents_mime_type_);
  if (!supports_loading_mode_header_.empty())
    response_headers_->SetHeader("Supports-Loading-Mode",
                                 supports_loading_mode_header_);
  PrepareCompleteCallbackOnRequest();
  request_->set_ready_to_commit_callback_for_testing(
      base::BindOnce(&NavigationSimulatorImpl::ReadyToCommitComplete,
                     weak_factory_.GetWeakPtr()));

  if (frame_tree_node_->navigation_request()) {
    NavigationRequest* request = frame_tree_node_->navigation_request();
    if (early_hints_preload_link_header_received_) {
      TestNavigationURLLoader* url_loader =
          static_cast<TestNavigationURLLoader*>(request->loader_for_testing());
      CHECK(url_loader);
      url_loader->SimulateEarlyHintsPreloadLinkHeaderReceived();
    }

    auto response = network::mojom::URLResponseHead::New();
    response->remote_endpoint = remote_endpoint_;
    response->was_fetched_via_cache = was_fetched_via_cache_;
    response->is_signed_exchange_inner_response =
        is_signed_exchange_inner_response_;
    response->connection_info = http_connection_info_;
    response->ssl_info = ssl_info_;
    response->headers = response_headers_;
    response->dns_aliases = response_dns_aliases_;
    if (response_postprocess_hook_) {
      response_postprocess_hook_.Run(*response);
    }
    static_cast<TestRenderFrameHost*>(frame_tree_node_->current_frame_host())
        ->PrepareForCommitDeprecatedForNavigationSimulator(
            std::move(response), std::move(response_body_));
  }

  // Synchronous failure can cause the navigation to finish here.
  if (!request_) {
    state_ = FAILED;
    return;
  }

  auto complete_closure =
      base::BindOnce(&NavigationSimulatorImpl::WillProcessResponseComplete,
                     weak_factory_.GetWeakPtr());
  if (NeedsPreCommitChecks()) {
    MaybeWaitForThrottleChecksComplete(std::move(complete_closure));
    MaybeWaitForReadyToCommitCheckComplete();
    if (state_ == READY_TO_COMMIT) {
      // `NavigationRequest::OnWillProcessResponseProcessed()` invokes the
      // completion callback but the commit may be deferred before dispatching
      // `ReadyToCommitNavigation()` to observers so we have to wait on that
      // too. Once these checks are complete, ensure that
      // `ReadyToCommitNavigation()` has been called as expected.
      CHECK_EQ(1, num_ready_to_commit_called_);
    }
    return;
  }

  std::move(complete_closure).Run();
  ReadyToCommitComplete();
}

void NavigationSimulatorImpl::WillProcessResponseComplete() {
  if (NeedsThrottleChecks()) {
    if (GetLastThrottleCheckResult().action() != NavigationThrottle::PROCEED) {
      state_ = FAILED;
      return;
    }
    CHECK_EQ(1, num_will_process_response_called_);
  }

  request_id_ = request_->GetGlobalRequestID();

  // Update the RenderFrameHost now that we know which RenderFrameHost will
  // commit the navigation.
  render_frame_host_ =
      static_cast<TestRenderFrameHost*>(request_->GetRenderFrameHost());
}

void NavigationSimulatorImpl::ReadyToCommitComplete() {
  // If the commit was deferred, this completes from a RunLoop wait so exit it
  // now.
  if (wait_closure_)
    std::move(wait_closure_).Run();
  state_ = READY_TO_COMMIT;
}

void NavigationSimulatorImpl::Commit() {
  CHECK_LE(state_, READY_TO_COMMIT)
      << "NavigationSimulatorImpl::Commit can only "
         "be called once, and cannot be called "
         "after NavigationSimulatorImpl::Fail";
  CHECK_EQ(0, num_did_finish_navigation_called_)
      << "NavigationSimulatorImpl::Commit cannot be called after the "
         "navigation "
         "has finished";

  if (state_ < READY_TO_COMMIT) {
    ReadyToCommit();
    if (state_ == FAILED || state_ == FINISHED)
      return;
  }

  // Keep a pointer to the current RenderFrameHost that may be pending deletion
  // after commit.
  base::WeakPtr<RenderFrameHostImpl> previous_rfh =
      render_frame_host_->frame_tree_node()->current_frame_host()->GetWeakPtr();

  // RenderDocument: Do not dispatch UnloadACK if the navigation was committed
  // in the same SiteInstance. This has already been dispatched during the
  // navigation in the renderer process.
  if (previous_rfh->GetSiteInstance() == render_frame_host_->GetSiteInstance())
    drop_unload_ack_ = true;

  // If the frame is not alive we do not displatch Unload ACK. CommitPending()
  // may be called immediately and delete the old RenderFrameHost, so we need to
  // record that now while we can still access the object.
  if (!previous_rfh->IsRenderFrameLive())
    drop_unload_ack_ = true;

  if (same_document_) {
    browser_interface_broker_receiver_.reset();
  }

  // The initial `navigation_url_` may have been mapped to a new URL at this
  // point. Overwrite it here with the desired value to correctly mock the
  // DidCommitProvisionalLoadParams.
  navigation_url_ = request_->GetURL();
  if (navigation_url_.is_empty()) {
    // Blink treats empty URLs as about:blank. Simulate that in the commit IPC
    // so that the RenderFrameHost does not reject the commit.
    navigation_url_ = GURL(url::kAboutBlankURL);
  }

  auto params = BuildDidCommitProvisionalLoadParams(
      same_document_ /* same_document */, false /* failed_navigation */,
      render_frame_host_->last_http_status_code());
  render_frame_host_->SimulateCommitProcessed(
      request_, std::move(params),
      std::move(browser_interface_broker_receiver_), same_document_);

  if (previous_rfh) {
    SimulateUnloadCompletionCallbackForPreviousFrameIfNeeded(
        previous_rfh.get());
  }

  loading_scenario_ =
      TestRenderFrameHost::LoadingScenario::NewDocumentNavigation;
  state_ = FINISHED;
  if (!keep_loading_)
    StopLoading();

  if (!blink::IsRendererDebugURL(navigation_url_))
    CHECK_EQ(1, num_did_finish_navigation_called_);
}

void NavigationSimulatorImpl::AbortCommit() {
  CHECK_LE(state_, FAILED)
      << "NavigationSimulatorImpl::AbortCommit cannot be called after "
         "NavigationSimulatorImpl::Commit or  "
         "NavigationSimulatorImpl::CommitErrorPage.";
  if (state_ < READY_TO_COMMIT) {
    ReadyToCommit();
    if (state_ == FINISHED)
      return;
  }

  CHECK(render_frame_host_)
      << "NavigationSimulatorImpl::AbortCommit can only be "
         "called for navigations that commit.";
  render_frame_host_->AbortCommit(request_);

  state_ = FINISHED;
  StopLoading();

  CHECK_EQ(1, num_did_finish_navigation_called_);
}

void NavigationSimulatorImpl::AbortFromRenderer() {
  CHECK(!browser_initiated_)
      << "NavigationSimulatorImpl::AbortFromRenderer cannot be called for "
         "browser-initiated navigation.";
  CHECK_LE(state_, FAILED)
      << "NavigationSimulatorImpl::AbortFromRenderer cannot be called after "
         "NavigationSimulatorImpl::Commit or  "
         "NavigationSimulatorImpl::CommitErrorPage.";

  if (state_ < READY_TO_COMMIT) {
    was_aborted_prior_to_ready_to_commit_ = true;
    request_->RendererRequestedNavigationCancellationForTesting();
    state_ = FINISHED;
  } else {
    // Calling RendererRequestedNavigationCancellationForTesting() after the
    // READY_TO_COMMIT stage will get ignored, so call AbortCommit() instead.
    AbortCommit();
  }

  CHECK_EQ(1, num_did_finish_navigation_called_);
}

void NavigationSimulatorImpl::Fail(int error_code) {
  CHECK_LE(state_, STARTED) << "NavigationSimulatorImpl::Fail can only be "
                               "called once, and cannot be called after "
                               "NavigationSimulatorImpl::ReadyToCommit";
  CHECK_EQ(0, num_did_finish_navigation_called_)
      << "NavigationSimulatorImpl::Fail cannot be called after the "
         "navigation has finished";
  CHECK(!blink::IsRendererDebugURL(navigation_url_));

  if (state_ == INITIALIZATION)
    Start();

  state_ = FAILED;

  PrepareCompleteCallbackOnRequest();
  CHECK(request_);
  TestNavigationURLLoader* url_loader =
      static_cast<TestNavigationURLLoader*>(request_->loader_for_testing());
  CHECK(url_loader);

  network::URLLoaderCompletionStatus status(error_code);
  status.resolve_error_info = resolve_error_info_;
  status.ssl_info = ssl_info_;
  url_loader->SimulateErrorWithStatus(status);

  auto complete_closure =
      base::BindOnce(&NavigationSimulatorImpl::FailComplete,
                     weak_factory_.GetWeakPtr(), error_code);
  if (error_code != net::ERR_ABORTED) {
    MaybeWaitForThrottleChecksComplete(std::move(complete_closure));
    return;
  }
  std::move(complete_closure).Run();
}

void NavigationSimulatorImpl::FailComplete(int error_code) {
  bool should_result_in_error_page = error_code != net::ERR_ABORTED;
  if (error_code != net::ERR_ABORTED) {
    NavigationThrottle::ThrottleCheckResult result =
        GetLastThrottleCheckResult();
    if (result.action() == NavigationThrottle::CANCEL ||
        result.action() == NavigationThrottle::CANCEL_AND_IGNORE) {
      should_result_in_error_page = false;
    }
  }

  if (should_result_in_error_page) {
    // TODO(clamy): Check that ReadyToCommit has been called once, once the test
    // architecture of NavigationRequest vs NavigationHandle has been clarified.
    // Currently, when auto-advance is off, this function will be called before
    // NavigationRequest::CommitErrorPage which is the one that triggers the
    // call to observers.
    CHECK_EQ(0, num_did_finish_navigation_called_);
    // Update the RenderFrameHost now that we know which RenderFrameHost will
    // commit the error page.
    render_frame_host_ =
        static_cast<TestRenderFrameHost*>(request_->GetRenderFrameHost());
  }
}

void NavigationSimulatorImpl::CommitErrorPage() {
  CHECK_EQ(FAILED, state_)
      << "NavigationSimulatorImpl::CommitErrorPage can only be "
         "called once, and should be called after Fail "
         "has been called";
  CHECK_EQ(0, num_did_finish_navigation_called_)
      << "NavigationSimulatorImpl::CommitErrorPage cannot be called after the "
         "navigation has finished";

  // Keep a pointer to the current RenderFrameHost that may be pending deletion
  // after commit.
  // RenderDocument: The |previous_rfh| might also be immediately deleted after
  // commit, because it has already run its unload handler.
  RenderFrameHostImpl* previous_rfh =
      render_frame_host_->frame_tree_node()->current_frame_host();

  // RenderDocument: Do not dispatch UnloadACK if the navigation was committed
  // in the same SiteInstance. This has already been dispatched during the
  // navigation in the renderer process.
  if (previous_rfh->GetSiteInstance() == render_frame_host_->GetSiteInstance())
    drop_unload_ack_ = true;

  // If the frame is not alive we do not displatch Unload ACK. CommitPending()
  // may be called immediately and delete the old RenderFrameHost, so we need to
  // record that now while we can still access the object.
  if (!previous_rfh->IsRenderFrameLive())
    drop_unload_ack_ = true;

  auto params = BuildDidCommitProvisionalLoadParams(
      false /* same_document */, true /* failed_navigation */,
      render_frame_host_->last_http_status_code());
  render_frame_host_->SimulateCommitProcessed(
      request_, std::move(params),
      std::move(browser_interface_broker_receiver_), false /* same_document */);

  SimulateUnloadCompletionCallbackForPreviousFrameIfNeeded(previous_rfh);

  state_ = FINISHED;
  if (!keep_loading_)
    StopLoading();

  CHECK_EQ(1, num_did_finish_navigation_called_);
}

void NavigationSimulatorImpl::CommitSameDocument() {
  if (!browser_initiated_) {
    CHECK_EQ(INITIALIZATION, state_)
        << "NavigationSimulatorImpl::CommitSameDocument should be the only "
           "navigation event function called on the NavigationSimulatorImpl";
  } else {
    // This function is intended for same document navigations initiating from
    // the renderer. For regular same document navigations simply use Commit().
    Commit();
    return;
  }

  auto params = BuildDidCommitProvisionalLoadParams(
      true /* same_document */, false /* failed_navigation */,
      render_frame_host_->last_http_status_code());

  browser_interface_broker_receiver_.reset();

  render_frame_host_->SimulateCommitProcessed(
      request_, std::move(params),
      mojo::NullReceiver() /* browser_interface_broker_receiver */,
      true /* same_document */);

  // Same-document commits should never hit network-related stages of committing
  // a navigation.
  CHECK_EQ(0, num_will_start_request_called_);
  CHECK_EQ(0, num_will_process_response_called_);
  CHECK_EQ(0, num_ready_to_commit_called_);

  if (num_did_finish_navigation_called_ == 0) {
    // Fail the navigation if it results in a process kill (e.g. see
    // NavigatorTestWithBrowserSideNavigation.CrossSiteClaimWithinPage test).
    state_ = FAILED;
    return;
  }
  loading_scenario_ =
      TestRenderFrameHost::LoadingScenario::kSameDocumentNavigation;
  state_ = FINISHED;
  if (!keep_loading_)
    StopLoading();

  CHECK_EQ(1, num_did_start_navigation_called_);
  CHECK_EQ(1, num_did_finish_navigation_called_);
}

void NavigationSimulatorImpl::SetInitiatorFrame(
    RenderFrameHost* initiator_frame_host) {
  // Browser-initiated navigations are not associated with an initiator frame.
  CHECK(!browser_initiated_);

  if (initiator_frame_host) {
    // TODO(crbug.com/40127276): Support cross-process initiators here by
    // using NavigationRequest::CreateBrowserInitiated() (like
    // RenderFrameProxyHost does) for the navigation.
    set_initiator_origin(initiator_frame_host->GetLastCommittedOrigin());
  }

  initiator_frame_host_ = initiator_frame_host;
}

void NavigationSimulatorImpl::SetTransition(ui::PageTransition transition) {
  if (frame_tree_node_ && !frame_tree_node_->IsMainFrame()) {
    // Subframe case. The subframe page transition is only set at commit time in
    // the navigation code, so it can be modified later in time.
    CHECK(PageTransitionCoreTypeIs(transition,
                                   ui::PAGE_TRANSITION_AUTO_SUBFRAME) ||
          PageTransitionCoreTypeIs(transition,
                                   ui::PAGE_TRANSITION_MANUAL_SUBFRAME))
        << "The transition type is not appropriate for a subframe";
  } else {
    CHECK_EQ(INITIALIZATION, state_)
        << "The transition cannot be set after the navigation has started";
    CHECK_EQ(ReloadType::NONE, reload_type_)
        << "The transition cannot be specified for reloads";
    CHECK_EQ(0, session_history_offset_)
        << "The transition cannot be specified for back/forward navigations";
  }
  transition_ = transition;
}

void NavigationSimulatorImpl::SetHasUserGesture(bool has_user_gesture) {
  CHECK_EQ(INITIALIZATION, state_) << "The has_user_gesture parameter cannot "
                                      "be set after the navigation has started";
  has_user_gesture_ = has_user_gesture;
}

void NavigationSimulatorImpl::SetNavigationInputStart(
    base::TimeTicks navigation_input_start) {
  navigation_input_start_ = navigation_input_start;
}

void NavigationSimulatorImpl::SetNavigationStart(
    base::TimeTicks navigation_start) {
  navigation_start_ = navigation_start;
}

void NavigationSimulatorImpl::SetReloadType(ReloadType reload_type) {
  CHECK_EQ(INITIALIZATION, state_) << "The reload_type parameter cannot "
                                      "be set after the navigation has started";
  CHECK(browser_initiated_) << "The reload_type parameter can only be set for "
                               "browser-intiated navigations";
  CHECK_EQ(0, session_history_offset_)
      << "The reload_type parameter cannot be set for "
         "session history navigations";
  reload_type_ = reload_type;
  if (reload_type_ != ReloadType::NONE)
    transition_ = ui::PAGE_TRANSITION_RELOAD;
}

void NavigationSimulatorImpl::SetMethod(const std::string& method) {
  CHECK_EQ(INITIALIZATION, state_) << "The method parameter cannot "
                                      "be set after the navigation has started";
  initial_method_ = method;
}

void NavigationSimulatorImpl::SetIsFormSubmission(bool is_form_submission) {
  CHECK_EQ(INITIALIZATION, state_) << "The form submission parameter cannot "
                                      "be set after the navigation has started";
  is_form_submission_ = is_form_submission;
}

void NavigationSimulatorImpl::SetReferrer(blink::mojom::ReferrerPtr referrer) {
  CHECK_LE(state_, STARTED) << "The referrer cannot be set after the "
                               "navigation has committed or has failed";
  referrer_ = std::move(referrer);
}

void NavigationSimulatorImpl::SetSocketAddress(
    const net::IPEndPoint& remote_endpoint) {
  CHECK_LE(state_, STARTED) << "The socket address cannot be set after the "
                               "navigation has committed or failed";
  remote_endpoint_ = remote_endpoint;
}

void NavigationSimulatorImpl::SetWasFetchedViaCache(
    bool was_fetched_via_cache) {
  CHECK_LE(state_, STARTED) << "The was_fetched_via_cache flag cannot be set "
                               "after the navigation has committed or failed";
  was_fetched_via_cache_ = was_fetched_via_cache;
}

void NavigationSimulatorImpl::SetIsSignedExchangeInnerResponse(
    bool is_signed_exchange_inner_response) {
  CHECK_LE(state_, STARTED) << "The signed exchange flag cannot be set after "
                               "the navigation has committed or failed";
  is_signed_exchange_inner_response_ = is_signed_exchange_inner_response;
}

void NavigationSimulatorImpl::SetPermissionsPolicyHeader(
    blink::ParsedPermissionsPolicy permissions_policy_header) {
  CHECK_LE(state_, STARTED) << "The Permissions-Policy headers cannot be set "
                               "after the navigation has committed or failed";
  permissions_policy_header_ = std::move(permissions_policy_header);
}

void NavigationSimulatorImpl::SetContentsMimeType(
    const std::string& contents_mime_type) {
  CHECK_LE(state_, STARTED) << "The contents mime type cannot be set after the "
                               "navigation has committed or failed";
  contents_mime_type_ = contents_mime_type;
}

void NavigationSimulatorImpl::SetRedirectHeaders(
    scoped_refptr<net::HttpResponseHeaders> redirect_headers) {
  CHECK_LE(state_, STARTED) << "The redirect headers cannot be set after the "
                               "navigation has committed or failed";
  redirect_headers_ = redirect_headers;
}

void NavigationSimulatorImpl::SetResponseHeaders(
    scoped_refptr<net::HttpResponseHeaders> response_headers) {
  CHECK_LE(state_, STARTED) << "The response headers cannot be set after the "
                               "navigation has committed or failed";
  response_headers_ = response_headers;
}

void NavigationSimulatorImpl::SetResponseBody(
    mojo::ScopedDataPipeConsumerHandle response_body) {
  CHECK_LE(state_, STARTED) << "The response body cannot be set after the "
                               "navigation has committed or failed";
  response_body_ = std::move(response_body);
}

void NavigationSimulatorImpl::SetLoadURLParams(
    NavigationController::LoadURLParams* load_url_params) {
  load_url_params_ = load_url_params;

  // Make sure the internal attributes of NavigationSimulatorImpl match the
  // LoadURLParams that is going to be sent.
  referrer_ = blink::mojom::Referrer::New(load_url_params->referrer.url,
                                          load_url_params->referrer.policy);
  transition_ = load_url_params->transition_type;
}

void NavigationSimulatorImpl::SetAutoAdvance(bool auto_advance) {
  auto_advance_ = auto_advance;
}

void NavigationSimulatorImpl::SetResolveErrorInfo(
    const net::ResolveErrorInfo& resolve_error_info) {
  resolve_error_info_ = resolve_error_info;
}

void NavigationSimulatorImpl::SetSSLInfo(const net::SSLInfo& ssl_info) {
  ssl_info_ = ssl_info;
}

void NavigationSimulatorImpl::SetResponseDnsAliases(
    std::vector<std::string> aliases) {
  response_dns_aliases_ = std::move(aliases);
}

void NavigationSimulatorImpl::SetEarlyHintsPreloadLinkHeaderReceived(
    bool received) {
  early_hints_preload_link_header_received_ = received;
}

NavigationThrottle::ThrottleCheckResult
NavigationSimulatorImpl::GetLastThrottleCheckResult() {
  return last_throttle_check_result_.value();
}

NavigationRequest* NavigationSimulatorImpl::GetNavigationHandle() {
  CHECK_GE(state_, STARTED);
  return request_;
}

content::GlobalRequestID NavigationSimulatorImpl::GetGlobalRequestID() {
  CHECK_GT(state_, STARTED) << "The GlobalRequestID is not available until "
                               "after the navigation has completed "
                               "WillProcessResponse";
  return request_id_;
}

void NavigationSimulatorImpl::BrowserInitiatedStartAndWaitBeforeUnload() {
  if (reload_type_ != ReloadType::NONE) {
    web_contents_->GetController().Reload(reload_type_,
                                          false /*check_for_repost */);
  } else if (session_history_offset_) {
    web_contents_->GetController().GoToOffset(session_history_offset_);
  } else {
    if (load_url_params_) {
      web_contents_->GetController().LoadURLWithParams(*load_url_params_);
      load_url_params_ = nullptr;
    } else {
      NavigationController::LoadURLParams load_url_params(navigation_url_);
      load_url_params.referrer = Referrer(*referrer_);
      load_url_params.transition_type = transition_;
      load_url_params.should_replace_current_entry =
          should_replace_current_entry_;
      load_url_params.initiator_origin = initiator_origin_;
      load_url_params.impression = impression_;
      if (initial_method_ == "POST")
        load_url_params.load_type = NavigationController::LOAD_TYPE_HTTP_POST;

      web_contents_->GetController().LoadURLWithParams(load_url_params);
    }
  }

  auto& controller =
      static_cast<NavigationControllerImpl&>(web_contents_->GetController());
  frame_tree_node_ = GetFrameTreeNodeForPendingEntry(controller);
  CHECK(frame_tree_node_);
  render_frame_host_ =
      static_cast<TestRenderFrameHost*>(frame_tree_node_->current_frame_host());

  // The navigation url might have been rewritten by the NavigationController.
  // Update it.
  NavigationEntryImpl* pending_entry =
      static_cast<NavigationEntryImpl*>(controller.GetPendingEntry());
  FrameNavigationEntry* pending_frame_entry =
      pending_entry->GetFrameEntry(frame_tree_node_);
  navigation_url_ = pending_frame_entry->url();

  state_ = WAITING_BEFORE_UNLOAD;
}

void NavigationSimulatorImpl::RenderFrameDeleted(
    RenderFrameHost* render_frame_host) {
  if (initiator_frame_host_ == render_frame_host) {
    initiator_frame_host_ = nullptr;
  }
}

void NavigationSimulatorImpl::DidStartNavigation(
    NavigationHandle* navigation_handle) {
  // Check if this navigation is the one we're simulating.
  NavigationRequest* request = NavigationRequest::From(navigation_handle);
  if (request_ && request_ != request)
    return;
  if (request->frame_tree_node() != frame_tree_node_)
    return;

  request_ = request;
  num_did_start_navigation_called_++;

  was_prerendered_page_activation_ = request_->IsPrerenderedPageActivation();

  // Some navigation requests are not directly created by the
  // NavigationSimulator, so we should set some parameters manually after the
  // navigation started.
  request_->set_has_user_gesture(has_user_gesture_);

  // Add a throttle to count NavigationThrottle calls count.
  RegisterTestThrottle();
  PrepareCompleteCallbackOnRequest();

  if (did_start_navigation_closure_)
    std::move(did_start_navigation_closure_).Run();
}

void NavigationSimulatorImpl::DidRedirectNavigation(
    NavigationHandle* navigation_handle) {
  if (request_ == navigation_handle)
    num_did_redirect_navigation_called_++;
}

void NavigationSimulatorImpl::ReadyToCommitNavigation(
    NavigationHandle* navigation_handle) {
  if (request_ && navigation_handle == request_)
    num_ready_to_commit_called_++;
}

void NavigationSimulatorImpl::DidFinishNavigation(
    NavigationHandle* navigation_handle) {
  NavigationRequest* request = NavigationRequest::From(navigation_handle);
  if (request == request_) {
    num_did_finish_navigation_called_++;
    if (request->IsPageActivation()) {
      // Back-forward cache navigations and prerendered page activations commit
      // and finish synchronously, unlike all other navigations, which wait for
      // a reply from the renderer. The |state_| is normally updated to
      // 'FINISHED' when we simulate a renderer reply at the end of the
      // NavigationSimulatorImpl::Commit() function, but we have not reached
      // this stage yet. Set |state_| to FINISHED to ensure that we would not
      // try to simulate navigation commit for the second time.
      RenderFrameHostImpl* previous_rfh = RenderFrameHostImpl::FromID(
          navigation_handle->GetPreviousRenderFrameHostId());
      CHECK(previous_rfh) << "Previous RenderFrameHost should not be destroyed "
                             "without a Unload_ACK";

      // If the frame is not alive we do not dispatch Unload ACK.
      // CommitPending() may be called immediately and delete the old
      // RenderFrameHost, so we need to record that now while we can still
      // access the object.
      if (!previous_rfh->IsRenderFrameLive())
        drop_unload_ack_ = true;
      SimulateUnloadCompletionCallbackForPreviousFrameIfNeeded(previous_rfh);
      state_ = FINISHED;
    }
    request_ = nullptr;
    if (was_aborted_prior_to_ready_to_commit_)
      CHECK_EQ(net::ERR_ABORTED, request->GetNetErrorCode());
  }
}

void NavigationSimulatorImpl::OnWillStartRequest() {
  num_will_start_request_called_++;
}

void NavigationSimulatorImpl::OnWillRedirectRequest() {
  num_will_redirect_request_called_++;
}

void NavigationSimulatorImpl::OnWillFailRequest() {
  num_will_fail_request_called_++;
}

void NavigationSimulatorImpl::OnWillProcessResponse() {
  num_will_process_response_called_++;
}

bool NavigationSimulatorImpl::SimulateBrowserInitiatedStart() {
  if (state_ == INITIALIZATION)
    BrowserInitiatedStartAndWaitBeforeUnload();

  // Simulate the BeforeUnload completion callback if needed.
  NavigationRequest* request = frame_tree_node_->navigation_request();
  if (request &&
      request->state() == NavigationRequest::WAITING_FOR_RENDERER_RESPONSE) {
    if (block_invoking_before_unload_completed_callback_) {
      // Since we do not simulate the BeforeUnloadCompleted, DidStartNavigation
      // will not have been called, and |request_| will not be properly set. Do
      // it manually.
      request_ = request;
      return false;
    }
    render_frame_host_->SimulateBeforeUnloadCompleted(true /* proceed */);
  }

  // Note: WillStartRequest checks can destroy the request synchronously, or
  // this can be a navigation that doesn't need a network request and that was
  // passed directly to a RenderFrameHost for commit.
  request = web_contents_->GetPrimaryMainFrame()
                ->frame_tree_node()
                ->navigation_request();
  if (!request) {
    if (blink::IsRendererDebugURL(navigation_url_)) {
      // We don't create NavigationRequests nor NavigationHandles for a
      // navigation to a renderer-debug URL. Instead, the URL is passed to the
      // current RenderFrameHost so that the renderer process can handle it.
      CHECK(!request_);
      CHECK(web_contents_->GetPrimaryMainFrame()->is_loading());

      // A navigation to a renderer-debug URL cannot commit. Simulate the
      // renderer process aborting it.
      render_frame_host_ = static_cast<TestRenderFrameHost*>(
          web_contents_->GetPrimaryMainFrame());
      StopLoading();
      state_ = FAILED;
      return false;
    } else if (request_ &&
               web_contents_->GetPrimaryMainFrame()
                   ->GetSameDocumentNavigationRequest(
                       request_->commit_params().navigation_token)) {
      CHECK(request_->IsSameDocument());
      same_document_ = true;
      return true;
    }
    return false;
  }

  // Prerendered page activation can be deferred by CommitDeferringConditions in
  // BeginNavigation(), and `request_` may not have been set by
  // DidStartNavigation() yet. In that case, we set the `request_` here.
  if (request->is_running_potential_prerender_activation_checks()) {
    DCHECK(!request_);
    request_ = request;
  }

  CHECK_EQ(request_, request);
  return true;
}

bool NavigationSimulatorImpl::SimulateRendererInitiatedStart() {
  if (session_history_offset_) {
    static_cast<NavigationControllerImpl&>(web_contents_->GetController())
        .GoToOffsetFromRenderer(
            session_history_offset_, render_frame_host_,
            /*soft_navigation_heuristics_task_id=*/std::nullopt);
    request_ = render_frame_host_->frame_tree_node()->navigation_request();
    return true;
  }

  blink::mojom::BeginNavigationParamsPtr begin_params =
      blink::mojom::BeginNavigationParams::New(
          initiator_frame_host_
              ? std::make_optional(initiator_frame_host_->GetFrameToken())
              : std::nullopt,
          headers_, load_flags_, skip_service_worker_, request_context_type_,
          mixed_content_context_type_, is_form_submission_,
          false /* was_initiated_by_link_click */,
          blink::mojom::ForceHistoryPush::kNo, searchable_form_url_,
          searchable_form_encoding_, GURL() /* client_side_redirect_url */,
          std::nullopt /* detools_initiator_info */,
          nullptr /* trust_token_params */, impression_,
          base::TimeTicks() /* renderer_before_unload_start */,
          base::TimeTicks() /* renderer_before_unload_end */,
          has_user_gesture_
              ? blink::mojom::NavigationInitiatorActivationAndAdStatus::
                    kStartedWithTransientActivationFromNonAd
              : blink::mojom::NavigationInitiatorActivationAndAdStatus::
                    kDidNotStartWithTransientActivation,
          false /* is_container_initiated */,
          net::StorageAccessApiStatus::kNone, false /* has_rel_opener */);
  auto common_params = blink::CreateCommonNavigationParams();
  common_params->navigation_start =
      navigation_start_.is_null() ? base::TimeTicks::Now() : navigation_start_;
  common_params->input_start = navigation_input_start_;
  common_params->url = navigation_url_;
  common_params->initiator_origin = initiator_origin_.value();
  common_params->method = initial_method_;
  common_params->referrer = referrer_.Clone();
  common_params->transition = transition_;
  common_params->navigation_type =
      PageTransitionCoreTypeIs(transition_, ui::PAGE_TRANSITION_RELOAD)
          ? blink::mojom::NavigationType::RELOAD
          : blink::mojom::NavigationType::DIFFERENT_DOCUMENT;
  common_params->has_user_gesture = has_user_gesture_;
  common_params->should_check_main_world_csp = should_check_main_world_csp_;
  common_params->should_replace_current_entry = should_replace_current_entry_;
  common_params->href_translate = href_translate_;
  common_params->request_destination =
      network::mojom::RequestDestination::kDocument;

  mojo::PendingAssociatedRemote<mojom::NavigationClient>
      navigation_client_remote;
  navigation_client_receiver_ =
      navigation_client_remote.InitWithNewEndpointAndPassReceiver();
  render_frame_host_->frame_host_receiver_for_testing().impl()->BeginNavigation(
      std::move(common_params), std::move(begin_params), mojo::NullRemote(),
      std::move(navigation_client_remote), mojo::NullRemote(),
      mojo::NullReceiver());

  NavigationRequest* request =
      render_frame_host_->frame_tree_node()->navigation_request();

  // The request failed synchronously.
  if (!request)
    return false;

  // `request_` may not have been set by DidStartNavigation() yet, due to
  // either:
  // 1) Prerendered page activation can be deferred by CommitDeferringConditions
  //    in BeginNavigation().
  // 2) Fenced frame navigation can be deferred on pending URL mapping.
  //
  // In these cases, we set the `request_` here.
  if (request->is_running_potential_prerender_activation_checks() ||
      request->is_deferred_on_fenced_frame_url_mapping_for_testing()) {
    DCHECK(!request_);
    request_ = request;
  }

  CHECK_EQ(request_, request);
  return true;
}

void NavigationSimulatorImpl::MaybeWaitForThrottleChecksComplete(
    base::OnceClosure complete_closure) {
  // If last_throttle_check_result_ is set, then the navigation phase completed
  // synchronously.
  if (last_throttle_check_result_) {
    std::move(complete_closure).Run();
    return;
  }

  throttle_checks_complete_closure_ = std::move(complete_closure);
  if (auto_advance_)
    Wait();
}

void NavigationSimulatorImpl::MaybeWaitForReadyToCommitCheckComplete() {
  if (state_ >= READY_TO_COMMIT || !auto_advance_)
    return;

  CHECK(!wait_closure_);
  base::RunLoop run_loop;
  wait_closure_ = run_loop.QuitClosure();
  run_loop.Run();
}

void NavigationSimulatorImpl::Wait() {
  CHECK(!wait_closure_);
  if (!IsDeferred())
    return;
  base::RunLoop run_loop;
  wait_closure_ = run_loop.QuitClosure();
  run_loop.Run();
}

bool NavigationSimulatorImpl::OnThrottleChecksComplete(
    NavigationThrottle::ThrottleCheckResult result) {
  if (request_->IsPageActivation()) {
    // Throttles don't run in page activations so we shouldn't ever get back
    // anything other than PROCEED.
    CHECK_EQ(result.action(), NavigationThrottle::PROCEED);
  }
  CHECK(!last_throttle_check_result_);
  last_throttle_check_result_ = result;
  if (wait_closure_)
    std::move(wait_closure_).Run();
  if (throttle_checks_complete_closure_)
    std::move(throttle_checks_complete_closure_).Run();
  return false;
}

void NavigationSimulatorImpl::PrepareCompleteCallbackOnRequest() {
  last_throttle_check_result_.reset();
  request_->set_complete_callback_for_testing(
      base::BindOnce(&NavigationSimulatorImpl::OnThrottleChecksComplete,
                     base::Unretained(this)));
}

RenderFrameHost* NavigationSimulatorImpl::GetFinalRenderFrameHost() {
  CHECK_GE(state_, READY_TO_COMMIT);
  return render_frame_host_;
}

bool NavigationSimulatorImpl::IsDeferred() {
  return !throttle_checks_complete_closure_.is_null();
}

bool NavigationSimulatorImpl::HasFailed() {
  return state_ == FAILED;
}

bool NavigationSimulatorImpl::DidCreateNewEntry(
    bool same_document,
    bool should_replace_current_entry) {
  if (session_history_offset_ ||
      (request_ && NavigationTypeUtils::IsHistory(
                       request_->common_params().navigation_type))) {
    // History navigation.
    return false;
  }
  bool has_valid_page_state =
      request_ && blink::PageState::CreateFromEncodedData(
                      request_->commit_params().page_state)
                      .IsValid();

  if (has_valid_page_state && NavigationTypeUtils::IsRestore(
                                  request_->common_params().navigation_type)) {
    // Restore navigation.
    return false;
  }

  bool is_reload = reload_type_ != ReloadType::NONE ||
                   (request_ && NavigationTypeUtils::IsReload(
                                    request_->common_params().navigation_type));

  // Return false if this is a "standard" (non replacement/reload) commit, or a
  // main frame cross-document replacement.
  return (!is_reload && !should_replace_current_entry) ||
         (frame_tree_node_->IsMainFrame() && should_replace_current_entry &&
          !same_document);
}

void NavigationSimulatorImpl::SetSessionHistoryOffset(
    int session_history_offset) {
  CHECK(session_history_offset);
  session_history_offset_ = session_history_offset;
  transition_ =
      ui::PageTransitionFromInt(transition_ | ui::PAGE_TRANSITION_FORWARD_BACK);
}

void NavigationSimulatorImpl::set_history_list_was_cleared(
    bool history_cleared) {
  history_list_was_cleared_ = history_cleared;
}

mojom::DidCommitProvisionalLoadParamsPtr
NavigationSimulatorImpl::BuildDidCommitProvisionalLoadParams(
    bool same_document,
    bool failed_navigation,
    int last_http_status_code) {
  auto params = mojom::DidCommitProvisionalLoadParams::New();
  params->url = navigation_url_;
  params->referrer = mojo::Clone(referrer_);
  params->contents_mime_type = contents_mime_type_;
  params->history_list_was_cleared = history_list_was_cleared_;

  RenderFrameHostImpl* current_rfh = frame_tree_node_->current_frame_host();

  params->did_create_new_entry = DidCreateNewEntry(
      same_document,
      should_replace_current_entry_ ||
          (request_ && request_->common_params().should_replace_current_entry));

  // See CalculateTransition() in render_frame_host_impl.cc.
  if (frame_tree_node_->IsMainFrame() && request_) {
    params->transition =
        ui::PageTransitionFromInt(request_->common_params().transition);
  } else if (!params->did_create_new_entry &&
             PageTransitionCoreTypeIs(transition_,
                                      ui::PAGE_TRANSITION_MANUAL_SUBFRAME)) {
    // Non-standard commits (replacements, reloads, history navigations, etc) on
    // subframe will result in PAGE_TRANSITION_AUTO_SUBFRAME transition. These
    // navigations can be detected from the |did_create_new_entry| value (on
    // subframes).
    params->transition = ui::PAGE_TRANSITION_AUTO_SUBFRAME;
  } else {
    params->transition = transition_;
  }

  params->navigation_token = request_
                                 ? request_->commit_params().navigation_token
                                 : base::UnguessableToken::Create();
  params->post_id = post_id_;

  params->method = request_ ? request_->common_params().method : "GET";

  if (failed_navigation) {
    params->url_is_unreachable = true;
  } else if (same_document) {
    params->should_update_history = true;
  } else {
    // TODO(crbug.com/40161149): Reconsider how we calculate
    // should_update_history.
    params->should_update_history = response_headers_->response_code() != 404;
  }

  // This mirrors the calculation in
  // RenderFrameImpl::MakeDidCommitProvisionalLoadParams.
  // TODO(wjmaclean): If params->url is about:blank or about:srcdoc then we
  // should also populate params->initiator_base_url in a manner similar to
  // RenderFrameImpl::MakeDidCommitProvisionalLoadParams.
  if (same_document) {
    params->origin = current_rfh->GetLastCommittedOrigin();
  } else {
    params->origin = origin_.value_or(
        request_->browser_side_origin_to_commit_with_debug_info()
            .first.value());
  }

  if (same_document) {
    // Same document navigations always retain the last HTTP status code.
    params->http_status_code = last_http_status_code;
  } else if (request_ && request_->commit_params().http_response_code != -1) {
    // If we have a valid HTTP response code in |request_|, use it.
    params->http_status_code = request_->commit_params().http_response_code;
  } else {
    // Otherwise, unit tests will never issue real network requests and thus
    // will never receive any HTTP response.
    params->http_status_code = 0;
  }

  CHECK(same_document || request_);

  params->permissions_policy_header = std::move(permissions_policy_header_);

  // Simulate Blink assigning a item sequence number and document sequence
  // number to the navigation.
  params->item_sequence_number = ++g_unique_identifier;
  if (same_document) {
    FrameNavigationEntry* current_entry =
        web_contents_->GetController().GetLastCommittedEntry()->GetFrameEntry(
            frame_tree_node_);
    params->document_sequence_number =
        current_entry->document_sequence_number();
  } else {
    params->document_sequence_number = ++g_unique_identifier;
  }

  // Simulate embedding token creation.
  if (!same_document)
    params->embedding_token = base::UnguessableToken::Create();

  params->page_state = page_state_.value_or(
      blink::PageState::CreateForTestingWithSequenceNumbers(
          navigation_url_, params->item_sequence_number,
          params->document_sequence_number));

  params->is_overriding_user_agent =
      request_ ? (request_->commit_params().is_overriding_user_agent &&
                  frame_tree_node_->IsMainFrame())
               : false;

  params->insecure_request_policy = insecure_request_policy_;
  params->insecure_navigations_set = insecure_navigations_set_;
  params->has_potentially_trustworthy_unique_origin =
      has_potentially_trustworthy_unique_origin_;

  return params;
}

void NavigationSimulatorImpl::SetKeepLoading(bool keep_loading) {
  keep_loading_ = keep_loading;
}

void NavigationSimulatorImpl::StopLoading() {
  CHECK(render_frame_host_);
  render_frame_host_->SimulateLoadingCompleted(loading_scenario_);
}

void NavigationSimulatorImpl::
    SimulateUnloadCompletionCallbackForPreviousFrameIfNeeded(
        RenderFrameHostImpl* previous_rfh) {
  // Do not dispatch mojo::AgentSchedulingGroupHost::DidUnloadRenderFrame if the
  // navigation was committed in the same RenderFrameHost.
  if (previous_rfh == render_frame_host_)
    return;
  if (drop_unload_ack_)
    return;
  // The previous RenderFrameHost is not live, we will not attempt to unload
  // it.
  if (!previous_rfh->IsRenderFrameLive())
    return;
  // The previous RenderFrameHost entered the back-forward cache and hasn't been
  // requested to unload. The browser process do not expect
  // mojo::AgentSchedulingGroupHost::DidUnloadRenderFrame.
  if (previous_rfh->IsInBackForwardCache())
    return;
  previous_rfh->OnUnloadACK();
}

bool NavigationSimulatorImpl::NeedsThrottleChecks() const {
  if (same_document_) {
    return false;
  }

  if (navigation_url_.IsAboutBlank()) {
    return false;
  }

  // Back/forward cache restores and prerendering page activations do not run
  // NavigationThrottles since they were already run when the page was first
  // loaded.
  DCHECK(request_);
  if (request_->is_running_potential_prerender_activation_checks() ||
      request_->IsPageActivation()) {
    return false;
  }

  return IsURLHandledByNetworkStack(navigation_url_);
}

bool NavigationSimulatorImpl::NeedsPreCommitChecks() const {
  DCHECK(request_);
  return NeedsThrottleChecks() || request_->IsPageActivation();
}

}  // namespace content
