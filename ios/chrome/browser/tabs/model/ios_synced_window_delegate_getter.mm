// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tabs/model/ios_synced_window_delegate_getter.h"

#import "base/check.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/tabs/model/synced_window_delegate_browser_agent.h"

IOSSyncedWindowDelegatesGetter::IOSSyncedWindowDelegatesGetter(
    BrowserList* browser_list)
    : browser_list_(browser_list) {
  DCHECK(browser_list);
}

IOSSyncedWindowDelegatesGetter::~IOSSyncedWindowDelegatesGetter() {}

IOSSyncedWindowDelegatesGetter::SyncedWindowDelegateMap
IOSSyncedWindowDelegatesGetter::GetSyncedWindowDelegates() {
  SyncedWindowDelegateMap synced_window_delegates;
  for (Browser* browser : browser_list_->BrowsersOfType(
           BrowserList::BrowserType::kRegularAndInactive)) {
    sync_sessions::SyncedWindowDelegate* synced_window_delegate =
        SyncedWindowDelegateBrowserAgent::FromBrowser(browser);
    synced_window_delegates[synced_window_delegate->GetSessionId()] =
        synced_window_delegate;
  }
  return synced_window_delegates;
}

const sync_sessions::SyncedWindowDelegate*
IOSSyncedWindowDelegatesGetter::FindById(SessionID session_id) {
  for (Browser* browser : browser_list_->BrowsersOfType(
           BrowserList::BrowserType::kRegularAndInactive)) {
    sync_sessions::SyncedWindowDelegate* synced_window_delegate =
        SyncedWindowDelegateBrowserAgent::FromBrowser(browser);
    if (synced_window_delegate->GetSessionId() == session_id) {
      return synced_window_delegate;
    }
  }
  return nullptr;
}
