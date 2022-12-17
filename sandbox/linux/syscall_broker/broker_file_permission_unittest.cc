// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/linux/syscall_broker/broker_file_permission.h"

#include <fcntl.h>
#include <string.h>
#include <sys/inotify.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "base/check.h"
#include "base/notreached.h"
#include "sandbox/linux/tests/test_utils.h"
#include "sandbox/linux/tests/unit_tests.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sandbox {
namespace syscall_broker {

class BrokerFilePermissionTester {
 public:
  BrokerFilePermissionTester(const BrokerFilePermissionTester&) = delete;
  BrokerFilePermissionTester& operator=(const BrokerFilePermissionTester&) =
      delete;

  static bool ValidatePath(const char* path) {
    return BrokerFilePermission::ValidatePath(path);
  }
  static const char* GetErrorMessage() {
    return BrokerFilePermission::GetErrorMessageForTests();
  }
};

namespace {

// Creation tests are DEATH tests as a bad permission causes termination.
SANDBOX_TEST(BrokerFilePermission, CreateGood) {
  const char kPath[] = "/tmp/good";
  BrokerFilePermission perm = BrokerFilePermission::ReadOnly(kPath);
}

SANDBOX_TEST(BrokerFilePermission, CreateGoodRecursive) {
  const char kPath[] = "/tmp/good/";
  BrokerFilePermission perm = BrokerFilePermission::ReadOnlyRecursive(kPath);
}

// In official builds, CHECK(x) causes a SIGTRAP on the architectures where the
// sanbox is enabled (that are x86, x86_64, arm64 and 32-bit arm processes
// running on a arm64 kernel).
#if defined(OFFICIAL_BUILD)
#define DEATH_BY_CHECK(msg) DEATH_BY_SIGNAL(SIGTRAP)
#else
#define DEATH_BY_CHECK(msg) DEATH_MESSAGE(msg)
#endif

SANDBOX_DEATH_TEST(
    BrokerFilePermission,
    CreateBad,
    DEATH_BY_CHECK(BrokerFilePermissionTester::GetErrorMessage())) {
  const char kPath[] = "/tmp/bad/";
  BrokerFilePermission perm = BrokerFilePermission::ReadOnly(kPath);
}

SANDBOX_DEATH_TEST(
    BrokerFilePermission,
    CreateBadRecursive,
    DEATH_BY_CHECK(BrokerFilePermissionTester::GetErrorMessage())) {
  const char kPath[] = "/tmp/bad";
  BrokerFilePermission perm = BrokerFilePermission::ReadOnlyRecursive(kPath);
}

SANDBOX_DEATH_TEST(
    BrokerFilePermission,
    CreateBadNotAbs,
    DEATH_BY_CHECK(BrokerFilePermissionTester::GetErrorMessage())) {
  const char kPath[] = "tmp/bad";
  BrokerFilePermission perm = BrokerFilePermission::ReadOnly(kPath);
}

SANDBOX_DEATH_TEST(
    BrokerFilePermission,
    CreateBadEmpty,
    DEATH_BY_CHECK(BrokerFilePermissionTester::GetErrorMessage())) {
  const char kPath[] = "";
  BrokerFilePermission perm = BrokerFilePermission::ReadOnly(kPath);
}

// CheckPerm tests |path| against |perm| given |access_flags|.
// If |create| is true then file creation is tested for success.
void CheckPerm(const BrokerFilePermission& perm,
               const char* path,
               int access_flags,
               bool create) {
  ASSERT_FALSE(perm.CheckAccess(path, X_OK));
  ASSERT_TRUE(perm.CheckAccess(path, F_OK));
  // check bad perms
  switch (access_flags) {
    case O_RDONLY:
      ASSERT_TRUE(perm.CheckOpen(path, O_RDONLY).first);
      ASSERT_FALSE(perm.CheckOpen(path, O_WRONLY).first);
      ASSERT_FALSE(perm.CheckOpen(path, O_RDWR).first);
      ASSERT_TRUE(perm.CheckAccess(path, R_OK));
      ASSERT_FALSE(perm.CheckAccess(path, W_OK));
      break;
    case O_WRONLY:
      ASSERT_FALSE(perm.CheckOpen(path, O_RDONLY).first);
      ASSERT_TRUE(perm.CheckOpen(path, O_WRONLY).first);
      ASSERT_FALSE(perm.CheckOpen(path, O_RDWR).first);
      ASSERT_FALSE(perm.CheckAccess(path, R_OK));
      ASSERT_TRUE(perm.CheckAccess(path, W_OK));
      break;
    case O_RDWR:
      ASSERT_TRUE(perm.CheckOpen(path, O_RDONLY).first);
      ASSERT_TRUE(perm.CheckOpen(path, O_WRONLY).first);
      ASSERT_TRUE(perm.CheckOpen(path, O_RDWR).first);
      ASSERT_TRUE(perm.CheckAccess(path, R_OK));
      ASSERT_TRUE(perm.CheckAccess(path, W_OK));
      break;
    default:
      // Bad test case
      NOTREACHED();
  }

// O_SYNC can be defined as (__O_SYNC|O_DSYNC)
#ifdef O_DSYNC
  const int kSyncFlag = O_SYNC & ~O_DSYNC;
#else
  const int kSyncFlag = O_SYNC;
#endif

  const int kNumberOfBitsInOAccMode = 2;
  static_assert(O_ACCMODE == ((1 << kNumberOfBitsInOAccMode) - 1),
                "incorrect number of bits");
  // check every possible flag and act accordingly.
  // Skipping AccMode bits as they are present in every case.
  for (int i = kNumberOfBitsInOAccMode; i < 32; i++) {
    int flag = 1 << i;
    switch (flag) {
      case O_APPEND:
      case O_ASYNC:
      case O_DIRECT:
      case O_DIRECTORY:
#ifdef O_DSYNC
      case O_DSYNC:
#endif
      case O_EXCL:
      case O_LARGEFILE:
      case O_NOATIME:
      case O_NOCTTY:
      case O_NOFOLLOW:
      case O_NONBLOCK:
#if (O_NONBLOCK != O_NDELAY)
      case O_NDELAY:
#endif
      case kSyncFlag:
        ASSERT_TRUE(perm.CheckOpen(path, access_flags | flag).first);
        break;
      case O_TRUNC: {
        // The effect of (O_RDONLY | O_TRUNC) is undefined, and in some cases it
        // actually truncates, so deny.
        const char* result = perm.CheckOpen(path, access_flags | flag).first;
        if (access_flags == O_RDONLY) {
          ASSERT_FALSE(result);
        } else {
          ASSERT_TRUE(result);
        }
        break;
      }
      case O_CREAT:
        continue;  // Handled below.
      case O_CLOEXEC:
      default:
        ASSERT_FALSE(perm.CheckOpen(path, access_flags | flag).first);
    }
  }
  if (create) {
    const char* result;
    bool unlink;
    std::tie(result, unlink) =
        perm.CheckOpen(path, O_CREAT | O_EXCL | access_flags);
    ASSERT_TRUE(result);
    ASSERT_FALSE(unlink);
  } else {
    ASSERT_FALSE(perm.CheckOpen(path, O_CREAT | O_EXCL | access_flags).first);
  }
}

TEST(BrokerFilePermission, ReadOnly) {
  const char kPath[] = "/tmp/good";
  BrokerFilePermission perm = BrokerFilePermission::ReadOnly(kPath);
  CheckPerm(perm, kPath, O_RDONLY, false);
  // Don't do anything here, so that ASSERT works in the subfunction as
  // expected.
}

TEST(BrokerFilePermission, ReadOnlyRecursive) {
  const char kPath[] = "/tmp/good/";
  const char kPathFile[] = "/tmp/good/file";
  BrokerFilePermission perm = BrokerFilePermission::ReadOnlyRecursive(kPath);
  CheckPerm(perm, kPathFile, O_RDONLY, false);
  // Don't do anything here, so that ASSERT works in the subfunction as
  // expected.
}

// Explicit test for O_RDONLY|O_TRUNC, which should be denied due to
// undefined behavior.
TEST(BrokerFilePermission, ReadOnlyTruncate) {
  const char kPath[] = "/tmp/good";
  BrokerFilePermission perm = BrokerFilePermission::ReadOnly(kPath);
  ASSERT_FALSE(perm.CheckOpen(kPath, O_RDONLY | O_TRUNC).first);
}

TEST(BrokerFilePermission, WriteOnly) {
  const char kPath[] = "/tmp/good";
  BrokerFilePermission perm = BrokerFilePermission::WriteOnly(kPath);
  CheckPerm(perm, kPath, O_WRONLY, false);
  // Don't do anything here, so that ASSERT works in the subfunction as
  // expected.
}

TEST(BrokerFilePermission, ReadWrite) {
  const char kPath[] = "/tmp/good";
  BrokerFilePermission perm = BrokerFilePermission::ReadWrite(kPath);
  CheckPerm(perm, kPath, O_RDWR, false);
  // Don't do anything here, so that ASSERT works in the subfunction as
  // expected.
}

TEST(BrokerFilePermission, ReadWriteCreate) {
  const char kPath[] = "/tmp/good";
  BrokerFilePermission perm = BrokerFilePermission::ReadWriteCreate(kPath);
  CheckPerm(perm, kPath, O_RDWR, true);
  // Don't do anything here, so that ASSERT works in the subfunction as
  // expected.
}

void CheckUnlink(BrokerFilePermission& perm,
                 const char* path,
                 int access_flags) {
  ASSERT_FALSE(perm.CheckOpen(path, access_flags).first);

  const char* result;
  bool unlink;
  std::tie(result, unlink) =
      perm.CheckOpen(path, access_flags | O_CREAT | O_EXCL);
  ASSERT_TRUE(result);
  ASSERT_TRUE(unlink);
}

TEST(BrokerFilePermission, ReadWriteCreateTemporaryRecursive) {
  const char kPath[] = "/tmp/good/";
  const char kPathFile[] = "/tmp/good/file";
  BrokerFilePermission perm =
      BrokerFilePermission::ReadWriteCreateTemporaryRecursive(kPath);
  CheckUnlink(perm, kPathFile, O_RDWR);
  // Don't do anything here, so that ASSERT works in the subfunction as
  // expected.
}

TEST(BrokerFilePermission, StatOnlyWithIntermediateDirs) {
  const char kPath[] = "/tmp/good/path";
  const char kLeading1[] = "/";
  const char kLeading2[] = "/tmp";
  const char kLeading3[] = "/tmp/good/path";
  const char kBadPrefix[] = "/tmp/good/pa";
  const char kTrailing[] = "/tmp/good/path/bad";

  BrokerFilePermission perm =
      BrokerFilePermission::StatOnlyWithIntermediateDirs(kPath);
  // No open or access permission.
  ASSERT_FALSE(perm.CheckOpen(kPath, O_RDONLY).first);
  ASSERT_FALSE(perm.CheckOpen(kPath, O_WRONLY).first);
  ASSERT_FALSE(perm.CheckOpen(kPath, O_RDWR).first);
  ASSERT_FALSE(perm.CheckAccess(kPath, R_OK));
  ASSERT_FALSE(perm.CheckAccess(kPath, W_OK));

  // Stat for all leading paths, but not trailing paths.
  ASSERT_TRUE(perm.CheckStatWithIntermediates(kPath));
  ASSERT_TRUE(perm.CheckStatWithIntermediates(kLeading1));
  ASSERT_TRUE(perm.CheckStatWithIntermediates(kLeading2));
  ASSERT_TRUE(perm.CheckStatWithIntermediates(kLeading3));
  ASSERT_FALSE(perm.CheckStatWithIntermediates(kBadPrefix));
  ASSERT_FALSE(perm.CheckStatWithIntermediates(kTrailing));
}

TEST(BrokerFilePermission, InotifyAddWatchWithIntermediateDirs) {
  const char kPath[] = "/tmp/good/path";
  const char kLeading1[] = "/";
  const char kLeading2[] = "/tmp";
  const char kLeading3[] = "/tmp/good/path";
  const char kBadPrefix[] = "/tmp/good/pa";
  const char kTrailing[] = "/tmp/good/path/bad";

  const uint32_t kBadMask =
      IN_CREATE | IN_DELETE | IN_CLOSE_WRITE | IN_MOVE | IN_ONLYDIR;
  const uint32_t kGoodMask = kBadMask | IN_ATTRIB;

  BrokerFilePermission perm =
      BrokerFilePermission::InotifyAddWatchWithIntermediateDirs(kPath);
  // No open or access permission.
  ASSERT_FALSE(perm.CheckOpen(kPath, O_RDONLY).first);
  ASSERT_FALSE(perm.CheckOpen(kPath, O_WRONLY).first);
  ASSERT_FALSE(perm.CheckOpen(kPath, O_RDWR).first);
  ASSERT_FALSE(perm.CheckAccess(kPath, R_OK));
  ASSERT_FALSE(perm.CheckAccess(kPath, W_OK));

  // Inotify_add_watch for all leading paths, but not trailing paths.
  ASSERT_TRUE(perm.CheckInotifyAddWatchWithIntermediates(kPath, kGoodMask));
  ASSERT_TRUE(perm.CheckInotifyAddWatchWithIntermediates(kLeading1, kGoodMask));
  ASSERT_TRUE(perm.CheckInotifyAddWatchWithIntermediates(kLeading2, kGoodMask));
  ASSERT_TRUE(perm.CheckInotifyAddWatchWithIntermediates(kLeading3, kGoodMask));
  ASSERT_FALSE(
      perm.CheckInotifyAddWatchWithIntermediates(kBadPrefix, kGoodMask));
  ASSERT_FALSE(
      perm.CheckInotifyAddWatchWithIntermediates(kTrailing, kGoodMask));

  // Fails without correct mask.
  ASSERT_FALSE(perm.CheckInotifyAddWatchWithIntermediates(kPath, kBadMask));
}

TEST(BrokerFilePermission, ValidatePath) {
  EXPECT_TRUE(BrokerFilePermissionTester::ValidatePath("/path"));
  EXPECT_TRUE(BrokerFilePermissionTester::ValidatePath("/"));
  EXPECT_TRUE(BrokerFilePermissionTester::ValidatePath("/..path"));

  EXPECT_FALSE(BrokerFilePermissionTester::ValidatePath(""));
  EXPECT_FALSE(BrokerFilePermissionTester::ValidatePath("bad"));
  EXPECT_FALSE(BrokerFilePermissionTester::ValidatePath("/bad/"));
  EXPECT_FALSE(BrokerFilePermissionTester::ValidatePath("bad/"));
  EXPECT_FALSE(BrokerFilePermissionTester::ValidatePath("/bad/.."));
  EXPECT_FALSE(BrokerFilePermissionTester::ValidatePath("/bad/../bad"));
  EXPECT_FALSE(BrokerFilePermissionTester::ValidatePath("/../bad"));
}

}  // namespace
}  // namespace syscall_broker
}  // namespace sandbox
