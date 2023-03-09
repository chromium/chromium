// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/file_system_access/file_system_access_directory_handle_impl.h"

#include <iterator>
#include <string>
#include <tuple>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "components/services/storage/public/cpp/buckets/bucket_locator.h"
#include "content/browser/file_system_access/features.h"
#include "content/browser/file_system_access/file_system_access_write_lock_manager.h"
#include "content/browser/file_system_access/fixed_file_system_access_permission_grant.h"
#include "content/browser/file_system_access/mock_file_system_access_permission_context.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/browser/test/test_file_system_context.h"
#include "storage/common/file_system/file_system_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/gurl.h"

namespace content {

using storage::FileSystemURL;
using testing::_;
using HandleType = FileSystemAccessPermissionContext::HandleType;
using SensitiveEntryResult =
    FileSystemAccessPermissionContext::SensitiveEntryResult;
using UserAction = FileSystemAccessPermissionContext::UserAction;
using WriteLockType = FileSystemAccessWriteLockManager::WriteLockType;

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

    auto url = manager_->CreateFileSystemURLFromPath(
        FileSystemAccessEntryFactory::PathType::kLocal, dir_.GetPath());
    handle_ = std::make_unique<FileSystemAccessDirectoryHandleImpl>(
        manager_.get(),
        FileSystemAccessManagerImpl::BindingContext(
            test_src_storage_key_, test_src_url_, /*worker_process_id=*/1),
        url,
        FileSystemAccessManagerImpl::SharedHandleState(allow_grant_,
                                                       allow_grant_));
    denied_handle_ = std::make_unique<FileSystemAccessDirectoryHandleImpl>(
        manager_.get(),
        FileSystemAccessManagerImpl::BindingContext(
            test_src_storage_key_, test_src_url_, /*worker_process_id=*/1),
        url,
        FileSystemAccessManagerImpl::SharedHandleState(deny_grant_,
                                                       deny_grant_));
  }

  void TearDown() override {
    manager_.reset();
    task_environment_.RunUntilIdle();
  }

  std::unique_ptr<FileSystemAccessDirectoryHandleImpl> GetHandleWithPermissions(
      const base::FilePath& path,
      bool read,
      bool write,
      const absl::optional<storage::BucketLocator> url_bucket_override =
          absl::nullopt) {
    auto url = manager_->CreateFileSystemURLFromPath(
        FileSystemAccessEntryFactory::PathType::kLocal, path);
    if (url_bucket_override.has_value()) {
      url.SetBucket(url_bucket_override.value());
    }
    auto handle = std::make_unique<FileSystemAccessDirectoryHandleImpl>(
        manager_.get(),
        FileSystemAccessManagerImpl::BindingContext(
            test_src_storage_key_, test_src_url_, /*worker_process_id=*/1),
        url,
        FileSystemAccessManagerImpl::SharedHandleState(
            /*read_grant=*/read ? allow_grant_ : deny_grant_,
            /*write_grant=*/write ? allow_grant_ : deny_grant_));
    return handle;
  }

 protected:
  const GURL test_src_url_ = GURL("http://example.com/foo");
  const blink::StorageKey test_src_storage_key_ =
      blink::StorageKey::CreateFromStringForTesting("http://example.com/foo");

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
          base::FilePath());
  scoped_refptr<FixedFileSystemAccessPermissionGrant> deny_grant_ =
      base::MakeRefCounted<FixedFileSystemAccessPermissionGrant>(
          FixedFileSystemAccessPermissionGrant::PermissionStatus::DENIED,
          base::FilePath());
  std::unique_ptr<FileSystemAccessDirectoryHandleImpl> handle_;
  std::unique_ptr<FileSystemAccessDirectoryHandleImpl> denied_handle_;
};

