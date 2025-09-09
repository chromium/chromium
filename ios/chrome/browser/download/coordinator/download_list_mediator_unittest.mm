// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/coordinator/download_list_mediator.h"

#import <memory>
#import <string>
#import <vector>

#import "base/run_loop.h"
#import "base/test/scoped_feature_list.h"
#import "ios/chrome/browser/download/model/download_filter_util.h"
#import "ios/chrome/browser/download/model/download_record.h"
#import "ios/chrome/browser/download/model/download_record_observer.h"
#import "ios/chrome/browser/download/model/download_record_service.h"
#import "ios/chrome/browser/download/ui/download_list/download_list_consumer.h"
#import "ios/chrome/browser/download/ui/download_list/download_list_item.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

namespace {

// Mock implementation of DownloadRecordService for testing.
class MockDownloadRecordService : public DownloadRecordService {
 public:
  MockDownloadRecordService() = default;
  ~MockDownloadRecordService() override = default;

  // Creates records directly for testing purposes.
  void CreateRecordsForTesting() {
    // Create DownloadRecord objects directly
    DownloadRecord pdf_record;
    pdf_record.download_id = "1";
    pdf_record.original_url = "https://testsite.org/document.pdf";
    pdf_record.mime_type = "application/pdf";
    pdf_record.file_name = "document.pdf";
    pdf_record.created_time = base::Time::Now();

    DownloadRecord image_record;
    image_record.download_id = "2";
    image_record.original_url = "https://testsite.org/image.jpg";
    image_record.mime_type = "image/jpeg";
    image_record.file_name = "image.jpg";
    image_record.created_time = base::Time::Now();

    DownloadRecord video_record;
    video_record.download_id = "3";
    video_record.original_url = "https://testsite.org/video.mp4";
    video_record.mime_type = "video/mp4";
    video_record.file_name = "video.mp4";
    video_record.created_time = base::Time::Now();

    DownloadRecord audio_record;
    audio_record.download_id = "4";
    audio_record.original_url = "https://testsite.org/audio.mp3";
    audio_record.mime_type = "audio/mpeg";
    audio_record.file_name = "audio.mp3";
    audio_record.created_time = base::Time::Now();

    DownloadRecord text_record;
    text_record.download_id = "5";
    text_record.original_url = "https://testsite.org/document.txt";
    text_record.mime_type = "text/plain";
    text_record.file_name = "document.txt";
    text_record.created_time = base::Time::Now();

    DownloadRecord zip_record;
    zip_record.download_id = "6";
    zip_record.original_url = "https://testsite.org/archive.zip";
    zip_record.mime_type = "application/zip";
    zip_record.file_name = "archive.zip";
    zip_record.created_time = base::Time::Now();

    stored_records_.push_back(pdf_record);
    stored_records_.push_back(image_record);
    stored_records_.push_back(video_record);
    stored_records_.push_back(audio_record);
    stored_records_.push_back(text_record);
    stored_records_.push_back(zip_record);
  }

  // Override virtual methods directly.
  void RecordDownload(web::DownloadTask* task) override {}

  void GetAllDownloadsAsync(DownloadRecordsCallback callback) override {
    std::move(callback).Run(stored_records_);
  }

  void GetDownloadByIdAsync(const std::string& download_id,
                            DownloadRecordCallback callback) override {}

  void RemoveDownloadByIdAsync(const std::string& download_id,
                               CompletionCallback callback) override {}

  void UpdateDownloadFilePathAsync(const std::string& download_id,
                                   const base::FilePath& file_path,
                                   CompletionCallback callback) override {}

  web::DownloadTask* GetDownloadTaskById(
      std::string_view download_id) const override {
    // For testing purposes, return nullptr as we're not tracking actual tasks.
    return nullptr;
  }

  void AddObserver(DownloadRecordObserver* observer) override {
    observers_.insert(observer);
  }

  void RemoveObserver(DownloadRecordObserver* observer) override {
    observers_.erase(observer);
  }

 private:
  std::vector<DownloadRecord> stored_records_;
  std::set<DownloadRecordObserver*> observers_;
};

}  // namespace

class DownloadListMediatorTest : public PlatformTest {
 protected:
  DownloadListMediatorTest() {}

  ~DownloadListMediatorTest() override { [mock_consumer_ stopMocking]; }

  void SetUp() override {
    PlatformTest::SetUp();

    feature_list_.InitAndEnableFeature(kDownloadList);

    mock_service_ = std::make_unique<MockDownloadRecordService>();
    ASSERT_TRUE(mock_service_);
    mock_service_->CreateRecordsForTesting();

    InitializeMediatorAndLoadTestData();
  }

