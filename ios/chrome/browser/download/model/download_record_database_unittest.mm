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
#import "sql/database.h"
#import "sql/meta_table.h"
#import "sql/statement.h"
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

// Incognito records are in-memory only at the service layer and must never
// reach the DB layer. The DB layer has a defensive release-build guard that
// drops `is_incognito = true` records on insert and update; this test pins
// that contract so a future regression in the service layer cannot silently
// persist private download metadata.
TEST_F(DownloadRecordDatabaseTest, IncognitoRecordsAreRejected) {
  DownloadRecord record = CreateTestRecord();
  record.is_incognito = true;

  // Insert must be rejected and the row must not exist in storage.
  EXPECT_FALSE(database()->InsertDownloadRecord(record));
  auto retrieved = database()->GetDownloadRecord(record.download_id);
  EXPECT_FALSE(retrieved.has_value());

  // Insert a non-incognito record and then attempt to flip it to incognito on
  // update — the update must also be rejected.
  DownloadRecord persisted = CreateTestRecord("persisted_id");
  ASSERT_TRUE(database()->InsertDownloadRecord(persisted));

  DownloadRecord incognito_update = persisted;
  incognito_update.is_incognito = true;
  incognito_update.file_name = "leaked.pdf";
  EXPECT_FALSE(database()->UpdateDownloadRecord(incognito_update));

  // The persisted row's file_name must be unchanged.
  auto after_update = database()->GetDownloadRecord(persisted.download_id);
  ASSERT_TRUE(after_update.has_value());
  EXPECT_EQ(persisted.file_name, after_update->file_name);
}

#pragma mark - Pagination Tests

namespace {

// Inserts `count` test records into `db`, with monotonically increasing
// created_time so the natural sort order matches insertion order reversed.
// `mime` and `start_index` allow callers to mix categories or insert in
// multiple batches.
std::vector<DownloadRecord> InsertSequentialRecords(
    DownloadRecordDatabase* db,
    int count,
    const base::Time& base_time,
    const std::string& mime = "application/pdf",
    int start_index = 0) {
  std::vector<DownloadRecord> inserted;
  for (int i = 0; i < count; ++i) {
    DownloadRecord record;
    record.download_id = base::StringPrintf("id_%04d", start_index + i);
    record.file_name = base::StringPrintf("file_%04d", start_index + i);
    record.original_url = "https://example.com/x";
    record.file_path = base::FilePath("/tmp/x");
    record.mime_type = mime;
    record.original_mime_type = mime;
    record.state = web::DownloadTask::State::kComplete;
    record.created_time = base_time + base::Seconds(start_index + i);
    EXPECT_TRUE(db->InsertDownloadRecord(record));
    inserted.push_back(record);
  }
  return inserted;
}

}  // namespace

// First page returns up to the internal page size in (created_time DESC,
// download_id DESC) order.
TEST_F(DownloadRecordDatabaseTest, GetDownloadRecordsPageFirstPage) {
  // Insert more than one page worth (internal page size is 50).
  InsertSequentialRecords(database(), 60, kTestCreatedTime);

  DownloadRecordDatabase::DownloadRecordQuery query;
  auto page = database()->GetDownloadRecordsPage(query);

  ASSERT_EQ(50u, page.size());
  // Newest first: id_0059 down to id_0010.
  EXPECT_EQ("id_0059", page.front().download_id);
  EXPECT_EQ("id_0010", page.back().download_id);
  for (size_t i = 1; i < page.size(); ++i) {
    EXPECT_GE(page[i - 1].created_time, page[i].created_time);
  }
}

