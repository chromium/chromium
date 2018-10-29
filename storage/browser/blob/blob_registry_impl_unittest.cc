// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/blob/blob_registry_impl.h"

#include <limits>
#include <memory>

#include "base/files/scoped_temp_dir.h"
#include "base/rand_util.h"
#include "base/run_loop.h"
#include "base/sequenced_task_runner.h"
#include "base/task/post_task.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_task_environment.h"
#include "base/threading/thread_restrictions.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "mojo/public/cpp/system/data_pipe_utils.h"
#include "storage/browser/blob/blob_data_builder.h"
#include "storage/browser/blob/blob_data_handle.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "storage/browser/test/fake_blob.h"
#include "storage/browser/test/fake_progress_client.h"
#include "storage/browser/test/mock_blob_registry_delegate.h"
#include "storage/browser/test/mock_bytes_provider.h"
#include "storage/browser/test/mock_special_storage_policy.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace storage {

namespace {

const size_t kTestBlobStorageIPCThresholdBytes = 5;
const size_t kTestBlobStorageMaxSharedMemoryBytes = 20;
const size_t kTestBlobStorageMaxBytesDataItemSize = 23;

const size_t kTestBlobStorageMaxBlobMemorySize = 400;
const uint64_t kTestBlobStorageMaxDiskSpace = 4000;
const uint64_t kTestBlobStorageMinFileSizeBytes = 10;
const uint64_t kTestBlobStorageMaxFileSizeBytes = 100;

void BindBytesProvider(std::unique_ptr<MockBytesProvider> impl,
                       blink::mojom::BytesProviderRequest request) {
  mojo::MakeStrongBinding(std::move(impl), std::move(request));
}

}  // namespace

class BlobRegistryImplTest : public testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(data_dir_.CreateUniqueTempDir());
    context_ = std::make_unique<BlobStorageContext>(
        data_dir_.GetPath(),
        base::CreateTaskRunnerWithTraits({base::MayBlock()}));
    auto storage_policy =
        base::MakeRefCounted<content::MockSpecialStoragePolicy>();
    file_system_context_ = base::MakeRefCounted<storage::FileSystemContext>(
        base::ThreadTaskRunnerHandle::Get().get(),
        base::ThreadTaskRunnerHandle::Get().get(),
        nullptr /* external_mount_points */, storage_policy.get(),
        nullptr /* quota_manager_proxy */,
        std::vector<std::unique_ptr<FileSystemBackend>>(),
        std::vector<URLRequestAutoMountHandler>(), data_dir_.GetPath(),
        FileSystemOptions(FileSystemOptions::PROFILE_MODE_INCOGNITO,
                          false /* force_in_memory */,
                          std::vector<std::string>()));
    registry_impl_ = std::make_unique<BlobRegistryImpl>(context_->AsWeakPtr(),
                                                        file_system_context_);
    auto delegate = std::make_unique<MockBlobRegistryDelegate>();
    delegate_ptr_ = delegate.get();
    registry_impl_->Bind(MakeRequest(&registry_), std::move(delegate));

    mojo::core::SetDefaultProcessErrorCallback(base::BindRepeating(
        &BlobRegistryImplTest::OnBadMessage, base::Unretained(this)));

    storage::BlobStorageLimits limits;
    limits.max_ipc_memory_size = kTestBlobStorageIPCThresholdBytes;
    limits.max_shared_memory_size = kTestBlobStorageMaxSharedMemoryBytes;
    limits.max_bytes_data_item_size = kTestBlobStorageMaxBytesDataItemSize;
    limits.max_blob_in_memory_space = kTestBlobStorageMaxBlobMemorySize;
    limits.desired_max_disk_space = kTestBlobStorageMaxDiskSpace;
    limits.effective_max_disk_space = kTestBlobStorageMaxDiskSpace;
    limits.min_page_file_size = kTestBlobStorageMinFileSizeBytes;
    limits.max_file_size = kTestBlobStorageMaxFileSizeBytes;
    context_->mutable_memory_controller()->set_limits_for_testing(limits);

    // Disallow IO on the main loop.
    base::ThreadRestrictions::SetIOAllowed(false);
  }

  void TearDown() override {
    base::ThreadRestrictions::SetIOAllowed(true);

    mojo::core::SetDefaultProcessErrorCallback(
        mojo::core::ProcessErrorCallback());
  }

  std::unique_ptr<BlobDataHandle> CreateBlobFromString(
      const std::string& uuid,
      const std::string& contents) {
    auto builder = std::make_unique<BlobDataBuilder>(uuid);
    builder->set_content_type("text/plain");
    builder->AppendData(contents);
    return context_->AddFinishedBlob(std::move(builder));
  }

  std::string UUIDFromBlob(blink::mojom::Blob* blob) {
    base::RunLoop loop;
    std::string received_uuid;
    blob->GetInternalUUID(base::BindOnce(
        [](base::Closure quit_closure, std::string* uuid_out,
           const std::string& uuid) {
          *uuid_out = uuid;
          quit_closure.Run();
        },
        loop.QuitClosure(), &received_uuid));
    loop.Run();
    return received_uuid;
  }

  void OnBadMessage(const std::string& error) {
    bad_messages_.push_back(error);
  }

  void WaitForBlobCompletion(BlobDataHandle* blob_handle) {
    base::RunLoop loop;
    blob_handle->RunOnConstructionComplete(base::BindOnce(
        [](const base::Closure& closure, BlobStatus status) { closure.Run(); },
        loop.QuitClosure()));
    loop.Run();
  }

  blink::mojom::BytesProviderPtrInfo CreateBytesProvider(
      const std::string& bytes) {
    if (!bytes_provider_runner_) {
      bytes_provider_runner_ =
          base::CreateSequencedTaskRunnerWithTraits({base::MayBlock()});
    }
    blink::mojom::BytesProviderPtrInfo result;
    auto provider = std::make_unique<MockBytesProvider>(
        std::vector<uint8_t>(bytes.begin(), bytes.end()), &reply_request_count_,
        &stream_request_count_, &file_request_count_);
    bytes_provider_runner_->PostTask(
        FROM_HERE, base::BindOnce(&BindBytesProvider, std::move(provider),
                                  MakeRequest(&result)));
    return result;
  }

  void CreateBytesProvider(const std::string& bytes,
                           blink::mojom::BytesProviderRequest request) {
    if (!bytes_provider_runner_) {
      bytes_provider_runner_ =
          base::CreateSequencedTaskRunnerWithTraits({base::MayBlock()});
    }
    auto provider = std::make_unique<MockBytesProvider>(
        std::vector<uint8_t>(bytes.begin(), bytes.end()), &reply_request_count_,
        &stream_request_count_, &file_request_count_);
    bytes_provider_runner_->PostTask(
        FROM_HERE, base::BindOnce(&BindBytesProvider, std::move(provider),
                                  std::move(request)));
  }

  size_t BlobsUnderConstruction() {
    return registry_impl_->BlobsUnderConstructionForTesting();
  }

 protected:
  base::ScopedTempDir data_dir_;
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  std::unique_ptr<BlobStorageContext> context_;
  scoped_refptr<storage::FileSystemContext> file_system_context_;
  std::unique_ptr<BlobRegistryImpl> registry_impl_;
  blink::mojom::BlobRegistryPtr registry_;
  MockBlobRegistryDelegate* delegate_ptr_;
  scoped_refptr<base::SequencedTaskRunner> bytes_provider_runner_;

  size_t reply_request_count_ = 0;
  size_t stream_request_count_ = 0;
  size_t file_request_count_ = 0;

  std::vector<std::string> bad_messages_;
};

