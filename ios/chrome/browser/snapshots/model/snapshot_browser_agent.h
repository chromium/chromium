// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SNAPSHOTS_MODEL_SNAPSHOT_BROWSER_AGENT_H_
#define IOS_CHROME_BROWSER_SNAPSHOTS_MODEL_SNAPSHOT_BROWSER_AGENT_H_

#import <Foundation/Foundation.h>

#import <string>
#import <vector>

#import "base/memory/raw_ptr.h"
#import "base/scoped_observation.h"
#import "ios/chrome/browser/shared/model/browser/browser_user_data.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer.h"

@class SnapshotIDWrapper;
@protocol SnapshotStorage;

// Associates a SnapshotStorage to a Browser.
class SnapshotBrowserAgent : public BrowserUserData<SnapshotBrowserAgent>,
                             public WebStateListObserver {
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

  id<SnapshotStorage> snapshot_storage() { return snapshot_storage_; }

 private:
  friend class BrowserUserData<SnapshotBrowserAgent>;

  // Policy for snapshot when detaching a WebState.
  enum class DetachPolicy {
    kPurge,
    kKeep,
  };

  explicit SnapshotBrowserAgent(Browser* browser);

  // WebStateListObserver methods
  void WebStateListDidChange(WebStateList* web_state_list,
                             const WebStateListChange& change,
                             const WebStateListStatus& status) override;

  // Returns the snapshot policy for `change`.
  DetachPolicy PolicyForChange(const WebStateListChangeDetach& change) const;

  // Helper methods to set a snapshot storage for `web_state`.
  void InsertWebState(web::WebState* web_state);
  void DetachWebState(web::WebState* web_state, DetachPolicy policy);

  // Migrates the snapshot storage if a folder exists in the old snapshots
  // storage location.
  void MigrateStorageIfNecessary();

  // Purges the snapshots folder of unused snapshots.
  void PurgeUnusedSnapshots();

  // Returns the snapshot IDs of all the WebStates in the Browser.
  NSArray<SnapshotIDWrapper*>* GetSnapshotIDs();

  __strong id<SnapshotStorage> snapshot_storage_;

  // Scoped observation of the WebStateList.
  base::ScopedObservation<WebStateList, WebStateListObserver>
      web_state_list_observation_{this};
};

#endif  // IOS_CHROME_BROWSER_SNAPSHOTS_MODEL_SNAPSHOT_BROWSER_AGENT_H_
