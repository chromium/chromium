// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sstream>
#include <string>

#include "media/base/media_log.h"
#include "media/base/mock_media_log.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace media {

// Friend class of MediaLog for access to internal constants.
class MediaLogTest : public testing::Test {
 protected:
  std::unique_ptr<MockMediaLog> root_log_;

  void CreateLog() { root_log_ = std::make_unique<MockMediaLog>(); }
};

TEST_F(MediaLogTest, EventsAreForwarded) {
  // Make sure that |root_log_| receives events.
  std::unique_ptr<MockMediaLog> root_log(std::make_unique<MockMediaLog>());
  std::unique_ptr<MediaLog> child_media_log(root_log->Clone());
  EXPECT_CALL(*root_log, DoAddLogRecordLogString(_)).Times(1);
  child_media_log->AddMessage(MediaLogMessageLevel::kERROR, "test");
}

TEST_F(MediaLogTest, EventsAreNotForwardedAfterInvalidate) {
  // Make sure that |root_log_| doesn't forward things after we invalidate the
  // underlying log.
  std::unique_ptr<MockMediaLog> root_log(std::make_unique<MockMediaLog>());
  std::unique_ptr<MediaLog> child_media_log(root_log->Clone());
  EXPECT_CALL(*root_log, DoAddLogRecordLogString(_)).Times(0);
  root_log.reset();
  child_media_log->AddMessage(MediaLogMessageLevel::kERROR, "test");
}

TEST_F(MediaLogTest, ClonedLogsInhertParentPlayerId) {
  std::unique_ptr<MockMediaLog> root_log(std::make_unique<MockMediaLog>());
  std::unique_ptr<MediaLog> child_media_log(root_log->Clone());
  EXPECT_CALL(*root_log, DoAddLogRecordLogString(_)).Times(1);
  child_media_log->AddMessage(MediaLogMessageLevel::kERROR, "test");
  auto event = root_log->take_most_recent_event();
  EXPECT_NE(event, nullptr);
  EXPECT_EQ(event->id, 0);
}

TEST_F(MediaLogTest, DontTruncateShortUrlString) {
  CreateLog();
  EXPECT_CALL(*root_log_, DoAddLogRecordLogString(_)).Times(2);
  const std::string short_url("chromium.org");

  // Verify that LoadEvent does not truncate the short URL.
  root_log_->AddEvent<MediaLogEvent::kLoad>(short_url);
  auto event = root_log_->take_most_recent_event();
  EXPECT_NE(event, nullptr);
  EXPECT_EQ(*event->params.FindString("url"), "chromium.org");

  // Verify that CreatedEvent does not truncate the short URL.
  root_log_->AddEvent<MediaLogEvent::kWebMediaPlayerCreated>(short_url);
  event = root_log_->take_most_recent_event();
  EXPECT_NE(event, nullptr);
  EXPECT_EQ(*event->params.FindString("origin_url"), "chromium.org");
}

TEST_F(MediaLogTest, TruncateLongUrlStrings) {
  CreateLog();
  EXPECT_CALL(*root_log_, DoAddLogRecordLogString(_)).Times(2);
  // Build a long string that exceeds the URL length limit.
  std::stringstream string_builder;
  constexpr size_t kLongStringLength = 1010;
  for (size_t i = 0; i < kLongStringLength; i++) {
    string_builder << "c";
  }
  const std::string long_url = string_builder.str();

  std::stringstream expected_string_builder;
  constexpr size_t kMaxLength = 1000;
  for (size_t i = 0; i < kMaxLength - 3; i++) {
    expected_string_builder << "c";
  }
  expected_string_builder << "...";
  const std::string expected_url = expected_string_builder.str();

  // Verify that LoadEvent does not truncate the short URL.
  root_log_->AddEvent<MediaLogEvent::kLoad>(long_url);
  auto event = root_log_->take_most_recent_event();
  EXPECT_NE(event, nullptr);
  EXPECT_EQ(*event->params.FindString("url"), expected_url);

  // Verify that CreatedEvent does not truncate the short URL.
  root_log_->AddEvent<MediaLogEvent::kWebMediaPlayerCreated>(long_url);
  event = root_log_->take_most_recent_event();
  EXPECT_NE(event, nullptr);
  EXPECT_EQ(*event->params.FindString("origin_url"), expected_url);
}

TEST_F(MediaLogTest, PLog) {
  CreateLog();

  const logging::SystemErrorCode kTestError = 28;
  EXPECT_CALL(*root_log_, DoAddLogRecordLogString(_)).Times(1);
  MEDIA_PLOG(ERROR, kTestError, root_log_.get()) << "Testing";

  auto event = root_log_->take_most_recent_event();
  ASSERT_NE(event, nullptr);
  EXPECT_EQ(*event->params.FindString("error"),
            "Testing: " + logging::SystemErrorCodeToString(kTestError));
}

}  // namespace media