TEST_F(BlobRegistryImplTest, GetBlobFromUUID) {
  const std::string kId = "id";
  std::unique_ptr<BlobDataHandle> handle =
      CreateBlobFromString(kId, "hello world");

  {
    blink::mojom::BlobPtr blob;
    registry_->GetBlobFromUUID(MakeRequest(&blob), kId);
    EXPECT_EQ(kId, UUIDFromBlob(blob.get()));
    EXPECT_FALSE(blob.encountered_error());
  }

  {
    blink::mojom::BlobPtr blob;
    registry_->GetBlobFromUUID(MakeRequest(&blob), "invalid id");
    blob.FlushForTesting();
    EXPECT_TRUE(blob.encountered_error());
  }
}

TEST_F(BlobRegistryImplTest, GetBlobFromEmptyUUID) {
  blink::mojom::BlobPtr blob;
  registry_->GetBlobFromUUID(MakeRequest(&blob), "");
  blob.FlushForTesting();
  EXPECT_EQ(1u, bad_messages_.size());
  EXPECT_TRUE(blob.encountered_error());
}

TEST_F(BlobRegistryImplTest, Register_EmptyUUID) {
  blink::mojom::BlobPtr blob;
  EXPECT_FALSE(
      registry_->Register(MakeRequest(&blob), "", "", "",
                          std::vector<blink::mojom::DataElementPtr>()));

  EXPECT_EQ(1u, bad_messages_.size());

  registry_.FlushForTesting();
  EXPECT_TRUE(registry_.encountered_error());

  blob.FlushForTesting();
  EXPECT_TRUE(blob.encountered_error());
  EXPECT_EQ(0u, BlobsUnderConstruction());
}

TEST_F(BlobRegistryImplTest, Register_ExistingUUID) {
  const std::string kId = "id";
  std::unique_ptr<BlobDataHandle> handle =
      CreateBlobFromString(kId, "hello world");

  blink::mojom::BlobPtr blob;
  EXPECT_FALSE(
      registry_->Register(MakeRequest(&blob), kId, "", "",
                          std::vector<blink::mojom::DataElementPtr>()));

  EXPECT_EQ(1u, bad_messages_.size());

  registry_.FlushForTesting();
  EXPECT_TRUE(registry_.encountered_error());

  blob.FlushForTesting();
  EXPECT_TRUE(blob.encountered_error());
  EXPECT_EQ(0u, BlobsUnderConstruction());
}

TEST_F(BlobRegistryImplTest, Register_EmptyBlob) {
  const std::string kId = "id";
  const std::string kContentType = "content/type";
  const std::string kContentDisposition = "disposition";

  blink::mojom::BlobPtr blob;
  EXPECT_TRUE(registry_->Register(MakeRequest(&blob), kId, kContentType,
                                  kContentDisposition,
                                  std::vector<blink::mojom::DataElementPtr>()));

  EXPECT_TRUE(bad_messages_.empty());

  EXPECT_EQ(kId, UUIDFromBlob(blob.get()));
  EXPECT_TRUE(context_->registry().HasEntry(kId));
  std::unique_ptr<BlobDataHandle> handle = context_->GetBlobDataFromUUID(kId);
  EXPECT_EQ(kContentType, handle->content_type());
  EXPECT_EQ(kContentDisposition, handle->content_disposition());
  EXPECT_EQ(0u, handle->size());

  WaitForBlobCompletion(handle.get());

  EXPECT_FALSE(handle->IsBroken());
  EXPECT_EQ(BlobStatus::DONE, handle->GetBlobStatus());
  EXPECT_EQ(0u, BlobsUnderConstruction());
}

