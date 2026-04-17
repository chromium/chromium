// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/win/event_trace_data.h"

#include <string>
#include <vector>

#include "base/check.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/logging_win.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

namespace {
constexpr int kProcessId = 12345;
constexpr int kThreadId = 54321;
constexpr DWORD kStackDepth = 5;
constexpr int32_t kLineNumber = 42;
constexpr char kFilePath[] = "//remoting/host/win/awesome_stuff.cc\0";
constexpr char kFileName[] = "awesome_stuff.cc\0";
constexpr char kTestLogMessage[] = "BlerghityBlah!\0";
constexpr char kWarning[] = "WARNING";
}  // namespace

class EventTraceDataTest : public ::testing::Test {
 protected:
  void InitForLogMessage();
  void InitForLogMessageFull();
  void InitSharedFields(uint8_t type);

  size_t ReserveBufferSpace(size_t space_needed);

  FILETIME time_ = {};
  EVENT_TRACE event_trace_ = {};
  std::vector<uint8_t> buffer_;
};

void EventTraceDataTest::InitSharedFields(uint8_t type) {
  event_trace_.Header.Class.Type = type;
  event_trace_.Header.Class.Level = TRACE_LEVEL_WARNING;
  event_trace_.Header.ProcessId = kProcessId;
  event_trace_.Header.ThreadId = kThreadId;

  GetSystemTimeAsFileTime(&time_);
  event_trace_.Header.TimeStamp.LowPart = time_.dwLowDateTime;
  event_trace_.Header.TimeStamp.HighPart = time_.dwHighDateTime;

  buffer_.clear();
}

void EventTraceDataTest::InitForLogMessage() {
  InitSharedFields(static_cast<uint8_t>(logging::LOG_MESSAGE));

  size_t message_length = strlen(kTestLogMessage) + 1;
  buffer_.resize(message_length);
  UNSAFE_TODO(memcpy(buffer_.data(), kTestLogMessage, message_length));

  event_trace_.MofData = buffer_.data();
  event_trace_.MofLength = buffer_.size();
}

void EventTraceDataTest::InitForLogMessageFull() {
  InitSharedFields(static_cast<uint8_t>(logging::LOG_MESSAGE_FULL));

  // Set up the stack trace info.
  size_t data_size = sizeof(DWORD);
  size_t offset = ReserveBufferSpace(data_size);
  UNSAFE_TODO(memcpy(buffer_.data() + offset, &kStackDepth, data_size));

  // Allocate space for the 'stack trace' info.
  data_size = kStackDepth * sizeof(intptr_t);
  offset = ReserveBufferSpace(data_size);
  UNSAFE_TODO(memset(buffer_.data() + offset, 0, data_size));

  // Set the line number.
  data_size = sizeof(DWORD);
  offset = ReserveBufferSpace(data_size);
  UNSAFE_TODO(memcpy(buffer_.data() + offset, &kLineNumber, data_size));

  // Set the file path.
  data_size = strlen(kFilePath) + 1;
  offset = ReserveBufferSpace(data_size);
  UNSAFE_TODO(memcpy(buffer_.data() + offset, &kFilePath, data_size));

  // Set the log message.
  data_size = strlen(kTestLogMessage) + 1;
  offset = ReserveBufferSpace(data_size);
  UNSAFE_TODO(memcpy(buffer_.data() + offset, &kTestLogMessage, data_size));

  event_trace_.MofData = buffer_.data();
  event_trace_.MofLength = buffer_.size();
}

size_t EventTraceDataTest::ReserveBufferSpace(size_t space_needed) {
  size_t original_size = buffer_.size();
  buffer_.resize(original_size + space_needed);
  return original_size;
}

TEST_F(EventTraceDataTest, LogMessage) {
  InitForLogMessage();

  EventTraceData data = EventTraceData::Create(&event_trace_);

  EXPECT_EQ(data.event_type, logging::LOG_MESSAGE);
  EXPECT_EQ(data.severity, logging::LOGGING_WARNING);
  EXPECT_EQ(data.process_id, kProcessId);
  EXPECT_EQ(data.thread_id, kThreadId);
  EXPECT_STREQ(data.message.c_str(), kTestLogMessage);
  EXPECT_EQ(data.message.length(), strlen(kTestLogMessage));

  // File and line data should not be filled in for this log message type.
  EXPECT_EQ(data.file_name, std::string());
  EXPECT_EQ(data.line, 0);
}

TEST_F(EventTraceDataTest, LogFullMessage) {
  InitForLogMessageFull();

  EventTraceData data = EventTraceData::Create(&event_trace_);

  EXPECT_EQ(data.event_type, logging::LOG_MESSAGE_FULL);
  EXPECT_EQ(data.severity, logging::LOGGING_WARNING);
  EXPECT_EQ(EventTraceData::SeverityToString(data.severity), kWarning);
  EXPECT_EQ(data.process_id, kProcessId);
  EXPECT_EQ(data.thread_id, kThreadId);
  EXPECT_EQ(data.line, kLineNumber);
  EXPECT_STREQ(data.file_name.c_str(), kFileName);
  EXPECT_STREQ(data.message.c_str(), kTestLogMessage);
}

TEST_F(EventTraceDataTest, LogFullMessage_LargeStackDepth) {
  InitSharedFields(static_cast<uint8_t>(logging::LOG_MESSAGE_FULL));

  // A large stack depth that would require more memory than is available in
  // the buffer should be handled safely.
  DWORD large_stack_depth = 0x3FFFFFFE;
  size_t data_size = sizeof(DWORD);
  size_t offset = ReserveBufferSpace(data_size);
  UNSAFE_TODO(memcpy(buffer_.data() + offset, &large_stack_depth, data_size));

  // Set the MofLength to the current buffer size, which only contains the
  // large stack depth value.
  event_trace_.MofData = buffer_.data();
  event_trace_.MofLength = buffer_.size();

  // The malformed data should be detected and handled without crashing or
  // reading out of bounds.
  EventTraceData data = EventTraceData::Create(&event_trace_);

  // Since the payload is malformed, the line number and message should not
  // be populated.
  EXPECT_EQ(data.line, 0);
  EXPECT_TRUE(data.message.empty());
}

}  // namespace remoting
