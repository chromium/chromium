// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TABS_MODEL_SYNCED_WINDOW_DELEGATE_BROWSER_AGENT_H_
#define IOS_CHROME_BROWSER_TABS_MODEL_SYNCED_WINDOW_DELEGATE_BROWSER_AGENT_H_

#import "base/memory/raw_ptr.h"
#import "components/sessions/core/session_id.h"
#import "components/sync_sessions/synced_window_delegate.h"
#import "ios/chrome/browser/shared/model/browser/browser_observer.h"
#import "ios/chrome/browser/shared/model/browser/browser_user_data.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer.h"

class WebStateList;

namespace browser_sync {
class SyncedTabDelegate;
}

// A SyncedWindowDelegateBrowserAgent is the iOS-based implementation of
// SyncedWindowDelegate.
class SyncedWindowDelegateBrowserAgent
    : public sync_sessions::SyncedWindowDelegate,
      public BrowserObserver,
      public BrowserUserData<SyncedWindowDelegateBrowserAgent>,
      public WebStateListObserver {
 public:
  // Not copyable or moveable
  SyncedWindowDelegateBrowserAgent(const SyncedWindowDelegateBrowserAgent&) =
      delete;
  SyncedWindowDelegateBrowserAgent& operator=(
      const SyncedWindowDelegateBrowserAgent&) = delete;
  ~SyncedWindowDelegateBrowserAgent() override;

  // Return the tab id for the tab at `index`.
  SessionID GetTabIdAt(int index) const override;
  bool IsSessionRestoreInProgress() const override;
  bool ShouldSync() const override;

  // SyncedWindowDelegate:
  bool HasWindow() const override;
  SessionID GetSessionId() const override;
  int GetTabCount() const override;
  bool IsTypeNormal() const override;
  bool IsTypePopup() const override;
  bool IsTabPinned(const sync_sessions::SyncedTabDelegate* tab) const override;
  sync_sessions::SyncedTabDelegate* GetTabAt(int index) const override;

  // WebStateListObserver:
  void WebStateListDidChange(WebStateList* web_state_list,
                             const WebStateListChange& change,
                             const WebStateListStatus& status) override;

 private:
  friend class BrowserUserData<SyncedWindowDelegateBrowserAgent>;
  BROWSER_USER_DATA_KEY_DECL();

  explicit SyncedWindowDelegateBrowserAgent(Browser* browser);

  // BrowserObserver
  void BrowserDestroyed(Browser* browser) override;

  // Sets the window id of `web_state` to `session_id_`.
  void SetWindowIdForWebState(web::WebState* web_state);

  raw_ptr<WebStateList> web_state_list_;
  SessionID session_id_;
};

#endif  // IOS_CHROME_BROWSER_TABS_MODEL_SYNCED_WINDOW_DELEGATE_BROWSER_AGENT_H_
