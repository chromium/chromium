// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/file_system_access/file_path_watcher/file_path_watcher_change_tracker.h"

#include <windows.h>

#include <winnt.h>

#include <algorithm>
#include <utility>
#include <vector>

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

using PathComponents = std::vector<base::FilePath::StringType>;

// Finds `related_components`' relationship to `self_components` from
// `self_components`' perspective.
PathRelation FindPathRelation(const PathComponents& self_components,
                              const PathComponents& related_components) {
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

PathRelation FindPathRelation(const base::FilePath& self_path,
                              const base::FilePath& related_path) {
  return FindPathRelation(self_path.GetComponents(),
                          related_path.GetComponents());
}

FilePathWatcher::FilePathType QueryFilePathType(const base::FilePath& file) {
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
      NOTREACHED();
  }
}

// Returns the path `descendant_path` would be in if `ancestor_path` was moved
// to or from `moved_path`.
base::FilePath GetMovedPathOfDescendant(const base::FilePath& descendant_path,
                                        const base::FilePath& ancestor_path,
                                        const base::FilePath& moved_path) {
  const auto ancestor_components = ancestor_path.GetComponents();
  const auto descendant_components = descendant_path.GetComponents();
  auto moved_path_of_descendant = moved_path;
  for (size_t i = ancestor_components.size(); i < descendant_components.size();
       ++i) {
    moved_path_of_descendant =
        moved_path_of_descendant.Append(descendant_components[i]);
  }
  return moved_path_of_descendant;
}

}  // namespace

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
  // Attempt to coalesce an overwrite into a single move event.
  //
  // Windows reports overwrites as a delete then a move.
  if (last_deleted_change_) {
    bool is_overwrite_event = win_change_type == FILE_ACTION_RENAMED_NEW_NAME &&
                              last_deleted_change_->modified_path == path;
    bool next_event_could_be_overwrite_event =
        win_change_type == FILE_ACTION_RENAMED_OLD_NAME;

    // If it's not an overwrite and the next event couldn't be an overwrite
    // event, then report the `last_deleted_change_` event instead of
    // coalescing.
    if (!is_overwrite_event && !next_event_could_be_overwrite_event) {
      changes_.push_back(*std::exchange(last_deleted_change_, std::nullopt));
    }

    // Coalesce the overwrite event by dropping the last deleted change.
    if (!next_event_could_be_overwrite_event) {
      last_deleted_change_ = std::nullopt;
    }
  }

  // Even a notification for a path that is already gone tells us that its
  // ancestors were directories at the time of the notification.
  auto ancestors = path.GetComponents();
  const auto target_components = target_path_.GetComponents();
  const PathRelation path_relation =
      FindPathRelation(target_components, ancestors);
  if (std::ranges::starts_with(ancestors, target_components)) {
    while (ancestors.size() > target_components.size()) {
      ancestors.pop_back();
      known_file_path_types_.Put(ancestors,
                                 FilePathWatcher::FilePathType::kDirectory);
    }
  }
  // Attempt to coalesce move.
  if (win_change_type == FILE_ACTION_RENAMED_OLD_NAME) {
    last_moved_from_path_ = path;
    return;
  }

  if (path_relation == PathRelation::kOther &&
      win_change_type != FILE_ACTION_RENAMED_NEW_NAME) {
    return;
  }

  ChangeInfo change;
  change.change_type = ToChangeType(win_change_type);
  change.modified_path = path;

  if (win_change_type == FILE_ACTION_ADDED) {
    // A creation starts a new lifetime, possibly with a different path type.
    RemoveKnownFilePathTypes(path);
  }

  if (win_change_type == FILE_ACTION_RENAMED_NEW_NAME) {
    if (!last_moved_from_path_.empty()) {
      change.moved_from_path =
          std::exchange(last_moved_from_path_, base::FilePath());
      MoveKnownFilePathTypes(*change.moved_from_path, path);
    } else {
      RemoveKnownFilePathTypes(path);
    }
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

  if (win_change_type == FILE_ACTION_REMOVED ||
      path_relation == PathRelation::kOther) {
    // Retain the old type until the deletion/move-out has been reported, but
    // don't reuse it for replacements or paths we no longer observe.
    RemoveKnownFilePathTypes(path);
  }
}

void FilePathWatcherChangeTracker::MayHaveMissedChanges() {
  last_deleted_change_ = std::nullopt;
  // A rename pair must not carry knowledge across a gap in notifications.
  last_moved_from_path_ = base::FilePath();
  known_file_path_types_.Clear();
  target_status_ =
      GetFilePathType(target_path_) == FilePathWatcher::FilePathType::kUnknown
          ? ExistenceStatus::kGone
          : ExistenceStatus::kExists;
}

