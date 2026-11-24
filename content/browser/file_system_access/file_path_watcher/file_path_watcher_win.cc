// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/file_system_access/file_path_watcher/file_path_watcher.h"

#include <windows.h>

#include <winnt.h>

#include <cstdint>
#include <map>
#include <memory>
#include <tuple>
#include <utility>

#include "base/auto_reset.h"
#include "base/containers/heap_array.h"
#include "base/containers/span.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/platform_thread.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/types/expected.h"
#include "base/types/id_type.h"
#include "base/win/object_watcher.h"
#include "base/win/scoped_handle.h"
#include "base/win/windows_types.h"
#include "content/browser/file_system_access/features.h"
#include "content/browser/file_system_access/file_path_watcher/file_path_watcher_change_tracker.h"
#include "content/browser/file_system_access/file_path_watcher/file_path_watcher_histogram.h"

namespace content {
namespace {

enum class CreateFileHandleError {
  // When watching a path, the path (or some of its ancestor directories) might
  // not exist yet. Failure to create a watcher because the path doesn't exist
  // (or is not a directory) should not be considered fatal, since the watcher
  // implementation can simply try again one directory level above.
  kNonFatal,
  kFatal,
};

base::expected<base::win::ScopedHandle, CreateFileHandleError>
CreateDirectoryHandle(const base::FilePath& dir) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  base::win::ScopedHandle handle(::CreateFileW(
      dir.value().c_str(), FILE_LIST_DIRECTORY,
      FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
      FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, nullptr));

  if (handle.is_valid()) {
    base::File::Info file_info;
    if (!GetFileInfo(dir, &file_info)) {
      // Windows sometimes hands out handles to files that are about to go away.
      return base::unexpected(CreateFileHandleError::kNonFatal);
    }

    // Only return the handle if its a directory.
    if (!file_info.is_directory) {
      return base::unexpected(CreateFileHandleError::kNonFatal);
    }

    return handle;
  }

  switch (::GetLastError()) {
    case ERROR_FILE_NOT_FOUND:
    case ERROR_PATH_NOT_FOUND:
    case ERROR_ACCESS_DENIED:
    case ERROR_SHARING_VIOLATION:
    case ERROR_DIRECTORY:
      // Failure to create the handle is ok if the target directory doesn't
      // exist, access is denied (happens if the file is already gone but there
      // are still handles open), or the target is not a directory.
      return base::unexpected(CreateFileHandleError::kNonFatal);
    default:
      DPLOG(ERROR) << "CreateFileW failed for " << dir.value();
      return base::unexpected(CreateFileHandleError::kFatal);
  }
}

class FilePathWatcherImpl;

class CompletionIOPortThread final : public base::PlatformThread::Delegate {
 public:
  using WatcherEntryId = base::IdTypeU64<class WatcherEntryIdTag>;

  CompletionIOPortThread(const CompletionIOPortThread&) = delete;
  CompletionIOPortThread& operator=(const CompletionIOPortThread&) = delete;

  static CompletionIOPortThread* Get() {
    static base::NoDestructor<CompletionIOPortThread> io_thread;
    return io_thread.get();
  }

  // Thread safe.
  base::expected<CompletionIOPortThread::WatcherEntryId,
                 WatchWithChangeInfoResult>
  AddWatcher(FilePathWatcherImpl& watcher,
             base::win::ScopedHandle watched_handle,
             base::FilePath watched_path);

  // Thread safe.
  void RemoveWatcher(WatcherEntryId watcher_id);

  base::Lock& GetLockForTest();  // IN-TEST
  const void* GetOverlappedPointerForTest(  // IN-TEST
      WatcherEntryId watcher_id);
  void SetNextReadDirectoryChangesErrorForTest(  // IN-TEST
      WatcherEntryId watcher_id,
      DWORD error);

  // The max size of a file notification assuming that long paths aren't
  // enabled.
  static constexpr size_t kMaxFileNotifySize =
      sizeof(FILE_NOTIFY_INFORMATION) + MAX_PATH;

