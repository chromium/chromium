// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FILE_SYSTEM_ACCESS_FILE_PATH_WATCHER_FILE_PATH_WATCHER_CHANGE_TRACKER_H_
#define CONTENT_BROWSER_FILE_SYSTEM_ACCESS_FILE_PATH_WATCHER_FILE_PATH_WATCHER_CHANGE_TRACKER_H_

#include "base/files/file_path.h"
#include "content/browser/file_system_access/file_path_watcher/file_path_watcher.h"

namespace content {

class FilePathWatcherChangeTracker {
 public:
  // TODO(crbug/321980205): Get rid of this struct once
  // FilePathWatcher::ChangeInfo has the same fields.
  struct ChangeInfo {
    ChangeInfo();
    ~ChangeInfo();

    ChangeInfo(ChangeInfo&&);
    ChangeInfo& operator=(ChangeInfo&&) = default;

    FilePathWatcher::ChangeType change_type =
        FilePathWatcher::ChangeType::kUnknown;

    base::FilePath modified_path;
    std::optional<base::FilePath> moved_from_path;
  };

  FilePathWatcherChangeTracker(base::FilePath target_path,
                               FilePathWatcher::Type type);
  FilePathWatcherChangeTracker(const FilePathWatcherChangeTracker&) = delete;
  FilePathWatcherChangeTracker& operator=(const FilePathWatcherChangeTracker&) =
      delete;

  FilePathWatcherChangeTracker(FilePathWatcherChangeTracker&&);
  FilePathWatcherChangeTracker& operator=(FilePathWatcherChangeTracker&&) =
      default;

  ~FilePathWatcherChangeTracker();

  // Returns whether the `FilePathWatcherChangeTracker` knows the target exists.
  bool KnowTargetExists();

  // Add a change reported by the Window's OS.
  void AddChange(base::FilePath path, DWORD win_change_type);

  // Call when changes may have been missed.
  void MayHaveMissedChanges();

  // Gets the number of changes to report since the last call to
  // `PopChangeCount`.
  int PopChangeCount();

 private:
  enum class ExistenceStatus {
    // We know the file exists.
    kExists,
    // We know the file is gone.
    kGone,
    // The file may or may not exist because an ancestor was moved into place.
    kMayHaveMovedIntoPlace,
  };

  void HandleChangeEffect(ExistenceStatus before_action,
                          ExistenceStatus after_action);

  void HandleSelfChange(ChangeInfo change);

  void HandleDescendantChange(ChangeInfo change, bool is_direct_child);

  void HandleAncestorChange(ChangeInfo change);

  void HandleOtherChange(ChangeInfo change);

  // The path that we're tracking changes for.
  base::FilePath target_path_;
  FilePathWatcher::Type type_;

  // Our current knowledge about the the existence of target based on what's
  // been passed to `AddChange` and calls to `GetFileInfo`.
  ExistenceStatus target_status_;

  // The number of changes to report based on what OS changes have been passed
  // to `AddChange`.
  int change_count_ = 0;

  // The path of the last `FILE_ACTION_RENAMED_OLD_NAME` OS change that was
  // passed to `AddChange`. Used to coalesce the move into a single event.
  base::FilePath last_moved_from_path_;
};
}  // namespace content

#endif  // CONTENT_BROWSER_FILE_SYSTEM_ACCESS_FILE_PATH_WATCHER_FILE_PATH_WATCHER_CHANGE_TRACKER_H_
