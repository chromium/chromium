// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/home_customization/model/theme_syncable_service_ios.h"

#import "base/check.h"
#import "base/check_op.h"
#import "base/location.h"
#import "components/sync/model/model_error.h"

namespace {

// "current_theme" is the legacy client tag used on Desktop; "current_theme_ios"
// is distinct for iOS to avoid any potential collision or confusion.
constexpr char kSyncEntityClientTag[] = "current_theme_ios";

}  // namespace

ThemeSyncableServiceIOS::ThemeSyncableServiceIOS() = default;

ThemeSyncableServiceIOS::~ThemeSyncableServiceIOS() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void ThemeSyncableServiceIOS::WaitUntilReadyToSync(base::OnceClosure done) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::move(done).Run();
}

void ThemeSyncableServiceIOS::WillStartInitialSync() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(crbug.com/481713548): Save current `ThemeIosSpecifics` to prefs. This
  // is used to restore the local theme upon signout.
}

std::optional<syncer::ModelError>
ThemeSyncableServiceIOS::MergeDataAndStartSyncing(
    syncer::DataType type,
    const syncer::SyncDataList& initial_sync_data,
    std::unique_ptr<syncer::SyncChangeProcessor> sync_processor) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(sync_processor);
  CHECK_EQ(type, syncer::THEMES_IOS);

  sync_processor_ = std::move(sync_processor);

  if (initial_sync_data.size() > 1) {
    return syncer::ModelError(FROM_HERE,
                              syncer::ModelError::Type::kThemeTooManySpecifics);
  }

  // TODO(crbug.com/481713548): Implement the logic to merge local theme prefs
  // with remote data.

  return std::nullopt;
}

void ThemeSyncableServiceIOS::StopSyncing(syncer::DataType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_EQ(type, syncer::THEMES_IOS);

  // TODO(crbug.com/481713548): Handle stopping sync (e.g., clearing observers),
  // and resetting back to the default theme.

  sync_processor_.reset();
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
  CHECK(!sync_processor_);
}

std::optional<syncer::ModelError> ThemeSyncableServiceIOS::ProcessSyncChanges(
    const base::Location& from_here,
    const syncer::SyncChangeList& change_list) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(crbug.com/481713548): Apply incoming changes from server to local UI.

  return std::nullopt;
}

std::string ThemeSyncableServiceIOS::GetClientTag(
    const syncer::EntityData& entity_data) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // iOS Theme always returns the same client tag as there is only one single
  // iOS theme entity.
  return kSyncEntityClientTag;
}

base::WeakPtr<syncer::SyncableService> ThemeSyncableServiceIOS::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}
