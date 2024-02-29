// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_LINUX_SYSCALL_BROKER_BROKER_PROCESS_H_
#define SANDBOX_LINUX_SYSCALL_BROKER_BROKER_PROCESS_H_

#include <sys/stat.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "sandbox/linux/syscall_broker/broker_sandbox_config.h"
#include "sandbox/sandbox_export.h"

namespace sandbox {

namespace syscall_broker {

class BrokerClient;

// Create a new "broker" process to which we can send requests via an IPC
// channel by forking the current process.
// This is a low level IPC mechanism that is suitable to be called from a
// signal handler.
// A process would typically create a broker process before entering
// sandboxing.
// 1. BrokerProcess open_broker(read_allowlist, write_allowlist);
// 2. CHECK(open_broker.Init(NULL));
// 3. Enable sandbox.
// 4. Use open_broker.Open() to open files.
class SANDBOX_EXPORT BrokerProcess {
 public:
  enum class BrokerType { SIGNAL_BASED };

  using BrokerSideCallback =
      base::OnceCallback<bool(const BrokerSandboxConfig&)>;

  // |policy| is the policy for the broker process.
  //
  // |denied_errno| is the error code returned when methods such as Open() or
  // Access() are invoked on a file which is not in the allowlist (EACCESS would
  // be a typical value).
  //
  // |fast_check_in_client| controls whether requests are
  // first filtered on the client side before being proxied. Apart from tests,
  // this should always be true since our main clients are not always
  // well-behaved. They may have third party libraries that don't know about
  // sandboxing, and typically try to open all sorts of stuff they don't really
  // need. It's important to reduce this load given that there is only one
  // pipeline to the broker process, and it is not multi-threaded.
  //
  // |quiet_failures_for_tests| is reserved for unit tests, don't use it.
  BrokerProcess(std::optional<BrokerSandboxConfig> policy,
                BrokerType broker_type,
                bool fast_check_in_client = true,
                bool quiet_failures_for_tests = false);

  BrokerProcess(const BrokerProcess&) = delete;
  BrokerProcess& operator=(const BrokerProcess&) = delete;

  ~BrokerProcess();

  // Will initialize the broker process. There should be no threads at this
  // point, since we need to fork().
  // broker_process_init_callback will be called in the new broker process,
  // after fork() returns.
  bool Fork(BrokerSideCallback broker_process_init_callback);

  // Return the PID of the child created by Fork().
  int broker_pid() const { return broker_pid_; }

  // Can be used in bpf_dsl::Policy::EvaluateSyscall() implementations to
  // determine if the system call |sysno| should be trapped and forwarded
  // to the broker process for handling. This examines the
  // |policy_->allowed_command_set| iff |fast_check_in_client_| is true. If
  // the fast checks are disabled, then all possible brokerable system
  // calls are forwarded to the broker process for handling.
  bool IsSyscallAllowed(int sysno) const;

  // Gets the signal-based BrokerClient created by Fork().
  syscall_broker::BrokerClient* GetBrokerClientSignalBased() const {
    return broker_client_.get();
  }

 private:
  friend class BrokerProcessTestHelper;
  friend class HandleFilesystemViaBrokerPolicy;

  // IsSyscallBrokerable() answers the same question as IsSyscallAllowed(),
  // but takes |fast_check| as a parameter. If |fast_check| is false, do not
  // check |policy_->allowed_command_set| before returning true for a syscall
  // that is brokerable.
  bool IsSyscallBrokerable(int sysno, bool fast_check) const;

  // Close the IPC channel with the other party. This should only be used
  // by tests and none of the class methods should be used afterwards.
  void CloseChannel();

  // Forks the signal-based broker, where syscall emulation is performed using
  // signals in the sandboxed process that connect to the broker via Unix
  // socket.
  bool ForkSignalBasedBroker(BrokerSideCallback broker_process_init_callback);

  // Variables initialized by the constructor.
  std::optional<BrokerSandboxConfig>
      policy_;  // Can also be created by SendPolicy().
  const BrokerType broker_type_;
  const bool fast_check_in_client_;
  const bool quiet_failures_for_tests_;

  bool initialized_ = false;  // Whether we've been through Fork() yet.
  pid_t broker_pid_ = -1;  // The PID of the broker (child) created in Fork().

  // Created in the parent process.
  std::unique_ptr<syscall_broker::BrokerClient> broker_client_;
};

}  // namespace syscall_broker

}  // namespace sandbox

#endif  // SANDBOX_LINUX_SYSCALL_BROKER_BROKER_PROCESS_H_