TEST_F(FileSystemAccessDirectoryHandleImplTest, IsSafePathComponent) {
  constexpr const char* kSafePathComponents[] = {
      "a", "a.txt", "a b.txt", "My Computer", ".a", "lnk.zip", "lnk", "a.local",
  };

  constexpr const char* kUnsafePathComponents[] = {
      "",
      ".",
      "..",
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
      "a/",
      "a\\",
      "a ",
      "a . .",
      " Computer",
      "My Computer.{a}",
      "My Computer.{20D04FE0-3AEA-1069-A2D8-08002B30309D}",
      "a\\a",
      "a.lnk",
      "a.url",
      "a/a",
      "C:\\",
      "C:/",
      "C:",
  };

  for (const char* component : kSafePathComponents) {
    EXPECT_TRUE(
        FileSystemAccessDirectoryHandleImpl::IsSafePathComponent(component))
        << component;
  }
  for (const char* component : kUnsafePathComponents) {
    EXPECT_FALSE(
        FileSystemAccessDirectoryHandleImpl::IsSafePathComponent(component))
        << component;
  }
}

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
  raw_ptr<std::vector<blink::mojom::FileSystemAccessEntryPtr>> entries_;
  raw_ptr<blink::mojom::FileSystemAccessErrorPtr> final_result_;
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

#if BUILDFLAG(IS_POSIX)
TEST_F(FileSystemAccessDirectoryHandleImplTest, GetFile_Symlink) {
  if (!base::FeatureList::IsEnabled(
          features::kFileSystemAccessDirectoryIterationSymbolicLinkCheck)) {
    return;
  }

  base::FilePath symlink_path(dir_.GetPath().AppendASCII("symlink"));
  base::FilePath target_path(dir_.GetPath().AppendASCII("target"));
  ASSERT_TRUE(base::CreateSymbolicLink(target_path, symlink_path));

  EXPECT_CALL(permission_context_,
              ConfirmSensitiveEntryAccess_(_, _, target_path, HandleType::kFile,
                                           UserAction::kNone, _, _))
      .WillOnce(base::test::RunOnceCallback<6>(SensitiveEntryResult::kAbort));

  base::test::TestFuture<
      blink::mojom::FileSystemAccessErrorPtr,
      mojo::PendingRemote<blink::mojom::FileSystemAccessFileHandle>>
      future;
  handle_->GetFile("symlink", /*create=*/false, future.GetCallback());
  EXPECT_EQ(future.Get<0>()->status,
            blink::mojom::FileSystemAccessStatus::kSecurityError);
}
#endif

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

TEST_F(FileSystemAccessDirectoryHandleImplTest, GetEntries_NoReadAccess) {
  ASSERT_TRUE(base::WriteFile(dir_.GetPath().AppendASCII("filename"), "data"));

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
    // The write lock acquired during the operation should be released by
    // the time the callback runs.
    EXPECT_TRUE(manager_->TakeWriteLock(file_url, WriteLockType::kExclusive));
  }

  // Acquire an exclusive lock on a file before removing to similate when the
  // file has an open access handle. This should fail.
  {
    base::CreateTemporaryFileInDir(dir, &file);
    auto base_name = storage::FilePathToString(file.BaseName());
    EXPECT_EQ(handle->GetChildURL(base_name, &file_url)->file_error,
              base::File::Error::FILE_OK);
    auto write_lock =
        manager_->TakeWriteLock(file_url, WriteLockType::kExclusive);
    EXPECT_TRUE(write_lock);

    base::test::TestFuture<blink::mojom::FileSystemAccessErrorPtr> future;
    handle->RemoveEntry(base_name,
                        /*recurse=*/false, future.GetCallback());
    EXPECT_EQ(
        future.Get()->status,
        blink::mojom::FileSystemAccessStatus::kNoModificationAllowedError);
    EXPECT_TRUE(base::PathExists(file));
  }

  // Acquire a shared lock on a file before removing to simulate when the file
  // has an open writable. This should also fail.
  {
    base::CreateTemporaryFileInDir(dir, &file);
    auto base_name = storage::FilePathToString(file.BaseName());
    EXPECT_EQ(handle->GetChildURL(base_name, &file_url)->file_error,
              base::File::Error::FILE_OK);
    auto write_lock = manager_->TakeWriteLock(file_url, WriteLockType::kShared);
    ASSERT_TRUE(write_lock);
    EXPECT_TRUE(write_lock->type() == WriteLockType::kShared);

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
