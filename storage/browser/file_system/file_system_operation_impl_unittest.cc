// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "storage/browser/file_system/file_system_operation_impl.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <utility>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/services/filesystem/public/mojom/types.mojom.h"
#include "storage/browser/blob/shareable_file_reference.h"
#include "storage/browser/file_system/copy_or_move_hook_delegate.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_file_util.h"
#include "storage/browser/file_system/file_system_operation_context.h"
#include "storage/browser/file_system/file_system_operation_runner.h"
#include "storage/browser/file_system/sandbox_file_system_backend.h"
#include "storage/browser/quota/quota_manager.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/browser/test/async_file_test_helper.h"
#include "storage/browser/test/mock_file_change_observer.h"
#include "storage/browser/test/mock_file_update_observer.h"
#include "storage/browser/test/mock_quota_manager.h"
#include "storage/browser/test/mock_quota_manager_proxy.h"
#include "storage/browser/test/mock_special_storage_policy.h"
#include "storage/browser/test/sandbox_file_system_test_helper.h"
#include "storage/common/file_system/file_system_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace storage {

// Test class for FileSystemOperationImpl.
class FileSystemOperationImplTest : public testing::Test {
 public:
  FileSystemOperationImplTest()
      : special_storage_policy_(
            base::MakeRefCounted<MockSpecialStoragePolicy>()),
        task_environment_(base::test::TaskEnvironment::MainThreadType::IO) {}

  FileSystemOperationImplTest(const FileSystemOperationImplTest&) = delete;
  FileSystemOperationImplTest& operator=(const FileSystemOperationImplTest&) =
      delete;

  void SetUp() override {
    EXPECT_TRUE(base_.CreateUniqueTempDir());
    change_observers_ = MockFileChangeObserver::CreateList(&change_observer_);
    update_observers_ = MockFileUpdateObserver::CreateList(&update_observer_);

    base::FilePath base_dir = base_.GetPath().AppendASCII("filesystem");
    quota_manager_ = base::MakeRefCounted<MockQuotaManager>(
        /* is_incognito= */ false, base_dir,
        base::SingleThreadTaskRunner::GetCurrentDefault(),
        special_storage_policy_);
    quota_manager_proxy_ = base::MakeRefCounted<MockQuotaManagerProxy>(
        quota_manager(), base::SingleThreadTaskRunner::GetCurrentDefault());
    sandbox_file_system_.SetUp(base_dir, quota_manager_proxy_.get());
    sandbox_file_system_.AddFileChangeObserver(&change_observer_);
    sandbox_file_system_.AddFileUpdateObserver(&update_observer_);
    update_observer_.Disable();
  }

  void TearDown() override {
    quota_manager_ = nullptr;
    quota_manager_proxy_ = nullptr;
    sandbox_file_system_.TearDown();
    task_environment_.RunUntilIdle();
  }

  FileSystemOperationRunner* operation_runner() {
    return sandbox_file_system_.operation_runner();
  }

  const base::File::Info& info() const { return info_; }
  const base::FilePath& path() const { return path_; }
  const std::vector<filesystem::mojom::DirectoryEntry>& entries() const {
    return entries_;
  }

  const ShareableFileReference* shareable_file_ref() const {
    return shareable_file_ref_.get();
  }

  MockQuotaManager* quota_manager() {
    return static_cast<MockQuotaManager*>(quota_manager_.get());
  }

  MockQuotaManagerProxy* quota_manager_proxy() {
    return static_cast<MockQuotaManagerProxy*>(quota_manager_proxy_.get());
  }

  FileSystemFileUtil* file_util() { return sandbox_file_system_.file_util(); }

  MockFileChangeObserver* change_observer() { return &change_observer_; }

  std::unique_ptr<FileSystemOperationContext> NewContext() {
    auto context = sandbox_file_system_.NewOperationContext();
    // Grant enough quota for all test cases.
    context->set_allowed_bytes_growth(1000000);
    return context;
  }

  FileSystemURL URLForPath(const std::string& path) const {
    return sandbox_file_system_.CreateURLFromUTF8(path);
  }

  base::FilePath PlatformPath(const std::string& path) {
    return sandbox_file_system_.GetLocalPath(
        base::FilePath::FromUTF8Unsafe(path));
  }

  bool FileExists(const std::string& path) {
    return AsyncFileTestHelper::FileExists(
        sandbox_file_system_.file_system_context(), URLForPath(path),
        AsyncFileTestHelper::kDontCheckSize);
  }

  bool DirectoryExists(const std::string& path) {
    return AsyncFileTestHelper::DirectoryExists(
        sandbox_file_system_.file_system_context(), URLForPath(path));
  }

  FileSystemURL CreateFile(const std::string& path) {
    FileSystemURL url = URLForPath(path);
    bool created = false;
    EXPECT_EQ(base::File::FILE_OK,
              file_util()->EnsureFileExists(NewContext().get(), url, &created));
    EXPECT_TRUE(created);
    return url;
  }

  FileSystemURL CreateDirectory(const std::string& path) {
    FileSystemURL url = URLForPath(path);
    EXPECT_EQ(base::File::FILE_OK,
              file_util()->CreateDirectory(NewContext().get(), url,
                                           false /* exclusive */, true));
    return url;
  }

  int64_t GetFileSize(const std::string& path) {
    base::File::Info info;
    EXPECT_TRUE(base::GetFileInfo(PlatformPath(path), &info));
    return info.size;
  }

  // Callbacks for recording test results.
  FileSystemOperation::StatusCallback RecordStatusCallback(
      base::OnceClosure closure,
      base::File::Error* status) {
    return base::BindOnce(&FileSystemOperationImplTest::DidFinish,
                          weak_factory_.GetWeakPtr(), std::move(closure),
                          status);
  }

  FileSystemOperation::ReadDirectoryCallback RecordReadDirectoryCallback(
      base::RepeatingClosure closure,
      base::File::Error* status) {
    return base::BindRepeating(&FileSystemOperationImplTest::DidReadDirectory,
                               weak_factory_.GetWeakPtr(), std::move(closure),
                               status);
  }

  FileSystemOperation::GetMetadataCallback RecordMetadataCallback(
      base::OnceClosure closure,
      base::File::Error* status) {
    return base::BindOnce(&FileSystemOperationImplTest::DidGetMetadata,
                          weak_factory_.GetWeakPtr(), std::move(closure),
                          status);
  }

  FileSystemOperation::SnapshotFileCallback RecordSnapshotFileCallback(
      base::OnceClosure closure,
      base::File::Error* status) {
    return base::BindOnce(&FileSystemOperationImplTest::DidCreateSnapshotFile,
                          weak_factory_.GetWeakPtr(), std::move(closure),
                          status);
  }

  void DidFinish(base::OnceClosure closure,
                 base::File::Error* status,
                 base::File::Error actual) {
    *status = actual;
    std::move(closure).Run();
  }

  void DidReadDirectory(base::RepeatingClosure closure,
                        base::File::Error* status,
                        base::File::Error actual,
                        std::vector<filesystem::mojom::DirectoryEntry> entries,
                        bool /* has_more */) {
    entries_ = std::move(entries);
    *status = actual;
    closure.Run();
  }

