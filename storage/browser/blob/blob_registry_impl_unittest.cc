// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/blob/blob_registry_impl.h"

#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/rand_util.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_restrictions.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "mojo/public/cpp/system/data_pipe_utils.h"
#include "mojo/public/cpp/system/functions.h"
#include "net/base/features.h"
#include "storage/browser/blob/blob_data_builder.h"
#include "storage/browser/blob/blob_data_handle.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "storage/browser/blob/blob_url_registry.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/browser/test/fake_blob.h"
#include "storage/browser/test/fake_progress_client.h"
#include "storage/browser/test/mock_blob_registry_delegate.h"
#include "storage/browser/test/mock_bytes_provider.h"
#include "storage/browser/test/mock_special_storage_policy.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/blob/data_element.mojom.h"
#include "third_party/blink/public/mojom/blob/serialized_blob.mojom.h"

namespace storage {

namespace {

const size_t kTestBlobStorageIPCThresholdBytes = 5;
const size_t kTestBlobStorageMaxSharedMemoryBytes = 20;
const size_t kTestBlobStorageMaxBytesDataItemSize = 23;

const size_t kTestBlobStorageMaxBlobMemorySize = 400;
const uint64_t kTestBlobStorageMaxDiskSpace = 4000;
const uint64_t kTestBlobStorageMinFileSizeBytes = 10;
const uint64_t kTestBlobStorageMaxFileSizeBytes = 100;

void BindBytesProvider(
    std::unique_ptr<MockBytesProvider> impl,
    mojo::PendingReceiver<blink::mojom::BytesProvider> receiver) {
  mojo::MakeSelfOwnedReceiver(std::move(impl), std::move(receiver));
}

}  // namespace

class BlobRegistryImplTest : public testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(data_dir_.CreateUniqueTempDir());
    context_ = std::make_unique<BlobStorageContext>(
        data_dir_.GetPath(), data_dir_.GetPath(),
        base::ThreadPool::CreateTaskRunner({base::MayBlock()}));
    auto storage_policy = base::MakeRefCounted<MockSpecialStoragePolicy>();
    registry_impl_ = std::make_unique<BlobRegistryImpl>(context_->AsWeakPtr());
    auto delegate = std::make_unique<MockBlobRegistryDelegate>();
    delegate_ptr_ = delegate->AsWeakPtr();
    registry_impl_->Bind(registry_.BindNewPipeAndPassReceiver(),
                         std::move(delegate));

    mojo::SetDefaultProcessErrorHandler(base::BindRepeating(
        &BlobRegistryImplTest::OnBadMessage, base::Unretained(this)));

