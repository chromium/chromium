// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/public/test/fakes/test_web_state_observer.h"

#include <memory>

#include "ios/web/navigation/navigation_context_impl.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#include "ios/web/public/security/ssl_status.h"
#import "ios/web/public/web_state.h"
#include "net/http/http_response_headers.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

TestWebStateObserver::TestWebStateObserver(WebState* web_state)
    : web_state_(web_state) {
  web_state_->AddObserver(this);
}

TestWebStateObserver::~TestWebStateObserver() {
  if (web_state_) {
    web_state_->RemoveObserver(this);
    web_state_ = nullptr;
  }
}

void TestWebStateObserver::WasShown(WebState* web_state) {
  ASSERT_EQ(web_state_, web_state);
  was_shown_info_ = std::make_unique<web::TestWasShownInfo>();
  was_shown_info_->web_state = web_state;
}

void TestWebStateObserver::WasHidden(WebState* web_state) {
  ASSERT_EQ(web_state_, web_state);
  was_hidden_info_ = std::make_unique<web::TestWasHiddenInfo>();
  was_hidden_info_->web_state = web_state;
}

void TestWebStateObserver::PageLoaded(
    WebState* web_state,
    PageLoadCompletionStatus load_completion_status) {
  ASSERT_EQ(web_state_, web_state);
  load_page_info_ = std::make_unique<web::TestLoadPageInfo>();
  load_page_info_->web_state = web_state;
  load_page_info_->success =
      load_completion_status == PageLoadCompletionStatus::SUCCESS;
}

void TestWebStateObserver::LoadProgressChanged(WebState* web_state,
                                               double progress) {
  ASSERT_EQ(web_state_, web_state);
  change_loading_progress_info_ =
      std::make_unique<web::TestChangeLoadingProgressInfo>();
  change_loading_progress_info_->web_state = web_state;
  change_loading_progress_info_->progress = progress;
}

void TestWebStateObserver::NavigationItemsPruned(WebState* web_state,
                                                 size_t pruned_item_count) {
  ASSERT_EQ(web_state_, web_state);
  navigation_items_pruned_info_ =
      std::make_unique<web::TestNavigationItemsPrunedInfo>();
  navigation_items_pruned_info_->web_state = web_state;
  navigation_items_pruned_info_->count = pruned_item_count;
}

void TestWebStateObserver::DidStartNavigation(WebState* web_state,
                                              NavigationContext* navigation) {
  ASSERT_EQ(web_state_, web_state);
  ASSERT_TRUE(!navigation->GetError() || !navigation->IsSameDocument());
  did_start_navigation_info_ =
      std::make_unique<web::TestDidStartNavigationInfo>();
  did_start_navigation_info_->web_state = web_state;
  std::unique_ptr<web::NavigationContextImpl> context =
      web::NavigationContextImpl::CreateNavigationContext(
          navigation->GetWebState(), navigation->GetUrl(),
          navigation->HasUserGesture(), navigation->GetPageTransition(),
          navigation->IsRendererInitiated());
  context->SetIsSameDocument(navigation->IsSameDocument());
  context->SetError(navigation->GetError());
  did_start_navigation_info_->context = std::move(context);
}

void TestWebStateObserver::DidFinishNavigation(WebState* web_state,
                                               NavigationContext* navigation) {
  ASSERT_EQ(web_state_, web_state);
  ASSERT_TRUE(!navigation->GetError() || !navigation->IsSameDocument());
  did_finish_navigation_info_ =
      std::make_unique<web::TestDidFinishNavigationInfo>();
  did_finish_navigation_info_->web_state = web_state;
  std::unique_ptr<web::NavigationContextImpl> context =
      web::NavigationContextImpl::CreateNavigationContext(
          navigation->GetWebState(), navigation->GetUrl(),
          navigation->HasUserGesture(), navigation->GetPageTransition(),
          navigation->IsRendererInitiated());
  context->SetIsSameDocument(navigation->IsSameDocument());
  context->SetError(navigation->GetError());
  did_finish_navigation_info_->context = std::move(context);
}

void TestWebStateObserver::TitleWasSet(WebState* web_state) {
  ASSERT_EQ(web_state_, web_state);
  title_was_set_info_ = std::make_unique<web::TestTitleWasSetInfo>();
  title_was_set_info_->web_state = web_state;
}

void TestWebStateObserver::DidChangeVisibleSecurityState(WebState* web_state) {
  ASSERT_EQ(web_state_, web_state);
  did_change_visible_security_state_info_ =
      std::make_unique<web::TestDidChangeVisibleSecurityStateInfo>();
  did_change_visible_security_state_info_->web_state = web_state;

  NavigationItem* visible_item =
      web_state->GetNavigationManager()->GetVisibleItem();
  if (!visible_item) {
    did_change_visible_security_state_info_->visible_ssl_status.reset();
  } else {
    did_change_visible_security_state_info_->visible_ssl_status =
        std::make_unique<SSLStatus>(visible_item->GetSSL());
  }
}

void TestWebStateObserver::FaviconUrlUpdated(
    WebState* web_state,
    const std::vector<FaviconURL>& candidates) {
  ASSERT_EQ(web_state_, web_state);
  update_favicon_url_candidates_info_ =
      std::make_unique<web::TestUpdateFaviconUrlCandidatesInfo>();
  update_favicon_url_candidates_info_->web_state = web_state;
  update_favicon_url_candidates_info_->candidates = candidates;
}

void TestWebStateObserver::WebFrameDidBecomeAvailable(WebState* web_state,
                                                      WebFrame* web_frame) {
  ASSERT_EQ(web_state_, web_state);
  web_frame_available_info_ =
      std::make_unique<web::TestWebFrameAvailabilityInfo>();
  web_frame_available_info_->web_state = web_state;
  web_frame_available_info_->web_frame = web_frame;
}

void TestWebStateObserver::WebFrameWillBecomeUnavailable(WebState* web_state,
                                                         WebFrame* web_frame) {
  ASSERT_EQ(web_state_, web_state);
  web_frame_unavailable_info_ =
      std::make_unique<web::TestWebFrameAvailabilityInfo>();
  web_frame_unavailable_info_->web_state = web_state;
  web_frame_unavailable_info_->web_frame = web_frame;
}

void TestWebStateObserver::RenderProcessGone(WebState* web_state) {
  ASSERT_EQ(web_state_, web_state);
  render_process_gone_info_ =
      std::make_unique<web::TestRenderProcessGoneInfo>();
  render_process_gone_info_->web_state = web_state;
}

void TestWebStateObserver::WebStateDestroyed(WebState* web_state) {
  ASSERT_EQ(web_state_, web_state);
  EXPECT_TRUE(web_state->IsBeingDestroyed());
  web_state_destroyed_info_ =
      std::make_unique<web::TestWebStateDestroyedInfo>();
  web_state_destroyed_info_->web_state = web_state;
  web_state_->RemoveObserver(this);
  web_state_ = nullptr;
}

void TestWebStateObserver::DidStartLoading(WebState* web_state) {
  ASSERT_EQ(web_state_, web_state);
  start_loading_info_ = std::make_unique<web::TestStartLoadingInfo>();
  start_loading_info_->web_state = web_state;
}

void TestWebStateObserver::DidStopLoading(WebState* web_state) {
  ASSERT_EQ(web_state_, web_state);
  stop_loading_info_ = std::make_unique<web::TestStopLoadingInfo>();
  stop_loading_info_->web_state = web_state;
}

}  // namespace web
