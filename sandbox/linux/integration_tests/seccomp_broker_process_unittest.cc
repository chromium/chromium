// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <memory>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/posix/eintr_wrapper.h"
#include "build/build_config.h"
#include "sandbox/linux/bpf_dsl/bpf_dsl.h"
#include "sandbox/linux/bpf_dsl/policy.h"
#include "sandbox/linux/bpf_dsl/seccomp_macros.h"
#include "sandbox/linux/seccomp-bpf/bpf_tests.h"
#include "sandbox/linux/seccomp-bpf/sandbox_bpf.h"
#include "sandbox/linux/syscall_broker/broker_command.h"
#include "sandbox/linux/syscall_broker/broker_file_permission.h"
#include "sandbox/linux/syscall_broker/broker_process.h"
#include "sandbox/linux/system_headers/linux_syscalls.h"
#include "sandbox/linux/tests/unit_tests.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sandbox {

namespace {

using bpf_dsl::Allow;
using bpf_dsl::ResultExpr;
using bpf_dsl::Trap;

// Test a trap handler that makes use of a broker process to open().

class InitializedOpenBroker {
 public:
  InitializedOpenBroker() : initialized_(false) {
    syscall_broker::BrokerCommandSet command_set;
    command_set.set(syscall_broker::COMMAND_OPEN);
    command_set.set(syscall_broker::COMMAND_ACCESS);
    std::vector<syscall_broker::BrokerFilePermission> permissions = {
        syscall_broker::BrokerFilePermission::ReadOnly("/proc/allowed"),
        syscall_broker::BrokerFilePermission::ReadOnly("/proc/cpuinfo")};
    broker_process_ = std::make_unique<syscall_broker::BrokerProcess>(
        EPERM, command_set, permissions);
    BPF_ASSERT(broker_process_->Init(base::BindOnce([]() { return true; })));
    initialized_ = true;
  }

  bool initialized() const { return initialized_; }

  syscall_broker::BrokerProcess* broker_process() const {
    return broker_process_.get();
  }

 private:
  bool initialized_;
  std::unique_ptr<syscall_broker::BrokerProcess> broker_process_;
  DISALLOW_COPY_AND_ASSIGN(InitializedOpenBroker);
};

intptr_t BrokerOpenTrapHandler(const struct arch_seccomp_data& args,
                               void* aux) {
  BPF_ASSERT(aux);
  syscall_broker::BrokerProcess* broker_process =
      static_cast<syscall_broker::BrokerProcess*>(aux);
  switch (args.nr) {
    case __NR_faccessat:  // access is a wrapper of faccessat in android
      BPF_ASSERT(static_cast<int>(args.args[0]) == AT_FDCWD);
      return broker_process->Access(reinterpret_cast<const char*>(args.args[1]),
                                    static_cast<int>(args.args[2]));
#if defined(__NR_access)
    case __NR_access:
      return broker_process->Access(reinterpret_cast<const char*>(args.args[0]),
                                    static_cast<int>(args.args[1]));
#endif
#if defined(__NR_open)
    case __NR_open:
      return broker_process->Open(reinterpret_cast<const char*>(args.args[0]),
                                  static_cast<int>(args.args[1]));
#endif
    case __NR_openat:
      // We only call open() so if we arrive here, it's because glibc uses
      // the openat() system call.
      BPF_ASSERT(static_cast<int>(args.args[0]) == AT_FDCWD);
      return broker_process->Open(reinterpret_cast<const char*>(args.args[1]),
                                  static_cast<int>(args.args[2]));
    default:
      BPF_ASSERT(false);
      return -ENOSYS;
  }
}

class DenyOpenPolicy : public bpf_dsl::Policy {
 public:
  explicit DenyOpenPolicy(InitializedOpenBroker* iob) : iob_(iob) {}
  ~DenyOpenPolicy() override {}

  ResultExpr EvaluateSyscall(int sysno) const override {
    DCHECK(SandboxBPF::IsValidSyscallNumber(sysno));

    switch (sysno) {
      case __NR_faccessat:
#if defined(__NR_access)
      case __NR_access:
#endif
#if defined(__NR_open)
      case __NR_open:
#endif
      case __NR_openat:
        // We get a InitializedOpenBroker class, but our trap handler wants
        // the syscall_broker::BrokerProcess object.
        return Trap(BrokerOpenTrapHandler, iob_->broker_process());
      default:
        return Allow();
    }
  }

 private:
  InitializedOpenBroker* iob_;

  DISALLOW_COPY_AND_ASSIGN(DenyOpenPolicy);
};

// We use a InitializedOpenBroker class, so that we can run unsandboxed
// code in its constructor, which is the only way to do so in a BPF_TEST.
BPF_TEST(SandboxBPF,
         UseOpenBroker,
         DenyOpenPolicy,
         InitializedOpenBroker /* (*BPF_AUX) */) {
  BPF_ASSERT(BPF_AUX->initialized());
  syscall_broker::BrokerProcess* broker_process = BPF_AUX->broker_process();
  BPF_ASSERT(broker_process != NULL);

  // First, use the broker "manually"
  BPF_ASSERT(broker_process->Open("/proc/denied", O_RDONLY) == -EPERM);
  BPF_ASSERT(broker_process->Access("/proc/denied", R_OK) == -EPERM);
  BPF_ASSERT(broker_process->Open("/proc/allowed", O_RDONLY) == -ENOENT);
  BPF_ASSERT(broker_process->Access("/proc/allowed", R_OK) == -ENOENT);

  // Now use glibc's open() as an external library would.
  BPF_ASSERT(open("/proc/denied", O_RDONLY) == -1);
  BPF_ASSERT(errno == EPERM);

  BPF_ASSERT(open("/proc/allowed", O_RDONLY) == -1);
  BPF_ASSERT(errno == ENOENT);

  // Also test glibc's openat(), some versions of libc use it transparently
  // instead of open().
  BPF_ASSERT(openat(AT_FDCWD, "/proc/denied", O_RDONLY) == -1);
  BPF_ASSERT(errno == EPERM);

  BPF_ASSERT(openat(AT_FDCWD, "/proc/allowed", O_RDONLY) == -1);
  BPF_ASSERT(errno == ENOENT);

  // And test glibc's access().
  BPF_ASSERT(access("/proc/denied", R_OK) == -1);
  BPF_ASSERT(errno == EPERM);

  BPF_ASSERT(access("/proc/allowed", R_OK) == -1);
  BPF_ASSERT(errno == ENOENT);

  // This is also white listed and does exist.
  int cpu_info_access = access("/proc/cpuinfo", R_OK);
  BPF_ASSERT(cpu_info_access == 0);
  int cpu_info_fd = open("/proc/cpuinfo", O_RDONLY);
  BPF_ASSERT(cpu_info_fd >= 0);
  char buf[1024];
  BPF_ASSERT(read(cpu_info_fd, buf, sizeof(buf)) > 0);
}

}  // namespace

}  // namespace sandbox