  // Choose a decent number of notifications to support that isn't too large.
  // Whatever we choose will be doubled by the kernel's copy of the buffer.
  static constexpr int kBufferNotificationCount = 20;
  static constexpr size_t kWatchBufferSizeBytes =
      kBufferNotificationCount * kMaxFileNotifySize;

  // Must be DWORD aligned.
  static_assert(kWatchBufferSizeBytes % sizeof(DWORD) == 0);
  // Must be less than the max network packet size for network drives. See
  // https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-readdirectorychangesw#remarks.
  static_assert(kWatchBufferSizeBytes <= 64 * 1024);

 private:
  friend base::NoDestructor<CompletionIOPortThread>;

  struct WatcherEntry {
    WatcherEntry(FilePathWatcherImpl* watcher_raw_ptr,
                 base::WeakPtr<FilePathWatcherImpl> watcher_weak_ptr,
                 scoped_refptr<base::SequencedTaskRunner> task_runner,
                 base::win::ScopedHandle watched_handle,
                 base::FilePath watched_path)
        : watcher_raw_ptr(watcher_raw_ptr),
          watcher_weak_ptr(std::move(watcher_weak_ptr)),
          task_runner(std::move(task_runner)),
          watched_handle(std::move(watched_handle)),
          watched_path(std::move(watched_path)) {}
    ~WatcherEntry() = default;

    // The kernel may hold pointers to `overlapped` and `buffer`.
    WatcherEntry(const WatcherEntry&) = delete;
    WatcherEntry& operator=(const WatcherEntry&) = delete;
    WatcherEntry(WatcherEntry&&) = delete;
    WatcherEntry& operator=(WatcherEntry&&) = delete;

    // Safe use of `raw_ptr` because it is only ever accessed in `ThreadMain`
    // after verifying that the watcher is still alive. Set to nullptr before
    // the underlying `FilePathWatcherImpl` is destroyed.
    raw_ptr<FilePathWatcherImpl> watcher_raw_ptr;
    base::WeakPtr<FilePathWatcherImpl> watcher_weak_ptr;
    scoped_refptr<base::SequencedTaskRunner> task_runner;

    base::win::ScopedHandle watched_handle;
    base::FilePath watched_path;

    OVERLAPPED overlapped = {};
    alignas(DWORD) uint8_t buffer[kWatchBufferSizeBytes];
    // True while the kernel may still access `overlapped` and `buffer`.
    bool has_pending_io = false;
    DWORD next_read_directory_changes_error_for_test = ERROR_SUCCESS;
  };

  CompletionIOPortThread();

  ~CompletionIOPortThread() override = default;

  void ThreadMain() override;

  [[nodiscard]] DWORD SetupWatch(WatcherEntry& watcher_entry);

  base::Lock watchers_lock_;

  WatcherEntryId::Generator watcher_id_generator_ GUARDED_BY(watchers_lock_);

  std::map<WatcherEntryId, WatcherEntry> watcher_entries_
      GUARDED_BY(watchers_lock_);

  // It is safe to access `io_completion_port_` on any thread without locks
  // since:
  //   - Windows Handles are thread safe
  //   - `io_completion_port_` is set once in the constructor of this class
  //   - This class is never destroyed.
  base::win::ScopedHandle io_completion_port_{
      ::CreateIoCompletionPort(INVALID_HANDLE_VALUE,
                               nullptr,
                               reinterpret_cast<ULONG_PTR>(nullptr),
                               1)};
};

class FilePathWatcherImpl : public FilePathWatcher::PlatformDelegate {
 public:
  FilePathWatcherImpl() = default;
  FilePathWatcherImpl(const FilePathWatcherImpl&) = delete;
  FilePathWatcherImpl& operator=(const FilePathWatcherImpl&) = delete;
  ~FilePathWatcherImpl() override;

  size_t current_usage() const override {
    return CompletionIOPortThread::kWatchBufferSizeBytes;
  }

  // FilePathWatcher::PlatformDelegate implementation:
  bool Watch(const base::FilePath& path,
             Type type,
             const FilePathWatcher::Callback& callback) override;

