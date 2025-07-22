// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/model/auto_deletion/auto_deletion_service.h"

#import "base/files/scoped_temp_dir.h"
#import "base/functional/bind.h"
#import "base/run_loop.h"
#import "base/test/ios/wait_util.h"
#import "base/values.h"
#import "components/prefs/pref_service.h"
#import "components/prefs/testing_pref_service.h"
#import "ios/chrome/browser/download/model/auto_deletion/auto_deletion_test_utils.h"
#import "ios/chrome/browser/download/model/auto_deletion/scheduler.h"
#import "ios/chrome/browser/shared/model/prefs/browser_prefs.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/chrome/test/testing_application_context.h"
#import "ios/web/public/test/fakes/fake_download_task.h"
#import "ios/web/public/test/web_task_environment.h"
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
std::unique_ptr<web::FakeDownloadTask> CreateTask(
    const base::FilePath& file_path) {
  auto task = std::make_unique<web::FakeDownloadTask>(GURL(kUrl), "test/test");
  NSData* data = [NSData dataWithBytes:kDownloadFileData.data()
                                length:kDownloadFileData.size()];
  task->SetResponseData(data);
  task->SetGeneratedFileName(base::FilePath(kDownloadFileName));
  task->Start(file_path);
  task->SetState(web::DownloadTask::State::kComplete);
  return task;
}

}  // namespace

namespace auto_deletion {

class AutoDeletionServiceTest : public PlatformTest {
 public:
  size_t GetNumberOfFilesScheduledForDeletion() const {
    return GetApplicationContext()
        ->GetLocalState()
        ->GetList(prefs::kDownloadAutoDeletionScheduledFiles)
        .size();
  }

 protected:
  AutoDeletionServiceTest() {
    auto_deletion_service_ =
        std::make_unique<AutoDeletionService>(local_state());
  }

  void SetUp() override {
    ASSERT_TRUE(scoped_temp_directory_.CreateUniqueTempDir());
    directory_ = scoped_temp_directory_.GetPath();
  }

  void TearDown() override {
    PlatformTest::TearDown();
  }

  PrefService* local_state() {
    return GetApplicationContext()->GetLocalState();
  }
  AutoDeletionService* service() { return auto_deletion_service_.get(); }
  const base::FilePath& directory() const { return directory_; }

 private:
  base::test::TaskEnvironment task_environment_;
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
  service()->MarkTaskForDeletion(
      task_ptr, auto_deletion::DeletionEnrollmentStatus::kEnrolled);
  service()->MarkTaskForDeletion(task_ptr, directory());

  // Check that the pref has one value.
  EXPECT_EQ(GetNumberOfFilesScheduledForDeletion(), 1u);
}

TEST_F(AutoDeletionServiceTest,
       ScheduleOneFileForDeletionWhenFileLocationIsSetFirst) {
  // Create web::DownloadTask & schedule download for auto deletion.
  std::unique_ptr<web::DownloadTask> task = CreateTask(directory());
  web::DownloadTask* task_ptr = task.get();
  service()->MarkTaskForDeletion(task_ptr, directory());
  service()->MarkTaskForDeletion(
      task_ptr, auto_deletion::DeletionEnrollmentStatus::kEnrolled);

  // Check that the pref has one value.
  EXPECT_EQ(GetNumberOfFilesScheduledForDeletion(), 1u);
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
    service()->MarkTaskForDeletion(
        task_ptr, auto_deletion::DeletionEnrollmentStatus::kEnrolled);
    service()->MarkTaskForDeletion(task_ptr, directory());
  }

  // Check that the pref has multiple values.
  EXPECT_EQ(GetNumberOfFilesScheduledForDeletion(), 10u);
}

TEST_F(AutoDeletionServiceTest, DeleteOneFileScheduledForDeletion) {
  // Create a Scheduler that contains no files that are ready for deletion.
  base::TimeDelta start_point_in_past = base::Days(20);
  size_t number_of_files = 10;
  Scheduler scheduler(local_state());
  PopulateSchedulerWithAutoDeletionSchedule(scheduler, start_point_in_past,
                                            number_of_files);
  ASSERT_EQ(GetNumberOfFilesScheduledForDeletion(), 10u);

  base::RunLoop run_loop;
  service()->RemoveScheduledFilesReadyForDeletion(run_loop.QuitClosure());

  EXPECT_EQ(GetNumberOfFilesScheduledForDeletion(), 10u);
  run_loop.Run();
  EXPECT_EQ(GetNumberOfFilesScheduledForDeletion(), 9u);
}

