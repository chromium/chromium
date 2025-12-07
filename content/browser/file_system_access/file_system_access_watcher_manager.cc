// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/file_system_access/file_system_access_watcher_manager.h"

#include <algorithm>
#include <cstddef>
#include <list>
#include <memory>

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/types/optional_ref.h"
#include "base/types/pass_key.h"
#include "build/buildflag.h"
#include "components/services/storage/public/cpp/buckets/bucket_locator.h"
#include "content/browser/file_system_access/file_system_access_bucket_path_watcher.h"
#include "content/browser/file_system_access/file_system_access_change_source.h"
#include "content/browser/file_system_access/file_system_access_error.h"
#include "content/browser/file_system_access/file_system_access_manager_impl.h"
#include "content/browser/file_system_access/file_system_access_observation_group.h"
#include "content/browser/file_system_access/file_system_access_observer_host.h"
#include "content/browser/file_system_access/file_system_access_observer_observation.h"
#include "content/browser/file_system_access/file_system_access_watch_scope.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/common/file_system/file_system_types.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_error.mojom-shared.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_observer.mojom.h"

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_FUCHSIA)
#include "content/browser/file_system_access/file_system_access_local_path_watcher.h"
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS) &&
        // !BUILDFLAG(IS_FUCHSIA)

namespace content {

using WatchType = FileSystemAccessWatchScope::WatchType;
using UsageChangeResult =
    FileSystemAccessObserverQuotaManager::UsageChangeResult;

namespace {

storage::FileSystemURL ToFileSystemURL(storage::FileSystemContext& context,
                                       const storage::FileSystemURL& base_url,
                                       const base::FilePath& absolute_path) {
  storage::FileSystemURL result = context.CreateCrackedFileSystemURL(
      base_url.storage_key(), base_url.mount_type(), absolute_path);
  if (base_url.bucket()) {
    result.SetBucket(base_url.bucket().value());
  }
  return result;
}

}  // namespace

FileSystemAccessWatcherManager::FileSystemAccessWatcherManager(
    FileSystemAccessManagerImpl* manager,
    base::PassKey<FileSystemAccessManagerImpl> /*pass_key*/)
    : manager_(manager),
      bucket_path_watcher_(std::make_unique<FileSystemAccessBucketPathWatcher>(
          base::WrapRefCounted(manager_->context()),
          base::PassKey<FileSystemAccessWatcherManager>())) {
  RegisterSource(bucket_path_watcher_.get());
}

FileSystemAccessWatcherManager::~FileSystemAccessWatcherManager() = default;

void FileSystemAccessWatcherManager::BindObserverHost(
    const BindingContext& binding_context,
    mojo::PendingReceiver<blink::mojom::FileSystemAccessObserverHost>
        host_receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  observer_hosts_.insert(std::make_unique<FileSystemAccessObserverHost>(
      manager_, this, binding_context, std::move(host_receiver)));
}

void FileSystemAccessWatcherManager::RemoveObserverHost(
    FileSystemAccessObserverHost* host) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  size_t count_removed = observer_hosts_.erase(host);
  CHECK_EQ(count_removed, 1u);
}

void FileSystemAccessWatcherManager::GetFileObservation(
    const blink::StorageKey& storage_key,
    const storage::FileSystemURL& file_url,
    ukm::SourceId ukm_source_id,
    GetObservationCallback get_observation_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto scope = FileSystemAccessWatchScope::GetScopeForFileWatch(file_url);
  EnsureSourceIsInitializedForScope(
      scope, base::BindOnce(
                 &FileSystemAccessWatcherManager::PrepareObservationForScope,
                 weak_factory_.GetWeakPtr(), storage_key, scope, ukm_source_id,
                 std::move(get_observation_callback)));
}

void FileSystemAccessWatcherManager::GetDirectoryObservation(
    const blink::StorageKey& storage_key,
    const storage::FileSystemURL& directory_url,
    bool is_recursive,
    ukm::SourceId ukm_source_id,
    GetObservationCallback get_observation_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto scope = FileSystemAccessWatchScope::GetScopeForDirectoryWatch(
      directory_url, is_recursive);

  EnsureSourceIsInitializedForScope(
      scope, base::BindOnce(
                 &FileSystemAccessWatcherManager::PrepareObservationForScope,
                 weak_factory_.GetWeakPtr(), storage_key, scope, ukm_source_id,
                 std::move(get_observation_callback)));
}

