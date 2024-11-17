// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/file_system_access/file_system_access_directory_handle_impl.h"

#include <iterator>
#include <optional>
#include <string>
#include <tuple>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "components/services/storage/public/cpp/buckets/bucket_locator.h"
#include "content/browser/file_system_access/features.h"
#include "content/browser/file_system_access/file_system_access_lock_manager.h"
#include "content/browser/file_system_access/fixed_file_system_access_permission_grant.h"
#include "content/browser/file_system_access/mock_file_system_access_permission_context.h"
#include "content/public/browser/file_system_access_permission_context.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/browser/test/test_file_system_context.h"
#include "storage/common/file_system/file_system_types.h"
#include "storage/common/file_system/file_system_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/gurl.h"

namespace content {

using storage::FileSystemURL;
using testing::_;
using HandleType = FileSystemAccessPermissionContext::HandleType;
using SensitiveEntryResult =
    FileSystemAccessPermissionContext::SensitiveEntryResult;
using UserAction = FileSystemAccessPermissionContext::UserAction;
using LockType = FileSystemAccessLockManager::LockType;

class FileSystemAccessDirectoryHandleImplTest : public testing::Test {
 public:
  FileSystemAccessDirectoryHandleImplTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::IO) {}

  void SetUp() override {
    ASSERT_TRUE(dir_.CreateUniqueTempDir());

    file_system_context_ = storage::CreateFileSystemContextForTesting(
        /*quota_manager_proxy=*/nullptr, dir_.GetPath());

    chrome_blob_context_ = base::MakeRefCounted<ChromeBlobStorageContext>();
    chrome_blob_context_->InitializeOnIOThread(base::FilePath(),
                                               base::FilePath(), nullptr);

    manager_ = base::MakeRefCounted<FileSystemAccessManagerImpl>(
        file_system_context_, chrome_blob_context_, &permission_context_,
        /*off_the_record=*/false);

    auto url = manager_->CreateFileSystemURLFromPath(PathInfo(dir_.GetPath()));
    handle_ = std::make_unique<FileSystemAccessDirectoryHandleImpl>(
        manager_.get(), kBindingContext, url,
        FileSystemAccessManagerImpl::SharedHandleState(allow_grant_,
                                                       allow_grant_));
    denied_handle_ = std::make_unique<FileSystemAccessDirectoryHandleImpl>(
        manager_.get(), kBindingContext, url,
        FileSystemAccessManagerImpl::SharedHandleState(deny_grant_,
                                                       deny_grant_));

    EXPECT_CALL(permission_context_, IsFileTypeDangerous_(_, _))
        .WillRepeatedly(testing::Return(false));
  }

  void TearDown() override {
    manager_.reset();
    task_environment_.RunUntilIdle();
  }

