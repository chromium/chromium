// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/navigation_simulator.h"

#include <utility>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "content/browser/frame_host/debug_urls.h"
#include "content/browser/frame_host/frame_tree_node.h"
#include "content/browser/frame_host/navigation_handle_impl.h"
#include "content/browser/frame_host/navigation_request.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/frame_messages.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/browser_side_navigation_policy.h"
#include "content/public/common/url_utils.h"
#include "content/test/mock_navigation_client_impl.h"
#include "content/test/test_navigation_url_loader.h"
#include "content/test/test_render_frame_host.h"
#include "content/test/test_web_contents.h"
#include "net/base/host_port_pair.h"
#include "net/base/load_flags.h"
#include "net/url_request/redirect_info.h"

namespace content {

namespace {

class NavigationThrottleCallbackRunner : public NavigationThrottle {
 public:
  NavigationThrottleCallbackRunner(
      NavigationHandle* handle,
      const base::Closure& on_will_start_request,
      const base::Closure& on_will_redirect_request,
      const base::Closure& on_will_fail_request,
      const base::Closure& on_will_process_response)
      : NavigationThrottle(handle),
        on_will_start_request_(on_will_start_request),
        on_will_redirect_request_(on_will_redirect_request),
        on_will_fail_request_(on_will_fail_request),
        on_will_process_response_(on_will_process_response) {}

  NavigationThrottle::ThrottleCheckResult WillStartRequest() override {
    on_will_start_request_.Run();
    return NavigationThrottle::PROCEED;
  }

  NavigationThrottle::ThrottleCheckResult WillRedirectRequest() override {
    on_will_redirect_request_.Run();
    return NavigationThrottle::PROCEED;
  }

  NavigationThrottle::ThrottleCheckResult WillFailRequest() override {
    on_will_fail_request_.Run();
    return NavigationThrottle::PROCEED;
  }

  NavigationThrottle::ThrottleCheckResult WillProcessResponse() override {
    on_will_process_response_.Run();
    return NavigationThrottle::PROCEED;
  }

  const char* GetNameForLogging() override {
    return "NavigationThrottleCallbackRunner";
  }

