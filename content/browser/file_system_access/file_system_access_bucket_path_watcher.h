// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_BUCKET_PATH_WATCHER_H_
#define CONTENT_BROWSER_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_BUCKET_PATH_WATCHER_H_

#include "base/memory/ref_counted_delete_on_sequence.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner_helpers.h"
#include "base/thread_annotations.h"
#include "base/threading/sequence_bound.h"
#include "base/types/pass_key.h"
#include "content/browser/file_system_access/file_system_access_change_source.h"
#include "content/browser/file_system_access/file_system_access_watch_scope.h"
#include "storage/browser/file_system/file_observers.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_url.h"

namespace content {

class FileSystemAccessWatcherManager;

// Watches changes to all bucket file system paths and reports changes to its
// observers. This class must be created, used, and destroyed on the same
// sequence as the `FileSystemContext` it holds a reference to.
class FileSystemAccessBucketPathWatcher
    : public FileSystemAccessChangeSource,
      public storage::FileChangeObserver,
      // `weak_factory_` must be destroyed on the correct thread.
      public base::RefCountedDeleteOnSequence<
          FileSystemAccessBucketPathWatcher> {
 public:
  FileSystemAccessBucketPathWatcher(
      scoped_refptr<storage::FileSystemContext> file_system_context,
      base::PassKey<FileSystemAccessWatcherManager> pass_key);
  FileSystemAccessBucketPathWatcher(const FileSystemAccessBucketPathWatcher&) =
      delete;
  FileSystemAccessBucketPathWatcher& operator=(
      const FileSystemAccessBucketPathWatcher&) = delete;

  void Disable() override;

  // FileSystemAccessChangeSource:
  void Initialize(
      base::OnceCallback<void(blink::mojom::FileSystemAccessErrorPtr)>
          on_source_initialized) override;

  // storage::FileChangeObserver:
  void AddRef() const override;
  void Release() const override;

  void OnCreateFile(const storage::FileSystemURL& url) override;
  void OnCreateFileFrom(const storage::FileSystemURL& url,
                        const storage::FileSystemURL& src) override;
  void OnMoveFileFrom(const storage::FileSystemURL& url,
                      const storage::FileSystemURL& src) override;
  void OnRemoveFile(const storage::FileSystemURL& url) override;
  void OnModifyFile(const storage::FileSystemURL& url) override;
  void OnCreateDirectory(const storage::FileSystemURL& url) override;
  void OnRemoveDirectory(const storage::FileSystemURL& url) override;

 private:
  friend class base::RefCountedDeleteOnSequence<
      FileSystemAccessBucketPathWatcher>;
  friend class base::DeleteHelper<FileSystemAccessBucketPathWatcher>;

  mutable base::Lock is_disabled_lock_;
  bool is_disabled_ GUARDED_BY(is_disabled_lock_) = false;
  ~FileSystemAccessBucketPathWatcher() override;

  base::WeakPtrFactory<FileSystemAccessBucketPathWatcher> weak_factory_
      GUARDED_BY_CONTEXT(sequence_checker_){this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_BUCKET_PATH_WATCHER_H_
