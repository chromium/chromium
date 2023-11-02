// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tabs/synced_window_delegate_browser_agent.h"

#import "base/check_op.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/sessions/ios_chrome_session_tab_helper.h"
#import "ios/chrome/browser/sync/ios_chrome_synced_tab_delegate.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/web/public/navigation/navigation_manager.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

BROWSER_USER_DATA_KEY_IMPL(SyncedWindowDelegateBrowserAgent)

SyncedWindowDelegateBrowserAgent::SyncedWindowDelegateBrowserAgent(
    Browser* browser)
    : web_state_list_(browser->GetWebStateList()),
      session_id_(SessionID::NewUnique()) {
  browser->AddObserver(this);
  for (int index = 0; index < web_state_list_->count(); ++index) {
    SetWindowIdForWebState(web_state_list_->GetWebStateAt(index));
  }
  web_state_list_->AddObserver(this);
}

SyncedWindowDelegateBrowserAgent::~SyncedWindowDelegateBrowserAgent() {}

SessionID SyncedWindowDelegateBrowserAgent::GetTabIdAt(int index) const {
  return GetTabAt(index)->GetSessionId();
}

bool SyncedWindowDelegateBrowserAgent::IsSessionRestoreInProgress() const {
  // On iOS, the WebStateList restoration is done in a batch operation on the
  // main thread.
  // * as this is in a batch operation, no event is forwarded to the sync engine
  // * as it is on main thread, the tab sync (also on the main thread) is not
  // called during the process.
  // TODO(crbug.com/1010164): Use SessionRestorationObserver to track if the
  // session is being restored.
  return false;
}

bool SyncedWindowDelegateBrowserAgent::ShouldSync() const {
  return true;
}

bool SyncedWindowDelegateBrowserAgent::HasWindow() const {
  return true;
}

SessionID SyncedWindowDelegateBrowserAgent::GetSessionId() const {
  return session_id_;
}

int SyncedWindowDelegateBrowserAgent::GetTabCount() const {
  return web_state_list_->count();
}

int SyncedWindowDelegateBrowserAgent::GetActiveIndex() const {
  DCHECK_NE(web_state_list_->active_index(), WebStateList::kInvalidIndex);
  return web_state_list_->active_index();
}

bool SyncedWindowDelegateBrowserAgent::IsTypeNormal() const {
  return true;
}

bool SyncedWindowDelegateBrowserAgent::IsTypePopup() const {
  return false;
}

bool SyncedWindowDelegateBrowserAgent::IsTabPinned(
    const sync_sessions::SyncedTabDelegate* tab) const {
  return false;
}

sync_sessions::SyncedTabDelegate* SyncedWindowDelegateBrowserAgent::GetTabAt(
    int index) const {
  return IOSChromeSyncedTabDelegate::FromWebState(
      web_state_list_->GetWebStateAt(index));
}

void SyncedWindowDelegateBrowserAgent::WebStateInsertedAt(
    WebStateList* web_state_list,
    web::WebState* web_state,
    int index,
    bool activating) {
  DCHECK_EQ(web_state_list_, web_state_list);
  SetWindowIdForWebState(web_state);
}

void SyncedWindowDelegateBrowserAgent::WebStateReplacedAt(
    WebStateList* web_state_list,
    web::WebState* old_web_state,
    web::WebState* new_web_state,
    int index) {
  DCHECK_EQ(web_state_list_, web_state_list);
  SetWindowIdForWebState(new_web_state);
}

void SyncedWindowDelegateBrowserAgent::SetWindowIdForWebState(
    web::WebState* web_state) {
  IOSChromeSessionTabHelper::FromWebState(web_state)->SetWindowID(session_id_);
}

void SyncedWindowDelegateBrowserAgent::BrowserDestroyed(Browser* browser) {
  web_state_list_->RemoveObserver(this);
  browser->RemoveObserver(this);
}
