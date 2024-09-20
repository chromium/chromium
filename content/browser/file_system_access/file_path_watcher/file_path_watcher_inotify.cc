// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/file_system_access/file_path_watcher/file_path_watcher_inotify.h"

#include <errno.h>
#include <poll.h>
#include <stddef.h>
#include <string.h>
#include <sys/inotify.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <fstream>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/posix/eintr_wrapper.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/platform_thread.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/trace_event/base_tracing.h"
#include "base/types/expected.h"
#include "build/build_config.h"
#include "content/browser/file_system_access/file_path_watcher/file_path_watcher.h"
#include "content/browser/file_system_access/file_path_watcher/file_path_watcher_histogram.h"

namespace content {

namespace {

#if !BUILDFLAG(IS_FUCHSIA)

// The /proc path to max_user_watches.
constexpr char kInotifyMaxUserWatchesPath[] =
    "/proc/sys/fs/inotify/max_user_watches";

// This is a soft limit. If there are more than |kExpectedFilePathWatches|
// FilePathWatchers for a user, than they might affect each other's inotify
// watchers limit.
constexpr size_t kExpectedFilePathWatchers = 16u;

// The default max inotify watchers limit per user, if reading
// /proc/sys/fs/inotify/max_user_watches fails.
constexpr size_t kDefaultInotifyMaxUserWatches = 8192u;

#endif  // !BUILDFLAG(IS_FUCHSIA)

class FilePathWatcherImpl;
class InotifyReader;

// Used by test to override inotify watcher limit.
size_t g_override_max_inotify_watches = 0u;

class InotifyReaderThreadDelegate final
    : public base::PlatformThread::Delegate {
 public:
  explicit InotifyReaderThreadDelegate(int inotify_fd)
      : inotify_fd_(inotify_fd) {}
  InotifyReaderThreadDelegate(const InotifyReaderThreadDelegate&) = delete;
  InotifyReaderThreadDelegate& operator=(const InotifyReaderThreadDelegate&) =
      delete;
  ~InotifyReaderThreadDelegate() override = default;

 private:
  void ThreadMain() override;

  const int inotify_fd_;
};

// Singleton to manage all inotify watches.
// TODO(tony): It would be nice if this wasn't a singleton.
// http://crbug.com/38174
class InotifyReader {
 public:
  // Watch descriptor used by AddWatch() and RemoveWatch().
#if BUILDFLAG(IS_ANDROID)
  using Watch = uint32_t;
#else
  using Watch = int;
#endif

  // Record of watchers tracked for watch descriptors.
  struct WatcherEntry {
    scoped_refptr<base::SequencedTaskRunner> task_runner;
    base::WeakPtr<FilePathWatcherImpl> watcher;
  };

  static constexpr Watch kInvalidWatch = static_cast<Watch>(-1);
  static constexpr Watch kWatchLimitExceeded = static_cast<Watch>(-2);

  InotifyReader(const InotifyReader&) = delete;
  InotifyReader& operator=(const InotifyReader&) = delete;

  // Watch directory |path| for changes. |watcher| will be notified on each
  // change. Returns |kInvalidWatch| on failure.
  Watch AddWatch(const base::FilePath& path, FilePathWatcherImpl* watcher);

  // Remove |watch| if it's valid.
  void RemoveWatch(Watch watch, FilePathWatcherImpl* watcher);

  // Invoked on "inotify_reader" thread to notify relevant watchers.
  void OnInotifyEvent(const inotify_event* event);
  void OnInotifyMatchingMoveEvents(const inotify_event* moved_from_event,
                                   const inotify_event* moved_to_event);

  // Returns true if any paths are actively being watched.
  bool HasWatches();

 private:
  friend struct base::LazyInstanceTraitsBase<InotifyReader>;

  InotifyReader();
  // There is no destructor because |g_inotify_reader| is a
  // base::LazyInstance::Leaky object. Having a destructor causes build
  // issues with GCC 6 (http://crbug.com/636346).

  // Returns true on successful thread creation.
  bool StartThread();

  base::Lock lock_;

  // Tracks which FilePathWatcherImpls to be notified on which watches.
  // The tracked FilePathWatcherImpl is keyed by raw pointers for fast look up
  // and mapped to a WatchEntry that is used to safely post a notification.
  std::unordered_map<Watch, std::map<FilePathWatcherImpl*, WatcherEntry>>
      watchers_ GUARDED_BY(lock_);

  // File descriptor returned by inotify_init.
  const int inotify_fd_;

  // Thread delegate for the Inotify thread.
  InotifyReaderThreadDelegate thread_delegate_;

  // Flag set to true when startup was successful.
  bool valid_ = false;
};

class FilePathWatcherImpl : public FilePathWatcher::PlatformDelegate {
 public:
  FilePathWatcherImpl();
  FilePathWatcherImpl(const FilePathWatcherImpl&) = delete;
  FilePathWatcherImpl& operator=(const FilePathWatcherImpl&) = delete;
  ~FilePathWatcherImpl() override;

  // Called for each event coming from the watch on the original thread.
  // `fired_watch` identifies the watch that fired, `child_name` indicates
  // what has changed, and is relative to the currently watched path for
  // `fired_watch`. `event_mask` represents inotify_event.mask, providing event
  // metadata.
  void OnFilePathChanged(InotifyReader::Watch fired_watch,
                         const base::FilePath::StringType& child_name,
                         uint32_t event_mask);

  // Similar to `OnFilePathChanged()`, but specifically used for reporting
  // a coalesced event for matching IN_MOVED_FROM and IN_MOVED_TO events.
  // It attempts to report one coalesced event, if paths from both events are
  // found within the watched scope (i.e. move within the watched scope).
  // Otherwise, only one event may be reported (i.e. move out of or into the
  // watched scope).
  void OnFilePathChangedForMoveEvents(
      InotifyReader::Watch moved_from_watch,
      const base::FilePath::StringType& moved_from_child_name,
      InotifyReader::Watch moved_to_watch,
      const base::FilePath::StringType& moved_to_child_name,
      FilePathWatcher::FilePathType file_path_type);

