// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/model/download_record_database.h"

#import <limits>

#import "base/files/file_path.h"
#import "base/files/scoped_temp_dir.h"
#import "base/strings/stringprintf.h"
#import "base/test/task_environment.h"
#import "base/time/time.h"
#import "ios/chrome/browser/download/model/download_record.h"
#import "ios/web/public/download/download_task.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace {

// Test constants.
const char kTestDownloadId[] = "test_download_id";
const char kTestFileName[] = "test_file.pdf";
const char kTestUrl[] = "https://example.com/test_file.pdf";
const char kTestRedirectedUrl[] = "https://cdn.example.com/test_file.pdf";
const char kTestMimeType[] = "application/pdf";
const char kTestOriginatingHost[] = "example.com";
const char kTestHttpMethod[] = "GET";
const char kTestFilePath[] = "/tmp/test_file.pdf";
const char kTestResponsePath[] = "/tmp/response.pdf";
const char kTestContentDisposition[] = "attachment; filename=\"test_file.pdf\"";
const char kNonExistentId[] = "non_existent_id";

// Test numeric constants.
const int kTestHttpCode = 200;
const int kTestErrorCode = 0;
const int64_t kTestTotalBytes = 1024;

// Fixed test times for precise comparison.
// 2026-01-01 00:00:00 UTC.
const base::Time kTestCreatedTime =
    base::Time::FromSecondsSinceUnixEpoch(1767225600.0);
// 2026-01-01 00:10:00 UTC.
const base::Time kTestCompletedTime =
    base::Time::FromSecondsSinceUnixEpoch(1767226200.0);

}  // namespace

class DownloadRecordDatabaseTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();

    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    db_path_ = temp_dir_.GetPath().AppendASCII("DownloadRecord");

    database_ = std::make_unique<DownloadRecordDatabase>(db_path_);
    ASSERT_EQ(sql::INIT_OK, database_->Init());
    ASSERT_TRUE(database_->IsInitialized());
  }

  void TearDown() override {
    database_.reset();
    PlatformTest::TearDown();
  }

  DownloadRecordDatabase* database() { return database_.get(); }

  // Creates a test record with all fields populated.
  // Note: received_bytes and progress_percent are not stored in database.
  DownloadRecord CreateTestRecord(
      const std::string& download_id = kTestDownloadId) {
    DownloadRecord record;
    record.download_id = download_id;
    record.file_name = kTestFileName;
    record.original_url = kTestUrl;
    record.redirected_url = kTestRedirectedUrl;
    record.file_path = base::FilePath(kTestFilePath);
    record.response_path = base::FilePath(kTestResponsePath);
    record.original_mime_type = kTestMimeType;
    record.mime_type = kTestMimeType;
    record.content_disposition = kTestContentDisposition;
    record.originating_host = kTestOriginatingHost;
    record.http_method = kTestHttpMethod;
    record.http_code = kTestHttpCode;
    record.error_code = kTestErrorCode;
    record.total_bytes = kTestTotalBytes;
    record.state = web::DownloadTask::State::kInProgress;
    record.created_time = kTestCreatedTime;
    record.completed_time = base::Time();  // Null time initially.
    record.has_performed_background_download = false;
    return record;
  }

  // Inserts a record and verifies it was inserted successfully.
  void InsertAndVerifyRecord(const DownloadRecord& record) {
    EXPECT_TRUE(database()->InsertDownloadRecord(record));
    auto retrieved = database()->GetDownloadRecord(record.download_id);
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_EQ(record, retrieved.value());
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  base::FilePath db_path_;
  std::unique_ptr<DownloadRecordDatabase> database_;
};

#pragma mark - Basic CRUD Tests

// Tests basic insert and retrieve functionality.
TEST_F(DownloadRecordDatabaseTest, InsertAndRetrieveRecord) {
  DownloadRecord original_record = CreateTestRecord();
  InsertAndVerifyRecord(original_record);
}

