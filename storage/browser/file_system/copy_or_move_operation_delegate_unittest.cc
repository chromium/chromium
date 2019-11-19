// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/containers/queue.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/services/filesystem/public/mojom/types.mojom.h"
#include "storage/browser/file_system/copy_or_move_file_validator.h"
#include "storage/browser/file_system/copy_or_move_operation_delegate.h"
#include "storage/browser/file_system/file_stream_reader.h"
#include "storage/browser/file_system/file_stream_writer.h"
#include "storage/browser/file_system/file_system_backend.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_operation.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/browser/quota/quota_manager.h"
#include "storage/browser/test/async_file_test_helper.h"
#include "storage/browser/test/file_system_test_file_set.h"
#include "storage/browser/test/mock_quota_manager.h"
#include "storage/browser/test/mock_quota_manager_proxy.h"
#include "storage/browser/test/test_file_system_backend.h"
#include "storage/browser/test/test_file_system_context.h"
#include "storage/common/file_system/file_system_mount_option.h"
#include "storage/common/file_system/file_system_util.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::AsyncFileTestHelper;
using storage::CopyOrMoveOperationDelegate;
using storage::FileStreamWriter;
using storage::FileSystemOperation;
using storage::FileSystemURL;

namespace content {

using FileEntryList = storage::FileSystemOperation::FileEntryList;

namespace {

void ExpectOk(const GURL& origin_url,
              const std::string& name,
              base::File::Error error) {
  ASSERT_EQ(base::File::FILE_OK, error);
}

class TestValidatorFactory : public storage::CopyOrMoveFileValidatorFactory {
 public:
  // A factory that creates validators that accept everything or nothing.
  TestValidatorFactory() = default;
  ~TestValidatorFactory() override = default;

  storage::CopyOrMoveFileValidator* CreateCopyOrMoveFileValidator(
      const FileSystemURL& /*src_url*/,
      const base::FilePath& /*platform_path*/) override {
    // Move arg management to TestValidator?
    return new TestValidator(true, true, std::string("2"));
  }

 private:
  class TestValidator : public storage::CopyOrMoveFileValidator {
   public:
    explicit TestValidator(bool pre_copy_valid,
                           bool post_copy_valid,
                           const std::string& reject_string)
        : result_(pre_copy_valid ? base::File::FILE_OK
                                 : base::File::FILE_ERROR_SECURITY),
          write_result_(post_copy_valid ? base::File::FILE_OK
                                        : base::File::FILE_ERROR_SECURITY),
          reject_string_(reject_string) {}
    ~TestValidator() override = default;

    void StartPreWriteValidation(
        const ResultCallback& result_callback) override {
      // Post the result since a real validator must do work asynchronously.
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::BindOnce(result_callback, result_));
    }

    void StartPostWriteValidation(
        const base::FilePath& dest_platform_path,
        const ResultCallback& result_callback) override {
      base::File::Error result = write_result_;
      std::string unsafe = dest_platform_path.BaseName().AsUTF8Unsafe();
      if (unsafe.find(reject_string_) != std::string::npos) {
        result = base::File::FILE_ERROR_SECURITY;
      }
      // Post the result since a real validator must do work asynchronously.
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::BindOnce(result_callback, result));
    }

   private:
    base::File::Error result_;
    base::File::Error write_result_;
    std::string reject_string_;

    DISALLOW_COPY_AND_ASSIGN(TestValidator);
  };
};

// Records CopyProgressCallback invocations.
struct ProgressRecord {
  storage::FileSystemOperation::CopyProgressType type;
  FileSystemURL source_url;
  FileSystemURL dest_url;
  int64_t size;
};

void RecordProgressCallback(std::vector<ProgressRecord>* records,
                            storage::FileSystemOperation::CopyProgressType type,
                            const FileSystemURL& source_url,
                            const FileSystemURL& dest_url,
                            int64_t size) {
  ProgressRecord record;
  record.type = type;
  record.source_url = source_url;
  record.dest_url = dest_url;
  record.size = size;
  records->push_back(record);
}

void RecordFileProgressCallback(std::vector<int64_t>* records,
                                int64_t progress) {
  records->push_back(progress);
}

void AssignAndQuit(base::RunLoop* run_loop,
                   base::File::Error* result_out,
                   base::File::Error result) {
  *result_out = result;
  run_loop->Quit();
}

