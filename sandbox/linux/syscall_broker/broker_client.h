// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_LINUX_SYSCALL_BROKER_BROKER_CLIENT_H_
#define SANDBOX_LINUX_SYSCALL_BROKER_BROKER_CLIENT_H_

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "base/memory/raw_ref.h"
#include "sandbox/linux/syscall_broker/broker_channel.h"
#include "sandbox/linux/syscall_broker/broker_command.h"
#include "sandbox/linux/syscall_broker/broker_sandbox_config.h"
#include "sandbox/linux/syscall_broker/syscall_dispatcher.h"
#include "sandbox/sandbox_export.h"

namespace sandbox {
namespace syscall_broker {

// This class can be embedded in a sandboxed process and can be
// used to perform certain system calls in another, presumably
// non-sandboxed process (which embeds BrokerHost).
// A key feature of this class is the ability to use some of its methods in a
// thread-safe and async-signal safe way. The goal is to be able to use it to
// replace the open() or access() system calls happening anywhere in a process
// (as allowed for instance by seccomp-bpf's SIGSYS mechanism).
class SANDBOX_EXPORT BrokerClient : public SyscallDispatcher {
 public:
  // Handler to be used with a bpf_dsl Trap() function to forward system calls
  // to the methods below.
  static intptr_t SIGSYS_Handler(const arch_seccomp_data& args,
                                 void* aux_broker_process);

  // |policy| needs to match the policy used by BrokerHost. This allows to
  // predict some of the requests which will be denied and save an IPC round
  // trip.
  // |ipc_channel| needs to be a suitable SOCK_SEQPACKET unix socket.
  // |fast_check_in_client| should be set to true and
  BrokerClient(const BrokerSandboxConfig& policy,
               BrokerChannel::EndPoint ipc_channel,
               bool fast_check_in_client);

  BrokerClient(const BrokerClient&) = delete;
  BrokerClient& operator=(const BrokerClient&) = delete;

  ~BrokerClient() override;

  // Get the file descriptor used for IPC.
  int GetIPCDescriptorForTesting() const { return ipc_channel_.get(); }

  // The following public methods can be used in place of the equivalently
  // name system calls. They all return -errno on errors. They are all async
  // signal safe so they may be called from a SIGSYS trap handler.

  // SyscallDispatcher implementation:
  int Access(const char* pathname, int mode) const override;
  int Mkdir(const char* path, int mode) const override;
  int Open(const char* pathname, int flags) const override;
  int Readlink(const char* path, char* buf, size_t bufsize) const override;
  int Rename(const char* oldpath, const char* newpath) const override;
  int Rmdir(const char* path) const override;
  int Stat(const char* pathname,
           bool follow_links,
           struct kernel_stat* sb) const override;
  int Stat64(const char* pathname,
             bool follow_links,
             struct kernel_stat64* sb) const override;
  int Unlink(const char* unlink) const override;
  int InotifyAddWatch(int fd,
                      const char* pathname,
                      uint32_t mask) const override;

  const BrokerSandboxConfig& policy() const { return *policy_; }

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

  const raw_ref<const BrokerSandboxConfig> policy_;
  const BrokerChannel::EndPoint ipc_channel_;
  const bool fast_check_in_client_;  // Whether to forward a request that we
                                     // know will be denied to the broker. (Used
                                     // for tests).
};

}  // namespace syscall_broker

}  // namespace sandbox

#endif  //  SANDBOX_LINUX_SYSCALL_BROKER_BROKER_CLIENT_H_
