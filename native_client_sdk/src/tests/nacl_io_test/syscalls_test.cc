// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "gtest/gtest.h"

#include "nacl_io/kernel_intercept.h"
#include "nacl_io/kernel_proxy.h"

using namespace nacl_io;

namespace {

class SyscallsTest : public ::testing::Test {
 public:
  SyscallsTest() {}

  void SetUp() {
    ASSERT_EQ(0, ki_push_state_for_testing());
    ASSERT_EQ(0, ki_init(&kp_));
    // Unmount the passthrough FS and mount a memfs.
    EXPECT_EQ(0, kp_.umount("/"));
    EXPECT_EQ(0, kp_.mount("", "/", "memfs", 0, NULL));
  }

  void TearDown() { ki_uninit(); }

 protected:
  KernelProxy kp_;
};

}  // namespace

#if defined(__native_client__) || defined(STANDALONE)

// The Linux standalone test is unique in that it calls the real Linux
// functions (realpath, mkdir, chdir, etc.), not the nacl_io functions. This is
// done to show that the tests match the behavior for a real implementation.

TEST_F(SyscallsTest, Realpath) {
  char buffer[PATH_MAX];
  int result;

#if defined(__native_client__)
  ASSERT_EQ(0, mkdir("/tmp", S_IRUSR | S_IWUSR));
#endif

  result = mkdir("/tmp/bar", S_IRUSR | S_IWUSR);
#if defined(__native_client__)
  ASSERT_EQ(0, result);
#else
  if (result == -1) {
    ASSERT_EQ(EEXIST, errno);
  } else {
    ASSERT_EQ(0, result);
  }
#endif

  int fd = open("/tmp/file", O_CREAT | O_RDWR, 0644);
  ASSERT_GT(fd, -1);
  ASSERT_EQ(0, close(fd));

  // Test absolute paths.
  EXPECT_STREQ("/", realpath("/", buffer));
  EXPECT_STREQ("/", realpath("/tmp/..", buffer));
  EXPECT_STREQ("/tmp", realpath("/tmp", buffer));
  EXPECT_STREQ("/tmp", realpath("/tmp/", buffer));
  EXPECT_STREQ("/tmp", realpath("/tmp/bar/..", buffer));
  EXPECT_STREQ("/tmp", realpath("/tmp/bar/../bar/../../tmp", buffer));
  EXPECT_STREQ("/tmp", realpath("/tmp/././", buffer));
  EXPECT_STREQ("/tmp", realpath("///tmp", buffer));
  EXPECT_STREQ("/tmp/bar", realpath("/tmp/bar", buffer));

  EXPECT_EQ(NULL, realpath("/blah", buffer));
  EXPECT_EQ(ENOENT, errno);

  EXPECT_EQ(NULL, realpath("/blah/blah", buffer));
  EXPECT_EQ(ENOENT, errno);

  EXPECT_EQ(NULL, realpath("/tmp/baz/..", buffer));
  EXPECT_EQ(ENOENT, errno);

  EXPECT_EQ(NULL, realpath("/tmp/file/", buffer));
  EXPECT_EQ(ENOTDIR, errno);

  EXPECT_EQ(NULL, realpath(NULL, buffer));
  EXPECT_EQ(EINVAL, errno);

  // Test relative paths.
  EXPECT_EQ(0, chdir("/tmp"));

  EXPECT_STREQ("/", realpath("..", buffer));
  EXPECT_STREQ("/tmp", realpath(".", buffer));
  EXPECT_STREQ("/tmp", realpath("./", buffer));
  EXPECT_STREQ("/tmp", realpath("bar/..", buffer));
  EXPECT_STREQ("/tmp", realpath("bar/../../tmp", buffer));
  EXPECT_STREQ("/tmp", realpath(".///", buffer));
  EXPECT_STREQ("/tmp/bar", realpath("bar", buffer));

  // Test when resolved_path is allocated.
  char* allocated = realpath("/tmp", NULL);
  EXPECT_STREQ("/tmp", allocated);
  free(allocated);
}

#endif  // defined(__native_client__) || defined(STANDALONE)