// Tests record update functionality.
TEST_F(DownloadRecordDatabaseTest, UpdateRecord) {
  // Insert initial record.
  DownloadRecord original_record = CreateTestRecord();
  EXPECT_TRUE(database()->InsertDownloadRecord(original_record));

  // Update values to distinctly different ones.
  const std::string kUpdatedOriginalUrl =
      "https://UPDATED.example.com/UPDATED_file.pdf";
  const std::string kUpdatedRedirectedUrl =
      "https://cdn.UPDATED.example.com/UPDATED_file.pdf";
  const std::string kUpdatedFileName = "UPDATED_file_name.pdf";
  const std::string kUpdatedOriginalMimeType = "application/UPDATED-pdf";
  const std::string kUpdatedMimeType = "application/UPDATED-pdf";
  const std::string kUpdatedContentDisposition =
      "attachment; filename=\"UPDATED_file_name.pdf\"";
  const std::string kUpdatedOriginatingHost = "UPDATED.example.com";
  const std::string kUpdatedHttpMethod = "POST";  // Original: "GET"
  const base::FilePath kUpdatedFilePath("/tmp/UPDATED_file_path.pdf");
  const base::FilePath kUpdatedResponsePath("/tmp/UPDATED_response_path.pdf");
  const int kUpdatedHttpCode = 201;         // Original: 200
  const int kUpdatedErrorCode = 42;         // Original: 0
  const int64_t kUpdatedTotalBytes = 2048;  // Original: 1024
  const web::DownloadTask::State kUpdatedState =
      web::DownloadTask::State::kComplete;  // Original: kInProgress
  const bool kUpdatedHasPerformedBackgroundDownload = true;  // Original: false
  const base::Time kUpdatedCreatedTime = base::Time::FromSecondsSinceUnixEpoch(
      1700000000.0);  // Different time for testing.

  // Modify all updateable fields to distinctly different values.
  DownloadRecord updated_record = original_record;
  updated_record.original_url = kUpdatedOriginalUrl;
  updated_record.redirected_url = kUpdatedRedirectedUrl;
  updated_record.file_name = kUpdatedFileName;
  updated_record.original_mime_type = kUpdatedOriginalMimeType;
  updated_record.mime_type = kUpdatedMimeType;
  updated_record.content_disposition = kUpdatedContentDisposition;
  updated_record.originating_host = kUpdatedOriginatingHost;
  updated_record.http_method = kUpdatedHttpMethod;
  updated_record.file_path = kUpdatedFilePath;
  updated_record.response_path = kUpdatedResponsePath;
  updated_record.http_code = kUpdatedHttpCode;
  updated_record.error_code = kUpdatedErrorCode;
  updated_record.total_bytes = kUpdatedTotalBytes;
  updated_record.state = kUpdatedState;
  updated_record.completed_time = kTestCompletedTime;
  updated_record.has_performed_background_download =
      kUpdatedHasPerformedBackgroundDownload;
  // Try to modify created_time (should be ignored by database).
  updated_record.created_time = kUpdatedCreatedTime;

  // Verify that updated values are actually different from originals.
  EXPECT_NE(original_record.original_url, updated_record.original_url);
  EXPECT_NE(original_record.redirected_url, updated_record.redirected_url);
  EXPECT_NE(original_record.file_name, updated_record.file_name);
  EXPECT_NE(original_record.file_path, updated_record.file_path);
  EXPECT_NE(original_record.response_path, updated_record.response_path);
  EXPECT_NE(original_record.original_mime_type,
            updated_record.original_mime_type);
  EXPECT_NE(original_record.mime_type, updated_record.mime_type);
  EXPECT_NE(original_record.content_disposition,
            updated_record.content_disposition);
  EXPECT_NE(original_record.originating_host, updated_record.originating_host);
  EXPECT_NE(original_record.http_method, updated_record.http_method);
  EXPECT_NE(original_record.http_code, updated_record.http_code);
  EXPECT_NE(original_record.error_code, updated_record.error_code);
  EXPECT_NE(original_record.total_bytes, updated_record.total_bytes);
  EXPECT_NE(original_record.state, updated_record.state);
  EXPECT_NE(original_record.completed_time, updated_record.completed_time);
  EXPECT_NE(original_record.has_performed_background_download,
            updated_record.has_performed_background_download);
  EXPECT_NE(
      original_record.created_time,
      updated_record.created_time);  // Verify attempt to change created_time.

  // Execute update.
  EXPECT_TRUE(database()->UpdateDownloadRecord(updated_record));

  // Verifies update results.
  auto retrieved_record = database()->GetDownloadRecord(kTestDownloadId);
  ASSERT_TRUE(retrieved_record.has_value());

  // Verifies all updateable fields were changed to the expected new values.
  EXPECT_EQ(kUpdatedOriginalUrl, retrieved_record->original_url);
  EXPECT_EQ(kUpdatedRedirectedUrl, retrieved_record->redirected_url);
  EXPECT_EQ(kUpdatedFileName, retrieved_record->file_name);
  EXPECT_EQ(kUpdatedFilePath, retrieved_record->file_path);
  EXPECT_EQ(kUpdatedResponsePath, retrieved_record->response_path);
  EXPECT_EQ(kUpdatedOriginalMimeType, retrieved_record->original_mime_type);
  EXPECT_EQ(kUpdatedMimeType, retrieved_record->mime_type);
  EXPECT_EQ(kUpdatedContentDisposition, retrieved_record->content_disposition);
  EXPECT_EQ(kUpdatedOriginatingHost, retrieved_record->originating_host);
  EXPECT_EQ(kUpdatedHttpMethod, retrieved_record->http_method);
  EXPECT_EQ(kUpdatedHttpCode, retrieved_record->http_code);
  EXPECT_EQ(kUpdatedErrorCode, retrieved_record->error_code);
  EXPECT_EQ(kUpdatedTotalBytes, retrieved_record->total_bytes);
  EXPECT_EQ(kUpdatedState, retrieved_record->state);
  EXPECT_EQ(kTestCompletedTime, retrieved_record->completed_time);
  EXPECT_EQ(kUpdatedHasPerformedBackgroundDownload,
            retrieved_record->has_performed_background_download);

  // Verifies created_time wasn't changed (should be immutable).
  EXPECT_EQ(original_record.created_time, retrieved_record->created_time);
}