  // Returns whether the number of inotify watches of this FilePathWatcherImpl
  // would exceed the limit if adding one more.
  bool WouldExceedWatchLimit() const;

  // Returns a WatcherEntry for this, must be called on the original sequence.
  InotifyReader::WatcherEntry GetWatcherEntry();

  void UpdateInotifyCountHighWaterMark() {
    int current_inotify_count =
        watches_.size() + recursive_watches_by_path_.size();
    inotify_count_high_water_mark_ =
        std::max(inotify_count_high_water_mark_, current_inotify_count);
  }

 private:
  // Start watching |path| for changes and notify |delegate| on each change.
  // Returns true if watch for |path| has been added successfully.
  bool Watch(const base::FilePath& path,
             Type type,
             const FilePathWatcher::Callback& callback) override;

  // A generalized version. It extends |Type|.
  bool WatchWithOptions(const base::FilePath& path,
                        const WatchOptions& flags,
                        const FilePathWatcher::Callback& callback) override;

  bool WatchWithChangeInfo(
      const base::FilePath& path,
      const WatchOptions& options,
      const FilePathWatcher::CallbackWithChangeInfo& callback) override;

  // Cancel the watch. This unregisters the instance with InotifyReader.
  void Cancel() override;

  // Finds the full modified path, given the path component `child_name`, and
  // updates the watches.
  enum ChangeProcessError {
    kNotFound,       // `child_name` is not found to be within the watch scope.
    kLimitExceeded,  // Error occurred while updating inotify watches.
  };
  base::expected<base::FilePath, ChangeProcessError>
  FindChangedPathAndUpdateWatches(InotifyReader::Watch fired_watch,
                                  const base::FilePath::StringType& child_name,
                                  FilePathWatcher::FilePathType file_path_type,
                                  bool created,
                                  bool deleted);

  // Inotify watches are installed for all directory components of |target_|.
  // A WatchEntry instance holds:
  // - |watch|: the watch descriptor for a component.
  // - |subdir|: the subdirectory that identifies the next component.
  //   - For the last component, there is no next component, so it is empty.
  // - |linkname|: the target of the symlink.
  //   - Only if the target being watched is a symbolic link.
  struct WatchEntry {
    explicit WatchEntry(const base::FilePath::StringType& dirname)
        : watch(InotifyReader::kInvalidWatch), subdir(dirname) {}

    InotifyReader::Watch watch;
    base::FilePath::StringType subdir;
    base::FilePath::StringType linkname;
  };

  // Reconfigure to watch for the most specific parent directory of |target_|
  // that exists. Also calls UpdateRecursiveWatches() below. Returns true if
  // watch limit is not hit. Otherwise, returns false.
  [[nodiscard]] bool UpdateWatches();

  // Reconfigure to recursively watch |target_| and all its sub-directories.
  // - This is a no-op if the watch is not recursive.
  // - If |target_| does not exist, then clear all the recursive watches.
  // - Assuming |target_| exists, passing kInvalidWatch as |fired_watch| forces
  //   addition of recursive watches for |target_|.
  // - Otherwise, only the directory associated with |fired_watch| and its
  //   sub-directories will be reconfigured.
  // Returns true if watch limit is not hit. Otherwise, returns false.
  [[nodiscard]] bool UpdateRecursiveWatches(InotifyReader::Watch fired_watch,
                                            bool is_dir);

  // Enumerate recursively through |path| and add / update watches.
  // Returns true if watch limit is not hit. Otherwise, returns false.
  [[nodiscard]] bool UpdateRecursiveWatchesForPath(const base::FilePath& path);

  // Do internal bookkeeping to update mappings between |watch| and its
  // associated full path |path|.
  void TrackWatchForRecursion(InotifyReader::Watch watch,
                              const base::FilePath& path);

  // Remove all the recursive watches.
  void RemoveRecursiveWatches();

  // |path| is a symlink to a non-existent target. Attempt to add a watch to
  // the link target's parent directory. Update |watch_entry| on success.
  // Returns true if watch limit is not hit. Otherwise, returns false.
  [[nodiscard]] bool AddWatchForBrokenSymlink(const base::FilePath& path,
                                              WatchEntry* watch_entry);

  bool HasValidWatchVector() const;

  // Invokes the callback with error, and cancels all watches. This occurs if
  // updating watches has caused the exceeded limit error.
  void CancelAndRunCallbackOnExceededLimit();

  // Callback to notify upon changes.
  FilePathWatcher::CallbackWithChangeInfo callback_;

  // The file or directory we're supposed to watch.
  base::FilePath target_;

  Type type_ = Type::kNonRecursive;
  bool report_modified_path_ = false;

  // The vector of watches and next component names for all path components,
  // starting at the root directory. The last entry corresponds to the watch for
  // |target_| and always stores an empty next component name in |subdir|.
  std::vector<WatchEntry> watches_;

  std::unordered_map<InotifyReader::Watch, base::FilePath>
      recursive_paths_by_watch_;
  std::map<base::FilePath, InotifyReader::Watch> recursive_watches_by_path_;

  int inotify_count_high_water_mark_ = 0;