  // FilePathWatcher::PlatformDelegate implementation:
  bool WatchWithOptions(const base::FilePath& path,
                        const WatchOptions& flags,
                        const FilePathWatcher::Callback& callback) override;

  // FilePathWatcher::PlatformDelegate implementation:
  bool WatchWithChangeInfo(
      const base::FilePath& path,
      const WatchOptions& options,
      const FilePathWatcher::CallbackWithChangeInfo& callback,
      const FilePathWatcher::UsageChangeCallback& usage_callback) override;

  void Cancel() override;

  base::Lock& GetWatchThreadLockForTest() override;  // IN-TEST
  const void* GetOverlappedPointerForTest() override;  // IN-TEST
  void SetNextReadDirectoryChangesErrorForTest(        // IN-TEST
      uint32_t error) override;

 private:
  friend CompletionIOPortThread;

  // TODO(crbug.com/343961295): The delay in processing delete event is needed
  // in order to coalesce deleted and created events, in case of an overwrite.
  // Adjust the delay time based on testing.
  static constexpr base::TimeDelta kDeletedChangeDelay =
      base::Milliseconds(500);

  // Decrements the `upcoming_batch_count_` on destruction unless `Cancel` is
  // called.
  class UpcomingBatchCountDecrementer {
   public:
    explicit UpcomingBatchCountDecrementer(
        base::WeakPtr<FilePathWatcherImpl> file_path_watcher_weak_ptr)
        : file_path_watcher_weak_ptr_(std::move(file_path_watcher_weak_ptr)) {}

    ~UpcomingBatchCountDecrementer() {
      if (file_path_watcher_weak_ptr_ && !canceled_) {
        file_path_watcher_weak_ptr_->DecrementAndGetUpcomingBatchCount();
      }
    }

    void Cancel() { canceled_ = true; }

   private:
    base::WeakPtr<FilePathWatcherImpl> file_path_watcher_weak_ptr_;

    bool canceled_ = false;
  };

  // Sets up a watch handle for either `target_` or one of its ancestors.
  // Returns true on success.
  [[nodiscard]] WatchWithChangeInfoResult SetupWatchHandleForTarget();

  void CloseWatchHandle();

  void BufferOverflowed();

  void WatchError(DWORD error,
                  base::FilePath watched_path,
                  base::HeapArray<uint8_t> notification_batch);

  void ProcessNotificationBatch(base::FilePath watched_path,
                                base::HeapArray<uint8_t> notification_batch);

  base::FilePath& GetReportedPath(base::FilePath& modified_path);

  int DecrementAndGetUpcomingBatchCount();

  void RunCallbackOnPendingDelete();

  // Incremented by the `CompletionIOPortThread` to indicate if there is another
  // batch for this `FilePathWatcherImpl` queued to process. Decremented by this
  // `FilePathWatcherImpl` every time a batch is processed.
  std::atomic_int upcoming_batch_count_ = 0;

  // Callback to notify upon changes.
  FilePathWatcher::CallbackWithChangeInfo callback_;

  // Path we're supposed to watch (passed to callback).
  base::FilePath target_;

  std::optional<CompletionIOPortThread::WatcherEntryId> watcher_id_;

  // True if should report the modified path rather than the watched path.
  bool report_modified_path_ = false;

  std::optional<FilePathWatcherChangeTracker> change_tracker_;

  // Timer to delay processing a pending delete change.
  base::OneShotTimer pending_delete_timer_;

