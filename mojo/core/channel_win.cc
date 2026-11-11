// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/core/channel.h"

#include <windows.h>

#include <stdint.h>

#include <algorithm>
#include <limits>
#include <memory>
#include <optional>
#include <tuple>
#include <utility>

#include "base/compiler_specific.h"
#include "base/containers/queue.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_pump_for_io.h"
#include "base/metrics/histogram_macros.h"
#include "base/process/current_process.h"
#include "base/process/process_handle.h"
#include "base/rand_util.h"
#include "base/synchronization/lock.h"
#include "base/task/current_thread.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_runner.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "base/win/scoped_handle.h"
#include "base/win/win_util.h"

namespace mojo {
namespace core {

namespace {

constexpr int kMaxOpenChannelSample = 101;

struct OpenChannelMetrics {
  size_t open_channels = 0;
  int recorded_peak = 0;
};

constinit thread_local OpenChannelMetrics g_open_channel_metrics;

class ChannelWinMessageQueue {
 public:
  ChannelWinMessageQueue() = default;
  ~ChannelWinMessageQueue() = default;

  void Append(Channel::MessagePtr message, base::TimeTicks creation_time) {
    queue_.emplace_back(std::move(message), creation_time);
  }

  Channel::Message* GetFirst() const { return queue_.front().message.get(); }

  Channel::MessagePtr TakeFirst() {
    QueuedMessage entry = std::move(queue_.front());
    queue_.pop_front();
    if (!entry.creation_time.is_null()) {
      base::TimeDelta latency = base::TimeTicks::Now() - entry.creation_time;
      UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES("Mojo.Channel.WriteLatencyUs",
                                              latency, base::Microseconds(1),
                                              base::Seconds(1), 100);
    }
    return std::move(entry.message);
  }

  bool IsEmpty() const { return queue_.empty(); }

 private:
  struct QueuedMessage {
    Channel::MessagePtr message;
    base::TimeTicks creation_time;
  };

  base::circular_deque<QueuedMessage> queue_;
};

class ChannelWin : public Channel,
                   public base::CurrentThread::DestructionObserver,
                   public base::MessagePumpForIO::IOHandler,
                   public base::MessagePumpForIO::IOEventHandler {
 public:
  ChannelWin(Delegate* delegate,
             ConnectionParams connection_params,
             HandlePolicy handle_policy,
             scoped_refptr<base::SingleThreadTaskRunner> io_task_runner)
      : Channel(delegate, handle_policy),
        base::MessagePumpForIO::IOHandler(FROM_HERE),
        base::MessagePumpForIO::IOEventHandler(FROM_HERE),
        is_untrusted_process_(connection_params.is_untrusted_process()),
        self_(this),
        io_task_runner_(io_task_runner) {
    handle_ =
        connection_params.TakeEndpoint().TakePlatformHandle().TakeHandle();
    CHECK(handle_.is_valid());
  }

  ChannelWin(const ChannelWin&) = delete;
  ChannelWin& operator=(const ChannelWin&) = delete;

  void Start() override {
    io_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&ChannelWin::StartOnIOThread, this));
  }

