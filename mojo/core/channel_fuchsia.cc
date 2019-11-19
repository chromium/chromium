// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/core/channel.h"

#include <lib/fdio/fd.h>
#include <lib/fdio/limits.h>
#include <lib/zx/channel.h>
#include <lib/zx/handle.h>
#include <zircon/processargs.h>
#include <zircon/status.h>
#include <zircon/syscalls.h>
#include <algorithm>

#include "base/bind.h"
#include "base/containers/circular_deque.h"
#include "base/files/scoped_file.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop_current.h"
#include "base/message_loop/message_pump_for_io.h"
#include "base/stl_util.h"
#include "base/synchronization/lock.h"
#include "base/task_runner.h"
#include "mojo/core/platform_handle_in_transit.h"

namespace mojo {
namespace core {

namespace {

const size_t kMaxBatchReadCapacity = 256 * 1024;

bool UnwrapFdioHandle(PlatformHandleInTransit handle,
                      PlatformHandleInTransit* out_handle,
                      Channel::Message::HandleInfoEntry* info) {
  DCHECK(handle.handle().is_valid());

  if (!handle.handle().is_valid_fd()) {
    info->is_file_descriptor = false;
    *out_handle = std::move(handle);
    return true;
  }

  // Try to transfer the FD, and if that fails (for example if the file has
  // already been dup()d into another FD) then fall back to cloning it.
  zx::handle result;
  zx_status_t status = fdio_fd_transfer(handle.handle().GetFD().get(),
                                        result.reset_and_get_address());
  if (status == ZX_OK) {
    // On success, the fd in |handle| has been transferred and is no longer
    // valid. Release from the PlatformHandle to avoid close()ing an invalid
    // an invalid handle.
    handle.CompleteTransit();
  } else if (status == ZX_ERR_UNAVAILABLE) {
    // No luck, try cloning instead.
    status = fdio_fd_clone(handle.handle().GetFD().get(),
                           result.reset_and_get_address());
  }

  if (status != ZX_OK) {
    ZX_DLOG(ERROR, status) << "fdio_fd_clone/transfer("
                           << handle.handle().GetFD().get() << ")";
    return false;
  }

  info->is_file_descriptor = true;
  *out_handle = PlatformHandleInTransit(PlatformHandle(std::move(result)));
  return true;
}

PlatformHandle WrapFdioHandle(zx::handle handle,
                              Channel::Message::HandleInfoEntry info) {
  if (!info.is_file_descriptor)
    return PlatformHandle(std::move(handle));

  base::ScopedFD out_fd;
  zx_status_t status =
      fdio_fd_create(handle.release(), base::ScopedFD::Receiver(out_fd).get());
  if (status != ZX_OK) {
    ZX_DLOG(ERROR, status) << "fdio_fd_create";
    return PlatformHandle();
  }
  return PlatformHandle(std::move(out_fd));
}

// A view over a Channel::Message object. The write queue uses these since
// large messages may need to be sent in chunks.
class MessageView {
 public:
  // Owns |message|. |offset| indexes the first unsent byte in the message.
  MessageView(Channel::MessagePtr message, size_t offset)
      : message_(std::move(message)),
        offset_(offset),
        handles_(message_->TakeHandles()) {
    DCHECK_GT(message_->data_num_bytes(), offset_);
  }

  MessageView(MessageView&& other) { *this = std::move(other); }

  MessageView& operator=(MessageView&& other) {
    message_ = std::move(other.message_);
    offset_ = other.offset_;
    handles_ = std::move(other.handles_);
    return *this;
  }

  ~MessageView() {}

  const void* data() const {
    return static_cast<const char*>(message_->data()) + offset_;
  }

  size_t data_num_bytes() const { return message_->data_num_bytes() - offset_; }

  size_t data_offset() const { return offset_; }
  void advance_data_offset(size_t num_bytes) {
    DCHECK_GT(message_->data_num_bytes(), offset_ + num_bytes);
    offset_ += num_bytes;
  }

  std::vector<PlatformHandleInTransit> TakeHandles() {
    if (handles_.empty())
      return std::vector<PlatformHandleInTransit>();

    // We can only pass Fuchsia handles via IPC, so unwrap any FDIO file-
    // descriptors in |handles_| into the underlying handles, with metadata in
    // the extra header to note which belong to FDIO.
    auto* handles_info = reinterpret_cast<Channel::Message::HandleInfoEntry*>(
        message_->mutable_extra_header());
    memset(handles_info, 0, message_->extra_header_size());

    // Since file descriptors unwrap to a single handle, we can unwrap in-place
    // in the |handles_| vector.
    for (size_t i = 0; i < handles_.size(); i++) {
      if (!UnwrapFdioHandle(std::move(handles_[i]), &handles_[i],
                            &handles_info[i])) {
        return std::vector<PlatformHandleInTransit>();
      }
    }
    return std::move(handles_);
  }

