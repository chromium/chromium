// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_LINUX_SYSCALL_BROKER_BROKER_CLIENT_H_
#define SANDBOX_LINUX_SYSCALL_BROKER_BROKER_CLIENT_H_

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "base/macros.h"
#include "sandbox/linux/syscall_broker/broker_channel.h"
#include "sandbox/linux/syscall_broker/broker_command.h"

namespace sandbox {
namespace syscall_broker {

class BrokerPermissionList;

// This class can be embedded in a sandboxed process and can be
// used to perform certain system calls in another, presumably
// non-sandboxed process (which embeds BrokerHost).
// A key feature of this class is the ability to use some of its methods in a
// thread-safe and async-signal safe way. The goal is to be able to use it to
// replace the open() or access() system calls happening anywhere in a process
// (as allowed for instance by seccomp-bpf's SIGSYS mechanism).
class BrokerClient {
 public:
  // |policy| needs to match the policy used by BrokerHost. This
  // allows to predict some of the requests which will be denied
  // and save an IPC round trip.
  // |ipc_channel| needs to be a suitable SOCK_SEQPACKET unix socket.
  // |fast_check_in_client| should be set to true and
  BrokerClient(const BrokerPermissionList& policy,
               BrokerChannel::EndPoint ipc_channel,
               const BrokerCommandSet& allowed_command_set,
               bool fast_check_in_client);
  ~BrokerClient();

  // Get the file descriptor used for IPC. This is used for tests.
  int GetIPCDescriptor() const { return ipc_channel_.get(); }

  // The following public methods can be used in place of the equivalently
  // name system calls. They all return -errno on errors. They are all async
  // signal safe so they may be called from a SIGSYS trap handler.

  // Can be used in place of access().
  // X_OK will always return an error in practice since the broker process
  // doesn't support execute permissions.
  int Access(const char* pathname, int mode) const;

  // Can be used in place of mkdir().
  int Mkdir(const char* path, int mode) const;

  // Can be used in place of open().
  // The implementation only supports certain white listed flags and will
  // return -EPERM on other flags.
  int Open(const char* pathname, int flags) const;

  // Can be used in place of Readlink().
  int Readlink(const char* path, char* buf, size_t bufsize) const;

  // Can be used in place of rename().
  int Rename(const char* oldpath, const char* newpath) const;

  // Can be used in place of rmdir().
  int Rmdir(const char* path) const;

  // Can be used in place of stat()/stat64()/lstat()/lstat64()
  int Stat(const char* pathname, bool follow_links, struct stat* sb) const;
  int Stat64(const char* pathname, bool folllow_links, struct stat64* sb) const;

  // Can be used in place of unlink().
  int Unlink(const char* unlink) const;

 private:
  int PathOnlySyscall(BrokerCommand syscall_type, const char* pathname) const;

  int PathAndFlagsSyscall(BrokerCommand syscall_type,
                          const char* pathname,
                          int flags) const;

  int PathAndFlagsSyscallReturningFD(BrokerCommand syscall_type,
                                     const char* pathname,
                                     int flags) const;

  int StatFamilySyscall(BrokerCommand syscall_type,
                        const char* pathname,
                        bool follow_links,
                        void* result_ptr,
                        size_t expected_result_size) const;

  const BrokerPermissionList& broker_permission_list_;
  const BrokerChannel::EndPoint ipc_channel_;
  const BrokerCommandSet allowed_command_set_;
  const bool fast_check_in_client_;  // Whether to forward a request that we
                                     // know will be denied to the broker. (Used
                                     // for tests).

  DISALLOW_COPY_AND_ASSIGN(BrokerClient);
};

}  // namespace syscall_broker

}  // namespace sandbox

#endif  //  SANDBOX_LINUX_SYSCALL_BROKER_BROKER_CLIENT_H_
