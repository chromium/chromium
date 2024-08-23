// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/zygote_host/zygote_host_impl_linux.h"

#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "base/files/file_enumerator.h"
#include "base/logging.h"
#include "base/posix/unix_domain_socket.h"
#include "base/process/kill.h"
#include "base/process/memory.h"
#include "base/strings/string_number_conversions.h"
#include "base/types/fixed_array.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "content/common/zygote/zygote_commands_linux.h"
#include "content/common/zygote/zygote_communication_linux.h"
#include "content/common/zygote/zygote_handle_impl_linux.h"
#include "content/public/common/zygote/zygote_handle.h"
#include "sandbox/linux/services/credentials.h"
#include "sandbox/linux/services/namespace_sandbox.h"
#include "sandbox/linux/suid/client/setuid_sandbox_host.h"
#include "sandbox/linux/suid/common/sandbox.h"
#include "sandbox/policy/linux/sandbox_linux.h"
#include "sandbox/policy/switches.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "content/common/zygote/zygote_communication_linux.h"
#include "content/common/zygote/zygote_handle_impl_linux.h"
#include "content/public/common/zygote/zygote_handle.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace content {

namespace {

// Receive a fixed message on fd and return the sender's PID.
// Returns true if the message received matches the expected message.
bool ReceiveFixedMessage(int fd,
                         const char* expect_msg,
                         size_t expect_len,
                         base::ProcessId* sender_pid) {
  // Allocate an extra byte of buffer space so we can check that we received
  // exactly |expect_len| bytes, and the message wasn't just truncated to fit.
  base::FixedArray<char> buf(expect_len + 1);
  std::vector<base::ScopedFD> fds_vec;

  const ssize_t len = base::UnixDomainSocket::RecvMsgWithPid(
      fd, buf.data(), buf.memsize(), &fds_vec, sender_pid);
  if (static_cast<size_t>(len) != expect_len)
    return false;
  if (memcmp(buf.data(), expect_msg, expect_len) != 0) {
    return false;
  }
  if (!fds_vec.empty())
    return false;
  return true;
}

}  // namespace

// static
ZygoteHost* ZygoteHost::GetInstance() {
  return ZygoteHostImpl::GetInstance();
}

ZygoteHostImpl::ZygoteHostImpl()
    : use_namespace_sandbox_(false),
      use_suid_sandbox_(false),
      use_suid_sandbox_for_adj_oom_score_(false),
      sandbox_binary_(),
      zygote_pids_lock_(),
      zygote_pids_() {}

ZygoteHostImpl::~ZygoteHostImpl() {}

// static
ZygoteHostImpl* ZygoteHostImpl::GetInstance() {
  return base::Singleton<ZygoteHostImpl>::get();
}

void ZygoteHostImpl::Init(const base::CommandLine& command_line) {
  if (command_line.HasSwitch(sandbox::policy::switches::kNoSandbox)) {
    return;
  }

  // Exit early if running as root without --no-sandbox. See
  // https://crbug.com/638180.
  // When running as root with the sandbox enabled, the browser process
  // crashes on zygote initialization. Running as root with the sandbox
  // is not supported, and if Chrome were able to display UI it would be showing
  // an error message. With the zygote crashing it doesn't even get to that,
  // so print an error message on the console.
  uid_t uid = 0;
  gid_t gid = 0;
  if (!sandbox::Credentials::GetRESIds(&uid, &gid) || uid == 0) {
    LOG(ERROR) << "Running as root without --"
               << sandbox::policy::switches::kNoSandbox
               << " is not supported. See https://crbug.com/638180.";
    exit(EXIT_FAILURE);
  }

  {
    std::unique_ptr<sandbox::SetuidSandboxHost> setuid_sandbox_host(
        sandbox::SetuidSandboxHost::Create());
    sandbox_binary_ = setuid_sandbox_host->GetSandboxBinaryPath().value();
  }

  if (!command_line.HasSwitch(
          sandbox::policy::switches::kDisableNamespaceSandbox) &&
      sandbox::Credentials::CanCreateProcessInNewUserNS()) {
    use_namespace_sandbox_ = true;
  } else if (!command_line.HasSwitch(
                 sandbox::policy::switches::kDisableSetuidSandbox) &&
             !sandbox_binary_.empty()) {
    use_suid_sandbox_ = true;

    // Use the SUID sandbox for adjusting OOM scores when we are using
    // the setuid sandbox. This is needed beacuse the processes are
    // non-dumpable, so /proc/pid/oom_score_adj can only be written by
    // root.
    use_suid_sandbox_for_adj_oom_score_ = use_suid_sandbox_;
  } else {
    LOG(FATAL)
        << "No usable sandbox! If you are running on Ubuntu 23.10+ or another "
           "Linux distro that has disabled unprivileged user namespaces with "
           "AppArmor, see "
           "https://chromium.googlesource.com/chromium/src/+/main/"
           "docs/security/apparmor-userns-restrictions.md. Otherwise see "
           "https://chromium.googlesource.com/chromium/src/+/main/"
           "docs/linux/suid_sandbox_development.md for more information on "
           "developing with the (older) SUID sandbox. "
           "If you want to live dangerously and need an immediate workaround, "
           "you can try using --"
        << sandbox::policy::switches::kNoSandbox << ".";
  }
}

