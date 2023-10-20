// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_TAB_RESUMPTION_TAB_RESUMPTION_HELPER_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_TAB_RESUMPTION_TAB_RESUMPTION_HELPER_H_

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#import "base/ios/block_types.h"
#import "components/sessions/core/session_id.h"
#import "ios/chrome/browser/favicon/favicon_loader.h"
#import "url/gurl.h"

class Browser;
class FaviconLoader;
class StartSurfaceRecentTabBrowserAgent;
@class TabResumptionItem;

namespace syncer {
class SyncService;
}  // namespace syncer

namespace sync_sessions {
class SessionSyncService;
}  // namespace sync_sessions

// Helper class to control the tab resumption feature.
class TabResumptionHelper {
 public:
  TabResumptionHelper(const TabResumptionHelper&) = delete;
  TabResumptionHelper& operator=(const TabResumptionHelper&) = delete;

  explicit TabResumptionHelper(Browser* browser);

  // Type for completion block for GetLastTabResumptionItem().
  typedef void (^TabResumptionItemCompletionBlock)(TabResumptionItem*);

  // Tries to return the last available TabResumptionItem.
  // If found, invokes `item_block_handler` and exits.
  void LastTabResumptionItem(
      TabResumptionItemCompletionBlock item_block_handler);

  // Sets `can_show_most_recent_item`.
  void SetCanSHowMostRecentItem(const bool show);

  // Opens the last synced tab from another device.
  void OpenDistantTab();

 private:
  // Bool that tracks if a most recent tab item can be displayed.
  bool can_show_most_recent_item_ = true;
  // Last distant tab resumption item URL.
  GURL last_distant_item_url_;

  // Tab identifier of the last distant tab resumption item.
  SessionID tab_id_ = SessionID::InvalidValue();
  // Session tag of the last distant tab resumption item.
  std::string session_tag_;

  // The owning Browser.
  raw_ptr<Browser> browser_ = nullptr;
  // Loads favicons.
  raw_ptr<FaviconLoader> favicon_loader_ = nullptr;
  // Browser Agent that manages the most recent WebState.
  raw_ptr<StartSurfaceRecentTabBrowserAgent> recent_tab_browser_agent_ =
      nullptr;
  // KeyedService responsible session sync.
  raw_ptr<sync_sessions::SessionSyncService> session_sync_service_ = nullptr;
  // KeyedService responsible for sync state.
  raw_ptr<syncer::SyncService> sync_service_ = nullptr;
};

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_TAB_RESUMPTION_TAB_RESUMPTION_HELPER_H_