  std::unique_ptr<FileSystemAccessDirectoryHandleImpl> GetHandleWithPermissions(
      const base::FilePath& path,
      bool read,
      bool write,
      const std::optional<storage::BucketLocator> url_bucket_override =
          std::nullopt) {
    auto url = manager_->CreateFileSystemURLFromPath(PathInfo(path));
    if (url_bucket_override.has_value()) {
      url.SetBucket(url_bucket_override.value());
    }
    auto handle = std::make_unique<FileSystemAccessDirectoryHandleImpl>(
        manager_.get(), kBindingContext, url,
        FileSystemAccessManagerImpl::SharedHandleState(
            /*read_grant=*/read ? allow_grant_ : deny_grant_,
            /*write_grant=*/write ? allow_grant_ : deny_grant_));
    return handle;
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
  const GURL test_src_url_ = GURL("http://example.com/foo");
  const blink::StorageKey test_src_storage_key_ =
      blink::StorageKey::CreateFromStringForTesting("http://example.com/foo");
  const FileSystemAccessManagerImpl::BindingContext kBindingContext = {
      test_src_storage_key_, test_src_url_, /*worker_process_id=*/1};

  BrowserTaskEnvironment task_environment_;

  base::ScopedTempDir dir_;
  scoped_refptr<storage::FileSystemContext> file_system_context_;
  scoped_refptr<ChromeBlobStorageContext> chrome_blob_context_;
  scoped_refptr<FileSystemAccessManagerImpl> manager_;
  testing::StrictMock<MockFileSystemAccessPermissionContext>
      permission_context_;

  scoped_refptr<FixedFileSystemAccessPermissionGrant> allow_grant_ =
      base::MakeRefCounted<FixedFileSystemAccessPermissionGrant>(
          FixedFileSystemAccessPermissionGrant::PermissionStatus::GRANTED,
          PathInfo());
  scoped_refptr<FixedFileSystemAccessPermissionGrant> deny_grant_ =
      base::MakeRefCounted<FixedFileSystemAccessPermissionGrant>(
          FixedFileSystemAccessPermissionGrant::PermissionStatus::DENIED,
          PathInfo());
  std::unique_ptr<FileSystemAccessDirectoryHandleImpl> handle_;
  std::unique_ptr<FileSystemAccessDirectoryHandleImpl> denied_handle_;
};

namespace {
class TestFileSystemAccessDirectoryEntriesListener
    : public blink::mojom::FileSystemAccessDirectoryEntriesListener {
 public:
  TestFileSystemAccessDirectoryEntriesListener(
      std::vector<blink::mojom::FileSystemAccessEntryPtr>* entries,
      blink::mojom::FileSystemAccessErrorPtr* final_result,
      base::OnceClosure done)
      : entries_(entries),
        final_result_(final_result),
        done_(std::move(done)) {}

  void DidReadDirectory(
      blink::mojom::FileSystemAccessErrorPtr result,
      std::vector<blink::mojom::FileSystemAccessEntryPtr> entries,
      bool has_more_entries) override {
    entries_->insert(entries_->end(), std::make_move_iterator(entries.begin()),
                     std::make_move_iterator(entries.end()));
    if (has_more_entries) {
      EXPECT_EQ(result->status, blink::mojom::FileSystemAccessStatus::kOk);
    } else {
      *final_result_ = std::move(result);
      std::move(done_).Run();
    }
  }

 private:
  raw_ptr<std::vector<blink::mojom::FileSystemAccessEntryPtr>> entries_ =
      nullptr;
  raw_ptr<blink::mojom::FileSystemAccessErrorPtr> final_result_ = nullptr;
  base::OnceClosure done_;
};
}  // namespace

TEST_F(FileSystemAccessDirectoryHandleImplTest, GetEntries) {
  constexpr const char* kSafeNames[] = {"a", "a.txt", "My Computer", "lnk.txt",
                                        "a.local"};
  constexpr const char* kUnsafeNames[] = {
      "con",   "con.zip",         "NUL",   "a.", "a\"a", "a . .",
      "a.lnk", "My Computer.{a}", "a.url",
  };
  for (const char* name : kSafeNames) {
    ASSERT_TRUE(base::WriteFile(dir_.GetPath().AppendASCII(name), "data"))
        << name;
  }
  for (const char* name : kUnsafeNames) {
    base::FilePath file_path = dir_.GetPath().AppendASCII(name);
    bool success = base::WriteFile(file_path, "data");
#if !BUILDFLAG(IS_WIN)
    // Some of the unsafe names are not legal file names on Windows. This is
    // okay, and doesn't materially effect the outcome of the test, so just
    // ignore any failures writing these files to disk.
    EXPECT_TRUE(success) << "Failed to create file " << file_path;
#else
    std::ignore = success;
#endif
  }
  if (base::FeatureList::IsEnabled(
          features::kFileSystemAccessDirectoryIterationBlocklistCheck)) {
    EXPECT_CALL(
        permission_context_,
        ConfirmSensitiveEntryAccess_(_, _, HandleType::kFile, UserAction::kNone,
                                     kBindingContext.frame_id, _))
        .WillRepeatedly(base::test::RunOnceCallbackRepeatedly<5>(
            SensitiveEntryResult::kAllowed));
  }

  std::vector<blink::mojom::FileSystemAccessEntryPtr> entries;
  blink::mojom::FileSystemAccessErrorPtr result;
  base::RunLoop loop;
  mojo::PendingRemote<blink::mojom::FileSystemAccessDirectoryEntriesListener>
      listener;
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<TestFileSystemAccessDirectoryEntriesListener>(
          &entries, &result, loop.QuitClosure()),
      listener.InitWithNewPipeAndPassReceiver());
  handle_->GetEntries(std::move(listener));
  loop.Run();

