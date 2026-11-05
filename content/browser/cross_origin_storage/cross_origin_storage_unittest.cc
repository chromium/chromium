// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "base/threading/sequence_bound.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/browser/cross_origin_storage/cross_origin_storage_file_handle_impl.h"
#include "content/browser/cross_origin_storage/cross_origin_storage_file_writer_impl.h"
#include "content/browser/cross_origin_storage/cross_origin_storage_manager_impl.h"
#include "content/browser/cross_origin_storage/cross_origin_storage_registry.h"
#include "content/browser/cross_origin_storage/cross_origin_storage_staging_writer.h"
#include "content/browser/cross_origin_storage/cross_origin_storage_types.h"
#include "content/browser/file_system_access/file_system_access_file_handle_impl.h"
#include "content/browser/file_system_access/file_system_access_manager_impl.h"
#include "content/browser/file_system_access/fixed_file_system_access_permission_grant.h"
#include "content/public/browser/back_forward_cache.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/storage_partition_config.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_renderer_host.h"
#include "crypto/hash.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/data_pipe_utils.h"
#include "mojo/public/cpp/system/simple_watcher.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "net/base/net_errors.h"
#include "net/base/schemeful_site.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/browser/test/mock_quota_manager.h"
#include "storage/browser/test/mock_quota_manager_proxy.h"
#include "storage/browser/test/mock_special_storage_policy.h"
#include "storage/browser/test/test_file_system_context.h"
#include "storage/common/file_system/file_system_types.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/blob/blob.mojom.h"
#include "third_party/blink/public/mojom/blob/serialized_blob.mojom.h"
#include "third_party/blink/public/mojom/cross_origin_storage/cross_origin_storage.mojom.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_access_handle_host.mojom.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_cloud_identifier.mojom.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_directory_handle.mojom.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_error.mojom.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_file_handle.mojom.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_file_writer.mojom.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_manager.mojom.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_transfer_token.mojom.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {
namespace {

using blink::mojom::FileSystemAccessErrorPtr;
using blink::mojom::FileSystemAccessPermissionMode;
using blink::mojom::FileSystemAccessStatus;
using blink::mojom::PermissionStatus;

CrossOriginStorageHash ComputeHashForString(std::string_view payload) {
  auto digest = crypto::hash::Sha256(base::as_byte_span(payload));
  std::string hex = base::HexEncodeLower(digest);
  auto hash = CrossOriginStorageHash::Create(
      blink::mojom::CrossOriginStorageAlgorithm::kSha256, hex);
  CHECK(hash.has_value());
  return *hash;
}

blink::mojom::CrossOriginStorageHashPtr ToMojoHash(
    const CrossOriginStorageHash& hash) {
  return blink::mojom::CrossOriginStorageHash::New(hash.algorithm(),
                                                   hash.value());
}

mojo::ScopedDataPipeConsumerHandle CreateDataPipeWithPayload(
    std::string_view payload) {
  mojo::ScopedDataPipeProducerHandle producer;
  mojo::ScopedDataPipeConsumerHandle consumer;
  uint32_t capacity = std::max<uint32_t>(
      64 * 1024, base::checked_cast<uint32_t>(payload.size() + 1));
  CHECK_EQ(MOJO_RESULT_OK, mojo::CreateDataPipe(capacity, producer, consumer));
  CHECK_EQ(MOJO_RESULT_OK, producer->WriteAllData(base::as_byte_span(payload)));
  producer.reset();
  return consumer;
}

class TestBlobReaderClient : public blink::mojom::BlobReaderClient {
 public:
  void OnCalculatedSize(uint64_t total_size,
                        uint64_t expected_content_size) override {}

  void OnComplete(int32_t status, uint64_t data_length) override {
    complete_future_.SetValue(status, data_length);
  }

  std::pair<int32_t, uint64_t> WaitForComplete() {
    return complete_future_.Take();
  }

 private:
  base::test::TestFuture<int32_t, uint64_t> complete_future_;
};

std::string ReadBlobContents(
    mojo::PendingRemote<blink::mojom::Blob> pending_blob) {
  mojo::Remote<blink::mojom::Blob> blob_remote(std::move(pending_blob));
  mojo::ScopedDataPipeProducerHandle producer;
  mojo::ScopedDataPipeConsumerHandle consumer;
  CHECK_EQ(MOJO_RESULT_OK, mojo::CreateDataPipe(64 * 1024, producer, consumer));

  TestBlobReaderClient client_impl;
  mojo::Receiver<blink::mojom::BlobReaderClient> client_receiver(&client_impl);
  blob_remote->ReadAll(std::move(producer),
                       client_receiver.BindNewPipeAndPassRemote());
  auto [status, data_length] = client_impl.WaitForComplete();
  EXPECT_EQ(status, net::OK);

  std::string result;
  CHECK(mojo::BlockingCopyToString(std::move(consumer), &result));
  EXPECT_EQ(result.size(), data_length);
  return result;
}

class DummyWorkerHost : public base::SupportsUserData {};

}  // namespace

class CrossOriginStorageImplTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    browser_context_ = std::make_unique<TestBrowserContext>();

    blob_storage_context_ = base::MakeRefCounted<ChromeBlobStorageContext>();
    blob_storage_context_->InitializeOnIOThread(base::FilePath(),
                                                base::FilePath(), nullptr);

    quota_manager_ = base::MakeRefCounted<storage::MockQuotaManager>(
        /*is_incognito=*/false, temp_dir_.GetPath(),
        base::SingleThreadTaskRunner::GetCurrentDefault(),
        base::MakeRefCounted<storage::MockSpecialStoragePolicy>());
    quota_manager_proxy_ = base::MakeRefCounted<storage::MockQuotaManagerProxy>(
        quota_manager_.get(),
        base::SingleThreadTaskRunner::GetCurrentDefault());
    file_system_context_ = storage::CreateFileSystemContextForTesting(
        quota_manager_proxy_.get(), temp_dir_.GetPath());
    fsa_manager_ = base::MakeRefCounted<FileSystemAccessManagerImpl>(
        file_system_context_, blob_storage_context_,
        /*permission_context=*/nullptr,
        /*off_the_record=*/false);
  }

  void TearDown() override {
    if (auto* reg = CrossOriginStorageRegistry::GetOrCreateForBrowserContext(
            browser_context_.get())) {
      reg->SynchronouslyResetBackendForTesting();
    }
    fsa_manager_.reset();
    browser_context_.reset();
  }

  CrossOriginStorageRegistry* registry() {
    return CrossOriginStorageRegistry::GetOrCreateForBrowserContext(
        browser_context_.get());
  }

  std::unique_ptr<CrossOriginStorageManagerImpl> CreateManager(
      const blink::StorageKey& storage_key) {
    return std::make_unique<CrossOriginStorageManagerImpl>(
        registry()->GetWeakPtr(), blob_storage_context_,
        fsa_manager_->AsWeakPtr(), storage_key);
  }

  std::unique_ptr<CrossOriginStorageFileHandleImpl> CreateHandle(
      const CrossOriginStorageHash& hash,
      const url::Origin& origin,
      CrossOriginStorageAccessSpec access_spec,
      bool is_writable,
      bool may_read) {
    return std::make_unique<CrossOriginStorageFileHandleImpl>(
        registry()->GetWeakPtr(), blob_storage_context_,
        fsa_manager_->AsWeakPtr(), hash, origin, std::move(access_spec),
        is_writable, may_read);
  }

  void WaitForRegistryInitialization() {
    base::test::TestFuture<CrossOriginStorageOpenResult> init_barrier;
    registry()->OpenForRead(ComputeHashForString(""), kOriginA,
                            /*update_access_time=*/false,
                            init_barrier.GetCallback());
    CHECK(init_barrier.Wait());
  }

  base::SequenceBound<CrossOriginStorageStagingWriter> CreateStagingWriter(
      base::FilePath* out_staging_path = nullptr) {
    if (!out_staging_path) {
      base::test::TestFuture<
          base::SequenceBound<CrossOriginStorageStagingWriter>>
          create_future;
      registry()->CreateStagingWriter(
          blink::mojom::CrossOriginStorageAlgorithm::kSha256,
          create_future.GetCallback());
      base::SequenceBound<CrossOriginStorageStagingWriter> writer =
          create_future.Take();
      CHECK(writer);
      return writer;
    }
    // Ensure `StorageBackend::Initialize()`'s startup sweep of `staging/` has
    // completed before creating `*out_staging_path`.
    WaitForRegistryInitialization();

    base::FilePath staging_dir = browser_context_->GetPath()
                                     .AppendASCII("CrossOriginStorage")
                                     .AppendASCII("staging");
    CHECK(base::CreateDirectory(staging_dir));
    base::File file =
        base::CreateAndOpenTemporaryFileInDir(staging_dir, out_staging_path);
    CHECK(file.IsValid());
    return base::SequenceBound<CrossOriginStorageStagingWriter>(
        base::ThreadPool::CreateSequencedTaskRunner(
            {base::MayBlock(), base::TaskPriority::USER_VISIBLE}),
        blink::mojom::CrossOriginStorageAlgorithm::kSha256, *out_staging_path,
        std::move(file));
  }

  void CommitPayloadInRegistry(const url::Origin& origin,
                               const CrossOriginStorageHash& hash,
                               std::string_view payload,
                               CrossOriginStorageAccessSpec spec = {}) {
    base::test::TestFuture<base::SequenceBound<CrossOriginStorageStagingWriter>>
        create_future;
    registry()->CreateStagingWriter(hash.algorithm(),
                                    create_future.GetCallback());
    base::SequenceBound<CrossOriginStorageStagingWriter> writer =
        create_future.Take();
    ASSERT_TRUE(writer);

    std::vector<uint8_t> bytes(payload.begin(), payload.end());
    base::test::TestFuture<CrossOriginStorageStatus> append_future;
    writer.AsyncCall(&CrossOriginStorageStagingWriter::Append)
        .WithArgs(std::move(bytes))
        .Then(append_future.GetCallback());
    ASSERT_EQ(append_future.Get(), CrossOriginStorageStatus::kSuccess);

    base::test::TestFuture<CrossOriginStorageStagingWriter::Result>
        finish_future;
    writer.AsyncCall(&CrossOriginStorageStagingWriter::Finish)
        .WithArgs(nullptr)
        .Then(finish_future.GetCallback());
    CrossOriginStorageStagingWriter::Result result = finish_future.Take();
    ASSERT_EQ(result.status, CrossOriginStorageStatus::kSuccess);

    base::test::TestFuture<CrossOriginStorageStatus> commit_future;
    registry()->Commit(hash, origin, std::move(spec), std::move(result),
                       commit_future.GetCallback());
    ASSERT_EQ(commit_future.Get(), CrossOriginStorageStatus::kSuccess);
  }

  const url::Origin kOriginA =
      url::Origin::Create(GURL("https://a.example.com"));
  const url::Origin kOriginB =
      url::Origin::Create(GURL("https://b.other.test"));

  BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::IO};
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<TestBrowserContext> browser_context_;
  scoped_refptr<ChromeBlobStorageContext> blob_storage_context_;
  scoped_refptr<storage::MockQuotaManager> quota_manager_;
  scoped_refptr<storage::MockQuotaManagerProxy> quota_manager_proxy_;
  scoped_refptr<storage::FileSystemContext> file_system_context_;
  scoped_refptr<FileSystemAccessManagerImpl> fsa_manager_;
};

// =============================================================================
// CrossOriginStorageManagerImpl Tests
// =============================================================================

