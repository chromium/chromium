// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/file_system_access/file_path_watcher/file_path_watcher_change_tracker.h"

#include <windows.h>

#include <winnt.h>

#include "base/files/file_util.h"
#include "content/browser/file_system_access/file_path_watcher/file_path_watcher.h"

namespace content {
namespace {

enum class PathRelation {
  kSelf,
  kAncestor,
  kDescendant,
  kDirectChild,
  kOther,
};

// Finds `related_path`'s relationship to `self_path` from `self_path`'s
// perspective.
PathRelation FindPathRelation(const base::FilePath& self_path,
                              const base::FilePath& related_path) {
  const auto self_components = self_path.GetComponents();
  const auto related_components = related_path.GetComponents();
  for (size_t i = 0;
       i < self_components.size() && i < related_components.size(); ++i) {
    if (self_components[i] != related_components[i]) {
      return PathRelation::kOther;
    }
  }
  if (self_components.size() + 1 == related_components.size()) {
    return PathRelation::kDirectChild;
  }
  if (self_components.size() < related_components.size()) {
    return PathRelation::kDescendant;
  }
  if (self_components.size() > related_components.size()) {
    return PathRelation::kAncestor;
  }
  return PathRelation::kSelf;
}

FilePathWatcher::FilePathType GetFilePathType(const base::FilePath& file) {
  base::File::Info file_info;
  if (!GetFileInfo(file, &file_info)) {
    return FilePathWatcher::FilePathType::kUnknown;
  }
  return file_info.is_directory ? FilePathWatcher::FilePathType::kDirectory
                                : FilePathWatcher::FilePathType::kFile;
}

FilePathWatcher::ChangeType ToChangeType(DWORD win_change_type) {
  switch (win_change_type) {
    case FILE_ACTION_ADDED:
      return FilePathWatcher::ChangeType::kCreated;
    case FILE_ACTION_REMOVED:
      return FilePathWatcher::ChangeType::kDeleted;
    case FILE_ACTION_MODIFIED:
      return FilePathWatcher::ChangeType::kModified;
    case FILE_ACTION_RENAMED_OLD_NAME:
    case FILE_ACTION_RENAMED_NEW_NAME:
      return FilePathWatcher::ChangeType::kMoved;
    default:
      NOTREACHED_NORETURN();
  }
}

}  // namespace

FilePathWatcherChangeTracker::ChangeInfo::ChangeInfo() = default;
FilePathWatcherChangeTracker::ChangeInfo::ChangeInfo(
    FilePathWatcherChangeTracker::ChangeInfo&&) = default;
FilePathWatcherChangeTracker::ChangeInfo::~ChangeInfo() = default;

FilePathWatcherChangeTracker::FilePathWatcherChangeTracker(
    base::FilePath target_path,
    FilePathWatcher::Type type)
    : target_path_(target_path), type_(type) {
  // Need to check for target existence in case the first change is a
  // FILE_ACTION_RENAMED_OLD_NAME event.
  target_status_ =
      GetFilePathType(target_path_) == FilePathWatcher::FilePathType::kUnknown
          ? ExistenceStatus::kGone
          : ExistenceStatus::kExists;
}

FilePathWatcherChangeTracker::FilePathWatcherChangeTracker(
    FilePathWatcherChangeTracker&&) = default;
FilePathWatcherChangeTracker::~FilePathWatcherChangeTracker() = default;

void FilePathWatcherChangeTracker::AddChange(base::FilePath path,
                                             DWORD win_change_type) {
  if (win_change_type == FILE_ACTION_RENAMED_OLD_NAME) {
    last_moved_from_path_ = path;
    return;
  }

  PathRelation path_relation = FindPathRelation(target_path_, path);

  FilePathWatcherChangeTracker::ChangeInfo change;
  change.change_type = ToChangeType(win_change_type);
  change.modified_path = std::move(path);

  if (win_change_type == FILE_ACTION_RENAMED_NEW_NAME) {
    change.moved_from_path = last_moved_from_path_;
  }

  switch (path_relation) {
    case PathRelation::kSelf:
      HandleSelfChange(std::move(change));
      break;
    case PathRelation::kAncestor:
      HandleAncestorChange(std::move(change));
      break;
    case PathRelation::kDescendant:
      HandleDescendantChange(std::move(change), /*is_direct_child=*/false);
      break;
    case PathRelation::kDirectChild:
      HandleDescendantChange(std::move(change), /*is_direct_child=*/true);
      break;
    case PathRelation::kOther:
      HandleOtherChange(std::move(change));
      break;
  }
}

void FilePathWatcherChangeTracker::MayHaveMissedChanges() {
  target_status_ =
      GetFilePathType(target_path_) == FilePathWatcher::FilePathType::kUnknown
          ? ExistenceStatus::kGone
          : ExistenceStatus::kExists;
}

int FilePathWatcherChangeTracker::PopChangeCount() {
  if (target_status_ == ExistenceStatus::kMayHaveMovedIntoPlace) {
    // Decide whether the target moved into place or not.
    ExistenceStatus status =
        GetFilePathType(target_path_) == FilePathWatcher::FilePathType::kUnknown
            ? ExistenceStatus::kGone
            : ExistenceStatus::kExists;

    HandleChangeEffect(status, status);
  }
  return std::exchange(change_count_, 0);
}

bool FilePathWatcherChangeTracker::KnowTargetExists() {
  return target_status_ == ExistenceStatus::kExists;
}

void FilePathWatcherChangeTracker::HandleChangeEffect(
    ExistenceStatus before_action,
    ExistenceStatus after_action) {
  // If the target may have moved into place and we now know that it exists,
  // then it definitely moved into place.
  if (target_status_ == ExistenceStatus::kMayHaveMovedIntoPlace &&
      before_action == ExistenceStatus::kExists) {
    ++change_count_;
  }

  target_status_ = after_action;
}

void FilePathWatcherChangeTracker::HandleSelfChange(
    FilePathWatcherChangeTracker::ChangeInfo change) {
  switch (change.change_type) {
    case FilePathWatcher::ChangeType::kCreated:
    case FilePathWatcher::ChangeType::kMoved:
      HandleChangeEffect(ExistenceStatus::kGone, ExistenceStatus::kExists);
      ++change_count_;
      break;
    case FilePathWatcher::ChangeType::kDeleted:
      HandleChangeEffect(ExistenceStatus::kExists, ExistenceStatus::kGone);
      ++change_count_;
      break;
    case FilePathWatcher::ChangeType::kModified:
      HandleChangeEffect(ExistenceStatus::kExists, ExistenceStatus::kExists);
      // Don't report modifications on directories.
      if (GetFilePathType(target_path_) !=
          FilePathWatcher::FilePathType::kDirectory) {
        ++change_count_;
      }
      break;
    case FilePathWatcher::ChangeType::kUnknown:
      // All changes passed into here come from `ToChangeType` which doesn't
      // return `kUnknown`.
      NOTREACHED_NORETURN();
  }
}

void FilePathWatcherChangeTracker::HandleDescendantChange(
    FilePathWatcherChangeTracker::ChangeInfo change,
    bool is_direct_child) {
  // Any notification on a descendant means the target existed before and
  // after.
  HandleChangeEffect(ExistenceStatus::kExists, ExistenceStatus::kExists);

  // Only report descendants that are direct children in non recursive types.
  if (type_ == FilePathWatcher::Type::kNonRecursive && !is_direct_child) {
    return;
  }

  // Don't report modifications on directories.
  if (change.change_type == FilePathWatcher::ChangeType::kModified &&
      GetFilePathType(change.modified_path) ==
          FilePathWatcher::FilePathType::kDirectory) {
    return;
  }

  ++change_count_;
}

void FilePathWatcherChangeTracker::HandleAncestorChange(
    FilePathWatcherChangeTracker::ChangeInfo change) {
  switch (change.change_type) {
    case FilePathWatcher::ChangeType::kDeleted:
    case FilePathWatcher::ChangeType::kCreated:
      // Can't delete a directory until all its children are gone. Can't create
      // a directory with existing children.
      HandleChangeEffect(ExistenceStatus::kGone, ExistenceStatus::kGone);
      break;
    case FilePathWatcher::ChangeType::kMoved:
      HandleChangeEffect(ExistenceStatus::kGone,
                         ExistenceStatus::kMayHaveMovedIntoPlace);
      break;
    case FilePathWatcher::ChangeType::kModified:
      // This tells us nothing.
      break;
    case FilePathWatcher::ChangeType::kUnknown:
      // All changes passed into here come from `ToChangeType` which doesn't
      // return `kUnknown`.
      NOTREACHED_NORETURN();
  }
}

void FilePathWatcherChangeTracker::HandleOtherChange(ChangeInfo change) {
  // Only moved types have the possibility of being in scope.
  if (change.change_type != FilePathWatcher::ChangeType::kMoved) {
    return;
  }

  CHECK(change.moved_from_path.has_value());

  switch (FindPathRelation(target_path_, *change.moved_from_path)) {
    case PathRelation::kSelf:
      HandleChangeEffect(ExistenceStatus::kExists, ExistenceStatus::kGone);
      ++change_count_;
      break;
    case PathRelation::kAncestor: {
      bool target_exists_before_move =
          target_status_ == ExistenceStatus::kExists;

      HandleChangeEffect(target_status_, ExistenceStatus::kGone);

      // If the target wasn't there before the move, then it wasn't moved.
      if (!target_exists_before_move) {
        break;
      }

      ++change_count_;
      break;
    }
    case PathRelation::kDescendant:
    case PathRelation::kDirectChild:
      // Any notification on a descendant means the target existed before and
      // after.
      HandleChangeEffect(ExistenceStatus::kExists, ExistenceStatus::kExists);
      ++change_count_;
      break;
    case PathRelation::kOther:
      // We don't care about files moving around out of scope.
      break;
  }
}
}  // namespace content
