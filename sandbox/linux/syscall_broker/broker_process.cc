// Copyright 2012 The Chromium Authors
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
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file_util.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "base/process/process_metrics.h"
#include "build/build_config.h"
#include "sandbox/linux/syscall_broker/broker_channel.h"
#include "sandbox/linux/syscall_broker/broker_client.h"
#include "sandbox/linux/syscall_broker/broker_command.h"
#include "sandbox/linux/syscall_broker/broker_host.h"
#include "sandbox/linux/syscall_broker/broker_permission_list.h"
#include "sandbox/linux/system_headers/linux_syscalls.h"

namespace sandbox {

namespace syscall_broker {

BrokerProcess::BrokerProcess(std::optional<BrokerSandboxConfig> policy,
                             BrokerType broker_type,
                             bool fast_check_in_client,
                             bool quiet_failures_for_tests)
    : policy_(std::move(policy)),
      broker_type_(broker_type),
      fast_check_in_client_(fast_check_in_client),
      quiet_failures_for_tests_(quiet_failures_for_tests) {}

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
    BrokerSideCallback broker_process_init_callback) {
  BrokerChannel::EndPoint ipc_reader, ipc_writer;
  BrokerChannel::CreatePair(&ipc_reader, &ipc_writer);

  pid_t parent_pid = getpid();

  pid_t child_pid = fork();
  if (child_pid == -1)
    return false;

  if (child_pid) {
    // We are the parent and we have just forked our broker process.
    ipc_reader.reset();

    // If we already know our policy we can go ahead and create the
    // BrokerClient.
    CHECK(policy_);
    broker_client_ = std::make_unique<BrokerClient>(
        *policy_, std::move(ipc_writer), fast_check_in_client_);

    broker_pid_ = child_pid;
    initialized_ = true;
    return true;
  }

  // We are the broker process. Make sure to close the writer's end so that
  // we get notified if the client disappears.
  ipc_writer.reset();

  CHECK(std::move(broker_process_init_callback).Run(*policy_));

  BrokerHost broker_host_signal_based(*policy_, std::move(ipc_reader),
                                      parent_pid);
  broker_host_signal_based.LoopAndHandleRequests();
  _exit(1);
  NOTREACHED();
}

bool BrokerProcess::Fork(BrokerSideCallback broker_process_init_callback) {
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
  CHECK(policy_);

  // The syscalls unavailable on aarch64 are all blocked by Android's default
  // seccomp policy, even on non-aarch64 architectures. I.e., the syscalls XX()
  // with a corresponding XXat() versions are typically unavailable in aarch64
  // and are default disabled in Android. So, we should refuse to broker them
  // to be consistent with the platform's restrictions.
  switch (sysno) {
#if !defined(__aarch64__) && !BUILDFLAG(IS_ANDROID)
    case __NR_access:
#endif
    case __NR_faccessat:
    case __NR_faccessat2:
      return !fast_check || policy_->allowed_command_set.test(COMMAND_ACCESS);

#if !defined(__aarch64__) && !BUILDFLAG(IS_ANDROID)
    case __NR_mkdir:
#endif
    case __NR_mkdirat:
      return !fast_check || policy_->allowed_command_set.test(COMMAND_MKDIR);

#if !defined(__aarch64__) && !BUILDFLAG(IS_ANDROID)
    case __NR_open:
#endif
    case __NR_openat:
      return !fast_check || policy_->allowed_command_set.test(COMMAND_OPEN);

#if !defined(__aarch64__) && !BUILDFLAG(IS_ANDROID)
    case __NR_readlink:
#endif
    case __NR_readlinkat:
      return !fast_check || policy_->allowed_command_set.test(COMMAND_READLINK);

#if !defined(__aarch64__) && !BUILDFLAG(IS_ANDROID)
    case __NR_rename:
#endif
    case __NR_renameat:
    case __NR_renameat2:
      return !fast_check || policy_->allowed_command_set.test(COMMAND_RENAME);

#if !defined(__aarch64__) && !BUILDFLAG(IS_ANDROID)
    case __NR_rmdir:
      return !fast_check || policy_->allowed_command_set.test(COMMAND_RMDIR);
#endif

#if !defined(__aarch64__) && !BUILDFLAG(IS_ANDROID)
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
      return !fast_check || policy_->allowed_command_set.test(COMMAND_STAT);

#if defined(__i386__) || defined(__arm__) || \
    (defined(ARCH_CPU_MIPS_FAMILY) && defined(ARCH_CPU_32_BITS))
    case __NR_stat64:
    case __NR_lstat64:
      // For security purposes, map stat64 to COMMAND_STAT permission. The
      // separate COMMAND_STAT64 only exists to broker different-sized
      // argument structs.
      return !fast_check || policy_->allowed_command_set.test(COMMAND_STAT);
#endif

#if !defined(__aarch64__) && !BUILDFLAG(IS_ANDROID)
    case __NR_unlink:
      return !fast_check || policy_->allowed_command_set.test(COMMAND_UNLINK);
#endif
    case __NR_unlinkat:
      // If rmdir() doesn't exist, unlinkat is used with AT_REMOVEDIR.
      return !fast_check || policy_->allowed_command_set.test(COMMAND_RMDIR) ||
             policy_->allowed_command_set.test(COMMAND_UNLINK);
    case __NR_inotify_add_watch:
      return !fast_check ||
             policy_->allowed_command_set.test(COMMAND_INOTIFY_ADD_WATCH);
    default:
      return false;
  }
}

void BrokerProcess::CloseChannel() {
  broker_client_.reset();
}

}  // namespace syscall_broker
}  // namespace sandbox
