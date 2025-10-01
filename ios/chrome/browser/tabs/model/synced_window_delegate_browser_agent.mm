// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tabs/model/synced_window_delegate_browser_agent.h"

#import "base/check_op.h"
#import "components/sync/base/features.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/tabs/model/ios_chrome_synced_tab_delegate.h"
#import "ios/web/public/navigation/navigation_manager.h"

namespace {

void ResetCachedLastActiveTimeForWebState(web::WebState* web_state) {
  if (!web_state) {
    return;
  }

  auto* tab_helper = IOSChromeSyncedTabDelegate::FromWebState(web_state);
  if (!tab_helper) {
    return;
  }

  tab_helper->ResetCachedLastActiveTime();
}

}  // namespace

SyncedWindowDelegateBrowserAgent::SyncedWindowDelegateBrowserAgent(
    Browser* browser)
    : BrowserUserData(browser),
      session_id_(SessionID::NewUnique()) {
  StartObserving(browser, Policy::kAccordingToFeature);
}

SyncedWindowDelegateBrowserAgent::~SyncedWindowDelegateBrowserAgent() {
  StopObserving();
}

SessionID SyncedWindowDelegateBrowserAgent::GetTabIdAt(int index) const {
  return GetWebStateAt(index)->GetUniqueIdentifier().ToSessionID();
}

bool SyncedWindowDelegateBrowserAgent::IsPlaceholderTabAt(int index) const {
  // A tab is considered as "placeholder" if it is not fully
  // loaded. This corresponds to "unrealized" tabs.
  return !GetWebStateAt(index)->IsRealized();
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
  return browser_->GetWebStateList()->count();
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
  return IOSChromeSyncedTabDelegate::FromWebState(GetWebStateAt(index));
}

#pragma mark - TabsDependencyInstaller

void SyncedWindowDelegateBrowserAgent::OnWebStateInserted(
    web::WebState* web_state) {
  IOSChromeSyncedTabDelegate::CreateForWebState(web_state, session_id_);
}

void SyncedWindowDelegateBrowserAgent::OnWebStateRemoved(
    web::WebState* web_state) {
  IOSChromeSyncedTabDelegate::RemoveFromWebState(web_state);
}

void SyncedWindowDelegateBrowserAgent::OnWebStateDeleted(
    web::WebState* web_state) {
  // Nothing to do.
}

void SyncedWindowDelegateBrowserAgent::OnActiveWebStateChanged(
    web::WebState* old_active,
    web::WebState* new_active) {
  ResetCachedLastActiveTimeForWebState(old_active);
  ResetCachedLastActiveTimeForWebState(new_active);
}

#pragma mark - Private methods

web::WebState* SyncedWindowDelegateBrowserAgent::GetWebStateAt(
    int index) const {
  return browser_->GetWebStateList()->GetWebStateAt(index);
}
