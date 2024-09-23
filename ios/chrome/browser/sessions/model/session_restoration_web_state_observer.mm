// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sessions/model/session_restoration_web_state_observer.h"

#import "base/check.h"
#import "base/notreached.h"
#import "ios/chrome/browser/sessions/model/session_restoration_scroll_observer.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/ui/crw_web_view_proxy.h"

SessionRestorationWebStateObserver::~SessionRestorationWebStateObserver() {
  if (web_state_->IsRealized()) {
    web_state_->GetPageWorldWebFramesManager()->RemoveObserver(this);
    [web_state_->GetWebViewProxy().scrollViewProxy
        removeObserver:scroll_observer_];

    [scroll_observer_ shutdown];
    scroll_observer_ = nil;
  }

  web_state_->RemoveObserver(this);
}

#pragma mark - web::WebStateObserver

void SessionRestorationWebStateObserver::WasShown(web::WebState* web_state) {
  // The last active time stamp is updated when a WebState is presented, so
  // mark it as dirty so that the change is reflected in the session storage.
  MarkDirty();
}

void SessionRestorationWebStateObserver::DidFinishNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  DCHECK_EQ(web_state, web_state_);
  // Don't record navigations that result in downloads, since these will be
  // discarded and there's no simple callback when discarded.
  if (navigation_context->IsDownload()) {
    return;
  }

  MarkDirty();
}

void SessionRestorationWebStateObserver::WebStateRealized(
    web::WebState* web_state) {
  DCHECK_EQ(web_state, web_state_);
  // Using base::Unretained(this) is safe since the object calls -shutdown
  // on the SessionRestorationScrollObserver in its destructor which
  // invalidates the callback.
  scroll_observer_ = [[SessionRestorationScrollObserver alloc]
      initWithClosure:base::BindRepeating(
                          &SessionRestorationWebStateObserver::OnScrollEvent,
                          base::Unretained(this))];

  [web_state_->GetWebViewProxy().scrollViewProxy addObserver:scroll_observer_];
  web_state_->GetPageWorldWebFramesManager()->AddObserver(this);
}

void SessionRestorationWebStateObserver::WebStateDestroyed(
    web::WebState* web_state) {
  NOTREACHED();
}

#pragma mark - web::WebFramesManager::Observer

void SessionRestorationWebStateObserver::WebFrameBecameAvailable(
    web::WebFramesManager* web_frames_manager,
    web::WebFrame* web_frame) {
  DCHECK_EQ(web_frames_manager, web_state_->GetPageWorldWebFramesManager());
  if (web_frame->IsMainFrame()) {
    return;
  }

  // -WebFrameBecameAvailable is called much more often than navigations, so
  // check if either `item_count_` or `last_committed_item_index_` has changed
  // before marking a page as stale.
  web::NavigationManager* navigation_manager =
      web_state_->GetNavigationManager();
  if (item_count_ == navigation_manager->GetItemCount() &&
      last_committed_item_index_ ==
          navigation_manager->GetLastCommittedItemIndex()) {
    return;
  }

  MarkDirty();
}

#pragma mark - Private methods

SessionRestorationWebStateObserver::SessionRestorationWebStateObserver(
    web::WebState* web_state,
    WebStateDirtyCallback callback)
    : web_state_(web_state), callback_(callback) {
  web_state_->AddObserver(this);
  if (web_state_->IsRealized()) {
    WebStateRealized(web_state_);
  }
}

void SessionRestorationWebStateObserver::OnScrollEvent() {
  MarkDirty();
}

void SessionRestorationWebStateObserver::MarkDirty() {
  if (is_dirty_) {
    return;
  }

  is_dirty_ = true;

  web::NavigationManager* navigation_manager =
      web_state_->GetNavigationManager();
  item_count_ = navigation_manager->GetItemCount();
  last_committed_item_index_ = navigation_manager->GetLastCommittedItemIndex();

  callback_.Run(web_state_.get());
}

WEB_STATE_USER_DATA_KEY_IMPL(SessionRestorationWebStateObserver)
