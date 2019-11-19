// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "storage/browser/file_system/file_system_backend.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_operation_runner.h"
#include "storage/browser/test/mock_special_storage_policy.h"
#include "storage/browser/test/test_file_system_context.h"
#include "storage/browser/test/test_file_system_options.h"
#include "testing/gtest/include/gtest/gtest.h"

using storage::FileSystemContext;
using storage::FileSystemOperationRunner;
using storage::FileSystemType;
using storage::FileSystemURL;

namespace content {

namespace {

void GetStatus(bool* done,
               base::File::Error* status_out,
               base::File::Error status) {
  ASSERT_FALSE(*done);
  *done = true;
  *status_out = status;
}

void GetCancelStatus(bool* operation_done,
                     bool* cancel_done,
                     base::File::Error* status_out,
                     base::File::Error status) {
  // Cancel callback must be always called after the operation's callback.
  ASSERT_TRUE(*operation_done);
  ASSERT_FALSE(*cancel_done);
  *cancel_done = true;
  *status_out = status;
}

void DidOpenFile(base::File file, base::OnceClosure on_close_callback) {}

}  // namespace

class FileSystemOperationRunnerTest : public testing::Test {
 protected:
  FileSystemOperationRunnerTest() {}
  ~FileSystemOperationRunnerTest() override {}

  void SetUp() override {
    ASSERT_TRUE(base_.CreateUniqueTempDir());
    base::FilePath base_dir = base_.GetPath();
    file_system_context_ = CreateFileSystemContextForTesting(nullptr, base_dir);
  }

  void TearDown() override {
    file_system_context_ = nullptr;
    base::RunLoop().RunUntilIdle();
  }

  FileSystemURL URL(const std::string& path) {
    return file_system_context_->CreateCrackedFileSystemURL(
        GURL("http://example.com"), storage::kFileSystemTypeTemporary,
        base::FilePath::FromUTF8Unsafe(path));
  }

  FileSystemOperationRunner* operation_runner() {
    return file_system_context_->operation_runner();
  }

 private:
  base::ScopedTempDir base_;
  base::test::SingleThreadTaskEnvironment task_environment_;
  scoped_refptr<FileSystemContext> file_system_context_;

