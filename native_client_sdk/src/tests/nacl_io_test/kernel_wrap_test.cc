// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The linux/mac host build of nacl_io can't do wrapping of syscalls so all
// these tests must be disabled.
#if !defined(__linux__) && !defined(__APPLE__)

#include <unistd.h>

#include <string>
#include <vector>

#include "gtest/gtest.h"
#include "mock_kernel_proxy.h"
#include "nacl_io/kernel_intercept.h"
#include "nacl_io/kernel_wrap.h"
#include "nacl_io/kernel_wrap_real.h"
#include "nacl_io/osmman.h"
#include "nacl_io/ossocket.h"
#include "nacl_io/ostermios.h"
#include "nacl_io/ostime.h"
#include "ppapi_simple/ps.h"

#if defined(__native_client__) && !defined(__GLIBC__)
extern "C" {
// TODO(sbc): remove once this gets added to the newlib toolchain headers.
int fchdir(int fd);
}
#endif

using namespace nacl_io;

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::DoAll;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::StrEq;

namespace {

#define COMPARE_FIELD(actual, expected, f)                                 \
  if (actual != expected) {                                                \
    *result_listener << "mismatch of field \"" f                           \
                        "\". "                                             \
                        "expected: " << expected << " actual: " << actual; \
    return false;                                                          \
  }

#define COMPARE_FIELD_SIMPLE(f) COMPARE_FIELD(arg->f, other->f, #f)

MATCHER_P(IsEqualToStatbuf, other, "") {
  COMPARE_FIELD_SIMPLE(st_dev);
  COMPARE_FIELD_SIMPLE(st_ino);
  COMPARE_FIELD_SIMPLE(st_mode);
  COMPARE_FIELD_SIMPLE(st_nlink);
  COMPARE_FIELD_SIMPLE(st_uid);
  COMPARE_FIELD_SIMPLE(st_gid);
  COMPARE_FIELD_SIMPLE(st_rdev);
  COMPARE_FIELD_SIMPLE(st_size);
  COMPARE_FIELD_SIMPLE(st_atime);
  COMPARE_FIELD_SIMPLE(st_mtime);
  COMPARE_FIELD_SIMPLE(st_ctime);
  return true;
}

MATCHER_P(IsEqualToUtimbuf, other, "") {
  COMPARE_FIELD(arg[0].tv_sec, other->actime, "actime");
  COMPARE_FIELD(arg[1].tv_sec, other->modtime, "modtime");
  return true;
}

MATCHER_P(IsEqualToTimeval, other, "") {
  COMPARE_FIELD(arg[0].tv_sec, other[0].tv_sec, "[0].tv_sec");
  COMPARE_FIELD(arg[0].tv_nsec, other[0].tv_usec * 1000, "[0].tv_usec");
  COMPARE_FIELD(arg[1].tv_sec, other[1].tv_sec, "[1].tv_sec");
  COMPARE_FIELD(arg[1].tv_nsec, other[1].tv_usec * 1000, "[1].tv_usec");
  return true;
}

#undef COMPARE_FIELD
#undef COMPARE_FIELD_SIMPLE


ACTION_P(SetErrno, value) {
  errno = value;
}

ACTION_P2(SetString, target, source) {
  strcpy(target, source);
}

ACTION_P(SetStat, statbuf) {
  memset(arg1, 0, sizeof(struct stat));
  arg1->st_dev = statbuf->st_dev;
  arg1->st_ino = statbuf->st_ino;
  arg1->st_mode = statbuf->st_mode;
  arg1->st_nlink = statbuf->st_nlink;
  arg1->st_uid = statbuf->st_uid;
  arg1->st_gid = statbuf->st_gid;
  arg1->st_rdev = statbuf->st_rdev;
  arg1->st_size = statbuf->st_size;
  arg1->st_atime = statbuf->st_atime;
  arg1->st_mtime = statbuf->st_mtime;
  arg1->st_ctime = statbuf->st_ctime;
}

void MakeDummyStatbuf(struct stat* statbuf) {
  memset(&statbuf[0], 0, sizeof(struct stat));
  statbuf->st_dev = 1;
  statbuf->st_ino = 2;
  statbuf->st_mode = 3;
  statbuf->st_nlink = 4;
  statbuf->st_uid = 5;
  statbuf->st_gid = 6;
  statbuf->st_rdev = 7;
  statbuf->st_size = 8;
  statbuf->st_atime = 9;
  statbuf->st_mtime = 10;
  statbuf->st_ctime = 11;
}

const mode_t kDummyMode = 0xbeef;
const int kDummyErrno = 0xfeeb;
const int kDummyInt = 0xdedbeef;
const int kDummyInt2 = 0xcabba6e;
const int kDummyInt3 = 0xf00ba4;
const int kDummyInt4 = 0xabacdba;
const size_t kDummySizeT = 0x60067e;
const char* kDummyConstChar = "foobar";
const char* kDummyConstChar2 = "g00gl3";
const char* kDummyConstChar3 = "fr00gl3";
const void* kDummyVoidPtr = "blahblah";
const uid_t kDummyUid = 1001;
const gid_t kDummyGid = 1002;

class KernelWrapTest : public ::testing::Test {
 public:
  KernelWrapTest() {}