TEST_F(BlobRegistryImplTest, Register_ReferencedBlobClosedPipe) {
  const std::string kId = "id";

  std::vector<blink::mojom::DataElementPtr> elements;
  blink::mojom::BlobPtrInfo referenced_blob_info;
  MakeRequest(&referenced_blob_info);
  elements.push_back(
      blink::mojom::DataElement::NewBlob(blink::mojom::DataElementBlob::New(
          std::move(referenced_blob_info), 0, 16)));

  blink::mojom::BlobPtr blob;
  EXPECT_TRUE(registry_->Register(MakeRequest(&blob), kId, "", "",
                                  std::move(elements)));
  EXPECT_TRUE(bad_messages_.empty());

  std::unique_ptr<BlobDataHandle> handle = context_->GetBlobDataFromUUID(kId);
  WaitForBlobCompletion(handle.get());

  EXPECT_TRUE(handle->IsBroken());
  EXPECT_EQ(BlobStatus::ERR_REFERENCED_BLOB_BROKEN, handle->GetBlobStatus());
  EXPECT_EQ(0u, BlobsUnderConstruction());
}

TEST_F(BlobRegistryImplTest, Register_SelfReference) {
  const std::string kId = "id";

  blink::mojom::BlobPtrInfo blob_info;
  blink::mojom::BlobRequest blob_request = MakeRequest(&blob_info);

  std::vector<blink::mojom::DataElementPtr> elements;
  elements.push_back(blink::mojom::DataElement::NewBlob(
      blink::mojom::DataElementBlob::New(std::move(blob_info), 0, 16)));

  EXPECT_TRUE(registry_->Register(std::move(blob_request), kId, "", "",
                                  std::move(elements)));
  EXPECT_TRUE(bad_messages_.empty());

  std::unique_ptr<BlobDataHandle> handle = context_->GetBlobDataFromUUID(kId);
  WaitForBlobCompletion(handle.get());

  EXPECT_TRUE(handle->IsBroken());
  EXPECT_EQ(BlobStatus::ERR_INVALID_CONSTRUCTION_ARGUMENTS,
            handle->GetBlobStatus());

  EXPECT_EQ(1u, bad_messages_.size());

  registry_.FlushForTesting();
  EXPECT_TRUE(registry_.encountered_error());
  EXPECT_EQ(0u, BlobsUnderConstruction());
}

TEST_F(BlobRegistryImplTest, Register_CircularReference) {
  const std::string kId1 = "id1";
  const std::string kId2 = "id2";
  const std::string kId3 = "id3";

  blink::mojom::BlobPtrInfo blob1_info, blob2_info, blob3_info;
  blink::mojom::BlobRequest blob_request1 = MakeRequest(&blob1_info);
  blink::mojom::BlobRequest blob_request2 = MakeRequest(&blob2_info);
  blink::mojom::BlobRequest blob_request3 = MakeRequest(&blob3_info);

  std::vector<blink::mojom::DataElementPtr> elements1;
  elements1.push_back(blink::mojom::DataElement::NewBlob(
      blink::mojom::DataElementBlob::New(std::move(blob1_info), 0, 16)));

  std::vector<blink::mojom::DataElementPtr> elements2;
  elements2.push_back(blink::mojom::DataElement::NewBlob(
      blink::mojom::DataElementBlob::New(std::move(blob2_info), 0, 16)));

  std::vector<blink::mojom::DataElementPtr> elements3;
  elements3.push_back(blink::mojom::DataElement::NewBlob(
      blink::mojom::DataElementBlob::New(std::move(blob3_info), 0, 16)));

  EXPECT_TRUE(registry_->Register(std::move(blob_request1), kId1, "", "",
                                  std::move(elements2)));
  EXPECT_TRUE(registry_->Register(std::move(blob_request2), kId2, "", "",
                                  std::move(elements3)));
  EXPECT_TRUE(registry_->Register(std::move(blob_request3), kId3, "", "",
                                  std::move(elements1)));
  EXPECT_TRUE(bad_messages_.empty());

#if DCHECK_IS_ON()
  // Without DCHECKs on this will just hang forever.
  std::unique_ptr<BlobDataHandle> handle1 = context_->GetBlobDataFromUUID(kId1);
  std::unique_ptr<BlobDataHandle> handle2 = context_->GetBlobDataFromUUID(kId2);
  std::unique_ptr<BlobDataHandle> handle3 = context_->GetBlobDataFromUUID(kId3);
  WaitForBlobCompletion(handle1.get());
  WaitForBlobCompletion(handle2.get());
  WaitForBlobCompletion(handle3.get());

  EXPECT_TRUE(handle1->IsBroken());
  EXPECT_TRUE(handle2->IsBroken());
  EXPECT_TRUE(handle3->IsBroken());

  BlobStatus status1 = handle1->GetBlobStatus();
  BlobStatus status2 = handle2->GetBlobStatus();
  BlobStatus status3 = handle3->GetBlobStatus();
  EXPECT_TRUE(status1 == BlobStatus::ERR_INVALID_CONSTRUCTION_ARGUMENTS ||
              status1 == BlobStatus::ERR_REFERENCED_BLOB_BROKEN);
  EXPECT_TRUE(status2 == BlobStatus::ERR_INVALID_CONSTRUCTION_ARGUMENTS ||
              status2 == BlobStatus::ERR_REFERENCED_BLOB_BROKEN);
  EXPECT_TRUE(status3 == BlobStatus::ERR_INVALID_CONSTRUCTION_ARGUMENTS ||
              status3 == BlobStatus::ERR_REFERENCED_BLOB_BROKEN);
  EXPECT_EQ((status1 == BlobStatus::ERR_INVALID_CONSTRUCTION_ARGUMENTS) +
                (status2 == BlobStatus::ERR_INVALID_CONSTRUCTION_ARGUMENTS) +
                (status3 == BlobStatus::ERR_INVALID_CONSTRUCTION_ARGUMENTS),
            1);

  EXPECT_EQ(1u, bad_messages_.size());

  registry_.FlushForTesting();
  EXPECT_TRUE(registry_.encountered_error());
  EXPECT_EQ(0u, BlobsUnderConstruction());
#endif
}

