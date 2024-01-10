// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_file_util.h"
#include "storage/browser/file_system/file_system_operation_context.h"
#include "storage/browser/file_system/file_system_operation_runner.h"
#include "storage/browser/file_system/file_system_util.h"
#include "storage/browser/file_system/local_file_util.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/browser/test/mock_blob_util.h"
#include "storage/browser/test/mock_file_change_observer.h"
#include "storage/browser/test/mock_quota_manager.h"
#include "storage/browser/test/mock_special_storage_policy.h"
#include "storage/browser/test/test_file_system_backend.h"
#include "storage/browser/test/test_file_system_context.h"
#include "storage/common/file_system/file_system_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace storage {

namespace {

const char kOrigin[] = "http://example.com";
const FileSystemType kFileSystemType = kFileSystemTypeTest;

}  // namespace

class FileSystemOperationImplWriteTest : public testing::Test {
 public:
  FileSystemOperationImplWriteTest()
      : special_storage_policy_(
            base::MakeRefCounted<MockSpecialStoragePolicy>()),
        task_environment_(base::test::TaskEnvironment::MainThreadType::IO),
        virtual_path_(FILE_PATH_LITERAL("temporary file")),
        status_(base::File::FILE_OK),
        cancel_status_(base::File::FILE_ERROR_FAILED),
        bytes_written_(0),
        complete_(false) {
    change_observers_ = MockFileChangeObserver::CreateList(&change_observer_);
  }

  FileSystemOperationImplWriteTest(const FileSystemOperationImplWriteTest&) =
      delete;
  FileSystemOperationImplWriteTest& operator=(
      const FileSystemOperationImplWriteTest&) = delete;

  void SetUp() override {
    ASSERT_TRUE(data_dir_.CreateUniqueTempDir());

    quota_manager_ = base::MakeRefCounted<MockQuotaManager>(
        /* is_incognito= */ false, data_dir_.GetPath(),
        base::SingleThreadTaskRunner::GetCurrentDefault(),
        special_storage_policy_);

    file_system_context_ = CreateFileSystemContextForTesting(
        quota_manager_->proxy(), data_dir_.GetPath());
    blob_storage_context_ = std::make_unique<BlobStorageContext>();

    base::test::TestFuture<base::File::Error> future;
    file_system_context_->operation_runner()->CreateFile(
        URLForPath(virtual_path_), true /* exclusive */, future.GetCallback());
    ASSERT_EQ(base::File::FILE_OK, future.Get());

    static_cast<TestFileSystemBackend*>(
        file_system_context_->GetFileSystemBackend(kFileSystemType))
        ->AddFileChangeObserver(change_observer());
  }
  void RunUntilIdle() { loop_.RunUntilIdle(); }
  void Run() { loop_.Run(); }
  void TearDown() override {
    quota_manager_ = nullptr;
    file_system_context_ = nullptr;
    base::RunLoop().RunUntilIdle();
  }

  base::File::Error status() const { return status_; }
  base::File::Error cancel_status() const { return cancel_status_; }
  void add_bytes_written(int64_t bytes, bool complete) {
    bytes_written_ += bytes;
    EXPECT_FALSE(complete_);
    complete_ = complete;
  }
  int64_t bytes_written() const { return bytes_written_; }
  bool complete() const { return complete_; }

 protected:
  const ChangeObserverList& change_observers() const {
    return change_observers_;
  }

  MockFileChangeObserver* change_observer() { return &change_observer_; }

  FileSystemURL URLForPath(const base::FilePath& path) const {
    return file_system_context_->CreateCrackedFileSystemURL(
        blink::StorageKey::CreateFromStringForTesting(kOrigin), kFileSystemType,
        path);
  }

  // Callback function for recording test results.
  FileSystemOperation::WriteCallback RecordWriteCallback() {
    return base::BindRepeating(&FileSystemOperationImplWriteTest::DidWrite,
                               weak_factory_.GetWeakPtr());
  }

  FileSystemOperation::StatusCallback RecordCancelCallback() {
    return base::BindOnce(&FileSystemOperationImplWriteTest::DidCancel,
                          weak_factory_.GetWeakPtr());
  }