TEST_F(CrossOriginStorageImplTest,
       ManagerRejectsDisallowedStorageKeysAndOrigins) {
  const CrossOriginStorageHash kHash = ComputeHashForString("test payload");

  const blink::StorageKey kDisallowedKeys[] = {
      // Nonce-partitioned StorageKey (e.g., credentialless iframe / fenced
      // frame) where ForbidsUnpartitionedStorageAccess() is true.
      blink::StorageKey::CreateWithNonce(kOriginA,
                                         base::UnguessableToken::Create()),
      // Third-party partitioned StorageKey where IsThirdPartyContext() is true.
      blink::StorageKey::Create(kOriginA, net::SchemefulSite(kOriginB),
                                blink::mojom::AncestorChainBit::kCrossSite),
      // Opaque origin.
      blink::StorageKey::CreateFirstParty(url::Origin()),
      // Non-HTTP(S) origin.
      blink::StorageKey::CreateFirstParty(
          url::Origin::Create(GURL("file:///tmp/test.html"))),
      // Non-trustworthy HTTP origin.
      blink::StorageKey::CreateFirstParty(
          url::Origin::Create(GURL("http://insecure.example.com"))),
  };

  for (const blink::StorageKey& key : kDisallowedKeys) {
    std::unique_ptr<CrossOriginStorageManagerImpl> manager = CreateManager(key);

    // Read mode (`create_origins == nullptr`) must reject with
    // kPermissionDenied.
    {
      base::test::TestFuture<
          FileSystemAccessErrorPtr,
          mojo::PendingRemote<blink::mojom::FileSystemAccessFileHandle>>
          future;
      manager->GetFileHandle(ToMojoHash(kHash), /*create_origins=*/nullptr,
                             future.GetCallback());
      auto [error, handle] = future.Take();
      EXPECT_EQ(error->status, FileSystemAccessStatus::kPermissionDenied);
      EXPECT_FALSE(handle.is_valid());
    }

    // Write mode (`create_origins != nullptr`) must also reject with
    // kPermissionDenied.
    {
      base::test::TestFuture<
          FileSystemAccessErrorPtr,
          mojo::PendingRemote<blink::mojom::FileSystemAccessFileHandle>>
          future;
      manager->GetFileHandle(
          ToMojoHash(kHash),
          blink::mojom::CrossOriginStorageOrigins::NewOrigins({}),
          future.GetCallback());
      auto [error, handle] = future.Take();
      EXPECT_EQ(error->status, FileSystemAccessStatus::kPermissionDenied);
      EXPECT_FALSE(handle.is_valid());
    }
  }
}

TEST_F(CrossOriginStorageImplTest,
       ManagerReportsBadMessageOnMalformedMojoInputs) {
  const CrossOriginStorageHash kValidHash =
      ComputeHashForString("valid payload");

  auto bind_manager = [&]() {
    auto manager = CreateManager(blink::StorageKey::CreateFirstParty(kOriginA));
    mojo::Remote<blink::mojom::CrossOriginStorageManager> remote;
    manager->AddReceiver(remote.BindNewPipeAndPassReceiver());
    return std::make_pair(std::move(manager), std::move(remote));
  };

  // 1. Null hash struct passed to HandleGetFileHandle returns
  // "Missing CrossOriginStorage hash" (on a Mojo pipe, Mojom wire validation
  // rejects a null non-nullable struct before dispatch).
  {
    auto manager = CreateManager(blink::StorageKey::CreateFirstParty(kOriginA));
    blink::mojom::CrossOriginStorageManager::GetFileHandleCallback callback =
        base::DoNothing();
    EXPECT_EQ(manager->HandleGetFileHandle(
                  /*hash=*/nullptr, /*create_origins=*/nullptr, callback),
              "Missing CrossOriginStorage hash");
  }

  // 2. Invalid hash values (empty, wrong length, uppercase hex, non-hex char).
  const std::string kInvalidHashes[] = {
      "",
      std::string(63, 'a'),
      std::string(65, 'a'),
      "BA7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
      "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ag",
  };
  for (const std::string& invalid_hex : kInvalidHashes) {
    auto [manager, remote] = bind_manager();
    mojo::test::BadMessageObserver bad_message_observer;
    remote->GetFileHandle(
        blink::mojom::CrossOriginStorageHash::New(
            blink::mojom::CrossOriginStorageAlgorithm::kSha256, invalid_hex),
        /*create_origins=*/nullptr, base::DoNothing());
    EXPECT_EQ(bad_message_observer.WaitForBadMessage(),
              "Invalid CrossOriginStorage hash");
  }

  // 3. Invalid `create_origins` (opaque origin, untrustworthy HTTP origin,
  // non-HTTP(S) origin, and exceeding kMaxTargetOriginsLength).
  {
    auto [manager, remote] = bind_manager();
    mojo::test::BadMessageObserver bad_message_observer;
    remote->GetFileHandle(
        ToMojoHash(kValidHash),
        blink::mojom::CrossOriginStorageOrigins::NewOrigins({url::Origin()}),
        base::DoNothing());
    EXPECT_EQ(bad_message_observer.WaitForBadMessage(),
              "Invalid CrossOriginStorage access spec");
  }
  {
    auto [manager, remote] = bind_manager();
    mojo::test::BadMessageObserver bad_message_observer;
    remote->GetFileHandle(
        ToMojoHash(kValidHash),
        blink::mojom::CrossOriginStorageOrigins::NewOrigins(
            {url::Origin::Create(GURL("http://untrustworthy.example.com"))}),
        base::DoNothing());
    EXPECT_EQ(bad_message_observer.WaitForBadMessage(),
              "Invalid CrossOriginStorage access spec");
  }
  {
    auto [manager, remote] = bind_manager();
    mojo::test::BadMessageObserver bad_message_observer;
    remote->GetFileHandle(
        ToMojoHash(kValidHash),
        blink::mojom::CrossOriginStorageOrigins::NewOrigins(
            {url::Origin::Create(GURL("file:///tmp/test.html"))}),
        base::DoNothing());
    EXPECT_EQ(bad_message_observer.WaitForBadMessage(),
              "Invalid CrossOriginStorage access spec");
  }
  {
    auto [manager, remote] = bind_manager();
    mojo::test::BadMessageObserver bad_message_observer;
    std::vector<url::Origin> too_many_origins(
        cos_constants::kMaxTargetOriginsLength + 1, kOriginB);
    remote->GetFileHandle(ToMojoHash(kValidHash),
                          blink::mojom::CrossOriginStorageOrigins::NewOrigins(
                              std::move(too_many_origins)),
                          base::DoNothing());
    EXPECT_EQ(bad_message_observer.WaitForBadMessage(),
              "Invalid CrossOriginStorage access spec");
  }
}

TEST_F(CrossOriginStorageImplTest,
       ManagerFailsWhenRegistryOrBlobContextIsNull) {
  const CrossOriginStorageHash kHash = ComputeHashForString("payload");
  const blink::StorageKey kStorageKey =
      blink::StorageKey::CreateFirstParty(kOriginA);

  // Null registry.
  {
    CrossOriginStorageManagerImpl manager(
        /*registry=*/nullptr, blob_storage_context_, fsa_manager_->AsWeakPtr(),
        kStorageKey);
    base::test::TestFuture<
        FileSystemAccessErrorPtr,
        mojo::PendingRemote<blink::mojom::FileSystemAccessFileHandle>>
        future;
    manager.GetFileHandle(ToMojoHash(kHash), /*create_origins=*/nullptr,
                          future.GetCallback());
    auto [error, handle] = future.Take();
    EXPECT_EQ(error->status, FileSystemAccessStatus::kOperationFailed);
    EXPECT_FALSE(handle.is_valid());
  }

  // Null blob storage context.
  {
    CrossOriginStorageManagerImpl manager(
        registry()->GetWeakPtr(), /*blob_storage_context=*/nullptr,
        fsa_manager_->AsWeakPtr(), kStorageKey);
    base::test::TestFuture<
        FileSystemAccessErrorPtr,
        mojo::PendingRemote<blink::mojom::FileSystemAccessFileHandle>>
        future;
    manager.GetFileHandle(
        ToMojoHash(kHash),
        blink::mojom::CrossOriginStorageOrigins::NewOrigins({}),
        future.GetCallback());
    auto [error, handle] = future.Take();
    EXPECT_EQ(error->status, FileSystemAccessStatus::kOperationFailed);
    EXPECT_FALSE(handle.is_valid());
  }
}