    BlobStorageLimits limits;
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
    disallow_blocking_.emplace();
  }

  void TearDown() override {
    mojo::SetDefaultProcessErrorHandler(base::NullCallback());

    // Give pending tasks a chance to run since they may release Mojo bindings
    // resources.
    base::RunLoop().RunUntilIdle();
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
        [](base::OnceClosure quit_closure, std::string* uuid_out,
           const std::string& uuid) {
          *uuid_out = uuid;
          std::move(quit_closure).Run();
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
    blob_handle->RunOnConstructionComplete(
        base::BindOnce([](base::OnceClosure closure,
                          BlobStatus status) { std::move(closure).Run(); },
                       loop.QuitClosure()));
    loop.Run();
  }

  mojo::PendingRemote<blink::mojom::BytesProvider> CreateBytesProvider(
      const std::string& bytes) {
    if (!bytes_provider_runner_) {
      bytes_provider_runner_ =
          base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()});
    }
    mojo::PendingRemote<blink::mojom::BytesProvider> result;
    auto provider = std::make_unique<MockBytesProvider>(
        std::vector<uint8_t>(bytes.begin(), bytes.end()), &reply_request_count_,
        &stream_request_count_, &file_request_count_);
    bytes_provider_runner_->PostTask(
        FROM_HERE, base::BindOnce(&BindBytesProvider, std::move(provider),
                                  result.InitWithNewPipeAndPassReceiver()));
    return result;
  }

  void CreateBytesProvider(
      const std::string& bytes,
      mojo::PendingReceiver<blink::mojom::BytesProvider> receiver) {
    if (!bytes_provider_runner_) {
      bytes_provider_runner_ =
          base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()});
    }
    auto provider = std::make_unique<MockBytesProvider>(
        std::vector<uint8_t>(bytes.begin(), bytes.end()), &reply_request_count_,
        &stream_request_count_, &file_request_count_);
    bytes_provider_runner_->PostTask(
        FROM_HERE, base::BindOnce(&BindBytesProvider, std::move(provider),
                                  std::move(receiver)));
  }

  size_t BlobsUnderConstruction() {
    return registry_impl_->BlobsUnderConstructionForTesting();
  }

  size_t BlobsBeingStreamed() {
    return registry_impl_->BlobsBeingStreamedForTesting();
  }

 protected:
  base::ScopedTempDir data_dir_;
  base::test::TaskEnvironment task_environment_;
  std::optional<base::ScopedDisallowBlocking> disallow_blocking_;
  std::unique_ptr<BlobStorageContext> context_;
  std::unique_ptr<BlobRegistryImpl> registry_impl_;
  mojo::Remote<blink::mojom::BlobRegistry> registry_;
  base::WeakPtr<MockBlobRegistryDelegate> delegate_ptr_;
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
    mojo::Remote<blink::mojom::Blob> blob;
    registry_->GetBlobFromUUID(blob.BindNewPipeAndPassReceiver(), kId);
    EXPECT_EQ(kId, UUIDFromBlob(blob.get()));
    EXPECT_TRUE(blob.is_connected());
  }

  {
    mojo::Remote<blink::mojom::Blob> blob;
    registry_->GetBlobFromUUID(blob.BindNewPipeAndPassReceiver(), "invalid id");
    blob.FlushForTesting();
    EXPECT_FALSE(blob.is_connected());
  }
}

TEST_F(BlobRegistryImplTest, GetBlobFromEmptyUUID) {
  mojo::Remote<blink::mojom::Blob> blob;
  registry_->GetBlobFromUUID(blob.BindNewPipeAndPassReceiver(), "");
  blob.FlushForTesting();
  EXPECT_EQ(1u, bad_messages_.size());
  EXPECT_FALSE(blob.is_connected());
}

TEST_F(BlobRegistryImplTest, Register_EmptyUUID) {
  mojo::Remote<blink::mojom::Blob> blob;
  EXPECT_FALSE(
      registry_->Register(blob.BindNewPipeAndPassReceiver(), "", "", "",
                          std::vector<blink::mojom::DataElementPtr>()));

  EXPECT_EQ(1u, bad_messages_.size());

  registry_.FlushForTesting();
  EXPECT_FALSE(registry_.is_connected());

  blob.FlushForTesting();
  EXPECT_FALSE(blob.is_connected());
  EXPECT_EQ(0u, BlobsUnderConstruction());
}

TEST_F(BlobRegistryImplTest, Register_ExistingUUID) {
  const std::string kId = "id";
  std::unique_ptr<BlobDataHandle> handle =
      CreateBlobFromString(kId, "hello world");

  mojo::Remote<blink::mojom::Blob> blob;
  EXPECT_FALSE(
      registry_->Register(blob.BindNewPipeAndPassReceiver(), kId, "", "",
                          std::vector<blink::mojom::DataElementPtr>()));

  EXPECT_EQ(1u, bad_messages_.size());

  registry_.FlushForTesting();
  EXPECT_FALSE(registry_.is_connected());

  blob.FlushForTesting();
  EXPECT_FALSE(blob.is_connected());
  EXPECT_EQ(0u, BlobsUnderConstruction());
}