  EXPECT_EQ(result->status, blink::mojom::FileSystemAccessStatus::kOk);
  std::vector<std::string> names;
  for (const auto& entry : entries) {
    names.push_back(entry->name);
  }
  EXPECT_THAT(names, testing::UnorderedElementsAreArray(kSafeNames));
}

TEST_F(FileSystemAccessDirectoryHandleImplTest,
       GetFile_SensitiveEntryAccessCheck) {
  if (!base::FeatureList::IsEnabled(
          features::kFileSystemAccessDirectoryIterationBlocklistCheck)) {
    return;
  }

  PathInfo child_path(dir_.GetPath().AppendASCII("blocked_path"));
  EXPECT_CALL(permission_context_,
              ConfirmSensitiveEntryAccess_(_, child_path, HandleType::kFile,
                                           UserAction::kNone,
                                           kBindingContext.frame_id, _))
      .WillOnce(base::test::RunOnceCallback<5>(SensitiveEntryResult::kAbort));

  base::test::TestFuture<
      blink::mojom::FileSystemAccessErrorPtr,
      mojo::PendingRemote<blink::mojom::FileSystemAccessFileHandle>>
      future;
  handle_->GetFile("blocked_path", /*create=*/false, future.GetCallback());
  EXPECT_EQ(future.Get<0>()->status,
            blink::mojom::FileSystemAccessStatus::kSecurityError);
}

TEST_F(FileSystemAccessDirectoryHandleImplTest, GetFile_NoReadAccess) {
  ASSERT_TRUE(base::WriteFile(dir_.GetPath().AppendASCII("filename"), "data"));

  base::test::TestFuture<
      blink::mojom::FileSystemAccessErrorPtr,
      mojo::PendingRemote<blink::mojom::FileSystemAccessFileHandle>>
      future;
  denied_handle_->GetFile("filename", /*create=*/false, future.GetCallback());
  EXPECT_EQ(future.Get<0>()->status,
            blink::mojom::FileSystemAccessStatus::kPermissionDenied);
  EXPECT_FALSE(future.Get<1>().is_valid());
}

TEST_F(FileSystemAccessDirectoryHandleImplTest, GetDirectory_NoReadAccess) {
  ASSERT_TRUE(base::CreateDirectory(dir_.GetPath().AppendASCII("dirname")));

  base::test::TestFuture<
      blink::mojom::FileSystemAccessErrorPtr,
      mojo::PendingRemote<blink::mojom::FileSystemAccessDirectoryHandle>>
      future;
  denied_handle_->GetDirectory("GetDirectory_NoReadAccess", /*create=*/false,
                               future.GetCallback());
  EXPECT_EQ(future.Get<0>()->status,
            blink::mojom::FileSystemAccessStatus::kPermissionDenied);
  EXPECT_FALSE(future.Get<1>().is_valid());
}

TEST_F(FileSystemAccessDirectoryHandleImplTest,
       GetEntries_SensitiveEntryAccessCheck) {
  if (!base::FeatureList::IsEnabled(
          features::kFileSystemAccessDirectoryIterationBlocklistCheck)) {
    return;
  }

  ASSERT_TRUE(
      base::WriteFile(dir_.GetPath().AppendASCII("allowed_file_path"), "data"));
  ASSERT_TRUE(
      base::WriteFile(dir_.GetPath().AppendASCII("blocked_file_path"), "data"));
  ASSERT_TRUE(base::CreateDirectory(dir_.GetPath().AppendASCII("subdir")));

  PathInfo allowed_file_path(dir_.GetPath().AppendASCII("allowed_file_path"));
  EXPECT_CALL(permission_context_,
              ConfirmSensitiveEntryAccess_(_, allowed_file_path,
                                           HandleType::kFile, UserAction::kNone,
                                           kBindingContext.frame_id, _))
      .WillOnce(base::test::RunOnceCallback<5>(SensitiveEntryResult::kAllowed));

  PathInfo blocked_file_path(dir_.GetPath().AppendASCII("blocked_file_path"));
  EXPECT_CALL(permission_context_,
              ConfirmSensitiveEntryAccess_(_, blocked_file_path,
                                           HandleType::kFile, UserAction::kNone,
                                           kBindingContext.frame_id, _))
      .WillOnce(base::test::RunOnceCallback<5>(SensitiveEntryResult::kAbort));

  // Sensitive entry access is not expected to perform on directories.
  EXPECT_CALL(permission_context_, ConfirmSensitiveEntryAccess_(
                                       _, _, HandleType::kDirectory, _, _, _))
      .Times(0);

  std::vector<blink::mojom::FileSystemAccessEntryPtr> entries;
  blink::mojom::FileSystemAccessErrorPtr result;
  base::RunLoop loop;
  mojo::PendingRemote<blink::mojom::FileSystemAccessDirectoryEntriesListener>
      listener;
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<TestFileSystemAccessDirectoryEntriesListener>(
          &entries, &result, loop.QuitClosure()),
      listener.InitWithNewPipeAndPassReceiver());
  handle_->GetEntries(std::move(listener));
  loop.Run();

  EXPECT_EQ(result->status, blink::mojom::FileSystemAccessStatus::kOk);
  std::vector<std::string> names;
  for (const auto& entry : entries) {
    names.push_back(entry->name);
  }
  EXPECT_THAT(names, testing::UnorderedElementsAreArray(
                         {"allowed_file_path", "subdir"}));
}

