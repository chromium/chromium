// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_LINUX_SYSCALL_BROKER_BROKER_SIMPLE_MESSAGE_H_
#define SANDBOX_LINUX_SYSCALL_BROKER_BROKER_SIMPLE_MESSAGE_H_

#include <stdint.h>
#include <sys/types.h>

#include "base/containers/span.h"
#include "base/files/scoped_file.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "sandbox/sandbox_export.h"

namespace sandbox {
namespace syscall_broker {

// This class is meant to provide a very simple messaging mechanism that is
// signal-safe for the broker to utilize. This addresses many of the issues
// outlined in https://crbug.com/255063. In short, the use of the standard
// base::UnixDomainSockets is not possible because it uses base::Pickle and
// std::vector, which are not signal-safe.
//
// In implementation, much of the code for sending and receiving is taken from
// base::UnixDomainSockets and re-used below. Thus, ultimately, it might be
// worthwhile making a first-class base-supported signal-safe set of mechanisms
// that reduces the code duplication.
class SANDBOX_EXPORT BrokerSimpleMessage {
 public:
  BrokerSimpleMessage() = default;

  // Signal-safe
  // A synchronous version of SendMsg/RecvMsgWithFlags that creates and sends a
  // temporary IPC socket over |fd|, then listens for a response on the IPC
  // socket using reply->RecvMsgWithFlags(temporary_ipc_socket, recvmsg_flags,
  // result_fd);
  ssize_t SendRecvMsgWithFlags(int fd,
                               int recvmsg_flags,
                               base::ScopedFD* result_fd,
                               BrokerSimpleMessage* reply);

  // Same as SendRecvMsgWithFlags(), but allows sending and receiving a variable
  // number of fds. The temporary IPC return socket is always sent as the first
  // fd in the cmsg.
  ssize_t SendRecvMsgWithFlagsMultipleFds(int fd,
                                          int recvmsg_flags,
                                          base::span<const int> send_fds,
                                          base::span<base::ScopedFD> result_fds,
                                          BrokerSimpleMessage* reply);

  // Use sendmsg to write the given msg and the file descriptor |send_fd|.
  // Returns true if successful. Signal-safe.
  bool SendMsg(int fd, int send_fd);

  // Same as SendMsg() but allows sending more than one fd.
  bool SendMsgMultipleFds(int fd, base::span<const int> send_fds);

  // Similar to RecvMsg, but allows to specify |flags| for recvmsg(2).
  // Guaranteed to return either 1 or 0 fds. Signal-safe.
  ssize_t RecvMsgWithFlags(int fd, int flags, base::ScopedFD* return_fd);

  // Same as RecvMsgWithFlags() but allows receiving more than one fd.
  ssize_t RecvMsgWithFlagsMultipleFds(int fd,
                                      int flags,
                                      base::span<base::ScopedFD> return_fds);

  // Adds a NUL-terminated C-style string to the message as a raw buffer.
  // Returns true if the internal message buffer has room for the data, and
  // the data is successfully appended.
  bool AddStringToMessage(const char* string);

  // Adds a raw data buffer to the message. If the raw data is actually a
  // string, be sure to have length explicitly include the '\0' terminating
  // character. Returns true if the internal message buffer has room for the
  // data, and the data is successfully appended.
  bool AddDataToMessage(const char* buffer, size_t length);

  // Adds an int to the message. Returns true if the internal message buffer
  // has room for the int and the int is successfully added.
  bool AddIntToMessage(int int_to_add);

  // This returns a pointer to the next available data buffer in |data|. The
  // pointer is owned by |this| class. The resulting buffer is a string and
  // terminated with a '\0' character.
  [[nodiscard]] bool ReadString(const char** string);

  // This returns a pointer to the next available data buffer in the message
  // in |data|, and the length of the buffer in |length|. The buffer is owned
  // by |this| class.
  [[nodiscard]] bool ReadData(const char** data, size_t* length);

  // This reads the next available int from the message and stores it in
  // |result|.
  [[nodiscard]] bool ReadInt(int* result);

  // The maximum length of a message in the fixed size buffer.
  static constexpr size_t kMaxMessageLength = 4096;

 private:
  friend class BrokerSimpleMessageTestHelper;

  enum class EntryType : uint32_t { DATA = 0xBDBDBD80, INT = 0xBDBDBD81 };

  // Returns whether or not the next available entry matches the expected
  // entry type.
  bool ValidateType(EntryType expected_type);

  // Set to true once a message is read from, it may never be written to.
  bool read_only_ = false;
  // Set to true once a message is written to, it may never be read from.
  bool write_only_ = false;
  // Set when an operation fails, so that all subsequed operations fail,
  // including any attempt to send the broken message.
  bool broken_ = false;
  // The current length of the contents in the |message_| buffer.
  size_t length_ = 0;
  // The statically allocated buffer of size |kMaxMessageLength|.
  uint8_t message_[kMaxMessageLength];

  // Next location in the `message_` buffer to read from/write to.
  // RAW_PTR_EXCLUSION: Point into the `message_` buffer above, so they are
  // valid whenever `this` is valid.
  RAW_PTR_EXCLUSION uint8_t* read_next_ = message_;
  RAW_PTR_EXCLUSION uint8_t* write_next_ = message_;
};

}  // namespace syscall_broker
}  // namespace sandbox

#endif  // SANDBOX_LINUX_SYSCALL_BROKER_BROKER_SIMPLE_MESSAGE_H_
