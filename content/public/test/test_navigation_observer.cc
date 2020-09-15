// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/test_navigation_observer.h"

#include "base/bind.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {

class TestNavigationObserver::TestWebContentsObserver
    : public WebContentsObserver {
 public:
  TestWebContentsObserver(TestNavigationObserver* parent,
                          WebContents* web_contents)
      : WebContentsObserver(web_contents),
        parent_(parent) {
  }

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
    if (!navigation_handle->HasCommitted())
      return;

    parent_->OnDidFinishNavigation(navigation_handle);
  }

  TestNavigationObserver* parent_;

  DISALLOW_COPY_AND_ASSIGN(TestWebContentsObserver);
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
    int number_of_navigations,
    MessageLoopRunner::QuitMode quit_mode)
    : TestNavigationObserver(web_contents,
                             number_of_navigations,
                             base::nullopt /* target_url */,
                             base::nullopt /* target_error */,
                             quit_mode) {}

TestNavigationObserver::TestNavigationObserver(
    WebContents* web_contents,
    MessageLoopRunner::QuitMode quit_mode)
    : TestNavigationObserver(web_contents, 1, quit_mode) {}

TestNavigationObserver::TestNavigationObserver(
    const GURL& target_url,
    MessageLoopRunner::QuitMode quit_mode)
    : TestNavigationObserver(nullptr,
                             1 /* num_of_navigations */,
                             target_url,
                             base::nullopt /* target_error */,
                             quit_mode) {}

TestNavigationObserver::TestNavigationObserver(
    WebContents* web_contents,
    net::Error target_error,
    MessageLoopRunner::QuitMode quit_mode)
    : TestNavigationObserver(web_contents,
                             1 /* num_of_navigations */,
                             base::nullopt,
                             target_error,
                             quit_mode) {}

TestNavigationObserver::~TestNavigationObserver() {
  StopWatchingNewWebContents();
}

void TestNavigationObserver::Wait() {
  message_loop_runner_->Run();
}

void TestNavigationObserver::WaitForNavigationFinished() {
  wait_event_ = WaitEvent::kNavigationFinished;
  message_loop_runner_->Run();
}

void TestNavigationObserver::StartWatchingNewWebContents() {
  WebContentsImpl::FriendWrapper::AddCreatedCallbackForTesting(
      web_contents_created_callback_);
}

void TestNavigationObserver::StopWatchingNewWebContents() {
  WebContentsImpl::FriendWrapper::RemoveCreatedCallbackForTesting(
      web_contents_created_callback_);
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
    int number_of_navigations,
    const base::Optional<GURL>& target_url,
    base::Optional<net::Error> target_error,
    MessageLoopRunner::QuitMode quit_mode)
    : wait_event_(WaitEvent::kLoadStopped),
      navigations_completed_(0),
      number_of_navigations_(number_of_navigations),
      target_url_(target_url),
      target_error_(target_error),
      last_navigation_succeeded_(false),
      last_net_error_code_(net::OK),
      last_navigation_type_(NAVIGATION_TYPE_UNKNOWN),
      message_loop_runner_(new MessageLoopRunner(quit_mode)),
      web_contents_created_callback_(
          base::BindRepeating(&TestNavigationObserver::OnWebContentsCreated,
                              base::Unretained(this))) {
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
  DCHECK(web_contents_state_iter != web_contents_state_.end());
  DCHECK_EQ(web_contents_state_iter->second.observer.get(), observer);

  web_contents_state_.erase(web_contents_state_iter);
}

void TestNavigationObserver::OnNavigationEntryCommitted(
    TestWebContentsObserver* observer,
    WebContents* web_contents,
    const LoadCommittedDetails& load_details) {
  WebContentsState* web_contents_state = GetWebContentsState(web_contents);
  web_contents_state->navigation_started = true;
  web_contents_state->last_navigation_matches_filter = false;
}

void TestNavigationObserver::OnDidStartLoading(WebContents* web_contents) {
  WebContentsState* web_contents_state = GetWebContentsState(web_contents);
  web_contents_state->navigation_started = true;
  web_contents_state->last_navigation_matches_filter = false;
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
  if (target_url_.has_value() &&
      target_url_.value() != navigation_handle->GetURL()) {
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
  if (target_url_.has_value() &&
      target_url_.value() != navigation_handle->GetURL()) {
    return;
  }
  if (target_error_.has_value() &&
      target_error_.value() != navigation_handle->GetNetErrorCode()) {
    return;
  }

  WebContentsState* web_contents_state =
      GetWebContentsState(navigation_handle->GetWebContents());
  if (!web_contents_state->navigation_started)
    return;
  if (HasFilter())
    web_contents_state->last_navigation_matches_filter = true;

  NavigationRequest* request = NavigationRequest::From(navigation_handle);

  last_navigation_url_ = navigation_handle->GetURL();
  last_navigation_initiator_origin_ = request->common_params().initiator_origin;
  last_initiator_routing_id_ = navigation_handle->GetInitiatorRoutingId();
  last_navigation_succeeded_ = !navigation_handle->IsErrorPage();
  last_net_error_code_ = navigation_handle->GetNetErrorCode();
  last_navigation_type_ = request->navigation_type();

  if (wait_event_ == WaitEvent::kNavigationFinished)
    EventTriggered(web_contents_state);
}

void TestNavigationObserver::EventTriggered(
    WebContentsState* web_contents_state) {
  if (HasFilter() && !web_contents_state->last_navigation_matches_filter)
    return;

  DCHECK_GE(navigations_completed_, 0);
  ++navigations_completed_;
  if (navigations_completed_ != number_of_navigations_) {
    return;
  }

  web_contents_state->navigation_started = false;
  message_loop_runner_->Quit();
}

bool TestNavigationObserver::HasFilter() {
  return target_url_.has_value() || target_error_.has_value();
}

TestNavigationObserver::WebContentsState*
TestNavigationObserver::GetWebContentsState(WebContents* web_contents) {
  auto web_contents_state_iter = web_contents_state_.find(web_contents);
  DCHECK(web_contents_state_iter != web_contents_state_.end());
  return &(web_contents_state_iter->second);
}

}  // namespace content
