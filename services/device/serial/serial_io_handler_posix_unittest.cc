// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "services/device/serial/serial_io_handler_posix.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace device {

class SerialIoHandlerPosixTest : public testing::Test {
 public:
  SerialIoHandlerPosixTest() = default;

  SerialIoHandlerPosixTest(const SerialIoHandlerPosixTest&) = delete;
  SerialIoHandlerPosixTest& operator=(const SerialIoHandlerPosixTest&) = delete;

  void SetUp() override {
    serial_io_handler_posix_ =
        new SerialIoHandlerPosix(base::FilePath("dummy-port"), nullptr);
  }

  void Initialize(bool parity_check_enabled,
                  const char* chars_stashed,
                  size_t num_chars_stashed) {
    serial_io_handler_posix_->error_detect_state_ = ErrorDetectState::NO_ERROR;
    serial_io_handler_posix_->parity_check_enabled_ = parity_check_enabled;
    serial_io_handler_posix_->num_chars_stashed_ = num_chars_stashed;
    for (size_t i = 0; i < num_chars_stashed; ++i) {
      serial_io_handler_posix_->chars_stashed_[i] = chars_stashed[i];
    }
  }

  void TestHelper(char* buffer,
                  size_t buffer_len,
                  size_t bytes_read,
                  ErrorDetectState error_detect_state_expected,
                  const char* chars_stashed_expected,
                  size_t num_chars_stashed_expected,
                  const char* buffer_expected,
                  size_t new_bytes_read_expected,
                  bool break_detected_expected,
                  bool parity_error_detected_expected) {
    bool break_detected = false;
    bool parity_error_detected = false;
    size_t new_bytes_read = serial_io_handler_posix_->CheckReceiveError(
        base::make_span(reinterpret_cast<uint8_t*>(buffer), buffer_len),
        bytes_read, break_detected, parity_error_detected);

    EXPECT_EQ(error_detect_state_expected,
              serial_io_handler_posix_->error_detect_state_);
    EXPECT_EQ(num_chars_stashed_expected,
              serial_io_handler_posix_->num_chars_stashed_);
    for (size_t i = 0; i < num_chars_stashed_expected; ++i) {
      EXPECT_EQ(chars_stashed_expected[i],
                static_cast<char>(serial_io_handler_posix_->chars_stashed_[i]));
    }
    EXPECT_EQ(new_bytes_read_expected, new_bytes_read);
    for (size_t i = 0; i < new_bytes_read_expected; ++i) {
      EXPECT_EQ(buffer_expected[i], buffer[i]);
    }
    EXPECT_EQ(break_detected_expected, break_detected);
    EXPECT_EQ(parity_error_detected_expected, parity_error_detected);
  }

 protected:
  scoped_refptr<SerialIoHandlerPosix> serial_io_handler_posix_;
};

// 'a' 'b' 'c'
TEST_F(SerialIoHandlerPosixTest, NoErrorReadOnce) {
  for (size_t buffer_len = 3; buffer_len <= 20; ++buffer_len) {
    Initialize(true, "", 0);

    char buffer[30] = {'a', 'b', 'c'};
    size_t bytes_read = 3;
    TestHelper(buffer, buffer_len, bytes_read, ErrorDetectState::NO_ERROR, "",
               0, "abc", 3, false, false);
  }
}

// 'a' 'b'
// 'c'
TEST_F(SerialIoHandlerPosixTest, NoErrorReadTwiceBytesReadTwoAndOne) {
  for (size_t buffer_len = 2; buffer_len <= 20; ++buffer_len) {
    Initialize(true, "", 0);

    char buffer_1[30] = {'a', 'b'};
    size_t bytes_read = 2;
    TestHelper(buffer_1, buffer_len, bytes_read, ErrorDetectState::NO_ERROR, "",
               0, "ab", 2, false, false);

    char buffer_2[30] = {'c'};
    bytes_read = 1;
    TestHelper(buffer_2, buffer_len, bytes_read, ErrorDetectState::NO_ERROR, "",
               0, "c", 1, false, false);
  }
}

// 'a'
// 'b' c'
TEST_F(SerialIoHandlerPosixTest, NoErrorReadTwiceBytesReadOneAndTwo) {
  for (size_t buffer_len = 2; buffer_len <= 20; ++buffer_len) {
    Initialize(true, "", 0);

    char buffer_1[30] = {'a'};
    size_t bytes_read = 1;
    TestHelper(buffer_1, buffer_len, bytes_read, ErrorDetectState::NO_ERROR, "",
               0, "a", 1, false, false);

    char buffer_2[30] = {'b', 'c'};
    bytes_read = 2;
    TestHelper(buffer_2, buffer_len, bytes_read, ErrorDetectState::NO_ERROR, "",
               0, "bc", 2, false, false);
  }
}

// 'a'
// 'b'
// 'c'
TEST_F(SerialIoHandlerPosixTest, NoErrorReadThreeTimes) {
  for (size_t buffer_len = 1; buffer_len <= 20; ++buffer_len) {
    Initialize(true, "", 0);

    char buffer_1[30] = {'a'};
    size_t bytes_read = 1;
    TestHelper(buffer_1, buffer_len, bytes_read, ErrorDetectState::NO_ERROR, "",
               0, "a", 1, false, false);

    char buffer_2[30] = {'b'};
    bytes_read = 1;
    TestHelper(buffer_2, buffer_len, bytes_read, ErrorDetectState::NO_ERROR, "",
               0, "b", 1, false, false);

    char buffer_3[30] = {'c'};
    bytes_read = 1;
    TestHelper(buffer_3, buffer_len, bytes_read, ErrorDetectState::NO_ERROR, "",
               0, "c", 1, false, false);
  }
}

