// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/blob/blob_storage_context.h"

#include <stdint.h>

#include <limits>
#include <memory>
#include <string>

#include "base/bind.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/task_environment.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "net/base/io_buffer.h"
#include "net/base/test_completion_callback.h"
#include "storage/browser/blob/blob_data_builder.h"
#include "storage/browser/blob/blob_data_handle.h"
#include "storage/browser/blob/blob_data_item.h"
#include "storage/browser/blob/blob_data_snapshot.h"
#include "storage/browser/blob/blob_impl.h"
#include "storage/browser/test/fake_blob_data_handle.h"
#include "testing/gtest/include/gtest/gtest.h"

using FileCreationInfo = storage::BlobMemoryController::FileCreationInfo;

namespace storage {
namespace {
using base::TestSimpleTaskRunner;

const std::string kBlobStorageDirectory = "blob_storage";
const size_t kTestBlobStorageIPCThresholdBytes = 20;
const size_t kTestBlobStorageMaxSharedMemoryBytes = 50;

const size_t kTestBlobStorageMaxBlobMemorySize = 400;
const uint64_t kTestBlobStorageMaxDiskSpace = 4000;
const uint64_t kTestBlobStorageMinFileSizeBytes = 10;
const uint64_t kTestBlobStorageMaxFileSizeBytes = 100;

void SaveBlobStatus(BlobStatus* status_ptr, BlobStatus status) {
  *status_ptr = status;
}

void SaveBlobStatusAndFiles(BlobStatus* status_ptr,
                            std::vector<FileCreationInfo>* files_ptr,
                            BlobStatus status,
                            std::vector<FileCreationInfo> files) {
  EXPECT_FALSE(BlobStatusIsError(status));
  *status_ptr = status;
  for (FileCreationInfo& info : files) {
    files_ptr->push_back(std::move(info));
  }
}

void IncrementNumber(size_t* number, BlobStatus status) {
  EXPECT_EQ(BlobStatus::DONE, status);
  *number = *number + 1;
}

}  // namespace

class BlobStorageContextTest : public testing::Test {
 protected:
  BlobStorageContextTest() = default;
  ~BlobStorageContextTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    context_ = std::make_unique<BlobStorageContext>();
  }

  void TearDown() override {
    base::RunLoop().RunUntilIdle();
    file_runner_->RunPendingTasks();
    ASSERT_TRUE(temp_dir_.Delete());
  }

  std::unique_ptr<BlobDataHandle> SetupBasicBlob(const std::string& id) {
    auto builder = std::make_unique<BlobDataBuilder>(id);
    builder->AppendData(std::string("1"));
    builder->set_content_type("text/plain");
    return context_->AddFinishedBlob(std::move(builder));
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
    context_->mutable_memory_controller()->set_limits_for_testing(limits);
  }

  void IncrementRefCount(const std::string& uuid) {
    context_->IncrementBlobRefCount(uuid);
  }

  void DecrementRefCount(const std::string& uuid) {
    context_->DecrementBlobRefCount(uuid);
  }

  std::string UUIDFromBlob(blink::mojom::Blob* blob) {
    base::RunLoop loop;
    std::string received_uuid;
    blob->GetInternalUUID(base::BindOnce(
        [](base::OnceClosure quit_closure, std::string* uuid_out,
           const std::string& uuid) {
          *uuid_out = uuid;
          std::move(quit_closure).Run();
        },
        loop.QuitClosure(), &received_uuid));
    loop.Run();
    return received_uuid;
  }

  std::vector<FileCreationInfo> files_;
  base::ScopedTempDir temp_dir_;
  scoped_refptr<TestSimpleTaskRunner> file_runner_ = new TestSimpleTaskRunner();

  base::test::SingleThreadTaskEnvironment task_environment_;
  std::unique_ptr<BlobStorageContext> context_;
};

TEST_F(BlobStorageContextTest, BuildBlobAsync) {
  const std::string kId("id");
  const size_t kSize = 10u;
  BlobStatus status = BlobStatus::ERR_INVALID_CONSTRUCTION_ARGUMENTS;

  auto builder = std::make_unique<BlobDataBuilder>(kId);
  BlobDataBuilder::FutureData future_data = builder->AppendFutureData(kSize);
  builder->set_content_type("text/plain");
  std::unique_ptr<BlobDataSnapshot> builder_snapshot =
      builder->CreateSnapshot();
  EXPECT_EQ(0lu, context_->memory_controller().memory_usage());
  std::unique_ptr<BlobDataHandle> handle = context_->BuildBlob(
      std::move(builder),
      base::BindOnce(&SaveBlobStatusAndFiles, &status, &files_));
  EXPECT_EQ(10lu, context_->memory_controller().memory_usage());
  EXPECT_TRUE(handle->IsBeingBuilt())
      << static_cast<int>(handle->GetBlobStatus());
  EXPECT_EQ(BlobStatus::PENDING_TRANSPORT, status);

  BlobStatus construction_done = BlobStatus::ERR_INVALID_CONSTRUCTION_ARGUMENTS;
  handle->RunOnConstructionComplete(
      base::BindOnce(&SaveBlobStatus, &construction_done));

  EXPECT_EQ(10u, context_->memory_controller().memory_usage());

  future_data.Populate(base::as_bytes(base::make_span("abcdefghij", 10)), 0);
  context_->NotifyTransportComplete(kId);

  // Check we're done.
  EXPECT_EQ(BlobStatus::DONE, handle->GetBlobStatus());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(BlobStatus::DONE, construction_done);

  EXPECT_EQ(*builder_snapshot, *handle->CreateSnapshot());

  handle.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0lu, context_->memory_controller().memory_usage());
}

