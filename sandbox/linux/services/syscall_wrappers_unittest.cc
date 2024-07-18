// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "sandbox/linux/services/syscall_wrappers.h"

#include <fcntl.h>
#include <stdint.h>
#include <string.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "base/logging.h"
#include "base/memory/page_size.h"
#include "base/posix/eintr_wrapper.h"
#include "base/system/sys_info.h"
#include "build/build_config.h"
#include "sandbox/linux/system_headers/linux_signal.h"
#include "sandbox/linux/system_headers/linux_stat.h"
#include "sandbox/linux/tests/scoped_temporary_file.h"
#include "sandbox/linux/tests/test_utils.h"
#include "sandbox/linux/tests/unit_tests.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sandbox {

namespace {

TEST(SyscallWrappers, BasicSyscalls) {
  EXPECT_EQ(getpid(), sys_getpid());
}

TEST(SyscallWrappers, CloneBasic) {
  pid_t child = sys_clone(SIGCHLD);
  TestUtils::HandlePostForkReturn(child);
  EXPECT_LT(0, child);
}

TEST(SyscallWrappers, CloneParentSettid) {
  pid_t ptid = 0;
  pid_t child = sys_clone(CLONE_PARENT_SETTID | SIGCHLD, nullptr, &ptid,
                          nullptr, nullptr);
  TestUtils::HandlePostForkReturn(child);
  EXPECT_LT(0, child);
  EXPECT_EQ(child, ptid);
}

TEST(SyscallWrappers, CloneChildSettid) {
  pid_t ctid = 0;
  pid_t pid =
      sys_clone(CLONE_CHILD_SETTID | SIGCHLD, nullptr, nullptr, &ctid, nullptr);

  const int kSuccessExit = 0;
  if (0 == pid) {
    // In child.
    if (sys_getpid() == ctid)
      _exit(kSuccessExit);
    _exit(1);
  }

  ASSERT_NE(-1, pid);
  int status = 0;
  ASSERT_EQ(pid, HANDLE_EINTR(waitpid(pid, &status, 0)));
  ASSERT_TRUE(WIFEXITED(status));
  EXPECT_EQ(kSuccessExit, WEXITSTATUS(status));
}

TEST(SyscallWrappers, GetRESUid) {
  uid_t ruid, euid, suid;
  uid_t sys_ruid, sys_euid, sys_suid;
  ASSERT_EQ(0, getresuid(&ruid, &euid, &suid));
  ASSERT_EQ(0, sys_getresuid(&sys_ruid, &sys_euid, &sys_suid));
  EXPECT_EQ(ruid, sys_ruid);
  EXPECT_EQ(euid, sys_euid);
  EXPECT_EQ(suid, sys_suid);
}

TEST(SyscallWrappers, GetRESGid) {
  gid_t rgid, egid, sgid;
  gid_t sys_rgid, sys_egid, sys_sgid;
  ASSERT_EQ(0, getresgid(&rgid, &egid, &sgid));
  ASSERT_EQ(0, sys_getresgid(&sys_rgid, &sys_egid, &sys_sgid));
  EXPECT_EQ(rgid, sys_rgid);
  EXPECT_EQ(egid, sys_egid);
  EXPECT_EQ(sgid, sys_sgid);
}

TEST(SyscallWrappers, LinuxSigSet) {
  sigset_t sigset;
  ASSERT_EQ(0, sigemptyset(&sigset));
  ASSERT_EQ(0, sigaddset(&sigset, LINUX_SIGSEGV));
  ASSERT_EQ(0, sigaddset(&sigset, LINUX_SIGBUS));
  uint64_t linux_sigset = 0;
  std::memcpy(&linux_sigset, &sigset,
              std::min(sizeof(sigset), sizeof(linux_sigset)));
  EXPECT_EQ((1ULL << (LINUX_SIGSEGV - 1)) | (1ULL << (LINUX_SIGBUS - 1)),
            linux_sigset);
}

TEST(SyscallWrappers, Stat) {
  // Create a file to stat, with 12 bytes of data.
  ScopedTemporaryFile tmp_file;
  EXPECT_EQ(12, write(tmp_file.fd(), "blahblahblah", 12));

  // To test we have the correct stat structures for each kernel/platform, we
  // will right-align them on a page, with a guard page after.
  char* two_pages = static_cast<char*>(TestUtils::MapPagesOrDie(2));
  TestUtils::MprotectLastPageOrDie(two_pages, 2);
  char* page1_end = two_pages + base::GetPageSize();

  // First, check that calling stat with |stat_buf| pointing to the last byte on
  // a page causes EFAULT.
  int res = sys_stat(tmp_file.full_file_name(),
                     reinterpret_cast<struct kernel_stat*>(page1_end - 1));
  ASSERT_EQ(res, -1);
  if (res < 0 && errno == EOVERFLOW) {
    GTEST_SKIP();
  }
  ASSERT_EQ(errno, EFAULT);

  // Now, check that we have the correctly sized stat structure.
  struct kernel_stat* sb = reinterpret_cast<struct kernel_stat*>(
      page1_end - sizeof(struct kernel_stat));
  // Memset to c's so we can check the kernel zero'd the padding...
  memset(sb, 'c', sizeof(struct kernel_stat));
  res = sys_stat(tmp_file.full_file_name(), sb);
  ASSERT_EQ(res, 0);

  // Following fields may never be consistent but should be non-zero.
  // Don't trust the platform to define fields with any particular sign.
  EXPECT_NE(0u, static_cast<unsigned int>(sb->st_dev));
  EXPECT_NE(0u, static_cast<unsigned int>(sb->st_ino));
  EXPECT_NE(0u, static_cast<unsigned int>(sb->st_mode));
  EXPECT_NE(0u, static_cast<unsigned int>(sb->st_blksize));
  EXPECT_NE(0u, static_cast<unsigned int>(sb->st_blocks));

// We are the ones that made the file.
// Note: normally gid and uid overflow on backwards-compatible 32-bit systems
// and we end up with dummy uids and gids in place here.
#if defined(ARCH_CPU_64_BITS)
  EXPECT_EQ(geteuid(), sb->st_uid);
  EXPECT_EQ(getegid(), sb->st_gid);
#endif

  // Wrote 12 bytes above which should fit in one block.
  EXPECT_EQ(12u, sb->st_size);

  // Can't go backwards in time, 1500000000 was some time ago.
  EXPECT_LT(1500000000u, static_cast<unsigned int>(sb->st_atime_));
  EXPECT_LT(1500000000u, static_cast<unsigned int>(sb->st_mtime_));
  EXPECT_LT(1500000000u, static_cast<unsigned int>(sb->st_ctime_));

  // Checking the padding for good measure.
#if defined(__x86_64__)
  EXPECT_EQ(0u, sb->__pad0);
  EXPECT_EQ(0u, sb->__unused4[0]);
  EXPECT_EQ(0u, sb->__unused4[1]);
  EXPECT_EQ(0u, sb->__unused4[2]);
#elif defined(__aarch64__)
  EXPECT_EQ(0u, sb->__pad1);
  EXPECT_EQ(0, sb->__pad2);
  EXPECT_EQ(0u, sb->__unused4);
  EXPECT_EQ(0u, sb->__unused5);
#endif
}

#if defined(__NR_fstatat64)
TEST(SyscallWrappers, Stat64) {
  static_assert(sizeof(struct kernel_stat64) == sizeof(default_stat_struct),
                "This test only works on systems where the default_stat_struct "
                "is kernel_stat64");
  // Create a file to stat, with 12 bytes of data.
  ScopedTemporaryFile tmp_file;
  EXPECT_EQ(12, write(tmp_file.fd(), "blahblahblah", 12));

  // To test we have the correct stat structures for each kernel/platform, we
  // will right-align them on a page, with a guard page after.
  char* two_pages = static_cast<char*>(TestUtils::MapPagesOrDie(2));
  TestUtils::MprotectLastPageOrDie(two_pages, 2);
  char* page1_end = two_pages + base::GetPageSize();

  // First, check that calling stat with |stat_buf| pointing to the last byte on
  // a page causes EFAULT.
  int res =
      sys_fstatat64(AT_FDCWD, tmp_file.full_file_name(),
                    reinterpret_cast<struct kernel_stat64*>(page1_end - 1), 0);
  ASSERT_EQ(res, -1);
  ASSERT_EQ(errno, EFAULT);

  // Now, check that we have the correctly sized stat structure.
  struct kernel_stat64* sb = reinterpret_cast<struct kernel_stat64*>(
      page1_end - sizeof(struct kernel_stat64));
  memset(sb, 0, sizeof(struct kernel_stat64));
  res = sys_fstatat64(AT_FDCWD, tmp_file.full_file_name(), sb, 0);
  ASSERT_EQ(res, 0);

  // Following fields may never be consistent but should be non-zero.
  // Don't trust the platform to define fields with any particular sign.
  EXPECT_NE(0u, static_cast<unsigned int>(sb->st_dev));
  EXPECT_NE(0u, static_cast<unsigned int>(sb->st_ino));
  EXPECT_NE(0u, static_cast<unsigned int>(sb->st_mode));
  EXPECT_NE(0u, static_cast<unsigned int>(sb->st_blksize));
  EXPECT_NE(0u, static_cast<unsigned int>(sb->st_blocks));

  // We are the ones that made the file.
  EXPECT_EQ(geteuid(), sb->st_uid);
  EXPECT_EQ(getegid(), sb->st_gid);

  // Wrote 12 bytes above which should fit in one block.
  EXPECT_EQ(12, sb->st_size);

  // Can't go backwards in time, 1500000000 was some time ago.
  EXPECT_LT(1500000000u, static_cast<unsigned int>(sb->st_atime_));
  EXPECT_LT(1500000000u, static_cast<unsigned int>(sb->st_mtime_));
  EXPECT_LT(1500000000u, static_cast<unsigned int>(sb->st_ctime_));
}
#endif  // defined(__NR_fstatat64)

TEST(SyscallWrappers, LStat) {
  // Create a file to stat, with 12 bytes of data.
  ScopedTemporaryFile tmp_file;
  EXPECT_EQ(12, write(tmp_file.fd(), "blahblahblah", 12));

  // Also create a symlink.
  std::string symlink_name;
  {
    ScopedTemporaryFile tmp_file2;
    symlink_name = tmp_file2.full_file_name();
  }
  int rc = symlink(tmp_file.full_file_name(), symlink_name.c_str());
  if (rc != 0) {
    PLOG(ERROR) << "Couldn't symlink " << symlink_name << " to target "
                << tmp_file.full_file_name();
    GTEST_FAIL();
  }

  struct kernel_stat lstat_info;
  rc = sys_lstat(symlink_name.c_str(), &lstat_info);
  if (rc < 0 && errno == EOVERFLOW) {
    GTEST_SKIP();
  }
  if (rc != 0) {
    PLOG(ERROR) << "Couldn't sys_lstat " << symlink_name;
    GTEST_FAIL();
  }

  struct kernel_stat tmp_file_stat_info;
  rc = sys_stat(tmp_file.full_file_name(), &tmp_file_stat_info);
  if (rc < 0 && errno == EOVERFLOW) {
    GTEST_SKIP();
  }
  if (rc != 0) {
    PLOG(ERROR) << "Couldn't sys_stat " << tmp_file.full_file_name();
    GTEST_FAIL();
  }

  // lstat should produce information about a symlink.
  ASSERT_TRUE(S_ISLNK(lstat_info.st_mode));

// /tmp is mounted with nosymfollow on ChromeOS so calling
// sys_stat leads to an error.
#if BUILDFLAG(IS_CHROMEOS)
  if (base::SysInfo::IsRunningOnChromeOS()) {
    GTEST_SKIP();
  }
#endif

  struct kernel_stat stat_info;
  rc = sys_stat(symlink_name.c_str(), &stat_info);
  if (rc < 0 && errno == EOVERFLOW) {
    GTEST_SKIP();
  }
  if (rc != 0) {
    PLOG(ERROR) << "Couldn't sys_stat " << symlink_name;
    GTEST_FAIL();
  }

  // stat-ing symlink_name and tmp_file should produce the same inode.
  ASSERT_EQ(stat_info.st_ino, tmp_file_stat_info.st_ino);

  // lstat-ing symlink_name should give a different inode than stat-ing
  // symlink_name.
  ASSERT_NE(stat_info.st_ino, lstat_info.st_ino);
}

}  // namespace

}  // namespace sandbox