 private:
  Channel::MessagePtr message_;
  size_t offset_;
  std::vector<PlatformHandleInTransit> handles_;

  DISALLOW_COPY_AND_ASSIGN(MessageView);
};

class ChannelFuchsia : public Channel,
                       public base::MessageLoopCurrent::DestructionObserver,
                       public base::MessagePumpForIO::ZxHandleWatcher {
 public:
  ChannelFuchsia(Delegate* delegate,
                 ConnectionParams connection_params,
                 HandlePolicy handle_policy,
                 scoped_refptr<base::TaskRunner> io_task_runner)
      : Channel(delegate, handle_policy),
        self_(this),
        handle_(
            connection_params.TakeEndpoint().TakePlatformHandle().TakeHandle()),
        io_task_runner_(io_task_runner) {
    CHECK(handle_.is_valid());
  }

  void Start() override {
    if (io_task_runner_->RunsTasksInCurrentSequence()) {
      StartOnIOThread();
    } else {
      io_task_runner_->PostTask(
          FROM_HERE, base::BindOnce(&ChannelFuchsia::StartOnIOThread, this));
    }
  }

  void ShutDownImpl() override {
    // Always shut down asynchronously when called through the public interface.
    io_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&ChannelFuchsia::ShutDownOnIOThread, this));
  }

