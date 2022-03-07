// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/file_system_access/file_system_access_manager_impl.h"

#include <string>
#include <utility>
#include <vector>

#include "base/callback_helpers.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "content/browser/file_system_access/file_system_access_data_transfer_token_impl.h"
#include "content/browser/file_system_access/file_system_access_directory_handle_impl.h"
#include "content/browser/file_system_access/file_system_access_file_handle_impl.h"
#include "content/browser/file_system_access/file_system_access_transfer_token_impl.h"
#include "content/browser/file_system_access/fixed_file_system_access_permission_grant.h"
#include "content/browser/file_system_access/mock_file_system_access_permission_context.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_web_contents_factory.h"
#include "content/test/test_render_frame_host.h"
#include "content/test/test_web_contents.h"
#include "mojo/public/c/system/data_pipe.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/data_pipe_drainer.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "mojo/public/mojom/base/file_info.mojom.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/browser/test/async_file_test_helper.h"
#include "storage/browser/test/mock_quota_manager.h"
#include "storage/browser/test/mock_quota_manager_proxy.h"
#include "storage/browser/test/mock_special_storage_policy.h"
#include "storage/browser/test/quota_manager_proxy_sync.h"
#include "storage/browser/test/test_file_system_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/blob/blob.mojom.h"
#include "third_party/blink/public/mojom/blob/serialized_blob.mojom.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_data_transfer_token.mojom.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_manager.mojom-forward.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_manager.mojom-shared.h"
#include "url/gurl.h"

namespace content {

namespace {

// A helper class that can be passed to mojo::DataPipeDrainer to read the
// information coming through a data pipe as a string and call a callback
// on completion.
class StringDataPipeReader : public mojo::DataPipeDrainer::Client {
 public:
  StringDataPipeReader(std::string* data_out, base::OnceClosure done_callback)
      : data_out_(data_out), done_callback_(std::move(done_callback)) {}

  void OnDataAvailable(const void* data, size_t num_bytes) override {
    data_out_->append(static_cast<const char*>(data), num_bytes);
  }

  void OnDataComplete() override { std::move(done_callback_).Run(); }

 private:
  raw_ptr<std::string> data_out_;
  base::OnceClosure done_callback_;
};

// Reads the incoming data in `pipe` as an `std::string`. Blocks until all the
// data has been read.
std::string ReadDataPipe(mojo::ScopedDataPipeConsumerHandle pipe) {
  base::RunLoop loop;
  std::string data;
  StringDataPipeReader reader(&data, loop.QuitClosure());
  mojo::DataPipeDrainer drainer(&reader, std::move(pipe));
  loop.Run();
  return data;
}

// Returns the contents of the file referred to by `file_remote` as a
// `std::string`.
std::string ReadStringFromFileRemote(
    mojo::Remote<blink::mojom::FileSystemAccessFileHandle> file_remote) {
  base::RunLoop await_get_blob;
  mojo::Remote<blink::mojom::Blob> blob;
  file_remote->AsBlob(base::BindLambdaForTesting(
      [&](blink::mojom::FileSystemAccessErrorPtr result,
          const base::File::Info& info,
          blink::mojom::SerializedBlobPtr received_blob) {
        EXPECT_EQ(result->status, blink::mojom::FileSystemAccessStatus::kOk);
        EXPECT_FALSE(received_blob.is_null());
        blob.Bind(std::move(received_blob->blob));
        await_get_blob.Quit();
      }));
  await_get_blob.Run();

  if (!blob) {
    return "";
  }

  mojo::ScopedDataPipeConsumerHandle consumer_handle;
  mojo::ScopedDataPipeProducerHandle producer_handle;

  CHECK_EQ(mojo::CreateDataPipe(nullptr, producer_handle, consumer_handle),
           MOJO_RESULT_OK);

  blob->ReadAll(std::move(producer_handle), mojo::NullRemote());

  return ReadDataPipe(std::move(consumer_handle));
}

constexpr char kTestMountPoint[] = "testfs";

}  // namespace

using base::test::RunOnceCallback;
using blink::mojom::PermissionStatus;
using HandleType = content::FileSystemAccessPermissionContext::HandleType;
using PathType = content::FileSystemAccessPermissionContext::PathType;
using PathInfo = content::FileSystemAccessPermissionContext::PathInfo;

class FileSystemAccessManagerImplTest : public testing::Test {
 public:
  FileSystemAccessManagerImplTest()
      : special_storage_policy_(
            base::MakeRefCounted<storage::MockSpecialStoragePolicy>()),
        task_environment_(base::test::TaskEnvironment::MainThreadType::IO) {}

  void SetUp() override {
    ASSERT_TRUE(dir_.CreateUniqueTempDir());
    ASSERT_TRUE(dir_.GetPath().IsAbsolute());

    quota_manager_ = base::MakeRefCounted<storage::MockQuotaManager>(
        /*is_incognito=*/false, dir_.GetPath(),
        base::ThreadTaskRunnerHandle::Get(), special_storage_policy_);
    quota_manager_proxy_ = base::MakeRefCounted<storage::MockQuotaManagerProxy>(
        quota_manager_.get(), base::ThreadTaskRunnerHandle::Get().get());

    file_system_context_ = storage::CreateFileSystemContextForTesting(
        quota_manager_proxy_.get(), dir_.GetPath());

    // Register an external mount point to test support for virtual paths.
    // This maps the virtual path a native local path to make sure an external
    // path round trips as an external path, even if the path resolves to a
    // local path.
    storage::ExternalMountPoints::GetSystemInstance()->RegisterFileSystem(
        kTestMountPoint, storage::kFileSystemTypeLocal,
        storage::FileSystemMountOption(), dir_.GetPath());

    chrome_blob_context_ = base::MakeRefCounted<ChromeBlobStorageContext>();
    chrome_blob_context_->InitializeOnIOThread(base::FilePath(),
                                               base::FilePath(), nullptr);

    manager_ = base::MakeRefCounted<FileSystemAccessManagerImpl>(
        file_system_context_, chrome_blob_context_, &permission_context_,
        /*off_the_record=*/false);

    web_contents_ = web_contents_factory_.CreateWebContents(&browser_context_);
    static_cast<TestWebContents*>(web_contents_)->NavigateAndCommit(kTestURL);

    manager_->BindReceiver(kBindingContext,
                           manager_remote_.BindNewPipeAndPassReceiver());
  }

  void TearDown() override {
    storage::ExternalMountPoints::GetSystemInstance()->RevokeFileSystem(
        kTestMountPoint);
    // TODO(mek): Figure out what code is leaking open files, and uncomment this
    // to prevent further regressions.
    // ASSERT_TRUE(dir_.Delete());
  }

  template <typename HandleType>
  PermissionStatus GetPermissionStatusSync(bool writable, HandleType* handle) {
    PermissionStatus result;
    base::RunLoop loop;
    handle->GetPermissionStatus(
        writable, base::BindLambdaForTesting([&](PermissionStatus status) {
          result = status;
          loop.Quit();
        }));
    loop.Run();
    return result;
  }

  mojo::Remote<blink::mojom::FileSystemAccessDirectoryHandle>
  GetHandleForDirectory(const base::FilePath& path) {
    auto grant = base::MakeRefCounted<FixedFileSystemAccessPermissionGrant>(
        FixedFileSystemAccessPermissionGrant::PermissionStatus::GRANTED, path);

    EXPECT_CALL(permission_context_,
                GetReadPermissionGrant(
                    kTestStorageKey.origin(), path, HandleType::kDirectory,
                    FileSystemAccessPermissionContext::UserAction::kOpen))
        .WillOnce(testing::Return(grant));
    EXPECT_CALL(permission_context_,
                GetWritePermissionGrant(
                    kTestStorageKey.origin(), path, HandleType::kDirectory,
                    FileSystemAccessPermissionContext::UserAction::kOpen))
        .WillOnce(testing::Return(grant));

    blink::mojom::FileSystemAccessEntryPtr entry =
        manager_->CreateDirectoryEntryFromPath(
            kBindingContext, FileSystemAccessEntryFactory::PathType::kLocal,
            path, FileSystemAccessPermissionContext::UserAction::kOpen);
    return mojo::Remote<blink::mojom::FileSystemAccessDirectoryHandle>(
        std::move(entry->entry_handle->get_directory()));
  }