TEST_F(BlobRegistryImplTest, Register_EmptyBlob) {
  const std::string kId = "id";
  const std::string kContentType = "content/type";
  const std::string kContentDisposition = "disposition";

  mojo::Remote<blink::mojom::Blob> blob;
  EXPECT_TRUE(registry_->Register(blob.BindNewPipeAndPassReceiver(), kId,
                                  kContentType, kContentDisposition,
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

TEST_F(BlobRegistryImplTest, Register_EmptyBytesBlob) {
  const std::string kId = "id";
  const std::string kContentType = "content/type";
  const std::string kContentDisposition = "disposition";

  std::vector<blink::mojom::DataElementPtr> elements;
  elements.push_back(
      blink::mojom::DataElement::NewBytes(blink::mojom::DataElementBytes::New(
          0, std::nullopt, CreateBytesProvider(""))));

  mojo::Remote<blink::mojom::Blob> blob;
  EXPECT_TRUE(registry_->Register(blob.BindNewPipeAndPassReceiver(), kId,
                                  kContentType, kContentDisposition,
                                  std::move(elements)));

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
  mojo::PendingRemote<blink::mojom::Blob> referenced_blob_remote;
  std::ignore = referenced_blob_remote.InitWithNewPipeAndPassReceiver();
  elements.push_back(
      blink::mojom::DataElement::NewBlob(blink::mojom::DataElementBlob::New(
          std::move(referenced_blob_remote), 0, 16)));

  mojo::PendingRemote<blink::mojom::Blob> blob;
  EXPECT_TRUE(registry_->Register(blob.InitWithNewPipeAndPassReceiver(), kId,
                                  "", "", std::move(elements)));
  EXPECT_TRUE(bad_messages_.empty());

  std::unique_ptr<BlobDataHandle> handle = context_->GetBlobDataFromUUID(kId);
  WaitForBlobCompletion(handle.get());

  EXPECT_TRUE(handle->IsBroken());
  EXPECT_EQ(BlobStatus::ERR_REFERENCED_BLOB_BROKEN, handle->GetBlobStatus());
  EXPECT_EQ(0u, BlobsUnderConstruction());
}

TEST_F(BlobRegistryImplTest, Register_SelfReference) {
  const std::string kId = "id";

  mojo::PendingRemote<blink::mojom::Blob> blob_remote;
  mojo::PendingReceiver<blink::mojom::Blob> receiver =
      blob_remote.InitWithNewPipeAndPassReceiver();

  std::vector<blink::mojom::DataElementPtr> elements;
  elements.push_back(blink::mojom::DataElement::NewBlob(
      blink::mojom::DataElementBlob::New(std::move(blob_remote), 0, 16)));

  EXPECT_TRUE(registry_->Register(std::move(receiver), kId, "", "",
                                  std::move(elements)));
  EXPECT_TRUE(bad_messages_.empty());

  std::unique_ptr<BlobDataHandle> handle = context_->GetBlobDataFromUUID(kId);
  WaitForBlobCompletion(handle.get());

  EXPECT_TRUE(handle->IsBroken());
  EXPECT_EQ(BlobStatus::ERR_INVALID_CONSTRUCTION_ARGUMENTS,
            handle->GetBlobStatus());

  EXPECT_EQ(1u, bad_messages_.size());

  registry_.FlushForTesting();
  EXPECT_FALSE(registry_.is_connected());
  EXPECT_EQ(0u, BlobsUnderConstruction());
}

TEST_F(BlobRegistryImplTest, Register_CircularReference) {
  const std::string kId1 = "id1";
  const std::string kId2 = "id2";
  const std::string kId3 = "id3";

  mojo::PendingRemote<blink::mojom::Blob> blob1_remote, blob2_remote,
      blob3_remote;
  mojo::PendingReceiver<blink::mojom::Blob> blob_receiver1 =
      blob1_remote.InitWithNewPipeAndPassReceiver();
  mojo::PendingReceiver<blink::mojom::Blob> blob_receiver2 =
      blob2_remote.InitWithNewPipeAndPassReceiver();
  mojo::PendingReceiver<blink::mojom::Blob> blob_receiver3 =
      blob3_remote.InitWithNewPipeAndPassReceiver();

  std::vector<blink::mojom::DataElementPtr> elements1;
  elements1.push_back(blink::mojom::DataElement::NewBlob(
      blink::mojom::DataElementBlob::New(std::move(blob1_remote), 0, 16)));

  std::vector<blink::mojom::DataElementPtr> elements2;
  elements2.push_back(blink::mojom::DataElement::NewBlob(
      blink::mojom::DataElementBlob::New(std::move(blob2_remote), 0, 16)));

  std::vector<blink::mojom::DataElementPtr> elements3;
  elements3.push_back(blink::mojom::DataElement::NewBlob(
      blink::mojom::DataElementBlob::New(std::move(blob3_remote), 0, 16)));

  EXPECT_TRUE(registry_->Register(std::move(blob_receiver1), kId1, "", "",
                                  std::move(elements2)));
  EXPECT_TRUE(registry_->Register(std::move(blob_receiver2), kId2, "", "",
                                  std::move(elements3)));
  EXPECT_TRUE(registry_->Register(std::move(blob_receiver3), kId3, "", "",
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
  EXPECT_FALSE(registry_.is_connected());
  EXPECT_EQ(0u, BlobsUnderConstruction());
#endif
}

TEST_F(BlobRegistryImplTest, Register_NonExistentBlob) {
  const std::string kId = "id";

  std::vector<blink::mojom::DataElementPtr> elements;
  mojo::PendingRemote<blink::mojom::Blob> referenced_blob_remote;
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<FakeBlob>("mock blob"),
      referenced_blob_remote.InitWithNewPipeAndPassReceiver());
  elements.push_back(
      blink::mojom::DataElement::NewBlob(blink::mojom::DataElementBlob::New(
          std::move(referenced_blob_remote), 0, 16)));

  mojo::PendingRemote<blink::mojom::Blob> blob;
  EXPECT_TRUE(registry_->Register(blob.InitWithNewPipeAndPassReceiver(), kId,
                                  "", "", std::move(elements)));
  EXPECT_TRUE(bad_messages_.empty());

  std::unique_ptr<BlobDataHandle> handle = context_->GetBlobDataFromUUID(kId);
  WaitForBlobCompletion(handle.get());

  EXPECT_TRUE(handle->IsBroken());
  EXPECT_EQ(BlobStatus::ERR_INVALID_CONSTRUCTION_ARGUMENTS,
            handle->GetBlobStatus());

  EXPECT_EQ(1u, bad_messages_.size());

  registry_.FlushForTesting();
  EXPECT_FALSE(registry_.is_connected());
  EXPECT_EQ(0u, BlobsUnderConstruction());
}

TEST_F(BlobRegistryImplTest, Register_ValidBlobReferences) {
  const std::string kId1 = "id1";
  std::unique_ptr<BlobDataHandle> handle =
      CreateBlobFromString(kId1, "hello world");
  mojo::PendingRemote<blink::mojom::Blob> blob1_remote;
  mojo::MakeSelfOwnedReceiver(std::make_unique<FakeBlob>(kId1),
                              blob1_remote.InitWithNewPipeAndPassReceiver());

  const std::string kId2 = "id2";
  mojo::PendingRemote<blink::mojom::Blob> blob2_remote;
  mojo::PendingReceiver<blink::mojom::Blob> blob_receiver2 =
      blob2_remote.InitWithNewPipeAndPassReceiver();

  std::vector<blink::mojom::DataElementPtr> elements1;
  elements1.push_back(blink::mojom::DataElement::NewBlob(
      blink::mojom::DataElementBlob::New(std::move(blob1_remote), 0, 8)));

  std::vector<blink::mojom::DataElementPtr> elements2;
  elements2.push_back(blink::mojom::DataElement::NewBlob(
      blink::mojom::DataElementBlob::New(std::move(blob2_remote), 0, 8)));

  mojo::PendingRemote<blink::mojom::Blob> final_blob;
  const std::string kId3 = "id3";
  EXPECT_TRUE(registry_->Register(final_blob.InitWithNewPipeAndPassReceiver(),
                                  kId3, "", "", std::move(elements2)));
  EXPECT_TRUE(registry_->Register(std::move(blob_receiver2), kId2, "", "",
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

TEST_F(BlobRegistryImplTest, Register_BlobReferencingPendingBlob) {
  // Create a blob that is pending population of its data.
  const std::string kId1 = "id1";
  const std::string kBlob1Data = "foobar";
  auto builder = std::make_unique<BlobDataBuilder>(kId1);
  builder->set_content_type("text/plain");
  BlobDataBuilder::FutureData future_data =
      builder->AppendFutureData(kBlob1Data.length());
  std::unique_ptr<BlobDataHandle> handle =
      context_->BuildBlob(std::move(builder), base::DoNothing());

  mojo::PendingRemote<blink::mojom::Blob> blob1_remote;
  mojo::MakeSelfOwnedReceiver(std::make_unique<FakeBlob>(kId1),
                              blob1_remote.InitWithNewPipeAndPassReceiver());

  // Now create a blob referencing the pending blob above.
  std::vector<blink::mojom::DataElementPtr> elements;
  elements.push_back(
      blink::mojom::DataElement::NewBlob(blink::mojom::DataElementBlob::New(
          std::move(blob1_remote), 0, kBlob1Data.length())));

  mojo::PendingRemote<blink::mojom::Blob> final_blob;
  const std::string kId2 = "id2";
  EXPECT_TRUE(registry_->Register(final_blob.InitWithNewPipeAndPassReceiver(),
                                  kId2, "", "", std::move(elements)));

  // Run the runloop to make sure registration of blob kId2 gets far enough
  // before blob kId1 is populated.
  base::RunLoop().RunUntilIdle();

  // Populate the data for the first blob.
  future_data.Populate(base::as_bytes(base::make_span(kBlob1Data)));
  context_->NotifyTransportComplete(kId1);

  // Wait for kId2 to also complete.
  std::unique_ptr<BlobDataHandle> handle2 = context_->GetBlobDataFromUUID(kId2);
  WaitForBlobCompletion(handle2.get());

  // Make sure blob was constructed correctly.
  EXPECT_FALSE(handle2->IsBroken());
  ASSERT_EQ(BlobStatus::DONE, handle2->GetBlobStatus());
  BlobDataBuilder expected_blob_data(kId2);
  expected_blob_data.AppendData(kBlob1Data);
  EXPECT_EQ(expected_blob_data, *handle2->CreateSnapshot());

  // And make sure we're not leaking any under construction blobs.
  EXPECT_EQ(0u, BlobsUnderConstruction());
}

TEST_F(BlobRegistryImplTest, Register_UnreadableFile) {
  delegate_ptr_->can_read_file_result = false;

  const std::string kId = "id";

  std::vector<blink::mojom::DataElementPtr> elements;
  elements.push_back(
      blink::mojom::DataElement::NewFile(blink::mojom::DataElementFile::New(
          base::FilePath(FILE_PATH_LITERAL("foobar")), 0, 16, std::nullopt)));

  mojo::PendingRemote<blink::mojom::Blob> blob;
  EXPECT_TRUE(registry_->Register(blob.InitWithNewPipeAndPassReceiver(), kId,
                                  "", "", std::move(elements)));
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
      blink::mojom::DataElementFile::New(path, 0, 16, std::nullopt)));

  mojo::PendingRemote<blink::mojom::Blob> blob;
  EXPECT_TRUE(registry_->Register(blob.InitWithNewPipeAndPassReceiver(), kId,
                                  "", "", std::move(elements)));
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

TEST_F(BlobRegistryImplTest, Register_BytesInvalidEmbeddedData) {
  const std::string kId = "id";

  std::vector<blink::mojom::DataElementPtr> elements;
  elements.push_back(
      blink::mojom::DataElement::NewBytes(blink::mojom::DataElementBytes::New(
          10, std::vector<uint8_t>(5), CreateBytesProvider(""))));

  mojo::PendingRemote<blink::mojom::Blob> blob;
  EXPECT_FALSE(registry_->Register(blob.InitWithNewPipeAndPassReceiver(), kId,
                                   "", "", std::move(elements)));
  EXPECT_EQ(1u, bad_messages_.size());

  registry_.FlushForTesting();
  EXPECT_FALSE(registry_.is_connected());

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
          8, std::nullopt, CreateBytesProvider(""))));
  elements.push_back(
      blink::mojom::DataElement::NewBytes(blink::mojom::DataElementBytes::New(
          std::numeric_limits<uint64_t>::max() - 4, std::nullopt,
          CreateBytesProvider(""))));

  mojo::PendingRemote<blink::mojom::Blob> blob;
  EXPECT_FALSE(registry_->Register(blob.InitWithNewPipeAndPassReceiver(), kId,
                                   "", "", std::move(elements)));
  EXPECT_EQ(1u, bad_messages_.size());

  registry_.FlushForTesting();
  EXPECT_FALSE(registry_.is_connected());

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
          kTestBlobStorageMaxDiskSpace, std::nullopt,
          CreateBytesProvider(""))));
  elements.push_back(
      blink::mojom::DataElement::NewBytes(blink::mojom::DataElementBytes::New(
          kTestBlobStorageMaxDiskSpace, std::nullopt,
          CreateBytesProvider(""))));

  mojo::PendingRemote<blink::mojom::Blob> blob;
  EXPECT_TRUE(registry_->Register(blob.InitWithNewPipeAndPassReceiver(), kId,
                                  "", "", std::move(elements)));

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

  mojo::PendingRemote<blink::mojom::Blob> blob;
  EXPECT_TRUE(registry_->Register(blob.InitWithNewPipeAndPassReceiver(), kId,
                                  "", "", std::move(elements)));

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
          kData.size(), std::nullopt, CreateBytesProvider(kData))));

  mojo::PendingRemote<blink::mojom::Blob> blob;
  EXPECT_TRUE(registry_->Register(blob.InitWithNewPipeAndPassReceiver(), kId,
                                  "", "", std::move(elements)));

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