TEST_F(BlobRegistryImplTest, Register_NonExistentBlob) {
  const std::string kId = "id";

  std::vector<blink::mojom::DataElementPtr> elements;
  blink::mojom::BlobPtrInfo referenced_blob_info;
  mojo::MakeStrongBinding(std::make_unique<FakeBlob>("mock blob"),
                          MakeRequest(&referenced_blob_info));
  elements.push_back(
      blink::mojom::DataElement::NewBlob(blink::mojom::DataElementBlob::New(
          std::move(referenced_blob_info), 0, 16)));

  blink::mojom::BlobPtr blob;
  EXPECT_TRUE(registry_->Register(MakeRequest(&blob), kId, "", "",
                                  std::move(elements)));
  EXPECT_TRUE(bad_messages_.empty());

  std::unique_ptr<BlobDataHandle> handle = context_->GetBlobDataFromUUID(kId);
  WaitForBlobCompletion(handle.get());

  EXPECT_TRUE(handle->IsBroken());
  EXPECT_EQ(BlobStatus::ERR_INVALID_CONSTRUCTION_ARGUMENTS,
            handle->GetBlobStatus());

  EXPECT_EQ(1u, bad_messages_.size());

  registry_.FlushForTesting();
  EXPECT_TRUE(registry_.encountered_error());
  EXPECT_EQ(0u, BlobsUnderConstruction());
}

TEST_F(BlobRegistryImplTest, Register_ValidBlobReferences) {
  const std::string kId1 = "id1";
  std::unique_ptr<BlobDataHandle> handle =
      CreateBlobFromString(kId1, "hello world");
  blink::mojom::BlobPtrInfo blob1_info;
  mojo::MakeStrongBinding(std::make_unique<FakeBlob>(kId1),
                          MakeRequest(&blob1_info));

  const std::string kId2 = "id2";
  blink::mojom::BlobPtrInfo blob2_info;
  blink::mojom::BlobRequest blob_request2 = MakeRequest(&blob2_info);

  std::vector<blink::mojom::DataElementPtr> elements1;
  elements1.push_back(blink::mojom::DataElement::NewBlob(
      blink::mojom::DataElementBlob::New(std::move(blob1_info), 0, 8)));

  std::vector<blink::mojom::DataElementPtr> elements2;
  elements2.push_back(blink::mojom::DataElement::NewBlob(
      blink::mojom::DataElementBlob::New(std::move(blob2_info), 0, 8)));

  blink::mojom::BlobPtr final_blob;
  const std::string kId3 = "id3";
  EXPECT_TRUE(registry_->Register(MakeRequest(&final_blob), kId3, "", "",
                                  std::move(elements2)));
  EXPECT_TRUE(registry_->Register(std::move(blob_request2), kId2, "", "",
                                  std::move(elements1)));

  // kId3 references kId2, kId2 reference kId1, kId1 is a simple string.
  std::unique_ptr<BlobDataHandle> handle2 = context_->GetBlobDataFromUUID(kId2);
  std::unique_ptr<BlobDataHandle> handle3 = context_->GetBlobDataFromUUID(kId3);
  WaitForBlobCompletion(handle2.get());
  WaitForBlobCompletion(handle3.get());

  EXPECT_FALSE(handle2->IsBroken());
  ASSERT_EQ(BlobStatus::DONE, handle2->GetBlobStatus());

  EXPECT_FALSE(handle3->IsBroken());
  ASSERT_EQ(BlobStatus::DONE, handle3->GetBlobStatus());

  BlobDataBuilder expected_blob_data(kId2);
  expected_blob_data.AppendData("hello wo");

  EXPECT_EQ(expected_blob_data, *handle2->CreateSnapshot());
  EXPECT_EQ(expected_blob_data, *handle3->CreateSnapshot());
  EXPECT_EQ(0u, BlobsUnderConstruction());
}

TEST_F(BlobRegistryImplTest, Register_UnreadableFile) {
  delegate_ptr_->can_read_file_result = false;

  const std::string kId = "id";

  std::vector<blink::mojom::DataElementPtr> elements;
  elements.push_back(
      blink::mojom::DataElement::NewFile(blink::mojom::DataElementFile::New(
          base::FilePath(FILE_PATH_LITERAL("foobar")), 0, 16, base::nullopt)));

  blink::mojom::BlobPtr blob;
  EXPECT_TRUE(registry_->Register(MakeRequest(&blob), kId, "", "",
                                  std::move(elements)));
  EXPECT_TRUE(bad_messages_.empty());

  std::unique_ptr<BlobDataHandle> handle = context_->GetBlobDataFromUUID(kId);
  WaitForBlobCompletion(handle.get());

  EXPECT_TRUE(handle->IsBroken());
  EXPECT_EQ(BlobStatus::ERR_REFERENCED_FILE_UNAVAILABLE,
            handle->GetBlobStatus());
  EXPECT_EQ(0u, BlobsUnderConstruction());
}

TEST_F(BlobRegistryImplTest, Register_ValidFile) {
  delegate_ptr_->can_read_file_result = true;

  const std::string kId = "id";
  const base::FilePath path(FILE_PATH_LITERAL("foobar"));

  std::vector<blink::mojom::DataElementPtr> elements;
  elements.push_back(blink::mojom::DataElement::NewFile(
      blink::mojom::DataElementFile::New(path, 0, 16, base::nullopt)));

  blink::mojom::BlobPtr blob;
  EXPECT_TRUE(registry_->Register(MakeRequest(&blob), kId, "", "",
                                  std::move(elements)));
  EXPECT_TRUE(bad_messages_.empty());

  std::unique_ptr<BlobDataHandle> handle = context_->GetBlobDataFromUUID(kId);
  WaitForBlobCompletion(handle.get());

  EXPECT_FALSE(handle->IsBroken());
  ASSERT_EQ(BlobStatus::DONE, handle->GetBlobStatus());

  BlobDataBuilder expected_blob_data(kId);
  expected_blob_data.AppendFile(path, 0, 16, base::Time());

  EXPECT_EQ(expected_blob_data, *handle->CreateSnapshot());
  EXPECT_EQ(0u, BlobsUnderConstruction());
}