TEST_F(BlobStorageContextTest, BuildBlobAndCancel) {
  const std::string kId("id");
  const size_t kSize = 10u;
  BlobStatus status = BlobStatus::ERR_INVALID_CONSTRUCTION_ARGUMENTS;

  auto builder = std::make_unique<BlobDataBuilder>(kId);
  builder->AppendFutureData(kSize);
  builder->set_content_type("text/plain");
  EXPECT_EQ(0lu, context_->memory_controller().memory_usage());
  std::unique_ptr<BlobDataHandle> handle = context_->BuildBlob(
      std::move(builder),
      base::BindOnce(&SaveBlobStatusAndFiles, &status, &files_));
  EXPECT_EQ(10lu, context_->memory_controller().memory_usage());
  EXPECT_TRUE(handle->IsBeingBuilt());
  EXPECT_EQ(BlobStatus::PENDING_TRANSPORT, status);
  EXPECT_EQ(10u, context_->memory_controller().memory_usage());

  BlobStatus construction_done = BlobStatus::ERR_INVALID_CONSTRUCTION_ARGUMENTS;
  handle->RunOnConstructionComplete(
      base::BindOnce(&SaveBlobStatus, &construction_done));

  context_->CancelBuildingBlob(kId, BlobStatus::ERR_SOURCE_DIED_IN_TRANSIT);
  EXPECT_TRUE(handle->IsBroken());
  EXPECT_EQ(0lu, context_->memory_controller().memory_usage());

  // Check we're broken.
  EXPECT_EQ(BlobStatus::ERR_SOURCE_DIED_IN_TRANSIT, handle->GetBlobStatus());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(BlobStatus::ERR_SOURCE_DIED_IN_TRANSIT, construction_done);
}

TEST_F(BlobStorageContextTest, CancelledReference) {
  const std::string kId1("id1");
  const std::string kId2("id2");
  const size_t kSize = 10u;
  BlobStatus status = BlobStatus::ERR_INVALID_CONSTRUCTION_ARGUMENTS;

  // Start our first blob.
  auto builder = std::make_unique<BlobDataBuilder>(kId1);
  builder->AppendFutureData(kSize);
  builder->set_content_type("text/plain");
  EXPECT_EQ(0lu, context_->memory_controller().memory_usage());
  std::unique_ptr<BlobDataHandle> handle = context_->BuildBlob(
      std::move(builder),
      base::BindOnce(&SaveBlobStatusAndFiles, &status, &files_));
  EXPECT_EQ(10lu, context_->memory_controller().memory_usage());
  EXPECT_TRUE(handle->IsBeingBuilt());
  EXPECT_EQ(BlobStatus::PENDING_TRANSPORT, status);

  BlobStatus construction_done = BlobStatus::ERR_INVALID_CONSTRUCTION_ARGUMENTS;
  handle->RunOnConstructionComplete(
      base::BindOnce(&SaveBlobStatus, &construction_done));

  EXPECT_EQ(10u, context_->memory_controller().memory_usage());

  // Create our second blob, which depends on the first.
  auto builder2 = std::make_unique<BlobDataBuilder>(kId2);
  builder2->AppendBlob(kId1, context_->registry());
  builder2->set_content_type("text/plain");
  std::unique_ptr<BlobDataHandle> handle2 = context_->BuildBlob(
      std::move(builder2), BlobStorageContext::TransportAllowedCallback());
  BlobStatus construction_done2 =
      BlobStatus::ERR_INVALID_CONSTRUCTION_ARGUMENTS;
  handle->RunOnConstructionComplete(
      base::BindOnce(&SaveBlobStatus, &construction_done2));
  EXPECT_TRUE(handle2->IsBeingBuilt());

  EXPECT_EQ(10lu, context_->memory_controller().memory_usage());

  // Cancel the first blob.
  context_->CancelBuildingBlob(kId1, BlobStatus::ERR_SOURCE_DIED_IN_TRANSIT);

  base::RunLoop().RunUntilIdle();
  // Check we broke successfully.
  EXPECT_EQ(BlobStatus::ERR_SOURCE_DIED_IN_TRANSIT, construction_done);
  EXPECT_EQ(BlobStatus::ERR_SOURCE_DIED_IN_TRANSIT, handle->GetBlobStatus());
  EXPECT_EQ(0lu, context_->memory_controller().memory_usage());
  EXPECT_TRUE(handle->IsBroken());

  // Check that it propagated.
  EXPECT_TRUE(handle2->IsBroken());
  EXPECT_EQ(BlobStatus::ERR_SOURCE_DIED_IN_TRANSIT, construction_done2);
  EXPECT_EQ(BlobStatus::ERR_SOURCE_DIED_IN_TRANSIT, handle->GetBlobStatus());
}

