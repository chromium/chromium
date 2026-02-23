// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/home_customization/model/theme_syncable_service_ios.h"

#import "base/auto_reset.h"
#import "base/check.h"
#import "base/check_op.h"
#import "base/location.h"
#import "base/metrics/histogram_functions.h"
#import "components/sync/model/model_error.h"
#import "components/sync/protocol/entity_specifics.pb.h"
#import "ios/chrome/browser/home_customization/model/theme_syncable_service_ios_constants.h"
#import "ios/chrome/browser/home_customization/utils/theme_ios_specifics_utils.h"

namespace {

using ::syncer::ModelError;
using ::syncer::SyncChange;
using ::syncer::SyncChangeList;
using ::syncer::SyncChangeProcessor;
using ::syncer::SyncData;
using ::syncer::SyncDataList;

// "current_theme" is the legacy client tag used on Desktop; "current_theme_ios"
// is distinct for iOS to avoid any potential collision or confusion.
constexpr char kSyncEntityClientTag[] = "current_theme_ios";

// "Current Theme" is the legacy entity title used on Desktop; "Current iOS
// Theme" is distinct for iOS to avoid any potential collision or confusion.
constexpr char kSyncEntityTitle[] = "Current iOS Theme";

}  // namespace

ThemeSyncableServiceIOS::ThemeSyncableServiceIOS(Delegate* delegate)
    : delegate_(delegate) {
  CHECK(delegate_);
}

ThemeSyncableServiceIOS::~ThemeSyncableServiceIOS() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void ThemeSyncableServiceIOS::WaitUntilReadyToSync(base::OnceClosure done) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::move(done).Run();
}

void ThemeSyncableServiceIOS::WillStartInitialSync() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Save the pre-sync local theme so it can be restored if the user signs out.
  delegate_->CacheLocalTheme();
}

std::optional<ModelError> ThemeSyncableServiceIOS::MergeDataAndStartSyncing(
    syncer::DataType type,
    const SyncDataList& initial_sync_data,
    std::unique_ptr<SyncChangeProcessor> sync_processor) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(sync_processor);
  CHECK_EQ(type, syncer::THEMES_IOS);

  sync_processor_ = std::move(sync_processor);

  if (initial_sync_data.size() > 1) {
    base::UmaHistogramEnumeration(
        kThemeSyncInitialState,
        IOSThemeSyncInitialState::kTooManySpecificsError);
    return ModelError(FROM_HERE, ModelError::Type::kThemeTooManySpecifics);
  }

  // If the Sync server has no data during the initial setup flow, do NOT upload
  // the local theme to the server. The server remains empty until an explicit
  // manual change is made.
  if (initial_sync_data.empty()) {
    base::UmaHistogramEnumeration(kThemeSyncInitialState,
                                  IOSThemeSyncInitialState::kEmptyServer);
    return std::nullopt;
  }

  base::UmaHistogramEnumeration(kThemeSyncInitialState,
                                IOSThemeSyncInitialState::kHasRemoteData);
  return ValidateAndApplyRemoteTheme(initial_sync_data[0]);
}

void ThemeSyncableServiceIOS::StopSyncing(syncer::DataType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_EQ(type, syncer::THEMES_IOS);

  StopSyncingAndRevertToLocalTheme();
}

void ThemeSyncableServiceIOS::OnBrowserShutdown(syncer::DataType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_EQ(type, syncer::THEMES_IOS);

  sync_processor_.reset();
}

void ThemeSyncableServiceIOS::StayStoppedAndMaybeClearData(
    syncer::DataType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_EQ(type, syncer::THEMES_IOS);

  // For this service, "clearing data" means reverting to the pre-sync theme.
  StopSyncingAndRevertToLocalTheme();
}

