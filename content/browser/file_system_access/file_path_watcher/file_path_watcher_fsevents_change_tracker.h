// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FILE_SYSTEM_ACCESS_FILE_PATH_WATCHER_FILE_PATH_WATCHER_FSEVENTS_CHANGE_TRACKER_H_
#define CONTENT_BROWSER_FILE_SYSTEM_ACCESS_FILE_PATH_WATCHER_FILE_PATH_WATCHER_FSEVENTS_CHANGE_TRACKER_H_

#include <CoreServices/CoreServices.h>

#include <map>

#include "content/browser/file_system_access/file_path_watcher/file_path_watcher.h"

namespace content {

class CONTENT_EXPORT FilePathWatcherFSEventsChangeTracker {
 public:
  using ChangeInfo = FilePathWatcher::ChangeInfo;
  using ChangeType = FilePathWatcher::ChangeType;

  // Represents a single FSEvents event.
  struct ChangeEvent {
    FSEventStreamEventFlags event_flags;
    base::FilePath event_path;
    std::optional<uint64_t> event_inode;
  };

  FilePathWatcherFSEventsChangeTracker(
      FilePathWatcher::CallbackWithChangeInfo callback,
      base::FilePath target,
      FilePathWatcher::Type type,
      bool report_modified_path);
  FilePathWatcherFSEventsChangeTracker(
      const FilePathWatcherFSEventsChangeTracker&) = delete;
  FilePathWatcherFSEventsChangeTracker& operator=(
      const FilePathWatcherFSEventsChangeTracker&) = delete;

  FilePathWatcherFSEventsChangeTracker(FilePathWatcherFSEventsChangeTracker&&);
  FilePathWatcherFSEventsChangeTracker& operator=(
      FilePathWatcherFSEventsChangeTracker&&) = default;

  ~FilePathWatcherFSEventsChangeTracker();

  // Called on the watcher task runner thread to dispatch path events.
  void DispatchEvents(std::map<FSEventStreamEventId, ChangeEvent> events);

 private:
  scoped_refptr<base::SequencedTaskRunner> task_runner() const;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  FilePathWatcher::CallbackWithChangeInfo callback_;

  base::FilePath target_;

  bool recursive_watch_;
  bool report_modified_path_;

  // Signals whether to check for a target deletion or creation event, and
  // coalesce the event if needed.
  bool coalesce_next_target_deletion_ = false;
  bool coalesce_next_target_creation_ = false;
};
}  // namespace content

#endif  // CONTENT_BROWSER_FILE_SYSTEM_ACCESS_FILE_PATH_WATCHER_FILE_PATH_WATCHER_FSEVENTS_CHANGE_TRACKER_H_
