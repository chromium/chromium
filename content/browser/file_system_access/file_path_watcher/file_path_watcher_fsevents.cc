// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "content/browser/file_system_access/file_path_watcher/file_path_watcher_fsevents.h"

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
          base::BindRepeating(std::move(callback))));
}

bool FilePathWatcherFSEvents::WatchWithChangeInfo(
    const base::FilePath& path,
    const WatchOptions& options,
    const FilePathWatcher::CallbackWithChangeInfo& callback) {
  set_task_runner(base::SequencedTaskRunner::GetCurrentDefault());
  recursive_watch_ = options.type == Type::kRecursive;
  report_modified_path_ = options.report_modified_path;
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
    DispatchEvents(std::move(events));
  }
}

void FilePathWatcherFSEvents::DispatchEvents(
    std::map<FSEventStreamEventId, ChangeEvent> events) {
  DCHECK(task_runner()->RunsTasksInCurrentSequence());
  DCHECK(!target_.empty());

  // Don't issue callbacks after Cancel() has been called.
  if (is_cancelled() || callback_.is_null()) {
    return;
  }

  std::vector<FSEventStreamEventId> coalesced_event_ids;
  bool coalesce_target_deletion = coalesce_next_target_deletion_;
  bool coalesce_target_creation = coalesce_next_target_creation_;
  coalesce_next_target_deletion_ = false;
  coalesce_next_target_creation_ = false;

  for (const auto& [event_id, event] : events) {
    const auto& [event_flags, event_path, event_inode] = event;

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
      // Based on testing, moves within-scope for FSEvents will have
      // consecutive event ids that differ by 1, and the event with the higher
      // event id represents the "moved to" part of a move event. This allows
      // us to check if there's a "matching" rename event, based on event id,
      // that needs to be coalesced in the case that a move within-scope has
      // occurred.
      const auto next_event_it = events.find(event_id + 1);
      if (next_event_it != events.end()) {
        ChangeEvent next_event = next_event_it->second;
        std::optional<uint64_t> next_event_inode = next_event.event_inode;
        const base::FilePath next_event_path = next_event.event_path;

        if ((next_event.event_flags & kFSEventStreamEventFlagItemRenamed) &&
            event_inode.has_value() && next_event_inode.has_value() &&
            event_inode == next_event_inode) {
          bool next_event_in_scope =
              IsPathInScope(target_, next_event_path, recursive_watch_);

          // Both the current event and the next event must be in-scope for a
          // move within-scope to be reported.
          if (event_in_scope && next_event_in_scope) {
            coalesced_event_ids.push_back(event_id + 1);
            FilePathWatcher::ChangeInfo change_info = {
                file_path_type, FilePathWatcher::ChangeType::kMoved,
                next_event_path, event_path};
            callback_.Run(std::move(change_info),
                          report_modified_path_ ? next_event_path : target_,
                          /*error=*/false);
            continue;
          }

          // It can occur in non-recursive watches that a "matching" move
          // event is found (passes all checks for event id, event flags, and
          // inode comparison), but either the current event path or the next
          // event path is out of scope, from the implementation's
          // perspective. When this is the case, determine if a move in or
          // out-of-scope has taken place.
          if (event_in_scope && !next_event_in_scope) {
            coalesced_event_ids.push_back(event_id + 1);
            FilePathWatcher::ChangeInfo change_info = {
                file_path_type, FilePathWatcher::ChangeType::kDeleted,
                event_path};
            callback_.Run(std::move(change_info),
                          report_modified_path_ ? event_path : target_,
                          /*error=*/false);
            continue;
          }
          if (!event_in_scope && next_event_in_scope) {
            coalesced_event_ids.push_back(event_id + 1);
            FilePathWatcher::ChangeInfo change_info = {
                file_path_type, FilePathWatcher::ChangeType::kCreated,
                next_event_path};
            callback_.Run(std::move(change_info),
                          report_modified_path_ ? next_event_path : target_,
                          /*error=*/false);
            continue;
          }
        }
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