class ScopedThreadStopper {
 public:
  ScopedThreadStopper(base::Thread* thread) : thread_(thread) {}

  ~ScopedThreadStopper() {
    if (thread_) {
      // Give another chance for deleted streams to perform Close.
      base::RunLoop run_loop;
      thread_->task_runner()->PostTaskAndReply(FROM_HERE, base::DoNothing(),
                                               run_loop.QuitClosure());
      run_loop.Run();
      thread_->Stop();
    }
  }

  bool is_valid() const { return thread_; }

 private:
  base::Thread* thread_;
  DISALLOW_COPY_AND_ASSIGN(ScopedThreadStopper);
};

}  // namespace

class CopyOrMoveOperationTestHelper {
 public:
  CopyOrMoveOperationTestHelper(const GURL& origin,
                                storage::FileSystemType src_type,
                                storage::FileSystemType dest_type)
      : origin_(origin),
        src_type_(src_type),
        dest_type_(dest_type),
        task_environment_(base::test::TaskEnvironment::MainThreadType::IO) {}

  ~CopyOrMoveOperationTestHelper() {
    file_system_context_ = nullptr;
    quota_manager_proxy_->SimulateQuotaManagerDestroyed();
    quota_manager_ = nullptr;
    quota_manager_proxy_ = nullptr;
    task_environment_.RunUntilIdle();
  }

  void SetUp() { SetUp(true, true); }

  void SetUpNoValidator() { SetUp(true, false); }

  void SetUp(bool require_copy_or_move_validator,
             bool init_copy_or_move_validator) {
    ASSERT_TRUE(base_.CreateUniqueTempDir());
    base::FilePath base_dir = base_.GetPath();
    quota_manager_ =
        new MockQuotaManager(false /* is_incognito */, base_dir,
                             base::ThreadTaskRunnerHandle::Get().get(),
                             nullptr /* special storage policy */);
    quota_manager_proxy_ = new MockQuotaManagerProxy(
        quota_manager_.get(), base::ThreadTaskRunnerHandle::Get().get());
    file_system_context_ =
        CreateFileSystemContextForTesting(quota_manager_proxy_.get(), base_dir);

    // Prepare the origin's root directory.
    storage::FileSystemBackend* backend =
        file_system_context_->GetFileSystemBackend(src_type_);
    backend->ResolveURL(
        FileSystemURL::CreateForTest(url::Origin::Create(origin_), src_type_,
                                     base::FilePath()),
        storage::OPEN_FILE_SYSTEM_CREATE_IF_NONEXISTENT,
        base::BindOnce(&ExpectOk));
    backend = file_system_context_->GetFileSystemBackend(dest_type_);
    if (dest_type_ == storage::kFileSystemTypeTest) {
      TestFileSystemBackend* test_backend =
          static_cast<TestFileSystemBackend*>(backend);
      std::unique_ptr<storage::CopyOrMoveFileValidatorFactory> factory(
          new TestValidatorFactory);
      test_backend->set_require_copy_or_move_validator(
          require_copy_or_move_validator);
      if (init_copy_or_move_validator)
        test_backend->InitializeCopyOrMoveFileValidatorFactory(
            std::move(factory));
    }
    backend->ResolveURL(
        FileSystemURL::CreateForTest(url::Origin::Create(origin_), dest_type_,
                                     base::FilePath()),
        storage::OPEN_FILE_SYSTEM_CREATE_IF_NONEXISTENT,
        base::BindOnce(&ExpectOk));
    task_environment_.RunUntilIdle();

    // Grant relatively big quota initially.
    quota_manager_->SetQuota(
        url::Origin::Create(origin_),
        storage::FileSystemTypeToQuotaStorageType(src_type_), 1024 * 1024);
    quota_manager_->SetQuota(
        url::Origin::Create(origin_),
        storage::FileSystemTypeToQuotaStorageType(dest_type_), 1024 * 1024);
  }

  int64_t GetSourceUsage() {
    int64_t usage = 0;
    GetUsageAndQuota(src_type_, &usage, nullptr);
    return usage;
  }

  int64_t GetDestUsage() {
    int64_t usage = 0;
    GetUsageAndQuota(dest_type_, &usage, nullptr);
    return usage;
  }

  FileSystemURL SourceURL(const std::string& path) {
    return file_system_context_->CreateCrackedFileSystemURL(
        origin_, src_type_, base::FilePath::FromUTF8Unsafe(path));
  }