  void DidGetMetadata(base::OnceClosure closure,
                      base::File::Error* status,
                      base::File::Error actual,
                      const base::File::Info& info) {
    info_ = info;
    *status = actual;
    std::move(closure).Run();
  }

  void DidCreateSnapshotFile(
      base::OnceClosure closure,
      base::File::Error* status,
      base::File::Error actual,
      const base::File::Info& info,
      const base::FilePath& platform_path,
      scoped_refptr<ShareableFileReference> shareable_file_ref) {
    info_ = info;
    path_ = platform_path;
    *status = actual;
    shareable_file_ref_ = std::move(shareable_file_ref);
    std::move(closure).Run();
  }

  int64_t GetDataSizeOnDisk() {
    return sandbox_file_system_.ComputeCurrentStorageKeyUsage() -
           sandbox_file_system_.ComputeCurrentDirectoryDatabaseUsage();
  }

  void GetUsageAndQuota(int64_t* usage, int64_t* quota) {
    blink::mojom::QuotaStatusCode status =
        AsyncFileTestHelper::GetUsageAndQuota(
            quota_manager_->proxy(), sandbox_file_system_.storage_key(),
            sandbox_file_system_.type(), usage, quota);
    task_environment_.RunUntilIdle();
    ASSERT_EQ(blink::mojom::QuotaStatusCode::kOk, status);
  }

  int64_t ComputePathCost(const FileSystemURL& url) {
    int64_t base_usage;
    GetUsageAndQuota(&base_usage, nullptr);

    AsyncFileTestHelper::CreateFile(sandbox_file_system_.file_system_context(),
                                    url);
    EXPECT_EQ(base::File::FILE_OK, Remove(url, false /* recursive */));

    change_observer()->ResetCount();

    int64_t total_usage;
    GetUsageAndQuota(&total_usage, nullptr);
    return total_usage - base_usage;
  }

  void GrantQuotaForCurrentUsage() {
    int64_t usage;
    GetUsageAndQuota(&usage, nullptr);
    quota_manager()->SetQuota(sandbox_file_system_.storage_key(),
                              sandbox_file_system_.storage_type(), usage);
  }

  int64_t GetUsage() {
    int64_t usage = 0;
    GetUsageAndQuota(&usage, nullptr);
    return usage;
  }

  void AddQuota(int64_t quota_delta) {
    int64_t quota;
    GetUsageAndQuota(nullptr, &quota);
    quota_manager()->SetQuota(sandbox_file_system_.storage_key(),
                              sandbox_file_system_.storage_type(),
                              quota + quota_delta);
  }

  base::File::Error Move(const FileSystemURL& src,
                         const FileSystemURL& dest,
                         FileSystemOperation::CopyOrMoveOptionSet options) {
    base::File::Error status;
    base::RunLoop run_loop;
    update_observer_.Enable();
    operation_runner()->Move(
        src, dest, options, FileSystemOperation::ERROR_BEHAVIOR_ABORT,
        std::make_unique<CopyOrMoveHookDelegate>(),
        RecordStatusCallback(run_loop.QuitClosure(), &status));
    run_loop.Run();
    update_observer_.Disable();
    return status;
  }

  base::File::Error Copy(const FileSystemURL& src,
                         const FileSystemURL& dest,
                         FileSystemOperation::CopyOrMoveOptionSet options) {
    base::File::Error status;
    base::RunLoop run_loop;
    update_observer_.Enable();
    operation_runner()->Copy(
        src, dest, options, FileSystemOperation::ERROR_BEHAVIOR_ABORT,
        std::make_unique<CopyOrMoveHookDelegate>(),
        RecordStatusCallback(run_loop.QuitClosure(), &status));
    run_loop.Run();
    update_observer_.Disable();
    return status;
  }

  base::File::Error CopyInForeignFile(const base::FilePath& src,
                                      const FileSystemURL& dest) {
    base::File::Error status;
    base::RunLoop run_loop;
    update_observer_.Enable();
    operation_runner()->CopyInForeignFile(
        src, dest, RecordStatusCallback(run_loop.QuitClosure(), &status));
    run_loop.Run();
    update_observer_.Disable();
    return status;
  }

  base::File::Error Truncate(const FileSystemURL& url, int size) {
    base::File::Error status;
    base::RunLoop run_loop;
    update_observer_.Enable();
    operation_runner()->Truncate(
        url, size, RecordStatusCallback(run_loop.QuitClosure(), &status));
    run_loop.Run();
    update_observer_.Disable();
    return status;
  }

  base::File::Error CreateFile(const FileSystemURL& url, bool exclusive) {
    base::File::Error status;
    base::RunLoop run_loop;
    update_observer_.Enable();
    operation_runner()->CreateFile(
        url, exclusive, RecordStatusCallback(run_loop.QuitClosure(), &status));
    run_loop.Run();
    update_observer_.Disable();
    return status;
  }

  base::File::Error Remove(const FileSystemURL& url, bool recursive) {
    base::File::Error status;
    base::RunLoop run_loop;
    update_observer_.Enable();
    operation_runner()->Remove(
        url, recursive, RecordStatusCallback(run_loop.QuitClosure(), &status));
    run_loop.Run();
    update_observer_.Disable();
    return status;
  }

  base::File::Error CreateDirectory(const FileSystemURL& url,
                                    bool exclusive,
                                    bool recursive) {
    base::File::Error status;
    base::RunLoop run_loop;
    update_observer_.Enable();
    operation_runner()->CreateDirectory(
        url, exclusive, recursive,
        RecordStatusCallback(run_loop.QuitClosure(), &status));
    run_loop.Run();
    update_observer_.Disable();
    return status;
  }

  base::File::Error GetMetadata(
      const FileSystemURL& url,
      FileSystemOperation::GetMetadataFieldSet fields) {
    base::File::Error status;
    base::RunLoop run_loop;
    update_observer_.Enable();
    operation_runner()->GetMetadata(
        url, fields, RecordMetadataCallback(run_loop.QuitClosure(), &status));
    run_loop.Run();
    update_observer_.Disable();
    return status;
  }

  base::File::Error ReadDirectory(const FileSystemURL& url) {
    base::File::Error status;
    base::RunLoop run_loop;
    update_observer_.Enable();
    operation_runner()->ReadDirectory(
        url, RecordReadDirectoryCallback(run_loop.QuitClosure(), &status));
    run_loop.Run();
    update_observer_.Disable();
    return status;
  }

  base::File::Error CreateSnapshotFile(const FileSystemURL& url) {
    base::File::Error status;
    base::RunLoop run_loop;
    update_observer_.Enable();
    operation_runner()->CreateSnapshotFile(
        url, RecordSnapshotFileCallback(run_loop.QuitClosure(), &status));
    run_loop.Run();
    update_observer_.Disable();
    return status;
  }