// '\377' '\0' '\0'
TEST_F(SerialIoHandlerPosixTest, BreakReadOnce) {
  for (size_t buffer_len = 3; buffer_len <= 20; ++buffer_len) {
    Initialize(true, "", 0);

    char buffer[30] = {'\377', '\0', '\0'};
    size_t bytes_read = 3;
    TestHelper(buffer, buffer_len, bytes_read, ErrorDetectState::NO_ERROR, "",
               0, "", 0, true, false);
  }
}

// 'a' 'b' '\377' '\0' '\0' 'c' 'd' 'e'
TEST_F(SerialIoHandlerPosixTest, BreakReadOnceHasBytesBeforeAndAfterBreak) {
  for (size_t buffer_len = 8; buffer_len <= 20; ++buffer_len) {
    Initialize(true, "", 0);

    char buffer[30] = {'a', 'b', '\377', '\0', '\0', 'c', 'd', 'e'};
    size_t bytes_read = 8;
    TestHelper(buffer, buffer_len, bytes_read, ErrorDetectState::NO_ERROR, "",
               0, "abcde", 5, true, false);
  }
}

// '\377' '\0'
// '\0'
TEST_F(SerialIoHandlerPosixTest, BreakReadTwiceBytesReadTwoAndOne) {
  for (size_t buffer_len = 2; buffer_len <= 20; ++buffer_len) {
    Initialize(true, "", 0);

    char buffer_1[30] = {'\377', '\0'};
    size_t bytes_read = 2;
    TestHelper(buffer_1, buffer_len, bytes_read, ErrorDetectState::MARK_0_SEEN,
               "\377\0", 2, "", 0, false, false);

    char buffer_2[30] = {'\0'};
    bytes_read = 1;
    TestHelper(buffer_2, buffer_len, bytes_read, ErrorDetectState::NO_ERROR, "",
               0, "", 0, true, false);
  }
}

// 'a' 'b' 'c' '\377' '\0'
// '\0' 'd' 'e'
TEST_F(SerialIoHandlerPosixTest,
       BreakReadTwiceBytesReadTwoAndOneHasBytesBeforeAndAfterBreak) {
  for (size_t buffer_len = 5; buffer_len <= 20; ++buffer_len) {
    Initialize(true, "", 0);

    char buffer_1[30] = {'a', 'b', 'c', '\377', '\0'};
    size_t bytes_read = 5;
    TestHelper(buffer_1, buffer_len, bytes_read, ErrorDetectState::MARK_0_SEEN,
               "\377\0", 2, "abc", 3, false, false);

    char buffer_2[30] = {'\0', 'd', 'e'};
    bytes_read = 3;
    TestHelper(buffer_2, buffer_len, bytes_read, ErrorDetectState::NO_ERROR, "",
               0, "de", 2, true, false);
  }
}

// '\377'
// '\0' '\0'
TEST_F(SerialIoHandlerPosixTest, BreakReadTwiceBytesReadOneAndTwo) {
  for (size_t buffer_len = 2; buffer_len <= 20; ++buffer_len) {
    Initialize(true, "", 0);

    char buffer_1[30] = {'\377'};
    size_t bytes_read = 1;
    TestHelper(buffer_1, buffer_len, bytes_read,
               ErrorDetectState::MARK_377_SEEN, "\377", 1, "", 0, false, false);

    char buffer_2[30] = {'\0', '\0'};
    bytes_read = 2;
    TestHelper(buffer_2, buffer_len, bytes_read, ErrorDetectState::NO_ERROR, "",
               0, "", 0, true, false);
  }
}

// 'a' 'b' '\377'
// '\0' '\0' 'c'
TEST_F(SerialIoHandlerPosixTest,
       BreakReadTwiceBytesReadOneAndTwoHasBytesBeforeAndAfterBreak) {
  for (size_t buffer_len = 3; buffer_len <= 20; ++buffer_len) {
    Initialize(true, "", 0);

    char buffer_1[30] = {'a', 'b', '\377'};
    size_t bytes_read = 3;
    TestHelper(buffer_1, buffer_len, bytes_read,
               ErrorDetectState::MARK_377_SEEN, "\377", 1, "ab", 2, false,
               false);

    char buffer_2[30] = {'\0', '\0', 'c'};
    bytes_read = 3;
    TestHelper(buffer_2, buffer_len, bytes_read, ErrorDetectState::NO_ERROR, "",
               0, "c", 1, true, false);
  }
}

// '\377'
// '\0'
// '\0'
TEST_F(SerialIoHandlerPosixTest, BreakReadThreeTimes) {
  for (size_t buffer_len = 1; buffer_len <= 20; ++buffer_len) {
    Initialize(true, "", 0);

    char buffer_1[30] = {'\377'};
    size_t bytes_read = 1;
    TestHelper(buffer_1, buffer_len, bytes_read,
               ErrorDetectState::MARK_377_SEEN, "\377", 1, "", 0, false, false);

    char buffer_2[30] = {'\0'};
    bytes_read = 1;
    TestHelper(buffer_2, buffer_len, bytes_read, ErrorDetectState::MARK_0_SEEN,
               "\377\0", 2, "", 0, false, false);

    char buffer_3[30] = {'\0'};
    bytes_read = 1;
    TestHelper(buffer_3, buffer_len, bytes_read, ErrorDetectState::NO_ERROR, "",
               0, "", 0, true, false);
  }
}