  FileSystemURL DestURL(const std::string& path) {
    return file_system_context_->CreateCrackedFileSystemURL(
        origin_, dest_type_, base::FilePath::FromUTF8Unsafe(path));
  }

  base::File::Error Copy(const FileSystemURL& src, const FileSystemURL& dest) {
    return AsyncFileTestHelper::Copy(file_system_context_.get(), src, dest);
  }

  base::File::Error CopyWithProgress(
      const FileSystemURL& src,
      const FileSystemURL& dest,
      const AsyncFileTestHelper::CopyProgressCallback& progress_callback) {
    return AsyncFileTestHelper::CopyWithProgress(file_system_context_.get(),
                                                 src, dest, progress_callback);
  }

  base::File::Error Move(const FileSystemURL& src, const FileSystemURL& dest) {
    return AsyncFileTestHelper::Move(file_system_context_.get(), src, dest);
  }

  base::File::Error SetUpTestCaseFiles(
      const FileSystemURL& root,
      const FileSystemTestCaseRecord* const test_cases,
      size_t test_case_size) {
    base::File::Error result = base::File::FILE_ERROR_FAILED;
    for (size_t i = 0; i < test_case_size; ++i) {
      const FileSystemTestCaseRecord& test_case = test_cases[i];
      FileSystemURL url = file_system_context_->CreateCrackedFileSystemURL(
          root.origin().GetURL(), root.mount_type(),
          root.virtual_path().Append(test_case.path));
      if (test_case.is_directory)
        result = CreateDirectory(url);
      else
        result = CreateFile(url, test_case.data_file_size);
      EXPECT_EQ(base::File::FILE_OK, result) << url.DebugString();
      if (result != base::File::FILE_OK)
        return result;
    }
    return result;
  }

  void VerifyTestCaseFiles(const FileSystemURL& root,
                           const FileSystemTestCaseRecord* const test_cases,
                           size_t test_case_size) {
    std::map<base::FilePath, const FileSystemTestCaseRecord*> test_case_map;
    for (size_t i = 0; i < test_case_size; ++i) {
      test_case_map[base::FilePath(test_cases[i].path)
                        .NormalizePathSeparators()] = &test_cases[i];
    }

    base::queue<FileSystemURL> directories;
    FileEntryList entries;
    directories.push(root);
    while (!directories.empty()) {
      FileSystemURL dir = directories.front();
      directories.pop();
      ASSERT_EQ(base::File::FILE_OK, ReadDirectory(dir, &entries));
      for (size_t i = 0; i < entries.size(); ++i) {
        FileSystemURL url = file_system_context_->CreateCrackedFileSystemURL(
            dir.origin().GetURL(), dir.mount_type(),
            dir.virtual_path().Append(entries[i].name));
        base::FilePath relative;
        root.virtual_path().AppendRelativePath(url.virtual_path(), &relative);
        relative = relative.NormalizePathSeparators();
        ASSERT_TRUE(base::Contains(test_case_map, relative));
        if (entries[i].type == filesystem::mojom::FsFileType::DIRECTORY) {
          EXPECT_TRUE(test_case_map[relative]->is_directory);
          directories.push(url);
        } else {
          EXPECT_FALSE(test_case_map[relative]->is_directory);
          EXPECT_TRUE(FileExists(url, test_case_map[relative]->data_file_size));
        }
        test_case_map.erase(relative);
      }
    }
    EXPECT_TRUE(test_case_map.empty());
    for (const auto& path_record_pair : test_case_map) {
      LOG(ERROR) << "Extra entry: "
                 << path_record_pair.first.LossyDisplayName();
    }
  }

  base::File::Error ReadDirectory(const FileSystemURL& url,
                                  FileEntryList* entries) {
    return AsyncFileTestHelper::ReadDirectory(file_system_context_.get(), url,
                                              entries);
  }

  base::File::Error CreateDirectory(const FileSystemURL& url) {
    return AsyncFileTestHelper::CreateDirectory(file_system_context_.get(),
                                                url);
  }

  base::File::Error CreateFile(const FileSystemURL& url, size_t size) {
    base::File::Error result =
        AsyncFileTestHelper::CreateFile(file_system_context_.get(), url);
    if (result != base::File::FILE_OK)
      return result;
    return AsyncFileTestHelper::TruncateFile(file_system_context_.get(), url,
                                             size);
  }