  void Write(MessagePtr message) override {
    bool write_error = false;
    {
      base::AutoLock lock(write_lock_);
      if (reject_writes_)
        return;
      if (!WriteNoLock(MessageView(std::move(message), 0)))
        reject_writes_ = write_error = true;
    }
    if (write_error) {
      // Do not synchronously invoke OnWriteError(). Write() may have been
      // called by the delegate and we don't want to re-enter it.
      io_task_runner_->PostTask(
          FROM_HERE, base::BindOnce(&ChannelFuchsia::OnWriteError, this,
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
    DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
    if (num_handles > std::numeric_limits<uint16_t>::max())
      return false;

    // Locate the handle info and verify there is enough of it.
    if (!extra_header)
      return false;
    const auto* handles_info =
        reinterpret_cast<const Channel::Message::HandleInfoEntry*>(
            extra_header);
    size_t handles_info_size = sizeof(handles_info[0]) * num_handles;
    if (handles_info_size > extra_header_size)
      return false;

    // If there are too few handles then we're not ready yet, so return true
    // indicating things are OK, but leave |handles| empty.
    if (incoming_handles_.size() < num_handles)
      return true;

    handles->reserve(num_handles);
    for (size_t i = 0; i < num_handles; ++i) {
      handles->emplace_back(WrapFdioHandle(std::move(incoming_handles_.front()),
                                           handles_info[i]));
      DCHECK(handles->back().is_valid());
      incoming_handles_.pop_front();
    }
    return true;
  }

 private:
  ~ChannelFuchsia() override { DCHECK(!read_watch_); }

  void StartOnIOThread() {
    DCHECK(!read_watch_);

    base::MessageLoopCurrent::Get()->AddDestructionObserver(this);

    read_watch_.reset(
        new base::MessagePumpForIO::ZxHandleWatchController(FROM_HERE));
    base::MessageLoopCurrentForIO::Get()->WatchZxHandle(
        handle_.get(), true /* persistent */,
        ZX_CHANNEL_READABLE | ZX_CHANNEL_PEER_CLOSED, read_watch_.get(), this);
  }

  void ShutDownOnIOThread() {
    base::MessageLoopCurrent::Get()->RemoveDestructionObserver(this);

    read_watch_.reset();
    if (leak_handle_)
      ignore_result(handle_.release());
    handle_.reset();

    // May destroy the |this| if it was the last reference.
    self_ = nullptr;
  }

  // base::MessageLoopCurrent::DestructionObserver:
  void WillDestroyCurrentMessageLoop() override {
    DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
    if (self_)
      ShutDownOnIOThread();
  }

  // base::MessagePumpForIO::ZxHandleWatcher:
  void OnZxHandleSignalled(zx_handle_t handle, zx_signals_t signals) override {
    DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
    CHECK_EQ(handle, handle_.get());
    DCHECK((ZX_CHANNEL_READABLE | ZX_CHANNEL_PEER_CLOSED) & signals);

    // We always try to read message(s), even if ZX_CHANNEL_PEER_CLOSED, since
    // the peer may have closed while messages were still unread, in the pipe.

    bool validation_error = false;
    bool read_error = false;
    size_t next_read_size = 0;
    size_t buffer_capacity = 0;
    size_t total_bytes_read = 0;
    do {
      buffer_capacity = next_read_size;
      char* buffer = GetReadBuffer(&buffer_capacity);
      DCHECK_GT(buffer_capacity, 0u);

      uint32_t bytes_read = 0;
      uint32_t handles_read = 0;
      zx_handle_t handles[ZX_CHANNEL_MAX_MSG_HANDLES] = {};

      zx_status_t read_result =
          handle_.read(0, buffer, handles, buffer_capacity, base::size(handles),
                       &bytes_read, &handles_read);
      if (read_result == ZX_OK) {
        for (size_t i = 0; i < handles_read; ++i) {
          incoming_handles_.emplace_back(handles[i]);
        }
        total_bytes_read += bytes_read;
        if (!OnReadComplete(bytes_read, &next_read_size)) {
          read_error = true;
          validation_error = true;
          break;
        }
      } else if (read_result == ZX_ERR_BUFFER_TOO_SMALL) {
        DCHECK_LE(handles_read, base::size(handles));
        next_read_size = bytes_read;
      } else if (read_result == ZX_ERR_SHOULD_WAIT) {
        break;
      } else {
        ZX_DLOG_IF(ERROR, read_result != ZX_ERR_PEER_CLOSED, read_result)
            << "zx_channel_read";
        read_error = true;
        break;
      }
    } while (total_bytes_read < kMaxBatchReadCapacity && next_read_size > 0);
    if (read_error) {
      // Stop receiving read notifications.
      read_watch_.reset();
      if (validation_error)
        OnError(Error::kReceivedMalformedData);
      else
        OnError(Error::kDisconnected);
    }
  }

  // Attempts to write a message directly to the channel. If the full message
  // cannot be written, it's queued and a wait is initiated to write the message
  // ASAP on the I/O thread.
  bool WriteNoLock(MessageView message_view) {
    uint32_t write_bytes = 0;
    do {
      message_view.advance_data_offset(write_bytes);

      std::vector<PlatformHandleInTransit> outgoing_handles =
          message_view.TakeHandles();
      zx_handle_t handles[ZX_CHANNEL_MAX_MSG_HANDLES] = {};
      size_t handles_count = outgoing_handles.size();

      DCHECK_LE(handles_count, base::size(handles));
      for (size_t i = 0; i < handles_count; ++i) {
        DCHECK(outgoing_handles[i].handle().is_valid());
        handles[i] = outgoing_handles[i].handle().GetHandle().get();
      }

      write_bytes = std::min(message_view.data_num_bytes(),
                             static_cast<size_t>(ZX_CHANNEL_MAX_MSG_BYTES));
      zx_status_t result = handle_.write(0, message_view.data(), write_bytes,
                                         handles, handles_count);
      // zx_channel_write() consumes |handles| whether or not it succeeds, so
      // release() our copies now, to avoid them being double-closed.
      for (auto& outgoing_handle : outgoing_handles)
        outgoing_handle.CompleteTransit();

      if (result != ZX_OK) {
        // TODO(fuchsia): Handle ZX_ERR_SHOULD_WAIT flow-control errors, once
        // the platform starts generating them. See https://crbug.com/754084.
        ZX_DLOG_IF(ERROR, result != ZX_ERR_PEER_CLOSED, result)
            << "WriteNoLock(zx_channel_write)";
        return false;
      }

    } while (write_bytes < message_view.data_num_bytes());

    return true;
  }

  void OnWriteError(Error error) {
    DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
    DCHECK(reject_writes_);

    if (error == Error::kDisconnected) {
      // If we can't write because the pipe is disconnected then continue
      // reading to fetch any in-flight messages, relying on end-of-stream to
      // signal the actual disconnection.
      if (read_watch_) {
        // TODO: When we add flow-control for writes, we also need to reset the
        // write-watcher here.
        return;
      }
    }

    OnError(error);
  }

  // Keeps the Channel alive at least until explicit shutdown on the IO thread.
  scoped_refptr<Channel> self_;

  zx::channel handle_;
  scoped_refptr<base::TaskRunner> io_task_runner_;

  // These members are only used on the IO thread.
  std::unique_ptr<base::MessagePumpForIO::ZxHandleWatchController> read_watch_;
  base::circular_deque<zx::handle> incoming_handles_;
  bool leak_handle_ = false;

  base::Lock write_lock_;
  bool reject_writes_ = false;

  DISALLOW_COPY_AND_ASSIGN(ChannelFuchsia);
};

}  // namespace

// static
scoped_refptr<Channel> Channel::Create(
    Delegate* delegate,
    ConnectionParams connection_params,
    HandlePolicy handle_policy,
    scoped_refptr<base::TaskRunner> io_task_runner) {
  return new ChannelFuchsia(delegate, std::move(connection_params),
                            handle_policy, std::move(io_task_runner));
}

}  // namespace core
}  // namespace mojo
