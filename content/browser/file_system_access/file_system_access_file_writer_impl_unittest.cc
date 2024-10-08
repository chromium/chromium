// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/file_system_access/file_system_access_file_writer_impl.h"

#include <limits>
#include <memory>
#include <string>
#include <utility>

#include "base/containers/contains.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/types/expected.h"
#include "components/services/storage/public/cpp/buckets/bucket_info.h"
#include "components/services/storage/public/cpp/buckets/bucket_locator.h"
#include "content/browser/file_system_access/file_system_access_lock_manager.h"
#include "content/browser/file_system_access/fixed_file_system_access_permission_grant.h"
#include "content/browser/file_system_access/mock_file_system_access_permission_context.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/system/data_pipe_producer.h"
#include "mojo/public/cpp/system/string_data_source.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "storage/browser/file_system/file_stream_reader.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/browser/test/async_file_test_helper.h"
#include "storage/browser/test/mock_quota_manager.h"
#include "storage/browser/test/mock_quota_manager_proxy.h"
#include "storage/browser/test/mock_special_storage_policy.h"
#include "storage/browser/test/quota_manager_proxy_sync.h"
#include "storage/browser/test/test_file_system_backend.h"
#include "storage/browser/test/test_file_system_context.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_directory_handle.mojom.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_manager.mojom.h"
#include "url/gurl.h"

using blink::mojom::FileSystemAccessStatus;
using storage::FileSystemURL;

using testing::_;
using testing::AllOf;
using testing::Eq;
using testing::Field;

namespace content {

namespace {

class MockQuarantine : public quarantine::mojom::Quarantine {
 public:
  MockQuarantine() = default;

  void QuarantineFile(const base::FilePath& full_path,
                      const GURL& source_url,
                      const GURL& referrer_url,
                      const std::optional<url::Origin>& request_initiator,
                      const std::string& client_guid,
                      QuarantineFileCallback callback) override {
    paths.push_back(full_path);
    std::move(callback).Run(quarantine::mojom::QuarantineFileResult::OK);
  }

  std::vector<base::FilePath> paths;
};

// File System Backend that can notify whenever a FileSystemOperation is
// created. This lets tests simulate race conditions between file operations and
// other work.
class TestFileSystemBackend : public storage::TestFileSystemBackend {
 public:
  TestFileSystemBackend(base::SequencedTaskRunner* task_runner,
                        const base::FilePath& base_path)
      : storage::TestFileSystemBackend(task_runner, base_path) {}

  std::unique_ptr<storage::FileSystemOperation> CreateFileSystemOperation(
      storage::OperationType type,
      const storage::FileSystemURL& url,
      storage::FileSystemContext* context,
      base::File::Error* error_code) const override {
    if (operation_created_callback_) {
      std::move(operation_created_callback_).Run(url);
    }
    return storage::TestFileSystemBackend::CreateFileSystemOperation(
        type, url, context, error_code);
  }

  void SetOperationCreatedCallback(
      base::OnceCallback<void(const storage::FileSystemURL&)> callback) {
    operation_created_callback_ = std::move(callback);
  }

 private:
  mutable base::OnceCallback<void(const storage::FileSystemURL&)>
      operation_created_callback_;
};

}  // namespace

class FileSystemAccessFileWriterImplTest : public testing::Test {
 public:
  FileSystemAccessFileWriterImplTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::IO) {}

  virtual FileSystemAccessPermissionContext* permission_context() {
    return nullptr;
  }

  void SetUp() override {
    SetupHelper(storage::FileSystemType::kFileSystemTypeLocal);
  }

  void TearDown() override {
    manager_.reset();

    task_environment_.RunUntilIdle();
    // TODO(crbug.com/40266589): Figure out what code is leaking open
    // files, and uncomment this to prevent further regressions.
    // ASSERT_TRUE(dir_.Delete());
  }

