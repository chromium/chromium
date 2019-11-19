// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webrtc_logging/browser/log_cleanup.h"

#include "base/files/file.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "components/webrtc_logging/browser/text_log_list.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace webrtc_logging {

class WebRtcLogCleanupTest : public testing::Test {
 public:
  WebRtcLogCleanupTest() = default;

  void SetUp() override {
    ASSERT_GT(kTimeToKeepLogs, base::TimeDelta::FromDays(2));

    // Create three files. One with modified date as of now, one with date one
    // day younger than the keep limit, one with date one day older than the
    // limit. The two former are expected to be kept and the last to be deleted
    // when deleting old logs.
    ASSERT_TRUE(dir_.CreateUniqueTempDir());
    base::FilePath file;
    ASSERT_TRUE(CreateTemporaryFileInDir(dir_.GetPath(), &file));
    ASSERT_TRUE(CreateTemporaryFileInDir(dir_.GetPath(), &file));
    base::Time time_expect_to_keep =
        base::Time::Now() - kTimeToKeepLogs + base::TimeDelta::FromDays(1);
    TouchFile(file, time_expect_to_keep, time_expect_to_keep);
    ASSERT_TRUE(CreateTemporaryFileInDir(dir_.GetPath(), &file));
    base::Time time_expect_to_delete =
        base::Time::Now() - kTimeToKeepLogs + -base::TimeDelta::FromDays(1);
    TouchFile(file, time_expect_to_delete, time_expect_to_delete);
  }

  void VerifyFiles(int expected_files) {
    base::FileEnumerator files(dir_.GetPath(), false,
                               base::FileEnumerator::FILES);
    int file_counter = 0;
    for (base::FilePath name = files.Next(); !name.empty();
         name = files.Next()) {
      EXPECT_LT(base::Time::Now() - files.GetInfo().GetLastModifiedTime(),
                kTimeToKeepLogs);
      ++file_counter;
    }
    EXPECT_EQ(expected_files, file_counter);
  }

  content::BrowserTaskEnvironment task_environment_;
  base::ScopedTempDir dir_;
};

TEST_F(WebRtcLogCleanupTest, DeleteOldWebRtcLogFiles) {
  DeleteOldWebRtcLogFiles(dir_.GetPath());
  VerifyFiles(2);
}

TEST_F(WebRtcLogCleanupTest, DeleteOldAndRecentWebRtcLogFiles) {
  base::Time delete_begin_time =
      base::Time::Now() - base::TimeDelta::FromDays(1);
  DeleteOldAndRecentWebRtcLogFiles(dir_.GetPath(), delete_begin_time);
  VerifyFiles(1);
}

// Fixture for testing the cleanup of entries from the index, for which the
// log files themselves have already been deleted.
class WebRtcTextLogIndexCleanupTest : public testing::Test {
 public:
  ~WebRtcTextLogIndexCleanupTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(dir_.CreateUniqueTempDir());
    log_list_path_ =
        TextLogList::GetWebRtcLogListFileForDirectory(dir_.GetPath());
  }

  void CreateLogListFileWithContents(const std::string& contents) {
    ASSERT_FALSE(base::PathExists(log_list_path_));  // Only call once per test.

    const int len = static_cast<int>(contents.length());
    ASSERT_EQ(base::WriteFile(log_list_path_, contents.c_str(), len), len);

    ASSERT_TRUE(base::PathExists(log_list_path_));  // Only call once per test.
  }

  void ExpectContents(std::string expected_contents) {
    ASSERT_TRUE(base::PathExists(log_list_path_));
    std::string contents;
    ASSERT_TRUE(ReadFileToString(log_list_path_, &contents));

    // For robustness' sake, we allow non-newline-terminated lines in the file,
    // even though we never produce them.
    if (expected_contents.length() > 0 &&
        expected_contents[expected_contents.length() - 1] != '\n') {
      expected_contents += "\n";
    }

    EXPECT_EQ(contents, expected_contents);
  }

  base::ScopedTempDir dir_;
  base::FilePath log_list_path_;
};

