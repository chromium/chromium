// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/file_system/recursive_operation_delegate.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "storage/browser/file_system/file_system_file_util.h"
#include "storage/browser/file_system/file_system_operation.h"
#include "storage/browser/file_system/file_system_operation_runner.h"
#include "storage/browser/test/mock_quota_manager.h"
#include "storage/browser/test/mock_quota_manager_proxy.h"
#include "storage/browser/test/mock_special_storage_policy.h"
#include "storage/browser/test/sandbox_file_system_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace storage {

namespace {

class LoggingRecursiveOperation final : public RecursiveOperationDelegate {
 public:
  struct LogEntry {
    enum Type { PROCESS_FILE, PROCESS_DIRECTORY, POST_PROCESS_DIRECTORY };
    Type type;
    FileSystemURL url;
  };

  LoggingRecursiveOperation(FileSystemContext* file_system_context,
                            const FileSystemURL& root,
                            StatusCallback callback)
      : RecursiveOperationDelegate(file_system_context),
        root_(root),
        callback_(std::move(callback)) {}

  LoggingRecursiveOperation(const LoggingRecursiveOperation&) = delete;
  LoggingRecursiveOperation& operator=(const LoggingRecursiveOperation&) =
      delete;

  ~LoggingRecursiveOperation() override = default;

  const std::vector<LogEntry>& log_entries() const { return log_entries_; }

  // RecursiveOperationDelegate overrides.
  void Run() override { NOTREACHED(); }

  void RunRecursively() override {
    StartRecursiveOperation(root_, FileSystemOperation::ERROR_BEHAVIOR_ABORT,
                            std::move(callback_));
  }

  void RunRecursivelyWithIgnoringError() {
    StartRecursiveOperation(root_, FileSystemOperation::ERROR_BEHAVIOR_SKIP,
                            std::move(callback_));
  }

  void ProcessFile(const FileSystemURL& url, StatusCallback callback) override {
    RecordLogEntry(LogEntry::PROCESS_FILE, url);

    if (error_url_.is_valid() && error_url_ == url) {
      std::move(callback).Run(base::File::FILE_ERROR_FAILED);
      return;
    }

    operation_runner()->GetMetadata(
        url, {FileSystemOperation::GetMetadataField::kIsDirectory},
        base::BindOnce(&LoggingRecursiveOperation::DidGetMetadata,
                       weak_factory_.GetWeakPtr(), std::move(callback)));
  }

  void ProcessDirectory(const FileSystemURL& url,
                        StatusCallback callback) override {
    RecordLogEntry(LogEntry::PROCESS_DIRECTORY, url);
    std::move(callback).Run(base::File::FILE_OK);
  }

  void PostProcessDirectory(const FileSystemURL& url,
                            StatusCallback callback) override {
    RecordLogEntry(LogEntry::POST_PROCESS_DIRECTORY, url);
    std::move(callback).Run(base::File::FILE_OK);
  }

  base::WeakPtr<RecursiveOperationDelegate> AsWeakPtr() override {
    return weak_factory_.GetWeakPtr();
  }

  void SetEntryToFail(const FileSystemURL& url) { error_url_ = url; }

 private:
  void RecordLogEntry(LogEntry::Type type, const FileSystemURL& url) {
    LogEntry entry;
    entry.type = type;
    entry.url = url;
    log_entries_.push_back(entry);
  }

  void DidGetMetadata(StatusCallback callback,
                      base::File::Error result,
                      const base::File::Info& file_info) {
    if (result != base::File::FILE_OK) {
      std::move(callback).Run(result);
      return;
    }

    std::move(callback).Run(file_info.is_directory
                                ? base::File::FILE_ERROR_NOT_A_FILE
                                : base::File::FILE_OK);
  }

  FileSystemURL root_;
  StatusCallback callback_;
  std::vector<LogEntry> log_entries_;
  FileSystemURL error_url_;

  base::WeakPtrFactory<LoggingRecursiveOperation> weak_factory_{this};
};

void ReportStatus(base::File::Error* out_error, base::File::Error error) {
  DCHECK(out_error);
  *out_error = error;
}

// To test the Cancel() during operation, calls Cancel() of |operation|
// after |counter| times message posting.
void CallCancelLater(RecursiveOperationDelegate* operation, int counter) {
  if (counter > 0) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&CallCancelLater, base::Unretained(operation),
                                  counter - 1));
    return;
  }

  operation->Cancel();
}

}  // namespace

class RecursiveOperationDelegateTest : public testing::Test {
 protected:
  void SetUp() override {
    EXPECT_TRUE(base_.CreateUniqueTempDir());
    base::FilePath base_dir = base_.GetPath().AppendASCII("filesystem");
    quota_manager_ = base::MakeRefCounted<storage::MockQuotaManager>(
        /*is_incognito=*/false, base_dir,
        base::SingleThreadTaskRunner::GetCurrentDefault(),
        base::MakeRefCounted<storage::MockSpecialStoragePolicy>());
    quota_manager_proxy_ = base::MakeRefCounted<storage::MockQuotaManagerProxy>(
        quota_manager_.get(),
        base::SingleThreadTaskRunner::GetCurrentDefault());
    sandbox_file_system_.SetUp(base_dir, quota_manager_proxy_);
  }