TEST_F(BlobRegistryImplTest, Register_InvalidBytesAsReply) {
  const std::string kId = "id";
  const std::string kData = "hello";

  std::vector<blink::mojom::DataElementPtr> elements;
  elements.push_back(
      blink::mojom::DataElement::NewBytes(blink::mojom::DataElementBytes::New(
          kData.size(), std::nullopt, CreateBytesProvider(""))));

  mojo::PendingRemote<blink::mojom::Blob> blob;
  EXPECT_TRUE(registry_->Register(blob.InitWithNewPipeAndPassReceiver(), kId,
                                  "", "", std::move(elements)));

  std::unique_ptr<BlobDataHandle> handle = context_->GetBlobDataFromUUID(kId);
  WaitForBlobCompletion(handle.get());

  EXPECT_TRUE(handle->IsBroken());
  ASSERT_EQ(BlobStatus::ERR_INVALID_CONSTRUCTION_ARGUMENTS,
            handle->GetBlobStatus());

  EXPECT_EQ(1u, reply_request_count_);
  EXPECT_EQ(0u, stream_request_count_);
  EXPECT_EQ(0u, file_request_count_);
  EXPECT_EQ(0u, BlobsUnderConstruction());

  // Expect 2 bad messages, one for the bad reply by the bytes provider, and one
  // for the original register call.
  EXPECT_EQ(2u, bad_messages_.size());
}

