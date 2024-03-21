// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/file_system_access/file_system_access_watcher_manager.h"

#include <algorithm>
#include <list>
#include <memory>

#include "base/check.h"
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

namespace {

blink::mojom::FileSystemAccessChangeTypePtr ToMojoChangeType(
    bool error,
    FileSystemAccessChangeSource::ChangeType change_type) {
  if (error) {
    return blink::mojom::FileSystemAccessChangeType::NewErrored(
        blink::mojom::FileSystemAccessChangeTypeErrored::New());
  }

  switch (change_type) {
    case FileSystemAccessChangeSource::ChangeType::kUnsupported:
      return blink::mojom::FileSystemAccessChangeType::NewUnsupported(
          blink::mojom::FileSystemAccessChangeTypeUnsupported::New());
    case FileSystemAccessChangeSource::ChangeType::kCreated:
      return blink::mojom::FileSystemAccessChangeType::NewCreated(
          blink::mojom::FileSystemAccessChangeTypeCreated::New());
    case FileSystemAccessChangeSource::ChangeType::kDeleted:
      return blink::mojom::FileSystemAccessChangeType::NewDeleted(
          blink::mojom::FileSystemAccessChangeTypeDeleted::New());
    case FileSystemAccessChangeSource::ChangeType::kModified:
      return blink::mojom::FileSystemAccessChangeType::NewModified(
          blink::mojom::FileSystemAccessChangeTypeModified::New());
    case FileSystemAccessChangeSource::ChangeType::kMoved:
      // TODO(https://crbug.com/1488864): Support setting
      // `former_relative_path`.
      return blink::mojom::FileSystemAccessChangeType::NewMoved(
          blink::mojom::FileSystemAccessChangeTypeMoved::New());
  }
}

}  // namespace

FileSystemAccessWatcherManager::Observation::Change::Change(
    storage::FileSystemURL url,
    blink::mojom::FileSystemAccessChangeTypePtr type,
    FileSystemAccessChangeSource::FilePathType file_path_type)
    : url(std::move(url)),
      type(std::move(type)),
      file_path_type(file_path_type) {}
FileSystemAccessWatcherManager::Observation::Change::~Change() = default;

FileSystemAccessWatcherManager::Observation::Change::Change(
    const FileSystemAccessWatcherManager::Observation::Change& other)
    : url(other.url),
      type(other.type->Clone()),
      file_path_type(other.file_path_type) {}
FileSystemAccessWatcherManager::Observation::Change::Change(
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
    const FileSystemAccessChangeSource::ChangeInfo& change_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(https://crbug.com/1488864): Use `change_info.cookie` to connect
  // related events.
  //
  // TODO(https://crbug.com/1488874): Ignore changes caused by API
  // implementation details, such as writes to swap files.
  //
  // TODO(https://crbug.com/1488875): Discard changes corresponding to
  // non-fully-active pages.
  //
  // TODO(https://crbug.com/1447240): Batch changes.

  const std::list<Observation::Change> changes = {
      {changed_url, ToMojoChangeType(error, change_info.change_type),
       change_info.file_path_type}};
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

  // TODO(https://crbug.com/1489057): Handle overlapping scopes and initializing
  // sources.

  FileSystemAccessChangeSource* raw_change_source = nullptr;
  auto it = base::ranges::find_if(
      all_sources_,
      [&scope](const raw_ref<FileSystemAccessChangeSource> source) {
        return source->scope().Contains(scope);
      });
  if (it != all_sources_.end()) {
    raw_change_source = &it->get();
  } else {
    auto owned_change_source = CreateOwnedSourceForScope(scope);
    if (!owned_change_source) {
      // TODO(https://crbug.com/1019297): Watching `scope` is not supported.
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
    // TODO(https://crbug.com/1019297): Decide how to handle unowned sources
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

    // TODO(https://crbug.com/1489061): Support non-local file systems.
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
