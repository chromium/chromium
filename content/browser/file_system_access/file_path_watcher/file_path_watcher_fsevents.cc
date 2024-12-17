// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/file_system_access/file_path_watcher/file_path_watcher_fsevents_change_tracker.h"
#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include <dispatch/dispatch.h>

#include <algorithm>
#include <list>

#include "base/apple/foundation_util.h"
#include "base/apple/scoped_cftyperef.h"
#include "base/check.h"
#include "base/containers/contains.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/lazy_instance.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/scoped_blocking_call.h"
#include "content/browser/file_system_access/file_path_watcher/file_path_watcher.h"
#include "content/browser/file_system_access/file_path_watcher/file_path_watcher_fsevents.h"

namespace content {

namespace {

// The latency parameter passed to FSEventsStreamCreate().
const CFAbsoluteTime kEventLatencySeconds = 0.7;

// Resolve any symlinks in the path.
base::FilePath ResolvePath(const base::FilePath& path) {
  const unsigned kMaxLinksToResolve = 255;

  std::vector<base::FilePath::StringType> component_vector =
      path.GetComponents();
  std::list<base::FilePath::StringType> components(component_vector.begin(),
                                                   component_vector.end());

  base::FilePath result;
  unsigned resolve_count = 0;
  while (resolve_count < kMaxLinksToResolve && !components.empty()) {
    base::FilePath component(*components.begin());
    components.pop_front();

    base::FilePath current;
    if (component.IsAbsolute()) {
      current = component;
    } else {
      current = result.Append(component);
    }

    base::FilePath target;
    if (base::ReadSymbolicLink(current, &target)) {
      if (target.IsAbsolute()) {
        result.clear();
      }
      std::vector<base::FilePath::StringType> target_components =
          target.GetComponents();
      components.insert(components.begin(), target_components.begin(),
                        target_components.end());
      resolve_count++;
    } else {
      result = current;
    }
  }

  if (resolve_count >= kMaxLinksToResolve) {
    result.clear();
  }
  return result;
}
}  // namespace

FilePathWatcherFSEvents::FilePathWatcherFSEvents()
    : queue_(dispatch_queue_create(
          base::StringPrintf(
              "org.chromium.file_system_access.FilePathWatcher.%p",
              this)
              .c_str(),
          DISPATCH_QUEUE_SERIAL)) {}

FilePathWatcherFSEvents::~FilePathWatcherFSEvents() {
  DCHECK(!task_runner() || task_runner()->RunsTasksInCurrentSequence());
  DCHECK(callback_.is_null())
      << "Cancel() must be called before FilePathWatcher is destroyed.";
}

size_t FilePathWatcherFSEvents::current_usage() const {
  return kNumberOfWatches;
}

bool FilePathWatcherFSEvents::Watch(const base::FilePath& path,
                                    Type type,
                                    const FilePathWatcher::Callback& callback) {
  DCHECK(!callback.is_null());
  DCHECK(callback_.is_null());

  // This class could support non-recursive watches, but that is currently
  // left to FilePathWatcherKQueue.
  if (type != Type::kRecursive) {
    return false;
  }
  return WatchWithChangeInfo(
      path, WatchOptions{.type = type},
      base::IgnoreArgs<const FilePathWatcher::ChangeInfo&>(
          base::BindRepeating(std::move(callback))),
      base::DoNothingAs<void(size_t, size_t)>());
}

bool FilePathWatcherFSEvents::WatchWithChangeInfo(
    const base::FilePath& path,
    const WatchOptions& options,
    const FilePathWatcher::CallbackWithChangeInfo& callback,
    const FilePathWatcher::UsageChangeCallback& usage_callback) {
  set_task_runner(base::SequencedTaskRunner::GetCurrentDefault());
  change_tracker_ = FilePathWatcherFSEventsChangeTracker(
      callback, path, options.type, options.report_modified_path);
  callback_ = callback;

  FSEventStreamEventId start_event = kFSEventStreamEventIdSinceNow;
  return StartEventStream(start_event, path);
}

void FilePathWatcherFSEvents::Cancel() {
  set_cancelled();
  callback_.Reset();

  if (fsevent_stream_) {
    DestroyEventStream();
    target_.clear();
    resolved_target_.clear();
  }
}

// static
void FilePathWatcherFSEvents::FSEventsCallback(
    ConstFSEventStreamRef stream,
    void* event_watcher,
    size_t num_events,
    void* event_paths,
    const FSEventStreamEventFlags flags[],
    const FSEventStreamEventId event_ids[]) {
  FilePathWatcherFSEvents* watcher =
      reinterpret_cast<FilePathWatcherFSEvents*>(event_watcher);
  bool is_root_changed_event = false;

  // The `root_changed_at` value represents the highest-numbered FSEvents event
  // id, given that FSEvents events have unique + increasing event id values
  // over time. The highest event id is updated upon receiving an event, and
  // just before invoking the client's callback
  // (https://developer.apple.com/documentation/coreservices/1446030-fseventstreamgetlatesteventid).
  FSEventStreamEventId root_change_at = FSEventStreamGetLatestEventId(stream);
  CFArrayRef cf_event_paths = base::apple::CFCast<CFArrayRef>(event_paths);
  std::map<FSEventStreamEventId, ChangeEvent> events;

  for (size_t i = 0; i < num_events; i++) {
    const FSEventStreamEventFlags event_flags = flags[i];

    // Ignore this sentinel event, per FSEvents guidelines:
    // (https://developer.apple.com/documentation/coreservices/1455361-fseventstreameventflags/kfseventstreameventflaghistorydone).
    if (event_flags & kFSEventStreamEventFlagHistoryDone) {
      continue;
    }

    if (event_flags & kFSEventStreamEventFlagRootChanged) {
      is_root_changed_event = true;
    }

    const FSEventStreamEventId event_id = event_ids[i];
    if (event_id) {
      root_change_at = std::min(root_change_at, event_id);
    }

    // Determine the inode value for the event.
    CFDictionaryRef cf_dict = base::apple::CFCast<CFDictionaryRef>(
        CFArrayGetValueAtIndex(cf_event_paths, i));
    CFStringRef cf_path = base::apple::GetValueFromDictionary<CFStringRef>(
        cf_dict, kFSEventStreamEventExtendedDataPathKey);
    const base::FilePath event_path = base::apple::CFStringToFilePath(cf_path);

    // Only report events with a non-empty path.
    if (event_path.empty()) {
      continue;
    }

    CFNumberRef cf_inode = base::apple::GetValueFromDictionary<CFNumberRef>(
        cf_dict, kFSEventStreamEventExtendedFileIDKey);
    if (cf_inode) {
      SInt64 sint_inode;
      if (CFNumberGetValue(cf_inode, kCFNumberSInt64Type, &sint_inode)) {
        events[event_id] = ChangeEvent(event_flags, event_path,
                                       static_cast<uint64_t>(sint_inode));
        continue;
      }
    }
    events[event_id] = ChangeEvent(event_flags, event_path, std::nullopt);
  }
  watcher->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&FilePathWatcherFSEvents::OnFilePathsChanged,
                     watcher->weak_factory_.GetWeakPtr(), is_root_changed_event,
                     root_change_at, std::move(events)));
}

