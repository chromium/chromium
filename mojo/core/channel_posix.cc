// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/core/channel.h"

#include <errno.h>
#include <sys/socket.h>

#include <algorithm>
#include <limits>
#include <memory>

#include "base/bind.h"
#include "base/containers/queue.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop_current.h"
#include "base/message_loop/message_pump_for_io.h"
#include "base/synchronization/lock.h"
#include "base/task_runner.h"
#include "build/build_config.h"
#include "mojo/core/core.h"
#include "mojo/public/cpp/platform/socket_utils_posix.h"

#if !defined(OS_NACL)
#include <sys/uio.h>
#endif

namespace mojo {
namespace core {

namespace {

const size_t kMaxBatchReadCapacity = 256 * 1024;

// A view over a Channel::Message object. The write queue uses these since
// large messages may need to be sent in chunks.
class MessageView {
 public:
  // Owns |message|. |offset| indexes the first unsent byte in the message.
  MessageView(Channel::MessagePtr message, size_t offset)
      : message_(std::move(message)),
        offset_(offset),
        handles_(message_->TakeHandles()) {
    DCHECK(!message_->data_num_bytes() || message_->data_num_bytes() > offset_);
  }

  MessageView(MessageView&& other) { *this = std::move(other); }

  MessageView& operator=(MessageView&& other) = default;

  ~MessageView() {}

  const void* data() const {
    return static_cast<const char*>(message_->data()) + offset_;
  }

  size_t data_num_bytes() const { return message_->data_num_bytes() - offset_; }

  size_t data_offset() const { return offset_; }
  void advance_data_offset(size_t num_bytes) {
    if (num_bytes) {
      DCHECK_GT(message_->data_num_bytes(), offset_ + num_bytes);
      offset_ += num_bytes;
    }
  }

  std::vector<PlatformHandleInTransit> TakeHandles() {
    return std::move(handles_);
  }
  Channel::MessagePtr TakeMessage() { return std::move(message_); }

  void SetHandles(std::vector<PlatformHandleInTransit> handles) {
    handles_ = std::move(handles);
  }

  size_t num_handles_sent() { return num_handles_sent_; }

  void set_num_handles_sent(size_t num_handles_sent) {
    num_handles_sent_ = num_handles_sent;
  }

 private:
  Channel::MessagePtr message_;
  size_t offset_;
  std::vector<PlatformHandleInTransit> handles_;
  size_t num_handles_sent_ = 0;

  DISALLOW_COPY_AND_ASSIGN(MessageView);
};

class ChannelPosix : public Channel,
                     public base::MessageLoopCurrent::DestructionObserver,
                     public base::MessagePumpForIO::FdWatcher {
 public:
  ChannelPosix(Delegate* delegate,
               ConnectionParams connection_params,
               HandlePolicy handle_policy,
               scoped_refptr<base::TaskRunner> io_task_runner)
      : Channel(delegate, handle_policy),
        self_(this),
        io_task_runner_(io_task_runner) {
    if (connection_params.server_endpoint().is_valid())
      server_ = connection_params.TakeServerEndpoint();
    else
      socket_ = connection_params.TakeEndpoint().TakePlatformHandle().TakeFD();

    CHECK(server_.is_valid() || socket_.is_valid());
  }

  void Start() override {
    if (io_task_runner_->RunsTasksInCurrentSequence()) {
      StartOnIOThread();
    } else {
      io_task_runner_->PostTask(
          FROM_HERE, base::BindOnce(&ChannelPosix::StartOnIOThread, this));
    }
  }

  void ShutDownImpl() override {
    // Always shut down asynchronously when called through the public interface.
    io_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&ChannelPosix::ShutDownOnIOThread, this));
  }