// 'a' '\377'
// '\0'
// '\0' 'b'
TEST_F(SerialIoHandlerPosixTest,
       BreakReadThreeTimesHasBytesBeforeAndAfterBreak) {
  for (size_t buffer_len = 2; buffer_len <= 20; ++buffer_len) {
    Initialize(true, "", 0);

    char buffer_1[30] = {'a', '\377'};
    size_t bytes_read = 2;
    TestHelper(buffer_1, buffer_len, bytes_read,
               ErrorDetectState::MARK_377_SEEN, "\377", 1, "a", 1, false,
               false);

    char buffer_2[30] = {'\0'};
    bytes_read = 1;
    TestHelper(buffer_2, buffer_len, bytes_read, ErrorDetectState::MARK_0_SEEN,
               "\377\0", 2, "", 0, false, false);

    char buffer_3[30] = {'\0', 'b'};
    bytes_read = 2;
    TestHelper(buffer_3, buffer_len, bytes_read, ErrorDetectState::NO_ERROR, "",
               0, "b", 1, true, false);
  }
}

// '\377' '\0' 'a'
TEST_F(SerialIoHandlerPosixTest, ParityErrorReadOnce) {
  for (size_t buffer_len = 3; buffer_len <= 20; ++buffer_len) {
    Initialize(true, "", 0);

    char buffer[30] = {'\377', '\0', 'a'};
    size_t bytes_read = 3;
    TestHelper(buffer, buffer_len, bytes_read, ErrorDetectState::NO_ERROR, "",
               0, "", 0, false, true);
  }
}

// 'b' 'c' '\377' '\0' 'a' 'd'
TEST_F(SerialIoHandlerPosixTest,
       ParityErrorReadOnceHasBytesBeforeAndAfterParityError) {
  for (size_t buffer_len = 6; buffer_len <= 20; ++buffer_len) {
    Initialize(true, "", 0);

    char buffer[30] = {'b', 'c', '\377', '\0', 'a', 'd'};
    size_t bytes_read = 6;
    TestHelper(buffer, buffer_len, bytes_read, ErrorDetectState::NO_ERROR, "",
               0, "bcd", 3, false, true);
  }
}

// '\377' '\0'
// 'a'
TEST_F(SerialIoHandlerPosixTest, ParityErrorReadTwiceBytesReadTwoAndOne) {
  for (int buffer_len = 2; buffer_len <= 20; ++buffer_len) {
    Initialize(true, "", 0);

    char buffer_1[30] = {'\377', '\0'};
    int bytes_read = 2;
    TestHelper(buffer_1, buffer_len, bytes_read, ErrorDetectState::MARK_0_SEEN,
               "\377\0", 2, "", 0, false, false);

    char buffer_2[30] = {'a'};
    bytes_read = 1;
    TestHelper(buffer_2, buffer_len, bytes_read, ErrorDetectState::NO_ERROR, "",
               0, "", 0, false, true);
  }
}

// 'b' '\377' '\0'
// 'a' 'c' 'd'
TEST_F(
    SerialIoHandlerPosixTest,
    ParityErrorReadTwiceBytesReadTwoAndOneHasBytesBeforeAndAfterParityError) {
  for (int buffer_len = 3; buffer_len <= 20; ++buffer_len) {
    Initialize(true, "", 0);

    char buffer_1[30] = {'b', '\377', '\0'};
    int bytes_read = 3;
    TestHelper(buffer_1, buffer_len, bytes_read, ErrorDetectState::MARK_0_SEEN,
               "\377\0", 2, "b", 1, false, false);

    char buffer_2[30] = {'a', 'c', 'd'};
    bytes_read = 3;
    TestHelper(buffer_2, buffer_len, bytes_read, ErrorDetectState::NO_ERROR, "",
               0, "cd", 2, false, true);
  }
}

// '\377'
// '\0' 'a'
TEST_F(SerialIoHandlerPosixTest, ParityErrorReadTwiceBytesReadOneAndTwo) {
  for (int buffer_len = 2; buffer_len <= 20; ++buffer_len) {
    Initialize(true, "", 0);

    char buffer_1[30] = {'\377'};
    int bytes_read = 1;
    TestHelper(buffer_1, buffer_len, bytes_read,
               ErrorDetectState::MARK_377_SEEN, "\377", 1, "", 0, false, false);

    char buffer_2[30] = {'\0', 'a'};
    bytes_read = 2;
    TestHelper(buffer_2, buffer_len, bytes_read, ErrorDetectState::NO_ERROR, "",
               0, "", 0, false, true);
  }
}

// 'b' 'c' '\377'
// '\0' 'a' 'd'
TEST_F(
    SerialIoHandlerPosixTest,
    ParityErrorReadTwiceBytesReadOneAndTwoHasBytesBeforeAndAfterParityError) {
  for (int buffer_len = 3; buffer_len <= 20; ++buffer_len) {
    Initialize(true, "", 0);

    char buffer_1[30] = {'b', 'c', '\377'};
    int bytes_read = 3;
    TestHelper(buffer_1, buffer_len, bytes_read,
               ErrorDetectState::MARK_377_SEEN, "\377", 1, "bc", 2, false,
               false);

    char buffer_2[30] = {'\0', 'a', 'd'};
    bytes_read = 3;
    TestHelper(buffer_2, buffer_len, bytes_read, ErrorDetectState::NO_ERROR, "",
               0, "d", 1, false, true);
  }
}

