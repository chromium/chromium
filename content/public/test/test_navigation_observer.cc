// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/test_navigation_observer.h"

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_url_handler.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_test_utils.h"

namespace content {

class TestNavigationObserver::TestWebContentsObserver
    : public WebContentsObserver {
 public:
  TestWebContentsObserver(TestNavigationObserver* parent,
                          WebContents* web_contents)
      : WebContentsObserver(web_contents),
        parent_(parent) {
  }

  TestWebContentsObserver(const TestWebContentsObserver&) = delete;
  TestWebContentsObserver& operator=(const TestWebContentsObserver&) = delete;

 private:
  // WebContentsObserver:
  void NavigationEntryCommitted(
      const LoadCommittedDetails& load_details) override {
    parent_->OnNavigationEntryCommitted(this, web_contents(), load_details);
  }

  void WebContentsDestroyed() override {
    parent_->OnWebContentsDestroyed(this, web_contents());
  }

  void DidStartLoading() override {
    parent_->OnDidStartLoading(web_contents());
  }

  void DidStopLoading() override {
    parent_->OnDidStopLoading(web_contents());
  }

  void DidStartNavigation(NavigationHandle* navigation_handle) override {
    if (navigation_handle->IsSameDocument())
      return;

    parent_->OnDidStartNavigation(navigation_handle);
  }

  void DidFinishNavigation(NavigationHandle* navigation_handle) override {
    parent_->OnDidFinishNavigation(navigation_handle);
  }

  raw_ptr<TestNavigationObserver> parent_;
};

TestNavigationObserver::WebContentsState::WebContentsState() = default;
TestNavigationObserver::WebContentsState::WebContentsState(
    WebContentsState&& other) = default;
TestNavigationObserver::WebContentsState&
TestNavigationObserver::WebContentsState::operator=(WebContentsState&& other) =
    default;
TestNavigationObserver::WebContentsState::~WebContentsState() = default;

TestNavigationObserver::TestNavigationObserver(
    WebContents* web_contents,
    int expected_number_of_navigations,
    MessageLoopRunner::QuitMode quit_mode,
    bool ignore_uncommitted_navigations)
    : TestNavigationObserver(web_contents,
                             expected_number_of_navigations,
                             std::nullopt /* target_url */,
                             std::nullopt /* target_error */,
                             quit_mode,
                             ignore_uncommitted_navigations) {}

TestNavigationObserver::TestNavigationObserver(
    WebContents* web_contents,
    MessageLoopRunner::QuitMode quit_mode,
    bool ignore_uncommitted_navigations)
    : TestNavigationObserver(web_contents,
                             1,
                             quit_mode,
                             ignore_uncommitted_navigations) {}

TestNavigationObserver::TestNavigationObserver(
    WebContents* web_contents,
    net::Error expected_target_error,
    MessageLoopRunner::QuitMode quit_mode,
    bool ignore_uncommitted_navigations)
    : TestNavigationObserver(web_contents,
                             1 /* num_of_navigations */,
                             std::nullopt,
                             expected_target_error,
                             quit_mode,
                             ignore_uncommitted_navigations) {}

TestNavigationObserver::TestNavigationObserver(
    const GURL& expected_target_url,
    MessageLoopRunner::QuitMode quit_mode,
    bool ignore_uncommitted_navigations)
    : TestNavigationObserver(nullptr,
                             1 /* num_of_navigations */,
                             expected_target_url,
                             std::nullopt /* target_error */,
                             quit_mode,
                             ignore_uncommitted_navigations) {}

TestNavigationObserver::~TestNavigationObserver() = default;

void TestNavigationObserver::Wait() {
  was_event_consumed_ = false;
  TRACE_EVENT1("test", "TestNavigationObserver::Wait", "params",
               [&](perfetto::TracedValue ctx) {
                 // TODO(crbug.com/40751990): Replace this with passing more
                 // parameters to TRACE_EVENT directly when available.
                 auto dict = std::move(ctx).WriteDictionary();
                 dict.Add("wait_event", wait_event_);
                 dict.Add("ignore_uncommitted_navigations",
                          ignore_uncommitted_navigations_);
                 dict.Add("expected_target_url", expected_target_url_);
                 dict.Add("expected_initial_url", expected_initial_url_);
                 dict.Add("expected_target_error", expected_target_error_);
               });
  message_loop_runner_->Run();
}

void TestNavigationObserver::WaitForNavigationFinished() {
  wait_event_ = WaitEvent::kNavigationFinished;
  Wait();
}

void TestNavigationObserver::StartWatchingNewWebContents() {
  creation_subscription_ = RegisterWebContentsCreationCallback(
      base::BindRepeating(&TestNavigationObserver::OnWebContentsCreated,
                          base::Unretained(this)));
}

void TestNavigationObserver::StopWatchingNewWebContents() {
  creation_subscription_ = base::CallbackListSubscription();
}

void TestNavigationObserver::WatchExistingWebContents() {
  for (auto* web_contents : WebContentsImpl::GetAllWebContents())
    RegisterAsObserver(web_contents);
}

void TestNavigationObserver::RegisterAsObserver(WebContents* web_contents) {
  web_contents_state_[web_contents].observer =
      std::make_unique<TestWebContentsObserver>(this, web_contents);
}

TestNavigationObserver::TestNavigationObserver(
    WebContents* web_contents,
    int expected_number_of_navigations,
    const std::optional<GURL>& expected_target_url,
    std::optional<net::Error> expected_target_error,
    MessageLoopRunner::QuitMode quit_mode,
    bool ignore_uncommitted_navigations)
    : wait_event_(WaitEvent::kLoadStopped),
      navigations_completed_(0),
      expected_number_of_navigations_(expected_number_of_navigations),
      expected_target_url_(expected_target_url),
      expected_initial_url_(std::nullopt),
      expected_target_error_(expected_target_error),
      ignore_uncommitted_navigations_(ignore_uncommitted_navigations),
      last_navigation_succeeded_(false),
      last_net_error_code_(net::OK),
      message_loop_runner_(new MessageLoopRunner(quit_mode)) {
  if (web_contents)
    RegisterAsObserver(web_contents);
}

void TestNavigationObserver::OnWebContentsCreated(WebContents* web_contents) {
  RegisterAsObserver(web_contents);
}

void TestNavigationObserver::OnWebContentsDestroyed(
    TestWebContentsObserver* observer,
    WebContents* web_contents) {
  auto web_contents_state_iter = web_contents_state_.find(web_contents);
  CHECK(web_contents_state_iter != web_contents_state_.end());
  DCHECK_EQ(web_contents_state_iter->second.observer.get(), observer);

  web_contents_state_.erase(web_contents_state_iter);
}

void TestNavigationObserver::OnNavigationEntryCommitted(
    TestWebContentsObserver* observer,
    WebContents* web_contents,
    const LoadCommittedDetails& load_details) {
  WebContentsState* web_contents_state = GetWebContentsState(web_contents);
  web_contents_state->navigation_started = true;
}

void TestNavigationObserver::OnDidStartLoading(WebContents* web_contents) {
  WebContentsState* web_contents_state = GetWebContentsState(web_contents);
  web_contents_state->navigation_started = true;
}

void TestNavigationObserver::OnDidStopLoading(WebContents* web_contents) {
  WebContentsState* web_contents_state = GetWebContentsState(web_contents);
  if (!web_contents_state->navigation_started)
    return;

  if (wait_event_ == WaitEvent::kLoadStopped)
    EventTriggered(web_contents_state);
}

void TestNavigationObserver::OnDidStartNavigation(
    NavigationHandle* navigation_handle) {
  if (expected_target_url_.has_value() &&
      expected_target_url_.value() != navigation_handle->GetURL()) {
    return;
  }
  if (!DoesNavigationMatchExpectedInitialUrl(
          NavigationRequest::From(navigation_handle))) {
    return;
  }

  WebContentsState* web_contents_state =
      GetWebContentsState(navigation_handle->GetWebContents());
  if (!web_contents_state->navigation_started)
    return;

  last_navigation_succeeded_ = false;
}

void TestNavigationObserver::OnDidFinishNavigation(
    NavigationHandle* navigation_handle) {
  if (ignore_uncommitted_navigations_ && !navigation_handle->HasCommitted())
    return;

  NavigationRequest* request = NavigationRequest::From(navigation_handle);
  if (expected_target_url_.has_value() &&
      expected_target_url_.value() != navigation_handle->GetURL()) {
    return;
  }
  if (!DoesNavigationMatchExpectedInitialUrl(request))
    return;
  if (expected_target_error_.has_value() &&
      expected_target_error_.value() != navigation_handle->GetNetErrorCode()) {
    return;
  }

  WebContentsState* web_contents_state =
      GetWebContentsState(navigation_handle->GetWebContents());

  // TODO(crbug.com/40191691): It is generally the case that we've received load
  // started events by this point, but we don't send load events for prerendered
  // pages (by design). It's also the case that frame tree nodes don't report
  // load start if the tree is already loading. For all of prerendering,
  // subframes and fenced frames (i.e., the cases where we cannot rely on
  // navigation_started being set correctly), we're not in the primary main
  // frame, so the DCHECK has been updated to ignore these cases. We also only
  // enforce this check if we haven't already called EventTriggered (since this
  // will reset navigation_started and can cause errors in subsequent
  // DidFinishNavigation calls).
  DCHECK(was_event_consumed_ || !navigation_handle->IsInPrimaryMainFrame() ||
         web_contents_state->navigation_started);

  if (HasFilter())
    web_contents_state->last_navigation_matches_filter = true;

  last_navigation_url_ = navigation_handle->GetURL();
  last_navigation_initiator_origin_ = request->common_params().initiator_origin;
  last_initiator_frame_token_ = navigation_handle->GetInitiatorFrameToken();
  last_initiator_process_id_ = navigation_handle->GetInitiatorProcessId();
  last_navigation_succeeded_ =
      navigation_handle->HasCommitted() && !navigation_handle->IsErrorPage();
  last_navigation_initiator_activation_and_ad_status_ =
      navigation_handle->GetNavigationInitiatorActivationAndAdStatus();
  last_net_error_code_ = navigation_handle->GetNetErrorCode();
  if (auto* headers = navigation_handle->GetResponseHeaders(); !!headers) {
    last_http_response_code_ =
        static_cast<net::HttpStatusCode>(headers->response_code());
  } else {
    last_http_response_code_ = std::nullopt;
  }
  last_nav_entry_id_ =
      NavigationRequest::From(navigation_handle)->nav_entry_id();
  last_source_site_instance_ = navigation_handle->GetSourceSiteInstance();
  next_page_ukm_source_id_ = navigation_handle->GetNextPageUkmSourceId();

  // Allow extending classes to fetch data available via navigation_handle.
  NavigationOfInterestDidFinish(navigation_handle);

  if (wait_event_ == WaitEvent::kNavigationFinished)
    EventTriggered(web_contents_state);
}

void TestNavigationObserver::NavigationOfInterestDidFinish(NavigationHandle*) {
  // Nothing in the base class.
}

void TestNavigationObserver::EventTriggered(
    WebContentsState* web_contents_state) {
  if (HasFilter() && !web_contents_state->last_navigation_matches_filter)
    return;

  DCHECK_GE(navigations_completed_, 0);
  ++navigations_completed_;
  if (navigations_completed_ != expected_number_of_navigations_) {
    return;
  }

  was_event_consumed_ = true;
  web_contents_state->navigation_started = false;
  message_loop_runner_->Quit();
}

bool TestNavigationObserver::DoesNavigationMatchExpectedInitialUrl(
    NavigationRequest* navigation_request) {
  if (!expected_initial_url_.has_value())
    return true;

  // Find the real URL being navigated to (e.g. stripping the "view-source:"
  // prefix if necessary).
  GURL expected_url = *expected_initial_url_;
  BrowserContext* browser_context = navigation_request->frame_tree_node()
                                        ->navigator()
                                        .controller()
                                        .GetBrowserContext();
  BrowserURLHandler::GetInstance()->RewriteURLIfNecessary(&expected_url,
                                                          browser_context);

  // Debug URLs do not go through NavigationRequest and therefore cannot be used
  // as an `expected_url`.
  DCHECK(!blink::IsRendererDebugURL(expected_url));

  GURL actual_url = navigation_request->GetOriginalRequestURL();
  return actual_url == expected_url;
}

bool TestNavigationObserver::HasFilter() {
  return expected_target_url_.has_value() ||
         expected_initial_url_.has_value() ||
         expected_target_error_.has_value();
}

TestNavigationObserver::WebContentsState*
TestNavigationObserver::GetWebContentsState(WebContents* web_contents) {
  auto web_contents_state_iter = web_contents_state_.find(web_contents);
  CHECK(web_contents_state_iter != web_contents_state_.end());
  return &(web_contents_state_iter->second);
}

}  // namespace content
