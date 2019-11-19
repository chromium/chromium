// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/tabs/tab_model_synced_window_delegate_getter.h"

#include "base/logging.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state_manager.h"
#import "ios/chrome/browser/tabs/tab_model.h"
#import "ios/chrome/browser/tabs/tab_model_list.h"
#import "ios/chrome/browser/tabs/tab_model_synced_window_delegate.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

TabModelSyncedWindowDelegatesGetter::TabModelSyncedWindowDelegatesGetter() {}

TabModelSyncedWindowDelegatesGetter::~TabModelSyncedWindowDelegatesGetter() {}

TabModelSyncedWindowDelegatesGetter::SyncedWindowDelegateMap
TabModelSyncedWindowDelegatesGetter::GetSyncedWindowDelegates() {
  SyncedWindowDelegateMap synced_window_delegates;

  std::vector<ios::ChromeBrowserState*> browser_states =
      GetApplicationContext()
          ->GetChromeBrowserStateManager()
          ->GetLoadedBrowserStates();

  for (auto* browser_state : browser_states) {
    DCHECK(!browser_state->IsOffTheRecord());
    NSArray<TabModel*>* tabModels =
        TabModelList::GetTabModelsForChromeBrowserState(browser_state);
    for (TabModel* tabModel in tabModels) {
      if (tabModel.webStateList->GetActiveWebState()) {
        sync_sessions::SyncedWindowDelegate* synced_window_delegate =
            tabModel.syncedWindowDelegate;
        synced_window_delegates[synced_window_delegate->GetSessionId()] =
            synced_window_delegate;
      }
    }
  }

  return synced_window_delegates;
}

const sync_sessions::SyncedWindowDelegate*
TabModelSyncedWindowDelegatesGetter::FindById(SessionID session_id) {
  for (const auto& iter : GetSyncedWindowDelegates()) {
    if (session_id == iter.second->GetSessionId())
      return iter.second;
  }
  return nullptr;
}
