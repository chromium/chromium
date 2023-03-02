// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/ipc_channel_nacl.h"

#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#include <algorithm>
#include <memory>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_pump_for_io.h"
#include "base/ranges/algorithm.h"
#include "base/synchronization/lock.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/simple_thread.h"
#include "base/trace_event/trace_event.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_logging.h"
#include "ipc/ipc_message_attachment_set.h"
#include "ipc/ipc_platform_file_attachment_posix.h"
#include "native_client/src/public/imc_syscalls.h"
#include "native_client/src/public/imc_types.h"

namespace IPC {

struct MessageContents {
  std::vector<char> data;
  std::vector<int> fds;
};

namespace {

bool ReadDataOnReaderThread(int pipe, MessageContents* contents) {
  DCHECK(pipe >= 0);
  if (pipe < 0)
    return false;

  contents->data.resize(Channel::kReadBufferSize);
  contents->fds.resize(NACL_ABI_IMC_DESC_MAX);

  NaClAbiNaClImcMsgIoVec iov = { &contents->data[0], contents->data.size() };
  NaClAbiNaClImcMsgHdr msg = {
    &iov, 1, &contents->fds[0], contents->fds.size()
  };

  int bytes_read = imc_recvmsg(pipe, &msg, 0);

  if (bytes_read <= 0) {
    // NaClIPCAdapter::BlockingReceive returns -1 when the pipe closes (either
    // due to error or for regular shutdown).
    contents->data.clear();
    contents->fds.clear();
    return false;
  }
  DCHECK(bytes_read);
  // Resize the buffers down to the number of bytes and fds we actually read.
  contents->data.resize(bytes_read);
  contents->fds.resize(msg.desc_length);
  return true;
}

}  // namespace

// static
constexpr size_t Channel::kMaximumMessageSize;

class ChannelNacl::ReaderThreadRunner
    : public base::DelegateSimpleThread::Delegate {
 public:
  // |pipe|: A file descriptor from which we will read using imc_recvmsg.
  // |data_read_callback|: A callback we invoke (on the main thread) when we
  //                       have read data.
  // |failure_callback|: A callback we invoke when we have a failure reading
  //                     from |pipe|.
  // |main_message_loop|: A proxy for the main thread, where we will invoke the
  //                      above callbacks.
  ReaderThreadRunner(
      int pipe,
      base::RepeatingCallback<void(std::unique_ptr<MessageContents>)>
          data_read_callback,
      base::RepeatingCallback<void()> failure_callback,
      scoped_refptr<base::SingleThreadTaskRunner> main_task_runner);

  ReaderThreadRunner(const ReaderThreadRunner&) = delete;
  ReaderThreadRunner& operator=(const ReaderThreadRunner&) = delete;

  // DelegateSimpleThread implementation. Reads data from the pipe in a loop
  // until either we are told to quit or a read fails.
  void Run() override;

 private:
  int pipe_;
  base::RepeatingCallback<void(std::unique_ptr<MessageContents>)>
      data_read_callback_;
  base::RepeatingCallback<void()> failure_callback_;
  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;
};

ChannelNacl::ReaderThreadRunner::ReaderThreadRunner(
    int pipe,
    base::RepeatingCallback<void(std::unique_ptr<MessageContents>)>
        data_read_callback,
    base::RepeatingCallback<void()> failure_callback,
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner)
    : pipe_(pipe),
      data_read_callback_(data_read_callback),
      failure_callback_(failure_callback),
      main_task_runner_(main_task_runner) {}

void ChannelNacl::ReaderThreadRunner::Run() {
  while (true) {
    std::unique_ptr<MessageContents> msg_contents(new MessageContents);
    bool success = ReadDataOnReaderThread(pipe_, msg_contents.get());
    if (success) {
      main_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(data_read_callback_, std::move(msg_contents)));
    } else {
      main_task_runner_->PostTask(FROM_HERE, failure_callback_);
      // Because the read failed, we know we're going to quit. Don't bother
      // trying to read again.
      return;
    }
  }
}

ChannelNacl::ChannelNacl(const IPC::ChannelHandle& channel_handle,
                         Mode mode,
                         Listener* listener)
    : ChannelReader(listener),
      mode_(mode),
      waiting_connect_(true),
      pipe_(-1),
      weak_ptr_factory_(this) {
  if (!CreatePipe(channel_handle)) {
    // The pipe may have been closed already.
    const char *modestr = (mode_ & MODE_SERVER_FLAG) ? "server" : "client";
    LOG(WARNING) << "Unable to create pipe in " << modestr << " mode";
  }
}

