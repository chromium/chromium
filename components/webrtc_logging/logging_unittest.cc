// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Note: this test tests RTC_LOG_V and RTC_LOG_E since all other logs are
// expressed in forms of them. RTC_LOG is also tested for good measure.
// Also note that we are only allowed to call InitLogging() twice so the test
// cases are more dense than normal.

// We must include Chromium headers before including the overrides header
// since webrtc's logging.h file may conflict with chromium.

#include "base/logging.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "build/build_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/webrtc_overrides/rtc_base/logging.h"

namespace {

static const int kDefaultVerbosity = 0;

static const char* AsString(rtc::LoggingSeverity severity) {
  switch (severity) {
    case rtc::LS_ERROR:
      return "LS_ERROR";
    case rtc::LS_WARNING:
      return "LS_WARNING";
    case rtc::LS_INFO:
      return "LS_INFO";
    case rtc::LS_VERBOSE:
      return "LS_VERBOSE";
    case rtc::LS_SENSITIVE:
      return "LS_SENSITIVE";
    default:
      return "";
  }
}

class WebRtcTextLogTest : public testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(log_dir_.CreateUniqueTempDir());
    log_file_path_ = log_dir_.GetPath().AppendASCII("webrtc_log");
  }

 protected:
  bool Initialize(int verbosity_level) {
    // The command line flags are parsed here and the log file name is set.
    logging::LoggingSettings settings;
    settings.logging_dest = logging::LOG_TO_FILE;
    settings.log_file_path = log_file_path_.value().data();
    settings.lock_log = logging::DONT_LOCK_LOG_FILE;
    settings.delete_old = logging::DELETE_OLD_LOG_FILE;
    if (!logging::InitLogging(settings)) {
      return false;
    }
    logging::SetMinLogLevel(-verbosity_level);

    EXPECT_TRUE(VLOG_IS_ON(verbosity_level));

    EXPECT_FALSE(VLOG_IS_ON(verbosity_level + 1));
    return true;
  }

  base::ScopedTempDir log_dir_;
  base::FilePath log_file_path_;
};

TEST_F(WebRtcTextLogTest, DefaultConfiguration) {
  ASSERT_TRUE(Initialize(kDefaultVerbosity));

  // In the default configuration only warnings and errors should be logged.
  RTC_LOG_V(rtc::LS_ERROR) << AsString(rtc::LS_ERROR);
  RTC_LOG_V(rtc::LS_WARNING) << AsString(rtc::LS_WARNING);
  RTC_LOG_V(rtc::LS_INFO) << AsString(rtc::LS_INFO);
  RTC_LOG_V(rtc::LS_VERBOSE) << AsString(rtc::LS_VERBOSE);
  RTC_LOG_V(rtc::LS_SENSITIVE) << AsString(rtc::LS_SENSITIVE);

  // Read file to string.
  std::string contents_of_file;
  base::ReadFileToString(log_file_path_, &contents_of_file);

  // Make sure string contains the expected values.
  EXPECT_THAT(contents_of_file, ::testing::HasSubstr(AsString(rtc::LS_ERROR)));
  EXPECT_THAT(contents_of_file,
              ::testing::HasSubstr(AsString(rtc::LS_WARNING)));
  EXPECT_THAT(contents_of_file,
              ::testing::Not(::testing::HasSubstr(AsString(rtc::LS_INFO))));
  EXPECT_THAT(contents_of_file,
              ::testing::Not(::testing::HasSubstr(AsString(rtc::LS_VERBOSE))));
  EXPECT_THAT(
      contents_of_file,
      ::testing::Not(::testing::HasSubstr(AsString(rtc::LS_SENSITIVE))));
}

TEST_F(WebRtcTextLogTest, InfoConfiguration) {
  ASSERT_TRUE(Initialize(0));  // 0 == Chrome's 'info' level.

  // In this configuration everything lower or equal to LS_INFO should be
  // logged.
  RTC_LOG_V(rtc::LS_ERROR) << AsString(rtc::LS_ERROR);
  RTC_LOG_V(rtc::LS_WARNING) << AsString(rtc::LS_WARNING);
  RTC_LOG_V(rtc::LS_INFO) << AsString(rtc::LS_INFO);
  RTC_LOG_V(rtc::LS_VERBOSE) << AsString(rtc::LS_VERBOSE);
  RTC_LOG_V(rtc::LS_SENSITIVE) << AsString(rtc::LS_SENSITIVE);

  // Read file to string.
  std::string contents_of_file;
  base::ReadFileToString(log_file_path_, &contents_of_file);

  // Make sure string contains the expected values.
  EXPECT_THAT(contents_of_file, ::testing::HasSubstr(AsString(rtc::LS_ERROR)));
  EXPECT_THAT(contents_of_file,
              ::testing::HasSubstr(AsString(rtc::LS_WARNING)));
  EXPECT_THAT(contents_of_file,
              ::testing::Not(::testing::HasSubstr(AsString(rtc::LS_INFO))));
  EXPECT_THAT(contents_of_file,
              ::testing::Not(::testing::HasSubstr(AsString(rtc::LS_VERBOSE))));
  EXPECT_THAT(
      contents_of_file,
      ::testing::Not(::testing::HasSubstr(AsString(rtc::LS_SENSITIVE))));

  // Also check that the log is proper.
  EXPECT_THAT(contents_of_file, ::testing::HasSubstr("logging_unittest.cc"));
  EXPECT_THAT(contents_of_file,
              ::testing::Not(::testing::HasSubstr("logging.h")));
  EXPECT_THAT(contents_of_file,
              ::testing::Not(::testing::HasSubstr("logging.cc")));
}

