// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/navigation_simulator_impl.h"

#include <utility>
#include "base/bind.h"
#include "base/debug/stack_trace.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "content/browser/renderer_host/debug_urls.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/navigation_entry_impl.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/frame_messages.h"
#include "content/common/navigation_params.h"
#include "content/common/navigation_params_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/navigation_policy.h"
#include "content/public/common/url_utils.h"
#include "content/test/test_navigation_url_loader.h"
#include "content/test/test_render_frame_host.h"
#include "content/test/test_web_contents.h"
#include "ipc/ipc_message.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/base/load_flags.h"
#include "net/url_request/redirect_info.h"
#include "services/metrics/public/cpp/ukm_recorder.h"

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

FrameTreeNode* GetFrameTreeNodeForPendingEntry(WebContentsImpl* contents) {
  NavigationEntryImpl* pending_entry =
      contents->GetController().GetPendingEntry();
  int frame_tree_node_id = pending_entry->frame_tree_node_id();
  FrameTree* frame_tree = contents->GetFrameTree();
  if (frame_tree_node_id == -1)
    return frame_tree->root();
  return frame_tree->FindByID(frame_tree_node_id);
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
  auto simulator =
      NavigationSimulatorImpl::CreateHistoryNavigation(offset, web_contents);
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
  auto simulator =
      NavigationSimulator::CreateHistoryNavigation(offset, web_contents);
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
                                             WebContents* web_contents) {
  return NavigationSimulatorImpl::CreateHistoryNavigation(offset, web_contents);
}

// static
std::unique_ptr<NavigationSimulatorImpl>
NavigationSimulatorImpl::CreateHistoryNavigation(int offset,
                                                 WebContents* web_contents) {
  auto simulator =
      NavigationSimulatorImpl::CreateBrowserInitiated(GURL(), web_contents);
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
  return std::unique_ptr<NavigationSimulatorImpl>(new NavigationSimulatorImpl(
      original_url, false /* browser_initiated */,
      static_cast<WebContentsImpl*>(
          WebContents::FromRenderFrameHost(render_frame_host)),
      static_cast<TestRenderFrameHost*>(render_frame_host)));
}

// static
std::unique_ptr<NavigationSimulator> NavigationSimulator::CreateFromPending(
    WebContents* contents) {
  return NavigationSimulatorImpl::CreateFromPending(contents);
}

// static
std::unique_ptr<NavigationSimulatorImpl>
NavigationSimulatorImpl::CreateFromPending(WebContents* contents) {
  WebContentsImpl* contents_impl = static_cast<WebContentsImpl*>(contents);

  FrameTreeNode* frame_tree_node =
      GetFrameTreeNodeForPendingEntry(contents_impl);
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
      frame_tree_node_(render_frame_host
                           ? render_frame_host->frame_tree_node()
                           : web_contents->GetMainFrame()->frame_tree_node()),
      request_(nullptr),
      original_url_(original_url),
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
    if (!render_frame_host->frame_tree_node()->has_committed_real_load())
      transition_ = ui::PAGE_TRANSITION_AUTO_SUBFRAME;
    else
      transition_ = ui::PAGE_TRANSITION_MANUAL_SUBFRAME;
  }

  mojo::PendingRemote<service_manager::mojom::InterfaceProvider>
      stub_interface_provider;
  interface_provider_receiver_ =
      stub_interface_provider.InitWithNewPipeAndPassReceiver();
  browser_interface_broker_receiver_ =
      mojo::PendingRemote<blink::mojom::BrowserInterfaceBroker>()
          .InitWithNewPipeAndPassReceiver();
}

NavigationSimulatorImpl::~NavigationSimulatorImpl() {}

void NavigationSimulatorImpl::SetIsPostWithId(int64_t post_id) {
  post_id_ = post_id;
  SetMethod("POST");
}