  base::WeakPtr<FileSystemAccessFileWriterImpl> CreateWritable(
      const FileSystemURL& file_url,
      const FileSystemURL& swap_url,
      mojo::PendingRemote<blink::mojom::FileSystemAccessFileWriter>& remote) {
    auto lock =
        TakeLockSync(kBindingContext, file_url, writable_shared_lock_type_);
    if (!lock) {
      return nullptr;
    }
    auto swap_lock = TakeLockSync(kBindingContext, swap_url,
                                  manager_->GetExclusiveLockType());
    if (!swap_lock) {
      return nullptr;
    }

    return manager_->CreateFileWriter(
        kBindingContext, file_url, swap_url, std::move(lock),
        std::move(swap_lock),
        FileSystemAccessManagerImpl::SharedHandleState(permission_grant_,
                                                       permission_grant_),
        remote.InitWithNewPipeAndPassReceiver(),
        /*has_transient_user_activation=*/false,
        /*auto_close=*/false, quarantine_callback_);
  }

  mojo::ScopedDataPipeConsumerHandle CreateStream(const std::string& contents) {
    // Test with a relatively low capacity pipe to make sure it isn't all
    // written/read in one go.
    mojo::ScopedDataPipeProducerHandle producer_handle;
    mojo::ScopedDataPipeConsumerHandle consumer_handle;
    MojoCreateDataPipeOptions options;
    options.struct_size = sizeof(MojoCreateDataPipeOptions);
    options.flags = MOJO_CREATE_DATA_PIPE_FLAG_NONE;
    options.element_num_bytes = 1;
    options.capacity_num_bytes = 16;
    mojo::CreateDataPipe(&options, producer_handle, consumer_handle);
    CHECK(producer_handle.is_valid());
    auto producer =
        std::make_unique<mojo::DataPipeProducer>(std::move(producer_handle));
    auto* producer_raw = producer.get();
    producer_raw->Write(
        std::make_unique<mojo::StringDataSource>(
            contents, mojo::StringDataSource::AsyncWritingMode::
                          STRING_MAY_BE_INVALIDATED_BEFORE_COMPLETION),
        base::BindOnce(
            [](std::unique_ptr<mojo::DataPipeProducer>, MojoResult) {},
            std::move(producer)));
    return consumer_handle;
  }

  std::string ReadFile(const FileSystemURL& url) {
    std::unique_ptr<storage::FileStreamReader> reader =
        file_system_context_->CreateFileStreamReader(
            url, 0, std::numeric_limits<int64_t>::max(), base::Time());
    std::string result;
    while (true) {
      auto buf = base::MakeRefCounted<net::IOBufferWithSize>(4096);
      net::TestCompletionCallback callback;
      int rv = reader->Read(buf.get(), buf->size(), callback.callback());
      if (rv == net::ERR_IO_PENDING) {
        rv = callback.WaitForResult();
      }
      EXPECT_GE(rv, 0);
      if (rv < 0) {
        return "(read failure)";
      }
      if (rv == 0) {
        return result;
      }
      result.append(buf->data(), rv);
    }
  }

  FileSystemAccessStatus WriteStreamSync(
      uint64_t position,
      mojo::ScopedDataPipeConsumerHandle data_pipe,
      uint64_t* bytes_written_out) {
    base::test::TestFuture<blink::mojom::FileSystemAccessErrorPtr, uint64_t>
        future;
    handle_->Write(position, std::move(data_pipe), future.GetCallback());
    blink::mojom::FileSystemAccessErrorPtr result;
    std::tie(result, *bytes_written_out) = future.Take();
    return result->status;
  }

  FileSystemAccessStatus TruncateSync(uint64_t length) {
    base::test::TestFuture<blink::mojom::FileSystemAccessErrorPtr> future;
    handle_->Truncate(length, future.GetCallback());
    return future.Get()->status;
  }

  FileSystemAccessStatus CloseSync() {
    base::test::TestFuture<blink::mojom::FileSystemAccessErrorPtr> future;
    handle_->Close(future.GetCallback());
    return future.Get()->status;
  }

  FileSystemAccessStatus AbortSync() {
    base::test::TestFuture<blink::mojom::FileSystemAccessErrorPtr> future;
    handle_->Abort(future.GetCallback());
    return future.Get()->status;
  }

  FileSystemAccessStatus WriteSync(uint64_t position,
                                   const std::string& contents,
                                   uint64_t* bytes_written_out) {
    return WriteStreamSync(position, CreateStream(contents), bytes_written_out);
  }

  scoped_refptr<FileSystemAccessLockManager::LockHandle> TakeLockSync(
      const FileSystemAccessManagerImpl::BindingContext binding_context,
      const storage::FileSystemURL& url,
      FileSystemAccessLockManager::LockType lock_type) {
    base::test::TestFuture<
        scoped_refptr<FileSystemAccessLockManager::LockHandle>>
        future;
    manager_->TakeLock(binding_context, url, lock_type, future.GetCallback());
    return future.Take();
  }

