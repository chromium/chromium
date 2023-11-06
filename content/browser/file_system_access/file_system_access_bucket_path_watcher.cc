// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/file_system_access/file_system_access_bucket_path_watcher.h"

#include "base/atomic_sequence_num.h"
#include "base/files/file_path.h"
#include "base/files/file_path_watcher.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/sequence_bound.h"
#include "content/browser/file_system_access/file_system_access_error.h"
#include "content/browser/file_system_access/file_system_access_watcher_manager.h"
#include "storage/browser/file_system/file_observers.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/browser/file_system/sandbox_file_system_backend_delegate.h"

namespace content {

FileSystemAccessBucketPathWatcher::FileSystemAccessBucketPathWatcher(
    scoped_refptr<storage::FileSystemContext> file_system_context,
    base::PassKey<FileSystemAccessWatcherManager> /*pass_key*/)
    : FileSystemAccessChangeSource(
          FileSystemAccessWatchScope::GetScopeForAllBucketFileSystems(),
          std::move(file_system_context)) {}

FileSystemAccessBucketPathWatcher::~FileSystemAccessBucketPathWatcher() =
    default;

void FileSystemAccessBucketPathWatcher::Initialize(
    base::OnceCallback<void(blink::mojom::FileSystemAccessErrorPtr)>
        on_source_initialized) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  storage::SandboxFileSystemBackendDelegate* sandbox_delegate =
      file_system_context()->sandbox_delegate();
  if (!sandbox_delegate) {
    std::move(on_source_initialized)
        .Run(file_system_access_error::FromStatus(
            blink::mojom::FileSystemAccessStatus::kOperationFailed));
    return;
  }

  sandbox_delegate->AddFileChangeObserver(
      storage::FileSystemType::kFileSystemTypeTemporary, this,
      base::SequencedTaskRunner::GetCurrentDefault().get());

  std::move(on_source_initialized).Run(file_system_access_error::Ok());
}

void FileSystemAccessBucketPathWatcher::OnCreateFile(
    const storage::FileSystemURL& url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NotifyOfChange(url, /*error=*/false,
                 ChangeInfo{.file_path_type = FilePathType::kFile,
                            .change_type = ChangeType::kCreated});
}

void FileSystemAccessBucketPathWatcher::OnCreateFileFrom(
    const storage::FileSystemURL& url,
    const storage::FileSystemURL& src) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Pass a unique `cookie` value such that a consumer could connect these
  // events.
  //
  // TODO(https://crbug.com/1425601): Consider coalescing into a single event.
  static base::AtomicSequenceNumber next_cookie;
  int cookie = next_cookie.GetNext();

  NotifyOfChange(src, /*error=*/false,
                 ChangeInfo{.file_path_type = FilePathType::kFile,
                            .change_type = ChangeType::kMoved,
                            .cookie = cookie});
  NotifyOfChange(url, /*error=*/false,
                 ChangeInfo{.file_path_type = FilePathType::kFile,
                            .change_type = ChangeType::kMoved,
                            .cookie = cookie});
}

void FileSystemAccessBucketPathWatcher::OnRemoveFile(
    const storage::FileSystemURL& url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NotifyOfChange(url, /*error=*/false,
                 ChangeInfo{.file_path_type = FilePathType::kFile,
                            .change_type = ChangeType::kDeleted});
}

void FileSystemAccessBucketPathWatcher::OnModifyFile(
    const storage::FileSystemURL& url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NotifyOfChange(url, /*error=*/false,
                 ChangeInfo{.file_path_type = FilePathType::kFile,
                            .change_type = ChangeType::kModified});
}

void FileSystemAccessBucketPathWatcher::OnCreateDirectory(
    const storage::FileSystemURL& url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NotifyOfChange(url, /*error=*/false,
                 ChangeInfo{.file_path_type = FilePathType::kDirectory,
                            .change_type = ChangeType::kCreated});
}

void FileSystemAccessBucketPathWatcher::OnRemoveDirectory(
    const storage::FileSystemURL& url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NotifyOfChange(url, /*error=*/false,
                 ChangeInfo{.file_path_type = FilePathType::kDirectory,
                            .change_type = ChangeType::kDeleted});
}

}  // namespace content