TEST_F(BlobStorageContextTest, IncorrectSlice) {
  const std::string kId1("id1");
  const std::string kId2("id2");

  std::unique_ptr<BlobDataHandle> handle = SetupBasicBlob(kId1);

  EXPECT_EQ(1lu, context_->memory_controller().memory_usage());

  auto builder = std::make_unique<BlobDataBuilder>(kId2);
  builder->AppendBlob(kId1, 1, 10, context_->registry());
  std::unique_ptr<BlobDataHandle> handle2 = context_->BuildBlob(
      std::move(builder), BlobStorageContext::TransportAllowedCallback());

  EXPECT_TRUE(handle2->IsBroken());
  EXPECT_EQ(BlobStatus::ERR_INVALID_CONSTRUCTION_ARGUMENTS,
            handle2->GetBlobStatus());
}

TEST_F(BlobStorageContextTest, IncrementDecrementRef) {
  // Build up a basic blob.
  const std::string kId("id");
  std::unique_ptr<BlobDataHandle> blob_data_handle = SetupBasicBlob(kId);

  // Do an extra increment to keep it around after we kill the handle.
  IncrementRefCount(kId);
  IncrementRefCount(kId);
  DecrementRefCount(kId);
  blob_data_handle = context_->GetBlobDataFromUUID(kId);
  EXPECT_TRUE(blob_data_handle);
  blob_data_handle.reset();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(context_->registry().HasEntry(kId));
  DecrementRefCount(kId);
  EXPECT_FALSE(context_->registry().HasEntry(kId));

  // Make sure it goes away in the end.
  blob_data_handle = context_->GetBlobDataFromUUID(kId);
  EXPECT_FALSE(blob_data_handle);
}

TEST_F(BlobStorageContextTest, BlobDataHandle) {
  // Build up a basic blob.
  const std::string kId("id");
  std::unique_ptr<BlobDataHandle> blob_data_handle = SetupBasicBlob(kId);
  EXPECT_TRUE(blob_data_handle);

  // Get another handle
  std::unique_ptr<BlobDataHandle> another_handle =
      context_->GetBlobDataFromUUID(kId);
  EXPECT_TRUE(another_handle);

  // Should disappear after dropping both handles.
  blob_data_handle.reset();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(context_->registry().HasEntry(kId));

  another_handle.reset();
  base::RunLoop().RunUntilIdle();

  blob_data_handle = context_->GetBlobDataFromUUID(kId);
  EXPECT_FALSE(blob_data_handle);
}

TEST_F(BlobStorageContextTest, MemoryUsage) {
  const std::string kId1("id1");
  const std::string kId2("id2");

  auto builder1 = std::make_unique<BlobDataBuilder>(kId1);
  builder1->AppendData("Data1Data2");
  EXPECT_EQ(0lu, context_->memory_controller().memory_usage());

  std::unique_ptr<BlobDataHandle> blob_data_handle =
      context_->AddFinishedBlob(std::move(builder1));
  EXPECT_EQ(10lu, context_->memory_controller().memory_usage());

  auto builder2 = std::make_unique<BlobDataBuilder>(kId2);
  builder2->AppendBlob(kId1, context_->registry());
  builder2->AppendBlob(kId1, context_->registry());
  builder2->AppendBlob(kId1, context_->registry());
  builder2->AppendBlob(kId1, context_->registry());
  builder2->AppendBlob(kId1, context_->registry());
  builder2->AppendBlob(kId1, context_->registry());
  builder2->AppendBlob(kId1, context_->registry());
  std::unique_ptr<BlobDataHandle> blob_data_handle2 =
      context_->AddFinishedBlob(std::move(builder2));
  EXPECT_EQ(10lu, context_->memory_controller().memory_usage());

  EXPECT_EQ(2u, context_->registry().blob_count());

  blob_data_handle.reset();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(10lu, context_->memory_controller().memory_usage());
  EXPECT_EQ(1u, context_->registry().blob_count());
  blob_data_handle2.reset();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(0lu, context_->memory_controller().memory_usage());
  EXPECT_EQ(0u, context_->registry().blob_count());
}

