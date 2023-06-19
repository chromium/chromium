// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/file_system_access/file_system_access_file_handle_impl.h"

#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "content/browser/file_system_access/features.h"
#include "content/browser/file_system_access/fixed_file_system_access_permission_grant.h"
#include "content/browser/file_system_access/mock_file_system_access_permission_grant.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_web_contents_factory.h"
#include "content/test/test_web_contents.h"
#include "file_system_access_directory_handle_impl.h"
#include "mock_file_system_access_permission_context.h"
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
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom-shared.h"
#include "url/gurl.h"

namespace content {

using blink::mojom::PermissionStatus;
using storage::FileSystemURL;

class FileSystemAccessFileHandleImplTest : public testing::Test {
 public:
  FileSystemAccessFileHandleImplTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::IO) {}

  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);

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

  std::unique_ptr<FileSystemAccessFileHandleImpl>
  GetHandleWithPermissions(const base::FilePath& path, bool read, bool write) {
    auto url = manager_->CreateFileSystemURLFromPath(
        FileSystemAccessEntryFactory::PathType::kLocal, path);
    auto handle = std::make_unique<FileSystemAccessFileHandleImpl>(
        manager_.get(),
        FileSystemAccessManagerImpl::BindingContext(
            test_src_storage_key_, test_src_url_,
            web_contents_->GetPrimaryMainFrame()->GetGlobalId()),
        url,
        FileSystemAccessManagerImpl::SharedHandleState(
            /*read_grant=*/read ? allow_grant_ : deny_grant_,
            /*write_grant=*/write ? allow_grant_ : deny_grant_));
    return handle;
  }

  std::unique_ptr<FileSystemAccessDirectoryHandleImpl>
  GetDirectoryHandleWithPermissions(const base::FilePath& path,
                                    bool read,
                                    bool write) {
    auto url = manager_->CreateFileSystemURLFromPath(
        FileSystemAccessEntryFactory::PathType::kLocal, path);
    auto handle = std::make_unique<FileSystemAccessDirectoryHandleImpl>(
        manager_.get(),
        FileSystemAccessManagerImpl::BindingContext(
            test_src_storage_key_, test_src_url_,
            web_contents_->GetPrimaryMainFrame()->GetGlobalId()),
        url,
        FileSystemAccessManagerImpl::SharedHandleState(
            /*read_grant=*/read ? allow_grant_ : deny_grant_,
            /*write_grant=*/write ? allow_grant_ : deny_grant_));
    return handle;
  }

  storage::QuotaErrorOr<storage::BucketLocator> CreateBucketForTesting() {
    base::test::TestFuture<storage::QuotaErrorOr<storage::BucketInfo>>
        bucket_future;
    quota_manager_proxy_->CreateBucketForTesting(
        test_src_storage_key_, "custom_bucket",
        blink::mojom::StorageType::kTemporary,
        base::SequencedTaskRunner::GetCurrentDefault(),
        bucket_future.GetCallback());
    return bucket_future.Take().transform(
        &storage::BucketInfo::ToBucketLocator);
  }

 protected:
  void SetupHelper(storage::FileSystemType type, bool is_incognito) {
    ASSERT_TRUE(dir_.CreateUniqueTempDir());
#if BUILDFLAG(IS_WIN)
    // Convert path to long format to avoid mixing long and 8.3 formats in test.
    ASSERT_TRUE(dir_.Set(base::MakeLongFilePath(dir_.Take())));
#endif  // BUILDFLAG(IS_WIN)

    web_contents_ = web_contents_factory_.CreateWebContents(&browser_context_);
    static_cast<TestWebContents*>(web_contents_)
        ->NavigateAndCommit(test_src_url_);

    quota_manager_ = base::MakeRefCounted<storage::MockQuotaManager>(
        is_incognito, dir_.GetPath(),
        base::SingleThreadTaskRunner::GetCurrentDefault(),
        base::MakeRefCounted<storage::MockSpecialStoragePolicy>());
    quota_manager_proxy_ = base::MakeRefCounted<storage::MockQuotaManagerProxy>(
        quota_manager_.get(),
        base::SingleThreadTaskRunner::GetCurrentDefault());

    if (is_incognito) {
      file_system_context_ =
          storage::CreateIncognitoFileSystemContextForTesting(
              base::SingleThreadTaskRunner::GetCurrentDefault(),
              base::SingleThreadTaskRunner::GetCurrentDefault(),
              quota_manager_proxy_.get(), dir_.GetPath());
    } else {
      file_system_context_ = storage::CreateFileSystemContextForTesting(
          quota_manager_proxy_.get(), dir_.GetPath());
    }

    // Use an absolute path for local files, or a relative path otherwise,
    auto test_file_path = type == storage::kFileSystemTypeLocal
                              ? dir_.GetPath().AppendASCII("test")
                              : base::FilePath::FromUTF8Unsafe("test");
    test_file_url_ = file_system_context_->CreateCrackedFileSystemURL(
        test_src_storage_key_, type, test_file_path);
    if (type == storage::kFileSystemTypeTemporary) {
      auto bucket = CreateBucketForTesting();
      ASSERT_TRUE(bucket.has_value());
      test_file_url_.SetBucket(*std::move(bucket));
    }

    ASSERT_EQ(base::File::FILE_OK,
              storage::AsyncFileTestHelper::CreateFile(
                  file_system_context_.get(), test_file_url_));

    chrome_blob_context_ = base::MakeRefCounted<ChromeBlobStorageContext>();
    chrome_blob_context_->InitializeOnIOThread(base::FilePath(),
                                               base::FilePath(), nullptr);

    manager_ = base::MakeRefCounted<FileSystemAccessManagerImpl>(
        file_system_context_, chrome_blob_context_,
        /*permission_context=*/nullptr,
        /*off_the_record=*/is_incognito);

    handle_ = std::make_unique<FileSystemAccessFileHandleImpl>(
        manager_.get(),
        FileSystemAccessManagerImpl::BindingContext(
            test_src_storage_key_, test_src_url_,
            web_contents_->GetPrimaryMainFrame()->GetGlobalId()),
        test_file_url_,
        FileSystemAccessManagerImpl::SharedHandleState(allow_grant_,
                                                       allow_grant_));
  }

  const GURL test_src_url_ = GURL("http://example.com/foo");
  const blink::StorageKey test_src_storage_key_ =
      blink::StorageKey::CreateFromStringForTesting("http://example.com/foo");

  BrowserTaskEnvironment task_environment_;

  base::ScopedTempDir dir_;

  TestBrowserContext browser_context_;
  TestWebContentsFactory web_contents_factory_;

  scoped_refptr<storage::MockQuotaManager> quota_manager_;
  scoped_refptr<storage::MockQuotaManagerProxy> quota_manager_proxy_;
  scoped_refptr<storage::FileSystemContext> file_system_context_;
  scoped_refptr<ChromeBlobStorageContext> chrome_blob_context_;
  scoped_refptr<FileSystemAccessManagerImpl> manager_;

  raw_ptr<WebContents> web_contents_;

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