std::optional<ModelError> ThemeSyncableServiceIOS::ProcessSyncChanges(
    const base::Location& from_here,
    const SyncChangeList& change_list) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!sync_processor_) {
    return ModelError(FROM_HERE,
                      ModelError::Type::kThemeSyncableServiceNotStarted);
  }

  // The iOS Theme is a single entity, so there should never be multiple
  // changes.
  if (change_list.size() != 1) {
    base::UmaHistogramEnumeration(
        kThemeSyncRemoteAction, IOSThemeSyncRemoteAction::kTooManyChangesError);
    return ModelError(FROM_HERE, ModelError::Type::kThemeTooManyChanges);
  }

  const SyncChange& change = change_list[0];

  // To mirror Desktop, treat `ACTION_DELETE` as an error since themes are only
  // ever added or updated.
  if (change.change_type() != SyncChange::ACTION_ADD &&
      change.change_type() != SyncChange::ACTION_UPDATE) {
    base::UmaHistogramEnumeration(
        kThemeSyncRemoteAction,
        IOSThemeSyncRemoteAction::kInvalidChangeTypeError);
    return ModelError(FROM_HERE, ModelError::Type::kThemeInvalidChangeType);
  }

  return ValidateAndApplyRemoteTheme(change.sync_data());
}

std::string ThemeSyncableServiceIOS::GetClientTag(
    const syncer::EntityData& entity_data) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // iOS Theme always returns the same client tag as there is only one single
  // iOS theme entity.
  return kSyncEntityClientTag;
}

void ThemeSyncableServiceIOS::OnThemeChanged() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // If currently applying a change from the server, or sync isn't ready, do not
  // upload anything. This prevents an infinite loop.
  if (processing_syncer_changes_ || !sync_processor_) {
    return;
  }

  if (!delegate_->IsCurrentThemeSyncable()) {
    return;
  }

  sync_pb::EntitySpecifics specifics;
  *specifics.mutable_theme_ios() = delegate_->GetCurrentTheme();

  SyncData sync_data = SyncData::CreateLocalData(kSyncEntityClientTag,
                                                 kSyncEntityTitle, specifics);

  sync_processor_->ProcessSyncChanges(
      FROM_HERE, {SyncChange(FROM_HERE, SyncChange::ACTION_UPDATE, sync_data)});
}

std::optional<ModelError> ThemeSyncableServiceIOS::ValidateAndApplyRemoteTheme(
    const SyncData& sync_data) {
  if (!sync_data.GetSpecifics().has_theme_ios()) {
    base::UmaHistogramEnumeration(kThemeSyncRemoteAction,
                                  IOSThemeSyncRemoteAction::kMissingSpecifics);
    return ModelError(FROM_HERE, ModelError::Type::kThemeMissingSpecifics);
  }

  const sync_pb::ThemeIosSpecifics& remote_theme =
      sync_data.GetSpecifics().theme_ios();

  if (delegate_->IsCurrentThemeManagedByPolicy()) {
    base::UmaHistogramEnumeration(
        kThemeSyncRemoteAction,
        IOSThemeSyncRemoteAction::kIgnoredManagedByPolicy);
    return std::nullopt;
  }

  if (home_customization::AreThemeIosSpecificsEquivalent(
          delegate_->GetCurrentTheme(), remote_theme)) {
    base::UmaHistogramEnumeration(
        kThemeSyncRemoteAction,
        IOSThemeSyncRemoteAction::kIgnoredAlreadyMatches);
    return std::nullopt;
  }

  base::UmaHistogramEnumeration(kThemeSyncRemoteAction,
                                IOSThemeSyncRemoteAction::kApplied);

  base::AutoReset<bool> processing_changes(&processing_syncer_changes_, true);

  delegate_->ApplyTheme(remote_theme);

  return std::nullopt;
}

void ThemeSyncableServiceIOS::StopSyncingAndRevertToLocalTheme() {
  if (!sync_processor_) {
    return;
  }

  sync_processor_.reset();

  delegate_->RestoreCachedTheme();

  base::UmaHistogramEnumeration(kThemeSyncStopAction,
                                IOSThemeSyncStopAction::kRestoredLocalTheme);
}

base::WeakPtr<syncer::SyncableService> ThemeSyncableServiceIOS::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}
