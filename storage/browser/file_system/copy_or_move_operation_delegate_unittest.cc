// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/containers/queue.h"
#include "base/containers/span.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "components/file_access/file_access_copy_or_move_delegate_factory.h"
#include "components/services/filesystem/public/mojom/types.mojom.h"
#include "storage/browser/file_system/copy_or_move_file_validator.h"
#include "storage/browser/file_system/copy_or_move_hook_delegate.h"
#include "storage/browser/file_system/copy_or_move_operation_delegate.h"
#include "storage/browser/file_system/file_stream_reader.h"
#include "storage/browser/file_system/file_stream_writer.h"
#include "storage/browser/file_system/file_system_backend.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_operation.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/browser/file_system/file_system_util.h"
#include "storage/browser/quota/quota_manager.h"
#include "storage/browser/test/async_file_test_helper.h"
#include "storage/browser/test/file_system_test_file_set.h"
#include "storage/browser/test/mock_copy_or_move_hook_delegate.h"
#include "storage/browser/test/mock_quota_manager.h"
#include "storage/browser/test/mock_quota_manager_proxy.h"
#include "storage/browser/test/mock_special_storage_policy.h"
#include "storage/browser/test/test_file_system_backend.h"
#include "storage/browser/test/test_file_system_context.h"
#include "storage/common/file_system/file_system_mount_option.h"
#include "storage/common/file_system/file_system_types.h"
#include "storage/common/file_system/file_system_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/gurl.h"
#include "url/origin.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/test/android/content_uri_test_utils.h"
#endif

namespace storage {

namespace {

using FileEntryList = FileSystemOperation::FileEntryList;

constexpr int64_t kDefaultFileSize = 10;

void ExpectOk(const GURL& origin_url,
              const std::string& name,
              base::File::Error error) {
  ASSERT_EQ(base::File::FILE_OK, error);
}

class TestValidatorFactory : public CopyOrMoveFileValidatorFactory {
 public:
  // A factory that creates validators that accept everything or nothing.
  TestValidatorFactory() = default;
  ~TestValidatorFactory() override = default;

  CopyOrMoveFileValidator* CreateCopyOrMoveFileValidator(
      const FileSystemURL& /*src_url*/,
      const base::FilePath& /*platform_path*/) override {
    // Move arg management to TestValidator?
    return new TestValidator(true, true, std::string("2"));
  }

 private:
  class TestValidator : public CopyOrMoveFileValidator {
   public:
    explicit TestValidator(bool pre_copy_valid,
                           bool post_copy_valid,
                           const std::string& reject_string)
        : result_(pre_copy_valid ? base::File::FILE_OK
                                 : base::File::FILE_ERROR_SECURITY),
          write_result_(post_copy_valid ? base::File::FILE_OK
                                        : base::File::FILE_ERROR_SECURITY),
          reject_string_(reject_string) {}

    TestValidator(const TestValidator&) = delete;
    TestValidator& operator=(const TestValidator&) = delete;

    ~TestValidator() override = default;

    void StartPreWriteValidation(ResultCallback result_callback) override {
      // Post the result since a real validator must do work asynchronously.
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(result_callback), result_));
    }

    void StartPostWriteValidation(const base::FilePath& dest_platform_path,
                                  ResultCallback result_callback) override {
      base::File::Error result = write_result_;
      std::string unsafe = dest_platform_path.BaseName().AsUTF8Unsafe();
      if (base::Contains(unsafe, reject_string_)) {
        result = base::File::FILE_ERROR_SECURITY;
      }
      // Post the result since a real validator must do work asynchronously.
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(result_callback), result));
    }

   private:
    base::File::Error result_;
    base::File::Error write_result_;
    std::string reject_string_;
  };
};

class CopyOrMoveRecordAndSecurityDelegate : public CopyOrMoveHookDelegate {
 public:
  // Records method invocations.
  struct ProgressRecord {
    enum class Type {
      kBeginFile = 0,
      kBeginDirectory,
      kProgress,
      kEndCopy,
      kEndMove,
      kEndRemoveSource,
      kError,
    } type;
    FileSystemURL source_url;
    FileSystemURL dest_url;
    int64_t size;
    base::File::Error error;
  };

  using StatusCallback = FileSystemOperation::StatusCallback;

  // Required callback to check whether a file/directory should be blocked for
  // transfer. The callback returns true if the transfer should be blocked.
  using ShouldBlockCallback =
      base::RepeatingCallback<bool(const FileSystemURL& source_url)>;

  explicit CopyOrMoveRecordAndSecurityDelegate(
      std::vector<ProgressRecord>& records,
      const ShouldBlockCallback& should_block_callback)
      : records_(records), should_block_callback_(should_block_callback) {}

  ~CopyOrMoveRecordAndSecurityDelegate() override = default;

  void OnBeginProcessFile(const FileSystemURL& source_url,
                          const FileSystemURL& destination_url,
                          StatusCallback callback) override {
    AddRecord(ProgressRecord::Type::kBeginFile, source_url, destination_url, 0,
              base::File::FILE_OK);
    if (should_block_callback_.Run(source_url)) {
      std::move(callback).Run(base::File::FILE_ERROR_SECURITY);
    } else {
      std::move(callback).Run(base::File::FILE_OK);
    }
  }

  void OnBeginProcessDirectory(const FileSystemURL& source_url,
                               const FileSystemURL& destination_url,
                               StatusCallback callback) override {
    AddRecord(ProgressRecord::Type::kBeginDirectory, source_url,
              destination_url, 0, base::File::FILE_OK);
    if (should_block_callback_.Run(source_url)) {
      std::move(callback).Run(base::File::FILE_ERROR_SECURITY);
    } else {
      std::move(callback).Run(base::File::FILE_OK);
    }
  }

  void OnProgress(const FileSystemURL& source_url,
                  const FileSystemURL& destination_url,
                  int64_t size) override {
    AddRecord(ProgressRecord::Type::kProgress, source_url, destination_url,
              size, base::File::FILE_OK);
  }

  void OnError(const FileSystemURL& source_url,
               const FileSystemURL& destination_url,
               base::File::Error error,
               ErrorCallback callback) override {
    std::move(callback).Run(ErrorAction::kDefault);
    AddRecord(ProgressRecord::Type::kError, source_url, destination_url, 0,
              error);
  }

  void OnEndCopy(const FileSystemURL& source_url,
                 const FileSystemURL& destination_url) override {
    AddRecord(ProgressRecord::Type::kEndCopy, source_url, destination_url, 0,
              base::File::FILE_OK);
  }

  void OnEndMove(const FileSystemURL& source_url,
                 const FileSystemURL& destination_url) override {
    AddRecord(ProgressRecord::Type::kEndMove, source_url, destination_url, 0,
              base::File::FILE_OK);
  }

  void OnEndRemoveSource(const FileSystemURL& source_url) override {
    AddRecord(ProgressRecord::Type::kEndRemoveSource, source_url,
              FileSystemURL(), 0, base::File::FILE_OK);
  }

 private:
  void AddRecord(ProgressRecord::Type type,
                 const FileSystemURL& source_url,
                 const FileSystemURL& dest_url,
                 int64_t size,
                 base::File::Error error) {
    ProgressRecord record;
    record.type = type;
    record.source_url = source_url;
    record.dest_url = dest_url;
    record.size = size;
    record.error = error;
    records_->push_back(record);
  }

  // Raw ptr safe here, because the records will be destructed at end of test,
  // i.e., after the CopyOrMove operation has finished.
  const raw_ref<std::vector<ProgressRecord>> records_;

  ShouldBlockCallback should_block_callback_;
};

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
  explicit ScopedThreadStopper(base::Thread* thread) : thread_(thread) {}

  ScopedThreadStopper(const ScopedThreadStopper&) = delete;
  ScopedThreadStopper& operator=(const ScopedThreadStopper&) = delete;

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
  raw_ptr<base::Thread> thread_;
};

class CopyOrMoveOperationTestHelper {
 public:
  CopyOrMoveOperationTestHelper(const std::string& origin,
                                FileSystemType src_type,
                                FileSystemType dest_type)
      : origin_(url::Origin::Create(GURL(origin))),
        src_type_(src_type),
        dest_type_(dest_type),
        special_storage_policy_(
            base::MakeRefCounted<MockSpecialStoragePolicy>()),
        task_environment_(base::test::TaskEnvironment::MainThreadType::IO) {}

  CopyOrMoveOperationTestHelper(const CopyOrMoveOperationTestHelper&) = delete;
  CopyOrMoveOperationTestHelper& operator=(
      const CopyOrMoveOperationTestHelper&) = delete;

