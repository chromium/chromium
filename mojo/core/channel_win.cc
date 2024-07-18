// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "mojo/core/channel.h"

#include <windows.h>

#include <stdint.h>

#include <algorithm>
#include <limits>
#include <memory>
#include <tuple>

#include "base/containers/queue.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_pump_for_io.h"
#include "base/process/process_handle.h"
#include "base/synchronization/lock.h"
#include "base/task/current_thread.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_runner.h"
#include "base/win/scoped_handle.h"
#include "base/win/win_util.h"

namespace mojo {
namespace core {

namespace {

class ChannelWinMessageQueue {
 public:
  ChannelWinMessageQueue() = default;
  ~ChannelWinMessageQueue() = default;

  void Append(Channel::MessagePtr message) {
    queue_.emplace_back(std::move(message));
  }

  Channel::Message* GetFirst() const { return queue_.front().get(); }

  Channel::MessagePtr TakeFirst() {
    Channel::MessagePtr message = std::move(queue_.front());
    queue_.pop_front();
    return message;
  }

  bool IsEmpty() const { return queue_.empty(); }

 private:
  base::circular_deque<Channel::MessagePtr> queue_;
};

class ChannelWin : public Channel,
                   public base::CurrentThread::DestructionObserver,
                   public base::MessagePumpForIO::IOHandler {
 public:
  ChannelWin(Delegate* delegate,
             ConnectionParams connection_params,
             HandlePolicy handle_policy,
             scoped_refptr<base::SingleThreadTaskRunner> io_task_runner)
      : Channel(delegate, handle_policy),
        base::MessagePumpForIO::IOHandler(FROM_HERE),
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
      if (reject_writes_)
        return;

      bool write_now = !delay_writes_ && outgoing_messages_.IsEmpty();
      outgoing_messages_.Append(std::move(message));
      if (write_now && !WriteNoLock(outgoing_messages_.GetFirst()))
        reject_writes_ = write_error = true;
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
                              std::vector<PlatformHandle>* handles,
                              bool* deferred) override {
    DCHECK(extra_header);
    if (num_handles > std::numeric_limits<uint16_t>::max())
      return false;
    using HandleEntry = Channel::Message::HandleEntry;
    size_t handles_size = sizeof(HandleEntry) * num_handles;
    if (handles_size > extra_header_size)
      return false;
    handles->reserve(num_handles);
    const HandleEntry* extra_header_handles =
        reinterpret_cast<const HandleEntry*>(extra_header);
    for (size_t i = 0; i < num_handles; i++) {
      HANDLE handle_value =
          base::win::Uint32ToHandle(extra_header_handles[i].handle);
      if (PlatformHandleInTransit::IsPseudoHandle(handle_value))
        return false;
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
  // May run on any thread.
  ~ChannelWin() override = default;

  void StartOnIOThread() {
    base::CurrentThread::Get()->AddDestructionObserver(this);
    base::CurrentIOThread::Get()->RegisterIOHandler(handle_.get(), this);

    // Now that we have registered our IOHandler, we can start writing.
    {
      base::AutoLock lock(write_lock_);
      if (delay_writes_) {
        delay_writes_ = false;
        WriteNextNoLock();
      }
    }

    // Keep this alive in case we synchronously run shutdown, via OnError(),
    // as a result of a ReadFile() failure on the channel.
    scoped_refptr<ChannelWin> keep_alive(this);
    ReadMore(0);
  }

  void ShutDownOnIOThread() {
    base::CurrentThread::Get()->RemoveDestructionObserver(this);

    {
      // Prevent attempts to write if we've closed the handle.
      base::AutoLock lock(write_lock_);
      reject_writes_ = true;
    }

    // TODO(crbug.com/40455076): This function is expected to be called
    // once, and |handle_| should be valid at this point.
    CHECK(handle_.is_valid());
    CancelIo(handle_.get());
    if (leak_handle_) {
      std::ignore = handle_.Take();
    } else {
      handle_.Close();
    }

    // Allow |this| to be destroyed as soon as no IO is pending.
    self_ = nullptr;
  }

  // base::CurrentThread::DestructionObserver:
  void WillDestroyCurrentMessageLoop() override {
    DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
    if (self_)
      ShutDownOnIOThread();
  }

  // base::MessageLoop::IOHandler:
  void OnIOCompleted(base::MessagePumpForIO::IOContext* context,
                     DWORD bytes_transfered,
                     DWORD error) override {
    if (error != ERROR_SUCCESS) {
      if (context == &write_context_) {
        {
          base::AutoLock lock(write_lock_);
          reject_writes_ = true;
        }
        OnWriteError(Error::kDisconnected);
      } else {
        OnError(Error::kDisconnected);
      }
    } else if (context == &read_context_) {
      OnReadDone(static_cast<size_t>(bytes_transfered));
    } else {
      CHECK(context == &write_context_);
      OnWriteDone(static_cast<size_t>(bytes_transfered));
    }
    Release();
  }

  void OnReadDone(size_t bytes_read) {
    DCHECK(is_read_pending_);
    is_read_pending_ = false;

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

  void OnWriteDone(size_t bytes_written) {
    if (bytes_written == 0)
      return;

    bool write_error = false;
    {
      base::AutoLock lock(write_lock_);

      DCHECK(is_write_pending_);
      is_write_pending_ = false;
      DCHECK(!outgoing_messages_.IsEmpty());

      Channel::MessagePtr message = outgoing_messages_.TakeFirst();

      // Overlapped WriteFile() to a pipe should always fully complete.
      if (message->data_num_bytes() != bytes_written)
        reject_writes_ = write_error = true;
      else if (!WriteNextNoLock())
        reject_writes_ = write_error = true;
    }
    if (write_error)
      OnWriteError(Error::kDisconnected);
  }

  void ReadMore(size_t next_read_size_hint) {
    DCHECK(!is_read_pending_);

    size_t buffer_capacity = next_read_size_hint;
    char* buffer = GetReadBuffer(&buffer_capacity);
    DCHECK_GT(buffer_capacity, 0u);

    BOOL ok =
        ::ReadFile(handle_.get(), buffer, static_cast<DWORD>(buffer_capacity),
                   NULL, &read_context_.overlapped);
    if (ok || GetLastError() == ERROR_IO_PENDING) {
      is_read_pending_ = true;
      AddRef();
    } else {
      OnError(Error::kDisconnected);
    }
  }

  bool WriteNoLock(Channel::Message* message) {
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
    for (auto& handle : handles)
      handle.CompleteTransit();

    DCHECK(handle_.is_valid());
    BOOL ok = WriteFile(handle_.get(), message->data(),
                        static_cast<DWORD>(message->data_num_bytes()), NULL,
                        &write_context_.overlapped);
    if (ok || GetLastError() == ERROR_IO_PENDING) {
      is_write_pending_ = true;
      AddRef();
      return true;
    }
    return false;
  }

  bool WriteNextNoLock() {
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
    DCHECK(reject_writes_);

    if (error == Error::kDisconnected) {
      // If we can't write because the pipe is disconnected then continue
      // reading to fetch any in-flight messages, relying on end-of-stream to
      // signal the actual disconnection.
      if (is_read_pending_) {
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

  base::MessagePumpForIO::IOContext read_context_;
  bool is_read_pending_ = false;

  // Protects all fields potentially accessed on multiple threads via Write().
  base::Lock write_lock_;
  base::MessagePumpForIO::IOContext write_context_;
  ChannelWinMessageQueue outgoing_messages_;
  bool delay_writes_ = true;
  bool reject_writes_ = false;
  bool is_write_pending_ = false;

  bool leak_handle_ = false;
};

}  // namespace

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