  base::WeakPtrFactory<FilePathWatcherImpl> weak_factory_{this};
};

base::LazyInstance<InotifyReader>::Leaky g_inotify_reader =
    LAZY_INSTANCE_INITIALIZER;

void InotifyReaderThreadDelegate::ThreadMain() {
  base::PlatformThread::SetName("file_system_access_inotify_reader");

  std::array<pollfd, 1> fdarray{{{inotify_fd_, POLLIN, 0}}};

  while (true) {
    {
      // Wait until some inotify events are available.
      int poll_result = HANDLE_EINTR(poll(fdarray.data(), fdarray.size(), -1));
      if (poll_result < 0) {
        DPLOG(WARNING) << "poll failed";
        return;
      }
    }

    bool has_batch = true;
    while (has_batch) {
      // Adjust buffer size to current event queue size.
      int buffer_size;
      int ioctl_result =
          HANDLE_EINTR(ioctl(inotify_fd_, FIONREAD, &buffer_size));

      if (ioctl_result != 0 || buffer_size < 0) {
        DPLOG(WARNING) << "ioctl failed";
        return;
      }

      std::vector<char> buffer(static_cast<size_t>(buffer_size));

      ssize_t bytes_read = HANDLE_EINTR(
          read(inotify_fd_, buffer.data(), static_cast<size_t>(buffer_size)));

      if (bytes_read < 0) {
        DPLOG(WARNING) << "read from inotify fd failed";
        return;
      }

      // Most events are notified one by one, except for move events, which are
      // expected to come in a pair (IN_MOVED_FROM and IN_MOVED_TO) with a
      // matching cookie value. IN_MOVED_FROM event is expected to arrive right
      // before the matching IN_MOVED_TO event, but inotify does not guarantee
      // that these events are consecutive in the stream, or that they exist in
      // the same buffer read. (i.e. IN_MOVED_FROM  is the last item to fit in
      // the current buffer, so the matching IN_MOVED_TO is read in the next
      // buffer). We don't want to wait indefinitely for the matching pair due
      // to this lack of guarantee, so perform the best-effort coalescing of
      // move events only within the same buffer.
      inotify_event* pending_move_from_event = nullptr;
      for (size_t i = 0; i < static_cast<size_t>(bytes_read);) {
        inotify_event* event = reinterpret_cast<inotify_event*>(&buffer[i]);
        size_t event_size = sizeof(inotify_event) + event->len;
        DUMP_WILL_BE_CHECK_LE(i + event_size, static_cast<size_t>(bytes_read));
        i += event_size;

        if (event->mask & IN_IGNORED) {
          continue;
        }

        if (pending_move_from_event) {
          if (event->mask & IN_MOVED_TO &&
              pending_move_from_event->cookie == event->cookie) {
            // Matching IN_MOVED_TO is observed for the existing pending move.
            // Match up the two move events, and reset
            // `pending_move_from_event`.
            g_inotify_reader.Get().OnInotifyMatchingMoveEvents(
                pending_move_from_event, event);
            pending_move_from_event = nullptr;
            continue;
          }
          // No matching IN_MOVED_TO is observed for `pending_move_from_event`.
          // Flush and reset `pending_move_from_event`.
          g_inotify_reader.Get().OnInotifyEvent(pending_move_from_event);
          pending_move_from_event = nullptr;
        }

        if (event->mask & IN_MOVED_FROM) {
          // IN_MOVED_FROM event is observed. Save as `pending_move_from_event`,
          // so that it can attempt to find the matching IN_MOVED_TO event for
          // the next iteration.
          pending_move_from_event = event;
        } else {
          // Process other events as normal.
          g_inotify_reader.Get().OnInotifyEvent(event);
        }
      }

      // Poll with zero timeout to see if another batch is immediately
      // available. This allows coalescing move events across batches.
      int poll_result = HANDLE_EINTR(poll(fdarray.data(), fdarray.size(), 0));
      has_batch = poll_result > 0;

      if (poll_result < 0) {
        DPLOG(WARNING) << "poll failed";
        return;
      }

      // If we don't have another batch to process, assume any pending move-from
      // event doesn't have a matching move-to event.
      if (!has_batch && pending_move_from_event) {
        g_inotify_reader.Get().OnInotifyEvent(pending_move_from_event);
      }
    }
  }
}

InotifyReader::InotifyReader()
    : inotify_fd_(inotify_init()), thread_delegate_(inotify_fd_) {
  if (inotify_fd_ < 0) {
    PLOG(ERROR) << "inotify_init() failed";
    return;
  }

  if (!StartThread()) {
    return;
  }

  valid_ = true;
}

bool InotifyReader::StartThread() {
  // This object is LazyInstance::Leaky, so thread_delegate_ will outlive the
  // thread.
  return base::PlatformThread::CreateNonJoinable(0, &thread_delegate_);
}

InotifyReader::Watch InotifyReader::AddWatch(const base::FilePath& path,
                                             FilePathWatcherImpl* watcher) {
  if (!valid_) {
    return kInvalidWatch;
  }

  if (watcher->WouldExceedWatchLimit()) {
    return kWatchLimitExceeded;
  }

  base::AutoLock auto_lock(lock_);

  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::WILL_BLOCK);
  const int watch_int = inotify_add_watch(
      inotify_fd_, path.value().c_str(),
      IN_CREATE | IN_DELETE | IN_CLOSE_WRITE | IN_MOVE | IN_ONLYDIR);
  if (watch_int == -1) {
    return kInvalidWatch;
  }
  const Watch watch = static_cast<Watch>(watch_int);

  watchers_[watch].emplace(std::make_pair(watcher, watcher->GetWatcherEntry()));

  return watch;
}

void InotifyReader::RemoveWatch(Watch watch, FilePathWatcherImpl* watcher) {
  if (!valid_ || (watch == kInvalidWatch)) {
    return;
  }

  base::AutoLock auto_lock(lock_);

  auto watchers_it = watchers_.find(watch);
  if (watchers_it == watchers_.end()) {
    return;
  }

  auto& watcher_map = watchers_it->second;
  watcher_map.erase(watcher);

  if (watcher_map.empty()) {
    watchers_.erase(watchers_it);

    base::ScopedBlockingCall scoped_blocking_call(
        FROM_HERE, base::BlockingType::WILL_BLOCK);
    inotify_rm_watch(inotify_fd_, watch);
  }
}

void InotifyReader::OnInotifyEvent(const inotify_event* event) {
  base::FilePath::StringType child_name(event->len ? event->name
                                                   : FILE_PATH_LITERAL(""));
  base::AutoLock auto_lock(lock_);

  // In racing conditions, RemoveWatch() could grab `lock_` first and remove
  // the entry for `event->wd`.
  auto watchers_it = watchers_.find(static_cast<Watch>(event->wd));
  if (watchers_it == watchers_.end()) {
    return;
  }

  auto& watcher_map = watchers_it->second;
  for (const auto& entry : watcher_map) {
    auto& watcher_entry = entry.second;
    watcher_entry.task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(&FilePathWatcherImpl::OnFilePathChanged,
                       watcher_entry.watcher, static_cast<Watch>(event->wd),
                       child_name, event->mask));
  }
}