// '\377'
// '\0'
// 'a'
TEST_F(SerialIoHandlerPosixTest, ParityErrorReadThreeTimes) {
  for (int buffer_len = 1; buffer_len <= 20; ++buffer_len) {
    Initialize(true, "", 0);

    char buffer_1[30] = {'\377'};
    int bytes_read = 1;
    TestHelper(buffer_1, buffer_len, bytes_read,
               ErrorDetectState::MARK_377_SEEN, "\377", 1, "", 0, false, false);

    char buffer_2[30] = {'\0'};
    bytes_read = 1;
    TestHelper(buffer_2, buffer_len, bytes_read, ErrorDetectState::MARK_0_SEEN,
               "\377\0", 2, "", 0, false, false);

    char buffer_3[30] = {'a'};
    bytes_read = 1;
    TestHelper(buffer_3, buffer_len, bytes_read, ErrorDetectState::NO_ERROR, "",
               0, "", 0, false, true);
  }
}

// 'b' '\377'
// '\0'
// 'a' 'c'
TEST_F(SerialIoHandlerPosixTest,
       ParityErrorReadThreeTimesHasBytesBeforeAndAfterParityError) {
  for (int buffer_len = 2; buffer_len <= 20; ++buffer_len) {
    Initialize(true, "", 0);

    char buffer_1[30] = {'b', '\377'};
    int bytes_read = 2;
    TestHelper(buffer_1, buffer_len, bytes_read,
               ErrorDetectState::MARK_377_SEEN, "\377", 1, "b", 1, false,
               false);

    char buffer_2[30] = {'\0'};
    bytes_read = 1;
    TestHelper(buffer_2, buffer_len, bytes_read, ErrorDetectState::MARK_0_SEEN,
               "\377\0", 2, "", 0, false, false);

    char buffer_3[30] = {'a', 'c'};
    bytes_read = 2;
    TestHelper(buffer_3, buffer_len, bytes_read, ErrorDetectState::NO_ERROR, "",
               0, "c", 1, false, true);
  }
}

// '\377' '\377'
TEST_F(SerialIoHandlerPosixTest, TwoEOFsReadOnce) {
  for (int buffer_len = 2; buffer_len <= 20; ++buffer_len) {
    Initialize(true, "", 0);

    char buffer[30] = {'\377', '\377'};
    int bytes_read = 2;
    TestHelper(buffer, buffer_len, bytes_read, ErrorDetectState::NO_ERROR, "",
               0, "\377", 1, false, false);
  }
}

// 'a' '\377' '\377' 'b' 'c'
TEST_F(SerialIoHandlerPosixTest, TwoEOFsReadOnceHasBytesBeforeAndAfterEOF) {
  for (int buffer_len = 5; buffer_len <= 20; ++buffer_len) {
    Initialize(true, "", 0);

    char buffer[30] = {'a', '\377', '\377', 'b', 'c'};
    int bytes_read = 5;
    TestHelper(buffer, buffer_len, bytes_read, ErrorDetectState::NO_ERROR, "",
               0, "a\377bc", 4, false, false);
  }
}

// '\377'
// '\377'
TEST_F(SerialIoHandlerPosixTest, TwoEOFsReadTwice) {
  for (int buffer_len = 1; buffer_len <= 20; ++buffer_len) {
    Initialize(true, "", 0);

    char buffer_1[30] = {'\377'};
    int bytes_read = 1;
    TestHelper(buffer_1, buffer_len, bytes_read,
               ErrorDetectState::MARK_377_SEEN, "\377", 1, "", 0, false, false);

    char buffer_2[30] = {'\377'};
    bytes_read = 1;
    TestHelper(buffer_2, buffer_len, bytes_read, ErrorDetectState::NO_ERROR, "",
               0, "\377", 1, false, false);
  }
}

// 'a' '\377'
// '\377' 'b'
TEST_F(SerialIoHandlerPosixTest, TwoEOFsReadTwiceHasBytesBeforeAndAfterEOF) {
  for (int buffer_len = 2; buffer_len <= 20; ++buffer_len) {
    Initialize(true, "", 0);

    char buffer_1[30] = {'a', '\377'};
    int bytes_read = 2;
    TestHelper(buffer_1, buffer_len, bytes_read,
               ErrorDetectState::MARK_377_SEEN, "\377", 1, "a", 1, false,
               false);

    char buffer_2[30] = {'\377', 'b'};
    bytes_read = 2;
    TestHelper(buffer_2, buffer_len, bytes_read, ErrorDetectState::NO_ERROR, "",
               0, "\377b", 2, false, false);
  }
}

// '\377' '\0' 'a'
TEST_F(SerialIoHandlerPosixTest, ParityCheckDisabledReadOnce) {
  for (int buffer_len = 3; buffer_len <= 20; ++buffer_len) {
    Initialize(false, "", 0);

    char buffer[30] = {'\377', '\0', 'a'};
    int bytes_read = 3;
    TestHelper(buffer, buffer_len, bytes_read, ErrorDetectState::NO_ERROR, "",
               0, "\377\0a", 3, false, false);
  }
}