TEST_F(BlobRegistryImplTest, Register_FileSystemFile_InvalidScheme) {
  const std::string kId = "id";

  std::vector<blink::mojom::DataElementPtr> elements;
  elements.push_back(blink::mojom::DataElement::NewFileFilesystem(
      blink::mojom::DataElementFilesystemURL::New(GURL("http://foobar.com/"), 0,
                                                  16, base::nullopt)));

  blink::mojom::BlobPtr blob;
  EXPECT_TRUE(registry_->Register(MakeRequest(&blob), kId, "", "",
                                  std::move(elements)));
  EXPECT_TRUE(bad_messages_.empty());

  std::unique_ptr<BlobDataHandle> handle = context_->GetBlobDataFromUUID(kId);
  WaitForBlobCompletion(handle.get());

  EXPECT_TRUE(handle->IsBroken());
  EXPECT_EQ(BlobStatus::ERR_REFERENCED_FILE_UNAVAILABLE,
            handle->GetBlobStatus());
  EXPECT_EQ(0u, BlobsUnderConstruction());
}

TEST_F(BlobRegistryImplTest, Register_FileSystemFile_UnreadablFile) {
  delegate_ptr_->can_read_file_system_file_result = false;

  const std::string kId = "id";
  const GURL url("filesystem:http://example.com/temporary/myfile.png");

  std::vector<blink::mojom::DataElementPtr> elements;
  elements.push_back(blink::mojom::DataElement::NewFileFilesystem(
      blink::mojom::DataElementFilesystemURL::New(url, 0, 16, base::nullopt)));

  blink::mojom::BlobPtr blob;
  EXPECT_TRUE(registry_->Register(MakeRequest(&blob), kId, "", "",
                                  std::move(elements)));
  EXPECT_TRUE(bad_messages_.empty());

  std::unique_ptr<BlobDataHandle> handle = context_->GetBlobDataFromUUID(kId);
  WaitForBlobCompletion(handle.get());

  EXPECT_TRUE(handle->IsBroken());
  EXPECT_EQ(BlobStatus::ERR_REFERENCED_FILE_UNAVAILABLE,
            handle->GetBlobStatus());
  EXPECT_EQ(0u, BlobsUnderConstruction());
}

TEST_F(BlobRegistryImplTest, Register_FileSystemFile_Valid) {
  delegate_ptr_->can_read_file_system_file_result = true;

  const std::string kId = "id";
  const GURL url("filesystem:http://example.com/temporary/myfile.png");

  std::vector<blink::mojom::DataElementPtr> elements;
  elements.push_back(blink::mojom::DataElement::NewFileFilesystem(
      blink::mojom::DataElementFilesystemURL::New(url, 0, 16, base::nullopt)));

  blink::mojom::BlobPtr blob;
  EXPECT_TRUE(registry_->Register(MakeRequest(&blob), kId, "", "",
                                  std::move(elements)));
  EXPECT_TRUE(bad_messages_.empty());

  std::unique_ptr<BlobDataHandle> handle = context_->GetBlobDataFromUUID(kId);
  WaitForBlobCompletion(handle.get());

  EXPECT_FALSE(handle->IsBroken());
  ASSERT_EQ(BlobStatus::DONE, handle->GetBlobStatus());

  BlobDataBuilder expected_blob_data(kId);
  expected_blob_data.AppendFileSystemFile(url, 0, 16, base::Time(), nullptr);

  EXPECT_EQ(expected_blob_data, *handle->CreateSnapshot());
  EXPECT_EQ(0u, BlobsUnderConstruction());
}

TEST_F(BlobRegistryImplTest, Register_BytesInvalidEmbeddedData) {
  const std::string kId = "id";

  std::vector<blink::mojom::DataElementPtr> elements;
  elements.push_back(
      blink::mojom::DataElement::NewBytes(blink::mojom::DataElementBytes::New(
          10, std::vector<uint8_t>(5), CreateBytesProvider(""))));

  blink::mojom::BlobPtr blob;
  EXPECT_FALSE(registry_->Register(MakeRequest(&blob), kId, "", "",
                                   std::move(elements)));
  EXPECT_EQ(1u, bad_messages_.size());

  registry_.FlushForTesting();
  EXPECT_TRUE(registry_.encountered_error());

  std::unique_ptr<BlobDataHandle> handle = context_->GetBlobDataFromUUID(kId);
  WaitForBlobCompletion(handle.get());

  EXPECT_TRUE(handle->IsBroken());
  EXPECT_EQ(BlobStatus::ERR_INVALID_CONSTRUCTION_ARGUMENTS,
            handle->GetBlobStatus());

  EXPECT_EQ(0u, reply_request_count_);
  EXPECT_EQ(0u, stream_request_count_);
  EXPECT_EQ(0u, file_request_count_);
  EXPECT_EQ(0u, BlobsUnderConstruction());
}