void InotifyReader::OnInotifyMatchingMoveEvents(
    const inotify_event* moved_from_event,
    const inotify_event* moved_to_event) {
  DUMP_WILL_BE_CHECK(moved_from_event && moved_to_event);
  DUMP_WILL_BE_CHECK((moved_from_event->mask & IN_MOVED_FROM) &&
                     (moved_to_event->mask & IN_MOVED_TO));
  DUMP_WILL_BE_CHECK((moved_from_event->mask & IN_ISDIR) ==
                     (moved_to_event->mask & IN_ISDIR));
  DUMP_WILL_BE_CHECK(moved_from_event->len != 0);
  DUMP_WILL_BE_CHECK(moved_to_event->len != 0);

  base::FilePath::StringType moved_from_event_child_name(
      moved_from_event->name);
  base::FilePath::StringType moved_to_event_child_name(moved_to_event->name);
  Watch moved_from_watch = static_cast<Watch>(moved_from_event->wd);
  Watch moved_to_watch = static_cast<Watch>(moved_to_event->wd);
  auto file_path_type = (moved_from_event->mask & IN_ISDIR)
                            ? FilePathWatcher::FilePathType::kDirectory
                            : FilePathWatcher::FilePathType::kFile;

  // In racing conditions, RemoveWatch() could grab `lock_` first and remove
  // the entry for `event->wd`.
  base::AutoLock auto_lock(lock_);

  // The set of watchers for IN_MOVED_FROM event is not necessarily the same as
  // the one for IN_MOVED_TO event. For the intersection of watchers set,
  // send the related move events to be processed further; for the rest of the
  // watchers, send a single move event.
  std::map<FilePathWatcherImpl*, WatcherEntry>* moved_from_watcher_map =
      nullptr;
  if (auto it = watchers_.find(moved_from_watch); it != watchers_.end()) {
    moved_from_watcher_map = &(it->second);
  }
  std::map<FilePathWatcherImpl*, WatcherEntry>* moved_to_watcher_map = nullptr;
  if (auto it = watchers_.find(moved_to_watch); it != watchers_.end()) {
    moved_to_watcher_map = &(it->second);
  }

  if (moved_from_watcher_map) {
    for (const auto& entry : *moved_from_watcher_map) {
      auto& watcher_entry = entry.second;
      if (moved_to_watcher_map && moved_to_watcher_map->contains(entry.first)) {
        watcher_entry.task_runner->PostTask(
            FROM_HERE,
            base::BindOnce(&FilePathWatcherImpl::OnFilePathChangedForMoveEvents,
                           watcher_entry.watcher, moved_from_watch,
                           moved_from_event_child_name, moved_to_watch,
                           moved_to_event_child_name, file_path_type));
      } else {
        watcher_entry.task_runner->PostTask(
            FROM_HERE, base::BindOnce(&FilePathWatcherImpl::OnFilePathChanged,
                                      watcher_entry.watcher, moved_from_watch,
                                      moved_from_event_child_name,
                                      moved_from_event->mask));
      }
    }
  }

  if (moved_to_watcher_map) {
    for (const auto& entry : *moved_to_watcher_map) {
      if (moved_from_watcher_map &&
          moved_from_watcher_map->contains(entry.first)) {
        // This moved_to event has been already posted with the matching
        // moved_from event, so we can skip it.
        continue;
      }
      auto& watcher_entry = entry.second;
      watcher_entry.task_runner->PostTask(
          FROM_HERE,
          base::BindOnce(&FilePathWatcherImpl::OnFilePathChanged,
                         watcher_entry.watcher, moved_to_watch,
                         moved_to_event_child_name, moved_to_event->mask));
    }
  }
}

bool InotifyReader::HasWatches() {
  base::AutoLock auto_lock(lock_);

  return !watchers_.empty();
}

FilePathWatcherImpl::FilePathWatcherImpl() = default;

FilePathWatcherImpl::~FilePathWatcherImpl() {
  DUMP_WILL_BE_CHECK(!task_runner() ||
                     task_runner()->RunsTasksInCurrentSequence());
}

void FilePathWatcherImpl::OnFilePathChanged(
    InotifyReader::Watch fired_watch,
    const base::FilePath::StringType& child_name,
    uint32_t event_mask) {
  DUMP_WILL_BE_CHECK(task_runner()->RunsTasksInCurrentSequence());
  DUMP_WILL_BE_CHECK(!watches_.empty());
  DUMP_WILL_BE_CHECK(HasValidWatchVector());

  auto file_path_type = event_mask & IN_ISDIR
                            ? FilePathWatcher::FilePathType::kDirectory
                            : FilePathWatcher::FilePathType::kFile;

  // Greedily select the most specific change type. It's possible that multiple
  // types may apply, so this is ordered by specificity (e.g. "created" may also
  // imply "modified", but the former is more useful).
  FilePathWatcher::ChangeType change_type;
  if (event_mask & (IN_CREATE | IN_MOVED_TO)) {
    // A non-paired IN_MOVED_TO event is considered as created.
    change_type = FilePathWatcher::ChangeType::kCreated;
  } else if (event_mask & (IN_DELETE | IN_MOVED_FROM)) {
    // A non-paired IN_MOVED_FROM event is considered as created.
    change_type = FilePathWatcher::ChangeType::kDeleted;
  } else if (event_mask & (IN_MODIFY | IN_CLOSE_WRITE)) {
    change_type = FilePathWatcher::ChangeType::kModified;
  } else {
    // Ignore other types of events.
    return;
  }
  auto result = FindChangedPathAndUpdateWatches(
      fired_watch, child_name, file_path_type,
      change_type == FilePathWatcher::ChangeType::kCreated,
      change_type == FilePathWatcher::ChangeType::kDeleted);

  if (!result.has_value()) {
    if (result.error() == ChangeProcessError::kLimitExceeded) {
      CancelAndRunCallbackOnExceededLimit();  // `this` may be deleted.
    }
    // No need to invoke the callback when the modified path is not found within
    // the watched scope (= ChangeProcessError::kNotFound)
    return;
  }

  FilePathWatcher::ChangeInfo change_info(file_path_type, change_type,
                                          result.value());
  callback_.Run(std::move(change_info),
                report_modified_path_ ? result.value() : target_,
                /*error=*/false);  // `this` may be deleted.
}