  DISALLOW_COPY_AND_ASSIGN(FileSystemOperationRunnerTest);
};

TEST_F(FileSystemOperationRunnerTest, NotFoundError) {
  bool done = false;
  base::File::Error status = base::File::FILE_ERROR_FAILED;

  // Regular NOT_FOUND error, which is called asynchronously.
  operation_runner()->Truncate(URL("foo"), 0,
                               base::BindOnce(&GetStatus, &done, &status));
  ASSERT_FALSE(done);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(done);
  ASSERT_EQ(base::File::FILE_ERROR_NOT_FOUND, status);
}

TEST_F(FileSystemOperationRunnerTest, InvalidURLError) {
  bool done = false;
  base::File::Error status = base::File::FILE_ERROR_FAILED;

  // Invalid URL error, which calls DidFinish synchronously.
  operation_runner()->Truncate(FileSystemURL(), 0,
                               base::BindOnce(&GetStatus, &done, &status));
  // The error call back shouldn't be fired synchronously.
  ASSERT_FALSE(done);

  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(done);
  ASSERT_EQ(base::File::FILE_ERROR_INVALID_URL, status);
}

TEST_F(FileSystemOperationRunnerTest, NotFoundErrorAndCancel) {
  bool done = false;
  bool cancel_done = false;
  base::File::Error status = base::File::FILE_ERROR_FAILED;
  base::File::Error cancel_status = base::File::FILE_ERROR_FAILED;

  // Call Truncate with non-existent URL, and try to cancel it immediately
  // after that (before its callback is fired).
  FileSystemOperationRunner::OperationID id = operation_runner()->Truncate(
      URL("foo"), 0, base::BindOnce(&GetStatus, &done, &status));
  operation_runner()->Cancel(id, base::BindOnce(&GetCancelStatus, &done,
                                                &cancel_done, &cancel_status));

  ASSERT_FALSE(done);
  ASSERT_FALSE(cancel_done);
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(done);
  ASSERT_TRUE(cancel_done);
  ASSERT_EQ(base::File::FILE_ERROR_NOT_FOUND, status);
  ASSERT_EQ(base::File::FILE_ERROR_INVALID_OPERATION, cancel_status);
}

TEST_F(FileSystemOperationRunnerTest, InvalidURLErrorAndCancel) {
  bool done = false;
  bool cancel_done = false;
  base::File::Error status = base::File::FILE_ERROR_FAILED;
  base::File::Error cancel_status = base::File::FILE_ERROR_FAILED;

  // Call Truncate with invalid URL, and try to cancel it immediately
  // after that (before its callback is fired).
  FileSystemOperationRunner::OperationID id = operation_runner()->Truncate(
      FileSystemURL(), 0, base::BindOnce(&GetStatus, &done, &status));
  operation_runner()->Cancel(id, base::BindOnce(&GetCancelStatus, &done,
                                                &cancel_done, &cancel_status));

  ASSERT_FALSE(done);
  ASSERT_FALSE(cancel_done);
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(done);
  ASSERT_TRUE(cancel_done);
  ASSERT_EQ(base::File::FILE_ERROR_INVALID_URL, status);
  ASSERT_EQ(base::File::FILE_ERROR_INVALID_OPERATION, cancel_status);
}

TEST_F(FileSystemOperationRunnerTest, CancelWithInvalidId) {
  const FileSystemOperationRunner::OperationID kInvalidId = -1;
  bool done = true;  // The operation is not running.
  bool cancel_done = false;
  base::File::Error cancel_status = base::File::FILE_ERROR_FAILED;
  operation_runner()->Cancel(
      kInvalidId,
      base::BindOnce(&GetCancelStatus, &done, &cancel_done, &cancel_status));

  ASSERT_TRUE(cancel_done);
  ASSERT_EQ(base::File::FILE_ERROR_INVALID_OPERATION, cancel_status);
}

class MultiThreadFileSystemOperationRunnerTest : public testing::Test {
 public:
  MultiThreadFileSystemOperationRunnerTest()
      : task_environment_(content::BrowserTaskEnvironment::IO_MAINLOOP) {}

  void SetUp() override {
    ASSERT_TRUE(base_.CreateUniqueTempDir());

    base::FilePath base_dir = base_.GetPath();
    file_system_context_ = new FileSystemContext(
        base::ThreadTaskRunnerHandle::Get().get(),
        base::CreateSequencedTaskRunner({base::ThreadPool(), base::MayBlock()})
            .get(),
        storage::ExternalMountPoints::CreateRefCounted().get(),
        base::MakeRefCounted<MockSpecialStoragePolicy>().get(), nullptr,
        std::vector<std::unique_ptr<storage::FileSystemBackend>>(),
        std::vector<storage::URLRequestAutoMountHandler>(), base_dir,
        CreateAllowFileAccessOptions());

    // Disallow IO on the main loop.
    base::ThreadRestrictions::SetIOAllowed(false);
  }

  void TearDown() override {
    base::ThreadRestrictions::SetIOAllowed(true);
    file_system_context_ = nullptr;
  }

  FileSystemURL URL(const std::string& path) {
    return file_system_context_->CreateCrackedFileSystemURL(
        GURL("http://example.com"), storage::kFileSystemTypeTemporary,
        base::FilePath::FromUTF8Unsafe(path));
  }

  FileSystemOperationRunner* operation_runner() {
    return file_system_context_->operation_runner();
  }

 private:
  base::ScopedTempDir base_;
  content::BrowserTaskEnvironment task_environment_;
  scoped_refptr<FileSystemContext> file_system_context_;

  DISALLOW_COPY_AND_ASSIGN(MultiThreadFileSystemOperationRunnerTest);
};

TEST_F(MultiThreadFileSystemOperationRunnerTest, OpenAndShutdown) {
  // Call OpenFile and immediately shutdown the runner.
  operation_runner()->OpenFile(
      URL("foo"), base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE,
      base::BindOnce(&DidOpenFile));
  operation_runner()->Shutdown();

  // Wait until the task posted on the blocking thread is done.
  base::ThreadPoolInstance::Get()->FlushForTesting();
  // This should finish without thread assertion failure on debug build.
}

}  // namespace content
