// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/file_system_access/file_system_access_file_handle_impl.h"

#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "content/browser/file_system_access/fixed_file_system_access_permission_grant.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_task_environment.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "storage/browser/file_system/file_stream_reader.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/browser/test/async_file_test_helper.h"
#include "storage/browser/test/mock_quota_manager.h"
#include "storage/browser/test/mock_quota_manager_proxy.h"
#include "storage/browser/test/mock_special_storage_policy.h"
#include "storage/browser/test/test_file_system_context.h"
#include "storage/common/file_system/file_system_types.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/gurl.h"

namespace content {

using blink::mojom::PermissionStatus;
using storage::FileSystemURL;

class FileSystemAccessFileHandleImplTest : public testing::Test {
 public:
  FileSystemAccessFileHandleImplTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::IO) {}

  void SetUp() override {
    SetupHelper(storage::kFileSystemTypeTest, /*is_incognito=*/false);
  }

  void TearDown() override { task_environment_.RunUntilIdle(); }

  std::string ReadFile(const FileSystemURL& url) {
    std::unique_ptr<storage::FileStreamReader> reader =
        file_system_context_->CreateFileStreamReader(
            url, 0, std::numeric_limits<int64_t>::max(), base::Time());
    std::string result;
    while (true) {
      auto buf = base::MakeRefCounted<net::IOBufferWithSize>(4096);
      net::TestCompletionCallback callback;
      int rv = reader->Read(buf.get(), buf->size(), callback.callback());
      if (rv == net::ERR_IO_PENDING)
        rv = callback.WaitForResult();
      EXPECT_GE(rv, 0);
      if (rv < 0)
        return "(read failure)";
      if (rv == 0)
        return result;
      result.append(buf->data(), rv);
    }
  }

  std::unique_ptr<FileSystemAccessFileHandleImpl>
  GetHandleWithPermissions(const base::FilePath& path, bool read, bool write) {
    auto url = manager_->CreateFileSystemURLFromPath(
        FileSystemAccessEntryFactory::PathType::kLocal, path);
    auto handle = std::make_unique<FileSystemAccessFileHandleImpl>(
        manager_.get(),
        FileSystemAccessManagerImpl::BindingContext(
            test_src_storage_key_, test_src_url_, /*worker_process_id=*/1),
        url,
        FileSystemAccessManagerImpl::SharedHandleState(
            /*read_grant=*/read ? allow_grant_ : deny_grant_,
            /*write_grant=*/write ? allow_grant_ : deny_grant_));
    return handle;
  }

  storage::BucketLocator CreateBucketForTesting() {
    base::test::TestFuture<storage::QuotaErrorOr<storage::BucketInfo>>
        bucket_future;
    quota_manager_proxy_->CreateBucketForTesting(
        test_src_storage_key_, "custom_bucket",
        blink::mojom::StorageType::kTemporary,
        base::SequencedTaskRunnerHandle::Get(), bucket_future.GetCallback());
    auto bucket = bucket_future.Take();
    EXPECT_TRUE(bucket.ok());
    return bucket->ToBucketLocator();
  }

 protected:
  void SetupHelper(storage::FileSystemType type, bool is_incognito) {
    ASSERT_TRUE(dir_.CreateUniqueTempDir());

    quota_manager_ = base::MakeRefCounted<storage::MockQuotaManager>(
        is_incognito, dir_.GetPath(), base::ThreadTaskRunnerHandle::Get(),
        base::MakeRefCounted<storage::MockSpecialStoragePolicy>());
    quota_manager_proxy_ = base::MakeRefCounted<storage::MockQuotaManagerProxy>(
        quota_manager_.get(), base::ThreadTaskRunnerHandle::Get());

    if (is_incognito) {
      file_system_context_ =
          storage::CreateIncognitoFileSystemContextForTesting(
              base::ThreadTaskRunnerHandle::Get(),
              base::ThreadTaskRunnerHandle::Get(), quota_manager_proxy_.get(),
              dir_.GetPath());
    } else {
      file_system_context_ = storage::CreateFileSystemContextForTesting(
          quota_manager_proxy_.get(), dir_.GetPath());
    }

    test_file_url_ = file_system_context_->CreateCrackedFileSystemURL(
        test_src_storage_key_, type, base::FilePath::FromUTF8Unsafe("test"));
    if (type == storage::kFileSystemTypeTemporary)
      test_file_url_.SetBucket(CreateBucketForTesting());

    ASSERT_EQ(base::File::FILE_OK,
              storage::AsyncFileTestHelper::CreateFile(
                  file_system_context_.get(), test_file_url_));

    chrome_blob_context_ = base::MakeRefCounted<ChromeBlobStorageContext>();
    chrome_blob_context_->InitializeOnIOThread(base::FilePath(),
                                               base::FilePath(), nullptr);

    manager_ = base::MakeRefCounted<FileSystemAccessManagerImpl>(
        file_system_context_, chrome_blob_context_,
        /*permission_context=*/nullptr,
        /*off_the_record=*/false);

    handle_ = std::make_unique<FileSystemAccessFileHandleImpl>(
        manager_.get(),
        FileSystemAccessManagerImpl::BindingContext(
            test_src_storage_key_, test_src_url_, /*worker_process_id=*/1),
        test_file_url_,
        FileSystemAccessManagerImpl::SharedHandleState(allow_grant_,
                                                       allow_grant_));
  }