  virtual void SetUp() {
    // Initialize the global errno value to a consistent value rather than
    // relying on its value from previous test runs.
    errno = 0;

    // Initializing the KernelProxy opens stdin/stdout/stderr.
    EXPECT_CALL(mock, open(_, _, _))
        .WillOnce(Return(0))
        .WillOnce(Return(1))
        .WillOnce(Return(2));

    // We allow write to be called any number of times, and it forwards to
    // _real_write. This prevents an infinite loop writing output if there is a
    // failure.
    ON_CALL(mock, write(_, _, _))
        .WillByDefault(Invoke(this, &KernelWrapTest::DefaultWrite));
    EXPECT_CALL(mock, write(_, _, _)).Times(AnyNumber());

#ifndef _NEWLIB_VERSION
    // Disable munmap mocking under newlib due to deadlock issues in dlmalloc
    ON_CALL(mock, munmap(_, _))
        .WillByDefault(Return(0));
    EXPECT_CALL(mock, munmap(_, _)).Times(AnyNumber());
#endif

    ASSERT_EQ(0, ki_push_state_for_testing());
    ASSERT_EQ(0, ki_init(&mock));
  }

  void TearDown() {
    // Uninitialize the kernel proxy so wrapped functions passthrough to their
    // unwrapped versions.
    ki_uninit();
  }

  MockKernelProxy mock;

 private:
  ssize_t DefaultWrite(int fd, const void* buf, size_t count) {
   assert(fd <= 2);
   size_t nwrote;
   int rtn = _real_write(fd, buf, count, &nwrote);
   if (rtn != 0) {
     errno = rtn;
     return -1;
   }
   return nwrote;
  }
};

}  // namespace

TEST_F(KernelWrapTest, access) {
  EXPECT_CALL(mock, access(kDummyConstChar, kDummyInt)) .WillOnce(Return(0));
  EXPECT_EQ(0, access(kDummyConstChar, kDummyInt));

  EXPECT_CALL(mock, access(kDummyConstChar, kDummyInt))
      .WillOnce(DoAll(SetErrno(kDummyErrno), Return(-1)));
  EXPECT_EQ(-1, access(kDummyConstChar, kDummyInt));
  EXPECT_EQ(kDummyErrno, errno);

}

TEST_F(KernelWrapTest, chdir) {
  EXPECT_CALL(mock, chdir(kDummyConstChar)).WillOnce(Return(0));
  EXPECT_EQ(0, chdir(kDummyConstChar));

  EXPECT_CALL(mock, chdir(kDummyConstChar))
    .WillOnce(DoAll(SetErrno(kDummyErrno), Return(-1)));
  EXPECT_EQ(-1, chdir(kDummyConstChar));
  ASSERT_EQ(kDummyErrno, errno);
}

TEST_F(KernelWrapTest, chmod) {
  EXPECT_CALL(mock, chmod(kDummyConstChar, kDummyMode))
      .WillOnce(DoAll(SetErrno(kDummyErrno), Return(-1)));
  EXPECT_EQ(-1, chmod(kDummyConstChar, kDummyMode));
  ASSERT_EQ(kDummyErrno, errno);
}

TEST_F(KernelWrapTest, chown) {
  EXPECT_CALL(mock, chown(kDummyConstChar, kDummyUid, kDummyGid))
      .WillOnce(DoAll(SetErrno(kDummyErrno), Return(-1)));
  EXPECT_EQ(-1, chown(kDummyConstChar, kDummyUid, kDummyGid));
  ASSERT_EQ(kDummyErrno, errno);
}

TEST_F(KernelWrapTest, close) {
  // The way we wrap close does not support returning arbitrary values, so we
  // test 0 and -1.
  EXPECT_CALL(mock, close(kDummyInt))
      .WillOnce(Return(0));

  EXPECT_EQ(0, close(kDummyInt));

  EXPECT_CALL(mock, close(kDummyInt))
      .WillOnce(DoAll(SetErrno(kDummyErrno), Return(-1)));
  EXPECT_EQ(-1, close(kDummyInt));
  ASSERT_EQ(kDummyErrno, errno);
}

TEST_F(KernelWrapTest, dup) {
  EXPECT_CALL(mock, dup(kDummyInt)).WillOnce(Return(kDummyInt2));
  EXPECT_EQ(kDummyInt2, dup(kDummyInt));
}

TEST_F(KernelWrapTest, dup2) {
  // The way we wrap dup2 does not support returning aribtrary values, only -1
  // or the value of the new fd.
  EXPECT_CALL(mock, dup2(kDummyInt, kDummyInt2))
      .WillOnce(Return(kDummyInt2))
      .WillOnce(DoAll(SetErrno(kDummyErrno), Return(-1)));

  EXPECT_EQ(kDummyInt2, dup2(kDummyInt, kDummyInt2));
  EXPECT_EQ(-1, dup2(kDummyInt, kDummyInt2));
  ASSERT_EQ(kDummyErrno, errno);
}

TEST_F(KernelWrapTest, fchdir) {
  EXPECT_CALL(mock, fchdir(kDummyInt))
      .WillOnce(DoAll(SetErrno(kDummyErrno), Return(-1)));

  EXPECT_EQ(-1, fchdir(kDummyInt));
  ASSERT_EQ(kDummyErrno, errno);
}

