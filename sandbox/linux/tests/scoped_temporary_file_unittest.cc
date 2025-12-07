// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/linux/tests/scoped_temporary_file.h"

#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <array>
#include <cstdint>
#include <string>
#include <string_view>

#include "base/compiler_specific.h"
#include "base/containers/auto_spanification_helper.h"
#include "base/containers/span.h"
#include "base/files/scoped_file.h"
#include "base/numerics/safe_conversions.h"
#include "base/posix/eintr_wrapper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sandbox {

namespace {

bool FullWrite(int fd, base::span<const char> buffer) {
  while (!buffer.empty()) {
    const ssize_t transfered =
        HANDLE_EINTR(write(fd, buffer.data(), buffer.size()));
    if (transfered <= 0 || static_cast<size_t>(transfered) > buffer.size()) {
      return false;
    }
    buffer.take_first(base::checked_cast<size_t>(transfered));
  }
  return true;
}

bool FullRead(int fd, base::span<char> buffer) {
  while (!buffer.empty()) {
    const ssize_t transfered =
        HANDLE_EINTR(read(fd, buffer.data(), buffer.size()));
    if (transfered <= 0 || static_cast<size_t>(transfered) > buffer.size()) {
      return false;
    }
    buffer.take_first(base::checked_cast<size_t>(transfered));
  }
  return true;
}

TEST(ScopedTemporaryFile, Basics) {
  std::string temp_file_name;
  {
    ScopedTemporaryFile temp_file_1;
    ASSERT_LE(0, temp_file_1.fd());

    temp_file_name = temp_file_1.full_file_name();
    base::ScopedFD temp_file_2(open(temp_file_1.full_file_name(), O_RDONLY));
    ASSERT_TRUE(temp_file_2.is_valid());

    static constexpr std::string_view kTestString = "This is a test";
    ASSERT_TRUE(FullWrite(temp_file_1.fd(), kTestString));

    std::array<char, kTestString.size()> test_string_read{};
    ASSERT_TRUE(FullRead(temp_file_2.get(), test_string_read));
    ASSERT_EQ(base::span(kTestString), base::span(test_string_read));
  }

  errno = 0;
  struct stat buf;
  ASSERT_EQ(-1, stat(temp_file_name.c_str(), &buf));
  ASSERT_EQ(ENOENT, errno);
}

}  // namespace

}  // namespace sandbox
