// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/linux/syscall_broker/broker_process.h"

#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "base/process/process_metrics.h"
#include "build/build_config.h"
#include "sandbox/linux/syscall_broker/broker_channel.h"
#include "sandbox/linux/syscall_broker/broker_client.h"
#include "sandbox/linux/syscall_broker/broker_host.h"

namespace sandbox {

namespace syscall_broker {

BrokerProcess::BrokerProcess(
    int denied_errno,
    const syscall_broker::BrokerCommandSet& allowed_command_set,
    const std::vector<syscall_broker::BrokerFilePermission>& permissions,
    BrokerType broker_type,
    bool fast_check_in_client,
    bool quiet_failures_for_tests)
    : initialized_(false),
      broker_pid_(-1),
      broker_type_(broker_type),
      fast_check_in_client_(fast_check_in_client),
      quiet_failures_for_tests_(quiet_failures_for_tests),
      allowed_command_set_(allowed_command_set),
      broker_permission_list_(denied_errno, permissions) {}

BrokerProcess::~BrokerProcess() {
  if (initialized_) {
    if (broker_client_.get()) {
      // Closing the socket should be enough to notify the child to die,
      // unless it has been duplicated.
      CloseChannel();
    }
    PCHECK(0 == kill(broker_pid_, SIGKILL));
    siginfo_t process_info;
    // Reap the child.
    int ret = HANDLE_EINTR(waitid(P_PID, broker_pid_, &process_info, WEXITED));
    PCHECK(0 == ret);
  }
}

bool BrokerProcess::ForkSignalBasedBroker(
    base::OnceCallback<bool(void)> broker_process_init_callback) {
  BrokerChannel::EndPoint ipc_reader;
  BrokerChannel::EndPoint ipc_writer;
  BrokerChannel::CreatePair(&ipc_reader, &ipc_writer);

  int child_pid = fork();
  if (child_pid == -1)
    return false;

  if (child_pid) {
    // This string is referenced in a ChromeOS integration test; do not change.
    // TODO(crbug.com/1044502): If we can fix setproctitle, the integration
    // test not longer needs to look for this message.
    VLOG(3) << "BrokerProcess::Init(), in parent, child is " << child_pid;
    // We are the parent and we have just forked our broker process.
    ipc_reader.reset();
    broker_pid_ = child_pid;

    broker_client_ = std::make_unique<BrokerClient>(
        broker_permission_list_, std::move(ipc_writer), allowed_command_set_,
        fast_check_in_client_);

    initialized_ = true;
    return true;
  }

  // This string is referenced in a ChromeOS integration test; do not change.
  // TODO(crbug.com/1044502): If we can fix setproctitle, the integration test
  // not longer needs to look for this message.
  VLOG(3) << "BrokerProcess::Init(), in child";

  // We are the broker process. Make sure to close the writer's end so that
  // we get notified if the client disappears.
  ipc_writer.reset();

  CHECK(std::move(broker_process_init_callback).Run());

  BrokerHost broker_host_signal_based(
      broker_permission_list_, allowed_command_set_, std::move(ipc_reader));
  broker_host_signal_based.LoopAndHandleRequests();
  _exit(1);
  NOTREACHED();
  return false;
}

bool BrokerProcess::Init(
    base::OnceCallback<bool(void)> broker_process_init_callback) {
  CHECK(!initialized_);

#if !defined(THREAD_SANITIZER)
  DCHECK_EQ(1, base::GetNumberOfThreads(base::GetCurrentProcessHandle()));
#endif

  return ForkSignalBasedBroker(std::move(broker_process_init_callback));
}

bool BrokerProcess::IsSyscallAllowed(int sysno) const {
  return IsSyscallBrokerable(sysno, fast_check_in_client_);
}

bool BrokerProcess::IsSyscallBrokerable(int sysno, bool fast_check) const {
  switch (sysno) {
#if !defined(__aarch64__)
    case __NR_access:
#endif
    case __NR_faccessat:
      return !fast_check || allowed_command_set_.test(COMMAND_ACCESS);

#if !defined(__aarch64__)
    case __NR_mkdir:
#endif
    case __NR_mkdirat:
      return !fast_check || allowed_command_set_.test(COMMAND_MKDIR);

#if !defined(__aarch64__)
    case __NR_open:
#endif
    case __NR_openat:
      return !fast_check || allowed_command_set_.test(COMMAND_OPEN);

#if !defined(__aarch64__)
    case __NR_readlink:
#endif
    case __NR_readlinkat:
      return !fast_check || allowed_command_set_.test(COMMAND_READLINK);

#if !defined(__aarch64__)
    case __NR_rename:
#endif
    case __NR_renameat:
    case __NR_renameat2:
      return !fast_check || allowed_command_set_.test(COMMAND_RENAME);

#if !defined(__aarch64__)
    case __NR_rmdir:
      return !fast_check || allowed_command_set_.test(COMMAND_RMDIR);
#endif

#if !defined(__aarch64__)
    case __NR_stat:
    case __NR_lstat:
#endif
#if defined(__NR_fstatat)
    case __NR_fstatat:
#endif
#if defined(__NR_fstatat64)
    case __NR_fstatat64:
#endif
#if defined(__x86_64__) || defined(__aarch64__)
    case __NR_newfstatat:
#endif
      return !fast_check || allowed_command_set_.test(COMMAND_STAT);

#if defined(__i386__) || defined(__arm__) || \
    (defined(ARCH_CPU_MIPS_FAMILY) && defined(ARCH_CPU_32_BITS))
    case __NR_stat64:
    case __NR_lstat64:
      // For security purposes, map stat64 to COMMAND_STAT permission. The
      // separate COMMAND_STAT64 only exists to broker different-sized
      // argument structs.
      return !fast_check || allowed_command_set_.test(COMMAND_STAT);
#endif

#if !defined(__aarch64__)
    case __NR_unlink:
      return !fast_check || allowed_command_set_.test(COMMAND_UNLINK);
#endif
    case __NR_unlinkat:
      // If rmdir() doesn't exist, unlinkat is used with AT_REMOVEDIR.
      return !fast_check || allowed_command_set_.test(COMMAND_RMDIR) ||
             allowed_command_set_.test(COMMAND_UNLINK);

    default:
      return false;
  }
}

void BrokerProcess::CloseChannel() {
  broker_client_.reset();
}

}  // namespace syscall_broker
}  // namespace sandbox