  base::File::Error FileExists(const FileSystemURL& url) {
    base::File::Error status;
    base::RunLoop run_loop;
    update_observer_.Enable();
    operation_runner()->FileExists(
        url, RecordStatusCallback(run_loop.QuitClosure(), &status));
    run_loop.Run();
    update_observer_.Disable();
    return status;
  }

  base::File::Error DirectoryExists(const FileSystemURL& url) {
    base::File::Error status;
    base::RunLoop run_loop;
    update_observer_.Enable();
    operation_runner()->DirectoryExists(
        url, RecordStatusCallback(run_loop.QuitClosure(), &status));
    run_loop.Run();
    update_observer_.Disable();
    return status;
  }

  base::File::Error TouchFile(const FileSystemURL& url,
                              const base::Time& last_access_time,
                              const base::Time& last_modified_time) {
    base::File::Error status;
    base::RunLoop run_loop;
    update_observer_.Enable();
    operation_runner()->TouchFile(
        url, last_access_time, last_modified_time,
        RecordStatusCallback(run_loop.QuitClosure(), &status));
    run_loop.Run();
    update_observer_.Disable();
    return status;
  }

 protected:
  scoped_refptr<MockSpecialStoragePolicy> special_storage_policy_;

  // Common temp base for nondestructive uses.
  base::ScopedTempDir base_;

  base::test::TaskEnvironment task_environment_;

  // These are mocks.
  scoped_refptr<QuotaManager> quota_manager_;
  scoped_refptr<QuotaManagerProxy> quota_manager_proxy_;

  SandboxFileSystemTestHelper sandbox_file_system_;

  // For post-operation status.
  base::File::Info info_;
  base::FilePath path_;
  std::vector<filesystem::mojom::DirectoryEntry> entries_;
  scoped_refptr<ShareableFileReference> shareable_file_ref_;

  MockFileChangeObserver change_observer_;
  ChangeObserverList change_observers_;
  MockFileUpdateObserver update_observer_;
  UpdateObserverList update_observers_;

  base::WeakPtrFactory<FileSystemOperationImplTest> weak_factory_{this};
};

TEST_F(FileSystemOperationImplTest, TestMoveFailureSrcDoesntExist) {
  change_observer()->ResetCount();
  EXPECT_EQ(base::File::FILE_ERROR_NOT_FOUND,
            Move(URLForPath("a"), URLForPath("b"),
                 FileSystemOperation::CopyOrMoveOptionSet()));
  EXPECT_TRUE(change_observer()->HasNoChange());
}

TEST_F(FileSystemOperationImplTest, TestMoveFailureContainsPath) {
  FileSystemURL src_dir(CreateDirectory("src"));
  FileSystemURL dest_dir(CreateDirectory("src/dest"));

  EXPECT_EQ(
      base::File::FILE_ERROR_INVALID_OPERATION,
      Move(src_dir, dest_dir, FileSystemOperation::CopyOrMoveOptionSet()));
  EXPECT_TRUE(change_observer()->HasNoChange());
}

TEST_F(FileSystemOperationImplTest, TestMoveFailureSrcDirExistsDestFile) {
  // Src exists and is dir. Dest is a file.
  FileSystemURL src_dir(CreateDirectory("src"));
  FileSystemURL dest_dir(CreateDirectory("dest"));
  FileSystemURL dest_file(CreateFile("dest/file"));

  EXPECT_EQ(
      base::File::FILE_ERROR_INVALID_OPERATION,
      Move(src_dir, dest_file, FileSystemOperation::CopyOrMoveOptionSet()));
  EXPECT_TRUE(change_observer()->HasNoChange());
}

TEST_F(FileSystemOperationImplTest,
       TestMoveFailureSrcFileExistsDestNonEmptyDir) {
  // Src exists and is a directory. Dest is a non-empty directory.
  FileSystemURL src_dir(CreateDirectory("src"));
  FileSystemURL dest_dir(CreateDirectory("dest"));
  FileSystemURL dest_file(CreateFile("dest/file"));

  EXPECT_EQ(
      base::File::FILE_ERROR_NOT_EMPTY,
      Move(src_dir, dest_dir, FileSystemOperation::CopyOrMoveOptionSet()));
  EXPECT_TRUE(change_observer()->HasNoChange());
}

TEST_F(FileSystemOperationImplTest, TestMoveFailureSrcFileExistsDestDir) {
  // Src exists and is a file. Dest is a directory.
  FileSystemURL src_dir(CreateDirectory("src"));
  FileSystemURL src_file(CreateFile("src/file"));
  FileSystemURL dest_dir(CreateDirectory("dest"));

  EXPECT_EQ(
      base::File::FILE_ERROR_INVALID_OPERATION,
      Move(src_file, dest_dir, FileSystemOperation::CopyOrMoveOptionSet()));
  EXPECT_TRUE(change_observer()->HasNoChange());
}

TEST_F(FileSystemOperationImplTest, TestMoveFailureDestParentDoesntExist) {
  // Dest. parent path does not exist.
  FileSystemURL src_dir(CreateDirectory("src"));
  EXPECT_EQ(base::File::FILE_ERROR_NOT_FOUND,
            Move(src_dir, URLForPath("nonexistent/deset"),
                 FileSystemOperation::CopyOrMoveOptionSet()));
  EXPECT_TRUE(change_observer()->HasNoChange());
}

TEST_F(FileSystemOperationImplTest, TestMoveSuccessSrcFileAndOverwrite) {
  FileSystemURL src_file(CreateFile("src"));
  FileSystemURL dest_file(CreateFile("dest"));

  EXPECT_EQ(
      base::File::FILE_OK,
      Move(src_file, dest_file, FileSystemOperation::CopyOrMoveOptionSet()));
  EXPECT_TRUE(FileExists("dest"));

  EXPECT_EQ(1, change_observer()->get_and_reset_create_file_from_count());
  EXPECT_EQ(1, change_observer()->get_and_reset_remove_file_count());
  EXPECT_TRUE(change_observer()->HasNoChange());

  EXPECT_EQ(1, quota_manager_proxy()->notify_bucket_accessed_count());
}

TEST_F(FileSystemOperationImplTest, TestMoveSuccessSrcFileAndNew) {
  FileSystemURL src_file(CreateFile("src"));

  EXPECT_EQ(base::File::FILE_OK,
            Move(src_file, URLForPath("new"),
                 FileSystemOperation::CopyOrMoveOptionSet()));
  EXPECT_TRUE(FileExists("new"));

  EXPECT_EQ(1, change_observer()->get_and_reset_create_file_from_count());
  EXPECT_EQ(1, change_observer()->get_and_reset_remove_file_count());
  EXPECT_TRUE(change_observer()->HasNoChange());
}

TEST_F(FileSystemOperationImplTest, TestMoveSuccessSrcDirAndOverwrite) {
  FileSystemURL src_dir(CreateDirectory("src"));
  FileSystemURL dest_dir(CreateDirectory("dest"));

  EXPECT_EQ(
      base::File::FILE_OK,
      Move(src_dir, dest_dir, FileSystemOperation::CopyOrMoveOptionSet()));
  EXPECT_FALSE(DirectoryExists("src"));

  EXPECT_EQ(1, change_observer()->get_and_reset_create_directory_count());
  EXPECT_EQ(2, change_observer()->get_and_reset_remove_directory_count());
  EXPECT_TRUE(change_observer()->HasNoChange());

  // Make sure we've overwritten but not moved the source under the |dest_dir|.
  EXPECT_TRUE(DirectoryExists("dest"));
  EXPECT_FALSE(DirectoryExists("dest/src"));
}

