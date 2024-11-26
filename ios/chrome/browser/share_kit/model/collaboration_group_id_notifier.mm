// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/share_kit/model/collaboration_group_id_notifier.h"

#import "components/saved_tab_groups/public/saved_tab_group.h"

CollaborationGroupIDNotifier::CollaborationGroupIDNotifier(
    id<CollaborationGroupIDNotifierObserver> observer)
    : observer_(observer) {}

CollaborationGroupIDNotifier::~CollaborationGroupIDNotifier() {}

void CollaborationGroupIDNotifier::OnTabGroupMigrated(
    const tab_groups::SavedTabGroup& new_group,
    const base::Uuid& old_sync_id,
    tab_groups::TriggerSource source) {
  [observer_ collaborationIDChangedForGroup:new_group];
}