TEST_F(FileSystemAccessDirectoryHandleImplTest, GetEntries_NoReadAccess) {
  ASSERT_TRUE(base::WriteFile(dir_.GetPath().AppendASCII("filename"), "data"));
  if (base::FeatureList::IsEnabled(
          features::kFileSystemAccessDirectoryIterationBlocklistCheck)) {
    EXPECT_CALL(
        permission_context_,
        ConfirmSensitiveEntryAccess_(_, _, HandleType::kFile, UserAction::kNone,
                                     kBindingContext.frame_id, _))
        .Times(0);
  }

  std::vector<blink::mojom::FileSystemAccessEntryPtr> entries;
  blink::mojom::FileSystemAccessErrorPtr result;
  base::RunLoop loop;
  mojo::PendingRemote<blink::mojom::FileSystemAccessDirectoryEntriesListener>
      listener;
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<TestFileSystemAccessDirectoryEntriesListener>(
          &entries, &result, loop.QuitClosure()),
      listener.InitWithNewPipeAndPassReceiver());
  denied_handle_->GetEntries(std::move(listener));
  loop.Run();

  EXPECT_EQ(result->status,
            blink::mojom::FileSystemAccessStatus::kPermissionDenied);
  EXPECT_TRUE(entries.empty());
}

TEST_F(FileSystemAccessDirectoryHandleImplTest, Remove_NoWriteAccess) {
  base::FilePath dir = dir_.GetPath().AppendASCII("dirname");
  ASSERT_TRUE(base::CreateDirectory(dir));

  auto handle = GetHandleWithPermissions(dir, /*read=*/true, /*write=*/false);

  base::test::TestFuture<blink::mojom::FileSystemAccessErrorPtr> future;
  handle->Remove(
      /*recurse=*/false, future.GetCallback());
  EXPECT_EQ(future.Get()->status,
            blink::mojom::FileSystemAccessStatus::kPermissionDenied);
  EXPECT_TRUE(base::DirectoryExists(dir));
}

TEST_F(FileSystemAccessDirectoryHandleImplTest, Remove_HasWriteAccess) {
  base::FilePath dir = dir_.GetPath().AppendASCII("dirname");
  ASSERT_TRUE(base::CreateDirectory(dir));

  auto handle = GetHandleWithPermissions(dir, /*read=*/true, /*write=*/true);

  base::test::TestFuture<blink::mojom::FileSystemAccessErrorPtr> future;
  handle->Remove(
      /*recurse=*/false, future.GetCallback());
  EXPECT_EQ(future.Get()->status, blink::mojom::FileSystemAccessStatus::kOk);
  EXPECT_FALSE(base::DirectoryExists(dir));
}