TEST_F(WebRtcTextLogIndexCleanupTest, OlderLinesNotDeleted) {
  const std::string contents =
      "100.1,report_id_1,local_id_1,101.1\n"
      "100.2,report_id_2,local_id_2,101.2\n"
      "100.3,report_id_3,local_id_3,101.3";
  CreateLogListFileWithContents(contents);

  const base::Time delete_begin_time = base::Time::FromDoubleT(150);
  DeleteOldAndRecentWebRtcLogFiles(dir_.GetPath(), delete_begin_time);

  const std::string expected_contents = contents;
  ExpectContents(expected_contents);
}

TEST_F(WebRtcTextLogIndexCleanupTest, LinesInDeletionTimeRangeDeleted) {
  const std::string contents =
      "160.1,report_id_1,local_id_1,161.1\n"
      "100.2,report_id_2,local_id_2,101.2\n"
      "160.3,report_id_3,local_id_3,161.3";
  CreateLogListFileWithContents(contents);

  const base::Time delete_begin_time = base::Time::FromDoubleT(150);
  DeleteOldAndRecentWebRtcLogFiles(dir_.GetPath(), delete_begin_time);

  const std::string expected_contents = "100.2,report_id_2,local_id_2,101.2\n";
  ExpectContents(expected_contents);
}

TEST_F(WebRtcTextLogIndexCleanupTest, ExpiredLinesDeleted) {
  const base::Time now = base::Time::Now();

  const std::string expired_capture_timestamp_line =
      base::NumberToString((now - base::TimeDelta::FromHours(1)).ToDoubleT()) +
      ",report_id_3,local_id_3,101.3\n";

  const std::string not_expired_line =
      base::NumberToString((now - base::TimeDelta::FromHours(2)).ToDoubleT()) +
      ",report_id_2,local_id_2," +
      base::NumberToString((now - base::TimeDelta::FromHours(3)).ToDoubleT()) +
      "\n";

  // Note: Would only happen if the clock is changed in between.
  const std::string expired_upload_timestamp_line =
      "100.1,report_id_1,local_id_1," +
      base::NumberToString((now - base::TimeDelta::FromHours(4)).ToDoubleT()) +
      "\n";

  const std::string contents = expired_capture_timestamp_line +  //
                               not_expired_line +                //
                               expired_upload_timestamp_line;

  CreateLogListFileWithContents(contents);

  DeleteOldWebRtcLogFiles(dir_.GetPath());

  const std::string expected_contents = not_expired_line;
  ExpectContents(expected_contents);
}

TEST_F(WebRtcTextLogIndexCleanupTest, AllLinesDeletedSanity) {
  const std::string contents =
      "160.1,report_id_1,local_id_1,161.1\n"
      "160.2,report_id_2,local_id_2,161.2\n"
      "160.3,report_id_3,local_id_3,161.3";
  CreateLogListFileWithContents(contents);

  const base::Time delete_begin_time = base::Time::FromDoubleT(150);
  DeleteOldAndRecentWebRtcLogFiles(dir_.GetPath(), delete_begin_time);

  const std::string expected_contents = "";
  ExpectContents(expected_contents);
}

TEST_F(WebRtcTextLogIndexCleanupTest, EmptyLinesRemoved) {
  const std::string contents =
      "100.1,report_id_1,local_id_1,101.1\n"
      "\n\n"
      "100.2,report_id_2,local_id_2,101.2\n"
      "\n"
      "100.3,report_id_3,local_id_3,101.3\n"
      "\n\n\n\n\n\n\n";
  CreateLogListFileWithContents(contents);

  const base::Time delete_begin_time = base::Time::FromDoubleT(150);
  DeleteOldAndRecentWebRtcLogFiles(dir_.GetPath(), delete_begin_time);

  const std::string expected_contents =
      "100.1,report_id_1,local_id_1,101.1\n"
      "100.2,report_id_2,local_id_2,101.2\n"
      "100.3,report_id_3,local_id_3,101.3\n";
  ExpectContents(expected_contents);
}

