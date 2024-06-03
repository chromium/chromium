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

#include "base/apple/scoped_cftyperef.h"
#include "base/check.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/lazy_instance.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/scoped_blocking_call.h"

namespace content {

namespace {

// The latency parameter passed to FSEventsStreamCreate().
const CFAbsoluteTime kEventLatencySeconds = 0.3;

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

  set_task_runner(base::SequencedTaskRunner::GetCurrentDefault());
  callback_ = callback;

  FSEventStreamEventId start_event = kFSEventStreamEventIdSinceNow;
  StartEventStream(start_event, path);
  return true;
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
  bool root_changed = watcher->ResolveTargetPath();
  std::vector<base::FilePath> paths;
  FSEventStreamEventId root_change_at = FSEventStreamGetLatestEventId(stream);
  for (size_t i = 0; i < num_events; i++) {
    if (flags[i] & kFSEventStreamEventFlagRootChanged) {
      root_changed = true;
    }
    if (event_ids[i]) {
      root_change_at = std::min(root_change_at, event_ids[i]);
    }
    paths.push_back(base::FilePath(reinterpret_cast<char**>(event_paths)[i])
                        .StripTrailingSeparators());
  }

  // Reinitialize the event stream if we find changes to the root. This is
  // necessary since FSEvents doesn't report any events for the subtree after
  // the directory to be watched gets created.
  if (root_changed) {
    // Resetting the event stream from within the callback fails (FSEvents spews
    // bad file descriptor errors), so do the reset asynchronously.
    watcher->task_runner()->PostTask(
        FROM_HERE, base::BindOnce(
                       [](base::WeakPtr<FilePathWatcherFSEvents> weak_watcher,
                          FSEventStreamEventId root_change_at) {
                         if (!weak_watcher) {
                           return;
                         }
                         FilePathWatcherFSEvents* watcher = weak_watcher.get();
                         watcher->UpdateEventStream(root_change_at);
                       },
                       watcher->weak_factory_.GetWeakPtr(), root_change_at));
  }

  watcher->OnFilePathsChanged(paths);
}

void FilePathWatcherFSEvents::OnFilePathsChanged(
    const std::vector<base::FilePath>& paths) {
  DCHECK(!resolved_target_.empty());
  task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&FilePathWatcherFSEvents::DispatchEvents,
                                weak_factory_.GetWeakPtr(), paths, target_,
                                resolved_target_));
}

void FilePathWatcherFSEvents::DispatchEvents(
    const std::vector<base::FilePath>& paths,
    const base::FilePath& target,
    const base::FilePath& resolved_target) {
  DCHECK(task_runner()->RunsTasksInCurrentSequence());

  // Don't issue callbacks after Cancel() has been called.
  if (is_cancelled() || callback_.is_null()) {
    return;
  }

  for (const base::FilePath& path : paths) {
    if (resolved_target.IsParent(path) || resolved_target == path) {
      callback_.Run(target, false);
      return;
    }
  }
}

void FilePathWatcherFSEvents::UpdateEventStream(
    FSEventStreamEventId start_event) {
  // It can happen that the watcher gets canceled while tasks that call this
  // function are still in flight, so abort if this situation is detected.
  if (resolved_target_.empty()) {
    return;
  }

  if (fsevent_stream_) {
    DestroyEventStream();
  }

  base::apple::ScopedCFTypeRef<CFStringRef> cf_path(CFStringCreateWithCString(
      NULL, resolved_target_.value().c_str(), kCFStringEncodingMacHFS));
  base::apple::ScopedCFTypeRef<CFStringRef> cf_dir_path(
      CFStringCreateWithCString(NULL,
                                resolved_target_.DirName().value().c_str(),
                                kCFStringEncodingMacHFS));
  CFStringRef paths_array[] = {cf_path.get(), cf_dir_path.get()};
  base::apple::ScopedCFTypeRef<CFArrayRef> watched_paths(
      CFArrayCreate(NULL, reinterpret_cast<const void**>(paths_array),
                    std::size(paths_array), &kCFTypeArrayCallBacks));

  FSEventStreamContext context;
  context.version = 0;
  context.info = this;
  context.retain = NULL;
  context.release = NULL;
  context.copyDescription = NULL;

  fsevent_stream_ = FSEventStreamCreate(
      NULL, &FSEventsCallback, &context, watched_paths.get(), start_event,
      kEventLatencySeconds, kFSEventStreamCreateFlagWatchRoot);
  FSEventStreamSetDispatchQueue(fsevent_stream_, queue_.get());

  if (!FSEventStreamStart(fsevent_stream_)) {
    task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&FilePathWatcherFSEvents::ReportError,
                                  weak_factory_.GetWeakPtr(), target_));
  }
}

bool FilePathWatcherFSEvents::ResolveTargetPath() {
  base::FilePath resolved = ResolvePath(target_).StripTrailingSeparators();
  bool changed = resolved != resolved_target_;
  resolved_target_ = resolved;
  if (resolved_target_.empty()) {
    task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&FilePathWatcherFSEvents::ReportError,
                                  weak_factory_.GetWeakPtr(), target_));
  }
  return changed;
}

void FilePathWatcherFSEvents::ReportError(const base::FilePath& target) {
  DCHECK(task_runner()->RunsTasksInCurrentSequence());
  if (!callback_.is_null()) {
    callback_.Run(target, true);
  }
}

void FilePathWatcherFSEvents::DestroyEventStream() {
  FSEventStreamStop(fsevent_stream_);
  FSEventStreamInvalidate(fsevent_stream_);
  FSEventStreamRelease(fsevent_stream_);
  fsevent_stream_ = nullptr;
}

void FilePathWatcherFSEvents::StartEventStream(FSEventStreamEventId start_event,
                                               const base::FilePath& path) {
  DCHECK(resolved_target_.empty());

  target_ = path;
  ResolveTargetPath();
  UpdateEventStream(start_event);
}

}  // namespace content