  const GURL test_src_url_ = GURL("http://example.com/foo");
  const blink::StorageKey test_src_storage_key_ =
      blink::StorageKey::CreateFromStringForTesting("http://example.com/foo");

  BrowserTaskEnvironment task_environment_;

  base::ScopedTempDir dir_;
  scoped_refptr<storage::MockQuotaManager> quota_manager_;
  scoped_refptr<storage::MockQuotaManagerProxy> quota_manager_proxy_;
  scoped_refptr<storage::FileSystemContext> file_system_context_;
  scoped_refptr<ChromeBlobStorageContext> chrome_blob_context_;
  scoped_refptr<FileSystemAccessManagerImpl> manager_;

  FileSystemURL test_file_url_;

  scoped_refptr<FixedFileSystemAccessPermissionGrant> allow_grant_ =
      base::MakeRefCounted<FixedFileSystemAccessPermissionGrant>(
          FixedFileSystemAccessPermissionGrant::PermissionStatus::GRANTED,
          base::FilePath());
  scoped_refptr<FixedFileSystemAccessPermissionGrant> deny_grant_ =
      base::MakeRefCounted<FixedFileSystemAccessPermissionGrant>(
          FixedFileSystemAccessPermissionGrant::PermissionStatus::DENIED,
          base::FilePath());
  std::unique_ptr<FileSystemAccessFileHandleImpl> handle_;
};

class FileSystemAccessAccessHandleTest
    : public FileSystemAccessFileHandleImplTest {
 public:
  void SetUp() override {
    // AccessHandles are only allowed for temporary file systems.
    SetupHelper(storage::kFileSystemTypeTemporary, /*is_incognito=*/false);
  }
};

class FileSystemAccessAccessHandleIncognitoTest
    : public FileSystemAccessAccessHandleTest {
  void SetUp() override {
    // AccessHandles are only allowed for temporary file systems.
    SetupHelper(storage::kFileSystemTypeTemporary, /*is_incognito=*/true);
  }
};

TEST_F(FileSystemAccessFileHandleImplTest, CreateFileWriterOverLimitNotOK) {
  int max_files = 5;
  handle_->set_max_swap_files_for_testing(max_files);

  const FileSystemURL base_swap_url =
      file_system_context_->CreateCrackedFileSystemURL(
          test_src_storage_key_, storage::kFileSystemTypeTest,
          base::FilePath::FromUTF8Unsafe("test.crswap"));

  std::vector<mojo::PendingRemote<blink::mojom::FileSystemAccessFileWriter>>
      writers;
  for (int i = 0; i < max_files; i++) {
    FileSystemURL swap_url;
    if (i == 0) {
      swap_url = base_swap_url;
    } else {
      swap_url = file_system_context_->CreateCrackedFileSystemURL(
          test_src_storage_key_, storage::kFileSystemTypeTest,
          base::FilePath::FromUTF8Unsafe(
              base::StringPrintf("test.%d.crswap", i)));
    }

    base::test::TestFuture<
        blink::mojom::FileSystemAccessErrorPtr,
        mojo::PendingRemote<blink::mojom::FileSystemAccessFileWriter>>
        future;
    handle_->CreateFileWriter(
        /*keep_existing_data=*/false,
        /*auto_close=*/false, future.GetCallback());
    blink::mojom::FileSystemAccessErrorPtr result;
    mojo::PendingRemote<blink::mojom::FileSystemAccessFileWriter> writer_remote;
    std::tie(result, writer_remote) = future.Take();
    EXPECT_EQ(result->status, blink::mojom::FileSystemAccessStatus::kOk);
    EXPECT_EQ("", ReadFile(swap_url));
    writers.push_back(std::move(writer_remote));
  }

  base::test::TestFuture<
      blink::mojom::FileSystemAccessErrorPtr,
      mojo::PendingRemote<blink::mojom::FileSystemAccessFileWriter>>
      future;
  handle_->CreateFileWriter(
      /*keep_existing_data=*/false,
      /*auto_close=*/false, future.GetCallback());
  blink::mojom::FileSystemAccessErrorPtr result;
  mojo::PendingRemote<blink::mojom::FileSystemAccessFileWriter> writer_remote;
  std::tie(result, writer_remote) = future.Take();
  EXPECT_EQ(result->status,
            blink::mojom::FileSystemAccessStatus::kOperationFailed);
  EXPECT_FALSE(writer_remote.is_valid());
}

