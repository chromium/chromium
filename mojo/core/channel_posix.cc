// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "mojo/core/channel_posix.h"

#include <errno.h>
#include <sys/socket.h>

#include <atomic>
#include <limits>
#include <memory>
#include <tuple>

#include "base/cpu_reduction_experiment.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/message_loop/message_pump_for_io.h"
#include "base/metrics/histogram_macros.h"
#include "base/synchronization/lock.h"
#include "base/task/current_thread.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_runner.h"
#include "base/time/time.h"
#include "base/types/fixed_array.h"
#include "build/build_config.h"
#include "mojo/public/cpp/platform/socket_utils_posix.h"

#if !BUILDFLAG(IS_NACL)
#include <limits.h>
#include <sys/uio.h>

#if (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID))
#include "mojo/core/channel_linux.h"
#endif

#endif  // !BUILDFLAG(IS_NACL)

#if BUILDFLAG(IS_ANDROID)
#include "mojo/core/channel_binder.h"
#endif

namespace mojo::core {

namespace {
#if !BUILDFLAG(IS_NACL)
std::atomic<bool> g_use_writev{false};
#endif  // !BUILDFLAG(IS_NACL)

const size_t kMaxBatchReadCapacity = 256 * 1024;
}  // namespace

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

  MessageView(MessageView&& other) = default;

  MessageView& operator=(MessageView&& other) = default;

  MessageView(const MessageView&) = delete;
  MessageView& operator=(const MessageView&) = delete;

  ~MessageView() {
    if (message_) {
      if (base::ShouldLogHistogramForCpuReductionExperiment()) {
        UMA_HISTOGRAM_TIMES("Mojo.Channel.WriteMessageLatency",
                            base::TimeTicks::Now() - start_time_);
      }
    }
  }

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

  void SetHandles(std::vector<PlatformHandleInTransit> handles) {
    handles_ = std::move(handles);
  }

  size_t num_handles_sent() { return num_handles_sent_; }

  void set_num_handles_sent(size_t num_handles_sent) {
    num_handles_sent_ = num_handles_sent;
  }

  size_t num_handles_remaining() const {
    return handles_.size() - num_handles_sent_;
  }

 private:
  Channel::MessagePtr message_;
  size_t offset_;
  std::vector<PlatformHandleInTransit> handles_;
  size_t num_handles_sent_ = 0;

  base::TimeTicks start_time_ = base::TimeTicks::Now();
};

ChannelPosix::ChannelPosix(
    Delegate* delegate,
    ConnectionParams connection_params,
    HandlePolicy handle_policy,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner)
    : Channel(delegate, handle_policy),
      self_(this),
      io_task_runner_(io_task_runner) {
  socket_ = connection_params.TakeEndpoint().TakePlatformHandle().TakeFD();
  CHECK(socket_.is_valid());
}

ChannelPosix::~ChannelPosix() {
  CHECK(!read_watcher_);
  CHECK(!write_watcher_);
}

void ChannelPosix::Start() {
  if (io_task_runner_->RunsTasksInCurrentSequence()) {
    StartOnIOThread();
  } else {
    io_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&ChannelPosix::StartOnIOThread, this));
  }
}

void ChannelPosix::ShutDownImpl() {
  // Always shut down asynchronously when called through the public interface.
  io_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&ChannelPosix::ShutDownOnIOThread, this));
}

void ChannelPosix::Write(MessagePtr message) {
  if (ShouldRecordSubsampledHistograms()) {
    UMA_HISTOGRAM_COUNTS_100000("Mojo.Channel.WriteMessageSize",
                                message->data_num_bytes());
    LogHistogramForIPCMetrics(MessageType::kSent);
  }

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
    io_task_runner_->PostTask(FROM_HERE,
                              base::BindOnce(&ChannelPosix::OnWriteError, this,
                                             Error::kDisconnected));
  }
}

void ChannelPosix::LeakHandle() {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
  leak_handle_ = true;
}

bool ChannelPosix::GetReadPlatformHandles(const void* payload,
                                          size_t payload_size,
                                          size_t num_handles,
                                          const void* extra_header,
                                          size_t extra_header_size,
                                          std::vector<PlatformHandle>* handles,
                                          bool* deferred) {
  if (num_handles > std::numeric_limits<uint16_t>::max())
    return false;

  return GetReadPlatformHandlesForIpcz(num_handles, *handles);
}

