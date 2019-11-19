// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_LINUX_SYSCALL_BROKER_BROKER_PROCESS_H_
#define SANDBOX_LINUX_SYSCALL_BROKER_BROKER_PROCESS_H_

#include <sys/stat.h>

#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/pickle.h"
#include "base/process/process.h"
#include "sandbox/linux/bpf_dsl/trap_registry.h"
#include "sandbox/linux/syscall_broker/broker_command.h"
#include "sandbox/linux/syscall_broker/broker_permission_list.h"
#include "sandbox/sandbox_export.h"

namespace sandbox {

namespace syscall_broker {

class BrokerClient;
class BrokerFilePermission;

// Create a new "broker" process to which we can send requests via an IPC
// channel by forking the current process.
// This is a low level IPC mechanism that is suitable to be called from a
// signal handler.
// A process would typically create a broker process before entering
// sandboxing.
// 1. BrokerProcess open_broker(read_whitelist, write_whitelist);
// 2. CHECK(open_broker.Init(NULL));
// 3. Enable sandbox.
// 4. Use open_broker.Open() to open files.
class SANDBOX_EXPORT BrokerProcess {
 public:
  // Handler to be used with a bpf_dsl Trap() function to forward system calls
  // to the methods below.
  static intptr_t SIGSYS_Handler(const arch_seccomp_data& args,
                                 void* aux_broker_process);

  // |denied_errno| is the error code returned when methods such as Open()
  // or Access() are invoked on a file which is not in the whitelist (EACCESS
  // would be a typical value).  |allowed_command_mask| is a bitwise-or of
  // kBrokerCommand*Mask constants from broker_command.h that further restrict
  // the syscalls to execute. |permissions| describes the whitelisted set
  // of files the broker is is allowed to access. |fast_check_in_client|
  // controls whether doomed requests are first filtered on the client side
  // before being proxied. Apart from tests, this should always be true since
  // our main clients are not always well-behaved. They may have third party
  // libraries that don't know about sandboxing, and typically try to open all
  // sorts of stuff they don't really need. It's important to reduce this load
  // given that there is only one pipeline to the broker process, and it is
  // not multi-threaded. |quiet_failures_for_tests| is reserved for unit tests,
  // don't use it.
  BrokerProcess(
      int denied_errno,
      const syscall_broker::BrokerCommandSet& allowed_command_set,
      const std::vector<syscall_broker::BrokerFilePermission>& permissions,
      bool fast_check_in_client = true,
      bool quiet_failures_for_tests = false);

  ~BrokerProcess();

  // Will initialize the broker process. There should be no threads at this
  // point, since we need to fork().
  // broker_process_init_callback will be called in the new broker process,
  // after fork() returns.
  bool Init(base::OnceCallback<bool(void)> broker_process_init_callback);

  // Return the PID of the child created by Init().
  int broker_pid() const { return broker_pid_; }

  // Can be used in bpf_dsl::Policy::EvaluateSyscall() implementations to
  // determine if the system call |sysno| should be trapped and forwarded
  // to the broker process for handling. This examines the
  // |allowed_command_set_| iff |fast_check_in_client_| is true. If
  // the fast checks are disabled, then all possible brokerable system
  // calls are forwarded to the broker process for handling.
  bool IsSyscallAllowed(int sysno) const;

  // The following methods are used in place of the equivalently-named
  // syscalls by the trap handler. They, in turn, forward the call onto
  // |broker_client_| for further processing. They will all be async signal
  // safe. They all return -errno on errors.

  // Can be used in place of access().
  // X_OK will always return an error in practice since the broker process
  // doesn't support execute permissions.
  int Access(const char* pathname, int mode) const;

  // Can be used in place of mkdir().
  int Mkdir(const char* path, int mode) const;

  // Can be used in place of open()
  // The implementation only supports certain white listed flags and will
  // return -EPERM on other flags.
  int Open(const char* pathname, int flags) const;

  // Can be used in place of readlink().
  int Readlink(const char* path, char* buf, size_t bufsize) const;

  // Can be used in place of rename().
  int Rename(const char* oldpath, const char* newpath) const;

  // Can be used in place of rmdir().
  int Rmdir(const char* path) const;

  // Can be used in place of stat()/stat64()/lstat()/lstat64().
  int Stat(const char* pathname, bool follow_links, struct stat* sb) const;
  int Stat64(const char* pathname, bool follow_links, struct stat64* sb) const;

  // Can be used in place of unlink().
  int Unlink(const char* path) const;

 private:
  friend class BrokerProcessTestHelper;

  // Close the IPC channel with the other party. This should only be used
  // by tests an none of the class methods should be used afterwards.
  void CloseChannel();

  bool initialized_;  // Whether we've been through Init() yet.
  pid_t broker_pid_;  // The PID of the broker (child) created in Init().
  const bool fast_check_in_client_;
  const bool quiet_failures_for_tests_;
  syscall_broker::BrokerCommandSet allowed_command_set_;
  syscall_broker::BrokerPermissionList
      broker_permission_list_;  // File access whitelist.
  std::unique_ptr<syscall_broker::BrokerClient> broker_client_;

  DISALLOW_COPY_AND_ASSIGN(BrokerProcess);
};

}  // namespace syscall_broker

}  // namespace sandbox

#endif  // SANDBOX_LINUX_SYSCALL_BROKER_BROKER_PROCESS_H_
