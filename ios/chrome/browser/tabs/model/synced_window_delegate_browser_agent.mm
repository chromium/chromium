// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tabs/model/synced_window_delegate_browser_agent.h"

#import "base/check_op.h"
#import "components/sync/base/features.h"
#import "ios/chrome/browser/sessions/model/ios_chrome_session_tab_helper.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/tabs/model/ios_chrome_synced_tab_delegate.h"
#import "ios/web/public/navigation/navigation_manager.h"

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
  // TODO(crbug.com/40650994): Use SessionRestorationObserver to track if the
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

void SyncedWindowDelegateBrowserAgent::WebStateListDidChange(
    WebStateList* web_state_list,
    const WebStateListChange& change,
    const WebStateListStatus& status) {
  DCHECK_EQ(web_state_list_, web_state_list);
  switch (change.type()) {
    case WebStateListChange::Type::kStatusOnly:
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
    case WebStateListChange::Type::kGroupCreate:
      // TODO(crbug.com/329640035): Should Sync be notified of the group
      // creation here?
      break;
    case WebStateListChange::Type::kGroupVisualDataUpdate:
      // TODO(crbug.com/329640035): Should Sync be notified of the group's
      // visual data update here?
      break;
    case WebStateListChange::Type::kGroupMove:
      // TODO(crbug.com/329640035): Should Sync be notified of the group move
      // here?
      break;
    case WebStateListChange::Type::kGroupDelete:
      // TODO(crbug.com/329640035): Should Sync be notified of the group
      // deletion here?
      break;
  }
  if (status.active_web_state_change()) {
    if (status.old_active_web_state &&
        IOSChromeSyncedTabDelegate::FromWebState(status.old_active_web_state)) {
      IOSChromeSyncedTabDelegate::FromWebState(status.old_active_web_state)
          ->ResetCachedLastActiveTime();
    }
    if (status.new_active_web_state &&
        IOSChromeSyncedTabDelegate::FromWebState(status.new_active_web_state)) {
      IOSChromeSyncedTabDelegate::FromWebState(status.new_active_web_state)
          ->ResetCachedLastActiveTime();
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