std::vector<FilePathWatcher::ChangeInfo>
FilePathWatcherChangeTracker::PopChanges(bool next_change_soon) {
  if (!next_change_soon) {
    if (target_status_ == ExistenceStatus::kMayHaveMovedIntoPlace) {
      // Decide whether the target moved into place or not.
      ExistenceStatus status = QueryFilePathType(target_path_) ==
                                       FilePathWatcher::FilePathType::kUnknown
                                   ? ExistenceStatus::kGone
                                   : ExistenceStatus::kExists;

      HandleChangeEffect(status, status);
    }
  }
  return std::move(changes_);
}

bool FilePathWatcherChangeTracker::KnowTargetExists() {
  return target_status_ == ExistenceStatus::kExists;
}

void FilePathWatcherChangeTracker::ConvertMoveToCreateIfOutOfScope(
    ChangeInfo& change) {
  CHECK_EQ(change.change_type, ChangeType::kMoved);
  if (!change.moved_from_path.has_value() ||
      FindPathRelation(target_path_, *change.moved_from_path) ==
          PathRelation::kOther) {
    change.moved_from_path = std::nullopt;
    change.change_type = ChangeType::kCreated;
  }
}

void FilePathWatcherChangeTracker::HandleChangeEffect(
    ExistenceStatus before_action,
    ExistenceStatus after_action) {
  // If the target may have moved into place and we now know that it exists,
  // then it definitely moved into place.
  if (target_status_ == ExistenceStatus::kMayHaveMovedIntoPlace &&
      before_action == ExistenceStatus::kExists) {
    CHECK(!last_move_change_.moved_from_path.has_value());

    last_move_change_.modified_path = target_path_;
    last_move_change_.file_path_type = GetFilePathType(target_path_);

    changes_.push_back(std::move(last_move_change_));
  }

  target_status_ = after_action;
}

void FilePathWatcherChangeTracker::HandleSelfChange(ChangeInfo change) {
  switch (change.change_type) {
    case ChangeType::kCreated:
      HandleChangeEffect(ExistenceStatus::kGone, ExistenceStatus::kExists);
      change.file_path_type = GetFilePathType(target_path_);
      changes_.push_back(std::move(change));
      break;
    case ChangeType::kMoved:
      HandleChangeEffect(ExistenceStatus::kGone, ExistenceStatus::kExists);
      change.file_path_type = GetFilePathType(target_path_);
      ConvertMoveToCreateIfOutOfScope(change);
      changes_.push_back(std::move(change));
      break;
    case ChangeType::kDeleted:
      HandleChangeEffect(ExistenceStatus::kExists, ExistenceStatus::kGone);
      change.file_path_type = GetKnownFilePathType(target_path_);
      last_deleted_change_ = std::move(change);
      break;
    case ChangeType::kModified:
      HandleChangeEffect(ExistenceStatus::kExists, ExistenceStatus::kExists);
      change.file_path_type = GetFilePathType(target_path_);
      // Don't report modifications on directories.
      if (change.file_path_type != FilePathWatcher::FilePathType::kDirectory) {
        changes_.push_back(std::move(change));
      }
      break;
    case ChangeType::kUnknown:
      // All changes passed into here come from `ToChangeType` which doesn't
      // return `kUnknown`.
      NOTREACHED();
  }
}

void FilePathWatcherChangeTracker::HandleDescendantChange(
    ChangeInfo change,
    bool is_direct_child) {
  // Any notification on a descendant means the target existed before and
  // after.
  HandleChangeEffect(ExistenceStatus::kExists, ExistenceStatus::kExists);

  // Only report descendants that are direct children in non recursive types.
  if (type_ == FilePathWatcher::Type::kNonRecursive && !is_direct_child) {
    return;
  }

  // A deleted path may have already been replaced, so only use earlier
  // knowledge for deletions, never the current filesystem's type.
  change.file_path_type = change.change_type == ChangeType::kDeleted
                              ? GetKnownFilePathType(change.modified_path)
                              : GetFilePathType(change.modified_path);

  // Don't report modifications on directories.
  if (change.change_type == ChangeType::kModified &&
      change.file_path_type == FilePathWatcher::FilePathType::kDirectory) {
    return;
  }

  if (change.change_type == ChangeType::kDeleted) {
    last_deleted_change_ = std::move(change);
    return;
  }

  if (change.change_type == ChangeType::kMoved) {
    ConvertMoveToCreateIfOutOfScope(change);
  }

  changes_.push_back(std::move(change));
}

void FilePathWatcherChangeTracker::HandleAncestorChange(ChangeInfo change) {
  switch (change.change_type) {
    case ChangeType::kDeleted:
    case ChangeType::kCreated:
      // Can't delete a directory until all its children are gone. Can't create
      // a directory with existing children.
      HandleChangeEffect(ExistenceStatus::kGone, ExistenceStatus::kGone);
      break;
    case ChangeType::kMoved:
      HandleChangeEffect(ExistenceStatus::kGone,
                         ExistenceStatus::kMayHaveMovedIntoPlace);
      ConvertMoveToCreateIfOutOfScope(change);
      last_move_change_ = std::move(change);
      break;
    case ChangeType::kModified:
      // This tells us nothing.
      break;
    case ChangeType::kUnknown:
      // All changes passed into here come from `ToChangeType` which doesn't
      // return `kUnknown`.
      NOTREACHED();
  }
}