// Cursor-based continuation: concatenated pages equal GetAllDownloadRecords().
TEST_F(DownloadRecordDatabaseTest, GetDownloadRecordsPageCursorContinuation) {
  // Need more than one page to exercise continuation.
  InsertSequentialRecords(database(), 130, kTestCreatedTime);

  auto all = database()->GetAllDownloadRecords();
  ASSERT_EQ(130u, all.size());

  std::vector<DownloadRecord> concatenated;
  DownloadRecordDatabase::DownloadRecordQuery query;
  while (true) {
    auto page = database()->GetDownloadRecordsPage(query);
    if (page.empty()) {
      break;
    }
    for (const auto& record : page) {
      concatenated.push_back(record);
    }
    query.cursor_created_time = page.back().created_time;
    query.cursor_download_id = page.back().download_id;
    // Last page when SQL stream is exhausted; loop will return empty next.
    if (page.size() < 50u) {
      break;
    }
  }

  ASSERT_EQ(all.size(), concatenated.size());
  for (size_t i = 0; i < all.size(); ++i) {
    EXPECT_EQ(all[i].download_id, concatenated[i].download_id);
  }
}

// Equal created_time: tie-break by download_id DESC is stable.
TEST_F(DownloadRecordDatabaseTest, GetDownloadRecordsPageTieBreakByDownloadId) {
  for (int i = 0; i < 5; ++i) {
    DownloadRecord record;
    record.download_id = base::StringPrintf("tie_%02d", i);
    record.file_name = "f";
    record.original_url = "https://example.com/";
    record.file_path = base::FilePath("/tmp/x");
    record.mime_type = "application/pdf";
    record.state = web::DownloadTask::State::kComplete;
    record.created_time = kTestCreatedTime;  // Same timestamp for all.
    EXPECT_TRUE(database()->InsertDownloadRecord(record));
  }

  DownloadRecordDatabase::DownloadRecordQuery query;
  auto page = database()->GetDownloadRecordsPage(query);
  ASSERT_EQ(5u, page.size());
  // download_id DESC: tie_04 .. tie_00.
  EXPECT_EQ("tie_04", page[0].download_id);
  EXPECT_EQ("tie_03", page[1].download_id);
  EXPECT_EQ("tie_02", page[2].download_id);
  EXPECT_EQ("tie_01", page[3].download_id);
  EXPECT_EQ("tie_00", page[4].download_id);

  // Cursor at tie_02 returns only tie_01 and tie_00.
  query.cursor_created_time = page[2].created_time;
  query.cursor_download_id = page[2].download_id;
  auto next = database()->GetDownloadRecordsPage(query);
  ASSERT_EQ(2u, next.size());
  EXPECT_EQ("tie_01", next[0].download_id);
  EXPECT_EQ("tie_00", next[1].download_id);
}

// Inserting a new top row between page 1 and page 2 must not duplicate or
// skip rows on page 2 (the whole point of keyset pagination).
TEST_F(DownloadRecordDatabaseTest, GetDownloadRecordsPageStableAcrossInsert) {
  // 60 rows so page 1 fills (50) and page 2 has 10.
  InsertSequentialRecords(database(), 60, kTestCreatedTime);

  DownloadRecordDatabase::DownloadRecordQuery query;
  auto page1 = database()->GetDownloadRecordsPage(query);
  ASSERT_EQ(50u, page1.size());
  EXPECT_EQ("id_0059", page1.front().download_id);
  EXPECT_EQ("id_0010", page1.back().download_id);

  // Insert a brand-new top row after page 1.
  DownloadRecord newest;
  newest.download_id = "id_9999";
  newest.file_name = "f";
  newest.original_url = "https://example.com/";
  newest.file_path = base::FilePath("/tmp/x");
  newest.mime_type = "application/pdf";
  newest.state = web::DownloadTask::State::kComplete;
  newest.created_time = kTestCreatedTime + base::Seconds(1000);
  EXPECT_TRUE(database()->InsertDownloadRecord(newest));

  // Page 2 continues from page 1's last row.
  query.cursor_created_time = page1.back().created_time;
  query.cursor_download_id = page1.back().download_id;
  auto page2 = database()->GetDownloadRecordsPage(query);
  ASSERT_EQ(10u, page2.size());

  // No duplicates and no skips: page 2 is id_0009 .. id_0000.
  EXPECT_EQ("id_0009", page2.front().download_id);
  EXPECT_EQ("id_0000", page2.back().download_id);

  // The newly inserted row is not present in page2.
  for (const auto& record : page2) {
    EXPECT_NE("id_9999", record.download_id);
  }
}

