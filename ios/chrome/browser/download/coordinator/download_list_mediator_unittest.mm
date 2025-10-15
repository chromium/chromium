// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/coordinator/download_list_mediator.h"

#import <UIKit/UIKit.h>

#import <algorithm>
#import <memory>
#import <set>
#import <string>
#import <vector>

#import "base/files/file_path.h"
#import "base/files/file_util.h"
#import "base/files/scoped_temp_dir.h"
#import "base/functional/bind.h"
#import "base/run_loop.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/scoped_feature_list.h"
#import "base/time/time.h"
#import "components/keyed_service/core/keyed_service.h"
#import "components/test/ios/test_utils.h"
#import "ios/chrome/browser/download/model/download_directory_util.h"
#import "ios/chrome/browser/download/model/download_filter_util.h"
#import "ios/chrome/browser/download/model/download_record.h"
#import "ios/chrome/browser/download/model/download_record_observer.h"
#import "ios/chrome/browser/download/model/download_record_observer_bridge.h"
#import "ios/chrome/browser/download/model/download_record_service.h"
#import "ios/chrome/browser/download/ui/download_list/download_list_consumer.h"
#import "ios/chrome/browser/download/ui/download_list/download_list_item.h"
#import "ios/chrome/browser/shared/model/utils/mime_type_util.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/web/public/download/download_task.h"
#import "ios/web/public/test/fakes/fake_download_task.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

using testing::_;

namespace {

// Mock implementation of DownloadRecordService for testing.
class MockDownloadRecordService : public DownloadRecordService {
 public:
  MockDownloadRecordService() = default;
  ~MockDownloadRecordService() override = default;

  // Import callback types from the base class
  using DownloadRecordsCallback =
      DownloadRecordService::DownloadRecordsCallback;
  using DownloadRecordCallback = DownloadRecordService::DownloadRecordCallback;
  using CompletionCallback = DownloadRecordService::CompletionCallback;

  // Creates records directly for testing purposes.
  void CreateRecordsForTesting() {
    // Create DownloadRecord objects directly
    DownloadRecord pdf_record;
    pdf_record.download_id = "1";
    pdf_record.original_url = "https://testsite.org/document.pdf";
    pdf_record.mime_type = kAdobePortableDocumentFormatMimeType;
    pdf_record.file_name = "document.pdf";
    pdf_record.created_time = base::Time::Now();

    DownloadRecord image_record;
    image_record.download_id = "2";
    image_record.original_url = "https://testsite.org/image.jpg";
    image_record.mime_type = kJPEGImageMimeType;
    image_record.file_name = "image.jpg";
    image_record.created_time = base::Time::Now();

    DownloadRecord video_record;
    video_record.download_id = "3";
    video_record.original_url = "https://testsite.org/video.mp4";
    video_record.mime_type = kMP4VideoMimeType;
    video_record.file_name = "video.mp4";
    video_record.created_time = base::Time::Now();

    DownloadRecord audio_record;
    audio_record.download_id = "4";
    audio_record.original_url = "https://testsite.org/audio.mp3";
    audio_record.mime_type = kMP3AudioMimeType;
    audio_record.file_name = "audio.mp3";
    audio_record.created_time = base::Time::Now();

    DownloadRecord text_record;
    text_record.download_id = "5";
    text_record.original_url = "https://testsite.org/document.txt";
    text_record.mime_type = kTextMimeType;
    text_record.file_name = "document.txt";
    text_record.created_time = base::Time::Now();

    DownloadRecord zip_record;
    zip_record.download_id = "6";
    zip_record.original_url = "https://testsite.org/archive.zip";
    zip_record.mime_type = kZipArchiveMimeType;
    zip_record.file_name = "archive.zip";
    zip_record.created_time = base::Time::Now();

    // Add incognito records for testing incognito functionality.
    DownloadRecord incognito_pdf_record;
    incognito_pdf_record.download_id = "7";
    incognito_pdf_record.original_url =
        "https://testsite.org/incognito_document.pdf";
    incognito_pdf_record.mime_type = kAdobePortableDocumentFormatMimeType;
    incognito_pdf_record.file_name = "incognito_document.pdf";
    incognito_pdf_record.created_time = base::Time::Now();
    incognito_pdf_record.is_incognito = true;

    DownloadRecord incognito_image_record;
    incognito_image_record.download_id = "8";
    incognito_image_record.original_url =
        "https://testsite.org/incognito_image.jpg";
    incognito_image_record.mime_type = kJPEGImageMimeType;
    incognito_image_record.file_name = "incognito_image.jpg";
    incognito_image_record.created_time = base::Time::Now();
    incognito_image_record.is_incognito = true;

    stored_records_.push_back(pdf_record);
    stored_records_.push_back(image_record);
    stored_records_.push_back(video_record);
    stored_records_.push_back(audio_record);
    stored_records_.push_back(text_record);
    stored_records_.push_back(zip_record);
    stored_records_.push_back(incognito_pdf_record);
    stored_records_.push_back(incognito_image_record);
  }