// 'b' '\377' '\0' 'a' 'c'
TEST_F(SerialIoHandlerPosixTest,
       ParityCheckDisabledReadOnceHasBytesBeforeAndAfter) {
  for (int buffer_len = 5; buffer_len <= 20; ++buffer_len) {
    Initialize(false, "", 0);

    char buffer[30] = {'b', '\377', '\0', 'a', 'c'};
    int bytes_read = 5;
    TestHelper(buffer, buffer_len, bytes_read, ErrorDetectState::NO_ERROR, "",
               0, "b\377\0ac", 5, false, false);
  }
}

// '\377' '\0'
// 'a'
TEST_F(SerialIoHandlerPosixTest,
       ParityCheckDisabledReadTwiceBytesReadTwoAndOne) {
  int buffer_len = 2;
  Initialize(false, "", 0);

  char buffer_1[30] = {'\377', '\0'};
  int bytes_read = 2;
  TestHelper(buffer_1, buffer_len, bytes_read, ErrorDetectState::MARK_0_SEEN,
             "\377\0", 2, "", 0, false, false);

  char buffer_2[30] = {'a'};
  bytes_read = 1;
  TestHelper(buffer_2, buffer_len, bytes_read, ErrorDetectState::NO_ERROR, "a",
             1, "\377\0", 2, false, false);
}

// '\377' '\0'
// 'a' 'b'
TEST_F(SerialIoHandlerPosixTest,
       ParityCheckDisabledReadTwiceBytesReadTwoAndOneHasBytesAfter) {
  int buffer_len = 2;
  Initialize(false, "", 0);

  char buffer_1[30] = {'\377', '\0'};
  int bytes_read = 2;
  TestHelper(buffer_1, buffer_len, bytes_read, ErrorDetectState::MARK_0_SEEN,
             "\377\0", 2, "", 0, false, false);

  char buffer_2[30] = {'a', 'b'};
  bytes_read = 2;
  TestHelper(buffer_2, buffer_len, bytes_read, ErrorDetectState::NO_ERROR, "ab",
             2, "\377\0", 2, false, false);
}

// '\377' '\0'
// 'a'
TEST_F(SerialIoHandlerPosixTest,
       ParityCheckDisabledReadTwiceBytesReadTwoAndOneLargerBufferLen) {
  for (int buffer_len = 3; buffer_len <= 20; ++buffer_len) {
    Initialize(false, "", 0);

    char buffer_1[30] = {'\377', '\0'};
    int bytes_read = 2;
    TestHelper(buffer_1, buffer_len, bytes_read, ErrorDetectState::MARK_0_SEEN,
               "\377\0", 2, "", 0, false, false);

    char buffer_2[30] = {'a'};
    bytes_read = 1;
    TestHelper(buffer_2, buffer_len, bytes_read, ErrorDetectState::NO_ERROR, "",
               0, "\377\0a", 3, false, false);
  }
}

// 'b' '\377' '\0'
// 'a' 'c'
TEST_F(
    SerialIoHandlerPosixTest,
    ParityCheckDisabledReadTwiceBytesReadTwoAndOneBufferLenThreeHasBytesBeforeAndAfter) {
  int buffer_len = 3;
  Initialize(false, "", 0);

  char buffer_1[30] = {'b', '\377', '\0'};
  int bytes_read = 3;
  TestHelper(buffer_1, buffer_len, bytes_read, ErrorDetectState::MARK_0_SEEN,
             "\377\0", 2, "b", 1, false, false);

  char buffer_2[30] = {'a', 'c'};
  bytes_read = 2;
  TestHelper(buffer_2, buffer_len, bytes_read, ErrorDetectState::NO_ERROR, "c",
             1, "\377\0a", 3, false, false);
}

// 'b' '\377' '\0'
// 'a' 'c'
TEST_F(
    SerialIoHandlerPosixTest,
    ParityCheckDisabledReadTwiceBytesReadTwoAndOneLargerBufferLenHasBytesBeforeAndAfter) {
  for (int buffer_len = 4; buffer_len <= 20; ++buffer_len) {
    Initialize(false, "", 0);

    char buffer_1[30] = {'b', '\377', '\0'};
    int bytes_read = 3;
    TestHelper(buffer_1, buffer_len, bytes_read, ErrorDetectState::MARK_0_SEEN,
               "\377\0", 2, "b", 1, false, false);

    char buffer_2[30] = {'a', 'c'};
    bytes_read = 2;
    TestHelper(buffer_2, buffer_len, bytes_read, ErrorDetectState::NO_ERROR, "",
               0, "\377\0ac", 4, false, false);
  }
}

// '\377'
// '\0' 'a'
TEST_F(SerialIoHandlerPosixTest,
       ParityCheckDisabledReadTwiceBytesReadOneAndTwo) {
  int buffer_len = 2;
  Initialize(false, "", 0);

  char buffer_1[30] = {'\377'};
  int bytes_read = 1;
  TestHelper(buffer_1, buffer_len, bytes_read, ErrorDetectState::MARK_377_SEEN,
             "\377", 1, "", 0, false, false);

  char buffer_2[30] = {'\0', 'a'};
  bytes_read = 2;
  TestHelper(buffer_2, buffer_len, bytes_read, ErrorDetectState::NO_ERROR, "a",
             1, "\377\0", 2, false, false);

  char buffer_3[30];
  bytes_read = 0;
  TestHelper(buffer_3, buffer_len, bytes_read, ErrorDetectState::NO_ERROR, "",
             0, "a", 1, false, false);
}