// Filter by mime_type composes correctly with the cursor.
TEST_F(DownloadRecordDatabaseTest,
       GetDownloadRecordsPageFilterComposesWithCursor) {
  // 5 PDFs + 5 images, interleaved created_time.
  for (int i = 0; i < 10; ++i) {
    DownloadRecord record;
    record.download_id = base::StringPrintf("mix_%02d", i);
    record.file_name = "f";
    record.original_url = "https://example.com/";
    record.file_path = base::FilePath("/tmp/x");
    record.mime_type = (i % 2 == 0) ? "application/pdf" : "image/png";
    record.state = web::DownloadTask::State::kComplete;
    record.created_time = kTestCreatedTime + base::Seconds(i);
    EXPECT_TRUE(database()->InsertDownloadRecord(record));
  }

  DownloadRecordDatabase::DownloadRecordQuery pdf_query;
  pdf_query.filter_type = DownloadFilterType::kPDF;
  auto pdfs = database()->GetDownloadRecordsPage(pdf_query);
  ASSERT_EQ(5u, pdfs.size());
  for (const auto& record : pdfs) {
    EXPECT_EQ("application/pdf", record.mime_type);
  }
  // Newest PDF index 8, then 6, 4, 2, 0.
  EXPECT_EQ("mix_08", pdfs[0].download_id);
  EXPECT_EQ("mix_00", pdfs.back().download_id);

  // Continue paging within the PDF filter from the 3rd row's cursor.
  pdf_query.cursor_created_time = pdfs[2].created_time;
  pdf_query.cursor_download_id = pdfs[2].download_id;
  auto pdfs_next = database()->GetDownloadRecordsPage(pdf_query);
  ASSERT_EQ(2u, pdfs_next.size());
  EXPECT_EQ("mix_02", pdfs_next[0].download_id);
  EXPECT_EQ("mix_00", pdfs_next[1].download_id);

  // Image filter independently.
  DownloadRecordDatabase::DownloadRecordQuery image_query;
  image_query.filter_type = DownloadFilterType::kImage;
  auto images = database()->GetDownloadRecordsPage(image_query);
  EXPECT_EQ(5u, images.size());
  for (const auto& record : images) {
    EXPECT_EQ("image/png", record.mime_type);
  }
}

// GetDownloadRecordsCount returns correct totals with/without filters.
TEST_F(DownloadRecordDatabaseTest, GetDownloadRecordsCountWithFilters) {
  for (int i = 0; i < 10; ++i) {
    DownloadRecord record;
    record.download_id = base::StringPrintf("cnt_%02d", i);
    record.file_name = "f";
    record.original_url = "https://example.com/";
    record.file_path = base::FilePath("/tmp/x");
    record.mime_type =
        (i < 3) ? "application/pdf" : (i < 7 ? "image/png" : "video/mp4");
    record.state = web::DownloadTask::State::kComplete;
    record.created_time = kTestCreatedTime + base::Seconds(i);
    EXPECT_TRUE(database()->InsertDownloadRecord(record));
  }

  DownloadRecordDatabase::DownloadRecordQuery all_query;
  EXPECT_EQ(10, database()->GetDownloadRecordsCount(all_query));

  DownloadRecordDatabase::DownloadRecordQuery pdf_query;
  pdf_query.filter_type = DownloadFilterType::kPDF;
  EXPECT_EQ(3, database()->GetDownloadRecordsCount(pdf_query));

  DownloadRecordDatabase::DownloadRecordQuery image_query;
  image_query.filter_type = DownloadFilterType::kImage;
  EXPECT_EQ(4, database()->GetDownloadRecordsCount(image_query));

  DownloadRecordDatabase::DownloadRecordQuery video_query;
  video_query.filter_type = DownloadFilterType::kVideo;
  EXPECT_EQ(3, database()->GetDownloadRecordsCount(video_query));

  // Count ignores cursor fields.
  pdf_query.cursor_created_time = kTestCreatedTime + base::Seconds(0);
  pdf_query.cursor_download_id = "cnt_00";
  EXPECT_EQ(3, database()->GetDownloadRecordsCount(pdf_query));
}

