// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TABS_TAB_MODEL_SYNCED_WINDOW_DELEGATE_H_
#define IOS_CHROME_BROWSER_TABS_TAB_MODEL_SYNCED_WINDOW_DELEGATE_H_

#include "base/macros.h"
#include "components/sessions/core/session_id.h"
#include "components/sync_sessions/synced_window_delegate.h"
#include "ios/chrome/browser/web_state_list/web_state_list_observer.h"

class WebStateList;

namespace browser_sync {
class SyncedTabDelegate;
}

// A TabModelSyncedWindowDelegate is the iOS-based implementation of
// SyncedWindowDelegate.
class TabModelSyncedWindowDelegate : public sync_sessions::SyncedWindowDelegate,
                                     public WebStateListObserver {
 public:
  // This constructor does not add the constructed object as an observere of
  // |web_state_list|; calling code is expected to do that.
  explicit TabModelSyncedWindowDelegate(WebStateList* web_state_list);

  // Return the tab id for the tab at |index|.
  SessionID GetTabIdAt(int index) const override;
  bool IsSessionRestoreInProgress() const override;
  bool ShouldSync() const override;

  // SyncedWindowDelegate:
  bool HasWindow() const override;
  SessionID GetSessionId() const override;
  int GetTabCount() const override;
  int GetActiveIndex() const override;
  bool IsTypeNormal() const override;
  bool IsTypePopup() const override;
  bool IsTabPinned(const sync_sessions::SyncedTabDelegate* tab) const override;
  sync_sessions::SyncedTabDelegate* GetTabAt(int index) const override;

  // WebStateListObserver:
  void WebStateInsertedAt(WebStateList* web_state_list,
                          web::WebState* web_state,
                          int index,
                          bool activating) override;
  void WebStateReplacedAt(WebStateList* web_state_list,
                          web::WebState* old_web_state,
                          web::WebState* new_web_state,
                          int index) override;

 private:
  // Sets the window id of |web_state| to |session_id_|.
  void SetWindowIdForWebState(web::WebState* web_state);

  WebStateList* web_state_list_;
  SessionID session_id_;

  DISALLOW_COPY_AND_ASSIGN(TabModelSyncedWindowDelegate);
};

#endif  // IOS_CHROME_BROWSER_TABS_TAB_MODEL_SYNCED_WINDOW_DELEGATE_H_