void NavigationSimulatorImpl::InitializeFromStartedRequest(
    NavigationRequest* request) {
  CHECK(request);
  request_ = request;
  CHECK(request_->IsNavigationStarted());
  CHECK_EQ(web_contents_, request_->GetDelegate());
  CHECK(render_frame_host_);
  CHECK_EQ(frame_tree_node_, request_->frame_tree_node());
  state_ = STARTED;
  original_url_ = request->commit_params().original_url;
  navigation_url_ = request_->GetURL();
  // |remote_endpoint_| cannot be inferred from the request.
  // |initial_method_| cannot be set after the request has started.
  browser_initiated_ = request_->browser_initiated();
  // |same_document_| should always be false here.
  referrer_ = request_->common_params().referrer.Clone();
  transition_ = request_->GetPageTransition();
  // |reload_type_| cannot be set after the request has started.
  // |session_history_offset_| cannot be set after the request has started.
  has_user_gesture_ = request_->HasUserGesture();
  // |contents_mime_type_| cannot be inferred from the request.

  // Add a throttle to count NavigationThrottle calls count. Bump
  // num_did_start_navigation to account for the fact that the navigation handle
  // has already been created.
  num_did_start_navigation_called_++;
  RegisterTestThrottle(request);
  PrepareCompleteCallbackOnRequest();
}

void NavigationSimulatorImpl::RegisterTestThrottle(NavigationRequest* request) {
  request->RegisterThrottleForTesting(
      std::make_unique<NavigationThrottleCallbackRunner>(
          request,
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
  if (IsRendererDebugURL(navigation_url_))
    return;

  if (same_document_ || !IsURLHandledByNetworkStack(navigation_url_) ||
      navigation_url_.IsAboutBlank()) {
    CHECK_EQ(1, num_did_start_navigation_called_);
    return;
  }

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
      if (state_ == FAILED)
        return;
    }
  }

  if (!response_headers_) {
    response_headers_ =
        base::MakeRefCounted<net::HttpResponseHeaders>(std::string());
  }
  response_headers_->SetHeader("Content-Type", contents_mime_type_);
  PrepareCompleteCallbackOnRequest();
  if (frame_tree_node_->navigation_request()) {
    static_cast<TestRenderFrameHost*>(frame_tree_node_->current_frame_host())
        ->PrepareForCommitDeprecatedForNavigationSimulator(
            remote_endpoint_, was_fetched_via_cache_,
            is_signed_exchange_inner_response_, http_connection_info_,
            ssl_info_, response_headers_);
  }

  // Synchronous failure can cause the navigation to finish here.
  if (!request_) {
    state_ = FAILED;
    return;
  }

  bool needs_throttle_checks = !same_document_ &&
                               !navigation_url_.IsAboutBlank() &&
                               IsURLHandledByNetworkStack(navigation_url_);
  auto complete_closure =
      base::BindOnce(&NavigationSimulatorImpl::ReadyToCommitComplete,
                     weak_factory_.GetWeakPtr(), needs_throttle_checks);
  if (needs_throttle_checks) {
    MaybeWaitForThrottleChecksComplete(std::move(complete_closure));
    return;
  }
  std::move(complete_closure).Run();
}

