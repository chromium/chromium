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
#include "google_apis/gaia/google_service_auth_error.h"

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
  switch (this->GetSyncServiceState()) {
    // No error.
    case SyncSetupService::kNoSyncServiceError:
    // These errors are transient and don't mean that sync is off.
    case SyncSetupService::kSyncServiceCouldNotConnect:
    case SyncSetupService::kSyncServiceServiceUnavailable:
    case SyncSetupService::kSyncServiceTrustedVaultRecoverabilityDegraded:
      return false;
    // These errors effectively amount to disabled sync and require a signin.
    case SyncSetupService::kSyncServiceSignInNeedsUpdate:
    case SyncSetupService::kSyncServiceNeedsPassphrase:
    case SyncSetupService::kSyncServiceUnrecoverableError:
      return true;
    case SyncSetupService::kSyncServiceNeedsTrustedVaultKey:
      return IsEncryptEverythingEnabled();
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
  return sync_service_->GetUserSettings()->IsSyncRequested();
}

bool SyncSetupService::CanSyncFeatureStart() const {
  return sync_service_->CanSyncFeatureStart();
}

void SyncSetupService::SetSyncEnabled(bool sync_enabled) {
  if (!sync_blocker_)
    sync_blocker_ = sync_service_->GetSetupInProgressHandle();
  if (!sync_enabled) {
    UMA_HISTOGRAM_ENUMERATION("Sync.StopSource", syncer::CHROME_SYNC_SETTINGS,
                              syncer::STOP_SOURCE_LIMIT);
  }
  sync_service_->GetUserSettings()->SetSyncRequested(sync_enabled);

  if (sync_enabled && GetPreferredDataTypes().Empty())
    SetSyncingAllDataTypes(true);
}

SyncSetupService::SyncServiceState SyncSetupService::GetSyncServiceState() {
  switch (sync_service_->GetAuthError().state()) {
    case GoogleServiceAuthError::REQUEST_CANCELED:
      return kSyncServiceCouldNotConnect;
    // TODO(crbug.com/1194007): This will support the SyncDisabled policy that
    // can force the Sync service to become unavailable.
    // Based on GetSyncStatusLabelsForAuthError, SERVICE_UNAVAILABLE
    // corresponds to sync having been disabled for the user's domain.
    case GoogleServiceAuthError::SERVICE_UNAVAILABLE:
      return kSyncServiceServiceUnavailable;
    case GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS:
      return kSyncServiceSignInNeedsUpdate;
    // The following errors are not shown to the user.
    case GoogleServiceAuthError::NONE:
    // Connection failed is not shown to the user, as this will happen if the
    // service retuned a 500 error. A more detail error can always be checked
    // on chrome://sync-internals.
    case GoogleServiceAuthError::CONNECTION_FAILED:
    case GoogleServiceAuthError::USER_NOT_SIGNED_UP:
    case GoogleServiceAuthError::UNEXPECTED_SERVICE_RESPONSE:
      break;
    // The following errors are unexpected on iOS.
    case GoogleServiceAuthError::SERVICE_ERROR:
    case GoogleServiceAuthError::SCOPE_LIMITED_UNRECOVERABLE_ERROR:
    // Conventional value for counting the states, never used.
    case GoogleServiceAuthError::NUM_STATES:
      NOTREACHED() << "Unexpected Auth error ("
                   << sync_service_->GetAuthError().state()
                   << "): " << sync_service_->GetAuthError().error_message();
      break;
  }
  if (sync_service_->HasUnrecoverableError())
    return kSyncServiceUnrecoverableError;
  if (sync_service_->GetUserSettings()
          ->IsPassphraseRequiredForPreferredDataTypes()) {
    return kSyncServiceNeedsPassphrase;
  }
  if (sync_service_->GetUserSettings()
          ->IsTrustedVaultKeyRequiredForPreferredDataTypes()) {
    return kSyncServiceNeedsTrustedVaultKey;
  }
  if (sync_service_->GetUserSettings()
          ->IsTrustedVaultRecoverabilityDegraded()) {
    return kSyncServiceTrustedVaultRecoverabilityDegraded;
  }
  return kNoSyncServiceError;
}

bool SyncSetupService::IsEncryptEverythingEnabled() const {
  return sync_service_->GetUserSettings()->IsEncryptEverythingEnabled();
}

bool SyncSetupService::IsInitialSetupOngoing() {
  // Sync initial setup is considered to finished iff:
  //   1. User is signed in with sync enabled and the sync setup was completed.
  //   OR
  //   2. User is not signed in or has disabled sync.
  // Otherwise we consider that the initial setup is still pending.
  // Note that if the user visits the Advanced Settings during the opt-in flow,
  // the Sync consent is not granted yet. In this case, IsSyncRequested() is
  // set to true, indicating that the sync was requested but the initial setup
  // has not been finished yet.
  return IsSyncRequested() &&
         !sync_service_->GetUserSettings()->IsFirstSetupComplete();
}

void SyncSetupService::PrepareForFirstSyncSetup() {
  // `PrepareForFirstSyncSetup` should always be called while the user is signed
  // out. At that time, sync setup is not completed.
  DCHECK(!sync_service_->GetUserSettings()->IsFirstSetupComplete());
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
