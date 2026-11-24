// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webrtc_logging/browser/log_cleanup.h"

#include "base/files/file.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/webrtc_logging/browser/text_log_list.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_POSIX)
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include "base/files/scoped_file.h"
#include "base/posix/eintr_wrapper.h"
#endif

#if BUILDFLAG(IS_WIN)
#include <optional>

#include "base/test/file_path_reparse_point_win.h"
#endif

namespace webrtc_logging {

#if BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_WIN)
namespace {

// Creates a directory symlink on POSIX or an NTFS junction on Windows, keeping
// the Windows reparse point handle alive for the scope.
class ScopedDirectoryLink {
 public:
  ScopedDirectoryLink() = default;
  ~ScopedDirectoryLink() = default;

  [[nodiscard]] bool Create(const base::FilePath& link_path,
                            const base::FilePath& target_dir) {
#if BUILDFLAG(IS_POSIX)
    return base::CreateSymbolicLink(target_dir, link_path);
#elif BUILDFLAG(IS_WIN)
    if (!base::CreateDirectory(link_path)) {
      return false;
    }
    reparse_point_ =
        base::test::FilePathReparsePoint::Create(link_path, target_dir);
    return reparse_point_.has_value();
#endif
  }

 private:
#if BUILDFLAG(IS_WIN)
  std::optional<base::test::FilePathReparsePoint> reparse_point_;
#endif
};

}  // namespace
#endif  // BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_WIN)

class WebRtcLogCleanupTest : public testing::Test {
 public:
  WebRtcLogCleanupTest() = default;

