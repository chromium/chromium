// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "sandbox/linux/services/namespace_sandbox.h"

#include <sched.h>
#include <signal.h>
#include <stddef.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

#include <string>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/command_line.h"
#include "base/environment.h"
#include "base/files/scoped_file.h"
#include "base/posix/eintr_wrapper.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "build/build_config.h"
#include "sandbox/linux/services/credentials.h"
#include "sandbox/linux/services/namespace_utils.h"
#include "sandbox/linux/services/syscall_wrappers.h"
#include "sandbox/linux/system_headers/linux_signal.h"

namespace sandbox {

namespace {

const char kSandboxUSERNSEnvironmentVarName[] = "SBX_USER_NS";
const char kSandboxPIDNSEnvironmentVarName[] = "SBX_PID_NS";
const char kSandboxNETNSEnvironmentVarName[] = "SBX_NET_NS";

class WriteUidGidMapDelegate : public base::LaunchOptions::PreExecDelegate {
 public:
  WriteUidGidMapDelegate()
      : uid_(getuid()),
        gid_(getgid()),
        supports_deny_setgroups_(
            NamespaceUtils::KernelSupportsDenySetgroups()) {}

  WriteUidGidMapDelegate(const WriteUidGidMapDelegate&) = delete;
  WriteUidGidMapDelegate& operator=(const WriteUidGidMapDelegate&) = delete;

  ~WriteUidGidMapDelegate() override {}

  void RunAsyncSafe() override {
    if (supports_deny_setgroups_) {
      RAW_CHECK(NamespaceUtils::DenySetgroups());
    }
    RAW_CHECK(NamespaceUtils::WriteToIdMapFile("/proc/self/uid_map", uid_));
    RAW_CHECK(NamespaceUtils::WriteToIdMapFile("/proc/self/gid_map", gid_));
  }