TEST_F(KernelWrapTest, fchmod) {
  EXPECT_CALL(mock, fchmod(kDummyInt, kDummyMode))
      .WillOnce(Return(0))
      .WillOnce(DoAll(SetErrno(kDummyErrno), Return(-1)));

  EXPECT_EQ(0, fchmod(kDummyInt, kDummyMode));
  EXPECT_EQ(-1, fchmod(kDummyInt, kDummyMode));
  ASSERT_EQ(kDummyErrno, errno);
}

TEST_F(KernelWrapTest, fchown) {
  EXPECT_CALL(mock, fchown(kDummyInt, kDummyUid, kDummyGid))
      .WillOnce(Return(kDummyInt));
  EXPECT_EQ(kDummyInt, fchown(kDummyInt, kDummyUid, kDummyGid));
}

TEST_F(KernelWrapTest, fcntl) {
  char buffer[] = "fcntl";
  EXPECT_CALL(mock, fcntl(kDummyInt, kDummyInt2, _))
      .WillOnce(Return(kDummyInt3));
  EXPECT_EQ(kDummyInt3, fcntl(kDummyInt, kDummyInt2, buffer));
}

TEST_F(KernelWrapTest, fdatasync) {
  EXPECT_CALL(mock, fdatasync(kDummyInt)).WillOnce(Return(0))
      .WillOnce(DoAll(SetErrno(kDummyErrno), Return(-1)));

  EXPECT_EQ(0, fdatasync(kDummyInt));
  EXPECT_EQ(-1, fdatasync(kDummyInt));
  ASSERT_EQ(kDummyErrno, errno);
}

TEST_F(KernelWrapTest, fstat) {
  // The way we wrap fstat does not support returning aribtrary values, only 0
  // or -1.
  struct stat in_statbuf;
  MakeDummyStatbuf(&in_statbuf);
  EXPECT_CALL(mock, fstat(kDummyInt, _))
      .WillOnce(DoAll(SetStat(&in_statbuf), Return(0)))
      .WillOnce(DoAll(SetErrno(kDummyErrno), Return(-1)));
  struct stat out_statbuf;

  EXPECT_EQ(0, fstat(kDummyInt, &out_statbuf));
  EXPECT_THAT(&in_statbuf, IsEqualToStatbuf(&out_statbuf));

  EXPECT_EQ(-1, fstat(kDummyInt, &out_statbuf));
  ASSERT_EQ(kDummyErrno, errno);
}

TEST_F(KernelWrapTest, ftruncate) {
  EXPECT_CALL(mock, ftruncate(kDummyInt, kDummyInt2))
      .WillOnce(Return(kDummyInt3));
  EXPECT_EQ(kDummyInt3, ftruncate(kDummyInt, kDummyInt2));
}

TEST_F(KernelWrapTest, fsync) {
  EXPECT_CALL(mock, fsync(kDummyInt))
      .WillOnce(DoAll(SetErrno(kDummyErrno), Return(-1)));
  EXPECT_EQ(-1, fsync(kDummyInt));
  ASSERT_EQ(kDummyErrno, errno);
}

TEST_F(KernelWrapTest, futimes) {
  struct timeval times[2] = {{123, 234}, {345, 456}};
  EXPECT_CALL(mock, futimens(kDummyInt, IsEqualToTimeval(times)))
      .WillOnce(DoAll(SetErrno(kDummyErrno), Return(-1)));
  EXPECT_EQ(-1, futimes(kDummyInt, times));
  ASSERT_EQ(kDummyErrno, errno);
}

TEST_F(KernelWrapTest, getcwd) {
  char buffer[PATH_MAX];
  char result[PATH_MAX];
  memset(buffer, 0, PATH_MAX);
  strcpy(result, "getcwd_result");
  EXPECT_CALL(mock, getcwd(buffer, kDummySizeT))
       .WillOnce(DoAll(SetString(buffer, result), Return(buffer)));
  EXPECT_STREQ(result, getcwd(buffer, kDummySizeT));
}

TEST_F(KernelWrapTest, getdents) {
#if !defined( __GLIBC__) && !defined(__BIONIC__)
  // TODO(sbc): Find a way to test the getdents wrapper under glibc.
  // It looks like the only way to exercise it is to call readdir(2).
  // There is an internal glibc function __getdents that will call the
  // IRT but that cannot be accessed from here as glibc does not export it.
  struct dirent dirent;
  EXPECT_CALL(mock, getdents(kDummyInt, &dirent, kDummyInt2))
      .WillOnce(Return(kDummyInt2));
  EXPECT_EQ(kDummyInt2, getdents(kDummyInt, &dirent, kDummyInt2));
#endif
}

// gcc gives error: getwd is deprecated.
#if defined(__GNUC__)
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
TEST_F(KernelWrapTest, getwd) {
  char result[] = "getwd_result";
  char buffer[] = "getwd";
  EXPECT_CALL(mock, getwd(buffer)).WillOnce(Return(result));
  EXPECT_EQ(result, getwd(buffer));
}
#if defined(__GNUC__)
#pragma GCC diagnostic warning "-Wdeprecated-declarations"
#endif

