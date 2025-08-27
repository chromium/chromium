// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/model/download_record_service.h"

#import <memory>
#import <optional>
#import <string>
#import <vector>

#import "base/barrier_closure.h"
#import "base/files/scoped_temp_dir.h"
#import "base/functional/bind.h"
#import "base/run_loop.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/bind.h"
#import "base/test/scoped_feature_list.h"
#import "ios/chrome/browser/download/model/download_record.h"
#import "ios/chrome/browser/download/model/download_record_observer.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/web/public/test/fakes/fake_download_task.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

using testing::_;
using testing::StrictMock;

namespace {

// Mock observer for testing notifications.
class MockDownloadRecordObserver : public DownloadRecordObserver {
 public:
  MOCK_METHOD(void,
              OnDownloadAdded,
              (const DownloadRecord& record),
              (override));
  MOCK_METHOD(void,
              OnDownloadUpdated,
              (const DownloadRecord& record),
              (override));
  MOCK_METHOD(void,
              OnDownloadsRemoved,
              (const std::vector<std::string_view>& download_ids),
              (override));
};

}  // namespace

class DownloadRecordServiceTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();

    feature_list_.InitAndEnableFeature(kDownloadList);

    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    service_ = std::make_unique<DownloadRecordService>(temp_dir_.GetPath());
    ASSERT_TRUE(service_);

    // Wait for database initialization to complete.
    task_environment_.RunUntilIdle();
  }

  void TearDown() override {
    service_.reset();
    PlatformTest::TearDown();
  }

  std::unique_ptr<web::FakeDownloadTask> CreateFakeDownloadTask(
      const std::string& identifier,
      const std::string& original_url = "https://example.com/file.pdf",
      const std::string& mime_type = "application/pdf") {
    EXPECT_FALSE(identifier.empty());

    auto task =
        std::make_unique<web::FakeDownloadTask>(GURL(original_url), mime_type);
    task->SetIdentifier(@(identifier.c_str()));
    EXPECT_NSEQ(@(identifier.c_str()), task->GetIdentifier());
    return task;
  }

  void RecordDownloadAndValidate(web::DownloadTask* task) {
    base::RunLoop run_loop;
    StrictMock<MockDownloadRecordObserver> mock_observer;
    service_->AddObserver(&mock_observer);

    EXPECT_CALL(mock_observer, OnDownloadAdded(_))
        .WillOnce([&](const DownloadRecord& record) { run_loop.Quit(); });

    service_->RecordDownload(task);
    run_loop.Run();

    service_->RemoveObserver(&mock_observer);
  }

  web::WebTaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<DownloadRecordService> service_;
};

TEST_F(DownloadRecordServiceTest, RecordDownload) {
  auto task = CreateFakeDownloadTask("test_download_1");
  RecordDownloadAndValidate(task.get());
}

TEST_F(DownloadRecordServiceTest, GetAllDownloads) {
  auto task1 = CreateFakeDownloadTask("download_1");
  auto task2 = CreateFakeDownloadTask("download_2");

  RecordDownloadAndValidate(task1.get());
  RecordDownloadAndValidate(task2.get());

  base::RunLoop run_loop;
  std::vector<DownloadRecord> result;

  service_->GetAllDownloadsAsync(
      base::BindLambdaForTesting([&](std::vector<DownloadRecord> records) {
        result = std::move(records);
        run_loop.Quit();
      }));

  run_loop.Run();

  EXPECT_EQ(2u, result.size());
  EXPECT_EQ(base::SysNSStringToUTF8(task1->GetIdentifier()),
            result[0].download_id);
  EXPECT_EQ(base::SysNSStringToUTF8(task2->GetIdentifier()),
            result[1].download_id);
}

TEST_F(DownloadRecordServiceTest, GetDownloadById) {
  const std::string download_id = "test_download";
  auto task = CreateFakeDownloadTask(download_id);
  RecordDownloadAndValidate(task.get());

  base::RunLoop run_loop;
  std::optional<DownloadRecord> result;

  service_->GetDownloadByIdAsync(
      download_id,
      base::BindLambdaForTesting([&](std::optional<DownloadRecord> record) {
        result = std::move(record);
        run_loop.Quit();
      }));

  run_loop.Run();

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(download_id, result->download_id);
}

TEST_F(DownloadRecordServiceTest, GetNonExistentDownloadById) {
  base::RunLoop run_loop;
  std::optional<DownloadRecord> result;

  service_->GetDownloadByIdAsync(
      "non_existent",
      base::BindLambdaForTesting([&](std::optional<DownloadRecord> record) {
        result = std::move(record);
        run_loop.Quit();
      }));

  run_loop.Run();

  EXPECT_FALSE(result.has_value());
}