void FilePathWatcherImpl::OnFilePathChangedForMoveEvents(
    InotifyReader::Watch moved_from_watch,
    const base::FilePath::StringType& moved_from_child_name,
    InotifyReader::Watch moved_to_watch,
    const base::FilePath::StringType& moved_to_child_name,
    FilePathWatcher::FilePathType file_path_type) {
  DUMP_WILL_BE_CHECK(task_runner()->RunsTasksInCurrentSequence());
  DUMP_WILL_BE_CHECK(!watches_.empty());
  DUMP_WILL_BE_CHECK(HasValidWatchVector());

  auto moved_from_result = FindChangedPathAndUpdateWatches(
      moved_from_watch, moved_from_child_name, file_path_type,
      /*created=*/false, /*deleted=*/true);
  auto moved_to_result = FindChangedPathAndUpdateWatches(
      moved_to_watch, moved_to_child_name, file_path_type, /*created=*/true,
      /*deleted=*/false);

  if ((!moved_from_result.has_value() &&
       moved_from_result.error() == ChangeProcessError::kLimitExceeded) ||
      (!moved_to_result.has_value() &&
       moved_to_result.error() == ChangeProcessError::kLimitExceeded)) {
    // If either result yielded the limit exceeded error, no successful callback
    // should be run.
    CancelAndRunCallbackOnExceededLimit();  // `this` may be deleted.
    return;
  }

  if (moved_from_result.has_value() && moved_to_result.has_value()) {
    FilePathWatcher::ChangeInfo change_info(
        file_path_type, FilePathWatcher::ChangeType::kMoved,
        moved_to_result.value(), moved_from_result.value());
    callback_.Run(std::move(change_info),
                  report_modified_path_ ? moved_to_result.value() : target_,
                  /*error=*/false);  // `this` may be deleted.
  } else if (moved_from_result.has_value()) {
    // Report file/dir moved out of the watch scope as `ChangeType::kDeleted`.
    FilePathWatcher::ChangeInfo change_info(
        file_path_type, FilePathWatcher::ChangeType::kDeleted,
        moved_from_result.value());
    callback_.Run(std::move(change_info),
                  report_modified_path_ ? moved_from_result.value() : target_,
                  /*error=*/false);  // `this` may be deleted.
  } else if (moved_to_result.has_value()) {
    // Report file/dir moved into the watch scope as `ChangeType::kCreated`.
    FilePathWatcher::ChangeInfo change_info(
        file_path_type, FilePathWatcher::ChangeType::kCreated,
        moved_to_result.value());
    callback_.Run(std::move(change_info),
                  report_modified_path_ ? moved_to_result.value() : target_,
                  /*error=*/false);  // `this` may be deleted.
  }

  // No need to invoke the callback when neither of the modified path is found
  // within the watched scope (= ChangeProcessError::kNotFound)
}