  void ShutDownImpl() override {
    // Always shut down asynchronously when called through the public interface.
    io_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&ChannelWin::ShutDownOnIOThread, this));
  }

  void Write(MessagePtr message) override {
    RecordSentMessageMetricsSubsampled(message->data_num_bytes());

    base::TimeTicks creation_time;
    if (base::ShouldRecordSubsampledMetric(
            Channel::kMetricSubsamplingProbability)) {
      creation_time = base::TimeTicks::Now();
    }

    if (remote_process().IsValid()) {
      // If we know the remote process handle, we transfer all outgoing handles
      // to the process now rewriting them in the message.
      std::vector<PlatformHandleInTransit> handles = message->TakeHandles();
      for (auto& handle : handles) {
        if (handle.handle().is_valid()) {
          handle.TransferToProcess(
              remote_process().Duplicate(),
              is_untrusted_process_ ? PlatformHandleInTransit::kUntrustedTarget
                                    : PlatformHandleInTransit::kTrustedTarget);
        }
      }
      message->SetHandles(std::move(handles));
    }

    bool write_error = false;
    {
      base::AutoLock lock(write_lock_);
      if (reject_writes_) {
        return;
      }

      bool write_now = !delay_writes_ && outgoing_messages_.IsEmpty();
      outgoing_messages_.Append(std::move(message), creation_time);
      if (write_now && !WriteNoLock(outgoing_messages_.GetFirst())) {
        reject_writes_ = write_error = true;
      }
    }
    if (write_error) {
      // Do not synchronously invoke OnWriteError(). Write() may have been
      // called by the delegate and we don't want to re-enter it.
      io_task_runner_->PostTask(FROM_HERE,
                                base::BindOnce(&ChannelWin::OnWriteError, this,
                                               Error::kDisconnected));
    }
  }

  void LeakHandle() override {
    DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
    leak_handle_ = true;
  }

  bool GetReadPlatformHandles(const void* payload,
                              size_t payload_size,
                              size_t num_handles,
                              const void* extra_header,
                              size_t extra_header_size,
                              std::vector<PlatformHandle>* handles) override {
    DCHECK(extra_header);
    if (num_handles > std::numeric_limits<uint16_t>::max()) {
      return false;
    }
    using HandleEntry = Channel::Message::HandleEntry;
    size_t handles_size = sizeof(HandleEntry) * num_handles;
    if (handles_size > extra_header_size) {
      return false;
    }
    handles->reserve(num_handles);
    const HandleEntry* extra_header_handles =
        reinterpret_cast<const HandleEntry*>(extra_header);
    for (size_t i = 0; i < num_handles; i++) {
      HANDLE handle_value = base::win::Uint32ToHandle(
          UNSAFE_TODO(extra_header_handles[i]).handle);
      if (PlatformHandleInTransit::IsPseudoHandle(handle_value)) {
        return false;
      }
      if (remote_process().IsValid() && handle_value != INVALID_HANDLE_VALUE) {
        // If we know the remote process's handle, we assume it doesn't know
        // ours; that means any handle values still belong to that process, and
        // we need to transfer them to this process.
        handle_value = PlatformHandleInTransit::TakeIncomingRemoteHandle(
                           handle_value, remote_process().Handle())
                           .ReleaseHandle();
      }
      handles->emplace_back(base::win::ScopedHandle(std::move(handle_value)));
    }
    return true;
  }

  bool GetReadPlatformHandlesForIpcz(
      size_t num_handles,
      std::vector<PlatformHandle>& handles) override {
    // Always a validation failure if we're asked for handles on Windows,
    // because ChannelWin for ipcz never sends handles out-of-band from data.
    return false;
  }

 private:
  struct EventIOState {
    EventIOState(base::win::ScopedHandle read_event,
                 base::win::ScopedHandle write_event)
        : read_watch(std::move(read_event)),
          write_watch(std::move(write_event)) {}

    base::MessagePumpForIO::IOEventWatchController read_watch;
    base::MessagePumpForIO::IOEventWatchController write_watch;
  };

  // May run on any thread.
  ~ChannelWin() override = default;

  void StartOnIOThread() {
    DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
    base::CurrentThread::Get()->AddDestructionObserver(this);
    auto io_thread = base::CurrentIOThread::Get();
    if (io_thread->SupportsIOEvents()) {
      // TODO(crbug.com/424506430): Supply brokered events for Basic Processes
      // instead of creating them locally.
      base::win::ScopedHandle read_event(
          ::CreateEventW(nullptr, FALSE, FALSE, nullptr));
      base::win::ScopedHandle write_event(
          ::CreateEventW(nullptr, FALSE, FALSE, nullptr));
      if (!read_event.is_valid() || !write_event.is_valid()) {
        OnError(Error::kConnectionFailed);
        return;
      }
      event_io_ = std::make_unique<EventIOState>(std::move(read_event),
                                                 std::move(write_event));
      if (!io_thread->WatchIOEvent(&event_io_->read_watch, this) ||
          !io_thread->WatchIOEvent(&event_io_->write_watch, this)) {
        event_io_.reset();
        OnError(Error::kConnectionFailed);
        return;
      }
      read_context_.GetOverlapped()->hEvent =
          event_io_->read_watch.event_handle();
    } else {
      if (!io_thread->RegisterIOHandler(handle_.get(), this)) {
        OnError(Error::kConnectionFailed);
        return;
      }
    }

    counted_for_metrics_ = IsRendererProcessForMetrics();
    if (counted_for_metrics_) {
      ++g_open_channel_metrics.open_channels;
      if (g_open_channel_metrics.recorded_peak < kMaxOpenChannelSample &&
          g_open_channel_metrics.open_channels >
              static_cast<size_t>(g_open_channel_metrics.recorded_peak)) {
        ++g_open_channel_metrics.recorded_peak;
        UMA_HISTOGRAM_EXACT_LINEAR(
            "Stability.Mojo.Channel.Win.PeakOpenChannels.Renderer",
            g_open_channel_metrics.recorded_peak, kMaxOpenChannelSample);
      }
    }

    // Keep this alive in case we synchronously run shutdown, via OnError(),
    // as a result of a ReadFile() failure on the channel.
    scoped_refptr<ChannelWin> keep_alive(this);

    // Completion dispatch is now ready, so queued writes can be issued.
    {
      base::AutoLock lock(write_lock_);
      if (event_io_) {
        write_context_.GetOverlapped()->hEvent =
            event_io_->write_watch.event_handle();
      }
      if (delay_writes_) {
        delay_writes_ = false;
        if (!WriteNextNoLock()) {
          reject_writes_ = true;
        }
      }
    }

    ReadMore(0);
  }

  void ShutDownOnIOThread() {
    DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
    is_shutting_down_ = true;
    base::CurrentThread::Get()->RemoveDestructionObserver(this);

    {
      // Prevent attempts to write if we've closed the handle.
      base::AutoLock lock(write_lock_);
      reject_writes_ = true;
    }

    // TODO(crbug.com/40455076): This function is expected to be called
    // once, and |handle_| should be valid at this point.
    CHECK(handle_.is_valid());
    // Write() may issue an overlapped WriteFile() from any thread, so pending
    // writes are not necessarily owned by the IO thread. CancelIo() only
    // cancels I/O issued by the calling thread; use CancelIoEx() with a null
    // OVERLAPPED to cancel all pending I/O on the handle regardless of which
    // thread issued it.
    CancelIoEx(handle_.get(), nullptr);
    if (leak_handle_) {
      std::ignore = handle_.release();
    } else {
      handle_.Close();
    }
    MaybeFinishEventShutdown();

    if (counted_for_metrics_) {
      DCHECK_GT(g_open_channel_metrics.open_channels, 0u);
      --g_open_channel_metrics.open_channels;
    }

    // Allow |this| to be destroyed as soon as no IO is pending.
    self_ = nullptr;
  }

  // base::CurrentThread::DestructionObserver:
  void WillDestroyCurrentMessageLoop() override {
    DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
    if (self_) {
      ShutDownOnIOThread();
    }
  }

  void MaybeFinishEventShutdown() {
    DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
    CHECK(is_shutting_down_);
    if (!event_io_ || pending_read_) {
      return;
    }
    {
      base::AutoLock lock(write_lock_);
      if (pending_write_) {
        return;
      }
      write_context_.GetOverlapped()->hEvent = nullptr;
    }
    read_context_.GetOverlapped()->hEvent = nullptr;
    event_io_.reset();
  }

  // base::MessagePumpForIO::IOEventHandler:
  void OnIOEventSignaled(
      base::MessagePumpForIO::IOEventWatchController* controller) override {
    DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
    CHECK(event_io_);

    if (is_shutting_down_) {
      // The pipe is closed or leaked. Retire only completed operations,
      // keeping this alive through watch cleanup and write-lock release.
      CHECK(!handle_.is_valid());
      scoped_refptr<ChannelWin> keep_alive;
      if (controller == &event_io_->read_watch) {
        if (!pending_read_ ||
            !HasOverlappedIoCompleted(read_context_.GetOverlapped())) {
          return;
        }
        keep_alive = std::move(pending_read_);
      } else {
        CHECK_EQ(controller, &event_io_->write_watch);
        base::AutoLock lock(write_lock_);
        if (!pending_write_ ||
            !HasOverlappedIoCompleted(write_context_.GetOverlapped())) {
          return;
        }
        keep_alive = std::move(pending_write_);
      }
      MaybeFinishEventShutdown();
      return;
    }

    if (controller == &event_io_->read_watch) {
      OnReadDone();
    } else {
      CHECK_EQ(controller, &event_io_->write_watch);
      // Synchronize with WriteFile(), which may still be returning on another
      // thread after the completion event has been signaled.
      OnWriteDone();
    }
  }

  // base::MessageLoop::IOHandler:
  void OnIOCompleted(base::MessagePumpForIO::IOContext* context,
                     DWORD /*bytes_transfered*/,
                     DWORD /*error*/) override {
    // I/O completion port association belongs to the underlying file object
    // rather than to `handle_`, so a packet delivered here does not necessarily
    // correspond to an operation started by this Channel. OnReadDone() and
    // OnWriteDone() act only on an outstanding operation of ours which the
    // kernel has completed, and take its result from our own OVERLAPPED rather
    // than from the packet.
    if (context == &read_context_) {
      OnReadDone();
    } else if (context == &write_context_) {
      OnWriteDone();
    }
  }

  // Returns the number of bytes transferred by the operation most recently
  // started with `context`, which the kernel must have completed, or nullopt if
  // that operation failed.
  std::optional<size_t> GetCompletedBytes(
      base::MessagePumpForIO::IOContext& context) {
    if (!handle_.is_valid()) {
      // ShutDownOnIOThread() has cancelled every outstanding operation.
      return std::nullopt;
    }
    DWORD bytes_transferred = 0;
    if (!::GetOverlappedResult(handle_.get(), context.GetOverlapped(),
                               &bytes_transferred, /*bWait=*/FALSE)) {
      return std::nullopt;
    }
    return bytes_transferred;
  }

  void OnReadDone() {
    if (!pending_read_ ||
        !HasOverlappedIoCompleted(read_context_.GetOverlapped())) {
      return;
    }
    scoped_refptr<ChannelWin> keep_alive = std::move(pending_read_);

    // A failed read is handled like the end of the stream.
    const size_t bytes_read = GetCompletedBytes(read_context_).value_or(0);
    if (bytes_read > 0) {
      size_t next_read_size = 0;
      if (OnReadComplete(bytes_read, &next_read_size)) {
        ReadMore(next_read_size);
      } else {
        OnError(Error::kReceivedMalformedData);
      }
    } else if (bytes_read == 0) {
      OnError(Error::kDisconnected);
    }
  }

  void OnWriteDone() {
    scoped_refptr<ChannelWin> keep_alive;
    bool write_error = false;
    {
      base::AutoLock lock(write_lock_);
      if (!pending_write_ ||
          !HasOverlappedIoCompleted(write_context_.GetOverlapped())) {
        return;
      }
      keep_alive = std::move(pending_write_);
      DCHECK(!outgoing_messages_.IsEmpty());

      Channel::MessagePtr message = outgoing_messages_.TakeFirst();

      // Overlapped WriteFile() to a pipe should always fully complete.
      std::optional<size_t> bytes_written = GetCompletedBytes(write_context_);
      if (!bytes_written || message->data_num_bytes() != *bytes_written) {
        reject_writes_ = write_error = true;
      } else if (!WriteNextNoLock()) {
        reject_writes_ = write_error = true;
      }
    }
    if (write_error) {
      OnWriteError(Error::kDisconnected);
    }
  }

  void ReadMore(size_t next_read_size_hint) {
    DCHECK(!pending_read_);

    size_t buffer_capacity = next_read_size_hint;
    char* buffer = GetReadBuffer(&buffer_capacity);
    // A null buffer means the size computation for the buffer overflowed;
    // break the connection.
    if (!buffer) {
      OnError(Error::kDisconnected);
      return;
    }
    DCHECK_GT(buffer_capacity, 0u);

    BOOL ok =
        ::ReadFile(handle_.get(), buffer, static_cast<DWORD>(buffer_capacity),
                   NULL, read_context_.GetOverlapped());
    if (ok || GetLastError() == ERROR_IO_PENDING) {
      pending_read_ = this;
    } else {
      OnError(Error::kDisconnected);
    }
  }

  bool WriteNoLock(Channel::Message* message)
      EXCLUSIVE_LOCKS_REQUIRED(write_lock_) {
    // We can release all the handles immediately now that we're attempting an
    // actual write to the remote process.
    //
    // If the HANDLE values are locally owned, that means we're sending them
    // to a broker who will duplicate-and-close them. If the broker never
    // receives that message (and thus we effectively leak these handles),
    // either it died (and our total dysfunction is imminent) or we died; in
    // either case the handle leak doesn't matter.
    //
    // If the handles have already been transferred and are therefore remotely
    // owned, the only way they won't eventually be managed by the remote
    // process is if the remote process dies before receiving this message. At
    // that point, again, potential handle leaks don't matter.
    std::vector<PlatformHandleInTransit> handles = message->TakeHandles();
    for (auto& handle : handles) {
      handle.CompleteTransit();
    }

    DCHECK(handle_.is_valid());
    BOOL ok = WriteFile(handle_.get(), message->data(),
                        static_cast<DWORD>(message->data_num_bytes()), NULL,
                        write_context_.GetOverlapped());
    const DWORD error = ok ? ERROR_SUCCESS : ::GetLastError();
    if (error == ERROR_SUCCESS || error == ERROR_IO_PENDING) {
      pending_write_ = this;
      return true;
    }
    return false;
  }

  bool WriteNextNoLock() EXCLUSIVE_LOCKS_REQUIRED(write_lock_) {
    if (outgoing_messages_.IsEmpty()) {
      return true;
    }
    if (reject_writes_) {
      return false;
    }
    return WriteNoLock(outgoing_messages_.GetFirst());
  }

  void OnWriteError(Error error) {
    DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
    {
      base::AutoLock lock(write_lock_);
      CHECK(reject_writes_);
    }

    if (error == Error::kDisconnected) {
      // If we can't write because the pipe is disconnected then continue
      // reading to fetch any in-flight messages, relying on end-of-stream to
      // signal the actual disconnection.
      if (pending_read_) {
        return;
      }
    }

    OnError(error);
  }

  const bool is_untrusted_process_;

  // Keeps the Channel alive at least until explicit shutdown on the IO thread.
  scoped_refptr<Channel> self_;

  // The pipe handle this Channel uses for communication.
  base::win::ScopedHandle handle_;

  const scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  bool counted_for_metrics_ = false;

  base::MessagePumpForIO::IOContext read_context_;
  // A reference to this Channel, held while a read is outstanding.
  scoped_refptr<ChannelWin> pending_read_;

  std::unique_ptr<EventIOState> event_io_;
  bool is_shutting_down_ = false;

  // Protects all fields potentially accessed on multiple threads via Write().
  base::Lock write_lock_;
  base::MessagePumpForIO::IOContext write_context_ GUARDED_BY(write_lock_);
  ChannelWinMessageQueue outgoing_messages_ GUARDED_BY(write_lock_);
  bool delay_writes_ GUARDED_BY(write_lock_) = true;
  bool reject_writes_ GUARDED_BY(write_lock_) = false;
  // A reference to this Channel, held while a write is outstanding.
  scoped_refptr<ChannelWin> pending_write_ GUARDED_BY(write_lock_);

  bool leak_handle_ = false;
};

}  // namespace

// static
bool Channel::IsRendererProcessForMetrics() {
  const auto process_type =
      base::CurrentProcess::GetInstance().GetShortType({});
  return process_type == base::ShortProcessType::kRenderer ||
         process_type == base::ShortProcessType::kRendererExtension ||
         process_type == base::ShortProcessType::kRendererTopWebUI;
}

// static
scoped_refptr<Channel> Channel::Create(
    Delegate* delegate,
    ConnectionParams connection_params,
    HandlePolicy handle_policy,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner) {
  return new ChannelWin(delegate, std::move(connection_params), handle_policy,
                        io_task_runner);
}

}  // namespace core
}  // namespace mojo