TEST_F(KernelWrapTest, ioctl) {
  char buffer[] = "ioctl";
  EXPECT_CALL(mock, ioctl(kDummyInt, kDummyInt2, _))
      .WillOnce(Return(kDummyInt3));
  EXPECT_EQ(kDummyInt3, ioctl(kDummyInt, kDummyInt2, buffer));
}

#if !defined(__BIONIC__)
TEST_F(KernelWrapTest, isatty) {
  EXPECT_CALL(mock, isatty(kDummyInt)).WillOnce(Return(kDummyInt2));
  EXPECT_EQ(kDummyInt2, isatty(kDummyInt));

  // This test verifies that the IRT interception wrapper for isatty
  // ignores the value of errno when isatty() returns 1.  We had a bug
  // where returning 1 from ki_isatty resulted in errno being returned
  // by the IRT interface.
  errno = kDummyInt3;
  EXPECT_CALL(mock, isatty(kDummyInt)).WillOnce(Return(1));
  EXPECT_EQ(1, isatty(kDummyInt));
}
#endif

TEST_F(KernelWrapTest, kill) {
  EXPECT_CALL(mock, kill(kDummyInt, kDummyInt2)).WillOnce(Return(kDummyInt3));
  EXPECT_EQ(kDummyInt3, kill(kDummyInt, kDummyInt2));
}

TEST_F(KernelWrapTest, lchown) {
  EXPECT_CALL(mock, lchown(kDummyConstChar, kDummyUid, kDummyGid))
      .WillOnce(Return(kDummyInt));
  EXPECT_EQ(kDummyInt, lchown(kDummyConstChar, kDummyUid, kDummyGid));
}

TEST_F(KernelWrapTest, link) {
  EXPECT_CALL(mock, link(kDummyConstChar, kDummyConstChar2))
      .WillOnce(DoAll(SetErrno(kDummyErrno), Return(-1)));
  EXPECT_EQ(-1, link(kDummyConstChar, kDummyConstChar2));
  ASSERT_EQ(kDummyErrno, errno);
}

TEST_F(KernelWrapTest, lseek) {
  EXPECT_CALL(mock, lseek(kDummyInt, kDummyInt2, kDummyInt3))
      .WillOnce(Return(kDummyInt4));
  EXPECT_EQ(kDummyInt4, lseek(kDummyInt, kDummyInt2, kDummyInt3));
}

TEST_F(KernelWrapTest, mkdir) {
#if defined(WIN32)
  EXPECT_CALL(mock, mkdir(kDummyConstChar, 0777)).WillOnce(Return(kDummyInt2));
  EXPECT_EQ(kDummyInt2, mkdir(kDummyConstChar));
#else
  EXPECT_CALL(mock, mkdir(kDummyConstChar, kDummyMode))
      .WillOnce(DoAll(SetErrno(kDummyErrno), Return(-1)));
  EXPECT_EQ(-1, mkdir(kDummyConstChar, kDummyMode));
  ASSERT_EQ(kDummyErrno, errno);
#endif
}

TEST_F(KernelWrapTest, mmap) {
  // We only wrap mmap if |flags| has the MAP_ANONYMOUS bit unset.
  int flags = kDummyInt2 & ~MAP_ANONYMOUS;

  const size_t kDummySizeT2 = 0xbadf00d;
  int dummy1 = 123;
  int dummy2 = 456;
  void* kDummyVoidPtr1 = &dummy1;
  void* kDummyVoidPtr2 = &dummy2;
  EXPECT_CALL(mock,
              mmap(kDummyVoidPtr1,
                   kDummySizeT,
                   kDummyInt,
                   flags,
                   kDummyInt3,
                   kDummySizeT2)).WillOnce(Return(kDummyVoidPtr2));
  EXPECT_EQ(kDummyVoidPtr2,
            mmap(kDummyVoidPtr1,
                 kDummySizeT,
                 kDummyInt,
                 flags,
                 kDummyInt3,
                 kDummySizeT2));
}

TEST_F(KernelWrapTest, mount) {
  EXPECT_CALL(mock,
              mount(kDummyConstChar,
                    kDummyConstChar2,
                    kDummyConstChar3,
                    kDummyInt,
                    kDummyVoidPtr)).WillOnce(Return(kDummyInt2));
  EXPECT_EQ(kDummyInt2,
            mount(kDummyConstChar,
                  kDummyConstChar2,
                  kDummyConstChar3,
                  kDummyInt,
                  kDummyVoidPtr));
}

#ifndef _NEWLIB_VERSION
// Disable munmap mocking under newlib due to deadlock in dlmalloc
TEST_F(KernelWrapTest, munmap) {
  // The way we wrap munmap, calls the "real" mmap as well as the intercepted
  // one. The result returned is from the "real" mmap.
  int dummy1 = 123;
  void* kDummyVoidPtr = &dummy1;
  size_t kDummySizeT = sizeof(kDummyVoidPtr);
  EXPECT_CALL(mock, munmap(kDummyVoidPtr, kDummySizeT));
  munmap(kDummyVoidPtr, kDummySizeT);
}
#endif