TEST_F(FileSystemAccessFileHandleImplTest, Remove_NoWriteAccess) {
  base::FilePath file;
  ASSERT_TRUE(base::CreateTemporaryFileInDir(dir_.GetPath(), &file));

  auto handle = GetHandleWithPermissions(file, /*read=*/true, /*write=*/false);

  base::test::TestFuture<blink::mojom::FileSystemAccessErrorPtr> future;
  handle->Remove(future.GetCallback());
  EXPECT_EQ(future.Get()->status,
            blink::mojom::FileSystemAccessStatus::kPermissionDenied);
  EXPECT_TRUE(base::PathExists(file));
}

TEST_F(FileSystemAccessFileHandleImplTest, Remove_HasWriteAccess) {
  base::FilePath file;
  ASSERT_TRUE(base::CreateTemporaryFileInDir(dir_.GetPath(), &file));

  auto handle = GetHandleWithPermissions(file, /*read=*/true, /*write=*/true);

  base::test::TestFuture<blink::mojom::FileSystemAccessErrorPtr> future;
  handle->Remove(future.GetCallback());
  EXPECT_EQ(future.Get()->status, blink::mojom::FileSystemAccessStatus::kOk);
  EXPECT_FALSE(base::PathExists(file));
}

TEST_F(FileSystemAccessFileHandleImplTest, GetSwapURL) {
  const base::FilePath test_path =
      base::FilePath::FromUTF8Unsafe("test.crswap");

  // Default case (empty bucket).
  auto default_handle = GetHandleWithPermissions(test_path, true, true);
  storage::FileSystemURL swap_url =
      default_handle->get_swap_url_for_testing(test_path);
  EXPECT_EQ(swap_url.bucket(), absl::nullopt);

  // Custom bucket case.
  const auto custom_bucket = storage::BucketLocator(
      storage::BucketId(1),
      blink::StorageKey::CreateFromStringForTesting("test.crswap"),
      blink::mojom::StorageType::kTemporary, /*is_default=*/false);
  FileSystemURL base_url = file_system_context_->CreateCrackedFileSystemURL(
      test_src_storage_key_, storage::kFileSystemTypeTest, test_path);
  base_url.SetBucket(custom_bucket);
  // Create a custom FileSystemAccessFileHandleImpl for the modified
  // FileSystemURL.
  const auto bucket_handle = std::make_unique<FileSystemAccessFileHandleImpl>(
      manager_.get(),
      FileSystemAccessManagerImpl::BindingContext(
          test_src_storage_key_, test_src_url_, /*worker_process_id=J*/ 1),
      base_url,
      FileSystemAccessManagerImpl::SharedHandleState(
          /*read_grant=*/allow_grant_,
          /*write_grant=*/allow_grant_));
  swap_url = bucket_handle->get_swap_url_for_testing(test_path);
  ASSERT_EQ(swap_url.bucket(), custom_bucket);
}

TEST_F(FileSystemAccessAccessHandleTest, OpenAccessHandle) {
  base::test::TestFuture<
      blink::mojom::FileSystemAccessErrorPtr,
      blink::mojom::FileSystemAccessAccessHandleFilePtr,
      mojo::PendingRemote<blink::mojom::FileSystemAccessAccessHandleHost>>
      future;
  handle_->OpenAccessHandle(future.GetCallback());
  blink::mojom::FileSystemAccessErrorPtr result;
  blink::mojom::FileSystemAccessAccessHandleFilePtr file;
  mojo::PendingRemote<blink::mojom::FileSystemAccessAccessHandleHost>
      access_handle_remote;
  std::tie(result, file, access_handle_remote) = future.Take();
  EXPECT_EQ(result->status, blink::mojom::FileSystemAccessStatus::kOk);
  // File should be valid and no incognito remote is needed.
  EXPECT_TRUE(file->is_regular_file());
  blink::mojom::FileSystemAccessRegularFilePtr regular_file =
      std::move(file->get_regular_file());
  EXPECT_TRUE(regular_file->os_file.IsValid());
  EXPECT_EQ(regular_file->file_size, 0);
  EXPECT_TRUE(regular_file->capacity_allocation_host.is_valid());
  EXPECT_TRUE(access_handle_remote.is_valid());
}

TEST_F(FileSystemAccessAccessHandleIncognitoTest, OpenAccessHandle) {
  base::test::TestFuture<
      blink::mojom::FileSystemAccessErrorPtr,
      blink::mojom::FileSystemAccessAccessHandleFilePtr,
      mojo::PendingRemote<blink::mojom::FileSystemAccessAccessHandleHost>>
      future;
  handle_->OpenAccessHandle(future.GetCallback());
  blink::mojom::FileSystemAccessErrorPtr result;
  blink::mojom::FileSystemAccessAccessHandleFilePtr file;
  mojo::PendingRemote<blink::mojom::FileSystemAccessAccessHandleHost>
      access_handle_remote;
  std::tie(result, file, access_handle_remote) = future.Take();
  EXPECT_EQ(result->status, blink::mojom::FileSystemAccessStatus::kOk);
  // Incognito remote should be valid and no file is needed.
  EXPECT_TRUE(file->is_incognito_file_delegate());
  EXPECT_TRUE(file->get_incognito_file_delegate().is_valid());
  EXPECT_TRUE(access_handle_remote.is_valid());
}

}  // namespace content