TEST_F(BlobRegistryImplTest, Register_ValidBytesAsStream) {
  const std::string kId = "id";
  const std::string kData =
      base::RandBytesAsString(kTestBlobStorageMaxSharedMemoryBytes * 3 + 13);

  std::vector<blink::mojom::DataElementPtr> elements;
  elements.push_back(
      blink::mojom::DataElement::NewBytes(blink::mojom::DataElementBytes::New(
          kData.size(), std::nullopt, CreateBytesProvider(kData))));

  mojo::PendingRemote<blink::mojom::Blob> blob;
  EXPECT_TRUE(registry_->Register(blob.InitWithNewPipeAndPassReceiver(), kId,
                                  "", "", std::move(elements)));

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
          kData.size(), std::nullopt, CreateBytesProvider(kData))));

  mojo::PendingRemote<blink::mojom::Blob> blob;
  EXPECT_TRUE(registry_->Register(blob.InitWithNewPipeAndPassReceiver(), kId,
                                  "", "", std::move(elements)));

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

  mojo::PendingRemote<blink::mojom::BytesProvider> bytes_provider_remote;
  std::ignore = bytes_provider_remote.InitWithNewPipeAndPassReceiver();

  std::vector<blink::mojom::DataElementPtr> elements;
  elements.push_back(
      blink::mojom::DataElement::NewBytes(blink::mojom::DataElementBytes::New(
          32, std::nullopt, std::move(bytes_provider_remote))));

  mojo::PendingRemote<blink::mojom::Blob> blob;
  EXPECT_TRUE(registry_->Register(blob.InitWithNewPipeAndPassReceiver(), kId,
                                  "", "", std::move(elements)));
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

  mojo::PendingRemote<blink::mojom::BytesProvider> bytes_provider_remote;
  auto receiver = bytes_provider_remote.InitWithNewPipeAndPassReceiver();

  std::vector<blink::mojom::DataElementPtr> elements;
  elements.push_back(
      blink::mojom::DataElement::NewBytes(blink::mojom::DataElementBytes::New(
          32, std::nullopt, std::move(bytes_provider_remote))));

  mojo::PendingRemote<blink::mojom::Blob> blob;
  EXPECT_TRUE(registry_->Register(blob.InitWithNewPipeAndPassReceiver(), kId,
                                  "", "", std::move(elements)));
  EXPECT_TRUE(bad_messages_.empty());

  EXPECT_TRUE(context_->registry().HasEntry(kId));
  EXPECT_TRUE(context_->GetBlobDataFromUUID(kId)->IsBeingBuilt());
  EXPECT_EQ(1u, BlobsUnderConstruction());

  // Now drop all references to the blob.
  blob.reset();
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(context_->registry().HasEntry(kId));

  // Now cause construction to fail, if it would still be going on.
  receiver.reset();
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
  mojo::PendingRemote<blink::mojom::Blob> referenced_blob_remote;
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<FakeBlob>(kDepId),
      referenced_blob_remote.InitWithNewPipeAndPassReceiver());

  // Create mojo blob depending on future blob.
  std::vector<blink::mojom::DataElementPtr> elements;
  elements.push_back(
      blink::mojom::DataElement::NewBlob(blink::mojom::DataElementBlob::New(
          std::move(referenced_blob_remote), 0, kData.size())));

  mojo::PendingRemote<blink::mojom::Blob> blob;
  EXPECT_TRUE(registry_->Register(blob.InitWithNewPipeAndPassReceiver(), kId,
                                  "", "", std::move(elements)));

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

  mojo::PendingRemote<blink::mojom::BytesProvider> bytes_provider_remote;
  auto receiver = bytes_provider_remote.InitWithNewPipeAndPassReceiver();

  std::vector<blink::mojom::DataElementPtr> elements;
  elements.push_back(
      blink::mojom::DataElement::NewBytes(blink::mojom::DataElementBytes::New(
          kData.size(), std::nullopt, std::move(bytes_provider_remote))));

  mojo::PendingRemote<blink::mojom::Blob> blob;
  EXPECT_TRUE(registry_->Register(blob.InitWithNewPipeAndPassReceiver(), kId,
                                  "", "", std::move(elements)));
  EXPECT_TRUE(bad_messages_.empty());

  EXPECT_TRUE(context_->registry().HasEntry(kId));
  EXPECT_TRUE(context_->GetBlobDataFromUUID(kId)->IsBeingBuilt());
  EXPECT_EQ(1u, BlobsUnderConstruction());

  // Now drop all references to the blob.
  blob.reset();
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(context_->registry().HasEntry(kId));

  // Now cause construction to complete, if it would still be going on.
  CreateBytesProvider(kData, std::move(receiver));
  task_environment_.RunUntilIdle();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0u, BlobsUnderConstruction());
}

