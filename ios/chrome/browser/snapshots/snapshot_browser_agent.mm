// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/snapshots/snapshot_browser_agent.h"

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/snapshots/snapshot_cache.h"
#import "ios/chrome/browser/snapshots/snapshot_tab_helper.h"
#import "ios/chrome/browser/ui/util/multi_window_support.h"
#import "ios/chrome/browser/web/tab_id_tab_helper.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

const base::FilePath::CharType kLegacyBaseDirectory[] =
    FILE_PATH_LITERAL("Chromium");
const base::FilePath::CharType kSessionsDirectory[] =
    FILE_PATH_LITERAL("Sessions");
const base::FilePath::CharType kSnapshotsDirectory[] =
    FILE_PATH_LITERAL("Snapshots");

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

void SnapshotBrowserAgent::SetSessionID(const std::string& session_identifier) {
  // It's probably incorrect to set this more than once.
  DCHECK(session_identifier_.empty() ||
         session_identifier_ == session_identifier);
  session_identifier_ = session_identifier;
  snapshot_cache_ =
      [[SnapshotCache alloc] initWithStoragePath:GetStoragePath()];
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
  legacy_directory =
      legacy_directory.Append(kLegacyBaseDirectory).Append(kSnapshotsDirectory);
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
  const base::Time one_minute_ago =
      base::Time::Now() - base::TimeDelta::FromMinutes(1);
  [snapshot_cache_ purgeCacheOlderThan:one_minute_ago keeping:snapshot_ids];
}

base::FilePath SnapshotBrowserAgent::GetStoragePath() {
  // TODO(crbug.com/1117317): This method should only need to append the
  // snapshots folder to a base path that already includes the browser state
  // path, sessions directory, and the session identifier.
  base::FilePath path = browser_->GetBrowserState()->GetStatePath();
  if (IsSceneStartupSupported() && !session_identifier_.empty()) {
    path = path.Append(kSessionsDirectory)
               .Append(session_identifier_)
               .Append(kSnapshotsDirectory);
  } else {
    path = path.Append(kSnapshotsDirectory);
  }
  return path;
}

NSSet<NSString*>* SnapshotBrowserAgent::GetTabIDs() {
  WebStateList* web_state_list = browser_->GetWebStateList();
  NSMutableSet<NSString*>* tab_ids =
      [NSMutableSet setWithCapacity:web_state_list->count()];
  for (int index = 0; index < web_state_list->count(); ++index) {
    web::WebState* web_state = web_state_list->GetWebStateAt(index);
    [tab_ids addObject:TabIdTabHelper::FromWebState(web_state)->tab_id()];
  }
  return tab_ids;
}
