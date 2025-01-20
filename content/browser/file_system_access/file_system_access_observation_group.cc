// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/file_system_access/file_system_access_observation_group.h"

#include "base/memory/ptr_util.h"
#include "content/browser/file_system_access/file_system_access_watcher_manager.h"

namespace content {

FileSystemAccessObservationGroup::Change::Change(
    storage::FileSystemURL url,
    FileSystemAccessChangeSource::ChangeInfo change_info)
    : url(std::move(url)), change_info(std::move(change_info)) {}
FileSystemAccessObservationGroup::Change::~Change() = default;

FileSystemAccessObservationGroup::Change::Change(
    const FileSystemAccessObservationGroup::Change& other)
    : url(other.url), change_info(std::move(other.change_info)) {}
FileSystemAccessObservationGroup::Change::Change(
    FileSystemAccessObservationGroup::Change&&) noexcept = default;

FileSystemAccessObservationGroup::Change&
FileSystemAccessObservationGroup::Change::operator=(
    const FileSystemAccessObservationGroup::Change&) = default;
FileSystemAccessObservationGroup::Change&
FileSystemAccessObservationGroup::Change::operator=(
    FileSystemAccessObservationGroup::Change&&) noexcept = default;

FileSystemAccessObservationGroup::Observer::Observer(
    FileSystemAccessObservationGroup& observation_group) {
  obs_.Observe(&observation_group);
}

FileSystemAccessObservationGroup::Observer::~Observer() = default;

void FileSystemAccessObservationGroup::Observer::SetCallback(
    OnChangesCallback on_change_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!on_change_callback_);
  on_change_callback_ = std::move(on_change_callback);
}

void FileSystemAccessObservationGroup::Observer::NotifyOfChanges(
    const std::optional<std::list<Change>>& changes_or_error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (on_change_callback_) {
    on_change_callback_.Run(std::move(changes_or_error));
  }
}

FileSystemAccessObservationGroup::FileSystemAccessObservationGroup(
    FileSystemAccessObserverQuotaManager::Handle quota_manager_handle,
    FileSystemAccessWatcherManager& watcher_manager,
    blink::StorageKey storage_key,
    FileSystemAccessWatchScope scope,
    base::PassKey<FileSystemAccessWatcherManager> pass_key)
    : storage_key_(std::move(storage_key)),
      scope_(std::move(scope)),
      quota_manager_handle_(std::move(quota_manager_handle)),
      watcher_manager_(watcher_manager) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  watcher_manager_->AddObserver(this);
}

FileSystemAccessObservationGroup::~FileSystemAccessObservationGroup() {
  watcher_manager_->RemoveObserver(this);
}

std::unique_ptr<FileSystemAccessObservationGroup::Observer>
FileSystemAccessObservationGroup::CreateObserver() {
  // Not using `std::make_unique` so that `Observer`'s constructor can remain
  // private.
  return base::WrapUnique(new Observer(*this));
}

void FileSystemAccessObservationGroup::AddObserver(Observer* observation) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  observations_.AddObserver(observation);
}

void FileSystemAccessObservationGroup::RemoveObserver(Observer* observation) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  observations_.RemoveObserver(observation);

  // `FileSystemAccessObservationGroup` lifetime is tied to its observations, so
  // destroy this when there are no more observations.
  if (observations_.empty()) {
    // `this` is destroyed after `RemoveObservationGroup` is called.
    watcher_manager_->RemoveObservationGroup(storage_key_, scope_);
  }
}

void FileSystemAccessObservationGroup::NotifyOfChanges(
    const std::optional<std::list<Change>>& changes_or_error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  for (auto& observation : observations_) {
    observation.NotifyOfChanges(changes_or_error);
  }
}

void FileSystemAccessObservationGroup::NotifyOfUsageChange(size_t old_usage,
                                                           size_t new_usage) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (received_quota_error_) {
    return;
  }

  if (on_usage_change_callback_) {
    on_usage_change_callback_.Run(old_usage, new_usage);
  }

  UsageChangeResult quota_result =
      quota_manager_handle_.OnUsageChange(new_usage);

  if (quota_result == UsageChangeResult::kQuotaUnavailable) {
    received_quota_error_ = true;

    // Send error to all observations by passing std::nullopt.
    for (auto& observation : observations_) {
      observation.NotifyOfChanges(/*changes_or_error=*/std::nullopt);
    }
  }
}

void FileSystemAccessObservationGroup::SetOnUsageCallbackForTesting(
    OnUsageChangeCallback on_usage_change_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  on_usage_change_callback_ = std::move(on_usage_change_callback);
}

}  // namespace content
