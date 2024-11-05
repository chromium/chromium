// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/file_system_access/file_path_watcher/file_path_watcher_fsevents_change_tracker.h"

#include "base/containers/contains.h"
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

bool IsPathInScope(const base::FilePath& target_path,
                   const base::FilePath& changed_path,
                   bool is_recursive) {
  PathRelation relation = FindPathRelation(target_path, changed_path);

  if (relation == PathRelation::kAncestor || relation == PathRelation::kOther) {
    return false;
  }

  if (!is_recursive && relation == PathRelation::kDescendant) {
    return false;
  }

  return true;
}

FilePathWatcher::FilePathType GetFilePathType(
    FSEventStreamEventFlags event_flags) {
  if (event_flags & kFSEventStreamEventFlagItemIsDir) {
    return FilePathWatcher::FilePathType::kDirectory;
  }
  if (event_flags & kFSEventStreamEventFlagItemIsFile) {
    return FilePathWatcher::FilePathType::kFile;
  }
  return FilePathWatcher::FilePathType::kUnknown;
}
}  // namespace

FilePathWatcherFSEventsChangeTracker::FilePathWatcherFSEventsChangeTracker(
    FilePathWatcher::CallbackWithChangeInfo callback,
    base::FilePath target,
    FilePathWatcher::Type type,
    bool report_modified_path)
    : task_runner_(base::SequencedTaskRunner::GetCurrentDefault()),
      callback_(std::move(callback)),
      target_(target),
      recursive_watch_(type == FilePathWatcher::Type::kRecursive),
      report_modified_path_(report_modified_path) {}

FilePathWatcherFSEventsChangeTracker::FilePathWatcherFSEventsChangeTracker(
    FilePathWatcherFSEventsChangeTracker&&) = default;
FilePathWatcherFSEventsChangeTracker::~FilePathWatcherFSEventsChangeTracker() =
    default;

scoped_refptr<base::SequencedTaskRunner>
FilePathWatcherFSEventsChangeTracker::task_runner() const {
  return task_runner_;
}