ChannelNacl::~ChannelNacl() {
  CleanUp();
  Close();
}

bool ChannelNacl::Connect() {
  WillConnect();

  if (pipe_ == -1) {
    DLOG(WARNING) << "Channel creation failed";
    return false;
  }

  // Note that Connect is called on the "Channel" thread (i.e., the same thread
  // where Channel::Send will be called, and the same thread that should receive
  // messages). The constructor might be invoked on another thread (see
  // ChannelProxy for an example of that). Therefore, we must wait until Connect
  // is called to decide which SingleThreadTaskRunner to pass to
  // ReaderThreadRunner.
  reader_thread_runner_ = std::make_unique<ReaderThreadRunner>(
      pipe_,
      base::BindRepeating(&ChannelNacl::DidRecvMsg,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindRepeating(&ChannelNacl::ReadDidFail,
                          weak_ptr_factory_.GetWeakPtr()),
      base::SingleThreadTaskRunner::GetCurrentDefault());
  reader_thread_ = std::make_unique<base::DelegateSimpleThread>(
      reader_thread_runner_.get(), "ipc_channel_nacl reader thread");
  reader_thread_->Start();
  waiting_connect_ = false;
  // If there were any messages queued before connection, send them.
  ProcessOutgoingMessages();
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&ChannelNacl::CallOnChannelConnected,
                                weak_ptr_factory_.GetWeakPtr()));

  return true;
}

void ChannelNacl::Close() {
  // For now, we assume that at shutdown, the reader thread will be woken with
  // a failure (see NaClIPCAdapter::BlockingRead and CloseChannel). Or... we
  // might simply be killed with no chance to clean up anyway :-).
  // If untrusted code tries to close the channel prior to shutdown, it's likely
  // to hang.
  // TODO(dmichael): Can we do anything smarter here to make sure the reader
  //                 thread wakes up and quits?
  reader_thread_->Join();
  close(pipe_);
  pipe_ = -1;
  reader_thread_runner_.reset();
  reader_thread_.reset();
  read_queue_.clear();
  output_queue_.clear();
}

bool ChannelNacl::Send(Message* message) {
  DCHECK(!message->HasAttachments());
  DVLOG(2) << "sending message @" << message << " on channel @" << this
           << " with type " << message->type();
  std::unique_ptr<Message> message_ptr(message);

#if BUILDFLAG(IPC_MESSAGE_LOG_ENABLED)
  Logging::GetInstance()->OnSendMessage(message_ptr.get());
#endif  // BUILDFLAG(IPC_MESSAGE_LOG_ENABLED)

  TRACE_EVENT_WITH_FLOW0("toplevel.flow", "ChannelNacl::Send",
                         message->header()->flags, TRACE_EVENT_FLAG_FLOW_OUT);
  output_queue_.push_back(std::move(message_ptr));
  if (!waiting_connect_)
    return ProcessOutgoingMessages();

  return true;
}

void ChannelNacl::DidRecvMsg(std::unique_ptr<MessageContents> contents) {
  // Close sets the pipe to -1. It's possible we'll get a buffer sent to us from
  // the reader thread after Close is called. If so, we ignore it.
  if (pipe_ == -1)
    return;

  auto data = std::make_unique<std::vector<char>>();
  data->swap(contents->data);
  read_queue_.push_back(std::move(data));

  input_attachments_.reserve(contents->fds.size());
  for (int fd : contents->fds) {
    input_attachments_.push_back(
        new internal::PlatformFileAttachment(base::ScopedFD(fd)));
  }
  contents->fds.clear();

  // In POSIX, we would be told when there are bytes to read by implementing
  // OnFileCanReadWithoutBlocking in MessagePumpForIO::FdWatcher. In NaCl, we
  // instead know at this point because the reader thread posted some data to
  // us.
  ProcessIncomingMessages();
}

void ChannelNacl::ReadDidFail() {
  Close();
}

bool ChannelNacl::CreatePipe(
    const IPC::ChannelHandle& channel_handle) {
  DCHECK(pipe_ == -1);

  // There's one possible case in NaCl:
  // 1) It's a channel wrapping a pipe that is given to us.
  // We don't support these:
  // 2) It's for a named channel.
  // 3) It's for a client that we implement ourself.
  // 4) It's the initial IPC channel.

  if (channel_handle.socket.fd == -1) {
    NOTIMPLEMENTED();
    return false;
  }
  pipe_ = channel_handle.socket.fd;
  return true;
}