void ZygoteHostImpl::AddZygotePid(pid_t pid) {
  base::AutoLock lock(zygote_pids_lock_);
  zygote_pids_.insert(pid);
}

bool ZygoteHostImpl::IsZygotePid(pid_t pid) {
  base::AutoLock lock(zygote_pids_lock_);
  return zygote_pids_.find(pid) != zygote_pids_.end();
}

void ZygoteHostImpl::SetRendererSandboxStatus(int status) {
  renderer_sandbox_status_ = status;
}

int ZygoteHostImpl::GetRendererSandboxStatus() {
  return renderer_sandbox_status_;
}

pid_t ZygoteHostImpl::LaunchZygote(
    base::CommandLine* cmd_line,
    base::ScopedFD* control_fd,
    base::FileHandleMappingVector additional_remapped_fds) {
  int fds[2];
  CHECK_EQ(0, socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0, fds));
  CHECK(base::UnixDomainSocket::EnableReceiveProcessId(fds[0]));

  base::LaunchOptions options;
  options.fds_to_remap = std::move(additional_remapped_fds);
  options.fds_to_remap.emplace_back(fds[1], kZygoteSocketPairFd);
  options.fds_to_remove_cloexec.push_back(fds[1]);

  const bool is_sandboxed_zygote =
      !cmd_line->HasSwitch(sandbox::policy::switches::kNoZygoteSandbox);

  base::ScopedFD dummy_fd;
  if (is_sandboxed_zygote && use_suid_sandbox_) {
    std::unique_ptr<sandbox::SetuidSandboxHost> sandbox_host(
        sandbox::SetuidSandboxHost::Create());
    sandbox_host->PrependWrapper(cmd_line);
    sandbox_host->SetupLaunchOptions(&options, &dummy_fd);
    sandbox_host->SetupLaunchEnvironment();
  }

  base::Process process =
      (is_sandboxed_zygote && use_namespace_sandbox_)
          ? sandbox::NamespaceSandbox::LaunchProcess(*cmd_line, options)
          : base::LaunchProcess(*cmd_line, options);
  CHECK(process.IsValid()) << "Failed to launch zygote process";

  dummy_fd.reset();
  close(fds[1]);
  control_fd->reset(fds[0]);

  pid_t pid = process.Pid();

  if (is_sandboxed_zygote && (use_namespace_sandbox_ || use_suid_sandbox_)) {
    // The namespace and SUID sandbox will execute the zygote in a new
    // PID namespace, and the main zygote process will then fork from
    // there. Watch now our elaborate dance to find and validate the
    // zygote's PID.

    // First we receive a message from the zygote boot process.
    base::ProcessId boot_pid;
    PCHECK(ReceiveFixedMessage(fds[0], kZygoteBootMessage,
                               sizeof(kZygoteBootMessage), &boot_pid));

    // Within the PID namespace, the zygote boot process thinks it's PID 1,
    // but its real PID can never be 1. This gives us a reliable test that
    // the kernel is translating the sender's PID to our namespace.
    CHECK_GT(boot_pid, 1)
        << "Received invalid process ID for zygote; kernel might be too old? "
           "See crbug.com/357670 or try using --"
        << sandbox::policy::switches::kNoSandbox << " to workaround.";

    // Now receive the message that the zygote's ready to go, along with the
    // main zygote process's ID.
    pid_t real_pid;
    PCHECK(ReceiveFixedMessage(fds[0], kZygoteHelloMessage,
                               sizeof(kZygoteHelloMessage), &real_pid));
    CHECK_GT(real_pid, 1);

    if (real_pid != pid) {
      // Reap the sandbox.
      base::EnsureProcessGetsReaped(std::move(process));
    }
    pid = real_pid;
  }

  AddZygotePid(pid);
  return pid;
}