TEST_F(BlobRegistryImplTest,
       Register_DefereferencedWhileBuildingBeforeTransportingByFile) {
  const std::string kId = "id";
  const std::string kData =
      base::RandBytesAsString(kTestBlobStorageMaxBlobMemorySize + 42);

  mojo::PendingRemote<blink::mojom::BytesProvider> bytes_provider_remote;
  auto receiver = bytes_provider_remote.InitWithNewPipeAndPassReceiver();

  std::vector<blink::mojom::DataElementPtr> elements;
  elements.push_back(
      blink::mojom::DataElement::NewBytes(blink::mojom::DataElementBytes::New(
          kData.size(), std::nullopt, std::move(bytes_provider_remote))));

  mojo::PendingRemote<blink::mojom::Blob> blob;
  EXPECT_TRUE(registry_->Register(blob.InitWithNewPipeAndPassReceiver(), kId,
                                  "", "", std::move(elements)));
  EXPECT_TRUE(bad_messages_.empty());

  EXPECT_TRUE(context_->registry().HasEntry(kId));
  EXPECT_TRUE(context_->GetBlobDataFromUUID(kId)->IsBeingBuilt());
  EXPECT_EQ(1u, BlobsUnderConstruction());

  // Now drop all references to the blob.
  blob.reset();
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(context_->registry().HasEntry(kId));

  // Now cause construction to complete, if it would still be going on.
  CreateBytesProvider(kData, std::move(receiver));
  task_environment_.RunUntilIdle();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0u, BlobsUnderConstruction());
}