TEST_F(BlobStorageContextTest, AddFinishedBlob) {
  const std::string kId1("id1");
  const std::string kId2("id12");
  const std::string kId3("id3");

  auto builder1 = std::make_unique<BlobDataBuilder>(kId1);
  builder1->AppendData("Data1Data2");
  std::unique_ptr<BlobDataSnapshot> builder1_data = builder1->CreateSnapshot();
  std::unique_ptr<BlobDataHandle> blob_data_handle =
      context_->AddFinishedBlob(std::move(builder1));

  auto builder2 = std::make_unique<BlobDataBuilder>(kId2);
  builder2->AppendBlob(kId1, 5, 5, context_->registry());
  builder2->AppendData(" is the best");
  BlobDataBuilder canonicalized_blob_data2(kId2);
  canonicalized_blob_data2.AppendData("Data2");
  canonicalized_blob_data2.AppendData(" is the best");
  std::unique_ptr<BlobDataHandle> blob_data_handle2 =
      context_->AddFinishedBlob(std::move(builder2));

  EXPECT_EQ(10u + 12u + 5u, context_->memory_controller().memory_usage());

  ASSERT_TRUE(blob_data_handle);
  ASSERT_TRUE(blob_data_handle2);
  std::unique_ptr<BlobDataSnapshot> data1 = blob_data_handle->CreateSnapshot();
  std::unique_ptr<BlobDataSnapshot> data2 = blob_data_handle2->CreateSnapshot();
  EXPECT_EQ(*data1, *builder1_data);
  EXPECT_EQ(*data2, canonicalized_blob_data2);
  blob_data_handle.reset();
  data2.reset();

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(12u + 5u, context_->memory_controller().memory_usage());

  blob_data_handle = context_->GetBlobDataFromUUID(kId1);
  EXPECT_FALSE(blob_data_handle);
  EXPECT_TRUE(blob_data_handle2);
  data2 = blob_data_handle2->CreateSnapshot();
  EXPECT_EQ(*data2, canonicalized_blob_data2);

  // Test shared elements stick around.
  auto builder3 = std::make_unique<BlobDataBuilder>(kId3);
  builder3->AppendBlob(kId2, context_->registry());
  builder3->AppendBlob(kId2, context_->registry());
  std::unique_ptr<BlobDataHandle> blob_data_handle3 =
      context_->AddFinishedBlob(std::move(builder3));
  EXPECT_FALSE(blob_data_handle3->IsBeingBuilt());
  blob_data_handle2.reset();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(12u + 5u, context_->memory_controller().memory_usage());

  blob_data_handle2 = context_->GetBlobDataFromUUID(kId2);
  EXPECT_FALSE(blob_data_handle2);
  EXPECT_TRUE(blob_data_handle3);
  std::unique_ptr<BlobDataSnapshot> data3 = blob_data_handle3->CreateSnapshot();

  BlobDataBuilder canonicalized_blob_data3(kId3);
  canonicalized_blob_data3.AppendData("Data2");
  canonicalized_blob_data3.AppendData(" is the best");
  canonicalized_blob_data3.AppendData("Data2");
  canonicalized_blob_data3.AppendData(" is the best");
  EXPECT_EQ(*data3, canonicalized_blob_data3);

  blob_data_handle.reset();
  blob_data_handle2.reset();
  blob_data_handle3.reset();
  base::RunLoop().RunUntilIdle();
}

TEST_F(BlobStorageContextTest, AddFinishedBlob_LargeOffset) {
  // A value which does not fit in a 4-byte data type. Used to confirm that
  // large values are supported on 32-bit Chromium builds. Regression test for:
  // crbug.com/458122.
  const uint64_t kLargeSize = std::numeric_limits<uint64_t>::max() - 1;

  const uint64_t kBlobLength = 5;
  const std::string kId1("id1");
  const std::string kId2("id2");

  auto builder1 = std::make_unique<BlobDataBuilder>(kId1);
  builder1->AppendFileSystemFile(GURL(), 0, kLargeSize, base::Time::Now(),
                                 nullptr);
  std::unique_ptr<BlobDataHandle> blob_data_handle1 =
      context_->AddFinishedBlob(std::move(builder1));

  auto builder2 = std::make_unique<BlobDataBuilder>(kId2);
  builder2->AppendBlob(kId1, kLargeSize - kBlobLength, kBlobLength,
                       context_->registry());
  std::unique_ptr<BlobDataHandle> blob_data_handle2 =
      context_->AddFinishedBlob(std::move(builder2));

  ASSERT_TRUE(blob_data_handle1);
  ASSERT_TRUE(blob_data_handle2);
  std::unique_ptr<BlobDataSnapshot> data = blob_data_handle2->CreateSnapshot();
  ASSERT_EQ(1u, data->items().size());
  BlobDataItem* item = data->items()[0].get();
  EXPECT_EQ(kLargeSize - kBlobLength, item->offset());
  EXPECT_EQ(kBlobLength, item->length());

  blob_data_handle1.reset();
  blob_data_handle2.reset();
  base::RunLoop().RunUntilIdle();
}

