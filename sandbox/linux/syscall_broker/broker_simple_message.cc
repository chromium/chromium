// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/linux/syscall_broker/broker_simple_message.h"

#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/files/scoped_file.h"
#include "base/notreached.h"
#include "base/numerics/safe_math.h"
#include "base/posix/eintr_wrapper.h"
#include "base/posix/unix_domain_socket.h"
#include "base/process/process_handle.h"
#include "build/build_config.h"

// Macro for suppressing clang's warning about variable-length arrays.
// For some reason the formatter gives very silly results here.
// clang-format off
#define ALLOW_VLA(line)                                 \
  _Pragma("GCC diagnostic push")                        \
  _Pragma("GCC diagnostic ignored \"-Wvla-extension\"") \
  line                                                  \
  _Pragma("GCC diagnostic pop")
// clang-format on

namespace sandbox {

namespace syscall_broker {

ssize_t BrokerSimpleMessage::SendRecvMsgWithFlags(int fd,
                                                  int recvmsg_flags,
                                                  base::ScopedFD* result_fd,
                                                  BrokerSimpleMessage* reply) {
  return SendRecvMsgWithFlagsMultipleFds(fd, recvmsg_flags, {},
                                         UNSAFE_TODO({result_fd, 1u}), reply);
}

ssize_t BrokerSimpleMessage::SendRecvMsgWithFlagsMultipleFds(
    int fd,
    int recvmsg_flags,
    base::span<const int> send_fds,
    base::span<base::ScopedFD> result_fds,
    BrokerSimpleMessage* reply) {
  RAW_CHECK(reply);
  RAW_CHECK(send_fds.size() + 1 <= base::UnixDomainSocket::kMaxFileDescriptors);

  // This socketpair is only used for the IPC and is cleaned up before
  // returning.
  base::ScopedFD recv_sock;
  base::ScopedFD send_sock;
  if (!base::CreateSocketPair(&recv_sock, &send_sock))
    return -1;

  // The length of this array is actually hardcoded, but the compiler isn't
  // smart enough to figure that out.
  ALLOW_VLA(int send_fds_with_reply_socket
                [base::UnixDomainSocket::kMaxFileDescriptors];)
  send_fds_with_reply_socket[0] = send_sock.get();
  for (size_t i = 0; i < send_fds.size(); i++) {
    UNSAFE_TODO(send_fds_with_reply_socket[i + 1]) = send_fds[i];
  }
  if (!SendMsgMultipleFds(
          fd, UNSAFE_TODO({send_fds_with_reply_socket, send_fds.size() + 1}))) {
    return -1;
  }

  // Close the sending end of the socket right away so that if our peer closes
  // it before sending a response (e.g., from exiting), RecvMsgWithFlags() will
  // return EOF instead of hanging.
  send_sock.reset();

  const ssize_t reply_len = reply->RecvMsgWithFlagsMultipleFds(
      recv_sock.get(), recvmsg_flags, result_fds);
  recv_sock.reset();
  if (reply_len == -1)
    return -1;

  return reply_len;
}

bool BrokerSimpleMessage::SendMsg(int fd, int send_fd) {
  return SendMsgMultipleFds(
      fd, send_fd == -1 ? base::span<int>()
                        : UNSAFE_TODO(base::span<int>(&send_fd, 1u)));
}

bool BrokerSimpleMessage::SendMsgMultipleFds(int fd,
                                             base::span<const int> send_fds) {
  if (broken_)
    return false;

  RAW_CHECK(send_fds.size() <= base::UnixDomainSocket::kMaxFileDescriptors);

  struct msghdr msg = {};
  const void* buf = reinterpret_cast<const void*>(message_.data());
  struct iovec iov = {const_cast<void*>(buf), length_};
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;

  const unsigned control_len = CMSG_SPACE(send_fds.size() * sizeof(int));
  // The RAW_CHECK above ensures that send_fds.size() is bounded by a constant,
  // so the length of this array is bounded as well.
  ALLOW_VLA(char control_buffer[control_len];)
  if (send_fds.size() >= 1) {
    struct cmsghdr* cmsg;
    msg.msg_control = control_buffer;
    msg.msg_controllen = control_len;
    cmsg = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    int len = 0;

    for (size_t i = 0; i < send_fds.size(); i++) {
      if (send_fds[i] < 0)
        return false;

      // CMSG_DATA() not guaranteed to be aligned so this must use memcpy.
      UNSAFE_TODO(memcpy(CMSG_DATA(cmsg) + (sizeof(int) * i), &send_fds[i],
                         sizeof(int)));
      len += sizeof(int);
    }
    cmsg->cmsg_len = CMSG_LEN(len);
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
  ssize_t ret = RecvMsgWithFlagsMultipleFds(
      fd, flags, UNSAFE_TODO(base::span<base::ScopedFD>(return_fd, 1u)));
  return ret;
}

ssize_t BrokerSimpleMessage::RecvMsgWithFlagsMultipleFds(
    int fd,
    int flags,
    base::span<base::ScopedFD> return_fds) {
  // The message must be fresh and unused.
  RAW_CHECK(!read_only_ && !write_only_);
  RAW_CHECK(return_fds.size() <= base::UnixDomainSocket::kMaxFileDescriptors);
  read_only_ = true;  // The message should not be written to again.
  struct msghdr msg = {};
  struct iovec iov = {message_.data(), message_.size()};
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;

  const size_t kControlBufferSize =
      CMSG_SPACE(sizeof(fd) * base::UnixDomainSocket::kMaxFileDescriptors) +
      CMSG_SPACE(sizeof(struct ucred));

  // The length of this array is actually a constant, but the compiler isn't
  // smart enough to figure that out.
  ALLOW_VLA(char control_buffer[kControlBufferSize];)
  msg.msg_control = control_buffer;
  msg.msg_controllen = sizeof(control_buffer);

  const ssize_t r = HANDLE_EINTR(recvmsg(fd, &msg, flags));
  if (r == -1)
    return -1;

  base::span<int> wire_fds;
  base::ProcessId pid = -1;

  if (msg.msg_controllen > 0) {
    struct cmsghdr* cmsg;
    for (cmsg = CMSG_FIRSTHDR(&msg); cmsg; cmsg = CMSG_NXTHDR(&msg, cmsg)) {
      const size_t payload_len = cmsg->cmsg_len - CMSG_LEN(0);
      if (cmsg->cmsg_level == SOL_SOCKET && cmsg->cmsg_type == SCM_RIGHTS) {
        DCHECK_EQ(payload_len % sizeof(fd), 0u);
        DCHECK(wire_fds.empty());
        // SAFETY: `CMSG_DATA()` Control message API; accesses
        // the data portion of the control message header cmsg. payload_len
        // comes from cmsg_len which specifies the length of the data held by
        // the control message. See:
        // https://man.openbsd.org/CMSG_DATA.3#CMSG_DATA
        wire_fds = UNSAFE_BUFFERS(base::span<int>(
            reinterpret_cast<int*>(CMSG_DATA(cmsg)), payload_len / sizeof(fd)));
        DCHECK_GE(wire_fds.data(), reinterpret_cast<int*>(&control_buffer[0]));
        DCHECK_LE(&wire_fds.back(),
                  UNSAFE_BUFFERS(reinterpret_cast<int*>(
                      &control_buffer[kControlBufferSize - 1])));
      }
      if (cmsg->cmsg_level == SOL_SOCKET &&
          cmsg->cmsg_type == SCM_CREDENTIALS) {
        DCHECK_EQ(payload_len, sizeof(struct ucred));
        DCHECK_EQ(pid, -1);
        pid = reinterpret_cast<struct ucred*>(CMSG_DATA(cmsg))->pid;
      }
    }
  }

  if (msg.msg_flags & MSG_TRUNC || msg.msg_flags & MSG_CTRUNC) {
    for (int wire_fd : wire_fds) {
      close(wire_fd);
    }
    errno = EMSGSIZE;
    return -1;
  }

  if (!wire_fds.empty()) {
    if (wire_fds.size() > return_fds.size()) {
      // The number of fds received is limited to return_fds.size(). If there
      // are more in the message than expected, close them and return an error.
      for (int wire_fd : wire_fds) {
        close(wire_fd);
      }
      errno = EMSGSIZE;
      return -1;
    }

    for (size_t i = 0; i < wire_fds.size(); i++) {
      return_fds[i] = base::ScopedFD(wire_fds[i]);
    }
  }

  // At this point, |r| is guaranteed to be >= 0.
  length_ = static_cast<size_t>(r);
  return r;
}

UNSAFE_BUFFER_USAGE
bool BrokerSimpleMessage::AddStringToMessage(const char* string) {
  // SAFETY: `string` has to be a valid null-terminated C string provided by the
  // caller. `strlen(string) + 1` to correctly calculate the length including
  // the null terminator '\0', ensuring the `base::span` covers the entire
  // string.
  auto data_span = UNSAFE_TODO(
      base::as_bytes(base::span<const char>(string, strlen(string) + 1)));
  return AddDataToMessage(data_span);
}

bool BrokerSimpleMessage::AddDataToMessage(base::span<const uint8_t> data) {
  if (read_only_ || broken_)
    return false;

  write_only_ = true;  // Message should only be written to going forward.

  size_t length = data.size();
  base::CheckedNumeric<size_t> safe_length(length_);
  safe_length += sizeof(EntryType);
  safe_length += sizeof(length);
  safe_length += length;

  if (safe_length.ValueOrDie() > message_.size()) {
    broken_ = true;
    return false;
  }

  EntryType type = EntryType::DATA;
  WriteBytes(base::byte_span_from_ref(type));
  WriteBytes(base::byte_span_from_ref(length));
  WriteBytes(data);

  return true;
}

bool BrokerSimpleMessage::AddIntToMessage(int data) {
  if (read_only_ || broken_)
    return false;

  write_only_ = true;  // Message should only be written to going forward.

  base::CheckedNumeric<size_t> safe_length(length_);
  safe_length += sizeof(EntryType);
  safe_length += sizeof(data);

  if (!safe_length.IsValid() || safe_length.ValueOrDie() > message_.size()) {
    broken_ = true;
    return false;
  }

  EntryType type = EntryType::INT;
  WriteBytes(base::byte_span_from_ref(type));
  WriteBytes(base::byte_span_from_ref(data));

  return true;
}

bool BrokerSimpleMessage::ReadString(const char** data) {
  size_t str_len;
  bool result = ReadData(data, &str_len);
  return result && UNSAFE_TODO((*data)[str_len - 1]) == '\0';
}

bool BrokerSimpleMessage::ReadData(const char** data, size_t* length) {
  if (write_only_ || broken_)
    return false;

  read_only_ = true;  // Message should not be written to.
  if (read_next_offset_ > length_) {
    broken_ = true;
    return false;
  }

  if (!ValidateType(EntryType::DATA)) {
    broken_ = true;
    return false;
  }

  // Get the length of the data buffer from the message.
  auto length_span = base::byte_span_from_ref(*length);
  if (read_next_offset_ + length_span.size() > length_) {
    broken_ = true;
    return false;
  }

  ReadBytes(length_span);

  // Get the raw data buffer from the message.
  if (read_next_offset_ + *length > length_) {
    broken_ = true;
    return false;
  }

  *data = reinterpret_cast<char*>(&message_[read_next_offset_]);
  read_next_offset_ += *length;
  return true;
}

bool BrokerSimpleMessage::ReadInt(int* result) {
  if (write_only_ || broken_)
    return false;

  read_only_ = true;  // Message should not be written to.
  if (read_next_offset_ > length_) {
    broken_ = true;
    return false;
  }

  if (!ValidateType(EntryType::INT)) {
    broken_ = true;
    return false;
  }

  size_t result_size = sizeof(*result);
  if (read_next_offset_ + result_size > length_) {
    broken_ = true;
    return false;
  }

  ReadBytes(base::byte_span_from_ref(*result));
  return true;
}

void BrokerSimpleMessage::ReadBytes(base::span<uint8_t> dest) {
  DCHECK_LE(read_next_offset_ + dest.size(), message_.size());
  dest.copy_from_nonoverlapping(
      base::span(message_).subspan(read_next_offset_, dest.size()));
  read_next_offset_ += dest.size();
}

bool BrokerSimpleMessage::ValidateType(EntryType expected_type) {
  EntryType type;
  auto type_span = base::byte_span_from_ref(type);
  if (read_next_offset_ + type_span.size() > length_) {
    return false;
  }
  ReadBytes(type_span);
  if (type != expected_type)
    return false;

  return true;
}

void BrokerSimpleMessage::WriteBytes(base::span<const uint8_t> bytes) {
  DCHECK_LT(write_next_offset_ + bytes.size(), message_.size());
  base::span(message_)
      .subspan(write_next_offset_, bytes.size())
      .copy_from_nonoverlapping(bytes);
  write_next_offset_ += bytes.size();
  length_ = write_next_offset_;
}

}  // namespace syscall_broker

}  // namespace sandbox