TEST_F(BlobRegistryImplTest, RegisterFromStream) {
  const std::string kData = "hello world, this is a blob";
  const std::string kContentType = "content/type";
  const std::string kContentDisposition = "disposition";

  FakeProgressClient progress_client;
  mojo::AssociatedReceiver<blink::mojom::ProgressClient> progress_receiver(
      &progress_client);

  mojo::ScopedDataPipeProducerHandle producer;
  mojo::ScopedDataPipeConsumerHandle consumer;
  mojo::CreateDataPipe(nullptr, producer, consumer);
  blink::mojom::SerializedBlobPtr blob;
  base::RunLoop loop;
  registry_->RegisterFromStream(
      kContentType, kContentDisposition, kData.length(), std::move(consumer),
      progress_receiver.BindNewEndpointAndPassRemote(),
      base::BindLambdaForTesting([&](blink::mojom::SerializedBlobPtr result) {
        blob = std::move(result);
        loop.Quit();
      }));
  mojo::BlockingCopyFromString(kData, producer);
  producer.reset();
  loop.Run();

  ASSERT_TRUE(blob);
  EXPECT_FALSE(blob->uuid.empty());
  EXPECT_EQ(kContentType, blob->content_type);
  EXPECT_EQ(kData.length(), blob->size);
  ASSERT_TRUE(blob->blob);
  mojo::Remote<blink::mojom::Blob> blob_remote(std::move(blob->blob));
  EXPECT_EQ(blob->uuid, UUIDFromBlob(blob_remote.get()));

  EXPECT_EQ(kData.length(), progress_client.total_size);
  EXPECT_GE(progress_client.call_count, 1);

  EXPECT_EQ(0u, BlobsBeingStreamed());
}