TEST_F(CrossOriginStorageImplTest, ManagerGetFileHandleReadMissAndSuccess) {
  constexpr std::string_view kPayload = "committed content";
  const CrossOriginStorageHash kHash = ComputeHashForString(kPayload);
  auto manager_a = CreateManager(blink::StorageKey::CreateFirstParty(kOriginA));
  auto manager_b = CreateManager(blink::StorageKey::CreateFirstParty(kOriginB));

  // 1. Reading a non-existent entry maps kNotFound to FILE_ERROR_NOT_FOUND.
  {
    base::test::TestFuture<
        FileSystemAccessErrorPtr,
        mojo::PendingRemote<blink::mojom::FileSystemAccessFileHandle>>
        future;
    manager_a->GetFileHandle(ToMojoHash(kHash), /*create_origins=*/nullptr,
                             future.GetCallback());
    auto [error, handle] = future.Take();
    EXPECT_EQ(error->status, FileSystemAccessStatus::kFileError);
    EXPECT_EQ(error->file_error, base::File::FILE_ERROR_NOT_FOUND);
    EXPECT_FALSE(handle.is_valid());
  }

  // 2. Obtaining a writable handle via GetFileHandle (`create_origins !=
  // nullptr`) grants kReadWrite, starts unreadable (`AsBlob()` returns
  // kPermissionDenied), and commits via CreateFileWriter().
  {
    base::test::TestFuture<
        FileSystemAccessErrorPtr,
        mojo::PendingRemote<blink::mojom::FileSystemAccessFileHandle>>
        create_future;
    manager_a->GetFileHandle(
        ToMojoHash(kHash),
        blink::mojom::CrossOriginStorageOrigins::NewOrigins({kOriginB}),
        create_future.GetCallback());
    auto [create_err, pending_write_handle] = create_future.Take();
    ASSERT_EQ(create_err->status, FileSystemAccessStatus::kOk);
    ASSERT_TRUE(pending_write_handle.is_valid());

    mojo::Remote<blink::mojom::FileSystemAccessFileHandle> write_handle(
        std::move(pending_write_handle));
    base::test::TestFuture<PermissionStatus> write_perm;
    write_handle->GetPermissionStatus(
        FileSystemAccessPermissionMode::kReadWrite, write_perm.GetCallback());
    EXPECT_EQ(write_perm.Get(), PermissionStatus::GRANTED);

    base::test::TestFuture<FileSystemAccessErrorPtr, const base::File::Info&,
                           blink::mojom::SerializedBlobPtr>
        unwritten_blob_future;
    write_handle->AsBlob(unwritten_blob_future.GetCallback());
    EXPECT_EQ(unwritten_blob_future.Get<0>()->status,
              FileSystemAccessStatus::kPermissionDenied);

    base::test::TestFuture<
        FileSystemAccessErrorPtr,
        mojo::PendingRemote<blink::mojom::FileSystemAccessFileWriter>>
        writer_future;
    write_handle->CreateFileWriter(
        /*keep_existing_data=*/false, /*auto_close=*/false,
        blink::mojom::FileSystemAccessWritableFileStreamLockMode::kSiloed,
        writer_future.GetCallback());
    auto [writer_err, pending_writer] = writer_future.Take();
    ASSERT_EQ(writer_err->status, FileSystemAccessStatus::kOk);
    mojo::Remote<blink::mojom::FileSystemAccessFileWriter> writer(
        std::move(pending_writer));

    base::test::TestFuture<FileSystemAccessErrorPtr, uint64_t> write_future;
    writer->Write(0, CreateDataPipeWithPayload(kPayload),
                  write_future.GetCallback());
    ASSERT_EQ(write_future.Get<0>()->status, FileSystemAccessStatus::kOk);

    base::test::TestFuture<FileSystemAccessErrorPtr> close_future;
    writer->Close(close_future.GetCallback());
    ASSERT_EQ(close_future.Get()->status, FileSystemAccessStatus::kOk);
  }

  // 3. Both `kOriginA` (the writer) and `kOriginB` (granted via `origins`) can
  // now open the entry in read mode and read the committed bytes.
  for (CrossOriginStorageManagerImpl* mgr :
       {manager_a.get(), manager_b.get()}) {
    base::test::TestFuture<
        FileSystemAccessErrorPtr,
        mojo::PendingRemote<blink::mojom::FileSystemAccessFileHandle>>
        future;
    mgr->GetFileHandle(ToMojoHash(kHash), /*create_origins=*/nullptr,
                       future.GetCallback());
    auto [error, pending_handle] = future.Take();
    ASSERT_EQ(error->status, FileSystemAccessStatus::kOk);
    ASSERT_TRUE(pending_handle.is_valid());

    mojo::Remote<blink::mojom::FileSystemAccessFileHandle> handle(
        std::move(pending_handle));
    base::test::TestFuture<PermissionStatus> read_perm;
    handle->GetPermissionStatus(FileSystemAccessPermissionMode::kRead,
                                read_perm.GetCallback());
    EXPECT_EQ(read_perm.Get(), PermissionStatus::GRANTED);

    base::test::TestFuture<PermissionStatus> write_perm;
    handle->GetPermissionStatus(FileSystemAccessPermissionMode::kReadWrite,
                                write_perm.GetCallback());
    EXPECT_EQ(write_perm.Get(), PermissionStatus::DENIED);

    base::test::TestFuture<FileSystemAccessErrorPtr, const base::File::Info&,
                           blink::mojom::SerializedBlobPtr>
        blob_future;
    handle->AsBlob(blob_future.GetCallback());
    auto [blob_err, blob_info, blob] = blob_future.Take();
    ASSERT_EQ(blob_err->status, FileSystemAccessStatus::kOk);
    ASSERT_TRUE(blob);
    EXPECT_EQ(ReadBlobContents(std::move(blob->blob)), kPayload);
  }

  // 4. Obtaining a writable handle with `NewAnyOrigin` (`origins: '*'`)
  // commits a globally disclosable ('*') grant (WICG § 2.2 & § 3.5) that
  // `kOriginB` can open in read mode.
  {
    constexpr std::string_view kWildcardPayload = "wildcard content";
    const CrossOriginStorageHash kWildcardHash =
        ComputeHashForString(kWildcardPayload);

    base::test::TestFuture<
        FileSystemAccessErrorPtr,
        mojo::PendingRemote<blink::mojom::FileSystemAccessFileHandle>>
        create_future;
    manager_a->GetFileHandle(
        ToMojoHash(kWildcardHash),
        blink::mojom::CrossOriginStorageOrigins::NewAnyOrigin(
            blink::mojom::CrossOriginStorageAnyOrigin::New()),
        create_future.GetCallback());
    auto [create_err, pending_write_handle] = create_future.Take();
    ASSERT_EQ(create_err->status, FileSystemAccessStatus::kOk);
    ASSERT_TRUE(pending_write_handle.is_valid());

    mojo::Remote<blink::mojom::FileSystemAccessFileHandle> write_handle(
        std::move(pending_write_handle));
    base::test::TestFuture<
        FileSystemAccessErrorPtr,
        mojo::PendingRemote<blink::mojom::FileSystemAccessFileWriter>>
        writer_future;
    write_handle->CreateFileWriter(
        /*keep_existing_data=*/false, /*auto_close=*/false,
        blink::mojom::FileSystemAccessWritableFileStreamLockMode::kSiloed,
        writer_future.GetCallback());
    auto [writer_err, pending_writer] = writer_future.Take();
    ASSERT_EQ(writer_err->status, FileSystemAccessStatus::kOk);
    mojo::Remote<blink::mojom::FileSystemAccessFileWriter> writer(
        std::move(pending_writer));

    base::test::TestFuture<FileSystemAccessErrorPtr, uint64_t> write_future;
    writer->Write(0, CreateDataPipeWithPayload(kWildcardPayload),
                  write_future.GetCallback());
    ASSERT_EQ(write_future.Get<0>()->status, FileSystemAccessStatus::kOk);

    base::test::TestFuture<FileSystemAccessErrorPtr> close_future;
    writer->Close(close_future.GetCallback());
    ASSERT_EQ(close_future.Get()->status, FileSystemAccessStatus::kOk);

    base::test::TestFuture<
        FileSystemAccessErrorPtr,
        mojo::PendingRemote<blink::mojom::FileSystemAccessFileHandle>>
        read_future;
    manager_b->GetFileHandle(ToMojoHash(kWildcardHash),
                             /*create_origins=*/nullptr,
                             read_future.GetCallback());
    auto [read_err, pending_read_handle] = read_future.Take();
    ASSERT_EQ(read_err->status, FileSystemAccessStatus::kOk);
    ASSERT_TRUE(pending_read_handle.is_valid());
  }

  // 5. When `OpenForRead` returns a status other than `kSuccess` or `kNotFound`
  // (e.g., `kOperationFailed` when `<profile>/CrossOriginStorage` is blocked by
  // a regular file so `StorageBackend::Initialize()` fails), both
  // `CrossOriginStorageManagerImpl::OnOpenedForRead` and
  // `CrossOriginStorageFileHandleImpl::OnOpenedForRead` map the status to
  // `base::File::FILE_ERROR_FAILED`.
  {
    TestBrowserContext broken_context;
    ASSERT_TRUE(base::WriteFile(
        broken_context.GetPath().AppendASCII("CrossOriginStorage"),
        "blocking file"));
    CrossOriginStorageRegistry* broken_reg =
        CrossOriginStorageRegistry::GetOrCreateForBrowserContext(
            &broken_context);
    ASSERT_TRUE(broken_reg);

    CrossOriginStorageManagerImpl broken_manager(
        broken_reg->GetWeakPtr(), blob_storage_context_,
        fsa_manager_->AsWeakPtr(),
        blink::StorageKey::CreateFirstParty(kOriginA));
    base::test::TestFuture<
        FileSystemAccessErrorPtr,
        mojo::PendingRemote<blink::mojom::FileSystemAccessFileHandle>>
        mgr_future;
    broken_manager.GetFileHandle(ToMojoHash(kHash), /*create_origins=*/nullptr,
                                 mgr_future.GetCallback());
    auto [mgr_err, mgr_handle] = mgr_future.Take();
    EXPECT_EQ(mgr_err->status, FileSystemAccessStatus::kFileError);
    EXPECT_EQ(mgr_err->file_error, base::File::FILE_ERROR_FAILED);
    EXPECT_FALSE(mgr_handle.is_valid());

    CrossOriginStorageFileHandleImpl broken_handle(
        broken_reg->GetWeakPtr(), blob_storage_context_,
        fsa_manager_->AsWeakPtr(), kHash, kOriginA,
        CrossOriginStorageAccessSpec(), /*is_writable=*/false,
        /*may_read=*/true);
    base::test::TestFuture<FileSystemAccessErrorPtr, const base::File::Info&,
                           blink::mojom::SerializedBlobPtr>
        blob_future;
    broken_handle.AsBlob(blob_future.GetCallback());
    auto [blob_err, blob_info, blob] = blob_future.Take();
    EXPECT_EQ(blob_err->status, FileSystemAccessStatus::kFileError);
    EXPECT_EQ(blob_err->file_error, base::File::FILE_ERROR_FAILED);
    EXPECT_FALSE(blob);

    broken_reg->SynchronouslyResetBackendForTesting();
  }
}

class CrossOriginStorageBinderTest : public RenderViewHostTestHarness {
 protected:
  void TearDown() override {
    if (browser_context()) {
      if (auto* reg = CrossOriginStorageRegistry::GetOrCreateForBrowserContext(
              browser_context())) {
        reg->SynchronouslyResetBackendForTesting();
      }
    }
    RenderViewHostTestHarness::TearDown();
  }
};