void FileSystemAccessWatcherManager::OnRawChange(
    const storage::FileSystemURL& changed_url,
    bool error,
    const FileSystemAccessChangeSource::ChangeInfo& raw_change_info,
    const FileSystemAccessWatchScope& scope) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(crbug.com/40268906): Batch changes.

  // Writes via FileSystemWritableFileStream are done on a temporary swap file,
  // so ignore the swap file changes. When a writable is closed, the swap file
  // overrides the original file, producing a moved event. We will convert this
  // "moved" event to "modified" below.
  if (changed_url.virtual_path().FinalExtension() ==
      FILE_PATH_LITERAL(".crswap")) {
    return;
  }

  if (raw_change_info != FileSystemAccessChangeSource::ChangeInfo()) {
    // If non-empty ChangeInfo exists, this change should not be an error.
    CHECK(!error);
  }
  // For ChangeType::kMoved, ChangeInfo.moved_from_path is expected.
  bool is_move_event = raw_change_info.change_type ==
                       FileSystemAccessChangeSource::ChangeType::kMoved;
  CHECK(!is_move_event || raw_change_info.moved_from_path.has_value());

  // Convert the "moved" event to "modified" if the event is from the swap file
  // overriding the original file due to `FileSystemWritableFileStream.close()`.
  FileSystemAccessChangeSource::ChangeInfo change_info = raw_change_info;
  if (is_move_event &&
      raw_change_info.moved_from_path.value().FinalExtension() ==
          FILE_PATH_LITERAL(".crswap")) {
    is_move_event = false;
    change_info = FileSystemAccessChangeSource::ChangeInfo(
        raw_change_info.file_path_type,
        FileSystemAccessChangeSource::ChangeType::kModified,
        raw_change_info.modified_path);
  }

  std::optional<storage::FileSystemURL> moved_from_url =
      is_move_event ? std::make_optional(
                          ToFileSystemURL(*manager_->context(), changed_url,
                                          change_info.moved_from_path.value()))
                    : std::nullopt;

  const std::optional<std::list<Change>> changes_or_error =
      error
          ? std::nullopt
          : std::make_optional<std::list<Change>>({{changed_url, change_info}});

  for (auto& observation_group : observation_groups_) {
    // TODO(crbug.com/376134535): We identify a change source by its scope.
    // Observations that have the same scope belong to that change source. The
    // bucket file system being the exception.
    //
    // Eventually we will want Observations to directly watch their
    // ChangeSource. However we will have to do some refactoring because of the
    // bucket file system exception.
    if (scope.GetWatchType() != WatchType::kAllBucketFileSystems &&
        observation_group.scope() != scope) {
      continue;
    }

    // On both Local and Bucket File Systems, errors shouldn't be sent to
    // observations based on their scope but based on the source the
    // observations are tied to.
    if (error) {
      observation_group.NotifyOfChanges(changes_or_error);
      continue;
    }
    bool modified_url_in_scope =
        observation_group.scope().Contains(changed_url);
    bool moved_from_url_in_scope =
        is_move_event &&
        observation_group.scope().Contains(moved_from_url.value());

    if (!modified_url_in_scope && !moved_from_url_in_scope) {
      continue;
    }

    if (is_move_event) {
      if (!moved_from_url_in_scope) {
        // If a file/dir is moved into the scope, the change should be reported
        // as ChangeType::kCreated.
        FileSystemAccessChangeSource::ChangeInfo updated_change_info(
            change_info.file_path_type,
            FileSystemAccessChangeSource::ChangeType::kCreated,
            change_info.modified_path);
        observation_group.NotifyOfChanges(
            std::list<Change>({{changed_url, std::move(updated_change_info)}}));
        continue;
      }
      if (!modified_url_in_scope) {
        // If a file/dir is moved out of the scope, the change should be
        // reported as ChangeType::kDeleted.
        FileSystemAccessChangeSource::ChangeInfo updated_change_info(
            change_info.file_path_type,
            FileSystemAccessChangeSource::ChangeType::kDeleted,
            change_info.moved_from_path.value());
        observation_group.NotifyOfChanges(std::list<Change>(
            {{moved_from_url.value(), std::move(updated_change_info)}}));
        continue;
      }
    }

    // The default case, including move within scope, should notify the changes
    // as is.
    observation_group.NotifyOfChanges(changes_or_error);
  }
}