#pragma mark - MarkUnfinishedDownloadsAsFailed Tests

// Only kInProgress / kNotStarted are flipped to kFailed; other states are
// untouched.
TEST_F(DownloadRecordDatabaseTest, MarkUnfinishedDownloadsAsFailed) {
  struct Entry {
    std::string id;
    web::DownloadTask::State initial;
    web::DownloadTask::State expected;
  };
  const std::vector<Entry> entries = {
      {"u_notstarted", web::DownloadTask::State::kNotStarted,
       web::DownloadTask::State::kFailed},
      {"u_inprogress", web::DownloadTask::State::kInProgress,
       web::DownloadTask::State::kFailed},
      {"u_complete", web::DownloadTask::State::kComplete,
       web::DownloadTask::State::kComplete},
      {"u_failed", web::DownloadTask::State::kFailed,
       web::DownloadTask::State::kFailed},
      {"u_cancelled", web::DownloadTask::State::kCancelled,
       web::DownloadTask::State::kCancelled},
  };

  int seconds_offset = 0;
  for (const auto& entry : entries) {
    DownloadRecord record;
    record.download_id = entry.id;
    record.file_name = "f";
    record.original_url = "https://example.com/";
    record.file_path = base::FilePath("/tmp/x");
    record.mime_type = "application/pdf";
    record.state = entry.initial;
    record.created_time = kTestCreatedTime + base::Seconds(seconds_offset++);
    EXPECT_TRUE(database()->InsertDownloadRecord(record));
  }

  EXPECT_TRUE(database()->MarkUnfinishedDownloadsAsFailed());

  for (const auto& entry : entries) {
    auto got = database()->GetDownloadRecord(entry.id);
    ASSERT_TRUE(got.has_value()) << entry.id;
    EXPECT_EQ(entry.expected, got->state) << entry.id;
  }
}

#pragma mark - Name Query (Search) Tests

namespace {

// Inserts records whose file_name is taken from `names`. Newer timestamps go
// to later entries so a default page returns names back-to-front.
void InsertNamedRecords(DownloadRecordDatabase* db,
                        const std::vector<std::string>& names,
                        const base::Time& base_time) {
  int i = 0;
  for (const std::string& name : names) {
    DownloadRecord record;
    record.download_id = base::StringPrintf("nm_%03d", i);
    record.file_name = name;
    record.original_url = "https://example.com/";
    record.file_path = base::FilePath("/tmp/x");
    record.mime_type = "application/pdf";
    record.state = web::DownloadTask::State::kComplete;
    record.created_time = base_time + base::Seconds(i);
    EXPECT_TRUE(db->InsertDownloadRecord(record));
    ++i;
  }
}

}  // namespace

// Basic case-insensitive substring search.
TEST_F(DownloadRecordDatabaseTest,
       GetDownloadRecordsPageSearchCaseInsensitive) {
  InsertNamedRecords(database(),
                     {"Report.pdf", "annual_REPORT.pdf", "summary.pdf",
                      "REPO.pdf", "other.pdf"},
                     kTestCreatedTime);

  DownloadRecordDatabase::DownloadRecordQuery query;
  query.name_query = "report";
  auto hits = database()->GetDownloadRecordsPage(query);
  ASSERT_EQ(2u, hits.size());
  // Newest first.
  EXPECT_EQ("annual_REPORT.pdf", hits[0].file_name);
  EXPECT_EQ("Report.pdf", hits[1].file_name);

  EXPECT_EQ(2, database()->GetDownloadRecordsCount(query));
}

// Diacritics: searching for "cafe" matches "café" (ICU UCOL_PRIMARY semantics).
TEST_F(DownloadRecordDatabaseTest, GetDownloadRecordsPageSearchIgnoresAccents) {
  InsertNamedRecords(database(),
                     {"café.pdf", "résumé.pdf", "naïve.pdf", "plain.pdf"},
                     kTestCreatedTime);

  DownloadRecordDatabase::DownloadRecordQuery query;
  query.name_query = "cafe";
  auto hits = database()->GetDownloadRecordsPage(query);
  ASSERT_EQ(1u, hits.size());
  EXPECT_EQ("café.pdf", hits[0].file_name);

  query.name_query = "RESUME";
  hits = database()->GetDownloadRecordsPage(query);
  ASSERT_EQ(1u, hits.size());
  EXPECT_EQ("résumé.pdf", hits[0].file_name);
}