TEST_F(KernelWrapTest, open) {
  // We pass O_RDONLY because we do not want an error in flags translation
  EXPECT_CALL(mock, open(kDummyConstChar, 0, 0))
      .WillOnce(Return(kDummyInt2))
      .WillOnce(Return(kDummyInt2));

  EXPECT_EQ(kDummyInt2, open(kDummyConstChar, 0, 0));
  EXPECT_EQ(kDummyInt2, open(kDummyConstChar, 0, 0));
}

TEST_F(KernelWrapTest, pipe) {
  int fds[] = {1, 2};
  EXPECT_CALL(mock, pipe(fds)).WillOnce(Return(kDummyInt));
  EXPECT_EQ(kDummyInt, pipe(fds));
}

TEST_F(KernelWrapTest, read) {
  int dummy_value;
  void* dummy_void_ptr = &dummy_value;
  EXPECT_CALL(mock, read(kDummyInt, dummy_void_ptr, kDummyInt2))
      .WillOnce(Return(kDummyInt3));
  EXPECT_EQ(kDummyInt3, read(kDummyInt, dummy_void_ptr, kDummyInt2));
}

TEST_F(KernelWrapTest, readlink) {
  char buf[10];

  EXPECT_CALL(mock, readlink(kDummyConstChar, buf, 10))
      .WillOnce(Return(kDummyInt))
      .WillOnce(DoAll(SetErrno(kDummyErrno), Return(-1)));

  EXPECT_EQ(kDummyInt, readlink(kDummyConstChar, buf, 10));
  EXPECT_EQ(-1, readlink(kDummyConstChar, buf, 10));
  ASSERT_EQ(kDummyErrno, errno);
}

TEST_F(KernelWrapTest, remove) {
  // The remove syscall is not directly intercepted. Instead it is implemented
  // in terms of unlink()/rmdir().
  EXPECT_CALL(mock, unlink(kDummyConstChar))
       .WillOnce(DoAll(SetErrno(kDummyErrno), Return(-1)));
  EXPECT_EQ(-1, remove(kDummyConstChar));
}

TEST_F(KernelWrapTest, rename) {
  EXPECT_CALL(mock, rename(kDummyConstChar, kDummyConstChar2))
      .WillOnce(Return(0))
      .WillOnce(DoAll(SetErrno(kDummyErrno), Return(-1)));

  EXPECT_EQ(0, rename(kDummyConstChar, kDummyConstChar2));
  EXPECT_EQ(-1, rename(kDummyConstChar, kDummyConstChar2));
  ASSERT_EQ(kDummyErrno, errno);
}

TEST_F(KernelWrapTest, rmdir) {
  EXPECT_CALL(mock, rmdir(kDummyConstChar))
      .WillOnce(DoAll(SetErrno(kDummyErrno), Return(-1)));
  EXPECT_EQ(-1, rmdir(kDummyConstChar));
  ASSERT_EQ(kDummyErrno, errno);
}

static void new_handler(int) {}

TEST_F(KernelWrapTest, sigaction) {
  struct sigaction action;
  struct sigaction oaction;
  EXPECT_CALL(mock, sigaction(kDummyInt, &action, &oaction))
      .WillOnce(Return(0));
  EXPECT_EQ(0, sigaction(kDummyInt, &action, &oaction));
}

TEST_F(KernelWrapTest, sigset) {
  EXPECT_CALL(mock, sigaction(kDummyInt, _, _))
      .WillOnce(Return(0));
  EXPECT_EQ(NULL, sigset(kDummyInt, new_handler));
}

TEST_F(KernelWrapTest, signal) {
  // KernelIntercept forwards calls to signal to KernelProxy::sigset.
  EXPECT_CALL(mock, sigaction(kDummyInt, _, _))
      .WillOnce(Return(0));
  EXPECT_EQ(NULL, signal(kDummyInt, new_handler));
}

TEST_F(KernelWrapTest, stat) {
  // The way we wrap stat does not support returning aribtrary values, only 0
  // or -1.
  struct stat in_statbuf;
  MakeDummyStatbuf(&in_statbuf);
  EXPECT_CALL(mock, stat(StrEq(kDummyConstChar), _))
      .WillOnce(DoAll(SetStat(&in_statbuf), Return(0)))
      .WillOnce(DoAll(SetErrno(kDummyErrno), Return(-1)));
  struct stat out_statbuf;

  EXPECT_EQ(0, stat(kDummyConstChar, &out_statbuf));
  EXPECT_THAT(&in_statbuf, IsEqualToStatbuf(&out_statbuf));

  EXPECT_EQ(-1, stat(kDummyConstChar, &out_statbuf));
  ASSERT_EQ(kDummyErrno, errno);
}

TEST_F(KernelWrapTest, symlink) {
  EXPECT_CALL(mock, symlink(kDummyConstChar, kDummyConstChar2))
      .WillOnce(Return(kDummyInt));
  EXPECT_EQ(kDummyInt, symlink(kDummyConstChar, kDummyConstChar2));
}

#ifndef __BIONIC__
TEST_F(KernelWrapTest, tcflush) {
  EXPECT_CALL(mock, tcflush(kDummyInt, kDummyInt2))
      .WillOnce(Return(kDummyInt3));
  EXPECT_EQ(kDummyInt3, tcflush(kDummyInt, kDummyInt2));
}