TEST_F(WebRtcTextLogTest, LogEverythingConfiguration) {
  ASSERT_TRUE(Initialize(2));  // verbosity at level 2 allows LS_SENSITIVE.

  // In this configuration everything should be logged.
  RTC_LOG_V(rtc::LS_ERROR) << AsString(rtc::LS_ERROR);
  RTC_LOG_V(rtc::LS_WARNING) << AsString(rtc::LS_WARNING);
  RTC_LOG(LS_INFO) << AsString(rtc::LS_INFO);
  static const int kFakeError = 1;
  RTC_LOG_E(LS_INFO, EN, kFakeError)
      << "RTC_LOG_E(" << AsString(rtc::LS_INFO) << ")";
  RTC_LOG_V(rtc::LS_VERBOSE) << AsString(rtc::LS_VERBOSE);
  RTC_LOG_V(rtc::LS_SENSITIVE) << AsString(rtc::LS_SENSITIVE);

  // Read file to string.
  std::string contents_of_file;
  base::ReadFileToString(log_file_path_, &contents_of_file);

  // Make sure string contains the expected values.
  EXPECT_THAT(contents_of_file, ::testing::HasSubstr(AsString(rtc::LS_ERROR)));
  EXPECT_THAT(contents_of_file,
              ::testing::HasSubstr(AsString(rtc::LS_WARNING)));

  EXPECT_THAT(contents_of_file, ::testing::HasSubstr(AsString(rtc::LS_INFO)));
  // RTC_LOG_E
  EXPECT_THAT(contents_of_file, ::testing::HasSubstr(strerror(kFakeError)));
  EXPECT_THAT(contents_of_file,
              ::testing::HasSubstr(AsString(rtc::LS_VERBOSE)));
  EXPECT_THAT(contents_of_file,
              ::testing::HasSubstr(AsString(rtc::LS_SENSITIVE)));
}

TEST_F(WebRtcTextLogTest, LogIf) {
  ASSERT_TRUE(Initialize(2));

  RTC_LOG_IF(LS_INFO, true) << "IfTrue";
  RTC_LOG_IF(LS_INFO, false) << "IfFalse";
  RTC_LOG_IF_F(LS_INFO, true) << "LogF";
  RTC_LOG_IF_F(LS_INFO, false) << "NoLogF";

  RTC_DLOG_IF(LS_INFO, true) << "DebugIfTrue";
  RTC_DLOG_IF(LS_INFO, false) << "DebugIfFalse";
  RTC_DLOG_IF_F(LS_INFO, true) << "DebugLogF";
  RTC_DLOG_IF_F(LS_INFO, false) << "DebugNotLogF";

  // Read file to string.
  std::string contents_of_file;
  base::ReadFileToString(log_file_path_, &contents_of_file);

  EXPECT_THAT(contents_of_file, ::testing::HasSubstr("IfTrue"));
  EXPECT_THAT(contents_of_file,
              ::testing::Not(::testing::HasSubstr("IfFalse")));
  EXPECT_THAT(contents_of_file, ::testing::HasSubstr(__FUNCTION__));
  EXPECT_THAT(contents_of_file, ::testing::HasSubstr("LogF"));
  EXPECT_THAT(contents_of_file, ::testing::Not(::testing::HasSubstr("NoLogF")));

#if RTC_DLOG_IS_ON
  EXPECT_THAT(contents_of_file, ::testing::HasSubstr("DebugIfTrue"));
  EXPECT_THAT(contents_of_file,
              ::testing::Not(::testing::HasSubstr("DebugIfFalse")));
  EXPECT_THAT(contents_of_file, ::testing::HasSubstr("DebugLogF"));
  EXPECT_THAT(contents_of_file,
              ::testing::Not(::testing::HasSubstr("DebugNoLogF")));
#endif  // RTC_DLOG_IF_ON
}

}  // namespace
