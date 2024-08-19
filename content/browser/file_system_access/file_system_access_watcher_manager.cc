// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/file_system_access/file_system_access_watcher_manager.h"

#include <algorithm>
#include <list>
#include <memory>

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/ranges/algorithm.h"
#include "base/sequence_checker.h"
#include "base/types/pass_key.h"
#include "build/buildflag.h"
#include "components/services/storage/public/cpp/buckets/bucket_locator.h"
#include "content/browser/file_system_access/file_system_access_bucket_path_watcher.h"
#include "content/browser/file_system_access/file_system_access_change_source.h"
#include "content/browser/file_system_access/file_system_access_error.h"
#include "content/browser/file_system_access/file_system_access_manager_impl.h"
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

FileSystemAccessWatcherManager::Observation::Change::Change(
    storage::FileSystemURL url,
    FileSystemAccessChangeSource::ChangeInfo change_info)
    : url(std::move(url)), change_info(std::move(change_info)) {}
FileSystemAccessWatcherManager::Observation::Change::~Change() = default;

FileSystemAccessWatcherManager::Observation::Change::Change(
    const FileSystemAccessWatcherManager::Observation::Change& other)
    : url(other.url), change_info(std::move(other.change_info)) {}
FileSystemAccessWatcherManager::Observation::Change::Change(
    FileSystemAccessWatcherManager::Observation::Change&&) noexcept = default;

FileSystemAccessWatcherManager::Observation::Change&
FileSystemAccessWatcherManager::Observation::Change::operator=(
    const FileSystemAccessWatcherManager::Observation::Change&) = default;
FileSystemAccessWatcherManager::Observation::Change&
FileSystemAccessWatcherManager::Observation::Change::operator=(
    FileSystemAccessWatcherManager::Observation::Change&&) noexcept = default;

FileSystemAccessWatcherManager::Observation::Observation(
    FileSystemAccessWatcherManager* watcher_manager,
    FileSystemAccessWatchScope scope,
    base::PassKey<FileSystemAccessWatcherManager> /*pass_key*/)
    : scope_(std::move(scope)) {
  CHECK(watcher_manager);
  obs_.Observe(watcher_manager);
}
FileSystemAccessWatcherManager::Observation::~Observation() = default;

void FileSystemAccessWatcherManager::Observation::SetCallback(
    OnChangesCallback on_change_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!on_change_callback_);
  on_change_callback_ = std::move(on_change_callback);
}

void FileSystemAccessWatcherManager::Observation::NotifyOfChanges(
    const std::optional<std::list<Change>>& changes_or_error,
    base::PassKey<FileSystemAccessWatcherManager> pass_key) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (on_change_callback_) {
    on_change_callback_.Run(std::move(changes_or_error));
  }
}

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
    const storage::FileSystemURL& file_url,
    GetObservationCallback get_observation_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto scope = FileSystemAccessWatchScope::GetScopeForFileWatch(file_url);
  EnsureSourceIsInitializedForScope(
      scope, base::BindOnce(
                 &FileSystemAccessWatcherManager::PrepareObservationForScope,
                 weak_factory_.GetWeakPtr(), scope,
                 std::move(get_observation_callback)));
}

void FileSystemAccessWatcherManager::GetDirectoryObservation(
    const storage::FileSystemURL& directory_url,
    bool is_recursive,
    GetObservationCallback get_observation_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto scope = FileSystemAccessWatchScope::GetScopeForDirectoryWatch(
      directory_url, is_recursive);

  EnsureSourceIsInitializedForScope(
      scope, base::BindOnce(
                 &FileSystemAccessWatcherManager::PrepareObservationForScope,
                 weak_factory_.GetWeakPtr(), scope,
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
                          ToFileSystemURL(*manager()->context(), changed_url,
                                          change_info.moved_from_path.value()))
                    : std::nullopt;

  const std::optional<std::list<Observation::Change>> changes_or_error =
      error ? std::nullopt
            : std::make_optional(
                  std::list<Observation::Change>({{changed_url, change_info}}));
  for (auto& observation : observations_) {
    // TODO(crbug.com/321980367): Currently, sharing partially overlapping
    // observations are not supported, hence checking for the exact scope
    // matching on Local FS.
    if (scope.GetWatchType() != WatchType::kAllBucketFileSystems &&
        observation.scope() != scope) {
      continue;
    }

    // On both Local and Bucket File Systems, errors shouldn't be sent to
    // observations based on their scope but based on the source the
    // observations are tied to.
    if (error) {
      observation.NotifyOfChanges(
          changes_or_error, base::PassKey<FileSystemAccessWatcherManager>());
      continue;
    }
    bool modified_url_in_scope = observation.scope().Contains(changed_url);
    bool moved_from_url_in_scope =
        is_move_event && observation.scope().Contains(moved_from_url.value());

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
        observation.NotifyOfChanges(
            std::list<Observation::Change>(
                {{changed_url, std::move(updated_change_info)}}),
            base::PassKey<FileSystemAccessWatcherManager>());
        continue;
      }
      if (!modified_url_in_scope) {
        // If a file/dir is moved out of the scope, the change should be
        // reported as ChangeType::kDeleted.
        FileSystemAccessChangeSource::ChangeInfo updated_change_info(
            change_info.file_path_type,
            FileSystemAccessChangeSource::ChangeType::kDeleted,
            change_info.moved_from_path.value());
        observation.NotifyOfChanges(
            std::list<Observation::Change>(
                {{moved_from_url.value(), std::move(updated_change_info)}}),
            base::PassKey<FileSystemAccessWatcherManager>());
        continue;
      }
    }

    // The default case, including move within scope, should notify the changes
    // as is.
    observation.NotifyOfChanges(
        changes_or_error, base::PassKey<FileSystemAccessWatcherManager>());
  }
}

