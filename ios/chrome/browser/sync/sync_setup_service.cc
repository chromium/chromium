// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/sync/sync_setup_service.h"

#include <stdio.h>

#include "base/metrics/histogram_macros.h"
#include "components/sync/base/stop_source.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/sync_user_settings.h"

namespace {
// The set of user-selectable datatypes. This must be in the same order as
// `SyncSetupService::SyncableDatatype`.
syncer::ModelType kDataTypes[] = {
    syncer::BOOKMARKS,    syncer::TYPED_URLS, syncer::PASSWORDS,
    syncer::PROXY_TABS,   syncer::AUTOFILL,   syncer::PREFERENCES,
    syncer::READING_LIST,
};
}  // namespace

SyncSetupService::SyncSetupService(syncer::SyncService* sync_service)
    : sync_service_(sync_service) {
  DCHECK(sync_service_);
}

SyncSetupService::~SyncSetupService() {}

// static
syncer::ModelType SyncSetupService::GetModelType(SyncableDatatype datatype) {
  DCHECK(datatype < std::size(kDataTypes));
  return kDataTypes[datatype];
}

syncer::ModelTypeSet SyncSetupService::GetPreferredDataTypes() const {
  return sync_service_->GetPreferredDataTypes();
}

bool SyncSetupService::IsDataTypeActive(syncer::ModelType datatype) const {
  return sync_service_->GetActiveDataTypes().Has(datatype);
}

bool SyncSetupService::IsDataTypePreferred(syncer::ModelType datatype) const {
  return GetPreferredDataTypes().Has(datatype);
}

void SyncSetupService::SetDataTypeEnabled(syncer::ModelType datatype,
                                          bool enabled) {
  if (!sync_blocker_)
    sync_blocker_ = sync_service_->GetSetupInProgressHandle();
  syncer::ModelTypeSet model_types = GetPreferredDataTypes();
  if (enabled)
    model_types.Put(datatype);
  else
    model_types.Remove(datatype);
  syncer::SyncUserSettings* user_settings = sync_service_->GetUserSettings();
  // TODO(crbug.com/950874): support syncer::UserSelectableType in ios code,
  // get rid of this workaround and consider getting rid of SyncableDatatype.
  syncer::UserSelectableTypeSet selected_types;
  for (syncer::UserSelectableType type :
       user_settings->GetRegisteredSelectableTypes()) {
    if (model_types.Has(syncer::UserSelectableTypeToCanonicalModelType(type))) {
      selected_types.Put(type);
    }
  }
  user_settings->SetSelectedTypes(IsSyncingAllDataTypes(), selected_types);
}

bool SyncSetupService::UserActionIsRequiredToHaveTabSyncWork() {
  if (!CanSyncFeatureStart() || !IsDataTypePreferred(syncer::PROXY_TABS)) {
    return true;
  }

  switch (sync_service_->GetUserActionableError()) {
    // No error.
    case syncer::SyncService::UserActionableError::kNone:
      return false;

    // These errors effectively amount to disabled sync or effectively paused.
    case syncer::SyncService::UserActionableError::kSignInNeedsUpdate:
    case syncer::SyncService::UserActionableError::kNeedsPassphrase:
    case syncer::SyncService::UserActionableError::kGenericUnrecoverableError:
    case syncer::SyncService::UserActionableError::
        kNeedsTrustedVaultKeyForEverything:
      return true;

    // This error doesn't stop tab sync.
    case syncer::SyncService::UserActionableError::
        kNeedsTrustedVaultKeyForPasswords:
      return false;

    // These errors don't actually stop sync.
    case syncer::SyncService::UserActionableError::
        kTrustedVaultRecoverabilityDegradedForPasswords:
    case syncer::SyncService::UserActionableError::
        kTrustedVaultRecoverabilityDegradedForEverything:
      return false;
  }

  NOTREACHED() << "Unknown sync service state.";
  return true;
}

bool SyncSetupService::IsSyncingAllDataTypes() const {
  return sync_service_->GetUserSettings()->IsSyncEverythingEnabled();
}

void SyncSetupService::SetSyncingAllDataTypes(bool sync_all) {
  if (!sync_blocker_)
    sync_blocker_ = sync_service_->GetSetupInProgressHandle();
  sync_service_->GetUserSettings()->SetSelectedTypes(
      sync_all, sync_service_->GetUserSettings()->GetSelectedTypes());
}

bool SyncSetupService::IsSyncRequested() const {
  return !sync_service_->GetDisableReasons().Has(
      syncer::SyncService::DISABLE_REASON_USER_CHOICE);
}

bool SyncSetupService::CanSyncFeatureStart() const {
  return sync_service_->CanSyncFeatureStart();
}

bool SyncSetupService::IsEncryptEverythingEnabled() const {
  return sync_service_->GetUserSettings()->IsEncryptEverythingEnabled();
}

void SyncSetupService::PrepareForFirstSyncSetup() {
  if (!sync_blocker_)
    sync_blocker_ = sync_service_->GetSetupInProgressHandle();
}

void SyncSetupService::SetFirstSetupComplete(
    syncer::SyncFirstSetupCompleteSource source) {
  DCHECK(sync_blocker_);
  // Turn on the sync setup completed flag only if the user did not turn sync
  // off.
  if (sync_service_->CanSyncFeatureStart()) {
    sync_service_->GetUserSettings()->SetFirstSetupComplete(source);
  }
}

bool SyncSetupService::IsFirstSetupComplete() const {
  return sync_service_->GetUserSettings()->IsFirstSetupComplete();
}

void SyncSetupService::CommitSyncChanges() {
  sync_blocker_.reset();
}

bool SyncSetupService::HasUncommittedChanges() {
  return sync_service_->IsSetupInProgress();
}
