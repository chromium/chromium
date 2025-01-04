// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_WATCHER_MANAGER_H_
#define CONTENT_BROWSER_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_WATCHER_MANAGER_H_

#include <memory>

#include "base/containers/flat_set.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/scoped_multi_source_observation.h"
#include "base/scoped_observation.h"
#include "base/sequence_checker.h"
#include "base/types/expected.h"
#include "base/types/pass_key.h"
#include "content/browser/file_system_access/file_system_access_bucket_path_watcher.h"
#include "content/browser/file_system_access/file_system_access_change_source.h"
#include "content/browser/file_system_access/file_system_access_observation_group.h"
#include "content/browser/file_system_access/file_system_access_observer_quota_manager.h"
#include "content/browser/file_system_access/file_system_access_watch_scope.h"
#include "content/common/content_export.h"
#include "content/public/browser/file_system_access_entry_factory.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_url.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_error.mojom.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_observer.mojom.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_observer_host.mojom.h"

namespace content {

class FileSystemAccessManagerImpl;
class FileSystemAccessObserverHost;

// Manages browser side resources for the `FileSystemObserver` API.
//
// Instances of this class must be accessed exclusively on the UI thread. Owned
// by the `FileSystemAccessManagerImpl`.
//
// TODO(crbug.com/376134535): Currently, raw changes from the underlying file
// system are plumbed through this class, to be filtered, batched, and
// transformed before being relayed to the appropriate
// `FileSystemAccessObservationGroup`s. Once `FileSystemAccessObservationGroup`s
// observe their change source directly, all this logic should be moved to the
// `FileSystemAccessObservationGroup`.
class CONTENT_EXPORT FileSystemAccessWatcherManager
    : public FileSystemAccessChangeSource::RawChangeObserver {
 public:
  using Change = FileSystemAccessObservationGroup::Change;
  using BindingContext = FileSystemAccessEntryFactory::BindingContext;
  using GetObservationCallback = base::OnceCallback<void(
      base::expected<
          std::unique_ptr<FileSystemAccessObservationGroup::Observer>,
          blink::mojom::FileSystemAccessErrorPtr>)>;
  using OnUsageChangeCallback = FilePathWatcher::UsageChangeCallback;

  FileSystemAccessWatcherManager(
      FileSystemAccessManagerImpl* manager,
      base::PassKey<FileSystemAccessManagerImpl> pass_key);
  ~FileSystemAccessWatcherManager() override;

  FileSystemAccessWatcherManager(FileSystemAccessWatcherManager const&) =
      delete;
  FileSystemAccessWatcherManager& operator=(
      FileSystemAccessWatcherManager const&) = delete;

  void BindObserverHost(
      const BindingContext& binding_context,
      mojo::PendingReceiver<blink::mojom::FileSystemAccessObserverHost>
          host_receiver);
  void RemoveObserverHost(FileSystemAccessObserverHost* host);

  // Prepares to watch the given file or directory for the `storage_key`. This
  // may create a new `FileSystemAccessChangeSource` if one does not already
  // cover the scope of the requested observation.
  //
  // `get_observation_callback` returns an `Observation`, or an appropriate
  // error if the given file or directory cannot be watched as requested.
  void GetFileObservation(const blink::StorageKey& storage_key,
                          const storage::FileSystemURL& file_url,
                          ukm::SourceId ukm_source_id,
                          GetObservationCallback get_observation_callback);
  void GetDirectoryObservation(const blink::StorageKey& storage_key,
                               const storage::FileSystemURL& directory_url,
                               bool is_recursive,
                               ukm::SourceId ukm_source_id,
                               GetObservationCallback get_observation_callback);

  // Subscribe this instance to raw changes from `source`.
  void RegisterSourceForTesting(FileSystemAccessChangeSource* source);

  bool HasObservationGroupsForTesting() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return !observation_groups_.empty();
  }
  bool HasObservationGroupForTesting(
      const FileSystemAccessObservationGroup* observation_group) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return observation_groups_.HasObserver(observation_group);
  }

  FileSystemAccessObserverQuotaManager::Handle
  GetOrCreateQuotaManagerForTesting(const blink::StorageKey& storage_key,
                                    ukm::SourceId ukm_source_id);
  FileSystemAccessObserverQuotaManager* GetQuotaManagerForTesting(
      const blink::StorageKey& storage_key);

  bool HasSourceForTesting(FileSystemAccessChangeSource* source) const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return source_observations_.IsObservingSource(source);
  }
  bool HasSourceContainingScopeForTesting(
      const FileSystemAccessWatchScope& scope) const;

 private:
  friend FileSystemAccessObservationGroup;
  friend FileSystemAccessObserverQuotaManager;
  // Called by `FileSystemAccessObservationGroup` to receive raw change events.
  //
  // TODO(crbug.com/376134535): Remove this once observation groups directly
  // observe their change source.
  void AddObserver(FileSystemAccessObservationGroup* observation_group);

  // Called by `FileSystemAccessObservationGroup` to stop receiving raw change
  // events.
  //
  // TODO(crbug.com/376134535): Remove this once observation groups directly
  // observe their change source.
  void RemoveObserver(FileSystemAccessObservationGroup* observation_group);

  // Called by `FileSystemAccessObservationGroup` to destroy itself.
  void RemoveObservationGroup(const blink::StorageKey& storage_key,
                              const FileSystemAccessWatchScope& scope);

  // Called by `FileSystemAccessObserverQuotaManager` remove it from
  // `quota_managers_` when it is destroyed.
  void RemoveQuotaManager(const blink::StorageKey& storage_key);

  // FileSystemAccessChangeSource::RawChangeObserver:
  void OnRawChange(const storage::FileSystemURL& changed_url,
                   bool error,
                   const FileSystemAccessChangeSource::ChangeInfo& change_info,
                   const FileSystemAccessWatchScope& scope) override;
  void OnUsageChange(size_t old_usage,
                     size_t new_usage,
                     const FileSystemAccessWatchScope& scope) override;
  void OnSourceBeingDestroyed(FileSystemAccessChangeSource* source) override;

  // Subscribe this instance to raw changes from `source`.
  void RegisterSource(FileSystemAccessChangeSource* source);

  // Attempts to create a change source for `scope` if it does not exist.
  void EnsureSourceIsInitializedForScope(
      FileSystemAccessWatchScope scope,
      base::OnceCallback<void(blink::mojom::FileSystemAccessErrorPtr, size_t)>
          on_source_initialized);
  void DidInitializeSource(
      base::WeakPtr<FileSystemAccessChangeSource> source,
      base::OnceCallback<void(blink::mojom::FileSystemAccessErrorPtr, size_t)>
          on_source_initialized,
      blink::mojom::FileSystemAccessErrorPtr result);

  FileSystemAccessObserverQuotaManager::Handle GetOrCreateQuotaManager(
      blink::StorageKey storage_key,
      ukm::SourceId ukm_source_id);

  base::optional_ref<FileSystemAccessObservationGroup>
  GetOrCreateObservationGroup(blink::StorageKey storage_key,
                              FileSystemAccessWatchScope scope,
                              size_t source_current_usage,
                              ukm::SourceId ukm_source_id);

  void PrepareObservationForScope(
      blink::StorageKey storage_key,
      FileSystemAccessWatchScope scope,
      ukm::SourceId ukm_source_id,
      GetObservationCallback callback,
      blink::mojom::FileSystemAccessErrorPtr source_initialization_result,
      size_t source_current_usage);

  // Creates and returns a new (uninitialized) change source for the given
  // scope, or nullptr if watching this scope is not supported.
  std::unique_ptr<FileSystemAccessChangeSource> CreateOwnedSourceForScope(
      FileSystemAccessWatchScope scope);

  SEQUENCE_CHECKER(sequence_checker_);

  // The manager which owns this instance.
  const raw_ptr<FileSystemAccessManagerImpl> manager_ = nullptr;

  // Watches changes to the all bucket file systems. Though this is technically
  // a change source which is owned by this instance, it is not included in
  // `owned_sources_` simply because this watcher should never be revoked.
  // TODO(crbug.com/40105284): Consider making the lifetime of this
  // watcher match other owned sources; creating an instance on-demand and then
  // destroying it when it is no longer needed.
  std::unique_ptr<FileSystemAccessBucketPathWatcher> bucket_path_watcher_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // TODO(crbug.com/321980367): Make more efficient mappings to observers
  // and sources. For now, most actions requires iterating through lists.

  // `raw_ref` safe because `FileSystemAccessObserverQuotaManager` removes
  // itself from this map on destruction.
  std::map<blink::StorageKey, raw_ref<FileSystemAccessObserverQuotaManager>>
      quota_managers_ GUARDED_BY_CONTEXT(sequence_checker_);

  std::map<std::pair<blink::StorageKey, FileSystemAccessWatchScope>,
           FileSystemAccessObservationGroup>
      watch_scope_obs_groups_map_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Observations to which this instance will notify of changes within their
  // respective scope.
  //
  // TODO(crbug.com/376134535): Remove this once observation groups directly
  // observe their change source.
  base::ObserverList<FileSystemAccessObservationGroup> observation_groups_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Sources created by this instance in response to a request to observe a
  // scope that is not currently contained by existing sources. These
  // sources may be removed when their scope is no longer being observed.
  base::flat_set<std::unique_ptr<FileSystemAccessChangeSource>,
                 base::UniquePtrComparator>
      owned_sources_ GUARDED_BY_CONTEXT(sequence_checker_);
  // Observations of all sources, including those not owned by this instance.
  base::ScopedMultiSourceObservation<FileSystemAccessChangeSource,
                                     RawChangeObserver>
      source_observations_ GUARDED_BY_CONTEXT(sequence_checker_){this};
  // Raw refs to each source in `source_observations_`.
  // Unfortunately, ScopedMultiSourceObservation does not allow for peeking
  // inside the list. This is a workaround.
  std::list<raw_ref<FileSystemAccessChangeSource>> all_sources_;

  // When a `FileSystemAccessObserverHost` is destroyed, it destroys several
  // elements in members of `this`. So destroy `observer_hosts_` first before
  // destroying other members.
  base::flat_set<std::unique_ptr<FileSystemAccessObserverHost>,
                 base::UniquePtrComparator>
      observer_hosts_;

  base::WeakPtrFactory<FileSystemAccessWatcherManager> weak_factory_
      GUARDED_BY_CONTEXT(sequence_checker_){this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_WATCHER_MANAGER_H_