 protected:
  void SetupHelper(storage::FileSystemType type) {
    ASSERT_TRUE(dir_.CreateUniqueTempDir());
    std::vector<std::unique_ptr<storage::FileSystemBackend>>
        additional_providers;
    additional_providers.push_back(std::make_unique<TestFileSystemBackend>(
        base::SingleThreadTaskRunner::GetCurrentDefault().get(),
        dir_.GetPath()));
    test_file_system_backend_ =
        static_cast<TestFileSystemBackend*>(additional_providers[0].get());

    if (type == storage::FileSystemType::kFileSystemTypeTemporary) {
      quota_manager_ = base::MakeRefCounted<storage::MockQuotaManager>(
          /*is_incognito=*/false, dir_.GetPath(),
          base::SingleThreadTaskRunner::GetCurrentDefault(),
          base::MakeRefCounted<storage::MockSpecialStoragePolicy>());
      quota_manager_proxy_ =
          base::MakeRefCounted<storage::MockQuotaManagerProxy>(
              quota_manager_.get(),
              base::SingleThreadTaskRunner::GetCurrentDefault());
    }
    file_system_context_ =
        storage::CreateFileSystemContextWithAdditionalProvidersForTesting(
            base::SingleThreadTaskRunner::GetCurrentDefault(),
            base::SingleThreadTaskRunner::GetCurrentDefault(),
            quota_manager_proxy_ ? quota_manager_proxy_.get() : nullptr,
            std::move(additional_providers), dir_.GetPath());

    chrome_blob_context_ = base::MakeRefCounted<ChromeBlobStorageContext>();
    chrome_blob_context_->InitializeOnIOThread(base::FilePath(),
                                               base::FilePath(), nullptr);
    blob_context_ = chrome_blob_context_->context();

    manager_ = base::MakeRefCounted<FileSystemAccessManagerImpl>(
        file_system_context_, chrome_blob_context_,
        /*permission_context=*/permission_context(),
        /*off_the_record=*/false);
    manager_->BindReceiver(kBindingContext,
                           manager_remote_.BindNewPipeAndPassReceiver());

    // Use an absolute path for local files, or a relative path otherwise,
    auto test_file_path = type == storage::kFileSystemTypeLocal
                              ? dir_.GetPath().AppendASCII("test")
                              : base::FilePath::FromUTF8Unsafe("test");
    auto test_swap_path = type == storage::kFileSystemTypeLocal
                              ? dir_.GetPath().AppendASCII("test.crswap")
                              : base::FilePath::FromUTF8Unsafe("test.crswap");

    test_file_url_ = file_system_context_->CreateCrackedFileSystemURL(
        kTestStorageKey, type, test_file_path);
    test_swap_url_ = file_system_context_->CreateCrackedFileSystemURL(
        kTestStorageKey, type, test_swap_path);

    if (type == storage::kFileSystemTypeTemporary) {
      const auto bucket = CreateSandboxFileSystemAndGetDefaultBucket();
      ASSERT_TRUE(bucket.has_value());
      test_file_url_.SetBucket(bucket.value());
      test_swap_url_.SetBucket(bucket.value());
    }

    ASSERT_EQ(base::File::FILE_OK,
              storage::AsyncFileTestHelper::CreateFile(
                  file_system_context_.get(), test_file_url_));

    ASSERT_EQ(base::File::FILE_OK,
              storage::AsyncFileTestHelper::CreateFile(
                  file_system_context_.get(), test_swap_url_));

    writable_shared_lock_type_ = manager_->GetWFSSiloedLockType();

    quarantine_callback_ = base::BindLambdaForTesting(
        [&](mojo::PendingReceiver<quarantine::mojom::Quarantine> receiver) {
          quarantine_receivers_.Add(&quarantine_, std::move(receiver));
        });

    auto lock = TakeLockSync(kBindingContext, test_file_url_,
                             writable_shared_lock_type_);
    ASSERT_TRUE(lock);

    handle_ = CreateWritable(test_file_url_, test_swap_url_, remote_);
    ASSERT_TRUE(handle_);
  }

