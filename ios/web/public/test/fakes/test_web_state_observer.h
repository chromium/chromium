// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_TEST_FAKES_TEST_WEB_STATE_OBSERVER_H_
#define IOS_WEB_PUBLIC_TEST_FAKES_TEST_WEB_STATE_OBSERVER_H_

#include "ios/web/public/test/fakes/test_web_state_observer_util.h"
#include "ios/web/public/web_state_observer.h"

namespace web {

class WebState;

// Test observer to check that the WebStateObserver methods are called as
// expected. Can only observe a single WebState.
// TODO(crbug.com/775684): fix this to allow observing multiple WebStates.
class TestWebStateObserver : public WebStateObserver {
 public:
  TestWebStateObserver(WebState* web_state);
  ~TestWebStateObserver() override;

  WebState* web_state() { return web_state_; }

  // Arguments passed to |WasShown|.
  web::TestWasShownInfo* was_shown_info() { return was_shown_info_.get(); }
  // Arguments passed to |WasHidden|.
  web::TestWasHiddenInfo* was_hidden_info() { return was_hidden_info_.get(); }
  // Arguments passed to |DidStartNavigation|.
  web::TestDidStartNavigationInfo* did_start_navigation_info() {
    return did_start_navigation_info_.get();
  }
  // Arguments passed to |DidFinishNavigation|.
  web::TestDidFinishNavigationInfo* did_finish_navigation_info() {
    return did_finish_navigation_info_.get();
  }
  // Arguments passed to |PageLoaded|.
  web::TestLoadPageInfo* load_page_info() { return load_page_info_.get(); }
  // Arguments passed to |LoadProgressChanged|.
  web::TestChangeLoadingProgressInfo* change_loading_progress_info() {
    return change_loading_progress_info_.get();
  }
  // Arguments passed to |NavigationItemsPruned|.
  web::TestNavigationItemsPrunedInfo* navigation_items_pruned_info() {
    return navigation_items_pruned_info_.get();
  }
  // Arguments passed to |TitleWasSet|.
  web::TestTitleWasSetInfo* title_was_set_info() {
    return title_was_set_info_.get();
  }
  // Arguments passed to |DidChangeVisibleSecurityState|.
  web::TestDidChangeVisibleSecurityStateInfo*
  did_change_visible_security_state_info() {
    return did_change_visible_security_state_info_.get();
  }
  // Arguments passed to |FaviconUrlUpdated|.
  web::TestUpdateFaviconUrlCandidatesInfo*
  update_favicon_url_candidates_info() {
    return update_favicon_url_candidates_info_.get();
  }
  // Arguments passed to |WebFrameDidBecomeAvailable|.
  web::TestWebFrameAvailabilityInfo* web_frame_available_info() {
    return web_frame_available_info_.get();
  }
  // Arguments passed to |WebFrameWillBecomeUnavailable|.
  web::TestWebFrameAvailabilityInfo* web_frame_unavailable_info() {
    return web_frame_unavailable_info_.get();
  }
  // Arguments passed to |RenderProcessGone|.
  web::TestRenderProcessGoneInfo* render_process_gone_info() {
    return render_process_gone_info_.get();
  }
  // Arguments passed to |WebStateDestroyed|.
  web::TestWebStateDestroyedInfo* web_state_destroyed_info() {
    return web_state_destroyed_info_.get();
  }
  // Arguments passed to |DidStartLoading|.
  web::TestStopLoadingInfo* stop_loading_info() {
    return stop_loading_info_.get();
  }
  // Arguments passed to |DidStopLoading|.
  web::TestStartLoadingInfo* start_loading_info() {
    return start_loading_info_.get();
  }

 private:
  // WebStateObserver implementation:
  void WasShown(WebState* web_state) override;
  void WasHidden(WebState* web_state) override;
  void PageLoaded(WebState* web_state,
                  PageLoadCompletionStatus load_completion_status) override;
  void LoadProgressChanged(WebState* web_state, double progress) override;
  void NavigationItemsPruned(WebState* web_state,
                             size_t pruned_item_count) override;
  void DidStartNavigation(WebState* web_state,
                          NavigationContext* context) override;
  void DidFinishNavigation(WebState* web_state,
                           NavigationContext* context) override;
  void TitleWasSet(WebState* web_state) override;
  void DidChangeVisibleSecurityState(WebState* web_state) override;
  void FaviconUrlUpdated(WebState* web_state,
                         const std::vector<FaviconURL>& candidates) override;
  void WebFrameDidBecomeAvailable(WebState* web_state,
                                  WebFrame* web_frame) override;
  void WebFrameWillBecomeUnavailable(WebState* web_state,
                                     WebFrame* web_frame) override;
  void RenderProcessGone(WebState* web_state) override;
  void WebStateDestroyed(WebState* web_state) override;
  void DidStartLoading(WebState* web_state) override;
  void DidStopLoading(WebState* web_state) override;

  // The WebState this instance is observing. Will be null after
  // WebStateDestroyed has been called.
  web::WebState* web_state_ = nullptr;

  std::unique_ptr<web::TestWasShownInfo> was_shown_info_;
  std::unique_ptr<web::TestWasHiddenInfo> was_hidden_info_;
  std::unique_ptr<web::TestLoadPageInfo> load_page_info_;
  std::unique_ptr<web::TestChangeLoadingProgressInfo>
      change_loading_progress_info_;
  std::unique_ptr<web::TestNavigationItemsPrunedInfo>
      navigation_items_pruned_info_;
  std::unique_ptr<web::TestDidStartNavigationInfo> did_start_navigation_info_;
  std::unique_ptr<web::TestDidFinishNavigationInfo> did_finish_navigation_info_;
  std::unique_ptr<web::TestTitleWasSetInfo> title_was_set_info_;
  std::unique_ptr<web::TestDidChangeVisibleSecurityStateInfo>
      did_change_visible_security_state_info_;
  std::unique_ptr<web::TestUpdateFaviconUrlCandidatesInfo>
      update_favicon_url_candidates_info_;
  std::unique_ptr<web::TestWebFrameAvailabilityInfo> web_frame_available_info_;
  std::unique_ptr<web::TestWebFrameAvailabilityInfo>
      web_frame_unavailable_info_;
  std::unique_ptr<web::TestRenderProcessGoneInfo> render_process_gone_info_;
  std::unique_ptr<web::TestWebStateDestroyedInfo> web_state_destroyed_info_;
  std::unique_ptr<web::TestStartLoadingInfo> start_loading_info_;
  std::unique_ptr<web::TestStopLoadingInfo> stop_loading_info_;
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_TEST_FAKES_TEST_WEB_STATE_OBSERVER_H_
