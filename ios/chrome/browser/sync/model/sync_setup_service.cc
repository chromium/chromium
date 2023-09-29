// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/sync/model/sync_setup_service.h"

#include <stdio.h>

#include "base/metrics/histogram_macros.h"
#include "components/sync/base/stop_source.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"

SyncSetupService::SyncSetupService(syncer::SyncService* sync_service)
    : sync_service_(sync_service) {
  DCHECK(sync_service_);
}

SyncSetupService::~SyncSetupService() {}

void SyncSetupService::PrepareForFirstSyncSetup() {
  if (!sync_blocker_) {
    sync_blocker_ = sync_service_->GetSetupInProgressHandle();
  }
}

void SyncSetupService::SetInitialSyncFeatureSetupComplete(
    syncer::SyncFirstSetupCompleteSource source) {
  CHECK(sync_blocker_);
  // Turn on the sync setup completed flag only if the user did not turn sync
  // off.
  // TODO(crbug.com/1462858): Remove this code once
  // kReplaceSyncPromosWithSignInPromos launches.
  if (sync_service_->CanSyncFeatureStart()) {
    sync_service_->GetUserSettings()->SetInitialSyncFeatureSetupComplete(
        source);
  }
}

void SyncSetupService::CommitSyncChanges() {
  sync_blocker_.reset();
}

bool SyncSetupService::HasUncommittedChanges() {
  return sync_service_->IsSetupInProgress();
}