void FileSystemAccessWatcherManager::OnUsageChange(
    size_t old_usage,
    size_t new_usage,
    const FileSystemAccessWatchScope& scope) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // The bucket file system's usage should not change.
  CHECK(scope.GetWatchType() != WatchType::kAllBucketFileSystems);

  for (auto& observation_group : observation_groups_) {
    // TODO(crbug.com/376134535): We identify a change source by its scope.
    // Observations that have the same scope belong to that change source. The
    // bucket file system being the exception.
    //
    // Eventually we will want Observations to directly watch their
    // ChangeSource. However we will have to do some refactoring because of the
    // bucket file system exception.
    if (observation_group.scope() != scope) {
      continue;
    }

    observation_group.NotifyOfUsageChange(old_usage, new_usage);
  }
}

void FileSystemAccessWatcherManager::OnSourceBeingDestroyed(
    FileSystemAccessChangeSource* source) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  source_observations_.RemoveObservation(source);
  size_t count_removed = std::erase(all_sources_, *source);
  CHECK_EQ(count_removed, 1u);
}

void FileSystemAccessWatcherManager::RegisterSourceForTesting(
    FileSystemAccessChangeSource* source) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  RegisterSource(source);
}

void FileSystemAccessWatcherManager::RegisterSource(
    FileSystemAccessChangeSource* source) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  source_observations_.AddObservation(source);
  all_sources_.emplace_back(*source);
}

void FileSystemAccessWatcherManager::AddObserver(
    FileSystemAccessObservationGroup* observation_group) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  observation_groups_.AddObserver(observation_group);
}

void FileSystemAccessWatcherManager::RemoveObserver(
    FileSystemAccessObservationGroup* observation_group) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const auto newly_unobserved_scope = observation_group->scope();
  observation_groups_.RemoveObserver(observation_group);

  // Remove the respective source if we own it and it was the only observer
  // for this scope.
  //
  // TODO(crbug.com/40105284): Handle initializing sources.
  base::EraseIf(owned_sources_, [&](const auto& source) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return source->scope().Contains(newly_unobserved_scope) &&
           std::ranges::none_of(
               observation_groups_, [&source](const auto& observation) {
                 return source->scope().Contains(observation.scope());
               });
  });
}

void FileSystemAccessWatcherManager::RemoveObservationGroup(
    const blink::StorageKey& storage_key,
    const FileSystemAccessWatchScope& scope) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  watch_scope_obs_groups_map_.erase({storage_key, scope});
}

void FileSystemAccessWatcherManager::RemoveQuotaManager(
    const blink::StorageKey& storage_key) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  quota_managers_.erase(storage_key);
}

bool FileSystemAccessWatcherManager::HasSourceContainingScopeForTesting(
    const FileSystemAccessWatchScope& scope) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return std::ranges::any_of(
      all_sources_,
      [&scope](const raw_ref<FileSystemAccessChangeSource> source) {
        return source->scope().Contains(scope);
      });
}

FileSystemAccessObserverQuotaManager*
FileSystemAccessWatcherManager::GetQuotaManagerForTesting(
    const blink::StorageKey& storage_key) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto quota_manager_iter = quota_managers_.find(storage_key);
  if (quota_manager_iter == quota_managers_.end()) {
    return nullptr;
  }

  return &quota_manager_iter->second.get();
}

