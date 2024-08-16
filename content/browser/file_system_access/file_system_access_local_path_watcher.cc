// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/file_system_access/file_system_access_local_path_watcher.h"

#include "base/files/file_path.h"
#include "base/task/bind_post_task.h"
#include "base/task/thread_pool.h"
#include "content/browser/file_system_access/file_path_watcher/file_path_watcher.h"
#include "content/browser/file_system_access/file_system_access_error.h"
#include "content/browser/file_system_access/file_system_access_watcher_manager.h"
#include "storage/browser/file_system/file_system_url.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_error.mojom-shared.h"

namespace content {

namespace {

// Creates a task runner suitable for file path watching.
scoped_refptr<base::SequencedTaskRunner> CreateFilePathWatcherTaskRunner() {
  return base::ThreadPool::CreateSequencedTaskRunner({
      // Needed for file I/O.
      base::MayBlock(),

      // File path watching is likely not user visible.
      base::TaskPriority::BEST_EFFORT,

      // File path watching should not block shutdown.
      base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN,
  });
}

}  // namespace

FileSystemAccessLocalPathWatcher::FileSystemAccessLocalPathWatcher(
    FileSystemAccessWatchScope scope,
    scoped_refptr<storage::FileSystemContext> file_system_context,
    base::PassKey<FileSystemAccessWatcherManager> /*pass_key*/)
    : FileSystemAccessChangeSource(std::move(scope),
                                   std::move(file_system_context)),
      watcher_(CreateFilePathWatcherTaskRunner()) {}

FileSystemAccessLocalPathWatcher::~FileSystemAccessLocalPathWatcher() = default;

void FileSystemAccessLocalPathWatcher::Initialize(
    base::OnceCallback<void(blink::mojom::FileSystemAccessErrorPtr)>
        on_source_initialized) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (scope().IsRecursive() && !FilePathWatcher::RecursiveWatchAvailable()) {
    std::move(on_source_initialized)
        .Run(file_system_access_error::FromStatus(
            blink::mojom::FileSystemAccessStatus::kNotSupportedError));
    return;
  }

  FilePathWatcher::CallbackWithChangeInfo on_change_callback =
      base::BindRepeating(&FileSystemAccessLocalPathWatcher::OnFilePathChanged,
                          weak_factory_.GetWeakPtr());

  FilePathWatcher::WatchOptions watch_options{
      .type = scope().IsRecursive() ? FilePathWatcher::Type::kRecursive
                                    : FilePathWatcher::Type::kNonRecursive,
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN) || \
    BUILDFLAG(IS_MAC)
      // Note: `report_modified_path` is also present on Android
      // and Fuchsia. Update this switch if support for watching
      // the local file system is added on those platforms.
      //
      // TODO(crbug.com/40260973): Report the affected
      // path on more platforms.
      .report_modified_path = true,
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN) ||
        // BUILDFLAG(IS_MAC)
  };

  watcher_.AsyncCall(&FilePathWatcher::WatchWithChangeInfo)
      .WithArgs(
          scope().root_url().path(), std::move(watch_options),
          base::BindPostTaskToCurrentDefault(std::move(on_change_callback)))
      .Then(base::BindOnce(
          [](base::OnceCallback<void(blink::mojom::FileSystemAccessErrorPtr)>
                 callback,
             bool result) {
            std::move(callback).Run(
                result ? file_system_access_error::Ok()
                       : file_system_access_error::FromStatus(
                             blink::mojom::FileSystemAccessStatus::
                                 kOperationFailed));
          },
          std::move(on_source_initialized)));
}

void FileSystemAccessLocalPathWatcher::OnFilePathChanged(
    const FilePathWatcher::ChangeInfo& change_info,
    const base::FilePath& changed_path,
    bool error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const base::FilePath& root_path = scope().root_url().path();

  base::FilePath relative_path;
  if (root_path.empty()) {
    relative_path = changed_path;
  } else if (root_path.IsParent(changed_path)) {
    CHECK(root_path.AppendRelativePath(changed_path, &relative_path));
  } else {
    // It is illegal for a source to notify of a change outside of its scope.
    CHECK_EQ(root_path, changed_path);
  }

  NotifyOfChange(relative_path, error, change_info);
}

}  // namespace content