TEST_F(BlobStorageContextTest, BuildReadableDataHandleBlob) {
  const std::string kTestBlobData = "Test Blob Data";
  auto data_handle =
      base::MakeRefCounted<storage::FakeBlobDataHandle>(kTestBlobData, "");

  {
    BlobStorageContext context;

    const std::string kId1Prime("id1.prime");
    BlobDataBuilder canonicalized_blob_data(kId1Prime);
    canonicalized_blob_data.AppendData(kTestBlobData.c_str());

    const std::string kId1("id1");
    auto builder = std::make_unique<BlobDataBuilder>(kId1);

    builder->AppendReadableDataHandle(data_handle);

    std::unique_ptr<BlobDataSnapshot> builder_data = builder->CreateSnapshot();
    std::unique_ptr<BlobDataHandle> blob_data_handle =
        context.AddFinishedBlob(std::move(builder));
    std::unique_ptr<BlobDataSnapshot> data = blob_data_handle->CreateSnapshot();
    EXPECT_EQ(*data, *builder_data);
    EXPECT_FALSE(data_handle->HasOneRef())
        << "Data handle was destructed while context and builder still exist.";
  }
  EXPECT_TRUE(data_handle->HasOneRef())
      << "Data handle was not destructed along with blob storage context.";
  base::RunLoop().RunUntilIdle();
}

TEST_F(BlobStorageContextTest, BuildFutureFileOnlyBlob) {
  const std::string kId1("id1");
  context_ =
      std::make_unique<BlobStorageContext>(temp_dir_.GetPath(), file_runner_);
  SetTestMemoryLimits();

  auto builder = std::make_unique<BlobDataBuilder>(kId1);
  builder->set_content_type("text/plain");
  BlobDataBuilder::FutureFile future_file = builder->AppendFutureFile(0, 10, 0);

  BlobStatus status = BlobStatus::ERR_INVALID_CONSTRUCTION_ARGUMENTS;
  std::unique_ptr<BlobDataHandle> handle = context_->BuildBlob(
      std::move(builder),
      base::BindOnce(&SaveBlobStatusAndFiles, &status, &files_));

  size_t blobs_finished = 0;
  EXPECT_EQ(BlobStatus::PENDING_QUOTA, handle->GetBlobStatus());
  EXPECT_EQ(BlobStatus::ERR_INVALID_CONSTRUCTION_ARGUMENTS, status);
  handle->RunOnConstructionComplete(
      base::BindOnce(&IncrementNumber, &blobs_finished));
  EXPECT_EQ(0u, blobs_finished);

  EXPECT_TRUE(file_runner_->HasPendingTask());
  file_runner_->RunPendingTasks();
  EXPECT_EQ(0u, blobs_finished);
  EXPECT_EQ(BlobStatus::ERR_INVALID_CONSTRUCTION_ARGUMENTS, status);
  EXPECT_EQ(BlobStatus::PENDING_QUOTA, handle->GetBlobStatus());
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(BlobStatus::PENDING_TRANSPORT, status);
  EXPECT_EQ(BlobStatus::PENDING_TRANSPORT, handle->GetBlobStatus());
  EXPECT_EQ(0u, blobs_finished);

  ASSERT_EQ(1u, files_.size());

  future_file.Populate(files_[0].file_reference, base::Time::Max());
  context_->NotifyTransportComplete(kId1);

  EXPECT_EQ(BlobStatus::DONE, handle->GetBlobStatus());
  EXPECT_EQ(0u, blobs_finished);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, blobs_finished);

  handle.reset();
  files_.clear();
  base::RunLoop().RunUntilIdle();
  // We should have file cleanup tasks.
  EXPECT_TRUE(file_runner_->HasPendingTask());
  file_runner_->RunPendingTasks();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0lu, context_->memory_controller().memory_usage());
  EXPECT_EQ(0lu, context_->memory_controller().disk_usage());
}

