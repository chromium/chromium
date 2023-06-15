// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tabs/synced_window_delegate_browser_agent.h"

#import "base/check_op.h"
#import "ios/chrome/browser/sessions/ios_chrome_session_tab_helper.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/sync/ios_chrome_synced_tab_delegate.h"
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

#pragma mark - WebStateListObserver

void SyncedWindowDelegateBrowserAgent::WebStateListChanged(
    WebStateList* web_state_list,
    const WebStateListChange& change,
    const WebStateSelection& selection) {
  DCHECK_EQ(web_state_list_, web_state_list);
  switch (change.type()) {
    case WebStateListChange::Type::kSelectionOnly:
      // Do nothing when a WebState is selected and its status is updated.
      break;
    case WebStateListChange::Type::kDetach:
      // Do nothing when a WebState is detached.
      break;
    case WebStateListChange::Type::kMove:
      // Do nothing when a WebState is moved.
      break;
    case WebStateListChange::Type::kReplace: {
      const WebStateListChangeReplace& replace_change =
          change.As<WebStateListChangeReplace>();
      SetWindowIdForWebState(replace_change.inserted_web_state());
      break;
    }
    case WebStateListChange::Type::kInsert: {
      const WebStateListChangeInsert& insert_change =
          change.As<WebStateListChangeInsert>();
      SetWindowIdForWebState(insert_change.inserted_web_state());
      break;
    }
  }
}

#pragma mark - BrowserObserver

void SyncedWindowDelegateBrowserAgent::BrowserDestroyed(Browser* browser) {
  web_state_list_->RemoveObserver(this);
  browser->RemoveObserver(this);
}

#pragma mark - Private

void SyncedWindowDelegateBrowserAgent::SetWindowIdForWebState(
    web::WebState* web_state) {
  IOSChromeSessionTabHelper::FromWebState(web_state)->SetWindowID(session_id_);
}