base::expected<base::FilePath, FilePathWatcherImpl::ChangeProcessError>
FilePathWatcherImpl::FindChangedPathAndUpdateWatches(
    InotifyReader::Watch fired_watch,
    const base::FilePath::StringType& child,
    FilePathWatcher::FilePathType file_path_type,
    bool created,
    bool deleted) {
  DUMP_WILL_BE_CHECK(task_runner()->RunsTasksInCurrentSequence());

  // Used below to avoid multiple recursive updates.
  bool did_update = false;

  // Find the entries in |watches_| that correspond to |fired_watch|.
  for (size_t i = 0; i < watches_.size(); ++i) {
    const WatchEntry& watch_entry = watches_[i];
    if (fired_watch != watch_entry.watch) {
      continue;
    }

    // Check whether a path component of |target_| changed.
    bool change_on_target_path = child.empty() ||
                                 (child == watch_entry.linkname) ||
                                 (child == watch_entry.subdir);

    // Check if the change references |target_| or a direct child of |target_|.
    bool target_changed;
    if (watch_entry.subdir.empty()) {
      // The fired watch is for a WatchEntry without a subdir. Thus for a given
      // |target_| = "/path/to/foo", this is for "foo". Here, check either:
      // - the target has no symlink: it is the target and it changed.
      // - the target has a symlink, and it matches |child|.
      target_changed =
          (watch_entry.linkname.empty() || child == watch_entry.linkname);
    } else {
      // The fired watch is for a WatchEntry with a subdir. Thus for a given
      // |target_| = "/path/to/foo", this is for {"/", "/path", "/path/to"}.
      // So we can safely access the next WatchEntry since we have not reached
      // the end yet. Check |watch_entry| is for "/path/to", i.e. the next
      // element is "foo".
      bool next_watch_may_be_for_target = watches_[i + 1].subdir.empty();
      if (next_watch_may_be_for_target) {
        // The current |watch_entry| is for "/path/to", so check if the |child|
        // that changed is "foo".
        target_changed = watch_entry.subdir == child;
      } else {
        // The current |watch_entry| is not for "/path/to", so the next entry
        // cannot be "foo". Thus |target_| has not changed.
        target_changed = false;
      }
    }

    // Update watches if a directory component of the |target_| path
    // (dis)appears. Note that we don't add the additional restriction of
    // checking the event mask to see if it is for a directory here as changes
    // to symlinks on the target path will not have IN_ISDIR set in the event
    // masks. As a result we may sometimes call UpdateWatches() unnecessarily.
    if (change_on_target_path && (created || deleted) && !did_update) {
      if (!UpdateWatches()) {
        return base::unexpected(ChangeProcessError::kLimitExceeded);
      }
      did_update = true;
    }

    // Report the following events:
    //  - The target or a direct child of the target got changed (in case the
    //    watched path refers to a directory).
    //  - One of the parent directories got moved or deleted, since the target
    //    disappears in this case.
    //  - One of the parent directories appears. The event corresponding to
    //    the target appearing might have been missed in this case, so recheck.
    if (target_changed || (change_on_target_path && deleted) ||
        (change_on_target_path && created && PathExists(target_))) {
      if (!did_update) {
        if (!UpdateRecursiveWatches(
                fired_watch,
                file_path_type == FilePathWatcher::FilePathType::kDirectory)) {
          return base::unexpected(ChangeProcessError::kLimitExceeded);
        }
        did_update = true;
      }
      return base::ok(change_on_target_path ? target_ : target_.Append(child));
    }
  }

  if (Contains(recursive_paths_by_watch_, fired_watch)) {
    base::FilePath child_path =
        recursive_paths_by_watch_[fired_watch].Append(child);
    if (!did_update) {
      if (!UpdateRecursiveWatches(
              fired_watch,
              file_path_type == FilePathWatcher::FilePathType::kDirectory)) {
        return base::unexpected(ChangeProcessError::kLimitExceeded);
      }
    }
    return base::ok(child_path);
  }

  return base::unexpected(ChangeProcessError::kNotFound);
}

void FilePathWatcherImpl::CancelAndRunCallbackOnExceededLimit() {
  DUMP_WILL_BE_CHECK(task_runner()->RunsTasksInCurrentSequence());

  // Cancels all in-flight events from inotify thread.
  weak_factory_.InvalidateWeakPtrs();

  // Reset states and cancels all watches.
  auto callback = callback_;
  Cancel();

  // Fires the error callback. `this` may be deleted as a result of this call.
  callback.Run(FilePathWatcher::ChangeInfo(), target_, /*error=*/true);

  RecordCallbackErrorUma(WatchWithChangeInfoResult::kInotifyWatchLimitExceeded);
}

bool FilePathWatcherImpl::WouldExceedWatchLimit() const {
  DUMP_WILL_BE_CHECK(task_runner()->RunsTasksInCurrentSequence());

  // `watches_` contains inotify watches of all dir components of `target_`.
  // `recursive_paths_by_watch_` contains inotify watches for sub dirs under
  // `target_` of a Type::kRecursive watcher and keyed by inotify watches.
  // All inotify watches used by this FilePathWatcherImpl are either in
  // `watches_` or as a key in `recursive_paths_by_watch_`. As a result, the
  // two provide a good estimate on the number of inofiy watches used by this
  // FilePathWatcherImpl.
  const size_t number_of_inotify_watches =
      watches_.size() + recursive_paths_by_watch_.size();
  return number_of_inotify_watches >= GetMaxNumberOfInotifyWatches();
}

InotifyReader::WatcherEntry FilePathWatcherImpl::GetWatcherEntry() {
  DUMP_WILL_BE_CHECK(task_runner()->RunsTasksInCurrentSequence());
  return {task_runner(), weak_factory_.GetWeakPtr()};
}

bool FilePathWatcherImpl::Watch(const base::FilePath& path,
                                Type type,
                                const FilePathWatcher::Callback& callback) {
  return WatchWithChangeInfo(
      path, WatchOptions{.type = type},
      base::IgnoreArgs<const FilePathWatcher::ChangeInfo&>(
          base::BindRepeating(std::move(callback))));
}

bool FilePathWatcherImpl::WatchWithOptions(
    const base::FilePath& path,
    const WatchOptions& options,
    const FilePathWatcher::Callback& callback) {
  return WatchWithChangeInfo(
      path, options,
      base::IgnoreArgs<const FilePathWatcher::ChangeInfo&>(
          base::BindRepeating(std::move(callback))));
}

bool FilePathWatcherImpl::WatchWithChangeInfo(
    const base::FilePath& path,
    const WatchOptions& options,
    const FilePathWatcher::CallbackWithChangeInfo& callback) {
  DUMP_WILL_BE_CHECK(target_.empty());

  set_task_runner(base::SequencedTaskRunner::GetCurrentDefault());
  callback_ = callback;
  target_ = path;
  type_ = options.type;
  report_modified_path_ = options.report_modified_path;

  std::vector<base::FilePath::StringType> comps = target_.GetComponents();
  DUMP_WILL_BE_CHECK(!comps.empty());
  for (size_t i = 1; i < comps.size(); ++i) {
    watches_.emplace_back(comps[i]);
  }
  watches_.emplace_back(base::FilePath::StringType());
  UpdateInotifyCountHighWaterMark();

  if (!UpdateWatches()) {
    RecordWatchWithChangeInfoResultUma(
        WatchWithChangeInfoResult::kInotifyWatchLimitExceeded);
    Cancel();
    // Note `callback` is not invoked since false is returned.
    return false;
  }

  RecordWatchWithChangeInfoResultUma(WatchWithChangeInfoResult::kSuccess);

  return true;
}