 private:
  base::Closure on_will_start_request_;
  base::Closure on_will_redirect_request_;
  base::Closure on_will_fail_request_;
  base::Closure on_will_process_response_;
};

int64_t g_unique_identifier = 0;

}  // namespace

// static
RenderFrameHost* NavigationSimulator::NavigateAndCommitFromBrowser(
    WebContents* web_contents,
    const GURL& url) {
  auto simulator =
      NavigationSimulator::CreateBrowserInitiated(url, web_contents);
  simulator->Commit();
  return simulator->GetFinalRenderFrameHost();
}

// static
RenderFrameHost* NavigationSimulator::Reload(WebContents* web_contents) {
  NavigationEntry* entry =
      web_contents->GetController().GetLastCommittedEntry();
  CHECK(entry);
  auto simulator = NavigationSimulator::CreateBrowserInitiated(entry->GetURL(),
                                                               web_contents);
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
      NavigationSimulator::CreateHistoryNavigation(offset, web_contents);
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
  return std::unique_ptr<NavigationSimulator>(new NavigationSimulator(
      original_url, true /* browser_initiated */,
      static_cast<WebContentsImpl*>(web_contents), nullptr));
}

// static
std::unique_ptr<NavigationSimulator>
NavigationSimulator::CreateHistoryNavigation(int offset,
                                             WebContents* web_contents) {
  auto simulator = std::unique_ptr<NavigationSimulator>(new NavigationSimulator(
      GURL(), true /* browser_initiated */,
      static_cast<WebContentsImpl*>(web_contents), nullptr));
  simulator->SetSessionHistoryOffset(offset);
  return simulator;
}

// static
std::unique_ptr<NavigationSimulator>
NavigationSimulator::CreateRendererInitiated(
    const GURL& original_url,
    RenderFrameHost* render_frame_host) {
  return std::unique_ptr<NavigationSimulator>(new NavigationSimulator(
      original_url, false /* browser_initiated */,
      static_cast<WebContentsImpl*>(
          WebContents::FromRenderFrameHost(render_frame_host)),
      static_cast<TestRenderFrameHost*>(render_frame_host)));
}

// static
std::unique_ptr<NavigationSimulator>
NavigationSimulator::CreateFromPendingBrowserInitiated(WebContents* contents) {
  TestRenderFrameHost* test_frame_host =
      static_cast<TestRenderFrameHost*>(contents->GetMainFrame());

  // Simulate the BeforeUnload ACK if needed.
  NavigationRequest* request =
      test_frame_host->frame_tree_node()->navigation_request();
  DCHECK(request);
  if (request->state() == NavigationRequest::WAITING_FOR_RENDERER_RESPONSE)
    test_frame_host->SendBeforeUnloadACK(true /*proceed */);

  auto simulator = base::WrapUnique(new NavigationSimulator(
      GURL(), true /* browser_initiated */,
      static_cast<WebContentsImpl*>(contents), test_frame_host));
  simulator->InitializeFromStartedRequest(request);
  return simulator;
}

NavigationSimulator::NavigationSimulator(const GURL& original_url,
                                         bool browser_initiated,
                                         WebContentsImpl* web_contents,
                                         TestRenderFrameHost* render_frame_host)
    : WebContentsObserver(web_contents),
      web_contents_(web_contents),
      render_frame_host_(render_frame_host),
      frame_tree_node_(render_frame_host
                           ? render_frame_host->frame_tree_node()
                           : web_contents->GetMainFrame()->frame_tree_node()),
      handle_(nullptr),
      navigation_url_(original_url),
      socket_address_("2001:db8::1", 80),
      initial_method_("GET"),
      browser_initiated_(browser_initiated),
      transition_(browser_initiated ? ui::PAGE_TRANSITION_TYPED
                                    : ui::PAGE_TRANSITION_LINK),
      contents_mime_type_("text/html"),
      weak_factory_(this) {
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

  service_manager::mojom::InterfaceProviderPtr stub_interface_provider;
  interface_provider_request_ = mojo::MakeRequest(&stub_interface_provider);
}

NavigationSimulator::~NavigationSimulator() {}

void NavigationSimulator::InitializeFromStartedRequest(
    NavigationRequest* request) {
  NavigationHandle* handle = request->navigation_handle();
  DCHECK(handle);
  DCHECK_EQ(NavigationRequest::STARTED, request->state());
  state_ = STARTED;
  DCHECK_EQ(web_contents_, handle->GetWebContents());
  DCHECK(render_frame_host_);
  DCHECK_EQ(frame_tree_node_, request->frame_tree_node());
  handle_ = static_cast<NavigationHandleImpl*>(handle);
  navigation_url_ = handle->GetURL();
  // |socket_address_| cannot be inferred from the request.
  // |initial_method_| cannot be set after the request has started.
  browser_initiated_ = request->browser_initiated();
  // |same_document_| should always be false here.
  referrer_ = request->common_params().referrer;
  transition_ = handle->GetPageTransition();
  // |reload_type_| cannot be set after the request has started.
  // |session_history_offset_| cannot be set after the request has started.
  has_user_gesture_ = handle->HasUserGesture();
  // |contents_mime_type_| cannot be inferred from the request.

  // Add a throttle to count NavigationThrottle calls count. Bump
  // num_did_start_navigation to account for the fact that the navigation handle
  // has already been created.
  num_did_start_navigation_called_++;
  RegisterTestThrottle(handle);
  PrepareCompleteCallbackOnHandle();
}

void NavigationSimulator::RegisterTestThrottle(NavigationHandle* handle) {
  handle->RegisterThrottleForTesting(
      std::make_unique<NavigationThrottleCallbackRunner>(
          handle,
          base::BindRepeating(&NavigationSimulator::OnWillStartRequest,
                              weak_factory_.GetWeakPtr()),
          base::BindRepeating(&NavigationSimulator::OnWillRedirectRequest,
                              weak_factory_.GetWeakPtr()),
          base::BindRepeating(&NavigationSimulator::OnWillFailRequest,
                              weak_factory_.GetWeakPtr()),
          base::BindRepeating(&NavigationSimulator::OnWillProcessResponse,
                              weak_factory_.GetWeakPtr())));
}

void NavigationSimulator::Start() {
  CHECK(state_ == INITIALIZATION)
      << "NavigationSimulator::Start should only be called once.";
  state_ = STARTED;

  if (browser_initiated_) {
    if (!SimulateBrowserInitiatedStart())
      return;
  } else {
    if (!SimulateRendererInitiatedStart())
      return;
  }

  CHECK(handle_);
  if (IsRendererDebugURL(navigation_url_))
    return;

  if (same_document_ || !IsURLHandledByNetworkStack(navigation_url_) ||
      navigation_url_.IsAboutBlank()) {
    CHECK_EQ(1, num_did_start_navigation_called_);
    return;
  }

  MaybeWaitForThrottleChecksComplete(base::BindOnce(
      &NavigationSimulator::StartComplete, weak_factory_.GetWeakPtr()));
}

void NavigationSimulator::StartComplete() {
  CHECK_EQ(1, num_did_start_navigation_called_);
  if (GetLastThrottleCheckResult().action() == NavigationThrottle::PROCEED) {
    CHECK_EQ(1, num_will_start_request_called_);
  } else {
    state_ = FAILED;
  }
}

void NavigationSimulator::Redirect(const GURL& new_url) {
  CHECK_LE(state_, STARTED) << "NavigationSimulator::Redirect should be "
                               "called before Fail or Commit";
  CHECK_EQ(0, num_did_finish_navigation_called_)
      << "NavigationSimulator::Redirect cannot be called after the "
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

  PrepareCompleteCallbackOnHandle();
  NavigationRequest* request =
      render_frame_host_->frame_tree_node()->navigation_request();
  TestNavigationURLLoader* url_loader =
      static_cast<TestNavigationURLLoader*>(request->loader_for_testing());
  CHECK(url_loader);

  net::RedirectInfo redirect_info;
  redirect_info.status_code = 302;
  redirect_info.new_method = "GET";
  redirect_info.new_url = new_url;
  redirect_info.new_site_for_cookies = new_url;
  redirect_info.new_referrer = referrer_.url.spec();
  redirect_info.new_referrer_policy =
      Referrer::ReferrerPolicyForUrlRequest(referrer_.policy);

  url_loader->CallOnRequestRedirected(
      redirect_info,
      scoped_refptr<network::ResourceResponse>(new network::ResourceResponse));

  MaybeWaitForThrottleChecksComplete(base::BindOnce(
      &NavigationSimulator::RedirectComplete, weak_factory_.GetWeakPtr(),
      previous_num_will_redirect_request_called,
      previous_did_redirect_navigation_called));
}

void NavigationSimulator::RedirectComplete(
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

void NavigationSimulator::ReadyToCommit() {
  CHECK_LE(state_, STARTED) << "NavigationSimulator::ReadyToCommit can only "
                               "be called once, and cannot be called after "
                               "NavigationSimulator::Fail";
  CHECK_EQ(0, num_did_finish_navigation_called_)
      << "NavigationSimulator::ReadyToCommit cannot be called after the "
         "navigation has finished";

  if (state_ < STARTED) {
    Start();
    if (state_ == FAILED)
      return;
  }

  PrepareCompleteCallbackOnHandle();
  if (frame_tree_node_->navigation_request()) {
    static_cast<TestRenderFrameHost*>(frame_tree_node_->current_frame_host())
        ->PrepareForCommitDeprecatedForNavigationSimulator(
            socket_address_, is_signed_exchange_inner_response_);
  }

  // Synchronous failure can cause the navigation to finish here.
  if (!handle_) {
    state_ = FAILED;
    return;
  }

  bool needs_throttle_checks = !same_document_ &&
                               !navigation_url_.IsAboutBlank() &&
                               IsURLHandledByNetworkStack(navigation_url_);
  auto complete_closure =
      base::BindOnce(&NavigationSimulator::ReadyToCommitComplete,
                     weak_factory_.GetWeakPtr(), needs_throttle_checks);
  if (needs_throttle_checks) {
    MaybeWaitForThrottleChecksComplete(std::move(complete_closure));
    return;
  }
  std::move(complete_closure).Run();
}

void NavigationSimulator::ReadyToCommitComplete(bool ran_throttles) {
  if (ran_throttles) {
    if (GetLastThrottleCheckResult().action() != NavigationThrottle::PROCEED) {
      state_ = FAILED;
      return;
    }
    CHECK_EQ(1, num_will_process_response_called_);
    CHECK_EQ(1, num_ready_to_commit_called_);
  }

  request_id_ = handle_->GetGlobalRequestID();

  // Update the RenderFrameHost now that we know which RenderFrameHost will
  // commit the navigation.
  render_frame_host_ =
      static_cast<TestRenderFrameHost*>(handle_->GetRenderFrameHost());
  state_ = READY_TO_COMMIT;
}

void NavigationSimulator::Commit() {
  CHECK_LE(state_, READY_TO_COMMIT) << "NavigationSimulator::Commit can only "
                                       "be called once, and cannot be called "
                                       "after NavigationSimulator::Fail";
  CHECK_EQ(0, num_did_finish_navigation_called_)
      << "NavigationSimulator::Commit cannot be called after the navigation "
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

  if (!same_document_) {
    render_frame_host_->SimulateCommitProcessed(handle_->GetNavigationId(),
                                                true /* was_successful */);
  }

  FrameHostMsg_DidCommitProvisionalLoad_Params params;
  params.nav_entry_id = handle_->pending_nav_entry_id();
  params.url = navigation_url_;
  params.origin = url::Origin::Create(navigation_url_);
  params.referrer = referrer_;
  params.transition = transition_;
  params.redirects.push_back(navigation_url_);
  params.should_update_history = true;
  params.did_create_new_entry = DidCreateNewEntry();
  params.gesture =
      has_user_gesture_ ? NavigationGestureUser : NavigationGestureAuto;
  params.contents_mime_type = contents_mime_type_;
  params.method = handle_->IsPost() ? "POST" : "GET";
  params.http_status_code = 200;
  params.history_list_was_cleared = false;
  params.original_request_url = navigation_url_;

  // Simulate Blink assigning an item and document sequence number to the
  // navigation.
  params.item_sequence_number = ++g_unique_identifier;
  params.document_sequence_number = ++g_unique_identifier;

  params.page_state = PageState::CreateForTestingWithSequenceNumbers(
      navigation_url_, params.item_sequence_number,
      params.document_sequence_number);

  if (same_document_)
    interface_provider_request_ = nullptr;

  render_frame_host_->SendNavigateWithParamsAndInterfaceProvider(
      &params, std::move(interface_provider_request_), same_document_);

  // Simulate the UnloadACK in the old RenderFrameHost if it was swapped out at
  // commit time.
  if (previous_rfh != render_frame_host_) {
    previous_rfh->OnMessageReceived(
        FrameHostMsg_SwapOut_ACK(previous_rfh->GetRoutingID()));
  }

  state_ = FINISHED;

  if (!IsRendererDebugURL(navigation_url_))
    CHECK_EQ(1, num_did_finish_navigation_called_);
}

void NavigationSimulator::AbortCommit() {
  CHECK_LE(state_, FAILED)
      << "NavigationSimulator::AbortCommit cannot be called after "
         "NavigationSimulator::Commit or  "
         "NavigationSimulator::CommitErrorPage.";
  if (state_ < READY_TO_COMMIT) {
    ReadyToCommit();
    if (state_ == FINISHED)
      return;
  }

  CHECK(render_frame_host_) << "NavigationSimulator::AbortCommit can only be "
                               "called for navigations that commit.";
  render_frame_host_->SimulateCommitProcessed(handle_->GetNavigationId(),
                                              false /* was_successful */);

  state_ = FINISHED;
  CHECK_EQ(1, num_did_finish_navigation_called_);
}

void NavigationSimulator::FailWithResponseHeaders(
    int error_code,
    scoped_refptr<net::HttpResponseHeaders> response_headers) {
  CHECK_LE(state_, STARTED) << "NavigationSimulator::Fail can only be "
                               "called once, and cannot be called after "
                               "NavigationSimulator::ReadyToCommit";
  CHECK_EQ(0, num_did_finish_navigation_called_)
      << "NavigationSimulator::Fail cannot be called after the "
         "navigation has finished";
  DCHECK(!IsRendererDebugURL(navigation_url_));

  if (state_ == INITIALIZATION)
    Start();

  DCHECK(!handle_->GetResponseHeaders());
  static_cast<NavigationHandleImpl*>(handle_)->set_response_headers_for_testing(
      response_headers);

  state_ = FAILED;

  PrepareCompleteCallbackOnHandle();
  NavigationRequest* request = frame_tree_node_->navigation_request();
  CHECK(request);
  TestNavigationURLLoader* url_loader =
      static_cast<TestNavigationURLLoader*>(request->loader_for_testing());
  CHECK(url_loader);
  url_loader->SimulateError(error_code);

  auto complete_closure =
      base::BindOnce(&NavigationSimulator::FailComplete,
                     weak_factory_.GetWeakPtr(), error_code);
  if (error_code != net::ERR_ABORTED) {
    MaybeWaitForThrottleChecksComplete(std::move(complete_closure));
    return;
  }
  std::move(complete_closure).Run();
}

void NavigationSimulator::Fail(int error_code) {
  FailWithResponseHeaders(error_code, nullptr);
}

void NavigationSimulator::FailComplete(int error_code) {
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
    CHECK_EQ(1, num_ready_to_commit_called_);
    CHECK_EQ(0, num_did_finish_navigation_called_);
    // Update the RenderFrameHost now that we know which RenderFrameHost will
    // commit the error page.
    render_frame_host_ =
        static_cast<TestRenderFrameHost*>(handle_->GetRenderFrameHost());
  }
}

void NavigationSimulator::CommitErrorPage() {
  CHECK_EQ(FAILED, state_)
      << "NavigationSimulator::CommitErrorPage can only be "
         "called once, and should be called after Fail "
         "has been called";
  CHECK_EQ(0, num_did_finish_navigation_called_)
      << "NavigationSimulator::CommitErrorPage cannot be called after the "
         "navigation has finished";

  // Keep a pointer to the current RenderFrameHost that may be pending deletion
  // after commit.
  RenderFrameHostImpl* previous_rfh =
      render_frame_host_->frame_tree_node()->current_frame_host();

  render_frame_host_->SimulateCommitProcessed(handle_->GetNavigationId(),
                                              true /* was_successful */);

  FrameHostMsg_DidCommitProvisionalLoad_Params params;
  params.nav_entry_id = handle_->pending_nav_entry_id();
  params.did_create_new_entry = DidCreateNewEntry();
  params.url = navigation_url_;
  params.referrer = referrer_;
  params.transition = transition_;
  params.url_is_unreachable = true;

  // Simulate Blink assigning an item and document sequence number to the
  // navigation.
  params.item_sequence_number = ++g_unique_identifier;
  params.document_sequence_number = ++g_unique_identifier;

  params.page_state = PageState::CreateForTestingWithSequenceNumbers(
      navigation_url_, params.item_sequence_number,
      params.document_sequence_number);

  render_frame_host_->SendNavigateWithParamsAndInterfaceProvider(
      &params, std::move(interface_provider_request_),
      false /* was_same_document */);

  // Simulate the UnloadACK in the old RenderFrameHost if it was swapped out at
  // commit time.
  if (previous_rfh != render_frame_host_) {
    previous_rfh->OnMessageReceived(
        FrameHostMsg_SwapOut_ACK(previous_rfh->GetRoutingID()));
  }

  state_ = FINISHED;

  CHECK_EQ(1, num_did_finish_navigation_called_);
}

void NavigationSimulator::CommitSameDocument() {
  if (!browser_initiated_) {
    CHECK_EQ(INITIALIZATION, state_)
        << "NavigationSimulator::CommitSameDocument should be the only "
           "navigation event function called on the NavigationSimulator";
  } else {
    CHECK(same_document_);
    CHECK_EQ(STARTED, state_);
  }

  FrameHostMsg_DidCommitProvisionalLoad_Params params;
  params.nav_entry_id = 0;
  params.url = navigation_url_;
  params.origin = url::Origin::Create(navigation_url_);
  params.referrer = referrer_;
  params.transition = transition_;
  params.should_update_history = true;
  params.did_create_new_entry = false;
  params.gesture =
      has_user_gesture_ ? NavigationGestureUser : NavigationGestureAuto;
  params.contents_mime_type = contents_mime_type_;
  params.method = "GET";
  params.http_status_code = 200;
  params.history_list_was_cleared = false;
  params.original_request_url = navigation_url_;
  params.page_state =
      PageState::CreateForTesting(navigation_url_, false, nullptr, nullptr);

  interface_provider_request_ = nullptr;
  render_frame_host_->SendNavigateWithParamsAndInterfaceProvider(
      &params, nullptr /* interface_provider_request */, true);

  state_ = FINISHED;

  CHECK_EQ(1, num_did_start_navigation_called_);
  CHECK_EQ(0, num_will_start_request_called_);
  CHECK_EQ(0, num_will_process_response_called_);
  CHECK_EQ(0, num_ready_to_commit_called_);
  CHECK_EQ(1, num_did_finish_navigation_called_);
}

void NavigationSimulator::SetTransition(ui::PageTransition transition) {
  CHECK_EQ(INITIALIZATION, state_)
      << "The transition cannot be set after the navigation has started";
  CHECK_EQ(ReloadType::NONE, reload_type_)
      << "The transition cannot be specified for reloads";
  CHECK_EQ(0, session_history_offset_)
      << "The transition cannot be specified for back/forward navigations";
  transition_ = transition;
}

void NavigationSimulator::SetHasUserGesture(bool has_user_gesture) {
  CHECK_EQ(INITIALIZATION, state_) << "The has_user_gesture parameter cannot "
                                      "be set after the navigation has started";
  has_user_gesture_ = has_user_gesture;
}

void NavigationSimulator::SetReloadType(ReloadType reload_type) {
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

void NavigationSimulator::SetMethod(const std::string& method) {
  CHECK_EQ(INITIALIZATION, state_) << "The method parameter cannot "
                                      "be set after the navigation has started";
  initial_method_ = method;
}

void NavigationSimulator::SetReferrer(const Referrer& referrer) {
  CHECK_LE(state_, STARTED) << "The referrer cannot be set after the "
                               "navigation has committed or has failed";
  referrer_ = referrer;
}

void NavigationSimulator::SetSocketAddress(
    const net::HostPortPair& socket_address) {
  CHECK_LE(state_, STARTED) << "The socket address cannot be set after the "
                               "navigation has committed or failed";
  socket_address_ = socket_address;
}

void NavigationSimulator::SetIsSignedExchangeInnerResponse(
    bool is_signed_exchange_inner_response) {
  CHECK_LE(state_, STARTED) << "The signed exchange flag cannot be set after "
                               "the navigation has committed or failed";
  is_signed_exchange_inner_response_ = is_signed_exchange_inner_response;
}

void NavigationSimulator::SetInterfaceProviderRequest(
    service_manager::mojom::InterfaceProviderRequest request) {
  CHECK_LE(state_, STARTED) << "The InterfaceProviderRequest cannot be set "
                               "after the navigation has committed or failed";
  CHECK(request.is_pending());
  interface_provider_request_ = std::move(request);
}

void NavigationSimulator::SetContentsMimeType(
    const std::string& contents_mime_type) {
  CHECK_LE(state_, STARTED) << "The contents mime type cannot be set after the "
                               "navigation has committed or failed";
  contents_mime_type_ = contents_mime_type;
}

void NavigationSimulator::SetAutoAdvance(bool auto_advance) {
  auto_advance_ = auto_advance;
}

NavigationThrottle::ThrottleCheckResult
NavigationSimulator::GetLastThrottleCheckResult() {
  return last_throttle_check_result_.value();
}

NavigationHandle* NavigationSimulator::GetNavigationHandle() const {
  CHECK_EQ(STARTED, state_);
  return handle_;
}

content::GlobalRequestID NavigationSimulator::GetGlobalRequestID() const {
  CHECK_GT(state_, STARTED) << "The GlobalRequestID is not available until "
                               "after the navigation has completed "
                               "WillProcessResponse";
  return request_id_;
}

void NavigationSimulator::DidStartNavigation(
    NavigationHandle* navigation_handle) {
  // Check if this navigation is the one we're simulating.
  if (handle_)
    return;

  NavigationHandleImpl* handle =
      static_cast<NavigationHandleImpl*>(navigation_handle);

  if (handle->frame_tree_node() != frame_tree_node_)
    return;

  handle_ = handle;

  num_did_start_navigation_called_++;

  // Add a throttle to count NavigationThrottle calls count.
  RegisterTestThrottle(handle);
  PrepareCompleteCallbackOnHandle();
}

void NavigationSimulator::DidRedirectNavigation(
    NavigationHandle* navigation_handle) {
  if (navigation_handle == handle_)
    num_did_redirect_navigation_called_++;
}

void NavigationSimulator::ReadyToCommitNavigation(
    NavigationHandle* navigation_handle) {
  if (navigation_handle == handle_)
    num_ready_to_commit_called_++;
}

void NavigationSimulator::DidFinishNavigation(
    NavigationHandle* navigation_handle) {
  if (navigation_handle == handle_) {
    num_did_finish_navigation_called_++;
    handle_ = nullptr;
  }
}

void NavigationSimulator::OnWillStartRequest() {
  num_will_start_request_called_++;
}

void NavigationSimulator::OnWillRedirectRequest() {
  num_will_redirect_request_called_++;
}

void NavigationSimulator::OnWillFailRequest() {
  num_will_fail_request_called_++;
}

void NavigationSimulator::OnWillProcessResponse() {
  num_will_process_response_called_++;
}

bool NavigationSimulator::SimulateBrowserInitiatedStart() {
  if (reload_type_ != ReloadType::NONE) {
    web_contents_->GetController().Reload(reload_type_,
                                          false /*check_for_repost */);
  } else if (session_history_offset_) {
    web_contents_->GetController().GoToOffset(session_history_offset_);
  } else {
    web_contents_->GetController().LoadURL(navigation_url_, referrer_,
                                           transition_, std::string());
  }

  // The navigation url might have been rewritten by the NavigationController.
  // Update it.
  navigation_url_ = web_contents_->GetController().GetPendingEntry()->GetURL();

  // Simulate the BeforeUnload ACK if needed.
  NavigationRequest* request = frame_tree_node_->navigation_request();
  if (request &&
      request->state() == NavigationRequest::WAITING_FOR_RENDERER_RESPONSE) {
    static_cast<TestRenderFrameHost*>(web_contents_->GetMainFrame())
        ->SendBeforeUnloadACK(true /*proceed */);
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
      DCHECK(!handle_);
      DCHECK(web_contents_->GetMainFrame()->is_loading());

      // A navigation to a renderer-debug URL cannot commit. Simulate the
      // renderer process aborting it.
      web_contents_->GetMainFrame()->OnMessageReceived(
          FrameHostMsg_DidStopLoading(
              web_contents_->GetMainFrame()->GetRoutingID()));
      state_ = FAILED;
      return false;
    } else if (handle_ &&
               web_contents_->GetMainFrame()->GetNavigationHandle() ==
                   handle_) {
      DCHECK(!IsURLHandledByNetworkStack(handle_->GetURL()));
      return true;
    } else if (web_contents_->GetMainFrame()
                   ->same_document_navigation_request() &&
               web_contents_->GetMainFrame()
                       ->same_document_navigation_request()
                       ->navigation_handle() == handle_) {
      DCHECK(handle_->IsSameDocument());
      same_document_ = true;
      return true;
    }
    return false;
  }

  DCHECK_EQ(handle_, request->navigation_handle());
  return true;
}

bool NavigationSimulator::SimulateRendererInitiatedStart() {
  mojom::BeginNavigationParamsPtr begin_params =
      mojom::BeginNavigationParams::New(
          std::string() /* headers */, net::LOAD_NORMAL,
          false /* skip_service_worker */,
          blink::mojom::RequestContextType::HYPERLINK,
          blink::WebMixedContentContextType::kBlockable,
          false /* is_form_submission */, GURL() /* searchable_form_url */,
          std::string() /* searchable_form_encoding */, url::Origin(),
          GURL() /* client_side_redirect_url */,
          base::nullopt /* detools_initiator_info */);
  CommonNavigationParams common_params;
  common_params.url = navigation_url_;
  common_params.method = initial_method_;
  common_params.referrer = referrer_;
  common_params.transition = transition_;
  common_params.navigation_type =
      PageTransitionCoreTypeIs(transition_, ui::PAGE_TRANSITION_RELOAD)
          ? FrameMsg_Navigate_Type::RELOAD
          : FrameMsg_Navigate_Type::DIFFERENT_DOCUMENT;
  common_params.has_user_gesture = has_user_gesture_;

  if (IsPerNavigationMojoInterfaceEnabled()) {
    mojom::NavigationClientAssociatedPtr navigation_client_ptr;
    StoreNavigationClientRequest(
        mojo::MakeRequestAssociatedWithDedicatedPipe(&navigation_client_ptr));
    render_frame_host_->frame_host_binding_for_testing()
        .impl()
        ->BeginNavigation(common_params, std::move(begin_params), nullptr,
                          navigation_client_ptr.PassInterface(), nullptr);
  } else {
    render_frame_host_->frame_host_binding_for_testing()
        .impl()
        ->BeginNavigation(common_params, std::move(begin_params), nullptr,
                          nullptr, nullptr);
  }

  NavigationRequest* request =
      render_frame_host_->frame_tree_node()->navigation_request();

  // The request failed synchronously.
  if (!request)
    return false;

  DCHECK_EQ(handle_, request->navigation_handle());
  return true;
}

void NavigationSimulator::MaybeWaitForThrottleChecksComplete(
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

void NavigationSimulator::Wait() {
  DCHECK(!wait_closure_);
  if (!IsDeferred())
    return;
  base::RunLoop run_loop;
  wait_closure_ = run_loop.QuitClosure();
  run_loop.Run();
}

void NavigationSimulator::OnThrottleChecksComplete(
    NavigationThrottle::ThrottleCheckResult result) {
  DCHECK(!last_throttle_check_result_);
  last_throttle_check_result_ = result;
  if (wait_closure_)
    std::move(wait_closure_).Run();
  if (throttle_checks_complete_closure_)
    std::move(throttle_checks_complete_closure_).Run();
}

void NavigationSimulator::PrepareCompleteCallbackOnHandle() {
  DCHECK(handle_);
  last_throttle_check_result_.reset();
  handle_->set_complete_callback_for_testing(
      base::Bind(&NavigationSimulator::OnThrottleChecksComplete,
                 weak_factory_.GetWeakPtr()));
}

RenderFrameHost* NavigationSimulator::GetFinalRenderFrameHost() {
  CHECK_GE(state_, READY_TO_COMMIT);
  return render_frame_host_;
}

bool NavigationSimulator::IsDeferred() {
  return !throttle_checks_complete_closure_.is_null();
}

bool NavigationSimulator::CheckIfSameDocument() {
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

bool NavigationSimulator::DidCreateNewEntry() {
  if (ui::PageTransitionCoreTypeIs(transition_,
                                   ui::PAGE_TRANSITION_AUTO_SUBFRAME))
    return false;
  if (reload_type_ != ReloadType::NONE)
    return false;
  if (session_history_offset_)
    return false;

  return true;
}

void NavigationSimulator::SetSessionHistoryOffset(int session_history_offset) {
  CHECK(session_history_offset);
  session_history_offset_ = session_history_offset;
  transition_ =
      ui::PageTransitionFromInt(transition_ | ui::PAGE_TRANSITION_FORWARD_BACK);
}

void NavigationSimulator::StoreNavigationClientRequest(
    mojom::NavigationClientAssociatedRequest navigation_client_request) {
  navigation_client_impl_.reset(
      new MockNavigationClientImpl(std::move(navigation_client_request)));
}

}  // namespace content
