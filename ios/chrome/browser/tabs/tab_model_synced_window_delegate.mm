// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/tabs/tab_model_synced_window_delegate.h"

#include "base/logging.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/sessions/ios_chrome_session_tab_helper.h"
#include "ios/chrome/browser/sync/ios_chrome_synced_tab_delegate.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/web/public/navigation/navigation_manager.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

TabModelSyncedWindowDelegate::TabModelSyncedWindowDelegate(
    WebStateList* web_state_list)
    : web_state_list_(web_state_list), session_id_(SessionID::NewUnique()) {
  for (int index = 0; index < web_state_list_->count(); ++index) {
    SetWindowIdForWebState(web_state_list_->GetWebStateAt(index));
  }
}

SessionID TabModelSyncedWindowDelegate::GetTabIdAt(int index) const {
  return GetTabAt(index)->GetSessionId();
}

bool TabModelSyncedWindowDelegate::IsSessionRestoreInProgress() const {
  for (int index = 0; index < web_state_list_->count(); ++index) {
    const web::NavigationManager* navigation_manager =
        web_state_list_->GetWebStateAt(index)->GetNavigationManager();
    if (navigation_manager->IsRestoreSessionInProgress()) {
      return true;
    }
  }
  return false;
}

bool TabModelSyncedWindowDelegate::ShouldSync() const {
  return true;
}

bool TabModelSyncedWindowDelegate::HasWindow() const {
  return true;
}

SessionID TabModelSyncedWindowDelegate::GetSessionId() const {
  return session_id_;
}

int TabModelSyncedWindowDelegate::GetTabCount() const {
  return web_state_list_->count();
}

int TabModelSyncedWindowDelegate::GetActiveIndex() const {
  DCHECK_NE(web_state_list_->active_index(), WebStateList::kInvalidIndex);
  return web_state_list_->active_index();
}

bool TabModelSyncedWindowDelegate::IsTypeNormal() const {
  return true;
}

bool TabModelSyncedWindowDelegate::IsTypePopup() const {
  return false;
}

bool TabModelSyncedWindowDelegate::IsTabPinned(
    const sync_sessions::SyncedTabDelegate* tab) const {
  return false;
}

sync_sessions::SyncedTabDelegate* TabModelSyncedWindowDelegate::GetTabAt(
    int index) const {
  return IOSChromeSyncedTabDelegate::FromWebState(
      web_state_list_->GetWebStateAt(index));
}

void TabModelSyncedWindowDelegate::WebStateInsertedAt(
    WebStateList* web_state_list,
    web::WebState* web_state,
    int index,
    bool activating) {
  DCHECK_EQ(web_state_list_, web_state_list);
  SetWindowIdForWebState(web_state);
}

void TabModelSyncedWindowDelegate::WebStateReplacedAt(
    WebStateList* web_state_list,
    web::WebState* old_web_state,
    web::WebState* new_web_state,
    int index) {
  DCHECK_EQ(web_state_list_, web_state_list);
  SetWindowIdForWebState(new_web_state);
}

void TabModelSyncedWindowDelegate::SetWindowIdForWebState(
    web::WebState* web_state) {
  IOSChromeSessionTabHelper::FromWebState(web_state)->SetWindowID(session_id_);
}