void FilePathWatcherFSEvents::OnFilePathsChanged(
    bool is_root_changed_event,
    FSEventStreamEventId root_change_at,
    std::map<FSEventStreamEventId, ChangeEvent> events) {
  // If we receive a root changed event, or the resolved target path has
  // changed, update the FSEvents event stream.
  if (is_root_changed_event || ResolveTargetPath()) {
    WatchWithChangeInfoResult update_stream_result =
        UpdateEventStream(root_change_at);

    if (update_stream_result != WatchWithChangeInfoResult::kSuccess) {
      // Failed to re-initialize the FSEvents event stream.
      RecordCallbackErrorUma(update_stream_result);
      ReportError(target_);
    }
  }

  // Only call `DispatchEvents` when there are events to process.
  if (!events.empty()) {
    change_tracker_->DispatchEvents(std::move(events));
  }
}

WatchWithChangeInfoResult FilePathWatcherFSEvents::UpdateEventStream(
    FSEventStreamEventId start_event) {
  // It can happen that the watcher gets canceled while tasks that call this
  // function are still in flight, so abort if this situation is detected.
  if (resolved_target_.empty()) {
    return WatchWithChangeInfoResult::kFSEventsResolvedTargetError;
  }

  if (fsevent_stream_) {
    DestroyEventStream();
  }

  base::apple::ScopedCFTypeRef<CFStringRef> cf_path =
      base::apple::FilePathToCFString(resolved_target_);
  CFStringRef paths_array[] = {cf_path.get()};

  static_assert(std::size(paths_array) == kNumberOfWatches,
                "Update kNumberOfWatches to equal the number of paths we're "
                "watching so that usage is reported accurately.");

  base::apple::ScopedCFTypeRef<CFArrayRef> watched_paths(
      CFArrayCreate(NULL, reinterpret_cast<const void**>(paths_array),
                    std::size(paths_array), &kCFTypeArrayCallBacks));

  FSEventStreamContext context;
  context.version = 0;
  context.info = this;
  context.retain = NULL;
  context.release = NULL;
  context.copyDescription = NULL;

  // The parameters of `FSEventStreamCreate` are defined by the FSEvents API:
  // (https://developer.apple.com/documentation/coreservices/1443980-fseventstreamcreate).
  fsevent_stream_ = FSEventStreamCreate(
      NULL, &FSEventsCallback, &context, watched_paths.get(), start_event,
      kEventLatencySeconds,
      kFSEventStreamCreateFlagWatchRoot | kFSEventStreamCreateFlagFileEvents |
          kFSEventStreamCreateFlagNoDefer | kFSEventStreamCreateFlagUseCFTypes |
          kFSEventStreamCreateFlagUseExtendedData);

  // Schedule the stream on the `queue_`
  // (https://developer.apple.com/documentation/coreservices/1444164-fseventstreamsetdispatchqueue).
  FSEventStreamSetDispatchQueue(fsevent_stream_, queue_.get());

  // Start the event stream, by attempting to register with the FSEvents service
  // to receive events, according to the stream parameters
  // (https://developer.apple.com/documentation/coreservices/1448000-fseventstreamstart).
  if (FSEventStreamStart(fsevent_stream_)) {
    return WatchWithChangeInfoResult::kSuccess;
  }
  return WatchWithChangeInfoResult::kFSEventsEventStreamStartError;
}