void FilePathWatcherImpl::Cancel() {
  if (!callback_) {
    // Watch() was never called.
    set_cancelled();
    return;
  }

  DUMP_WILL_BE_CHECK(task_runner()->RunsTasksInCurrentSequence());
  DUMP_WILL_BE_CHECK(!is_cancelled());

  set_cancelled();
  callback_.Reset();

  for (const auto& watch : watches_) {
    g_inotify_reader.Get().RemoveWatch(watch.watch, this);
  }
  watches_.clear();
  target_.clear();
  RemoveRecursiveWatches();

  RecordInotifyWatchCountUma(inotify_count_high_water_mark_);
}

bool FilePathWatcherImpl::UpdateWatches() {
  // Ensure this runs on the task_runner() exclusively in order to avoid
  // concurrency issues.
  DUMP_WILL_BE_CHECK(task_runner()->RunsTasksInCurrentSequence());
  DUMP_WILL_BE_CHECK(HasValidWatchVector());

  // Walk the list of watches and update them as we go.
  base::FilePath path(FILE_PATH_LITERAL("/"));
  for (WatchEntry& watch_entry : watches_) {
    InotifyReader::Watch old_watch = watch_entry.watch;
    watch_entry.watch = InotifyReader::kInvalidWatch;
    watch_entry.linkname.clear();
    watch_entry.watch = g_inotify_reader.Get().AddWatch(path, this);
    if (watch_entry.watch == InotifyReader::kWatchLimitExceeded) {
      return false;
    }
    if (watch_entry.watch == InotifyReader::kInvalidWatch) {
      // Ignore the error code (beyond symlink handling) to attempt to add
      // watches on accessible children of unreadable directories. Note that
      // this is a best-effort attempt; we may not catch events in this
      // scenario.
      if (IsLink(path)) {
        if (!AddWatchForBrokenSymlink(path, &watch_entry)) {
          return false;
        }
      }
    }
    if (old_watch != watch_entry.watch) {
      g_inotify_reader.Get().RemoveWatch(old_watch, this);
    }
    path = path.Append(watch_entry.subdir);
  }

  return UpdateRecursiveWatches(InotifyReader::kInvalidWatch, /*is_dir=*/false);
}

bool FilePathWatcherImpl::UpdateRecursiveWatches(
    InotifyReader::Watch fired_watch,
    bool is_dir) {
  DUMP_WILL_BE_CHECK(HasValidWatchVector());

  if (type_ != Type::kRecursive) {
    return true;
  }

  if (!DirectoryExists(target_)) {
    RemoveRecursiveWatches();
    return true;
  }

  // Check to see if this is a forced update or if some component of |target_|
  // has changed. For these cases, redo the watches for |target_| and below.
  if (!Contains(recursive_paths_by_watch_, fired_watch) &&
      fired_watch != watches_.back().watch) {
    return UpdateRecursiveWatchesForPath(target_);
  }

  // Underneath |target_|, only directory changes trigger watch updates.
  if (!is_dir) {
    return true;
  }

  const base::FilePath& changed_dir =
      Contains(recursive_paths_by_watch_, fired_watch)
          ? recursive_paths_by_watch_[fired_watch]
          : target_;

  auto start_it = recursive_watches_by_path_.upper_bound(changed_dir);
  auto end_it = start_it;
  for (; end_it != recursive_watches_by_path_.end(); ++end_it) {
    const base::FilePath& cur_path = end_it->first;
    if (!changed_dir.IsParent(cur_path)) {
      break;
    }

    // There could be a race when another process is changing contents under
    // `changed_dir` while chrome is watching (e.g. an Android app updating
    // a dir with Chrome OS file manager open for the dir). In such case,
    // `cur_dir` under `changed_dir` could exist in this loop but not in
    // the FileEnumerator loop in the upcoming UpdateRecursiveWatchesForPath(),
    // As a result, `g_inotify_reader` would have an entry in its `watchers_`
    // pointing to `this` but `this` is no longer aware of that. Crash in
    // http://crbug/990004 could happen later.
    //
    // Remove the watcher of `cur_path` regardless of whether it exists
    // or not to keep `this` and `g_inotify_reader` consistent even when the
    // race happens. The watcher will be added back if `cur_path` exists in
    // the FileEnumerator loop in UpdateRecursiveWatchesForPath().
    g_inotify_reader.Get().RemoveWatch(end_it->second, this);

    // Keep it in sync with |recursive_watches_by_path_| crbug.com/995196.
    recursive_paths_by_watch_.erase(end_it->second);
  }
  recursive_watches_by_path_.erase(start_it, end_it);

  // If `changed_dir` does not exist anymore, then there is no need to call
  // UpdateRecursiveWatchesForPath().
  if (!DirectoryExists(changed_dir)) {
    return true;
  }

  return UpdateRecursiveWatchesForPath(changed_dir);
}

bool FilePathWatcherImpl::UpdateRecursiveWatchesForPath(
    const base::FilePath& path) {
  DUMP_WILL_BE_CHECK_EQ(type_, Type::kRecursive);
  DUMP_WILL_BE_CHECK(!path.empty());
  DUMP_WILL_BE_CHECK(DirectoryExists(path));

  // Note: SHOW_SYM_LINKS exposes symlinks as symlinks, so they are ignored
  // rather than followed. Following symlinks can easily lead to the undesirable
  // situation where the entire file system is being watched.
  base::FileEnumerator enumerator(
      path, true /* recursive enumeration */,
      base::FileEnumerator::DIRECTORIES | base::FileEnumerator::SHOW_SYM_LINKS);
  for (base::FilePath current = enumerator.Next(); !current.empty();
       current = enumerator.Next()) {
    DUMP_WILL_BE_CHECK(enumerator.GetInfo().IsDirectory());

    // Check `recursive_watches_by_path_` as a heuristic to determine if this
    // needs to be an add or update operation.
    if (!Contains(recursive_watches_by_path_, current)) {
      // Try to add new watches.
      InotifyReader::Watch watch =
          g_inotify_reader.Get().AddWatch(current, this);
      if (watch == InotifyReader::kWatchLimitExceeded) {
        return false;
      }

      // The `watch` returned by inotify already exists. This is actually an
      // update operation.
      auto it = recursive_paths_by_watch_.find(watch);
      if (it != recursive_paths_by_watch_.end()) {
        recursive_watches_by_path_.erase(it->second);
        recursive_paths_by_watch_.erase(it);
      }
      TrackWatchForRecursion(watch, current);
    } else {
      // Update existing watches.
      InotifyReader::Watch old_watch = recursive_watches_by_path_[current];
      DUMP_WILL_BE_CHECK_NE(InotifyReader::kInvalidWatch, old_watch);
      InotifyReader::Watch watch =
          g_inotify_reader.Get().AddWatch(current, this);
      if (watch == InotifyReader::kWatchLimitExceeded) {
        return false;
      }
      if (watch != old_watch) {
        g_inotify_reader.Get().RemoveWatch(old_watch, this);
        recursive_paths_by_watch_.erase(old_watch);
        recursive_watches_by_path_.erase(current);
        TrackWatchForRecursion(watch, current);
      }
    }
  }
  return true;
}