  void TearDown() override { sandbox_file_system_.TearDown(); }

  std::unique_ptr<FileSystemOperationContext> NewContext() {
    auto context = sandbox_file_system_.NewOperationContext();
    // Grant enough quota for all test cases.
    context->set_allowed_bytes_growth(1000000);
    return context;
  }

  FileSystemFileUtil* file_util() { return sandbox_file_system_.file_util(); }

  FileSystemURL URLForPath(const std::string& path) const {
    return sandbox_file_system_.CreateURLFromUTF8(path);
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

 private:
  base::test::TaskEnvironment task_environment_;

  // Common temp base for nondestructive uses.
  base::ScopedTempDir base_;
  SandboxFileSystemTestHelper sandbox_file_system_;
  scoped_refptr<storage::MockQuotaManager> quota_manager_;
  scoped_refptr<storage::MockQuotaManagerProxy> quota_manager_proxy_;
};

TEST_F(RecursiveOperationDelegateTest, RootIsFile) {
  FileSystemURL src_file(CreateFile("src"));

  base::File::Error error = base::File::FILE_ERROR_FAILED;
  std::unique_ptr<FileSystemOperationContext> context = NewContext();
  auto operation = std::make_unique<LoggingRecursiveOperation>(
      context->file_system_context(), src_file,
      base::BindOnce(&ReportStatus, &error));
  operation->RunRecursively();
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(base::File::FILE_OK, error);

  const std::vector<LoggingRecursiveOperation::LogEntry>& log_entries =
      operation->log_entries();
  ASSERT_EQ(1U, log_entries.size());
  const LoggingRecursiveOperation::LogEntry& entry = log_entries[0];
  EXPECT_EQ(LoggingRecursiveOperation::LogEntry::PROCESS_FILE, entry.type);
  EXPECT_EQ(src_file, entry.url);
}

TEST_F(RecursiveOperationDelegateTest, RootIsDirectory) {
  FileSystemURL src_root(CreateDirectory("src"));
  FileSystemURL src_dir1(CreateDirectory("src/dir1"));
  FileSystemURL src_file1(CreateFile("src/file1"));
  FileSystemURL src_file2(CreateFile("src/dir1/file2"));
  FileSystemURL src_file3(CreateFile("src/dir1/file3"));

  base::File::Error error = base::File::FILE_ERROR_FAILED;
  std::unique_ptr<FileSystemOperationContext> context = NewContext();
  auto operation = std::make_unique<LoggingRecursiveOperation>(
      context->file_system_context(), src_root,
      base::BindOnce(&ReportStatus, &error));
  operation->RunRecursively();
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(base::File::FILE_OK, error);

  const std::vector<LoggingRecursiveOperation::LogEntry>& log_entries =
      operation->log_entries();
  ASSERT_EQ(8U, log_entries.size());

  EXPECT_EQ(LoggingRecursiveOperation::LogEntry::PROCESS_FILE,
            log_entries[0].type);
  EXPECT_EQ(src_root, log_entries[0].url);

  EXPECT_EQ(LoggingRecursiveOperation::LogEntry::PROCESS_DIRECTORY,
            log_entries[1].type);
  EXPECT_EQ(src_root, log_entries[1].url);

  EXPECT_EQ(LoggingRecursiveOperation::LogEntry::PROCESS_FILE,
            log_entries[2].type);
  EXPECT_EQ(src_file1, log_entries[2].url);

  EXPECT_EQ(LoggingRecursiveOperation::LogEntry::PROCESS_DIRECTORY,
            log_entries[3].type);
  EXPECT_EQ(src_dir1, log_entries[3].url);

  // The order of src/dir1/file2 and src/dir1/file3 depends on the file system
  // implementation (can be swapped).
  EXPECT_EQ(LoggingRecursiveOperation::LogEntry::PROCESS_FILE,
            log_entries[4].type);
  EXPECT_EQ(LoggingRecursiveOperation::LogEntry::PROCESS_FILE,
            log_entries[5].type);
  EXPECT_TRUE(
      (src_file2 == log_entries[4].url && src_file3 == log_entries[5].url) ||
      (src_file3 == log_entries[4].url && src_file2 == log_entries[5].url));

  EXPECT_EQ(LoggingRecursiveOperation::LogEntry::POST_PROCESS_DIRECTORY,
            log_entries[6].type);
  EXPECT_EQ(src_dir1, log_entries[6].url);

  EXPECT_EQ(LoggingRecursiveOperation::LogEntry::POST_PROCESS_DIRECTORY,
            log_entries[7].type);
  EXPECT_EQ(src_root, log_entries[7].url);
}

TEST_F(RecursiveOperationDelegateTest, Cancel) {
  FileSystemURL src_root(CreateDirectory("src"));
  FileSystemURL src_dir1(CreateDirectory("src/dir1"));
  FileSystemURL src_file1(CreateFile("src/file1"));
  FileSystemURL src_file2(CreateFile("src/dir1/file2"));

  base::File::Error error = base::File::FILE_ERROR_FAILED;
  std::unique_ptr<FileSystemOperationContext> context = NewContext();
  auto operation = std::make_unique<LoggingRecursiveOperation>(
      context->file_system_context(), src_root,
      base::BindOnce(&ReportStatus, &error));
  operation->RunRecursively();

  // Invoke Cancel(), after 5 times message posting.
  CallCancelLater(operation.get(), 5);
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(base::File::FILE_ERROR_ABORT, error);
}

TEST_F(RecursiveOperationDelegateTest, AbortWithError) {
  FileSystemURL src_root(CreateDirectory("src"));
  FileSystemURL src_dir1(CreateDirectory("src/dir1"));
  FileSystemURL src_file1(CreateFile("src/file1"));
  FileSystemURL src_file2(CreateFile("src/dir1/file2"));
  FileSystemURL src_file3(CreateFile("src/dir1/file3"));

  base::File::Error error = base::File::FILE_ERROR_FAILED;
  std::unique_ptr<FileSystemOperationContext> context = NewContext();
  auto operation = std::make_unique<LoggingRecursiveOperation>(
      context->file_system_context(), src_root,
      base::BindOnce(&ReportStatus, &error));
  operation->SetEntryToFail(src_file1);
  operation->RunRecursively();
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(base::File::FILE_ERROR_FAILED, error);

  // Confirm that operation has been aborted in the middle.
  const std::vector<LoggingRecursiveOperation::LogEntry>& log_entries =
      operation->log_entries();
  ASSERT_EQ(3U, log_entries.size());

  EXPECT_EQ(LoggingRecursiveOperation::LogEntry::PROCESS_FILE,
            log_entries[0].type);
  EXPECT_EQ(src_root, log_entries[0].url);

  EXPECT_EQ(LoggingRecursiveOperation::LogEntry::PROCESS_DIRECTORY,
            log_entries[1].type);
  EXPECT_EQ(src_root, log_entries[1].url);

  EXPECT_EQ(LoggingRecursiveOperation::LogEntry::PROCESS_FILE,
            log_entries[2].type);
  EXPECT_EQ(src_file1, log_entries[2].url);
}

TEST_F(RecursiveOperationDelegateTest, ContinueWithError) {
  FileSystemURL src_root(CreateDirectory("src"));
  FileSystemURL src_dir1(CreateDirectory("src/dir1"));
  FileSystemURL src_file1(CreateFile("src/file1"));
  FileSystemURL src_file2(CreateFile("src/dir1/file2"));
  FileSystemURL src_file3(CreateFile("src/dir1/file3"));

  base::File::Error error = base::File::FILE_ERROR_FAILED;
  std::unique_ptr<FileSystemOperationContext> context = NewContext();
  auto operation = std::make_unique<LoggingRecursiveOperation>(
      context->file_system_context(), src_root,
      base::BindOnce(&ReportStatus, &error));
  operation->SetEntryToFail(src_file1);
  operation->RunRecursivelyWithIgnoringError();
  base::RunLoop().RunUntilIdle();

  // Error code should be base::File::FILE_ERROR_FAILED.
  ASSERT_EQ(base::File::FILE_ERROR_FAILED, error);

  // Confirm that operation continues after the error.
  const std::vector<LoggingRecursiveOperation::LogEntry>& log_entries =
      operation->log_entries();
  ASSERT_EQ(8U, log_entries.size());

  EXPECT_EQ(LoggingRecursiveOperation::LogEntry::PROCESS_FILE,
            log_entries[0].type);
  EXPECT_EQ(src_root, log_entries[0].url);

  EXPECT_EQ(LoggingRecursiveOperation::LogEntry::PROCESS_DIRECTORY,
            log_entries[1].type);
  EXPECT_EQ(src_root, log_entries[1].url);

  EXPECT_EQ(LoggingRecursiveOperation::LogEntry::PROCESS_FILE,
            log_entries[2].type);
  EXPECT_EQ(src_file1, log_entries[2].url);

  EXPECT_EQ(LoggingRecursiveOperation::LogEntry::PROCESS_DIRECTORY,
            log_entries[3].type);
  EXPECT_EQ(src_dir1, log_entries[3].url);

  EXPECT_EQ(LoggingRecursiveOperation::LogEntry::PROCESS_FILE,
            log_entries[4].type);
  EXPECT_EQ(src_file3, log_entries[4].url);

  EXPECT_EQ(LoggingRecursiveOperation::LogEntry::PROCESS_FILE,
            log_entries[5].type);
  EXPECT_EQ(src_file2, log_entries[5].url);

  EXPECT_EQ(LoggingRecursiveOperation::LogEntry::POST_PROCESS_DIRECTORY,
            log_entries[6].type);
  EXPECT_EQ(src_dir1, log_entries[6].url);

  EXPECT_EQ(LoggingRecursiveOperation::LogEntry::POST_PROCESS_DIRECTORY,
            log_entries[7].type);
  EXPECT_EQ(src_root, log_entries[7].url);
}

}  // namespace storage
