// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/crash/crashpad_database_manager.h"

#include <string>
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

}  // namespace remoting