#if !BUILDFLAG(IS_OPENBSD)
void ZygoteHostImpl::AdjustRendererOOMScore(base::ProcessHandle pid,
                                            int score) {
  // 1) You can't change the oom_score_adj of a non-dumpable process
  //    (EPERM) unless you're root. Because of this, we can't set the
  //    oom_adj from the browser process.
  //
  // 2) We can't set the oom_score_adj before entering the sandbox
  //    because the zygote is in the sandbox and the zygote is as
  //    critical as the browser process. Its oom_adj value shouldn't
  //    be changed.
  //
  // 3) A non-dumpable process can't even change its own oom_score_adj
  //    because it's root owned 0644. The sandboxed processes don't
  //    even have /proc, but one could imagine passing in a descriptor
  //    from outside.
  //
  // So, in the normal case, we use the SUID binary to change it for us.
  // However, Fedora (and other SELinux systems) don't like us touching other
  // process's oom_score_adj (or oom_adj) values
  // (https://bugzilla.redhat.com/show_bug.cgi?id=581256).
  //
  // The offical way to get the SELinux mode is selinux_getenforcemode, but I
  // don't want to add another library to the build as it's sure to cause
  // problems with other, non-SELinux distros.
  //
  // So we just check for files in /selinux. This isn't foolproof, but it's not
  // bad and it's easy.

  static bool selinux;
  static bool selinux_valid = false;

  if (!selinux_valid) {
    const base::FilePath kSelinuxPath("/selinux");
    base::FileEnumerator en(kSelinuxPath, false, base::FileEnumerator::FILES);
    bool has_selinux_files = !en.Next().empty();

    selinux =
        has_selinux_files && access(kSelinuxPath.value().c_str(), X_OK) == 0;
    selinux_valid = true;
  }

  if (!use_suid_sandbox_for_adj_oom_score_) {
    if (!base::AdjustOOMScore(pid, score))
      PLOG(ERROR) << "Failed to adjust OOM score of renderer with pid " << pid;
    return;
  }

  if (selinux)
    return;

  std::vector<std::string> adj_oom_score_cmdline;
  adj_oom_score_cmdline.push_back(sandbox_binary_);
  adj_oom_score_cmdline.push_back(sandbox::kAdjustOOMScoreSwitch);
  adj_oom_score_cmdline.push_back(base::NumberToString(pid));
  adj_oom_score_cmdline.push_back(base::NumberToString(score));

  // sandbox_helper_process is a setuid binary.
  base::LaunchOptions options;
  options.allow_new_privs = true;

  base::Process sandbox_helper_process =
      base::LaunchProcess(adj_oom_score_cmdline, options);
  if (sandbox_helper_process.IsValid())
    base::EnsureProcessGetsReaped(std::move(sandbox_helper_process));
}
#endif

#if BUILDFLAG(IS_CHROMEOS)
void ZygoteHostImpl::ReinitializeLogging(uint32_t logging_dest,
                                         base::PlatformFile log_file_fd) {
  if (!HasZygote()) {
    return;
  }

  content::ZygoteCommunication* generic_zygote = content::GetGenericZygote();
  content::ZygoteCommunication* unsandboxed_zygote =
      content::GetUnsandboxedZygote();

  generic_zygote->ReinitializeLogging(logging_dest, log_file_fd);
  if (unsandboxed_zygote) {
    unsandboxed_zygote->ReinitializeLogging(logging_dest, log_file_fd);
  }
}
#endif  // BUILDFLAG(IS_CHROMEOS)

}  // namespace content