TEST_F(FileSystemAccessFileHandleImplTest,
       CreateFileWriterWithExistingSwapFile) {
  const FileSystemURL swap_url =
      file_system_context_->CreateCrackedFileSystemURL(
          test_src_storage_key_, storage::kFileSystemTypeTest,
          base::FilePath::FromUTF8Unsafe("test.crswap"));

  // Create pre-existing swap file.
  ASSERT_EQ(base::File::FILE_OK, storage::AsyncFileTestHelper::CreateFile(
                                     file_system_context_.get(), swap_url));

  // Creating the writer still succeeds.
  base::test::TestFuture<
      blink::mojom::FileSystemAccessErrorPtr,
      mojo::PendingRemote<blink::mojom::FileSystemAccessFileWriter>>
      future;
  handle_->CreateFileWriter(
      /*keep_existing_data=*/true,
      /*auto_close=*/false, future.GetCallback());
  blink::mojom::FileSystemAccessErrorPtr result;
  mojo::PendingRemote<blink::mojom::FileSystemAccessFileWriter> writer_remote;
  std::tie(result, writer_remote) = future.Take();
  EXPECT_EQ(result->status, blink::mojom::FileSystemAccessStatus::kOk);
  EXPECT_TRUE(writer_remote.is_valid());
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

TEST_F(FileSystemAccessFileHandleImplTest, Rename_NoWriteAccess) {
  base::FilePath file;
  ASSERT_TRUE(base::CreateTemporaryFileInDir(dir_.GetPath(), &file));
  base::FilePath renamed_file = file.DirName().AppendASCII("new_name.txt");

  auto handle = GetHandleWithPermissions(file, /*read=*/true, /*write=*/false);

  base::test::TestFuture<blink::mojom::FileSystemAccessErrorPtr> future;
  handle->Rename(renamed_file.BaseName().AsUTF8Unsafe(), future.GetCallback());
  EXPECT_EQ(future.Get()->status,
            blink::mojom::FileSystemAccessStatus::kPermissionDenied);
  EXPECT_TRUE(base::PathExists(file));
  EXPECT_FALSE(base::PathExists(renamed_file));
}

TEST_F(FileSystemAccessFileHandleImplTest, Rename_HasWriteAccess) {
  base::FilePath file;
  ASSERT_TRUE(base::CreateTemporaryFileInDir(dir_.GetPath(), &file));
  base::FilePath renamed_file = file.DirName().AppendASCII("new_name.txt");

  auto handle = GetHandleWithPermissions(file, /*read=*/true, /*write=*/true);

  base::test::TestFuture<blink::mojom::FileSystemAccessErrorPtr> future;
  handle->Rename(renamed_file.BaseName().AsUTF8Unsafe(), future.GetCallback());
  EXPECT_EQ(future.Get()->status, blink::mojom::FileSystemAccessStatus::kOk);
  EXPECT_FALSE(base::PathExists(file));
  EXPECT_TRUE(base::PathExists(renamed_file));
}

TEST_F(FileSystemAccessFileHandleImplTest, Move_NoWriteAccess) {
  base::FilePath dest_dir;
  ASSERT_TRUE(base::CreateTemporaryDirInDir(
      dir_.GetPath(), FILE_PATH_LITERAL("dest"), &dest_dir));
  base::FilePath file;
  ASSERT_TRUE(base::CreateTemporaryFileInDir(dir_.GetPath(), &file));
  base::FilePath renamed_file = dest_dir.AppendASCII("new_name.txt");

  auto dest_dir_handle =
      GetDirectoryHandleWithPermissions(dest_dir, /*read=*/true,
                                        /*write=*/true);
  auto handle = GetHandleWithPermissions(file, /*read=*/true, /*write=*/false);

  mojo::PendingRemote<blink::mojom::FileSystemAccessTransferToken> dir_remote;
  manager_->CreateTransferToken(*dest_dir_handle,
                                dir_remote.InitWithNewPipeAndPassReceiver());

  base::test::TestFuture<blink::mojom::FileSystemAccessErrorPtr> future;
  handle->Move(std::move(dir_remote), renamed_file.BaseName().AsUTF8Unsafe(),
               future.GetCallback());
  EXPECT_EQ(future.Get()->status,
            blink::mojom::FileSystemAccessStatus::kPermissionDenied);
  EXPECT_TRUE(base::PathExists(file));
  EXPECT_FALSE(base::PathExists(renamed_file));
}

TEST_F(FileSystemAccessFileHandleImplTest, Move_NoDestWriteAccess) {
  base::FilePath dest_dir;
  ASSERT_TRUE(base::CreateTemporaryDirInDir(
      dir_.GetPath(), FILE_PATH_LITERAL("dest"), &dest_dir));
  base::FilePath file;
  ASSERT_TRUE(base::CreateTemporaryFileInDir(dir_.GetPath(), &file));
  base::FilePath renamed_file = dest_dir.AppendASCII("new_name.txt");

  auto dest_dir_handle =
      GetDirectoryHandleWithPermissions(dest_dir, /*read=*/true,
                                        /*write=*/false);
  auto handle = GetHandleWithPermissions(file, /*read=*/true, /*write=*/true);

  mojo::PendingRemote<blink::mojom::FileSystemAccessTransferToken> dir_remote;
  manager_->CreateTransferToken(*dest_dir_handle,
                                dir_remote.InitWithNewPipeAndPassReceiver());

  base::test::TestFuture<blink::mojom::FileSystemAccessErrorPtr> future;
  handle->Move(std::move(dir_remote), renamed_file.BaseName().AsUTF8Unsafe(),
               future.GetCallback());
  EXPECT_EQ(future.Get()->status,
            blink::mojom::FileSystemAccessStatus::kPermissionDenied);
  EXPECT_TRUE(base::PathExists(file));
  EXPECT_FALSE(base::PathExists(renamed_file));
}

TEST_F(FileSystemAccessFileHandleImplTest, Move_HasDestWriteAccess) {
  base::FilePath dest_dir;
  ASSERT_TRUE(base::CreateTemporaryDirInDir(
      dir_.GetPath(), FILE_PATH_LITERAL("dest"), &dest_dir));
  base::FilePath file;
  ASSERT_TRUE(base::CreateTemporaryFileInDir(dir_.GetPath(), &file));
  base::FilePath renamed_file = dest_dir.AppendASCII("new_name.txt");

  auto dest_dir_handle =
      GetDirectoryHandleWithPermissions(dest_dir, /*read=*/true,
                                        /*write=*/true);
  auto handle = GetHandleWithPermissions(file, /*read=*/true, /*write=*/true);

  mojo::PendingRemote<blink::mojom::FileSystemAccessTransferToken> dir_remote;
  manager_->CreateTransferToken(*dest_dir_handle,
                                dir_remote.InitWithNewPipeAndPassReceiver());

  base::test::TestFuture<blink::mojom::FileSystemAccessErrorPtr> future;
  handle->Move(std::move(dir_remote), renamed_file.BaseName().AsUTF8Unsafe(),
               future.GetCallback());
  EXPECT_EQ(future.Get()->status, blink::mojom::FileSystemAccessStatus::kOk);
  EXPECT_FALSE(base::PathExists(file));
  EXPECT_TRUE(base::PathExists(renamed_file));
}

#if BUILDFLAG(IS_MAC)
// Tests that swap file cloning (i.e. creating a swap file using underlying
// platform support for copy-on-write files) behaves as expected. Swap file
// cloning requires storage::kFileSystemTypeLocal.
class FileSystemAccessFileHandleSwapFileCloningTest
    : public FileSystemAccessFileHandleImplTest {
 public:
  enum class CloneFileResult {
    kDidNotAttempt,
    kAttemptedAndAborted,
    kAttemptedAndCompletedUnexpectedly,
    kAttemptedAndCompletedAsExpected
  };

  FileSystemAccessFileHandleSwapFileCloningTest()
      : scoped_feature_list_(features::kFileSystemAccessCowSwapFile) {}
  void SetUp() override {
    SetupHelper(storage::kFileSystemTypeLocal, /*is_incognito=*/false);
  }

  CloneFileResult GetCloneFileResult(
      const std::unique_ptr<FileSystemAccessFileHandleImpl>& handle) {
    auto maybe_clone_result = handle->get_swap_file_clone_result_for_testing();

    if (!maybe_clone_result.has_value()) {
      return CloneFileResult::kDidNotAttempt;
    }

    if (maybe_clone_result.value() == base::File::Error::FILE_ERROR_ABORT) {
      return CloneFileResult::kAttemptedAndAborted;
    }

    // We should not attempt to clone the file if the swap file exists. Other
    // errors are okay.
    if (maybe_clone_result.value() == base::File::Error::FILE_ERROR_EXISTS) {
      return CloneFileResult::kAttemptedAndCompletedUnexpectedly;
    }

    // TODO(https://crbug.com/1439179): Remove this expectation once we have a
    // better idea of what's causing the spurious failures.
    EXPECT_EQ(maybe_clone_result.value(), base::File::Error::FILE_OK);

    // Ideally we could just check that the result is FILE_OK, but
    // clonefile() may spuriously fail. See https://crbug.com/1439179. For the
    // purposes of these tests, we'll consider these spurious errors as
    // "expected".
    return CloneFileResult::kAttemptedAndCompletedAsExpected;
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(FileSystemAccessFileHandleSwapFileCloningTest, BasicClone) {
  base::test::TestFuture<
      blink::mojom::FileSystemAccessErrorPtr,
      mojo::PendingRemote<blink::mojom::FileSystemAccessFileWriter>>
      future;
  handle_->CreateFileWriter(
      /*keep_existing_data=*/true,
      /*auto_close=*/false, future.GetCallback());
  blink::mojom::FileSystemAccessErrorPtr result;
  mojo::PendingRemote<blink::mojom::FileSystemAccessFileWriter> writer_remote;
  std::tie(result, writer_remote) = future.Take();
  EXPECT_EQ(result->status, blink::mojom::FileSystemAccessStatus::kOk);
  EXPECT_TRUE(writer_remote.is_valid());
  EXPECT_EQ(GetCloneFileResult(handle_),
            CloneFileResult::kAttemptedAndCompletedAsExpected);
}

TEST_F(FileSystemAccessFileHandleSwapFileCloningTest,
       IgnoringExistingDataDoesNotClone) {
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
  EXPECT_TRUE(writer_remote.is_valid());
  EXPECT_EQ(GetCloneFileResult(handle_), CloneFileResult::kDidNotAttempt);
}

TEST_F(FileSystemAccessFileHandleSwapFileCloningTest, HandleExistingSwapFile) {
  const FileSystemURL swap_url =
      file_system_context_->CreateCrackedFileSystemURL(
          test_src_storage_key_, storage::kFileSystemTypeLocal,
          dir_.GetPath().AppendASCII("test.crswap"));

  // Create pre-existing swap file.
  ASSERT_EQ(base::File::FILE_OK, storage::AsyncFileTestHelper::CreateFile(
                                     file_system_context_.get(), swap_url));

  // Creating the writer still succeeds, even though clonefile() will fail if
  // the destination file already exists.
  base::test::TestFuture<
      blink::mojom::FileSystemAccessErrorPtr,
      mojo::PendingRemote<blink::mojom::FileSystemAccessFileWriter>>
      future;
  handle_->CreateFileWriter(
      /*keep_existing_data=*/true,
      /*auto_close=*/false, future.GetCallback());
  blink::mojom::FileSystemAccessErrorPtr result;
  mojo::PendingRemote<blink::mojom::FileSystemAccessFileWriter> writer_remote;
  std::tie(result, writer_remote) = future.Take();
  EXPECT_EQ(result->status, blink::mojom::FileSystemAccessStatus::kOk);
  EXPECT_TRUE(writer_remote.is_valid());
  EXPECT_EQ(GetCloneFileResult(handle_),
            CloneFileResult::kAttemptedAndCompletedAsExpected);
}

TEST_F(FileSystemAccessFileHandleSwapFileCloningTest, HandleCloneFailure) {
  handle_->set_swap_file_cloning_will_fail_for_testing();

  // Creating the writer still succeeds, even if cloning fails.
  base::test::TestFuture<
      blink::mojom::FileSystemAccessErrorPtr,
      mojo::PendingRemote<blink::mojom::FileSystemAccessFileWriter>>
      future;
  handle_->CreateFileWriter(
      /*keep_existing_data=*/true,
      /*auto_close=*/false, future.GetCallback());
  blink::mojom::FileSystemAccessErrorPtr result;
  mojo::PendingRemote<blink::mojom::FileSystemAccessFileWriter> writer_remote;
  std::tie(result, writer_remote) = future.Take();
  EXPECT_EQ(result->status, blink::mojom::FileSystemAccessStatus::kOk);
  EXPECT_TRUE(writer_remote.is_valid());
  EXPECT_EQ(GetCloneFileResult(handle_), CloneFileResult::kAttemptedAndAborted);
}
#endif  // BUILDFLAG(IS_MAC)

// Uses a mock permission context to ensure the correct permission grant for the
// target file (and parent, for renames) is used, since moves retrieve the
// target's permission grant via GetSharedHandleStateForPath() which always
// returns GRANTED for tests without a permission context.
//
// Moves do not call GetSharedHandleStateForPath() on the destination directory,
// so the above tests are sufficient to test moves without access to the
// destination directory. These tests always grant write access to the
// destination directory.
class FileSystemAccessFileHandleImplMovePermissionsTest
    : public FileSystemAccessFileHandleImplTest,
      public testing::WithParamInterface<std::tuple<bool, bool, bool, bool>> {
 public:
  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);

    SetupHelper(storage::kFileSystemTypeLocal, /*is_incognito=*/false);
    manager_->SetPermissionContextForTesting(&permission_context_);

    std::vector<base::test::FeatureRef> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;

    // TODO(crbug.com/1381621): Remove this alongside the corresponding flag.
    // This feature controls whether overwrites are NOT allowed. Yes, this is
    // very confusing. Lesson learned not to name flags as a negative.
    if (overwrites_disabled()) {
      enabled_features.push_back(
          features::kFileSystemAccessDoNotOverwriteOnMove);
    } else {
      disabled_features.push_back(
          features::kFileSystemAccessDoNotOverwriteOnMove);
    }
    // TODO(crbug.com/1394837): Remove this alongside the corresponding flag.
    if (gesture_required()) {
      enabled_features.push_back(
          features::
              kFileSystemAccessRenameWithoutParentAccessRequiresUserActivation);
    } else {
      disabled_features.push_back(
          features::
              kFileSystemAccessRenameWithoutParentAccessRequiresUserActivation);
    }

    scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }

  bool overwrites_disabled() const { return std::get<0>(GetParam()); }
  bool target_present() const { return std::get<1>(GetParam()); }
  bool gesture_required() const { return std::get<2>(GetParam()); }
  bool gesture_present() const { return std::get<3>(GetParam()); }

  std::pair<base::FilePath, base::FilePath> CreateSourceAndMaybeTarget() {
    base::FilePath source;
    EXPECT_TRUE(base::CreateTemporaryFileInDir(dir_.GetPath(), &source));

    base::FilePath target;
    if (target_present()) {
      EXPECT_TRUE(base::CreateTemporaryFileInDir(dir_.GetPath(), &target));
    } else {
      target = dir_.GetPath().AppendASCII("new_name.txt");
    }

    return {source, target};
  }

  void ExpectFileRenameSuccess(
      const base::FilePath& parent,
      const base::FilePath& source,
      const base::FilePath& target,
      scoped_refptr<FixedFileSystemAccessPermissionGrant> parent_grant,
      scoped_refptr<FixedFileSystemAccessPermissionGrant> target_grant) {
    auto handle =
        GetHandleWithPermissions(source, /*read=*/true, /*write=*/true);

    EXPECT_CALL(permission_context_,
                GetReadPermissionGrant(
                    test_src_storage_key_.origin(), parent,
                    FileSystemAccessPermissionContext::HandleType::kDirectory,
                    FileSystemAccessPermissionContext::UserAction::kNone))
        .WillOnce(testing::Return(parent_grant));
    EXPECT_CALL(permission_context_,
                GetWritePermissionGrant(
                    test_src_storage_key_.origin(), parent,
                    FileSystemAccessPermissionContext::HandleType::kDirectory,
                    FileSystemAccessPermissionContext::UserAction::kNone))
        .WillOnce(testing::Return(parent_grant));
    EXPECT_CALL(permission_context_,
                GetReadPermissionGrant(
                    test_src_storage_key_.origin(), target,
                    FileSystemAccessPermissionContext::HandleType::kFile,
                    FileSystemAccessPermissionContext::UserAction::kNone))
        .WillOnce(testing::Return(target_grant));
    EXPECT_CALL(permission_context_,
                GetWritePermissionGrant(
                    test_src_storage_key_.origin(), target,
                    FileSystemAccessPermissionContext::HandleType::kFile,
                    FileSystemAccessPermissionContext::UserAction::kNone))
        .WillOnce(testing::Return(target_grant));

    // These checks should only be called if the file is successfully moved.

    // On Windows, CreateTemporaryFileInDir() creates files with the '.tmp'
    // extension. When this feature flag is enabled, Safe Browsing checks are
    // not run on same-file-system moves in which the extension does not change.
    if (!base::FeatureList::IsEnabled(
            features::
                kFileSystemAccessSkipAfterWriteChecksIfUnchangingExtension) ||
        source.Extension() != FILE_PATH_LITERAL(".tmp") ||
        target.Extension() != FILE_PATH_LITERAL(".tmp")) {
      EXPECT_CALL(permission_context_,
                  PerformAfterWriteChecks_(testing::_, testing::_, testing::_))
          .WillOnce(base::test::RunOnceCallback<2>(
              FileSystemAccessPermissionContext::AfterWriteCheckResult::
                  kAllow));
    }
    EXPECT_CALL(
        permission_context_,
        NotifyEntryMoved(test_src_storage_key_.origin(), source, target));

    if (gesture_present()) {
      static_cast<TestRenderFrameHost*>(web_contents_->GetPrimaryMainFrame())
          ->SimulateUserActivation();
    }

    base::test::TestFuture<blink::mojom::FileSystemAccessErrorPtr> future;
    handle->Rename(target.BaseName().AsUTF8Unsafe(), future.GetCallback());
    EXPECT_EQ(future.Get()->status, blink::mojom::FileSystemAccessStatus::kOk);
    EXPECT_FALSE(base::PathExists(source));
    EXPECT_TRUE(base::PathExists(target));
  }

  void ExpectFileRenameFailure(
      const base::FilePath& parent,
      const base::FilePath& source,
      const base::FilePath& target,
      scoped_refptr<FixedFileSystemAccessPermissionGrant> parent_grant,
      scoped_refptr<FixedFileSystemAccessPermissionGrant> target_grant,
      blink::mojom::FileSystemAccessStatus result) {
    auto handle =
        GetHandleWithPermissions(source, /*read=*/true, /*write=*/true);

    EXPECT_CALL(permission_context_,
                GetReadPermissionGrant(
                    test_src_storage_key_.origin(), parent,
                    FileSystemAccessPermissionContext::HandleType::kDirectory,
                    FileSystemAccessPermissionContext::UserAction::kNone))
        .WillOnce(testing::Return(parent_grant));
    EXPECT_CALL(permission_context_,
                GetWritePermissionGrant(
                    test_src_storage_key_.origin(), parent,
                    FileSystemAccessPermissionContext::HandleType::kDirectory,
                    FileSystemAccessPermissionContext::UserAction::kNone))
        .WillOnce(testing::Return(parent_grant));
    EXPECT_CALL(permission_context_,
                GetReadPermissionGrant(
                    test_src_storage_key_.origin(), target,
                    FileSystemAccessPermissionContext::HandleType::kFile,
                    FileSystemAccessPermissionContext::UserAction::kNone))
        .WillOnce(testing::Return(target_grant));
    EXPECT_CALL(permission_context_,
                GetWritePermissionGrant(
                    test_src_storage_key_.origin(), target,
                    FileSystemAccessPermissionContext::HandleType::kFile,
                    FileSystemAccessPermissionContext::UserAction::kNone))
        .WillOnce(testing::Return(target_grant));

    // No after-write checks needed since the file should not have been moved.

    if (gesture_present()) {
      static_cast<TestRenderFrameHost*>(web_contents_->GetPrimaryMainFrame())
          ->SimulateUserActivation();
    }

    base::test::TestFuture<blink::mojom::FileSystemAccessErrorPtr> future;
    handle->Rename(target.BaseName().AsUTF8Unsafe(), future.GetCallback());
    EXPECT_EQ(future.Get()->status, result);

    // The source file should not have been removed.
    EXPECT_TRUE(base::PathExists(source));

    // The target file should remain untouched.
    EXPECT_EQ(target_present(), base::PathExists(target));
  }

  void ExpectFileMoveSuccess(
      const base::FilePath& parent,
      const base::FilePath& source,
      const base::FilePath& target,
      scoped_refptr<FixedFileSystemAccessPermissionGrant> target_grant) {
    // The site has write access to the destination directory.
    auto dest_dir_handle =
        GetDirectoryHandleWithPermissions(parent, /*read=*/true,
                                          /*write=*/true);
    auto handle =
        GetHandleWithPermissions(source, /*read=*/true, /*write=*/true);

    EXPECT_CALL(permission_context_,
                GetReadPermissionGrant(
                    test_src_storage_key_.origin(), target,
                    FileSystemAccessPermissionContext::HandleType::kFile,
                    FileSystemAccessPermissionContext::UserAction::kNone))
        .WillOnce(testing::Return(target_grant));
    EXPECT_CALL(permission_context_,
                GetWritePermissionGrant(
                    test_src_storage_key_.origin(), target,
                    FileSystemAccessPermissionContext::HandleType::kFile,
                    FileSystemAccessPermissionContext::UserAction::kNone))
        .WillOnce(testing::Return(target_grant));

    // These checks should only be called if the file is successfully moved.

    // On Windows, CreateTemporaryFileInDir() creates files with the '.tmp'
    // extension. Safe Browsing checks are not run on same-file-system moves in
    // which the extension does not change. For more context, see
    // FileSystemAccessSafeMoveHelper::RequireAfterWriteChecks().
    if (source.Extension() != FILE_PATH_LITERAL(".tmp") ||
        target.Extension() != FILE_PATH_LITERAL(".tmp")) {
      EXPECT_CALL(permission_context_,
                  PerformAfterWriteChecks_(testing::_, testing::_, testing::_))
          .WillOnce(base::test::RunOnceCallback<2>(
              FileSystemAccessPermissionContext::AfterWriteCheckResult::
                  kAllow));
    }
    EXPECT_CALL(
        permission_context_,
        NotifyEntryMoved(test_src_storage_key_.origin(), source, target));

    mojo::PendingRemote<blink::mojom::FileSystemAccessTransferToken> dir_remote;
    manager_->CreateTransferToken(*dest_dir_handle,
                                  dir_remote.InitWithNewPipeAndPassReceiver());

    base::test::TestFuture<blink::mojom::FileSystemAccessErrorPtr> future;
    handle->Move(std::move(dir_remote), target.BaseName().AsUTF8Unsafe(),
                 future.GetCallback());
    EXPECT_EQ(future.Get()->status, blink::mojom::FileSystemAccessStatus::kOk);
    EXPECT_FALSE(base::PathExists(source));
    EXPECT_TRUE(base::PathExists(target));
  }

  void ExpectFileMoveFailure(
      const base::FilePath& parent,
      const base::FilePath& source,
      const base::FilePath& target,
      scoped_refptr<FixedFileSystemAccessPermissionGrant> target_grant,
      blink::mojom::FileSystemAccessStatus result) {
    // The site has write access to the destination directory.
    auto dest_dir_handle =
        GetDirectoryHandleWithPermissions(parent, /*read=*/true,
                                          /*write=*/true);
    auto handle =
        GetHandleWithPermissions(source, /*read=*/true, /*write=*/true);

    EXPECT_CALL(permission_context_,
                GetReadPermissionGrant(
                    test_src_storage_key_.origin(), target,
                    FileSystemAccessPermissionContext::HandleType::kFile,
                    FileSystemAccessPermissionContext::UserAction::kNone))
        .WillOnce(testing::Return(target_grant));
    EXPECT_CALL(permission_context_,
                GetWritePermissionGrant(
                    test_src_storage_key_.origin(), target,
                    FileSystemAccessPermissionContext::HandleType::kFile,
                    FileSystemAccessPermissionContext::UserAction::kNone))
        .WillOnce(testing::Return(target_grant));

    // No after-write checks needed since the file should not have been moved.

    mojo::PendingRemote<blink::mojom::FileSystemAccessTransferToken> dir_remote;
    manager_->CreateTransferToken(*dest_dir_handle,
                                  dir_remote.InitWithNewPipeAndPassReceiver());

    base::test::TestFuture<blink::mojom::FileSystemAccessErrorPtr> future;
    handle->Move(std::move(dir_remote), target.BaseName().AsUTF8Unsafe(),
                 future.GetCallback());
    EXPECT_EQ(future.Get()->status, result);

    // The source file should not have been removed.
    EXPECT_TRUE(base::PathExists(source));
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;

  scoped_refptr<FixedFileSystemAccessPermissionGrant> ask_grant_ =
      base::MakeRefCounted<FixedFileSystemAccessPermissionGrant>(
          FixedFileSystemAccessPermissionGrant::PermissionStatus::ASK,
          base::FilePath());

  testing::StrictMock<MockFileSystemAccessPermissionContext>
      permission_context_;
};

TEST_P(FileSystemAccessFileHandleImplMovePermissionsTest,
       Rename_HasTargetNoParentWriteAccess) {
  auto [source, target] = CreateSourceAndMaybeTarget();

  auto parent_grant = deny_grant_;
  auto target_grant = allow_grant_;

  if (overwrites_disabled() && target_present()) {
    ExpectFileRenameFailure(
        /*parent=*/dir_.GetPath(), source, target, parent_grant, target_grant,
        blink::mojom::FileSystemAccessStatus::kInvalidModificationError);
  } else {
    ExpectFileRenameSuccess(
        /*parent=*/dir_.GetPath(), source, target, parent_grant, target_grant);
  }
}

TEST_P(FileSystemAccessFileHandleImplMovePermissionsTest,
       Rename_HasTargetAndParentWriteAccess) {
  auto [source, target] = CreateSourceAndMaybeTarget();

  auto parent_grant = allow_grant_;
  auto target_grant = allow_grant_;

  if (overwrites_disabled() && target_present()) {
    ExpectFileRenameFailure(
        /*parent=*/dir_.GetPath(), source, target, parent_grant, target_grant,
        blink::mojom::FileSystemAccessStatus::kInvalidModificationError);
  } else {
    ExpectFileRenameSuccess(
        /*parent=*/dir_.GetPath(), source, target, parent_grant, target_grant);
  }
}

TEST_P(FileSystemAccessFileHandleImplMovePermissionsTest,
       Rename_NoTargetHasParentWriteAccessFails) {
  auto [source, target] = CreateSourceAndMaybeTarget();

  // The user has explicitly denied access to the target entry.
  auto parent_grant = allow_grant_;
  auto target_grant = deny_grant_;

  ExpectFileRenameFailure(
      /*parent=*/dir_.GetPath(), source, target, parent_grant, target_grant,
      blink::mojom::FileSystemAccessStatus::kPermissionDenied);
}

TEST_P(FileSystemAccessFileHandleImplMovePermissionsTest,
       Rename_AskTargetAskParentWriteAccess) {
  auto [source, target] = CreateSourceAndMaybeTarget();

  // The site has not yet asked for access to the target entry.
  auto parent_grant = ask_grant_;
  auto target_grant = ask_grant_;

  // Cannot overwrite a file without a user gesture or explicit access to the
  // parent or target (even if overwrites are enabled). Reject with a
  // permission error.
  if (target_present() || (gesture_required() && !gesture_present())) {
    ExpectFileRenameFailure(
        /*parent=*/dir_.GetPath(), source, target, parent_grant, target_grant,
        blink::mojom::FileSystemAccessStatus::kPermissionDenied);
  } else {
    ExpectFileRenameSuccess(
        /*parent=*/dir_.GetPath(), source, target, parent_grant, target_grant);
  }
}

TEST_P(FileSystemAccessFileHandleImplMovePermissionsTest, Rename_SameFile) {
  auto [source, target] = CreateSourceAndMaybeTarget();

  target = source;

  auto parent_grant = allow_grant_;

  // We want ExpectFileRenameSuccess(), but without most EXPECT_CALLs since we
  // should exit early if we detect that the target is the source.
  auto handle = GetHandleWithPermissions(source, /*read=*/true, /*write=*/true);

  EXPECT_CALL(permission_context_,
              GetReadPermissionGrant(
                  test_src_storage_key_.origin(), dir_.GetPath(),
                  FileSystemAccessPermissionContext::HandleType::kDirectory,
                  FileSystemAccessPermissionContext::UserAction::kNone))
      .WillOnce(testing::Return(parent_grant));
  EXPECT_CALL(permission_context_,
              GetWritePermissionGrant(
                  test_src_storage_key_.origin(), dir_.GetPath(),
                  FileSystemAccessPermissionContext::HandleType::kDirectory,
                  FileSystemAccessPermissionContext::UserAction::kNone))
      .WillOnce(testing::Return(parent_grant));

  base::test::TestFuture<blink::mojom::FileSystemAccessErrorPtr> future;
  handle->Rename(target.BaseName().AsUTF8Unsafe(), future.GetCallback());
  EXPECT_EQ(future.Get()->status, blink::mojom::FileSystemAccessStatus::kOk);
  EXPECT_TRUE(base::PathExists(source));
}

TEST_P(FileSystemAccessFileHandleImplMovePermissionsTest,
       Move_HasTargetWriteAccess) {
  auto [source, target] = CreateSourceAndMaybeTarget();

  // The site has not yet asked for access to the target entry.
  auto target_grant = ask_grant_;

  if (overwrites_disabled() && target_present()) {
    ExpectFileMoveFailure(
        /*parent=*/dir_.GetPath(), source, target, target_grant,
        blink::mojom::FileSystemAccessStatus::kInvalidModificationError);
  } else {
    ExpectFileMoveSuccess(
        /*parent=*/dir_.GetPath(), source, target, target_grant);
  }
}

TEST_P(FileSystemAccessFileHandleImplMovePermissionsTest,
       Move_NoTargetWriteAccessFails) {
  auto [source, target] = CreateSourceAndMaybeTarget();

  // The user has explicitly denied access to the target entry.
  auto target_grant = deny_grant_;

  ExpectFileMoveFailure(
      /*parent=*/dir_.GetPath(), source, target, target_grant,
      blink::mojom::FileSystemAccessStatus::kPermissionDenied);
}

TEST_P(FileSystemAccessFileHandleImplMovePermissionsTest,
       Move_AskTargetWriteAccess) {
  auto [source, target] = CreateSourceAndMaybeTarget();

  // The site has not yet asked for access to the target entry.
  auto target_grant = ask_grant_;

  if (overwrites_disabled() && target_present()) {
    ExpectFileMoveFailure(
        /*parent=*/dir_.GetPath(), source, target, target_grant,
        blink::mojom::FileSystemAccessStatus::kInvalidModificationError);
  } else {
    ExpectFileMoveSuccess(
        /*parent=*/dir_.GetPath(), source, target, target_grant);
  }
}

TEST_P(FileSystemAccessFileHandleImplMovePermissionsTest, Move_SameFile) {
  auto [source, target] = CreateSourceAndMaybeTarget();

  target = source;

  // We want ExpectFileMoveSuccess(), but without any EXPECT_CALLs since we
  // should exit early if we detect that the target is the source.
  auto dest_dir_handle =
      GetDirectoryHandleWithPermissions(dir_.GetPath(), /*read=*/true,
                                        /*write=*/true);
  auto handle = GetHandleWithPermissions(source, /*read=*/true, /*write=*/true);

  mojo::PendingRemote<blink::mojom::FileSystemAccessTransferToken> dir_remote;
  manager_->CreateTransferToken(*dest_dir_handle,
                                dir_remote.InitWithNewPipeAndPassReceiver());

  base::test::TestFuture<blink::mojom::FileSystemAccessErrorPtr> future;
  handle->Move(std::move(dir_remote), target.BaseName().AsUTF8Unsafe(),
               future.GetCallback());
  EXPECT_EQ(future.Get()->status, blink::mojom::FileSystemAccessStatus::kOk);
  EXPECT_TRUE(base::PathExists(source));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    FileSystemAccessFileHandleImplMovePermissionsTest,
    ::testing::Combine(
        // Is kFileSystemAccessDoNotOverwriteOnMove flag enabled?
        ::testing::Bool(),
        // Is there a file to be overwritten?
        ::testing::Bool(),
        // Is kFileSystemAccessRenameWithoutParentAccessRequiresUserActivation
        // flag enabled?
        ::testing::Bool(),
        // Does the site have user activation?
        ::testing::Bool()));

}  // namespace content