  void Write(MessagePtr message) override {
    bool write_error = false;
    {
      base::AutoLock lock(write_lock_);
      if (reject_writes_)
        return;
      if (outgoing_messages_.empty()) {
        if (!WriteNoLock(MessageView(std::move(message), 0)))
          reject_writes_ = write_error = true;
      } else {
        outgoing_messages_.emplace_back(std::move(message), 0);
      }
    }
    if (write_error) {
      // Invoke OnWriteError() asynchronously on the IO thread, in case Write()
      // was called by the delegate, in which case we should not re-enter it.
      io_task_runner_->PostTask(
          FROM_HERE, base::BindOnce(&ChannelPosix::OnWriteError, this,
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
    if (num_handles > std::numeric_limits<uint16_t>::max())
      return false;
    if (incoming_fds_.size() < num_handles)
      return true;

    handles->resize(num_handles);
    for (size_t i = 0; i < num_handles; ++i) {
      handles->at(i) = PlatformHandle(std::move(incoming_fds_.front()));
      incoming_fds_.pop_front();
    }

    return true;
  }

 private:
  ~ChannelPosix() override {
    DCHECK(!read_watcher_);
    DCHECK(!write_watcher_);
  }

  void StartOnIOThread() {
    DCHECK(!read_watcher_);
    DCHECK(!write_watcher_);
    read_watcher_.reset(
        new base::MessagePumpForIO::FdWatchController(FROM_HERE));
    base::MessageLoopCurrent::Get()->AddDestructionObserver(this);
    if (server_.is_valid()) {
      base::MessageLoopCurrentForIO::Get()->WatchFileDescriptor(
          server_.platform_handle().GetFD().get(), false /* persistent */,
          base::MessagePumpForIO::WATCH_READ, read_watcher_.get(), this);
    } else {
      write_watcher_.reset(
          new base::MessagePumpForIO::FdWatchController(FROM_HERE));
      base::MessageLoopCurrentForIO::Get()->WatchFileDescriptor(
          socket_.get(), true /* persistent */,
          base::MessagePumpForIO::WATCH_READ, read_watcher_.get(), this);
      base::AutoLock lock(write_lock_);
      FlushOutgoingMessagesNoLock();
    }
  }

  void WaitForWriteOnIOThread() {
    base::AutoLock lock(write_lock_);
    WaitForWriteOnIOThreadNoLock();
  }

  void WaitForWriteOnIOThreadNoLock() {
    if (pending_write_)
      return;
    if (!write_watcher_)
      return;
    if (io_task_runner_->RunsTasksInCurrentSequence()) {
      pending_write_ = true;
      base::MessageLoopCurrentForIO::Get()->WatchFileDescriptor(
          socket_.get(), false /* persistent */,
          base::MessagePumpForIO::WATCH_WRITE, write_watcher_.get(), this);
    } else {
      io_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&ChannelPosix::WaitForWriteOnIOThread, this));
    }
  }

  void ShutDownOnIOThread() {
    base::MessageLoopCurrent::Get()->RemoveDestructionObserver(this);

    read_watcher_.reset();
    write_watcher_.reset();
    if (leak_handle_) {
      ignore_result(socket_.release());
      server_.TakePlatformHandle().release();
    } else {
      socket_.reset();
      ignore_result(server_.TakePlatformHandle());
    }
#if defined(OS_IOS)
    fds_to_close_.clear();
#endif

    // May destroy the |this| if it was the last reference.
    self_ = nullptr;
  }

  // base::MessageLoopCurrent::DestructionObserver:
  void WillDestroyCurrentMessageLoop() override {
    DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
    if (self_)
      ShutDownOnIOThread();
  }

  // base::MessagePumpForIO::FdWatcher:
  void OnFileCanReadWithoutBlocking(int fd) override {
    if (server_.is_valid()) {
      CHECK_EQ(fd, server_.platform_handle().GetFD().get());
#if !defined(OS_NACL)
      read_watcher_.reset();
      base::MessageLoopCurrent::Get()->RemoveDestructionObserver(this);

      AcceptSocketConnection(server_.platform_handle().GetFD().get(), &socket_);
      ignore_result(server_.TakePlatformHandle());
      if (!socket_.is_valid()) {
        OnError(Error::kConnectionFailed);
        return;
      }
      StartOnIOThread();
#else
      NOTREACHED();
#endif
      return;
    }
    CHECK_EQ(fd, socket_.get());

    bool validation_error = false;
    bool read_error = false;
    size_t next_read_size = 0;
    size_t buffer_capacity = 0;
    size_t total_bytes_read = 0;
    size_t bytes_read = 0;
    do {
      buffer_capacity = next_read_size;
      char* buffer = GetReadBuffer(&buffer_capacity);
      DCHECK_GT(buffer_capacity, 0u);

      std::vector<base::ScopedFD> incoming_fds;
      ssize_t read_result =
          SocketRecvmsg(socket_.get(), buffer, buffer_capacity, &incoming_fds);
      for (auto& incoming_fd : incoming_fds)
        incoming_fds_.emplace_back(std::move(incoming_fd));

      if (read_result > 0) {
        bytes_read = static_cast<size_t>(read_result);
        total_bytes_read += bytes_read;
        if (!OnReadComplete(bytes_read, &next_read_size)) {
          read_error = true;
          validation_error = true;
          break;
        }
      } else if (read_result == 0 ||
                 (errno != EAGAIN && errno != EWOULDBLOCK)) {
        read_error = true;
        break;
      } else {
        // We expect more data but there is none to read. The
        // FileDescriptorWatcher will wake us up again once there is.
        DCHECK(errno == EAGAIN || errno == EWOULDBLOCK);
        return;
      }
    } while (bytes_read == buffer_capacity &&
             total_bytes_read < kMaxBatchReadCapacity && next_read_size > 0);
    if (read_error) {
      // Stop receiving read notifications.
      read_watcher_.reset();
      if (validation_error)
        OnError(Error::kReceivedMalformedData);
      else
        OnError(Error::kDisconnected);
    }
  }

  void OnFileCanWriteWithoutBlocking(int fd) override {
    bool write_error = false;
    {
      base::AutoLock lock(write_lock_);
      pending_write_ = false;
      if (!FlushOutgoingMessagesNoLock())
        reject_writes_ = write_error = true;
    }
    if (write_error)
      OnWriteError(Error::kDisconnected);
  }

  // Attempts to write a message directly to the channel. If the full message
  // cannot be written, it's queued and a wait is initiated to write the message
  // ASAP on the I/O thread.
  bool WriteNoLock(MessageView message_view) {
    if (server_.is_valid()) {
      outgoing_messages_.emplace_front(std::move(message_view));
      return true;
    }
    size_t bytes_written = 0;
    std::vector<PlatformHandleInTransit> handles = message_view.TakeHandles();
    size_t num_handles = handles.size();
    size_t handles_written = message_view.num_handles_sent();
    do {
      message_view.advance_data_offset(bytes_written);

      ssize_t result;
      if (handles_written < num_handles) {
        iovec iov = {const_cast<void*>(message_view.data()),
                     message_view.data_num_bytes()};
        size_t num_handles_to_send =
            std::min(num_handles - handles_written, kMaxSendmsgHandles);
        std::vector<base::ScopedFD> fds(num_handles_to_send);
        for (size_t i = 0; i < num_handles_to_send; ++i)
          fds[i] = handles[i + handles_written].TakeHandle().TakeFD();
        // TODO: Handle lots of handles.
        result = SendmsgWithHandles(socket_.get(), &iov, 1, fds);
        if (result >= 0) {
#if defined(OS_IOS)
          // There is a bug in XNU which makes it dangerous to close
          // a file descriptor while it is in transit. So instead we
          // store the file descriptor in a set and send a message to
          // the recipient, which is queued AFTER the message that
          // sent the FD. The recipient will reply to the message,
          // letting us know that it is now safe to close the file
          // descriptor. For more information, see:
          // http://crbug.com/298276
          MessagePtr fds_message(new Channel::Message(
              sizeof(int) * fds.size(), 0, Message::MessageType::HANDLES_SENT));
          int* fd_data = reinterpret_cast<int*>(fds_message->mutable_payload());
          for (size_t i = 0; i < fds.size(); ++i)
            fd_data[i] = fds[i].get();
          outgoing_messages_.emplace_back(std::move(fds_message), 0);
          {
            base::AutoLock l(fds_to_close_lock_);
            for (auto& fd : fds)
              fds_to_close_.emplace_back(std::move(fd));
          }
#endif  // defined(OS_IOS)
          handles_written += num_handles_to_send;
          DCHECK_LE(handles_written, num_handles);
          message_view.set_num_handles_sent(handles_written);
        } else {
          // Message transmission failed, so pull the FDs back into |handles|
          // so they can be held by the Message again.
          for (size_t i = 0; i < fds.size(); ++i) {
            handles[i + handles_written] =
                PlatformHandleInTransit(PlatformHandle(std::move(fds[i])));
          }
        }
      } else {
        result = SocketWrite(socket_.get(), message_view.data(),
                             message_view.data_num_bytes());
      }

      if (result < 0) {
        if (errno != EAGAIN &&
            errno != EWOULDBLOCK
#if defined(OS_IOS)
            // On iOS if sendmsg() is trying to send fds between processes and
            // there isn't enough room in the output buffer to send the fd
            // structure over atomically then EMSGSIZE is returned.
            //
            // EMSGSIZE presents a problem since the system APIs can only call
            // us when there's room in the socket buffer and not when there is
            // "enough" room.
            //
            // The current behavior is to return to the event loop when EMSGSIZE
            // is received and hopefull service another FD.  This is however
            // still technically a busy wait since the event loop will call us
            // right back until the receiver has read enough data to allow
            // passing the FD over atomically.
            && errno != EMSGSIZE
#endif
        ) {
          return false;
        }
        message_view.SetHandles(std::move(handles));
        outgoing_messages_.emplace_front(std::move(message_view));
        WaitForWriteOnIOThreadNoLock();
        return true;
      }

      bytes_written = static_cast<size_t>(result);
    } while (handles_written < num_handles ||
             bytes_written < message_view.data_num_bytes());

    return FlushOutgoingMessagesNoLock();
  }

  bool FlushOutgoingMessagesNoLock() {
    base::circular_deque<MessageView> messages;
    std::swap(outgoing_messages_, messages);

    while (!messages.empty()) {
      if (!WriteNoLock(std::move(messages.front())))
        return false;

      messages.pop_front();
      if (!outgoing_messages_.empty()) {
        // The message was requeued by WriteNoLock(), so we have to wait for
        // pipe to become writable again. Repopulate the message queue and exit.
        // If sending the message triggered any control messages, they may be
        // in |outgoing_messages_| in addition to or instead of the message
        // being sent.
        std::swap(messages, outgoing_messages_);
        while (!messages.empty()) {
          outgoing_messages_.push_front(std::move(messages.back()));
          messages.pop_back();
        }
        return true;
      }
    }

    return true;
  }

#if defined(OS_IOS)
  bool OnControlMessage(Message::MessageType message_type,
                        const void* payload,
                        size_t payload_size,
                        std::vector<PlatformHandle> handles) override {
    switch (message_type) {
      case Message::MessageType::HANDLES_SENT: {
        if (payload_size == 0)
          break;
        MessagePtr message(new Channel::Message(
            payload_size, 0, Message::MessageType::HANDLES_SENT_ACK));
        memcpy(message->mutable_payload(), payload, payload_size);
        Write(std::move(message));
        return true;
      }

      case Message::MessageType::HANDLES_SENT_ACK: {
        size_t num_fds = payload_size / sizeof(int);
        if (num_fds == 0 || payload_size % sizeof(int) != 0)
          break;

        const int* fds = reinterpret_cast<const int*>(payload);
        if (!CloseHandles(fds, num_fds))
          break;
        return true;
      }

      default:
        break;
    }

    return false;
  }

  // Closes handles referenced by |fds|. Returns false if |num_fds| is 0, or if
  // |fds| does not match a sequence of handles in |fds_to_close_|.
  bool CloseHandles(const int* fds, size_t num_fds) {
    base::AutoLock l(fds_to_close_lock_);
    if (!num_fds)
      return false;

    auto start = std::find_if(
        fds_to_close_.begin(), fds_to_close_.end(),
        [&fds](const base::ScopedFD& fd) { return fd.get() == fds[0]; });
    if (start == fds_to_close_.end())
      return false;

    auto it = start;
    size_t i = 0;
    // The FDs in the message should match a sequence of handles in
    // |fds_to_close_|.
    // TODO(wez): Consider making |fds_to_close_| a circular_deque<>
    // for greater efficiency? Or assign a unique Id to each FD-containing
    // message, and map that to a vector of FDs to close, to avoid the
    // need for this traversal? Id could even be the first FD in the message.
    for (; i < num_fds && it != fds_to_close_.end(); i++, ++it) {
      if (it->get() != fds[i])
        return false;
    }
    if (i != num_fds)
      return false;

    // Close the FDs by erase()ing their ScopedFDs.
    fds_to_close_.erase(start, it);
    return true;
  }
#endif  // defined(OS_IOS)

  void OnWriteError(Error error) {
    DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
    DCHECK(reject_writes_);

    if (error == Error::kDisconnected) {
      // If we can't write because the pipe is disconnected then continue
      // reading to fetch any in-flight messages, relying on end-of-stream to
      // signal the actual disconnection.
      if (read_watcher_) {
        write_watcher_.reset();
        return;
      }
    }

    OnError(error);
  }

  // Keeps the Channel alive at least until explicit shutdown on the IO thread.
  scoped_refptr<Channel> self_;

  // We may be initialized with a server socket, in which case this will be
  // valid until it accepts an incoming connection.
  PlatformChannelServerEndpoint server_;

  // The socket over which to communicate. May be passed in at construction time
  // or accepted over |server_|.
  base::ScopedFD socket_;

  scoped_refptr<base::TaskRunner> io_task_runner_;

  // These watchers must only be accessed on the IO thread.
  std::unique_ptr<base::MessagePumpForIO::FdWatchController> read_watcher_;
  std::unique_ptr<base::MessagePumpForIO::FdWatchController> write_watcher_;

  base::circular_deque<base::ScopedFD> incoming_fds_;

  // Protects |pending_write_| and |outgoing_messages_|.
  base::Lock write_lock_;
  bool pending_write_ = false;
  bool reject_writes_ = false;
  base::circular_deque<MessageView> outgoing_messages_;

  bool leak_handle_ = false;

#if defined(OS_IOS)
  base::Lock fds_to_close_lock_;
  std::vector<base::ScopedFD> fds_to_close_;
#endif  // defined(OS_IOS)

  DISALLOW_COPY_AND_ASSIGN(ChannelPosix);
};

}  // namespace

// static
scoped_refptr<Channel> Channel::Create(
    Delegate* delegate,
    ConnectionParams connection_params,
    HandlePolicy handle_policy,
    scoped_refptr<base::TaskRunner> io_task_runner) {
  return new ChannelPosix(delegate, std::move(connection_params), handle_policy,
                          io_task_runner);
}

}  // namespace core
}  // namespace mojo
