// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/file_system_access/file_system_access_manager_impl.h"

#include <string>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/unguessable_token.h"
#include "components/services/storage/public/cpp/buckets/bucket_id.h"
#include "components/services/storage/public/cpp/buckets/bucket_locator.h"
#include "components/services/storage/public/cpp/buckets/constants.h"
#include "content/browser/file_system_access/features.h"
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
#include "storage/common/file_system/file_system_types.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/blob/blob.mojom.h"
#include "third_party/blink/public/mojom/blob/serialized_blob.mojom.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_data_transfer_token.mojom.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_manager.mojom-forward.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_manager.mojom-shared.h"
#include "ui/shell_dialogs/select_file_dialog.h"
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

  void OnDataAvailable(base::span<const uint8_t> data) override {
    data_out_->append(base::as_string_view(data));
  }

  void OnDataComplete() override { std::move(done_callback_).Run(); }

 private:
  raw_ptr<std::string> data_out_ = nullptr;
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
  base::test::TestFuture<blink::mojom::FileSystemAccessErrorPtr,
                         base::File::Info, blink::mojom::SerializedBlobPtr>
      future;
  file_remote->AsBlob(future.GetCallback<blink::mojom::FileSystemAccessErrorPtr,
                                         const base::File::Info&,
                                         blink::mojom::SerializedBlobPtr>());
  EXPECT_EQ(future.Get<0>()->status, blink::mojom::FileSystemAccessStatus::kOk);
  blink::mojom::SerializedBlobPtr received_blob = std::get<2>(future.Take());
  EXPECT_FALSE(received_blob.is_null());

  mojo::Remote<blink::mojom::Blob> blob;
  blob.Bind(std::move(received_blob->blob));

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
        base::SingleThreadTaskRunner::GetCurrentDefault(),
        special_storage_policy_);
    quota_manager_proxy_ = base::MakeRefCounted<storage::MockQuotaManagerProxy>(
        quota_manager_.get(),
        base::SingleThreadTaskRunner::GetCurrentDefault().get());

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

    EXPECT_CALL(permission_context_, IsFileTypeDangerous_)
        .WillRepeatedly(testing::Return(false));
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
    base::test::TestFuture<PermissionStatus> future;
    handle->GetPermissionStatus(writable, future.GetCallback());
    return future.Get();
  }

  mojo::Remote<blink::mojom::FileSystemAccessDirectoryHandle>
  GetHandleForDirectory(const PathInfo& path_info) {
    auto grant = base::MakeRefCounted<FixedFileSystemAccessPermissionGrant>(
        FixedFileSystemAccessPermissionGrant::PermissionStatus::GRANTED,
        path_info);

    EXPECT_CALL(permission_context_,
                GetReadPermissionGrant(
                    kTestStorageKey.origin(), path_info, HandleType::kDirectory,
                    FileSystemAccessPermissionContext::UserAction::kOpen))
        .WillOnce(testing::Return(grant));
    EXPECT_CALL(permission_context_,
                GetWritePermissionGrant(
                    kTestStorageKey.origin(), path_info, HandleType::kDirectory,
                    FileSystemAccessPermissionContext::UserAction::kOpen))
        .WillOnce(testing::Return(grant));

    blink::mojom::FileSystemAccessEntryPtr entry =
        manager_->CreateDirectoryEntryFromPath(
            kBindingContext, path_info,
            FileSystemAccessPermissionContext::UserAction::kOpen);
    return mojo::Remote<blink::mojom::FileSystemAccessDirectoryHandle>(
        std::move(entry->entry_handle->get_directory()));
  }

  FileSystemAccessTransferTokenImpl* SerializeAndDeserializeToken(
      mojo::PendingRemote<blink::mojom::FileSystemAccessTransferToken>
          token_remote) {
    base::test::TestFuture<std::vector<uint8_t>> serialize_future;
    manager_->SerializeHandle(
        std::move(token_remote),
        serialize_future.GetCallback<const std::vector<uint8_t>&>());
    std::vector<uint8_t> serialized = serialize_future.Take();
    EXPECT_FALSE(serialized.empty());

    manager_->DeserializeHandle(kTestStorageKey, serialized,
                                token_remote.InitWithNewPipeAndPassReceiver());
    base::test::TestFuture<FileSystemAccessTransferTokenImpl*> resolve_future;
    manager_->ResolveTransferToken(std::move(token_remote),
                                   resolve_future.GetCallback());
    return resolve_future.Get();
  }

  void GetEntryFromDataTransferTokenFileTest(
      const PathInfo& file_path_info,
      const std::string& expected_file_contents) {
    // Create a token representing a dropped file at `file_path`.
    mojo::PendingRemote<blink::mojom::FileSystemAccessDataTransferToken>
        token_remote;
    manager_->CreateFileSystemAccessDataTransferToken(
        file_path_info, kBindingContext.process_id(),
        token_remote.InitWithNewPipeAndPassReceiver());

    // Expect permission requests when the token is sent to be redeemed.
    EXPECT_CALL(
        permission_context_,
        GetReadPermissionGrant(
            kTestStorageKey.origin(), file_path_info, HandleType::kFile,
            FileSystemAccessPermissionContext::UserAction::kDragAndDrop))
        .WillOnce(testing::Return(allow_grant_));

    EXPECT_CALL(
        permission_context_,
        GetWritePermissionGrant(
            kTestStorageKey.origin(), file_path_info, HandleType::kFile,
            FileSystemAccessPermissionContext::UserAction::kDragAndDrop))
        .WillOnce(testing::Return(allow_grant_));

    // Attempt to resolve `token_remote` and store the resulting
    // FileSystemAccessFileHandle in `file_remote`.
    base::test::TestFuture<blink::mojom::FileSystemAccessErrorPtr,
                           blink::mojom::FileSystemAccessEntryPtr>
        future;
    manager_remote_->GetEntryFromDataTransferToken(std::move(token_remote),
                                                   future.GetCallback());
    DCHECK_EQ(future.Get<0>()->status,
              blink::mojom::FileSystemAccessStatus::kOk);
    auto file_system_access_entry = std::get<1>(future.Take());

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
      const PathInfo& dir_path_info,
      const std::string& expected_child_dir_name) {
    mojo::PendingRemote<blink::mojom::FileSystemAccessDataTransferToken>
        token_remote;
    manager_->CreateFileSystemAccessDataTransferToken(
        dir_path_info, kBindingContext.process_id(),
        token_remote.InitWithNewPipeAndPassReceiver());

    if (base::FeatureList::IsEnabled(
            features::kFileSystemAccessDragAndDropCheckBlocklist)) {
      EXPECT_CALL(
          permission_context_,
          ConfirmSensitiveEntryAccess_(
              kTestStorageKey.origin(), dir_path_info,
              FileSystemAccessPermissionContext::HandleType::kDirectory,
              FileSystemAccessPermissionContext::UserAction::kDragAndDrop,
              kFrameId, testing::_))
          .WillOnce(RunOnceCallback<5>(FileSystemAccessPermissionContext::
                                           SensitiveEntryResult::kAllowed));
    }

    // Expect permission requests when the token is sent to be redeemed.
    EXPECT_CALL(
        permission_context_,
        GetReadPermissionGrant(
            kTestStorageKey.origin(), dir_path_info, HandleType::kDirectory,
            FileSystemAccessPermissionContext::UserAction::kDragAndDrop))
        .WillOnce(testing::Return(allow_grant_));

    EXPECT_CALL(
        permission_context_,
        GetWritePermissionGrant(
            kTestStorageKey.origin(), dir_path_info, HandleType::kDirectory,
            FileSystemAccessPermissionContext::UserAction::kDragAndDrop))
        .WillOnce(testing::Return(allow_grant_));

    // Attempt to resolve `token_remote` and store the resulting
    // FileSystemAccessDirectoryHandle in `dir_remote`.
    base::test::TestFuture<blink::mojom::FileSystemAccessErrorPtr,
                           blink::mojom::FileSystemAccessEntryPtr>
        get_entry_future;
    manager_remote_->GetEntryFromDataTransferToken(
        std::move(token_remote), get_entry_future.GetCallback());
    DCHECK_EQ(get_entry_future.Get<0>()->status,
              blink::mojom::FileSystemAccessStatus::kOk);
    auto file_system_access_entry = std::get<1>(get_entry_future.Take());

    ASSERT_FALSE(file_system_access_entry.is_null());
    ASSERT_TRUE(file_system_access_entry->entry_handle->is_directory());
    mojo::Remote<blink::mojom::FileSystemAccessDirectoryHandle> dir_remote(
        std::move(file_system_access_entry->entry_handle->get_directory()));

    // Use `dir_remote` to verify that dir_path contains a child called
    // expected_child_dir_name.
    base::test::TestFuture<
        blink::mojom::FileSystemAccessErrorPtr,
        mojo::PendingRemote<blink::mojom::FileSystemAccessDirectoryHandle>>
        get_directory_future;
    dir_remote->GetDirectory(expected_child_dir_name, /*create=*/false,
                             get_directory_future.GetCallback());
    ASSERT_EQ(get_directory_future.Get<0>()->status,
              blink::mojom::FileSystemAccessStatus::kOk);
  }

  storage::QuotaErrorOr<storage::BucketLocator> CreateBucketForTesting() {
    base::test::TestFuture<storage::QuotaErrorOr<storage::BucketInfo>>
        bucket_future;
    quota_manager_proxy_->CreateBucketForTesting(
        kTestStorageKey, "custom_bucket", blink::mojom::StorageType::kTemporary,
        base::SequencedTaskRunner::GetCurrentDefault(),
        bucket_future.GetCallback());
    return bucket_future.Take().transform(
        &storage::BucketInfo::ToBucketLocator);
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

  raw_ptr<WebContents> web_contents_ = nullptr;

  testing::StrictMock<MockFileSystemAccessPermissionContext>
      permission_context_;
  scoped_refptr<FileSystemAccessManagerImpl> manager_;
  mojo::Remote<blink::mojom::FileSystemAccessManager> manager_remote_;

  scoped_refptr<FixedFileSystemAccessPermissionGrant> ask_grant_ =
      base::MakeRefCounted<FixedFileSystemAccessPermissionGrant>(
          FixedFileSystemAccessPermissionGrant::PermissionStatus::ASK,
          PathInfo());
  scoped_refptr<FixedFileSystemAccessPermissionGrant> ask_grant2_ =
      base::MakeRefCounted<FixedFileSystemAccessPermissionGrant>(
          FixedFileSystemAccessPermissionGrant::PermissionStatus::ASK,
          PathInfo());
  scoped_refptr<FixedFileSystemAccessPermissionGrant> allow_grant_ =
      base::MakeRefCounted<FixedFileSystemAccessPermissionGrant>(
          FixedFileSystemAccessPermissionGrant::PermissionStatus::GRANTED,
          PathInfo());
};

