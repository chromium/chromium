// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SNAPSHOTS_MODEL_SNAPSHOT_BROWSER_AGENT_H_
#define IOS_CHROME_BROWSER_SNAPSHOTS_MODEL_SNAPSHOT_BROWSER_AGENT_H_

#import <string>
#import <vector>

#import "base/memory/raw_ptr.h"
#import "ios/chrome/browser/shared/model/browser/browser_observer.h"
#import "ios/chrome/browser/shared/model/browser/browser_user_data.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer.h"
#import "ios/chrome/browser/snapshots/model/snapshot_id.h"

@class SnapshotStorageWrapper;

// Associates a SnapshotStorage to a Browser.
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
  void SetSessionID(const std::string& identifier);

  // Maintains the snapshots storage including purging unused images and
  // performing any necessary migrations.
  void PerformStorageMaintenance();

  // Permanently removes all snapshots.
  void RemoveAllSnapshots();

  SnapshotStorageWrapper* snapshot_storage() { return snapshot_storage_; }

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

  // Helper methods to set a snapshot storage for `web_state`.
  void InsertWebState(web::WebState* web_state);
  void DetachWebState(web::WebState* web_state);

  // Migrates the snapshot storage if a folder exists in the old snapshots
  // storage location.
  void MigrateStorageIfNecessary();

  // Purges the snapshots folder of unused snapshots.
  void PurgeUnusedSnapshots();

  // Returns the snapshot IDs of all the WebStates in the Browser.
  std::vector<SnapshotID> GetSnapshotIDs();

  __strong SnapshotStorageWrapper* snapshot_storage_;

  raw_ptr<Browser> browser_ = nullptr;
};

#endif  // IOS_CHROME_BROWSER_SNAPSHOTS_MODEL_SNAPSHOT_BROWSER_AGENT_H_