void FilePathWatcherFSEventsChangeTracker::DispatchEvents(
    std::map<FSEventStreamEventId, ChangeEvent> events) {
  DCHECK(task_runner()->RunsTasksInCurrentSequence());
  DCHECK(!target_.empty());

  // Don't issue callbacks after Cancel() has been called.
  if (callback_.is_null()) {
    return;
  }

  std::vector<FSEventStreamEventId> coalesced_event_ids;
  bool coalesce_target_deletion = coalesce_next_target_deletion_;
  bool coalesce_target_creation = coalesce_next_target_creation_;
  coalesce_next_target_deletion_ = false;
  coalesce_next_target_creation_ = false;

  for (auto it = events.begin(); it != events.end(); it++) {
    auto event_id = it->first;
    const auto& [event_flags, event_path, event_inode] = it->second;

    // Skip coalesced events.
    if (base::Contains(coalesced_event_ids, event_id)) {
      continue;
    }

    const FilePathWatcher::FilePathType file_path_type =
        GetFilePathType(event_flags);
    bool event_in_scope = IsPathInScope(target_, event_path, recursive_watch_);

    // Use the event flag values to determine which change event to report for a
    // given FSEvents event. Documentation of the different types of
    // FSEventStreamEventFlags can be found here:
    // https://developer.apple.com/documentation/coreservices/file_system_events/1455361-fseventstreameventflags
    //
    // The `kFSEventStreamEventFlagRootChanged` flag signals that there has been
    // a change along the root path.
    if (event_flags & kFSEventStreamEventFlagRootChanged) {
      // The event path should always be the same path as the target for a root
      // changed event. In the case that it's not, skip processing the event.
      if (event_path != target_) {
        // TODO(b/362494756): Cleanup usage of this macro once the File System
        // Change Observers feature is rolled out.
        DUMP_WILL_BE_NOTREACHED();
        continue;
      }

      // If the target path does not exist, either the target or one of its
      // parent directories have been deleted or renamed.
      struct stat buffer;
      if (stat(target_.value().c_str(), &buffer) == -1) {
        // If the next event is a deletion of the target path itself, coalesce
        // the following, duplicate delete event.
        coalesce_next_target_deletion_ = true;
        FilePathWatcher::ChangeInfo change_info = {
            file_path_type, FilePathWatcher::ChangeType::kDeleted, target_};
        callback_.Run(std::move(change_info), target_,
                      /*error=*/false);
        continue;
      }

      // Otherwise, a rename has occurred on the target path (which represents a
      // move into-scope), or the target has been created initially. Both
      // scenarios are reported as 'create' events.
      coalesce_next_target_creation_ = true;
      FilePathWatcher::ChangeInfo change_info = {
          file_path_type, FilePathWatcher::ChangeType::kCreated, target_};
      callback_.Run(std::move(change_info), target_,
                    /*error=*/false);
      continue;
    }

    // Use the `kFSEventStreamEventFlagItemRenamed` flag to identify a 'move'
    // event.
    if (event_flags & kFSEventStreamEventFlagItemRenamed) {
      // Find the matching moved_to event via inode.
      auto move_to_event_it = std::find_if(
          std::next(it, 1), events.end(),
          [event_inode](
              const std::pair<FSEventStreamEventId, ChangeEvent>& entry) {
            ChangeEvent change_event = entry.second;
            return change_event.event_inode == event_inode &&
                   (change_event.event_flags &
                    kFSEventStreamEventFlagItemRenamed);
          });
      if (move_to_event_it != events.end()) {
        const base::FilePath& move_to_event_path =
            move_to_event_it->second.event_path;
        bool move_to_event_in_scope =
            IsPathInScope(target_, move_to_event_path, recursive_watch_);
        if (!event_in_scope && !move_to_event_in_scope) {
          continue;
        }
        auto move_to_event_id = move_to_event_it->first;
        coalesced_event_ids.push_back(move_to_event_id);

        // In some cases such as an overwrite, FSEvents send additional event
        // with kFSEventStreamEventFlagItemRenamed on the moved_to path. This
        // causes additional "deleted" events on the next iteration, so we
        // want to ignore this event.
        auto ignore_event_it = std::find_if(
            std::next(it, 1), events.end(),
            [move_to_event_id, move_to_event_path](
                const std::pair<FSEventStreamEventId, ChangeEvent>& entry) {
              ChangeEvent change_event = entry.second;
              return move_to_event_id != entry.first &&
                     change_event.event_path == move_to_event_path &&
                     (change_event.event_flags &
                      kFSEventStreamEventFlagItemRenamed);
            });
        if (ignore_event_it != events.end()) {
          coalesced_event_ids.push_back(ignore_event_it->first);
        }

        // It can occur in non-recursive watches that a "matching" move
        // event is found (passes all checks for event id, event flags, and
        // inode comparison), but either the current event path or the next
        // event path is out of scope, from the implementation's
        // perspective. When this is the case, determine if a move in or
        // out-of-scope has taken place.
        if (!move_to_event_in_scope) {
          FilePathWatcher::ChangeInfo change_info = {
              file_path_type, FilePathWatcher::ChangeType::kDeleted,
              event_path};
          callback_.Run(std::move(change_info),
                        report_modified_path_ ? event_path : target_,
                        /*error=*/false);
          continue;
        }

        if (!event_in_scope) {
          FilePathWatcher::ChangeInfo change_info = {
              file_path_type, FilePathWatcher::ChangeType::kCreated,
              move_to_event_path};
          callback_.Run(std::move(change_info),
                        report_modified_path_ ? move_to_event_path : target_,
                        /*error=*/false);
          continue;
        }

        // Both the current event and the next event must be in-scope for a
        // move within-scope to be reported.
        FilePathWatcher::ChangeInfo change_info = {
            file_path_type, FilePathWatcher::ChangeType::kMoved,
            move_to_event_path, event_path};
        callback_.Run(std::move(change_info),
                      report_modified_path_ ? move_to_event_path : target_,
                      /*error=*/false);
        continue;
      }

      if (!event_in_scope) {
        continue;
      }

      // There is no "next event" found to compare the current "rename" event
      // with. Determine if a move into-scope or a move out-of-scope has taken
      // place.
      struct stat file_stat;
      bool exists = (stat(event_path.value().c_str(), &file_stat) == 0) &&
                    (file_stat.st_ino == event_inode.value_or(0));

      // If we've already reported a create event resulting from a move
      // into-scope for the target path, skip reporting a duplicate create
      // event which has already been reported as a result of the previous root
      // changed event.
      if (exists && event_path == target_ && coalesce_target_creation) {
        coalesce_next_target_creation_ = false;
        continue;
      }

      // If the current event's inode exists, the underlying file or
      // directory exists. This signals a move into-scope and is reported as
      // a 'created event. Otherwise, the event is reported as a 'deleted'
      // event.
      FilePathWatcher::ChangeInfo change_info = {
          file_path_type,
          exists ? FilePathWatcher::ChangeType::kCreated
                 : FilePathWatcher::ChangeType::kDeleted,
          event_path};
      callback_.Run(std::move(change_info),
                    report_modified_path_ ? event_path : target_,
                    /*error=*/false);
      continue;
    }

    // Determine which of the remaining change event types is reported (created,
    // modified, or deleted). Only report events that are in-scope.
    if (!event_in_scope) {
      continue;
    }

    // If `kFSEventStreamEventFlagItemRemoved` is present, prioritize reporting
    // that the file has been deleted.
    if (event_flags & kFSEventStreamEventFlagItemRemoved) {
      // Skip over coalesced delete events, that have already been reported for
      // a delete event on the target path.
      if (coalesce_target_deletion && event_path == target_) {
        coalesce_next_target_deletion_ = false;
        continue;
      }
      FilePathWatcher::ChangeInfo change_info = {
          file_path_type, FilePathWatcher::ChangeType::kDeleted, event_path};
      callback_.Run(std::move(change_info),
                    report_modified_path_ ? event_path : target_,
                    /*error=*/false);
      continue;
    }

    // When both the `kFSEventStreamEventFlagItemInodeMetaMod` and
    // `kFSEventStreamEventFlagItemModified` flags are present, this is a signal
    // that the contents of a file have been modified. This takes precedence
    // over reporting a 'create' event, given that it's possible for the
    // `kFSEventStreamEventFlagItemCreated` flag to be reported in the same
    // `event_flags` batch as both of the
    // `kFSEventStreamEventFlagItemInodeMetaMod` and
    // `kFSEventStreamEventFlagItemModified` flags.
    if ((event_flags & kFSEventStreamEventFlagItemInodeMetaMod) &&
        (event_flags & kFSEventStreamEventFlagItemModified)) {
      FilePathWatcher::ChangeInfo change_info = {
          file_path_type, FilePathWatcher::ChangeType::kModified, event_path};
      callback_.Run(std::move(change_info),
                    report_modified_path_ ? event_path : target_,
                    /*error=*/false);
      continue;
    }

    // The `kFSEventStreamEventFlagItemCreated` flag signals a create event.
    // The `kFSEventStreamEventFlagItemCreated` flag takes precedence over the
    // `kFSEventStreamEventFlagItemModified` flag, in the scenario that both the
    // `kFSEventStreamEventFlagItemCreated` and the
    // `kFSEventStreamEventFlagItemModified` flag are present in the same batch
    // of `event_flags`.
    if (event_flags & kFSEventStreamEventFlagItemCreated) {
      // If the current event is for the target path, skip reporting a duplicate
      // create event, since we've already reported one earlier as a result of
      // the previous root changed event.
      if (coalesce_target_creation && event_path == target_) {
        coalesce_next_target_creation_ = false;
        continue;
      }
      FilePathWatcher::ChangeInfo change_info = {
          file_path_type, FilePathWatcher::ChangeType::kCreated, event_path};
      callback_.Run(std::move(change_info),
                    report_modified_path_ ? event_path : target_,
                    /*error=*/false);
      continue;
    }

    // Otherwise, if the `kFSEventStreamEventFlagItemModified` flag is present,
    // report a 'modified' event.
    if (event_flags & kFSEventStreamEventFlagItemModified) {
      FilePathWatcher::ChangeInfo change_info = {
          file_path_type, FilePathWatcher::ChangeType::kModified, event_path};
      callback_.Run(std::move(change_info),
                    report_modified_path_ ? event_path : target_,
                    /*error=*/false);
      continue;
    }
  }
}
}  // namespace content