  storage::QuotaErrorOr<storage::BucketLocator>
  CreateSandboxFileSystemAndGetDefaultBucket() {
    base::test::TestFuture<
        blink::mojom::FileSystemAccessErrorPtr,
        mojo::PendingRemote<blink::mojom::FileSystemAccessDirectoryHandle>>
        future;
    manager_remote_->GetSandboxedFileSystem(future.GetCallback());
    blink::mojom::FileSystemAccessErrorPtr get_fs_result;
    mojo::PendingRemote<blink::mojom::FileSystemAccessDirectoryHandle>
        directory_remote;
    std::tie(get_fs_result, directory_remote) = future.Take();
    EXPECT_EQ(get_fs_result->status, blink::mojom::FileSystemAccessStatus::kOk);
    mojo::Remote<blink::mojom::FileSystemAccessDirectoryHandle> root(
        std::move(directory_remote));
    EXPECT_TRUE(root);

    storage::QuotaManagerProxySync quota_manager_proxy_sync(
        quota_manager_proxy_.get());

    // Check default bucket exists.
    return quota_manager_proxy_sync
        .GetBucket(kTestStorageKey, storage::kDefaultBucketName,
                   blink::mojom::StorageType::kTemporary)
        .transform([&](storage::BucketInfo result) {
          EXPECT_EQ(result.name, storage::kDefaultBucketName);
          EXPECT_EQ(result.storage_key, kTestStorageKey);
          EXPECT_GT(result.id.value(), 0);
          return result.ToBucketLocator();
        });
  }

  const GURL kTestURL = GURL("https://example.com/test");
  const blink::StorageKey kTestStorageKey =
      blink::StorageKey::CreateFromStringForTesting("https://example.com/test");
  const int kProcessId = 1;
  const int kFrameRoutingId = 2;
  const GlobalRenderFrameHostId kFrameId{kProcessId, kFrameRoutingId};
  const FileSystemAccessManagerImpl::BindingContext kBindingContext = {
      kTestStorageKey, kTestURL, kFrameId};
  BrowserTaskEnvironment task_environment_;

  base::ScopedTempDir dir_;
  scoped_refptr<storage::MockQuotaManager> quota_manager_;
  scoped_refptr<storage::MockQuotaManagerProxy> quota_manager_proxy_;
  scoped_refptr<storage::FileSystemContext> file_system_context_;
  raw_ptr<TestFileSystemBackend> test_file_system_backend_ = nullptr;
  scoped_refptr<ChromeBlobStorageContext> chrome_blob_context_;
  raw_ptr<storage::BlobStorageContext> blob_context_ = nullptr;
  scoped_refptr<FileSystemAccessManagerImpl> manager_;
  mojo::Remote<blink::mojom::FileSystemAccessManager> manager_remote_;

  FileSystemURL test_file_url_;
  FileSystemURL test_swap_url_;

  MockQuarantine quarantine_;
  mojo::ReceiverSet<quarantine::mojom::Quarantine> quarantine_receivers_;
  download::QuarantineConnectionCallback quarantine_callback_;

  scoped_refptr<FixedFileSystemAccessPermissionGrant> permission_grant_ =
      base::MakeRefCounted<FixedFileSystemAccessPermissionGrant>(
          FixedFileSystemAccessPermissionGrant::PermissionStatus::GRANTED,
          PathInfo());

  mojo::PendingRemote<blink::mojom::FileSystemAccessFileWriter> remote_;
  base::WeakPtr<FileSystemAccessFileWriterImpl> handle_;

  FileSystemAccessLockManager::LockType writable_shared_lock_type_;
};

TEST_F(FileSystemAccessFileWriterImplTest, WriteValidEmptyString) {
  uint64_t bytes_written;
  FileSystemAccessStatus result = WriteSync(0, "", &bytes_written);
  EXPECT_EQ(result, FileSystemAccessStatus::kOk);
  EXPECT_EQ(bytes_written, 0u);

  result = CloseSync();
  EXPECT_EQ(result, FileSystemAccessStatus::kOk);
  EXPECT_TRUE(base::Contains(quarantine_.paths, test_file_url_.path()));

  EXPECT_EQ("", ReadFile(test_file_url_));
}