TEST_F(BlobRegistryImplTest, RegisterFromStream_NoDiskSpace) {
  const std::string kData =
      base::RandBytesAsString(kTestBlobStorageMaxDiskSpace + 1);
  const std::string kContentType = "content/type";
  const std::string kContentDisposition = "disposition";

  FakeProgressClient progress_client;
  mojo::AssociatedReceiver<blink::mojom::ProgressClient> progress_receiver(
      &progress_client);

  mojo::ScopedDataPipeProducerHandle producer;
  mojo::ScopedDataPipeConsumerHandle consumer;
  mojo::CreateDataPipe(nullptr, producer, consumer);
  blink::mojom::SerializedBlobPtr blob;
  base::RunLoop loop;
  registry_->RegisterFromStream(
      kContentType, kContentDisposition, kData.length(), std::move(consumer),
      progress_receiver.BindNewEndpointAndPassRemote(),
      base::BindLambdaForTesting([&](blink::mojom::SerializedBlobPtr result) {
        blob = std::move(result);
        loop.Quit();
      }));
  mojo::BlockingCopyFromString(kData, producer);
  producer.reset();
  loop.Run();

  EXPECT_FALSE(blob);
  EXPECT_EQ(0u, BlobsBeingStreamed());
}

TEST_F(BlobRegistryImplTest, DestroyWithUnfinishedStream) {
  mojo::ScopedDataPipeProducerHandle producer_handle1;
  mojo::ScopedDataPipeConsumerHandle consumer_handle1;
  ASSERT_EQ(mojo::CreateDataPipe(nullptr, producer_handle1, consumer_handle1),
            MOJO_RESULT_OK);

  mojo::ScopedDataPipeProducerHandle producer_handle2;
  mojo::ScopedDataPipeConsumerHandle consumer_handle2;
  ASSERT_EQ(mojo::CreateDataPipe(nullptr, producer_handle2, consumer_handle2),
            MOJO_RESULT_OK);

  registry_->RegisterFromStream("", "", 0, std::move(consumer_handle1),
                                mojo::NullAssociatedRemote(),
                                base::DoNothing());
  registry_->RegisterFromStream("", "", 0, std::move(consumer_handle2),
                                mojo::NullAssociatedRemote(),
                                base::DoNothing());
  registry_.FlushForTesting();
  // This test just makes sure no crash happens if we're shut down while still
  // creating blobs from streams.
}

}  // namespace storage