void FileSystemAccessWatcherManager::EnsureSourceIsInitializedForScope(
    FileSystemAccessWatchScope scope,
    base::OnceCallback<void(blink::mojom::FileSystemAccessErrorPtr, size_t)>
        on_source_initialized) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(crbug.com/40283894): Handle overlapping scopes and initializing
  // sources.

  FileSystemAccessChangeSource* raw_change_source = nullptr;
  auto it = std::ranges::find_if(
      all_sources_,
      [&scope](const raw_ref<FileSystemAccessChangeSource> source) {
        return source->scope().GetWatchType() ==
                       WatchType::kAllBucketFileSystems
                   ? source->scope().Contains(scope)
                   : source->scope() == scope;
      });
  if (it != all_sources_.end()) {
    raw_change_source = &it->get();
  } else {
    auto owned_change_source = CreateOwnedSourceForScope(scope);
    if (!owned_change_source) {
      // TODO(crbug.com/40105284): Watching `scope` is not supported.
      std::move(on_source_initialized)
          .Run(file_system_access_error::FromStatus(
                   blink::mojom::FileSystemAccessStatus::kNotSupportedError),
               /*source_current_usage=*/0);
      return;
    }
    raw_change_source = owned_change_source.get();
    owned_sources_.insert(std::move(owned_change_source));
  }

  CHECK(raw_change_source);
  raw_change_source->EnsureInitialized(
      base::BindOnce(&FileSystemAccessWatcherManager::DidInitializeSource,
                     weak_factory_.GetWeakPtr(), raw_change_source->AsWeakPtr(),
                     std::move(on_source_initialized)));
}

void FileSystemAccessWatcherManager::DidInitializeSource(
    base::WeakPtr<FileSystemAccessChangeSource> source,
    base::OnceCallback<void(blink::mojom::FileSystemAccessErrorPtr, size_t)>
        on_source_initialized,
    blink::mojom::FileSystemAccessErrorPtr result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!source) {
    // `source` was destroyed as we tried to initialize it. Abort.
    std::move(on_source_initialized)
        .Run(file_system_access_error::FromStatus(
                 blink::mojom::FileSystemAccessStatus::kOperationFailed),
             /*source_current_usage=*/0);
    return;
  }

  if (result->status != blink::mojom::FileSystemAccessStatus::kOk) {
    // If we owned this source, remove it. A source which is not initialized
    // will not notify of changes, so there's no use keeping it around.
    //
    // TODO(crbug.com/40105284): Decide how to handle unowned sources
    // which fail to initialize.
    base::EraseIf(
        owned_sources_,
        [&source](
            const std::unique_ptr<FileSystemAccessChangeSource>& owned_source) {
          return owned_source.get() == source.get();
        });
  }

  std::move(on_source_initialized)
      .Run(std::move(result), source->current_usage());
}

FileSystemAccessObserverQuotaManager::Handle
FileSystemAccessWatcherManager::GetOrCreateQuotaManagerForTesting(
    const blink::StorageKey& storage_key,
    ukm::SourceId ukm_source_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return GetOrCreateQuotaManager(storage_key, ukm_source_id);
}

FileSystemAccessObserverQuotaManager::Handle
FileSystemAccessWatcherManager::GetOrCreateQuotaManager(
    blink::StorageKey storage_key,
    ukm::SourceId ukm_source_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto quota_manager_iter = quota_managers_.find(storage_key);
  if (quota_manager_iter != quota_managers_.end()) {
    return quota_manager_iter->second.get().CreateHandle();
  }

  // ukm_source_id is expected to be unique per navigation ID and could be
  // different for the same StorageKey, if opened from different tabs.
  // For the purpose of UKM analysis, the first ukm_source_id used to create
  // FileSystemAccessObserverQuotaManager is used for the same StorageKey,
  // since it will be sliced per URL anyways.
  FileSystemAccessObserverQuotaManager* quota_manager =
      new FileSystemAccessObserverQuotaManager(storage_key, ukm_source_id,
                                               *this);

  quota_managers_.emplace(std::piecewise_construct,
                          std::forward_as_tuple(std::move(storage_key)),
                          std::forward_as_tuple(*quota_manager));

  return quota_manager->CreateHandle();
}