 private:
  const uid_t uid_;
  const gid_t gid_;
  const bool supports_deny_setgroups_;
};

void SetEnvironForNamespaceType(base::EnvironmentMap* environ,
                                base::NativeEnvironmentString env_var,
                                bool value) {
  // An empty string causes the env var to be unset in the child process.
  (*environ)[env_var] = value ? "1" : "";
}

// Linux supports up to 64 signals. This should be updated if that ever changes.
int g_signal_exit_codes[64];

void TerminationSignalHandler(int sig) {
  // Return a special exit code so that the process is detected as terminated by
  // a signal.
  const size_t sig_idx = static_cast<size_t>(sig);
  if (sig_idx < std::size(g_signal_exit_codes)) {
    _exit(g_signal_exit_codes[sig_idx]);
  }

  _exit(NamespaceSandbox::SignalExitCode(sig));
}

#if defined(LIBC_GLIBC)
// The first few fields of glibc's struct pthread.  The full
// definition is in:
// https://sourceware.org/git/?p=glibc.git;a=blob;f=nptl/descr.h;hb=95a73392580761abc62fc9b1386d232cd55878e9#l121
struct glibc_pthread {
  union {
#if defined(ARCH_CPU_X86_64)
    // On x86_64, sizeof(tcbhead_t) > sizeof(void*)*24.
    // https://sourceware.org/git/?p=glibc.git;a=blob;f=sysdeps/x86_64/nptl/tls.h;hb=95a73392580761abc62fc9b1386d232cd55878e9#l65
    // For all other architectures, sizeof(tcbhead_t) <= sizeof(void*)*24.
    // https://sourceware.org/git/?p=glibc.git&a=search&h=HEAD&st=grep&s=%7D+tcbhead_t
    char header[704];
#endif
    void* padding[24];
  } header;
  void* list[2];
  pid_t tid;
};

pid_t GetGlibcCachedTid() {
  pthread_mutex_t lock = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;
  CHECK_EQ(0, pthread_mutex_lock(&lock));
  pid_t tid = lock.__data.__owner;
  CHECK_EQ(0, pthread_mutex_unlock(&lock));
  CHECK_EQ(0, pthread_mutex_destroy(&lock));
  return tid;
}

void MaybeUpdateGlibcTidCache() {
  // After the below CL, glibc does not does not reset the cached
  // TID/PID on clone(), but pthread depends on it being up-to-date.
  // This CL was introduced in glibc 2.25, and backported to 2.24 on
  // at least Debian and Fedora.  This is a workaround that updates
  // the cache manually.
  // https://sourceware.org/git/?p=glibc.git;a=commit;h=c579f48edba88380635ab98cb612030e3ed8691e
  pid_t real_tid = sys_gettid();
  pid_t cached_tid = GetGlibcCachedTid();
  if (cached_tid != real_tid) {
    pid_t* cached_tid_location =
        &reinterpret_cast<struct glibc_pthread*>(pthread_self())->tid;
    CHECK_EQ(cached_tid, *cached_tid_location);
    *cached_tid_location = real_tid;
    CHECK_EQ(real_tid, GetGlibcCachedTid());
  }
}
#endif  // defined(LIBC_GLIBC)

}  // namespace

NamespaceSandbox::Options::Options()
    : ns_types(CLONE_NEWUSER | CLONE_NEWPID | CLONE_NEWNET),
      fail_on_unsupported_ns_type(false) {}

NamespaceSandbox::Options::~Options() {}

// static
base::Process NamespaceSandbox::LaunchProcess(
    const base::CommandLine& cmdline,
    const base::LaunchOptions& launch_options) {
  return LaunchProcessWithOptions(cmdline.argv(), launch_options, Options());
}

// static
base::Process NamespaceSandbox::LaunchProcess(
    const std::vector<std::string>& argv,
    const base::LaunchOptions& launch_options) {
  return LaunchProcessWithOptions(argv, launch_options, Options());
}

// static
base::Process NamespaceSandbox::LaunchProcessWithOptions(
    const base::CommandLine& cmdline,
    const base::LaunchOptions& launch_options,
    const Options& ns_sandbox_options) {
  return LaunchProcessWithOptions(cmdline.argv(), launch_options,
                                  ns_sandbox_options);
}

// static
base::Process NamespaceSandbox::LaunchProcessWithOptions(
    const std::vector<std::string>& argv,
    const base::LaunchOptions& launch_options,
    const Options& ns_sandbox_options) {
  // These fields may not be set by the caller.
  CHECK(launch_options.pre_exec_delegate == nullptr);
  CHECK_EQ(0, launch_options.clone_flags);

  int clone_flags = 0;
  const int kSupportedTypes[] = {CLONE_NEWUSER, CLONE_NEWPID, CLONE_NEWNET};
  for (const int ns_type : kSupportedTypes) {
    if ((ns_type & ns_sandbox_options.ns_types) == 0) {
      continue;
    }

    if (NamespaceUtils::KernelSupportsUnprivilegedNamespace(ns_type)) {
      clone_flags |= ns_type;
    } else if (ns_sandbox_options.fail_on_unsupported_ns_type) {
      return base::Process();
    }
  }
  CHECK(clone_flags & CLONE_NEWUSER);

  WriteUidGidMapDelegate write_uid_gid_map_delegate;

  base::LaunchOptions launch_options_copy = launch_options;
  launch_options_copy.pre_exec_delegate = &write_uid_gid_map_delegate;
  launch_options_copy.clone_flags = clone_flags;

  const std::pair<int, const char*> clone_flag_environ[] = {
      std::make_pair(CLONE_NEWUSER, kSandboxUSERNSEnvironmentVarName),
      std::make_pair(CLONE_NEWPID, kSandboxPIDNSEnvironmentVarName),
      std::make_pair(CLONE_NEWNET, kSandboxNETNSEnvironmentVarName),
  };

  base::EnvironmentMap* environ = &launch_options_copy.environment;
  for (const auto& entry : clone_flag_environ) {
    const int flag = entry.first;
    const char* environ_name = entry.second;
    SetEnvironForNamespaceType(environ, environ_name, clone_flags & flag);
  }

  return base::LaunchProcess(argv, launch_options_copy);
}

// static
pid_t NamespaceSandbox::ForkInNewPidNamespace(bool drop_capabilities_in_child) {
  const pid_t pid =
      base::ForkWithFlags(CLONE_NEWPID | LINUX_SIGCHLD, nullptr, nullptr);
  if (pid < 0) {
    return pid;
  }

  if (pid == 0) {
    DCHECK_EQ(1, getpid());
    if (drop_capabilities_in_child) {
      // Since we just forked, we are single-threaded, so this should be safe.
      CHECK(Credentials::DropAllCapabilitiesOnCurrentThread());
    }
#if defined(LIBC_GLIBC)
    MaybeUpdateGlibcTidCache();
#endif
    return 0;
  }

  return pid;
}

// static
void NamespaceSandbox::InstallDefaultTerminationSignalHandlers() {
  static const int kDefaultTermSignals[] = {
      LINUX_SIGHUP,  LINUX_SIGINT,  LINUX_SIGABRT, LINUX_SIGQUIT,
      LINUX_SIGPIPE, LINUX_SIGTERM, LINUX_SIGUSR1, LINUX_SIGUSR2,
  };

  for (const int sig : kDefaultTermSignals) {
    InstallTerminationSignalHandler(sig, SignalExitCode(sig));
  }
}

// static
bool NamespaceSandbox::InstallTerminationSignalHandler(
    int sig,
    int exit_code) {
  struct sigaction old_action;
  PCHECK(sys_sigaction(sig, nullptr, &old_action) == 0);

  if (old_action.sa_flags & SA_SIGINFO &&
      old_action.sa_sigaction != nullptr) {
    return false;
  }

  if (old_action.sa_handler != LINUX_SIG_DFL) {
    return false;
  }

  const size_t sig_idx = static_cast<size_t>(sig);
  CHECK_LT(sig_idx, std::size(g_signal_exit_codes));

  DCHECK_GE(exit_code, 0);
  DCHECK_LT(exit_code, 256);

  g_signal_exit_codes[sig_idx] = exit_code;

  struct sigaction action = {};
  action.sa_handler = &TerminationSignalHandler;
  PCHECK(sys_sigaction(sig, &action, nullptr) == 0);
  return true;
}

// static
bool NamespaceSandbox::InNewUserNamespace() {
  return getenv(kSandboxUSERNSEnvironmentVarName) != nullptr;
}

// static
bool NamespaceSandbox::InNewPidNamespace() {
  return getenv(kSandboxPIDNSEnvironmentVarName) != nullptr;
}

// static
bool NamespaceSandbox::InNewNetNamespace() {
  return getenv(kSandboxNETNSEnvironmentVarName) != nullptr;
}

}  // namespace sandbox
