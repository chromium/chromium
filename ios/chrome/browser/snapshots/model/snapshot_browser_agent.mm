// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/snapshots/model/snapshot_browser_agent.h"

#import "base/base_paths.h"
#import "base/files/file_path.h"
#import "base/ios/ios_util.h"
#import "base/path_service.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/sessions/model/session_constants.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/snapshots/model/model_swift.h"
#import "ios/chrome/browser/snapshots/model/snapshot_storage_util.h"
#import "ios/chrome/browser/snapshots/model/snapshot_tab_helper.h"

namespace {

// Name of the directory containing the tab snapshots.
const base::FilePath::CharType kSnapshots[] = FILE_PATH_LITERAL("Snapshots");

// Converts `snapshot_id` to a SnapshotIDWrapper.
SnapshotIDWrapper* ToWrapper(SnapshotID snapshot_id) {
  return [[SnapshotIDWrapper alloc] initWithSnapshotID:snapshot_id];
}

}  // anonymous namespace

SnapshotBrowserAgent::SnapshotBrowserAgent(Browser* browser)
    : BrowserUserData(browser) {
  web_state_list_observation_.Observe(browser_->GetWebStateList());
}

SnapshotBrowserAgent::~SnapshotBrowserAgent() {
  [snapshot_storage_ shutdown];
}

#pragma mark - WebStateListObserver

void SnapshotBrowserAgent::WebStateListDidChange(
    WebStateList* web_state_list,
    const WebStateListChange& change,
    const WebStateListStatus& status) {
  switch (change.type()) {
    case WebStateListChange::Type::kStatusOnly:
      // Do nothing when a WebState is selected and its status is updated.
      break;
    case WebStateListChange::Type::kDetach: {
      const WebStateListChangeDetach& detach_change =
          change.As<WebStateListChangeDetach>();
      DetachWebState(detach_change.detached_web_state(),
                     PolicyForChange(detach_change));
      break;
    }
    case WebStateListChange::Type::kMove:
      // Do nothing when a WebState is moved.
      break;
    case WebStateListChange::Type::kReplace: {
      const WebStateListChangeReplace& replace_change =
          change.As<WebStateListChangeReplace>();
      DetachWebState(replace_change.replaced_web_state(), DetachPolicy::kPurge);
      InsertWebState(replace_change.inserted_web_state());
      break;
    }
    case WebStateListChange::Type::kInsert: {
      const WebStateListChangeInsert& insert_change =
          change.As<WebStateListChangeInsert>();
      InsertWebState(insert_change.inserted_web_state());
      break;
    }
    case WebStateListChange::Type::kGroupCreate:
      // Do nothing when a group is created.
      break;
    case WebStateListChange::Type::kGroupVisualDataUpdate:
      // Do nothing when a tab group's visual data are updated.
      break;
    case WebStateListChange::Type::kGroupMove:
      // Do nothing when a tab group is moved.
      break;
    case WebStateListChange::Type::kGroupDelete:
      // Do nothing when a group is deleted.
      break;
  }
}

SnapshotBrowserAgent::DetachPolicy SnapshotBrowserAgent::PolicyForChange(
    const WebStateListChangeDetach& change) const {
  // The tab is detached without being closed, it is likely going to be
  // moved to another Browser, so keep the snapshot (it will be moved).
  if (!change.is_closing()) {
    return DetachPolicy::kKeep;
  }

  // If the tab is closed due to an user action, or due to tab cleanup,
  // then it won't be reopened and the snapshot can be deleted.
  if (change.is_user_action() || change.is_tabs_cleanup()) {
    return DetachPolicy::kPurge;
  }

  // Do not delete the snapshot otherwise (it is likely because the window
  // is being closed, and the Browser destroyed).
  return DetachPolicy::kKeep;
}