TEST_F(FileSystemAccessDirectoryHandleImplTest, RemoveEntry) {
  base::FilePath dir = dir_.GetPath().AppendASCII("dirname");
  ASSERT_TRUE(base::CreateDirectory(dir));
  base::FilePath file;
  storage::FileSystemURL file_url;
  base::CreateTemporaryFileInDir(dir, &file);

  auto handle = GetHandleWithPermissions(dir, /*read=*/true, /*write=*/true);

  LockType exclusive_lock_type = manager_->GetExclusiveLockType();
  LockType wfs_siloed_lock_type = manager_->GetWFSSiloedLockType();

  // Calling removeEntry() on an unlocked file should succeed.
  {
    base::CreateTemporaryFileInDir(dir, &file);
    auto base_name = storage::FilePathToString(file.BaseName());
    EXPECT_EQ(handle->GetChildURL(base_name, &file_url)->file_error,
              base::File::Error::FILE_OK);

    base::test::TestFuture<blink::mojom::FileSystemAccessErrorPtr> future;
    handle->RemoveEntry(base_name,
                        /*recurse=*/false, future.GetCallback());
    EXPECT_EQ(future.Get()->status, blink::mojom::FileSystemAccessStatus::kOk);
    EXPECT_FALSE(base::PathExists(file));
    // The lock acquired during the operation should be released by the time the
    // callback runs.
    EXPECT_TRUE(TakeLockSync(kBindingContext, file_url, exclusive_lock_type));
  }

  // Acquire an exclusive lock on a file before removing to simulate when the
  // file has an open access handle. This should fail.
  {
    base::CreateTemporaryFileInDir(dir, &file);
    auto base_name = storage::FilePathToString(file.BaseName());
    EXPECT_EQ(handle->GetChildURL(base_name, &file_url)->file_error,
              base::File::Error::FILE_OK);
    auto lock = TakeLockSync(kBindingContext, file_url, exclusive_lock_type);
    EXPECT_TRUE(lock);

    base::test::TestFuture<blink::mojom::FileSystemAccessErrorPtr> future;
    handle->RemoveEntry(base_name,
                        /*recurse=*/false, future.GetCallback());
    EXPECT_EQ(
        future.Get()->status,
        blink::mojom::FileSystemAccessStatus::kNoModificationAllowedError);
    EXPECT_TRUE(base::PathExists(file));
  }

  // Acquire a wfs siloed lock on a file before removing to simulate when the
  // file has an open writable. This should also fail.
  {
    base::CreateTemporaryFileInDir(dir, &file);
    auto base_name = storage::FilePathToString(file.BaseName());
    EXPECT_EQ(handle->GetChildURL(base_name, &file_url)->file_error,
              base::File::Error::FILE_OK);
    auto lock = TakeLockSync(kBindingContext, file_url, wfs_siloed_lock_type);
    ASSERT_TRUE(lock);
    EXPECT_TRUE(lock->type() == wfs_siloed_lock_type);

    base::test::TestFuture<blink::mojom::FileSystemAccessErrorPtr> future;
    handle->RemoveEntry(base_name,
                        /*recurse=*/false, future.GetCallback());
    EXPECT_EQ(
        future.Get()->status,
        blink::mojom::FileSystemAccessStatus::kNoModificationAllowedError);
    EXPECT_TRUE(base::PathExists(file));
  }
}

TEST_F(FileSystemAccessDirectoryHandleImplTest, GetChildURL_CustomBucket) {
  base::FilePath dir = dir_.GetPath().AppendASCII("dirname");
  ASSERT_TRUE(base::CreateDirectory(dir));
  base::FilePath file;
  base::CreateTemporaryFileInDir(dir, &file);
  auto base_name = storage::FilePathToString(file.BaseName());
  storage::FileSystemURL file_url;

  auto default_handle =
      GetHandleWithPermissions(dir, /*read=*/true, /*write=*/true);
  EXPECT_EQ(default_handle->GetChildURL(base_name, &file_url)->file_error,
            base::File::Error::FILE_OK);
  EXPECT_FALSE(file_url.bucket());

  const auto custom_bucket = storage::BucketLocator(
      storage::BucketId(1),
      blink::StorageKey::CreateFromStringForTesting("http://example/"),
      blink::mojom::StorageType::kTemporary, /*is_default=*/false);
  auto custom_handle =
      GetHandleWithPermissions(dir, /*read=*/true, /*write=*/true,
                               /*url_bucket_override=*/custom_bucket);
  EXPECT_EQ(custom_handle->GetChildURL(base_name, &file_url)->file_error,
            base::File::Error::FILE_OK);
  EXPECT_TRUE(file_url.bucket());
  EXPECT_EQ(file_url.bucket().value(), custom_bucket);
}

}  // namespace content