  base::WeakPtrFactory<FilePathWatcherImpl> weak_factory_{this};
};

CompletionIOPortThread::CompletionIOPortThread() {
  base::PlatformThread::CreateNonJoinable(0, this);
}

DWORD CompletionIOPortThread::SetupWatch(WatcherEntry& watcher_entry) {
  CHECK(!watcher_entry.has_pending_io);
  if (watcher_entry.next_read_directory_changes_error_for_test !=
      ERROR_SUCCESS) {
    return std::exchange(
        watcher_entry.next_read_directory_changes_error_for_test,
        ERROR_SUCCESS);
  }

  bool success = ReadDirectoryChangesW(
      watcher_entry.watched_handle.get(), &watcher_entry.buffer,
      kWatchBufferSizeBytes, /*bWatchSubtree=*/true,
      FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_SIZE |
          FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_DIR_NAME,
      nullptr, &watcher_entry.overlapped, nullptr);
  if (!success) {
    return ::GetLastError();
  }
  watcher_entry.has_pending_io = true;
  return ERROR_SUCCESS;
}

base::expected<CompletionIOPortThread::WatcherEntryId,
               WatchWithChangeInfoResult>
CompletionIOPortThread::AddWatcher(FilePathWatcherImpl& watcher,
                                   base::win::ScopedHandle watched_handle,
                                   base::FilePath watched_path) {
  base::AutoLock auto_lock(watchers_lock_);

  WatcherEntryId watcher_id = watcher_id_generator_.GenerateNextId();
  HANDLE port = ::CreateIoCompletionPort(
      watched_handle.get(), io_completion_port_.get(),
      static_cast<ULONG_PTR>(watcher_id.GetUnsafeValue()), 1);
  if (port == nullptr) {
    return base::unexpected(
        WatchWithChangeInfoResult::kWinCreateIoCompletionPortError);
  }

  auto [it, inserted] = watcher_entries_.emplace(
      std::piecewise_construct, std::forward_as_tuple(watcher_id),
      std::forward_as_tuple(&watcher, watcher.weak_factory_.GetWeakPtr(),
                            watcher.task_runner(), std::move(watched_handle),
                            std::move(watched_path)));

  CHECK(inserted);

  DWORD result = SetupWatch(it->second);

  if (result != ERROR_SUCCESS) {
    watcher_entries_.erase(it);
    return base::unexpected(
        WatchWithChangeInfoResult::kWinReadDirectoryChangesWError);
  }

  return watcher_id;
}

void CompletionIOPortThread::RemoveWatcher(WatcherEntryId watcher_id) {
  HANDLE raw_watched_handle;
  {
    base::AutoLock auto_lock(watchers_lock_);

    auto it = watcher_entries_.find(watcher_id);
    CHECK(it != watcher_entries_.end());

    auto& watched_handle = it->second.watched_handle;
    CHECK(watched_handle.is_valid());
    raw_watched_handle = watched_handle.release();

    it->second.watcher_raw_ptr = nullptr;

    // An unsuccessful submission does not post a completion. Keep the entry
    // only while a completion packet is outstanding.
    if (!it->second.has_pending_io) {
      watcher_entries_.erase(it);
    }
  }

  {
    base::ScopedBlockingCall scoped_blocking_call(
        FROM_HERE, base::BlockingType::MAY_BLOCK);

    // Closing the handle cancels any pending directory read. Its completion
    // lets ThreadMain release the entry.
    ::CloseHandle(raw_watched_handle);
  }
}

base::Lock& CompletionIOPortThread::GetLockForTest() {
  return watchers_lock_;
}

const void* CompletionIOPortThread::GetOverlappedPointerForTest(
    WatcherEntryId watcher_id) {
  base::AutoLock auto_lock(watchers_lock_);
  auto it = watcher_entries_.find(watcher_id);
  CHECK(it != watcher_entries_.end());
  return &it->second.overlapped;
}

void CompletionIOPortThread::SetNextReadDirectoryChangesErrorForTest(
    WatcherEntryId watcher_id,
    DWORD error) {
  base::AutoLock auto_lock(watchers_lock_);
  auto it = watcher_entries_.find(watcher_id);
  CHECK(it != watcher_entries_.end());
  CHECK_NE(error, static_cast<DWORD>(ERROR_SUCCESS));
  CHECK_EQ(it->second.next_read_directory_changes_error_for_test,
           static_cast<DWORD>(ERROR_SUCCESS));
  it->second.next_read_directory_changes_error_for_test = error;
}

void CompletionIOPortThread::ThreadMain() {
  while (true) {
    DWORD bytes_transferred;
    ULONG_PTR key = reinterpret_cast<ULONG_PTR>(nullptr);
    OVERLAPPED* overlapped_out = nullptr;

    BOOL io_port_result = ::GetQueuedCompletionStatus(
        io_completion_port_.get(), &bytes_transferred, &key, &overlapped_out,
        INFINITE);
    const DWORD io_port_error =
        io_port_result ? ERROR_SUCCESS : ::GetLastError();

    // The port stays open and the wait is infinite. Even failed I/O must
    // return a completion packet with an OVERLAPPED pointer.
    CHECK(overlapped_out);

    base::AutoLock auto_lock(watchers_lock_);

    WatcherEntryId watcher_id = WatcherEntryId::FromUnsafeValue(key);

    auto watcher_entry_it = watcher_entries_.find(watcher_id);

    CHECK(watcher_entry_it != watcher_entries_.end())
        << "WatcherEntryId not in map";

    auto& watcher_entry = watcher_entry_it->second;
    CHECK_EQ(&watcher_entry.overlapped, overlapped_out);
    watcher_entry.has_pending_io = false;

    if (!watcher_entry.watched_handle.is_valid()) {
      // The last I/O has completed, so the entry's storage can be released.
      watcher_entries_.erase(watcher_entry_it);
      continue;
    }

    // If watched_handle hasn't been released yet, then the `watcher` is
    // still alive, and it is safe to access via raw pointer.
    watcher_entry.watcher_raw_ptr->upcoming_batch_count_++;

    if (!io_port_result) {
      watcher_entry.task_runner->PostTask(
          FROM_HERE, base::BindOnce(&FilePathWatcherImpl::WatchError,
                                    watcher_entry.watcher_weak_ptr,
                                    io_port_error, watcher_entry.watched_path,
                                    base::HeapArray<uint8_t>()));
      continue;
    }

    base::HeapArray<uint8_t> notification_batch;
    if (bytes_transferred > 0) {
      notification_batch = base::HeapArray<uint8_t>::CopiedFrom(
          base::span(watcher_entry.buffer).first(bytes_transferred));
    }

    // Let the kernel know that we're ready to receive change events again in
    // the `watcher_entry`'s `buffer`.
    //
    // We do this as soon as possible, so that not too many events are received
    // in the next batch. Too many events can cause a buffer overflow.
    DWORD result = SetupWatch(watcher_entry);

    // `SetupWatch` can fail if the watched directory was deleted before
    // `SetupWatch` was called but after `GetQueuedCompletionStatus` returned.
    if (result != ERROR_SUCCESS) {
      watcher_entry.task_runner->PostTask(
          FROM_HERE, base::BindOnce(&FilePathWatcherImpl::WatchError,
                                    watcher_entry.watcher_weak_ptr, result,
                                    watcher_entry.watched_path,
                                    std::move(notification_batch)));
      continue;
    }

    // `GetQueuedCompletionStatus` succeeds with zero bytes transferred if there
    // is a buffer overflow.
    if (bytes_transferred == 0) {
      watcher_entry.task_runner->PostTask(
          FROM_HERE, base::BindOnce(&FilePathWatcherImpl::BufferOverflowed,
                                    watcher_entry.watcher_weak_ptr));
      continue;
    }

    watcher_entry.task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(&FilePathWatcherImpl::ProcessNotificationBatch,
                       watcher_entry.watcher_weak_ptr,
                       watcher_entry.watched_path,
                       std::move(notification_batch)));
  }
}