void NavigationSimulatorImpl::ReadyToCommitComplete(bool ran_throttles) {
  if (ran_throttles) {
    if (GetLastThrottleCheckResult().action() != NavigationThrottle::PROCEED) {
      state_ = FAILED;
      return;
    }
    CHECK_EQ(1, num_will_process_response_called_);
    CHECK_EQ(1, num_ready_to_commit_called_);
  }

  request_id_ = request_->GetGlobalRequestID();

  // Update the RenderFrameHost now that we know which RenderFrameHost will
  // commit the navigation.
  render_frame_host_ =
      static_cast<TestRenderFrameHost*>(request_->GetRenderFrameHost());
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
  RenderFrameHostImpl* previous_rfh =
      render_frame_host_->frame_tree_node()->current_frame_host();

  // RenderDocument: Do not dispatch UnloadACK if the navigation was committed
  // in the same SiteInstance. This has already been dispatched during the
  // navigation in the renderer process.
  if (previous_rfh->GetSiteInstance() == render_frame_host_->GetSiteInstance())
    drop_unload_ack_ = true;

  if (same_document_) {
    interface_provider_receiver_.reset();
    browser_interface_broker_receiver_.reset();
  }

  auto params = BuildDidCommitProvisionalLoadParams(
      same_document_ /* same_document */, false /* failed_navigation */);
  render_frame_host_->SimulateCommitProcessed(
      request_, std::move(params), std::move(interface_provider_receiver_),
      std::move(browser_interface_broker_receiver_), same_document_);

  SimulateUnloadCompletionCallbackForPreviousFrameIfNeeded(previous_rfh);

  loading_scenario_ =
      TestRenderFrameHost::LoadingScenario::NewDocumentNavigation;
  state_ = FINISHED;
  if (!keep_loading_)
    StopLoading();

  if (!IsRendererDebugURL(navigation_url_))
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

  was_aborted_ = true;
  request_->RendererAbortedNavigationForTesting();
  state_ = FINISHED;

  CHECK_EQ(1, num_did_finish_navigation_called_);
}