TEST_F(BlobStorageContextTest, CompoundBlobs) {
  const std::string kId1("id1");
  const std::string kId2("id2");
  const std::string kId3("id3");

  // Setup a set of blob data for testing.
  base::Time time1, time2;
  ASSERT_TRUE(base::Time::FromString("Tue, 15 Nov 1994, 12:45:26 GMT", &time1));
  ASSERT_TRUE(base::Time::FromString("Mon, 14 Nov 1994, 11:30:49 GMT", &time2));

  auto blob_data1_builder = std::make_unique<BlobDataBuilder>(kId1);
  blob_data1_builder->AppendData("Data1");
  blob_data1_builder->AppendData("Data2");
  blob_data1_builder->AppendFile(base::FilePath(FILE_PATH_LITERAL("File1.txt")),
                                 10, 1024, time1);
  std::unique_ptr<BlobDataSnapshot> blob_data1 =
      blob_data1_builder->CreateSnapshot();

  std::unique_ptr<BlobDataHandle> blob_data_handle;
  // Test a blob referring to only data and a file.
  blob_data_handle = context_->AddFinishedBlob(std::move(blob_data1_builder));

  ASSERT_TRUE(blob_data_handle);
  std::unique_ptr<BlobDataSnapshot> data = blob_data_handle->CreateSnapshot();
  ASSERT_TRUE(blob_data_handle);
  EXPECT_EQ(*data, *blob_data1);

  auto blob_data2 = std::make_unique<BlobDataBuilder>(kId2);
  blob_data2->AppendData("Data3");
  blob_data2->AppendBlob(kId1, 8, 100, context_->registry());
  blob_data2->AppendFile(base::FilePath(FILE_PATH_LITERAL("File2.txt")), 0, 20,
                         time2);

  auto blob_data3_builder = std::make_unique<BlobDataBuilder>(kId3);
  blob_data3_builder->AppendData("Data4");
  auto data_handle =
      base::MakeRefCounted<storage::FakeBlobDataHandle>("Data5", "");
  blob_data3_builder->AppendReadableDataHandle(std::move(data_handle));
  std::unique_ptr<BlobDataSnapshot> blob_data3 =
      blob_data3_builder->CreateSnapshot();

  BlobDataBuilder canonicalized_blob_data2(kId2);
  canonicalized_blob_data2.AppendData("Data3");
  canonicalized_blob_data2.AppendData("a2");
  canonicalized_blob_data2.AppendFile(
      base::FilePath(FILE_PATH_LITERAL("File1.txt")), 10, 98, time1);
  canonicalized_blob_data2.AppendFile(
      base::FilePath(FILE_PATH_LITERAL("File2.txt")), 0, 20, time2);

  // Test a blob composed in part with another blob.
  blob_data_handle = context_->AddFinishedBlob(std::move(blob_data2));
  data = blob_data_handle->CreateSnapshot();
  ASSERT_TRUE(blob_data_handle);
  ASSERT_TRUE(data);
  EXPECT_EQ(*data, canonicalized_blob_data2);

  // Test a blob referring to only data and a disk cache entry.
  blob_data_handle = context_->AddFinishedBlob(std::move(blob_data3_builder));
  data = blob_data_handle->CreateSnapshot();
  ASSERT_TRUE(blob_data_handle);
  EXPECT_EQ(*data, *blob_data3);

  blob_data_handle.reset();
  base::RunLoop().RunUntilIdle();
}

TEST_F(BlobStorageContextTest, PublicBlobUrls) {
  // Build up a basic blob.
  const std::string kId("id");
  mojo::PendingRemote<blink::mojom::Blob> pending_blob_remote;
  BlobImpl::Create(SetupBasicBlob(kId),
                   pending_blob_remote.InitWithNewPipeAndPassReceiver());

  // Now register a url for that blob.
  GURL kUrl("blob:id");
  context_->RegisterPublicBlobURL(kUrl, std::move(pending_blob_remote));
  pending_blob_remote = context_->GetBlobFromPublicURL(kUrl);
  ASSERT_TRUE(pending_blob_remote);
  mojo::Remote<blink::mojom::Blob> blob_remote(std::move(pending_blob_remote));
  EXPECT_EQ(kId, UUIDFromBlob(blob_remote.get()));
  blob_remote.reset();
  base::RunLoop().RunUntilIdle();

  // The url registration should keep the blob alive even after
  // explicit references are dropped.
  pending_blob_remote = context_->GetBlobFromPublicURL(kUrl);
  EXPECT_TRUE(pending_blob_remote);
  pending_blob_remote.reset();

  base::RunLoop().RunUntilIdle();
  // Finally get rid of the url registration and the blob.
  context_->RevokePublicBlobURL(kUrl);
  pending_blob_remote = context_->GetBlobFromPublicURL(kUrl);
  EXPECT_FALSE(pending_blob_remote);
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(context_->registry().HasEntry(kId));
}

TEST_F(BlobStorageContextTest, TestUnknownBrokenAndBuildingBlobReference) {
  const std::string kBrokenId("broken_id");
  const std::string kBuildingId("building_id");
  const std::string kReferencingId("referencing_id");
  const std::string kUnknownId("unknown_id");

  // Create a broken blob.
  std::unique_ptr<BlobDataHandle> broken_handle =
      context_->AddBrokenBlob(kBrokenId, "", "", BlobStatus::ERR_OUT_OF_MEMORY);
  EXPECT_TRUE(broken_handle->GetBlobStatus() == BlobStatus::ERR_OUT_OF_MEMORY);
  EXPECT_TRUE(context_->registry().HasEntry(kBrokenId));

  // Try to create a blob with a reference to an unknown blob.
  auto builder = std::make_unique<BlobDataBuilder>(kReferencingId);
  builder->AppendData("data");
  builder->AppendBlob(kUnknownId, context_->registry());
  std::unique_ptr<BlobDataHandle> handle =
      context_->AddFinishedBlob(std::move(builder));
  EXPECT_TRUE(handle->IsBroken());
  EXPECT_TRUE(context_->registry().HasEntry(kReferencingId));
  handle.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(context_->registry().HasEntry(kReferencingId));

  // Try to create a blob with a reference to the broken blob.
  auto builder2 = std::make_unique<BlobDataBuilder>(kReferencingId);
  builder2->AppendData("data");
  builder2->AppendBlob(kBrokenId, context_->registry());
  handle = context_->AddFinishedBlob(std::move(builder2));
  EXPECT_TRUE(handle->IsBroken());
  EXPECT_TRUE(context_->registry().HasEntry(kReferencingId));
  handle.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(context_->registry().HasEntry(kReferencingId));

  // Try to create a blob with a reference to the building blob.
  auto builder3 = std::make_unique<BlobDataBuilder>(kReferencingId);
  builder3->AppendData("data");
  builder3->AppendBlob(kBuildingId, context_->registry());
  handle = context_->AddFinishedBlob(std::move(builder3));
  EXPECT_TRUE(handle->IsBroken());
  EXPECT_TRUE(context_->registry().HasEntry(kReferencingId));
  handle.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(context_->registry().HasEntry(kReferencingId));
}