  bool FileExists(const FileSystemURL& url, int64_t expected_size) {
    return AsyncFileTestHelper::FileExists(file_system_context_.get(), url,
                                           expected_size);
  }

  bool DirectoryExists(const FileSystemURL& url) {
    return AsyncFileTestHelper::DirectoryExists(file_system_context_.get(),
                                                url);
  }

 private:
  void GetUsageAndQuota(storage::FileSystemType type,
                        int64_t* usage,
                        int64_t* quota) {
    blink::mojom::QuotaStatusCode status =
        AsyncFileTestHelper::GetUsageAndQuota(quota_manager_.get(),
                                              url::Origin::Create(origin_),
                                              type, usage, quota);
    ASSERT_EQ(blink::mojom::QuotaStatusCode::kOk, status);
  }

 private:
  base::ScopedTempDir base_;

  const GURL origin_;
  const storage::FileSystemType src_type_;
  const storage::FileSystemType dest_type_;

  base::test::TaskEnvironment task_environment_;
  scoped_refptr<storage::FileSystemContext> file_system_context_;
  scoped_refptr<MockQuotaManagerProxy> quota_manager_proxy_;
  scoped_refptr<MockQuotaManager> quota_manager_;

  DISALLOW_COPY_AND_ASSIGN(CopyOrMoveOperationTestHelper);
};

TEST(LocalFileSystemCopyOrMoveOperationTest, CopySingleFile) {
  CopyOrMoveOperationTestHelper helper(GURL("http://foo"),
                                       storage::kFileSystemTypeTemporary,
                                       storage::kFileSystemTypePersistent);
  helper.SetUp();

  FileSystemURL src = helper.SourceURL("a");
  FileSystemURL dest = helper.DestURL("b");
  int64_t src_initial_usage = helper.GetSourceUsage();
  int64_t dest_initial_usage = helper.GetDestUsage();

  // Set up a source file.
  ASSERT_EQ(base::File::FILE_OK, helper.CreateFile(src, 10));
  int64_t src_increase = helper.GetSourceUsage() - src_initial_usage;

  // Copy it.
  ASSERT_EQ(base::File::FILE_OK, helper.Copy(src, dest));

  // Verify.
  ASSERT_TRUE(helper.FileExists(src, 10));
  ASSERT_TRUE(helper.FileExists(dest, 10));

  int64_t src_new_usage = helper.GetSourceUsage();
  ASSERT_EQ(src_initial_usage + src_increase, src_new_usage);

  int64_t dest_increase = helper.GetDestUsage() - dest_initial_usage;
  ASSERT_EQ(src_increase, dest_increase);
}

TEST(LocalFileSystemCopyOrMoveOperationTest, MoveSingleFile) {
  CopyOrMoveOperationTestHelper helper(GURL("http://foo"),
                                       storage::kFileSystemTypeTemporary,
                                       storage::kFileSystemTypePersistent);
  helper.SetUp();

  FileSystemURL src = helper.SourceURL("a");
  FileSystemURL dest = helper.DestURL("b");
  int64_t src_initial_usage = helper.GetSourceUsage();
  int64_t dest_initial_usage = helper.GetDestUsage();

  // Set up a source file.
  ASSERT_EQ(base::File::FILE_OK, helper.CreateFile(src, 10));
  int64_t src_increase = helper.GetSourceUsage() - src_initial_usage;

  // Move it.
  ASSERT_EQ(base::File::FILE_OK, helper.Move(src, dest));

  // Verify.
  ASSERT_FALSE(helper.FileExists(src, AsyncFileTestHelper::kDontCheckSize));
  ASSERT_TRUE(helper.FileExists(dest, 10));

  int64_t src_new_usage = helper.GetSourceUsage();
  ASSERT_EQ(src_initial_usage, src_new_usage);

  int64_t dest_increase = helper.GetDestUsage() - dest_initial_usage;
  ASSERT_EQ(src_increase, dest_increase);
}

