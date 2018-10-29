// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/sync/sync_setup_service.h"

#include <stdio.h>

#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "components/prefs/pref_service.h"
#include "components/sync/base/stop_source.h"
#include "components/sync/base/sync_prefs.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/protocol/sync_protocol_error.h"
#include "components/unified_consent/feature.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/pref_names.h"
#include "net/base/network_change_notifier.h"

namespace {
// The set of user-selectable datatypes. This must be in the same order as
// |SyncSetupService::SyncableDatatype|.
syncer::ModelType kDataTypes[] = {
    syncer::BOOKMARKS,    syncer::TYPED_URLS,  syncer::PASSWORDS,
    syncer::PROXY_TABS,   syncer::AUTOFILL,    syncer::PREFERENCES,
    syncer::READING_LIST, syncer::USER_EVENTS,
};
}  // namespace

SyncSetupService::SyncSetupService(syncer::SyncService* sync_service,
                                   PrefService* prefs)
    : sync_service_(sync_service), prefs_(prefs) {
  DCHECK(sync_service_);
  DCHECK(prefs_);
  for (unsigned int i = 0; i < base::size(kDataTypes); ++i) {
    if (kDataTypes[i] == syncer::USER_EVENTS &&
        !unified_consent::IsUnifiedConsentFeatureEnabled())
      continue;
    user_selectable_types_.Put(kDataTypes[i]);
  }
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
  syncer::ModelTypeSet types = GetPreferredDataTypes();
  if (enabled)
    types.Put(datatype);
  else
    types.Remove(datatype);
  types.RetainAll(user_selectable_types_);
  if (enabled && !IsSyncEnabled())
    SetSyncEnabledWithoutChangingDatatypes(true);
  sync_service_->OnUserChoseDatatypes(IsSyncingAllDataTypes(), types);
  if (GetPreferredDataTypes().Empty())
    SetSyncEnabled(false);
}

bool SyncSetupService::UserActionIsRequiredToHaveSyncWork() {
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
      return true;
    default:
      NOTREACHED() << "Unknown sync service state.";
      return true;
  }
}

bool SyncSetupService::IsSyncingAllDataTypes() const {
  syncer::SyncPrefs sync_prefs(prefs_);
  return sync_prefs.HasKeepEverythingSynced();
}

void SyncSetupService::SetSyncingAllDataTypes(bool sync_all) {
  if (!sync_blocker_)
    sync_blocker_ = sync_service_->GetSetupInProgressHandle();
  if (sync_all && !IsSyncEnabled())
    SetSyncEnabled(true);
  sync_service_->OnUserChoseDatatypes(
      sync_all,
      Intersection(GetPreferredDataTypes(), syncer::UserSelectableTypes()));
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
    case GoogleServiceAuthError::CAPTCHA_REQUIRED:
    case GoogleServiceAuthError::ACCOUNT_DELETED:
    case GoogleServiceAuthError::ACCOUNT_DISABLED:
    case GoogleServiceAuthError::TWO_FACTOR:
    case GoogleServiceAuthError::HOSTED_NOT_ALLOWED_DEPRECATED:
    case GoogleServiceAuthError::SERVICE_ERROR:
    case GoogleServiceAuthError::WEB_LOGIN_REQUIRED:
    // Conventional value for counting the states, never used.
    case GoogleServiceAuthError::NUM_STATES:
      NOTREACHED() << "Unexpected Auth error ("
                   << sync_service_->GetAuthError().state()
                   << "): " << sync_service_->GetAuthError().error_message();
      break;
  }
  if (sync_service_->HasUnrecoverableError())
    return kSyncServiceUnrecoverableError;
  if (sync_service_->IsPassphraseRequiredForDecryption())
    return kSyncServiceNeedsPassphrase;
  return kNoSyncServiceError;
}

bool SyncSetupService::HasFinishedInitialSetup() {
  // Sync initial setup is considered to finished iff:
  //   1. User is signed in with sync enabled and the sync setup was completed.
  //   OR
  //   2. User is not signed in or has disabled sync.
  return !sync_service_->CanSyncFeatureStart() ||
         sync_service_->IsFirstSetupComplete();
}

void SyncSetupService::PrepareForFirstSyncSetup() {
  // |PrepareForFirstSyncSetup| should always be called while the user is signed
  // out. At that time, sync setup is not completed.
  DCHECK(!sync_service_->IsFirstSetupComplete());
  if (!sync_blocker_)
    sync_blocker_ = sync_service_->GetSetupInProgressHandle();
}

void SyncSetupService::CommitChanges() {
  if (sync_service_->IsFirstSetupInProgress()) {
    // Turn on the sync setup completed flag only if the user did not turn sync
    // off.
    if (sync_service_->CanSyncFeatureStart()) {
      sync_service_->SetFirstSetupComplete();
    }
  }

  sync_blocker_.reset();
}

bool SyncSetupService::HasUncommittedChanges() {
  return sync_service_->IsSetupInProgress();
}

void SyncSetupService::SetSyncEnabledWithoutChangingDatatypes(
    bool sync_enabled) {
  if (!sync_blocker_)
    sync_blocker_ = sync_service_->GetSetupInProgressHandle();
  if (sync_enabled) {
    sync_service_->RequestStart();
  } else {
    UMA_HISTOGRAM_ENUMERATION("Sync.StopSource", syncer::CHROME_SYNC_SETTINGS,
                              syncer::STOP_SOURCE_LIMIT);
    sync_service_->RequestStop(syncer::SyncService::KEEP_DATA);
  }
}