bool FilePathWatcherFSEvents::ResolveTargetPath() {
  base::FilePath resolved = ResolvePath(target_).StripTrailingSeparators();
  bool changed = resolved != resolved_target_;
  resolved_target_ = resolved;
  return changed;
}

void FilePathWatcherFSEvents::ReportError(const base::FilePath& target) {
  DCHECK(task_runner()->RunsTasksInCurrentSequence());
  if (!callback_.is_null()) {
    callback_.Run(FilePathWatcher::ChangeInfo(), target, true);
  }
}

void FilePathWatcherFSEvents::DestroyEventStream() {
  // Unregister the FSEvents service. The client callback will not be called for
  // this stream while it is stopped
  // (https://developer.apple.com/documentation/coreservices/1447673-fseventstreamstop).
  FSEventStreamStop(fsevent_stream_);

  // Stream will be unscheduled from any run loops or dispatch queues
  // (https://developer.apple.com/documentation/coreservices/1446990-fseventstreaminvalidate).
  FSEventStreamInvalidate(fsevent_stream_);

  // Decrement the stream's event count. If the refcount reaches zero, the
  // stream will be deallocated
  // (https://developer.apple.com/documentation/coreservices/1445989-fseventstreamrelease).
  FSEventStreamRelease(fsevent_stream_);
  fsevent_stream_ = nullptr;
}

bool FilePathWatcherFSEvents::StartEventStream(FSEventStreamEventId start_event,
                                               const base::FilePath& path) {
  DCHECK(resolved_target_.empty());

  target_ = path;
  ResolveTargetPath();
  WatchWithChangeInfoResult stream_start_result =
      UpdateEventStream(start_event);

  RecordWatchWithChangeInfoResultUma(stream_start_result);

  return stream_start_result == WatchWithChangeInfoResult::kSuccess;
}

}  // namespace content