FilePathWatcherImpl::~FilePathWatcherImpl() {
  CHECK(!task_runner() || task_runner()->RunsTasksInCurrentSequence(),
        base::NotFatalUntil::M159);
}

bool FilePathWatcherImpl::Watch(const base::FilePath& path,
                                Type type,
                                const FilePathWatcher::Callback& callback) {
  return WatchWithChangeInfo(
      path, WatchOptions{.type = type},
      base::IgnoreArgs<const FilePathWatcher::ChangeInfo&>(
          base::BindRepeating(std::move(callback))),
      base::DoNothingAs<void(size_t, size_t)>());
}

bool FilePathWatcherImpl::WatchWithOptions(
    const base::FilePath& path,
    const WatchOptions& options,
    const FilePathWatcher::Callback& callback) {
  return WatchWithChangeInfo(
      path, options,
      base::IgnoreArgs<const FilePathWatcher::ChangeInfo&>(
          base::BindRepeating(std::move(callback))),
      base::DoNothingAs<void(size_t, size_t)>());
}

bool FilePathWatcherImpl::WatchWithChangeInfo(
    const base::FilePath& path,
    const WatchOptions& options,
    const FilePathWatcher::CallbackWithChangeInfo& callback,
    const FilePathWatcher::UsageChangeCallback& usage_callback) {
  CHECK(target_.empty(),
        base::NotFatalUntil::M159);  // Can only watch one path.
  CHECK(!callback.is_null());

  set_task_runner(base::SequencedTaskRunner::GetCurrentDefault());
  callback_ = callback;
  target_ = path;
  report_modified_path_ = options.report_modified_path;

  change_tracker_ = FilePathWatcherChangeTracker(target_, options.type);

  WatchWithChangeInfoResult result = SetupWatchHandleForTarget();

  RecordWatchWithChangeInfoResultUma(result);

  return result == WatchWithChangeInfoResult::kSuccess;
}