namespace {
constexpr size_t kTotalRawBlobs = 200;
constexpr size_t kTotalSlicedBlobs = 100;
constexpr char kTestDataHandleData[] = "Test Blob Data";

// Appends data and data types that depend on the index. This is designed to
// exercise all types of combinations of data, future data, files, future files,
// and disk cache entries.
size_t AppendDataInBuilder(
    BlobDataBuilder* builder,
    std::vector<BlobDataBuilder::FutureData>* future_datas,
    std::vector<BlobDataBuilder::FutureFile>* future_files,
    size_t index,
    scoped_refptr<storage::BlobDataItem::DataHandle> data_handle) {
  size_t size = 0;
  // We can't have both future data and future files, so split those up.
  if (index % 2 != 0) {
    future_datas->emplace_back(builder->AppendFutureData(5u));
    size += 5u;
    if (index % 3 == 1) {
      builder->AppendData("abcd");
      size += 4u;
    }
    if (index % 3 == 0) {
      future_datas->emplace_back(builder->AppendFutureData(1u));
      size += 1u;
    }
  } else if (index % 3 == 0) {
    future_files->emplace_back(builder->AppendFutureFile(0lu, 3lu, 0));
    size += 3u;
  }
  if (index % 5 != 0) {
    builder->AppendFile(
        base::FilePath::FromUTF8Unsafe(base::NumberToString(index)), 0ul, 20ul,
        base::Time::Max());
    size += 20u;
  }
  if (index % 3 != 0) {
    builder->AppendReadableDataHandle(data_handle);
    size += strlen(kTestDataHandleData);
  }
  return size;
}

bool DoesBuilderHaveFutureData(size_t index) {
  return index < kTotalRawBlobs && (index % 2 != 0 || index % 3 == 0);
}

void PopulateDataInBuilder(
    std::vector<BlobDataBuilder::FutureData>* future_datas,
    std::vector<BlobDataBuilder::FutureFile>* future_files,
    size_t index,
    base::TaskRunner* file_runner) {
  if (index % 2 != 0) {
    (*future_datas)[0].Populate(base::as_bytes(base::make_span("abcde", 5)), 0);
    if (index % 3 == 0) {
      (*future_datas)[1].Populate(base::as_bytes(base::make_span("1", 1)), 0);
    }
  } else if (index % 3 == 0) {
    scoped_refptr<ShareableFileReference> file_ref =
        ShareableFileReference::GetOrCreate(
            base::FilePath::FromUTF8Unsafe(
                base::NumberToString(index + kTotalRawBlobs)),
            ShareableFileReference::DONT_DELETE_ON_FINAL_RELEASE, file_runner);
    (*future_files)[0].Populate(file_ref, base::Time::Max());
  }
}
}  // namespace