TEST_F(CrossOriginStorageBinderTest, BindForWorkerValidationAndReceiverReuse) {
  const url::Origin kOriginA =
      url::Origin::Create(GURL("https://a.example.com"));
  const url::Origin kOriginB =
      url::Origin::Create(GURL("https://b.other.test"));
  const blink::StorageKey kValidKey =
      blink::StorageKey::CreateFirstParty(kOriginA);
  const blink::StorageKey kMismatchedKey =
      blink::StorageKey::CreateFirstParty(kOriginB);

  // Navigate `main_rfh()` to `kOriginA` so `process()` is locked to `kOriginA`
  // in `ChildProcessSecurityPolicy`.
  NavigateAndCommit(GURL("https://a.example.com/index.html"));
  RenderProcessHost* locked_process_host = main_rfh()->GetProcess();
  DummyWorkerHost worker_host;

  // 1. Null worker_host or null process_host drops the receiver cleanly.
  {
    mojo::Remote<blink::mojom::CrossOriginStorageManager> remote;
    CrossOriginStorageManagerImpl::BindForWorker(
        /*worker_host=*/nullptr, locked_process_host, kValidKey,
        remote.BindNewPipeAndPassReceiver());
    remote.FlushForTesting();
    EXPECT_FALSE(remote.is_connected());
  }
  {
    mojo::Remote<blink::mojom::CrossOriginStorageManager> remote;
    CrossOriginStorageManagerImpl::BindForWorker(
        &worker_host, /*process_host=*/nullptr, kValidKey,
        remote.BindNewPipeAndPassReceiver());
    remote.FlushForTesting();
    EXPECT_FALSE(remote.is_connected());
  }

  // 2. Mismatched origin (`kOriginB` on a process locked to `kOriginA`) fails
  // ChildProcessSecurityPolicy::CanAccessDataForOrigin and drops the receiver.
  {
    mojo::Remote<blink::mojom::CrossOriginStorageManager> remote;
    CrossOriginStorageManagerImpl::BindForWorker(
        &worker_host, locked_process_host, kMismatchedKey,
        remote.BindNewPipeAndPassReceiver());
    remote.FlushForTesting();
    EXPECT_FALSE(remote.is_connected());
  }

  // 3. In-memory or non-default StoragePartitionConfig drops the receiver
  // (using an opaque-origin StorageKey so CanAccessDataForOrigin passes on the
  // unlocked MockRenderProcessHost and execution reaches the
  // StoragePartitionConfig check).
  {
    StoragePartitionConfig in_memory_config = StoragePartitionConfig::Create(
        browser_context(), "custom_domain", "custom_partition",
        /*in_memory=*/true);
    MockRenderProcessHost in_memory_process_host(browser_context(),
                                                 in_memory_config,
                                                 /*is_for_guests_only=*/false);
    mojo::Remote<blink::mojom::CrossOriginStorageManager> remote;
    CrossOriginStorageManagerImpl::BindForWorker(
        &worker_host, &in_memory_process_host,
        blink::StorageKey::CreateFirstParty(url::Origin()),
        remote.BindNewPipeAndPassReceiver());
    remote.FlushForTesting();
    EXPECT_FALSE(remote.is_connected());
  }
  {
    StoragePartitionConfig non_default_persistent_config =
        StoragePartitionConfig::Create(browser_context(), "custom_domain",
                                       "custom_partition",
                                       /*in_memory=*/false);
    MockRenderProcessHost custom_process_host(browser_context(),
                                              non_default_persistent_config,
                                              /*is_for_guests_only=*/false);
    mojo::Remote<blink::mojom::CrossOriginStorageManager> remote;
    CrossOriginStorageManagerImpl::BindForWorker(
        &worker_host, &custom_process_host,
        blink::StorageKey::CreateFirstParty(url::Origin()),
        remote.BindNewPipeAndPassReceiver());
    remote.FlushForTesting();
    EXPECT_FALSE(remote.is_connected());
  }

  // 4. Off-the-record BrowserContext drops the receiver.
  {
    TestBrowserContext otr_context;
    otr_context.set_is_off_the_record(true);
    MockRenderProcessHost otr_process_host(&otr_context);
    DummyWorkerHost otr_worker_host;
    mojo::Remote<blink::mojom::CrossOriginStorageManager> remote;
    CrossOriginStorageManagerImpl::BindForWorker(
        &otr_worker_host, &otr_process_host,
        blink::StorageKey::CreateFirstParty(url::Origin()),
        remote.BindNewPipeAndPassReceiver());
    remote.FlushForTesting();
    EXPECT_FALSE(remote.is_connected());
  }

  // 5. Valid worker binding succeeds, attaches to worker_host user data, and
  // reuses the existing manager for subsequent receivers (while still enforcing
  // CanAccessDataForOrigin on reuse and isolating bad-message disconnects to
  // the offending receiver).
  {
    const CrossOriginStorageHash kValidHash =
        ComputeHashForString("worker payload");
    mojo::Remote<blink::mojom::CrossOriginStorageManager> remote1;
    CrossOriginStorageManagerImpl::BindForWorker(
        &worker_host, locked_process_host, kValidKey,
        remote1.BindNewPipeAndPassReceiver());
    remote1.FlushForTesting();
    EXPECT_TRUE(remote1.is_connected());

    mojo::Remote<blink::mojom::CrossOriginStorageManager> remote2;
    CrossOriginStorageManagerImpl::BindForWorker(
        &worker_host, locked_process_host, kValidKey,
        remote2.BindNewPipeAndPassReceiver());
    remote2.FlushForTesting();
    EXPECT_TRUE(remote2.is_connected());

    // Attempting to bind a receiver with a mismatched origin (`kOriginB`) on
    // the same worker_host must still fail CanAccessDataForOrigin and drop the
    // new receiver without disconnecting remote1 or remote2.
    mojo::Remote<blink::mojom::CrossOriginStorageManager> bad_origin_remote;
    CrossOriginStorageManagerImpl::BindForWorker(
        &worker_host, locked_process_host, kMismatchedKey,
        bad_origin_remote.BindNewPipeAndPassReceiver());
    bad_origin_remote.FlushForTesting();
    EXPECT_FALSE(bad_origin_remote.is_connected());
    EXPECT_TRUE(remote1.is_connected());
    EXPECT_TRUE(remote2.is_connected());

    // Verify RPC execution on `remote1`.
    {
      base::test::TestFuture<
          FileSystemAccessErrorPtr,
          mojo::PendingRemote<blink::mojom::FileSystemAccessFileHandle>>
          future;
      remote1->GetFileHandle(
          ToMojoHash(kValidHash),
          blink::mojom::CrossOriginStorageOrigins::NewOrigins({}),
          future.GetCallback());
      auto [error, handle] = future.Take();
      EXPECT_EQ(error->status, FileSystemAccessStatus::kOk);
      EXPECT_TRUE(handle.is_valid());
    }

    // Sending a malformed hash on `remote1` triggers
    // `receivers_.ReportBadMessage` and disconnects only `remote1`, while
    // `remote2` remains connected and functional.
    {
      mojo::test::BadMessageObserver bad_message_observer;
      remote1->GetFileHandle(
          blink::mojom::CrossOriginStorageHash::New(
              blink::mojom::CrossOriginStorageAlgorithm::kSha256, "bad-hex"),
          /*create_origins=*/nullptr, base::DoNothing());
      EXPECT_EQ(bad_message_observer.WaitForBadMessage(),
                "Invalid CrossOriginStorage hash");
      remote1.FlushForTesting();
      EXPECT_FALSE(remote1.is_connected());
      EXPECT_TRUE(remote2.is_connected());

      base::test::TestFuture<
          FileSystemAccessErrorPtr,
          mojo::PendingRemote<blink::mojom::FileSystemAccessFileHandle>>
          future2;
      remote2->GetFileHandle(
          ToMojoHash(kValidHash),
          blink::mojom::CrossOriginStorageOrigins::NewOrigins({}),
          future2.GetCallback());
      auto [error2, handle2] = future2.Take();
      EXPECT_EQ(error2->status, FileSystemAccessStatus::kOk);
      EXPECT_TRUE(handle2.is_valid());
    }
  }
}

TEST_F(CrossOriginStorageBinderTest,
       BindForFrameValidationAndBadMessageDeletesService) {
  const CrossOriginStorageHash kValidHash =
      ComputeHashForString("frame payload");

  // 1. Null RenderFrameHost drops the receiver cleanly.
  {
    mojo::Remote<blink::mojom::CrossOriginStorageManager> remote;
    CrossOriginStorageManagerImpl::BindForFrame(
        /*frame_host=*/nullptr, remote.BindNewPipeAndPassReceiver());
    remote.FlushForTesting();
    EXPECT_FALSE(remote.is_connected());
  }

  // 2. Verify that when `CreateManagerIfAllowed` fails (e.g., an off-the-record
  // BrowserContext whose default StoragePartitionConfig is in-memory),
  // `BindForFrame` drops the receiver cleanly.
  {
    auto* test_browser_context =
        static_cast<TestBrowserContext*>(browser_context());
    test_browser_context->set_is_off_the_record(true);
    std::unique_ptr<WebContents> otr_contents = CreateTestWebContents();
    mojo::Remote<blink::mojom::CrossOriginStorageManager> otr_remote;
    CrossOriginStorageManagerImpl::BindForFrame(
        otr_contents->GetPrimaryMainFrame(),
        otr_remote.BindNewPipeAndPassReceiver());
    otr_remote.FlushForTesting();
    EXPECT_FALSE(otr_remote.is_connected());
    otr_contents.reset();
    test_browser_context->set_is_off_the_record(false);
  }

  // 3. Navigate to a trustworthy HTTPS origin in a normal BrowserContext, bind
  // a frame manager, and verify that a valid GetFileHandle call succeeds
  // through FrameCrossOriginStorageManager.
  NavigateAndCommit(GURL("https://a.example.com/index.html"));
  mojo::Remote<blink::mojom::CrossOriginStorageManager> remote;
  CrossOriginStorageManagerImpl::BindForFrame(
      main_rfh(), remote.BindNewPipeAndPassReceiver());
  remote.FlushForTesting();
  ASSERT_TRUE(remote.is_connected());
  {
    base::test::TestFuture<
        FileSystemAccessErrorPtr,
        mojo::PendingRemote<blink::mojom::FileSystemAccessFileHandle>>
        future;
    remote->GetFileHandle(
        ToMojoHash(kValidHash),
        blink::mojom::CrossOriginStorageOrigins::NewOrigins({}),
        future.GetCallback());
    auto [error, handle] = future.Take();
    EXPECT_EQ(error->status, FileSystemAccessStatus::kOk);
    EXPECT_TRUE(handle.is_valid());
  }

  // 4. Navigating the frame to a new document (with BackForwardCache disabled
  // so the previous RenderFrameHost is not cached) destroys the document-scoped
  // DocumentService and disconnects `remote`.
  web_contents()->GetController().GetBackForwardCache().DisableForTesting(
      BackForwardCache::TEST_REQUIRES_NO_CACHING);
  NavigateAndCommit(GURL("https://b.other.test/index.html"));
  remote.FlushForTesting();
  EXPECT_FALSE(remote.is_connected());

  // 5. Re-bind on the new document and verify that sending a malformed hash
  // triggers ReportBadMessageAndDeleteThis on the DocumentService and
  // disconnects the Mojo pipe.
  mojo::Remote<blink::mojom::CrossOriginStorageManager> remote2;
  CrossOriginStorageManagerImpl::BindForFrame(
      main_rfh(), remote2.BindNewPipeAndPassReceiver());
  remote2.FlushForTesting();
  ASSERT_TRUE(remote2.is_connected());

  mojo::test::BadMessageObserver bad_message_observer;
  remote2->GetFileHandle(
      blink::mojom::CrossOriginStorageHash::New(
          blink::mojom::CrossOriginStorageAlgorithm::kSha256, "invalid-hex"),
      /*create_origins=*/nullptr, base::DoNothing());
  EXPECT_EQ(bad_message_observer.WaitForBadMessage(),
            "Invalid CrossOriginStorage hash");
  remote2.FlushForTesting();
  EXPECT_FALSE(remote2.is_connected());
}

// =============================================================================
// CrossOriginStorageFileHandleImpl Tests
// =============================================================================

TEST_F(CrossOriginStorageImplTest,
       FileHandleGetPermissionStatusAndRequestPermission) {
  const CrossOriginStorageHash kHash = ComputeHashForString("payload");

  // Read-only handle: kRead is GRANTED, kReadWrite is DENIED.
  auto read_only_handle =
      CreateHandle(kHash, kOriginA, CrossOriginStorageAccessSpec(),
                   /*is_writable=*/false, /*may_read=*/true);
  {
    base::test::TestFuture<PermissionStatus> status_future;
    read_only_handle->GetPermissionStatus(FileSystemAccessPermissionMode::kRead,
                                          status_future.GetCallback());
    EXPECT_EQ(status_future.Get(), PermissionStatus::GRANTED);

    base::test::TestFuture<PermissionStatus> write_status_future;
    read_only_handle->GetPermissionStatus(
        FileSystemAccessPermissionMode::kReadWrite,
        write_status_future.GetCallback());
    EXPECT_EQ(write_status_future.Get(), PermissionStatus::DENIED);

    base::test::TestFuture<FileSystemAccessErrorPtr, PermissionStatus>
        req_read_future;
    read_only_handle->RequestPermission(FileSystemAccessPermissionMode::kRead,
                                        req_read_future.GetCallback());
    EXPECT_EQ(req_read_future.Get<0>()->status, FileSystemAccessStatus::kOk);
    EXPECT_EQ(req_read_future.Get<1>(), PermissionStatus::GRANTED);

    base::test::TestFuture<FileSystemAccessErrorPtr, PermissionStatus>
        req_write_future;
    read_only_handle->RequestPermission(
        FileSystemAccessPermissionMode::kReadWrite,
        req_write_future.GetCallback());
    EXPECT_EQ(req_write_future.Get<0>()->status, FileSystemAccessStatus::kOk);
    EXPECT_EQ(req_write_future.Get<1>(), PermissionStatus::DENIED);
  }

  // Writable handle: both kRead and kReadWrite are GRANTED.
  auto writable_handle =
      CreateHandle(kHash, kOriginA, CrossOriginStorageAccessSpec(),
                   /*is_writable=*/true, /*may_read=*/false);
  {
    base::test::TestFuture<PermissionStatus> write_status_future;
    writable_handle->GetPermissionStatus(
        FileSystemAccessPermissionMode::kReadWrite,
        write_status_future.GetCallback());
    EXPECT_EQ(write_status_future.Get(), PermissionStatus::GRANTED);

    base::test::TestFuture<FileSystemAccessErrorPtr, PermissionStatus>
        req_write_future;
    writable_handle->RequestPermission(
        FileSystemAccessPermissionMode::kReadWrite,
        req_write_future.GetCallback());
    EXPECT_EQ(req_write_future.Get<0>()->status, FileSystemAccessStatus::kOk);
    EXPECT_EQ(req_write_future.Get<1>(), PermissionStatus::GRANTED);
  }
}