TEST_F(AutoDeletionServiceTest, DeleteMultipleFilesScheduledForDeletion) {
  // Create a Scheduler where half of the files are ready for deletion.
  base::TimeDelta start_point_in_past = base::Days(24);
  size_t number_of_files = 10;
  Scheduler scheduler(local_state());
  PopulateSchedulerWithAutoDeletionSchedule(scheduler, start_point_in_past,
                                            number_of_files);
  ASSERT_EQ(GetNumberOfFilesScheduledForDeletion(), 10u);

  base::RunLoop run_loop;
  service()->RemoveScheduledFilesReadyForDeletion(run_loop.QuitClosure());

  EXPECT_EQ(GetNumberOfFilesScheduledForDeletion(), 10u);
  run_loop.Run();
  EXPECT_EQ(GetNumberOfFilesScheduledForDeletion(), 5u);
}

TEST_F(AutoDeletionServiceTest, DeleteAllFilesScheduledForDeletion) {
  // Create a Scheduler that contains no files that are ready for deletion.
  base::TimeDelta start_point_in_past = base::Days(31);
  size_t number_of_files = 10;
  Scheduler scheduler(local_state());
  PopulateSchedulerWithAutoDeletionSchedule(scheduler, start_point_in_past,
                                            number_of_files);
  ASSERT_EQ(GetNumberOfFilesScheduledForDeletion(), 10u);

  base::RunLoop run_loop;
  service()->RemoveScheduledFilesReadyForDeletion(run_loop.QuitClosure());

  EXPECT_EQ(GetNumberOfFilesScheduledForDeletion(), 10u);

  run_loop.Run();

  EXPECT_EQ(GetNumberOfFilesScheduledForDeletion(), 0u);
}

// Tests that the auto deletion service untracks the scheduled file.
TEST_F(AutoDeletionServiceTest, UntrackScheduledFileWhenServiceIsDisabled) {
  // Create web::DownloadTask & schedule download for auto deletion.
  std::unique_ptr<web::DownloadTask> task = CreateTask(directory());
  web::DownloadTask* task_ptr = task.get();
  service()->MarkTaskForDeletion(
      task_ptr, auto_deletion::DeletionEnrollmentStatus::kEnrolled);
  service()->MarkTaskForDeletion(task_ptr, directory());
  // Check that the pref has one value.
  ASSERT_EQ(GetNumberOfFilesScheduledForDeletion(), 1u);

  // This function is invoked when the Auto-deletion feature is disabled.
  service()->Clear();

  EXPECT_EQ(GetNumberOfFilesScheduledForDeletion(), 0u);
}

// Tests that the auto deletion service subscribes to the DownloadTask,
// schedules the file for deletion once the task is finished downloading, and
// remove itself from the task's observer list.
TEST_F(AutoDeletionServiceTest,
       DownloadTaskIsScheduledForDeletionOnceFinishedDownloading) {
  // Create web::DownloadTask.
  std::unique_ptr<web::FakeDownloadTask> task = CreateTask(directory());
  task->SetState(web::DownloadTask::State::kInProgress);
  web::DownloadTask* task_ptr = task.get();

  service()->MarkTaskForDeletion(
      task_ptr, auto_deletion::DeletionEnrollmentStatus::kEnrolled);
  service()->MarkTaskForDeletion(task_ptr, directory());
  ASSERT_EQ(GetNumberOfFilesScheduledForDeletion(), 0u);
  task->SetState(web::DownloadTask::State::kComplete);
  // Wait for the AutoDeletionService to be notified of the change in the
  // DownloadTask's state and schedule the file for deletion.
  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kSpinDelaySeconds, ^{
        return GetNumberOfFilesScheduledForDeletion() == 1u;
      }));

  EXPECT_EQ(GetNumberOfFilesScheduledForDeletion(), 1u);
}

}  // namespace auto_deletion