TEST_F(FileSystemAccessManagerImplTest, GetSandboxedFileSystem_CreateBucket) {
  // Check default bucket exists.
  EXPECT_THAT(CreateSandboxFileSystemAndGetDefaultBucket(),
              base::test::ValueIs(
                  ::testing::Field(&storage::BucketLocator::is_default, true)));
}

TEST_F(FileSystemAccessManagerImplTest, GetSandboxedFileSystem_CustomBucket) {
  mojo::PendingRemote<blink::mojom::FileSystemAccessDirectoryHandle>
      directory_remote;
  FileSystemAccessManagerImpl::BindingContext binding_context = {
      kTestStorageKey, kTestURL,
      web_contents_->GetPrimaryMainFrame()->GetGlobalId()};
  base::test::TestFuture<storage::QuotaErrorOr<storage::BucketInfo>>
      bucket_future;
  quota_manager_proxy_->CreateBucketForTesting(
      kTestStorageKey, "custom_bucket", blink::mojom::StorageType::kTemporary,
      base::SequencedTaskRunner::GetCurrentDefault(),
      bucket_future.GetCallback());
  ASSERT_OK_AND_ASSIGN(auto bucket, bucket_future.Take());

  base::test::TestFuture<
      blink::mojom::FileSystemAccessErrorPtr,
      mojo::PendingRemote<blink::mojom::FileSystemAccessDirectoryHandle>>
      handle_future;
  manager_->GetSandboxedFileSystem(binding_context, bucket.ToBucketLocator(),
                                   /*directory_path_components=*/{},
                                   handle_future.GetCallback());
  EXPECT_EQ(handle_future.Get<0>()->status,
            blink::mojom::FileSystemAccessStatus::kOk);

  mojo::Remote<blink::mojom::FileSystemAccessDirectoryHandle> root(
      std::move(std::get<1>(handle_future.Take())));
  // Note: we can test that the open succeeded, but because the FileSystemURL
  // is not exposed to the callback we rely on WPTs to ensure the bucket
  // locator was actually used.
  // TODO(crbug.com/40224463): Ensure the bucket override is actually used.
  ASSERT_TRUE(root);

  base::test::TestFuture<
      blink::mojom::FileSystemAccessErrorPtr,
      mojo::PendingRemote<blink::mojom::FileSystemAccessDirectoryHandle>>
      nested_handle_future;

  // Create a directory that we will then retrieve directly from the
  // `GetSandboxedFileSystem` method.
  root->GetDirectory("test_directory", true,
                     nested_handle_future.GetCallback());

  EXPECT_EQ(nested_handle_future.Get<0>()->status,
            blink::mojom::FileSystemAccessStatus::kOk);

  base::test::TestFuture<
      blink::mojom::FileSystemAccessErrorPtr,
      mojo::PendingRemote<blink::mojom::FileSystemAccessDirectoryHandle>>
      nested_directory_handle_future;
  // Retrieve the directory from the `GetSandboxedFileSystem` method.
  manager_->GetSandboxedFileSystem(
      binding_context, bucket.ToBucketLocator(), {"test_directory"},
      nested_directory_handle_future.GetCallback());

  EXPECT_EQ(nested_directory_handle_future.Get<0>()->status,
            blink::mojom::FileSystemAccessStatus::kOk);
}

TEST_F(FileSystemAccessManagerImplTest,
       GetSandboxedFileSystem_CustomBucketInvalidPath) {
  mojo::PendingRemote<blink::mojom::FileSystemAccessDirectoryHandle>
      directory_remote;
  FileSystemAccessManagerImpl::BindingContext binding_context = {
      kTestStorageKey, kTestURL,
      web_contents_->GetPrimaryMainFrame()->GetGlobalId()};
  base::test::TestFuture<storage::QuotaErrorOr<storage::BucketInfo>>
      bucket_future;
  quota_manager_proxy_->CreateBucketForTesting(
      kTestStorageKey, "custom_bucket", blink::mojom::StorageType::kTemporary,
      base::SequencedTaskRunner::GetCurrentDefault(),
      bucket_future.GetCallback());
  ASSERT_OK_AND_ASSIGN(auto bucket, bucket_future.Take());

  base::test::TestFuture<
      blink::mojom::FileSystemAccessErrorPtr,
      mojo::PendingRemote<blink::mojom::FileSystemAccessDirectoryHandle>>
      handle_future;
  manager_->GetSandboxedFileSystem(
      binding_context, bucket.ToBucketLocator(),
      /*directory_path_components=*/{"invalid_path"},
      handle_future.GetCallback());
  EXPECT_EQ(handle_future.Get<0>()->status,
            blink::mojom::FileSystemAccessStatus::kFileError);
}

TEST_F(FileSystemAccessManagerImplTest, GetSandboxedFileSystem_BadBucket) {
  mojo::PendingRemote<blink::mojom::FileSystemAccessDirectoryHandle>
      directory_remote;
  FileSystemAccessManagerImpl::BindingContext binding_context = {
      kTestStorageKey, kTestURL,
      web_contents_->GetPrimaryMainFrame()->GetGlobalId()};
  const auto bucket = storage::BucketLocator(
      storage::BucketId(12), kTestStorageKey,
      blink::mojom::StorageType::kUnknown, /*is_default=*/false);

  base::test::TestFuture<
      blink::mojom::FileSystemAccessErrorPtr,
      mojo::PendingRemote<blink::mojom::FileSystemAccessDirectoryHandle>>
      handle_future;
  manager_->GetSandboxedFileSystem(binding_context, bucket,
                                   /*directory_path_components=*/{},
                                   handle_future.GetCallback());
  EXPECT_EQ(blink::mojom::FileSystemAccessStatus::kFileError,
            handle_future.Get<0>()->status);
  mojo::Remote<blink::mojom::FileSystemAccessDirectoryHandle> root(
      std::move(std::get<1>(handle_future.Take())));
  EXPECT_FALSE(root);
}

TEST_F(FileSystemAccessManagerImplTest, GetSandboxedFileSystem_Permissions) {
  base::test::TestFuture<
      blink::mojom::FileSystemAccessErrorPtr,
      mojo::PendingRemote<blink::mojom::FileSystemAccessDirectoryHandle>>
      future;
  manager_remote_->GetSandboxedFileSystem(future.GetCallback());
  blink::mojom::FileSystemAccessErrorPtr result;
  mojo::PendingRemote<blink::mojom::FileSystemAccessDirectoryHandle>
      directory_remote;
  std::tie(result, directory_remote) = future.Take();
  EXPECT_EQ(result->status, blink::mojom::FileSystemAccessStatus::kOk);
  mojo::Remote<blink::mojom::FileSystemAccessDirectoryHandle> root(
      std::move(directory_remote));
  ASSERT_TRUE(root);
  EXPECT_EQ(PermissionStatus::GRANTED,
            GetPermissionStatusSync(/*writable=*/false, root.get()));
  EXPECT_EQ(PermissionStatus::GRANTED,
            GetPermissionStatusSync(/*writable=*/true, root.get()));
}

TEST_F(FileSystemAccessManagerImplTest, CreateFileEntryFromPath_Permissions) {
  const PathInfo kTestPathInfo(dir_.GetPath().AppendASCII("foo"));

  EXPECT_CALL(permission_context_,
              GetReadPermissionGrant(
                  kTestStorageKey.origin(), kTestPathInfo, HandleType::kFile,
                  FileSystemAccessPermissionContext::UserAction::kOpen))
      .WillOnce(testing::Return(allow_grant_));
  EXPECT_CALL(permission_context_,
              GetWritePermissionGrant(
                  kTestStorageKey.origin(), kTestPathInfo, HandleType::kFile,
                  FileSystemAccessPermissionContext::UserAction::kOpen))
      .WillOnce(testing::Return(ask_grant_));

  blink::mojom::FileSystemAccessEntryPtr entry =
      manager_->CreateFileEntryFromPath(
          kBindingContext, kTestPathInfo,
          FileSystemAccessPermissionContext::UserAction::kOpen);
  mojo::Remote<blink::mojom::FileSystemAccessFileHandle> handle(
      std::move(entry->entry_handle->get_file()));

  EXPECT_EQ(PermissionStatus::GRANTED,
            GetPermissionStatusSync(/*writable=*/false, handle.get()));
  EXPECT_EQ(PermissionStatus::ASK,
            GetPermissionStatusSync(/*writable=*/true, handle.get()));
}