TEST(LocalFileSystemCopyOrMoveOperationTest, CopySingleDirectory) {
  CopyOrMoveOperationTestHelper helper(GURL("http://foo"),
                                       storage::kFileSystemTypeTemporary,
                                       storage::kFileSystemTypePersistent);
  helper.SetUp();

  FileSystemURL src = helper.SourceURL("a");
  FileSystemURL dest = helper.DestURL("b");
  int64_t src_initial_usage = helper.GetSourceUsage();
  int64_t dest_initial_usage = helper.GetDestUsage();

  // Set up a source directory.
  ASSERT_EQ(base::File::FILE_OK, helper.CreateDirectory(src));
  int64_t src_increase = helper.GetSourceUsage() - src_initial_usage;

  // Copy it.
  ASSERT_EQ(base::File::FILE_OK, helper.Copy(src, dest));

  // Verify.
  ASSERT_TRUE(helper.DirectoryExists(src));
  ASSERT_TRUE(helper.DirectoryExists(dest));

  int64_t src_new_usage = helper.GetSourceUsage();
  ASSERT_EQ(src_initial_usage + src_increase, src_new_usage);

  int64_t dest_increase = helper.GetDestUsage() - dest_initial_usage;
  ASSERT_EQ(src_increase, dest_increase);
}

TEST(LocalFileSystemCopyOrMoveOperationTest, MoveSingleDirectory) {
  CopyOrMoveOperationTestHelper helper(GURL("http://foo"),
                                       storage::kFileSystemTypeTemporary,
                                       storage::kFileSystemTypePersistent);
  helper.SetUp();

  FileSystemURL src = helper.SourceURL("a");
  FileSystemURL dest = helper.DestURL("b");
  int64_t src_initial_usage = helper.GetSourceUsage();
  int64_t dest_initial_usage = helper.GetDestUsage();

  // Set up a source directory.
  ASSERT_EQ(base::File::FILE_OK, helper.CreateDirectory(src));
  int64_t src_increase = helper.GetSourceUsage() - src_initial_usage;

  // Move it.
  ASSERT_EQ(base::File::FILE_OK, helper.Move(src, dest));

  // Verify.
  ASSERT_FALSE(helper.DirectoryExists(src));
  ASSERT_TRUE(helper.DirectoryExists(dest));

  int64_t src_new_usage = helper.GetSourceUsage();
  ASSERT_EQ(src_initial_usage, src_new_usage);

  int64_t dest_increase = helper.GetDestUsage() - dest_initial_usage;
  ASSERT_EQ(src_increase, dest_increase);
}

TEST(LocalFileSystemCopyOrMoveOperationTest, CopyDirectory) {
  CopyOrMoveOperationTestHelper helper(GURL("http://foo"),
                                       storage::kFileSystemTypeTemporary,
                                       storage::kFileSystemTypePersistent);
  helper.SetUp();

  FileSystemURL src = helper.SourceURL("a");
  FileSystemURL dest = helper.DestURL("b");
  int64_t src_initial_usage = helper.GetSourceUsage();
  int64_t dest_initial_usage = helper.GetDestUsage();

  // Set up a source directory.
  ASSERT_EQ(base::File::FILE_OK, helper.CreateDirectory(src));
  ASSERT_EQ(base::File::FILE_OK,
            helper.SetUpTestCaseFiles(src, kRegularFileSystemTestCases,
                                      kRegularFileSystemTestCaseSize));
  int64_t src_increase = helper.GetSourceUsage() - src_initial_usage;

  // Copy it.
  ASSERT_EQ(base::File::FILE_OK,
            helper.CopyWithProgress(
                src, dest, AsyncFileTestHelper::CopyProgressCallback()));

  // Verify.
  ASSERT_TRUE(helper.DirectoryExists(src));
  ASSERT_TRUE(helper.DirectoryExists(dest));

  helper.VerifyTestCaseFiles(dest, kRegularFileSystemTestCases,
                             kRegularFileSystemTestCaseSize);

  int64_t src_new_usage = helper.GetSourceUsage();
  ASSERT_EQ(src_initial_usage + src_increase, src_new_usage);

  int64_t dest_increase = helper.GetDestUsage() - dest_initial_usage;
  ASSERT_EQ(src_increase, dest_increase);
}