TEST_F(CrossOriginStorageImplTest, FileHandleAsBlobErrorAndSuccessPaths) {
  constexpr std::string_view kPayload = "hello cross-origin storage";
  const CrossOriginStorageHash kHash = ComputeHashForString(kPayload);
  const CrossOriginStorageHash kEmptyHash = ComputeHashForString("");

  // 1. Unwritten create handle (`may_read == false`) rejects AsBlob() with
  // kPermissionDenied so `{create: true}` cannot act as an existence oracle.
  {
    auto unwritten_handle =
        CreateHandle(kHash, kOriginA, CrossOriginStorageAccessSpec(),
                     /*is_writable=*/true, /*may_read=*/false);
    base::test::TestFuture<FileSystemAccessErrorPtr, const base::File::Info&,
                           blink::mojom::SerializedBlobPtr>
        future;
    unwritten_handle->AsBlob(future.GetCallback());
    auto [error, info, blob] = future.Take();
    EXPECT_EQ(error->status, FileSystemAccessStatus::kPermissionDenied);
    EXPECT_FALSE(blob);
  }

  // 2. Null registry or null blob_storage_context rejects AsBlob() with
  // kOperationFailed.
  {
    CrossOriginStorageFileHandleImpl handle_no_registry(
        /*registry=*/nullptr, blob_storage_context_, fsa_manager_->AsWeakPtr(),
        kHash, kOriginA, CrossOriginStorageAccessSpec(),
        /*is_writable=*/false, /*may_read=*/true);
    base::test::TestFuture<FileSystemAccessErrorPtr, const base::File::Info&,
                           blink::mojom::SerializedBlobPtr>
        future;
    handle_no_registry.AsBlob(future.GetCallback());
    EXPECT_EQ(future.Get<0>()->status,
              FileSystemAccessStatus::kOperationFailed);
  }
  {
    CrossOriginStorageFileHandleImpl handle_no_blob_ctx(
        registry()->GetWeakPtr(), /*blob_storage_context=*/nullptr,
        fsa_manager_->AsWeakPtr(), kHash, kOriginA,
        CrossOriginStorageAccessSpec(),
        /*is_writable=*/false, /*may_read=*/true);
    base::test::TestFuture<FileSystemAccessErrorPtr, const base::File::Info&,
                           blink::mojom::SerializedBlobPtr>
        future;
    handle_no_blob_ctx.AsBlob(future.GetCallback());
    EXPECT_EQ(future.Get<0>()->status,
              FileSystemAccessStatus::kOperationFailed);
  }

  // 3. Missing/evicted file in registry maps kNotFound to FILE_ERROR_NOT_FOUND.
  {
    auto missing_handle =
        CreateHandle(kHash, kOriginA, CrossOriginStorageAccessSpec(),
                     /*is_writable=*/false, /*may_read=*/true);
    base::test::TestFuture<FileSystemAccessErrorPtr, const base::File::Info&,
                           blink::mojom::SerializedBlobPtr>
        future;
    missing_handle->AsBlob(future.GetCallback());
    auto [error, info, blob] = future.Take();
    EXPECT_EQ(error->status, FileSystemAccessStatus::kFileError);
    EXPECT_EQ(error->file_error, base::File::FILE_ERROR_NOT_FOUND);
    EXPECT_FALSE(blob);
  }

  // 4. Committed non-empty and 0-byte files succeed with
  // "application/octet-stream", UnixEpoch() last_modified timestamp, and exact
  // payload bytes.
  CommitPayloadInRegistry(kOriginA, kHash, kPayload);
  CommitPayloadInRegistry(kOriginA, kEmptyHash, "");
  {
    auto handle = CreateHandle(kHash, kOriginA, CrossOriginStorageAccessSpec(),
                               /*is_writable=*/false, /*may_read=*/true);
    base::test::TestFuture<FileSystemAccessErrorPtr, const base::File::Info&,
                           blink::mojom::SerializedBlobPtr>
        future;
    handle->AsBlob(future.GetCallback());
    auto [error, info, blob] = future.Take();
    ASSERT_EQ(error->status, FileSystemAccessStatus::kOk);
    EXPECT_EQ(info.size, static_cast<int64_t>(kPayload.size()));
    EXPECT_FALSE(info.is_directory);
    EXPECT_EQ(info.last_modified, base::Time::UnixEpoch());
    ASSERT_TRUE(blob);
    EXPECT_EQ(blob->content_type, "application/octet-stream");
    EXPECT_EQ(blob->size, kPayload.size());
    ASSERT_TRUE(blob->blob.is_valid());
    EXPECT_EQ(ReadBlobContents(std::move(blob->blob)), kPayload);
  }
  {
    auto empty_handle =
        CreateHandle(kEmptyHash, kOriginA, CrossOriginStorageAccessSpec(),
                     /*is_writable=*/false, /*may_read=*/true);
    base::test::TestFuture<FileSystemAccessErrorPtr, const base::File::Info&,
                           blink::mojom::SerializedBlobPtr>
        future;
    empty_handle->AsBlob(future.GetCallback());
    auto [error, info, blob] = future.Take();
    ASSERT_EQ(error->status, FileSystemAccessStatus::kOk);
    EXPECT_EQ(info.size, 0);
    EXPECT_EQ(info.last_modified, base::Time::UnixEpoch());
    ASSERT_TRUE(blob);
    EXPECT_EQ(blob->content_type, "application/octet-stream");
    EXPECT_EQ(blob->size, 0u);
    ASSERT_TRUE(blob->blob.is_valid());
    EXPECT_EQ(ReadBlobContents(std::move(blob->blob)), "");
  }
}

TEST_F(CrossOriginStorageImplTest, FileHandleCreateFileWriterErrorPaths) {
  const CrossOriginStorageHash kHash = ComputeHashForString("payload");

  // 1. Read-only handle rejects CreateFileWriter with kPermissionDenied.
  {
    auto read_only_handle =
        CreateHandle(kHash, kOriginA, CrossOriginStorageAccessSpec(),
                     /*is_writable=*/false, /*may_read=*/true);
    base::test::TestFuture<
        FileSystemAccessErrorPtr,
        mojo::PendingRemote<blink::mojom::FileSystemAccessFileWriter>>
        future;
    read_only_handle->CreateFileWriter(
        /*keep_existing_data=*/false, /*auto_close=*/false,
        blink::mojom::FileSystemAccessWritableFileStreamLockMode::kSiloed,
        future.GetCallback());
    auto [error, writer] = future.Take();
    EXPECT_EQ(error->status, FileSystemAccessStatus::kPermissionDenied);
    EXPECT_FALSE(writer.is_valid());
  }

  // 2. Null registry rejects CreateFileWriter with kOperationFailed.
  {
    CrossOriginStorageFileHandleImpl handle_no_registry(
        /*registry=*/nullptr, blob_storage_context_, fsa_manager_->AsWeakPtr(),
        kHash, kOriginA, CrossOriginStorageAccessSpec(),
        /*is_writable=*/true, /*may_read=*/false);
    base::test::TestFuture<
        FileSystemAccessErrorPtr,
        mojo::PendingRemote<blink::mojom::FileSystemAccessFileWriter>>
        future;
    handle_no_registry.CreateFileWriter(
        /*keep_existing_data=*/false, /*auto_close=*/false,
        blink::mojom::FileSystemAccessWritableFileStreamLockMode::kSiloed,
        future.GetCallback());
    auto [error, writer] = future.Take();
    EXPECT_EQ(error->status, FileSystemAccessStatus::kOperationFailed);
    EXPECT_FALSE(writer.is_valid());
  }

  // 3. When staging file creation fails on disk (e.g., staging directory is
  // replaced by a regular file), CreateFileWriter returns kOperationFailed.
  {
    auto writable_handle =
        CreateHandle(kHash, kOriginA, CrossOriginStorageAccessSpec(),
                     /*is_writable=*/true, /*may_read=*/false);
    // Ensure registry initialization completes first, then replace `staging`
    // directory with a regular file so `StorageBackend::CreateStagingWriter` /
    // `base::CreateDirectory(StagingDirectory())` fails.
    CommitPayloadInRegistry(kOriginA, kHash, "payload");
    base::FilePath staging_dir = browser_context_->GetPath()
                                     .AppendASCII("CrossOriginStorage")
                                     .AppendASCII("staging");
    ASSERT_TRUE(base::DeletePathRecursively(staging_dir));
    ASSERT_TRUE(base::WriteFile(staging_dir, "not a directory"));

    base::test::TestFuture<
        FileSystemAccessErrorPtr,
        mojo::PendingRemote<blink::mojom::FileSystemAccessFileWriter>>
        future;
    writable_handle->CreateFileWriter(
        /*keep_existing_data=*/false, /*auto_close=*/false,
        blink::mojom::FileSystemAccessWritableFileStreamLockMode::kSiloed,
        future.GetCallback());
    auto [error, writer] = future.Take();
    EXPECT_EQ(error->status, FileSystemAccessStatus::kOperationFailed);
    EXPECT_FALSE(writer.is_valid());
    ASSERT_TRUE(base::DeleteFile(staging_dir));
  }
}

TEST_F(CrossOriginStorageImplTest,
       FileHandleHardBlockedAndUnsupportedOperations) {
  const CrossOriginStorageHash kHash = ComputeHashForString("payload");
  auto handle = CreateHandle(kHash, kOriginA, CrossOriginStorageAccessSpec(),
                             /*is_writable=*/true, /*may_read=*/true);

  // Move, Rename, and Remove must return kPermissionDenied.
  {
    base::test::TestFuture<FileSystemAccessErrorPtr> move_future;
    handle->Move(mojo::NullRemote(), "new_name.txt", move_future.GetCallback());
    EXPECT_EQ(move_future.Get()->status,
              FileSystemAccessStatus::kPermissionDenied);

    base::test::TestFuture<FileSystemAccessErrorPtr> rename_future;
    handle->Rename("new_name.txt", rename_future.GetCallback());
    EXPECT_EQ(rename_future.Get()->status,
              FileSystemAccessStatus::kPermissionDenied);

    base::test::TestFuture<FileSystemAccessErrorPtr> remove_future;
    handle->Remove(remove_future.GetCallback());
    EXPECT_EQ(remove_future.Get()->status,
              FileSystemAccessStatus::kPermissionDenied);
  }

  // OpenAccessHandle must return kInvalidState so a renderer can never obtain a
  // direct OS file descriptor to a shared Cross-Origin Storage file.
  {
    base::test::TestFuture<
        FileSystemAccessErrorPtr,
        blink::mojom::FileSystemAccessAccessHandleFilePtr,
        mojo::PendingRemote<blink::mojom::FileSystemAccessAccessHandleHost>>
        access_future;
    handle->OpenAccessHandle(
        blink::mojom::FileSystemAccessAccessHandleLockMode::kReadOnly,
        access_future.GetCallback());
    auto [error, file, host] = access_future.Take();
    EXPECT_EQ(error->status, FileSystemAccessStatus::kInvalidState);
    EXPECT_FALSE(file);
    EXPECT_FALSE(host.is_valid());
  }

  // GetUniqueId and GetCloudIdentifiers must return kNotSupportedError.
  {
    base::test::TestFuture<FileSystemAccessErrorPtr, const std::string&>
        id_future;
    handle->GetUniqueId(id_future.GetCallback());
    EXPECT_EQ(id_future.Get<0>()->status,
              FileSystemAccessStatus::kNotSupportedError);
    EXPECT_TRUE(id_future.Get<1>().empty());

    base::test::TestFuture<
        FileSystemAccessErrorPtr,
        std::vector<blink::mojom::FileSystemAccessCloudIdentifierPtr>>
        cloud_future;
    handle->GetCloudIdentifiers(cloud_future.GetCallback());
    EXPECT_EQ(cloud_future.Get<0>()->status,
              FileSystemAccessStatus::kNotSupportedError);
    EXPECT_TRUE(cloud_future.Get<1>().empty());
  }
}