TEST_F(FileSystemAccessFileWriterImplTest, WriteValidNonEmpty) {
  std::string test_data("abcdefghijklmnopqrstuvwxyz");
  uint64_t bytes_written;
  FileSystemAccessStatus result = WriteSync(0, test_data, &bytes_written);
  EXPECT_EQ(result, FileSystemAccessStatus::kOk);
  EXPECT_EQ(bytes_written, test_data.size());

  result = CloseSync();
  EXPECT_EQ(result, FileSystemAccessStatus::kOk);
  EXPECT_TRUE(base::Contains(quarantine_.paths, test_file_url_.path()));

  EXPECT_EQ(test_data, ReadFile(test_file_url_));
}

TEST_F(FileSystemAccessFileWriterImplTest, WriteWithOffsetInFile) {
  uint64_t bytes_written;
  FileSystemAccessStatus result;

  result = WriteSync(0, "1234567890", &bytes_written);
  EXPECT_EQ(result, FileSystemAccessStatus::kOk);
  EXPECT_EQ(bytes_written, 10u);

  result = WriteSync(4, "abc", &bytes_written);
  EXPECT_EQ(result, FileSystemAccessStatus::kOk);
  EXPECT_EQ(bytes_written, 3u);

  result = CloseSync();
  EXPECT_EQ(result, FileSystemAccessStatus::kOk);
  EXPECT_TRUE(base::Contains(quarantine_.paths, test_file_url_.path()));

  EXPECT_EQ("1234abc890", ReadFile(test_file_url_));
}

TEST_F(FileSystemAccessFileWriterImplTest, WriteWithOffsetPastFile) {
  uint64_t bytes_written;
  FileSystemAccessStatus result = WriteSync(4, "abc", &bytes_written);
  EXPECT_EQ(result, FileSystemAccessStatus::kOk);
  EXPECT_EQ(bytes_written, 3u);

  result = CloseSync();
  EXPECT_EQ(result, FileSystemAccessStatus::kOk);
  EXPECT_TRUE(base::Contains(quarantine_.paths, test_file_url_.path()));

  using std::string_literals::operator""s;
  EXPECT_EQ("\0\0\0\0abc"s, ReadFile(test_file_url_));
}

TEST_F(FileSystemAccessFileWriterImplTest, TruncateShrink) {
  uint64_t bytes_written;
  FileSystemAccessStatus result;

  result = WriteSync(0, "1234567890", &bytes_written);
  EXPECT_EQ(result, FileSystemAccessStatus::kOk);
  EXPECT_EQ(bytes_written, 10u);

  result = TruncateSync(5);
  EXPECT_EQ(result, FileSystemAccessStatus::kOk);

  result = CloseSync();
  EXPECT_EQ(result, FileSystemAccessStatus::kOk);

  EXPECT_EQ("12345", ReadFile(test_file_url_));
}

TEST_F(FileSystemAccessFileWriterImplTest, TruncateGrow) {
  uint64_t bytes_written;
  FileSystemAccessStatus result;

  result = WriteSync(0, "abc", &bytes_written);
  EXPECT_EQ(result, FileSystemAccessStatus::kOk);
  EXPECT_EQ(bytes_written, 3u);

  result = TruncateSync(5);
  EXPECT_EQ(result, FileSystemAccessStatus::kOk);

  result = CloseSync();
  EXPECT_EQ(result, FileSystemAccessStatus::kOk);

  EXPECT_EQ(std::string("abc\0\0", 5), ReadFile(test_file_url_));
}

TEST_F(FileSystemAccessFileWriterImplTest, WriterDestroyedAfterClose) {
  uint64_t bytes_written;
  FileSystemAccessStatus result = WriteSync(0, "abc", &bytes_written);
  EXPECT_EQ(result, FileSystemAccessStatus::kOk);
  EXPECT_EQ(bytes_written, 3u);

  result = CloseSync();
  EXPECT_EQ(result, FileSystemAccessStatus::kOk);
  EXPECT_TRUE(handle_.WasInvalidated());
  EXPECT_FALSE(storage::AsyncFileTestHelper::FileExists(
      file_system_context_.get(), test_swap_url_,
      storage::AsyncFileTestHelper::kDontCheckSize));
}

