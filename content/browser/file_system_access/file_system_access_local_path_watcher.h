// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_LOCAL_PATH_WATCHER_H_
#define CONTENT_BROWSER_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_LOCAL_PATH_WATCHER_H_

#include "base/files/file_path_watcher.h"
#include "base/thread_annotations.h"
#include "base/threading/sequence_bound.h"
#include "base/types/pass_key.h"
#include "content/browser/file_system_access/file_system_access_change_source.h"
#include "content/browser/file_system_access/file_system_access_watch_scope.h"

namespace content {

class FileSystemAccessWatcherManager;

// Watches a local file path and reports changes to its observers.
// This class must constructed, used, and destroyed on the same sequence.
class FileSystemAccessLocalPathWatcher : public FileSystemAccessChangeSource {
 public:
  FileSystemAccessLocalPathWatcher(
      FileSystemAccessWatchScope scope,
      base::PassKey<FileSystemAccessWatcherManager> pass_key);
  FileSystemAccessLocalPathWatcher(const FileSystemAccessLocalPathWatcher&) =
      delete;
  FileSystemAccessLocalPathWatcher& operator=(
      const FileSystemAccessLocalPathWatcher&) = delete;
  ~FileSystemAccessLocalPathWatcher() override;

  // FileSystemAccessChangeSource:
  void Initialize(
      base::OnceCallback<void(bool)> on_source_initialized) override;

 private:
  void OnFilePathChanged(const base::FilePath& changed_path, bool error);

  base::SequenceBound<base::FilePathWatcher> watcher_
      GUARDED_BY_CONTEXT(sequence_checker_);

  base::WeakPtrFactory<FileSystemAccessLocalPathWatcher> weak_factory_
      GUARDED_BY_CONTEXT(sequence_checker_){this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_LOCAL_PATH_WATCHER_H_