// LIKE wildcards in the user's input are escaped, so they match literally.
TEST_F(DownloadRecordDatabaseTest,
       GetDownloadRecordsPageSearchEscapesWildcards) {
  InsertNamedRecords(
      database(),
      {"100%_done.pdf", "summary.pdf", "snake_case.pdf", "100x_done.pdf"},
      kTestCreatedTime);

  DownloadRecordDatabase::DownloadRecordQuery query;
  query.name_query = "100%";
  auto hits = database()->GetDownloadRecordsPage(query);
  ASSERT_EQ(1u, hits.size());
  EXPECT_EQ("100%_done.pdf", hits[0].file_name);

  query.name_query = "snake_";
  hits = database()->GetDownloadRecordsPage(query);
  ASSERT_EQ(1u, hits.size());
  EXPECT_EQ("snake_case.pdf", hits[0].file_name);
}

// Search composes with mime_type filter and cursor pagination.
TEST_F(DownloadRecordDatabaseTest,
       GetDownloadRecordsPageSearchComposesFilterAndCursor) {
  // 3 PDFs containing "doc", 2 images containing "doc", noise rows.
  std::vector<std::pair<std::string, std::string>> rows = {
      {"doc_a.pdf", "application/pdf"}, {"doc_b.pdf", "application/pdf"},
      {"doc_c.pdf", "application/pdf"}, {"doc_d.png", "image/png"},
      {"doc_e.png", "image/png"},       {"other.pdf", "application/pdf"},
      {"hello.png", "image/png"},
  };
  int i = 0;
  for (const auto& [name, mime] : rows) {
    DownloadRecord record;
    record.download_id = base::StringPrintf("sf_%02d", i);
    record.file_name = name;
    record.original_url = "https://example.com/";
    record.file_path = base::FilePath("/tmp/x");
    record.mime_type = mime;
    record.state = web::DownloadTask::State::kComplete;
    record.created_time = kTestCreatedTime + base::Seconds(i);
    EXPECT_TRUE(database()->InsertDownloadRecord(record));
    ++i;
  }

  // Search "doc" within PDFs only -> 3 hits.
  DownloadRecordDatabase::DownloadRecordQuery query;
  query.filter_type = DownloadFilterType::kPDF;
  query.name_query = "doc";
  auto hits = database()->GetDownloadRecordsPage(query);
  ASSERT_EQ(3u, hits.size());
  EXPECT_EQ("doc_c.pdf", hits[0].file_name);
  EXPECT_EQ("doc_b.pdf", hits[1].file_name);
  EXPECT_EQ("doc_a.pdf", hits[2].file_name);
  EXPECT_EQ(3, database()->GetDownloadRecordsCount(query));

  // Continue from middle hit's cursor.
  query.cursor_created_time = hits[1].created_time;
  query.cursor_download_id = hits[1].download_id;
  auto next = database()->GetDownloadRecordsPage(query);
  ASSERT_EQ(1u, next.size());
  EXPECT_EQ("doc_a.pdf", next[0].file_name);
}

// New records are normalized on insert/update so subsequent searches find them.
TEST_F(DownloadRecordDatabaseTest, GetDownloadRecordsPageSearchAfterUpdate) {
  InsertNamedRecords(database(), {"first.pdf"}, kTestCreatedTime);

  auto got = database()->GetDownloadRecord("nm_000");
  ASSERT_TRUE(got.has_value());
  got->file_name = "RENAMED-Résumé.pdf";
  EXPECT_TRUE(database()->UpdateDownloadRecord(*got));

  DownloadRecordDatabase::DownloadRecordQuery query;
  query.name_query = "resume";
  auto hits = database()->GetDownloadRecordsPage(query);
  ASSERT_EQ(1u, hits.size());
  EXPECT_EQ("RENAMED-Résumé.pdf", hits[0].file_name);
}