TEST_F(FileSystemAccessFileWriterImplTest, WriterDestroyedAfterAbort) {
  uint64_t bytes_written;
  FileSystemAccessStatus result = WriteSync(0, "abc", &bytes_written);
  EXPECT_EQ(result, FileSystemAccessStatus::kOk);
  EXPECT_EQ(bytes_written, 3u);

  result = AbortSync();
  EXPECT_EQ(result, FileSystemAccessStatus::kOk);
  EXPECT_EQ("", ReadFile(test_file_url_));
  EXPECT_TRUE(handle_.WasInvalidated());
  EXPECT_FALSE(storage::AsyncFileTestHelper::FileExists(
      file_system_context_.get(), test_swap_url_,
      storage::AsyncFileTestHelper::kDontCheckSize));
}

// TODO(crbug.com/40266589): Add more tests, particularly for error
// conditions.

class FileSystemAccessSandboxedFileWriterImplTest
    : public FileSystemAccessFileWriterImplTest {
 public:
  void SetUp() override {
    SetupHelper(storage::FileSystemType::kFileSystemTypeTemporary);
  }
};

TEST_F(FileSystemAccessSandboxedFileWriterImplTest, SkipQuarantine) {
  std::string test_data("abcdefghijklmnopqrstuvwxyz");
  uint64_t bytes_written;
  FileSystemAccessStatus result = WriteSync(0, test_data, &bytes_written);
  EXPECT_EQ(result, FileSystemAccessStatus::kOk);
  EXPECT_EQ(bytes_written, test_data.size());

  result = CloseSync();
  EXPECT_EQ(result, FileSystemAccessStatus::kOk);
  // Files in the sandboxed file system should skip quarantine.
  EXPECT_THAT(quarantine_.paths, testing::IsEmpty());

  EXPECT_EQ(test_data, ReadFile(test_file_url_));
}

TEST_F(FileSystemAccessSandboxedFileWriterImplTest, QuotaError) {
  ASSERT_TRUE(quota_manager_);
  quota_manager_->SetQuota(kTestStorageKey,
                           blink::mojom::StorageType::kTemporary,
                           /*quota=*/1);

  uint64_t bytes_written;
  FileSystemAccessStatus result = WriteSync(0, "abc", &bytes_written);
  LOG(ERROR) << "after WriteSync";
  // TODO(crbug.com/40266589): Refactor WriteSync to return a
  // base::expected<uint64_t, FileSystemAccessErrorPtr>. For now, it seems safe
  // to assume that this file error is a quota error.
  EXPECT_EQ(result, FileSystemAccessStatus::kFileError);
  EXPECT_EQ(bytes_written, 0u);

  // In practice, the renderer should disconnect the mojo pipe to the writer on
  // receiving the quota error from the write call above. For the purpose of
  // this test, we'll confirm that the above write never made it to the swap
  // file, then abort.
  EXPECT_EQ("", ReadFile(test_swap_url_));

  result = CloseSync();
  EXPECT_EQ(result, FileSystemAccessStatus::kOk);
  EXPECT_EQ("", ReadFile(test_file_url_));
  EXPECT_TRUE(handle_.WasInvalidated());
  EXPECT_FALSE(storage::AsyncFileTestHelper::FileExists(
      file_system_context_.get(), test_swap_url_,
      storage::AsyncFileTestHelper::kDontCheckSize));
}

class FileSystemAccessFileWriterAfterWriteChecksTest
    : public FileSystemAccessFileWriterImplTest {
 public:
  FileSystemAccessPermissionContext* permission_context() override {
    return &permission_context_;
  }

 protected:
  testing::StrictMock<MockFileSystemAccessPermissionContext>
      permission_context_;
};

TEST_F(FileSystemAccessFileWriterAfterWriteChecksTest, Allow) {
  uint64_t bytes_written;
  FileSystemAccessStatus result = WriteSync(0, "abc", &bytes_written);
  EXPECT_EQ(result, FileSystemAccessStatus::kOk);
  EXPECT_EQ(bytes_written, 3u);

  std::string expected_hash;
  ASSERT_TRUE(base::HexStringToString(
      "BA7816BF8F01CFEA414140DE5DAE2223B00361A396177A9CB410FF61F20015AD",
      &expected_hash));

  EXPECT_CALL(
      permission_context_,
      PerformAfterWriteChecks_(
          AllOf(
              Field(&FileSystemAccessWriteItem::target_file_path,
                    Eq(test_file_url_.path())),
              Field(&FileSystemAccessWriteItem::full_path,
                    Eq(test_swap_url_.path())),
              Field(&FileSystemAccessWriteItem::sha256_hash, Eq(expected_hash)),
              Field(&FileSystemAccessWriteItem::size, Eq(3)),
              Field(&FileSystemAccessWriteItem::frame_url, Eq(kTestURL)),
              Field(&FileSystemAccessWriteItem::has_user_gesture, Eq(false))),
          kFrameId, _))
      .WillOnce(base::test::RunOnceCallback<2>(
          FileSystemAccessPermissionContext::AfterWriteCheckResult::kAllow));

  result = CloseSync();
  EXPECT_EQ(result, FileSystemAccessStatus::kOk);

  task_environment_.RunUntilIdle();
  EXPECT_FALSE(storage::AsyncFileTestHelper::FileExists(
      file_system_context_.get(), test_swap_url_,
      storage::AsyncFileTestHelper::kDontCheckSize));
  EXPECT_TRUE(storage::AsyncFileTestHelper::FileExists(
      file_system_context_.get(), test_file_url_, 3));
}