TEST_F(FileSystemAccessManagerImplTest,
       CreateWritableFileEntryFromPath_Permissions) {
  const PathInfo kTestPathInfo(dir_.GetPath().AppendASCII("foo"));

  EXPECT_CALL(permission_context_,
              GetReadPermissionGrant(
                  kTestStorageKey.origin(), kTestPathInfo, HandleType::kFile,
                  FileSystemAccessPermissionContext::UserAction::kSave))
      .WillOnce(testing::Return(allow_grant_));
  EXPECT_CALL(permission_context_,
              GetWritePermissionGrant(
                  kTestStorageKey.origin(), kTestPathInfo, HandleType::kFile,
                  FileSystemAccessPermissionContext::UserAction::kSave))
      .WillOnce(testing::Return(allow_grant_));

  blink::mojom::FileSystemAccessEntryPtr entry =
      manager_->CreateFileEntryFromPath(
          kBindingContext, kTestPathInfo,
          FileSystemAccessPermissionContext::UserAction::kSave);
  mojo::Remote<blink::mojom::FileSystemAccessFileHandle> handle(
      std::move(entry->entry_handle->get_file()));

  EXPECT_EQ(PermissionStatus::GRANTED,
            GetPermissionStatusSync(/*writable=*/false, handle.get()));
  EXPECT_EQ(PermissionStatus::GRANTED,
            GetPermissionStatusSync(/*writable=*/true, handle.get()));
}