#pragma mark - EXPLAIN QUERY PLAN

// Best-effort sanity check that the pagination index is created. The exact
// EXPLAIN QUERY PLAN output isn't easily reachable from the public surface
// of DownloadRecordDatabase, so we instead verify functional behavior the
// index enables (correct ordering with many rows and stable cursor paging).
// This test ensures schema migration created the index without crashing
// the database.
TEST_F(DownloadRecordDatabaseTest, GetDownloadRecordsPageUsesIndexedOrdering) {
  // Insert in scrambled timestamp order to make sure ORDER BY actually fires.
  std::vector<int> order = {3, 0, 7, 1, 5, 9, 2, 6, 4, 8};
  for (int i : order) {
    DownloadRecord record;
    record.download_id = base::StringPrintf("ix_%02d", i);
    record.file_name = "f";
    record.original_url = "https://example.com/";
    record.file_path = base::FilePath("/tmp/x");
    record.mime_type = "application/pdf";
    record.state = web::DownloadTask::State::kComplete;
    record.created_time = kTestCreatedTime + base::Seconds(i);
    EXPECT_TRUE(database()->InsertDownloadRecord(record));
  }

  DownloadRecordDatabase::DownloadRecordQuery query;
  auto page = database()->GetDownloadRecordsPage(query);
  ASSERT_EQ(10u, page.size());
  for (size_t i = 1; i < page.size(); ++i) {
    EXPECT_GE(page[i - 1].created_time, page[i].created_time);
  }
  EXPECT_EQ("ix_09", page.front().download_id);
  EXPECT_EQ("ix_00", page.back().download_id);
}

#pragma mark - Schema Migration Tests

// Fixture for migration tests. Unlike DownloadRecordDatabaseTest, this fixture
// does NOT pre-create the database, so individual tests can construct an older
// schema first and then trigger the upgrade.
// Derives from PlatformTest to drain the autorelease pool between tests, as
// required for Objective-C++ unit tests.
class DownloadRecordDatabaseMigrationTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    db_path_ = temp_dir_.GetPath().AppendASCII("DownloadRecord");
  }

  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  base::FilePath db_path_;
};