TEST_F(FileSystemAccessFileWriterAfterWriteChecksTest, Block) {
  uint64_t bytes_written;
  FileSystemAccessStatus result = WriteSync(0, "abc", &bytes_written);
  EXPECT_EQ(result, FileSystemAccessStatus::kOk);
  EXPECT_EQ(bytes_written, 3u);

  EXPECT_CALL(permission_context_, PerformAfterWriteChecks_(_, kFrameId, _))
      .WillOnce(base::test::RunOnceCallback<2>(
          FileSystemAccessPermissionContext::AfterWriteCheckResult::kBlock));

  result = CloseSync();
  EXPECT_EQ(result, FileSystemAccessStatus::kOperationAborted);

  task_environment_.RunUntilIdle();
  EXPECT_FALSE(storage::AsyncFileTestHelper::FileExists(
      file_system_context_.get(), test_swap_url_,
      storage::AsyncFileTestHelper::kDontCheckSize));
  EXPECT_TRUE(storage::AsyncFileTestHelper::FileExists(
      file_system_context_.get(), test_file_url_, 0));
}

TEST_F(FileSystemAccessFileWriterAfterWriteChecksTest,
       HandleCloseDuringCheckOK) {
  uint64_t bytes_written;
  FileSystemAccessStatus result = WriteSync(0, "abc", &bytes_written);
  EXPECT_EQ(result, FileSystemAccessStatus::kOk);
  EXPECT_EQ(bytes_written, 3u);

  using SBCallback = base::OnceCallback<void(
      FileSystemAccessPermissionContext::AfterWriteCheckResult)>;
  SBCallback sb_callback;
  base::RunLoop loop;
  EXPECT_CALL(permission_context_, PerformAfterWriteChecks_)
      .WillOnce(testing::Invoke([&](FileSystemAccessWriteItem* item,
                                    GlobalRenderFrameHostId frame_id,
                                    SBCallback& callback) {
        sb_callback = std::move(callback);
        loop.Quit();
      }));

  handle_->Close(base::DoNothing());
  loop.Run();

  remote_.reset();
  // Destructor should not have deleted swap file with an active safe browsing
  // check pending.
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(storage::AsyncFileTestHelper::FileExists(
      file_system_context_.get(), test_swap_url_,
      storage::AsyncFileTestHelper::kDontCheckSize));

  std::move(sb_callback)
      .Run(FileSystemAccessPermissionContext::AfterWriteCheckResult::kAllow);

  // Swap file should now be deleted, target file should be written out.
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(storage::AsyncFileTestHelper::FileExists(
      file_system_context_.get(), test_swap_url_,
      storage::AsyncFileTestHelper::kDontCheckSize));
  EXPECT_TRUE(storage::AsyncFileTestHelper::FileExists(
      file_system_context_.get(), test_file_url_, 3));
}