TEST(LocalFileSystemCopyOrMoveOperationTest, MoveDirectory) {
  CopyOrMoveOperationTestHelper helper(GURL("http://foo"),
                                       storage::kFileSystemTypeTemporary,
                                       storage::kFileSystemTypePersistent);
  helper.SetUp();

  FileSystemURL src = helper.SourceURL("a");
  FileSystemURL dest = helper.DestURL("b");
  int64_t src_initial_usage = helper.GetSourceUsage();
  int64_t dest_initial_usage = helper.GetDestUsage();

  // Set up a source directory.
  ASSERT_EQ(base::File::FILE_OK, helper.CreateDirectory(src));
  ASSERT_EQ(base::File::FILE_OK,
            helper.SetUpTestCaseFiles(src, kRegularFileSystemTestCases,
                                      kRegularFileSystemTestCaseSize));
  int64_t src_increase = helper.GetSourceUsage() - src_initial_usage;

  // Move it.
  ASSERT_EQ(base::File::FILE_OK, helper.Move(src, dest));

  // Verify.
  ASSERT_FALSE(helper.DirectoryExists(src));
  ASSERT_TRUE(helper.DirectoryExists(dest));

  helper.VerifyTestCaseFiles(dest, kRegularFileSystemTestCases,
                             kRegularFileSystemTestCaseSize);

  int64_t src_new_usage = helper.GetSourceUsage();
  ASSERT_EQ(src_initial_usage, src_new_usage);

  int64_t dest_increase = helper.GetDestUsage() - dest_initial_usage;
  ASSERT_EQ(src_increase, dest_increase);
}

TEST(LocalFileSystemCopyOrMoveOperationTest,
     MoveDirectoryFailPostWriteValidation) {
  CopyOrMoveOperationTestHelper helper(GURL("http://foo"),
                                       storage::kFileSystemTypeTemporary,
                                       storage::kFileSystemTypeTest);
  helper.SetUp();

  FileSystemURL src = helper.SourceURL("a");
  FileSystemURL dest = helper.DestURL("b");

  // Set up a source directory.
  ASSERT_EQ(base::File::FILE_OK, helper.CreateDirectory(src));
  ASSERT_EQ(base::File::FILE_OK,
            helper.SetUpTestCaseFiles(src, kRegularFileSystemTestCases,
                                      kRegularFileSystemTestCaseSize));

  // Move it.
  helper.Move(src, dest);

  // Verify.
  ASSERT_TRUE(helper.DirectoryExists(src));
  ASSERT_TRUE(helper.DirectoryExists(dest));

  // In the move operation, [file 0, file 2, file 3] are processed as LIFO.
  // After file 3 is processed, file 2 is rejected by the validator and the
  // operation fails. That is, only file 3 should be in dest.
  FileSystemTestCaseRecord kMoveDirResultCases[] = {
      {false, FILE_PATH_LITERAL("file 3"), 0},
  };

  helper.VerifyTestCaseFiles(dest, kMoveDirResultCases,
                             base::size(kMoveDirResultCases));
}

TEST(LocalFileSystemCopyOrMoveOperationTest, CopySingleFileNoValidator) {
  CopyOrMoveOperationTestHelper helper(GURL("http://foo"),
                                       storage::kFileSystemTypeTemporary,
                                       storage::kFileSystemTypeTest);
  helper.SetUpNoValidator();

  FileSystemURL src = helper.SourceURL("a");
  FileSystemURL dest = helper.DestURL("b");

  // Set up a source file.
  ASSERT_EQ(base::File::FILE_OK, helper.CreateFile(src, 10));

  // The copy attempt should fail with a security error -- getting
  // the factory returns a security error, and the copy operation must
  // respect that.
  ASSERT_EQ(base::File::FILE_ERROR_SECURITY, helper.Copy(src, dest));
}