TEST_F(FileSystemOperationImplTest, TestMoveSuccessSrcDirAndNew) {
  FileSystemURL src_dir(CreateDirectory("src"));
  FileSystemURL dest_dir(CreateDirectory("dest"));

  EXPECT_EQ(base::File::FILE_OK,
            Move(src_dir, URLForPath("dest/new"),
                 FileSystemOperation::CopyOrMoveOptionSet()));
  EXPECT_FALSE(DirectoryExists("src"));
  EXPECT_TRUE(DirectoryExists("dest/new"));

  EXPECT_EQ(1, change_observer()->get_and_reset_remove_directory_count());
  EXPECT_EQ(1, change_observer()->get_and_reset_create_directory_count());
  EXPECT_TRUE(change_observer()->HasNoChange());
}

TEST_F(FileSystemOperationImplTest, TestMoveSuccessSrcDirRecursive) {
  FileSystemURL src_dir(CreateDirectory("src"));
  CreateDirectory("src/dir");
  CreateFile("src/dir/sub");

  FileSystemURL dest_dir(CreateDirectory("dest"));

  EXPECT_EQ(
      base::File::FILE_OK,
      Move(src_dir, dest_dir, FileSystemOperation::CopyOrMoveOptionSet()));
  EXPECT_TRUE(DirectoryExists("dest/dir"));
  EXPECT_TRUE(FileExists("dest/dir/sub"));

  EXPECT_EQ(3, change_observer()->get_and_reset_remove_directory_count());
  EXPECT_EQ(2, change_observer()->get_and_reset_create_directory_count());
  EXPECT_EQ(1, change_observer()->get_and_reset_remove_file_count());
  EXPECT_EQ(1, change_observer()->get_and_reset_create_file_from_count());
  EXPECT_TRUE(change_observer()->HasNoChange());
}

TEST_F(FileSystemOperationImplTest, TestMoveSuccessSamePath) {
  FileSystemURL src_dir(CreateDirectory("src"));
  CreateDirectory("src/dir");
  CreateFile("src/dir/sub");

  EXPECT_EQ(base::File::FILE_OK,
            Move(src_dir, src_dir, FileSystemOperation::CopyOrMoveOptionSet()));
  EXPECT_TRUE(DirectoryExists("src/dir"));
  EXPECT_TRUE(FileExists("src/dir/sub"));

  EXPECT_EQ(0, change_observer()->get_and_reset_remove_directory_count());
  EXPECT_EQ(0, change_observer()->get_and_reset_create_directory_count());
  EXPECT_EQ(0, change_observer()->get_and_reset_remove_file_count());
  EXPECT_EQ(0, change_observer()->get_and_reset_create_file_from_count());
  EXPECT_TRUE(change_observer()->HasNoChange());
}

TEST_F(FileSystemOperationImplTest, TestCopyFailureSrcDoesntExist) {
  EXPECT_EQ(base::File::FILE_ERROR_NOT_FOUND,
            Copy(URLForPath("a"), URLForPath("b"),
                 FileSystemOperation::CopyOrMoveOptionSet()));
  EXPECT_TRUE(change_observer()->HasNoChange());
}

TEST_F(FileSystemOperationImplTest, TestCopyFailureContainsPath) {
  FileSystemURL src_dir(CreateDirectory("src"));
  FileSystemURL dest_dir(CreateDirectory("src/dir"));

  EXPECT_EQ(
      base::File::FILE_ERROR_INVALID_OPERATION,
      Copy(src_dir, dest_dir, FileSystemOperation::CopyOrMoveOptionSet()));
  EXPECT_TRUE(change_observer()->HasNoChange());
}

TEST_F(FileSystemOperationImplTest, TestCopyFailureSrcDirExistsDestFile) {
  // Src exists and is dir. Dest is a file.
  FileSystemURL src_dir(CreateDirectory("src"));
  FileSystemURL dest_dir(CreateDirectory("dest"));
  FileSystemURL dest_file(CreateFile("dest/file"));

  EXPECT_EQ(
      base::File::FILE_ERROR_INVALID_OPERATION,
      Copy(src_dir, dest_file, FileSystemOperation::CopyOrMoveOptionSet()));
  EXPECT_TRUE(change_observer()->HasNoChange());
}

TEST_F(FileSystemOperationImplTest,
       TestCopyFailureSrcFileExistsDestNonEmptyDir) {
  // Src exists and is a directory. Dest is a non-empty directory.
  FileSystemURL src_dir(CreateDirectory("src"));
  FileSystemURL dest_dir(CreateDirectory("dest"));
  FileSystemURL dest_file(CreateFile("dest/file"));

  EXPECT_EQ(
      base::File::FILE_ERROR_NOT_EMPTY,
      Copy(src_dir, dest_dir, FileSystemOperation::CopyOrMoveOptionSet()));
  EXPECT_TRUE(change_observer()->HasNoChange());
}

TEST_F(FileSystemOperationImplTest, TestCopyFailureSrcFileExistsDestDir) {
  // Src exists and is a file. Dest is a directory.
  FileSystemURL src_file(CreateFile("src"));
  FileSystemURL dest_dir(CreateDirectory("dest"));

  EXPECT_EQ(
      base::File::FILE_ERROR_INVALID_OPERATION,
      Copy(src_file, dest_dir, FileSystemOperation::CopyOrMoveOptionSet()));
  EXPECT_TRUE(change_observer()->HasNoChange());
}

TEST_F(FileSystemOperationImplTest, TestCopyFailureDestParentDoesntExist) {
  // Dest. parent path does not exist.
  FileSystemURL src_dir(CreateDirectory("src"));

  EXPECT_EQ(base::File::FILE_ERROR_NOT_FOUND,
            Copy(src_dir, URLForPath("nonexistent/dest"),
                 FileSystemOperation::CopyOrMoveOptionSet()));
  EXPECT_TRUE(change_observer()->HasNoChange());
}

TEST_F(FileSystemOperationImplTest, TestCopyFailureByQuota) {
  FileSystemURL src_dir(CreateDirectory("src"));
  FileSystemURL src_file(CreateFile("src/file"));
  FileSystemURL dest_dir(CreateDirectory("dest"));
  EXPECT_EQ(base::File::FILE_OK, Truncate(src_file, 6));
  EXPECT_EQ(6, GetFileSize("src/file"));

  FileSystemURL dest_file(URLForPath("dest/file"));
  int64_t dest_path_cost = ComputePathCost(dest_file);
  GrantQuotaForCurrentUsage();
  AddQuota(6 + dest_path_cost - 1);

  EXPECT_EQ(
      base::File::FILE_ERROR_NO_SPACE,
      Copy(src_file, dest_file, FileSystemOperation::CopyOrMoveOptionSet()));
  EXPECT_FALSE(FileExists("dest/file"));
}