TEST_F(DownloadRecordServiceTest, RemoveDownloadById) {
  const std::string download_id = "test_download";
  auto task = CreateFakeDownloadTask(download_id);
  RecordDownloadAndValidate(task.get());

  StrictMock<MockDownloadRecordObserver> mock_observer;
  service_->AddObserver(&mock_observer);

  base::RunLoop run_loop;
  bool removal_success = false;

  // Expects OnDownloadsRemoved to be called with the correct download ID.
  EXPECT_CALL(mock_observer, OnDownloadsRemoved(_))
      .WillOnce([&](const std::vector<std::string_view>& download_ids) {
        ASSERT_EQ(1u, download_ids.size());
        EXPECT_EQ(download_id, download_ids[0]);
      });

  service_->RemoveDownloadByIdAsync(
      download_id, base::BindLambdaForTesting([&](bool success) {
        removal_success = success;
        run_loop.Quit();
      }));

  run_loop.Run();

  EXPECT_TRUE(removal_success);
  service_->RemoveObserver(&mock_observer);
}

TEST_F(DownloadRecordServiceTest, RemoveNonExistentDownloadById) {
  base::RunLoop run_loop;
  bool removal_success = false;

  service_->RemoveDownloadByIdAsync(
      "non_existent_download", base::BindLambdaForTesting([&](bool success) {
        removal_success = success;
        run_loop.Quit();
      }));

  run_loop.Run();

  // The implementation considers removing non-existent records as success.
  EXPECT_TRUE(removal_success);
}

TEST_F(DownloadRecordServiceTest, UpdateDownloadStates) {
  const std::string download_id = "state_test_download";
  auto task = CreateFakeDownloadTask(download_id);
  RecordDownloadAndValidate(task.get());

  StrictMock<MockDownloadRecordObserver> mock_observer;
  service_->AddObserver(&mock_observer);

  // Tests progress update.
  base::RunLoop progress_loop;
  EXPECT_CALL(mock_observer, OnDownloadUpdated(_))
      .WillOnce([&](const DownloadRecord& record) {
        EXPECT_EQ(500, record.received_bytes);
        progress_loop.Quit();
      });
  task->SetReceivedBytes(500);
  progress_loop.Run();

  // Tests completion update.
  base::RunLoop completion_loop;
  EXPECT_CALL(mock_observer, OnDownloadUpdated(_))
      .WillOnce([&](const DownloadRecord& record) {
        EXPECT_EQ(web::DownloadTask::State::kComplete, record.state);
        completion_loop.Quit();
      });
  task->SetDone(true);
  completion_loop.Run();

  service_->RemoveObserver(&mock_observer);
}

TEST_F(DownloadRecordServiceTest, NotifiesAllObservers) {
  MockDownloadRecordObserver observer1;
  MockDownloadRecordObserver observer2;

  service_->AddObserver(&observer1);
  service_->AddObserver(&observer2);

  auto task = CreateFakeDownloadTask("test_download");

  base::RunLoop run_loop1;
  auto barrier = base::BarrierClosure(2, run_loop1.QuitClosure());

  // Both observers should be notified.
  EXPECT_CALL(observer1, OnDownloadAdded(_))
      .WillOnce([barrier](const DownloadRecord& record) { barrier.Run(); });
  EXPECT_CALL(observer2, OnDownloadAdded(_))
      .WillOnce([barrier](const DownloadRecord& record) { barrier.Run(); });

  service_->RecordDownload(task.get());
  run_loop1.Run();

  // Removes one observer.
  service_->RemoveObserver(&observer1);

  auto task2 = CreateFakeDownloadTask("test_download_2");

  base::RunLoop run_loop2;

  // Only observer2 should be notified.
  EXPECT_CALL(observer2, OnDownloadAdded(_))
      .WillOnce([&](const DownloadRecord& record) { run_loop2.Quit(); });

  service_->RecordDownload(task2.get());
  run_loop2.Run();

  service_->RemoveObserver(&observer2);
}

TEST_F(DownloadRecordServiceTest, PersistDataInDatabase) {
  const std::string download_id = "persistent_download";
  auto task = CreateFakeDownloadTask(download_id);
  RecordDownloadAndValidate(task.get());

  // Creates new service instance with same database path.
  service_.reset();
  service_ = std::make_unique<DownloadRecordService>(temp_dir_.GetPath());

  base::RunLoop verify_loop;
  std::optional<DownloadRecord> result;

  service_->GetDownloadByIdAsync(
      download_id,
      base::BindLambdaForTesting([&](std::optional<DownloadRecord> record) {
        result = std::move(record);
        verify_loop.Quit();
      }));

  verify_loop.Run();
  // Verifies download still exists.
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(download_id, result->download_id);
}