  FileSystemAccessTransferTokenImpl* SerializeAndDeserializeToken(
      mojo::PendingRemote<blink::mojom::FileSystemAccessTransferToken>
          token_remote) {
    std::vector<uint8_t> serialized;
    base::RunLoop serialize_loop;
    manager_->SerializeHandle(
        std::move(token_remote),
        base::BindLambdaForTesting([&](const std::vector<uint8_t>& bits) {
          EXPECT_FALSE(bits.empty());
          serialized = bits;
          serialize_loop.Quit();
        }));
    serialize_loop.Run();

    manager_->DeserializeHandle(kTestStorageKey, serialized,
                                token_remote.InitWithNewPipeAndPassReceiver());
    base::RunLoop resolve_loop;
    FileSystemAccessTransferTokenImpl* result;
    manager_->ResolveTransferToken(
        std::move(token_remote),
        base::BindLambdaForTesting(
            [&](FileSystemAccessTransferTokenImpl* token) {
              result = token;
              resolve_loop.Quit();
            }));
    resolve_loop.Run();
    return result;
  }

  void GetEntryFromDataTransferTokenFileTest(
      const base::FilePath& file_path,
      FileSystemAccessEntryFactory::PathType path_type,
      const std::string& expected_file_contents) {
    // Create a token representing a dropped file at `file_path`.
    mojo::PendingRemote<blink::mojom::FileSystemAccessDataTransferToken>
        token_remote;
    manager_->CreateFileSystemAccessDataTransferToken(
        path_type, file_path, kBindingContext.process_id(),
        token_remote.InitWithNewPipeAndPassReceiver());

    // Expect permission requests when the token is sent to be redeemed.
    EXPECT_CALL(
        permission_context_,
        GetReadPermissionGrant(
            kTestStorageKey.origin(), file_path, HandleType::kFile,
            FileSystemAccessPermissionContext::UserAction::kDragAndDrop))
        .WillOnce(testing::Return(allow_grant_));

    EXPECT_CALL(
        permission_context_,
        GetWritePermissionGrant(
            kTestStorageKey.origin(), file_path, HandleType::kFile,
            FileSystemAccessPermissionContext::UserAction::kDragAndDrop))
        .WillOnce(testing::Return(allow_grant_));

    // Attempt to resolve `token_remote` and store the resulting
    // FileSystemAccessFileHandle in `file_remote`.
    base::RunLoop await_token_resolution;
    blink::mojom::FileSystemAccessEntryPtr file_system_access_entry;
    manager_remote_->GetEntryFromDataTransferToken(
        std::move(token_remote),
        base::BindLambdaForTesting([&](blink::mojom::FileSystemAccessEntryPtr
                                           returned_file_system_access_entry) {
          file_system_access_entry =
              std::move(returned_file_system_access_entry);
          await_token_resolution.Quit();
        }));
    await_token_resolution.Run();

    ASSERT_FALSE(file_system_access_entry.is_null());
    ASSERT_TRUE(file_system_access_entry->entry_handle->is_file());
    mojo::Remote<blink::mojom::FileSystemAccessFileHandle> file_handle(
        std::move(file_system_access_entry->entry_handle->get_file()));

    // Check to see if the resulting FileSystemAccessFileHandle can read the
    // contents of the file at `file_path`.
    EXPECT_EQ(ReadStringFromFileRemote(std::move(file_handle)),
              expected_file_contents);
  }

  void GetEntryFromDataTransferTokenDirectoryTest(
      const base::FilePath& dir_path,
      FileSystemAccessEntryFactory::PathType path_type,
      const std::string& expected_child_file_name) {
    mojo::PendingRemote<blink::mojom::FileSystemAccessDataTransferToken>
        token_remote;
    manager_->CreateFileSystemAccessDataTransferToken(
        path_type, dir_path, kBindingContext.process_id(),
        token_remote.InitWithNewPipeAndPassReceiver());

    // Expect permission requests when the token is sent to be redeemed.
    EXPECT_CALL(
        permission_context_,
        GetReadPermissionGrant(
            kTestStorageKey.origin(), dir_path, HandleType::kDirectory,
            FileSystemAccessPermissionContext::UserAction::kDragAndDrop))
        .WillOnce(testing::Return(allow_grant_));

    EXPECT_CALL(
        permission_context_,
        GetWritePermissionGrant(
            kTestStorageKey.origin(), dir_path, HandleType::kDirectory,
            FileSystemAccessPermissionContext::UserAction::kDragAndDrop))
        .WillOnce(testing::Return(allow_grant_));

    // Attempt to resolve `token_remote` and store the resulting
    // FileSystemAccessDirectoryHandle in `dir_remote`.
    base::RunLoop await_token_resolution;
    blink::mojom::FileSystemAccessEntryPtr file_system_access_entry;
    manager_remote_->GetEntryFromDataTransferToken(
        std::move(token_remote),
        base::BindLambdaForTesting([&](blink::mojom::FileSystemAccessEntryPtr
                                           returned_file_system_access_entry) {
          file_system_access_entry =
              std::move(returned_file_system_access_entry);
          await_token_resolution.Quit();
        }));
    await_token_resolution.Run();

    ASSERT_FALSE(file_system_access_entry.is_null());
    ASSERT_TRUE(file_system_access_entry->entry_handle->is_directory());
    mojo::Remote<blink::mojom::FileSystemAccessDirectoryHandle> dir_remote(
        std::move(file_system_access_entry->entry_handle->get_directory()));

    // Use `dir_remote` to verify that dir_path contains a child called
    // expected_child_file_name.
    base::RunLoop await_get_file;
    dir_remote->GetFile(
        expected_child_file_name, /*create=*/false,
        base::BindLambdaForTesting(
            [&](blink::mojom::FileSystemAccessErrorPtr result,
                mojo::PendingRemote<blink::mojom::FileSystemAccessFileHandle>
                    file_handle) {
              await_get_file.Quit();
              ASSERT_EQ(blink::mojom::FileSystemAccessStatus::kOk,
                        result->status);
            }));
    await_get_file.Run();
  }

 protected:
  const GURL kTestURL = GURL("https://example.com/test");
  const blink::StorageKey kTestStorageKey =
      blink::StorageKey::CreateFromStringForTesting("https://example.com/test");
  const int kProcessId = 1;
  const int kFrameRoutingId = 2;
  const GlobalRenderFrameHostId kFrameId{kProcessId, kFrameRoutingId};
  const FileSystemAccessManagerImpl::BindingContext kBindingContext = {
      kTestStorageKey, kTestURL, kFrameId};

  scoped_refptr<storage::MockSpecialStoragePolicy> special_storage_policy_;

  base::ScopedTempDir dir_;
  BrowserTaskEnvironment task_environment_;

  TestBrowserContext browser_context_;
  TestWebContentsFactory web_contents_factory_;

  scoped_refptr<storage::FileSystemContext> file_system_context_;
  scoped_refptr<ChromeBlobStorageContext> chrome_blob_context_;
  scoped_refptr<storage::MockQuotaManager> quota_manager_;
  scoped_refptr<storage::MockQuotaManagerProxy> quota_manager_proxy_;

  raw_ptr<WebContents> web_contents_;

  testing::StrictMock<MockFileSystemAccessPermissionContext>
      permission_context_;
  scoped_refptr<FileSystemAccessManagerImpl> manager_;
  mojo::Remote<blink::mojom::FileSystemAccessManager> manager_remote_;