void FilePathWatcherImpl::Cancel() {
  set_cancelled();

  if (callback_.is_null()) {
    // Watch was never called, or the `task_runner_` has already quit.
    return;
  }

  CHECK(task_runner()->RunsTasksInCurrentSequence(), base::NotFatalUntil::M159);

  CloseWatchHandle();

  callback_.Reset();
}

base::Lock& FilePathWatcherImpl::GetWatchThreadLockForTest() {
  return CompletionIOPortThread::Get()->GetLockForTest();  // IN-TEST
}

const void* FilePathWatcherImpl::GetOverlappedPointerForTest() {
  CHECK(watcher_id_.has_value());
  return CompletionIOPortThread::Get()->GetOverlappedPointerForTest(  // IN-TEST
      watcher_id_.value());
}

void FilePathWatcherImpl::SetNextReadDirectoryChangesErrorForTest(
    uint32_t error) {
  CHECK(watcher_id_.has_value());
  CompletionIOPortThread::Get()
      ->SetNextReadDirectoryChangesErrorForTest(  // IN-TEST
          watcher_id_.value(), error);
}

void FilePathWatcherImpl::BufferOverflowed() {
  CHECK(task_runner()->RunsTasksInCurrentSequence(), base::NotFatalUntil::M159);

  DecrementAndGetUpcomingBatchCount();

  // Do not send a pendeing delete, which may not have been coalesced.
  // Consider it as lost in the buffer overflowed batch.
  pending_delete_timer_.Stop();

  change_tracker_->MayHaveMissedChanges();

  // `this` may be deleted after `callback_` is run.
  callback_.Run(FilePathWatcher::ChangeInfo(), target_, /*error=*/false);
}