TEST_F(FileSystemOperationImplTest, TestCopySuccessSrcFileAndOverwrite) {
  FileSystemURL src_file(CreateFile("src"));
  FileSystemURL dest_file(CreateFile("dest"));

  EXPECT_EQ(
      base::File::FILE_OK,
      Copy(src_file, dest_file, FileSystemOperation::CopyOrMoveOptionSet()));

  EXPECT_TRUE(FileExists("dest"));
  EXPECT_EQ(4, quota_manager_proxy()->notify_bucket_accessed_count());
  EXPECT_EQ(2, change_observer()->get_and_reset_modify_file_count());

  EXPECT_TRUE(change_observer()->HasNoChange());
}

TEST_F(FileSystemOperationImplTest, TestCopySuccessSrcFileAndNew) {
  FileSystemURL src_file(CreateFile("src"));

  EXPECT_EQ(base::File::FILE_OK,
            Copy(src_file, URLForPath("new"),
                 FileSystemOperation::CopyOrMoveOptionSet()));
  EXPECT_TRUE(FileExists("new"));
  EXPECT_EQ(4, quota_manager_proxy()->notify_bucket_accessed_count());

  EXPECT_EQ(1, change_observer()->get_and_reset_create_file_count());
  EXPECT_EQ(1, change_observer()->get_and_reset_modify_file_count());
  EXPECT_TRUE(change_observer()->HasNoChange());
}

TEST_F(FileSystemOperationImplTest, TestCopySuccessSrcDirAndOverwrite) {
  FileSystemURL src_dir(CreateDirectory("src"));
  FileSystemURL dest_dir(CreateDirectory("dest"));

  EXPECT_EQ(
      base::File::FILE_OK,
      Copy(src_dir, dest_dir, FileSystemOperation::CopyOrMoveOptionSet()));

  // Make sure we've overwritten but not copied the source under the |dest_dir|.
  EXPECT_TRUE(DirectoryExists("dest"));
  EXPECT_FALSE(DirectoryExists("dest/src"));
  EXPECT_GE(quota_manager_proxy()->notify_bucket_accessed_count(), 3);

  EXPECT_EQ(1, change_observer()->get_and_reset_create_directory_count());
  EXPECT_EQ(1, change_observer()->get_and_reset_remove_directory_count());
  EXPECT_TRUE(change_observer()->HasNoChange());
}

TEST_F(FileSystemOperationImplTest, TestCopySuccessSrcDirAndNew) {
  FileSystemURL src_dir(CreateDirectory("src"));
  FileSystemURL dest_dir_new(URLForPath("dest"));

  EXPECT_EQ(
      base::File::FILE_OK,
      Copy(src_dir, dest_dir_new, FileSystemOperation::CopyOrMoveOptionSet()));
  EXPECT_TRUE(DirectoryExists("dest"));
  EXPECT_GE(quota_manager_proxy()->notify_bucket_accessed_count(), 2);

  EXPECT_EQ(1, change_observer()->get_and_reset_create_directory_count());
  EXPECT_TRUE(change_observer()->HasNoChange());
}

TEST_F(FileSystemOperationImplTest, TestCopySuccessSrcDirRecursive) {
  FileSystemURL src_dir(CreateDirectory("src"));
  CreateDirectory("src/dir");
  CreateFile("src/dir/sub");

  FileSystemURL dest_dir(CreateDirectory("dest"));

  EXPECT_EQ(
      base::File::FILE_OK,
      Copy(src_dir, dest_dir, FileSystemOperation::CopyOrMoveOptionSet()));

  EXPECT_TRUE(DirectoryExists("dest/dir"));
  EXPECT_TRUE(FileExists("dest/dir/sub"));

  // For recursive copy we may record multiple read access.
  EXPECT_GE(quota_manager_proxy()->notify_bucket_accessed_count(), 1);

  EXPECT_EQ(2, change_observer()->get_and_reset_create_directory_count());
  EXPECT_EQ(1, change_observer()->get_and_reset_remove_directory_count());
  EXPECT_EQ(1, change_observer()->get_and_reset_create_file_count());
  EXPECT_EQ(1, change_observer()->get_and_reset_modify_file_count());
  EXPECT_TRUE(change_observer()->HasNoChange());
}

TEST_F(FileSystemOperationImplTest, TestCopySuccessSamePath) {
  FileSystemURL src_dir(CreateDirectory("src"));
  CreateDirectory("src/dir");
  CreateFile("src/dir/sub");

  EXPECT_EQ(base::File::FILE_OK,
            Copy(src_dir, src_dir, FileSystemOperation::CopyOrMoveOptionSet()));

  EXPECT_TRUE(DirectoryExists("src/dir"));
  EXPECT_TRUE(FileExists("src/dir/sub"));

  EXPECT_EQ(0, change_observer()->get_and_reset_create_directory_count());
  EXPECT_EQ(0, change_observer()->get_and_reset_remove_file_count());
  EXPECT_EQ(0, change_observer()->get_and_reset_create_file_from_count());
  EXPECT_TRUE(change_observer()->HasNoChange());
}

TEST_F(FileSystemOperationImplTest, TestCopyInForeignFileSuccess) {
  base::FilePath src_local_disk_file_path;
  base::CreateTemporaryFile(&src_local_disk_file_path);
  constexpr std::string_view test_data = "foo";
  constexpr int data_size = test_data.size();
  base::WriteFile(src_local_disk_file_path, test_data);

  FileSystemURL dest_dir(CreateDirectory("dest"));

  int64_t before_usage;
  GetUsageAndQuota(&before_usage, nullptr);

  // Check that the file copied and corresponding usage increased.
  EXPECT_EQ(base::File::FILE_OK, CopyInForeignFile(src_local_disk_file_path,
                                                   URLForPath("dest/file")));

  EXPECT_EQ(1, change_observer()->create_file_count());
  EXPECT_TRUE(FileExists("dest/file"));
  int64_t after_usage;
  GetUsageAndQuota(&after_usage, nullptr);
  EXPECT_GT(after_usage, before_usage);

  // Compare contents of src and copied file.
  char buffer[100];
  EXPECT_EQ(data_size,
            base::ReadFile(PlatformPath("dest/file"), buffer, data_size));
  for (int i = 0; i < data_size; ++i)
    EXPECT_EQ(test_data.at(i), buffer[i]);
}

TEST_F(FileSystemOperationImplTest, TestCopyInForeignFileFailureByQuota) {
  base::FilePath src_local_disk_file_path;
  base::CreateTemporaryFile(&src_local_disk_file_path);
  constexpr std::string_view test_data = "foo";
  base::WriteFile(src_local_disk_file_path, test_data);

  FileSystemURL dest_dir(CreateDirectory("dest"));

  GrantQuotaForCurrentUsage();
  EXPECT_EQ(
      base::File::FILE_ERROR_NO_SPACE,
      CopyInForeignFile(src_local_disk_file_path, URLForPath("dest/file")));

  EXPECT_FALSE(FileExists("dest/file"));
  EXPECT_EQ(0, change_observer()->create_file_count());
}