TEST_F(BlobRegistryImplTest, Register_BytesInvalidDataSize) {
  const std::string kId = "id";

  // Two elements that together are more than uint64_t::max bytes.
  std::vector<blink::mojom::DataElementPtr> elements;
  elements.push_back(
      blink::mojom::DataElement::NewBytes(blink::mojom::DataElementBytes::New(
          8, base::nullopt, CreateBytesProvider(""))));
  elements.push_back(
      blink::mojom::DataElement::NewBytes(blink::mojom::DataElementBytes::New(
          std::numeric_limits<uint64_t>::max() - 4, base::nullopt,
          CreateBytesProvider(""))));

  blink::mojom::BlobPtr blob;
  EXPECT_FALSE(registry_->Register(MakeRequest(&blob), kId, "", "",
                                   std::move(elements)));
  EXPECT_EQ(1u, bad_messages_.size());

  registry_.FlushForTesting();
  EXPECT_TRUE(registry_.encountered_error());

  std::unique_ptr<BlobDataHandle> handle = context_->GetBlobDataFromUUID(kId);
  WaitForBlobCompletion(handle.get());

  EXPECT_TRUE(handle->IsBroken());
  EXPECT_EQ(BlobStatus::ERR_INVALID_CONSTRUCTION_ARGUMENTS,
            handle->GetBlobStatus());

  EXPECT_EQ(0u, reply_request_count_);
  EXPECT_EQ(0u, stream_request_count_);
  EXPECT_EQ(0u, file_request_count_);
  EXPECT_EQ(0u, BlobsUnderConstruction());
}

TEST_F(BlobRegistryImplTest, Register_BytesOutOfMemory) {
  const std::string kId = "id";

  // Two elements that together don't fit in the test quota.
  std::vector<blink::mojom::DataElementPtr> elements;
  elements.push_back(
      blink::mojom::DataElement::NewBytes(blink::mojom::DataElementBytes::New(
          kTestBlobStorageMaxDiskSpace, base::nullopt,
          CreateBytesProvider(""))));
  elements.push_back(
      blink::mojom::DataElement::NewBytes(blink::mojom::DataElementBytes::New(
          kTestBlobStorageMaxDiskSpace, base::nullopt,
          CreateBytesProvider(""))));

  blink::mojom::BlobPtr blob;
  EXPECT_TRUE(registry_->Register(MakeRequest(&blob), kId, "", "",
                                  std::move(elements)));

  std::unique_ptr<BlobDataHandle> handle = context_->GetBlobDataFromUUID(kId);
  WaitForBlobCompletion(handle.get());

  EXPECT_TRUE(handle->IsBroken());
  EXPECT_EQ(BlobStatus::ERR_OUT_OF_MEMORY, handle->GetBlobStatus());

  EXPECT_EQ(0u, reply_request_count_);
  EXPECT_EQ(0u, stream_request_count_);
  EXPECT_EQ(0u, file_request_count_);
  EXPECT_EQ(0u, BlobsUnderConstruction());
}

TEST_F(BlobRegistryImplTest, Register_ValidEmbeddedBytes) {
  const std::string kId = "id";
  const std::string kData = "hello world";

  std::vector<blink::mojom::DataElementPtr> elements;
  elements.push_back(
      blink::mojom::DataElement::NewBytes(blink::mojom::DataElementBytes::New(
          kData.size(), std::vector<uint8_t>(kData.begin(), kData.end()),
          CreateBytesProvider(kData))));

  blink::mojom::BlobPtr blob;
  EXPECT_TRUE(registry_->Register(MakeRequest(&blob), kId, "", "",
                                  std::move(elements)));

  std::unique_ptr<BlobDataHandle> handle = context_->GetBlobDataFromUUID(kId);
  WaitForBlobCompletion(handle.get());

  EXPECT_FALSE(handle->IsBroken());
  ASSERT_EQ(BlobStatus::DONE, handle->GetBlobStatus());

  BlobDataBuilder expected_blob_data(kId);
  expected_blob_data.AppendData(kData);

  EXPECT_EQ(expected_blob_data, *handle->CreateSnapshot());

  EXPECT_EQ(0u, reply_request_count_);
  EXPECT_EQ(0u, stream_request_count_);
  EXPECT_EQ(0u, file_request_count_);
  EXPECT_EQ(0u, BlobsUnderConstruction());
}

TEST_F(BlobRegistryImplTest, Register_ValidBytesAsReply) {
  const std::string kId = "id";
  const std::string kData = "hello";

  std::vector<blink::mojom::DataElementPtr> elements;
  elements.push_back(
      blink::mojom::DataElement::NewBytes(blink::mojom::DataElementBytes::New(
          kData.size(), base::nullopt, CreateBytesProvider(kData))));

  blink::mojom::BlobPtr blob;
  EXPECT_TRUE(registry_->Register(MakeRequest(&blob), kId, "", "",
                                  std::move(elements)));

  std::unique_ptr<BlobDataHandle> handle = context_->GetBlobDataFromUUID(kId);
  WaitForBlobCompletion(handle.get());

  EXPECT_FALSE(handle->IsBroken());
  ASSERT_EQ(BlobStatus::DONE, handle->GetBlobStatus());

  BlobDataBuilder expected_blob_data(kId);
  expected_blob_data.AppendData(kData);

  EXPECT_EQ(expected_blob_data, *handle->CreateSnapshot());

  EXPECT_EQ(1u, reply_request_count_);
  EXPECT_EQ(0u, stream_request_count_);
  EXPECT_EQ(0u, file_request_count_);
  EXPECT_EQ(0u, BlobsUnderConstruction());
}