// Tests record deletion.
TEST_F(DownloadRecordDatabaseTest, DeleteRecord) {
  DownloadRecord record = CreateTestRecord();
  EXPECT_TRUE(database()->InsertDownloadRecord(record));

  auto retrieved_record = database()->GetDownloadRecord(kTestDownloadId);
  EXPECT_TRUE(retrieved_record.has_value());

  EXPECT_TRUE(database()->DeleteDownloadRecord(kTestDownloadId));

  auto deleted_record = database()->GetDownloadRecord(kTestDownloadId);
  EXPECT_FALSE(deleted_record.has_value());
}

// Tests retrieving all records.
TEST_F(DownloadRecordDatabaseTest, GetAllDownloadRecords) {
  const int kRecordCount = 5;
  std::vector<DownloadRecord> inserted_records;

  // Insert multiple records with different creation times
  for (int i = 0; i < kRecordCount; ++i) {
    DownloadRecord record = CreateTestRecord();
    record.download_id = base::StringPrintf("download_%d", i);
    record.file_name = base::StringPrintf("file_%d.pdf", i);
    record.created_time = kTestCreatedTime + base::Seconds(i);

    EXPECT_TRUE(database()->InsertDownloadRecord(record));
    inserted_records.push_back(record);
  }

  std::vector<DownloadRecord> all_records = database()->GetAllDownloadRecords();

  // Verifies count.
  EXPECT_EQ(static_cast<size_t>(kRecordCount), all_records.size());

  // Verifies ordering (should be newest first by created_time).
  for (size_t i = 1; i < all_records.size(); ++i) {
    EXPECT_GE(all_records[i - 1].created_time, all_records[i].created_time);
  }
}

#pragma mark - Boundary Tests

// Tests boundary values for all data types.
TEST_F(DownloadRecordDatabaseTest, DataTypeBoundaryValues) {
  // Define boundary test values.
  const int64_t max_total_bytes = std::numeric_limits<int64_t>::max();
  const int max_http_code = 999;
  const int max_error_code = std::numeric_limits<int>::max();
  const base::Time early_time =
      base::Time::FromSecondsSinceUnixEpoch(1.0);  // Early but valid time.
  const base::Time max_time = base::Time::Max();
  const std::string long_filename = std::string(500, 'a');  // Long string.
  const std::string empty_content_disposition = "";         // Empty string.

  DownloadRecord record = CreateTestRecord();
  record.download_id = "boundary_test";
  record.total_bytes = max_total_bytes;
  record.http_code = max_http_code;
  record.error_code = max_error_code;
  record.created_time = early_time;
  record.completed_time = max_time;
  record.file_name = long_filename;
  record.content_disposition = empty_content_disposition;

  InsertAndVerifyRecord(record);
}

// Tests special characters and URL encoding.
TEST_F(DownloadRecordDatabaseTest, SpecialCharactersAndEncoding) {
  // Defines test values with special characters.
  const std::string sql_injection_id = "test'\"\\;DROP TABLE;--";
  const std::string unicode_filename =
      "file_name.test.pdf";  // Special characters.
  const std::string url_with_spaces =
      "https://example.com/path with spaces & symbols";
  const std::string encoded_content_disposition =
      "attachment; filename*=UTF-8''test_file.pdf";
  const std::string unicode_host = "test.example.com";

  DownloadRecord record = CreateTestRecord();
  record.download_id = sql_injection_id;
  record.file_name = unicode_filename;
  record.original_url = url_with_spaces;
  record.content_disposition = encoded_content_disposition;
  record.originating_host = unicode_host;

  InsertAndVerifyRecord(record);
}

#pragma mark - Error Handling and Edge Cases

