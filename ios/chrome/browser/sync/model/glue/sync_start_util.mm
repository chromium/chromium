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
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"

namespace ios {
namespace {

void StartSyncOnUIThread(base::WeakPtr<ProfileIOS> weak_profile,
                         syncer::DataType type) {
  ProfileIOS* profile = weak_profile.get();
  if (!profile) {
    DVLOG(2) << "ProfileIOS destroyed, can't start sync.";
    return;
  }

  syncer::SyncService* sync_service =
      SyncServiceFactory::GetForProfile(profile);
  if (!sync_service) {
    DVLOG(2) << "No SyncService for ProfileIOS, can't start sync.";
    return;
  }
  sync_service->OnDataTypeRequestsSyncStartup(type);
}

}  // namespace

namespace sync_start_util {

syncer::SyncableService::StartSyncFlare GetFlareForSyncableService(
    ProfileIOS* profile) {
  return base::BindPostTask(
      base::SequencedTaskRunner::GetCurrentDefault(),
      base::BindRepeating(&StartSyncOnUIThread, profile->AsWeakPtr()));
}

}  // namespace sync_start_util
}  // namespace ios
