// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/file_system_access/file_system_access_watcher_manager.h"

#include <algorithm>
#include <memory>

#include "base/check.h"
#include "base/containers/cxx20_erase_list.h"
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/ranges/algorithm.h"
#include "base/sequence_checker.h"
#include "base/types/pass_key.h"
#include "build/buildflag.h"
#include "components/services/storage/public/cpp/buckets/bucket_locator.h"
#include "content/browser/file_system_access/file_system_access_change_source.h"
#include "content/browser/file_system_access/file_system_access_manager_impl.h"
#include "content/browser/file_system_access/file_system_access_observer_host.h"
#include "content/browser/file_system_access/file_system_access_observer_observation.h"
#include "content/browser/file_system_access/file_system_access_watch_scope.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/common/file_system/file_system_types.h"

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_FUCHSIA)
#include "content/browser/file_system_access/file_system_access_local_path_watcher.h"
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_FUCHSIA)

namespace content {

namespace {

FileSystemAccessWatcherManager::Observation::Change ToChange(
    storage::FileSystemContext& context,
    const storage::FileSystemURL& root_url,
    const base::FilePath& relative_path,
    bool error) {
  CHECK(!relative_path.IsAbsolute());
  CHECK(!relative_path.ReferencesParent());

  auto result = context.CreateCrackedFileSystemURL(
      root_url.storage_key(), root_url.mount_type(),
      root_url.virtual_path().Append(relative_path));
  if (root_url.bucket()) {
    result.SetBucket(root_url.bucket().value());
  }
  return {std::move(result), error};
}

}  // namespace

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
    const std::list<Change>& changes,
    base::PassKey<FileSystemAccessWatcherManager> pass_key) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (on_change_callback_) {
    on_change_callback_.Run(std::move(changes));
  }
}

FileSystemAccessWatcherManager::FileSystemAccessWatcherManager(
    FileSystemAccessManagerImpl* manager,
    base::PassKey<FileSystemAccessManagerImpl> /*pass_key*/)
    : manager_(manager) {}

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
    FileSystemAccessChangeSource* source,
    const base::FilePath& relative_path,
    bool error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto change = ToChange(*manager()->context(), source->scope().root_url(),
                         relative_path, error);
  const storage::FileSystemURL changed_url = change.url;

  // TODO(https://crbug.com/1019297):
  //   - Batch changes.
  //   - Ignore changes caused by API implementation details, such as writes to
  //     swap files.
  //   - Discard changes corresponding to non-fully-active pages.

  const std::list<Observation::Change> changes = {std::move(change)};
  for (auto& observation : observations_) {
    if (observation.scope().Contains(changed_url)) {
      observation.NotifyOfChanges(
          changes, base::PassKey<FileSystemAccessWatcherManager>());
    }
  }
}

void FileSystemAccessWatcherManager::OnSourceBeingDestroyed(
    FileSystemAccessChangeSource* source) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  source_observations_.RemoveObservation(source);
  size_t count_removed = base::Erase(all_sources_, source);
  CHECK_EQ(count_removed, 1u);
}

void FileSystemAccessWatcherManager::RegisterSource(
    FileSystemAccessChangeSource* source) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  source_observations_.AddObservation(source);
  all_sources_.push_back(source);
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
  // TODO(https://crbug.com/1019297): Handle initializing sources.
  base::EraseIf(owned_sources_, [&](const auto& source) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return source->scope().Contains(newly_unobserved_scope) &&
           base::ranges::none_of(
               observations_, [&source](const auto& observation) {
                 return source->scope().Contains(observation.scope());
               });
  });
}

void FileSystemAccessWatcherManager::EnsureSourceIsInitializedForScope(
    FileSystemAccessWatchScope scope,
    base::OnceCallback<void(bool)> on_source_initialized) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(https://crbug.com/1019297): Handle overlapping scopes and initializing
  // sources.

  FileSystemAccessChangeSource* raw_change_source = nullptr;
  auto it = base::ranges::find_if(
      all_sources_, [&scope](const FileSystemAccessChangeSource* source) {
        return source->scope().Contains(scope);
      });
  if (it != all_sources_.end()) {
    raw_change_source = *it;
  } else {
    auto owned_change_source = CreateOwnedSourceForScope(scope);
    if (!owned_change_source) {
      // TODO(https://crbug.com/1019297): Watching `scope` is not supported.
      std::move(on_source_initialized).Run(false);
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
    base::OnceCallback<void(bool)> on_source_initialized,
    bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!source) {
    std::move(on_source_initialized).Run(false);
    return;
  }

  if (!success) {
    // If we owned this source, remove it. A source which is not initialized
    // will not notify of changes, so there's no use keeping it around.
    //
    // TODO(https://crbug.com/1019297): Decide how to handle unowned sources
    // which fail to initialize.
    base::EraseIf(
        owned_sources_,
        [&source](
            const std::unique_ptr<FileSystemAccessChangeSource>& owned_source) {
          return owned_source.get() == source.get();
        });
  }

  std::move(on_source_initialized).Run(success);
}

void FileSystemAccessWatcherManager::PrepareObservationForScope(
    FileSystemAccessWatchScope scope,
    GetObservationCallback get_observation_callback,
    bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!success) {
    std::move(get_observation_callback).Run(nullptr);
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

  if (scope.root_url().type() != storage::kFileSystemTypeLocal) {
    // TODO(https://crbug.com/1019297): Support non-local file systems.
    return nullptr;
  }

  // Access to the local file system is not supported on Android. See
  // https://crbug.com/1011535.
  // Meanwhile, `base::FilePatchWatcher` is not implemented on Fuchsia. See
  // https://crbug.com/851641.
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_FUCHSIA)
  return nullptr;
#else
  auto new_source = std::make_unique<FileSystemAccessLocalPathWatcher>(
      std::move(scope), base::PassKey<FileSystemAccessWatcherManager>());
  RegisterSource(new_source.get());
  return new_source;
#endif  //  BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_FUCHSIA)
}

}  // namespace content