TEST_F(FileSystemOperationImplTest, TestCreateFileFailure) {
  // Already existing file and exclusive true.
  FileSystemURL file(CreateFile("file"));
  EXPECT_EQ(base::File::FILE_ERROR_EXISTS, CreateFile(file, true));
  EXPECT_TRUE(change_observer()->HasNoChange());
}

TEST_F(FileSystemOperationImplTest, TestCreateFileSuccessFileExists) {
  // Already existing file and exclusive false.
  FileSystemURL file(CreateFile("file"));
  EXPECT_EQ(base::File::FILE_OK, CreateFile(file, false));
  EXPECT_TRUE(FileExists("file"));

  // The file was already there; did nothing.
  EXPECT_TRUE(change_observer()->HasNoChange());
}

TEST_F(FileSystemOperationImplTest, TestCreateFileSuccessExclusive) {
  // File doesn't exist but exclusive is true.
  EXPECT_EQ(base::File::FILE_OK, CreateFile(URLForPath("new"), true));
  EXPECT_TRUE(FileExists("new"));
  EXPECT_EQ(1, change_observer()->get_and_reset_create_file_count());
}

TEST_F(FileSystemOperationImplTest, TestCreateFileSuccessFileDoesntExist) {
  // Non existing file.
  EXPECT_EQ(base::File::FILE_OK, CreateFile(URLForPath("nonexistent"), false));
  EXPECT_EQ(1, change_observer()->get_and_reset_create_file_count());
}

TEST_F(FileSystemOperationImplTest, TestCreateDirFailureDestParentDoesntExist) {
  // Dest. parent path does not exist.
  EXPECT_EQ(base::File::FILE_ERROR_NOT_FOUND,
            CreateDirectory(URLForPath("nonexistent/dir"), false, false));
  EXPECT_TRUE(change_observer()->HasNoChange());
}

TEST_F(FileSystemOperationImplTest, TestCreateDirFailureDirExists) {
  // Exclusive and dir existing at path.
  FileSystemURL dir(CreateDirectory("dir"));
  EXPECT_EQ(base::File::FILE_ERROR_EXISTS, CreateDirectory(dir, true, false));
  EXPECT_TRUE(change_observer()->HasNoChange());
}

TEST_F(FileSystemOperationImplTest, TestCreateDirFailureFileExists) {
  // Exclusive true and file existing at path.
  FileSystemURL file(CreateFile("file"));
  EXPECT_EQ(base::File::FILE_ERROR_EXISTS, CreateDirectory(file, true, false));
  EXPECT_TRUE(change_observer()->HasNoChange());
}

TEST_F(FileSystemOperationImplTest, TestCreateDirSuccess) {
  // Dir exists and exclusive is false.
  FileSystemURL dir(CreateDirectory("dir"));
  EXPECT_EQ(base::File::FILE_OK, CreateDirectory(dir, false, false));
  EXPECT_TRUE(change_observer()->HasNoChange());

  // Dir doesn't exist.
  EXPECT_EQ(base::File::FILE_OK,
            CreateDirectory(URLForPath("new"), false, false));
  EXPECT_TRUE(DirectoryExists("new"));
  EXPECT_EQ(1, change_observer()->get_and_reset_create_directory_count());
}

TEST_F(FileSystemOperationImplTest, TestCreateDirSuccessExclusive) {
  // Dir doesn't exist.
  EXPECT_EQ(base::File::FILE_OK,
            CreateDirectory(URLForPath("new"), true, false));
  EXPECT_TRUE(DirectoryExists("new"));
  EXPECT_EQ(1, change_observer()->get_and_reset_create_directory_count());
  EXPECT_TRUE(change_observer()->HasNoChange());
}

TEST_F(FileSystemOperationImplTest, TestExistsAndMetadataFailure) {
  EXPECT_EQ(base::File::FILE_ERROR_NOT_FOUND,
            GetMetadata(URLForPath("nonexistent"), {}));

  EXPECT_EQ(base::File::FILE_ERROR_NOT_FOUND,
            FileExists(URLForPath("nonexistent")));

  EXPECT_EQ(base::File::FILE_ERROR_NOT_FOUND,
            DirectoryExists(URLForPath("nonexistent")));
  EXPECT_TRUE(change_observer()->HasNoChange());
}

TEST_F(FileSystemOperationImplTest, TestExistsAndMetadataSuccess) {
  FileSystemURL dir(CreateDirectory("dir"));
  FileSystemURL file(CreateFile("dir/file"));
  int read_access = 0;

  EXPECT_EQ(base::File::FILE_OK, DirectoryExists(dir));
  ++read_access;

  EXPECT_EQ(
      base::File::FILE_OK,
      GetMetadata(dir, {FileSystemOperation::GetMetadataField::kIsDirectory}));
  EXPECT_TRUE(info().is_directory);
  ++read_access;

  EXPECT_EQ(base::File::FILE_OK, FileExists(file));
  ++read_access;

  EXPECT_EQ(
      base::File::FILE_OK,
      GetMetadata(file, {FileSystemOperation::GetMetadataField::kIsDirectory}));
  EXPECT_FALSE(info().is_directory);
  ++read_access;

  EXPECT_EQ(read_access, quota_manager_proxy()->notify_bucket_accessed_count());
  EXPECT_TRUE(change_observer()->HasNoChange());
}

TEST_F(FileSystemOperationImplTest, TestTypeMismatchErrors) {
  FileSystemURL dir(CreateDirectory("dir"));
  EXPECT_EQ(base::File::FILE_ERROR_NOT_A_FILE, FileExists(dir));

  FileSystemURL file(CreateFile("file"));
  EXPECT_EQ(base::File::FILE_ERROR_NOT_A_DIRECTORY, DirectoryExists(file));
}

TEST_F(FileSystemOperationImplTest, TestReadDirFailure) {
  // Path doesn't exist
  EXPECT_EQ(base::File::FILE_ERROR_NOT_FOUND,
            ReadDirectory(URLForPath("nonexistent")));

  // File exists.
  FileSystemURL file(CreateFile("file"));
  EXPECT_EQ(base::File::FILE_ERROR_NOT_A_DIRECTORY, ReadDirectory(file));
  EXPECT_TRUE(change_observer()->HasNoChange());
}

TEST_F(FileSystemOperationImplTest, TestReadDirSuccess) {
  //      parent_dir
  //       |       |
  //  child_dir  child_file
  // Verify reading parent_dir.
  FileSystemURL parent_dir(CreateDirectory("dir"));
  FileSystemURL child_dir(CreateDirectory("dir/child_dir"));
  FileSystemURL child_file(CreateFile("dir/child_file"));

  EXPECT_EQ(base::File::FILE_OK, ReadDirectory(parent_dir));
  EXPECT_EQ(2u, entries().size());

  for (const filesystem::mojom::DirectoryEntry& entry : entries()) {
    if (entry.type == filesystem::mojom::FsFileType::DIRECTORY) {
      EXPECT_EQ(FILE_PATH_LITERAL("child_dir"), entry.name.value());
    } else {
      EXPECT_EQ(FILE_PATH_LITERAL("child_file"), entry.name.value());
    }
  }
  EXPECT_EQ(1, quota_manager_proxy()->notify_bucket_accessed_count());
  EXPECT_TRUE(change_observer()->HasNoChange());
}