TEST(LocalFileSystemCopyOrMoveOperationTest, ProgressCallback) {
  CopyOrMoveOperationTestHelper helper(GURL("http://foo"),
                                       storage::kFileSystemTypeTemporary,
                                       storage::kFileSystemTypePersistent);
  helper.SetUp();

  FileSystemURL src = helper.SourceURL("a");
  FileSystemURL dest = helper.DestURL("b");

  // Set up a source directory.
  ASSERT_EQ(base::File::FILE_OK, helper.CreateDirectory(src));
  ASSERT_EQ(base::File::FILE_OK,
            helper.SetUpTestCaseFiles(src, kRegularFileSystemTestCases,
                                      kRegularFileSystemTestCaseSize));

  std::vector<ProgressRecord> records;
  ASSERT_EQ(
      base::File::FILE_OK,
      helper.CopyWithProgress(src, dest,
                              base::BindRepeating(&RecordProgressCallback,
                                                  base::Unretained(&records))));

  // Verify progress callback.
  for (size_t i = 0; i < kRegularFileSystemTestCaseSize; ++i) {
    const FileSystemTestCaseRecord& test_case = kRegularFileSystemTestCases[i];

    FileSystemURL src_url = helper.SourceURL(
        std::string("a/") + base::FilePath(test_case.path).AsUTF8Unsafe());
    FileSystemURL dest_url = helper.DestURL(
        std::string("b/") + base::FilePath(test_case.path).AsUTF8Unsafe());

    // Find the first and last progress record.
    size_t begin_index = records.size();
    size_t end_index = records.size();
    for (size_t j = 0; j < records.size(); ++j) {
      if (records[j].source_url == src_url) {
        if (begin_index == records.size())
          begin_index = j;
        end_index = j;
      }
    }

    // The record should be found.
    ASSERT_NE(begin_index, records.size());
    ASSERT_NE(end_index, records.size());
    ASSERT_NE(begin_index, end_index);

    EXPECT_EQ(FileSystemOperation::BEGIN_COPY_ENTRY, records[begin_index].type);
    EXPECT_FALSE(records[begin_index].dest_url.is_valid());
    EXPECT_EQ(FileSystemOperation::END_COPY_ENTRY, records[end_index].type);
    EXPECT_EQ(dest_url, records[end_index].dest_url);

    if (test_case.is_directory) {
      // For directory copy, the progress shouldn't be interlaced.
      EXPECT_EQ(begin_index + 1, end_index);
    } else {
      // PROGRESS event's size should be assending order.
      int64_t current_size = 0;
      for (size_t j = begin_index + 1; j < end_index; ++j) {
        if (records[j].source_url == src_url) {
          EXPECT_EQ(FileSystemOperation::PROGRESS, records[j].type);
          EXPECT_FALSE(records[j].dest_url.is_valid());
          EXPECT_GE(records[j].size, current_size);
          current_size = records[j].size;
        }
      }
    }
  }
}

TEST(LocalFileSystemCopyOrMoveOperationTest, StreamCopyHelper) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath source_path = temp_dir.GetPath().AppendASCII("source");
  base::FilePath dest_path = temp_dir.GetPath().AppendASCII("dest");
  const char kTestData[] = "abcdefghijklmnopqrstuvwxyz0123456789";
  base::WriteFile(source_path, kTestData,
                  base::size(kTestData) - 1);  // Exclude trailing '\0'.

  base::test::SingleThreadTaskEnvironment task_environment(
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO);
  base::Thread file_thread("file_thread");
  ASSERT_TRUE(file_thread.Start());
  ScopedThreadStopper thread_stopper(&file_thread);
  ASSERT_TRUE(thread_stopper.is_valid());

  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      file_thread.task_runner();

  std::unique_ptr<storage::FileStreamReader> reader =
      storage::FileStreamReader::CreateForLocalFile(
          task_runner.get(), source_path, 0, base::Time());

  std::unique_ptr<FileStreamWriter> writer =
      FileStreamWriter::CreateForLocalFile(task_runner.get(), dest_path, 0,
                                           FileStreamWriter::CREATE_NEW_FILE);

  std::vector<int64_t> progress;
  CopyOrMoveOperationDelegate::StreamCopyHelper helper(
      std::move(reader), std::move(writer),
      storage::FlushPolicy::NO_FLUSH_ON_COMPLETION,
      10,  // buffer size
      base::BindRepeating(&RecordFileProgressCallback,
                          base::Unretained(&progress)),
      base::TimeDelta());  // For testing, we need all the progress.

  base::File::Error error = base::File::FILE_ERROR_FAILED;
  base::RunLoop run_loop;
  helper.Run(base::BindOnce(&AssignAndQuit, &run_loop, &error));
  run_loop.Run();

  EXPECT_EQ(base::File::FILE_OK, error);
  ASSERT_EQ(5U, progress.size());
  EXPECT_EQ(0, progress[0]);
  EXPECT_EQ(10, progress[1]);
  EXPECT_EQ(20, progress[2]);
  EXPECT_EQ(30, progress[3]);
  EXPECT_EQ(36, progress[4]);

  std::string content;
  ASSERT_TRUE(base::ReadFileToString(dest_path, &content));
  EXPECT_EQ(kTestData, content);
}