  scoped_refptr<FixedFileSystemAccessPermissionGrant> ask_grant_ =
      base::MakeRefCounted<FixedFileSystemAccessPermissionGrant>(
          FixedFileSystemAccessPermissionGrant::PermissionStatus::ASK,
          base::FilePath());
  scoped_refptr<FixedFileSystemAccessPermissionGrant> ask_grant2_ =
      base::MakeRefCounted<FixedFileSystemAccessPermissionGrant>(
          FixedFileSystemAccessPermissionGrant::PermissionStatus::ASK,
          base::FilePath());
  scoped_refptr<FixedFileSystemAccessPermissionGrant> allow_grant_ =
      base::MakeRefCounted<FixedFileSystemAccessPermissionGrant>(
          FixedFileSystemAccessPermissionGrant::PermissionStatus::GRANTED,
          base::FilePath());
};

TEST_F(FileSystemAccessManagerImplTest, GetSandboxedFileSystem_CreateBucket) {
  mojo::PendingRemote<blink::mojom::FileSystemAccessDirectoryHandle>
      directory_remote;
  base::RunLoop loop;
  manager_remote_->GetSandboxedFileSystem(base::BindLambdaForTesting(
      [&](blink::mojom::FileSystemAccessErrorPtr result,
          mojo::PendingRemote<blink::mojom::FileSystemAccessDirectoryHandle>
              handle) {
        EXPECT_EQ(blink::mojom::FileSystemAccessStatus::kOk, result->status);
        directory_remote = std::move(handle);
        loop.Quit();
      }));
  loop.Run();
  mojo::Remote<blink::mojom::FileSystemAccessDirectoryHandle> root(
      std::move(directory_remote));
  ASSERT_TRUE(root);

  storage::QuotaManagerProxySync quota_manager_proxy_sync(
      quota_manager_proxy_.get());

  // Check default bucket exists.
  storage::QuotaErrorOr<storage::BucketInfo> result =
      quota_manager_proxy_sync.GetBucket(kTestStorageKey,
                                         storage::kDefaultBucketName,
                                         blink::mojom::StorageType::kTemporary);
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(result->name, storage::kDefaultBucketName);
  EXPECT_EQ(result->storage_key, kTestStorageKey);
  EXPECT_GT(result->id.value(), 0);
}

TEST_F(FileSystemAccessManagerImplTest, GetSandboxedFileSystem_Permissions) {
  mojo::PendingRemote<blink::mojom::FileSystemAccessDirectoryHandle>
      directory_remote;
  base::RunLoop loop;
  manager_remote_->GetSandboxedFileSystem(base::BindLambdaForTesting(
      [&](blink::mojom::FileSystemAccessErrorPtr result,
          mojo::PendingRemote<blink::mojom::FileSystemAccessDirectoryHandle>
              handle) {
        EXPECT_EQ(blink::mojom::FileSystemAccessStatus::kOk, result->status);
        directory_remote = std::move(handle);
        loop.Quit();
      }));
  loop.Run();
  mojo::Remote<blink::mojom::FileSystemAccessDirectoryHandle> root(
      std::move(directory_remote));
  ASSERT_TRUE(root);
  EXPECT_EQ(PermissionStatus::GRANTED,
            GetPermissionStatusSync(/*writable=*/false, root.get()));
  EXPECT_EQ(PermissionStatus::GRANTED,
            GetPermissionStatusSync(/*writable=*/true, root.get()));
}

TEST_F(FileSystemAccessManagerImplTest, CreateFileEntryFromPath_Permissions) {
  const base::FilePath kTestPath(dir_.GetPath().AppendASCII("foo"));

  EXPECT_CALL(permission_context_,
              GetReadPermissionGrant(
                  kTestStorageKey.origin(), kTestPath, HandleType::kFile,
                  FileSystemAccessPermissionContext::UserAction::kOpen))
      .WillOnce(testing::Return(allow_grant_));
  EXPECT_CALL(permission_context_,
              GetWritePermissionGrant(
                  kTestStorageKey.origin(), kTestPath, HandleType::kFile,
                  FileSystemAccessPermissionContext::UserAction::kOpen))
      .WillOnce(testing::Return(ask_grant_));

  blink::mojom::FileSystemAccessEntryPtr entry =
      manager_->CreateFileEntryFromPath(
          kBindingContext, FileSystemAccessEntryFactory::PathType::kLocal,
          kTestPath, FileSystemAccessPermissionContext::UserAction::kOpen);
  mojo::Remote<blink::mojom::FileSystemAccessFileHandle> handle(
      std::move(entry->entry_handle->get_file()));

  EXPECT_EQ(PermissionStatus::GRANTED,
            GetPermissionStatusSync(/*writable=*/false, handle.get()));
  EXPECT_EQ(PermissionStatus::ASK,
            GetPermissionStatusSync(/*writable=*/true, handle.get()));
}

TEST_F(FileSystemAccessManagerImplTest,
       CreateWritableFileEntryFromPath_Permissions) {
  const base::FilePath kTestPath(dir_.GetPath().AppendASCII("foo"));

  EXPECT_CALL(permission_context_,
              GetReadPermissionGrant(
                  kTestStorageKey.origin(), kTestPath, HandleType::kFile,
                  FileSystemAccessPermissionContext::UserAction::kSave))
      .WillOnce(testing::Return(allow_grant_));
  EXPECT_CALL(permission_context_,
              GetWritePermissionGrant(
                  kTestStorageKey.origin(), kTestPath, HandleType::kFile,
                  FileSystemAccessPermissionContext::UserAction::kSave))
      .WillOnce(testing::Return(allow_grant_));

  blink::mojom::FileSystemAccessEntryPtr entry =
      manager_->CreateFileEntryFromPath(
          kBindingContext, FileSystemAccessEntryFactory::PathType::kLocal,
          kTestPath, FileSystemAccessPermissionContext::UserAction::kSave);
  mojo::Remote<blink::mojom::FileSystemAccessFileHandle> handle(
      std::move(entry->entry_handle->get_file()));

  EXPECT_EQ(PermissionStatus::GRANTED,
            GetPermissionStatusSync(/*writable=*/false, handle.get()));
  EXPECT_EQ(PermissionStatus::GRANTED,
            GetPermissionStatusSync(/*writable=*/true, handle.get()));
}

TEST_F(FileSystemAccessManagerImplTest,
       CreateDirectoryEntryFromPath_Permissions) {
  const base::FilePath kTestPath(dir_.GetPath().AppendASCII("foo"));

  EXPECT_CALL(permission_context_,
              GetReadPermissionGrant(
                  kTestStorageKey.origin(), kTestPath, HandleType::kDirectory,
                  FileSystemAccessPermissionContext::UserAction::kOpen))
      .WillOnce(testing::Return(allow_grant_));
  EXPECT_CALL(permission_context_,
              GetWritePermissionGrant(
                  kTestStorageKey.origin(), kTestPath, HandleType::kDirectory,
                  FileSystemAccessPermissionContext::UserAction::kOpen))
      .WillOnce(testing::Return(ask_grant_));

  blink::mojom::FileSystemAccessEntryPtr entry =
      manager_->CreateDirectoryEntryFromPath(
          kBindingContext, FileSystemAccessEntryFactory::PathType::kLocal,
          kTestPath, FileSystemAccessPermissionContext::UserAction::kOpen);
  mojo::Remote<blink::mojom::FileSystemAccessDirectoryHandle> handle(
      std::move(entry->entry_handle->get_directory()));
  EXPECT_EQ(PermissionStatus::GRANTED,
            GetPermissionStatusSync(/*writable=*/false, handle.get()));
  EXPECT_EQ(PermissionStatus::ASK,
            GetPermissionStatusSync(/*writable=*/true, handle.get()));
}

TEST_F(FileSystemAccessManagerImplTest,
       FileWriterSwapDeletedOnConnectionClose) {
  auto test_file_url = file_system_context_->CreateCrackedFileSystemURL(
      kTestStorageKey, storage::kFileSystemTypeTest,
      base::FilePath::FromUTF8Unsafe("test"));

  auto test_swap_url = file_system_context_->CreateCrackedFileSystemURL(
      kTestStorageKey, storage::kFileSystemTypeTest,
      base::FilePath::FromUTF8Unsafe("test.crswap"));

  ASSERT_EQ(base::File::FILE_OK,
            storage::AsyncFileTestHelper::CreateFile(file_system_context_.get(),
                                                     test_file_url));

  ASSERT_EQ(base::File::FILE_OK,
            storage::AsyncFileTestHelper::CreateFile(file_system_context_.get(),
                                                     test_swap_url));

  auto lock = manager_->TakeWriteLock(
      test_file_url, FileSystemAccessWriteLockManager::WriteLockType::kShared);
  ASSERT_TRUE(lock.has_value());

  mojo::Remote<blink::mojom::FileSystemAccessFileWriter> writer_remote(
      manager_->CreateFileWriter(kBindingContext, test_file_url, test_swap_url,
                                 std::move(lock.value()),
                                 FileSystemAccessManagerImpl::SharedHandleState(
                                     allow_grant_, allow_grant_),
                                 /*auto_close=*/false));

  ASSERT_TRUE(writer_remote.is_bound());
  ASSERT_TRUE(storage::AsyncFileTestHelper::FileExists(
      file_system_context_.get(), test_swap_url,
      storage::AsyncFileTestHelper::kDontCheckSize));

  // Severs the mojo pipe, causing the writer to be destroyed.
  writer_remote.reset();
  base::RunLoop().RunUntilIdle();

  ASSERT_FALSE(storage::AsyncFileTestHelper::FileExists(
      file_system_context_.get(), test_swap_url,
      storage::AsyncFileTestHelper::kDontCheckSize));
}

TEST_F(FileSystemAccessManagerImplTest, FileWriterCloseDoesNotAbortOnDestruct) {
  auto test_file_url = file_system_context_->CreateCrackedFileSystemURL(
      kTestStorageKey, storage::kFileSystemTypeTest,
      base::FilePath::FromUTF8Unsafe("test"));

  auto test_swap_url = file_system_context_->CreateCrackedFileSystemURL(
      kTestStorageKey, storage::kFileSystemTypeTest,
      base::FilePath::FromUTF8Unsafe("test.crswap"));

  ASSERT_EQ(base::File::FILE_OK,
            storage::AsyncFileTestHelper::CreateFileWithData(
                file_system_context_.get(), test_swap_url, "foo", 3));

  auto lock = manager_->TakeWriteLock(
      test_file_url, FileSystemAccessWriteLockManager::WriteLockType::kShared);
  ASSERT_TRUE(lock.has_value());

  mojo::Remote<blink::mojom::FileSystemAccessFileWriter> writer_remote(
      manager_->CreateFileWriter(kBindingContext, test_file_url, test_swap_url,
                                 std::move(lock.value()),
                                 FileSystemAccessManagerImpl::SharedHandleState(
                                     allow_grant_, allow_grant_),
                                 /*auto_close=*/false));

  ASSERT_TRUE(writer_remote.is_bound());
  ASSERT_FALSE(storage::AsyncFileTestHelper::FileExists(
      file_system_context_.get(), test_file_url,
      storage::AsyncFileTestHelper::kDontCheckSize));
  writer_remote->Close(base::DoNothing());

  EXPECT_CALL(permission_context_,
              PerformAfterWriteChecks_(testing::_, kFrameId, testing::_))
      .WillOnce(base::test::RunOnceCallback<2>(
          FileSystemAccessPermissionContext::AfterWriteCheckResult::kAllow));

  // Severs the mojo pipe, but the writer should not be destroyed.
  writer_remote.reset();
  base::RunLoop().RunUntilIdle();

  // Since the close should complete, the swap file should have been destroyed
  // and the write should be reflected in the target file.
  ASSERT_FALSE(storage::AsyncFileTestHelper::FileExists(
      file_system_context_.get(), test_swap_url,
      storage::AsyncFileTestHelper::kDontCheckSize));
  ASSERT_TRUE(storage::AsyncFileTestHelper::FileExists(
      file_system_context_.get(), test_file_url, 3));
}

TEST_F(FileSystemAccessManagerImplTest,
       FileWriterNoWritesIfConnectionLostBeforeClose) {
  auto test_file_url = file_system_context_->CreateCrackedFileSystemURL(
      kTestStorageKey, storage::kFileSystemTypeTest,
      base::FilePath::FromUTF8Unsafe("test"));

  auto test_swap_url = file_system_context_->CreateCrackedFileSystemURL(
      kTestStorageKey, storage::kFileSystemTypeTest,
      base::FilePath::FromUTF8Unsafe("test.crswap"));

  ASSERT_EQ(base::File::FILE_OK,
            storage::AsyncFileTestHelper::CreateFileWithData(
                file_system_context_.get(), test_swap_url, "foo", 3));

  auto lock = manager_->TakeWriteLock(
      test_file_url, FileSystemAccessWriteLockManager::WriteLockType::kShared);
  ASSERT_TRUE(lock.has_value());

  mojo::Remote<blink::mojom::FileSystemAccessFileWriter> writer_remote(
      manager_->CreateFileWriter(kBindingContext, test_file_url, test_swap_url,
                                 std::move(lock.value()),
                                 FileSystemAccessManagerImpl::SharedHandleState(
                                     allow_grant_, allow_grant_),
                                 /*auto_close=*/false));

  // Severs the mojo pipe. The writer should be destroyed.
  writer_remote.reset();
  base::RunLoop().RunUntilIdle();

  // Neither the target file nor the swap file should exist.
  ASSERT_FALSE(storage::AsyncFileTestHelper::FileExists(
      file_system_context_.get(), test_file_url,
      storage::AsyncFileTestHelper::kDontCheckSize));
  ASSERT_FALSE(storage::AsyncFileTestHelper::FileExists(
      file_system_context_.get(), test_swap_url,
      storage::AsyncFileTestHelper::kDontCheckSize));
}

TEST_F(FileSystemAccessManagerImplTest,
       FileWriterAutoCloseIfConnectionLostBeforeClose) {
  auto test_file_url = file_system_context_->CreateCrackedFileSystemURL(
      kTestStorageKey, storage::kFileSystemTypeTest,
      base::FilePath::FromUTF8Unsafe("test"));

  auto test_swap_url = file_system_context_->CreateCrackedFileSystemURL(
      kTestStorageKey, storage::kFileSystemTypeTest,
      base::FilePath::FromUTF8Unsafe("test.crswap"));

  ASSERT_EQ(base::File::FILE_OK,
            storage::AsyncFileTestHelper::CreateFileWithData(
                file_system_context_.get(), test_swap_url, "foo", 3));

  auto lock = manager_->TakeWriteLock(
      test_file_url, FileSystemAccessWriteLockManager::WriteLockType::kShared);
  ASSERT_TRUE(lock.has_value());

  mojo::Remote<blink::mojom::FileSystemAccessFileWriter> writer_remote(
      manager_->CreateFileWriter(kBindingContext, test_file_url, test_swap_url,
                                 std::move(lock.value()),
                                 FileSystemAccessManagerImpl::SharedHandleState(
                                     allow_grant_, allow_grant_),
                                 /*auto_close=*/true));

  ASSERT_TRUE(writer_remote.is_bound());
  ASSERT_FALSE(storage::AsyncFileTestHelper::FileExists(
      file_system_context_.get(), test_file_url,
      storage::AsyncFileTestHelper::kDontCheckSize));

  EXPECT_CALL(permission_context_,
              PerformAfterWriteChecks_(testing::_, kFrameId, testing::_))
      .WillOnce(base::test::RunOnceCallback<2>(
          FileSystemAccessPermissionContext::AfterWriteCheckResult::kAllow));

  // Severs the mojo pipe. Since autoClose was specified, the Writer will not be
  // destroyed until the file is written out.
  writer_remote.reset();
  base::RunLoop().RunUntilIdle();

  // Since the close should complete, the swap file should have been destroyed
  // and the write should be reflected in the target file.
  EXPECT_FALSE(storage::AsyncFileTestHelper::FileExists(
      file_system_context_.get(), test_swap_url,
      storage::AsyncFileTestHelper::kDontCheckSize));
  ASSERT_TRUE(storage::AsyncFileTestHelper::FileExists(
      file_system_context_.get(), test_file_url, 3));
}

TEST_F(FileSystemAccessManagerImplTest, SerializeHandle_SandboxedFile) {
  auto test_file_url = file_system_context_->CreateCrackedFileSystemURL(
      kTestStorageKey, storage::kFileSystemTypeTemporary,
      base::FilePath::FromUTF8Unsafe("test/foo/bar"));
  FileSystemAccessFileHandleImpl file(manager_.get(), kBindingContext,
                                      test_file_url, {ask_grant_, ask_grant_});
  mojo::PendingRemote<blink::mojom::FileSystemAccessTransferToken> token_remote;
  manager_->CreateTransferToken(file,
                                token_remote.InitWithNewPipeAndPassReceiver());

  FileSystemAccessTransferTokenImpl* token =
      SerializeAndDeserializeToken(std::move(token_remote));
  ASSERT_TRUE(token);
  EXPECT_EQ(test_file_url, token->url());
  EXPECT_EQ(HandleType::kFile, token->type());

  // Deserialized sandboxed filesystem handles should always be readable and
  // writable.
  ASSERT_TRUE(token->GetReadGrant());
  EXPECT_EQ(PermissionStatus::GRANTED, token->GetReadGrant()->GetStatus());
  ASSERT_TRUE(token->GetWriteGrant());
  EXPECT_EQ(PermissionStatus::GRANTED, token->GetWriteGrant()->GetStatus());
}

TEST_F(FileSystemAccessManagerImplTest, SerializeHandle_SandboxedDirectory) {
  auto test_file_url = file_system_context_->CreateCrackedFileSystemURL(
      kTestStorageKey, storage::kFileSystemTypeTemporary,
      base::FilePath::FromUTF8Unsafe("hello/world/"));
  FileSystemAccessDirectoryHandleImpl directory(
      manager_.get(), kBindingContext, test_file_url, {ask_grant_, ask_grant_});
  mojo::PendingRemote<blink::mojom::FileSystemAccessTransferToken> token_remote;
  manager_->CreateTransferToken(directory,
                                token_remote.InitWithNewPipeAndPassReceiver());

  FileSystemAccessTransferTokenImpl* token =
      SerializeAndDeserializeToken(std::move(token_remote));
  ASSERT_TRUE(token);
  EXPECT_EQ(test_file_url, token->url());
  EXPECT_EQ(HandleType::kDirectory, token->type());

  // Deserialized sandboxed filesystem handles should always be readable and
  // writable.
  ASSERT_TRUE(token->GetReadGrant());
  EXPECT_EQ(PermissionStatus::GRANTED, token->GetReadGrant()->GetStatus());
  ASSERT_TRUE(token->GetWriteGrant());
  EXPECT_EQ(PermissionStatus::GRANTED, token->GetWriteGrant()->GetStatus());
}

TEST_F(FileSystemAccessManagerImplTest, SerializeHandle_Native_SingleFile) {
  const base::FilePath kTestPath(dir_.GetPath().AppendASCII("foo"));

  auto grant = base::MakeRefCounted<FixedFileSystemAccessPermissionGrant>(
      FixedFileSystemAccessPermissionGrant::PermissionStatus::GRANTED,
      kTestPath);

  // Expect calls to get grants when creating the initial handle.
  EXPECT_CALL(permission_context_,
              GetReadPermissionGrant(
                  kTestStorageKey.origin(), kTestPath, HandleType::kFile,
                  FileSystemAccessPermissionContext::UserAction::kOpen))
      .WillOnce(testing::Return(grant));
  EXPECT_CALL(permission_context_,
              GetWritePermissionGrant(
                  kTestStorageKey.origin(), kTestPath, HandleType::kFile,
                  FileSystemAccessPermissionContext::UserAction::kOpen))
      .WillOnce(testing::Return(grant));

  blink::mojom::FileSystemAccessEntryPtr entry =
      manager_->CreateFileEntryFromPath(
          kBindingContext, FileSystemAccessEntryFactory::PathType::kLocal,
          kTestPath, FileSystemAccessPermissionContext::UserAction::kOpen);
  mojo::Remote<blink::mojom::FileSystemAccessFileHandle> handle(
      std::move(entry->entry_handle->get_file()));

  mojo::PendingRemote<blink::mojom::FileSystemAccessTransferToken> token_remote;
  handle->Transfer(token_remote.InitWithNewPipeAndPassReceiver());

  // Deserializing tokens should re-request grants, with correct user action.
  EXPECT_CALL(
      permission_context_,
      GetReadPermissionGrant(
          kTestStorageKey.origin(), kTestPath, HandleType::kFile,
          FileSystemAccessPermissionContext::UserAction::kLoadFromStorage))
      .WillOnce(testing::Return(ask_grant_));
  EXPECT_CALL(
      permission_context_,
      GetWritePermissionGrant(
          kTestStorageKey.origin(), kTestPath, HandleType::kFile,
          FileSystemAccessPermissionContext::UserAction::kLoadFromStorage))
      .WillOnce(testing::Return(ask_grant2_));

  FileSystemAccessTransferTokenImpl* token =
      SerializeAndDeserializeToken(std::move(token_remote));
  ASSERT_TRUE(token);
  const storage::FileSystemURL& url = token->url();
  EXPECT_TRUE(url.origin().opaque());
  EXPECT_EQ(kTestPath, url.path());
  EXPECT_EQ(storage::kFileSystemTypeLocal, url.type());
  EXPECT_EQ(storage::kFileSystemTypeLocal, url.mount_type());
  EXPECT_EQ(HandleType::kFile, token->type());
  EXPECT_EQ(ask_grant_, token->GetReadGrant());
  EXPECT_EQ(ask_grant2_, token->GetWriteGrant());
}

TEST_F(FileSystemAccessManagerImplTest,
       SerializeHandle_Native_SingleDirectory) {
  const base::FilePath kTestPath(dir_.GetPath().AppendASCII("foobar"));
  mojo::Remote<blink::mojom::FileSystemAccessDirectoryHandle> handle =
      GetHandleForDirectory(kTestPath);

  mojo::PendingRemote<blink::mojom::FileSystemAccessTransferToken> token_remote;
  handle->Transfer(token_remote.InitWithNewPipeAndPassReceiver());

  // Deserializing tokens should re-request grants, with correct user action.
  EXPECT_CALL(
      permission_context_,
      GetReadPermissionGrant(
          kTestStorageKey.origin(), kTestPath, HandleType::kDirectory,
          FileSystemAccessPermissionContext::UserAction::kLoadFromStorage))
      .WillOnce(testing::Return(ask_grant_));
  EXPECT_CALL(
      permission_context_,
      GetWritePermissionGrant(
          kTestStorageKey.origin(), kTestPath, HandleType::kDirectory,
          FileSystemAccessPermissionContext::UserAction::kLoadFromStorage))
      .WillOnce(testing::Return(ask_grant2_));

  FileSystemAccessTransferTokenImpl* token =
      SerializeAndDeserializeToken(std::move(token_remote));
  ASSERT_TRUE(token);
  const storage::FileSystemURL& url = token->url();
  EXPECT_TRUE(url.origin().opaque());
  EXPECT_EQ(kTestPath, url.path());
  EXPECT_EQ(storage::kFileSystemTypeLocal, url.type());
  EXPECT_EQ(storage::kFileSystemTypeLocal, url.mount_type());
  EXPECT_EQ(HandleType::kDirectory, token->type());
  EXPECT_EQ(ask_grant_, token->GetReadGrant());
  EXPECT_EQ(ask_grant2_, token->GetWriteGrant());
}

TEST_F(FileSystemAccessManagerImplTest,
       SerializeHandle_Native_FileInsideDirectory) {
  const base::FilePath kDirectoryPath(dir_.GetPath().AppendASCII("foo"));
  const std::string kTestName = "test file name ☺";
  base::CreateDirectory(kDirectoryPath);

  mojo::Remote<blink::mojom::FileSystemAccessDirectoryHandle> directory_handle =
      GetHandleForDirectory(kDirectoryPath);

  mojo::Remote<blink::mojom::FileSystemAccessFileHandle> file_handle;
  base::RunLoop get_file_loop;
  directory_handle->GetFile(
      kTestName, /*create=*/true,
      base::BindLambdaForTesting(
          [&](blink::mojom::FileSystemAccessErrorPtr result,
              mojo::PendingRemote<blink::mojom::FileSystemAccessFileHandle>
                  handle) {
            get_file_loop.Quit();
            ASSERT_EQ(blink::mojom::FileSystemAccessStatus::kOk,
                      result->status);
            file_handle.Bind(std::move(handle));
          }));
  get_file_loop.Run();
  ASSERT_TRUE(file_handle.is_bound());

  mojo::PendingRemote<blink::mojom::FileSystemAccessTransferToken> token_remote;
  file_handle->Transfer(token_remote.InitWithNewPipeAndPassReceiver());

  // Deserializing tokens should re-request grants, with correct user action.
  EXPECT_CALL(
      permission_context_,
      GetReadPermissionGrant(
          kTestStorageKey.origin(), kDirectoryPath, HandleType::kDirectory,
          FileSystemAccessPermissionContext::UserAction::kLoadFromStorage))
      .WillOnce(testing::Return(ask_grant_));
  EXPECT_CALL(
      permission_context_,
      GetWritePermissionGrant(
          kTestStorageKey.origin(), kDirectoryPath, HandleType::kDirectory,
          FileSystemAccessPermissionContext::UserAction::kLoadFromStorage))
      .WillOnce(testing::Return(ask_grant2_));

  FileSystemAccessTransferTokenImpl* token =
      SerializeAndDeserializeToken(std::move(token_remote));
  ASSERT_TRUE(token);
  const storage::FileSystemURL& url = token->url();
  EXPECT_TRUE(url.origin().opaque());
  EXPECT_EQ(kDirectoryPath.Append(base::FilePath::FromUTF8Unsafe(kTestName)),
            url.path());
  EXPECT_EQ(storage::kFileSystemTypeLocal, url.type());
  EXPECT_EQ(storage::kFileSystemTypeLocal, url.mount_type());
  EXPECT_EQ(HandleType::kFile, token->type());
  EXPECT_EQ(ask_grant_, token->GetReadGrant());
  EXPECT_EQ(ask_grant2_, token->GetWriteGrant());
}

TEST_F(FileSystemAccessManagerImplTest,
       SerializeHandle_Native_DirectoryInsideDirectory) {
  const base::FilePath kDirectoryPath(dir_.GetPath().AppendASCII("foo"));
  const std::string kTestName = "test dir name";
  base::CreateDirectory(kDirectoryPath);

  mojo::Remote<blink::mojom::FileSystemAccessDirectoryHandle> directory_handle =
      GetHandleForDirectory(kDirectoryPath);

  mojo::Remote<blink::mojom::FileSystemAccessDirectoryHandle> child_handle;
  base::RunLoop get_directory_loop;
  directory_handle->GetDirectory(
      kTestName, /*create=*/true,
      base::BindLambdaForTesting(
          [&](blink::mojom::FileSystemAccessErrorPtr result,
              mojo::PendingRemote<blink::mojom::FileSystemAccessDirectoryHandle>
                  handle) {
            get_directory_loop.Quit();
            ASSERT_EQ(blink::mojom::FileSystemAccessStatus::kOk,
                      result->status);
            child_handle.Bind(std::move(handle));
          }));
  get_directory_loop.Run();
  ASSERT_TRUE(child_handle.is_bound());

  mojo::PendingRemote<blink::mojom::FileSystemAccessTransferToken> token_remote;
  child_handle->Transfer(token_remote.InitWithNewPipeAndPassReceiver());

  // Deserializing tokens should re-request grants, with correct user action.
  EXPECT_CALL(
      permission_context_,
      GetReadPermissionGrant(
          kTestStorageKey.origin(), kDirectoryPath, HandleType::kDirectory,
          FileSystemAccessPermissionContext::UserAction::kLoadFromStorage))
      .WillOnce(testing::Return(ask_grant_));
  EXPECT_CALL(
      permission_context_,
      GetWritePermissionGrant(
          kTestStorageKey.origin(), kDirectoryPath, HandleType::kDirectory,
          FileSystemAccessPermissionContext::UserAction::kLoadFromStorage))
      .WillOnce(testing::Return(ask_grant2_));

  FileSystemAccessTransferTokenImpl* token =
      SerializeAndDeserializeToken(std::move(token_remote));
  ASSERT_TRUE(token);
  const storage::FileSystemURL& url = token->url();
  EXPECT_TRUE(url.origin().opaque());
  EXPECT_EQ(kDirectoryPath.AppendASCII(kTestName), url.path());
  EXPECT_EQ(storage::kFileSystemTypeLocal, url.type());
  EXPECT_EQ(storage::kFileSystemTypeLocal, url.mount_type());
  EXPECT_EQ(HandleType::kDirectory, token->type());
  EXPECT_EQ(ask_grant_, token->GetReadGrant());
  EXPECT_EQ(ask_grant2_, token->GetWriteGrant());
}

TEST_F(FileSystemAccessManagerImplTest, SerializeHandle_ExternalFile) {
  const base::FilePath kTestPath =
      base::FilePath::FromUTF8Unsafe(kTestMountPoint).AppendASCII("foo");

  auto grant = base::MakeRefCounted<FixedFileSystemAccessPermissionGrant>(
      FixedFileSystemAccessPermissionGrant::PermissionStatus::GRANTED,
      kTestPath);

  // Expect calls to get grants when creating the initial handle.
  EXPECT_CALL(permission_context_,
              GetReadPermissionGrant(
                  kTestStorageKey.origin(), kTestPath, HandleType::kFile,
                  FileSystemAccessPermissionContext::UserAction::kOpen))
      .WillOnce(testing::Return(grant));
  EXPECT_CALL(permission_context_,
              GetWritePermissionGrant(
                  kTestStorageKey.origin(), kTestPath, HandleType::kFile,
                  FileSystemAccessPermissionContext::UserAction::kOpen))
      .WillOnce(testing::Return(grant));

  blink::mojom::FileSystemAccessEntryPtr entry =
      manager_->CreateFileEntryFromPath(
          kBindingContext, FileSystemAccessEntryFactory::PathType::kExternal,
          kTestPath, FileSystemAccessPermissionContext::UserAction::kOpen);
  mojo::Remote<blink::mojom::FileSystemAccessFileHandle> handle(
      std::move(entry->entry_handle->get_file()));

  mojo::PendingRemote<blink::mojom::FileSystemAccessTransferToken> token_remote;
  handle->Transfer(token_remote.InitWithNewPipeAndPassReceiver());

  // Deserializing tokens should re-request grants, with correct user action.
  EXPECT_CALL(
      permission_context_,
      GetReadPermissionGrant(
          kTestStorageKey.origin(), kTestPath, HandleType::kFile,
          FileSystemAccessPermissionContext::UserAction::kLoadFromStorage))
      .WillOnce(testing::Return(ask_grant_));
  EXPECT_CALL(
      permission_context_,
      GetWritePermissionGrant(
          kTestStorageKey.origin(), kTestPath, HandleType::kFile,
          FileSystemAccessPermissionContext::UserAction::kLoadFromStorage))
      .WillOnce(testing::Return(ask_grant2_));

  FileSystemAccessTransferTokenImpl* token =
      SerializeAndDeserializeToken(std::move(token_remote));
  ASSERT_TRUE(token);
  const storage::FileSystemURL& url = token->url();
  EXPECT_TRUE(url.origin().opaque());
  EXPECT_EQ(kTestPath, url.virtual_path());
  EXPECT_EQ(storage::kFileSystemTypeExternal, url.mount_type());
  EXPECT_EQ(HandleType::kFile, token->type());
  EXPECT_EQ(ask_grant_, token->GetReadGrant());
  EXPECT_EQ(ask_grant2_, token->GetWriteGrant());
}

// FileSystemAccessManager should successfully resolve a
// FileSystemAccessDataTransferToken representing a file in the user's file
// system into a valid Remote<blink::mojom::FileSystemAccessFileHandle>, given
// that the PID is valid.
TEST_F(FileSystemAccessManagerImplTest,
       GetEntryFromDataTransferToken_File_ValidPID) {
  // Create a file and write some text into it.
  const base::FilePath file_path = dir_.GetPath().AppendASCII("mr_file");
  const std::string file_contents = "Deleted code is debugged code.";
  ASSERT_TRUE(base::WriteFile(file_path, file_contents));

  GetEntryFromDataTransferTokenFileTest(
      file_path, FileSystemAccessEntryFactory::PathType::kLocal, file_contents);
}

// FileSystemAccessManager should successfully resolve a
// FileSystemAccessDataTransferToken representing a
// FileSystemAccessDirectoryEntry into a valid
// Remote<blink::mojom::FileSystemAccessDirectoryHandle>, given that the PID is
// valid.
TEST_F(FileSystemAccessManagerImplTest,
       GetEntryFromDataTransferToken_Directory_ValidPID) {
  // Create a directory and create a FileSystemAccessDataTransferToken
  // representing the new directory.
  const base::FilePath dir_path = dir_.GetPath().AppendASCII("mr_dir");
  ASSERT_TRUE(base::CreateDirectory(dir_path));
  const std::string child_file_name = "child-file-name.txt";
  ASSERT_TRUE(base::WriteFile(dir_path.AppendASCII(child_file_name), ""));

  GetEntryFromDataTransferTokenDirectoryTest(
      dir_path, FileSystemAccessEntryFactory::PathType::kLocal,
      child_file_name);
}

// FileSystemAccessManager should successfully resolve a
// FileSystemAccessDataTransferToken representing a file in the user's file
// system into a valid Remote<blink::mojom::FileSystemAccessFileHandle>, given
// that the PID is valid.
TEST_F(FileSystemAccessManagerImplTest,
       GetEntryFromDataTransferToken_File_ExternalPath) {
  // Create a file and write some text into it.
  const base::FilePath file_path = dir_.GetPath().AppendASCII("mr_file");
  const std::string file_contents = "Deleted code is debugged code.";
  ASSERT_TRUE(base::WriteFile(file_path, file_contents));

  const base::FilePath virtual_file_path =
      base::FilePath::FromUTF8Unsafe(kTestMountPoint)
          .Append(file_path.BaseName());

  GetEntryFromDataTransferTokenFileTest(
      virtual_file_path, FileSystemAccessEntryFactory::PathType::kExternal,
      file_contents);
}

// FileSystemAccessManager should successfully resolve a
// FileSystemAccessDataTransferToken representing a
// FileSystemAccessDirectoryEntry into a valid
// Remote<blink::mojom::FileSystemAccessDirectoryHandle>, given that the PID is
// valid.
TEST_F(FileSystemAccessManagerImplTest,
       GetEntryFromDataTransferToken_Directory_ExternalPath) {
  // Create a directory and create a FileSystemAccessDataTransferToken
  // representing the new directory.
  const base::FilePath dir_path = dir_.GetPath().AppendASCII("mr_dir");
  ASSERT_TRUE(base::CreateDirectory(dir_path));
  const std::string child_file_name = "child-file-name.txt";
  ASSERT_TRUE(base::WriteFile(dir_path.AppendASCII(child_file_name), ""));

  const base::FilePath virtual_dir_path =
      base::FilePath::FromUTF8Unsafe(kTestMountPoint)
          .Append(dir_path.BaseName());

  GetEntryFromDataTransferTokenDirectoryTest(
      virtual_dir_path, FileSystemAccessEntryFactory::PathType::kExternal,
      child_file_name);
}

// FileSystemAccessManager should refuse to resolve a
// FileSystemAccessDataTransferToken representing a file on the user's file
// system if the PID of the redeeming process doesn't match the one assigned at
// creation.
TEST_F(FileSystemAccessManagerImplTest,
       GetEntryFromDataTransferToken_File_InvalidPID) {
  base::FilePath file_path = dir_.GetPath().AppendASCII("mr_file");
  ASSERT_TRUE(base::CreateTemporaryFile(&file_path));

  // Create a FileSystemAccessDataTransferToken with a PID different than the
  // process attempting to redeem to the token.
  mojo::PendingRemote<blink::mojom::FileSystemAccessDataTransferToken>
      token_remote;
  manager_->CreateFileSystemAccessDataTransferToken(
      FileSystemAccessEntryFactory::PathType::kLocal, file_path,
      /*renderer_id=*/kBindingContext.process_id() - 1,
      token_remote.InitWithNewPipeAndPassReceiver());

  // Try to redeem the FileSystemAccessDataTransferToken for a
  // FileSystemAccessFileHandle, expecting `bad_message_observer` to intercept
  // a bad message callback.
  mojo::test::BadMessageObserver bad_message_observer;
  manager_remote_->GetEntryFromDataTransferToken(std::move(token_remote),
                                                 base::DoNothing());
  EXPECT_EQ("Invalid renderer ID.", bad_message_observer.WaitForBadMessage());
}

// FileSystemAccessManager should refuse to resolve a
// FileSystemAccessDataTransferToken representing a directory on the user's file
// system if the PID of the redeeming process doesn't match the one assigned at
// creation.
TEST_F(FileSystemAccessManagerImplTest,
       GetEntryFromDataTransferToken_Directory_InvalidPID) {
  const base::FilePath& kDirPath = dir_.GetPath().AppendASCII("mr_directory");
  ASSERT_TRUE(base::CreateDirectory(kDirPath));

  // Create a FileSystemAccessDataTransferToken with an PID different than the
  // process attempting to redeem to the token.
  mojo::PendingRemote<blink::mojom::FileSystemAccessDataTransferToken>
      token_remote;
  manager_->CreateFileSystemAccessDataTransferToken(
      FileSystemAccessEntryFactory::PathType::kLocal, kDirPath,
      /*renderer_id=*/kBindingContext.process_id() - 1,
      token_remote.InitWithNewPipeAndPassReceiver());

  // Try to redeem the FileSystemAccessDataTransferToken for a
  // FileSystemAccessFileHandle, expecting `bad_message_observer` to intercept
  // a bad message callback.
  mojo::test::BadMessageObserver bad_message_observer;
  manager_remote_->GetEntryFromDataTransferToken(std::move(token_remote),
                                                 base::DoNothing());
  EXPECT_EQ("Invalid renderer ID.", bad_message_observer.WaitForBadMessage());
}

// FileSystemAccessManager should refuse to resolve a
// FileSystemAccessDataTransferToken if the value of the token was not
// recognized by the FileSystemAccessManager.
TEST_F(FileSystemAccessManagerImplTest,
       GetEntryFromDataTransferToken_UnrecognizedToken) {
  const base::FilePath& kDirPath = dir_.GetPath().AppendASCII("mr_directory");
  ASSERT_TRUE(base::CreateDirectory(kDirPath));

  // Create a FileSystemAccessDataTransferToken without registering it to the
  // FileSystemAccessManager.
  mojo::PendingRemote<blink::mojom::FileSystemAccessDataTransferToken>
      token_remote;
  auto drag_drop_token_impl =
      std::make_unique<FileSystemAccessDataTransferTokenImpl>(
          manager_.get(), FileSystemAccessEntryFactory::PathType::kLocal,
          kDirPath, kBindingContext.process_id(),
          token_remote.InitWithNewPipeAndPassReceiver());

  // Try to redeem the FileSystemAccessDataTransferToken for a
  // FileSystemAccessFileHandle, expecting `bad_message_observer` to intercept
  // a bad message callback.
  mojo::test::BadMessageObserver bad_message_observer;
  manager_remote_->GetEntryFromDataTransferToken(std::move(token_remote),
                                                 base::DoNothing());
  EXPECT_EQ("Unrecognized drag drop token.",
            bad_message_observer.WaitForBadMessage());
}

TEST_F(FileSystemAccessManagerImplTest, ChooseEntries_OpenFile) {
  base::FilePath test_file = dir_.GetPath().AppendASCII("foo");
  ASSERT_TRUE(base::CreateTemporaryFile(&test_file));

  FileSystemChooser::ResultEntry entry;
  entry.type = PathType::kLocal;
  entry.path = test_file;
  manager_->SetFilePickerResultForTesting(std::move(entry));

  static_cast<TestRenderFrameHost*>(web_contents_->GetMainFrame())
      ->SimulateUserActivation();

  mojo::Remote<blink::mojom::FileSystemAccessManager> manager_remote;
  FileSystemAccessManagerImpl::BindingContext binding_context = {
      kTestStorageKey, kTestURL, web_contents_->GetMainFrame()->GetGlobalId()};
  manager_->BindReceiver(binding_context,
                         manager_remote.BindNewPipeAndPassReceiver());

  EXPECT_CALL(permission_context_,
              CanObtainReadPermission(kTestStorageKey.origin()))
      .WillOnce(testing::Return(true));

  EXPECT_CALL(
      permission_context_,
      GetWellKnownDirectoryPath(blink::mojom::WellKnownDirectory::kDefault))
      .WillOnce(testing::Return(base::FilePath()));
  EXPECT_CALL(permission_context_,
              GetLastPickedDirectory(kTestStorageKey.origin(), std::string()))
      .WillOnce(testing::Return(PathInfo()));
  EXPECT_CALL(permission_context_,
              SetLastPickedDirectory(kTestStorageKey.origin(), std::string(),
                                     test_file.DirName(), PathType::kLocal));

  EXPECT_CALL(
      permission_context_,
      ConfirmSensitiveDirectoryAccess_(
          kTestStorageKey.origin(),
          FileSystemAccessPermissionContext::PathType::kLocal, test_file,
          FileSystemAccessPermissionContext::HandleType::kFile,
          web_contents_->GetMainFrame()->GetGlobalId(), testing::_))
      .WillOnce(RunOnceCallback<5>(FileSystemAccessPermissionContext::
                                       SensitiveDirectoryResult::kAllowed));

  EXPECT_CALL(permission_context_,
              GetReadPermissionGrant(
                  kTestStorageKey.origin(), test_file,
                  FileSystemAccessPermissionContext::HandleType::kFile,
                  FileSystemAccessPermissionContext::UserAction::kOpen))
      .WillOnce(testing::Return(allow_grant_));
  EXPECT_CALL(permission_context_,
              GetWritePermissionGrant(
                  kTestStorageKey.origin(), test_file,
                  FileSystemAccessPermissionContext::HandleType::kFile,
                  FileSystemAccessPermissionContext::UserAction::kOpen))
      .WillOnce(testing::Return(allow_grant_));

  auto open_file_picker_options = blink::mojom::OpenFilePickerOptions::New(
      blink::mojom::AcceptsTypesInfo::New(
          std::vector<blink::mojom::ChooseFileSystemEntryAcceptsOptionPtr>(),
          /*include_accepts_all=*/true),
      /*can_select_multiple_files=*/false);
  auto common_file_picker_options = blink::mojom::CommonFilePickerOptions::New(
      /*starting_directory_id=*/std::string(),
      blink::mojom::WellKnownDirectory::kDefault,
      /*starting_directory_token=*/mojo::NullRemote());

  base::RunLoop loop;
  manager_remote->ChooseEntries(
      blink::mojom::FilePickerOptions::NewOpenFilePickerOptions(
          std::move(open_file_picker_options)),
      std::move(common_file_picker_options),
      base::BindLambdaForTesting(
          [&](blink::mojom::FileSystemAccessErrorPtr result,
              std::vector<blink::mojom::FileSystemAccessEntryPtr> entries) {
            loop.Quit();
          }));
  loop.Run();
}

TEST_F(FileSystemAccessManagerImplTest, ChooseEntries_SaveFile) {
  base::FilePath test_file = dir_.GetPath().AppendASCII("foo");
  ASSERT_TRUE(base::CreateTemporaryFile(&test_file));

  FileSystemChooser::ResultEntry entry;
  entry.type = PathType::kLocal;
  entry.path = test_file;
  manager_->SetFilePickerResultForTesting(std::move(entry));

  static_cast<TestRenderFrameHost*>(web_contents_->GetMainFrame())
      ->SimulateUserActivation();

  mojo::Remote<blink::mojom::FileSystemAccessManager> manager_remote;
  FileSystemAccessManagerImpl::BindingContext binding_context = {
      kTestStorageKey, kTestURL, web_contents_->GetMainFrame()->GetGlobalId()};
  manager_->BindReceiver(binding_context,
                         manager_remote.BindNewPipeAndPassReceiver());

  EXPECT_CALL(permission_context_,
              CanObtainReadPermission(kTestStorageKey.origin()))
      .WillOnce(testing::Return(true));
  EXPECT_CALL(permission_context_,
              CanObtainWritePermission(kTestStorageKey.origin()))
      .WillOnce(testing::Return(true));

  EXPECT_CALL(
      permission_context_,
      GetWellKnownDirectoryPath(blink::mojom::WellKnownDirectory::kDefault))
      .WillOnce(testing::Return(base::FilePath()));
  EXPECT_CALL(permission_context_,
              GetLastPickedDirectory(kTestStorageKey.origin(), std::string()))
      .WillOnce(testing::Return(PathInfo()));
  EXPECT_CALL(permission_context_,
              SetLastPickedDirectory(kTestStorageKey.origin(), std::string(),
                                     test_file.DirName(), PathType::kLocal));

  EXPECT_CALL(
      permission_context_,
      ConfirmSensitiveDirectoryAccess_(
          kTestStorageKey.origin(),
          FileSystemAccessPermissionContext::PathType::kLocal, test_file,
          FileSystemAccessPermissionContext::HandleType::kFile,
          web_contents_->GetMainFrame()->GetGlobalId(), testing::_))
      .WillOnce(RunOnceCallback<5>(FileSystemAccessPermissionContext::
                                       SensitiveDirectoryResult::kAllowed));

  EXPECT_CALL(permission_context_,
              GetReadPermissionGrant(
                  kTestStorageKey.origin(), test_file,
                  FileSystemAccessPermissionContext::HandleType::kFile,
                  FileSystemAccessPermissionContext::UserAction::kSave))
      .WillOnce(testing::Return(allow_grant_));
  EXPECT_CALL(permission_context_,
              GetWritePermissionGrant(
                  kTestStorageKey.origin(), test_file,
                  FileSystemAccessPermissionContext::HandleType::kFile,
                  FileSystemAccessPermissionContext::UserAction::kSave))
      .WillOnce(testing::Return(allow_grant_));

  auto save_file_picker_options = blink::mojom::SaveFilePickerOptions::New(
      blink::mojom::AcceptsTypesInfo::New(
          std::vector<blink::mojom::ChooseFileSystemEntryAcceptsOptionPtr>(),
          /*include_accepts_all=*/true),
      /*suggested_name=*/std::string());
  auto common_file_picker_options = blink::mojom::CommonFilePickerOptions::New(
      /*starting_directory_id=*/std::string(),
      blink::mojom::WellKnownDirectory::kDefault,
      /*starting_directory_token=*/mojo::NullRemote());

  base::RunLoop loop;
  manager_remote->ChooseEntries(
      blink::mojom::FilePickerOptions::NewSaveFilePickerOptions(
          std::move(save_file_picker_options)),
      std::move(common_file_picker_options),
      base::BindLambdaForTesting(
          [&](blink::mojom::FileSystemAccessErrorPtr result,
              std::vector<blink::mojom::FileSystemAccessEntryPtr> entries) {
            loop.Quit();
          }));
  loop.Run();
}

TEST_F(FileSystemAccessManagerImplTest, ChooseEntries_OpenDirectory) {
  base::FilePath test_dir = dir_.GetPath();

  FileSystemChooser::ResultEntry entry;
  entry.type = PathType::kLocal;
  entry.path = test_dir;
  manager_->SetFilePickerResultForTesting(std::move(entry));

  static_cast<TestRenderFrameHost*>(web_contents_->GetMainFrame())
      ->SimulateUserActivation();

  mojo::Remote<blink::mojom::FileSystemAccessManager> manager_remote;
  FileSystemAccessManagerImpl::BindingContext binding_context = {
      kTestStorageKey, kTestURL, web_contents_->GetMainFrame()->GetGlobalId()};
  manager_->BindReceiver(binding_context,
                         manager_remote.BindNewPipeAndPassReceiver());

  EXPECT_CALL(permission_context_,
              CanObtainReadPermission(kTestStorageKey.origin()))
      .WillOnce(testing::Return(true));

  EXPECT_CALL(
      permission_context_,
      GetWellKnownDirectoryPath(blink::mojom::WellKnownDirectory::kDefault))
      .WillOnce(testing::Return(base::FilePath()));
  EXPECT_CALL(permission_context_,
              GetLastPickedDirectory(kTestStorageKey.origin(), std::string()))
      .WillOnce(testing::Return(PathInfo()));
  EXPECT_CALL(permission_context_,
              SetLastPickedDirectory(kTestStorageKey.origin(), std::string(),
                                     test_dir, PathType::kLocal));

  EXPECT_CALL(permission_context_,
              ConfirmSensitiveDirectoryAccess_(
                  kTestStorageKey.origin(),
                  FileSystemAccessPermissionContext::PathType::kLocal, test_dir,
                  FileSystemAccessPermissionContext::HandleType::kDirectory,
                  web_contents_->GetMainFrame()->GetGlobalId(), testing::_))
      .WillOnce(RunOnceCallback<5>(FileSystemAccessPermissionContext::
                                       SensitiveDirectoryResult::kAllowed));

  EXPECT_CALL(permission_context_,
              GetReadPermissionGrant(
                  kTestStorageKey.origin(), test_dir,
                  FileSystemAccessPermissionContext::HandleType::kDirectory,
                  FileSystemAccessPermissionContext::UserAction::kOpen))
      .WillOnce(testing::Return(allow_grant_));
  EXPECT_CALL(permission_context_,
              GetWritePermissionGrant(
                  kTestStorageKey.origin(), test_dir,
                  FileSystemAccessPermissionContext::HandleType::kDirectory,
                  FileSystemAccessPermissionContext::UserAction::kOpen))
      .WillOnce(testing::Return(allow_grant_));

  auto directory_picker_options = blink::mojom::DirectoryPickerOptions::New();
  auto common_file_picker_options = blink::mojom::CommonFilePickerOptions::New(
      /*starting_directory_id=*/std::string(),
      blink::mojom::WellKnownDirectory::kDefault,
      /*starting_directory_token=*/mojo::NullRemote());

  base::RunLoop loop;
  manager_remote->ChooseEntries(
      blink::mojom::FilePickerOptions::NewDirectoryPickerOptions(
          std::move(directory_picker_options)),
      std::move(common_file_picker_options),
      base::BindLambdaForTesting(
          [&](blink::mojom::FileSystemAccessErrorPtr result,
              std::vector<blink::mojom::FileSystemAccessEntryPtr> entries) {
            loop.Quit();
          }));
  loop.Run();
}

TEST_F(FileSystemAccessManagerImplTest, ChooseEntries_InvalidStartInID) {
  base::FilePath test_dir = dir_.GetPath();

  FileSystemChooser::ResultEntry entry;
  entry.type = PathType::kLocal;
  entry.path = test_dir;
  manager_->SetFilePickerResultForTesting(std::move(entry));

  static_cast<TestRenderFrameHost*>(web_contents_->GetMainFrame())
      ->SimulateUserActivation();

  mojo::Remote<blink::mojom::FileSystemAccessManager> manager_remote;
  FileSystemAccessManagerImpl::BindingContext binding_context = {
      kTestStorageKey, kTestURL, web_contents_->GetMainFrame()->GetGlobalId()};
  manager_->BindReceiver(binding_context,
                         manager_remote.BindNewPipeAndPassReceiver());

  // Specifying a `starting_directory_id` with invalid characters should trigger
  // a bad message callback.
  auto directory_picker_options = blink::mojom::DirectoryPickerOptions::New();
  auto common_file_picker_options = blink::mojom::CommonFilePickerOptions::New(
      /*starting_directory_id=*/"inv*l!d <hars",
      blink::mojom::WellKnownDirectory::kDefault,
      /*starting_directory_token=*/mojo::NullRemote());

  mojo::test::BadMessageObserver bad_message_observer;
  manager_remote->ChooseEntries(
      blink::mojom::FilePickerOptions::NewDirectoryPickerOptions(
          std::move(directory_picker_options)),
      std::move(common_file_picker_options), base::DoNothing());
  EXPECT_EQ("Invalid starting directory ID in browser",
            bad_message_observer.WaitForBadMessage());
}

}  // namespace content
