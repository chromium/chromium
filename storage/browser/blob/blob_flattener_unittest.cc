// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/blob/blob_storage_context.h"

#include <memory>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/test/test_simple_task_runner.h"
#include "base/time/time.h"
#include "storage/browser/blob/blob_data_builder.h"
#include "storage/browser/blob/blob_data_handle.h"
#include "storage/browser/blob/blob_data_item.h"
#include "storage/browser/blob/blob_entry.h"
#include "storage/browser/blob/blob_memory_controller.h"
#include "storage/browser/blob/blob_storage_registry.h"
#include "storage/browser/blob/shareable_blob_data_item.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace storage {
namespace {
using base::TestSimpleTaskRunner;
using FileCreationInfo = BlobMemoryController::FileCreationInfo;

const size_t kTestBlobStorageIPCThresholdBytes = 20;
const size_t kTestBlobStorageMaxSharedMemoryBytes = 50;

const size_t kTestBlobStorageMaxBlobMemorySize = 400;
const uint64_t kTestBlobStorageMaxDiskSpace = 4000;
const uint64_t kTestBlobStorageMinFileSizeBytes = 10;
const uint64_t kTestBlobStorageMaxFileSizeBytes = 100;

void SaveBlobStatusAndFiles(BlobStatus* status_ptr,
                            std::vector<FileCreationInfo>* files_ptr,
                            BlobStatus status,
                            std::vector<FileCreationInfo> files) {
  EXPECT_FALSE(BlobStatusIsError(status));
  *status_ptr = status;
  std::move(files.begin(), files.end(), std::back_inserter(*files_ptr));
}

}  // namespace

// Historically BlobFlattener was a separate class. All that functionality
// was merged into BlobDataBuilder though, so now this test just tests that
// subset of the BlobDataBuilder functionality.
class BlobFlattenerTest : public testing::Test {
 protected:
  BlobFlattenerTest()
      : fake_file_path_(base::FilePath(FILE_PATH_LITERAL("kFakePath"))) {}
  ~BlobFlattenerTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    context_ = std::make_unique<BlobStorageContext>();
  }

  void TearDown() override {
    base::RunLoop().RunUntilIdle();
    file_runner_->RunPendingTasks();
    base::RunLoop().RunUntilIdle();
    ASSERT_TRUE(temp_dir_.Delete());
  }

  scoped_refptr<BlobDataItem> CreateDataDescriptionItem(size_t size) {
    return BlobDataItem::CreateBytesDescription(size);
  }

  scoped_refptr<BlobDataItem> CreateDataItem(const char* memory, size_t size) {
    return BlobDataItem::CreateBytes(
        base::as_bytes(base::make_span(memory, size)));
  }

  scoped_refptr<BlobDataItem> CreateFileItem(size_t offset, size_t size) {
    return BlobDataItem::CreateFile(fake_file_path_, offset, size,
                                    base::Time::Max());
  }

  scoped_refptr<BlobDataItem> CreateFutureFileItem(size_t offset, size_t size) {
    return BlobDataItem::CreateFutureFile(offset, size, 0);
  }

  std::unique_ptr<BlobDataHandle> SetupBasicBlob(const std::string& id) {
    auto builder = std::make_unique<BlobDataBuilder>(id);
    builder->AppendData(std::string("1"));
    builder->set_content_type("text/plain");
    return context_->AddFinishedBlob(std::move(builder));
  }

  const BlobStorageRegistry& registry() { return context_->registry(); }

  const ShareableBlobDataItem& GetItemInBlob(const std::string& uuid,
                                             size_t index) {
    const BlobEntry* entry = registry().GetEntry(uuid);
    EXPECT_TRUE(entry);
    return *entry->items()[index];
  }

  void SetTestMemoryLimits() {
    BlobStorageLimits limits;
    limits.max_ipc_memory_size = kTestBlobStorageIPCThresholdBytes;
    limits.max_shared_memory_size = kTestBlobStorageMaxSharedMemoryBytes;
    limits.max_blob_in_memory_space = kTestBlobStorageMaxBlobMemorySize;
    limits.desired_max_disk_space = kTestBlobStorageMaxDiskSpace;
    limits.effective_max_disk_space = kTestBlobStorageMaxDiskSpace;
    limits.min_page_file_size = kTestBlobStorageMinFileSizeBytes;
    limits.max_file_size = kTestBlobStorageMaxFileSizeBytes;
    context_->set_limits_for_testing(limits);
  }

  base::FilePath fake_file_path_;
  base::ScopedTempDir temp_dir_;
  scoped_refptr<TestSimpleTaskRunner> file_runner_ = new TestSimpleTaskRunner();

  base::test::SingleThreadTaskEnvironment task_environment;
  std::unique_ptr<BlobStorageContext> context_;
};

TEST_F(BlobFlattenerTest, NoBlobItems) {
  const std::string kBlobUUID = "kId";

  BlobDataBuilder builder(kBlobUUID);
  builder.AppendData(std::string("hi"));
  builder.AppendFile(fake_file_path_, 0u, 10u, base::Time::Max());

  EXPECT_EQ(0u, builder.dependent_blobs().size());
  EXPECT_EQ(0u, builder.copies().size());
  EXPECT_EQ(12u, builder.total_size());
  EXPECT_EQ(2u, builder.transport_quota_needed());

  ASSERT_EQ(2u, builder.items().size());
  EXPECT_EQ(*CreateDataItem("hi", 2u), *builder.items()[0]->item());
  EXPECT_EQ(*CreateFileItem(0, 10u), *builder.items()[1]->item());
}

