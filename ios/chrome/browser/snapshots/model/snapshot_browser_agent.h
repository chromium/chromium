// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SNAPSHOTS_MODEL_SNAPSHOT_BROWSER_AGENT_H_
#define IOS_CHROME_BROWSER_SNAPSHOTS_MODEL_SNAPSHOT_BROWSER_AGENT_H_

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#import <string>
#import <vector>

#import "base/memory/raw_ptr.h"
#import "base/scoped_observation.h"
#import "ios/chrome/browser/shared/model/browser/browser_user_data.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/snapshots/model/snapshot_types.h"
#import "ios/chrome/browser/tabs/model/tabs_dependency_installer.h"

@protocol SnapshotStorage;

// Associates a SnapshotStorage to a Browser.
class SnapshotBrowserAgent : public BrowserUserData<SnapshotBrowserAgent>,
                             public TabsDependencyInstaller {
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

  // Retrieves snapshot of `snapshot_kind` for `snapshot_id` invoking
  // `completion` with the image retrieved (or nil in case of failure).
  void RetrieveSnapshotWithID(SnapshotID snapshot_id,
                              SnapshotKind snapshot_kind,
                              SnapshotRetrievedBlock completion);

  // TabsDependencyInstaller
  void OnWebStateInserted(web::WebState* web_state) override;
  void OnWebStateRemoved(web::WebState* web_state) override;
  void OnWebStateDeleted(web::WebState* web_state) override;
  void OnActiveWebStateChanged(web::WebState* old_active,
                               web::WebState* new_active) override;

 private:
  friend class BrowserUserData<SnapshotBrowserAgent>;

  explicit SnapshotBrowserAgent(Browser* browser);

  // Purges the snapshots folder of unused snapshots.
  void PurgeUnusedSnapshots();

  __strong id<SnapshotStorage> snapshot_storage_;
};

#endif  // IOS_CHROME_BROWSER_SNAPSHOTS_MODEL_SNAPSHOT_BROWSER_AGENT_H_
