// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FILE_SYSTEM_ACCESS_FILE_PATH_WATCHER_FILE_PATH_WATCHER_FSEVENTS_H_
#define CONTENT_BROWSER_FILE_SYSTEM_ACCESS_FILE_PATH_WATCHER_FILE_PATH_WATCHER_FSEVENTS_H_

#include <CoreServices/CoreServices.h>
#include <stddef.h>

#include <map>
#include <memory>
#include <vector>

#include "base/apple/scoped_dispatch_object.h"
#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/file_system_access/file_path_watcher/file_path_watcher.h"
#include "content/browser/file_system_access/file_path_watcher/file_path_watcher_fsevents_change_tracker.h"
#include "content/browser/file_system_access/file_path_watcher/file_path_watcher_histogram.h"

namespace content {

// Mac-specific file watcher implementation based on FSEvents.
// There are trade-offs between the FSEvents implementation and a kqueue
// implementation. The biggest issues are that FSEvents on 10.6 sometimes drops
// events and kqueue does not trigger for modifications to a file in a watched
// directory. See file_path_watcher_mac.cc for the code that decides when to
// use which one.
class FilePathWatcherFSEvents : public FilePathWatcher::PlatformDelegate {
 public:
  using ChangeEvent = FilePathWatcherFSEventsChangeTracker::ChangeEvent;

  static constexpr size_t kNumberOfWatches = 1;

  FilePathWatcherFSEvents();
  FilePathWatcherFSEvents(const FilePathWatcherFSEvents&) = delete;
  FilePathWatcherFSEvents& operator=(const FilePathWatcherFSEvents&) = delete;
  ~FilePathWatcherFSEvents() override;

  size_t current_usage() const override;

  // FilePathWatcher::PlatformDelegate overrides.
  bool Watch(const base::FilePath& path,
             Type type,
             const FilePathWatcher::Callback& callback) override;
  bool WatchWithChangeInfo(
      const base::FilePath& path,
      const WatchOptions& options,
      const FilePathWatcher::CallbackWithChangeInfo& callback,
      const FilePathWatcher::UsageChangeCallback& usage_callback) override;
  void Cancel() override;

 private:
  static void FSEventsCallback(ConstFSEventStreamRef stream,
                               void* event_watcher,
                               size_t num_events,
                               void* event_paths,
                               const FSEventStreamEventFlags flags[],
                               const FSEventStreamEventId event_ids[]);

  // Called on the watcher task runner from the FSEventsCallback whenever
  // there is a change to the paths.
  void OnFilePathsChanged(bool is_root_changed_event,
                          FSEventStreamEventId root_change_at,
                          std::map<FSEventStreamEventId, ChangeEvent> events);

  // (Re-)Initialize the event stream to start reporting events from
  // |start_event|.
  WatchWithChangeInfoResult UpdateEventStream(FSEventStreamEventId start_event);

  // Returns true if resolving the target path got a different result than
  // last time it was done.
  bool ResolveTargetPath();

  // Report an error watching the given target.
  void ReportError(const base::FilePath& target);

  // Destroy the event stream.
  void DestroyEventStream();

  // Start watching the FSEventStream. Returns `true` if the FS Events event
  // stream starts successfully.
  bool StartEventStream(FSEventStreamEventId start_event,
                        const base::FilePath& path);

  std::optional<FilePathWatcherFSEventsChangeTracker> change_tracker_;

  // Callback to notify upon changes.
  // (Only accessed from the task_runner() thread).
  FilePathWatcher::CallbackWithChangeInfo callback_;

  // The dispatch queue on which the event stream is scheduled.
  base::apple::ScopedDispatchObject<dispatch_queue_t> queue_;

  base::FilePath target_;
  base::FilePath resolved_target_;

  // Backend stream we receive event callbacks from (strong reference).
  // (Only accessed from the libdispatch queue.)
  FSEventStreamRef fsevent_stream_ = nullptr;

  base::WeakPtrFactory<FilePathWatcherFSEvents> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_FILE_SYSTEM_ACCESS_FILE_PATH_WATCHER_FILE_PATH_WATCHER_FSEVENTS_H_