void FileSystemAccessWatcherManager::OnSourceBeingDestroyed(
    FileSystemAccessChangeSource* source) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  source_observations_.RemoveObservation(source);
  size_t count_removed = std::erase(all_sources_, *source);
  CHECK_EQ(count_removed, 1u);
}

void FileSystemAccessWatcherManager::RegisterSource(
    FileSystemAccessChangeSource* source) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  source_observations_.AddObservation(source);
  all_sources_.emplace_back(*source);
}

void FileSystemAccessWatcherManager::AddObserver(Observation* observation) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  observations_.AddObserver(observation);
}

void FileSystemAccessWatcherManager::RemoveObserver(Observation* observation) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const auto newly_unobserved_scope = observation->scope();
  observations_.RemoveObserver(observation);

  // Remove the respective source if we own it and it was the only observer
  // for this scope.
  //
  // TODO(crbug.com/40105284): Handle initializing sources.
  base::EraseIf(owned_sources_, [&](const auto& source) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return source->scope().Contains(newly_unobserved_scope) &&
           base::ranges::none_of(
               observations_, [&source](const auto& observation) {
                 return source->scope().Contains(observation.scope());
               });
  });
}

bool FileSystemAccessWatcherManager::HasSourceContainingScopeForTesting(
    const FileSystemAccessWatchScope& scope) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return base::ranges::any_of(
      all_sources_,
      [&scope](const raw_ref<FileSystemAccessChangeSource> source) {
        return source->scope().Contains(scope);
      });
}

void FileSystemAccessWatcherManager::EnsureSourceIsInitializedForScope(
    FileSystemAccessWatchScope scope,
    base::OnceCallback<void(blink::mojom::FileSystemAccessErrorPtr)>
        on_source_initialized) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(crbug.com/40283894): Handle overlapping scopes and initializing
  // sources.

  FileSystemAccessChangeSource* raw_change_source = nullptr;
  auto it = base::ranges::find_if(
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
              blink::mojom::FileSystemAccessStatus::kNotSupportedError));
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
    base::OnceCallback<void(blink::mojom::FileSystemAccessErrorPtr)>
        on_source_initialized,
    blink::mojom::FileSystemAccessErrorPtr result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!source) {
    // `source` was destroyed as we tried to initialize it. Abort.
    std::move(on_source_initialized)
        .Run(file_system_access_error::FromStatus(
            blink::mojom::FileSystemAccessStatus::kOperationFailed));
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

  std::move(on_source_initialized).Run(std::move(result));
}

void FileSystemAccessWatcherManager::PrepareObservationForScope(
    FileSystemAccessWatchScope scope,
    GetObservationCallback get_observation_callback,
    blink::mojom::FileSystemAccessErrorPtr source_initialization_result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (source_initialization_result->status !=
      blink::mojom::FileSystemAccessStatus::kOk) {
    std::move(get_observation_callback)
        .Run(base::unexpected(std::move(source_initialization_result)));
    return;
  }

  std::move(get_observation_callback)
      .Run(std::make_unique<Observation>(
          this, std::move(scope),
          base::PassKey<FileSystemAccessWatcherManager>()));
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
      std::move(scope), base::WrapRefCounted(manager()->context()),
      base::PassKey<FileSystemAccessWatcherManager>());
  RegisterSource(new_source.get());
  return new_source;
#endif  //  BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS) || BUILDFLAG(IS_FUCHSIA)
}

}  // namespace content
