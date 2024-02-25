// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/win/event_trace_data.h"

#include "base/check.h"
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

  size_t ReserveBufferSpace(size_t space_needed);

  FILETIME time_ = {};
  EVENT_TRACE event_trace_ = {};
  std::vector<uint8_t> buffer_;

 private:
  void InitSharedFields(uint8_t type);
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
  memcpy(buffer_.data(), kTestLogMessage, message_length);

  event_trace_.MofData = buffer_.data();
  event_trace_.MofLength = buffer_.size();
}

void EventTraceDataTest::InitForLogMessageFull() {
  InitSharedFields(static_cast<uint8_t>(logging::LOG_MESSAGE_FULL));

  // Set up the stack trace info.
  size_t data_size = sizeof(DWORD);
  size_t offset = ReserveBufferSpace(data_size);
  memcpy(buffer_.data() + offset, &kStackDepth, data_size);

  // Allocate space for the 'stack trace' info.
  data_size = kStackDepth * sizeof(intptr_t);
  offset = ReserveBufferSpace(data_size);
  memset(buffer_.data() + offset, 0, data_size);

  // Set the line number.
  data_size = sizeof(DWORD);
  offset = ReserveBufferSpace(data_size);
  memcpy(buffer_.data() + offset, &kLineNumber, data_size);

  // Set the file path.
  data_size = strlen(kFilePath) + 1;
  offset = ReserveBufferSpace(data_size);
  memcpy(buffer_.data() + offset, &kFilePath, data_size);

  // Set the log message.
  data_size = strlen(kTestLogMessage) + 1;
  offset = ReserveBufferSpace(data_size);
  memcpy(buffer_.data() + offset, &kTestLogMessage, data_size);

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

  EXPECT_EQ(logging::LOG_MESSAGE, data.event_type);
  EXPECT_EQ(logging::LOGGING_WARNING, data.severity);
  EXPECT_EQ(kProcessId, data.process_id);
  EXPECT_EQ(kThreadId, data.thread_id);
  EXPECT_STREQ(kTestLogMessage, data.message.c_str());

  // File and line data should not be filled in for this log message type.
  EXPECT_EQ(std::string(), data.file_name);
  EXPECT_EQ(0, data.line);
}

TEST_F(EventTraceDataTest, LogFullMessage) {
  InitForLogMessageFull();

  EventTraceData data = EventTraceData::Create(&event_trace_);

  EXPECT_EQ(logging::LOG_MESSAGE_FULL, data.event_type);
  EXPECT_EQ(logging::LOGGING_WARNING, data.severity);
  EXPECT_EQ(kWarning, EventTraceData::SeverityToString(data.severity));
  EXPECT_EQ(kProcessId, data.process_id);
  EXPECT_EQ(kThreadId, data.thread_id);
  EXPECT_EQ(kLineNumber, data.line);
  EXPECT_STREQ(kFileName, data.file_name.c_str());
  EXPECT_STREQ(kTestLogMessage, data.message.c_str());
}

}  // namespace remoting