bool ChannelPosix::GetReadPlatformHandlesForIpcz(
    size_t num_handles,
    std::vector<PlatformHandle>& handles) {
  if (incoming_fds_.size() < num_handles) {
    return true;
  }

  DCHECK(handles.empty());
  handles.reserve(num_handles);
  while (num_handles--) {
    handles.emplace_back(std::move(incoming_fds_.front()));
    incoming_fds_.pop_front();
  }
  return true;
}

void ChannelPosix::StartOnIOThread() {
  base::CurrentThread::Get()->AddDestructionObserver(this);
  base::AutoLock lock(write_lock_);
  DCHECK(!read_watcher_);
  read_watcher_ =
      std::make_unique<base::MessagePumpForIO::FdWatchController>(FROM_HERE);
  base::CurrentIOThread::Get()->WatchFileDescriptor(
      socket_.get(), true /* persistent */, base::MessagePumpForIO::WATCH_READ,
      read_watcher_.get(), this);
  DCHECK(!write_watcher_);
  write_watcher_ =
      std::make_unique<base::MessagePumpForIO::FdWatchController>(FROM_HERE);
  FlushOutgoingMessagesNoLock();
}

void ChannelPosix::WaitForWriteOnIOThread() {
  base::AutoLock lock(write_lock_);
  WaitForWriteOnIOThreadNoLock();
}

void ChannelPosix::WaitForWriteOnIOThreadNoLock() {
  if (pending_write_)
    return;
  if (!write_watcher_)
    return;
  // This may be called from a `RunOrPostTask()` callback running in sequence
  // with the IO thread, but on a different thread. In that case,
  // `RunsTaskInCurrentSequence()` would return true, so use
  // `BelongsToCurrentThread()` to detect that we aren't on the IO thread
  // (otherwise, `base::CurrentIOThread::Get()` would fail).
  if (io_task_runner_->BelongsToCurrentThread()) {
    pending_write_ = true;
    base::CurrentIOThread::Get()->WatchFileDescriptor(
        socket_.get(), false /* persistent */,
        base::MessagePumpForIO::WATCH_WRITE, write_watcher_.get(), this);
  } else {
    io_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&ChannelPosix::WaitForWriteOnIOThread, this));
  }
}

void ChannelPosix::ShutDownOnIOThread() {
  base::CurrentThread::Get()->RemoveDestructionObserver(this);

  {
    base::AutoLock lock(write_lock_);
    reject_writes_ = true;
    read_watcher_.reset();
    write_watcher_.reset();
    if (leak_handle_) {
      std::ignore = socket_.release();
    } else {
      socket_.reset();
    }
#if BUILDFLAG(IS_IOS)
    base::AutoLock fd_lock(fds_to_close_lock_);
    fds_to_close_.clear();
#endif
  }

  // May destroy the |this| if it was the last reference.
  self_ = nullptr;
}

void ChannelPosix::WillDestroyCurrentMessageLoop() {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
  if (self_)
    ShutDownOnIOThread();
}

void ChannelPosix::OnFileCanReadWithoutBlocking(int fd) {
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
    } else if (read_result == 0 || (errno != EAGAIN && errno != EWOULDBLOCK)) {
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
    {
      base::AutoLock lock(write_lock_);
      read_watcher_.reset();
    }
    if (validation_error)
      OnError(Error::kReceivedMalformedData);
    else
      OnError(Error::kDisconnected);
  }
}