TEST_F(FileSystemOperationImplTest, TestRemoveFailure) {
  // Path doesn't exist.
  EXPECT_EQ(base::File::FILE_ERROR_NOT_FOUND,
            Remove(URLForPath("nonexistent"), false /* recursive */));

  // It's an error to try to remove a non-empty directory if recursive flag
  // is false.
  //      parent_dir
  //       |       |
  //  child_dir  child_file
  // Verify deleting parent_dir.
  FileSystemURL parent_dir(CreateDirectory("dir"));
  FileSystemURL child_dir(CreateDirectory("dir/child_dir"));
  FileSystemURL child_file(CreateFile("dir/child_file"));

  EXPECT_EQ(base::File::FILE_ERROR_NOT_EMPTY,
            Remove(parent_dir, false /* recursive */));
  EXPECT_TRUE(change_observer()->HasNoChange());
}

TEST_F(FileSystemOperationImplTest, TestRemoveSuccess) {
  FileSystemURL empty_dir(CreateDirectory("empty_dir"));
  EXPECT_TRUE(DirectoryExists("empty_dir"));
  EXPECT_EQ(base::File::FILE_OK, Remove(empty_dir, false /* recursive */));
  EXPECT_FALSE(DirectoryExists("empty_dir"));

  EXPECT_EQ(1, change_observer()->get_and_reset_remove_directory_count());
  EXPECT_TRUE(change_observer()->HasNoChange());
}

TEST_F(FileSystemOperationImplTest, TestRemoveSuccessRecursive) {
  // Removing a non-empty directory with recursive flag == true should be ok.
  //      parent_dir
  //       |       |
  //  child_dir  child_files
  //       |
  //  child_files
  //
  // Verify deleting parent_dir.
  FileSystemURL parent_dir(CreateDirectory("dir"));
  for (int i = 0; i < 8; ++i)
    CreateFile(base::StringPrintf("dir/file-%d", i));
  FileSystemURL child_dir(CreateDirectory("dir/child_dir"));
  for (int i = 0; i < 8; ++i)
    CreateFile(base::StringPrintf("dir/child_dir/file-%d", i));

  EXPECT_EQ(base::File::FILE_OK, Remove(parent_dir, true /* recursive */));
  EXPECT_FALSE(DirectoryExists("parent_dir"));

  EXPECT_EQ(2, change_observer()->get_and_reset_remove_directory_count());
  EXPECT_EQ(16, change_observer()->get_and_reset_remove_file_count());
  EXPECT_TRUE(change_observer()->HasNoChange());
}

TEST_F(FileSystemOperationImplTest, TestTruncate) {
  FileSystemURL file(CreateFile("file"));
  base::FilePath platform_path = PlatformPath("file");

  constexpr std::string_view test_data = "test data";
  constexpr int data_size = test_data.size();
  EXPECT_TRUE(base::WriteFile(platform_path, test_data));

  // Check that its length is the size of the data written.
  EXPECT_EQ(
      base::File::FILE_OK,
      GetMetadata(file,
                  {storage::FileSystemOperation::GetMetadataField::kIsDirectory,
                   storage::FileSystemOperation::GetMetadataField::kSize}));
  EXPECT_FALSE(info().is_directory);
  EXPECT_EQ(data_size, info().size);

  // Extend the file by truncating it.
  int length = 17;
  EXPECT_EQ(base::File::FILE_OK, Truncate(file, length));

  EXPECT_EQ(1, change_observer()->get_and_reset_modify_file_count());
  EXPECT_TRUE(change_observer()->HasNoChange());

  // Check that its length is now 17 and that it's all zeroes after the test
  // data.
  EXPECT_EQ(length, GetFileSize("file"));
  char data[100];
  EXPECT_EQ(length, base::ReadFile(platform_path, data, length));
  for (int i = 0; i < length; ++i) {
    if (i < static_cast<int>(test_data.size())) {
      EXPECT_EQ(test_data.at(i), data[i]);
    } else {
      EXPECT_EQ(0, data[i]);
    }
  }

  // Shorten the file by truncating it.
  length = 3;
  EXPECT_EQ(base::File::FILE_OK, Truncate(file, length));

  EXPECT_EQ(1, change_observer()->get_and_reset_modify_file_count());
  EXPECT_TRUE(change_observer()->HasNoChange());

  // Check that its length is now 3 and that it contains only bits of test data.
  EXPECT_EQ(length, GetFileSize("file"));
  EXPECT_EQ(length, base::ReadFile(platform_path, data, length));
  for (int i = 0; i < length; ++i)
    EXPECT_EQ(test_data.at(i), data[i]);

  // Truncate is not a 'read' access.  (Here expected access count is 1
  // since we made 1 read access for GetMetadata.)
  EXPECT_EQ(1, quota_manager_proxy()->notify_bucket_accessed_count());
}

TEST_F(FileSystemOperationImplTest, TestTruncateFailureByQuota) {
  FileSystemURL dir(CreateDirectory("dir"));
  FileSystemURL file(CreateFile("dir/file"));

  GrantQuotaForCurrentUsage();
  AddQuota(10);

  EXPECT_EQ(base::File::FILE_OK, Truncate(file, 10));
  EXPECT_EQ(1, change_observer()->get_and_reset_modify_file_count());
  EXPECT_TRUE(change_observer()->HasNoChange());

  EXPECT_EQ(10, GetFileSize("dir/file"));

  EXPECT_EQ(base::File::FILE_ERROR_NO_SPACE, Truncate(file, 11));
  EXPECT_TRUE(change_observer()->HasNoChange());

  EXPECT_EQ(10, GetFileSize("dir/file"));
}

// TODO(crbug.com/40511450): Remove this test once last_access_time has
// been removed after PPAPI has been deprecated. Fuchsia does not support touch,
// which breaks this test that relies on it. Since PPAPI is being deprecated,
// this test is excluded from the Fuchsia build.
// See https://crbug.com/1077456 for details.
#if !BUILDFLAG(IS_FUCHSIA)
TEST_F(FileSystemOperationImplTest, TestTouchFile) {
  FileSystemURL file(CreateFile("file"));
  base::FilePath platform_path = PlatformPath("file");

  base::File::Info info;
  EXPECT_TRUE(base::GetFileInfo(platform_path, &info));
  EXPECT_FALSE(info.is_directory);
  EXPECT_EQ(0, info.size);
  const base::Time last_modified = info.last_modified;
  const base::Time last_accessed = info.last_accessed;

  const base::Time new_modified_time = base::Time::UnixEpoch();
  const base::Time new_accessed_time = new_modified_time + base::Hours(77);
  ASSERT_NE(last_modified, new_modified_time);
  ASSERT_NE(last_accessed, new_accessed_time);

  EXPECT_EQ(base::File::FILE_OK,
            TouchFile(file, new_accessed_time, new_modified_time));
  EXPECT_TRUE(change_observer()->HasNoChange());

  EXPECT_TRUE(base::GetFileInfo(platform_path, &info));
  // We compare as time_t here to lower our resolution, to avoid false
  // negatives caused by conversion to the local filesystem's native
  // representation and back.
  EXPECT_EQ(new_modified_time.ToTimeT(), info.last_modified.ToTimeT());
  EXPECT_EQ(new_accessed_time.ToTimeT(), info.last_accessed.ToTimeT());
}
#endif  // !BUILDFLAG(IS_FUCHSIA)

