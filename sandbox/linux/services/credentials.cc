// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "sandbox/linux/services/credentials.h"

#include <errno.h>
#include <limits.h>
#include <sched.h>
#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "base/process/launch.h"
#include "build/build_config.h"
#include "sandbox/linux/services/namespace_utils.h"
#include "sandbox/linux/services/proc_util.h"
#include "sandbox/linux/services/syscall_wrappers.h"
#include "sandbox/linux/services/thread_helpers.h"
#include "sandbox/linux/system_headers/capability.h"
#include "sandbox/linux/system_headers/linux_signal.h"

namespace sandbox {

namespace {

const int kExitSuccess = 0;
#if !defined(THREAD_SANITIZER)
const int kExitFailure = 1;
#endif

#if defined(__clang__)
// Disable sanitizers that rely on TLS and may write to non-stack memory.
__attribute__((no_sanitize_address))
__attribute__((no_sanitize_thread))
__attribute__((no_sanitize_memory))
#endif
int ChrootToSelfFdinfo(void*) {
  // This function can be run from a vforked child, so it should not write to
  // any memory other than the stack or errno. Reads from TLS may be different
  // from in the parent process.
  RAW_CHECK(sys_chroot("/proc/self/fdinfo/") == 0);

  // CWD is essentially an implicit file descriptor, so be careful to not
  // leave it behind.
  RAW_CHECK(chdir("/") == 0);
  _exit(kExitSuccess);
}

// chroot() to an empty dir that is "safe". To be safe, it must not contain
// any subdirectory (chroot-ing there would allow a chroot escape) and it must
// be impossible to create an empty directory there.
// We achieve this by doing the following:
// 1. We create a new process sharing file system information.
// 2. In the child, we chroot to /proc/self/fdinfo/
// This is already "safe", since fdinfo/ does not contain another directory and
// one cannot create another directory there.
// 3. The process dies
// After (3) happens, the directory is not available anymore in /proc.
bool ChrootToSafeEmptyDir() {
  // We need to chroot to a fdinfo that is unique to a process and have that
  // process die.
  // 1. We don't want to simply fork() because duplicating the page tables is
  // slow with a big address space.
  // 2. We do not use a regular thread (that would unshare CLONE_FILES) because
  // when we are in a PID namespace, we cannot easily get a handle to the
  // /proc/tid directory for the thread (since /proc may not be aware of the
  // PID namespace). With a process, we can just use /proc/self.
  pid_t pid = -1;

  alignas(16) char stack_buf[PTHREAD_STACK_MIN_CONST];

#if defined(ARCH_CPU_X86_FAMILY) || defined(ARCH_CPU_ARM_FAMILY) || \
    defined(ARCH_CPU_MIPS_FAMILY)
  // The stack grows downward.
  void* stack = stack_buf + sizeof(stack_buf);
#else
#error "Unsupported architecture"
#endif

  int clone_flags = CLONE_FS | LINUX_SIGCHLD;
  void* tls = nullptr;
#if (defined(ARCH_CPU_X86_64) || defined(ARCH_CPU_ARM_FAMILY)) && \
    !defined(MEMORY_SANITIZER)
  // Use CLONE_VM | CLONE_VFORK as an optimization to avoid copying page tables.
  // Since clone writes to the new child's TLS before returning, we must set a
  // new TLS to avoid corrupting the current process's TLS. On ARCH_CPU_X86,
  // glibc performs syscalls by calling a function pointer in TLS, so we do not
  // attempt this optimization.
  // TODO(crbug.com/40196869) Broken in MSan builds after LLVM f1bb30a4956f.
  clone_flags |= CLONE_VM | CLONE_VFORK | CLONE_SETTLS;

  char tls_buf[PTHREAD_STACK_MIN_CONST] = {0};
  tls = tls_buf;
#endif

  pid = clone(ChrootToSelfFdinfo, stack, clone_flags, nullptr, nullptr, tls,
              nullptr);
  PCHECK(pid != -1);

  int status = -1;
  PCHECK(HANDLE_EINTR(waitpid(pid, &status, 0)) == pid);

  return WIFEXITED(status) && WEXITSTATUS(status) == kExitSuccess;
}

// CHECK() that an attempt to move to a new user namespace raised an expected
// errno.
void CheckCloneNewUserErrno(int error) {
  // EPERM can happen if already in a chroot. EUSERS if too many nested
  // namespaces are used. EINVAL for kernels that don't support the feature.
  // ENOSPC can occur when the system has reached its maximum configured
  // number of user namespaces.
  PCHECK(error == EPERM || error == EUSERS || error == EINVAL ||
         error == ENOSPC);
}

// Converts a Capability to the corresponding Linux CAP_XXX value.
int CapabilityToKernelValue(Credentials::Capability cap) {
  switch (cap) {
    case Credentials::Capability::SYS_CHROOT:
      return CAP_SYS_CHROOT;
    case Credentials::Capability::SYS_ADMIN:
      return CAP_SYS_ADMIN;
  }

  LOG(FATAL) << "Invalid Capability: " << static_cast<int>(cap);
}

}  // namespace.

// static
bool Credentials::GetRESIds(uid_t* resuid, gid_t* resgid) {
  uid_t ruid, euid, suid;
  gid_t rgid, egid, sgid;
  PCHECK(sys_getresuid(&ruid, &euid, &suid) == 0);
  PCHECK(sys_getresgid(&rgid, &egid, &sgid) == 0);
  const bool uids_are_equal = (ruid == euid) && (ruid == suid);
  const bool gids_are_equal = (rgid == egid) && (rgid == sgid);
  if (!uids_are_equal || !gids_are_equal) return false;
  if (resuid) *resuid = euid;
  if (resgid) *resgid = egid;
  return true;
}

// static
bool Credentials::SetGidAndUidMaps(gid_t gid, uid_t uid) {
  const char kGidMapFile[] = "/proc/self/gid_map";
  const char kUidMapFile[] = "/proc/self/uid_map";
  if (NamespaceUtils::KernelSupportsDenySetgroups() &&
      !NamespaceUtils::DenySetgroups()) {
    return false;
  }
  DCHECK(GetRESIds(NULL, NULL));
  if (!NamespaceUtils::WriteToIdMapFile(kGidMapFile, gid) ||
      !NamespaceUtils::WriteToIdMapFile(kUidMapFile, uid)) {
    return false;
  }
  DCHECK(GetRESIds(NULL, NULL));
  return true;
}

// static
bool Credentials::DropAllCapabilities(int proc_fd) {
  if (!SetCapabilities(proc_fd, std::vector<Capability>())) {
    return false;
  }

  CHECK(!HasAnyCapability());
  return true;
}

// static
bool Credentials::DropAllCapabilities() {
  base::ScopedFD proc_fd(ProcUtil::OpenProc());
  return Credentials::DropAllCapabilities(proc_fd.get());
}

// static
bool Credentials::DropAllCapabilitiesOnCurrentThread() {
  return SetCapabilitiesOnCurrentThread(std::vector<Capability>());
}

// static
bool Credentials::SetCapabilitiesOnCurrentThread(
    const std::vector<Capability>& caps) {
  struct cap_hdr hdr = {};
  hdr.version = _LINUX_CAPABILITY_VERSION_3;
  struct cap_data data[_LINUX_CAPABILITY_U32S_3] = {{}};

  // Initially, cap has no capability flags set. Enable the effective and
  // permitted flags only for the requested capabilities.
  for (const Capability cap : caps) {
    const int cap_num = CapabilityToKernelValue(cap);
    const size_t index = CAP_TO_INDEX(cap_num);
    const uint32_t mask = CAP_TO_MASK(cap_num);
    data[index].effective |= mask;
    data[index].permitted |= mask;
  }

  return sys_capset(&hdr, data) == 0;
}

// static
bool Credentials::SetCapabilities(int proc_fd,
                                  const std::vector<Capability>& caps) {
  DCHECK_LE(0, proc_fd);

#if !defined(THREAD_SANITIZER)
  // With TSAN, accept to break the security model as it is a testing
  // configuration.
  CHECK(ThreadHelpers::IsSingleThreaded(proc_fd));
#endif

  return SetCapabilitiesOnCurrentThread(caps);
}

bool Credentials::HasAnyCapability() {
  struct cap_hdr hdr = {};
  hdr.version = _LINUX_CAPABILITY_VERSION_3;
  struct cap_data data[_LINUX_CAPABILITY_U32S_3] = {{}};

  PCHECK(sys_capget(&hdr, data) == 0);

  for (size_t i = 0; i < std::size(data); ++i) {
    if (data[i].effective || data[i].permitted || data[i].inheritable) {
      return true;
    }
  }

  return false;
}

bool Credentials::HasCapability(Capability cap) {
  struct cap_hdr hdr = {};
  hdr.version = _LINUX_CAPABILITY_VERSION_3;
  struct cap_data data[_LINUX_CAPABILITY_U32S_3] = {{}};

  PCHECK(sys_capget(&hdr, data) == 0);

  const int cap_num = CapabilityToKernelValue(cap);
  const size_t index = CAP_TO_INDEX(cap_num);
  const uint32_t mask = CAP_TO_MASK(cap_num);

  return (data[index].effective | data[index].permitted |
          data[index].inheritable) &
         mask;
}

// static
bool Credentials::CanCreateProcessInNewUserNS() {
#if defined(THREAD_SANITIZER)
  // With TSAN, processes will always have threads running and can never
  // enter a new user namespace with MoveToNewUserNS().
  return false;
#else
  uid_t uid;
  gid_t gid;
  if (!GetRESIds(&uid, &gid)) {
    return false;
  }

  const pid_t pid =
      base::ForkWithFlags(CLONE_NEWUSER | SIGCHLD, nullptr, nullptr);

  if (pid == -1) {
    CheckCloneNewUserErrno(errno);
    return false;
  }

  // The parent process could have had threads. In the child, these threads
  // have disappeared.
  if (pid == 0) {
    // unshare() requires the effective uid and gid to have a mapping in the
    // parent namespace.
    if (!SetGidAndUidMaps(gid, uid))
      _exit(kExitFailure);

    // Make sure we drop CAP_SYS_ADMIN.
    CHECK(sandbox::Credentials::DropAllCapabilities());

    // Ensure we have unprivileged use of CLONE_NEWUSER.  Debian
    // Jessie explicitly forbids this case.  See:
    // add-sysctl-to-disallow-unprivileged-CLONE_NEWUSER-by-default.patch
    if (sys_unshare(CLONE_NEWUSER))
      _exit(kExitFailure);

    _exit(kExitSuccess);
  }

  // Always reap the child.
  int status = -1;
  PCHECK(HANDLE_EINTR(waitpid(pid, &status, 0)) == pid);

  DCHECK(WIFEXITED(status) && (WEXITSTATUS(status) == kExitSuccess ||
                               WEXITSTATUS(status) == kExitFailure));

  // clone(2) succeeded.  Now return true only if the system grants
  // unprivileged use of CLONE_NEWUSER as well.
  return WIFEXITED(status) && WEXITSTATUS(status) == kExitSuccess;
#endif
}

bool Credentials::MoveToNewUserNS() {
  uid_t uid;
  gid_t gid;
  if (!GetRESIds(&uid, &gid)) {
    // If all the uids (or gids) are not equal to each other, the security
    // model will most likely confuse the caller, abort.
    DVLOG(1) << "uids or gids differ!";
    return false;
  }
  int ret = sys_unshare(CLONE_NEWUSER);
  if (ret) {
    const int unshare_errno = errno;
    VLOG(1) << "Looks like unprivileged CLONE_NEWUSER may not be available "
            << "on this kernel.";
    CheckCloneNewUserErrno(unshare_errno);
    return false;
  }

  // The current {r,e,s}{u,g}id is now an overflow id (c.f.
  // /proc/sys/kernel/overflowuid). Setup the uid and gid maps.
  PCHECK(SetGidAndUidMaps(gid, uid));
  return true;
}

bool Credentials::DropFileSystemAccess(int proc_fd) {
  CHECK_LE(0, proc_fd);

  CHECK(ChrootToSafeEmptyDir());
  CHECK(!HasFileSystemAccess());
  CHECK(!ProcUtil::HasOpenDirectory(proc_fd));
  // We never let this function fail.
  return true;
}

bool Credentials::HasFileSystemAccess() {
  return base::DirectoryExists(base::FilePath("/proc"));
}

pid_t Credentials::ForkAndDropCapabilitiesInChild() {
  pid_t pid = fork();
  if (pid != 0) {
    return pid;
  }

  // Since we just forked, we are single threaded.
  PCHECK(DropAllCapabilitiesOnCurrentThread());
  return 0;
}

}  // namespace sandbox.
