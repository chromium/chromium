// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/tabs/ios_synced_window_delegate_getter.h"

#include <vector>

#include "base/check.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state_manager.h"
#include "ios/chrome/browser/main/browser_list.h"
#include "ios/chrome/browser/main/browser_list_factory.h"
#import "ios/chrome/browser/tabs/synced_window_delegate_browser_agent.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

IOSSyncedWindowDelegatesGetter::IOSSyncedWindowDelegatesGetter() {}

IOSSyncedWindowDelegatesGetter::~IOSSyncedWindowDelegatesGetter() {}

IOSSyncedWindowDelegatesGetter::SyncedWindowDelegateMap
IOSSyncedWindowDelegatesGetter::GetSyncedWindowDelegates() {
  SyncedWindowDelegateMap synced_window_delegates;

  std::vector<ChromeBrowserState*> browser_states =
      GetApplicationContext()
          ->GetChromeBrowserStateManager()
          ->GetLoadedBrowserStates();

  for (auto* browser_state : browser_states) {
    DCHECK(!browser_state->IsOffTheRecord());
    BrowserList* browsers =
        BrowserListFactory::GetForBrowserState(browser_state);
    for (Browser* browser : browsers->AllRegularBrowsers()) {
      if (browser->GetWebStateList()->GetActiveWebState()) {
        sync_sessions::SyncedWindowDelegate* synced_window_delegate =
            SyncedWindowDelegateBrowserAgent::FromBrowser(browser);
        synced_window_delegates[synced_window_delegate->GetSessionId()] =
            synced_window_delegate;
      }
    }
  }

  return synced_window_delegates;
}

const sync_sessions::SyncedWindowDelegate*
IOSSyncedWindowDelegatesGetter::FindById(SessionID session_id) {
  for (const auto& iter : GetSyncedWindowDelegates()) {
    if (session_id == iter.second->GetSessionId())
      return iter.second;
  }
  return nullptr;
}