  // Override virtual methods directly.
  void RecordDownload(web::DownloadTask* task) override {}

  void GetAllDownloadsAsync(DownloadRecordsCallback callback) override {
    std::move(callback).Run(stored_records_);
  }

  void GetDownloadByIdAsync(const std::string& download_id,
                            DownloadRecordCallback callback) override {}

  void UpdateDownloadFilePathAsync(const std::string& download_id,
                                   const base::FilePath& file_path,
                                   CompletionCallback callback) override {}

  web::DownloadTask* GetDownloadTaskById(
      std::string_view download_id) const override {
    // Return mock task if it exists for the given download_id.
    auto it = mock_download_tasks_.find(std::string(download_id));
    if (it != mock_download_tasks_.end()) {
      return it->second.get();
    }
    return nullptr;
  }

  void AddObserver(DownloadRecordObserver* observer) override {
    observers_.insert(observer);
  }

  void RemoveObserver(DownloadRecordObserver* observer) override {
    observers_.erase(observer);
  }

  size_t GetRecordCount() const { return stored_records_.size(); }

  // Mock methods for testing.
  MOCK_METHOD(void,
              RemoveDownloadByIdAsync,
              (const std::string& download_id, CompletionCallback callback),
              (override));

  // Public methods for testing access to internal state.
  void AddRecordForTesting(const DownloadRecord& record) {
    stored_records_.push_back(record);
    for (auto* observer : observers_) {
      observer->OnDownloadAdded(record);
    }
  }

  void TriggerUpdateForTesting(const DownloadRecord& record) {
    // First update the stored record
    auto it =
        std::find_if(stored_records_.begin(), stored_records_.end(),
                     [&record](const DownloadRecord& stored_record) {
                       return stored_record.download_id == record.download_id;
                     });

    if (it != stored_records_.end()) {
      *it = record;  // Update the existing record
    }

    // Then notify observers
    for (auto* observer : observers_) {
      observer->OnDownloadUpdated(record);
    }
  }

  void RemoveRecordForTesting(const std::string& download_id) {
    // Remove record from stored_records_
    auto it = std::find_if(stored_records_.begin(), stored_records_.end(),
                           [&download_id](const DownloadRecord& record) {
                             return record.download_id == download_id;
                           });

    if (it != stored_records_.end()) {
      stored_records_.erase(it);
      std::vector<std::string_view> removed_ids = {download_id};
      for (auto* observer : observers_) {
        observer->OnDownloadsRemoved(removed_ids);
      }
    }
  }

  // Adds a mock download task for testing cancelDownloadItem.
  void AddMockDownloadTaskForTesting(
      const std::string& download_id,
      std::unique_ptr<web::FakeDownloadTask> task) {
    mock_download_tasks_[download_id] = std::move(task);
  }

 private:
  std::vector<DownloadRecord> stored_records_;
  std::set<DownloadRecordObserver*> observers_;
  std::map<std::string, std::unique_ptr<web::FakeDownloadTask>>
      mock_download_tasks_;
};

}  // namespace

class DownloadListMediatorTest : public PlatformTest {
 protected:
  DownloadListMediatorTest() {}

  ~DownloadListMediatorTest() override { [mock_consumer_ stopMocking]; }

  void SetUp() override {
    PlatformTest::SetUp();

    // Set up test downloads directory.
    ASSERT_TRUE(test_downloads_dir_.CreateUniqueTempDir());
    downloads_path_ = test_downloads_dir_.GetPath().AppendASCII("downloads");
    ASSERT_TRUE(base::CreateDirectory(downloads_path_));
    test::SetDownloadsDirectoryForTesting(&downloads_path_);

    feature_list_.InitAndEnableFeature(kDownloadList);

    mock_service_ = std::make_unique<MockDownloadRecordService>();
    ASSERT_TRUE(mock_service_);
    mock_service_->CreateRecordsForTesting();

    InitializeMediatorAndLoadTestData();
  }

  void TearDown() override {
    // Reset downloads directory override.
    test::SetDownloadsDirectoryForTesting(nullptr);

    [mediator_ disconnect];
    mediator_ = nil;
    mock_service_.reset();
    PlatformTest::TearDown();
  }