TEST_F(BlobRegistryImplTest, Register_ValidBytesAsStream) {
  const std::string kId = "id";
  const std::string kData =
      base::RandBytesAsString(kTestBlobStorageMaxSharedMemoryBytes * 3 + 13);

  std::vector<blink::mojom::DataElementPtr> elements;
  elements.push_back(
      blink::mojom::DataElement::NewBytes(blink::mojom::DataElementBytes::New(
          kData.size(), base::nullopt, CreateBytesProvider(kData))));

  blink::mojom::BlobPtr blob;
  EXPECT_TRUE(registry_->Register(MakeRequest(&blob), kId, "", "",
                                  std::move(elements)));

  std::unique_ptr<BlobDataHandle> handle = context_->GetBlobDataFromUUID(kId);
  WaitForBlobCompletion(handle.get());

  EXPECT_FALSE(handle->IsBroken());
  ASSERT_EQ(BlobStatus::DONE, handle->GetBlobStatus());

  size_t offset = 0;
  BlobDataBuilder expected_blob_data(kId);
  while (offset < kData.size()) {
    expected_blob_data.AppendData(
        kData.substr(offset, kTestBlobStorageMaxBytesDataItemSize));
    offset += kTestBlobStorageMaxBytesDataItemSize;
  }

  EXPECT_EQ(expected_blob_data, *handle->CreateSnapshot());

  EXPECT_EQ(0u, reply_request_count_);
  EXPECT_EQ(1u, stream_request_count_);
  EXPECT_EQ(0u, file_request_count_);
  EXPECT_EQ(0u, BlobsUnderConstruction());
}

TEST_F(BlobRegistryImplTest, Register_ValidBytesAsFile) {
  const std::string kId = "id";
  const std::string kData =
      base::RandBytesAsString(kTestBlobStorageMaxBlobMemorySize + 42);

  std::vector<blink::mojom::DataElementPtr> elements;
  elements.push_back(
      blink::mojom::DataElement::NewBytes(blink::mojom::DataElementBytes::New(
          kData.size(), base::nullopt, CreateBytesProvider(kData))));

  blink::mojom::BlobPtr blob;
  EXPECT_TRUE(registry_->Register(MakeRequest(&blob), kId, "", "",
                                  std::move(elements)));

  std::unique_ptr<BlobDataHandle> handle = context_->GetBlobDataFromUUID(kId);
  WaitForBlobCompletion(handle.get());

  EXPECT_FALSE(handle->IsBroken());
  ASSERT_EQ(BlobStatus::DONE, handle->GetBlobStatus());

  BlobDataBuilder expected_blob_data(kId);
  expected_blob_data.AppendData(kData);

  size_t expected_file_count =
      1 + kData.size() / kTestBlobStorageMaxFileSizeBytes;
  EXPECT_EQ(0u, reply_request_count_);
  EXPECT_EQ(0u, stream_request_count_);
  EXPECT_EQ(expected_file_count, file_request_count_);

  auto snapshot = handle->CreateSnapshot();
  EXPECT_EQ(expected_file_count, snapshot->items().size());
  size_t remaining_size = kData.size();
  for (const auto& item : snapshot->items()) {
    EXPECT_EQ(BlobDataItem::Type::kFile, item->type());
    EXPECT_EQ(0u, item->offset());
    if (remaining_size > kTestBlobStorageMaxFileSizeBytes)
      EXPECT_EQ(kTestBlobStorageMaxFileSizeBytes, item->length());
    else
      EXPECT_EQ(remaining_size, item->length());
    remaining_size -= item->length();
  }
  EXPECT_EQ(0u, remaining_size);
  EXPECT_EQ(0u, BlobsUnderConstruction());
}

TEST_F(BlobRegistryImplTest, Register_BytesProviderClosedPipe) {
  const std::string kId = "id";

  blink::mojom::BytesProviderPtrInfo bytes_provider_info;
  MakeRequest(&bytes_provider_info);

  std::vector<blink::mojom::DataElementPtr> elements;
  elements.push_back(
      blink::mojom::DataElement::NewBytes(blink::mojom::DataElementBytes::New(
          32, base::nullopt, std::move(bytes_provider_info))));

  blink::mojom::BlobPtr blob;
  EXPECT_TRUE(registry_->Register(MakeRequest(&blob), kId, "", "",
                                  std::move(elements)));
  EXPECT_TRUE(bad_messages_.empty());

  std::unique_ptr<BlobDataHandle> handle = context_->GetBlobDataFromUUID(kId);
  WaitForBlobCompletion(handle.get());

  EXPECT_TRUE(handle->IsBroken());
  EXPECT_EQ(BlobStatus::ERR_SOURCE_DIED_IN_TRANSIT, handle->GetBlobStatus());
  EXPECT_EQ(0u, BlobsUnderConstruction());
}

TEST_F(BlobRegistryImplTest,
       Register_DefereferencedWhileBuildingBeforeBreaking) {
  const std::string kId = "id";

  blink::mojom::BytesProviderPtrInfo bytes_provider_info;
  auto request = MakeRequest(&bytes_provider_info);

  std::vector<blink::mojom::DataElementPtr> elements;
  elements.push_back(
      blink::mojom::DataElement::NewBytes(blink::mojom::DataElementBytes::New(
          32, base::nullopt, std::move(bytes_provider_info))));

  blink::mojom::BlobPtr blob;
  EXPECT_TRUE(registry_->Register(MakeRequest(&blob), kId, "", "",
                                  std::move(elements)));
  EXPECT_TRUE(bad_messages_.empty());

  EXPECT_TRUE(context_->registry().HasEntry(kId));
  EXPECT_TRUE(context_->GetBlobDataFromUUID(kId)->IsBeingBuilt());
  EXPECT_EQ(1u, BlobsUnderConstruction());

  // Now drop all references to the blob.
  blob.reset();
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(context_->registry().HasEntry(kId));

  // Now cause construction to fail, if it would still be going on.
  request = nullptr;
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0u, BlobsUnderConstruction());
}