bool ChannelNacl::ProcessOutgoingMessages() {
  DCHECK(!waiting_connect_);  // Why are we trying to send messages if there's
                              // no connection?
  if (output_queue_.empty())
    return true;

  if (pipe_ == -1)
    return false;

  // Write out all the messages. The trusted implementation is guaranteed to not
  // block. See NaClIPCAdapter::Send for the implementation of imc_sendmsg.
  while (!output_queue_.empty()) {
    std::unique_ptr<Message> msg = std::move(output_queue_.front());
    output_queue_.pop_front();

    const size_t num_fds = msg->attachment_set()->size();
    DCHECK(num_fds <= MessageAttachmentSet::kMaxDescriptorsPerMessage);
    std::vector<int> fds;
    fds.reserve(num_fds);
    for (size_t i = 0; i < num_fds; i++) {
      scoped_refptr<MessageAttachment> attachment =
          msg->attachment_set()->GetAttachmentAt(i);
      DCHECK_EQ(MessageAttachment::Type::PLATFORM_FILE, attachment->GetType());
      fds.push_back(static_cast<internal::PlatformFileAttachment&>(*attachment)
                        .TakePlatformFile());
    }

    NaClAbiNaClImcMsgIoVec iov = {const_cast<uint8_t*>(msg->data()),
                                  msg->size()};
    NaClAbiNaClImcMsgHdr msgh = {&iov, 1, fds.data(), num_fds};
    ssize_t bytes_written = imc_sendmsg(pipe_, &msgh, 0);

    DCHECK(bytes_written);  // The trusted side shouldn't return 0.
    if (bytes_written < 0) {
      // The trusted side should only ever give us an error of EPIPE. We
      // should never be interrupted, nor should we get EAGAIN.
      DCHECK(errno == EPIPE);
      Close();
      PLOG(ERROR) << "pipe_ error on "
                  << pipe_
                  << " Currently writing message of size: "
                  << msg->size();
      return false;
    } else {
      msg->attachment_set()->CommitAllDescriptors();
    }

    // Message sent OK!
    DVLOG(2) << "sent message @" << msg.get() << " with type " << msg->type()
             << " on fd " << pipe_;
  }
  return true;
}

void ChannelNacl::CallOnChannelConnected() {
  listener()->OnChannelConnected(-1);
}

ChannelNacl::ReadState ChannelNacl::ReadData(
    char* buffer,
    int buffer_len,
    int* bytes_read) {
  *bytes_read = 0;
  if (pipe_ == -1)
    return READ_FAILED;
  if (read_queue_.empty())
    return READ_PENDING;
  while (!read_queue_.empty() && *bytes_read < buffer_len) {
    std::vector<char>* vec = read_queue_.front().get();
    size_t bytes_to_read = buffer_len - *bytes_read;
    if (vec->size() <= bytes_to_read) {
      // We can read and discard the entire vector.
      base::ranges::copy(*vec, buffer + *bytes_read);
      *bytes_read += vec->size();
      read_queue_.pop_front();
    } else {
      // Read all the bytes we can and discard them from the front of the
      // vector. (This can be slowish, since erase has to move the back of the
      // vector to the front, but it's hopefully a temporary hack and it keeps
      // the code simple).
      std::copy(vec->begin(), vec->begin() + bytes_to_read,
                buffer + *bytes_read);
      vec->erase(vec->begin(), vec->begin() + bytes_to_read);
      *bytes_read += bytes_to_read;
    }
  }
  return READ_SUCCEEDED;
}

bool ChannelNacl::ShouldDispatchInputMessage(Message* msg) {
  return true;
}

bool ChannelNacl::GetAttachments(Message* msg) {
  uint16_t header_fds = msg->header()->num_fds;
  CHECK(header_fds == input_attachments_.size());
  if (header_fds == 0)
    return true;  // Nothing to do.

  for (auto& attachment : input_attachments_) {
    msg->attachment_set()->AddAttachment(std::move(attachment));
  }
  input_attachments_.clear();
  return true;
}

bool ChannelNacl::DidEmptyInputBuffers() {
  // When the input data buffer is empty, the attachments should be too.
  return input_attachments_.empty();
}

void ChannelNacl::HandleInternalMessage(const Message& msg) {
  // The trusted side IPC::Channel should handle the "hello" handshake; we
  // should not receive the "Hello" message.
  NOTREACHED();
}

// Channel's methods

// static
std::unique_ptr<Channel> Channel::Create(
    const IPC::ChannelHandle& channel_handle,
    Mode mode,
    Listener* listener) {
  return std::make_unique<ChannelNacl>(channel_handle, mode, listener);
}

}  // namespace IPC
