// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/crash/crashpad_database_manager.h"

#include <string>
#include <string_view>
#include <vector>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/crashpad/crashpad/client/crash_report_database.h"

namespace remoting {

namespace {

class TestLogger : public CrashpadDatabaseManager::Logger {
 public:
  void Log(std::string_view message) const override {
    logged_messages_.emplace_back(message);
  }

  void LogError(std::string_view message) const override {
    logged_errors_.emplace_back(message);
  }

  const std::vector<std::string>& logged_messages() const {
    return logged_messages_;
  }

  const std::vector<std::string>& logged_errors() const {
    return logged_errors_;
  }

 private:
  mutable std::vector<std::string> logged_messages_;
  mutable std::vector<std::string> logged_errors_;
};

}  // namespace

class CrashpadDatabaseManagerTest : public testing::Test {
 protected:
  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  base::ScopedTempDir temp_dir_;
  TestLogger logger_;
};

TEST_F(CrashpadDatabaseManagerTest, InitializeWithCustomPath) {
  CrashpadDatabaseManager manager(logger_);
  base::FilePath db_path =
      temp_dir_.GetPath().Append(FILE_PATH_LITERAL("crash_db"));

  EXPECT_TRUE(manager.InitializeCrashpadDatabase(db_path));
  EXPECT_TRUE(base::DirectoryExists(db_path));
  EXPECT_TRUE(logger_.logged_errors().empty());
}

TEST_F(CrashpadDatabaseManagerTest, EnableReportUploads) {
  CrashpadDatabaseManager manager(logger_);
  base::FilePath db_path =
      temp_dir_.GetPath().Append(FILE_PATH_LITERAL("crash_db"));

  ASSERT_TRUE(manager.InitializeCrashpadDatabase(db_path));
  EXPECT_TRUE(manager.EnableReportUploads());
  EXPECT_TRUE(logger_.logged_errors().empty());
}

TEST_F(CrashpadDatabaseManagerTest, LogAndCleanupReportsEmptyDatabase) {
  CrashpadDatabaseManager manager(logger_);
  base::FilePath db_path =
      temp_dir_.GetPath().Append(FILE_PATH_LITERAL("crash_db"));

  ASSERT_TRUE(manager.InitializeCrashpadDatabase(db_path));
  manager.LogCompletedCrashpadReports();
  manager.LogPendingCrashpadReports();
  EXPECT_TRUE(manager.CleanupCompletedCrashpadReports());
  EXPECT_TRUE(logger_.logged_errors().empty());
}

TEST_F(CrashpadDatabaseManagerTest, LogAndCleanupWithReports) {
  base::FilePath db_path =
      temp_dir_.GetPath().Append(FILE_PATH_LITERAL("crash_db"));
  auto database = crashpad::CrashReportDatabase::Initialize(db_path);
  ASSERT_TRUE(database);

  std::unique_ptr<crashpad::CrashReportDatabase::NewReport> new_report;
  ASSERT_EQ(database->PrepareNewCrashReport(&new_report),
            crashpad::CrashReportDatabase::kNoError);
  const std::string dummy_dump = "minidump content";
  ASSERT_TRUE(
      new_report->Writer()->Write(dummy_dump.data(), dummy_dump.size()));
  crashpad::UUID uuid;
  ASSERT_EQ(database->FinishedWritingCrashReport(std::move(new_report), &uuid),
            crashpad::CrashReportDatabase::kNoError);

  std::unique_ptr<const crashpad::CrashReportDatabase::UploadReport>
      upload_report;
  ASSERT_EQ(database->GetReportForUploading(uuid, &upload_report),
            crashpad::CrashReportDatabase::kNoError);
  ASSERT_EQ(
      database->RecordUploadComplete(std::move(upload_report), "test_crash_id"),
      crashpad::CrashReportDatabase::kNoError);

  CrashpadDatabaseManager manager(logger_);
  ASSERT_TRUE(manager.InitializeCrashpadDatabase(db_path));
  manager.LogCompletedCrashpadReports();
  manager.LogPendingCrashpadReports();
  EXPECT_TRUE(manager.CleanupCompletedCrashpadReports());

  EXPECT_TRUE(logger_.logged_errors().empty());
  bool found_created_timestamp = false;
  for (const auto& msg : logger_.logged_messages()) {
    if (msg.find("created:") != std::string::npos &&
        msg.find("UTC") != std::string::npos) {
      found_created_timestamp = true;
    }
  }
  EXPECT_TRUE(found_created_timestamp);
}

TEST_F(CrashpadDatabaseManagerTest, GetCrashpadDatabasePathIsNonEmpty) {
  EXPECT_FALSE(GetCrashpadDatabasePath().empty());
}

TEST_F(CrashpadDatabaseManagerTest, IsValidCrashpadUploadUrlOverride) {
  constexpr std::string_view kValidUrls[] = {
      "https://clients2.google.com/cr/staging_report",
      "https://example.com:8443/upload?foo=1",
      "https://my-host.corp.example.com/",
      "http://127.0.0.1/cr/report",
      "http://127.0.0.1:8080/cr/report",
      "http://localhost:9000/cr/report",
      "http://LocalHost/x",
      "https://127.0.0.1:65535/cr/report",
  };
  for (std::string_view url : kValidUrls) {
    EXPECT_TRUE(IsValidCrashpadUploadUrlOverride(url)) << url;
  }

  constexpr std::string_view kInvalidUrls[] = {
      "",
      "http://example.com/cr/report",
      "http://127.0.0.1.example.com/x",
      "http://localhost.example.com/x",
      "http://127.0.0.1@example.com/x",
      "http://127.0.0.2/x",
      "HTTP://127.0.0.1/x",
      "https://clients2.google.com",
      "https://host?q=/x",
      "https://host#/x",
      "https://example.com:8443/upload?foo=1#frag",
      "https://example.com/cr/report#frag",
      "https://user:pass@clients2.google.com/cr/report",
      "http://127.0.0.1:0/cr/report",
      "http://127.0.0.1:+80/x",
      "https://host:/x",
      "http://127.0.0.1:99999/cr/report",
      "https://[2001:db8::1]/cr/report",
      "http://[::1]/cr/report",
      "http://localhost./x",
      "https://example.com./x",
      "https://.example.com/cr/report",
      "https://-example.com/cr/report",
      "https://example-/x",
      "https://example.com-/x",
      "https://example-.com/x",
      "https://sub.-example.com/x",
      "https://exa..mple.com/cr/report",
      "HTTPS://clients2.google.com/cr/report",
      "ftp://127.0.0.1/cr/report",
      "https://clients2.google.com/cr/report ",
      "https://clients2.google.com/cr/\nreport",
      "https://ex\xC3\xA4mple.com/cr/report",
  };
  for (std::string_view url : kInvalidUrls) {
    EXPECT_FALSE(IsValidCrashpadUploadUrlOverride(url)) << url;
  }

  constexpr std::string_view kPrefix = "https://example.com/";
  const std::string max_len_url =
      std::string(kPrefix) +
      std::string(kMaxCrashpadUploadUrlChars - kPrefix.size(), 'a');
  EXPECT_EQ(max_len_url.size(), kMaxCrashpadUploadUrlChars);
  EXPECT_TRUE(IsValidCrashpadUploadUrlOverride(max_len_url));
  EXPECT_FALSE(IsValidCrashpadUploadUrlOverride(max_len_url + "a"));
}

TEST_F(CrashpadDatabaseManagerTest,
       InitializeWithoutCreatingDoesNotCreateDirectory) {
  base::FilePath db_path =
      temp_dir_.GetPath().Append(FILE_PATH_LITERAL("crash_db"));

  CrashpadDatabaseManager reader_before_init(logger_);
  EXPECT_FALSE(
      reader_before_init.InitializeCrashpadDatabaseWithoutCreating(db_path));
  EXPECT_FALSE(base::PathExists(db_path));

  CrashpadDatabaseManager creator(logger_);
  ASSERT_TRUE(creator.InitializeCrashpadDatabase(db_path));
  EXPECT_TRUE(base::DirectoryExists(db_path));

  CrashpadDatabaseManager reader_after_init(logger_);
  EXPECT_TRUE(
      reader_after_init.InitializeCrashpadDatabaseWithoutCreating(db_path));

  // Only creation of the root directory is skipped. Given an existing empty
  // directory, Crashpad still succeeds and creates its subdirectories inside
  // it, so callers that must not write into an unrelated directory have to
  // check for an initialized database first.
  base::FilePath empty_dir =
      temp_dir_.GetPath().Append(FILE_PATH_LITERAL("empty_dir"));
  ASSERT_TRUE(base::CreateDirectory(empty_dir));
  CrashpadDatabaseManager reader_empty_dir(logger_);
  EXPECT_TRUE(
      reader_empty_dir.InitializeCrashpadDatabaseWithoutCreating(empty_dir));
  EXPECT_FALSE(base::IsDirectoryEmpty(empty_dir));
}

}  // namespace remoting