TEST_F(KernelWrapTest, tcgetattr) {
  struct termios term;
  EXPECT_CALL(mock, tcgetattr(kDummyInt, &term)).WillOnce(Return(kDummyInt2));
  EXPECT_EQ(kDummyInt2, tcgetattr(kDummyInt, &term));
}

TEST_F(KernelWrapTest, tcsetattr) {
  struct termios term;
  EXPECT_CALL(mock, tcsetattr(kDummyInt, kDummyInt2, &term))
      .WillOnce(Return(kDummyInt3));
  EXPECT_EQ(kDummyInt3, tcsetattr(kDummyInt, kDummyInt2, &term));
}
#endif

TEST_F(KernelWrapTest, umount) {
  EXPECT_CALL(mock, umount(kDummyConstChar)).WillOnce(Return(kDummyInt));
  EXPECT_EQ(kDummyInt, umount(kDummyConstChar));
}

TEST_F(KernelWrapTest, truncate) {
  EXPECT_CALL(mock, truncate(kDummyConstChar, kDummyInt3))
      .WillOnce(Return(0))
      .WillOnce(DoAll(SetErrno(kDummyErrno), Return(-1)));

  EXPECT_EQ(0, truncate(kDummyConstChar, kDummyInt3));

  EXPECT_EQ(-1, truncate(kDummyConstChar, kDummyInt3));
}

TEST_F(KernelWrapTest, lstat) {
  struct stat in_statbuf;
  MakeDummyStatbuf(&in_statbuf);
  EXPECT_CALL(mock, lstat(StrEq(kDummyConstChar), _))
      .WillOnce(DoAll(SetStat(&in_statbuf), Return(0)))
      .WillOnce(DoAll(SetErrno(kDummyErrno), Return(-1)));
  struct stat out_statbuf;

  EXPECT_EQ(0, lstat(kDummyConstChar, &out_statbuf));
  EXPECT_THAT(&in_statbuf, IsEqualToStatbuf(&out_statbuf));

  EXPECT_EQ(-1, lstat(kDummyConstChar, &out_statbuf));
  ASSERT_EQ(kDummyErrno, errno);
}

TEST_F(KernelWrapTest, unlink) {
  EXPECT_CALL(mock, unlink(kDummyConstChar))
      .WillOnce(DoAll(SetErrno(kDummyErrno), Return(-1)));
  EXPECT_EQ(-1, unlink(kDummyConstChar));
  ASSERT_EQ(kDummyErrno, errno);
}

TEST_F(KernelWrapTest, utime) {
  const struct utimbuf times = {123, 456};
  EXPECT_CALL(mock, utimens(kDummyConstChar, IsEqualToUtimbuf(&times)))
      .WillOnce(Return(kDummyInt));
  EXPECT_EQ(kDummyInt, utime(kDummyConstChar, &times));
}

TEST_F(KernelWrapTest, utimes) {
  struct timeval times[2] = {{123, 234}, {345, 456}};
  EXPECT_CALL(mock, utimens(kDummyConstChar, IsEqualToTimeval(times)))
      .WillOnce(DoAll(SetErrno(kDummyErrno), Return(-1)));
  EXPECT_EQ(-1, utimes(kDummyConstChar, times));
  ASSERT_EQ(kDummyErrno, errno);
}

TEST_F(KernelWrapTest, write) {
  EXPECT_CALL(mock, write(kDummyInt, kDummyVoidPtr, kDummyInt2))
      .WillOnce(Return(kDummyInt3));
  EXPECT_EQ(kDummyInt3, write(kDummyInt, kDummyVoidPtr, kDummyInt2));
}

class KernelWrapTestUninit : public ::testing::Test {
  void SetUp() {
    ASSERT_EQ(0, ki_push_state_for_testing());
    kernel_wrap_uninit();
  }

  void TearDown() {
    kernel_wrap_init();
    ki_pop_state_for_testing();
  }
};

TEST_F(KernelWrapTestUninit, Mkdir_Uninitialised) {
  // If we are running within chrome we can't use these calls without
  // nacl_io initialized.
  if (PSGetInstanceId() != 0)
    return;
  EXPECT_EQ(0, mkdir("./foo", S_IREAD | S_IWRITE));
  EXPECT_EQ(0, rmdir("./foo"));
}

TEST_F(KernelWrapTestUninit, Getcwd_Uninitialised) {
  // If we are running within chrome we can't use these calls without
  // nacl_io initialized.
  if (PSGetInstanceId() != 0)
    return;
  char dir[PATH_MAX];
  ASSERT_NE((char*)NULL, getcwd(dir, PATH_MAX));
  // Verify that the CWD ends with 'nacl_io_test'
  const char* suffix = "nacl_io_test";
  ASSERT_GT(strlen(dir), strlen(suffix));
  ASSERT_EQ(0, strcmp(dir+strlen(dir)-strlen(suffix), suffix));
}

#if defined(PROVIDES_SOCKET_API) and !defined(__BIONIC__)
TEST_F(KernelWrapTest, poll) {
  struct pollfd fds;
  EXPECT_CALL(mock, poll(&fds, kDummyInt, kDummyInt2))
      .WillOnce(Return(kDummyInt3));
  EXPECT_EQ(kDummyInt3, poll(&fds, kDummyInt, kDummyInt2));
}

