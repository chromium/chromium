// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_MOCK_MEDIA_LOG_H_
#define MEDIA_BASE_MOCK_MEDIA_LOG_H_

#include <memory>
#include <string>

#include "media/base/media_log.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace {
struct TypeAndCodec {
  TypeAndCodec(std::string t, std::string c) : type(t), codec(c) {}
  std::string type;
  std::string codec;
};
}  // namespace

// Helper macros to reduce boilerplate when verifying media log entries.
// |outer| is the std::string searched for substring |sub|.
#define CONTAINS_STRING(outer, sub) (std::string::npos != (outer).find(sub))

// Assumes |media_log_| is available which is a MockMediaLog, optionally a
// NiceMock or StrictMock, in scope of the usage of this macro.
#define EXPECT_MEDIA_LOG(x) EXPECT_MEDIA_LOG_ON(media_log_, x)

// Same as EXPECT_MEDIA_LOG, but for LIMITED_MEDIA_LOG.
#define EXPECT_LIMITED_MEDIA_LOG(x, count, max) \
  if (count < max) {                            \
    EXPECT_MEDIA_LOG_ON(media_log_, x);         \
    count++;                                    \
  }

// |log| is expected to evaluate to a MockMediaLog, optionally a NiceMock or
// StrictMock, in scope of the usage of this macro.
#define EXPECT_MEDIA_LOG_ON(log, x) \
  EXPECT_CALL((log), DoAddLogRecordLogString((x)))

// Requires |media_log_| to be available.
#define EXPECT_MEDIA_LOG_PROPERTY(property, value)                             \
  EXPECT_CALL(                                                                 \
      media_log_,                                                              \
      DoAddLogRecordLogString(                                                 \
          MatchesPropertyExactValue(MockMediaLog::MediaEventToLogString(       \
              *media_log_.CreatePropertyTestEvent<MediaLogProperty::property>( \
                  value)))))

#define EXPECT_MEDIA_LOG_PROPERTY_ANY_VALUE(property)                       \
  EXPECT_CALL(                                                              \
      media_log_,                                                           \
      DoAddLogRecordLogString(MatchesPropertyAnyValue(                      \
          "{\"" + MediaLogPropertyKeyToString(MediaLogProperty::property) + \
          "\"")))

#define EXPECT_FOUND_CODEC_NAME(stream_type, codec_name)                      \
  EXPECT_CALL(media_log_,                                                     \
              DoAddLogRecordLogString(TracksHasCodecName(                     \
                  TypeAndCodec(MediaLogPropertyKeyToString(                   \
                                   MediaLogProperty::k##stream_type##Tracks), \
                               codec_name))))

namespace media {

MATCHER_P(TracksHasCodecName, tandc, "") {
  return (arg.substr(0, 15) == "{\"" + tandc.type + "\"") && (arg[16] != ']') &&
         CONTAINS_STRING(arg, tandc.codec);
}

MATCHER_P(MatchesPropertyExactValue, message, "") {
  return arg == message;
}

MATCHER_P(MatchesPropertyAnyValue, message, "") {
  return CONTAINS_STRING(arg, message);
}

class MockMediaLog : public MediaLog {
 public:
  MockMediaLog();

  MockMediaLog(const MockMediaLog&) = delete;
  MockMediaLog& operator=(const MockMediaLog&) = delete;

  ~MockMediaLog() override;

  MOCK_METHOD1(DoAddLogRecordLogString, void(const std::string& event));

  // Trampoline method to workaround GMOCK problems with std::unique_ptr<>.
  // Also simplifies tests to be able to string match on the log string
  // representation on the added event.
  void AddLogRecordLocked(std::unique_ptr<MediaLogRecord> event) override;

  template <MediaLogProperty P, typename T>
  std::unique_ptr<MediaLogRecord> CreatePropertyTestEvent(const T& value) {
    return CreatePropertyRecord<P, T>(value);
  }

  static std::string MediaEventToLogString(const MediaLogRecord& event);

  // Return whatever the last AddLogRecordLocked() was given.
  // TODO(tmathmeyer): Remove this in favor of a mock, to allow EXPECT_CALL.
  std::unique_ptr<MediaLogRecord> take_most_recent_event() {
    return std::move(most_recent_event_);
  }

  // Disable console logging since the mock log is used in spammy tests.
  bool ShouldLogToDebugConsole() const override;

 private:
  std::unique_ptr<MediaLogRecord> most_recent_event_;
};

}  // namespace media

#endif  // MEDIA_BASE_MOCK_MEDIA_LOG_H_