void FilePathWatcherImpl::TrackWatchForRecursion(InotifyReader::Watch watch,
                                                 const base::FilePath& path) {
  DUMP_WILL_BE_CHECK_EQ(type_, Type::kRecursive);
  DUMP_WILL_BE_CHECK(!path.empty());
  DUMP_WILL_BE_CHECK(target_.IsParent(path));

  if (watch == InotifyReader::kInvalidWatch) {
    return;
  }

  DUMP_WILL_BE_CHECK(!Contains(recursive_paths_by_watch_, watch));
  DUMP_WILL_BE_CHECK(!Contains(recursive_watches_by_path_, path));
  recursive_paths_by_watch_[watch] = path;
  recursive_watches_by_path_[path] = watch;

  UpdateInotifyCountHighWaterMark();
}

void FilePathWatcherImpl::RemoveRecursiveWatches() {
  if (type_ != Type::kRecursive) {
    return;
  }

  for (const auto& it : recursive_paths_by_watch_) {
    g_inotify_reader.Get().RemoveWatch(it.first, this);
  }

  recursive_paths_by_watch_.clear();
  recursive_watches_by_path_.clear();
}

bool FilePathWatcherImpl::AddWatchForBrokenSymlink(const base::FilePath& path,
                                                   WatchEntry* watch_entry) {
#if BUILDFLAG(IS_FUCHSIA)
  // Fuchsia does not support symbolic links.
  return false;
#else   // BUILDFLAG(IS_FUCHSIA)
  DUMP_WILL_BE_CHECK_EQ(InotifyReader::kInvalidWatch, watch_entry->watch);
  std::optional<base::FilePath> link = ReadSymbolicLinkAbsolute(path);
  if (!link) {
    return true;
  }
  DUMP_WILL_BE_CHECK(link->IsAbsolute());

  // Try watching symlink target directory. If the link target is "/", then we
  // shouldn't get here in normal situations and if we do, we'd watch "/" for
  // changes to a component "/" which is harmless so no special treatment of
  // this case is required.
  InotifyReader::Watch watch =
      g_inotify_reader.Get().AddWatch(link->DirName(), this);
  if (watch == InotifyReader::kWatchLimitExceeded) {
    return false;
  }
  if (watch == InotifyReader::kInvalidWatch) {
    // TODO(craig) Symlinks only work if the parent directory for the target
    // exist. Ideally we should make sure we've watched all the components of
    // the symlink path for changes. See crbug.com/91561 for details.
    DPLOG(WARNING) << "Watch failed for " << link->DirName().value();
    return true;
  }
  watch_entry->watch = watch;
  watch_entry->linkname = link->BaseName().value();
  return true;
#endif  // BUILDFLAG(IS_FUCHSIA)
}

bool FilePathWatcherImpl::HasValidWatchVector() const {
  if (watches_.empty()) {
    return false;
  }
  for (size_t i = 0; i < watches_.size() - 1; ++i) {
    if (watches_[i].subdir.empty()) {
      return false;
    }
  }
  return watches_.back().subdir.empty();
}

}  // namespace

size_t GetMaxNumberOfInotifyWatches() {
#if BUILDFLAG(IS_FUCHSIA)
  // Fuchsia has no limit on the number of watches.
  return std::numeric_limits<int>::max();
#else
  static const size_t max = []() {
    size_t max_number_of_inotify_watches = 0u;

    std::ifstream in(kInotifyMaxUserWatchesPath);
    if (!in.is_open() || !(in >> max_number_of_inotify_watches)) {
      LOG(ERROR) << "Failed to read " << kInotifyMaxUserWatchesPath;
      return kDefaultInotifyMaxUserWatches / kExpectedFilePathWatchers;
    }

    return max_number_of_inotify_watches / kExpectedFilePathWatchers;
  }();
  return g_override_max_inotify_watches ? g_override_max_inotify_watches : max;
#endif  // if BUILDFLAG(IS_FUCHSIA)
}

ScopedMaxNumberOfInotifyWatchesOverrideForTest::
    ScopedMaxNumberOfInotifyWatchesOverrideForTest(size_t override_max) {
  DUMP_WILL_BE_CHECK_EQ(g_override_max_inotify_watches, 0u);
  g_override_max_inotify_watches = override_max;
}

ScopedMaxNumberOfInotifyWatchesOverrideForTest::
    ~ScopedMaxNumberOfInotifyWatchesOverrideForTest() {
  g_override_max_inotify_watches = 0u;
}

FilePathWatcher::FilePathWatcher()
    : FilePathWatcher(std::make_unique<FilePathWatcherImpl>()) {}

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
// Put inside "BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)" because Android
// includes file_path_watcher_linux.cc.

// static
bool FilePathWatcher::HasWatchesForTest() {
  return g_inotify_reader.Get().HasWatches();
}
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

}  // namespace content