void NavigationSimulatorImpl::Fail(int error_code) {
  CHECK_LE(state_, STARTED) << "NavigationSimulatorImpl::Fail can only be "
                               "called once, and cannot be called after "
                               "NavigationSimulatorImpl::ReadyToCommit";
  CHECK_EQ(0, num_did_finish_navigation_called_)
      << "NavigationSimulatorImpl::Fail cannot be called after the "
         "navigation has finished";
  CHECK(!IsRendererDebugURL(navigation_url_));

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

  auto params = BuildDidCommitProvisionalLoadParams(
      false /* same_document */, true /* failed_navigation */);
  render_frame_host_->SimulateCommitProcessed(
      request_, std::move(params), std::move(interface_provider_receiver_),
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
      true /* same_document */, false /* failed_navigation */);

  interface_provider_receiver_.reset();
  browser_interface_broker_receiver_.reset();

  render_frame_host_->SimulateCommitProcessed(
      request_, std::move(params),
      mojo::NullReceiver() /* interface_provider_receiver_ */,
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
  CHECK(initiator_frame_host);

  // TODO(https://crbug.com/1072790): Support cross-process initiators here by
  // using NavigationRequest::CreateBrowserInitiated() (like
  // RenderFrameProxyHost does) for the navigation.
  CHECK_EQ(render_frame_host_->GetProcess(), initiator_frame_host->GetProcess())
      << "The initiator frame must belong to the same process as the frame you "
         "are navigating";
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

void NavigationSimulatorImpl::SetWasInitiatedByLinkClick(
    bool was_initiated_by_link_click) {
  CHECK_EQ(INITIALIZATION, state_) << "The form submission parameter cannot "
                                      "be set after the navigation has started";
  was_initiated_by_link_click_ = was_initiated_by_link_click;
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

void NavigationSimulatorImpl::SetInterfaceProviderReceiver(
    mojo::PendingReceiver<service_manager::mojom::InterfaceProvider> receiver) {
  CHECK_LE(state_, STARTED) << "The InterfaceProvider cannot be set "
                               "after the navigation has committed or failed";
  CHECK(receiver.is_valid());
  interface_provider_receiver_ = std::move(receiver);
}

void NavigationSimulatorImpl::SetContentsMimeType(
    const std::string& contents_mime_type) {
  CHECK_LE(state_, STARTED) << "The contents mime type cannot be set after the "
                               "navigation has committed or failed";
  contents_mime_type_ = contents_mime_type;
}

void NavigationSimulatorImpl::SetResponseHeaders(
    scoped_refptr<net::HttpResponseHeaders> response_headers) {
  CHECK_LE(state_, STARTED) << "The response headers cannot be set after the "
                               "navigation has committed or failed";
  response_headers_ = response_headers;
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
      if (initial_method_ == "POST")
        load_url_params.load_type = NavigationController::LOAD_TYPE_HTTP_POST;

      web_contents_->GetController().LoadURLWithParams(load_url_params);
    }
  }

  frame_tree_node_ = GetFrameTreeNodeForPendingEntry(web_contents_);
  CHECK(frame_tree_node_);
  render_frame_host_ =
      static_cast<TestRenderFrameHost*>(frame_tree_node_->current_frame_host());

  // The navigation url might have been rewritten by the NavigationController.
  // Update it.
  navigation_url_ = web_contents_->GetController().GetPendingEntry()->GetURL();

  state_ = WAITING_BEFORE_UNLOAD;
}

void NavigationSimulatorImpl::DidStartNavigation(
    NavigationHandle* navigation_handle) {
  // Check if this navigation is the one we're simulating.
  if (request_)
    return;

  NavigationRequest* request = NavigationRequest::From(navigation_handle);

  if (request->frame_tree_node() != frame_tree_node_)
    return;

  request_ = request;
  num_did_start_navigation_called_++;

  // Add a throttle to count NavigationThrottle calls count.
  RegisterTestThrottle(request);
  PrepareCompleteCallbackOnRequest();
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
    if (navigation_handle->IsServedFromBackForwardCache()) {
      // Back-forward cache navigations commit and finish synchronously, unlike
      // all other navigations, which wait for a reply from the renderer.
      // The |state_| is normally updated to 'FINISHED' when we simulate a
      // renderer reply at the end of the NavigationSimulatorImpl::Commit()
      // function, but we have not reached this stage yet.
      // Set |state_| to FINISHED to ensure that we would not try to simulate
      // navigation commit for the second time.
      RenderFrameHostImpl* previous_rfh = RenderFrameHostImpl::FromID(
          navigation_handle->GetPreviousRenderFrameHostId());
      CHECK(previous_rfh) << "Previous RenderFrameHost should not be destroyed "
                             "without a Unload_ACK";
      SimulateUnloadCompletionCallbackForPreviousFrameIfNeeded(previous_rfh);
      state_ = FINISHED;
    }
    request_ = nullptr;
    if (was_aborted_)
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
  request =
      web_contents_->GetMainFrame()->frame_tree_node()->navigation_request();
  if (!request) {
    if (IsRendererDebugURL(navigation_url_)) {
      // We don't create NavigationRequests nor NavigationHandles for a
      // navigation to a renderer-debug URL. Instead, the URL is passed to the
      // current RenderFrameHost so that the renderer process can handle it.
      CHECK(!request_);
      CHECK(web_contents_->GetMainFrame()->is_loading());

      // A navigation to a renderer-debug URL cannot commit. Simulate the
      // renderer process aborting it.
      render_frame_host_ =
          static_cast<TestRenderFrameHost*>(web_contents_->GetMainFrame());
      StopLoading();
      state_ = FAILED;
      return false;
    } else if (request_ &&
               web_contents_->GetMainFrame()->navigation_request() ==
                   request_) {
      CHECK(!IsURLHandledByNetworkStack(request_->common_params().url));
      return true;
    } else if (web_contents_->GetMainFrame()
                   ->same_document_navigation_request() &&
               web_contents_->GetMainFrame()
                       ->same_document_navigation_request() == request_) {
      CHECK(request_->IsSameDocument());
      same_document_ = true;
      return true;
    }
    return false;
  }

  CHECK_EQ(request_, request);
  return true;
}

bool NavigationSimulatorImpl::SimulateRendererInitiatedStart() {
  mojom::BeginNavigationParamsPtr begin_params =
      mojom::BeginNavigationParams::New(
          initiator_frame_host_ ? initiator_frame_host_->GetRoutingID()
                                : MSG_ROUTING_NONE /* initiator_routing_id */,
          std::string() /* headers */, net::LOAD_NORMAL,
          false /* skip_service_worker */,
          blink::mojom::RequestContextType::HYPERLINK,
          network::mojom::RequestDestination::kDocument,
          blink::WebMixedContentContextType::kBlockable, is_form_submission_,
          was_initiated_by_link_click_, GURL() /* searchable_form_url */,
          std::string() /* searchable_form_encoding */,
          GURL() /* client_side_redirect_url */,
          base::nullopt /* detools_initiator_info */,
          false /* force_ignore_site_for_cookies */,
          nullptr /* trust_token_params */, impression_,
          base::TimeTicks() /* renderer_before_unload_start */,
          base::TimeTicks() /* renderer_before_unload_end */);
  auto common_params = CreateCommonNavigationParams();
  common_params->navigation_start = base::TimeTicks::Now();
  common_params->url = navigation_url_;
  common_params->initiator_origin = url::Origin();
  common_params->method = initial_method_;
  common_params->referrer = referrer_.Clone();
  common_params->transition = transition_;
  common_params->navigation_type =
      PageTransitionCoreTypeIs(transition_, ui::PAGE_TRANSITION_RELOAD)
          ? mojom::NavigationType::RELOAD
          : mojom::NavigationType::DIFFERENT_DOCUMENT;
  common_params->has_user_gesture = has_user_gesture_;
  common_params->initiator_csp_info = mojom::InitiatorCSPInfo::New(
      should_check_main_world_csp_,
      std::vector<network::mojom::ContentSecurityPolicyPtr>(), nullptr);

  mojo::PendingAssociatedRemote<mojom::NavigationClient>
      navigation_client_remote;
  navigation_client_receiver_ =
      navigation_client_remote.InitWithNewEndpointAndPassReceiver();
  render_frame_host_->frame_host_receiver_for_testing().impl()->BeginNavigation(
      std::move(common_params), std::move(begin_params), mojo::NullRemote(),
      std::move(navigation_client_remote), mojo::NullRemote());

  NavigationRequest* request =
      render_frame_host_->frame_tree_node()->navigation_request();

  // The request failed synchronously.
  if (!request)
    return false;

  CHECK_EQ(request_, request);
  return true;
}

void NavigationSimulatorImpl::MaybeWaitForThrottleChecksComplete(
    base::OnceClosure complete_closure) {
  // If last_throttle_check_result_ is set, then throttle checks completed
  // synchronously.
  if (last_throttle_check_result_) {
    std::move(complete_closure).Run();
    return;
  }

  throttle_checks_complete_closure_ = std::move(complete_closure);
  if (auto_advance_)
    Wait();
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

bool NavigationSimulatorImpl::CheckIfSameDocument() {
  // This approach to determining whether a navigation is to be treated as
  // same document is not robust, as it will not handle pushState type
  // navigation. Do not use elsewhere!

  // First we need a valid document that is not an error page.
  if (!render_frame_host_->GetLastCommittedURL().is_valid() ||
      render_frame_host_->last_commit_was_error_page()) {
    return false;
  }

  // Exclude reloads.
  if (ui::PageTransitionCoreTypeIs(transition_, ui::PAGE_TRANSITION_RELOAD)) {
    return false;
  }

  // A browser-initiated navigation to the exact same url in the address bar is
  // not a same document navigation.
  if (browser_initiated_ &&
      render_frame_host_->GetLastCommittedURL() == navigation_url_) {
    return false;
  }

  // Finally, the navigation url and the last committed url should match,
  // except for the fragment.
  GURL url_copy(navigation_url_);
  url::Replacements<char> replacements;
  replacements.ClearRef();
  return url_copy.ReplaceComponents(replacements) ==
         render_frame_host_->GetLastCommittedURL().ReplaceComponents(
             replacements);
}

bool NavigationSimulatorImpl::DidCreateNewEntry() {
  if (did_create_new_entry_.has_value())
    return did_create_new_entry_.value();
  if (ui::PageTransitionCoreTypeIs(transition_,
                                   ui::PAGE_TRANSITION_AUTO_SUBFRAME))
    return false;
  if (reload_type_ != ReloadType::NONE ||
      (request_ && NavigationTypeUtils::IsReload(
                       request_->common_params().navigation_type))) {
    return false;
  }
  if (session_history_offset_ ||
      (request_ && NavigationTypeUtils::IsHistory(
                       request_->common_params().navigation_type))) {
    return false;
  }
  if (request_ && (request_->common_params().navigation_type ==
                       mojom::NavigationType::RESTORE ||
                   request_->common_params().navigation_type ==
                       mojom::NavigationType::RESTORE_WITH_POST)) {
    return false;
  }

  return true;
}

void NavigationSimulatorImpl::SetSessionHistoryOffset(
    int session_history_offset) {
  CHECK(session_history_offset);
  session_history_offset_ = session_history_offset;
  transition_ =
      ui::PageTransitionFromInt(transition_ | ui::PAGE_TRANSITION_FORWARD_BACK);
}

void NavigationSimulatorImpl::set_did_create_new_entry(
    bool did_create_new_entry) {
  did_create_new_entry_ = did_create_new_entry;
}

void NavigationSimulatorImpl::set_history_list_was_cleared(
    bool history_cleared) {
  history_list_was_cleared_ = history_cleared;
}

std::unique_ptr<FrameHostMsg_DidCommitProvisionalLoad_Params>
NavigationSimulatorImpl::BuildDidCommitProvisionalLoadParams(
    bool same_document,
    bool failed_navigation) {
  std::unique_ptr<FrameHostMsg_DidCommitProvisionalLoad_Params> params =
      std::make_unique<FrameHostMsg_DidCommitProvisionalLoad_Params>();
  params->url = navigation_url_;
  params->original_request_url = original_url_;
  params->referrer = Referrer(*referrer_);
  params->contents_mime_type = contents_mime_type_;
  params->transition = transition_;
  params->gesture =
      has_user_gesture_ ? NavigationGestureUser : NavigationGestureAuto;
  params->history_list_was_cleared = history_list_was_cleared_;
  params->did_create_new_entry = DidCreateNewEntry();
  params->should_replace_current_entry = should_replace_current_entry_;
  params->navigation_token = request_
                                 ? request_->commit_params().navigation_token
                                 : base::UnguessableToken::Create();
  params->post_id = post_id_;

  if (intended_as_new_entry_.has_value())
    params->intended_as_new_entry = intended_as_new_entry_.value();

  if (failed_navigation) {
    // Note: Error pages must commit in a unique origin. So it is left unset.
    params->url_is_unreachable = true;
  } else {
    params->origin = origin_.value_or(url::Origin::Create(navigation_url_));
    params->redirects.push_back(navigation_url_);
    params->method = request_ ? request_->common_params().method : "GET";
    params->http_status_code = 200;
    params->should_update_history = true;
  }

  CHECK(same_document || request_);
  params->nav_entry_id = request_ ? request_->nav_entry_id() : 0;

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

  params->page_state =
      page_state_.value_or(PageState::CreateForTestingWithSequenceNumbers(
          navigation_url_, params->item_sequence_number,
          params->document_sequence_number));

  return params;
}

void NavigationSimulatorImpl::SetKeepLoading(bool keep_loading) {
  keep_loading_ = keep_loading;
}

void NavigationSimulatorImpl::StopLoading() {
  CHECK(render_frame_host_);
  render_frame_host_->SimulateLoadingCompleted(loading_scenario_);
}

void NavigationSimulatorImpl::FailLoading(const GURL& url, int error_code) {
  CHECK(render_frame_host_);
  render_frame_host_->DidFailLoadWithError(url, error_code);
}

void NavigationSimulatorImpl::
    SimulateUnloadCompletionCallbackForPreviousFrameIfNeeded(
        RenderFrameHostImpl* previous_rfh) {
  // Do not dispatch FrameHostMsg_Unload_ACK if the navigation was committed in
  // the same RenderFrameHost.
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
  // FrameHostMsg_Unload_ACK.
  if (previous_rfh->IsInBackForwardCache())
    return;
  previous_rfh->OnMessageReceived(
      FrameHostMsg_Unload_ACK(previous_rfh->GetRoutingID()));
}

}  // namespace content
