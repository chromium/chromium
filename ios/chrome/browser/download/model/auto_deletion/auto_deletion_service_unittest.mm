// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/model/auto_deletion/auto_deletion_service.h"

#import "base/files/scoped_temp_dir.h"
#import "base/values.h"
#import "components/prefs/pref_service.h"
#import "components/prefs/testing_pref_service.h"
#import "ios/chrome/browser/download/model/auto_deletion/auto_deletion_test_utils.h"
#import "ios/chrome/browser/download/model/auto_deletion/scheduled_file_queue.h"
#import "ios/chrome/browser/download/model/auto_deletion/scheduler.h"
#import "ios/chrome/browser/shared/model/prefs/browser_prefs.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/chrome/test/testing_application_context.h"
#import "ios/web/public/test/fakes/fake_download_task.h"
#import "testing/platform_test.h"

namespace {

// Arbitrary URL.
const char kUrl[] = "https://example.test/";
// Arbitrary example file name.
const base::FilePath::CharType kDownloadFileName[] =
    FILE_PATH_LITERAL("file_download.pdf");
// Arbitrary file content.
const std::string kDownloadFileData = "file_download_data";

// Creates a FakeDownloadTask for testing.
std::unique_ptr<web::DownloadTask> CreateTask(const base::FilePath& file_path) {
  auto task = std::make_unique<web::FakeDownloadTask>(GURL(kUrl), "test/test");
  NSData* data = [NSData dataWithBytes:kDownloadFileData.data()
                                length:kDownloadFileData.size()];
  task->SetResponseData(data);
  task->SetGeneratedFileName(base::FilePath(kDownloadFileName));
  task->Start(file_path);
  return task;
}

}  // namespace

namespace auto_deletion {

class AutoDeletionServiceTest : public PlatformTest {
 protected:
  AutoDeletionServiceTest() {
    auto_deletion_service_ = std::make_unique<AutoDeletionService>();
  }

  void SetUp() override {
    ASSERT_TRUE(scoped_temp_directory_.CreateUniqueTempDir());
    directory_ = scoped_temp_directory_.GetPath();
  }

  void TearDown() override {
    auto_deletion_service_.reset();
    PlatformTest::TearDown();
  }

  PrefService* local_state() {
    return GetApplicationContext()->GetLocalState();
  }
  AutoDeletionService* service() { return auto_deletion_service_.get(); }
  const base::FilePath& directory() const { return directory_; }

 private:
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  std::unique_ptr<AutoDeletionService> auto_deletion_service_;
  base::ScopedTempDir scoped_temp_directory_;
  base::FilePath directory_;
};

// Tests that the auto deletion service successfully schedules one file for
// deletion.
TEST_F(AutoDeletionServiceTest, ScheduleOneFileForDeletion) {
  // Create web::DownloadTask & schedule download for auto deletion.
  std::unique_ptr<web::DownloadTask> task = CreateTask(directory());
  web::DownloadTask* task_ptr = task.get();
  service()->ScheduleFileForDeletion(std::move(task_ptr));

  // Check that the pref has one value.
  const base::Value::List& downloads =
      local_state()->GetList(prefs::kDownloadAutoDeletionScheduledFiles);
  EXPECT_EQ(downloads.size(), 1u);
}

// Tests that the auto deletion service successfully schedules multiple file for
// deletion.
TEST_F(AutoDeletionServiceTest, ScheduleMultipleFilesForDeletion) {
  // Create multiple web::DownloadTask tasks.
  std::vector<std::unique_ptr<web::DownloadTask>> tasks;
  for (int i = 0; i < 10; i++) {
    auto task = CreateTask(directory());
    tasks.push_back(std::move(task));
  }

  // Invoke the FileSchedule on all the `tasks`.
  for (const auto& task : tasks) {
    web::DownloadTask* task_ptr = task.get();
    service()->ScheduleFileForDeletion(task_ptr);
  }

  // Check that the pref has multiple values.
  const base::Value::List& downloads =
      local_state()->GetList(prefs::kDownloadAutoDeletionScheduledFiles);
  EXPECT_EQ(downloads.size(), 10u);
}

}  // namespace auto_deletion