void FilePathWatcherImpl::WatchError(
    DWORD error,
    base::FilePath watched_path,
    base::HeapArray<uint8_t> notification_batch) {
  CHECK(task_runner()->RunsTasksInCurrentSequence(), base::NotFatalUntil::M159);

  UpcomingBatchCountDecrementer upcoming_batch_count_decrementer(
      weak_factory_.GetWeakPtr());
  RecordReadDirectoryChangesErrorUma(static_cast<int>(error));

  bool target_missing = false;
  bool recovered = false;
  if (error == ERROR_ACCESS_DENIED &&
      SetupWatchHandleForTarget() == WatchWithChangeInfoResult::kSuccess) {
    // Access denied alone does not establish that the target disappeared.
    base::ScopedBlockingCall scoped_blocking_call(
        FROM_HERE, base::BlockingType::MAY_BLOCK);
    if (::GetFileAttributesW(target_.value().c_str()) !=
        INVALID_FILE_ATTRIBUTES) {
      recovered = true;
    } else {
      const DWORD path_error = ::GetLastError();
      target_missing = path_error == ERROR_FILE_NOT_FOUND ||
                       path_error == ERROR_PATH_NOT_FOUND;
      recovered = target_missing;
    }
  }

  if (!recovered) {
    pending_delete_timer_.Stop();
    CloseWatchHandle();
    weak_factory_.InvalidateWeakPtrs();
    auto callback = std::move(callback_);
    callback.Run(FilePathWatcher::ChangeInfo(), target_, /*error=*/true);
    return;
  }

  auto self = weak_factory_.GetWeakPtr();
  const bool watched_target = watched_path == target_;
  if (!notification_batch.empty()) {
    // `ProcessNotificationBatch` will decrement `upcoming_batch_count`.
    upcoming_batch_count_decrementer.Cancel();

    // `ProcessNotificationBatch` may delete `this`.
    ProcessNotificationBatch(std::move(watched_path),
                             std::move(notification_batch));

    if (!self) {
      return;
    }
  }

  // Deliver the pending change before MayHaveMissedChanges clears it.
  if (change_tracker_->HasPendingDelete()) {
    pending_delete_timer_.Stop();
    RunCallbackOnPendingDelete();
    if (!self) {
      return;
    }
  }

  if (target_missing &&
      (watched_target || change_tracker_->KnowTargetExists())) {
    // `this` may be deleted after `callback_` is run.
    callback_.Run(FilePathWatcher::ChangeInfo(
                      FilePathWatcher::FilePathType::kDirectory,
                      FilePathWatcher::ChangeType::kDeleted, target_),
                  target_, /*error=*/false);

    if (!self) {
      return;
    }
  }

  change_tracker_->MayHaveMissedChanges();
}

void FilePathWatcherImpl::ProcessNotificationBatch(
    base::FilePath watched_path,
    base::HeapArray<uint8_t> notification_batch) {
  CHECK(task_runner()->RunsTasksInCurrentSequence(), base::NotFatalUntil::M159);
  CHECK(!notification_batch.empty());

  // When a new batch arrives, cancel the timer to send a pending delete since
  // it may be coalesced if any matching event is found from the new batch in
  // the change tracker.
  pending_delete_timer_.Stop();

  auto self = weak_factory_.GetWeakPtr();

  auto sub_span = notification_batch.as_span();
  bool has_next_entry = true;

  while (has_next_entry) {
    const auto& file_notify_info =
        *reinterpret_cast<FILE_NOTIFY_INFORMATION*>(sub_span.data());

    has_next_entry = file_notify_info.NextEntryOffset != 0;
    if (has_next_entry) {
      sub_span = sub_span.subspan(file_notify_info.NextEntryOffset);
    }

    base::FilePath change_path =
        watched_path.Append(std::basic_string_view<wchar_t>(
            file_notify_info.FileName,
            file_notify_info.FileNameLength / sizeof(wchar_t)));

    change_tracker_->AddChange(std::move(change_path), file_notify_info.Action);
  }

  bool next_change_soon = DecrementAndGetUpcomingBatchCount() > 0;

  // Check for any pending delete, and start a timer to be sent later,
  // in order to give it a chance to be coalesced.
  if (!next_change_soon && change_tracker_->HasPendingDelete()) {
    pending_delete_timer_.Start(
        FROM_HERE, kDeletedChangeDelay,
        base::BindOnce(&FilePathWatcherImpl::RunCallbackOnPendingDelete,
                       weak_factory_.GetWeakPtr()));
  }

  for (auto& change : change_tracker_->PopChanges(next_change_soon)) {
    // `this` may be deleted after `callback_` is run.
    callback_.Run(std::move(change), GetReportedPath(change.modified_path),
                  /*error=*/false);
    if (!self) {
      return;
    }
  }
}

