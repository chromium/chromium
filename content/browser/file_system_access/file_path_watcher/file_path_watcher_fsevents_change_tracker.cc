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

void FilePathWatcherFSEventsChangeTracker::ReportChangeEvent(
    FilePathWatcher::ChangeInfo change_info) {
  callback_.Run(std::move(change_info),
                report_modified_path_ ? change_info.modified_path : target_,
                /*error=*/false);
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
    //
    // TODO(crbug.com/381136602): Consider implementing queueing for calls to
    // `DispatchEvents` so that we can wait and avoid processing 'root changed'
    // events when possible, instead of reporting an event immediately when a
    // 'root changed' event occurs.
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
        ReportChangeEvent(
            {file_path_type, FilePathWatcher::ChangeType::kDeleted, target_});
        continue;
      }

      // Otherwise, a rename has occurred on the target path (which represents a
      // move into-scope), or the target has been created. Both scenarios are
      // reported as 'create' events.
      coalesce_next_target_creation_ = true;
      ReportChangeEvent(
          {file_path_type, FilePathWatcher::ChangeType::kCreated, target_});
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
          ReportChangeEvent({file_path_type,
                             FilePathWatcher::ChangeType::kDeleted,
                             event_path});
          continue;
        }

        if (!event_in_scope) {
          ReportChangeEvent({file_path_type,
                             FilePathWatcher::ChangeType::kCreated,
                             move_to_event_path});
          continue;
        }

        // Both the current event and the next event must be in-scope for a
        // move within-scope to be reported.
        ReportChangeEvent({file_path_type, FilePathWatcher::ChangeType::kMoved,
                           move_to_event_path, event_path});
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
      if (exists) {
        ReportChangeEvent({file_path_type,
                           FilePathWatcher::ChangeType::kCreated, event_path});
      } else {
        ReportChangeEvent({file_path_type,
                           FilePathWatcher::ChangeType::kDeleted, event_path});
      }
      continue;
    }

    // Determine which of the remaining change event types is reported (created,
    // modified, or deleted). Only report events that are in-scope.
    if (!event_in_scope) {
      continue;
    }

    // When the `kFSEventStreamEventFlagItemCreated`,
    // `kFSEventStreamEventFlagItemInodeMetaMod` and
    // `kFSEventStreamEventFlagItemModified` flags are present, this is a
    // signal that the contents of a file have been modified.
    if ((event_flags & kFSEventStreamEventFlagItemCreated) &&
        (event_flags & kFSEventStreamEventFlagItemInodeMetaMod) &&
        (event_flags & kFSEventStreamEventFlagItemModified)) {
      // Only report a 'modified' event if the removed event flag is not
      // present.
      if (!(event_flags & kFSEventStreamEventFlagItemRemoved)) {
        ReportChangeEvent({file_path_type,
                           FilePathWatcher::ChangeType::kModified, event_path});
        continue;
      }

      // Otherwise, both a 'created' and a 'modified' event should be reported.
      // The 'deleted' event is reported if it has not been coalesced.
      ReportChangeEvent(
          {file_path_type, FilePathWatcher::ChangeType::kCreated, event_path});
      ReportChangeEvent(
          {file_path_type, FilePathWatcher::ChangeType::kModified, event_path});

      if (coalesce_target_deletion && event_path == target_) {
        coalesce_next_target_deletion_ = false;
        continue;
      }
      ReportChangeEvent(
          {file_path_type, FilePathWatcher::ChangeType::kDeleted, event_path});
      continue;
    }

    if (event_flags & kFSEventStreamEventFlagItemRemoved) {
      // Skip this event if it's been coalesced.
      if (event_path == target_ && coalesce_target_deletion) {
        coalesce_next_target_deletion_ = false;
        continue;
      }

      struct stat file_recreated_stat;
      bool file_recreated_after_deletion =
          (stat(event_path.value().c_str(), &file_recreated_stat) == 0) &&
          (file_recreated_stat.st_ino == event_inode.value_or(0));

      // It's possible the file has been re-created immediately after deletion.
      // Report the 'deleted' event first.
      if (file_recreated_after_deletion) {
        ReportChangeEvent({file_path_type,
                           FilePathWatcher::ChangeType::kDeleted, event_path});
      } else {
        // The file has been deleted and does not exist.
        if (event_flags & kFSEventStreamEventFlagItemCreated) {
          // Special handling if the file does not exist, but there's a created
          // event flag present. We have to handle this flag to make sure
          // no events are missed.
          if (event_path == target_ && coalesce_target_creation) {
            // In this case, we previously reported a 'created' event in
            // evaluating a 'root changed' event on the prior call to
            // `DispatchEvents`. The target does not exist, despite being
            // reported as 'created' based on the previous 'root changed' event.
            //
            // Based on testing, this means that the target was deleted
            // immediately before being re-created, which is why the previous
            // 'root changed' event was reported as a 'created' event instead of
            // 'deleted', and `coalesce_target_creation` evaluates to `true`.
            // This seems to be an FSEvents peculiarty that could be corrected /
            // handled by implementing queueuing for calls to `DispatchEvents`
            // (crbug.com/381136602).
            //
            // While this is considered an edge case scenario, in order to
            // achieve "best effort" reporting of change events for this edge
            // case, we need to additionally reset the
            // `coalesce_target_creation` bit. The current event represents a
            // 'deleted' event, and the `coalesce_target_creation` bit was set
            // unexpectedly as a result of the previous call to
            // `DispatchEvents`, as described above. This prevents erroneously
            // coalescing the potential, following 'created' event that arrives
            // in the next iteration of `events`.
            coalesce_target_creation = false;
            coalesce_next_target_creation_ = false;
          } else if (!(event_flags & kFSEventStreamEventFlagItemModified)) {
            // Otherwise, based on testing, only report the a 'created' event
            // before reporting a 'deleted' event if the modified event flag is
            // *not* present.
            ReportChangeEvent({file_path_type,
                               FilePathWatcher::ChangeType::kCreated,
                               event_path});
          }
        }
        // Since the file has not been re-created after deletion, do not report
        // any events after the 'deleted' event is reported.
        ReportChangeEvent({file_path_type,
                           FilePathWatcher::ChangeType::kDeleted, event_path});
        continue;
      }
    }

    if (event_flags & kFSEventStreamEventFlagItemCreated) {
      // Even if the 'created' event has been coalesced as a result of the
      // target being created initially as a 'root changed' event, we still
      // want to carry on and process a modified event flag if it exists in
      // `event_flags`.
      //
      // This is a "best effort" attempt to maintain the expectation that a new
      // file write will result in two events (created + modified), even when
      // this occurs as a result of `target_`'s initial creation.
      if (event_path == target_ && coalesce_target_creation) {
        coalesce_next_target_creation_ = false;
      } else {
        ReportChangeEvent({file_path_type,
                           FilePathWatcher::ChangeType::kCreated, event_path});
      }
    }

    if (event_flags & kFSEventStreamEventFlagItemModified) {
      ReportChangeEvent(
          {file_path_type, FilePathWatcher::ChangeType::kModified, event_path});
    }
  }
}
}  // namespace content
