// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/snapshots/snapshot_browser_agent.h"

#import "base/base_paths.h"
#import "base/files/file_path.h"
#import "base/ios/ios_util.h"
#import "base/path_service.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/sessions/scene_util.h"
#import "ios/chrome/browser/snapshots/snapshot_cache.h"
#import "ios/chrome/browser/snapshots/snapshot_tab_helper.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

const base::FilePath::CharType kLegacyBaseDirectory[] =
    FILE_PATH_LITERAL("Chromium");

}  // namespace

BROWSER_USER_DATA_KEY_IMPL(SnapshotBrowserAgent)

SnapshotBrowserAgent::SnapshotBrowserAgent(Browser* browser)
    : browser_(browser) {
  browser->AddObserver(this);
  browser->GetWebStateList()->AddObserver(this);
}

SnapshotBrowserAgent::~SnapshotBrowserAgent() = default;

// Browser Observer methods:
void SnapshotBrowserAgent::BrowserDestroyed(Browser* browser) {
  DCHECK_EQ(browser, browser_);
  browser->GetWebStateList()->RemoveObserver(this);
  browser->RemoveObserver(this);
  browser_ = nullptr;
  [snapshot_cache_ shutdown];
}

void SnapshotBrowserAgent::WebStateInsertedAt(WebStateList* web_state_list,
                                              web::WebState* web_state,
                                              int index,
                                              bool activating) {
  SnapshotTabHelper::FromWebState(web_state)->SetSnapshotCache(snapshot_cache_);
}

void SnapshotBrowserAgent::WebStateReplacedAt(WebStateList* web_state_list,
                                              web::WebState* old_web_state,
                                              web::WebState* new_web_state,
                                              int index) {
  SnapshotTabHelper::FromWebState(old_web_state)->SetSnapshotCache(nil);
  SnapshotTabHelper::FromWebState(new_web_state)
      ->SetSnapshotCache(snapshot_cache_);
}

void SnapshotBrowserAgent::WebStateDetachedAt(WebStateList* web_state_list,
                                              web::WebState* web_state,
                                              int index) {
  SnapshotTabHelper::FromWebState(web_state)->SetSnapshotCache(nil);
}

void SnapshotBrowserAgent::WillBeginBatchOperation(
    WebStateList* web_state_list) {
  for (int i = 0; i < web_state_list->count(); ++i) {
    web::WebState* web_state = web_state_list->GetWebStateAt(i);
    SnapshotTabHelper::FromWebState(web_state)->SetSnapshotCache(nil);
  }
}

void SnapshotBrowserAgent::BatchOperationEnded(WebStateList* web_state_list) {
  for (int i = 0; i < web_state_list->count(); ++i) {
    web::WebState* web_state = web_state_list->GetWebStateAt(i);
    SnapshotTabHelper::FromWebState(web_state)->SetSnapshotCache(
        snapshot_cache_);
  }
}

void SnapshotBrowserAgent::SetSessionID(NSString* session_identifier) {
  // It is incorrect to call this method twice.
  DCHECK(!snapshot_cache_);
  const base::FilePath storage_path =
      SessionPathForDirectory(browser_->GetBrowserState()->GetStatePath(),
                              session_identifier, kSnapshotsDirectoryName);
  snapshot_cache_ = [[SnapshotCache alloc] initWithStoragePath:storage_path];
}

void SnapshotBrowserAgent::PerformStorageMaintenance() {
  MigrateStorageIfNecessary();
  PurgeUnusedSnapshots();
}

void SnapshotBrowserAgent::RemoveAllSnapshots() {
  [snapshot_cache_ removeAllImages];
}

void SnapshotBrowserAgent::MigrateStorageIfNecessary() {
  DCHECK(snapshot_cache_);
  base::FilePath legacy_directory;
  DCHECK(base::PathService::Get(base::DIR_CACHE, &legacy_directory));
  legacy_directory = legacy_directory.Append(kLegacyBaseDirectory)
                         .Append(kSnapshotsDirectoryName);
  // The legacy directory is deleted in migration, and migration is NO-OP if
  // directory does not exist.
  [snapshot_cache_ migrateSnapshotsWithIDs:GetTabIDs()
                            fromSourcePath:legacy_directory];
}

void SnapshotBrowserAgent::PurgeUnusedSnapshots() {
  DCHECK(snapshot_cache_);
  NSSet<NSString*>* snapshot_ids = GetTabIDs();
  // Keep snapshots that are less than one minute old, to prevent a concurrency
  // issue if they are created while the purge is running.
  const base::Time one_minute_ago = base::Time::Now() - base::Minutes(1);
  [snapshot_cache_ purgeCacheOlderThan:one_minute_ago keeping:snapshot_ids];
}

NSSet<NSString*>* SnapshotBrowserAgent::GetTabIDs() {
  WebStateList* web_state_list = browser_->GetWebStateList();
  NSMutableSet<NSString*>* tab_ids =
      [NSMutableSet setWithCapacity:web_state_list->count()];
  for (int index = 0; index < web_state_list->count(); ++index) {
    web::WebState* web_state = web_state_list->GetWebStateAt(index);
    [tab_ids addObject:web_state->GetStableIdentifier()];
  }
  return tab_ids;
}
