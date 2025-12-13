// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_OBSERVATION_GROUP_H_
#define CONTENT_BROWSER_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_OBSERVATION_GROUP_H_

#include <list>

#include "base/functional/callback.h"
#include "base/observer_list_types.h"
#include "base/scoped_observation.h"
#include "base/scoped_observation_traits.h"
#include "base/sequence_checker.h"
#include "content/browser/file_system_access/file_system_access_change_source.h"
#include "content/browser/file_system_access/file_system_access_observer_quota_manager.h"
#include "content/browser/file_system_access/file_system_access_watch_scope.h"

namespace content {
class FileSystemAccessWatcherManager;

// Represents a group of observations that all have the same
// `FileSystemAccessChangeSource` and `base::StorageKey`. This is created and
// maintained by the `FileSystemAccessWatcherManager`.
//
// Instances of this class must be accessed exclusively on the UI thread. Owned
// by the `FileSystemAccessWatcherManager`.
//
// TODO(crbug.com/376134535): Once the watcher manager layer is removed, the
// base class should be `FileSystemAccessChangeSource::RawChangeObserver` rather
// than `base::CheckedObserver`.
class CONTENT_EXPORT FileSystemAccessObservationGroup
    : public base::CheckedObserver {
 public:
  // Describes a change to some location in a file system.
  struct CONTENT_EXPORT Change {
    Change(storage::FileSystemURL url,
           FileSystemAccessChangeSource::ChangeInfo change_info);
    ~Change();

    // Copyable and movable.
    Change(const Change&);
    Change(Change&&) noexcept;
    Change& operator=(const Change&);
    Change& operator=(Change&&) noexcept;

    storage::FileSystemURL url;
    FileSystemAccessChangeSource::ChangeInfo change_info;

    bool operator==(const Change& other) const {
      return url == other.url && change_info == other.change_info;
    }
  };

  // An observer of a `FileSystemAccessObservationGroup`.
  //
  // The common source/observer pattern is for the source to provide an abstract
  // class for observers to implement. Here we instead fully implement the
  // observer and let end observers set a callback for its events.
  //
  // This pattern is chosen because we want the lifetime of the
  // `FileSystemAccessObservationGroup` to be tied to its `Observer`. We don't
  // want there to be an empty `FileSystemAccessObservationGroup`, so the
  // creator of the `FileSystemAccessObservationGroup` should be able to
  // immediately create an `Observer`.
  class CONTENT_EXPORT Observer : public base::CheckedObserver {
   public:
    using OnChangesCallback = base::RepeatingCallback<void(
        const std::optional<std::list<Change>>& changes_or_error)>;

    ~Observer() override;

    // Not copyable or movable.
    Observer(const Observer&) = delete;
    Observer(Observer&&) = delete;
    Observer& operator=(const Observer&) = delete;
    Observer& operator=(Observer&&) = delete;

    // Set the callback to which changes will be reported. It is illegal to call
    // this method more than once.
    void SetCallback(OnChangesCallback on_change_callback);

    const FileSystemAccessWatchScope& scope() const {
      DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

      return obs_.GetSource()->scope();
    }

    FileSystemAccessObservationGroup* GetObservationGroupForTesting() {
      DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
      return obs_.GetSource();
    }

   private:
    friend FileSystemAccessObservationGroup;

    explicit Observer(FileSystemAccessObservationGroup& observation_group);

    void NotifyOfChanges(
        const std::optional<std::list<Change>>& changes_or_error);

    SEQUENCE_CHECKER(sequence_checker_);

    OnChangesCallback on_change_callback_ GUARDED_BY_CONTEXT(sequence_checker_);

    base::ScopedObservation<FileSystemAccessObservationGroup, Observer> obs_
        GUARDED_BY_CONTEXT(sequence_checker_){this};
  };

  using OnUsageChangeCallback = FilePathWatcher::UsageChangeCallback;

  explicit FileSystemAccessObservationGroup(
      FileSystemAccessObserverQuotaManager::Handle quota_manager_handle,
      FileSystemAccessWatcherManager& watcher_manager,
      blink::StorageKey storage_key,
      FileSystemAccessWatchScope scope,
      base::PassKey<FileSystemAccessWatcherManager> pass_key);
  ~FileSystemAccessObservationGroup() override;

  // Not copyable or movable.
  FileSystemAccessObservationGroup(const FileSystemAccessObservationGroup&) =
      delete;
  FileSystemAccessObservationGroup(FileSystemAccessObservationGroup&&) = delete;
  FileSystemAccessObservationGroup& operator=(
      const FileSystemAccessObservationGroup&) = delete;
  FileSystemAccessObservationGroup& operator=(
      FileSystemAccessObservationGroup&&) = delete;

  // Set the callback to which usage changes will be reported. It is illegal to
  // call this method more than once.
  void SetOnUsageCallbackForTesting(
      OnUsageChangeCallback on_usage_change_callback);

  FileSystemAccessObserverQuotaManager* GetQuotaManagerForTesting() {
    return quota_manager_handle_.GetQuotaManagerForTesting();  // IN-TEST
  }

 private:
  friend FileSystemAccessWatcherManager;
  friend base::ScopedObservationTraits<FileSystemAccessObservationGroup,
                                       Observer>;

  using UsageChangeResult =
      FileSystemAccessObserverQuotaManager::UsageChangeResult;

  std::unique_ptr<Observer> CreateObserver();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  void NotifyOfChanges(
      const std::optional<std::list<Change>>& changes_or_error);

  void NotifyOfUsageChange(size_t old_usage, size_t new_usage);

  const FileSystemAccessWatchScope& scope() const { return scope_; }

  SEQUENCE_CHECKER(sequence_checker_);

  const blink::StorageKey storage_key_;
  const FileSystemAccessWatchScope scope_;

  // Observations to which this instance will notify of changes within their
  // respective scope.
  base::ObserverList<Observer> observations_
      GUARDED_BY_CONTEXT(sequence_checker_);

  OnUsageChangeCallback on_usage_change_callback_;

  // The quota manager for our storage key.
  FileSystemAccessObserverQuotaManager::Handle quota_manager_handle_;

  // The `FileSystemAccessWatcherManager` that we're observing. Safe because
  // `watcher_manager_` owns this.
  //
  // We use this instead of a `ScopedObservation` because
  // `ScopedObservationTraits` needs a full interface definition rather than a
  // forward declaration of `FileSystemAccessWatcherManager`. It's not worth
  // defining a custom `ScopedObservationTraits` for the minimum value
  // `ScopedObservation` brings.
  base::raw_ref<FileSystemAccessWatcherManager> watcher_manager_;

  bool received_quota_error_ = false;
};

}  // namespace content

#endif  // CONTENT_BROWSER_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_OBSERVATION_GROUP_H_
