// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sync/model/glue/sync_start_util.h"

#import "base/files/file_path.h"
#import "base/functional/bind.h"
#import "base/location.h"
#import "components/sync/service/sync_service.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state_manager.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/web/public/thread/web_task_traits.h"
#import "ios/web/public/thread/web_thread.h"

namespace ios {
namespace {

void StartSyncOnUIThread(const base::FilePath& browser_state_path,
                         syncer::ModelType type) {
  ChromeBrowserStateManager* browser_state_manager =
      GetApplicationContext()->GetChromeBrowserStateManager();
  if (!browser_state_manager) {
    // Can happen in tests.
    DVLOG(2) << "No ChromeBrowserStateManager, can't start sync.";
    return;
  }

  ChromeBrowserState* browser_state =
      browser_state_manager->GetBrowserState(browser_state_path);
  if (!browser_state) {
    DVLOG(2) << "ChromeBrowserState not found, can't start sync.";
    return;
  }

  syncer::SyncService* sync_service =
      SyncServiceFactory::GetForBrowserState(browser_state);
  if (!sync_service) {
    DVLOG(2) << "No SyncService for browser state, can't start sync.";
    return;
  }
  sync_service->OnDataTypeRequestsSyncStartup(type);
}

void StartSyncProxy(const base::FilePath& browser_state_path,
                    syncer::ModelType type) {
  web::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&StartSyncOnUIThread, browser_state_path, type));
}

}  // namespace

namespace sync_start_util {

syncer::SyncableService::StartSyncFlare GetFlareForSyncableService(
    const base::FilePath& browser_state_path) {
  return base::BindRepeating(&StartSyncProxy, browser_state_path);
}

}  // namespace sync_start_util
}  // namespace ios
