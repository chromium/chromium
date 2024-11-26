// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARE_KIT_MODEL_COLLABORATION_GROUP_ID_NOTIFIER_H_
#define IOS_CHROME_BROWSER_SHARE_KIT_MODEL_COLLABORATION_GROUP_ID_NOTIFIER_H_

#import "components/saved_tab_groups/public/tab_group_sync_service.h"

// Objective-C protocol for collaboration group id updates.
@protocol CollaborationGroupIDNotifierObserver

// Called when the collaboration group id for a tab group changes.
// The `newGroup` parameter contains the updated group with the new
// collaboration group id.
- (void)collaborationIDChangedForGroup:
    (const tab_groups::SavedTabGroup&)newGroup;

@end

// Forwards collaboration group id update events from the
// tab_groups::TabGroupSyncService.
class CollaborationGroupIDNotifier final
    : public tab_groups::TabGroupSyncService::Observer {
 public:
  explicit CollaborationGroupIDNotifier(
      id<CollaborationGroupIDNotifierObserver> observer);

  CollaborationGroupIDNotifier(const CollaborationGroupIDNotifier&) = delete;
  CollaborationGroupIDNotifier& operator=(const CollaborationGroupIDNotifier&) =
      delete;

  ~CollaborationGroupIDNotifier() override;

  // TabGroupSyncService::Observer implementation.
  void OnTabGroupMigrated(const tab_groups::SavedTabGroup& new_group,
                          const base::Uuid& old_sync_id,
                          tab_groups::TriggerSource source) override;

 private:
  __weak id<CollaborationGroupIDNotifierObserver> observer_ = nil;
};

#endif  // IOS_CHROME_BROWSER_SHARE_KIT_MODEL_COLLABORATION_GROUP_ID_NOTIFIER_H_