  ~CopyOrMoveOperationTestHelper() {
    file_system_context_ = nullptr;
    quota_manager_proxy_ = nullptr;
    quota_manager_ = nullptr;
    task_environment_.RunUntilIdle();
  }

  void SetUp() { SetUp(true, true); }

  void SetUpNoValidator() { SetUp(true, false); }

  void SetUp(bool require_copy_or_move_validator,
             bool init_copy_or_move_validator) {
    ASSERT_TRUE(base_.CreateUniqueTempDir());
    base::FilePath base_dir = base_.GetPath();
    quota_manager_ = base::MakeRefCounted<MockQuotaManager>(
        false /* is_incognito */, base_dir,
        base::SingleThreadTaskRunner::GetCurrentDefault(),
        special_storage_policy_);
    quota_manager_proxy_ = base::MakeRefCounted<MockQuotaManagerProxy>(
        quota_manager_.get(),
        base::SingleThreadTaskRunner::GetCurrentDefault());
    file_system_context_ =
        CreateFileSystemContextForTesting(quota_manager_proxy_, base_dir);

    // Prepare the origin's root directory.
    FileSystemBackend* backend =
        file_system_context_->GetFileSystemBackend(src_type_);
    backend->ResolveURL(
        FileSystemURL::CreateForTest(
            blink::StorageKey::CreateFirstParty(url::Origin(origin_)),
            src_type_, base::FilePath()),
        OPEN_FILE_SYSTEM_CREATE_IF_NONEXISTENT, base::BindOnce(&ExpectOk));
    backend = file_system_context_->GetFileSystemBackend(dest_type_);
    if (dest_type_ == kFileSystemTypeTest) {
      TestFileSystemBackend* test_backend =
          static_cast<TestFileSystemBackend*>(backend);
      auto factory = std::make_unique<TestValidatorFactory>();
      test_backend->set_require_copy_or_move_validator(
          require_copy_or_move_validator);
      if (init_copy_or_move_validator)
        test_backend->InitializeCopyOrMoveFileValidatorFactory(
            std::move(factory));
    }
    backend->ResolveURL(
        FileSystemURL::CreateForTest(
            blink::StorageKey::CreateFirstParty(url::Origin(origin_)),
            dest_type_, base::FilePath()),
        OPEN_FILE_SYSTEM_CREATE_IF_NONEXISTENT, base::BindOnce(&ExpectOk));
    task_environment_.RunUntilIdle();

    // Grant relatively big quota initially.
    quota_manager_->SetQuota(blink::StorageKey::CreateFirstParty(origin_),
                             FileSystemTypeToQuotaStorageType(src_type_),
                             1024 * 1024);
    quota_manager_->SetQuota(blink::StorageKey::CreateFirstParty(origin_),
                             FileSystemTypeToQuotaStorageType(dest_type_),
                             1024 * 1024);
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
        blink::StorageKey::CreateFirstParty(origin_), src_type_,
        base::FilePath::FromUTF8Unsafe(path));
  }

  FileSystemURL DestURL(const std::string& path) {
    return file_system_context_->CreateCrackedFileSystemURL(
        blink::StorageKey::CreateFirstParty(origin_), dest_type_,
        base::FilePath::FromUTF8Unsafe(path));
  }

  base::File::Error Copy(const FileSystemURL& src, const FileSystemURL& dest) {
    return AsyncFileTestHelper::Copy(file_system_context_.get(), src, dest);
  }

  base::File::Error CopyWithHookDelegate(
      const FileSystemURL& src,
      const FileSystemURL& dest,
      FileSystemOperation::ErrorBehavior error_behavior,
      std::unique_ptr<storage::CopyOrMoveHookDelegate>
          copy_or_move_hook_delegate) {
    return AsyncFileTestHelper::CopyWithHookDelegate(
        file_system_context_.get(), src, dest, error_behavior,
        std::move(copy_or_move_hook_delegate));
  }

  base::File::Error CopyBlockAll(const FileSystemURL& src,
                                 const FileSystemURL& dest) {
    CopyOrMoveRecordAndSecurityDelegate::ShouldBlockCallback blocking_callback =
        base::BindRepeating(
            [](const FileSystemURL& source_url) { return true; });
    std::vector<CopyOrMoveRecordAndSecurityDelegate::ProgressRecord> records;
    return CopyWithHookDelegate(
        src, dest, FileSystemOperation::ERROR_BEHAVIOR_ABORT,
        std::make_unique<CopyOrMoveRecordAndSecurityDelegate>(
            records, blocking_callback));
  }

  // Determines whether a specific `src` should be blocked.
  // It uses the relative path of `src` from `root_src` and compares this path
  // with the records within `kRegularFileSystemTestCases`. It blocks the `src`
  // if the corresponding test case's block_action is `BLOCKED`.
  // Additionally, it ensures that this function is not called for test cases
  // with block_action `PARENT_BLOCKED`.
  static bool ShouldBlockSource(const FileSystemURL& root_src,
                                const FileSystemURL& src) {
    base::FilePath src_relative_path;
    if (root_src == src) {
      // Don't block root, because we only care about the directories/folders
      // within the root.
      return false;
    }

    root_src.virtual_path().AppendRelativePath(src.virtual_path(),
                                               &src_relative_path);
    src_relative_path = src_relative_path.NormalizePathSeparators();
    const auto records = base::make_span(kRegularFileSystemTestCases,
                                         kRegularFileSystemTestCaseSize);
    auto record_it = base::ranges::find(
        records, src_relative_path, [](const FileSystemTestCaseRecord& record) {
          return base::FilePath(record.path).NormalizePathSeparators();
        });

    EXPECT_NE(record_it, records.end());
    EXPECT_NE(record_it->block_action, TestBlockAction::PARENT_BLOCKED);
    return record_it->block_action == TestBlockAction::BLOCKED;
  }

  // Copy the `root` directory, but block some files that are expected to be
  // blocked based on `kRegularFileSystemTestCases`.
  base::File::Error CopyBlockSome(
      const FileSystemURL& src,
      const FileSystemURL& dest,
      std::vector<CopyOrMoveRecordAndSecurityDelegate::ProgressRecord>&
          records) {
    return CopyWithHookDelegate(
        src, dest, FileSystemOperation::ERROR_BEHAVIOR_SKIP,
        std::make_unique<CopyOrMoveRecordAndSecurityDelegate>(
            records,
            base::BindRepeating(
                &CopyOrMoveOperationTestHelper::ShouldBlockSource, src)));
  }

  base::File::Error Move(const FileSystemURL& src, const FileSystemURL& dest) {
    return AsyncFileTestHelper::Move(file_system_context_.get(), src, dest);
  }

  base::File::Error MoveWithHookDelegate(
      const FileSystemURL& src,
      const FileSystemURL& dest,
      FileSystemOperation::ErrorBehavior error_behavior,
      std::unique_ptr<storage::CopyOrMoveHookDelegate>
          copy_or_move_hook_delegate) {
    return AsyncFileTestHelper::MoveWithHookDelegate(
        file_system_context_.get(), src, dest, error_behavior,
        std::move(copy_or_move_hook_delegate));
  }

  base::File::Error MoveBlockAll(const FileSystemURL& src,
                                 const FileSystemURL& dest) {
    CopyOrMoveRecordAndSecurityDelegate::ShouldBlockCallback blocking_callback =
        base::BindRepeating(
            [](const FileSystemURL& source_url) { return true; });
    std::vector<CopyOrMoveRecordAndSecurityDelegate::ProgressRecord> records;
    return MoveWithHookDelegate(
        src, dest, FileSystemOperation::ERROR_BEHAVIOR_ABORT,
        std::make_unique<CopyOrMoveRecordAndSecurityDelegate>(
            records, blocking_callback));
  }

  // Move the `root` directory, but block some files that are expected to be
  // blocked based on `kRegularFileSystemTestCases`.
  base::File::Error MoveBlockSome(
      const FileSystemURL& src,
      const FileSystemURL& dest,
      std::vector<CopyOrMoveRecordAndSecurityDelegate::ProgressRecord>&
          records) {
    return MoveWithHookDelegate(
        src, dest, FileSystemOperation::ERROR_BEHAVIOR_SKIP,
        std::make_unique<CopyOrMoveRecordAndSecurityDelegate>(
            records,
            base::BindRepeating(
                &CopyOrMoveOperationTestHelper::ShouldBlockSource, src)));
  }

  base::File::Error SetUpTestCaseFiles(
      const FileSystemURL& root,
      const FileSystemTestCaseRecord* const test_cases,
      size_t test_case_size) {
    base::File::Error result = base::File::FILE_ERROR_FAILED;
    for (size_t i = 0; i < test_case_size; ++i) {
      const FileSystemTestCaseRecord& test_case = test_cases[i];
      FileSystemURL url = file_system_context_->CreateCrackedFileSystemURL(
          root.storage_key(), root.mount_type(),
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

  enum class VerifyDirectoryState {
    // Verifies all files.
    // Used if no blocking is specified and for the source directory after a
    // move.
    ALL_FILES_EXIST,
    // Verifies that only the blocked files and their ancestors exist.
    // Used to check the source for a move.
    ONLY_BLOCKED_FILES_AND_PARENTS,
    // Verifies that only the allowed files exist.
    // Used to check the destination.
    ONLY_ALLOWED_FILES,
  };

  void VerifyTestCaseFiles(const FileSystemURL& root,
                           const FileSystemTestCaseRecord* const test_cases,
                           size_t test_case_size,
                           VerifyDirectoryState check_state) {
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
      for (const filesystem::mojom::DirectoryEntry& entry : entries) {
        FileSystemURL url = file_system_context_->CreateCrackedFileSystemURL(
            dir.storage_key(), dir.mount_type(),
            dir.virtual_path().Append(entry.name));
        base::FilePath relative;
        root.virtual_path().AppendRelativePath(url.virtual_path(), &relative);
        relative = relative.NormalizePathSeparators();
        ASSERT_TRUE(base::Contains(test_case_map, relative));
        if (entry.type == filesystem::mojom::FsFileType::DIRECTORY) {
          EXPECT_TRUE(test_case_map[relative]->is_directory);
          directories.push(url);
        } else {
          EXPECT_FALSE(test_case_map[relative]->is_directory);
          EXPECT_TRUE(FileExists(url, test_case_map[relative]->data_file_size));
        }
        if (check_state ==
            VerifyDirectoryState::ONLY_BLOCKED_FILES_AND_PARENTS) {
          // We check for remaining blocked files, i.e., all files that are
          // actually blocked (BLOCK, PARENT_BLOCKED) and their parent
          // directories (CHILD_BLOCKED).
          EXPECT_THAT(test_case_map[relative]->block_action,
                      ::testing::AnyOf(TestBlockAction::CHILD_BLOCKED,
                                       TestBlockAction::BLOCKED,
                                       TestBlockAction::PARENT_BLOCKED))
              << "path: " << relative.LossyDisplayName() << "\nblock_action: "
              << static_cast<int>(test_case_map[relative]->block_action);
        } else if (check_state == VerifyDirectoryState::ONLY_ALLOWED_FILES) {
          // We check for allowed files, i.e., all files that were transferred
          // and their parent directories (CHILD_BLOCKED, ALLOW).
          EXPECT_THAT(test_case_map[relative]->block_action,
                      ::testing::AnyOf(TestBlockAction::ALLOWED,
                                       TestBlockAction::CHILD_BLOCKED))
              << "path: " << relative.LossyDisplayName() << "\nblock_action: "
              << static_cast<int>(test_case_map[relative]->block_action);
        }
        test_case_map.erase(relative);
      }
    }
    if (check_state == VerifyDirectoryState::ALL_FILES_EXIST) {
      EXPECT_TRUE(test_case_map.empty());
      for (const auto& path_record_pair : test_case_map) {
        LOG(ERROR) << "Extra entry: "
                   << path_record_pair.first.LossyDisplayName();
      }
    } else {
      for (const auto& path_record_pair : test_case_map) {
        auto* record = path_record_pair.second;
        if (check_state ==
            VerifyDirectoryState::ONLY_BLOCKED_FILES_AND_PARENTS) {
          EXPECT_THAT(record->block_action,
                      ::testing::AnyOf(TestBlockAction::ALLOWED))
              << "path: " << path_record_pair.first.LossyDisplayName()
              << "\nblock_action: " << static_cast<int>(record->block_action);
        } else if (check_state == VerifyDirectoryState::ONLY_ALLOWED_FILES) {
          EXPECT_THAT(record->block_action,
                      ::testing::AnyOf(TestBlockAction::BLOCKED,
                                       TestBlockAction::PARENT_BLOCKED))
              << "path: " << path_record_pair.first.LossyDisplayName()
              << "\nblock_action: " << static_cast<int>(record->block_action);
        }
      }
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
  void GetUsageAndQuota(FileSystemType type, int64_t* usage, int64_t* quota) {
    blink::mojom::QuotaStatusCode status =
        AsyncFileTestHelper::GetUsageAndQuota(
            quota_manager_->proxy(),
            blink::StorageKey::CreateFirstParty(origin_), type, usage, quota);
    ASSERT_EQ(blink::mojom::QuotaStatusCode::kOk, status);
  }

 private:
  const url::Origin origin_;
  const FileSystemType src_type_;
  const FileSystemType dest_type_;

  scoped_refptr<MockSpecialStoragePolicy> special_storage_policy_;

  base::ScopedTempDir base_;
  base::test::TaskEnvironment task_environment_;

  scoped_refptr<FileSystemContext> file_system_context_;
  scoped_refptr<MockQuotaManager> quota_manager_;
  scoped_refptr<MockQuotaManagerProxy> quota_manager_proxy_;
};

}  // namespace

class LocalFileSystemCopyOrMoveOperationTest
    : public ::testing::TestWithParam<std::tuple</*is_move*/ bool,
                                                 /*is_local*/ bool,
                                                 /*blocking_enabled*/ bool>> {
 protected:
  bool IsMove() { return std::get<0>(GetParam()); }
  bool IsLocal() { return std::get<1>(GetParam()); }
  bool BlockingEnabled() { return std::get<2>(GetParam()); }
};

INSTANTIATE_TEST_SUITE_P(
    All,
    LocalFileSystemCopyOrMoveOperationTest,
    ::testing::Combine(::testing::Bool(), ::testing::Bool(), ::testing::Bool()),
    [](const ::testing::TestParamInfo<
        LocalFileSystemCopyOrMoveOperationTest::ParamType>& info) {
      std::string name;
      name += std::get<0>(info.param) ? "Move" : "Copy";
      name += std::get<1>(info.param) ? "Local" : "";
      name += std::get<2>(info.param) ? "WithBlocking" : "";
      return name;
    });

TEST_P(LocalFileSystemCopyOrMoveOperationTest, SingleFile) {
  CopyOrMoveOperationTestHelper helper(
      "http://foo", kFileSystemTypeTemporary,
      IsLocal() ? kFileSystemTypeTemporary : kFileSystemTypePersistent);

  helper.SetUp();

  FileSystemURL src = helper.SourceURL("a");
  FileSystemURL dest = helper.DestURL("b");
  int64_t src_initial_usage = helper.GetSourceUsage();

  // Set up a source file.
  ASSERT_EQ(base::File::FILE_OK, helper.CreateFile(src, 10));
  int64_t src_increase = helper.GetSourceUsage() - src_initial_usage;

  if (IsMove()) {
    // Move it.
    if (BlockingEnabled()) {
      ASSERT_EQ(base::File::FILE_ERROR_SECURITY, helper.MoveBlockAll(src, dest))
          << "For single file moves, the last error should be a security "
             "error!";
    } else {
      ASSERT_EQ(base::File::FILE_OK, helper.Move(src, dest));
    }
  } else {
    // Copy it.
    if (BlockingEnabled()) {
      ASSERT_EQ(base::File::FILE_ERROR_SECURITY, helper.CopyBlockAll(src, dest))
          << "For single file copies, the last error should be a security "
             "error!";
    } else {
      ASSERT_EQ(base::File::FILE_OK, helper.Copy(src, dest));
    }
  }

  bool src_should_exist = !IsMove() || BlockingEnabled();
  bool dest_should_exist = !BlockingEnabled();
  // Verify.
  ASSERT_EQ(src_should_exist, helper.FileExists(src, 10));
  ASSERT_EQ(dest_should_exist, helper.FileExists(dest, 10));

  int64_t src_new_usage = helper.GetSourceUsage();
  if (IsMove() || BlockingEnabled()) {
    EXPECT_EQ(src_new_usage, src_initial_usage + src_increase);
  } else {
    EXPECT_EQ(src_new_usage, src_initial_usage + 2 * src_increase);
  }
}

TEST_P(LocalFileSystemCopyOrMoveOperationTest, EmptyDirectory) {
  CopyOrMoveOperationTestHelper helper(
      "http://foo", kFileSystemTypeTemporary,
      IsLocal() ? kFileSystemTypeTemporary : kFileSystemTypePersistent);
  helper.SetUp();

  FileSystemURL src = helper.SourceURL("a");
  FileSystemURL dest = helper.DestURL("b");
  int64_t src_initial_usage = helper.GetSourceUsage();

  // Set up a source directory.
  ASSERT_EQ(base::File::FILE_OK, helper.CreateDirectory(src));
  int64_t src_increase = helper.GetSourceUsage() - src_initial_usage;

  if (IsMove()) {
    // Move it.
    if (BlockingEnabled()) {
      ASSERT_EQ(base::File::FILE_ERROR_SECURITY, helper.MoveBlockAll(src, dest))
          << "For single directory moves, the last error should be a security "
             "error!";
    } else {
      ASSERT_EQ(base::File::FILE_OK, helper.Move(src, dest));
    }
  } else {
    // Copy it.
    if (BlockingEnabled()) {
      ASSERT_EQ(base::File::FILE_ERROR_SECURITY, helper.CopyBlockAll(src, dest))
          << "For single directory copies, the last error should be a security "
             "error!";
    } else {
      ASSERT_EQ(base::File::FILE_OK, helper.Copy(src, dest));
    }
  }

  bool src_should_exist = !IsMove() || BlockingEnabled();
  bool dest_should_exist = !BlockingEnabled();
  // Verify.
  ASSERT_EQ(src_should_exist, helper.DirectoryExists(src));
  ASSERT_EQ(dest_should_exist, helper.DirectoryExists(dest));

  int64_t src_new_usage = helper.GetSourceUsage();
  if (IsMove() || BlockingEnabled()) {
    EXPECT_EQ(src_new_usage, src_initial_usage + src_increase);
  } else {
    EXPECT_EQ(src_new_usage, src_initial_usage + 2 * src_increase);
  }
}

TEST_P(LocalFileSystemCopyOrMoveOperationTest, FilesAndDirectories) {
  CopyOrMoveOperationTestHelper helper(
      "http://foo", kFileSystemTypeTemporary,
      IsLocal() ? kFileSystemTypeTemporary : kFileSystemTypePersistent);
  helper.SetUp();

  FileSystemURL src = helper.SourceURL("a");
  FileSystemURL dest = helper.DestURL("b");
  int64_t src_initial_usage = helper.GetSourceUsage();

  // Set up a source directory.
  ASSERT_EQ(base::File::FILE_OK, helper.CreateDirectory(src));
  ASSERT_EQ(base::File::FILE_OK,
            helper.SetUpTestCaseFiles(src, kRegularFileSystemTestCases,
                                      kRegularFileSystemTestCaseSize));
  int64_t src_increase = helper.GetSourceUsage() - src_initial_usage;

  if (IsMove()) {
    // Move it.
    if (BlockingEnabled()) {
      // Records are not checked in this test.
      std::vector<CopyOrMoveRecordAndSecurityDelegate::ProgressRecord> records;
      ASSERT_EQ(base::File::FILE_ERROR_NOT_EMPTY,
                helper.MoveBlockSome(src, dest, records))
          << "For multi-directory moves, the last error should be "
             "FILE_ERROR_NOT_EMPTY because the src directory could not be "
             "removed!";
    } else {
      ASSERT_EQ(base::File::FILE_OK, helper.Move(src, dest));
    }
  } else {
    // Copy it.
    if (BlockingEnabled()) {
      // For copies, only forbidden file/directory access errors are reported.
      // Records are not checked in this test.
      std::vector<CopyOrMoveRecordAndSecurityDelegate::ProgressRecord> records;
      ASSERT_EQ(base::File::FILE_ERROR_SECURITY,
                helper.CopyBlockSome(src, dest, records))
          << "For multi-directory copies, the last error should be "
             "FILE_ERROR_SECURITY!";
    } else {
      ASSERT_EQ(base::File::FILE_OK, helper.Copy(src, dest));
    }
  }

  bool src_should_exist = !IsMove() || BlockingEnabled();

  // Verify.
  ASSERT_EQ(src_should_exist, helper.DirectoryExists(src));
  // For recursive transfers, the destination directory is always created
  // (unless the root source directory is blocked, which is not blocked here)!
  ASSERT_TRUE(helper.DirectoryExists(dest));

  if (!IsMove()) {
    // For copies, all files should be there!
    helper.VerifyTestCaseFiles(
        src, kRegularFileSystemTestCases, kRegularFileSystemTestCaseSize,
        CopyOrMoveOperationTestHelper::VerifyDirectoryState::ALL_FILES_EXIST);
  } else {
    if (BlockingEnabled()) {
      // For moves, only blocked files should remain.
      helper.VerifyTestCaseFiles(
          src, kRegularFileSystemTestCases, kRegularFileSystemTestCaseSize,
          CopyOrMoveOperationTestHelper::VerifyDirectoryState::
              ONLY_BLOCKED_FILES_AND_PARENTS);
    }
  }

  // Destination checking is independent of whether the operation is a copy or
  // move.
  if (BlockingEnabled()) {
    helper.VerifyTestCaseFiles(dest, kRegularFileSystemTestCases,
                               kRegularFileSystemTestCaseSize,
                               CopyOrMoveOperationTestHelper::
                                   VerifyDirectoryState::ONLY_ALLOWED_FILES);
  } else {
    helper.VerifyTestCaseFiles(
        dest, kRegularFileSystemTestCases, kRegularFileSystemTestCaseSize,
        CopyOrMoveOperationTestHelper::VerifyDirectoryState::ALL_FILES_EXIST);
  }

  // For local operations we can only check the size if there is no blocking
  // involved.
  if (!BlockingEnabled()) {
    int64_t src_new_usage = helper.GetSourceUsage();
    if (IsMove()) {
      ASSERT_EQ(src_initial_usage + src_increase, src_new_usage);
    } else {
      // Copies duplicate used size on common file system.
      ASSERT_EQ(src_initial_usage + 2 * src_increase, src_new_usage);
    }
  }
}

TEST(LocalFileSystemCopyOrMoveOperationTest,
     MoveDirectoryFailPostWriteValidation) {
  CopyOrMoveOperationTestHelper helper("http://foo", kFileSystemTypeTemporary,
                                       kFileSystemTypeTest);
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

  helper.VerifyTestCaseFiles(
      dest, kMoveDirResultCases, std::size(kMoveDirResultCases),
      CopyOrMoveOperationTestHelper::VerifyDirectoryState::ALL_FILES_EXIST);
}

TEST(LocalFileSystemCopyOrMoveOperationTest, CopySingleFileNoValidator) {
  CopyOrMoveOperationTestHelper helper("http://foo", kFileSystemTypeTemporary,
                                       kFileSystemTypeTest);
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

#if BUILDFLAG(IS_ANDROID)
TEST(LocalFileSystemCopyOrMoveOperationTest, CopyToExistingContentUri) {
  base::test::TaskEnvironment task_environment(
      base::test::TaskEnvironment::MainThreadType::IO);
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  // Create source file and dest file.
  base::FilePath source_path = temp_dir.GetPath().Append("source");
  base::FilePath dest_path = temp_dir.GetPath().Append("dest");
  base::WriteFile(source_path, "foobar");
  base::WriteFile(dest_path, "will-be-truncated");
  base::FilePath source_content_uri =
      *base::test::android::GetContentUriFromCacheDirFilePath(source_path);
  base::FilePath dest_content_uri =
      *base::test::android::GetContentUriFromCacheDirFilePath(dest_path);

  // Copy using content-URIs.
  auto storage_key =
      blink::StorageKey::CreateFromStringForTesting("http://foo");
  auto file_system_context =
      storage::CreateFileSystemContextForTesting(nullptr, temp_dir.GetPath());
  FileSystemURL src = file_system_context->CreateCrackedFileSystemURL(
      storage_key, kFileSystemTypeLocal, source_content_uri);
  FileSystemURL dest = file_system_context->CreateCrackedFileSystemURL(
      storage_key, kFileSystemTypeLocal, dest_content_uri);
  EXPECT_EQ(base::File::FILE_OK,
            AsyncFileTestHelper::Copy(file_system_context.get(), src, dest));

  // Verify.
  EXPECT_TRUE(
      AsyncFileTestHelper::FileExists(file_system_context.get(), src, 6));
  EXPECT_TRUE(
      AsyncFileTestHelper::FileExists(file_system_context.get(), dest, 6));
}
#endif

TEST_P(LocalFileSystemCopyOrMoveOperationTest, FilesAndDirectoriesProgress) {
  CopyOrMoveOperationTestHelper helper(
      "http://foo", kFileSystemTypeTemporary,
      IsLocal() ? kFileSystemTypeTemporary : kFileSystemTypePersistent);
  helper.SetUp();

  FileSystemURL src = helper.SourceURL("a");
  FileSystemURL dest = helper.DestURL("b");

  // Set up a source directory.
  ASSERT_EQ(base::File::FILE_OK, helper.CreateDirectory(src));
  ASSERT_EQ(base::File::FILE_OK,
            helper.SetUpTestCaseFiles(src, kRegularFileSystemTestCases,
                                      kRegularFileSystemTestCaseSize));

  CopyOrMoveRecordAndSecurityDelegate::ShouldBlockCallback allow_all_callback =
      base::BindRepeating(
          [](const FileSystemURL& source_url) { return false; });
  std::vector<CopyOrMoveRecordAndSecurityDelegate::ProgressRecord> records;

  if (IsMove()) {
    // Move it.
    if (BlockingEnabled()) {
      ASSERT_EQ(base::File::FILE_ERROR_NOT_EMPTY,
                helper.MoveBlockSome(src, dest, records))
          << "For multi-directory moves, the last error should be "
             "FILE_ERROR_NOT_EMPTY because the src directory could not be "
             "removed!";
    } else {
      ASSERT_EQ(base::File::FILE_OK,
                helper.MoveWithHookDelegate(
                    src, dest, FileSystemOperation::ERROR_BEHAVIOR_ABORT,
                    std::make_unique<CopyOrMoveRecordAndSecurityDelegate>(
                        records, allow_all_callback)));
    }
  } else {
    // Copy it.
    if (BlockingEnabled()) {
      // For copies, only forbidden file/directory access errors are reported.
      ASSERT_EQ(base::File::FILE_ERROR_SECURITY,
                helper.CopyBlockSome(src, dest, records))
          << "For multi-directory copies, the last error should be "
             "FILE_ERROR_SECURITY!";
    } else {
      ASSERT_EQ(base::File::FILE_OK,
                helper.CopyWithHookDelegate(
                    src, dest, FileSystemOperation::ERROR_BEHAVIOR_ABORT,
                    std::make_unique<CopyOrMoveRecordAndSecurityDelegate>(
                        records, allow_all_callback)));
    }
  }

  // Verify that for `src` kBeginFile is called.
  // This behavior is expected, because for the src entry, ProcessFile is
  // always called independent of whether it is a directory or not. Note: This
  // might change if the behavior of RecursiveOperationDelegate is changed.
  EXPECT_EQ(
      CopyOrMoveRecordAndSecurityDelegate::ProgressRecord::Type::kBeginFile,
      records[0].type);
  EXPECT_EQ(dest, records[0].dest_url);

  // Verify progress records.
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

    if (BlockingEnabled() &&
        test_case.block_action == TestBlockAction::PARENT_BLOCKED) {
      // There shouldn't be any record, if the parent is blocked.
      ASSERT_EQ(begin_index, records.size());
      continue;
    }

    // The record should be found.
    ASSERT_NE(begin_index, records.size());
    ASSERT_NE(end_index, records.size());
    ASSERT_NE(begin_index, end_index);

    if (test_case.is_directory) {
      // A directory copy/move starts with kBegin and kEndCopy.
      EXPECT_EQ(CopyOrMoveRecordAndSecurityDelegate::ProgressRecord::Type::
                    kBeginDirectory,
                records[begin_index].type);
      EXPECT_EQ(dest_url, records[begin_index].dest_url);

      if (BlockingEnabled() &&
          test_case.block_action == TestBlockAction::BLOCKED) {
        EXPECT_EQ(
            CopyOrMoveRecordAndSecurityDelegate::ProgressRecord::Type::kError,
            records[begin_index + 1].type);
        EXPECT_EQ(base::File::FILE_ERROR_SECURITY,
                  records[begin_index + 1].error);
        EXPECT_EQ(dest_url, records[begin_index + 1].dest_url);
        EXPECT_EQ(begin_index + 1, end_index);
        continue;
      }

      EXPECT_EQ(
          CopyOrMoveRecordAndSecurityDelegate::ProgressRecord::Type::kEndCopy,
          records[begin_index + 1].type);
      EXPECT_EQ(dest_url, records[begin_index + 1].dest_url);

      if (IsMove()) {
        if (BlockingEnabled() &&
            test_case.block_action == TestBlockAction::CHILD_BLOCKED) {
          EXPECT_EQ(
              CopyOrMoveRecordAndSecurityDelegate::ProgressRecord::Type::kError,
              records[end_index].type);
          EXPECT_FALSE(records[end_index].dest_url.is_valid());
          EXPECT_EQ(base::File::FILE_ERROR_NOT_EMPTY, records[end_index].error);
          EXPECT_NE(records[end_index - 1].source_url, src_url);
          continue;
        }
        // A directory move ends with kEndRemoveSource, after the contents of
        // the directory have been copied.
        EXPECT_EQ(CopyOrMoveRecordAndSecurityDelegate::ProgressRecord::Type::
                      kEndRemoveSource,
                  records[end_index].type);
        EXPECT_FALSE(records[end_index].dest_url.is_valid());
      } else {
        // For directory copy, the progress shouldn't be interlaced.
        EXPECT_EQ(begin_index + 1, end_index);
      }
    } else {
      // A file copy/move starts with kBeginFile.
      EXPECT_EQ(
          CopyOrMoveRecordAndSecurityDelegate::ProgressRecord::Type::kBeginFile,
          records[begin_index].type);
      EXPECT_EQ(dest_url, records[begin_index].dest_url);

      if (BlockingEnabled() &&
          test_case.block_action == TestBlockAction::BLOCKED) {
        // If a file is blocked, the second and last record for that file should
        // be an error.
        EXPECT_EQ(
            CopyOrMoveRecordAndSecurityDelegate::ProgressRecord::Type::kError,
            records[begin_index + 1].type);
        EXPECT_EQ(base::File::FILE_ERROR_SECURITY,
                  records[begin_index + 1].error);
        EXPECT_EQ(dest_url, records[begin_index + 1].dest_url);
        EXPECT_EQ(begin_index + 1, end_index);
        continue;
      }

      // PROGRESS event's size should be ascending order.
      int64_t current_size = 0;
      size_t end_progress_index = end_index;
      if (IsMove() && !IsLocal()) {
        end_progress_index = end_index - 1;
      }
      if (IsMove() && IsLocal()) {
        EXPECT_EQ(begin_index, end_progress_index - 1)
            << "No progress should be reported for local moves!";
      } else {
        EXPECT_LT(begin_index, end_progress_index - 1)
            << "There should be some progress reported.";
      }
      for (size_t j = begin_index + 1; j < end_progress_index; ++j) {
        if (records[j].source_url == src_url) {
          EXPECT_EQ(CopyOrMoveRecordAndSecurityDelegate::ProgressRecord::Type::
                        kProgress,
                    records[j].type);
          EXPECT_EQ(dest_url, records[j].dest_url);
          EXPECT_GE(records[j].size, current_size);
          current_size = records[j].size;
        }
      }
      if (IsMove() && IsLocal()) {
        // For local moves, we expect kEndMove at the end.
        EXPECT_EQ(
            CopyOrMoveRecordAndSecurityDelegate::ProgressRecord::Type::kEndMove,
            records[end_index].type);
        EXPECT_EQ(dest_url, records[end_index].dest_url);
      } else {
        // A file move ends with kEndCopy and kEndRemoveSource, a copy with just
        // kEndCopy.
        size_t end_copy_index = IsMove() ? end_index - 1 : end_index;
        EXPECT_EQ(
            CopyOrMoveRecordAndSecurityDelegate::ProgressRecord::Type::kEndCopy,
            records[end_copy_index].type);
        EXPECT_EQ(dest_url, records[end_copy_index].dest_url);
        if (IsMove()) {
          EXPECT_EQ(CopyOrMoveRecordAndSecurityDelegate::ProgressRecord::Type::
                        kEndRemoveSource,
                    records[end_index].type);
          EXPECT_FALSE(records[end_index].dest_url.is_valid());
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
  base::WriteFile(source_path, kTestData);

  base::test::SingleThreadTaskEnvironment task_environment(
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO);
  base::Thread file_thread("file_thread");
  ASSERT_TRUE(file_thread.Start());
  ScopedThreadStopper thread_stopper(&file_thread);
  ASSERT_TRUE(thread_stopper.is_valid());

  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      file_thread.task_runner();

  std::unique_ptr<FileStreamReader> reader =
      FileStreamReader::CreateForLocalFile(task_runner.get(), source_path, 0,
                                           base::Time());

  std::unique_ptr<FileStreamWriter> writer =
      FileStreamWriter::CreateForLocalFile(task_runner.get(), dest_path, 0,
                                           FileStreamWriter::CREATE_NEW_FILE);

  std::vector<int64_t> progress;
  CopyOrMoveOperationDelegate::StreamCopyHelper helper(
      std::move(reader), std::move(writer), FlushPolicy::NO_FLUSH_ON_COMPLETION,
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
  base::WriteFile(source_path, kTestData);

  base::test::SingleThreadTaskEnvironment task_environment(
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO);
  base::Thread file_thread("file_thread");
  ASSERT_TRUE(file_thread.Start());
  ScopedThreadStopper thread_stopper(&file_thread);
  ASSERT_TRUE(thread_stopper.is_valid());

  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      file_thread.task_runner();

  std::unique_ptr<FileStreamReader> reader =
      FileStreamReader::CreateForLocalFile(task_runner.get(), source_path, 0,
                                           base::Time());

  std::unique_ptr<FileStreamWriter> writer =
      FileStreamWriter::CreateForLocalFile(task_runner.get(), dest_path, 0,
                                           FileStreamWriter::CREATE_NEW_FILE);

  std::vector<int64_t> progress;
  CopyOrMoveOperationDelegate::StreamCopyHelper helper(
      std::move(reader), std::move(writer), FlushPolicy::NO_FLUSH_ON_COMPLETION,
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
  base::WriteFile(source_path, kTestData);

  base::test::SingleThreadTaskEnvironment task_environment(
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO);
  base::Thread file_thread("file_thread");
  ASSERT_TRUE(file_thread.Start());
  ScopedThreadStopper thread_stopper(&file_thread);
  ASSERT_TRUE(thread_stopper.is_valid());

  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      file_thread.task_runner();

  std::unique_ptr<FileStreamReader> reader =
      FileStreamReader::CreateForLocalFile(task_runner.get(), source_path, 0,
                                           base::Time());

  std::unique_ptr<FileStreamWriter> writer =
      FileStreamWriter::CreateForLocalFile(task_runner.get(), dest_path, 0,
                                           FileStreamWriter::CREATE_NEW_FILE);

  std::vector<int64_t> progress;
  CopyOrMoveOperationDelegate::StreamCopyHelper helper(
      std::move(reader), std::move(writer), FlushPolicy::NO_FLUSH_ON_COMPLETION,
      10,  // buffer size
      base::BindRepeating(&RecordFileProgressCallback,
                          base::Unretained(&progress)),
      base::TimeDelta());  // For testing, we need all the progress.

  // Call Cancel() later.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&CopyOrMoveOperationDelegate::StreamCopyHelper::Cancel,
                     base::Unretained(&helper)));

  base::File::Error error = base::File::FILE_ERROR_FAILED;
  base::RunLoop run_loop;
  helper.Run(base::BindOnce(&AssignAndQuit, &run_loop, &error));
  run_loop.Run();

  EXPECT_EQ(base::File::FILE_ERROR_ABORT, error);
}

class CopyOrMoveOperationDelegateTestHelper {
 public:
  CopyOrMoveOperationDelegateTestHelper(
      const std::string& origin,
      FileSystemType src_type,
      FileSystemType dest_type,
      FileSystemOperation::CopyOrMoveOptionSet options)
      : origin_(url::Origin::Create(GURL(origin))),
        src_type_(src_type),
        dest_type_(dest_type),
        options_(options),
        task_environment_(base::test::TaskEnvironment::MainThreadType::IO) {}

  CopyOrMoveOperationDelegateTestHelper(
      const CopyOrMoveOperationDelegateTestHelper&) = delete;
  CopyOrMoveOperationDelegateTestHelper& operator=(
      const CopyOrMoveOperationDelegateTestHelper&) = delete;

  ~CopyOrMoveOperationDelegateTestHelper() {
    file_system_context_ = nullptr;
    task_environment_.RunUntilIdle();
  }

  void SetUp() {
    ASSERT_TRUE(base_.CreateUniqueTempDir());
    base::FilePath base_dir = base_.GetPath();
    quota_manager_ = base::MakeRefCounted<storage::MockQuotaManager>(
        /*is_incognito=*/false, base_dir,
        base::SingleThreadTaskRunner::GetCurrentDefault(),
        base::MakeRefCounted<storage::MockSpecialStoragePolicy>());
    quota_manager_proxy_ = base::MakeRefCounted<storage::MockQuotaManagerProxy>(
        quota_manager_.get(),
        base::SingleThreadTaskRunner::GetCurrentDefault());
    // Prepare file system.
    file_system_context_ = storage::CreateFileSystemContextForTesting(
        quota_manager_proxy_.get(), base_dir);

    // Prepare the origin's root directory.
    FileSystemBackend* backend =
        file_system_context_->GetFileSystemBackend(src_type_);
    backend->ResolveURL(
        FileSystemURL::CreateForTest(
            blink::StorageKey::CreateFirstParty(url::Origin(origin_)),
            src_type_, base::FilePath()),
        OPEN_FILE_SYSTEM_CREATE_IF_NONEXISTENT, base::BindOnce(&ExpectOk));
    backend = file_system_context_->GetFileSystemBackend(dest_type_);
    backend->ResolveURL(
        FileSystemURL::CreateForTest(
            blink::StorageKey::CreateFirstParty(url::Origin(origin_)),
            dest_type_, base::FilePath()),
        OPEN_FILE_SYSTEM_CREATE_IF_NONEXISTENT, base::BindOnce(&ExpectOk));
    task_environment_.RunUntilIdle();
  }

  FileSystemURL GenerateSourceUrlFromPath(const std::string& path) {
    return file_system_context_->CreateCrackedFileSystemURL(
        blink::StorageKey::CreateFirstParty(origin_), src_type_,
        base::FilePath::FromUTF8Unsafe(path));
  }

  FileSystemURL GenerateDestinationUrlFromPath(const std::string& path) {
    return file_system_context_->CreateCrackedFileSystemURL(
        blink::StorageKey::CreateFirstParty(origin_), dest_type_,
        base::FilePath::FromUTF8Unsafe(path));
  }

  base::File::Error CreateFile(const FileSystemURL& url, size_t size) {
    base::File::Error result =
        AsyncFileTestHelper::CreateFile(file_system_context_.get(), url);
    if (result != base::File::FILE_OK)
      return result;
    return AsyncFileTestHelper::TruncateFile(file_system_context_.get(), url,
                                             size);
  }

  base::File::Error CreateDirectory(const FileSystemURL& url) {
    return AsyncFileTestHelper::CreateDirectory(file_system_context_.get(),
                                                url);
  }

  bool FileExists(const FileSystemURL& url, int64_t expected_size) {
    return AsyncFileTestHelper::FileExists(file_system_context_.get(), url,
                                           expected_size);
  }

  bool DirectoryExists(const FileSystemURL& url) {
    return AsyncFileTestHelper::DirectoryExists(file_system_context_.get(),
                                                url);
  }

  // Force Copy or Move error when a given URL is encountered.
  void SetErrorUrl(const FileSystemURL& url) { error_url_ = url; }

  base::File::Error Copy(
      const FileSystemURL& src,
      const FileSystemURL& dest,
      std::unique_ptr<storage::CopyOrMoveHookDelegate> hook_delegate =
          std::make_unique<storage::CopyOrMoveHookDelegate>()) {
    base::RunLoop run_loop;
    base::File::Error result = base::File::FILE_ERROR_FAILED;

    CopyOrMoveOperationDelegate copy_or_move_operation_delegate(
        file_system_context_.get(), src, dest,
        CopyOrMoveOperationDelegate::OPERATION_COPY, options_,
        FileSystemOperation::ERROR_BEHAVIOR_ABORT, std::move(hook_delegate),
        base::BindOnce(&AssignAndQuit, &run_loop, base::Unretained(&result)));
    if (error_url_.is_valid()) {
      copy_or_move_operation_delegate.SetErrorUrlForTest(&error_url_);
    }
    copy_or_move_operation_delegate.RunRecursively();
    run_loop.Run();
    copy_or_move_operation_delegate.SetErrorUrlForTest(nullptr);
    return result;
  }

  base::File::Error Move(
      const FileSystemURL& src,
      const FileSystemURL& dest,
      std::unique_ptr<storage::CopyOrMoveHookDelegate> hook_delegate =
          std::make_unique<storage::CopyOrMoveHookDelegate>()) {
    base::RunLoop run_loop;
    base::File::Error result = base::File::FILE_ERROR_FAILED;

    CopyOrMoveOperationDelegate copy_or_move_operation_delegate(
        file_system_context_.get(), src, dest,
        CopyOrMoveOperationDelegate::OPERATION_MOVE, options_,
        FileSystemOperation::ERROR_BEHAVIOR_ABORT, std::move(hook_delegate),
        base::BindOnce(&AssignAndQuit, &run_loop, base::Unretained(&result)));
    if (error_url_.is_valid()) {
      copy_or_move_operation_delegate.SetErrorUrlForTest(&error_url_);
    }
    copy_or_move_operation_delegate.RunRecursively();
    run_loop.Run();
    copy_or_move_operation_delegate.SetErrorUrlForTest(nullptr);
    return result;
  }

 private:
  base::ScopedTempDir base_;

  const url::Origin origin_;
  const FileSystemType src_type_;
  const FileSystemType dest_type_;
  FileSystemOperation::CopyOrMoveOptionSet options_;

  FileSystemURL error_url_;

  base::test::TaskEnvironment task_environment_;
  scoped_refptr<storage::MockQuotaManager> quota_manager_;
  scoped_refptr<storage::MockQuotaManagerProxy> quota_manager_proxy_;
  scoped_refptr<FileSystemContext> file_system_context_;
};

TEST(CopyOrMoveOperationDelegateTest, StopRecursionOnCopyError) {
  FileSystemOperation::CopyOrMoveOptionSet options;
  CopyOrMoveOperationDelegateTestHelper helper(
      "http://foo", kFileSystemTypePersistent, kFileSystemTypePersistent,
      options);
  helper.SetUp();

  FileSystemURL src = helper.GenerateSourceUrlFromPath("a");
  FileSystemURL src_file_1 = helper.GenerateSourceUrlFromPath("a/file 1");
  FileSystemURL src_file_2 = helper.GenerateSourceUrlFromPath("a/file 2");
  FileSystemURL dest = helper.GenerateDestinationUrlFromPath("b");
  FileSystemURL dest_file_1 = helper.GenerateDestinationUrlFromPath("b/file 1");
  FileSystemURL dest_file_2 = helper.GenerateDestinationUrlFromPath("b/file 2");

  // Set up source files.
  ASSERT_EQ(base::File::FILE_OK, helper.CreateDirectory(src));
  ASSERT_EQ(base::File::FILE_OK,
            helper.CreateFile(src_file_1, kDefaultFileSize));
  ASSERT_EQ(base::File::FILE_OK,
            helper.CreateFile(src_file_2, kDefaultFileSize));

  // [file 1, file 2] are processed as a LIFO. An error is returned after
  // copying file 2.
  helper.SetErrorUrl(src_file_2);
  ASSERT_EQ(base::File::FILE_ERROR_FAILED, helper.Copy(src, dest));

  EXPECT_TRUE(helper.DirectoryExists(src));
  EXPECT_TRUE(helper.DirectoryExists(dest));
  // Check: file 2 is copied, even though the copy results in an error.
  EXPECT_TRUE(helper.FileExists(src_file_2, kDefaultFileSize));
  EXPECT_TRUE(
      helper.FileExists(dest_file_2, AsyncFileTestHelper::kDontCheckSize));
  // Check: the recursion has been interrupted after the error, so file 1
  // hasn't been copied.
  EXPECT_TRUE(helper.FileExists(src_file_1, kDefaultFileSize));
  EXPECT_FALSE(
      helper.FileExists(dest_file_1, AsyncFileTestHelper::kDontCheckSize));
}

TEST(CopyOrMoveOperationDelegateTest, ContinueRecursionOnCopyIgnored) {
  FileSystemOperation::CopyOrMoveOptionSet options;
  CopyOrMoveOperationDelegateTestHelper helper(
      "http://foo", kFileSystemTypePersistent, kFileSystemTypePersistent,
      options);
  helper.SetUp();

  FileSystemURL src = helper.GenerateSourceUrlFromPath("a");
  FileSystemURL src_file_1 = helper.GenerateSourceUrlFromPath("a/file 1");
  FileSystemURL src_file_2 = helper.GenerateSourceUrlFromPath("a/file 2");
  FileSystemURL dest = helper.GenerateDestinationUrlFromPath("b");
  FileSystemURL dest_file_1 = helper.GenerateDestinationUrlFromPath("b/file 1");
  FileSystemURL dest_file_2 = helper.GenerateDestinationUrlFromPath("b/file 2");

  // Set up source files.
  ASSERT_EQ(base::File::FILE_OK, helper.CreateDirectory(src));
  ASSERT_EQ(base::File::FILE_OK,
            helper.CreateFile(src_file_1, kDefaultFileSize));
  ASSERT_EQ(base::File::FILE_OK,
            helper.CreateFile(src_file_2, kDefaultFileSize));

  // Create a hook delegate which will skip errors.
  auto delegate = std::make_unique<MockCopyOrMoveHookDelegate>();
  ON_CALL(*delegate.get(), OnBeginProcessFile)
      .WillByDefault([](const FileSystemURL&, const FileSystemURL&,
                        base::OnceCallback<void(base::File::Error)> cb) {
        std::move(cb).Run(base::File::FILE_OK);
      });
  ON_CALL(*delegate.get(), OnError)
      .WillByDefault([](const FileSystemURL&, const FileSystemURL&,
                        base::File::Error,
                        CopyOrMoveHookDelegate::ErrorCallback cb) {
        std::move(cb).Run(CopyOrMoveHookDelegate::ErrorAction::kSkip);
      });
  // One file out of two is expected to be copied successfully, add one more
  // call for the directory itself.
  EXPECT_CALL(*delegate.get(), OnEndCopy).Times(2);

  // [file 1, file 2] are processed as a LIFO. An error is returned after
  // copying file 1 and ignored by the hook delegate.
  helper.SetErrorUrl(src_file_1);
  ASSERT_EQ(base::File::FILE_OK, helper.Copy(src, dest, std::move(delegate)));

  // Check that the second file was actually copied, but the sources remained
  // untouched.
  EXPECT_TRUE(helper.FileExists(src_file_1, kDefaultFileSize));
  EXPECT_TRUE(helper.FileExists(src_file_2, kDefaultFileSize));
  EXPECT_TRUE(helper.FileExists(dest_file_2, kDefaultFileSize));
}

TEST(CopyOrMoveOperationDelegateTest, ContinueRecursionOnMoveIgnored) {
  FileSystemOperation::CopyOrMoveOptionSet options;
  CopyOrMoveOperationDelegateTestHelper helper(
      "http://foo", kFileSystemTypeTemporary, kFileSystemTypePersistent,
      options);
  helper.SetUp();

  FileSystemURL src = helper.GenerateSourceUrlFromPath("a");
  FileSystemURL src_file_1 = helper.GenerateSourceUrlFromPath("a/file 1");
  FileSystemURL src_file_2 = helper.GenerateSourceUrlFromPath("a/file 2");
  FileSystemURL dest = helper.GenerateDestinationUrlFromPath("b");
  FileSystemURL dest_file_1 = helper.GenerateDestinationUrlFromPath("b/file 1");
  FileSystemURL dest_file_2 = helper.GenerateDestinationUrlFromPath("b/file 2");

  // Set up source files.
  ASSERT_EQ(base::File::FILE_OK, helper.CreateDirectory(src));
  ASSERT_EQ(base::File::FILE_OK,
            helper.CreateFile(src_file_1, kDefaultFileSize));
  ASSERT_EQ(base::File::FILE_OK,
            helper.CreateFile(src_file_2, kDefaultFileSize));

  // Create a hook delegate which will skip errors.
  auto delegate = std::make_unique<MockCopyOrMoveHookDelegate>();
  ON_CALL(*delegate.get(), OnBeginProcessFile)
      .WillByDefault([](const FileSystemURL&, const FileSystemURL&,
                        base::OnceCallback<void(base::File::Error)> cb) {
        std::move(cb).Run(base::File::FILE_OK);
      });
  ON_CALL(*delegate.get(), OnError)
      .WillByDefault([](const FileSystemURL&, const FileSystemURL&,
                        base::File::Error,
                        CopyOrMoveHookDelegate::ErrorCallback cb) {
        std::move(cb).Run(CopyOrMoveHookDelegate::ErrorAction::kSkip);
      });
  // One file out of two is expected to be moved successfully, add one more
  // call for the directory itself. The event is `OnEndCopy`, not `OnEndMove`
  // because we do a cross-filesystem move.
  EXPECT_CALL(*delegate.get(), OnEndCopy).Times(2);
  EXPECT_CALL(*delegate.get(), OnEndMove).Times(0);

  // [file 1, file 2] are processed as a LIFO. An error is returned after
  // copying file 1 and ignored by the hook delegate.
  helper.SetErrorUrl(src_file_1);
  ASSERT_EQ(base::File::FILE_OK, helper.Move(src, dest, std::move(delegate)));

  // Check that the second file was actually copied, but the sources remained
  // untouched.
  EXPECT_TRUE(helper.FileExists(src_file_1, kDefaultFileSize));
  EXPECT_FALSE(
      helper.FileExists(src_file_2, AsyncFileTestHelper::kDontCheckSize));
  EXPECT_TRUE(helper.FileExists(dest_file_2, kDefaultFileSize));
}

TEST(CopyOrMoveOperationDelegateTest, RemoveDestFileOnCopyError) {
  FileSystemOperation::CopyOrMoveOptionSet options = {
      storage::FileSystemOperation::CopyOrMoveOption::
          kRemovePartiallyCopiedFilesOnError};
  CopyOrMoveOperationDelegateTestHelper helper(
      "http://foo", kFileSystemTypePersistent, kFileSystemTypePersistent,
      options);
  helper.SetUp();

  FileSystemURL src = helper.GenerateSourceUrlFromPath("a");
  FileSystemURL src_file_1 = helper.GenerateSourceUrlFromPath("a/file 1");
  FileSystemURL src_file_2 = helper.GenerateSourceUrlFromPath("a/file 2");
  FileSystemURL dest = helper.GenerateDestinationUrlFromPath("b");
  FileSystemURL dest_file_1 = helper.GenerateDestinationUrlFromPath("b/file 1");
  FileSystemURL dest_file_2 = helper.GenerateDestinationUrlFromPath("b/file 2");

  // Set up source files.
  ASSERT_EQ(base::File::FILE_OK, helper.CreateDirectory(src));
  ASSERT_EQ(base::File::FILE_OK,
            helper.CreateFile(src_file_1, kDefaultFileSize));
  ASSERT_EQ(base::File::FILE_OK,
            helper.CreateFile(src_file_2, kDefaultFileSize));

  // [file 1, file 2] are processed as a LIFO. An error is returned after
  // copying file 1.
  helper.SetErrorUrl(src_file_1);
  ASSERT_EQ(base::File::FILE_ERROR_FAILED, helper.Copy(src, dest));

  EXPECT_TRUE(helper.DirectoryExists(src));
  EXPECT_TRUE(helper.DirectoryExists(dest));
  // Check: file 2 is properly copied.
  EXPECT_TRUE(helper.FileExists(src_file_2, kDefaultFileSize));
  EXPECT_TRUE(helper.FileExists(dest_file_2, kDefaultFileSize));
  // Check: file 1 has been removed on error after being copied.
  EXPECT_TRUE(helper.FileExists(src_file_1, kDefaultFileSize));
  EXPECT_FALSE(
      helper.FileExists(dest_file_1, AsyncFileTestHelper::kDontCheckSize));
}

TEST(CopyOrMoveOperationDelegateTest,
     RemoveDestFileOnCrossFilesystemMoveError) {
  FileSystemOperation::CopyOrMoveOptionSet options = {
      storage::FileSystemOperation::CopyOrMoveOption::
          kRemovePartiallyCopiedFilesOnError};
  // Removing destination files on Move errors applies only to cross-filesystem
  // moves.
  CopyOrMoveOperationDelegateTestHelper helper(
      "http://foo", kFileSystemTypeTemporary, kFileSystemTypePersistent,
      options);
  helper.SetUp();

  FileSystemURL src = helper.GenerateSourceUrlFromPath("a");
  FileSystemURL src_file_1 = helper.GenerateSourceUrlFromPath("a/file 1");
  FileSystemURL src_file_2 = helper.GenerateSourceUrlFromPath("a/file 2");
  FileSystemURL dest = helper.GenerateDestinationUrlFromPath("b");
  FileSystemURL dest_file_1 = helper.GenerateDestinationUrlFromPath("b/file 1");
  FileSystemURL dest_file_2 = helper.GenerateDestinationUrlFromPath("b/file 2");

  // Set up source files.
  ASSERT_EQ(base::File::FILE_OK, helper.CreateDirectory(src));
  ASSERT_EQ(base::File::FILE_OK,
            helper.CreateFile(src_file_1, kDefaultFileSize));
  ASSERT_EQ(base::File::FILE_OK,
            helper.CreateFile(src_file_2, kDefaultFileSize));

  // [file 1, file 2] are processed as a LIFO. An error is returned after
  // copying file 1.
  helper.SetErrorUrl(src_file_1);
  ASSERT_EQ(base::File::FILE_ERROR_FAILED, helper.Move(src, dest));

  EXPECT_TRUE(helper.DirectoryExists(src));
  EXPECT_TRUE(helper.DirectoryExists(dest));
  // Check: file 2 is moved.
  EXPECT_FALSE(
      helper.FileExists(src_file_2, AsyncFileTestHelper::kDontCheckSize));
  EXPECT_TRUE(helper.FileExists(dest_file_2, kDefaultFileSize));
  // Check: destination file 1 has been removed on error, and its source still
  // exists.
  EXPECT_TRUE(helper.FileExists(src_file_1, kDefaultFileSize));
  EXPECT_FALSE(
      helper.FileExists(dest_file_1, AsyncFileTestHelper::kDontCheckSize));
}

class MockFileAccessCopyOrMoveDelegateFactory
    : public file_access::FileAccessCopyOrMoveDelegateFactory {
 public:
  MOCK_METHOD(std::unique_ptr<storage::CopyOrMoveHookDelegate>,
              MakeHook,
              (),
              (override));
};

TEST(CopyOrMoveOperationDelegateTest, InjectHook) {
  FileSystemOperation::CopyOrMoveOptionSet options;
  CopyOrMoveOperationDelegateTestHelper helper(
      "http://foo", kFileSystemTypePersistent, kFileSystemTypePersistent,
      options);
  helper.SetUp();

  MockFileAccessCopyOrMoveDelegateFactory factory;
  std::unique_ptr<MockCopyOrMoveHookDelegate> hook_delegate =
      std::make_unique<MockCopyOrMoveHookDelegate>();
  ON_CALL(*hook_delegate.get(), OnBeginProcessFile)
      .WillByDefault([](const FileSystemURL&, const FileSystemURL&,
                        base::OnceCallback<void(base::File::Error)> cb) {
        std::move(cb).Run(base::File::FILE_OK);
      });
  EXPECT_CALL(*hook_delegate.get(), OnBeginProcessFile).Times(3);
  EXPECT_CALL(*hook_delegate.get(), OnEndCopy).Times(3);
  EXPECT_CALL(factory, MakeHook).WillOnce([&hook_delegate] {
    return std::move(hook_delegate);
  });

  FileSystemURL src = helper.GenerateSourceUrlFromPath("a");
  FileSystemURL src_file_1 = helper.GenerateSourceUrlFromPath("a/file 1");
  FileSystemURL src_file_2 = helper.GenerateSourceUrlFromPath("a/file 2");
  FileSystemURL dest = helper.GenerateDestinationUrlFromPath("b");
  FileSystemURL dest_file_1 = helper.GenerateDestinationUrlFromPath("b/file 1");
  FileSystemURL dest_file_2 = helper.GenerateDestinationUrlFromPath("b/file 2");

  ASSERT_EQ(base::File::FILE_OK, helper.CreateDirectory(src));
  ASSERT_EQ(base::File::FILE_OK,
            helper.CreateFile(src_file_1, kDefaultFileSize));
  ASSERT_EQ(base::File::FILE_OK,
            helper.CreateFile(src_file_2, kDefaultFileSize));

  ASSERT_EQ(base::File::FILE_OK, helper.Copy(src, dest));

  EXPECT_TRUE(helper.DirectoryExists(src));
  EXPECT_TRUE(helper.DirectoryExists(dest));
  // Check: file 2 is properly copied.
  EXPECT_TRUE(helper.FileExists(src_file_2, kDefaultFileSize));
  EXPECT_TRUE(helper.FileExists(dest_file_2, kDefaultFileSize));
  // Check: file 1 is properly copied.
  EXPECT_TRUE(helper.FileExists(src_file_1, kDefaultFileSize));
  EXPECT_TRUE(helper.FileExists(dest_file_1, kDefaultFileSize));
}

}  // namespace storage
