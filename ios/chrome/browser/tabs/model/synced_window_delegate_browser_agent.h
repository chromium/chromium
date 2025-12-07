// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TABS_MODEL_SYNCED_WINDOW_DELEGATE_BROWSER_AGENT_H_
#define IOS_CHROME_BROWSER_TABS_MODEL_SYNCED_WINDOW_DELEGATE_BROWSER_AGENT_H_

#import "base/memory/raw_ptr.h"
#import "components/sessions/core/session_id.h"
#import "components/sync_sessions/synced_window_delegate.h"
#import "ios/chrome/browser/shared/model/browser/browser_user_data.h"
#import "ios/chrome/browser/tabs/model/tabs_dependency_installer.h"

namespace browser_sync {
class SyncedTabDelegate;
}

// A SyncedWindowDelegateBrowserAgent is the iOS-based implementation of
// SyncedWindowDelegate.
class SyncedWindowDelegateBrowserAgent
    : public sync_sessions::SyncedWindowDelegate,
      public BrowserUserData<SyncedWindowDelegateBrowserAgent>,
      public TabsDependencyInstaller {
 public:
  // Not copyable or moveable
  SyncedWindowDelegateBrowserAgent(const SyncedWindowDelegateBrowserAgent&) =
      delete;
  SyncedWindowDelegateBrowserAgent& operator=(
      const SyncedWindowDelegateBrowserAgent&) = delete;
  ~SyncedWindowDelegateBrowserAgent() override;

  // Return the tab id for the tab at `index`.
  SessionID GetTabIdAt(int index) const override;
  bool IsPlaceholderTabAt(int index) const override;
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

  // TabsDependencyInstaller:
  void OnWebStateInserted(web::WebState* web_state) override;
  void OnWebStateRemoved(web::WebState* web_state) override;
  void OnWebStateDeleted(web::WebState* web_state) override;
  void OnActiveWebStateChanged(web::WebState* old_active,
                               web::WebState* new_active) override;

 private:
  friend class BrowserUserData<SyncedWindowDelegateBrowserAgent>;

  explicit SyncedWindowDelegateBrowserAgent(Browser* browser);

  // Returns the WebState at index.
  web::WebState* GetWebStateAt(int index) const;

  const SessionID session_id_;
};

#endif  // IOS_CHROME_BROWSER_TABS_MODEL_SYNCED_WINDOW_DELEGATE_BROWSER_AGENT_H_