TEST_F(CrossOriginStorageImplTest,
       FileHandleTransferAndIsSameEntryValidationAndMayReadCopy) {
  constexpr std::string_view kPayload1 = "payload one";
  constexpr std::string_view kPayload2 = "payload two";
  const CrossOriginStorageHash kHash1 = ComputeHashForString(kPayload1);
  const CrossOriginStorageHash kHash2 = ComputeHashForString(kPayload2);

  auto handle1 = CreateHandle(kHash1, kOriginA, CrossOriginStorageAccessSpec(),
                              /*is_writable=*/true, /*may_read=*/false);
  auto handle2 = CreateHandle(kHash2, kOriginA, CrossOriginStorageAccessSpec(),
                              /*is_writable=*/false, /*may_read=*/true);
  auto cross_origin_handle =
      CreateHandle(kHash1, kOriginB, CrossOriginStorageAccessSpec(),
                   /*is_writable=*/false, /*may_read=*/true);

  // 1. IsSameEntry with invalid token or null fsa_manager returns
  // kPermissionDenied.
  {
    base::test::TestFuture<FileSystemAccessErrorPtr, bool> future;
    handle1->IsSameEntry(mojo::NullRemote(), future.GetCallback());
    EXPECT_EQ(future.Get<0>()->status,
              FileSystemAccessStatus::kPermissionDenied);
    EXPECT_FALSE(future.Get<1>());
  }
  {
    CrossOriginStorageFileHandleImpl handle_no_fsa(
        registry()->GetWeakPtr(), blob_storage_context_,
        /*file_system_access_manager=*/nullptr, kHash1, kOriginA,
        CrossOriginStorageAccessSpec(), /*is_writable=*/false,
        /*may_read=*/true);
    // Transfer with null fsa_manager or invalid receiver is a safe no-op.
    handle_no_fsa.Transfer(mojo::NullReceiver());
    mojo::PendingRemote<blink::mojom::FileSystemAccessTransferToken> token;
    handle_no_fsa.Transfer(token.InitWithNewPipeAndPassReceiver());

    base::test::TestFuture<FileSystemAccessErrorPtr, bool> future;
    handle_no_fsa.IsSameEntry(std::move(token), future.GetCallback());
    EXPECT_EQ(future.Get<0>()->status,
              FileSystemAccessStatus::kPermissionDenied);
    EXPECT_FALSE(future.Get<1>());
  }

  // 2. IsSameEntry with a disconnected / unknown transfer token returns
  // kPermissionDenied.
  {
    mojo::PendingRemote<blink::mojom::FileSystemAccessTransferToken>
        dangling_token;
    std::ignore = dangling_token.InitWithNewPipeAndPassReceiver();
    base::test::TestFuture<FileSystemAccessErrorPtr, bool> future;
    handle1->IsSameEntry(std::move(dangling_token), future.GetCallback());
    EXPECT_EQ(future.Get<0>()->status,
              FileSystemAccessStatus::kPermissionDenied);
    EXPECT_FALSE(future.Get<1>());
  }

  // 3. IsSameEntry with a non-COS FileSystemAccessFileHandleImpl transfer token
  // returns kPermissionDenied.
  {
    storage::FileSystemURL regular_url =
        file_system_context_->CreateCrackedFileSystemURL(
            blink::StorageKey::CreateFirstParty(kOriginA),
            storage::kFileSystemTypeTest,
            base::FilePath::FromUTF8Unsafe("regular.txt"));
    auto allow_grant =
        base::MakeRefCounted<FixedFileSystemAccessPermissionGrant>(
            FixedFileSystemAccessPermissionGrant::PermissionStatus::GRANTED,
            PathInfo());
    FileSystemAccessFileHandleImpl regular_handle(
        fsa_manager_.get(),
        FileSystemAccessManagerImpl::BindingContext(
            blink::StorageKey::CreateFirstParty(kOriginA), kOriginA.GetURL(),
            GlobalRenderFrameHostId()),
        regular_url, "regular.txt",
        FileSystemAccessManagerImpl::SharedHandleState(allow_grant,
                                                       allow_grant));
    mojo::PendingRemote<blink::mojom::FileSystemAccessTransferToken>
        regular_token;
    regular_handle.Transfer(regular_token.InitWithNewPipeAndPassReceiver());

    base::test::TestFuture<FileSystemAccessErrorPtr, bool> future;
    handle1->IsSameEntry(std::move(regular_token), future.GetCallback());
    EXPECT_EQ(future.Get<0>()->status,
              FileSystemAccessStatus::kPermissionDenied);
    EXPECT_FALSE(future.Get<1>());
  }

  // 4. IsSameEntry with a cross-origin COS transfer token returns
  // kPermissionDenied.
  {
    mojo::PendingRemote<blink::mojom::FileSystemAccessTransferToken>
        cross_origin_token;
    cross_origin_handle->Transfer(
        cross_origin_token.InitWithNewPipeAndPassReceiver());

    base::test::TestFuture<FileSystemAccessErrorPtr, bool> future;
    handle1->IsSameEntry(std::move(cross_origin_token), future.GetCallback());
    EXPECT_EQ(future.Get<0>()->status,
              FileSystemAccessStatus::kPermissionDenied);
    EXPECT_FALSE(future.Get<1>());
  }

  // 5. IsSameEntry with same-origin COS tokens returns true when hashes match
  // and false when hashes differ.
  {
    mojo::PendingRemote<blink::mojom::FileSystemAccessTransferToken> token1;
    handle1->Transfer(token1.InitWithNewPipeAndPassReceiver());
    base::test::TestFuture<FileSystemAccessErrorPtr, bool> same_future;
    handle1->IsSameEntry(std::move(token1), same_future.GetCallback());
    EXPECT_EQ(same_future.Get<0>()->status, FileSystemAccessStatus::kOk);
    EXPECT_TRUE(same_future.Get<1>());

    mojo::PendingRemote<blink::mojom::FileSystemAccessTransferToken> token2;
    handle2->Transfer(token2.InitWithNewPipeAndPassReceiver());
    base::test::TestFuture<FileSystemAccessErrorPtr, bool> diff_future;
    handle1->IsSameEntry(std::move(token2), diff_future.GetCallback());
    EXPECT_EQ(diff_future.Get<0>()->status, FileSystemAccessStatus::kOk);
    EXPECT_FALSE(diff_future.Get<1>());
  }

  // 6. End-to-end FileSystemAccessManagerImpl token redemption and
  // serialization:
  // - Transfer copies `may_read_` by value (WICG § 4.1): a token transferred
  //   before write commit remains unreadable even after the original handle
  //   commits, whereas a token transferred after commit is readable.
  // - `GetFileHandleFromToken` succeeds for a matching first-party StorageKey
  //   and drops the pipe for cross-origin, third-party, or nonce-partitioned
  //   StorageKeys (`IsValidTransferToken`).
  // - `GetDirectoryHandleFromToken` drops the pipe when given a COS file token.
  // - `FileSystemAccessManagerImpl::SerializeHandle` resolves with empty bytes
  //   `{}` because COS tokens are not `FileSystemURL`-backed, and
  //   `ResolveTransferToken` (URL overload) resolves with `std::nullopt`.
  {
    mojo::PendingRemote<blink::mojom::FileSystemAccessTransferToken>
        pre_commit_token;
    handle1->Transfer(pre_commit_token.InitWithNewPipeAndPassReceiver());

    // Write and commit via handle1 so handle1's `may_read_` flips to true.
    base::test::TestFuture<
        FileSystemAccessErrorPtr,
        mojo::PendingRemote<blink::mojom::FileSystemAccessFileWriter>>
        writer_future;
    handle1->CreateFileWriter(
        /*keep_existing_data=*/false, /*auto_close=*/false,
        blink::mojom::FileSystemAccessWritableFileStreamLockMode::kSiloed,
        writer_future.GetCallback());
    auto [writer_err, pending_writer] = writer_future.Take();
    ASSERT_EQ(writer_err->status, FileSystemAccessStatus::kOk);
    mojo::Remote<blink::mojom::FileSystemAccessFileWriter> writer(
        std::move(pending_writer));

    base::test::TestFuture<FileSystemAccessErrorPtr, uint64_t> write_future;
    writer->Write(0, CreateDataPipeWithPayload(kPayload1),
                  write_future.GetCallback());
    ASSERT_EQ(write_future.Get<0>()->status, FileSystemAccessStatus::kOk);

    base::test::TestFuture<FileSystemAccessErrorPtr> close_future;
    writer->Close(close_future.GetCallback());
    ASSERT_EQ(close_future.Get()->status, FileSystemAccessStatus::kOk);

    // Transfer a second token now that handle1 has committed (`may_read_ ==
    // true`).
    mojo::PendingRemote<blink::mojom::FileSystemAccessTransferToken>
        post_commit_token;
    handle1->Transfer(post_commit_token.InitWithNewPipeAndPassReceiver());

    // Bind `FileSystemAccessManager` for first-party `kOriginA`.
    mojo::Remote<blink::mojom::FileSystemAccessManager> fsa_remote_a;
    fsa_manager_->BindReceiver(
        FileSystemAccessManagerImpl::BindingContext(
            blink::StorageKey::CreateFirstParty(kOriginA), kOriginA.GetURL(),
            GlobalRenderFrameHostId()),
        fsa_remote_a.BindNewPipeAndPassReceiver());

    // Redeem `pre_commit_token` via `GetFileHandleFromToken` and verify its
    // reconstructed handle has `may_read == false` (`AsBlob` returns
    // `kPermissionDenied`).
    mojo::Remote<blink::mojom::FileSystemAccessFileHandle> redeemed_pre;
    fsa_remote_a->GetFileHandleFromToken(
        std::move(pre_commit_token), redeemed_pre.BindNewPipeAndPassReceiver());
    base::test::TestFuture<FileSystemAccessErrorPtr, const base::File::Info&,
                           blink::mojom::SerializedBlobPtr>
        pre_blob_future;
    redeemed_pre->AsBlob(pre_blob_future.GetCallback());
    EXPECT_EQ(pre_blob_future.Get<0>()->status,
              FileSystemAccessStatus::kPermissionDenied);

    // Redeem `post_commit_token` via `GetFileHandleFromToken` and verify its
    // reconstructed handle has `may_read == true` (`AsBlob` succeeds and
    // returns the exact payload).
    mojo::Remote<blink::mojom::FileSystemAccessFileHandle> redeemed_post;
    fsa_remote_a->GetFileHandleFromToken(
        std::move(post_commit_token),
        redeemed_post.BindNewPipeAndPassReceiver());
    base::test::TestFuture<FileSystemAccessErrorPtr, const base::File::Info&,
                           blink::mojom::SerializedBlobPtr>
        post_blob_future;
    redeemed_post->AsBlob(post_blob_future.GetCallback());
    auto [post_error, post_info, post_blob] = post_blob_future.Take();
    EXPECT_EQ(post_error->status, FileSystemAccessStatus::kOk);
    ASSERT_TRUE(post_blob);
    EXPECT_EQ(ReadBlobContents(std::move(post_blob->blob)), kPayload1);

    // Redeeming a COS token in a cross-origin, third-party, or
    // nonce-partitioned `BindingContext` fails `IsValidTransferToken` and drops
    // the receiver pipe.
    const blink::StorageKey kRejectedRedeemKeys[] = {
        blink::StorageKey::CreateFirstParty(kOriginB),
        blink::StorageKey::Create(kOriginA, net::SchemefulSite(kOriginB),
                                  blink::mojom::AncestorChainBit::kCrossSite),
        blink::StorageKey::CreateWithNonce(kOriginA,
                                           base::UnguessableToken::Create()),
    };
    for (const blink::StorageKey& bad_key : kRejectedRedeemKeys) {
      mojo::Remote<blink::mojom::FileSystemAccessManager> bad_fsa_remote;
      fsa_manager_->BindReceiver(
          FileSystemAccessManagerImpl::BindingContext(
              bad_key, bad_key.origin().GetURL(), GlobalRenderFrameHostId()),
          bad_fsa_remote.BindNewPipeAndPassReceiver());

      mojo::PendingRemote<blink::mojom::FileSystemAccessTransferToken> token;
      handle1->Transfer(token.InitWithNewPipeAndPassReceiver());

      mojo::Remote<blink::mojom::FileSystemAccessFileHandle> bad_handle;
      bad_fsa_remote->GetFileHandleFromToken(
          std::move(token), bad_handle.BindNewPipeAndPassReceiver());
      bad_handle.FlushForTesting();
      EXPECT_FALSE(bad_handle.is_connected());
    }

    // Redeeming a COS file token via `GetDirectoryHandleFromToken` fails
    // `IsValidTransferToken` (`token->type() != HandleType::kDirectory`) and
    // drops the directory handle pipe.
    {
      mojo::PendingRemote<blink::mojom::FileSystemAccessTransferToken> token;
      handle1->Transfer(token.InitWithNewPipeAndPassReceiver());
      mojo::Remote<blink::mojom::FileSystemAccessDirectoryHandle> dir_handle;
      fsa_remote_a->GetDirectoryHandleFromToken(
          std::move(token), dir_handle.BindNewPipeAndPassReceiver());
      dir_handle.FlushForTesting();
      EXPECT_FALSE(dir_handle.is_connected());
    }

    // `SerializeHandle` resolves with empty bytes `{}` for a COS token, and
    // `ResolveTransferToken` (the `FileSystemURL` overload) resolves with
    // `std::nullopt`.
    {
      mojo::PendingRemote<blink::mojom::FileSystemAccessTransferToken>
          serialize_token;
      handle1->Transfer(serialize_token.InitWithNewPipeAndPassReceiver());
      base::test::TestFuture<const std::vector<uint8_t>&> serialize_future;
      fsa_manager_->SerializeHandle(
          blink::StorageKey::CreateFirstParty(kOriginA),
          std::move(serialize_token), serialize_future.GetCallback());
      EXPECT_TRUE(serialize_future.Get().empty());

      mojo::PendingRemote<blink::mojom::FileSystemAccessTransferToken>
          url_token;
      handle1->Transfer(url_token.InitWithNewPipeAndPassReceiver());
      base::test::TestFuture<std::optional<storage::FileSystemURL>> url_future;
      fsa_manager_->ResolveTransferToken(std::move(url_token),
                                         url_future.GetCallback());
      EXPECT_FALSE(url_future.Get().has_value());
    }
  }
}