  void SetUp() override {
    ASSERT_GT(kTimeToKeepLogs, base::Days(2));

    // Create three files. One with modified date as of now, one with date one
    // day younger than the keep limit, one with date one day older than the
    // limit. The two former are expected to be kept and the last to be deleted
    // when deleting old logs.
    ASSERT_TRUE(dir_.CreateUniqueTempDir());
    base::FilePath file;
    ASSERT_TRUE(CreateTemporaryFileInDir(dir_.GetPath(), &file));
    ASSERT_TRUE(CreateTemporaryFileInDir(dir_.GetPath(), &file));
    base::Time time_expect_to_keep =
        base::Time::Now() - kTimeToKeepLogs + base::Days(1);
    TouchFile(file, time_expect_to_keep, time_expect_to_keep);
    ASSERT_TRUE(CreateTemporaryFileInDir(dir_.GetPath(), &file));
    base::Time time_expect_to_delete =
        base::Time::Now() - kTimeToKeepLogs + -base::Days(1);
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
  DeleteOldWebRtcLogFiles({dir_.GetPath()});
  VerifyFiles(2);
}

TEST_F(WebRtcLogCleanupTest, DeleteOldAndRecentWebRtcLogFiles) {
  base::Time delete_begin_time = base::Time::Now() - base::Days(1);
  DeleteOldAndRecentWebRtcLogFiles({dir_.GetPath()}, delete_begin_time);
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

    ASSERT_TRUE(base::WriteFile(log_list_path_, contents));

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

  const base::Time delete_begin_time =
      base::Time::FromSecondsSinceUnixEpoch(150);
  DeleteOldAndRecentWebRtcLogFiles({dir_.GetPath()}, delete_begin_time);

  const std::string expected_contents = contents;
  ExpectContents(expected_contents);
}

TEST_F(WebRtcTextLogIndexCleanupTest, LinesInDeletionTimeRangeDeleted) {
  const std::string contents =
      "160.1,report_id_1,local_id_1,161.1\n"
      "100.2,report_id_2,local_id_2,101.2\n"
      "160.3,report_id_3,local_id_3,161.3";
  CreateLogListFileWithContents(contents);

  const base::Time delete_begin_time =
      base::Time::FromSecondsSinceUnixEpoch(150);
  DeleteOldAndRecentWebRtcLogFiles({dir_.GetPath()}, delete_begin_time);

  const std::string expected_contents = "100.2,report_id_2,local_id_2,101.2\n";
  ExpectContents(expected_contents);
}

TEST_F(WebRtcTextLogIndexCleanupTest, ExpiredLinesDeleted) {
  const base::Time now = base::Time::Now();

  const std::string expired_capture_timestamp_line =
      base::NumberToString((now - base::Hours(1)).InSecondsFSinceUnixEpoch()) +
      ",report_id_3,local_id_3,101.3\n";

  const std::string not_expired_line =
      base::NumberToString((now - base::Hours(2)).InSecondsFSinceUnixEpoch()) +
      ",report_id_2,local_id_2," +
      base::NumberToString((now - base::Hours(3)).InSecondsFSinceUnixEpoch()) +
      "\n";

  // Note: Would only happen if the clock is changed in between.
  const std::string expired_upload_timestamp_line =
      "100.1,report_id_1,local_id_1," +
      base::NumberToString((now - base::Hours(4)).InSecondsFSinceUnixEpoch()) +
      "\n";

  const std::string contents = expired_capture_timestamp_line +  //
                               not_expired_line +                //
                               expired_upload_timestamp_line;

  CreateLogListFileWithContents(contents);

  DeleteOldWebRtcLogFiles({dir_.GetPath()});

  const std::string expected_contents = not_expired_line;
  ExpectContents(expected_contents);
}

TEST_F(WebRtcTextLogIndexCleanupTest, AllLinesDeletedSanity) {
  const std::string contents =
      "160.1,report_id_1,local_id_1,161.1\n"
      "160.2,report_id_2,local_id_2,161.2\n"
      "160.3,report_id_3,local_id_3,161.3";
  CreateLogListFileWithContents(contents);

  const base::Time delete_begin_time =
      base::Time::FromSecondsSinceUnixEpoch(150);
  DeleteOldAndRecentWebRtcLogFiles({dir_.GetPath()}, delete_begin_time);

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

  const base::Time delete_begin_time =
      base::Time::FromSecondsSinceUnixEpoch(150);
  DeleteOldAndRecentWebRtcLogFiles({dir_.GetPath()}, delete_begin_time);

  const std::string expected_contents =
      "100.1,report_id_1,local_id_1,101.1\n"
      "100.2,report_id_2,local_id_2,101.2\n"
      "100.3,report_id_3,local_id_3,101.3\n";
  ExpectContents(expected_contents);
}

TEST_F(WebRtcTextLogIndexCleanupTest, SanityEmptyFile) {
  const std::string contents = "";
  CreateLogListFileWithContents(contents);

  const base::Time delete_begin_time =
      base::Time::FromSecondsSinceUnixEpoch(150);
  DeleteOldAndRecentWebRtcLogFiles({dir_.GetPath()}, delete_begin_time);

  const std::string expected_contents = "";
  ExpectContents(expected_contents);
}

TEST_F(WebRtcTextLogIndexCleanupTest, SanityFileWithOneEmptyLine) {
  const std::string contents = "\n";
  CreateLogListFileWithContents(contents);

  const base::Time delete_begin_time =
      base::Time::FromSecondsSinceUnixEpoch(150);
  DeleteOldAndRecentWebRtcLogFiles({dir_.GetPath()}, delete_begin_time);

  const std::string expected_contents = "";
  ExpectContents(expected_contents);
}

// SingleLineSanity and CanRemoveAllLines combined prove that we can write
// the following tests using a single line, and not pass by mistake.
TEST_F(WebRtcTextLogIndexCleanupTest, SingleLineSanity) {
  const std::string contents = "100.1,report_id_1,local_id_1,101.1\n";
  CreateLogListFileWithContents(contents);

  const base::Time delete_begin_time =
      base::Time::FromSecondsSinceUnixEpoch(150);
  DeleteOldAndRecentWebRtcLogFiles({dir_.GetPath()}, delete_begin_time);

  const std::string expected_contents = contents;
  ExpectContents(expected_contents);
}

// SingleLineSanity and CanRemoveAllLines combined prove that we can write
// the following tests using a single line, and not pass by mistake.
TEST_F(WebRtcTextLogIndexCleanupTest, CanRemoveAllLines) {
  const std::string contents = "200.1,report_id_1,local_id_1,201.1\n";
  CreateLogListFileWithContents(contents);

  const base::Time delete_begin_time =
      base::Time::FromSecondsSinceUnixEpoch(150);
  DeleteOldAndRecentWebRtcLogFiles({dir_.GetPath()}, delete_begin_time);

  const std::string expected_contents = "";
  ExpectContents(expected_contents);
}

TEST_F(WebRtcTextLogIndexCleanupTest, LinesWithoutUploadDateConsideredValid) {
  const std::string contents = ",report_id_1,local_id_1,101.1\n";
  CreateLogListFileWithContents(contents);

  const base::Time delete_begin_time =
      base::Time::FromSecondsSinceUnixEpoch(150);
  DeleteOldAndRecentWebRtcLogFiles({dir_.GetPath()}, delete_begin_time);

  const std::string expected_contents = contents;
  ExpectContents(expected_contents);
}

TEST_F(WebRtcTextLogIndexCleanupTest, LinesWithoutReportIdConsideredValid) {
  const std::string contents = "100.1,,local_id_1,101.1\n";
  CreateLogListFileWithContents(contents);

  const base::Time delete_begin_time =
      base::Time::FromSecondsSinceUnixEpoch(150);
  DeleteOldAndRecentWebRtcLogFiles({dir_.GetPath()}, delete_begin_time);

  const std::string expected_contents = contents;
  ExpectContents(expected_contents);
}

TEST_F(WebRtcTextLogIndexCleanupTest, LinesWithoutLocalIdConsideredValid) {
  const std::string contents = "100.1,report_id_1,,101.1\n";
  CreateLogListFileWithContents(contents);

  const base::Time delete_begin_time =
      base::Time::FromSecondsSinceUnixEpoch(150);
  DeleteOldAndRecentWebRtcLogFiles({dir_.GetPath()}, delete_begin_time);

  const std::string expected_contents = contents;
  ExpectContents(expected_contents);
}

TEST_F(WebRtcTextLogIndexCleanupTest,
       LinesWithoutCaptureDateConsideredInvalid) {
  const std::string contents = "100.1,report_id_1,local_id_1,\n";
  CreateLogListFileWithContents(contents);

  const base::Time delete_begin_time =
      base::Time::FromSecondsSinceUnixEpoch(150);
  DeleteOldAndRecentWebRtcLogFiles({dir_.GetPath()}, delete_begin_time);

  const std::string expected_contents = "";
  ExpectContents(expected_contents);
}

TEST_F(WebRtcTextLogIndexCleanupTest, CanBeConsideredObsoleteDueToCaptureDate) {
  const std::string contents = ",report_id_1,local_id_1,161.1\n";
  CreateLogListFileWithContents(contents);

  const base::Time delete_begin_time =
      base::Time::FromSecondsSinceUnixEpoch(150);
  DeleteOldAndRecentWebRtcLogFiles({dir_.GetPath()}, delete_begin_time);

  const std::string expected_contents = "";
  ExpectContents(expected_contents);
}

TEST_F(WebRtcTextLogIndexCleanupTest, CanBeConsideredObsoleteDueToUploadDate) {
  const std::string contents = "160.1,report_id_1,local_id_1,101.1\n";
  CreateLogListFileWithContents(contents);

  const base::Time delete_begin_time =
      base::Time::FromSecondsSinceUnixEpoch(150);
  DeleteOldAndRecentWebRtcLogFiles({dir_.GetPath()}, delete_begin_time);

  const std::string expected_contents = "";
  ExpectContents(expected_contents);
}

TEST_F(WebRtcTextLogIndexCleanupTest, LinesWithTooFewTokensConsideredInvalid) {
  const std::string contents = "100.1,report_id_1,local_id_1\n";
  CreateLogListFileWithContents(contents);

  const base::Time delete_begin_time =
      base::Time::FromSecondsSinceUnixEpoch(150);
  DeleteOldAndRecentWebRtcLogFiles({dir_.GetPath()}, delete_begin_time);

  const std::string expected_contents = "";
  ExpectContents(expected_contents);
}

TEST_F(WebRtcTextLogIndexCleanupTest,
       LinesWithTooManyTokensConsideredInvalidEmptyVersion) {
  const std::string contents = "100.1,report_id_1,local_id_1,101.1,\n";
  CreateLogListFileWithContents(contents);

  const base::Time delete_begin_time =
      base::Time::FromSecondsSinceUnixEpoch(150);
  DeleteOldAndRecentWebRtcLogFiles({dir_.GetPath()}, delete_begin_time);

  const std::string expected_contents = "";
  ExpectContents(expected_contents);
}

TEST_F(WebRtcTextLogIndexCleanupTest,
       LinesWithTooManyTokensConsideredInvalidNonEmptyVersion) {
  const std::string contents = "100.1,report_id_1,local_id_1,101.1,102\n";
  CreateLogListFileWithContents(contents);

  const base::Time delete_begin_time =
      base::Time::FromSecondsSinceUnixEpoch(150);
  DeleteOldAndRecentWebRtcLogFiles({dir_.GetPath()}, delete_begin_time);

  const std::string expected_contents = "";
  ExpectContents(expected_contents);
}

TEST_F(WebRtcTextLogIndexCleanupTest,
       LinesWithUnparsableUploadDateConsideredInvalid) {
  const std::string contents = "100.1.2,report_id_1,local_id_1,101.1\n";
  CreateLogListFileWithContents(contents);

  const base::Time delete_begin_time =
      base::Time::FromSecondsSinceUnixEpoch(150);
  DeleteOldAndRecentWebRtcLogFiles({dir_.GetPath()}, delete_begin_time);

  const std::string expected_contents = "";
  ExpectContents(expected_contents);
}

TEST_F(WebRtcTextLogIndexCleanupTest,
       LinesWithUnparsableCaptureDateConsideredInvalid) {
  const std::string contents = "100.1,report_id_1,local_id_1,101.1.2\n";
  CreateLogListFileWithContents(contents);

  const base::Time delete_begin_time =
      base::Time::FromSecondsSinceUnixEpoch(150);
  DeleteOldAndRecentWebRtcLogFiles({dir_.GetPath()}, delete_begin_time);

  const std::string expected_contents = "";
  ExpectContents(expected_contents);
}

TEST_F(WebRtcTextLogIndexCleanupTest, LeavesOversizedLogListUntouched) {
  const std::string oversized_contents(kMaxLogListFileSizeBytes + 1, 'a');
  CreateLogListFileWithContents(oversized_contents);
  const base::FilePath log_file = dir_.GetPath().AppendASCII("123.gz");
  ASSERT_TRUE(base::WriteFile(log_file, "log"));

  const base::Time delete_begin_time =
      base::Time::FromSecondsSinceUnixEpoch(150);
  DeleteOldAndRecentWebRtcLogFiles({dir_.GetPath()}, delete_begin_time);

  EXPECT_FALSE(base::PathExists(log_file));
  std::string actual_contents;
  ASSERT_TRUE(base::ReadFileToString(log_list_path_, &actual_contents));
  EXPECT_EQ(oversized_contents.size(), actual_contents.size());
  EXPECT_EQ(oversized_contents, actual_contents);
}

TEST_F(WebRtcTextLogIndexCleanupTest, DeletingLogFileClearsItsLocalId) {
  CreateLogListFileWithContents("100.2,report_id_2,local_id_2,101.2\n");
  const base::FilePath log_file = dir_.GetPath().AppendASCII("local_id_2.gz");
  ASSERT_TRUE(base::WriteFile(log_file, "log"));

  // The file (modified now) is in the deletion range; the index line
  // (timestamps 100.2 and 101.2) is not, so it stays with its ID cleared.
  DeleteOldAndRecentWebRtcLogFiles({dir_.GetPath()},
                                   base::Time::FromSecondsSinceUnixEpoch(150));

  EXPECT_FALSE(base::PathExists(log_file));
  ExpectContents("100.2,report_id_2,,101.2\n");
}

#if BUILDFLAG(IS_POSIX)
TEST_F(WebRtcTextLogIndexCleanupTest, LeavesUnreadableLogListUntouched) {
  if (geteuid() == 0) {
    GTEST_SKIP() << "chmod 000 does not block reads when running as root.";
  }

  const std::string contents = "200.1,report_id_1,local_id_1,201.1\n";
  CreateLogListFileWithContents(contents);
  ASSERT_EQ(0, chmod(log_list_path_.value().c_str(), 0000));
  const base::FilePath log_file = dir_.GetPath().AppendASCII("123.gz");
  ASSERT_TRUE(base::WriteFile(log_file, "log"));

  const base::Time delete_begin_time =
      base::Time::FromSecondsSinceUnixEpoch(150);
  DeleteOldAndRecentWebRtcLogFiles({dir_.GetPath()}, delete_begin_time);

  EXPECT_FALSE(base::PathExists(log_file));
  ASSERT_EQ(0, chmod(log_list_path_.value().c_str(), 0600));
  ExpectContents(contents);
}

TEST_F(WebRtcTextLogIndexCleanupTest, ReplacesFifoAtLogListPathWithoutHanging) {
  ASSERT_EQ(0, mkfifo(log_list_path_.value().c_str(), 0600));

  DeleteOldWebRtcLogFiles({dir_.GetPath()});

  struct stat st;
  ASSERT_EQ(0, lstat(log_list_path_.value().c_str(), &st));
  EXPECT_TRUE(S_ISREG(st.st_mode));
  ExpectContents("");

  // Also test with a writer held open on the FIFO so that a non-blocking read
  // would fail with EAGAIN without the S_ISFIFO check.
  ASSERT_TRUE(base::DeleteFile(log_list_path_));
  ASSERT_EQ(0, mkfifo(log_list_path_.value().c_str(), 0600));
  base::ScopedFD writer(
      HANDLE_EINTR(open(log_list_path_.value().c_str(), O_RDWR | O_NONBLOCK)));
  ASSERT_TRUE(writer.is_valid());

  DeleteOldWebRtcLogFiles({dir_.GetPath()});

  ASSERT_EQ(0, lstat(log_list_path_.value().c_str(), &st));
  EXPECT_TRUE(S_ISREG(st.st_mode));
  ExpectContents("");
}

TEST_F(WebRtcTextLogIndexCleanupTest, IgnoresDirectoryAtLogListPath) {
  ASSERT_TRUE(base::CreateDirectory(log_list_path_));

  std::string contents;
  EXPECT_EQ(LogListReadResult::kError,
            TextLogList::ReadLogListFile(log_list_path_, &contents));

  DeleteOldWebRtcLogFiles({dir_.GetPath()});

  EXPECT_TRUE(base::DirectoryExists(log_list_path_));
}

TEST_F(WebRtcTextLogIndexCleanupTest,
       DoesNotFollowSymlinkEntryInsideLogDirectory) {
  base::ScopedTempDir victim_dir;
  ASSERT_TRUE(victim_dir.CreateUniqueTempDir());
  const base::FilePath old_external_file =
      victim_dir.GetPath().AppendASCII("old_external_file");
  ASSERT_TRUE(base::WriteFile(old_external_file, "important external data"));
  const base::Time old_time =
      base::Time::Now() - kTimeToKeepLogs - base::Days(2);
  ASSERT_TRUE(base::TouchFile(old_external_file, old_time, old_time));

  const base::FilePath old_regular_file =
      dir_.GetPath().AppendASCII("old_log.gz");
  ASSERT_TRUE(base::WriteFile(old_regular_file, "old log data"));
  ASSERT_TRUE(base::TouchFile(old_regular_file, old_time, old_time));

  const base::FilePath symlink_entry =
      dir_.GetPath().AppendASCII("symlink_log.gz");
  ASSERT_TRUE(base::CreateSymbolicLink(old_external_file, symlink_entry));

  // Use a recent delete_begin_time so the newly created symlink's own
  // timestamp falls within the deletion window and it only survives because of
  // the !S_ISREG check.
  DeleteOldAndRecentWebRtcLogFiles({dir_.GetPath()},
                                   base::Time::Now() - base::Hours(1));

  EXPECT_FALSE(base::PathExists(old_regular_file));
  EXPECT_TRUE(base::PathExists(old_external_file));
  // The symlink entry itself is skipped, not deleted.
  EXPECT_TRUE(base::IsLink(symlink_entry));
}

TEST_F(WebRtcTextLogIndexCleanupTest, DoesNotFollowSymlinkForLogList) {
  base::ScopedTempDir victim_dir;
  ASSERT_TRUE(victim_dir.CreateUniqueTempDir());
  const base::FilePath victim_file =
      victim_dir.GetPath().AppendASCII("victim_file");
  const std::string victim_contents =
      "100.1,report_id_1,local_id_1,101.1\n"
      "200.1,report_id_2,local_id_2,201.1\n";
  ASSERT_TRUE(base::WriteFile(victim_file, victim_contents));
  ASSERT_TRUE(base::CreateSymbolicLink(victim_file, log_list_path_));
  ASSERT_TRUE(base::IsLink(log_list_path_));

  const base::Time delete_begin_time =
      base::Time::FromSecondsSinceUnixEpoch(150);
  DeleteOldAndRecentWebRtcLogFiles({dir_.GetPath()}, delete_begin_time);

  EXPECT_FALSE(base::IsLink(log_list_path_));
  ExpectContents("");

  std::string actual_victim_contents;
  ASSERT_TRUE(base::ReadFileToString(victim_file, &actual_victim_contents));
  EXPECT_EQ(victim_contents, actual_victim_contents);
}

TEST_F(WebRtcTextLogIndexCleanupTest, DoesNotFollowDanglingSymlinkForLogList) {
  base::ScopedTempDir victim_dir;
  ASSERT_TRUE(victim_dir.CreateUniqueTempDir());
  const base::FilePath victim_file =
      victim_dir.GetPath().AppendASCII("nonexistent_victim");
  ASSERT_TRUE(base::CreateSymbolicLink(victim_file, log_list_path_));
  ASSERT_TRUE(base::IsLink(log_list_path_));

  DeleteOldWebRtcLogFiles({dir_.GetPath()});

  EXPECT_FALSE(base::IsLink(log_list_path_));
  EXPECT_FALSE(base::PathExists(victim_file));
  ExpectContents("");
}
#endif  // BUILDFLAG(IS_POSIX)

#if BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_WIN)
TEST_F(WebRtcTextLogIndexCleanupTest, DoesNotFollowDirectorySymlink) {
  base::ScopedTempDir victim_dir;
  ASSERT_TRUE(victim_dir.CreateUniqueTempDir());
  const base::FilePath old_victim_file =
      victim_dir.GetPath().AppendASCII("old_file");
  ASSERT_TRUE(base::WriteFile(old_victim_file, "important data"));
  const base::Time old_time =
      base::Time::Now() - kTimeToKeepLogs - base::Days(2);
  ASSERT_TRUE(base::TouchFile(old_victim_file, old_time, old_time));

  const base::FilePath symlink_log_dir =
      dir_.GetPath().AppendASCII("symlink_dir");
  ScopedDirectoryLink dir_link;
  ASSERT_TRUE(dir_link.Create(symlink_log_dir, victim_dir.GetPath()));
  ASSERT_TRUE(base::IsLink(symlink_log_dir));

  DeleteOldWebRtcLogFiles({symlink_log_dir});

  EXPECT_TRUE(base::PathExists(old_victim_file));
}
#endif  // BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_WIN)

}  // namespace webrtc_logging