// Simulates a v1-shaped database (no `file_name_normalized` column, no
// pagination index, meta-table at version=1, compatible_version=1), then
// opens it through DownloadRecordDatabase::Init() and verifies that the
// v1 -> v2 upgrade path:
//   (a) backfills `file_name_normalized` for every existing row,
//   (b) creates the `idx_records_created_time` index, and
//   (c) bumps both `version` and `last_compatible_version` to 2 so older
//       binaries can no longer open the upgraded database.
TEST_F(DownloadRecordDatabaseMigrationTest, V1ToV2BackfillsIndexAndBumpsVersion) {
  const base::FilePath& db_path = db_path_;

  // Step 1: build a v1-shaped database directly via sql::Database. The schema
  // matches what shipped before this CL: 19 columns, no normalized-name
  // column, no pagination index.
  {
    sql::Database raw_db(sql::Database::Tag("DownloadRecord"));
    ASSERT_TRUE(raw_db.Open(db_path));

    sql::MetaTable meta;
    ASSERT_TRUE(meta.Init(&raw_db, /*version=*/1, /*compatible_version=*/1));

    ASSERT_TRUE(raw_db.Execute(
        "CREATE TABLE IF NOT EXISTS download_records ("
        "download_id TEXT PRIMARY KEY NOT NULL,"
        "original_url TEXT NOT NULL,"
        "redirected_url TEXT,"
        "file_name TEXT NOT NULL,"
        "file_path TEXT,"
        "response_path TEXT,"
        "original_mime_type TEXT,"
        "mime_type TEXT,"
        "content_disposition TEXT,"
        "originating_host TEXT,"
        "http_method TEXT,"
        "http_code INTEGER,"
        "error_code INTEGER,"
        "total_bytes INTEGER,"
        "state INTEGER,"
        "created_time INTEGER,"
        "completed_time INTEGER,"
        "has_performed_background_download INTEGER)"));

    // Insert two sample rows representative of accented / mixed-case names so
    // the backfill has something non-trivial to normalize.
    const auto insert_v1 = [&](const std::string& id,
                               const std::string& file_name) {
      sql::Statement s(raw_db.GetUniqueStatement(
          "INSERT INTO download_records "
          "(download_id, original_url, file_name, mime_type, state, "
          "created_time) VALUES (?, ?, ?, ?, ?, ?)"));
      s.BindString(0, id);
      s.BindString(1, "https://example.com/x");
      s.BindString(2, file_name);
      s.BindString(3, "application/pdf");
      s.BindInt(4, static_cast<int>(web::DownloadTask::State::kComplete));
      s.BindInt64(5, base::Time::Now()
                         .ToDeltaSinceWindowsEpoch()
                         .InMicroseconds());
      ASSERT_TRUE(s.Run());
    };
    insert_v1("mig_01", "Résumé.PDF");
    insert_v1("mig_02", "Annual_REPORT.pdf");

    // Sanity check pre-migration state: column and index do not yet exist.
    ASSERT_FALSE(raw_db.DoesColumnExist("download_records",
                                        "file_name_normalized"));
    ASSERT_FALSE(raw_db.DoesIndexExist("idx_records_created_time"));
    ASSERT_EQ(1, meta.GetVersionNumber());
    ASSERT_EQ(1, meta.GetCompatibleVersionNumber());
  }

  // Step 2: open the same file via DownloadRecordDatabase, triggering the
  // v1 -> v2 upgrade path.
  {
    DownloadRecordDatabase database(db_path);
    ASSERT_EQ(sql::INIT_OK, database.Init());

    // The two pre-existing rows are still readable.
    auto retrieved_1 = database.GetDownloadRecord("mig_01");
    auto retrieved_2 = database.GetDownloadRecord("mig_02");
    ASSERT_TRUE(retrieved_1.has_value());
    ASSERT_TRUE(retrieved_2.has_value());
    EXPECT_EQ("Résumé.PDF", retrieved_1->file_name);
    EXPECT_EQ("Annual_REPORT.pdf", retrieved_2->file_name);

    // (a) Backfill verification: searching by the case-folded /
    // diacritic-stripped form returns the matching row. If the backfill
    // hadn't run, `file_name_normalized` would be NULL for these rows and the
    // LIKE pre-filter in GetDownloadRecordsPage would skip them.
    DownloadRecordDatabase::DownloadRecordQuery resume_query;
    resume_query.name_query = "resume";
    auto resume_hits = database.GetDownloadRecordsPage(resume_query);
    ASSERT_EQ(1u, resume_hits.size());
    EXPECT_EQ("mig_01", resume_hits[0].download_id);

    DownloadRecordDatabase::DownloadRecordQuery report_query;
    report_query.name_query = "report";
    auto report_hits = database.GetDownloadRecordsPage(report_query);
    ASSERT_EQ(1u, report_hits.size());
    EXPECT_EQ("mig_02", report_hits[0].download_id);
  }

  // Step 3: re-open with sql::Database directly to read the meta-table and
  // verify the index now exists.
  {
    sql::Database raw_db(sql::Database::Tag("DownloadRecord"));
    ASSERT_TRUE(raw_db.Open(db_path));

    // (b) New index was created.
    EXPECT_TRUE(raw_db.DoesIndexExist("idx_records_created_time"));
    // Backfill column exists at the schema level too.
    EXPECT_TRUE(raw_db.DoesColumnExist("download_records",
                                       "file_name_normalized"));

    // (c) Both version and last_compatible_version are now 2 — older v1
    // binaries will be locked out, as intended.
    sql::MetaTable meta;
    ASSERT_TRUE(meta.Init(&raw_db, /*version=*/2, /*compatible_version=*/2));
    EXPECT_EQ(2, meta.GetVersionNumber());
    EXPECT_EQ(2, meta.GetCompatibleVersionNumber());
  }
}