  void TearDown() override {
    [mediator_ disconnect];
    mediator_ = nil;
    mock_service_.reset();
    PlatformTest::TearDown();
  }

  // Helper method to initialize mediator and load test data.
  void InitializeMediatorAndLoadTestData() {
    // Create mediator.
    mediator_ = [[DownloadListMediator alloc]
        initWithDownloadRecordService:mock_service_.get()];
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
  [[mock_consumer_ expect] setLoadingState:NO];

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
  [[mock_consumer_ expect] setLoadingState:NO];

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
  [[mock_consumer_ expect] setLoadingState:NO];

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
  [[mock_consumer_ expect] setLoadingState:NO];

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
  [[mock_consumer_ expect] setLoadingState:NO];

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
  [[mock_consumer_ expect] setLoadingState:NO];

  [mediator_ filterRecordsWithType:DownloadFilterType::kOther];
  [mock_consumer_ verify];

  // Test All filter - expect all 6 items.
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
  [[mock_consumer_ expect] setLoadingState:NO];

  [mediator_ filterRecordsWithType:DownloadFilterType::kAll];
  [mock_consumer_ verify];
}

// Test search filtering with keyword validation.
TEST_F(DownloadListMediatorTest, TestSearchRecordsWithKeywordValidation) {
  __block NSArray<DownloadListItem*>* capturedItems = nil;

  // Test search for "document" - should match both document.pdf and
  // document.txt.
  [[mock_consumer_ expect]
      setDownloadListItems:[OCMArg checkWithBlock:^BOOL(
                                       NSArray<DownloadListItem*>* items) {
        capturedItems = items;
        return YES;
      }]];
  [[mock_consumer_ expect] setLoadingState:NO];

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
      setDownloadListItems:[OCMArg checkWithBlock:^BOOL(
                                       NSArray<DownloadListItem*>* items) {
        capturedItems = items;
        return YES;
      }]];
  [[mock_consumer_ expect] setLoadingState:NO];

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
      setDownloadListItems:[OCMArg checkWithBlock:^BOOL(
                                       NSArray<DownloadListItem*>* items) {
        capturedItems = items;
        return YES;
      }]];
  [[mock_consumer_ expect] setLoadingState:NO];

  [mediator_ filterRecordsWithKeyword:@"nonexistent"];
  [mock_consumer_ verify];

  EXPECT_EQ(capturedItems.count, 0U);

  // Test empty search - should return all items.
  [[mock_consumer_ expect]
      setDownloadListItems:[OCMArg checkWithBlock:^BOOL(
                                       NSArray<DownloadListItem*>* items) {
        capturedItems = items;
        return YES;
      }]];
  [[mock_consumer_ expect] setLoadingState:NO];

  [mediator_ filterRecordsWithKeyword:@""];
  [mock_consumer_ verify];

  EXPECT_EQ(capturedItems.count, 6U);
}

// Test combined filtering and search functionality.
TEST_F(DownloadListMediatorTest, TestCombinedFilterAndSearch) {
  __block NSArray<DownloadListItem*>* capturedItems = nil;

  // First apply PDF filter.
  [[mock_consumer_ expect] setDownloadListItems:[OCMArg any]];
  [[mock_consumer_ expect] setLoadingState:NO];

  [mediator_ filterRecordsWithType:DownloadFilterType::kPDF];
  [mock_consumer_ verify];

  // Then search for "document" within PDF filter - should still find
  // document.pdf.
  [[mock_consumer_ expect]
      setDownloadListItems:[OCMArg checkWithBlock:^BOOL(
                                       NSArray<DownloadListItem*>* items) {
        capturedItems = items;
        return YES;
      }]];
  [[mock_consumer_ expect] setLoadingState:NO];

  [mediator_ filterRecordsWithKeyword:@"document"];
  [mock_consumer_ verify];

  EXPECT_EQ(capturedItems.count, 1U);
  EXPECT_TRUE([capturedItems[0].downloadID isEqualToString:@"1"]);

  // Search for "image" within PDF filter - should return empty.
  [[mock_consumer_ expect]
      setDownloadListItems:[OCMArg checkWithBlock:^BOOL(
                                       NSArray<DownloadListItem*>* items) {
        capturedItems = items;
        return YES;
      }]];
  [[mock_consumer_ expect] setLoadingState:NO];

  [mediator_ filterRecordsWithKeyword:@"image"];
  [mock_consumer_ verify];

  EXPECT_EQ(capturedItems.count, 0U);
}