TEST_F(KernelWrapTest, select) {
  fd_set readfds;
  fd_set writefds;
  fd_set exceptfds;
  EXPECT_CALL(mock, select(kDummyInt, &readfds, &writefds, &exceptfds, NULL))
      .WillOnce(Return(kDummyInt2));
  EXPECT_EQ(kDummyInt2,
            select(kDummyInt, &readfds, &writefds, &exceptfds, NULL));
}

// Socket Functions
TEST_F(KernelWrapTest, accept) {
  struct sockaddr addr;
  socklen_t len;
  EXPECT_CALL(mock, accept(kDummyInt, &addr, &len))
      .WillOnce(Return(kDummyInt2));
  EXPECT_EQ(kDummyInt2, accept(kDummyInt, &addr, &len));
}

TEST_F(KernelWrapTest, bind) {
  // The way we wrap bind does not support returning arbitrary values, so we
  // test 0 and -1.
  struct sockaddr addr;
  EXPECT_CALL(mock, bind(kDummyInt, &addr, kDummyInt2))
      .WillOnce(Return(0))
      .WillOnce(DoAll(SetErrno(kDummyErrno), Return(-1)));
  EXPECT_EQ(0, bind(kDummyInt, &addr, kDummyInt2));
  EXPECT_EQ(-1, bind(kDummyInt, &addr, kDummyInt2));
  EXPECT_EQ(kDummyErrno, errno);
}

TEST_F(KernelWrapTest, connect) {
  // The way we wrap connect does not support returning arbitrary values, so we
  // test 0 and -1.
  struct sockaddr addr;
  EXPECT_CALL(mock, connect(kDummyInt, &addr, kDummyInt2))
      .WillOnce(Return(0))
      .WillOnce(DoAll(SetErrno(kDummyErrno), Return(-1)));
  EXPECT_EQ(0, connect(kDummyInt, &addr, kDummyInt2));
  EXPECT_EQ(-1, connect(kDummyInt, &addr, kDummyInt2));
  EXPECT_EQ(kDummyErrno, errno);
}

TEST_F(KernelWrapTest, gethostbyname) {
  struct hostent result;
  EXPECT_CALL(mock, gethostbyname(kDummyConstChar)).WillOnce(Return(&result));
  EXPECT_EQ(&result, gethostbyname(kDummyConstChar));
}

TEST_F(KernelWrapTest, getpeername) {
  // The way we wrap getpeername does not support returning arbitrary values,
  // so we test 0 and -1.
  struct sockaddr addr;
  socklen_t len;
  EXPECT_CALL(mock, getpeername(kDummyInt, &addr, &len))
      .WillOnce(Return(0))
      .WillOnce(DoAll(SetErrno(kDummyErrno), Return(-1)));
  EXPECT_EQ(0, getpeername(kDummyInt, &addr, &len));
  EXPECT_EQ(-1, getpeername(kDummyInt, &addr, &len));
  EXPECT_EQ(kDummyErrno, errno);
}

TEST_F(KernelWrapTest, getsockname) {
  // The way we wrap getsockname does not support returning arbitrary values,
  // so we test 0 and -1.
  struct sockaddr addr;
  socklen_t len;

  EXPECT_CALL(mock, getsockname(kDummyInt, &addr, &len))
      .WillOnce(Return(0))
      .WillOnce(DoAll(SetErrno(kDummyErrno), Return(-1)));
  EXPECT_EQ(0, getsockname(kDummyInt, &addr, &len));
  EXPECT_EQ(-1, getsockname(kDummyInt, &addr, &len));
  EXPECT_EQ(kDummyErrno, errno);
}

TEST_F(KernelWrapTest, getsockopt) {
  // The way we wrap getsockname does not support returning arbitrary values,
  // so we test 0 and -1.
  int dummy_val;
  void* dummy_void_ptr = &dummy_val;
  socklen_t len;
  EXPECT_CALL(
      mock, getsockopt(kDummyInt, kDummyInt2, kDummyInt3, dummy_void_ptr, &len))
      .WillOnce(Return(0))
      .WillOnce(DoAll(SetErrno(kDummyErrno), Return(-1)));
  EXPECT_EQ(
      0,
      getsockopt(kDummyInt, kDummyInt2, kDummyInt3, dummy_void_ptr, &len));
  EXPECT_EQ(
      -1,
      getsockopt(kDummyInt, kDummyInt2, kDummyInt3, dummy_void_ptr, &len));
  EXPECT_EQ(kDummyErrno, errno);
}

TEST_F(KernelWrapTest, listen) {
  // The way we wrap listen does not support returning arbitrary values, so we
  // test 0 and -1.
  EXPECT_CALL(mock, listen(kDummyInt, kDummyInt2))
      .WillOnce(Return(0))
      .WillOnce(DoAll(SetErrno(kDummyErrno), Return(-1)));
  EXPECT_EQ(0, listen(kDummyInt, kDummyInt2));
  EXPECT_EQ(-1, listen(kDummyInt, kDummyInt2));
  EXPECT_EQ(kDummyErrno, errno);
}