TEST_F(FileSystemOperationImplTest, TestCreateSnapshotFile) {
  FileSystemURL dir(CreateDirectory("dir"));

  // Create a file for the testing.
  EXPECT_EQ(base::File::FILE_OK, DirectoryExists(dir));
  FileSystemURL file(CreateFile("dir/file"));
  EXPECT_EQ(base::File::FILE_OK, FileExists(file));

  // See if we can get a 'snapshot' file info for the file.
  // Since FileSystemOperationImpl assumes the file exists in the local
  // directory it should just returns the same metadata and platform_path
  // as the file itself.
  EXPECT_EQ(base::File::FILE_OK, CreateSnapshotFile(file));
  EXPECT_FALSE(info().is_directory);
  EXPECT_EQ(PlatformPath("dir/file"), path());
  EXPECT_TRUE(change_observer()->HasNoChange());

  // The FileSystemOpration implementation does not create a
  // shareable file reference.
  EXPECT_EQ(nullptr, shareable_file_ref());
}

TEST_F(FileSystemOperationImplTest, TestMoveSuccessSrcDirRecursiveWithQuota) {
  FileSystemURL src(CreateDirectory("src"));
  int64_t src_path_cost = GetUsage();

  FileSystemURL dest(CreateDirectory("dest"));
  FileSystemURL child_file1(CreateFile("src/file1"));
  FileSystemURL child_file2(CreateFile("src/file2"));
  FileSystemURL child_dir(CreateDirectory("src/dir"));
  FileSystemURL grandchild_file1(CreateFile("src/dir/file1"));
  FileSystemURL grandchild_file2(CreateFile("src/dir/file2"));

  int64_t total_path_cost = GetUsage();
  EXPECT_EQ(0, GetDataSizeOnDisk());

  EXPECT_EQ(base::File::FILE_OK, Truncate(child_file1, 5000));
  EXPECT_EQ(base::File::FILE_OK, Truncate(child_file2, 400));
  EXPECT_EQ(base::File::FILE_OK, Truncate(grandchild_file1, 30));
  EXPECT_EQ(base::File::FILE_OK, Truncate(grandchild_file2, 2));

  const int64_t all_file_size = 5000 + 400 + 30 + 2;
  EXPECT_EQ(all_file_size, GetDataSizeOnDisk());
  EXPECT_EQ(all_file_size + total_path_cost, GetUsage());

  EXPECT_EQ(base::File::FILE_OK,
            Move(src, dest, FileSystemOperation::CopyOrMoveOptionSet()));

  EXPECT_FALSE(DirectoryExists("src/dir"));
  EXPECT_FALSE(FileExists("src/dir/file2"));
  EXPECT_TRUE(DirectoryExists("dest/dir"));
  EXPECT_TRUE(FileExists("dest/dir/file2"));

  EXPECT_EQ(all_file_size, GetDataSizeOnDisk());
  EXPECT_EQ(all_file_size + total_path_cost - src_path_cost, GetUsage());
}

TEST_F(FileSystemOperationImplTest, TestCopySuccessSrcDirRecursiveWithQuota) {
  FileSystemURL src(CreateDirectory("src"));
  FileSystemURL dest1(CreateDirectory("dest1"));
  FileSystemURL dest2(CreateDirectory("dest2"));

  int64_t usage = GetUsage();
  FileSystemURL child_file1(CreateFile("src/file1"));
  FileSystemURL child_file2(CreateFile("src/file2"));
  FileSystemURL child_dir(CreateDirectory("src/dir"));
  int64_t child_path_cost = GetUsage() - usage;
  usage += child_path_cost;

  FileSystemURL grandchild_file1(CreateFile("src/dir/file1"));
  FileSystemURL grandchild_file2(CreateFile("src/dir/file2"));
  int64_t total_path_cost = GetUsage();
  int64_t grandchild_path_cost = total_path_cost - usage;

  EXPECT_EQ(0, GetDataSizeOnDisk());

  EXPECT_EQ(base::File::FILE_OK, Truncate(child_file1, 8000));
  EXPECT_EQ(base::File::FILE_OK, Truncate(child_file2, 700));
  EXPECT_EQ(base::File::FILE_OK, Truncate(grandchild_file1, 60));
  EXPECT_EQ(base::File::FILE_OK, Truncate(grandchild_file2, 5));

  const int64_t child_file_size = 8000 + 700;
  const int64_t grandchild_file_size = 60 + 5;
  const int64_t all_file_size = child_file_size + grandchild_file_size;
  int64_t expected_usage = all_file_size + total_path_cost;

  usage = GetUsage();
  EXPECT_EQ(all_file_size, GetDataSizeOnDisk());
  EXPECT_EQ(expected_usage, usage);

  EXPECT_EQ(base::File::FILE_OK,
            Copy(src, dest1, FileSystemOperation::CopyOrMoveOptionSet()));

  expected_usage += all_file_size + child_path_cost + grandchild_path_cost;
  EXPECT_TRUE(DirectoryExists("src/dir"));
  EXPECT_TRUE(FileExists("src/dir/file2"));
  EXPECT_TRUE(DirectoryExists("dest1/dir"));
  EXPECT_TRUE(FileExists("dest1/dir/file2"));

  EXPECT_EQ(2 * all_file_size, GetDataSizeOnDisk());
  EXPECT_EQ(expected_usage, GetUsage());

  EXPECT_EQ(base::File::FILE_OK,
            Copy(child_dir, dest2, FileSystemOperation::CopyOrMoveOptionSet()));

  expected_usage += grandchild_file_size + grandchild_path_cost;
  usage = GetUsage();
  EXPECT_EQ(2 * child_file_size + 3 * grandchild_file_size,
            GetDataSizeOnDisk());
  EXPECT_EQ(expected_usage, usage);
}

TEST_F(FileSystemOperationImplTest,
       TestCopySuccessSrcFileWithDifferentFileSize) {
  FileSystemURL src_file(CreateFile("src"));
  FileSystemURL dest_file(CreateFile("dest"));

  EXPECT_EQ(base::File::FILE_OK, Truncate(dest_file, 6));
  EXPECT_EQ(
      base::File::FILE_OK,
      Copy(src_file, dest_file, FileSystemOperation::CopyOrMoveOptionSet()));
  EXPECT_EQ(0, GetFileSize("dest"));
}

}  // namespace storage
