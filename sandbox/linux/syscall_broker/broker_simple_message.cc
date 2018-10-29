// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/linux/syscall_broker/broker_simple_message.h"

#include <errno.h>
#include <sys/socket.h>
#include <unistd.h>

#include "base/files/scoped_file.h"
#include "base/logging.h"
#include "base/numerics/safe_math.h"
#include "base/posix/eintr_wrapper.h"
#include "base/posix/unix_domain_socket.h"
#include "base/process/process_handle.h"
#include "build/build_config.h"

namespace sandbox {

namespace syscall_broker {

BrokerSimpleMessage::BrokerSimpleMessage()
    : read_only_(false),
      write_only_(false),
      broken_(false),
      length_(0),
      read_next_(message_),
      write_next_(message_) {}

ssize_t BrokerSimpleMessage::SendRecvMsgWithFlags(int fd,
                                                  int recvmsg_flags,
                                                  int* result_fd,
                                                  BrokerSimpleMessage* reply) {
  RAW_CHECK(reply);

  // This socketpair is only used for the IPC and is cleaned up before
  // returning.
  base::ScopedFD recv_sock;
  base::ScopedFD send_sock;
  if (!base::CreateSocketPair(&recv_sock, &send_sock))
    return -1;

  if (!SendMsg(fd, send_sock.get()))
    return -1;

  // Close the sending end of the socket right away so that if our peer closes
  // it before sending a response (e.g., from exiting), RecvMsgWithFlags() will
  // return EOF instead of hanging.
  send_sock.reset();

  base::ScopedFD recv_fd;
  const ssize_t reply_len =
      reply->RecvMsgWithFlags(recv_sock.get(), recvmsg_flags, &recv_fd);
  recv_sock.reset();
  if (reply_len == -1)
    return -1;

  if (result_fd)
    *result_fd = (recv_fd == -1) ? -1 : recv_fd.release();

  return reply_len;
}

bool BrokerSimpleMessage::SendMsg(int fd, int send_fd) {
  if (broken_)
    return false;

  struct msghdr msg = {};
  const void* buf = reinterpret_cast<const void*>(message_);
  struct iovec iov = {const_cast<void*>(buf), length_};
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;

  const unsigned control_len = CMSG_SPACE(sizeof(send_fd));
  char control_buffer[control_len];
  if (send_fd >= 0) {
    struct cmsghdr* cmsg;
    msg.msg_control = control_buffer;
    msg.msg_controllen = control_len;
    cmsg = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    cmsg->cmsg_len = CMSG_LEN(sizeof(send_fd));
    memcpy(CMSG_DATA(cmsg), &send_fd, sizeof(send_fd));
    msg.msg_controllen = cmsg->cmsg_len;
  }

  // Avoid a SIGPIPE if the other end breaks the connection.
  // Due to a bug in the Linux kernel (net/unix/af_unix.c) MSG_NOSIGNAL isn't
  // regarded for SOCK_SEQPACKET in the AF_UNIX domain, but it is mandated by
  // POSIX.
  const int flags = MSG_NOSIGNAL;
  const ssize_t r = HANDLE_EINTR(sendmsg(fd, &msg, flags));
  return static_cast<ssize_t>(length_) == r;
}

ssize_t BrokerSimpleMessage::RecvMsgWithFlags(int fd,
                                              int flags,
                                              base::ScopedFD* return_fd) {
  // The message must be fresh and unused.
  RAW_CHECK(!read_only_ && !write_only_);
  read_only_ = true;  // The message should not be written to again.
  struct msghdr msg = {};
  struct iovec iov = {message_, kMaxMessageLength};
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;

#if defined(OS_NACL_NONSFI)
  const size_t kControlBufferSize =
      CMSG_SPACE(sizeof(fd) * base::UnixDomainSocket::kMaxFileDescriptors);
#else
  const size_t kControlBufferSize =
      CMSG_SPACE(sizeof(fd) * base::UnixDomainSocket::kMaxFileDescriptors) +
      // The PNaCl toolchain for Non-SFI binary build does not support ucred.
      CMSG_SPACE(sizeof(struct ucred));
#endif  // defined(OS_NACL_NONSFI)

  char control_buffer[kControlBufferSize];
  msg.msg_control = control_buffer;
  msg.msg_controllen = sizeof(control_buffer);

  const ssize_t r = HANDLE_EINTR(recvmsg(fd, &msg, flags));
  if (r == -1)
    return -1;

  int* wire_fds = NULL;
  size_t wire_fds_len = 0;
  base::ProcessId pid = -1;

  if (msg.msg_controllen > 0) {
    struct cmsghdr* cmsg;
    for (cmsg = CMSG_FIRSTHDR(&msg); cmsg; cmsg = CMSG_NXTHDR(&msg, cmsg)) {
      const size_t payload_len = cmsg->cmsg_len - CMSG_LEN(0);
      if (cmsg->cmsg_level == SOL_SOCKET && cmsg->cmsg_type == SCM_RIGHTS) {
        DCHECK_EQ(payload_len % sizeof(fd), 0u);
        DCHECK_EQ(wire_fds, static_cast<void*>(nullptr));
        wire_fds = reinterpret_cast<int*>(CMSG_DATA(cmsg));
        wire_fds_len = payload_len / sizeof(fd);
      }
#if !defined(OS_NACL_NONSFI)
      // The PNaCl toolchain for Non-SFI binary build does not support
      // SCM_CREDENTIALS.
      if (cmsg->cmsg_level == SOL_SOCKET &&
          cmsg->cmsg_type == SCM_CREDENTIALS) {
        DCHECK_EQ(payload_len, sizeof(struct ucred));
        DCHECK_EQ(pid, -1);
        pid = reinterpret_cast<struct ucred*>(CMSG_DATA(cmsg))->pid;
      }
#endif
    }
  }

  if (msg.msg_flags & MSG_TRUNC || msg.msg_flags & MSG_CTRUNC) {
    for (size_t i = 0; i < wire_fds_len; ++i) {
      close(wire_fds[i]);
    }
    errno = EMSGSIZE;
    return -1;
  }

  if (wire_fds) {
    if (wire_fds_len > 1) {
      // Only one FD is accepted by this receive.
      for (unsigned i = 0; i < wire_fds_len; ++i) {
        close(wire_fds[i]);
      }
      errno = EMSGSIZE;
      NOTREACHED();
      return -1;
    }

    *return_fd = base::ScopedFD(wire_fds[0]);
  }

  // At this point, |r| is guaranteed to be >= 0.
  length_ = static_cast<size_t>(r);
  return r;
}

bool BrokerSimpleMessage::AddStringToMessage(const char* string) {
  // strlen() + 1 to always include the '\0' terminating character.
  return AddDataToMessage(string, strlen(string) + 1);
}

bool BrokerSimpleMessage::AddDataToMessage(const char* data, size_t length) {
  if (read_only_ || broken_)
    return false;

  write_only_ = true;  // Message should only be written to going forward.

  base::CheckedNumeric<size_t> safe_length(length);
  safe_length += length_;
  safe_length += sizeof(EntryType);
  safe_length += sizeof(length);

  if (safe_length.ValueOrDie() > kMaxMessageLength) {
    broken_ = true;
    return false;
  }

  EntryType type = EntryType::DATA;

  // Write the type to the message
  memcpy(write_next_, &type, sizeof(EntryType));
  write_next_ += sizeof(EntryType);
  // Write the length of the buffer to the message
  memcpy(write_next_, &length, sizeof(length));
  write_next_ += sizeof(length);
  // Write the data in the buffer to the message
  memcpy(write_next_, data, length);
  write_next_ += length;
  length_ = write_next_ - message_;

  return true;
}

bool BrokerSimpleMessage::AddIntToMessage(int data) {
  if (read_only_ || broken_)
    return false;

  write_only_ = true;  // Message should only be written to going forward.

  base::CheckedNumeric<size_t> safe_length(length_);
  safe_length += sizeof(data);
  safe_length += sizeof(EntryType);

  if (!safe_length.IsValid() || safe_length.ValueOrDie() > kMaxMessageLength) {
    broken_ = true;
    return false;
  }

  EntryType type = EntryType::INT;

  memcpy(write_next_, &type, sizeof(EntryType));
  write_next_ += sizeof(EntryType);
  memcpy(write_next_, &data, sizeof(data));
  write_next_ += sizeof(data);
  length_ = write_next_ - message_;

  return true;
}

bool BrokerSimpleMessage::ReadString(const char** data) {
  size_t str_len;
  bool result = ReadData(data, &str_len);
  return result && (*data)[str_len - 1] == '\0';
}

bool BrokerSimpleMessage::ReadData(const char** data, size_t* length) {
  if (write_only_ || broken_)
    return false;

  read_only_ = true;  // Message should not be written to.
  if (read_next_ > (message_ + length_)) {
    broken_ = true;
    return false;
  }

  if (!ValidateType(EntryType::DATA)) {
    broken_ = true;
    return false;
  }

  // Get the length of the data buffer from the message.
  if ((read_next_ + sizeof(size_t)) > (message_ + length_)) {
    broken_ = true;
    return false;
  }
  memcpy(length, read_next_, sizeof(size_t));
  read_next_ = read_next_ + sizeof(size_t);

  // Get the raw data buffer from the message.
  if ((read_next_ + *length) > (message_ + length_)) {
    broken_ = true;
    return false;
  }
  *data = reinterpret_cast<char*>(read_next_);
  read_next_ = read_next_ + *length;
  return true;
}

bool BrokerSimpleMessage::ReadInt(int* result) {
  if (write_only_ || broken_)
    return false;

  read_only_ = true;  // Message should not be written to.
  if (read_next_ > (message_ + length_)) {
    broken_ = true;
    return false;
  }

  if (!ValidateType(EntryType::INT)) {
    broken_ = true;
    return false;
  }

  if ((read_next_ + sizeof(*result)) > (message_ + length_)) {
    broken_ = true;
    return false;
  }
  memcpy(result, read_next_, sizeof(*result));
  read_next_ = read_next_ + sizeof(*result);
  return true;
}

bool BrokerSimpleMessage::ValidateType(EntryType expected_type) {
  if ((read_next_ + sizeof(EntryType)) > (message_ + length_))
    return false;

  EntryType type;
  memcpy(&type, read_next_, sizeof(EntryType));
  if (type != expected_type)
    return false;

  read_next_ = read_next_ + sizeof(EntryType);
  return true;
}

}  // namespace syscall_broker

}  // namespace sandbox