TEST_F(FileSystemAccessFileWriterAfterWriteChecksTest,
       HandleCloseDuringCheckNotOK) {
  uint64_t bytes_written;
  FileSystemAccessStatus result = WriteSync(0, "abc", &bytes_written);
  EXPECT_EQ(result, FileSystemAccessStatus::kOk);
  EXPECT_EQ(bytes_written, 3u);

  using SBCallback = base::OnceCallback<void(
      FileSystemAccessPermissionContext::AfterWriteCheckResult)>;
  SBCallback sb_callback;
  base::RunLoop loop;
  EXPECT_CALL(permission_context_, PerformAfterWriteChecks_)
      .WillOnce(testing::Invoke([&](FileSystemAccessWriteItem* item,
                                    GlobalRenderFrameHostId frame_id,
                                    SBCallback& callback) {
        sb_callback = std::move(callback);
        loop.Quit();
      }));

  handle_->Close(base::DoNothing());
  loop.Run();

  remote_.reset();
  // Destructor should not have deleted swap file with an active safe browsing
  // check pending.
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(storage::AsyncFileTestHelper::FileExists(
      file_system_context_.get(), test_swap_url_,
      storage::AsyncFileTestHelper::kDontCheckSize));

  std::move(sb_callback)
      .Run(FileSystemAccessPermissionContext::AfterWriteCheckResult::kBlock);

  // Swap file should now be deleted, target file should be unmodified.
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(storage::AsyncFileTestHelper::FileExists(
      file_system_context_.get(), test_swap_url_,
      storage::AsyncFileTestHelper::kDontCheckSize));
  EXPECT_TRUE(storage::AsyncFileTestHelper::FileExists(
      file_system_context_.get(), test_file_url_, 0));
}

TEST_F(FileSystemAccessFileWriterAfterWriteChecksTest,
       DestructDuringMoveQuarantines) {
  // This test uses kFileSystemTypeTest to be able to intercept file system
  // operations. As such, recreate urls and handle_.
  test_file_url_ = file_system_context_->CreateCrackedFileSystemURL(
      kTestStorageKey, storage::kFileSystemTypeTest,
      base::FilePath::FromUTF8Unsafe("test2"));

  test_swap_url_ = file_system_context_->CreateCrackedFileSystemURL(
      kTestStorageKey, storage::kFileSystemTypeTest,
      base::FilePath::FromUTF8Unsafe("test2.crswap"));

  ASSERT_EQ(base::File::FILE_OK,
            storage::AsyncFileTestHelper::CreateFile(file_system_context_.get(),
                                                     test_file_url_));

  ASSERT_EQ(base::File::FILE_OK,
            storage::AsyncFileTestHelper::CreateFile(file_system_context_.get(),
                                                     test_swap_url_));

  mojo::PendingRemote<blink::mojom::FileSystemAccessFileWriter> remote;
  handle_ = CreateWritable(test_file_url_, test_swap_url_, remote);
  ASSERT_TRUE(handle_);

  uint64_t bytes_written;
  FileSystemAccessStatus result = WriteSync(0, "foo", &bytes_written);
  EXPECT_EQ(result, FileSystemAccessStatus::kOk);
  EXPECT_EQ(bytes_written, 3u);

  using SBCallback = base::OnceCallback<void(
      FileSystemAccessPermissionContext::AfterWriteCheckResult)>;
  SBCallback sb_callback;
  base::RunLoop sb_loop;
  EXPECT_CALL(permission_context_, PerformAfterWriteChecks_)
      .WillOnce(testing::Invoke([&](FileSystemAccessWriteItem* item,
                                    GlobalRenderFrameHostId frame_id,
                                    SBCallback& callback) {
        sb_callback = std::move(callback);
        sb_loop.Quit();
      }));

  handle_->Close(base::DoNothing());
  sb_loop.Run();
  std::move(sb_callback)
      .Run(FileSystemAccessPermissionContext::AfterWriteCheckResult::kAllow);

  base::test::TestFuture<storage::FileSystemURL> future;
  test_file_system_backend_->SetOperationCreatedCallback(
      future.GetCallback<const storage::FileSystemURL&>());
  EXPECT_EQ(future.Get(), test_file_url_);

  // About to start the move operation. Now destroy the writer. The
  // move will still complete, but make sure that quarantine was also
  // applied to the resulting file.
  remote_.reset();
  task_environment_.RunUntilIdle();

  // Swap file should have been deleted since writer was closed.
  ASSERT_FALSE(storage::AsyncFileTestHelper::FileExists(
      file_system_context_.get(), test_swap_url_,
      storage::AsyncFileTestHelper::kDontCheckSize));
  // And destination file should have been created, since writer was
  // destroyed after move was started.
  ASSERT_TRUE(storage::AsyncFileTestHelper::FileExists(
      file_system_context_.get(), test_file_url_, 3));

  // Destination file should also have been quarantined.
  EXPECT_TRUE(base::Contains(quarantine_.paths, test_file_url_.path()));
}

}  // namespace content