void FilePathWatcherChangeTracker::HandleOtherChange(ChangeInfo change) {
  // Only moved types have the possibility of being in scope.
  if (change.change_type != ChangeType::kMoved ||
      !change.moved_from_path.has_value()) {
    return;
  }

  base::FilePath moved_from_path = *std::move(change.moved_from_path);
  base::FilePath moved_to_path = std::move(change.modified_path);

  // Move out of scopes are reported as deleted.
  change.change_type = ChangeType::kDeleted;
  change.moved_from_path = std::nullopt;

  switch (FindPathRelation(target_path_, moved_from_path)) {
    case PathRelation::kSelf:
      HandleChangeEffect(ExistenceStatus::kExists, ExistenceStatus::kGone);

      change.file_path_type = GetFilePathType(moved_to_path);
      change.modified_path = std::move(moved_from_path);

      changes_.push_back(std::move(change));
      break;
    case PathRelation::kAncestor: {
      bool target_exists_before_move =
          target_status_ == ExistenceStatus::kExists;

      HandleChangeEffect(target_status_, ExistenceStatus::kGone);

      // If the target wasn't there before the move, then it wasn't moved.
      if (!target_exists_before_move) {
        break;
      }

      // Figure out where the target was exists now so that you can get its
      // `file_path_type`.
      base::FilePath target_move_to_path = GetMovedPathOfDescendant(
          target_path_, moved_from_path, moved_to_path);

      change.file_path_type = GetFilePathType(target_move_to_path);
      change.modified_path = target_path_;

      changes_.push_back(std::move(change));
      break;
    }
    case PathRelation::kDescendant:
    case PathRelation::kDirectChild:
      // Any notification on a descendant means the target existed before and
      // after.
      HandleChangeEffect(ExistenceStatus::kExists, ExistenceStatus::kExists);

      change.file_path_type = GetFilePathType(moved_to_path);
      change.modified_path = std::move(moved_from_path);

      changes_.push_back(std::move(change));
      break;
    case PathRelation::kOther:
      // We don't care about files moving around out of scope.
      break;
  }
}

FilePathWatcher::FilePathType FilePathWatcherChangeTracker::GetFilePathType(
    const base::FilePath& path) {
  auto type = GetKnownFilePathType(path);
  if (type != FilePathWatcher::FilePathType::kUnknown) {
    return type;
  }
  type = QueryFilePathType(path);
  if (type != FilePathWatcher::FilePathType::kUnknown) {
    known_file_path_types_.Put(path.GetComponents(), type);
  }
  return type;
}

FilePathWatcher::FilePathType
FilePathWatcherChangeTracker::GetKnownFilePathType(const base::FilePath& path) {
  auto it = known_file_path_types_.Get(path.GetComponents());
  return it == known_file_path_types_.end()
             ? FilePathWatcher::FilePathType::kUnknown
             : it->second;
}

void FilePathWatcherChangeTracker::RemoveKnownFilePathTypes(
    const base::FilePath& path) {
  const auto components = path.GetComponents();
  auto entry = known_file_path_types_.Peek(components);
  if (entry != known_file_path_types_.end() &&
      entry->second == FilePathWatcher::FilePathType::kFile) {
    known_file_path_types_.Erase(entry);
    return;
  }
  for (auto it = known_file_path_types_.begin();
       it != known_file_path_types_.end();) {
    if (std::ranges::starts_with(it->first, components)) {
      it = known_file_path_types_.Erase(it);
    } else {
      ++it;
    }
  }
}

void FilePathWatcherChangeTracker::MoveKnownFilePathTypes(
    const base::FilePath& from,
    const base::FilePath& to) {
  const auto from_components = from.GetComponents();
  const auto to_components = to.GetComponents();
  std::vector<std::pair<PathComponents, FilePathWatcher::FilePathType>> moved;
  for (auto it = known_file_path_types_.rbegin();
       it != known_file_path_types_.rend(); ++it) {
    if (std::ranges::starts_with(it->first, from_components)) {
      auto components = to_components;
      components.insert(components.end(),
                        it->first.begin() + from_components.size(),
                        it->first.end());
      moved.emplace_back(std::move(components), it->second);
    }
  }
  RemoveKnownFilePathTypes(from);
  RemoveKnownFilePathTypes(to);
  for (auto& [path, type] : moved) {
    known_file_path_types_.Put(std::move(path), type);
  }
}
}  // namespace content