TEST(LocalFileSystemCopyOrMoveOperationTest, StreamCopyHelperWithFlush) {
  // Testing the same configuration as StreamCopyHelper, but with |need_flush|
  // parameter set to true. Since it is hard to test that the flush is indeed
  // taking place, this test just only verifies that the file is correctly
  // written with or without the flag.
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath source_path = temp_dir.GetPath().AppendASCII("source");
  base::FilePath dest_path = temp_dir.GetPath().AppendASCII("dest");
  const char kTestData[] = "abcdefghijklmnopqrstuvwxyz0123456789";
  base::WriteFile(source_path, kTestData,
                  base::size(kTestData) - 1);  // Exclude trailing '\0'.

  base::test::SingleThreadTaskEnvironment task_environment(
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO);
  base::Thread file_thread("file_thread");
  ASSERT_TRUE(file_thread.Start());
  ScopedThreadStopper thread_stopper(&file_thread);
  ASSERT_TRUE(thread_stopper.is_valid());

  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      file_thread.task_runner();

  std::unique_ptr<storage::FileStreamReader> reader =
      storage::FileStreamReader::CreateForLocalFile(
          task_runner.get(), source_path, 0, base::Time());

  std::unique_ptr<FileStreamWriter> writer =
      FileStreamWriter::CreateForLocalFile(task_runner.get(), dest_path, 0,
                                           FileStreamWriter::CREATE_NEW_FILE);

  std::vector<int64_t> progress;
  CopyOrMoveOperationDelegate::StreamCopyHelper helper(
      std::move(reader), std::move(writer),
      storage::FlushPolicy::NO_FLUSH_ON_COMPLETION,
      10,  // buffer size
      base::BindRepeating(&RecordFileProgressCallback,
                          base::Unretained(&progress)),
      base::TimeDelta());  // For testing, we need all the progress.

  base::File::Error error = base::File::FILE_ERROR_FAILED;
  base::RunLoop run_loop;
  helper.Run(base::BindOnce(&AssignAndQuit, &run_loop, &error));
  run_loop.Run();

  EXPECT_EQ(base::File::FILE_OK, error);
  ASSERT_EQ(5U, progress.size());
  EXPECT_EQ(0, progress[0]);
  EXPECT_EQ(10, progress[1]);
  EXPECT_EQ(20, progress[2]);
  EXPECT_EQ(30, progress[3]);
  EXPECT_EQ(36, progress[4]);

  std::string content;
  ASSERT_TRUE(base::ReadFileToString(dest_path, &content));
  EXPECT_EQ(kTestData, content);
}

TEST(LocalFileSystemCopyOrMoveOperationTest, StreamCopyHelper_Cancel) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath source_path = temp_dir.GetPath().AppendASCII("source");
  base::FilePath dest_path = temp_dir.GetPath().AppendASCII("dest");
  const char kTestData[] = "abcdefghijklmnopqrstuvwxyz0123456789";
  base::WriteFile(source_path, kTestData,
                  base::size(kTestData) - 1);  // Exclude trailing '\0'.

  base::test::SingleThreadTaskEnvironment task_environment(
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO);
  base::Thread file_thread("file_thread");
  ASSERT_TRUE(file_thread.Start());
  ScopedThreadStopper thread_stopper(&file_thread);
  ASSERT_TRUE(thread_stopper.is_valid());

  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      file_thread.task_runner();

  std::unique_ptr<storage::FileStreamReader> reader =
      storage::FileStreamReader::CreateForLocalFile(
          task_runner.get(), source_path, 0, base::Time());

  std::unique_ptr<FileStreamWriter> writer =
      FileStreamWriter::CreateForLocalFile(task_runner.get(), dest_path, 0,
                                           FileStreamWriter::CREATE_NEW_FILE);

  std::vector<int64_t> progress;
  CopyOrMoveOperationDelegate::StreamCopyHelper helper(
      std::move(reader), std::move(writer),
      storage::FlushPolicy::NO_FLUSH_ON_COMPLETION,
      10,  // buffer size
      base::BindRepeating(&RecordFileProgressCallback,
                          base::Unretained(&progress)),
      base::TimeDelta());  // For testing, we need all the progress.

  // Call Cancel() later.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&CopyOrMoveOperationDelegate::StreamCopyHelper::Cancel,
                     base::Unretained(&helper)));

  base::File::Error error = base::File::FILE_ERROR_FAILED;
  base::RunLoop run_loop;
  helper.Run(base::BindOnce(&AssignAndQuit, &run_loop, &error));
  run_loop.Run();

  EXPECT_EQ(base::File::FILE_ERROR_ABORT, error);
}

}  // namespace content