// 'b' '\377'
// '\0' 'a'
TEST_F(SerialIoHandlerPosixTest,
       ParityCheckDisabledReadTwiceBytesReadOneAndTwoHasBytesBefore) {
  int buffer_len = 2;
  Initialize(false, "", 0);

  char buffer_1[30] = {'b', '\377'};
  int bytes_read = 2;
  TestHelper(buffer_1, buffer_len, bytes_read, ErrorDetectState::MARK_377_SEEN,
             "\377", 1, "b", 1, false, false);

  char buffer_2[30] = {'\0', 'a'};
  bytes_read = 2;
  TestHelper(buffer_2, buffer_len, bytes_read, ErrorDetectState::NO_ERROR, "a",
             1, "\377\0", 2, false, false);

  char buffer_3[30];
  bytes_read = 0;
  TestHelper(buffer_3, buffer_len, bytes_read, ErrorDetectState::NO_ERROR, "",
             0, "a", 1, false, false);
}

// '\377'
// '\0' 'a'
TEST_F(SerialIoHandlerPosixTest,
       ParityCheckDisabledReadTwiceBytesReadOneAndTwoLargerBufferLen) {
  for (int buffer_len = 3; buffer_len <= 20; ++buffer_len) {
    Initialize(false, "", 0);

    char buffer_1[30] = {'\377'};
    int bytes_read = 1;
    TestHelper(buffer_1, buffer_len, bytes_read,
               ErrorDetectState::MARK_377_SEEN, "\377", 1, "", 0, false, false);

    char buffer_2[30] = {'\0', 'a'};
    bytes_read = 2;
    TestHelper(buffer_2, buffer_len, bytes_read, ErrorDetectState::NO_ERROR, "",
               0, "\377\0a", 3, false, false);
  }
}

// 'b' '\377'
// '\0' 'a' 'c'
TEST_F(
    SerialIoHandlerPosixTest,
    ParityCheckDisabledReadTwiceBytesReadOneAndTwoBufferLenThreeHasBytesBeforeAndAfter) {
  int buffer_len = 3;
  Initialize(false, "", 0);

  char buffer_1[30] = {'b', '\377'};
  int bytes_read = 2;
  TestHelper(buffer_1, buffer_len, bytes_read, ErrorDetectState::MARK_377_SEEN,
             "\377", 1, "b", 1, false, false);

  char buffer_2[30] = {'\0', 'a', 'c'};
  bytes_read = 3;
  TestHelper(buffer_2, buffer_len, bytes_read, ErrorDetectState::NO_ERROR, "c",
             1, "\377\0a", 3, false, false);
}

// 'b' '\377'
// '\0' 'a' 'c'
TEST_F(
    SerialIoHandlerPosixTest,
    ParityCheckDisabledReadTwiceBytesReadOneAndTwoLargerBufferLenHasBytesBeforeAndAfter) {
  for (int buffer_len = 4; buffer_len <= 20; ++buffer_len) {
    Initialize(false, "", 0);

    char buffer_1[30] = {'b', '\377'};
    int bytes_read = 2;
    TestHelper(buffer_1, buffer_len, bytes_read,
               ErrorDetectState::MARK_377_SEEN, "\377", 1, "b", 1, false,
               false);

    char buffer_2[30] = {'\0', 'a', 'c'};
    bytes_read = 3;
    TestHelper(buffer_2, buffer_len, bytes_read, ErrorDetectState::NO_ERROR, "",
               0, "\377\0ac", 4, false, false);
  }
}

// '\377'
// '\0'
// 'a'
TEST_F(SerialIoHandlerPosixTest,
       ParityCheckDisabledReadThreeTimesBufferLenOne) {
  int buffer_len = 1;
  Initialize(false, "", 0);

  char buffer_1[30] = {'\377'};
  int bytes_read = 1;
  TestHelper(buffer_1, buffer_len, bytes_read, ErrorDetectState::MARK_377_SEEN,
             "\377", 1, "", 0, false, false);

  char buffer_2[30] = {'\0'};
  bytes_read = 1;
  TestHelper(buffer_2, buffer_len, bytes_read, ErrorDetectState::MARK_0_SEEN,
             "\377\0", 2, "", 0, false, false);

  char buffer_3[30] = {'a'};
  bytes_read = 1;
  TestHelper(buffer_3, buffer_len, bytes_read, ErrorDetectState::NO_ERROR,
             "\0a", 2, "\377", 1, false, false);
}

// '\377'
// '\0'
// 'a'
TEST_F(SerialIoHandlerPosixTest,
       ParityCheckDisabledReadThreeTimesBufferLenTwo) {
  int buffer_len = 2;
  Initialize(false, "", 0);

  char buffer_1[30] = {'\377'};
  int bytes_read = 1;
  TestHelper(buffer_1, buffer_len, bytes_read, ErrorDetectState::MARK_377_SEEN,
             "\377", 1, "", 0, false, false);

  char buffer_2[30] = {'\0'};
  bytes_read = 1;
  TestHelper(buffer_2, buffer_len, bytes_read, ErrorDetectState::MARK_0_SEEN,
             "\377\0", 2, "", 0, false, false);

  char buffer_3[30] = {'a'};
  bytes_read = 1;
  TestHelper(buffer_3, buffer_len, bytes_read, ErrorDetectState::NO_ERROR, "a",
             1, "\377\0", 2, false, false);
}