TEST_F(WebRtcTextLogIndexCleanupTest, SanityEmptyFile) {
  const std::string contents = "";
  CreateLogListFileWithContents(contents);

  const base::Time delete_begin_time = base::Time::FromDoubleT(150);
  DeleteOldAndRecentWebRtcLogFiles(dir_.GetPath(), delete_begin_time);

  const std::string expected_contents = "";
  ExpectContents(expected_contents);
}

TEST_F(WebRtcTextLogIndexCleanupTest, SanityFileWithOneEmptyLine) {
  const std::string contents = "\n";
  CreateLogListFileWithContents(contents);

  const base::Time delete_begin_time = base::Time::FromDoubleT(150);
  DeleteOldAndRecentWebRtcLogFiles(dir_.GetPath(), delete_begin_time);

  const std::string expected_contents = "";
  ExpectContents(expected_contents);
}

// SingleLineSanity and CanRemoveAllLines combined prove that we can write
// the following tests using a single line, and not pass by mistake.
TEST_F(WebRtcTextLogIndexCleanupTest, SingleLineSanity) {
  const std::string contents = "100.1,report_id_1,local_id_1,101.1\n";
  CreateLogListFileWithContents(contents);

  const base::Time delete_begin_time = base::Time::FromDoubleT(150);
  DeleteOldAndRecentWebRtcLogFiles(dir_.GetPath(), delete_begin_time);

  const std::string expected_contents = contents;
  ExpectContents(expected_contents);
}

// SingleLineSanity and CanRemoveAllLines combined prove that we can write
// the following tests using a single line, and not pass by mistake.
TEST_F(WebRtcTextLogIndexCleanupTest, CanRemoveAllLines) {
  const std::string contents = "200.1,report_id_1,local_id_1,201.1\n";
  CreateLogListFileWithContents(contents);

  const base::Time delete_begin_time = base::Time::FromDoubleT(150);
  DeleteOldAndRecentWebRtcLogFiles(dir_.GetPath(), delete_begin_time);

  const std::string expected_contents = "";
  ExpectContents(expected_contents);
}

TEST_F(WebRtcTextLogIndexCleanupTest, LinesWithoutUploadDateConsideredValid) {
  const std::string contents = ",report_id_1,local_id_1,101.1\n";
  CreateLogListFileWithContents(contents);

  const base::Time delete_begin_time = base::Time::FromDoubleT(150);
  DeleteOldAndRecentWebRtcLogFiles(dir_.GetPath(), delete_begin_time);

  const std::string expected_contents = contents;
  ExpectContents(expected_contents);
}

TEST_F(WebRtcTextLogIndexCleanupTest, LinesWithoutReportIdConsideredValid) {
  const std::string contents = "100.1,,local_id_1,101.1\n";
  CreateLogListFileWithContents(contents);

  const base::Time delete_begin_time = base::Time::FromDoubleT(150);
  DeleteOldAndRecentWebRtcLogFiles(dir_.GetPath(), delete_begin_time);

  const std::string expected_contents = contents;
  ExpectContents(expected_contents);
}

TEST_F(WebRtcTextLogIndexCleanupTest, LinesWithoutLocalIdConsideredValid) {
  const std::string contents = "100.1,report_id_1,,101.1\n";
  CreateLogListFileWithContents(contents);

  const base::Time delete_begin_time = base::Time::FromDoubleT(150);
  DeleteOldAndRecentWebRtcLogFiles(dir_.GetPath(), delete_begin_time);

  const std::string expected_contents = contents;
  ExpectContents(expected_contents);
}