TEST_F(BlobRegistryImplTest,
       Register_DefereferencedWhileBuildingBeforeResolvingDeps) {
  const std::string kId = "id";
  const std::string kData = "hello world";
  const std::string kDepId = "dep-id";

  // Create future blob.
  auto blob_handle = context_->AddFutureBlob(
      kDepId, "", "", BlobStorageContext::BuildAbortedCallback());
  blink::mojom::BlobPtrInfo referenced_blob_info;
  mojo::MakeStrongBinding(std::make_unique<FakeBlob>(kDepId),
                          MakeRequest(&referenced_blob_info));

  // Create mojo blob depending on future blob.
  std::vector<blink::mojom::DataElementPtr> elements;
  elements.push_back(
      blink::mojom::DataElement::NewBlob(blink::mojom::DataElementBlob::New(
          std::move(referenced_blob_info), 0, kData.size())));

  blink::mojom::BlobPtr blob;
  EXPECT_TRUE(registry_->Register(MakeRequest(&blob), kId, "", "",
                                  std::move(elements)));

  EXPECT_TRUE(bad_messages_.empty());

  EXPECT_TRUE(context_->registry().HasEntry(kId));
  EXPECT_TRUE(context_->GetBlobDataFromUUID(kId)->IsBeingBuilt());
  EXPECT_EQ(1u, BlobsUnderConstruction());

  // Now drop all references to the blob.
  blob.reset();
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(context_->registry().HasEntry(kId));

  // Now cause construction to complete, if it would still be going on.
  auto builder = std::make_unique<BlobDataBuilder>(kDepId);
  builder->AppendData(kData);
  context_->BuildPreregisteredBlob(
      std::move(builder), BlobStorageContext::TransportAllowedCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0u, BlobsUnderConstruction());
}

TEST_F(BlobRegistryImplTest,
       Register_DefereferencedWhileBuildingBeforeTransporting) {
  const std::string kId = "id";
  const std::string kData = "hello world";

  blink::mojom::BytesProviderPtrInfo bytes_provider_info;
  auto request = MakeRequest(&bytes_provider_info);

  std::vector<blink::mojom::DataElementPtr> elements;
  elements.push_back(
      blink::mojom::DataElement::NewBytes(blink::mojom::DataElementBytes::New(
          kData.size(), base::nullopt, std::move(bytes_provider_info))));

  blink::mojom::BlobPtr blob;
  EXPECT_TRUE(registry_->Register(MakeRequest(&blob), kId, "", "",
                                  std::move(elements)));
  EXPECT_TRUE(bad_messages_.empty());

  EXPECT_TRUE(context_->registry().HasEntry(kId));
  EXPECT_TRUE(context_->GetBlobDataFromUUID(kId)->IsBeingBuilt());
  EXPECT_EQ(1u, BlobsUnderConstruction());

  // Now drop all references to the blob.
  blob.reset();
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(context_->registry().HasEntry(kId));

  // Now cause construction to complete, if it would still be going on.
  CreateBytesProvider(kData, std::move(request));
  scoped_task_environment_.RunUntilIdle();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0u, BlobsUnderConstruction());
}

TEST_F(BlobRegistryImplTest,
       Register_DefereferencedWhileBuildingBeforeTransportingByFile) {
  const std::string kId = "id";
  const std::string kData =
      base::RandBytesAsString(kTestBlobStorageMaxBlobMemorySize + 42);

  blink::mojom::BytesProviderPtrInfo bytes_provider_info;
  auto request = MakeRequest(&bytes_provider_info);

  std::vector<blink::mojom::DataElementPtr> elements;
  elements.push_back(
      blink::mojom::DataElement::NewBytes(blink::mojom::DataElementBytes::New(
          kData.size(), base::nullopt, std::move(bytes_provider_info))));

  blink::mojom::BlobPtr blob;
  EXPECT_TRUE(registry_->Register(MakeRequest(&blob), kId, "", "",
                                  std::move(elements)));
  EXPECT_TRUE(bad_messages_.empty());

  EXPECT_TRUE(context_->registry().HasEntry(kId));
  EXPECT_TRUE(context_->GetBlobDataFromUUID(kId)->IsBeingBuilt());
  EXPECT_EQ(1u, BlobsUnderConstruction());

  // Now drop all references to the blob.
  blob.reset();
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(context_->registry().HasEntry(kId));

  // Now cause construction to complete, if it would still be going on.
  CreateBytesProvider(kData, std::move(request));
  scoped_task_environment_.RunUntilIdle();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0u, BlobsUnderConstruction());
}

TEST_F(BlobRegistryImplTest, RegisterFromStream) {
  const std::string kData = "hello world, this is a blob";
  const std::string kContentType = "content/type";
  const std::string kContentDisposition = "disposition";

  FakeProgressClient progress_client;
  blink::mojom::ProgressClientAssociatedPtrInfo progress_client_ptr;
  mojo::AssociatedBinding<blink::mojom::ProgressClient> progress_binding(
      &progress_client, MakeRequest(&progress_client_ptr));

  mojo::DataPipe pipe;
  blink::mojom::SerializedBlobPtr blob;
  base::RunLoop loop;
  registry_->RegisterFromStream(
      kContentType, kContentDisposition, kData.length(),
      std::move(pipe.consumer_handle), std::move(progress_client_ptr),
      base::BindLambdaForTesting([&](blink::mojom::SerializedBlobPtr result) {
        blob = std::move(result);
        loop.Quit();
      }));
  mojo::BlockingCopyFromString(kData, pipe.producer_handle);
  pipe.producer_handle.reset();
  loop.Run();

  ASSERT_TRUE(blob);
  EXPECT_FALSE(blob->uuid.empty());
  EXPECT_EQ(kContentType, blob->content_type);
  EXPECT_EQ(kData.length(), blob->size);
  ASSERT_TRUE(blob->blob);
  blink::mojom::BlobPtr blob_ptr(std::move(blob->blob));
  EXPECT_EQ(blob->uuid, UUIDFromBlob(blob_ptr.get()));

  EXPECT_EQ(kData.length(), progress_client.total_size);
  EXPECT_GE(progress_client.call_count, 1);
}

}  // namespace storage