WatchWithChangeInfoResult FilePathWatcherImpl::SetupWatchHandleForTarget() {
  CloseWatchHandle();

  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  // Start at the target and walk up the directory chain until we successfully
  // create a file handle in `watched_handle_`. `child_dirs` keeps a stack of
  // child directories stripped from target, in reverse order.
  std::vector<base::FilePath> child_dirs;
  base::FilePath path_to_watch(target_);

  base::win::ScopedHandle watched_handle;
  base::FilePath watched_path;
  while (true) {
    auto result = CreateDirectoryHandle(path_to_watch);

    // Break if a valid handle is returned.
    if (result.has_value()) {
      watched_handle = std::move(result.value());
      watched_path = path_to_watch;
      break;
    }

    // We're in an unknown state if `CreateDirectoryHandle` returns an `kFatal`
    // error, so return failure.
    if (result.error() == CreateFileHandleError::kFatal) {
      return WatchWithChangeInfoResult::kWinCreateFileHandleErrorFatal;
    }

    // Abort if we hit the root directory.
    child_dirs.push_back(path_to_watch.BaseName());
    base::FilePath parent(path_to_watch.DirName());
    if (parent == path_to_watch) {
      DLOG(ERROR) << "Reached the root directory";
      return WatchWithChangeInfoResult::kWinReachedRootDirectory;
    }
    path_to_watch = std::move(parent);
  }

  // At this point, `watched_handle` is valid. However, the bottom-up search
  // that the above code performs races against directory creation. So try to
  // walk back down and see whether any children appeared in the mean time.
  while (!child_dirs.empty()) {
    path_to_watch = path_to_watch.Append(child_dirs.back());
    child_dirs.pop_back();
    auto result = CreateDirectoryHandle(path_to_watch);
    if (!result.has_value()) {
      // We're in an unknown state if `CreateDirectoryHandle` returns an
      // `kFatal` error, so return failure.
      if (result.error() == CreateFileHandleError::kFatal) {
        return WatchWithChangeInfoResult::kWinCreateFileHandleErrorFatal;
      }
      // Otherwise go with the current `watched_handle`.
      break;
    }
    watched_handle = std::move(result.value());
    watched_path = path_to_watch;
  }

  auto watcher_id_or_error = CompletionIOPortThread::Get()->AddWatcher(
      *this, std::move(watched_handle), std::move(watched_path));

  if (watcher_id_or_error.has_value()) {
    watcher_id_ = watcher_id_or_error.value();

    return WatchWithChangeInfoResult::kSuccess;
  } else {
    watcher_id_ = std::nullopt;
    return watcher_id_or_error.error();
  }
}

void FilePathWatcherImpl::CloseWatchHandle() {
  if (watcher_id_.has_value()) {
    CompletionIOPortThread::Get()->RemoveWatcher(watcher_id_.value());
    watcher_id_.reset();
  }
}

base::FilePath& FilePathWatcherImpl::GetReportedPath(
    base::FilePath& modified_path) {
  return report_modified_path_ ? modified_path : target_;
}

int FilePathWatcherImpl::DecrementAndGetUpcomingBatchCount() {
  int upcoming_batch_count = --upcoming_batch_count_;
  CHECK(upcoming_batch_count >= 0);
  return upcoming_batch_count;
}

void FilePathWatcherImpl::RunCallbackOnPendingDelete() {
  CHECK(task_runner()->RunsTasksInCurrentSequence(), base::NotFatalUntil::M159);

  if (callback_.is_null()) {
    return;
  }

  std::optional<FilePathWatcher::ChangeInfo> opt_pending_delete =
      change_tracker_->TakePendingDelete();
  CHECK(opt_pending_delete.has_value());
  FilePathWatcher::ChangeInfo change = *std::move(opt_pending_delete);
  // `this` may be deleted after `callback_` is run.
  callback_.Run(std::move(change), GetReportedPath(change.modified_path),
                /*error=*/false);
}

}  // namespace

FilePathWatcher::FilePathWatcher()
    : FilePathWatcher(std::make_unique<FilePathWatcherImpl>()) {}

// static
size_t FilePathWatcher::GetQuotaLimitImpl() {
  return features::kFileSystemObserverQuotaLimitWindows.Get();
}

}  // namespace content
