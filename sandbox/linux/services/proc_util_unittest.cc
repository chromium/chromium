// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/linux/services/proc_util.h"

#include <fcntl.h>
#include <unistd.h>

#include "base/files/scoped_file.h"
#include "base/posix/eintr_wrapper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sandbox {

TEST(ProcUtil, CountOpenFds) {
  base::ScopedFD proc_fd(open("/proc/", O_RDONLY | O_DIRECTORY));
  ASSERT_TRUE(proc_fd.is_valid());
  int fd_count = ProcUtil::CountOpenFds(proc_fd.get());
  int fd = open("/dev/null", O_RDONLY);
  ASSERT_LE(0, fd);
  EXPECT_EQ(fd_count + 1, ProcUtil::CountOpenFds(proc_fd.get()));
  ASSERT_EQ(0, IGNORE_EINTR(close(fd)));
  EXPECT_EQ(fd_count, ProcUtil::CountOpenFds(proc_fd.get()));
}

TEST(ProcUtil, HasOpenDirectory) {
  // No open directory should exist at startup.
  EXPECT_FALSE(ProcUtil::HasOpenDirectory());
  {
    // Have a "/proc" file descriptor around.
    int proc_fd = open("/proc/", O_RDONLY | O_DIRECTORY);
    base::ScopedFD proc_fd_closer(proc_fd);
    EXPECT_TRUE(ProcUtil::HasOpenDirectory());
  }
  EXPECT_FALSE(ProcUtil::HasOpenDirectory());
}

TEST(ProcUtil, HasOpenDirectoryWithFD) {
  int proc_fd = open("/proc/", O_RDONLY | O_DIRECTORY);
  base::ScopedFD proc_fd_closer(proc_fd);
  ASSERT_LE(0, proc_fd);

  // Don't pass |proc_fd|, an open directory (proc_fd) should
  // be detected.
  EXPECT_TRUE(ProcUtil::HasOpenDirectory());
  // Pass |proc_fd| and no open directory should be detected.
  EXPECT_FALSE(ProcUtil::HasOpenDirectory(proc_fd));

  {
    // Have a directory file descriptor around.
    int open_directory_fd = open("/proc/self/", O_RDONLY | O_DIRECTORY);
    base::ScopedFD open_directory_fd_closer(open_directory_fd);
    EXPECT_TRUE(ProcUtil::HasOpenDirectory(proc_fd));
  }

  // The "/proc/" file descriptor should now be closed, |proc_fd| is the
  // only directory file descriptor open.
  EXPECT_FALSE(ProcUtil::HasOpenDirectory(proc_fd));
}

}  // namespace sandbox