// '\377'
// '\0'
// 'a'
TEST_F(SerialIoHandlerPosixTest,
       ParityCheckDisabledReadThreeTimesLargerBufferLen) {
  for (int buffer_len = 3; buffer_len <= 20; ++buffer_len) {
    Initialize(false, "", 0);

    char buffer_1[30] = {'\377'};
    int bytes_read = 1;
    TestHelper(buffer_1, buffer_len, bytes_read,
               ErrorDetectState::MARK_377_SEEN, "\377", 1, "", 0, false, false);

    char buffer_2[30] = {'\0'};
    bytes_read = 1;
    TestHelper(buffer_2, buffer_len, bytes_read, ErrorDetectState::MARK_0_SEEN,
               "\377\0", 2, "", 0, false, false);

    char buffer_3[30] = {'a'};
    bytes_read = 1;
    TestHelper(buffer_3, buffer_len, bytes_read, ErrorDetectState::NO_ERROR, "",
               0, "\377\0a", 3, false, false);
  }
}

// 'b' '\377'
// '\0'
// 'a' 'c'
TEST_F(SerialIoHandlerPosixTest,
       ParityCheckDisabledReadThreeTimesBufferLenThreeByteBeforeAndAfter) {
  int buffer_len = 3;
  Initialize(false, "", 0);

  char buffer_1[30] = {'b', '\377'};
  int bytes_read = 2;
  TestHelper(buffer_1, buffer_len, bytes_read, ErrorDetectState::MARK_377_SEEN,
             "\377", 1, "b", 1, false, false);

  char buffer_2[30] = {'\0'};
  bytes_read = 1;
  TestHelper(buffer_2, buffer_len, bytes_read, ErrorDetectState::MARK_0_SEEN,
             "\377\0", 2, "", 0, false, false);

  char buffer_3[30] = {'a', 'c'};
  bytes_read = 2;
  TestHelper(buffer_3, buffer_len, bytes_read, ErrorDetectState::NO_ERROR, "c",
             1, "\377\0a", 3, false, false);
}

// 'b' '\377'
// '\0'
// 'a' 'c'
TEST_F(SerialIoHandlerPosixTest,
       ParityCheckDisabledReadThreeTimesLargerBufferLenHasBytesBeforeAndAfter) {
  for (int buffer_len = 4; buffer_len <= 20; ++buffer_len) {
    Initialize(false, "", 0);

    char buffer_1[30] = {'b', '\377'};
    int bytes_read = 2;
    TestHelper(buffer_1, buffer_len, bytes_read,
               ErrorDetectState::MARK_377_SEEN, "\377", 1, "b", 1, false,
               false);

    char buffer_2[30] = {'\0'};
    bytes_read = 1;
    TestHelper(buffer_2, buffer_len, bytes_read, ErrorDetectState::MARK_0_SEEN,
               "\377\0", 2, "", 0, false, false);

    char buffer_3[30] = {'a', 'c'};
    bytes_read = 2;
    TestHelper(buffer_3, buffer_len, bytes_read, ErrorDetectState::NO_ERROR, "",
               0, "\377\0ac", 4, false, false);
  }
}

TEST_F(SerialIoHandlerPosixTest, BytesReadZero) {
  for (int buffer_len = 1; buffer_len <= 20; ++buffer_len) {
    Initialize(true, "", 0);

    char buffer[30];
    int bytes_read = 0;
    TestHelper(buffer, buffer_len, bytes_read, ErrorDetectState::NO_ERROR, "",
               0, "", 0, false, false);
  }
}

// '\377' 'a' 'b'
TEST_F(SerialIoHandlerPosixTest, InvalidSequenceReadOnce) {
  for (int buffer_len = 3; buffer_len <= 20; ++buffer_len) {
    Initialize(true, "", 0);

    char buffer[30] = {'\377', 'a', 'b'};
    int bytes_read = 3;
    TestHelper(buffer, buffer_len, bytes_read, ErrorDetectState::NO_ERROR, "",
               0, "\377ab", 3, false, false);
  }
}

// '\377' 'a'
// 'b'
TEST_F(SerialIoHandlerPosixTest, InvalidSequenceReadTwiceBytesReadTwoAndOne) {
  for (int buffer_len = 2; buffer_len <= 20; ++buffer_len) {
    Initialize(true, "", 0);

    char buffer_1[30] = {'\377', 'a'};
    int bytes_read = 2;
    TestHelper(buffer_1, buffer_len, bytes_read, ErrorDetectState::NO_ERROR, "",
               0, "\377a", 2, false, false);

    char buffer_2[30] = {'b'};
    bytes_read = 1;
    TestHelper(buffer_2, buffer_len, bytes_read, ErrorDetectState::NO_ERROR, "",
               0, "b", 1, false, false);
  }
}