base::optional_ref<FileSystemAccessObservationGroup>
FileSystemAccessWatcherManager::GetOrCreateObservationGroup(
    blink::StorageKey storage_key,
    FileSystemAccessWatchScope scope,
    size_t source_current_usage,
    ukm::SourceId ukm_source_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::pair<blink::StorageKey, FileSystemAccessWatchScope> key(storage_key,
                                                               scope);

  auto observation_group_iter = watch_scope_obs_groups_map_.find(key);
  if (observation_group_iter != watch_scope_obs_groups_map_.end()) {
    return observation_group_iter->second;
  }

  FileSystemAccessObserverQuotaManager::Handle quota_manager =
      GetOrCreateQuotaManager(storage_key, ukm_source_id);
  UsageChangeResult usage_change_result =
      quota_manager.OnUsageChange(source_current_usage);
  if (usage_change_result == UsageChangeResult::kQuotaUnavailable) {
    return std::nullopt;
  }

  auto [created_observation_group_iter, inserted] =
      watch_scope_obs_groups_map_.emplace(
          std::piecewise_construct, std::forward_as_tuple(key),
          std::forward_as_tuple(
              std::move(quota_manager), *this, std::move(storage_key),
              std::move(scope),
              base::PassKey<FileSystemAccessWatcherManager>()));
  CHECK(inserted);

  return created_observation_group_iter->second;
}

void FileSystemAccessWatcherManager::PrepareObservationForScope(
    blink::StorageKey storage_key,
    FileSystemAccessWatchScope scope,
    ukm::SourceId ukm_source_id,
    GetObservationCallback get_observation_callback,
    blink::mojom::FileSystemAccessErrorPtr source_initialization_result,
    size_t source_current_usage) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (source_initialization_result->status !=
      blink::mojom::FileSystemAccessStatus::kOk) {
    std::move(get_observation_callback)
        .Run(base::unexpected(std::move(source_initialization_result)));
    return;
  }

  std::pair<blink::StorageKey, FileSystemAccessWatchScope> key(storage_key,
                                                               scope);
  auto observation_group_iter = watch_scope_obs_groups_map_.find(key);
  if (observation_group_iter != watch_scope_obs_groups_map_.end()) {
    // No need to report the usage change to the quota manager, since an
    // existing observation group is used.
    std::move(get_observation_callback)
        .Run(observation_group_iter->second.CreateObserver());
    return;
  }

  base::optional_ref<FileSystemAccessObservationGroup> observation_group =
      GetOrCreateObservationGroup(std::move(storage_key), std::move(scope),
                                  source_current_usage, ukm_source_id);
  if (observation_group.has_value()) {
    std::move(get_observation_callback)
        .Run(observation_group->CreateObserver());
  } else {
    std::move(get_observation_callback)
        .Run(base::unexpected(file_system_access_error::FromStatus(
            blink::mojom::FileSystemAccessStatus::kOperationFailed)));
  }
}

std::unique_ptr<FileSystemAccessChangeSource>
FileSystemAccessWatcherManager::CreateOwnedSourceForScope(
    FileSystemAccessWatchScope scope) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(scope.root_url().is_valid());

  if (scope.root_url().mount_type() !=
      storage::FileSystemType::kFileSystemTypeLocal) {
    // We should never have to create an owned source for a bucket file system,
    // since `bucket_path_watcher_` covers all possible bucket scopes.
    CHECK(scope.root_url().type() !=
          storage::FileSystemType::kFileSystemTypeTemporary);

    // TODO(crbug.com/40283896): Support non-local file systems.
    return nullptr;
  }

  // Access to the local file system is not supported on Android or iOS. See
  // https://crbug.com/1011535.
  // Meanwhile, `base::FilePatchWatcher` is not implemented on Fuchsia. See
  // https://crbug.com/851641.
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS) || BUILDFLAG(IS_FUCHSIA)
  return nullptr;
#else
  auto new_source = std::make_unique<FileSystemAccessLocalPathWatcher>(
      std::move(scope), base::WrapRefCounted(manager_->context()),
      base::PassKey<FileSystemAccessWatcherManager>());
  RegisterSource(new_source.get());
  return new_source;
#endif  //  BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS) || BUILDFLAG(IS_FUCHSIA)
}

}  // namespace content
