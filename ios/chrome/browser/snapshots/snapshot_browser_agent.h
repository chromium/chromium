// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SNAPSHOTS_SNAPSHOT_BROWSER_AGENT_H_
#define IOS_CHROME_BROWSER_SNAPSHOTS_SNAPSHOT_BROWSER_AGENT_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/shared/model/browser/browser_observer.h"
#import "ios/chrome/browser/shared/model/browser/browser_user_data.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer.h"

@class SnapshotCache;

// Associates a SnapshotCache to a Browser.
class SnapshotBrowserAgent : public BrowserObserver,
                             public WebStateListObserver,
                             public BrowserUserData<SnapshotBrowserAgent> {
 public:
  SnapshotBrowserAgent(const SnapshotBrowserAgent&) = delete;
  SnapshotBrowserAgent& operator=(const SnapshotBrowserAgent&) = delete;

  ~SnapshotBrowserAgent() override;

  // Set a session identification string that will be used to locate the
  // snapshots directory. Setting this more than once on the same agent is
  // probably a programming error.
  void SetSessionID(NSString* session_identifier);

  // Maintains the snapshots storage including purging unused images and
  // performing any necessary migrations.
  void PerformStorageMaintenance();

  // Permanently removes all snapshots.
  void RemoveAllSnapshots();

  SnapshotCache* snapshot_cache() { return snapshot_cache_; }

 private:
  friend class BrowserUserData<SnapshotBrowserAgent>;
  BROWSER_USER_DATA_KEY_DECL();

  explicit SnapshotBrowserAgent(Browser* browser);

  // BrowserObserver methods
  void BrowserDestroyed(Browser* browser) override;

  // WebStateListObserver methods
  void WebStateListDidChange(WebStateList* web_state_list,
                             const WebStateListChange& change,
                             const WebStateListStatus& status) override;
  void WillBeginBatchOperation(WebStateList* web_state_list) override;
  void BatchOperationEnded(WebStateList* web_state_list) override;

  // Helper methods to set a snapshot cache for `web_state`.
  void InsertWebState(web::WebState* web_state);
  void DetachWebState(web::WebState* web_state);

  // Migrates the snapshot storage if a folder exists in the old snapshots
  // storage location.
  void MigrateStorageIfNecessary();

  // Purges the snapshots folder of unused snapshots.
  void PurgeUnusedSnapshots();

  // Returns the snapshot IDs of all the WebStates in the Browser.
  NSSet<NSString*>* GetSnapshotIDs();

  __strong SnapshotCache* snapshot_cache_;

  Browser* browser_ = nullptr;
};

#endif  // IOS_CHROME_BROWSER_SNAPSHOTS_SNAPSHOT_BROWSER_AGENT_H_