// =============================================================================
// CrossOriginStorageFileWriterImpl Tests
// =============================================================================

TEST_F(CrossOriginStorageImplTest,
       FileWriterRejectsConcurrentAndPostCloseOperationsWithInvalidState) {
  constexpr std::string_view kPayload = "streamed payload";
  const CrossOriginStorageHash kHash = ComputeHashForString(kPayload);

  bool committed = false;
  CrossOriginStorageFileWriterImpl writer(
      registry()->GetWeakPtr(), kHash, kOriginA, CrossOriginStorageAccessSpec(),
      CreateStagingWriter(),
      base::BindLambdaForTesting([&]() { committed = true; }));

  // Create a DataPipe and keep the producer handle open so the first Write()
  // remains in-flight.
  mojo::ScopedDataPipeProducerHandle producer;
  mojo::ScopedDataPipeConsumerHandle consumer;
  ASSERT_EQ(MOJO_RESULT_OK,
            mojo::CreateDataPipe(/*options=*/nullptr, producer, consumer));
  ASSERT_TRUE(mojo::BlockingCopyFromString(std::string(kPayload), producer));

  base::test::TestFuture<FileSystemAccessErrorPtr, uint64_t> first_write_future;
  writer.Write(0, std::move(consumer), first_write_future.GetCallback());
  EXPECT_FALSE(first_write_future.IsReady());

  // While `first_write_future` is in-flight (`has_pending_operation()`),
  // concurrent Write, Truncate, and Close calls must immediately reject with
  // kInvalidState.
  {
    base::test::TestFuture<FileSystemAccessErrorPtr, uint64_t>
        concurrent_write_future;
    writer.Write(0, CreateDataPipeWithPayload("extra"),
                 concurrent_write_future.GetCallback());
    EXPECT_EQ(concurrent_write_future.Get<0>()->status,
              FileSystemAccessStatus::kInvalidState);
    EXPECT_EQ(concurrent_write_future.Get<1>(), 0u);

    base::test::TestFuture<FileSystemAccessErrorPtr> concurrent_truncate_future;
    writer.Truncate(0, concurrent_truncate_future.GetCallback());
    EXPECT_EQ(concurrent_truncate_future.Get()->status,
              FileSystemAccessStatus::kInvalidState);

    base::test::TestFuture<FileSystemAccessErrorPtr> concurrent_close_future;
    writer.Close(concurrent_close_future.GetCallback());
    EXPECT_EQ(concurrent_close_future.Get()->status,
              FileSystemAccessStatus::kInvalidState);
  }

  // Close the producer handle so the in-flight Write finishes.
  producer.reset();
  ASSERT_EQ(first_write_future.Get<0>()->status, FileSystemAccessStatus::kOk);
  EXPECT_EQ(first_write_future.Get<1>(), kPayload.size());

  // Truncate to current size and then Close().
  {
    base::test::TestFuture<FileSystemAccessErrorPtr> truncate_future;
    writer.Truncate(kPayload.size(), truncate_future.GetCallback());
    ASSERT_EQ(truncate_future.Get()->status, FileSystemAccessStatus::kOk);
  }
  {
    base::test::TestFuture<FileSystemAccessErrorPtr> close_future;
    writer.Close(close_future.GetCallback());
    // While Close() is in-flight (`closed_or_aborted_ == true`), calling
    // Abort() must reject with kInvalidState and must NOT cancel the commit.
    base::test::TestFuture<FileSystemAccessErrorPtr> abort_during_close_future;
    writer.Abort(abort_during_close_future.GetCallback());
    EXPECT_EQ(abort_during_close_future.Get()->status,
              FileSystemAccessStatus::kInvalidState);

    ASSERT_EQ(close_future.Get()->status, FileSystemAccessStatus::kOk);
    EXPECT_TRUE(committed);
  }

  // After Close(), all operations (Write, Truncate, Close, Abort) must reject
  // with kInvalidState.
  {
    base::test::TestFuture<FileSystemAccessErrorPtr, uint64_t>
        post_close_write_future;
    writer.Write(0, CreateDataPipeWithPayload("x"),
                 post_close_write_future.GetCallback());
    EXPECT_EQ(post_close_write_future.Get<0>()->status,
              FileSystemAccessStatus::kInvalidState);

    base::test::TestFuture<FileSystemAccessErrorPtr> post_close_truncate_future;
    writer.Truncate(0, post_close_truncate_future.GetCallback());
    EXPECT_EQ(post_close_truncate_future.Get()->status,
              FileSystemAccessStatus::kInvalidState);

    base::test::TestFuture<FileSystemAccessErrorPtr> post_close_close_future;
    writer.Close(post_close_close_future.GetCallback());
    EXPECT_EQ(post_close_close_future.Get()->status,
              FileSystemAccessStatus::kInvalidState);

    base::test::TestFuture<FileSystemAccessErrorPtr> post_close_abort_future;
    writer.Abort(post_close_abort_future.GetCallback());
    EXPECT_EQ(post_close_abort_future.Get()->status,
              FileSystemAccessStatus::kInvalidState);
  }
}

TEST_F(CrossOriginStorageImplTest,
       FileWriterAbortWhileWriteInFlightCancelsWriteAndCleansUpStagingFile) {
  const CrossOriginStorageHash kHash = ComputeHashForString("partial data");

  // 1. Abort() while Write() is in-flight resolves Write() with
  // kOperationAborted, resolves Abort() with kOk, and deletes the staging file.
  {
    base::FilePath staging_path;
    bool committed = false;

    CrossOriginStorageFileWriterImpl writer(
        registry()->GetWeakPtr(), kHash, kOriginA,
        CrossOriginStorageAccessSpec(), CreateStagingWriter(&staging_path),
        base::BindLambdaForTesting([&]() { committed = true; }));

    mojo::ScopedDataPipeProducerHandle producer;
    mojo::ScopedDataPipeConsumerHandle consumer;
    ASSERT_EQ(MOJO_RESULT_OK, mojo::CreateDataPipe(4u, producer, consumer));
    ASSERT_EQ(MOJO_RESULT_OK,
              producer->WriteAllData(base::byte_span_from_cstring("part")));

    base::test::TestFuture<FileSystemAccessErrorPtr, uint64_t> write_future;
    writer.Write(0, std::move(consumer), write_future.GetCallback());

    // Wait until the background staging writer drains "part" and parks on
    // MOJO_RESULT_SHOULD_WAIT before calling Abort().
    base::test::TestFuture<MojoResult> drained_future;
    mojo::SimpleWatcher watcher(FROM_HERE,
                                mojo::SimpleWatcher::ArmingPolicy::AUTOMATIC);
    watcher.Watch(producer.get(), MOJO_HANDLE_SIGNAL_WRITABLE,
                  base::BindLambdaForTesting([&](MojoResult result) {
                    if (!drained_future.IsReady()) {
                      drained_future.SetValue(result);
                    }
                  }));
    ASSERT_EQ(drained_future.Get(), MOJO_RESULT_OK);
    watcher.Cancel();

    base::test::TestFuture<FileSystemAccessErrorPtr> abort_future;
    writer.Abort(abort_future.GetCallback());

    ASSERT_EQ(write_future.Get<0>()->status,
              FileSystemAccessStatus::kOperationAborted);
    EXPECT_EQ(write_future.Get<1>(), 0u);
    ASSERT_EQ(abort_future.Get()->status, FileSystemAccessStatus::kOk);
    EXPECT_FALSE(committed);
    EXPECT_FALSE(base::PathExists(staging_path));

    producer.reset();
  }

  // 2. Abort() while Truncate() is in-flight resolves Truncate() with
  // kOperationAborted, resolves Abort() with kOk, and deletes the staging file.
  {
    base::FilePath staging_path;
    bool committed = false;

    CrossOriginStorageFileWriterImpl writer(
        registry()->GetWeakPtr(), kHash, kOriginA,
        CrossOriginStorageAccessSpec(), CreateStagingWriter(&staging_path),
        base::BindLambdaForTesting([&]() { committed = true; }));

    base::test::TestFuture<FileSystemAccessErrorPtr> truncate_future;
    writer.Truncate(16, truncate_future.GetCallback());

    base::test::TestFuture<FileSystemAccessErrorPtr> abort_future;
    writer.Abort(abort_future.GetCallback());

    ASSERT_EQ(truncate_future.Get()->status,
              FileSystemAccessStatus::kOperationAborted);
    ASSERT_EQ(abort_future.Get()->status, FileSystemAccessStatus::kOk);
    EXPECT_FALSE(committed);
    EXPECT_FALSE(base::PathExists(staging_path));
  }
}