TEST_F(WebRtcTextLogIndexCleanupTest,
       LinesWithoutCaptureDateConsideredInvalid) {
  const std::string contents = "100.1,report_id_1,local_id_1,\n";
  CreateLogListFileWithContents(contents);

  const base::Time delete_begin_time = base::Time::FromDoubleT(150);
  DeleteOldAndRecentWebRtcLogFiles(dir_.GetPath(), delete_begin_time);

  const std::string expected_contents = "";
  ExpectContents(expected_contents);
}

TEST_F(WebRtcTextLogIndexCleanupTest, CanBeConsideredObsoleteDueToCaptureDate) {
  const std::string contents = ",report_id_1,local_id_1,161.1\n";
  CreateLogListFileWithContents(contents);

  const base::Time delete_begin_time = base::Time::FromDoubleT(150);
  DeleteOldAndRecentWebRtcLogFiles(dir_.GetPath(), delete_begin_time);

  const std::string expected_contents = "";
  ExpectContents(expected_contents);
}

TEST_F(WebRtcTextLogIndexCleanupTest, CanBeConsideredObsoleteDueToUploadDate) {
  const std::string contents = "160.1,report_id_1,local_id_1,101.1\n";
  CreateLogListFileWithContents(contents);

  const base::Time delete_begin_time = base::Time::FromDoubleT(150);
  DeleteOldAndRecentWebRtcLogFiles(dir_.GetPath(), delete_begin_time);

  const std::string expected_contents = "";
  ExpectContents(expected_contents);
}

TEST_F(WebRtcTextLogIndexCleanupTest, LinesWithTooFewTokensConsideredInvalid) {
  const std::string contents = "100.1,report_id_1,local_id_1\n";
  CreateLogListFileWithContents(contents);

  const base::Time delete_begin_time = base::Time::FromDoubleT(150);
  DeleteOldAndRecentWebRtcLogFiles(dir_.GetPath(), delete_begin_time);

  const std::string expected_contents = "";
  ExpectContents(expected_contents);
}

TEST_F(WebRtcTextLogIndexCleanupTest,
       LinesWithTooManyTokensConsideredInvalidEmptyVersion) {
  const std::string contents = "100.1,report_id_1,local_id_1,101.1,\n";
  CreateLogListFileWithContents(contents);

  const base::Time delete_begin_time = base::Time::FromDoubleT(150);
  DeleteOldAndRecentWebRtcLogFiles(dir_.GetPath(), delete_begin_time);

  const std::string expected_contents = "";
  ExpectContents(expected_contents);
}

TEST_F(WebRtcTextLogIndexCleanupTest,
       LinesWithTooManyTokensConsideredInvalidNonEmptyVersion) {
  const std::string contents = "100.1,report_id_1,local_id_1,101.1,102\n";
  CreateLogListFileWithContents(contents);

  const base::Time delete_begin_time = base::Time::FromDoubleT(150);
  DeleteOldAndRecentWebRtcLogFiles(dir_.GetPath(), delete_begin_time);

  const std::string expected_contents = "";
  ExpectContents(expected_contents);
}

TEST_F(WebRtcTextLogIndexCleanupTest,
       LinesWithUnparsableUploadDateConsideredInvalid) {
  const std::string contents = "100.1.2,report_id_1,local_id_1,101.1\n";
  CreateLogListFileWithContents(contents);

  const base::Time delete_begin_time = base::Time::FromDoubleT(150);
  DeleteOldAndRecentWebRtcLogFiles(dir_.GetPath(), delete_begin_time);

  const std::string expected_contents = "";
  ExpectContents(expected_contents);
}

TEST_F(WebRtcTextLogIndexCleanupTest,
       LinesWithUnparsableCaptureDateConsideredInvalid) {
  const std::string contents = "100.1,report_id_1,local_id_1,101.1.2\n";
  CreateLogListFileWithContents(contents);

  const base::Time delete_begin_time = base::Time::FromDoubleT(150);
  DeleteOldAndRecentWebRtcLogFiles(dir_.GetPath(), delete_begin_time);

  const std::string expected_contents = "";
  ExpectContents(expected_contents);
}

}  // namespace webrtc_logging