void ChannelPosix::OnFileCanWriteWithoutBlocking(int fd) {
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
bool ChannelPosix::WriteNoLock(MessageView message_view) {
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
#if BUILDFLAG(IS_IOS)
        // There is a bug in XNU which makes it dangerous to close
        // a file descriptor while it is in transit. So instead we
        // store the file descriptor in a set and send a message to
        // the recipient, which is queued AFTER the message that
        // sent the FD. The recipient will reply to the message,
        // letting us know that it is now safe to close the file
        // descriptor. For more information, see:
        // http://crbug.com/298276
        MessagePtr fds_message = Message::CreateMessage(
            sizeof(int) * fds.size(), 0, Message::MessageType::HANDLES_SENT);
        int* fd_data = reinterpret_cast<int*>(fds_message->mutable_payload());
        for (size_t i = 0; i < fds.size(); ++i)
          fd_data[i] = fds[i].get();
        outgoing_messages_.emplace_back(std::move(fds_message), 0);
        {
          base::AutoLock l(fds_to_close_lock_);
          for (auto& fd : fds)
            fds_to_close_.emplace_back(std::move(fd));
        }
#endif  // BUILDFLAG(IS_IOS)
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
#if BUILDFLAG(IS_IOS)
          // On iOS if sendmsg() is trying to send fds between processes and
          // there isn't enough room in the output buffer to send the fd
          // structure over atomically then EMSGSIZE is returned.
          //
          // EMSGSIZE presents a problem since the system APIs can only call
          // us when there's room in the socket buffer and not when there is
          // "enough" room.
          //
          // The current behavior is to return to the event loop when EMSGSIZE
          // is received and hopefully service another FD.  This is however
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

bool ChannelPosix::FlushOutgoingMessagesNoLock() {
#if !BUILDFLAG(IS_NACL)
  if (g_use_writev)
    return FlushOutgoingMessagesWritevNoLock();
#endif

  base::circular_deque<MessageView> messages;
  std::swap(outgoing_messages_, messages);

  if (!messages.empty()) {
    UMA_HISTOGRAM_COUNTS_1000("Mojo.Channel.WriteQueuePendingMessages",
                              messages.size());
  }

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

void ChannelPosix::RejectUpgradeOffer() {
  Write(Message::CreateMessage(0, 0, Message::MessageType::UPGRADE_REJECT));
}

void ChannelPosix::AcceptUpgradeOffer() {
  Write(Message::CreateMessage(0, 0, Message::MessageType::UPGRADE_ACCEPT));
}

void ChannelPosix::OnWriteError(Error error) {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
  DCHECK([&]() {
    base::AutoLock lock(write_lock_);
    return reject_writes_;
  }());

  if (error == Error::kDisconnected) {
    base::AutoLock lock(write_lock_);
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

#if !BUILDFLAG(IS_NACL)
bool ChannelPosix::WriteOutgoingMessagesWithWritev() {
  if (outgoing_messages_.empty())
    return true;

  // If all goes well we can submit a writev(2) with a iovec of size
  // outgoing_messages_.size() but never more than the kernel allows.
  size_t num_messages_to_send =
      std::min<size_t>(IOV_MAX, outgoing_messages_.size());
  base::FixedArray<iovec> iov(num_messages_to_send);

  // Populate the iov.
  size_t num_iovs_set = 0;
  for (auto it = outgoing_messages_.begin();
       num_iovs_set < num_messages_to_send; ++it) {
    if (it->num_handles_remaining() > 0) {
      // We can't send handles with writev(2) so stop at this message.
      break;
    }

    iov[num_iovs_set].iov_base = const_cast<void*>(it->data());
    iov[num_iovs_set].iov_len = it->data_num_bytes();
    num_iovs_set++;
  }

  size_t iov_offset = 0;
  while (iov_offset < num_iovs_set) {
    ssize_t bytes_written = SocketWritev(socket_.get(), &iov[iov_offset],
                                         num_iovs_set - iov_offset);
    if (bytes_written < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        WaitForWriteOnIOThreadNoLock();
        return true;
      }
      return false;
    }

    // Let's walk our outgoing_messages_ popping off outgoing_messages_
    // that were fully written.
    size_t bytes_remaining = bytes_written;
    while (bytes_remaining > 0) {
      if (bytes_remaining >= outgoing_messages_.front().data_num_bytes()) {
        // This message was fully written.
        bytes_remaining -= outgoing_messages_.front().data_num_bytes();
        outgoing_messages_.pop_front();
        iov_offset++;
      } else {
        // This message was partially written, account for what was
        // already written.
        outgoing_messages_.front().advance_data_offset(bytes_remaining);
        bytes_remaining = 0;

        // Update the iov too as we will call writev again.
        iov[iov_offset].iov_base =
            const_cast<void*>(outgoing_messages_.front().data());
        iov[iov_offset].iov_len = outgoing_messages_.front().data_num_bytes();
      }
    }
  }

  return true;
}

// FlushOutgoingMessagesWritevNoLock is equivalent to
// FlushOutgoingMessagesNoLock except it looks for opportunities to make only
// a single write syscall by using writev(2) instead of write(2). In most
// situations this is very straight forward; however, when a handle needs to
// be transferred we cannot use writev(2) and instead will fall back to the
// standard write.
bool ChannelPosix::FlushOutgoingMessagesWritevNoLock() {
  do {
    // If the first message contains a handle we will flush it first using a
    // standard write, we will also use the standard write if we only have a
    // single message.
    while (!outgoing_messages_.empty() &&
           (outgoing_messages_.front().num_handles_remaining() > 0 ||
            outgoing_messages_.size() == 1)) {
      MessageView message = std::move(outgoing_messages_.front());

      outgoing_messages_.pop_front();
      size_t messages_before_write = outgoing_messages_.size();
      if (!WriteNoLock(std::move(message)))
        return false;

      if (outgoing_messages_.size() > messages_before_write) {
        // It was re-queued by WriteNoLock.
        return true;
      }
    }

    if (!WriteOutgoingMessagesWithWritev())
      return false;

    // At this point if we have more messages then it's either because we
    // exceeded IOV_MAX OR it's because we ran into a FileHandle. Either way
    // we just start the process all over again and it will flush any
    // FileHandles before attempting writev(2) again.
  } while (!outgoing_messages_.empty());
  return true;
}
#endif  // !BUILDFLAG(IS_NACL)

bool ChannelPosix::OnControlMessage(Message::MessageType message_type,
                                    const void* payload,
                                    size_t payload_size,
                                    std::vector<PlatformHandle> handles) {
  switch (message_type) {
    case Message::MessageType::UPGRADE_OFFER: {
      // ChannelPosix itself does not support upgrades, if the message was
      // delivered here it could have been when this channel was created we
      // didn't support upgrades but another process does.
      RejectUpgradeOffer();
      return true;
    }
#if BUILDFLAG(IS_IOS)
    case Message::MessageType::HANDLES_SENT: {
      if (payload_size == 0)
        break;
      MessagePtr message = Message::CreateMessage(
          payload_size, 0, Message::MessageType::HANDLES_SENT_ACK);
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
#endif
    default:
      break;
  }

  return false;
}

#if BUILDFLAG(IS_IOS)
// Closes handles referenced by |fds|. Returns false if |num_fds| is 0, or if
// |fds| does not match a sequence of handles in |fds_to_close_|.
bool ChannelPosix::CloseHandles(const int* fds, size_t num_fds) {
  base::AutoLock l(fds_to_close_lock_);
  if (!num_fds)
    return false;

  auto start = base::ranges::find(fds_to_close_, fds[0], &base::ScopedFD::get);
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
#endif  // BUILDFLAG(IS_IOS)

#if !BUILDFLAG(IS_NACL)
// static
void Channel::set_posix_use_writev(bool use_writev) {
  g_use_writev = use_writev;
}
#endif  // !BUILDFLAG(IS_NACL)

// static
scoped_refptr<Channel> Channel::Create(
    Delegate* delegate,
    ConnectionParams connection_params,
    HandlePolicy handle_policy,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner) {
#if BUILDFLAG(IS_ANDROID)
  if (connection_params.endpoint().platform_handle().is_valid_binder()) {
    return base::MakeRefCounted<ChannelBinder>(
        delegate, std::move(connection_params), handle_policy,
        std::move(io_task_runner));
  }
#endif
#if !BUILDFLAG(IS_NACL) && \
    (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID))
  return new ChannelLinux(delegate, std::move(connection_params), handle_policy,
                          io_task_runner);
#else
  return new ChannelPosix(delegate, std::move(connection_params), handle_policy,
                          io_task_runner);
#endif
}

#if !BUILDFLAG(IS_NACL)
#if (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID))
// static
bool Channel::SupportsChannelUpgrade() {
  return ChannelLinux::KernelSupportsUpgradeRequirements() &&
         ChannelLinux::UpgradesEnabled();
}

void Channel::OfferChannelUpgrade() {
  if (!SupportsChannelUpgrade()) {
    return;
  }
  static_cast<ChannelLinux*>(this)->OfferSharedMemUpgrade();
}
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) ||
        // BUILDFLAG(IS_ANDROID)
#endif  // !BUILDFLAG(IS_NACL)

}  // namespace mojo::core