  // Helper method to initialize mediator with specific incognito setting and
  // load test data.
  void InitializeMediatorAndLoadTestData() {
    // Create mediator.
    mediator_ = [[DownloadListMediator alloc]
        initWithDownloadRecordService:mock_service_.get()
                          isIncognito:NO];
    ASSERT_TRUE(mediator_);

    // Create mock consumer.
    mock_consumer_ = OCMProtocolMock(@protocol(DownloadListConsumer));

    base::RunLoop run_loop;
    auto quit_closure = run_loop.QuitClosure();

    [mediator_ setConsumer:mock_consumer_];

    // Set up expectations for the initial loadDownloadRecords call.
    OCMStub([mock_consumer_ setEmptyState:NO])
        .andDo(
            [quit_closure](NSInvocation* invocation) { quit_closure.Run(); });
    [[mock_consumer_ expect] setLoadingState:YES];
    [[mock_consumer_ expect] setDownloadListItems:[OCMArg any]];
    [[mock_consumer_ expect] setLoadingState:NO];

    // Load records.
    [mediator_ loadDownloadRecords];

    // Wait for async operations to complete.
    run_loop.Run();
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  DownloadListMediator* mediator_;
  id mock_consumer_;
  std::unique_ptr<MockDownloadRecordService> mock_service_;
  base::test::ScopedFeatureList feature_list_;
  base::ScopedTempDir test_downloads_dir_;
  base::FilePath downloads_path_;
};

// Test filtering records by different types with content validation.
TEST_F(DownloadListMediatorTest, TestFilterRecordsWithTypeAndValidateContent) {
  // Test PDF filter - expect 1 item (document.pdf).
  [[mock_consumer_ expect]
      setDownloadListItems:[OCMArg checkWithBlock:^BOOL(
                                       NSArray<DownloadListItem*>* items) {
        EXPECT_EQ(items.count, 1U);
        EXPECT_TRUE([items[0].downloadID isEqualToString:@"1"]);
        return YES;
      }]];

  [mediator_ filterRecordsWithType:DownloadFilterType::kPDF];
  [mock_consumer_ verify];

  // Test Image filter - expect 1 item (image.jpg).
  [[mock_consumer_ expect]
      setDownloadListItems:[OCMArg checkWithBlock:^BOOL(
                                       NSArray<DownloadListItem*>* items) {
        EXPECT_EQ(items.count, 1U);
        EXPECT_TRUE([items[0].downloadID isEqualToString:@"2"]);
        return YES;
      }]];

  [mediator_ filterRecordsWithType:DownloadFilterType::kImage];
  [mock_consumer_ verify];

  // Test Video filter - expect 1 item (video.mp4).
  [[mock_consumer_ expect]
      setDownloadListItems:[OCMArg checkWithBlock:^BOOL(
                                       NSArray<DownloadListItem*>* items) {
        EXPECT_EQ(items.count, 1U);
        EXPECT_TRUE([items[0].downloadID isEqualToString:@"3"]);
        return YES;
      }]];

  [mediator_ filterRecordsWithType:DownloadFilterType::kVideo];
  [mock_consumer_ verify];

  // Test Audio filter - expect 1 item (audio.mp3).
  [[mock_consumer_ expect]
      setDownloadListItems:[OCMArg checkWithBlock:^BOOL(
                                       NSArray<DownloadListItem*>* items) {
        EXPECT_EQ(items.count, 1U);
        EXPECT_TRUE([items[0].downloadID isEqualToString:@"4"]);
        return YES;
      }]];

  [mediator_ filterRecordsWithType:DownloadFilterType::kAudio];
  [mock_consumer_ verify];

  // Test Document filter - expect 1 item (document.txt).
  [[mock_consumer_ expect]
      setDownloadListItems:[OCMArg checkWithBlock:^BOOL(
                                       NSArray<DownloadListItem*>* items) {
        EXPECT_EQ(items.count, 1U);
        EXPECT_TRUE([items[0].downloadID isEqualToString:@"5"]);
        return YES;
      }]];

  [mediator_ filterRecordsWithType:DownloadFilterType::kDocument];
  [mock_consumer_ verify];

  // Test Other filter - expect 1 item (archive.zip).
  [[mock_consumer_ expect]
      setDownloadListItems:[OCMArg checkWithBlock:^BOOL(
                                       NSArray<DownloadListItem*>* items) {
        EXPECT_EQ(items.count, 1U);
        EXPECT_TRUE([items[0].downloadID isEqualToString:@"6"]);
        return YES;
      }]];

  [mediator_ filterRecordsWithType:DownloadFilterType::kOther];
  [mock_consumer_ verify];

  // Test All filter - expect all 6 non-incognito items (incognito items are
  // filtered out).
  [[mock_consumer_ expect]
      setDownloadListItems:[OCMArg checkWithBlock:^BOOL(
                                       NSArray<DownloadListItem*>* items) {
        EXPECT_EQ(items.count, 6U);
        // Verify all expected IDs are present.
        NSSet<NSString*>* expectedIDs =
            [NSSet setWithArray:@[ @"1", @"2", @"3", @"4", @"5", @"6" ]];
        NSMutableSet<NSString*>* actualIDs = [NSMutableSet set];
        for (DownloadListItem* item in items) {
          [actualIDs addObject:item.downloadID];
        }
        EXPECT_TRUE([expectedIDs isEqualToSet:actualIDs]);
        return YES;
      }]];

  [mediator_ filterRecordsWithType:DownloadFilterType::kAll];
  [mock_consumer_ verify];
}

// Test search filtering with keyword validation.
TEST_F(DownloadListMediatorTest, TestSearchRecordsWithKeywordValidation) {
  __block NSArray<DownloadListItem*>* capturedItems = nil;

  // Test search for "document" - should match both document.pdf and
  // document.txt.
  [[mock_consumer_ expect]
      setDownloadListItems:AssignValueToVariable(capturedItems)];

  [mediator_ filterRecordsWithKeyword:@"document"];
  [mock_consumer_ verify];

  EXPECT_EQ(capturedItems.count, 2U);
  BOOL foundPDF = NO, foundTXT = NO;
  for (DownloadListItem* item in capturedItems) {
    if ([item.downloadID isEqualToString:@"1"]) {
      foundPDF = YES;
    } else if ([item.downloadID isEqualToString:@"5"]) {
      foundTXT = YES;
    }
  }
  EXPECT_TRUE(foundPDF);
  EXPECT_TRUE(foundTXT);

  // Test search for "mp" - should match both video.mp4 and audio.mp3.
  [[mock_consumer_ expect]
      setDownloadListItems:AssignValueToVariable(capturedItems)];

  [mediator_ filterRecordsWithKeyword:@"mp"];
  [mock_consumer_ verify];

  EXPECT_EQ(capturedItems.count, 2U);
  BOOL foundMP4 = NO, foundMP3 = NO;
  for (DownloadListItem* item in capturedItems) {
    if ([item.downloadID isEqualToString:@"3"]) {
      foundMP4 = YES;
    } else if ([item.downloadID isEqualToString:@"4"]) {
      foundMP3 = YES;
    }
  }
  EXPECT_TRUE(foundMP4);
  EXPECT_TRUE(foundMP3);

  // Test search for "nonexistent" - should return empty.
  [[mock_consumer_ expect]
      setDownloadListItems:AssignValueToVariable(capturedItems)];

  [mediator_ filterRecordsWithKeyword:@"nonexistent"];
  [mock_consumer_ verify];

  EXPECT_EQ(capturedItems.count, 0U);

  // Test empty search - should return all items.
  [[mock_consumer_ expect]
      setDownloadListItems:AssignValueToVariable(capturedItems)];

  [mediator_ filterRecordsWithKeyword:@""];
  [mock_consumer_ verify];

  EXPECT_EQ(capturedItems.count, 6U);
}

// Test combined filtering and search functionality.
TEST_F(DownloadListMediatorTest, TestCombinedFilterAndSearch) {
  __block NSArray<DownloadListItem*>* capturedItems = nil;

  // First apply PDF filter.
  [[mock_consumer_ expect] setDownloadListItems:[OCMArg any]];

  [mediator_ filterRecordsWithType:DownloadFilterType::kPDF];
  [mock_consumer_ verify];

  // Then search for "document" within PDF filter - should still find
  // document.pdf.
  [[mock_consumer_ expect]
      setDownloadListItems:AssignValueToVariable(capturedItems)];

  [mediator_ filterRecordsWithKeyword:@"document"];
  [mock_consumer_ verify];

  EXPECT_EQ(capturedItems.count, 1U);
  EXPECT_TRUE([capturedItems[0].downloadID isEqualToString:@"1"]);

  // Search for "image" within PDF filter - should return empty.
  [[mock_consumer_ expect]
      setDownloadListItems:AssignValueToVariable(capturedItems)];

  [mediator_ filterRecordsWithKeyword:@"image"];
  [mock_consumer_ verify];

  EXPECT_EQ(capturedItems.count, 0U);
}

// Test adding a new download record through mock service.
TEST_F(DownloadListMediatorTest, TestDownloadRecordWasAdded) {
  // Connect mediator to enable observer notifications.
  [mediator_ connect];

  // Create a new download record and add it through the service.
  DownloadRecord newRecord;
  newRecord.download_id = "7";
  newRecord.original_url = "https://testsite.org/newfile.pdf";
  newRecord.mime_type = kAdobePortableDocumentFormatMimeType;
  newRecord.file_name = "newfile.pdf";
  newRecord.created_time = base::Time::Now();

  // Set up expectation for consumer update when record is added.
  [[mock_consumer_ expect]
      setDownloadListItems:[OCMArg checkWithBlock:^BOOL(
                                       NSArray<DownloadListItem*>* items) {
        // Should have more than original 6 records
        EXPECT_GT(items.count, 6U);
        return YES;
      }]];

  // Add record through mock service - this will trigger observer notification.
  mock_service_->AddRecordForTesting(newRecord);

  [mock_consumer_ verify];
}

// Test updating an existing download record through mock service.
TEST_F(DownloadListMediatorTest, TestDownloadRecordWasUpdated) {
  // Connect mediator to enable observer notifications.
  [mediator_ connect];

  // Create an updated record for testing.
  DownloadRecord updatedRecord;
  updatedRecord.download_id = "1";
  updatedRecord.original_url = "https://testsite.org/document.pdf";
  updatedRecord.mime_type = kAdobePortableDocumentFormatMimeType;
  updatedRecord.file_name = "updated_document.pdf";
  updatedRecord.created_time = base::Time::Now();

  // Set up expectation for consumer update when record is updated.
  [[mock_consumer_ expect]
      setDownloadListItems:[OCMArg checkWithBlock:^BOOL(
                                       NSArray<DownloadListItem*>* items) {
        // Should still have records after update
        EXPECT_GT(items.count, 0U);
        return YES;
      }]];

  // Update record through mock service which will trigger observer
  // notification.
  mock_service_->TriggerUpdateForTesting(updatedRecord);

  [mock_consumer_ verify];
}

// Test removing download records through mock service.
TEST_F(DownloadListMediatorTest, TestDownloadsWereRemovedWithIDs) {
  // Connect mediator to enable observer notifications.
  [mediator_ connect];

  // Set up expectation for consumer update when records are removed.
  // Note: Observer notifications do not show loading state
  // (loadDownloadRecordsWithLoading:NO)
  [[mock_consumer_ expect]
      setDownloadListItems:[OCMArg checkWithBlock:^BOOL(
                                       NSArray<DownloadListItem*>* items) {
        // Should have fewer records after removal
        EXPECT_LT(items.count, 6U);
        return YES;
      }]];

  // Remove record through mock service which will trigger observer
  // notification.
  mock_service_->RemoveRecordForTesting("1");

  [mock_consumer_ verify];
}

// Test incognito mediator filtering - non-incognito mediator should filter out
// incognito records.
TEST_F(DownloadListMediatorTest,
       TestNonIncognitoMediatorFiltersIncognitoRecords) {
  // The default mediator is non-incognito, so it should only show non-incognito
  // records. This means we should have 6 records (the original non-incognito
  // ones).
  __block NSArray<DownloadListItem*>* capturedItems = nil;

  [[mock_consumer_ expect]
      setDownloadListItems:AssignValueToVariable(capturedItems)];

  [mediator_ filterRecordsWithType:DownloadFilterType::kAll];
  [mock_consumer_ verify];

  EXPECT_EQ(capturedItems.count, 6U);

  // Verify that no incognito records are present (IDs 7 and 8 are incognito).
  for (DownloadListItem* item in capturedItems) {
    EXPECT_FALSE([item.downloadID isEqualToString:@"7"]);
    EXPECT_FALSE([item.downloadID isEqualToString:@"8"]);
  }
}

// Test incognito record observer filtering for non-incognito mediator.
TEST_F(DownloadListMediatorTest,
       TestNonIncognitoMediatorIgnoresIncognitoObserverUpdates) {
  // Connect mediator to enable observer notifications.
  [mediator_ connect];

  // Create an incognito record and try to add it.
  DownloadRecord incognitoRecord;
  incognitoRecord.download_id = "9";
  incognitoRecord.original_url = "https://testsite.org/incognito_new.pdf";
  incognitoRecord.mime_type = kAdobePortableDocumentFormatMimeType;
  incognitoRecord.file_name = "incognito_new.pdf";
  incognitoRecord.created_time = base::Time::Now();
  incognitoRecord.is_incognito = true;

  // The consumer should NOT be called because the non-incognito mediator
  // should ignore incognito record additions.
  [[mock_consumer_ reject] setDownloadListItems:[OCMArg any]];

  // Add incognito record through mock service.
  mock_service_->AddRecordForTesting(incognitoRecord);

  // Verify that consumer was not called.
  [mock_consumer_ verify];
}

// Test non-incognito record observer behavior for non-incognito mediator.
TEST_F(DownloadListMediatorTest,
       TestNonIncognitoMediatorAcceptsNonIncognitoObserverUpdates) {
  // Connect mediator to enable observer notifications.
  [mediator_ connect];

  // Create a non-incognito record and add it.
  DownloadRecord nonIncognitoRecord;
  nonIncognitoRecord.download_id = "10";
  nonIncognitoRecord.original_url = "https://testsite.org/regular_new.pdf";
  nonIncognitoRecord.mime_type = kAdobePortableDocumentFormatMimeType;
  nonIncognitoRecord.file_name = "regular_new.pdf";
  nonIncognitoRecord.created_time = base::Time::Now();
  nonIncognitoRecord.is_incognito = false;

  // The consumer SHOULD be called because the non-incognito mediator
  // should accept non-incognito record additions.
  [[mock_consumer_ expect]
      setDownloadListItems:[OCMArg checkWithBlock:^BOOL(
                                       NSArray<DownloadListItem*>* items) {
        // Should have more than original 6 records.
        EXPECT_GT(items.count, 6U);
        return YES;
      }]];

  // Add non-incognito record through mock service.
  mock_service_->AddRecordForTesting(nonIncognitoRecord);

  // Verify that consumer was called.
  [mock_consumer_ verify];
}

// Test application state handling when app becomes active.
TEST_F(DownloadListMediatorTest, TestApplicationDidBecomeActive) {
  // Connect the mediator to enable state handling.
  [mediator_ connect];

  // Simulate app going to background first.
  [[NSNotificationCenter defaultCenter]
      postNotificationName:UIApplicationWillResignActiveNotification
                    object:nil];

  // Set up expectation for sync operation when app becomes active.
  [[mock_consumer_ expect] setLoadingState:YES];
  [[mock_consumer_ expect]
      setDownloadListItems:[OCMArg checkWithBlock:^BOOL(
                                       NSArray<DownloadListItem*>* items) {
        // Should have the expected number of records after sync
        EXPECT_EQ(items.count, 6U);
        return YES;
      }]];
  [[mock_consumer_ expect] setLoadingState:NO];

  // Simulate app coming back to foreground.
  [[NSNotificationCenter defaultCenter]
      postNotificationName:UIApplicationDidBecomeActiveNotification
                    object:nil];

  [mock_consumer_ verify];
}

// Test deleteDownloadItem functionality.
TEST_F(DownloadListMediatorTest, TestDeleteDownloadItem) {
  const std::string downloadId = "1";
  const std::string fileName = "test.pdf";

  // Create test file in downloads directory.
  base::FilePath testFile = downloads_path_.Append(fileName);
  ASSERT_TRUE(base::WriteFile(testFile, "content"));

  // Create a download record for testing.
  DownloadRecord record;
  record.download_id = downloadId;
  record.file_path = base::FilePath(fileName);

  DownloadListItem* item =
      [[DownloadListItem alloc] initWithDownloadRecord:record];

  // Set up RunLoop to wait for async file deletion completion.
  base::RunLoop run_loop;
  auto quit_closure = run_loop.QuitClosure();

  // Mock the service call and trigger quit_closure when it's called.
  EXPECT_CALL(*mock_service_, RemoveDownloadByIdAsync(downloadId, _))
      .Times(1)
      .WillOnce([quit_closure](const std::string& id, auto callback) {
        if (callback) {
          std::move(callback).Run(true);
        }
        quit_closure.Run();
      });

  [mediator_ deleteDownloadItem:item];

  // Wait for the async operation to complete.
  run_loop.Run();

  // Verify mock state and file deletion.
  [mock_consumer_ verify];
  EXPECT_FALSE(base::PathExists(testFile));
}

// Test class specifically for incognito mediator testing - equivalent to
// DownloadListMediatorTest.
class DownloadListMediatorIncognitoTest : public PlatformTest {
 protected:
  DownloadListMediatorIncognitoTest() {}