TEST_F(KernelWrapTest, recv) {
  int dummy_val;
  void* dummy_void_ptr = &dummy_val;
  EXPECT_CALL(mock, recv(kDummyInt, dummy_void_ptr, kDummySizeT, kDummyInt2))
      .WillOnce(Return(kDummyInt3));
  EXPECT_EQ(kDummyInt3,
            recv(kDummyInt, dummy_void_ptr, kDummySizeT, kDummyInt2));
}

TEST_F(KernelWrapTest, recvfrom) {
  int dummy_val;
  void* dummy_void_ptr = &dummy_val;
  struct sockaddr addr;
  socklen_t len;
  EXPECT_CALL(
      mock,
      recvfrom(kDummyInt, dummy_void_ptr, kDummyInt2, kDummyInt3, &addr, &len))
      .WillOnce(Return(kDummyInt4));
  EXPECT_EQ(
      kDummyInt4,
      recvfrom(kDummyInt, dummy_void_ptr, kDummyInt2, kDummyInt3, &addr, &len));
}

#ifndef __BIONIC__
TEST_F(KernelWrapTest, recvmsg) {
  struct msghdr msg;
  EXPECT_CALL(mock, recvmsg(kDummyInt, &msg, kDummyInt2))
      .WillOnce(Return(kDummyInt3));
  EXPECT_EQ(kDummyInt3, recvmsg(kDummyInt, &msg, kDummyInt2));
}
#endif

TEST_F(KernelWrapTest, send) {
  EXPECT_CALL(mock, send(kDummyInt, kDummyVoidPtr, kDummySizeT, kDummyInt2))
      .WillOnce(Return(kDummyInt3));
  EXPECT_EQ(kDummyInt3,
            send(kDummyInt, kDummyVoidPtr, kDummySizeT, kDummyInt2));
}

TEST_F(KernelWrapTest, sendto) {
  const socklen_t kDummySockLen = 0x50cc5;
  struct sockaddr addr;
  EXPECT_CALL(mock,
              sendto(kDummyInt,
                     kDummyVoidPtr,
                     kDummyInt2,
                     kDummyInt3,
                     &addr,
                     kDummySockLen)).WillOnce(Return(kDummyInt4));
  EXPECT_EQ(kDummyInt4,
            sendto(kDummyInt,
                   kDummyVoidPtr,
                   kDummyInt2,
                   kDummyInt3,
                   &addr,
                   kDummySockLen));
}

TEST_F(KernelWrapTest, sendmsg) {
  struct msghdr msg;
  EXPECT_CALL(mock, sendmsg(kDummyInt, &msg, kDummyInt2))
      .WillOnce(Return(kDummyInt3));
  EXPECT_EQ(kDummyInt3, sendmsg(kDummyInt, &msg, kDummyInt2));
}

TEST_F(KernelWrapTest, setsockopt) {
  // The way we wrap setsockopt does not support returning arbitrary values, so
  // we test 0 and -1.
  const socklen_t kDummySockLen = 0x50cc5;
  EXPECT_CALL(
      mock,
      setsockopt(
          kDummyInt, kDummyInt2, kDummyInt3, kDummyVoidPtr, kDummySockLen))
      .WillOnce(Return(0))
      .WillOnce(DoAll(SetErrno(kDummyErrno), Return(-1)));
  EXPECT_EQ(
      0,
      setsockopt(
          kDummyInt, kDummyInt2, kDummyInt3, kDummyVoidPtr, kDummySockLen));
  EXPECT_EQ(
      -1,
      setsockopt(
          kDummyInt, kDummyInt2, kDummyInt3, kDummyVoidPtr, kDummySockLen));
  EXPECT_EQ(kDummyErrno, errno);
}

TEST_F(KernelWrapTest, shutdown) {
  // The way we wrap shutdown does not support returning arbitrary values, so we
  // test 0 and -1.
  EXPECT_CALL(mock, shutdown(kDummyInt, kDummyInt2))
      .WillOnce(Return(0))
      .WillOnce(DoAll(SetErrno(kDummyErrno), Return(-1)));
  EXPECT_EQ(0, shutdown(kDummyInt, kDummyInt2));
  EXPECT_EQ(-1, shutdown(kDummyInt, kDummyInt2));
  EXPECT_EQ(kDummyErrno, errno);
}

TEST_F(KernelWrapTest, socket) {
  EXPECT_CALL(mock, socket(kDummyInt, kDummyInt2, kDummyInt3))
      .WillOnce(Return(kDummyInt4));
  EXPECT_EQ(kDummyInt4, socket(kDummyInt, kDummyInt2, kDummyInt3));
}

TEST_F(KernelWrapTest, socketpair) {
  // The way we wrap socketpair does not support returning arbitrary values,
  // so we test 0 and -1.
  int dummy_val;
  EXPECT_CALL(mock, socketpair(kDummyInt, kDummyInt2, kDummyInt3, &dummy_val))
      .WillOnce(Return(0))
      .WillOnce(DoAll(SetErrno(kDummyErrno), Return(-1)));
  EXPECT_EQ(0, socketpair(kDummyInt, kDummyInt2, kDummyInt3, &dummy_val));
  EXPECT_EQ(-1, socketpair(kDummyInt, kDummyInt2, kDummyInt3, &dummy_val));
  EXPECT_EQ(kDummyErrno, errno);
}

#endif  // PROVIDES_SOCKET_API

#endif  // __linux__