TEST_F(FileSystemAccessManagerImplTest,
       CreateDirectoryEntryFromPath_Permissions) {
  const content::PathInfo kTestPathInfo(dir_.GetPath().AppendASCII("foo"));

  EXPECT_CALL(
      permission_context_,
      GetReadPermissionGrant(
          kTestStorageKey.origin(), kTestPathInfo, HandleType::kDirectory,
          FileSystemAccessPermissionContext::UserAction::kOpen))
      .WillOnce(testing::Return(allow_grant_));
  EXPECT_CALL(
      permission_context_,
      GetWritePermissionGrant(
          kTestStorageKey.origin(), kTestPathInfo, HandleType::kDirectory,
          FileSystemAccessPermissionContext::UserAction::kOpen))
      .WillOnce(testing::Return(ask_grant_));

  blink::mojom::FileSystemAccessEntryPtr entry =
      manager_->CreateDirectoryEntryFromPath(
          kBindingContext, kTestPathInfo,
          FileSystemAccessPermissionContext::UserAction::kOpen);
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

  auto lock = TakeLockSync(kBindingContext, test_file_url,
                           manager_->GetWFSSiloedLockType());
  ASSERT_TRUE(lock);
  auto swap_lock = TakeLockSync(kBindingContext, test_swap_url,
                                manager_->GetExclusiveLockType());
  ASSERT_TRUE(swap_lock);

  mojo::Remote<blink::mojom::FileSystemAccessFileWriter> writer_remote(
      manager_->CreateFileWriter(kBindingContext, test_file_url, test_swap_url,
                                 std::move(lock), std::move(swap_lock),
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
                file_system_context_.get(), test_swap_url, "foo"));

  auto lock = TakeLockSync(kBindingContext, test_file_url,
                           manager_->GetWFSSiloedLockType());
  ASSERT_TRUE(lock);
  auto swap_lock = TakeLockSync(kBindingContext, test_swap_url,
                                manager_->GetExclusiveLockType());
  ASSERT_TRUE(swap_lock);

  mojo::Remote<blink::mojom::FileSystemAccessFileWriter> writer_remote(
      manager_->CreateFileWriter(kBindingContext, test_file_url, test_swap_url,
                                 std::move(lock), std::move(swap_lock),
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
                file_system_context_.get(), test_swap_url, "foo"));

  auto lock = TakeLockSync(kBindingContext, test_file_url,
                           manager_->GetWFSSiloedLockType());
  ASSERT_TRUE(lock);
  auto swap_lock = TakeLockSync(kBindingContext, test_swap_url,
                                manager_->GetExclusiveLockType());
  ASSERT_TRUE(swap_lock);

  mojo::Remote<blink::mojom::FileSystemAccessFileWriter> writer_remote(
      manager_->CreateFileWriter(kBindingContext, test_file_url, test_swap_url,
                                 std::move(lock), std::move(swap_lock),
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
                file_system_context_.get(), test_swap_url, "foo"));

  auto lock = TakeLockSync(kBindingContext, test_file_url,
                           manager_->GetWFSSiloedLockType());
  ASSERT_TRUE(lock);
  auto swap_lock = TakeLockSync(kBindingContext, test_swap_url,
                                manager_->GetExclusiveLockType());
  ASSERT_TRUE(swap_lock);

  mojo::Remote<blink::mojom::FileSystemAccessFileWriter> writer_remote(
      manager_->CreateFileWriter(kBindingContext, test_file_url, test_swap_url,
                                 std::move(lock), std::move(swap_lock),
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

TEST_F(FileSystemAccessManagerImplTest,
       SerializeHandle_SandboxedFile_DefaultBucket) {
  ASSERT_OK_AND_ASSIGN(auto default_bucket,
                       CreateSandboxFileSystemAndGetDefaultBucket());
  auto test_file_url = file_system_context_->CreateCrackedFileSystemURL(
      kTestStorageKey, storage::kFileSystemTypeTemporary,
      base::FilePath::FromUTF8Unsafe("test/foo/bar"));
  test_file_url.SetBucket(default_bucket);
  FileSystemAccessFileHandleImpl file(manager_.get(), kBindingContext,
                                      test_file_url, {ask_grant_, ask_grant_});
  mojo::PendingRemote<blink::mojom::FileSystemAccessTransferToken> token_remote;
  manager_->CreateTransferToken(file,
                                token_remote.InitWithNewPipeAndPassReceiver());

  FileSystemAccessTransferTokenImpl* token =
      SerializeAndDeserializeToken(std::move(token_remote));
  ASSERT_TRUE(token);
  ASSERT_TRUE(token->url().bucket().has_value());
  EXPECT_EQ(test_file_url, token->url());
  EXPECT_EQ(HandleType::kFile, token->type());

  // Deserialized sandboxed filesystem handles should always be readable and
  // writable.
  ASSERT_TRUE(token->GetReadGrant());
  EXPECT_EQ(PermissionStatus::GRANTED, token->GetReadGrant()->GetStatus());
  ASSERT_TRUE(token->GetWriteGrant());
  EXPECT_EQ(PermissionStatus::GRANTED, token->GetWriteGrant()->GetStatus());
}

TEST_F(FileSystemAccessManagerImplTest,
       SerializeHandle_SandboxedFile_CustomBucket) {
  auto test_file_url = file_system_context_->CreateCrackedFileSystemURL(
      kTestStorageKey, storage::kFileSystemTypeTemporary,
      base::FilePath::FromUTF8Unsafe("test/foo/bar"));
  ASSERT_OK_AND_ASSIGN(auto bucket, CreateBucketForTesting());
  test_file_url.SetBucket(std::move(bucket));
  FileSystemAccessFileHandleImpl file(manager_.get(), kBindingContext,
                                      test_file_url, {ask_grant_, ask_grant_});
  mojo::PendingRemote<blink::mojom::FileSystemAccessTransferToken> token_remote;
  manager_->CreateTransferToken(file,
                                token_remote.InitWithNewPipeAndPassReceiver());

  FileSystemAccessTransferTokenImpl* token =
      SerializeAndDeserializeToken(std::move(token_remote));
  ASSERT_TRUE(token);
  ASSERT_TRUE(token->url().bucket().has_value());
  EXPECT_EQ(test_file_url, token->url());
  EXPECT_EQ(HandleType::kFile, token->type());

  // Deserialized sandboxed filesystem handles should always be readable and
  // writable.
  ASSERT_TRUE(token->GetReadGrant());
  EXPECT_EQ(PermissionStatus::GRANTED, token->GetReadGrant()->GetStatus());
  ASSERT_TRUE(token->GetWriteGrant());
  EXPECT_EQ(PermissionStatus::GRANTED, token->GetWriteGrant()->GetStatus());
}

TEST_F(FileSystemAccessManagerImplTest,
       SerializeHandle_SandboxedDirectory_DefaultBucket) {
  ASSERT_OK_AND_ASSIGN(auto default_bucket,
                       CreateSandboxFileSystemAndGetDefaultBucket());
  auto test_file_url = file_system_context_->CreateCrackedFileSystemURL(
      kTestStorageKey, storage::kFileSystemTypeTemporary,
      base::FilePath::FromUTF8Unsafe("hello/world/"));
  test_file_url.SetBucket(default_bucket);
  FileSystemAccessDirectoryHandleImpl directory(
      manager_.get(), kBindingContext, test_file_url, {ask_grant_, ask_grant_});
  mojo::PendingRemote<blink::mojom::FileSystemAccessTransferToken> token_remote;
  manager_->CreateTransferToken(directory,
                                token_remote.InitWithNewPipeAndPassReceiver());

  FileSystemAccessTransferTokenImpl* token =
      SerializeAndDeserializeToken(std::move(token_remote));
  ASSERT_TRUE(token);
  ASSERT_TRUE(token->url().bucket().has_value());
  EXPECT_EQ(test_file_url, token->url());
  EXPECT_EQ(HandleType::kDirectory, token->type());

  // Deserialized sandboxed filesystem handles should always be readable and
  // writable.
  ASSERT_TRUE(token->GetReadGrant());
  EXPECT_EQ(PermissionStatus::GRANTED, token->GetReadGrant()->GetStatus());
  ASSERT_TRUE(token->GetWriteGrant());
  EXPECT_EQ(PermissionStatus::GRANTED, token->GetWriteGrant()->GetStatus());
}

TEST_F(FileSystemAccessManagerImplTest,
       SerializeHandle_SandboxedDirectory_CustomBucket) {
  auto test_file_url = file_system_context_->CreateCrackedFileSystemURL(
      kTestStorageKey, storage::kFileSystemTypeTemporary,
      base::FilePath::FromUTF8Unsafe("hello/world/"));
  ASSERT_OK_AND_ASSIGN(auto bucket, CreateBucketForTesting());
  test_file_url.SetBucket(std::move(bucket));
  FileSystemAccessDirectoryHandleImpl directory(
      manager_.get(), kBindingContext, test_file_url, {ask_grant_, ask_grant_});
  mojo::PendingRemote<blink::mojom::FileSystemAccessTransferToken> token_remote;
  manager_->CreateTransferToken(directory,
                                token_remote.InitWithNewPipeAndPassReceiver());

  FileSystemAccessTransferTokenImpl* token =
      SerializeAndDeserializeToken(std::move(token_remote));
  ASSERT_TRUE(token);
  ASSERT_TRUE(token->url().bucket().has_value());
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
  const PathInfo kTestPathInfo(dir_.GetPath().AppendASCII("foo"));

  auto grant = base::MakeRefCounted<FixedFileSystemAccessPermissionGrant>(
      FixedFileSystemAccessPermissionGrant::PermissionStatus::GRANTED,
      kTestPathInfo);

  // Expect calls to get grants when creating the initial handle.
  EXPECT_CALL(permission_context_,
              GetReadPermissionGrant(
                  kTestStorageKey.origin(), kTestPathInfo, HandleType::kFile,
                  FileSystemAccessPermissionContext::UserAction::kOpen))
      .WillOnce(testing::Return(grant));
  EXPECT_CALL(permission_context_,
              GetWritePermissionGrant(
                  kTestStorageKey.origin(), kTestPathInfo, HandleType::kFile,
                  FileSystemAccessPermissionContext::UserAction::kOpen))
      .WillOnce(testing::Return(grant));

  blink::mojom::FileSystemAccessEntryPtr entry =
      manager_->CreateFileEntryFromPath(
          kBindingContext, kTestPathInfo,
          FileSystemAccessPermissionContext::UserAction::kOpen);
  mojo::Remote<blink::mojom::FileSystemAccessFileHandle> handle(
      std::move(entry->entry_handle->get_file()));

  mojo::PendingRemote<blink::mojom::FileSystemAccessTransferToken> token_remote;
  handle->Transfer(token_remote.InitWithNewPipeAndPassReceiver());

  // Deserializing tokens should re-request grants, with correct user action.
  EXPECT_CALL(
      permission_context_,
      GetReadPermissionGrant(
          kTestStorageKey.origin(), kTestPathInfo, HandleType::kFile,
          FileSystemAccessPermissionContext::UserAction::kLoadFromStorage))
      .WillOnce(testing::Return(ask_grant_));
  EXPECT_CALL(
      permission_context_,
      GetWritePermissionGrant(
          kTestStorageKey.origin(), kTestPathInfo, HandleType::kFile,
          FileSystemAccessPermissionContext::UserAction::kLoadFromStorage))
      .WillOnce(testing::Return(ask_grant2_));

  FileSystemAccessTransferTokenImpl* token =
      SerializeAndDeserializeToken(std::move(token_remote));
  ASSERT_TRUE(token);
  const storage::FileSystemURL& url = token->url();
  EXPECT_TRUE(url.origin().opaque());
  EXPECT_EQ(kTestPathInfo.path, url.path());
  EXPECT_EQ(storage::kFileSystemTypeLocal, url.type());
  EXPECT_EQ(storage::kFileSystemTypeLocal, url.mount_type());
  EXPECT_EQ(HandleType::kFile, token->type());
  EXPECT_EQ(ask_grant_, token->GetReadGrant());
  EXPECT_EQ(ask_grant2_, token->GetWriteGrant());
}

TEST_F(FileSystemAccessManagerImplTest,
       SerializeHandle_Native_SingleDirectory) {
  const PathInfo kTestPathInfo(dir_.GetPath().AppendASCII("foobar"));
  mojo::Remote<blink::mojom::FileSystemAccessDirectoryHandle> handle =
      GetHandleForDirectory(kTestPathInfo);

  mojo::PendingRemote<blink::mojom::FileSystemAccessTransferToken> token_remote;
  handle->Transfer(token_remote.InitWithNewPipeAndPassReceiver());

  // Deserializing tokens should re-request grants, with correct user action.
  EXPECT_CALL(
      permission_context_,
      GetReadPermissionGrant(
          kTestStorageKey.origin(), kTestPathInfo, HandleType::kDirectory,
          FileSystemAccessPermissionContext::UserAction::kLoadFromStorage))
      .WillOnce(testing::Return(ask_grant_));
  EXPECT_CALL(
      permission_context_,
      GetWritePermissionGrant(
          kTestStorageKey.origin(), kTestPathInfo, HandleType::kDirectory,
          FileSystemAccessPermissionContext::UserAction::kLoadFromStorage))
      .WillOnce(testing::Return(ask_grant2_));

  FileSystemAccessTransferTokenImpl* token =
      SerializeAndDeserializeToken(std::move(token_remote));
  ASSERT_TRUE(token);
  const storage::FileSystemURL& url = token->url();
  EXPECT_TRUE(url.origin().opaque());
  EXPECT_EQ(kTestPathInfo.path, url.path());
  EXPECT_EQ(storage::kFileSystemTypeLocal, url.type());
  EXPECT_EQ(storage::kFileSystemTypeLocal, url.mount_type());
  EXPECT_EQ(HandleType::kDirectory, token->type());
  EXPECT_EQ(ask_grant_, token->GetReadGrant());
  EXPECT_EQ(ask_grant2_, token->GetWriteGrant());
}

TEST_F(FileSystemAccessManagerImplTest,
       SerializeHandle_Native_FileInsideDirectory) {
  const PathInfo kDirectoryPathInfo(dir_.GetPath().AppendASCII("foo"));
  const std::string kTestName = "test file name";
  base::CreateDirectory(kDirectoryPathInfo.path);
  if (base::FeatureList::IsEnabled(
          features::kFileSystemAccessDirectoryIterationBlocklistCheck)) {
    EXPECT_CALL(permission_context_,
                ConfirmSensitiveEntryAccess_(
                    kTestStorageKey.origin(),
                    PathInfo(kDirectoryPathInfo.path.AppendASCII(kTestName)),
                    FileSystemAccessPermissionContext::HandleType::kFile,
                    FileSystemAccessPermissionContext::UserAction::kNone,
                    kFrameId, testing::_))
        .WillOnce(RunOnceCallback<5>(
            FileSystemAccessPermissionContext::SensitiveEntryResult::kAllowed));
  }

  mojo::Remote<blink::mojom::FileSystemAccessDirectoryHandle> directory_handle =
      GetHandleForDirectory(kDirectoryPathInfo);

  mojo::Remote<blink::mojom::FileSystemAccessFileHandle> file_handle;
  base::test::TestFuture<
      blink::mojom::FileSystemAccessErrorPtr,
      mojo::PendingRemote<blink::mojom::FileSystemAccessFileHandle>>
      future;
  directory_handle->GetFile(kTestName, /*create=*/true, future.GetCallback());
  blink::mojom::FileSystemAccessErrorPtr result;
  mojo::PendingRemote<blink::mojom::FileSystemAccessFileHandle> handle;
  std::tie(result, handle) = future.Take();
  ASSERT_EQ(result->status, blink::mojom::FileSystemAccessStatus::kOk);
  file_handle.Bind(std::move(handle));
  ASSERT_TRUE(file_handle.is_bound());

  mojo::PendingRemote<blink::mojom::FileSystemAccessTransferToken> token_remote;
  file_handle->Transfer(token_remote.InitWithNewPipeAndPassReceiver());

  // Deserializing tokens should re-request grants, with correct user action.
  EXPECT_CALL(
      permission_context_,
      GetReadPermissionGrant(
          kTestStorageKey.origin(), kDirectoryPathInfo, HandleType::kDirectory,
          FileSystemAccessPermissionContext::UserAction::kLoadFromStorage))
      .WillOnce(testing::Return(ask_grant_));
  EXPECT_CALL(
      permission_context_,
      GetWritePermissionGrant(
          kTestStorageKey.origin(), kDirectoryPathInfo, HandleType::kDirectory,
          FileSystemAccessPermissionContext::UserAction::kLoadFromStorage))
      .WillOnce(testing::Return(ask_grant2_));

  FileSystemAccessTransferTokenImpl* token =
      SerializeAndDeserializeToken(std::move(token_remote));
  ASSERT_TRUE(token);
  const storage::FileSystemURL& url = token->url();
  EXPECT_TRUE(url.origin().opaque());
  EXPECT_EQ(
      kDirectoryPathInfo.path.Append(base::FilePath::FromUTF8Unsafe(kTestName)),
      url.path());
  EXPECT_EQ(storage::kFileSystemTypeLocal, url.type());
  EXPECT_EQ(storage::kFileSystemTypeLocal, url.mount_type());
  EXPECT_EQ(HandleType::kFile, token->type());
  EXPECT_EQ(ask_grant_, token->GetReadGrant());
  EXPECT_EQ(ask_grant2_, token->GetWriteGrant());
}

TEST_F(FileSystemAccessManagerImplTest,
       SerializeHandle_Native_DirectoryInsideDirectory) {
  const PathInfo kDirectoryPathInfo(dir_.GetPath().AppendASCII("foo"));
  const std::string kTestName = "test dir name";
  base::CreateDirectory(kDirectoryPathInfo.path);

  mojo::Remote<blink::mojom::FileSystemAccessDirectoryHandle> directory_handle =
      GetHandleForDirectory(kDirectoryPathInfo);

  mojo::Remote<blink::mojom::FileSystemAccessDirectoryHandle> child_handle;
  base::test::TestFuture<
      blink::mojom::FileSystemAccessErrorPtr,
      mojo::PendingRemote<blink::mojom::FileSystemAccessDirectoryHandle>>
      future;
  directory_handle->GetDirectory(kTestName, /*create=*/true,
                                 future.GetCallback());
  blink::mojom::FileSystemAccessErrorPtr result;
  mojo::PendingRemote<blink::mojom::FileSystemAccessDirectoryHandle> handle;
  std::tie(result, handle) = future.Take();
  ASSERT_EQ(result->status, blink::mojom::FileSystemAccessStatus::kOk);
  child_handle.Bind(std::move(handle));
  ASSERT_TRUE(child_handle.is_bound());

  mojo::PendingRemote<blink::mojom::FileSystemAccessTransferToken> token_remote;
  child_handle->Transfer(token_remote.InitWithNewPipeAndPassReceiver());

  // Deserializing tokens should re-request grants, with correct user action.
  EXPECT_CALL(
      permission_context_,
      GetReadPermissionGrant(
          kTestStorageKey.origin(), kDirectoryPathInfo, HandleType::kDirectory,
          FileSystemAccessPermissionContext::UserAction::kLoadFromStorage))
      .WillOnce(testing::Return(ask_grant_));
  EXPECT_CALL(
      permission_context_,
      GetWritePermissionGrant(
          kTestStorageKey.origin(), kDirectoryPathInfo, HandleType::kDirectory,
          FileSystemAccessPermissionContext::UserAction::kLoadFromStorage))
      .WillOnce(testing::Return(ask_grant2_));

  FileSystemAccessTransferTokenImpl* token =
      SerializeAndDeserializeToken(std::move(token_remote));
  ASSERT_TRUE(token);
  const storage::FileSystemURL& url = token->url();
  EXPECT_TRUE(url.origin().opaque());
  EXPECT_EQ(kDirectoryPathInfo.path.AppendASCII(kTestName), url.path());
  EXPECT_EQ(storage::kFileSystemTypeLocal, url.type());
  EXPECT_EQ(storage::kFileSystemTypeLocal, url.mount_type());
  EXPECT_EQ(HandleType::kDirectory, token->type());
  EXPECT_EQ(ask_grant_, token->GetReadGrant());
  EXPECT_EQ(ask_grant2_, token->GetWriteGrant());
}

TEST_F(FileSystemAccessManagerImplTest, SerializeHandle_ExternalFile) {
  const PathInfo kTestPathInfo(
      PathType::kExternal,
      base::FilePath::FromUTF8Unsafe(kTestMountPoint).AppendASCII("foo"));

  auto grant = base::MakeRefCounted<FixedFileSystemAccessPermissionGrant>(
      FixedFileSystemAccessPermissionGrant::PermissionStatus::GRANTED,
      kTestPathInfo);

  // Expect calls to get grants when creating the initial handle.
  EXPECT_CALL(permission_context_,
              GetReadPermissionGrant(
                  kTestStorageKey.origin(), kTestPathInfo, HandleType::kFile,
                  FileSystemAccessPermissionContext::UserAction::kOpen))
      .WillOnce(testing::Return(grant));
  EXPECT_CALL(permission_context_,
              GetWritePermissionGrant(
                  kTestStorageKey.origin(), kTestPathInfo, HandleType::kFile,
                  FileSystemAccessPermissionContext::UserAction::kOpen))
      .WillOnce(testing::Return(grant));

  blink::mojom::FileSystemAccessEntryPtr entry =
      manager_->CreateFileEntryFromPath(
          kBindingContext, kTestPathInfo,
          FileSystemAccessPermissionContext::UserAction::kOpen);
  mojo::Remote<blink::mojom::FileSystemAccessFileHandle> handle(
      std::move(entry->entry_handle->get_file()));

  mojo::PendingRemote<blink::mojom::FileSystemAccessTransferToken> token_remote;
  handle->Transfer(token_remote.InitWithNewPipeAndPassReceiver());

  // Deserializing tokens should re-request grants, with correct user action.
  EXPECT_CALL(
      permission_context_,
      GetReadPermissionGrant(
          kTestStorageKey.origin(), kTestPathInfo, HandleType::kFile,
          FileSystemAccessPermissionContext::UserAction::kLoadFromStorage))
      .WillOnce(testing::Return(ask_grant_));
  EXPECT_CALL(
      permission_context_,
      GetWritePermissionGrant(
          kTestStorageKey.origin(), kTestPathInfo, HandleType::kFile,
          FileSystemAccessPermissionContext::UserAction::kLoadFromStorage))
      .WillOnce(testing::Return(ask_grant2_));

  FileSystemAccessTransferTokenImpl* token =
      SerializeAndDeserializeToken(std::move(token_remote));
  ASSERT_TRUE(token);
  const storage::FileSystemURL& url = token->url();
  EXPECT_TRUE(url.origin().opaque());
  EXPECT_EQ(kTestPathInfo.path, url.virtual_path());
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

  GetEntryFromDataTransferTokenFileTest(PathInfo(file_path), file_contents);
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
  const std::string child_dir_name = "child_dir";
  ASSERT_TRUE(base::CreateDirectory(dir_path.AppendASCII(child_dir_name)));

  GetEntryFromDataTransferTokenDirectoryTest(PathInfo(dir_path),
                                             child_dir_name);
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
      PathInfo(PathType::kExternal, virtual_file_path), file_contents);
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
  const std::string child_dir_name = "child_dir";
  ASSERT_TRUE(base::CreateDirectory(dir_path.AppendASCII(child_dir_name)));

  const base::FilePath virtual_dir_path =
      base::FilePath::FromUTF8Unsafe(kTestMountPoint)
          .Append(dir_path.BaseName());

  GetEntryFromDataTransferTokenDirectoryTest(
      PathInfo(PathType::kExternal, virtual_dir_path), child_dir_name);
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
      PathInfo(file_path),
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
      PathInfo(kDirPath),
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

TEST_F(FileSystemAccessManagerImplTest,
       GetEntryFromDataTransferToken_File_NoSensitiveAccessCheck) {
  if (!base::FeatureList::IsEnabled(
          features::kFileSystemAccessDragAndDropCheckBlocklist)) {
    return;
  }

  PathInfo file_info(dir_.GetPath().AppendASCII("mr_file"));
  const std::string file_contents = "Deleted code is debugged code.";
  ASSERT_TRUE(base::WriteFile(file_info.path, file_contents));

  mojo::PendingRemote<blink::mojom::FileSystemAccessDataTransferToken>
      token_remote;
  manager_->CreateFileSystemAccessDataTransferToken(
      file_info, kBindingContext.process_id(),
      token_remote.InitWithNewPipeAndPassReceiver());

  EXPECT_CALL(permission_context_,
              ConfirmSensitiveEntryAccess_(testing::_, testing::_, testing::_,
                                           testing::_, testing::_, testing::_))
      .Times(0);

  // Expect permission requests when the token is sent to be redeemed.
  EXPECT_CALL(permission_context_,
              GetReadPermissionGrant(
                  kTestStorageKey.origin(), file_info, HandleType::kFile,
                  FileSystemAccessPermissionContext::UserAction::kDragAndDrop))
      .WillOnce(testing::Return(allow_grant_));

  EXPECT_CALL(permission_context_,
              GetWritePermissionGrant(
                  kTestStorageKey.origin(), file_info, HandleType::kFile,
                  FileSystemAccessPermissionContext::UserAction::kDragAndDrop))
      .WillOnce(testing::Return(allow_grant_));

  // Attempt to resolve `token_remote` and store the resulting
  // FileSystemAccessFileHandle in `file_remote`.
  base::test::TestFuture<blink::mojom::FileSystemAccessErrorPtr,
                         blink::mojom::FileSystemAccessEntryPtr>
      get_entry_future;
  manager_remote_->GetEntryFromDataTransferToken(
      std::move(token_remote), get_entry_future.GetCallback());
  DCHECK_EQ(get_entry_future.Get<0>()->status,
            blink::mojom::FileSystemAccessStatus::kOk);
  auto file_system_access_entry = std::get<1>(get_entry_future.Take());

  EXPECT_FALSE(file_system_access_entry.is_null());
  ASSERT_TRUE(file_system_access_entry->entry_handle->is_file());
  mojo::Remote<blink::mojom::FileSystemAccessFileHandle> file_handle(
      std::move(file_system_access_entry->entry_handle->get_file()));

  // Check to see if the resulting FileSystemAccessFileHandle can read the
  // contents of the file at `file_path`.
  EXPECT_EQ(ReadStringFromFileRemote(std::move(file_handle)), file_contents);
}

TEST_F(FileSystemAccessManagerImplTest,
       GetEntryFromDataTransferToken_Directory_SensitivePath) {
  if (!base::FeatureList::IsEnabled(
          features::kFileSystemAccessDragAndDropCheckBlocklist)) {
    return;
  }

  const PathInfo kDirPathInfo(dir_.GetPath().AppendASCII("mr_directory"));
  ASSERT_TRUE(base::CreateDirectory(kDirPathInfo.path));

  mojo::PendingRemote<blink::mojom::FileSystemAccessDataTransferToken>
      token_remote;
  manager_->CreateFileSystemAccessDataTransferToken(
      kDirPathInfo, kBindingContext.process_id(),
      token_remote.InitWithNewPipeAndPassReceiver());

  EXPECT_CALL(permission_context_,
              ConfirmSensitiveEntryAccess_(
                  kTestStorageKey.origin(), kDirPathInfo,
                  FileSystemAccessPermissionContext::HandleType::kDirectory,
                  FileSystemAccessPermissionContext::UserAction::kDragAndDrop,
                  kFrameId, testing::_))
      .WillOnce(RunOnceCallback<5>(
          FileSystemAccessPermissionContext::SensitiveEntryResult::kAbort));

  // Attempt to resolve `token_remote` should abort.
  base::test::TestFuture<blink::mojom::FileSystemAccessErrorPtr,
                         blink::mojom::FileSystemAccessEntryPtr>
      get_entry_future;
  manager_remote_->GetEntryFromDataTransferToken(
      std::move(token_remote), get_entry_future.GetCallback());
  DCHECK_EQ(get_entry_future.Get<0>()->status,
            blink::mojom::FileSystemAccessStatus::kOperationAborted);
  auto file_system_access_entry = std::get<1>(get_entry_future.Take());
  EXPECT_TRUE(file_system_access_entry.is_null());
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
          manager_.get(), PathInfo(kDirPath), kBindingContext.process_id(),
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
  PathInfo test_file_info(test_file);

  manager_->SetFilePickerResultForTesting(test_file_info);

  static_cast<TestRenderFrameHost*>(web_contents_->GetPrimaryMainFrame())
      ->SimulateUserActivation();

  mojo::Remote<blink::mojom::FileSystemAccessManager> manager_remote;
  FileSystemAccessManagerImpl::BindingContext binding_context = {
      kTestStorageKey, kTestURL,
      web_contents_->GetPrimaryMainFrame()->GetGlobalId()};
  manager_->BindReceiver(binding_context,
                         manager_remote.BindNewPipeAndPassReceiver());

  EXPECT_CALL(permission_context_,
              CanObtainReadPermission(kTestStorageKey.origin()))
      .WillOnce(testing::Return(true));

  EXPECT_CALL(
      permission_context_,
      GetWellKnownDirectoryPath(blink::mojom::WellKnownDirectory::kDirDocuments,
                                kTestStorageKey.origin()))
      .WillOnce(testing::Return(base::FilePath()));
  EXPECT_CALL(permission_context_,
              GetLastPickedDirectory(kTestStorageKey.origin(), std::string()))
      .WillOnce(testing::Return(PathInfo()));
  EXPECT_CALL(permission_context_, GetPickerTitle(testing::_))
      .WillOnce(testing::Return(std::u16string()));
  EXPECT_CALL(permission_context_,
              SetLastPickedDirectory(kTestStorageKey.origin(), std::string(),
                                     PathInfo(test_file_info.path.DirName())));

  EXPECT_CALL(
      permission_context_,
      ConfirmSensitiveEntryAccess_(
          kTestStorageKey.origin(), test_file_info,
          FileSystemAccessPermissionContext::HandleType::kFile,
          FileSystemAccessPermissionContext::UserAction::kOpen,
          web_contents_->GetPrimaryMainFrame()->GetGlobalId(), testing::_))
      .WillOnce(RunOnceCallback<5>(
          FileSystemAccessPermissionContext::SensitiveEntryResult::kAllowed));

  EXPECT_CALL(permission_context_,
              GetReadPermissionGrant(
                  kTestStorageKey.origin(), test_file_info,
                  FileSystemAccessPermissionContext::HandleType::kFile,
                  FileSystemAccessPermissionContext::UserAction::kOpen))
      .WillOnce(testing::Return(allow_grant_));
  EXPECT_CALL(permission_context_,
              GetWritePermissionGrant(
                  kTestStorageKey.origin(), test_file_info,
                  FileSystemAccessPermissionContext::HandleType::kFile,
                  FileSystemAccessPermissionContext::UserAction::kOpen))
      .WillOnce(testing::Return(allow_grant_));
  EXPECT_CALL(permission_context_, CheckPathsAgainstEnterprisePolicy(
                                       testing::_, testing::_, testing::_))
      .WillOnce(testing::Invoke(
          [](std::vector<PathInfo> entries,
             content::GlobalRenderFrameHostId frame_id,
             MockFileSystemAccessPermissionContext::
                 EntriesAllowedByEnterprisePolicyCallback callback) {
            std::move(callback).Run(std::move(entries));
          }));

  auto open_file_picker_options = blink::mojom::OpenFilePickerOptions::New(
      blink::mojom::AcceptsTypesInfo::New(
          std::vector<blink::mojom::ChooseFileSystemEntryAcceptsOptionPtr>(),
          /*include_accepts_all=*/true),
      /*can_select_multiple_files=*/false);
  auto picker_options = blink::mojom::FilePickerOptions::New(
      blink::mojom::TypeSpecificFilePickerOptionsUnion::
          NewOpenFilePickerOptions(std::move(open_file_picker_options)),
      /*starting_directory_id=*/std::string(),
      blink::mojom::FilePickerStartInOptionsUnionPtr());

  base::test::TestFuture<blink::mojom::FileSystemAccessErrorPtr,
                         std::vector<blink::mojom::FileSystemAccessEntryPtr>>
      future;
  manager_remote->ChooseEntries(std::move(picker_options),
                                future.GetCallback());
  ASSERT_TRUE(future.Wait());
}

TEST_F(FileSystemAccessManagerImplTest,
       ChooseEntries_OpenFile_EnterpriseBlock) {
  base::FilePath test_file = dir_.GetPath().AppendASCII("foo");
  ASSERT_TRUE(base::CreateTemporaryFile(&test_file));
  PathInfo test_file_info(test_file);

  manager_->SetFilePickerResultForTesting(test_file_info);

  static_cast<TestRenderFrameHost*>(web_contents_->GetPrimaryMainFrame())
      ->SimulateUserActivation();

  mojo::Remote<blink::mojom::FileSystemAccessManager> manager_remote;
  FileSystemAccessManagerImpl::BindingContext binding_context = {
      kTestStorageKey, kTestURL,
      web_contents_->GetPrimaryMainFrame()->GetGlobalId()};
  manager_->BindReceiver(binding_context,
                         manager_remote.BindNewPipeAndPassReceiver());

  EXPECT_CALL(permission_context_,
              CanObtainReadPermission(kTestStorageKey.origin()))
      .WillOnce(testing::Return(true));

  EXPECT_CALL(
      permission_context_,
      GetWellKnownDirectoryPath(blink::mojom::WellKnownDirectory::kDirDocuments,
                                kTestStorageKey.origin()))
      .WillOnce(testing::Return(base::FilePath()));
  EXPECT_CALL(permission_context_,
              GetLastPickedDirectory(kTestStorageKey.origin(), std::string()))
      .WillOnce(testing::Return(PathInfo()));
  EXPECT_CALL(permission_context_, GetPickerTitle(testing::_))
      .WillOnce(testing::Return(std::u16string()));

  EXPECT_CALL(
      permission_context_,
      ConfirmSensitiveEntryAccess_(
          kTestStorageKey.origin(), test_file_info,
          FileSystemAccessPermissionContext::HandleType::kFile,
          FileSystemAccessPermissionContext::UserAction::kOpen,
          web_contents_->GetPrimaryMainFrame()->GetGlobalId(), testing::_))
      .WillOnce(RunOnceCallback<5>(
          FileSystemAccessPermissionContext::SensitiveEntryResult::kAllowed));

  // This is where the tests mocks the enterprise check as blocking the file.
  // The callback is invoked with an empty path.
  EXPECT_CALL(permission_context_, CheckPathsAgainstEnterprisePolicy(
                                       testing::_, testing::_, testing::_))
      .WillOnce(testing::Invoke(
          [](std::vector<PathInfo> entries,
             content::GlobalRenderFrameHostId frame_id,
             MockFileSystemAccessPermissionContext::
                 EntriesAllowedByEnterprisePolicyCallback callback) {
            std::move(callback).Run(std::vector<PathInfo>());
          }));

  auto open_file_picker_options = blink::mojom::OpenFilePickerOptions::New(
      blink::mojom::AcceptsTypesInfo::New(
          std::vector<blink::mojom::ChooseFileSystemEntryAcceptsOptionPtr>(),
          /*include_accepts_all=*/true),
      /*can_select_multiple_files=*/false);
  auto picker_options = blink::mojom::FilePickerOptions::New(
      blink::mojom::TypeSpecificFilePickerOptionsUnion::
          NewOpenFilePickerOptions(std::move(open_file_picker_options)),
      /*starting_directory_id=*/std::string(),
      blink::mojom::FilePickerStartInOptionsUnionPtr());

  base::test::TestFuture<blink::mojom::FileSystemAccessErrorPtr,
                         std::vector<blink::mojom::FileSystemAccessEntryPtr>>
      future;
  manager_remote->ChooseEntries(std::move(picker_options),
                                future.GetCallback());
  ASSERT_TRUE(future.Wait());
}

TEST_F(FileSystemAccessManagerImplTest, ChooseEntries_SaveFile) {
  base::FilePath test_file = dir_.GetPath().AppendASCII("foo");
  ASSERT_TRUE(base::CreateTemporaryFile(&test_file));
  PathInfo test_file_info(test_file);

  manager_->SetFilePickerResultForTesting(test_file_info);

  static_cast<TestRenderFrameHost*>(web_contents_->GetPrimaryMainFrame())
      ->SimulateUserActivation();

  mojo::Remote<blink::mojom::FileSystemAccessManager> manager_remote;
  FileSystemAccessManagerImpl::BindingContext binding_context = {
      kTestStorageKey, kTestURL,
      web_contents_->GetPrimaryMainFrame()->GetGlobalId()};
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
      GetWellKnownDirectoryPath(blink::mojom::WellKnownDirectory::kDirDocuments,
                                kTestStorageKey.origin()))
      .WillOnce(testing::Return(base::FilePath()));
  EXPECT_CALL(permission_context_,
              GetLastPickedDirectory(kTestStorageKey.origin(), std::string()))
      .WillOnce(testing::Return(PathInfo()));
  EXPECT_CALL(permission_context_, GetPickerTitle(testing::_))
      .WillOnce(testing::Return(std::u16string()));
  EXPECT_CALL(permission_context_,
              SetLastPickedDirectory(kTestStorageKey.origin(), std::string(),
                                     PathInfo(test_file_info.path.DirName())));

  EXPECT_CALL(
      permission_context_,
      ConfirmSensitiveEntryAccess_(
          kTestStorageKey.origin(), test_file_info,
          FileSystemAccessPermissionContext::HandleType::kFile,
          FileSystemAccessPermissionContext::UserAction::kSave,
          web_contents_->GetPrimaryMainFrame()->GetGlobalId(), testing::_))
      .WillOnce(RunOnceCallback<5>(
          FileSystemAccessPermissionContext::SensitiveEntryResult::kAllowed));

  EXPECT_CALL(permission_context_,
              GetReadPermissionGrant(
                  kTestStorageKey.origin(), test_file_info,
                  FileSystemAccessPermissionContext::HandleType::kFile,
                  FileSystemAccessPermissionContext::UserAction::kSave))
      .WillOnce(testing::Return(allow_grant_));
  EXPECT_CALL(permission_context_,
              GetWritePermissionGrant(
                  kTestStorageKey.origin(), test_file_info,
                  FileSystemAccessPermissionContext::HandleType::kFile,
                  FileSystemAccessPermissionContext::UserAction::kSave))
      .WillOnce(testing::Return(allow_grant_));

  EXPECT_CALL(permission_context_,
              OnFileCreatedFromShowSaveFilePicker(
                  /*file_picker_binding_context=*/binding_context.url,
                  file_system_context_->CreateCrackedFileSystemURL(
                      blink::StorageKey(),
                      storage::FileSystemType::kFileSystemTypeLocal,
                      test_file_info.path)));
  EXPECT_CALL(permission_context_, CheckPathsAgainstEnterprisePolicy(
                                       testing::_, testing::_, testing::_))
      .Times(0);

  auto save_file_picker_options = blink::mojom::SaveFilePickerOptions::New(
      blink::mojom::AcceptsTypesInfo::New(
          std::vector<blink::mojom::ChooseFileSystemEntryAcceptsOptionPtr>(),
          /*include_accepts_all=*/true),
      /*suggested_name=*/std::string());
  auto picker_options = blink::mojom::FilePickerOptions::New(
      blink::mojom::TypeSpecificFilePickerOptionsUnion::
          NewSaveFilePickerOptions(std::move(save_file_picker_options)),
      /*starting_directory_id=*/std::string(),
      blink::mojom::FilePickerStartInOptionsUnionPtr());

  base::test::TestFuture<blink::mojom::FileSystemAccessErrorPtr,
                         std::vector<blink::mojom::FileSystemAccessEntryPtr>>
      future;
  manager_remote->ChooseEntries(std::move(picker_options),
                                future.GetCallback());
  ASSERT_TRUE(future.Wait());
}

TEST_F(FileSystemAccessManagerImplTest, ChooseEntries_OpenDirectory) {
  PathInfo test_dir_info(dir_.GetPath());

  manager_->SetFilePickerResultForTesting(test_dir_info);

  static_cast<TestRenderFrameHost*>(web_contents_->GetPrimaryMainFrame())
      ->SimulateUserActivation();

  mojo::Remote<blink::mojom::FileSystemAccessManager> manager_remote;
  FileSystemAccessManagerImpl::BindingContext binding_context = {
      kTestStorageKey, kTestURL,
      web_contents_->GetPrimaryMainFrame()->GetGlobalId()};
  manager_->BindReceiver(binding_context,
                         manager_remote.BindNewPipeAndPassReceiver());

  EXPECT_CALL(permission_context_,
              CanObtainReadPermission(kTestStorageKey.origin()))
      .WillOnce(testing::Return(true));

  EXPECT_CALL(
      permission_context_,
      GetWellKnownDirectoryPath(blink::mojom::WellKnownDirectory::kDirDocuments,
                                kTestStorageKey.origin()))
      .WillOnce(testing::Return(base::FilePath()));
  EXPECT_CALL(permission_context_,
              GetLastPickedDirectory(kTestStorageKey.origin(), std::string()))
      .WillOnce(testing::Return(PathInfo()));
  EXPECT_CALL(permission_context_, GetPickerTitle(testing::_))
      .WillOnce(testing::Return(std::u16string()));
  EXPECT_CALL(permission_context_,
              SetLastPickedDirectory(kTestStorageKey.origin(), std::string(),
                                     test_dir_info));

  EXPECT_CALL(
      permission_context_,
      ConfirmSensitiveEntryAccess_(
          kTestStorageKey.origin(), test_dir_info,
          FileSystemAccessPermissionContext::HandleType::kDirectory,
          FileSystemAccessPermissionContext::UserAction::kOpen,
          web_contents_->GetPrimaryMainFrame()->GetGlobalId(), testing::_))
      .WillOnce(RunOnceCallback<5>(
          FileSystemAccessPermissionContext::SensitiveEntryResult::kAllowed));

  EXPECT_CALL(permission_context_,
              GetReadPermissionGrant(
                  kTestStorageKey.origin(), test_dir_info,
                  FileSystemAccessPermissionContext::HandleType::kDirectory,
                  FileSystemAccessPermissionContext::UserAction::kOpen))
      .WillOnce(testing::Return(allow_grant_));
  EXPECT_CALL(permission_context_,
              GetWritePermissionGrant(
                  kTestStorageKey.origin(), test_dir_info,
                  FileSystemAccessPermissionContext::HandleType::kDirectory,
                  FileSystemAccessPermissionContext::UserAction::kOpen))
      .WillOnce(testing::Return(allow_grant_));
  EXPECT_CALL(permission_context_, CheckPathsAgainstEnterprisePolicy(
                                       testing::_, testing::_, testing::_))
      .WillOnce(testing::Invoke(
          [](std::vector<PathInfo> entries,
             content::GlobalRenderFrameHostId frame_id,
             MockFileSystemAccessPermissionContext::
                 EntriesAllowedByEnterprisePolicyCallback callback) {
            std::move(callback).Run(std::move(entries));
          }));

  auto picker_options = blink::mojom::FilePickerOptions::New(
      blink::mojom::TypeSpecificFilePickerOptionsUnion::
          NewDirectoryPickerOptions(
              blink::mojom::DirectoryPickerOptions::New()),
      /*starting_directory_id=*/std::string(),
      blink::mojom::FilePickerStartInOptionsUnionPtr());

  base::test::TestFuture<blink::mojom::FileSystemAccessErrorPtr,
                         std::vector<blink::mojom::FileSystemAccessEntryPtr>>
      future;
  manager_remote->ChooseEntries(std::move(picker_options),
                                future.GetCallback());
  ASSERT_TRUE(future.Wait());
}

TEST_F(FileSystemAccessManagerImplTest,
       ChooseEntries_OpenDirectory_EnterpriseBlock) {
  PathInfo test_dir_info(dir_.GetPath());

  manager_->SetFilePickerResultForTesting(test_dir_info);

  static_cast<TestRenderFrameHost*>(web_contents_->GetPrimaryMainFrame())
      ->SimulateUserActivation();

  mojo::Remote<blink::mojom::FileSystemAccessManager> manager_remote;
  FileSystemAccessManagerImpl::BindingContext binding_context = {
      kTestStorageKey, kTestURL,
      web_contents_->GetPrimaryMainFrame()->GetGlobalId()};
  manager_->BindReceiver(binding_context,
                         manager_remote.BindNewPipeAndPassReceiver());

  EXPECT_CALL(permission_context_,
              CanObtainReadPermission(kTestStorageKey.origin()))
      .WillOnce(testing::Return(true));

  EXPECT_CALL(
      permission_context_,
      GetWellKnownDirectoryPath(blink::mojom::WellKnownDirectory::kDirDocuments,
                                kTestStorageKey.origin()))
      .WillOnce(testing::Return(base::FilePath()));
  EXPECT_CALL(permission_context_,
              GetLastPickedDirectory(kTestStorageKey.origin(), std::string()))
      .WillOnce(testing::Return(PathInfo()));
  EXPECT_CALL(permission_context_, GetPickerTitle(testing::_))
      .WillOnce(testing::Return(std::u16string()));

  EXPECT_CALL(
      permission_context_,
      ConfirmSensitiveEntryAccess_(
          kTestStorageKey.origin(), test_dir_info,
          FileSystemAccessPermissionContext::HandleType::kDirectory,
          FileSystemAccessPermissionContext::UserAction::kOpen,
          web_contents_->GetPrimaryMainFrame()->GetGlobalId(), testing::_))
      .WillOnce(RunOnceCallback<5>(
          FileSystemAccessPermissionContext::SensitiveEntryResult::kAllowed));

  // This is where the tests mocks the enterprise check as blocking the file.
  // The callback is invoked with an empty path.
  EXPECT_CALL(permission_context_, CheckPathsAgainstEnterprisePolicy(
                                       testing::_, testing::_, testing::_))
      .WillOnce(testing::Invoke(
          [](std::vector<PathInfo> entries,
             content::GlobalRenderFrameHostId frame_id,
             MockFileSystemAccessPermissionContext::
                 EntriesAllowedByEnterprisePolicyCallback callback) {
            std::move(callback).Run(std::vector<PathInfo>());
          }));

  auto picker_options = blink::mojom::FilePickerOptions::New(
      blink::mojom::TypeSpecificFilePickerOptionsUnion::
          NewDirectoryPickerOptions(
              blink::mojom::DirectoryPickerOptions::New()),
      /*starting_directory_id=*/std::string(),
      blink::mojom::FilePickerStartInOptionsUnionPtr());

  base::test::TestFuture<blink::mojom::FileSystemAccessErrorPtr,
                         std::vector<blink::mojom::FileSystemAccessEntryPtr>>
      future;
  manager_remote->ChooseEntries(std::move(picker_options),
                                future.GetCallback());
  ASSERT_TRUE(future.Wait());
}

TEST_F(FileSystemAccessManagerImplTest, ChooseEntries_InvalidStartInID) {
  PathInfo test_dir_info(dir_.GetPath());

  manager_->SetFilePickerResultForTesting(test_dir_info);

  static_cast<TestRenderFrameHost*>(web_contents_->GetPrimaryMainFrame())
      ->SimulateUserActivation();

  mojo::Remote<blink::mojom::FileSystemAccessManager> manager_remote;
  FileSystemAccessManagerImpl::BindingContext binding_context = {
      kTestStorageKey, kTestURL,
      web_contents_->GetPrimaryMainFrame()->GetGlobalId()};
  manager_->BindReceiver(binding_context,
                         manager_remote.BindNewPipeAndPassReceiver());

  // Specifying a `id` with invalid characters should trigger
  // a bad message callback.
  auto picker_options = blink::mojom::FilePickerOptions::New(
      blink::mojom::TypeSpecificFilePickerOptionsUnion::
          NewDirectoryPickerOptions(
              blink::mojom::DirectoryPickerOptions::New()),
      /*starting_directory_id=*/"inv*l!d <hars",
      blink::mojom::FilePickerStartInOptionsUnionPtr());

  mojo::test::BadMessageObserver bad_message_observer;
  manager_remote->ChooseEntries(std::move(picker_options), base::DoNothing());
  EXPECT_EQ("Invalid starting directory ID in browser",
            bad_message_observer.WaitForBadMessage());
}

TEST_F(FileSystemAccessManagerImplTest, GetUniqueId) {
  const PathInfo kTestPathInfo(dir_.GetPath().AppendASCII("foo"));
  ASSERT_OK_AND_ASSIGN(auto default_bucket,
                       CreateSandboxFileSystemAndGetDefaultBucket());

  auto grant = base::MakeRefCounted<FixedFileSystemAccessPermissionGrant>(
      FixedFileSystemAccessPermissionGrant::PermissionStatus::GRANTED,
      kTestPathInfo);

  auto test_url = file_system_context_->CreateCrackedFileSystemURL(
      kTestStorageKey, storage::kFileSystemTypeTemporary, kTestPathInfo.path);
  test_url.SetBucket(default_bucket);

  FileSystemAccessFileHandleImpl file(manager_.get(), kBindingContext, test_url,
                                      {ask_grant_, ask_grant_});
  auto file_id = manager_->GetUniqueId(file);
  // Ensure a valid ID is provided.
  EXPECT_TRUE(file_id.is_valid());

  // Create a dir handle to the same path. The ID should be different than the
  // ID for the file.
  FileSystemAccessDirectoryHandleImpl dir(manager_.get(), kBindingContext,
                                          test_url, {ask_grant_, ask_grant_});
  auto dir_id = manager_->GetUniqueId(dir);
  EXPECT_TRUE(dir_id.is_valid());
  EXPECT_NE(file_id, dir_id);

  // Create a file handle to another path. The ID should be different from
  // either of the other IDs.
  auto other_url = file_system_context_->CreateCrackedFileSystemURL(
      kTestStorageKey, storage::kFileSystemTypeTemporary,
      kTestPathInfo.path.AppendASCII("bar"));
  other_url.SetBucket(default_bucket);
  FileSystemAccessFileHandleImpl other_file(
      manager_.get(), kBindingContext, other_url, {ask_grant_, ask_grant_});
  auto other_id = manager_->GetUniqueId(other_file);
  EXPECT_TRUE(other_id.is_valid());
  EXPECT_NE(other_id, file_id);
  EXPECT_NE(other_id, dir_id);
}

TEST_F(FileSystemAccessManagerImplTest, IsSafePathComponent) {
  // Path components which are allowed everywhere.
  constexpr const char* kSafePathComponents[] = {
      "a", "a.txt", "a b.txt", "My Computer", ".a", "lnk.zip", "lnk", "a.local",
  };

  // Path components which are disallowed everywhere.
  constexpr const char* kAlwaysUnsafePathComponents[] = {
      "", ".", "..", "a/", "a\\", "a\\a", "a/a", "C:\\", "C:/",
  };

  // Path components which are allowed only in sandboxed file systems.
  constexpr const char* kUnsafeLocalPathComponents[] = {
      "...",
      "con",
      "con.zip",
      "NUL",
      "NUL.zip",
      "a.",
      "a\"a",
      "a<a",
      "a>a",
      "a?a",
      "a ",
      "a . .",
      " Computer",
      "My Computer.{a}",
      "My Computer.{20D04FE0-3AEA-1069-A2D8-08002B30309D}",
      "a.lnk",
      "a.url",
      "C:",
  };

  for (const char* component : kSafePathComponents) {
    EXPECT_TRUE(manager_->IsSafePathComponent(
        storage::kFileSystemTypeTemporary, kTestStorageKey.origin(), component))
        << component;
    EXPECT_TRUE(manager_->IsSafePathComponent(
        storage::kFileSystemTypeLocal, kTestStorageKey.origin(), component))
        << component;
    EXPECT_TRUE(manager_->IsSafePathComponent(
        storage::kFileSystemTypeExternal, kTestStorageKey.origin(), component))
        << component;
  }
  for (const char* component : kAlwaysUnsafePathComponents) {
    EXPECT_FALSE(manager_->IsSafePathComponent(
        storage::kFileSystemTypeTemporary, kTestStorageKey.origin(), component))
        << component;
    EXPECT_FALSE(manager_->IsSafePathComponent(
        storage::kFileSystemTypeLocal, kTestStorageKey.origin(), component))
        << component;
    EXPECT_FALSE(manager_->IsSafePathComponent(
        storage::kFileSystemTypeExternal, kTestStorageKey.origin(), component))
        << component;
  }
  for (const char* component : kUnsafeLocalPathComponents) {
    EXPECT_TRUE(manager_->IsSafePathComponent(
        storage::kFileSystemTypeTemporary, kTestStorageKey.origin(), component))
        << component;
    EXPECT_FALSE(manager_->IsSafePathComponent(
        storage::kFileSystemTypeLocal, kTestStorageKey.origin(), component))
        << component;
    EXPECT_FALSE(manager_->IsSafePathComponent(
        storage::kFileSystemTypeExternal, kTestStorageKey.origin(), component))
        << component;
  }
}

}  // namespace content
