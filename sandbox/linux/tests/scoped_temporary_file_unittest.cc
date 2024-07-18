// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "sandbox/linux/tests/scoped_temporary_file.h"

#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <string>

#include "base/files/scoped_file.h"
#include "base/posix/eintr_wrapper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sandbox {

namespace {

bool FullWrite(int fd, const char* buffer, size_t count) {
  while (count > 0) {
    const ssize_t transfered = HANDLE_EINTR(write(fd, buffer, count));
    if (transfered <= 0 || static_cast<size_t>(transfered) > count) {
      return false;
    }
    count -= transfered;
    buffer += transfered;
  }
  return true;
}

bool FullRead(int fd, char* buffer, size_t count) {
  while (count > 0) {
    const ssize_t transfered = HANDLE_EINTR(read(fd, buffer, count));
    if (transfered <= 0 || static_cast<size_t>(transfered) > count) {
      return false;
    }
    count -= transfered;
    buffer += transfered;
  }
  return true;
}

TEST(ScopedTemporaryFile, Basics) {
  std::string temp_file_name;
  {
    ScopedTemporaryFile temp_file_1;
    const char kTestString[] = "This is a test";
    ASSERT_LE(0, temp_file_1.fd());

    temp_file_name = temp_file_1.full_file_name();
    base::ScopedFD temp_file_2(open(temp_file_1.full_file_name(), O_RDONLY));
    ASSERT_TRUE(temp_file_2.is_valid());

    ASSERT_TRUE(FullWrite(temp_file_1.fd(), kTestString, sizeof(kTestString)));

    char test_string_read[sizeof(kTestString)] = {0};
    ASSERT_TRUE(FullRead(
        temp_file_2.get(), test_string_read, sizeof(test_string_read)));
    ASSERT_EQ(0, memcmp(kTestString, test_string_read, sizeof(kTestString)));
  }

  errno = 0;
  struct stat buf;
  ASSERT_EQ(-1, stat(temp_file_name.c_str(), &buf));
  ASSERT_EQ(ENOENT, errno);
}

}  // namespace

}  // namespace sandbox