// '\377'
// 'a' 'b'
TEST_F(SerialIoHandlerPosixTest, InvalidSequenceReadTwiceBytesReadOneAndTwo) {
  int buffer_len = 2;
  Initialize(true, "", 0);

  char buffer_1[30] = {'\377'};
  int bytes_read = 1;
  TestHelper(buffer_1, buffer_len, bytes_read, ErrorDetectState::MARK_377_SEEN,
             "\377", 1, "", 0, false, false);

  char buffer_2[30] = {'a', 'b'};
  bytes_read = 2;
  TestHelper(buffer_2, buffer_len, bytes_read, ErrorDetectState::NO_ERROR, "b",
             1, "\377a", 2, false, false);
}

// '\377'
// 'a' 'b'
TEST_F(SerialIoHandlerPosixTest,
       InvalidSequenceReadTwiceBytesReadOneAndTwoLargerBufferLen) {
  for (int buffer_len = 3; buffer_len <= 20; ++buffer_len) {
    Initialize(true, "", 0);

    char buffer_1[30] = {'\377'};
    int bytes_read = 1;
    TestHelper(buffer_1, buffer_len, bytes_read,
               ErrorDetectState::MARK_377_SEEN, "\377", 1, "", 0, false, false);

    char buffer_2[30] = {'a', 'b'};
    bytes_read = 2;
    TestHelper(buffer_2, buffer_len, bytes_read, ErrorDetectState::NO_ERROR, "",
               0, "\377ab", 3, false, false);
  }
}

// '\377'
// 'a'
// 'b'
TEST_F(SerialIoHandlerPosixTest, InvalidSequenceReadThreeTimes) {
  int buffer_len = 1;
  Initialize(true, "", 0);

  char buffer_1[30] = {'\377'};
  int bytes_read = 1;
  TestHelper(buffer_1, buffer_len, bytes_read, ErrorDetectState::MARK_377_SEEN,
             "\377", 1, "", 0, false, false);

  char buffer_2[30] = {'a'};
  bytes_read = 1;
  TestHelper(buffer_2, buffer_len, bytes_read, ErrorDetectState::NO_ERROR, "a",
             1, "\377", 1, false, false);

  char buffer_3[30] = {'b'};
  bytes_read = 1;
  TestHelper(buffer_3, buffer_len, bytes_read, ErrorDetectState::NO_ERROR, "b",
             1, "a", 1, false, false);
}

// '\377'
// 'a'
// 'b'
TEST_F(SerialIoHandlerPosixTest, InvalidSequenceReadThreeTimesLargerBufferLen) {
  for (int buffer_len = 2; buffer_len <= 20; ++buffer_len) {
    Initialize(true, "", 0);

    char buffer_1[30] = {'\377'};
    int bytes_read = 1;
    TestHelper(buffer_1, buffer_len, bytes_read,
               ErrorDetectState::MARK_377_SEEN, "\377", 1, "", 0, false, false);

    char buffer_2[30] = {'a'};
    bytes_read = 1;
    TestHelper(buffer_2, buffer_len, bytes_read, ErrorDetectState::NO_ERROR, "",
               0, "\377a", 2, false, false);

    char buffer_3[30] = {'b'};
    bytes_read = 1;
    TestHelper(buffer_3, buffer_len, bytes_read, ErrorDetectState::NO_ERROR, "",
               0, "b", 1, false, false);
  }
}

// 'a' 'b' 'c' '\377'
TEST_F(SerialIoHandlerPosixTest, CharsStashedPreset) {
  int buffer_len = 2;
  Initialize(true, "ab", 2);

  char buffer[30] = {'c', '\377'};
  int bytes_read = 2;
  TestHelper(buffer, buffer_len, bytes_read, ErrorDetectState::MARK_377_SEEN,
             "c\377", 2, "ab", 2, false, false);
}

// 'b' 'c' '\377' '\0' '\0' '\377' '\377' '\377' '\0' 'a' 'd' 'e'
TEST_F(SerialIoHandlerPosixTest, BreakAndEOFAndParityError) {
  int buffer_len = 12;
  Initialize(true, "", 0);

  char buffer[30] = {'b',    'c',    '\377', '\0', '\0', '\377',
                     '\377', '\377', '\0',   'a',  'd',  'e'};
  int bytes_read = 12;
  TestHelper(buffer, buffer_len, bytes_read, ErrorDetectState::NO_ERROR, "", 0,
             "bc\377de", 5, true, true);
}

// 'b' 'c' '\377' '\0' '\0' '\377'
// '\377' '\377' '\0'
// 'a' 'd' 'e'
TEST_F(SerialIoHandlerPosixTest, BreakAndEOFAndParityErrorReadThreeTimes) {
  for (int buffer_len = 6; buffer_len <= 20; ++buffer_len) {
    Initialize(true, "", 0);

    char buffer_1[30] = {'b', 'c', '\377', '\0', '\0', '\377'};
    int bytes_read = 6;
    TestHelper(buffer_1, buffer_len, bytes_read,
               ErrorDetectState::MARK_377_SEEN, "\377", 1, "bc", 2, true,
               false);

    char buffer_2[30] = {'\377', '\377', '\0'};
    bytes_read = 3;
    TestHelper(buffer_2, buffer_len, bytes_read, ErrorDetectState::MARK_0_SEEN,
               "\377\0", 2, "\377", 1, false, false);

    char buffer_3[30] = {'a', 'd', 'e'};
    bytes_read = 3;
    TestHelper(buffer_3, buffer_len, bytes_read, ErrorDetectState::NO_ERROR, "",
               0, "de", 2, false, true);
  }
}

}  // namespace device