// Tests duplicate ID insertion.
TEST_F(DownloadRecordDatabaseTest, InsertDuplicateRecord) {
  DownloadRecord record = CreateTestRecord();

  // First insertion should succeed.
  EXPECT_TRUE(database()->InsertDownloadRecord(record));

  // Duplicate insertion should fail.
  EXPECT_FALSE(database()->InsertDownloadRecord(record));
}

// Tests updating non-existent record.
TEST_F(DownloadRecordDatabaseTest, UpdateNonExistentRecord) {
  DownloadRecord record = CreateTestRecord();
  record.download_id = kNonExistentId;

  // Update should succeed but have no effect (standard SQL behavior).
  EXPECT_TRUE(database()->UpdateDownloadRecord(record));

  // Verifies the record was not actually created.
  auto retrieved = database()->GetDownloadRecord(kNonExistentId);
  EXPECT_FALSE(retrieved.has_value());
}

// Tests getting non-existent record.
TEST_F(DownloadRecordDatabaseTest, GetNonExistentRecord) {
  auto result = database()->GetDownloadRecord(kNonExistentId);
  EXPECT_FALSE(result.has_value());
}

// Tests deleting non-existent record (should be idempotent).
TEST_F(DownloadRecordDatabaseTest, DeleteNonExistentRecord) {
  // Delete should succeed even if record doesn't exist (idempotent).
  EXPECT_TRUE(database()->DeleteDownloadRecord(kNonExistentId));
}

// Tests empty database state.
TEST_F(DownloadRecordDatabaseTest, EmptyDatabaseOperations) {
  auto empty_result = database()->GetAllDownloadRecords();
  EXPECT_TRUE(empty_result.empty());

  auto single_result = database()->GetDownloadRecord("any_id");
  EXPECT_FALSE(single_result.has_value());
}

#pragma mark - Batch Operations Tests

// Tests batch state update functionality.
TEST_F(DownloadRecordDatabaseTest, UpdateDownloadRecordsState) {
  const int kRecordCount = 3;
  std::vector<std::string> download_ids;

  // Insert multiple records
  for (int i = 0; i < kRecordCount; ++i) {
    DownloadRecord record = CreateTestRecord();
    record.download_id = base::StringPrintf("batch_update_%d", i);
    record.state = web::DownloadTask::State::kInProgress;

    EXPECT_TRUE(database()->InsertDownloadRecord(record));
    download_ids.push_back(record.download_id);
  }

  // Batch update state
  EXPECT_TRUE(database()->UpdateDownloadRecordsState(
      download_ids, web::DownloadTask::State::kFailed));

  // Verifies all records updated.
  for (const std::string& id : download_ids) {
    auto record = database()->GetDownloadRecord(id);
    ASSERT_TRUE(record.has_value());
    EXPECT_EQ(web::DownloadTask::State::kFailed, record->state);
  }
}

// Tests batch update with empty ID list.
TEST_F(DownloadRecordDatabaseTest, UpdateDownloadRecordsStateEmptyList) {
  std::vector<std::string> empty_ids;

  // Should return false for empty list.
  EXPECT_FALSE(database()->UpdateDownloadRecordsState(
      empty_ids, web::DownloadTask::State::kComplete));
}

// Tests batch update with non-existent IDs.
TEST_F(DownloadRecordDatabaseTest, UpdateDownloadRecordsStateNonExistent) {
  const std::vector<std::string> non_existent_ids = {"fake_1", "fake_2"};

  // Should succeed but not affect any records.
  EXPECT_TRUE(database()->UpdateDownloadRecordsState(
      non_existent_ids, web::DownloadTask::State::kComplete));
}

// Tests batch update with mix of existing and non-existent IDs.
TEST_F(DownloadRecordDatabaseTest, UpdateDownloadRecordsStateMixedExistence) {
  const std::string existing_id = "existing_record";

  // Inserts one record.
  DownloadRecord existing_record = CreateTestRecord();
  existing_record.download_id = existing_id;
  existing_record.state = web::DownloadTask::State::kInProgress;
  EXPECT_TRUE(database()->InsertDownloadRecord(existing_record));

  // Try to update both existing and non-existent IDs.
  const std::vector<std::string> mixed_ids = {existing_id, kNonExistentId};

  // Should succeed and update only the existing record.
  EXPECT_TRUE(database()->UpdateDownloadRecordsState(
      mixed_ids, web::DownloadTask::State::kComplete));

  // Verifies existing record was updated.
  auto retrieved = database()->GetDownloadRecord(existing_id);
  ASSERT_TRUE(retrieved.has_value());
  EXPECT_EQ(web::DownloadTask::State::kComplete, retrieved->state);

  // Verifies non-existent record still doesn't exist.
  auto non_existent = database()->GetDownloadRecord(kNonExistentId);
  EXPECT_FALSE(non_existent.has_value());
}