  void DidWrite(base::File::Error status, int64_t bytes, bool complete) {
    if (status == base::File::FILE_OK) {
      add_bytes_written(bytes, complete);
      if (complete) {
        ASSERT_FALSE(loop_.AnyQuitCalled());
        loop_.QuitWhenIdle();
      }
    } else {
      EXPECT_FALSE(complete_);
      EXPECT_EQ(status_, base::File::FILE_OK);
      complete_ = true;
      status_ = status;
      if (base::RunLoop::IsRunningOnCurrentThread()) {
        ASSERT_FALSE(loop_.AnyQuitCalled());
        loop_.QuitWhenIdle();
      }
    }
  }

  void DidCancel(base::File::Error status) { cancel_status_ = status; }

  BlobStorageContext* blob_storage_context() const {
    return blob_storage_context_.get();
  }

  scoped_refptr<MockSpecialStoragePolicy> special_storage_policy_;

  base::ScopedTempDir data_dir_;
  base::test::TaskEnvironment task_environment_;
  base::RunLoop loop_;

  scoped_refptr<FileSystemContext> file_system_context_;
  scoped_refptr<MockQuotaManager> quota_manager_;

  const base::FilePath virtual_path_;

  // For post-operation status.
  base::File::Error status_;
  base::File::Error cancel_status_;
  int64_t bytes_written_;
  bool complete_;

  std::unique_ptr<BlobStorageContext> blob_storage_context_;

  MockFileChangeObserver change_observer_;
  ChangeObserverList change_observers_;

  base::WeakPtrFactory<FileSystemOperationImplWriteTest> weak_factory_{this};
};

TEST_F(FileSystemOperationImplWriteTest, TestWriteSuccess) {
  ScopedTextBlob blob(blob_storage_context(), "blob-id:success",
                      "Hello, world!\n");
  file_system_context_->operation_runner()->Write(URLForPath(virtual_path_),
                                                  blob.GetBlobDataHandle(), 0,
                                                  RecordWriteCallback());
  Run();

  EXPECT_EQ(14, bytes_written());
  EXPECT_EQ(base::File::FILE_OK, status());
  EXPECT_TRUE(complete());

  EXPECT_EQ(1, change_observer()->get_and_reset_modify_file_count());
}

TEST_F(FileSystemOperationImplWriteTest, TestWriteZero) {
  ScopedTextBlob blob(blob_storage_context(), "blob_id:zero", "");
  file_system_context_->operation_runner()->Write(URLForPath(virtual_path_),
                                                  blob.GetBlobDataHandle(), 0,
                                                  RecordWriteCallback());
  Run();

  EXPECT_EQ(0, bytes_written());
  EXPECT_EQ(base::File::FILE_OK, status());
  EXPECT_TRUE(complete());

  EXPECT_EQ(1, change_observer()->get_and_reset_modify_file_count());
}

TEST_F(FileSystemOperationImplWriteTest, TestWriteInvalidBlob) {
  std::unique_ptr<BlobDataHandle> null_handle;
  file_system_context_->operation_runner()->Write(URLForPath(virtual_path_),
                                                  std::move(null_handle), 0,
                                                  RecordWriteCallback());
  Run();

  EXPECT_EQ(0, bytes_written());
  EXPECT_EQ(base::File::FILE_ERROR_FAILED, status());
  EXPECT_TRUE(complete());

  EXPECT_EQ(0, change_observer()->get_and_reset_modify_file_count());
}

TEST_F(FileSystemOperationImplWriteTest, TestWriteInvalidFile) {
  ScopedTextBlob blob(blob_storage_context(), "blob_id:writeinvalidfile",
                      "It\'ll not be written.");
  file_system_context_->operation_runner()->Write(
      URLForPath(base::FilePath(FILE_PATH_LITERAL("nonexist"))),
      blob.GetBlobDataHandle(), 0, RecordWriteCallback());
  Run();

  EXPECT_EQ(0, bytes_written());
  EXPECT_EQ(base::File::FILE_ERROR_NOT_FOUND, status());
  EXPECT_TRUE(complete());

  EXPECT_EQ(1, change_observer()->get_and_reset_modify_file_count());
}

