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
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "components/services/storage/public/cpp/buckets/bucket_locator.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/browser/file_system_access/features.h"
#include "content/browser/file_system_access/file_system_access_lock_manager.h"
#include "content/browser/file_system_access/fixed_file_system_access_permission_grant.h"
#include "content/browser/file_system_access/mock_file_system_access_permission_context.h"
#include "content/browser/file_system_access/mock_file_system_access_permission_grant.h"
#include "content/public/browser/file_system_access_permission_context.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/browser/test/test_file_system_context.h"
#include "storage/common/file_system/file_system_types.h"
#include "storage/common/file_system/file_system_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/features_generated.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/gurl.h"

namespace content {
namespace {
using storage::FileSystemURL;
using testing::_;
using HandleType = FileSystemAccessPermissionContext::HandleType;
using SensitiveEntryResult =
    FileSystemAccessPermissionContext::SensitiveEntryResult;
using UserAction = FileSystemAccessPermissionContext::UserAction;
using LockType = FileSystemAccessLockManager::LockType;
using blink::mojom::PermissionStatus;

// A matcher to check if a `RequestPermission()` call is successful and
// returns the expected permission status.
MATCHER_P(IsOkAndPermissionStatus, status, "") {
  if (arg.first->status != blink::mojom::FileSystemAccessStatus::kOk) {
    *result_listener << "FileSystemAccessStatus is " << arg.first->status;
    return false;
  }
  if (arg.second != status) {
    *result_listener << "PermissionStatus is " << arg.second;
    return false;
  }
  return true;
}
struct WriteModeTestParams {
  const char* test_name_suffix;
  bool is_feature_enabled;
};

constexpr WriteModeTestParams kTestParams[] = {
    {"WriteModeDisabled", false},
    {"WriteModeEnabled", true},
};

// A test implementation of FileSystemAccessDirectoryEntriesListener interface.
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

// Base class for FileSystemAccessDirectoryHandleImpl unit tests.
class FileSystemAccessDirectoryHandleImplTestBase : public testing::Test {
 public:
  FileSystemAccessDirectoryHandleImplTestBase()
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

// Base class for permission-related FileSystemAccessDirectoryHandleImpl unit
// tests.
class FileSystemAccessDirectoryHandleImplPermissionTestBase
    : public FileSystemAccessDirectoryHandleImplTestBase {
 public:
  void SetUp() override {
    FileSystemAccessDirectoryHandleImplTestBase::SetUp();
    mock_read_grant_ = base::MakeRefCounted<
        testing::StrictMock<MockFileSystemAccessPermissionGrant>>();
    mock_write_grant_ = base::MakeRefCounted<
        testing::StrictMock<MockFileSystemAccessPermissionGrant>>();
  }

 protected:
  // Creates a handle to a directory named "test_dir" in the test directory.
  std::unique_ptr<FileSystemAccessDirectoryHandleImpl> CreateHandle() {
    auto test_path = dir_.GetPath().AppendASCII("test_dir");
    EXPECT_TRUE(base::CreateDirectory(test_path));
    auto url = manager_->CreateFileSystemURLFromPath(PathInfo(test_path));
    return std::make_unique<FileSystemAccessDirectoryHandleImpl>(
        manager_.get(), kFrameBindingContext, url,
        FileSystemAccessManagerImpl::SharedHandleState(mock_read_grant_,
                                                       mock_write_grant_));
  }

  // Calls GetPermissionStatus() on the given `handle` and waits for the result.
  void GetPermissionStatus(FileSystemAccessDirectoryHandleImpl* handle,
                           blink::mojom::FileSystemAccessPermissionMode mode,
                           blink::mojom::PermissionStatus* out_status) {
    base::test::TestFuture<blink::mojom::PermissionStatus> future;
    handle->GetPermissionStatus(mode, future.GetCallback());
    *out_status = future.Get();
  }

  // Calls RequestPermission() on the given `handle` and waits for the result.
  std::pair<blink::mojom::FileSystemAccessErrorPtr,
            blink::mojom::PermissionStatus>
  RequestPermission(FileSystemAccessDirectoryHandleImpl* handle,
                    blink::mojom::FileSystemAccessPermissionMode mode) {
    base::test::TestFuture<blink::mojom::FileSystemAccessErrorPtr,
                           blink::mojom::PermissionStatus>
        future;
    handle->RequestPermission(mode, future.GetCallback());
    return future.Take();
  }

  // Sets up expectations for a call to `RequestPermission()` on a mock grant.
  void SetUpGrantExpectations(
      testing::StrictMock<MockFileSystemAccessPermissionGrant>& grant,
      PermissionStatus new_status,
      FileSystemAccessPermissionGrant::PermissionRequestOutcome outcome) {
    EXPECT_CALL(grant, GetStatus())
        .WillRepeatedly(testing::Return(PermissionStatus::ASK));
    EXPECT_CALL(
        grant,
        RequestPermission_(
            kFrameId,
            FileSystemAccessPermissionGrant::UserActivationState::kRequired, _))
        .WillOnce(
            testing::DoAll(testing::InvokeWithoutArgs([&grant, new_status]() {
                             EXPECT_CALL(grant, GetStatus())
                                 .WillRepeatedly(testing::Return(new_status));
                           }),
                           base::test::RunOnceCallback<2>(outcome)));
  }

  const int kProcessId = 1;
  const int kFrameRoutingId = 2;
  const GlobalRenderFrameHostId kFrameId{kProcessId, kFrameRoutingId};
  const FileSystemAccessManagerImpl::BindingContext kFrameBindingContext = {
      test_src_storage_key_, test_src_url_, kFrameId};

  scoped_refptr<testing::StrictMock<MockFileSystemAccessPermissionGrant>>
      mock_read_grant_;
  scoped_refptr<testing::StrictMock<MockFileSystemAccessPermissionGrant>>
      mock_write_grant_;
};

class FileSystemAccessDirectoryHandleImplTest
    : public FileSystemAccessDirectoryHandleImplTestBase {};

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

// Tests for `FileSystemAccessDirectoryHandleImpl::Remove()`.
class FileSystemAccessDirectoryHandleImplRemoveTest
    : public FileSystemAccessDirectoryHandleImplPermissionTestBase,
      public testing::WithParamInterface<WriteModeTestParams> {
 public:
  FileSystemAccessDirectoryHandleImplRemoveTest() {
    if (GetParam().is_feature_enabled) {
      scoped_feature_list_.InitWithFeatures(
          {blink::features::kFileSystemAccessWriteMode,
           blink::features::kFileSystemAccessRevokeReadOnRemove},
          {});
    } else {
      scoped_feature_list_.InitWithFeatures(
          {}, {blink::features::kFileSystemAccessWriteMode,
               blink::features::kFileSystemAccessRevokeReadOnRemove});
    }
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Verifies that `Remove` returns a permission denied error when the handle does
// not have write access.
TEST_P(FileSystemAccessDirectoryHandleImplRemoveTest, NoWriteAccess) {
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

// Verifies that `Remove` successfully removes a directory when the handle has
// write access.
TEST_P(FileSystemAccessDirectoryHandleImplRemoveTest, HasWriteAccess) {
  base::FilePath dir = dir_.GetPath().AppendASCII("dirname");
  ASSERT_TRUE(base::CreateDirectory(dir));

  if (GetParam().is_feature_enabled) {
    EXPECT_CALL(permission_context_, NotifyEntryRemoved(_, _)).Times(1);
  }

  auto handle = GetHandleWithPermissions(dir, /*read=*/true, /*write=*/true);

  base::test::TestFuture<blink::mojom::FileSystemAccessErrorPtr> future;
  handle->Remove(
      /*recurse=*/false, future.GetCallback());
  EXPECT_EQ(future.Get()->status, blink::mojom::FileSystemAccessStatus::kOk);
  EXPECT_FALSE(base::DirectoryExists(dir));
}

// Verifies that `Remove` with `recurse=true` successfully removes a non-empty
// directory.
TEST_P(FileSystemAccessDirectoryHandleImplRemoveTest, Recurse_NonEmpty) {
  base::FilePath dir = dir_.GetPath().AppendASCII("dirname");
  ASSERT_TRUE(base::CreateDirectory(dir));
  base::FilePath file = dir.AppendASCII("test_file.txt");
  ASSERT_TRUE(base::WriteFile(file, "test data"));
  if (GetParam().is_feature_enabled) {
    EXPECT_CALL(permission_context_, NotifyEntryRemoved(_, _)).Times(1);
  }

  auto handle = GetHandleWithPermissions(dir, /*read=*/true, /*write=*/true);

  base::test::TestFuture<blink::mojom::FileSystemAccessErrorPtr> future;
  handle->Remove(
      /*recurse=*/true, future.GetCallback());
  EXPECT_EQ(future.Get()->status, blink::mojom::FileSystemAccessStatus::kOk);
  EXPECT_FALSE(base::DirectoryExists(dir));
}

// Verifies that `Remove` with `recurse=false` fails to remove a non-empty
// directory.
TEST_P(FileSystemAccessDirectoryHandleImplRemoveTest, NoRecurse_NonEmpty) {
  base::FilePath dir = dir_.GetPath().AppendASCII("dirname");
  ASSERT_TRUE(base::CreateDirectory(dir));
  base::FilePath file = dir.AppendASCII("test_file.txt");
  ASSERT_TRUE(base::WriteFile(file, "test data"));

  auto handle = GetHandleWithPermissions(dir, /*read=*/true, /*write=*/true);

  base::test::TestFuture<blink::mojom::FileSystemAccessErrorPtr> future;
  handle->Remove(
      /*recurse=*/false, future.GetCallback());
  EXPECT_EQ(future.Get()->status,
            blink::mojom::FileSystemAccessStatus::kFileError);
  EXPECT_TRUE(base::DirectoryExists(dir));
}

// Verifies that `Remove` fails when the directory does not exist.
TEST_P(FileSystemAccessDirectoryHandleImplRemoveTest, NonExistent) {
  base::FilePath dir = dir_.GetPath().AppendASCII("non_existent_dir");
  auto handle = GetHandleWithPermissions(dir, /*read=*/true, /*write=*/true);

  base::test::TestFuture<blink::mojom::FileSystemAccessErrorPtr> future;
  handle->Remove(
      /*recurse=*/false, future.GetCallback());
  EXPECT_EQ(future.Get()->status,
            blink::mojom::FileSystemAccessStatus::kFileError);
}

// Verifies that `Remove()` requests the correct permissions before removing a
// directory. When `kFileSystemAccessWriteMode` is
// - disabled: it should request both read and write permissions.
// - enabled: it should only request write permission.
TEST_P(FileSystemAccessDirectoryHandleImplRemoveTest,
       RequestsCorrectPermissions) {
  auto handle = CreateHandle();
  auto test_dir_path = dir_.GetPath().AppendASCII("test_dir");

  if (GetParam().is_feature_enabled) {
    EXPECT_CALL(permission_context_, NotifyEntryRemoved(_, _)).Times(1);
  } else {
    SetUpGrantExpectations(*mock_read_grant_, PermissionStatus::GRANTED,
                           FileSystemAccessPermissionGrant::
                               PermissionRequestOutcome::kUserGranted);
  }
  SetUpGrantExpectations(
      *mock_write_grant_, PermissionStatus::GRANTED,
      FileSystemAccessPermissionGrant::PermissionRequestOutcome::kUserGranted);

  base::test::TestFuture<blink::mojom::FileSystemAccessErrorPtr> future;
  handle->Remove(/*recurse=*/false, future.GetCallback());
  EXPECT_EQ(future.Get()->status, blink::mojom::FileSystemAccessStatus::kOk);
  EXPECT_FALSE(base::PathExists(test_dir_path));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    FileSystemAccessDirectoryHandleImplRemoveTest,
    testing::ValuesIn(kTestParams),
    [](const testing::TestParamInfo<WriteModeTestParams>& info) {
      return info.param.test_name_suffix;
    });

// TODO(crbug.com/40276567): Verifies that `Remove` fails when called on a file
// handle.

// Tests for `FileSystemAccessDirectoryHandleImpl::RemoveEntry()`.
class FileSystemAccessDirectoryHandleImplRemoveEntryTest
    : public FileSystemAccessDirectoryHandleImplPermissionTestBase,
      public testing::WithParamInterface<WriteModeTestParams> {
 public:
  FileSystemAccessDirectoryHandleImplRemoveEntryTest() {
    if (GetParam().is_feature_enabled) {
      scoped_feature_list_.InitWithFeatures(
          {blink::features::kFileSystemAccessWriteMode,
           blink::features::kFileSystemAccessRevokeReadOnRemove},
          {});
    } else {
      scoped_feature_list_.InitWithFeatures(
          {}, {blink::features::kFileSystemAccessWriteMode,
               blink::features::kFileSystemAccessRevokeReadOnRemove});
    }
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Covers various scenarios for `RemoveEntry`, including removing an unlocked
// file, attempting to remove a file with an exclusive lock, and attempting to
// remove a file with a WFS siloed lock.
TEST_P(FileSystemAccessDirectoryHandleImplRemoveEntryTest, RemoveEntry) {
  base::FilePath dir = dir_.GetPath().AppendASCII("dirname");
  ASSERT_TRUE(base::CreateDirectory(dir));
  base::FilePath file;
  storage::FileSystemURL file_url;
  base::CreateTemporaryFileInDir(dir, &file);

  auto handle = GetHandleWithPermissions(dir, /*read=*/true, /*write=*/true);

  LockType exclusive_lock_type = manager_->GetExclusiveLockType();
  LockType wfs_siloed_lock_type = manager_->GetWFSSiloedLockType();

  if (GetParam().is_feature_enabled) {
    EXPECT_CALL(permission_context_, NotifyEntryRemoved(_, _)).Times(1);
  }

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

// Verifies that `RemoveEntry` fails with an invalid file name.
TEST_P(FileSystemAccessDirectoryHandleImplRemoveEntryTest, InvalidName) {
  base::test::TestFuture<blink::mojom::FileSystemAccessErrorPtr> future;
  handle_->RemoveEntry("invalid:name", /*recurse=*/false, future.GetCallback());

  EXPECT_EQ(future.Get()->status,
            blink::mojom::FileSystemAccessStatus::kInvalidArgument);
}

// Verifies that `RemoveEntry` fails when the entry does not exist.
TEST_P(FileSystemAccessDirectoryHandleImplRemoveEntryTest, NonExistent) {
  base::test::TestFuture<blink::mojom::FileSystemAccessErrorPtr> future;
  handle_->RemoveEntry("non_existent_file", /*recurse=*/false,
                       future.GetCallback());

  EXPECT_EQ(future.Get()->file_error, base::File::FILE_ERROR_NOT_FOUND);
}

// Verifies that `RemoveEntry` fails when the handle does not have write
// access.
TEST_P(FileSystemAccessDirectoryHandleImplRemoveEntryTest, NoWriteAccess) {
  base::FilePath file_path = dir_.GetPath().AppendASCII("test_file.txt");
  ASSERT_TRUE(base::WriteFile(file_path, "test data"));

  auto handle =
      GetHandleWithPermissions(dir_.GetPath(), /*read=*/true, /*write=*/false);

  base::test::TestFuture<blink::mojom::FileSystemAccessErrorPtr> future;
  handle->RemoveEntry("test_file.txt", /*recurse=*/false, future.GetCallback());

  EXPECT_EQ(future.Get()->status,
            blink::mojom::FileSystemAccessStatus::kPermissionDenied);
  EXPECT_TRUE(base::PathExists(file_path));
}

// Verifies that `RemoveEntry` with `recurse=true` successfully removes a
// non-empty directory.
TEST_P(FileSystemAccessDirectoryHandleImplRemoveEntryTest,
       Recurse_NonEmptyDirectory) {
  base::FilePath subdir_path = dir_.GetPath().AppendASCII("subdir");
  ASSERT_TRUE(base::CreateDirectory(subdir_path));
  base::FilePath file_path = subdir_path.AppendASCII("test_file.txt");
  ASSERT_TRUE(base::WriteFile(file_path, "test data"));
  if (GetParam().is_feature_enabled) {
    EXPECT_CALL(permission_context_, NotifyEntryRemoved(_, _)).Times(1);
  }

  base::test::TestFuture<blink::mojom::FileSystemAccessErrorPtr> future;
  handle_->RemoveEntry("subdir", /*recurse=*/true, future.GetCallback());

  EXPECT_EQ(future.Get()->status, blink::mojom::FileSystemAccessStatus::kOk);
  EXPECT_FALSE(base::DirectoryExists(subdir_path));
}

// Verifies that `RemoveEntry` with `recurse=false` fails to remove a non-empty
// directory.
TEST_P(FileSystemAccessDirectoryHandleImplRemoveEntryTest,
       NoRecurse_NonEmptyDirectory) {
  base::FilePath subdir_path = dir_.GetPath().AppendASCII("subdir");
  ASSERT_TRUE(base::CreateDirectory(subdir_path));
  base::FilePath file_path = subdir_path.AppendASCII("test_file.txt");
  ASSERT_TRUE(base::WriteFile(file_path, "test data"));

  base::test::TestFuture<blink::mojom::FileSystemAccessErrorPtr> future;
  handle_->RemoveEntry("subdir", /*recurse=*/false, future.GetCallback());

  EXPECT_EQ(future.Get()->status,
            blink::mojom::FileSystemAccessStatus::kFileError);
  EXPECT_TRUE(base::DirectoryExists(subdir_path));
}

// Verifies that `RemoveEntry()` requests the correct permissions before
// removing an entry. When `kFileSystemAccessWriteMode` is
// - disabled: it should request both read and write permissions.
// - enabled: it should only request write permission.
TEST_P(FileSystemAccessDirectoryHandleImplRemoveEntryTest,
       RequestsCorrectPermissions) {
  auto handle = CreateHandle();
  base::FilePath test_dir = dir_.GetPath().AppendASCII("test_dir");
  base::FilePath file_path = test_dir.AppendASCII("test_file.txt");
  ASSERT_TRUE(base::WriteFile(file_path, "test data"));

  if (GetParam().is_feature_enabled) {
    EXPECT_CALL(permission_context_, NotifyEntryRemoved(_, _)).Times(1);
  } else {
    SetUpGrantExpectations(*mock_read_grant_, PermissionStatus::GRANTED,
                           FileSystemAccessPermissionGrant::
                               PermissionRequestOutcome::kUserGranted);
  }
  SetUpGrantExpectations(
      *mock_write_grant_, PermissionStatus::GRANTED,
      FileSystemAccessPermissionGrant::PermissionRequestOutcome::kUserGranted);

  base::test::TestFuture<blink::mojom::FileSystemAccessErrorPtr> future;
  handle->RemoveEntry("test_file.txt", /*recurse=*/false, future.GetCallback());
  EXPECT_EQ(future.Get()->status, blink::mojom::FileSystemAccessStatus::kOk);
  EXPECT_FALSE(base::PathExists(file_path));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    FileSystemAccessDirectoryHandleImplRemoveEntryTest,
    testing::ValuesIn(kTestParams),
    [](const testing::TestParamInfo<WriteModeTestParams>& info) {
      return info.param.test_name_suffix;
    });

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
      /*is_default=*/false);
  auto custom_handle =
      GetHandleWithPermissions(dir, /*read=*/true, /*write=*/true,
                               /*url_bucket_override=*/custom_bucket);
  EXPECT_EQ(custom_handle->GetChildURL(base_name, &file_url)->file_error,
            base::File::Error::FILE_OK);
  EXPECT_TRUE(file_url.bucket());
  EXPECT_EQ(file_url.bucket().value(), custom_bucket);
}

// Tests for `FileSystemAccessDirectoryHandleImpl::GetPermissionStatus()`.
class FileSystemAccessDirectoryHandleImplGetPermissionStatusTest
    : public FileSystemAccessDirectoryHandleImplPermissionTestBase {};

TEST_F(FileSystemAccessDirectoryHandleImplGetPermissionStatusTest, ReadHandle) {
  auto test_path = dir_.GetPath().AppendASCII("test_dir");
  ASSERT_TRUE(base::CreateDirectory(test_path));

  auto read_only_handle =
      GetHandleWithPermissions(test_path, /*read=*/true, /*write=*/false);

  blink::mojom::PermissionStatus status;
  GetPermissionStatus(read_only_handle.get(),
                      blink::mojom::FileSystemAccessPermissionMode::kRead,
                      &status);
  EXPECT_EQ(status, blink::mojom::PermissionStatus::GRANTED);

  {
    base::test::ScopedFeatureList features;
    features.InitAndEnableFeature(blink::features::kFileSystemAccessWriteMode);
    GetPermissionStatus(read_only_handle.get(),
                        blink::mojom::FileSystemAccessPermissionMode::kWrite,
                        &status);
    EXPECT_EQ(status, blink::mojom::PermissionStatus::DENIED);
  }

  GetPermissionStatus(read_only_handle.get(),
                      blink::mojom::FileSystemAccessPermissionMode::kReadWrite,
                      &status);
  EXPECT_EQ(status, blink::mojom::PermissionStatus::DENIED);
}

TEST_F(FileSystemAccessDirectoryHandleImplGetPermissionStatusTest,
       WriteHandle) {
  auto test_path = dir_.GetPath().AppendASCII("test_dir");
  ASSERT_TRUE(base::CreateDirectory(test_path));

  auto write_handle =
      GetHandleWithPermissions(test_path, /*read=*/false, /*write=*/true);

  blink::mojom::PermissionStatus status;
  GetPermissionStatus(write_handle.get(),
                      blink::mojom::FileSystemAccessPermissionMode::kRead,
                      &status);
  EXPECT_EQ(status, blink::mojom::PermissionStatus::DENIED);

  {
    base::test::ScopedFeatureList features;
    features.InitAndEnableFeature(blink::features::kFileSystemAccessWriteMode);
    GetPermissionStatus(write_handle.get(),
                        blink::mojom::FileSystemAccessPermissionMode::kWrite,
                        &status);
    EXPECT_EQ(status, blink::mojom::PermissionStatus::GRANTED);
  }

  GetPermissionStatus(write_handle.get(),
                      blink::mojom::FileSystemAccessPermissionMode::kReadWrite,
                      &status);
  EXPECT_EQ(status, blink::mojom::PermissionStatus::DENIED);
}

TEST_F(FileSystemAccessDirectoryHandleImplGetPermissionStatusTest,
       ReadWriteHandle) {
  auto test_path = dir_.GetPath().AppendASCII("test_dir");
  ASSERT_TRUE(base::CreateDirectory(test_path));

  auto read_write_handle =
      GetHandleWithPermissions(test_path, /*read=*/true, /*write=*/true);

  blink::mojom::PermissionStatus status;
  GetPermissionStatus(read_write_handle.get(),
                      blink::mojom::FileSystemAccessPermissionMode::kRead,
                      &status);
  EXPECT_EQ(status, blink::mojom::PermissionStatus::GRANTED);

  {
    base::test::ScopedFeatureList features;
    features.InitAndEnableFeature(blink::features::kFileSystemAccessWriteMode);
    GetPermissionStatus(read_write_handle.get(),
                        blink::mojom::FileSystemAccessPermissionMode::kWrite,
                        &status);
    EXPECT_EQ(status, blink::mojom::PermissionStatus::GRANTED);
  }

  GetPermissionStatus(read_write_handle.get(),
                      blink::mojom::FileSystemAccessPermissionMode::kReadWrite,
                      &status);
  EXPECT_EQ(status, blink::mojom::PermissionStatus::GRANTED);
}

// Tests for `FileSystemAccessDirectoryHandleImpl::RequestPermission()`.
class FileSystemAccessDirectoryHandleImplRequestPermissionTest
    : public FileSystemAccessDirectoryHandleImplPermissionTestBase {};

TEST_F(FileSystemAccessDirectoryHandleImplRequestPermissionTest,
       RequestRead_Granted) {
  auto handle = CreateHandle();

  SetUpGrantExpectations(
      *mock_read_grant_, PermissionStatus::GRANTED,
      FileSystemAccessPermissionGrant::PermissionRequestOutcome::kUserGranted);

  EXPECT_THAT(
      RequestPermission(handle.get(),
                        blink::mojom::FileSystemAccessPermissionMode::kRead),
      IsOkAndPermissionStatus(PermissionStatus::GRANTED));
}

TEST_F(FileSystemAccessDirectoryHandleImplRequestPermissionTest,
       RequestRead_Denied) {
  auto handle = CreateHandle();

  SetUpGrantExpectations(
      *mock_read_grant_, PermissionStatus::DENIED,
      FileSystemAccessPermissionGrant::PermissionRequestOutcome::kUserDenied);

  EXPECT_THAT(
      RequestPermission(handle.get(),
                        blink::mojom::FileSystemAccessPermissionMode::kRead),
      IsOkAndPermissionStatus(PermissionStatus::DENIED));
}

TEST_F(FileSystemAccessDirectoryHandleImplRequestPermissionTest,
       RequestReadWrite_Granted) {
  auto handle = CreateHandle();

  SetUpGrantExpectations(
      *mock_read_grant_, PermissionStatus::GRANTED,
      FileSystemAccessPermissionGrant::PermissionRequestOutcome::kUserGranted);
  SetUpGrantExpectations(
      *mock_write_grant_, PermissionStatus::GRANTED,
      FileSystemAccessPermissionGrant::PermissionRequestOutcome::kUserGranted);

  EXPECT_THAT(RequestPermission(
                  handle.get(),
                  blink::mojom::FileSystemAccessPermissionMode::kReadWrite),
              IsOkAndPermissionStatus(PermissionStatus::GRANTED));
}

TEST_F(FileSystemAccessDirectoryHandleImplRequestPermissionTest,
       RequestReadWrite_ReadDenied) {
  auto handle = CreateHandle();

  SetUpGrantExpectations(
      *mock_read_grant_, PermissionStatus::DENIED,
      FileSystemAccessPermissionGrant::PermissionRequestOutcome::kUserDenied);
  SetUpGrantExpectations(
      *mock_write_grant_, PermissionStatus::GRANTED,
      FileSystemAccessPermissionGrant::PermissionRequestOutcome::kUserGranted);

  EXPECT_THAT(RequestPermission(
                  handle.get(),
                  blink::mojom::FileSystemAccessPermissionMode::kReadWrite),
              IsOkAndPermissionStatus(PermissionStatus::DENIED));
}

TEST_F(FileSystemAccessDirectoryHandleImplRequestPermissionTest,
       RequestReadWrite_WriteDenied) {
  auto handle = CreateHandle();

  EXPECT_CALL(*mock_read_grant_, GetStatus())
      .WillRepeatedly(testing::Return(PermissionStatus::GRANTED));
  SetUpGrantExpectations(
      *mock_write_grant_, PermissionStatus::DENIED,
      FileSystemAccessPermissionGrant::PermissionRequestOutcome::kUserDenied);

  EXPECT_THAT(RequestPermission(
                  handle.get(),
                  blink::mojom::FileSystemAccessPermissionMode::kReadWrite),
              IsOkAndPermissionStatus(PermissionStatus::DENIED));
}

TEST_F(FileSystemAccessDirectoryHandleImplRequestPermissionTest,
       RequestWrite_Granted) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(blink::features::kFileSystemAccessWriteMode);
  auto handle = CreateHandle();

  SetUpGrantExpectations(
      *mock_write_grant_, PermissionStatus::GRANTED,
      FileSystemAccessPermissionGrant::PermissionRequestOutcome::kUserGranted);

  EXPECT_THAT(
      RequestPermission(handle.get(),
                        blink::mojom::FileSystemAccessPermissionMode::kWrite),
      IsOkAndPermissionStatus(PermissionStatus::GRANTED));
}

TEST_F(FileSystemAccessDirectoryHandleImplRequestPermissionTest,
       RequestWrite_Denied) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(blink::features::kFileSystemAccessWriteMode);
  auto handle = CreateHandle();

  SetUpGrantExpectations(
      *mock_write_grant_, PermissionStatus::DENIED,
      FileSystemAccessPermissionGrant::PermissionRequestOutcome::kUserDenied);

  EXPECT_THAT(
      RequestPermission(handle.get(),
                        blink::mojom::FileSystemAccessPermissionMode::kWrite),
      IsOkAndPermissionStatus(PermissionStatus::DENIED));
}

}  // namespace content