TEST_F(CrossOriginStorageImplTest,
       FileWriterQuotaExceededOnWriteAndTruncateLatchesClosedState) {
  const CrossOriginStorageHash kHash = ComputeHashForString("123456789");

  // 1. Write exceeding max_size_bytes returns kQuotaExceededError and latches
  // closed_or_aborted_.
  {
    base::SequenceBound<CrossOriginStorageStagingWriter> staging_writer =
        CreateStagingWriter();
    staging_writer
        .AsyncCall(&CrossOriginStorageStagingWriter::SetMaxSizeBytesForTesting)
        .WithArgs(8u);

    CrossOriginStorageFileWriterImpl writer(
        registry()->GetWeakPtr(), kHash, kOriginA,
        CrossOriginStorageAccessSpec(), std::move(staging_writer),
        base::DoNothing());

    base::test::TestFuture<FileSystemAccessErrorPtr, uint64_t> write_future;
    writer.Write(0, CreateDataPipeWithPayload("123456789"),
                 write_future.GetCallback());
    EXPECT_EQ(write_future.Get<0>()->status,
              FileSystemAccessStatus::kQuotaExceededError);
    EXPECT_EQ(write_future.Get<1>(), 0u);

    // Subsequent operations must return kInvalidState because the writer is now
    // closed.
    base::test::TestFuture<FileSystemAccessErrorPtr> close_future;
    writer.Close(close_future.GetCallback());
    EXPECT_EQ(close_future.Get()->status,
              FileSystemAccessStatus::kInvalidState);
  }

  // 2. Truncate exceeding max_size_bytes returns kQuotaExceededError and
  // latches closed_or_aborted_.
  {
    base::SequenceBound<CrossOriginStorageStagingWriter> staging_writer =
        CreateStagingWriter();
    staging_writer
        .AsyncCall(&CrossOriginStorageStagingWriter::SetMaxSizeBytesForTesting)
        .WithArgs(8u);

    CrossOriginStorageFileWriterImpl writer(
        registry()->GetWeakPtr(), kHash, kOriginA,
        CrossOriginStorageAccessSpec(), std::move(staging_writer),
        base::DoNothing());

    base::test::TestFuture<FileSystemAccessErrorPtr> truncate_future;
    writer.Truncate(9, truncate_future.GetCallback());
    EXPECT_EQ(truncate_future.Get()->status,
              FileSystemAccessStatus::kQuotaExceededError);

    base::test::TestFuture<FileSystemAccessErrorPtr> close_future;
    writer.Close(close_future.GetCallback());
    EXPECT_EQ(close_future.Get()->status,
              FileSystemAccessStatus::kInvalidState);
  }
}

TEST_F(CrossOriginStorageImplTest,
       FileWriterCloseDigestMismatchNullRegistryAndWriterDestruction) {
  constexpr std::string_view kExpectedPayload = "expected payload";
  constexpr std::string_view kWrongPayload = "wrong payload";
  const CrossOriginStorageHash kHash = ComputeHashForString(kExpectedPayload);

  // 1. Digest mismatch on Close() returns kDataError and does not invoke
  // on_write_committed.
  {
    bool committed = false;
    CrossOriginStorageFileWriterImpl writer(
        registry()->GetWeakPtr(), kHash, kOriginA,
        CrossOriginStorageAccessSpec(), CreateStagingWriter(),
        base::BindLambdaForTesting([&]() { committed = true; }));

    base::test::TestFuture<FileSystemAccessErrorPtr, uint64_t> write_future;
    writer.Write(0, CreateDataPipeWithPayload(kWrongPayload),
                 write_future.GetCallback());
    ASSERT_EQ(write_future.Get<0>()->status, FileSystemAccessStatus::kOk);

    base::test::TestFuture<FileSystemAccessErrorPtr> close_future;
    writer.Close(close_future.GetCallback());
    EXPECT_EQ(close_future.Get()->status, FileSystemAccessStatus::kDataError);
    EXPECT_FALSE(committed);
  }

  // 2. Null registry on Close() returns kOperationFailed.
  {
    bool committed = false;
    CrossOriginStorageFileWriterImpl writer(
        /*registry=*/nullptr, kHash, kOriginA, CrossOriginStorageAccessSpec(),
        CreateStagingWriter(),
        base::BindLambdaForTesting([&]() { committed = true; }));

    base::test::TestFuture<FileSystemAccessErrorPtr, uint64_t> write_future;
    writer.Write(0, CreateDataPipeWithPayload(kExpectedPayload),
                 write_future.GetCallback());
    ASSERT_EQ(write_future.Get<0>()->status, FileSystemAccessStatus::kOk);

    base::test::TestFuture<FileSystemAccessErrorPtr> close_future;
    writer.Close(close_future.GetCallback());
    EXPECT_EQ(close_future.Get()->status,
              FileSystemAccessStatus::kOperationFailed);
    EXPECT_FALSE(committed);
  }

  // 3. Destroying the writer after Close() has dispatched Commit() to the
  // registry (so `!self` is true in `OnCommitted`) still invokes
  // `on_write_committed`.
  {
    // Bind `staging_writer` to the current single-thread runner so `Finish()`,
    // `OnStagingFinished`, and `writer.reset()` execute in strict FIFO order on
    // the UI thread before `registry_->Commit()` posts `OnCommitted` back from
    // the registry's background sequence.
    WaitForRegistryInitialization();
    scoped_refptr<base::SequencedTaskRunner> staging_runner =
        base::SingleThreadTaskRunner::GetCurrentDefault();
    base::FilePath staging_dir = browser_context_->GetPath()
                                     .AppendASCII("CrossOriginStorage")
                                     .AppendASCII("staging");
    ASSERT_TRUE(base::CreateDirectory(staging_dir));
    base::FilePath staging_path;
    base::File file =
        base::CreateAndOpenTemporaryFileInDir(staging_dir, &staging_path);
    ASSERT_TRUE(file.IsValid());
    base::SequenceBound<CrossOriginStorageStagingWriter> staging_writer(
        staging_runner, blink::mojom::CrossOriginStorageAlgorithm::kSha256,
        staging_path, std::move(file));

    base::test::TestFuture<void> committed_future;
    auto writer = std::make_unique<CrossOriginStorageFileWriterImpl>(
        registry()->GetWeakPtr(), kHash, kOriginA,
        CrossOriginStorageAccessSpec(), std::move(staging_writer),
        committed_future.GetCallback());

    base::test::TestFuture<FileSystemAccessErrorPtr, uint64_t> write_future;
    writer->Write(0, CreateDataPipeWithPayload(kExpectedPayload),
                  write_future.GetCallback());
    ASSERT_EQ(write_future.Get<0>()->status, FileSystemAccessStatus::kOk);

    bool close_callback_ran = false;
    writer->Close(base::BindLambdaForTesting(
        [&](FileSystemAccessErrorPtr) { close_callback_ran = true; }));

    // Queue a task on `staging_runner` after `Finish()` whose reply runs
    // immediately after `OnStagingFinished` has dispatched
    // `registry_->Commit()`, destroying `writer` before `OnCommitted` runs.
    staging_runner->PostTaskAndReply(
        FROM_HERE, base::DoNothing(),
        base::BindLambdaForTesting([&]() { writer.reset(); }));

    ASSERT_TRUE(committed_future.Wait());
    EXPECT_FALSE(writer);
    EXPECT_FALSE(close_callback_ran);
  }
}

TEST_F(CrossOriginStorageImplTest, FileWriterEndToEndCommitFlipsHandleMayRead) {
  constexpr std::string_view kPayload = "hello world";
  const CrossOriginStorageHash kHash = ComputeHashForString(kPayload);

  // 1. Write at offset 0 followed by a non-zero offset write (exercising
  // `needs_rehash_` and `RehashFromDisk()` on `Close()`), and verify committing
  // flips `may_read_` on the handle and produces a blob with exact bytes.
  auto handle_a =
      CreateHandle(kHash, kOriginA, CrossOriginStorageAccessSpec::AnyOrigin(),
                   /*is_writable=*/true, /*may_read=*/false);
  {
    base::test::TestFuture<
        FileSystemAccessErrorPtr,
        mojo::PendingRemote<blink::mojom::FileSystemAccessFileWriter>>
        writer_future;
    handle_a->CreateFileWriter(
        /*keep_existing_data=*/false, /*auto_close=*/false,
        blink::mojom::FileSystemAccessWritableFileStreamLockMode::kSiloed,
        writer_future.GetCallback());
    auto [writer_err, pending_writer] = writer_future.Take();
    ASSERT_EQ(writer_err->status, FileSystemAccessStatus::kOk);

    mojo::Remote<blink::mojom::FileSystemAccessFileWriter> writer(
        std::move(pending_writer));
    base::test::TestFuture<FileSystemAccessErrorPtr, uint64_t> write1_future;
    writer->Write(0, CreateDataPipeWithPayload("hello XXXXX"),
                  write1_future.GetCallback());
    ASSERT_EQ(write1_future.Get<0>()->status, FileSystemAccessStatus::kOk);

    base::test::TestFuture<FileSystemAccessErrorPtr, uint64_t> write2_future;
    writer->Write(6, CreateDataPipeWithPayload("world"),
                  write2_future.GetCallback());
    ASSERT_EQ(write2_future.Get<0>()->status, FileSystemAccessStatus::kOk);

    base::test::TestFuture<FileSystemAccessErrorPtr> close_future;
    writer->Close(close_future.GetCallback());
    ASSERT_EQ(close_future.Get()->status, FileSystemAccessStatus::kOk);

    writer.reset();
    base::test::TestFuture<FileSystemAccessErrorPtr, const base::File::Info&,
                           blink::mojom::SerializedBlobPtr>
        blob_future;
    handle_a->AsBlob(blob_future.GetCallback());
    auto [blob_err, blob_info, serialized_blob] = blob_future.Take();
    EXPECT_EQ(blob_err->status, FileSystemAccessStatus::kOk);
    ASSERT_TRUE(serialized_blob);
    EXPECT_EQ(ReadBlobContents(std::move(serialized_blob->blob)), kPayload);
  }

  // 2. Per WICG § 3.3, `FileSystemFileHandle.createWritable()` (implemented in
  // the browser via `CreateFileWriter`) must produce a stream whose contents
  // start empty regardless of `keepExistingData` (`keep_existing_data`), even
  // when `kHash` already has an entry in the registry. Calling `Close()`
  // without writing must fail with kDataError and leave `kOriginB`'s handle
  // unreadable.
  {
    auto handle_b =
        CreateHandle(kHash, kOriginB, CrossOriginStorageAccessSpec::AnyOrigin(),
                     /*is_writable=*/true, /*may_read=*/false);
    base::test::TestFuture<
        FileSystemAccessErrorPtr,
        mojo::PendingRemote<blink::mojom::FileSystemAccessFileWriter>>
        writer_future;
    handle_b->CreateFileWriter(
        /*keep_existing_data=*/true, /*auto_close=*/false,
        blink::mojom::FileSystemAccessWritableFileStreamLockMode::kSiloed,
        writer_future.GetCallback());
    auto [writer_err, pending_writer] = writer_future.Take();
    ASSERT_EQ(writer_err->status, FileSystemAccessStatus::kOk);

    mojo::Remote<blink::mojom::FileSystemAccessFileWriter> writer(
        std::move(pending_writer));
    base::test::TestFuture<FileSystemAccessErrorPtr> close_future;
    writer->Close(close_future.GetCallback());
    EXPECT_EQ(close_future.Get()->status, FileSystemAccessStatus::kDataError);

    base::test::TestFuture<FileSystemAccessErrorPtr, const base::File::Info&,
                           blink::mojom::SerializedBlobPtr>
        blob_future;
    handle_b->AsBlob(blob_future.GetCallback());
    EXPECT_EQ(blob_future.Get<0>()->status,
              FileSystemAccessStatus::kPermissionDenied);
  }
}

}  // namespace content