TEST_F(BlobStorageContextTest, BuildBlobCombinations) {
  const std::string kId("id");

  context_ =
      std::make_unique<BlobStorageContext>(temp_dir_.GetPath(), file_runner_);

  SetTestMemoryLimits();
  auto data_handle = base::MakeRefCounted<storage::FakeBlobDataHandle>(
      kTestDataHandleData, "");

  // This tests mixed blob content with both synchronous and asynchronous
  // construction. Blobs should also be paged to disk during execution.
  std::vector<std::unique_ptr<BlobDataBuilder>> builders;
  std::vector<std::vector<BlobDataBuilder::FutureData>> future_datas;
  std::vector<std::vector<BlobDataBuilder::FutureFile>> future_files;
  std::vector<size_t> sizes;
  for (size_t i = 0; i < kTotalRawBlobs; i++) {
    builders.emplace_back(new BlobDataBuilder(base::NumberToString(i)));
    future_datas.emplace_back();
    future_files.emplace_back();
    auto& builder = *builders.back();
    size_t size = AppendDataInBuilder(&builder, &future_datas.back(),
                                      &future_files.back(), i, data_handle);
    EXPECT_NE(0u, size);
    sizes.push_back(size);
  }

  size_t total_finished_blobs = 0;
  std::vector<std::unique_ptr<BlobDataHandle>> handles;
  std::vector<BlobStatus> statuses;
  std::vector<bool> populated;
  statuses.resize(kTotalRawBlobs,
                  BlobStatus::ERR_INVALID_CONSTRUCTION_ARGUMENTS);
  populated.resize(kTotalRawBlobs, false);
  for (size_t i = 0; i < builders.size(); i++) {
    std::unique_ptr<BlobDataBuilder> builder = std::move(builders[i]);
    builder->set_content_type("text/plain");
    bool has_pending_memory = DoesBuilderHaveFutureData(i);
    std::unique_ptr<BlobDataHandle> handle = context_->BuildBlob(
        std::move(builder),
        has_pending_memory
            ? base::BindOnce(&SaveBlobStatusAndFiles, &statuses[0] + i, &files_)
            : BlobStorageContext::TransportAllowedCallback());
    handle->RunOnConstructionComplete(
        base::BindOnce(&IncrementNumber, &total_finished_blobs));
    handles.push_back(std::move(handle));
  }
  builders.clear();

  for (size_t i = 0; i < kTotalSlicedBlobs; i++) {
    auto builder = std::make_unique<BlobDataBuilder>(
        base::NumberToString(i + kTotalRawBlobs));
    size_t source_size = sizes[i];
    size_t offset = source_size == 1 ? 0 : i % (source_size - 1);
    size_t size = (i % (source_size - offset)) + 1;
    builder->AppendBlob(base::NumberToString(i), offset, size,
                        context_->registry());
    builder->set_content_type("text/plain");
    std::unique_ptr<BlobDataHandle> handle = context_->BuildBlob(
        std::move(builder), BlobStorageContext::TransportAllowedCallback());
    handle->RunOnConstructionComplete(
        base::BindOnce(&IncrementNumber, &total_finished_blobs));
    handles.push_back(std::move(handle));
  }

  base::RunLoop().RunUntilIdle();

  // We should be needing to send a page or two to disk.
  EXPECT_TRUE(file_runner_->HasPendingTask());
  do {
    file_runner_->RunPendingTasks();
    base::RunLoop().RunUntilIdle();
    // Continue populating data for items that can fit.
    for (size_t i = 0; i < kTotalRawBlobs; i++) {
      if (DoesBuilderHaveFutureData(i) && !populated[i] &&
          statuses[i] == BlobStatus::PENDING_TRANSPORT) {
        PopulateDataInBuilder(&future_datas[i], &future_files[i], i,
                              file_runner_.get());
        context_->NotifyTransportComplete(base::NumberToString(i));
        populated[i] = true;
      }
    }
    base::RunLoop().RunUntilIdle();
  } while (file_runner_->HasPendingTask());

  // Check all builders with future items were signaled and populated.
  for (size_t i = 0; i < populated.size(); i++) {
    if (DoesBuilderHaveFutureData(i)) {
      EXPECT_EQ(BlobStatus::PENDING_TRANSPORT, statuses[i]) << i;
      EXPECT_TRUE(populated[i]) << i;
    }
  }
  base::RunLoop().RunUntilIdle();

  // We should be completely built now.
  EXPECT_EQ(kTotalRawBlobs + kTotalSlicedBlobs, total_finished_blobs);
  for (std::unique_ptr<BlobDataHandle>& handle : handles) {
    EXPECT_EQ(BlobStatus::DONE, handle->GetBlobStatus());
  }
  handles.clear();
  base::RunLoop().RunUntilIdle();
  files_.clear();
  // We should have file cleanup tasks.
  EXPECT_TRUE(file_runner_->HasPendingTask());
  file_runner_->RunPendingTasks();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0lu, context_->memory_controller().memory_usage());
  EXPECT_EQ(0lu, context_->memory_controller().disk_usage());
}

TEST_F(BlobStorageContextTest, NegativeSlice) {
  const std::string kId1("id1");
  const std::string kId2("id2");

  std::unique_ptr<BlobDataHandle> handle = SetupBasicBlob(kId1);

  EXPECT_EQ(1lu, context_->memory_controller().memory_usage());

  auto builder = std::make_unique<BlobDataBuilder>(kId2);
  builder->AppendBlob(kId1, static_cast<uint64_t>(-10), 11,
                      context_->registry());
  std::unique_ptr<BlobDataHandle> handle2 = context_->BuildBlob(
      std::move(builder), BlobStorageContext::TransportAllowedCallback());

  EXPECT_TRUE(handle2->IsBroken());
  EXPECT_EQ(BlobStatus::ERR_INVALID_CONSTRUCTION_ARGUMENTS,
            handle2->GetBlobStatus());
}

// TODO(michaeln): tests for the deprecated url stuff

}  // namespace storage