void SnapshotBrowserAgent::SetSessionID(const std::string& identifier) {
  // It is incorrect to call this method twice.
  DCHECK(!snapshot_storage_);
  DCHECK(!identifier.empty());

  const base::FilePath& profile_path = browser_->GetProfile()->GetStatePath();

  // The snapshots are stored in a sub-directory of the session storage.
  // TODO(crbug.com/40942167): change this before launching the optimised
  // session storage as the session directory will be renamed.
  const base::FilePath legacy_path = profile_path.Append(kLegacySessionsDirname)
                                         .Append(identifier)
                                         .Append(kSnapshots);

  const base::FilePath storage_path =
      profile_path.Append(kSnapshots).Append(identifier);

  snapshot_storage_ = CreateSnapshotStorage(storage_path, legacy_path);
}

void SnapshotBrowserAgent::PerformStorageMaintenance() {
  MigrateStorageIfNecessary();
  PurgeUnusedSnapshots();
}

void SnapshotBrowserAgent::RemoveAllSnapshots() {
  [snapshot_storage_ removeAllImages];
}

void SnapshotBrowserAgent::InsertWebState(web::WebState* web_state) {
  SnapshotTabHelper::FromWebState(web_state)->SetSnapshotStorage(
      snapshot_storage_);
}

void SnapshotBrowserAgent::DetachWebState(web::WebState* web_state,
                                          DetachPolicy policy) {
  if (policy == DetachPolicy::kPurge) {
    const SnapshotID snapshot_id(web_state->GetUniqueIdentifier());
    [snapshot_storage_ removeImageWithSnapshotID:ToWrapper(snapshot_id)];
  }
  SnapshotTabHelper::FromWebState(web_state)->SetSnapshotStorage(nil);
}

void SnapshotBrowserAgent::MigrateStorageIfNecessary() {
  DCHECK(snapshot_storage_);

  WebStateList* web_state_list = browser_->GetWebStateList();
  const int web_state_list_count = web_state_list->count();
  if (!web_state_list_count) {
    return;
  }

  // Snapshots used to be identified by the web state stable identifier, but are
  // now identified by a snapshot ID.
  NSMutableArray<NSString*>* stable_identifiers =
      [NSMutableArray arrayWithCapacity:web_state_list_count];

  NSMutableArray<SnapshotIDWrapper*>* snapshot_identifiers =
      [[NSMutableArray alloc] initWithCapacity:web_state_list_count];

  for (int index = 0; index < web_state_list_count; ++index) {
    web::WebState* web_state = web_state_list->GetWebStateAt(index);
    [stable_identifiers addObject:web_state->GetStableIdentifier()];
    [snapshot_identifiers
        addObject:ToWrapper(SnapshotID(web_state->GetUniqueIdentifier()))];
  }

  [snapshot_storage_ renameSnapshotsWithOldIDs:stable_identifiers
                                        newIDs:snapshot_identifiers];
}

void SnapshotBrowserAgent::PurgeUnusedSnapshots() {
  DCHECK(snapshot_storage_);
  NSArray<SnapshotIDWrapper*>* snapshot_ids = GetSnapshotIDs();
  // Keep snapshots that are less than one minute old, to prevent a concurrency
  // issue if they are created while the purge is running.
  const base::Time one_minute_ago = base::Time::Now() - base::Minutes(1);
  [snapshot_storage_
      purgeImagesOlderThanWithThresholdDate:one_minute_ago.ToNSDate()
                            liveSnapshotIDs:snapshot_ids];
}

NSArray<SnapshotIDWrapper*>* SnapshotBrowserAgent::GetSnapshotIDs() {
  WebStateList* web_state_list = browser_->GetWebStateList();
  const int web_state_list_count = web_state_list->count();

  NSMutableArray<SnapshotIDWrapper*>* snapshot_ids =
      [[NSMutableArray alloc] initWithCapacity:web_state_list_count];

  for (int index = 0; index < web_state_list_count; ++index) {
    web::WebState* web_state = web_state_list->GetWebStateAt(index);
    [snapshot_ids
        addObject:ToWrapper(SnapshotID(web_state->GetUniqueIdentifier()))];
  }
  return snapshot_ids;
}
