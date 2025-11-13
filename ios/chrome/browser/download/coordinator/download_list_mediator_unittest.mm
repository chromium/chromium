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

  // Imports callback types from the base class.
  using DownloadRecordsCallback =
      DownloadRecordService::DownloadRecordsCallback;
  using DownloadRecordCallback = DownloadRecordService::DownloadRecordCallback;
  using CompletionCallback = DownloadRecordService::CompletionCallback;

  // Sets up the test downloads directory and creates test files.
  void SetupTestEnvironment() {
    CHECK(temp_dir_.CreateUniqueTempDir());
    downloads_path_ = temp_dir_.GetPath().AppendASCII("downloads");
    CHECK(base::CreateDirectory(downloads_path_));
    test::SetDownloadsDirectoryForTesting(&downloads_path_);
    CreateTestFiles();
  }

  // Cleans up the test environment.
  void CleanupTestEnvironment() {
    test::SetDownloadsDirectoryForTesting(nullptr);
  }

  // Creates actual test files in the downloads directory.
  void CreateTestFiles() {
    base::FilePath downloads_dir = downloads_path_;

    base::WriteFile(downloads_dir.Append("document.pdf"), "PDF content");
    base::WriteFile(downloads_dir.Append("image.jpg"), "JPEG content");
    base::WriteFile(downloads_dir.Append("video.mp4"), "MP4 content");
    base::WriteFile(downloads_dir.Append("audio.mp3"), "MP3 content");
    base::WriteFile(downloads_dir.Append("document.txt"), "TXT content");
    base::WriteFile(downloads_dir.Append("archive.zip"), "ZIP content");
    base::WriteFile(downloads_dir.Append("incognito_document.pdf"),
                    "Incognito PDF content");
    base::WriteFile(downloads_dir.Append("incognito_image.jpg"),
                    "Incognito JPEG content");
  }

  // Creates records directly for testing purposes.
  void CreateRecordsForTesting() {
    DownloadRecord pdf_record;
    pdf_record.download_id = "1";
    pdf_record.original_url = "https://testsite.org/document.pdf";
    pdf_record.mime_type = kAdobePortableDocumentFormatMimeType;
    pdf_record.file_name = "document.pdf";
    pdf_record.file_path = base::FilePath("document.pdf");
    pdf_record.created_time = base::Time::Now();

    DownloadRecord image_record;
    image_record.download_id = "2";
    image_record.original_url = "https://testsite.org/image.jpg";
    image_record.mime_type = kJPEGImageMimeType;
    image_record.file_name = "image.jpg";
    image_record.file_path = base::FilePath("image.jpg");
    image_record.created_time = base::Time::Now();

    DownloadRecord video_record;
    video_record.download_id = "3";
    video_record.original_url = "https://testsite.org/video.mp4";
    video_record.mime_type = kMP4VideoMimeType;
    video_record.file_name = "video.mp4";
    video_record.file_path = base::FilePath("video.mp4");
    video_record.created_time = base::Time::Now();

    DownloadRecord audio_record;
    audio_record.download_id = "4";
    audio_record.original_url = "https://testsite.org/audio.mp3";
    audio_record.mime_type = kMP3AudioMimeType;
    audio_record.file_name = "audio.mp3";
    audio_record.file_path = base::FilePath("audio.mp3");
    audio_record.created_time = base::Time::Now();

    DownloadRecord text_record;
    text_record.download_id = "5";
    text_record.original_url = "https://testsite.org/document.txt";
    text_record.mime_type = kTextMimeType;
    text_record.file_name = "document.txt";
    text_record.file_path = base::FilePath("document.txt");
    text_record.created_time = base::Time::Now();

    DownloadRecord zip_record;
    zip_record.download_id = "6";
    zip_record.original_url = "https://testsite.org/archive.zip";
    zip_record.mime_type = kZipArchiveMimeType;
    zip_record.file_name = "archive.zip";
    zip_record.file_path = base::FilePath("archive.zip");
    zip_record.created_time = base::Time::Now();

    // Add incognito records for testing incognito functionality.
    DownloadRecord incognito_pdf_record;
    incognito_pdf_record.download_id = "7";
    incognito_pdf_record.original_url =
        "https://testsite.org/incognito_document.pdf";
    incognito_pdf_record.mime_type = kAdobePortableDocumentFormatMimeType;
    incognito_pdf_record.file_name = "incognito_document.pdf";
    incognito_pdf_record.file_path = base::FilePath("incognito_document.pdf");
    incognito_pdf_record.created_time = base::Time::Now();
    incognito_pdf_record.is_incognito = true;

    DownloadRecord incognito_image_record;
    incognito_image_record.download_id = "8";
    incognito_image_record.original_url =
        "https://testsite.org/incognito_image.jpg";
    incognito_image_record.mime_type = kJPEGImageMimeType;
    incognito_image_record.file_name = "incognito_image.jpg";
    incognito_image_record.file_path = base::FilePath("incognito_image.jpg");
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

  // Overrides virtual methods directly.
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

  // Returns the temp directory path for creating test files.
  base::FilePath GetTempDirPath() const { return downloads_path_; }

  // Provides public methods for testing access to internal state.
  void AddRecordForTesting(const DownloadRecord& record) {
    stored_records_.push_back(record);
    for (auto* observer : observers_) {
      observer->OnDownloadAdded(record);
    }
  }

  void TriggerUpdateForTesting(const DownloadRecord& record) {
    auto it =
        std::find_if(stored_records_.begin(), stored_records_.end(),
                     [&record](const DownloadRecord& stored_record) {
                       return stored_record.download_id == record.download_id;
                     });

    if (it != stored_records_.end()) {
      *it = record;
    }

    for (auto* observer : observers_) {
      observer->OnDownloadUpdated(record);
    }
  }

  void RemoveRecordForTesting(const std::string& download_id) {
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
  base::ScopedTempDir temp_dir_;
  base::FilePath downloads_path_;
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

    mock_service_->SetupTestEnvironment();
    mock_service_->CreateRecordsForTesting();

    InitializeMediatorAndLoadTestData();
  }

  void TearDown() override {
    // Reset downloads directory override.
    test::SetDownloadsDirectoryForTesting(nullptr);

    [mediator_ disconnect];
    mediator_ = nil;

    if (mock_service_) {
      mock_service_->CleanupTestEnvironment();
    }
    mock_service_.reset();
    PlatformTest::TearDown();
  }

  // Initializes the mediator with specific incognito setting and loads test
  // data.
  void InitializeMediatorAndLoadTestData() {
    mediator_ = [[DownloadListMediator alloc]
        initWithDownloadRecordService:mock_service_.get()
                          isIncognito:NO];
    ASSERT_TRUE(mediator_);

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

    [mediator_ loadDownloadRecords];
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

// Tests filtering records by different types with content validation.
TEST_F(DownloadListMediatorTest, TestFilterRecordsWithTypeAndValidateContent) {
  [[mock_consumer_ expect]
      setDownloadListItems:[OCMArg checkWithBlock:^BOOL(
                                       NSArray<DownloadListItem*>* items) {
        EXPECT_EQ(items.count, 1U);
        EXPECT_TRUE([items[0].downloadID isEqualToString:@"1"]);
        return YES;
      }]];

  [mediator_ filterRecordsWithType:DownloadFilterType::kPDF];
  [mock_consumer_ verify];

  [[mock_consumer_ expect]
      setDownloadListItems:[OCMArg checkWithBlock:^BOOL(
                                       NSArray<DownloadListItem*>* items) {
        EXPECT_EQ(items.count, 1U);
        EXPECT_TRUE([items[0].downloadID isEqualToString:@"2"]);
        return YES;
      }]];

  [mediator_ filterRecordsWithType:DownloadFilterType::kImage];
  [mock_consumer_ verify];

  [[mock_consumer_ expect]
      setDownloadListItems:[OCMArg checkWithBlock:^BOOL(
                                       NSArray<DownloadListItem*>* items) {
        EXPECT_EQ(items.count, 1U);
        EXPECT_TRUE([items[0].downloadID isEqualToString:@"3"]);
        return YES;
      }]];

  [mediator_ filterRecordsWithType:DownloadFilterType::kVideo];
  [mock_consumer_ verify];

  [[mock_consumer_ expect]
      setDownloadListItems:[OCMArg checkWithBlock:^BOOL(
                                       NSArray<DownloadListItem*>* items) {
        EXPECT_EQ(items.count, 1U);
        EXPECT_TRUE([items[0].downloadID isEqualToString:@"4"]);
        return YES;
      }]];

  [mediator_ filterRecordsWithType:DownloadFilterType::kAudio];
  [mock_consumer_ verify];

  [[mock_consumer_ expect]
      setDownloadListItems:[OCMArg checkWithBlock:^BOOL(
                                       NSArray<DownloadListItem*>* items) {
        EXPECT_EQ(items.count, 1U);
        EXPECT_TRUE([items[0].downloadID isEqualToString:@"5"]);
        return YES;
      }]];

  [mediator_ filterRecordsWithType:DownloadFilterType::kDocument];
  [mock_consumer_ verify];

  [[mock_consumer_ expect]
      setDownloadListItems:[OCMArg checkWithBlock:^BOOL(
                                       NSArray<DownloadListItem*>* items) {
        EXPECT_EQ(items.count, 1U);
        EXPECT_TRUE([items[0].downloadID isEqualToString:@"6"]);
        return YES;
      }]];

  [mediator_ filterRecordsWithType:DownloadFilterType::kOther];
  [mock_consumer_ verify];

  // Tests All filter - expect all 6 non-incognito items (incognito items are
  // filtered out).
  [[mock_consumer_ expect]
      setDownloadListItems:[OCMArg checkWithBlock:^BOOL(
                                       NSArray<DownloadListItem*>* items) {
        EXPECT_EQ(items.count, 6U);
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

// Tests search filtering with keyword validation.
TEST_F(DownloadListMediatorTest, TestSearchRecordsWithKeywordValidation) {
  __block NSArray<DownloadListItem*>* capturedItems = nil;

  [[mock_consumer_ expect]
      setDownloadListItems:[OCMArg checkWithBlock:^BOOL(
                                       NSArray<DownloadListItem*>* items) {
        capturedItems = items;
        return YES;
      }]];

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

  [[mock_consumer_ expect]
      setDownloadListItems:[OCMArg checkWithBlock:^BOOL(
                                       NSArray<DownloadListItem*>* items) {
        capturedItems = items;
        return YES;
      }]];

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

  [[mock_consumer_ expect]
      setDownloadListItems:[OCMArg checkWithBlock:^BOOL(
                                       NSArray<DownloadListItem*>* items) {
        capturedItems = items;
        return YES;
      }]];

  [mediator_ filterRecordsWithKeyword:@"nonexistent"];
  [mock_consumer_ verify];

  EXPECT_EQ(capturedItems.count, 0U);

  [[mock_consumer_ expect]
      setDownloadListItems:[OCMArg checkWithBlock:^BOOL(
                                       NSArray<DownloadListItem*>* items) {
        capturedItems = items;
        return YES;
      }]];

  [mediator_ filterRecordsWithKeyword:@""];
  [mock_consumer_ verify];

  EXPECT_EQ(capturedItems.count, 6U);
}

// Tests combined filtering and search functionality.
TEST_F(DownloadListMediatorTest, TestCombinedFilterAndSearch) {
  __block NSArray<DownloadListItem*>* capturedItems = nil;

  [[mock_consumer_ expect] setDownloadListItems:[OCMArg any]];

  [mediator_ filterRecordsWithType:DownloadFilterType::kPDF];
  [mock_consumer_ verify];

  [[mock_consumer_ expect]
      setDownloadListItems:[OCMArg checkWithBlock:^BOOL(
                                       NSArray<DownloadListItem*>* items) {
        capturedItems = items;
        return YES;
      }]];

  [mediator_ filterRecordsWithKeyword:@"document"];
  [mock_consumer_ verify];

  EXPECT_EQ(capturedItems.count, 1U);
  EXPECT_TRUE([capturedItems[0].downloadID isEqualToString:@"1"]);

  [[mock_consumer_ expect]
      setDownloadListItems:[OCMArg checkWithBlock:^BOOL(
                                       NSArray<DownloadListItem*>* items) {
        capturedItems = items;
        return YES;
      }]];

  [mediator_ filterRecordsWithKeyword:@"image"];
  [mock_consumer_ verify];

  EXPECT_EQ(capturedItems.count, 0U);
}

// Tests adding a new download record through mock service.
TEST_F(DownloadListMediatorTest, TestDownloadRecordWasAdded) {
  [mediator_ connect];

  DownloadRecord newRecord;
  newRecord.download_id = "7";
  newRecord.original_url = "https://testsite.org/newfile.pdf";
  newRecord.mime_type = kAdobePortableDocumentFormatMimeType;
  newRecord.file_name = "newfile.pdf";
  newRecord.created_time = base::Time::Now();

  base::FilePath downloads_dir = downloads_path_;
  base::FilePath file_path = downloads_dir.AppendASCII("newfile.pdf");
  base::WriteFile(file_path, "Test content for newfile.pdf");

  [[mock_consumer_ expect]
      setDownloadListItems:[OCMArg checkWithBlock:^BOOL(
                                       NSArray<DownloadListItem*>* items) {
        EXPECT_GT(items.count, 6U);
        return YES;
      }]];

  mock_service_->AddRecordForTesting(newRecord);

  [mock_consumer_ verify];
}

// Tests updating an existing download record through mock service.
TEST_F(DownloadListMediatorTest, TestDownloadRecordWasUpdated) {
  [mediator_ connect];

  DownloadRecord updatedRecord;
  updatedRecord.download_id = "1";
  updatedRecord.original_url = "https://testsite.org/document.pdf";
  updatedRecord.mime_type = kAdobePortableDocumentFormatMimeType;
  updatedRecord.file_name = "updated_document.pdf";
  updatedRecord.created_time = base::Time::Now();
  updatedRecord.file_path = base::FilePath("updated_document.pdf");

  base::FilePath file_path =
      downloads_path_.AppendASCII("updated_document.pdf");
  base::WriteFile(file_path, "Test content for updated_document.pdf");

  [[mock_consumer_ expect]
      setDownloadListItems:[OCMArg checkWithBlock:^BOOL(
                                       NSArray<DownloadListItem*>* items) {
        EXPECT_GT(items.count, 0U);
        return YES;
      }]];

  mock_service_->TriggerUpdateForTesting(updatedRecord);

  [mock_consumer_ verify];
}

// Tests removing download records through mock service.
TEST_F(DownloadListMediatorTest, TestDownloadsWereRemovedWithIDs) {
  [mediator_ connect];

  // Set up expectation for consumer update when records are removed.
  // Note: Observer notifications do not show loading state
  // (loadDownloadRecordsWithLoading:NO).
  [[mock_consumer_ expect]
      setDownloadListItems:[OCMArg checkWithBlock:^BOOL(
                                       NSArray<DownloadListItem*>* items) {
        EXPECT_LT(items.count, 6U);
        return YES;
      }]];

  mock_service_->RemoveRecordForTesting("1");

  [mock_consumer_ verify];
}

// Tests incognito mediator filtering - non-incognito mediator should filter out
// incognito records.
TEST_F(DownloadListMediatorTest,
       TestNonIncognitoMediatorFiltersIncognitoRecords) {
  __block NSArray<DownloadListItem*>* capturedItems = nil;

  [[mock_consumer_ expect]
      setDownloadListItems:[OCMArg checkWithBlock:^BOOL(
                                       decltype(capturedItems) param) {
        capturedItems = param;
        return YES;
      }]];

  [mediator_ filterRecordsWithType:DownloadFilterType::kAll];
  [mock_consumer_ verify];

  EXPECT_EQ(capturedItems.count, 6U);

  for (DownloadListItem* item in capturedItems) {
    EXPECT_FALSE([item.downloadID isEqualToString:@"7"]);
    EXPECT_FALSE([item.downloadID isEqualToString:@"8"]);
  }
}

// Tests incognito record observer filtering for non-incognito mediator.
TEST_F(DownloadListMediatorTest,
       TestNonIncognitoMediatorIgnoresIncognitoObserverUpdates) {
  [mediator_ connect];

  DownloadRecord incognitoRecord;
  incognitoRecord.download_id = "9";
  incognitoRecord.original_url = "https://testsite.org/incognito_new.pdf";
  incognitoRecord.mime_type = kAdobePortableDocumentFormatMimeType;
  incognitoRecord.file_name = "incognito_new.pdf";
  incognitoRecord.created_time = base::Time::Now();
  incognitoRecord.is_incognito = true;
  incognitoRecord.file_path = base::FilePath("incognito_new.pdf");

  base::FilePath file_path = downloads_path_.AppendASCII("incognito_new.pdf");
  base::WriteFile(file_path, "Test content for incognito_new.pdf");

  // The consumer should NOT be called because the non-incognito mediator
  // should ignore incognito record additions.
  [[mock_consumer_ reject] setDownloadListItems:[OCMArg any]];

  mock_service_->AddRecordForTesting(incognitoRecord);
  [mock_consumer_ verify];
}

// Tests non-incognito record observer behavior for non-incognito mediator.
TEST_F(DownloadListMediatorTest,
       TestNonIncognitoMediatorAcceptsNonIncognitoObserverUpdates) {
  [mediator_ connect];

  DownloadRecord nonIncognitoRecord;
  nonIncognitoRecord.download_id = "10";
  nonIncognitoRecord.original_url = "https://testsite.org/regular_new.pdf";
  nonIncognitoRecord.mime_type = kAdobePortableDocumentFormatMimeType;
  nonIncognitoRecord.file_name = "regular_new.pdf";
  nonIncognitoRecord.created_time = base::Time::Now();
  nonIncognitoRecord.is_incognito = false;
  nonIncognitoRecord.file_path = base::FilePath("regular_new.pdf");

  base::FilePath file_path = downloads_path_.AppendASCII("regular_new.pdf");
  base::WriteFile(file_path, "Test content for regular_new.pdf");

  // The consumer SHOULD be called because the non-incognito mediator
  // should accept non-incognito record additions.
  [[mock_consumer_ expect]
      setDownloadListItems:[OCMArg checkWithBlock:^BOOL(
                                       NSArray<DownloadListItem*>* items) {
        EXPECT_GT(items.count, 6U);
        return YES;
      }]];

  mock_service_->AddRecordForTesting(nonIncognitoRecord);
  [mock_consumer_ verify];
}

// Tests application state handling when app becomes active.
TEST_F(DownloadListMediatorTest, TestApplicationDidBecomeActive) {
  [mediator_ connect];

  [[NSNotificationCenter defaultCenter]
      postNotificationName:UIApplicationWillResignActiveNotification
                    object:nil];

  // Set up a run loop to wait for the asynchronous file existence check.
  base::RunLoop run_loop;
  auto quit_closure = run_loop.QuitClosure();

  [[mock_consumer_ expect]
      setDownloadListItems:[OCMArg checkWithBlock:^BOOL(
                                       NSArray<DownloadListItem*>* items) {
        EXPECT_EQ(items.count, 6U);
        quit_closure.Run();
        return YES;
      }]];

  [[NSNotificationCenter defaultCenter]
      postNotificationName:UIApplicationDidBecomeActiveNotification
                    object:nil];

  // Wait for the asynchronous file existence check to complete.
  run_loop.Run();

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

  [mock_consumer_ verify];
}

// Regression test: deleteDownloadItem should work even when the file doesn't
// exist on disk. This is important for canceled or failed downloads that may
// not have created files.
TEST_F(DownloadListMediatorTest, TestDeleteDownloadItemFileNotExists) {
  const std::string downloadId = "missing_file_download";
  const std::string fileName = "missing_file.pdf";

  // Create a download record but intentionally don't create the actual file.
  DownloadRecord record;
  record.download_id = downloadId;
  record.file_path = base::FilePath(fileName);
  record.state = web::DownloadTask::State::kCancelled;

  DownloadListItem* item =
      [[DownloadListItem alloc] initWithDownloadRecord:record];

  // Verify the file doesn't exist before deletion.
  base::FilePath fullPath = downloads_path_.Append(fileName);
  EXPECT_FALSE(base::PathExists(fullPath));

  // Set up RunLoop to wait for async file deletion completion.
  base::RunLoop run_loop;
  auto quit_closure = run_loop.QuitClosure();

  // The service should still remove the record even if file deletion fails.
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

  [mock_consumer_ verify];
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
    mock_service_->SetupTestEnvironment();
    downloads_path_ = mock_service_->GetTempDirPath();
    mock_service_->CreateRecordsForTesting();

    // Initialize with incognito mediator - this is the key difference from the
    // base test class.
    InitializeMediatorAndLoadTestData();
  }

  void TearDown() override {
    [mediator_ disconnect];
    mediator_ = nil;

    if (mock_service_) {
      mock_service_->CleanupTestEnvironment();
    }
    mock_service_.reset();
    PlatformTest::TearDown();
  }

  // Initializes the incognito mediator and loads test data.
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
  base::FilePath downloads_path_;
};

// Tests that incognito mediator shows all records (both incognito and
// non-incognito).
TEST_F(DownloadListMediatorIncognitoTest, TestShowsAllRecords) {
  __block NSArray<DownloadListItem*>* capturedItems = nil;

  [[mock_consumer_ expect]
      setDownloadListItems:[OCMArg checkWithBlock:^BOOL(
                                       NSArray<DownloadListItem*>* items) {
        capturedItems = items;
        return YES;
      }]];

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

// Tests incognito mediator filtering with PDF type.
TEST_F(DownloadListMediatorIncognitoTest, TestFilterRecordsWithPDFType) {
  // Tests PDF filter - should include both regular and incognito PDF files.
  [[mock_consumer_ expect]
      setDownloadListItems:[OCMArg checkWithBlock:^BOOL(
                                       NSArray<DownloadListItem*>* items) {
        EXPECT_EQ(items.count,
                  2U);  // Regular PDF (ID=1) and incognito PDF (ID=7).
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

// Tests incognito mediator search functionality.
TEST_F(DownloadListMediatorIncognitoTest, TestSearchRecordsWithKeyword) {
  __block NSArray<DownloadListItem*>* capturedItems = nil;

  // Search for "incognito" - should match incognito records.
  [[mock_consumer_ expect]
      setDownloadListItems:[OCMArg checkWithBlock:^BOOL(
                                       decltype(capturedItems) param) {
        capturedItems = param;
        return YES;
      }]];

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

// Tests that incognito mediator handles incognito record additions correctly.
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
  incognitoRecord.file_path = base::FilePath("incognito_new.pdf");

  base::FilePath file_path = downloads_path_.AppendASCII("incognito_new.pdf");
  base::WriteFile(file_path, "Test content for incognito_new.pdf");

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

// Tests that incognito mediator handles non-incognito record additions
// Tests cancelDownloadItem method with actual task cancellation.
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
  nonIncognitoRecord.file_path = base::FilePath("regular_new.pdf");

  base::FilePath file_path = downloads_path_.AppendASCII("regular_new.pdf");
  base::WriteFile(file_path, "Test content for regular_new.pdf");

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