TEST_F(FileSystemOperationImplWriteTest, TestWriteDir) {
  base::FilePath virtual_dir_path(FILE_PATH_LITERAL("d"));

  base::test::TestFuture<base::File::Error> future;
  file_system_context_->operation_runner()->CreateDirectory(
      URLForPath(virtual_dir_path), true /* exclusive */, false /* recursive */,
      future.GetCallback());
  ASSERT_EQ(base::File::FILE_OK, future.Get());

  ScopedTextBlob blob(blob_storage_context(), "blob:writedir",
                      "It\'ll not be written, too.");
  file_system_context_->operation_runner()->Write(URLForPath(virtual_dir_path),
                                                  blob.GetBlobDataHandle(), 0,
                                                  RecordWriteCallback());
  Run();

  EXPECT_EQ(0, bytes_written());
  // TODO(kinuko): This error code is platform- or fileutil- dependent
  // right now.  Make it return File::FILE_ERROR_NOT_A_FILE in every case.
  EXPECT_TRUE(status() == base::File::FILE_ERROR_NOT_A_FILE ||
              status() == base::File::FILE_ERROR_ACCESS_DENIED ||
              status() == base::File::FILE_ERROR_FAILED);
  EXPECT_TRUE(complete());

  EXPECT_EQ(1, change_observer()->get_and_reset_modify_file_count());
}

TEST_F(FileSystemOperationImplWriteTest, TestWriteFailureByQuota) {
  ScopedTextBlob blob(blob_storage_context(), "blob:success",
                      "Hello, world!\n");
  quota_manager_->SetQuota(
      blink::StorageKey::CreateFromStringForTesting(kOrigin),
      FileSystemTypeToQuotaStorageType(kFileSystemType), 10);
  file_system_context_->operation_runner()->Write(URLForPath(virtual_path_),
                                                  blob.GetBlobDataHandle(), 0,
                                                  RecordWriteCallback());
  Run();

  EXPECT_EQ(10, bytes_written());
  EXPECT_EQ(base::File::FILE_ERROR_NO_SPACE, status());
  EXPECT_TRUE(complete());

  EXPECT_EQ(1, change_observer()->get_and_reset_modify_file_count());
}

TEST_F(FileSystemOperationImplWriteTest, TestImmediateCancelSuccessfulWrite) {
  ScopedTextBlob blob(blob_storage_context(), "blob:success",
                      "Hello, world!\n");
  FileSystemOperationRunner::OperationID id =
      file_system_context_->operation_runner()->Write(URLForPath(virtual_path_),
                                                      blob.GetBlobDataHandle(),
                                                      0, RecordWriteCallback());
  file_system_context_->operation_runner()->Cancel(id, RecordCancelCallback());
  // We use RunAllPendings() instead of Run() here, because we won't dispatch
  // callbacks after Cancel() is issued (so no chance to Quit) nor do we need
  // to run another write cycle.
  RunUntilIdle();

  // Issued Cancel() before receiving any response from Write(),
  // so nothing should have happen.
  EXPECT_EQ(0, bytes_written());
  EXPECT_EQ(base::File::FILE_ERROR_ABORT, status());
  EXPECT_EQ(base::File::FILE_OK, cancel_status());
  EXPECT_TRUE(complete());

  EXPECT_EQ(0, change_observer()->get_and_reset_modify_file_count());
}

TEST_F(FileSystemOperationImplWriteTest, TestImmediateCancelFailingWrite) {
  ScopedTextBlob blob(blob_storage_context(), "blob:writeinvalidfile",
                      "It\'ll not be written.");
  FileSystemOperationRunner::OperationID id =
      file_system_context_->operation_runner()->Write(
          URLForPath(base::FilePath(FILE_PATH_LITERAL("nonexist"))),
          blob.GetBlobDataHandle(), 0, RecordWriteCallback());
  file_system_context_->operation_runner()->Cancel(id, RecordCancelCallback());
  // We use RunAllPendings() instead of Run() here, because we won't dispatch
  // callbacks after Cancel() is issued (so no chance to Quit) nor do we need
  // to run another write cycle.
  RunUntilIdle();

  // Issued Cancel() before receiving any response from Write(),
  // so nothing should have happen.
  EXPECT_EQ(0, bytes_written());
  EXPECT_EQ(base::File::FILE_ERROR_ABORT, status());
  EXPECT_EQ(base::File::FILE_OK, cancel_status());
  EXPECT_TRUE(complete());

  EXPECT_EQ(0, change_observer()->get_and_reset_modify_file_count());
}

// TODO(ericu,dmikurube,kinuko): Add more tests for cancel cases.

}  // namespace storage