  ~DownloadListMediatorIncognitoTest() override {
    [mock_consumer_ stopMocking];
  }

  void SetUp() override {
    PlatformTest::SetUp();

    feature_list_.InitAndEnableFeature(kDownloadList);

    mock_service_ = std::make_unique<MockDownloadRecordService>();
    ASSERT_TRUE(mock_service_);
    mock_service_->CreateRecordsForTesting();

    // Initialize with incognito mediator - this is the key difference from the
    // base test class
    InitializeMediatorAndLoadTestData();
  }

  void TearDown() override {
    [mediator_ disconnect];
    mediator_ = nil;
    mock_service_.reset();
    PlatformTest::TearDown();
  }

  // Helper method to initialize incognito mediator and load test data.
  void InitializeMediatorAndLoadTestData() {
    // Create incognito mediator (isIncognito:YES).
    mediator_ = [[DownloadListMediator alloc]
        initWithDownloadRecordService:mock_service_.get()
                          isIncognito:YES];
    ASSERT_TRUE(mediator_);

    // Create mock consumer.
    mock_consumer_ = OCMProtocolMock(@protocol(DownloadListConsumer));

    base::RunLoop run_loop;
    auto quit_closure = run_loop.QuitClosure();

    [mediator_ setConsumer:mock_consumer_];

    // Set up expectations for the initial loadDownloadRecords call.
    OCMStub([mock_consumer_ setEmptyState:NO])
        .andDo(
            [quit_closure](NSInvocation* invocation) { quit_closure.Run(); });
    [[mock_consumer_ expect] setLoadingState:YES];
    [[mock_consumer_ expect] setDownloadListItems:[OCMArg any]];
    [[mock_consumer_ expect] setLoadingState:NO];

    // Load records.
    [mediator_ loadDownloadRecords];

    // Wait for async operations to complete.
    run_loop.Run();
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  DownloadListMediator* mediator_;
  id mock_consumer_;
  std::unique_ptr<MockDownloadRecordService> mock_service_;
  base::test::ScopedFeatureList feature_list_;
};

// Test that incognito mediator shows all records (both incognito and
// non-incognito).
TEST_F(DownloadListMediatorIncognitoTest, TestShowsAllRecords) {
  __block NSArray<DownloadListItem*>* capturedItems = nil;

  [[mock_consumer_ expect]
      setDownloadListItems:AssignValueToVariable(capturedItems)];

  [mediator_ filterRecordsWithType:DownloadFilterType::kAll];
  [mock_consumer_ verify];

  // Incognito mediator should show all 8 records (6 regular + 2 incognito).
  EXPECT_EQ(capturedItems.count, 8U);

  // Verify that both regular and incognito records are present.
  NSSet<NSString*>* expectedIDs =
      [NSSet setWithArray:@[ @"1", @"2", @"3", @"4", @"5", @"6", @"7", @"8" ]];
  NSMutableSet<NSString*>* actualIDs = [NSMutableSet set];
  for (DownloadListItem* item in capturedItems) {
    [actualIDs addObject:item.downloadID];
  }
  EXPECT_TRUE([expectedIDs isEqualToSet:actualIDs]);
}

// Test incognito mediator filtering with PDF type.
TEST_F(DownloadListMediatorIncognitoTest, TestFilterRecordsWithPDFType) {
  // Test PDF filter - should include both regular and incognito PDF files.
  [[mock_consumer_ expect]
      setDownloadListItems:[OCMArg checkWithBlock:^BOOL(
                                       NSArray<DownloadListItem*>* items) {
        EXPECT_EQ(items.count,
                  2U);  // Regular PDF (ID=1) and incognito PDF (ID=7)
        BOOL foundRegularPDF = NO, foundIncognitoPDF = NO;
        for (DownloadListItem* item in items) {
          if ([item.downloadID isEqualToString:@"1"]) {
            foundRegularPDF = YES;
          } else if ([item.downloadID isEqualToString:@"7"]) {
            foundIncognitoPDF = YES;
          }
        }
        EXPECT_TRUE(foundRegularPDF);
        EXPECT_TRUE(foundIncognitoPDF);
        return YES;
      }]];

  [mediator_ filterRecordsWithType:DownloadFilterType::kPDF];
  [mock_consumer_ verify];
}

// Test incognito mediator search functionality.
TEST_F(DownloadListMediatorIncognitoTest, TestSearchRecordsWithKeyword) {
  __block NSArray<DownloadListItem*>* capturedItems = nil;

  // Search for "incognito" - should match incognito records.
  [[mock_consumer_ expect]
      setDownloadListItems:AssignValueToVariable(capturedItems)];

  [mediator_ filterRecordsWithKeyword:@"incognito"];
  [mock_consumer_ verify];

  // Should find both incognito records (IDs 7 and 8).
  EXPECT_EQ(capturedItems.count, 2U);
  BOOL foundIncognito7 = NO, foundIncognito8 = NO;
  for (DownloadListItem* item in capturedItems) {
    if ([item.downloadID isEqualToString:@"7"]) {
      foundIncognito7 = YES;
    } else if ([item.downloadID isEqualToString:@"8"]) {
      foundIncognito8 = YES;
    }
  }
  EXPECT_TRUE(foundIncognito7);
  EXPECT_TRUE(foundIncognito8);
}

// Test that incognito mediator handles incognito record additions correctly.
TEST_F(DownloadListMediatorIncognitoTest, TestHandlesIncognitoRecordAddition) {
  // Connect mediator to enable observer notifications.
  [mediator_ connect];

  // Create an incognito record and add it.
  DownloadRecord incognitoRecord;
  incognitoRecord.download_id = "9";
  incognitoRecord.original_url = "https://testsite.org/incognito_new.pdf";
  incognitoRecord.mime_type = kAdobePortableDocumentFormatMimeType;
  incognitoRecord.file_name = "incognito_new.pdf";
  incognitoRecord.created_time = base::Time::Now();
  incognitoRecord.is_incognito = true;

  // The consumer SHOULD be called because the incognito mediator
  // should accept incognito record additions.
  [[mock_consumer_ expect]
      setDownloadListItems:[OCMArg checkWithBlock:^BOOL(
                                       NSArray<DownloadListItem*>* items) {
        // Should have more than original 8 records.
        EXPECT_GT(items.count, 8U);
        return YES;
      }]];

  // Add incognito record through mock service.
  mock_service_->AddRecordForTesting(incognitoRecord);

  [mock_consumer_ verify];
}

// Test cancelDownloadItem method with actual task cancellation.
TEST_F(DownloadListMediatorTest, TestCancelDownloadItemInProgress) {
  // Create a fake download task.
  auto fake_task = std::make_unique<web::FakeDownloadTask>(
      GURL("https://testsite.org/document.pdf"),
      kAdobePortableDocumentFormatMimeType);
  web::FakeDownloadTask* task_ptr = fake_task.get();

  // Set task state to in progress to simulate an active download.
  task_ptr->SetState(web::DownloadTask::State::kInProgress);

  // Add the mock task to the service.
  mock_service_->AddMockDownloadTaskForTesting("test_download_1",
                                               std::move(fake_task));

  // Create a download record with kInProgress state.
  DownloadRecord inProgressRecord;
  inProgressRecord.download_id = "test_download_1";
  inProgressRecord.original_url = "https://testsite.org/document.pdf";
  inProgressRecord.mime_type = kAdobePortableDocumentFormatMimeType;
  inProgressRecord.file_name = "document.pdf";
  inProgressRecord.created_time = base::Time::Now();
  inProgressRecord.state = web::DownloadTask::State::kInProgress;
  inProgressRecord.received_bytes = 50;
  inProgressRecord.total_bytes = 100;
  inProgressRecord.progress_percent = 50;

  // Create download list item.
  DownloadListItem* item =
      [[DownloadListItem alloc] initWithDownloadRecord:inProgressRecord];

  // Verify initial state is kInProgress.
  EXPECT_EQ(task_ptr->GetState(), web::DownloadTask::State::kInProgress);

  // Call cancelDownloadItem - this should call GetDownloadTaskById and then
  // Cancel().
  [mediator_ cancelDownloadItem:item];

  // Verify that Cancel() was called on the task by checking if state changed.
  // Note: FakeDownloadTask's Cancel() method should set state to kCancelled.
  EXPECT_EQ(task_ptr->GetState(), web::DownloadTask::State::kCancelled);

  // Verify all mock expectations have been met.
  [mock_consumer_ verify];
}

// Test that incognito mediator handles non-incognito record additions
// correctly.
TEST_F(DownloadListMediatorIncognitoTest,
       TestHandlesNonIncognitoRecordAddition) {
  // Connect mediator to enable observer notifications.
  [mediator_ connect];

  // Create a non-incognito record and add it.
  DownloadRecord nonIncognitoRecord;
  nonIncognitoRecord.download_id = "10";
  nonIncognitoRecord.original_url = "https://testsite.org/regular_new.pdf";
  nonIncognitoRecord.mime_type = kAdobePortableDocumentFormatMimeType;
  nonIncognitoRecord.file_name = "regular_new.pdf";
  nonIncognitoRecord.created_time = base::Time::Now();
  nonIncognitoRecord.is_incognito = false;

  // The consumer SHOULD be called because the incognito mediator
  // should accept all record additions.
  [[mock_consumer_ expect]
      setDownloadListItems:[OCMArg checkWithBlock:^BOOL(
                                       NSArray<DownloadListItem*>* items) {
        // Should have more than original 8 records.
        EXPECT_GT(items.count, 8U);
        return YES;
      }]];

  // Add non-incognito record through mock service.
  mock_service_->AddRecordForTesting(nonIncognitoRecord);

  [mock_consumer_ verify];
}
