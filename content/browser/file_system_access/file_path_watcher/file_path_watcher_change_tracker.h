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
  using ChangeInfo = FilePathWatcher::ChangeInfo;
  using ChangeType = FilePathWatcher::ChangeType;

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

  // Gets the ChangeInfo's to report since the last call to `PopChanges`.
  //
  // Passing `next_change_soon` as true indicates another change will be added
  // soon so we don't need to finish coalescing events yet.
  std::vector<ChangeInfo> PopChanges(bool next_change_soon);

 private:
  enum class ExistenceStatus {
    // We know the file exists.
    kExists,
    // We know the file is gone.
    kGone,
    // The file may or may not exist because an ancestor was moved into place.
    kMayHaveMovedIntoPlace,
  };

  // Converts a `change_info` from `kMoved` to `kCreated` if it's a move from
  // out of scope to in scope. All moves into scope should be reported as
  // created.
  void ConvertMoveToCreateIfOutOfScope(ChangeInfo& change_info);

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

  // Changes to report based on what OS changes have been passed to `AddChange`.
  std::vector<ChangeInfo> changes_;

  // The path of the last `FILE_ACTION_RENAMED_OLD_NAME` OS change that was
  // passed to `AddChange`. Used to coalesce the move into a single event.
  base::FilePath last_moved_from_path_;

  // The `ChangeInfo` of the last `FILE_ACTION_RENAMED_OLD_NAME` OS change that
  // was passed to `AddChange`. Used to coalesce the move into a single event.
  ChangeInfo last_move_change_;

  // The `ChangeInfo` of the last `FILE_ACTION_REMOVED` OS change that was
  // passed to `AddChange`. Used to coalesce the overwrites into a single move
  // event.
  std::optional<ChangeInfo> last_deleted_change_;
};
}  // namespace content

#endif  // CONTENT_BROWSER_FILE_SYSTEM_ACCESS_FILE_PATH_WATCHER_FILE_PATH_WATCHER_CHANGE_TRACKER_H_
