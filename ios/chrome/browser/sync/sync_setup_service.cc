// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/sync/sync_setup_service.h"

#include <stdio.h>

#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "components/sync/base/stop_source.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/sync_user_settings.h"
#include "google_apis/gaia/google_service_auth_error.h"

namespace {
// The set of user-selectable datatypes. This must be in the same order as
// |SyncSetupService::SyncableDatatype|.
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

SyncSetupService::~SyncSetupService() {
}

syncer::ModelType SyncSetupService::GetModelType(SyncableDatatype datatype) {
  DCHECK(datatype < base::size(kDataTypes));
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
  // TODO(crbug.com/950874): support syncer::UserSelectableType in ios code,
  // get rid of this workaround and consider getting rid of SyncableDatatype.
  syncer::UserSelectableTypeSet selected_types;
  for (syncer::UserSelectableType type : syncer::UserSelectableTypeSet::All()) {
    if (model_types.Has(syncer::UserSelectableTypeToCanonicalModelType(type))) {
      selected_types.Put(type);
    }
  }
  if (enabled && !IsSyncEnabled())
    SetSyncEnabledWithoutChangingDatatypes(true);
  sync_service_->GetUserSettings()->SetSelectedTypes(IsSyncingAllDataTypes(),
                                                     selected_types);
  if (GetPreferredDataTypes().Empty())
    SetSyncEnabled(false);
}

bool SyncSetupService::UserActionIsRequiredToHaveTabSyncWork() {
  if (!IsSyncEnabled() || !IsDataTypePreferred(syncer::PROXY_TABS)) {
    return true;
  }
  switch (this->GetSyncServiceState()) {
    // No error.
    case SyncSetupService::kNoSyncServiceError:
    // These errors are transient and don't mean that sync is off.
    case SyncSetupService::kSyncServiceCouldNotConnect:
    case SyncSetupService::kSyncServiceServiceUnavailable:
      return false;
    // These errors effectively amount to disabled sync and require a signin.
    case SyncSetupService::kSyncServiceSignInNeedsUpdate:
    case SyncSetupService::kSyncServiceNeedsPassphrase:
    case SyncSetupService::kSyncServiceUnrecoverableError:
    case SyncSetupService::kSyncSettingsNotConfirmed:
      return true;
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
  if (sync_all && !IsSyncEnabled())
    SetSyncEnabled(true);
  sync_service_->GetUserSettings()->SetSelectedTypes(
      sync_all, sync_service_->GetUserSettings()->GetSelectedTypes());
}

bool SyncSetupService::IsSyncEnabled() const {
  return sync_service_->CanSyncFeatureStart();
}

void SyncSetupService::SetSyncEnabled(bool sync_enabled) {
  SetSyncEnabledWithoutChangingDatatypes(sync_enabled);
  if (sync_enabled && GetPreferredDataTypes().Empty())
    SetSyncingAllDataTypes(true);
}

SyncSetupService::SyncServiceState SyncSetupService::GetSyncServiceState() {
  switch (sync_service_->GetAuthError().state()) {
    case GoogleServiceAuthError::REQUEST_CANCELED:
      return kSyncServiceCouldNotConnect;
    // Based on sync_ui_util::GetStatusLabelsForAuthError, SERVICE_UNAVAILABLE
    // corresponds to sync having been disabled for the user's domain.
    case GoogleServiceAuthError::SERVICE_UNAVAILABLE:
      return kSyncServiceServiceUnavailable;
    case GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS:
      return kSyncServiceSignInNeedsUpdate;
    // The following errors are not shown to the user.
    case GoogleServiceAuthError::NONE:
    // Connection failed is not shown to the user, as this will happen if the
    // service retuned a 500 error. A more detail error can always be checked
    // on about:sync.
    case GoogleServiceAuthError::CONNECTION_FAILED:
    case GoogleServiceAuthError::USER_NOT_SIGNED_UP:
    case GoogleServiceAuthError::UNEXPECTED_SERVICE_RESPONSE:
      break;
    // The following errors are unexpected on iOS.
    case GoogleServiceAuthError::SERVICE_ERROR:
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
          ->IsPassphraseRequiredForPreferredDataTypes())
    return kSyncServiceNeedsPassphrase;
  if (!IsFirstSetupComplete() && IsSyncEnabled())
    return kSyncSettingsNotConfirmed;
  return kNoSyncServiceError;
}

bool SyncSetupService::HasFinishedInitialSetup() {
  // Sync initial setup is considered to finished iff:
  //   1. User is signed in with sync enabled and the sync setup was completed.
  //   OR
  //   2. User is not signed in or has disabled sync.
  return !sync_service_->CanSyncFeatureStart() ||
         sync_service_->GetUserSettings()->IsFirstSetupComplete();
}

void SyncSetupService::PrepareForFirstSyncSetup() {
  // |PrepareForFirstSyncSetup| should always be called while the user is signed
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

void SyncSetupService::SetSyncEnabledWithoutChangingDatatypes(
    bool sync_enabled) {
  if (!sync_blocker_)
    sync_blocker_ = sync_service_->GetSetupInProgressHandle();
  if (!sync_enabled) {
    UMA_HISTOGRAM_ENUMERATION("Sync.StopSource", syncer::CHROME_SYNC_SETTINGS,
                              syncer::STOP_SOURCE_LIMIT);
  }
  sync_service_->GetUserSettings()->SetSyncRequested(sync_enabled);
}