TEST_F(BlobFlattenerTest, ErrorCases) {
  const std::string kBlobUUID = "kId";
  const std::string kBlob2UUID = "kId2";

  // Invalid blob reference.
  {
    BlobDataBuilder builder(kBlobUUID);
    builder.AppendBlob("doesnotexist", registry());
    EXPECT_FALSE(builder.IsValid());
  }

  // Circular reference.
  {
    BlobDataBuilder builder(kBlobUUID);
    builder.AppendBlob(kBlobUUID, registry());
    EXPECT_FALSE(builder.IsValid());
  }

  // Bad slice.
  {
    std::unique_ptr<BlobDataHandle> handle = SetupBasicBlob(kBlob2UUID);
    BlobDataBuilder builder(kBlobUUID);
    builder.AppendBlob(kBlob2UUID, 1, 2, registry());
    EXPECT_FALSE(builder.IsValid());
  }
}

TEST_F(BlobFlattenerTest, BlobWithSlices) {
  const std::string kBlobUUID = "kId";
  const std::string kDataBlob = "kId2";
  const std::string kFileBlob = "kId3";
  const std::string kPendingFileBlob = "kId4";

  // We have the following:
  // * data,
  // * sliced data blob,
  // * file
  // * full data blob,
  // * pending data,

  context_ = std::make_unique<BlobStorageContext>(
      temp_dir_.GetPath(), temp_dir_.GetPath(), file_runner_);
  SetTestMemoryLimits();

  std::unique_ptr<BlobDataHandle> data_blob;
  {
    auto builder = std::make_unique<BlobDataBuilder>(kDataBlob);
    builder->AppendData(std::string("12345"));
    builder->set_content_type("text/plain");
    data_blob = context_->AddFinishedBlob(std::move(builder));
  }

  std::unique_ptr<BlobDataHandle> file_blob;
  {
    auto builder = std::make_unique<BlobDataBuilder>(kFileBlob);
    builder->AppendFile(fake_file_path_, 1u, 10u, base::Time::Max());
    file_blob = context_->AddFinishedBlob(std::move(builder));
  }

  BlobStatus file_status = BlobStatus::ERR_INVALID_CONSTRUCTION_ARGUMENTS;
  std::vector<FileCreationInfo> file_handles;
  std::unique_ptr<BlobDataHandle> future_file_blob;
  {
    auto builder = std::make_unique<BlobDataBuilder>(kPendingFileBlob);
    builder->AppendFutureFile(0u, 2u, 0);
    builder->AppendFutureFile(2u, 5u, 0);
    future_file_blob = context_->BuildBlob(
        std::move(builder),
        base::BindOnce(&SaveBlobStatusAndFiles, &file_status, &file_handles));
  }

  BlobDataBuilder builder(kBlobUUID);
  builder.AppendData(std::string("hi"));
  builder.AppendBlob(kDataBlob, 1u, 2u, registry());
  builder.AppendFile(fake_file_path_, 3u, 5u, base::Time::Max());
  builder.AppendBlob(kDataBlob, registry());
  builder.AppendBlob(kFileBlob, 1u, 3u, registry());
  builder.AppendFutureData(12u);
  builder.AppendBlob(kPendingFileBlob, 1u, 3u, registry());

  ASSERT_TRUE(builder.IsValid());
  EXPECT_EQ(3u, builder.dependent_blobs().size());
  EXPECT_EQ(32u, builder.total_size());
  EXPECT_EQ(14u, builder.transport_quota_needed());
  EXPECT_EQ(2u, builder.copy_quota_needed());

  ASSERT_EQ(8u, builder.items().size());
  EXPECT_EQ(*CreateDataItem("hi", 2u), *builder.items()[0]->item());
  EXPECT_EQ(*CreateDataDescriptionItem(2u), *builder.items()[1]->item());
  EXPECT_EQ(*CreateFileItem(3u, 5u), *builder.items()[2]->item());
  EXPECT_EQ(GetItemInBlob(kDataBlob, 0), *builder.items()[3]);
  EXPECT_EQ(*CreateFileItem(2u, 3u), *builder.items()[4]->item());
  EXPECT_EQ(*CreateDataDescriptionItem(12u), *builder.items()[5]->item());
  EXPECT_EQ(*CreateFutureFileItem(1u, 1u), *builder.items()[6]->item());
  EXPECT_EQ(*CreateFutureFileItem(2u, 2u), *builder.items()[7]->item());

  // We're copying items at index 1, 6, and 7.
  ASSERT_EQ(3u, builder.copies().size());
  EXPECT_EQ(*builder.copies()[0].dest_item, *builder.items()[1]);
  EXPECT_EQ(GetItemInBlob(kDataBlob, 0), *builder.copies()[0].source_item);
  EXPECT_EQ(1u, builder.copies()[0].source_item_offset);
  EXPECT_EQ(*builder.copies()[1].dest_item, *builder.items()[6]);
  EXPECT_EQ(GetItemInBlob(kPendingFileBlob, 0),
            *builder.copies()[1].source_item);
  EXPECT_EQ(1u, builder.copies()[1].source_item_offset);
  EXPECT_EQ(*builder.copies()[2].dest_item, *builder.items()[7]);
  EXPECT_EQ(GetItemInBlob(kPendingFileBlob, 1),
            *builder.copies()[2].source_item);
  EXPECT_EQ(0u, builder.copies()[2].source_item_offset);

  // Clean up temp files.
  EXPECT_TRUE(file_runner_->HasPendingTask());
  file_runner_->RunPendingTasks();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(BlobStatus::PENDING_TRANSPORT, file_status);
  EXPECT_FALSE(file_handles.empty());
}

}  // namespace storage
