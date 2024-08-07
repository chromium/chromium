// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sync/model/glue/sync_start_util.h"

#import "base/functional/bind.h"
#import "base/location.h"
#import "base/memory/weak_ptr.h"
#import "base/task/bind_post_task.h"
#import "base/task/sequenced_task_runner.h"
#import "components/sync/service/sync_service.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"

namespace ios {
namespace {

void StartSyncOnUIThread(base::WeakPtr<ChromeBrowserState> weak_browser_state,
                         syncer::DataType type) {
  ChromeBrowserState* browser_state = weak_browser_state.get();
  if (!browser_state) {
    DVLOG(2) << "ChromeBrowserState destroyed, can't start sync.";
    return;
  }

  syncer::SyncService* sync_service =
      SyncServiceFactory::GetForBrowserState(browser_state);
  if (!sync_service) {
    DVLOG(2) << "No SyncService for ChromeBrowserState, can't start sync.";
    return;
  }
  sync_service->OnDataTypeRequestsSyncStartup(type);
}

}  // namespace

namespace sync_start_util {

syncer::SyncableService::StartSyncFlare GetFlareForSyncableService(
    ChromeBrowserState* browser_state) {
  return base::BindPostTask(
      base::SequencedTaskRunner::GetCurrentDefault(),
      base::BindRepeating(&StartSyncOnUIThread, browser_state->AsWeakPtr()));
}

}  // namespace sync_start_util
}  // namespace ios
