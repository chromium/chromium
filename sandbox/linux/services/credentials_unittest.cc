// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/linux/services/credentials.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <sys/capability.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <memory>
#include <vector>

#include "base/check_op.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "sandbox/linux/services/proc_util.h"
#include "sandbox/linux/services/syscall_wrappers.h"
#include "sandbox/linux/system_headers/capability.h"
#include "sandbox/linux/tests/unit_tests.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sandbox {

namespace {

struct CapFreeDeleter {
  inline void operator()(cap_t cap) const {
    int ret = cap_free(cap);
    CHECK_EQ(0, ret);
  }
};

// Wrapper to manage libcap2's cap_t type.
typedef std::unique_ptr<std::remove_reference<decltype(*((cap_t)0))>::type,
                        CapFreeDeleter>
    ScopedCap;

bool WorkingDirectoryIsRoot() {
  char current_dir[PATH_MAX];
  char* cwd = getcwd(current_dir, sizeof(current_dir));

  // Kernel commit 7bc3e6e55acf ("proc: Use a list of inodes to flush from
  // proc"), present in 5.6 and later, changed how procfs inodes are cleaned up
  // when a process exits. Credentials::DropFileSystemAccess() relies forking a
  // child process which shares the same file system information (using
  // clone(CLONE_FS)), and chroot()'ing to /proc/self/fdinfo/ in that process.
  // However, when that child process exits, its procfs directories are
  // unlinked, causing getcwd() to return ENOENT. getcwd() has been documented
  // as returning ENOENT when the directory has been unlinked since at least
  // 2004 (man-pages commit fea681daf).
  if (cwd) {
    if (strcmp("/", cwd))
      return false;
  } else {
    PCHECK(errno == ENOENT);
  }

  // The current directory is the root. Add a few paranoid checks.
  struct stat current;
  CHECK_EQ(0, stat(".", &current));
  struct stat parrent;
  CHECK_EQ(0, stat("..", &parrent));
  CHECK_EQ(current.st_dev, parrent.st_dev);
  CHECK_EQ(current.st_ino, parrent.st_ino);
  CHECK_EQ(current.st_mode, parrent.st_mode);
  CHECK_EQ(current.st_uid, parrent.st_uid);
  CHECK_EQ(current.st_gid, parrent.st_gid);
  return true;
}

SANDBOX_TEST(Credentials, DropAllCaps) {
  CHECK(Credentials::DropAllCapabilities());
  CHECK(!Credentials::HasAnyCapability());
}

SANDBOX_TEST(Credentials, MoveToNewUserNS) {
  CHECK(Credentials::DropAllCapabilities());
  bool moved_to_new_ns = Credentials::MoveToNewUserNS();
  fprintf(stdout,
          "Unprivileged CLONE_NEWUSER supported: %s\n",
          moved_to_new_ns ? "true." : "false.");
  fflush(stdout);
  if (!moved_to_new_ns) {
    fprintf(stdout, "This kernel does not support unprivileged namespaces. "
            "USERNS tests will succeed without running.\n");
    fflush(stdout);
    return;
  }
  CHECK(Credentials::HasAnyCapability());
  CHECK(Credentials::DropAllCapabilities());
  CHECK(!Credentials::HasAnyCapability());
}

SANDBOX_TEST(Credentials, CanCreateProcessInNewUserNS) {
  CHECK(Credentials::DropAllCapabilities());
  bool user_ns_supported = Credentials::CanCreateProcessInNewUserNS();
  bool moved_to_new_ns = Credentials::MoveToNewUserNS();
  CHECK_EQ(user_ns_supported, moved_to_new_ns);
}

SANDBOX_TEST(Credentials, UidIsPreserved) {
  CHECK(Credentials::DropAllCapabilities());
  uid_t old_ruid, old_euid, old_suid;
  gid_t old_rgid, old_egid, old_sgid;
  PCHECK(0 == getresuid(&old_ruid, &old_euid, &old_suid));
  PCHECK(0 == getresgid(&old_rgid, &old_egid, &old_sgid));
  // Probably missing kernel support.
  if (!Credentials::MoveToNewUserNS()) return;
  uid_t new_ruid, new_euid, new_suid;
  PCHECK(0 == getresuid(&new_ruid, &new_euid, &new_suid));
  CHECK(old_ruid == new_ruid);
  CHECK(old_euid == new_euid);
  CHECK(old_suid == new_suid);

  gid_t new_rgid, new_egid, new_sgid;
  PCHECK(0 == getresgid(&new_rgid, &new_egid, &new_sgid));
  CHECK(old_rgid == new_rgid);
  CHECK(old_egid == new_egid);
  CHECK(old_sgid == new_sgid);
}

bool NewUserNSCycle() {
  if (!Credentials::MoveToNewUserNS() ||
      !Credentials::HasAnyCapability() ||
      !Credentials::DropAllCapabilities() ||
      Credentials::HasAnyCapability()) {
    return false;
  }
  return true;
}

SANDBOX_TEST(Credentials, NestedUserNS) {
  CHECK(Credentials::DropAllCapabilities());
  // Probably missing kernel support.
  if (!Credentials::MoveToNewUserNS()) return;
  CHECK(Credentials::DropAllCapabilities());
  // As of 3.12, the kernel has a limit of 32. See create_user_ns().
  const int kNestLevel = 10;
  for (int i = 0; i < kNestLevel; ++i) {
    CHECK(NewUserNSCycle()) << "Creating new user NS failed at iteration "
                                  << i << ".";
  }
}

// Test the WorkingDirectoryIsRoot() helper.
SANDBOX_TEST(Credentials, CanDetectRoot) {
  PCHECK(0 == chdir("/proc/"));
  CHECK(!WorkingDirectoryIsRoot());
  PCHECK(0 == chdir("/"));
  CHECK(WorkingDirectoryIsRoot());
}

// Disabled on ASAN because of crbug.com/451603.
// Disabled on MSAN due to crbug.com/1180105
SANDBOX_TEST_ALLOW_NOISE(
    Credentials,
    // TODO(crbug.com/370792794): Re-enable this test
    DISABLE_ON_SANITIZERS(DISABLED_DropFileSystemAccessIsSafe)) {
  CHECK(Credentials::HasFileSystemAccess());
  CHECK(Credentials::DropAllCapabilities());
  // Probably missing kernel support.
  if (!Credentials::MoveToNewUserNS()) return;
  CHECK(Credentials::DropFileSystemAccess(ProcUtil::OpenProc().get()));
  CHECK(!Credentials::HasFileSystemAccess());
  CHECK(WorkingDirectoryIsRoot());
  CHECK(base::IsDirectoryEmpty(base::FilePath("/")));
  // We want the chroot to never have a subdirectory. A subdirectory
  // could allow a chroot escape.
  CHECK_NE(0, mkdir("/test", 0700));
}

// Check that after dropping filesystem access and dropping privileges
// it is not possible to regain capabilities.
// Disabled on MSAN due to crbug.com/1180105
SANDBOX_TEST(Credentials, DISABLE_ON_SANITIZERS(CannotRegainPrivileges)) {
  base::ScopedFD proc_fd(ProcUtil::OpenProc());
  CHECK(Credentials::DropAllCapabilities(proc_fd.get()));
  // Probably missing kernel support.
  if (!Credentials::MoveToNewUserNS()) return;
  CHECK(Credentials::DropFileSystemAccess(proc_fd.get()));
  CHECK(Credentials::DropAllCapabilities(proc_fd.get()));

  // The kernel should now prevent us from regaining capabilities because we
  // are in a chroot.
  CHECK(!Credentials::CanCreateProcessInNewUserNS());
  CHECK(!Credentials::MoveToNewUserNS());
}

SANDBOX_TEST(Credentials, SetCapabilities) {
  // Probably missing kernel support.
  if (!Credentials::MoveToNewUserNS())
    return;

  base::ScopedFD proc_fd(ProcUtil::OpenProc());

  CHECK(Credentials::HasCapability(Credentials::Capability::SYS_ADMIN));
  CHECK(Credentials::HasCapability(Credentials::Capability::SYS_CHROOT));

  std::vector<Credentials::Capability> caps;
  caps.push_back(Credentials::Capability::SYS_CHROOT);
  CHECK(Credentials::SetCapabilities(proc_fd.get(), caps));

  CHECK(!Credentials::HasCapability(Credentials::Capability::SYS_ADMIN));
  CHECK(Credentials::HasCapability(Credentials::Capability::SYS_CHROOT));

  const std::vector<Credentials::Capability> no_caps;
  CHECK(Credentials::SetCapabilities(proc_fd.get(), no_caps));
  CHECK(!Credentials::HasAnyCapability());
}

SANDBOX_TEST(Credentials, SetCapabilitiesAndChroot) {
  // Probably missing kernel support.
  if (!Credentials::MoveToNewUserNS())
    return;

  base::ScopedFD proc_fd(ProcUtil::OpenProc());

  CHECK(Credentials::HasCapability(Credentials::Capability::SYS_CHROOT));
  PCHECK(chroot("/") == 0);

  std::vector<Credentials::Capability> caps;
  caps.push_back(Credentials::Capability::SYS_CHROOT);
  CHECK(Credentials::SetCapabilities(proc_fd.get(), caps));
  PCHECK(chroot("/") == 0);

  CHECK(Credentials::DropAllCapabilities());
  PCHECK(chroot("/") == -1 && errno == EPERM);
}

SANDBOX_TEST(Credentials, SetCapabilitiesMatchesLibCap2) {
  // Probably missing kernel support.
  if (!Credentials::MoveToNewUserNS())
    return;

  base::ScopedFD proc_fd(ProcUtil::OpenProc());

  std::vector<Credentials::Capability> caps;
  caps.push_back(Credentials::Capability::SYS_CHROOT);
  CHECK(Credentials::SetCapabilities(proc_fd.get(), caps));

  ScopedCap actual_cap(cap_get_proc());
  PCHECK(actual_cap != nullptr);

  ScopedCap expected_cap(cap_init());
  PCHECK(expected_cap != nullptr);

  const cap_value_t allowed_cap = CAP_SYS_CHROOT;
  for (const cap_flag_t flag : {CAP_EFFECTIVE, CAP_PERMITTED}) {
    PCHECK(cap_set_flag(expected_cap.get(), flag, 1, &allowed_cap, CAP_SET) ==
           0);
  }

  CHECK_EQ(0, cap_compare(expected_cap.get(), actual_cap.get()));
}

volatile sig_atomic_t signal_handler_called;
void SignalHandler(int sig) {
  signal_handler_called = 1;
}

// glibc (and some other libcs) caches the PID and TID in TLS. This test
// verifies that these values are correct after DropFilesystemAccess.
// Disabled on ASAN because of crbug.com/451603.
// Disabled on MSAN due to crbug.com/1180105
SANDBOX_TEST(Credentials,
             DISABLE_ON_SANITIZERS(DropFileSystemAccessPreservesTLS)) {
  // Probably missing kernel support.
  if (!Credentials::MoveToNewUserNS()) return;
  CHECK(Credentials::DropFileSystemAccess(ProcUtil::OpenProc().get()));

  // The libc getpid implementation may return a cached PID. Ensure that
  // it matches the PID returned from the getpid system call.
  CHECK_EQ(sys_getpid(), getpid());

  // raise uses the cached TID in glibc.
  struct sigaction action = {};
  action.sa_handler = &SignalHandler;
  PCHECK(sigaction(SIGUSR1, &action, nullptr) == 0);

  PCHECK(raise(SIGUSR1) == 0);
  CHECK_EQ(1, signal_handler_called);
}

}  // namespace.

}  // namespace sandbox.
