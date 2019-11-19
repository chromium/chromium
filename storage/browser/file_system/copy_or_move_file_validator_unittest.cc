// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>
#include <utility>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "storage/browser/blob/shareable_file_reference.h"
#include "storage/browser/file_system/copy_or_move_file_validator.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "storage/browser/file_system/file_system_backend.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/browser/file_system/isolated_context.h"
#include "storage/browser/test/async_file_test_helper.h"
#include "storage/browser/test/mock_special_storage_policy.h"
#include "storage/browser/test/test_file_system_backend.h"
#include "storage/browser/test/test_file_system_context.h"
#include "storage/common/file_system/file_system_util.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::AsyncFileTestHelper;
using storage::CopyOrMoveFileValidator;
using storage::CopyOrMoveFileValidatorFactory;
using storage::FileSystemURL;

namespace content {

namespace {

const storage::FileSystemType kNoValidatorType =
    storage::kFileSystemTypeTemporary;
const storage::FileSystemType kWithValidatorType = storage::kFileSystemTypeTest;

void ExpectOk(const GURL& origin_url,
              const std::string& name,
              base::File::Error error) {
  ASSERT_EQ(base::File::FILE_OK, error);
}

class CopyOrMoveFileValidatorTestHelper {
 public:
  CopyOrMoveFileValidatorTestHelper(const GURL& origin,
                                    storage::FileSystemType src_type,
                                    storage::FileSystemType dest_type)
      : origin_(origin), src_type_(src_type), dest_type_(dest_type) {}

  ~CopyOrMoveFileValidatorTestHelper() {
    file_system_context_ = nullptr;
    base::RunLoop().RunUntilIdle();
  }

  void SetUp() {
    ASSERT_TRUE(base_.CreateUniqueTempDir());
    base::FilePath base_dir = base_.GetPath();

    file_system_context_ = CreateFileSystemContextForTesting(nullptr, base_dir);

    // Set up TestFileSystemBackend to require CopyOrMoveFileValidator.
    storage::FileSystemBackend* test_file_system_backend =
        file_system_context_->GetFileSystemBackend(kWithValidatorType);
    static_cast<TestFileSystemBackend*>(test_file_system_backend)
        ->set_require_copy_or_move_validator(true);

    // Sets up source.
    storage::FileSystemBackend* src_file_system_backend =
        file_system_context_->GetFileSystemBackend(src_type_);
    src_file_system_backend->ResolveURL(
        FileSystemURL::CreateForTest(url::Origin::Create(origin_), src_type_,
                                     base::FilePath()),
        storage::OPEN_FILE_SYSTEM_CREATE_IF_NONEXISTENT,
        base::BindOnce(&ExpectOk));
    base::RunLoop().RunUntilIdle();
    ASSERT_EQ(base::File::FILE_OK, CreateDirectory(SourceURL("")));

    // Sets up dest.
    DCHECK_EQ(kWithValidatorType, dest_type_);
    ASSERT_EQ(base::File::FILE_OK, CreateDirectory(DestURL("")));

    copy_src_ = SourceURL("copy_src.jpg");
    move_src_ = SourceURL("move_src.jpg");
    copy_dest_ = DestURL("copy_dest.jpg");
    move_dest_ = DestURL("move_dest.jpg");

    ASSERT_EQ(base::File::FILE_OK, CreateFile(copy_src_, 10));
    ASSERT_EQ(base::File::FILE_OK, CreateFile(move_src_, 10));

    ASSERT_TRUE(FileExists(copy_src_, 10));
    ASSERT_TRUE(FileExists(move_src_, 10));
    ASSERT_FALSE(FileExists(copy_dest_, 10));
    ASSERT_FALSE(FileExists(move_dest_, 10));
  }

  void SetMediaCopyOrMoveFileValidatorFactory(
      std::unique_ptr<storage::CopyOrMoveFileValidatorFactory> factory) {
    TestFileSystemBackend* backend = static_cast<TestFileSystemBackend*>(
        file_system_context_->GetFileSystemBackend(kWithValidatorType));
    backend->InitializeCopyOrMoveFileValidatorFactory(std::move(factory));
  }

  void CopyTest(base::File::Error expected) {
    ASSERT_TRUE(FileExists(copy_src_, 10));
    ASSERT_FALSE(FileExists(copy_dest_, 10));

    EXPECT_EQ(expected, AsyncFileTestHelper::Copy(file_system_context_.get(),
                                                  copy_src_, copy_dest_));

    EXPECT_TRUE(FileExists(copy_src_, 10));
    if (expected == base::File::FILE_OK)
      EXPECT_TRUE(FileExists(copy_dest_, 10));
    else
      EXPECT_FALSE(FileExists(copy_dest_, 10));
  }

  void MoveTest(base::File::Error expected) {
    ASSERT_TRUE(FileExists(move_src_, 10));
    ASSERT_FALSE(FileExists(move_dest_, 10));

    EXPECT_EQ(expected, AsyncFileTestHelper::Move(file_system_context_.get(),
                                                  move_src_, move_dest_));

    if (expected == base::File::FILE_OK) {
      EXPECT_FALSE(FileExists(move_src_, 10));
      EXPECT_TRUE(FileExists(move_dest_, 10));
    } else {
      EXPECT_TRUE(FileExists(move_src_, 10));
      EXPECT_FALSE(FileExists(move_dest_, 10));
    }
  }

 private:
  FileSystemURL SourceURL(const std::string& path) {
    return file_system_context_->CreateCrackedFileSystemURL(
        origin_, src_type_,
        base::FilePath().AppendASCII("src").AppendASCII(path));
  }

  FileSystemURL DestURL(const std::string& path) {
    return file_system_context_->CreateCrackedFileSystemURL(
        origin_, dest_type_,
        base::FilePath().AppendASCII("dest").AppendASCII(path));
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

  base::ScopedTempDir base_;

  const GURL origin_;

  const storage::FileSystemType src_type_;
  const storage::FileSystemType dest_type_;
  std::string src_fsid_;
  std::string dest_fsid_;

  base::test::TaskEnvironment task_environment_;
  scoped_refptr<storage::FileSystemContext> file_system_context_;

  FileSystemURL copy_src_;
  FileSystemURL copy_dest_;
  FileSystemURL move_src_;
  FileSystemURL move_dest_;

  DISALLOW_COPY_AND_ASSIGN(CopyOrMoveFileValidatorTestHelper);
};

// For TestCopyOrMoveFileValidatorFactory
enum Validity { VALID, PRE_WRITE_INVALID, POST_WRITE_INVALID };

class TestCopyOrMoveFileValidatorFactory
    : public storage::CopyOrMoveFileValidatorFactory {
 public:
  // A factory that creates validators that accept everything or nothing.
  // TODO(gbillock): switch args to enum or something
  explicit TestCopyOrMoveFileValidatorFactory(Validity validity)
      : validity_(validity) {}
  ~TestCopyOrMoveFileValidatorFactory() override = default;

  storage::CopyOrMoveFileValidator* CreateCopyOrMoveFileValidator(
      const FileSystemURL& /*src_url*/,
      const base::FilePath& /*platform_path*/) override {
    return new TestCopyOrMoveFileValidator(validity_);
  }

 private:
  class TestCopyOrMoveFileValidator : public CopyOrMoveFileValidator {
   public:
    explicit TestCopyOrMoveFileValidator(Validity validity)
        : result_(validity == VALID || validity == POST_WRITE_INVALID
                      ? base::File::FILE_OK
                      : base::File::FILE_ERROR_SECURITY),
          write_result_(validity == VALID || validity == PRE_WRITE_INVALID
                            ? base::File::FILE_OK
                            : base::File::FILE_ERROR_SECURITY) {}
    ~TestCopyOrMoveFileValidator() override = default;

    void StartPreWriteValidation(
        const ResultCallback& result_callback) override {
      // Post the result since a real validator must do work asynchronously.
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::BindOnce(result_callback, result_));
    }

    void StartPostWriteValidation(
        const base::FilePath& dest_platform_path,
        const ResultCallback& result_callback) override {
      // Post the result since a real validator must do work asynchronously.
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::BindOnce(result_callback, write_result_));
    }

   private:
    base::File::Error result_;
    base::File::Error write_result_;

    DISALLOW_COPY_AND_ASSIGN(TestCopyOrMoveFileValidator);
  };

  Validity validity_;

  DISALLOW_COPY_AND_ASSIGN(TestCopyOrMoveFileValidatorFactory);
};

}  // namespace

TEST(CopyOrMoveFileValidatorTest, NoValidatorWithinSameFSType) {
  // Within a file system type, validation is not expected, so it should
  // work for kWithValidatorType without a validator set.
  CopyOrMoveFileValidatorTestHelper helper(
      GURL("http://foo"), kWithValidatorType, kWithValidatorType);
  helper.SetUp();
  helper.CopyTest(base::File::FILE_OK);
  helper.MoveTest(base::File::FILE_OK);
}

TEST(CopyOrMoveFileValidatorTest, MissingValidator) {
  // Copying or moving into a kWithValidatorType requires a file
  // validator.  An error is expected if copy is attempted without a validator.
  CopyOrMoveFileValidatorTestHelper helper(GURL("http://foo"), kNoValidatorType,
                                           kWithValidatorType);
  helper.SetUp();
  helper.CopyTest(base::File::FILE_ERROR_SECURITY);
  helper.MoveTest(base::File::FILE_ERROR_SECURITY);
}

TEST(CopyOrMoveFileValidatorTest, AcceptAll) {
  CopyOrMoveFileValidatorTestHelper helper(GURL("http://foo"), kNoValidatorType,
                                           kWithValidatorType);
  helper.SetUp();
  std::unique_ptr<CopyOrMoveFileValidatorFactory> factory(
      new TestCopyOrMoveFileValidatorFactory(VALID));
  helper.SetMediaCopyOrMoveFileValidatorFactory(std::move(factory));

  helper.CopyTest(base::File::FILE_OK);
  helper.MoveTest(base::File::FILE_OK);
}

TEST(CopyOrMoveFileValidatorTest, AcceptNone) {
  CopyOrMoveFileValidatorTestHelper helper(GURL("http://foo"), kNoValidatorType,
                                           kWithValidatorType);
  helper.SetUp();
  std::unique_ptr<CopyOrMoveFileValidatorFactory> factory(
      new TestCopyOrMoveFileValidatorFactory(PRE_WRITE_INVALID));
  helper.SetMediaCopyOrMoveFileValidatorFactory(std::move(factory));

  helper.CopyTest(base::File::FILE_ERROR_SECURITY);
  helper.MoveTest(base::File::FILE_ERROR_SECURITY);
}

TEST(CopyOrMoveFileValidatorTest, OverrideValidator) {
  // Once set, you can not override the validator.
  CopyOrMoveFileValidatorTestHelper helper(GURL("http://foo"), kNoValidatorType,
                                           kWithValidatorType);
  helper.SetUp();
  std::unique_ptr<CopyOrMoveFileValidatorFactory> reject_factory(
      new TestCopyOrMoveFileValidatorFactory(PRE_WRITE_INVALID));
  helper.SetMediaCopyOrMoveFileValidatorFactory(std::move(reject_factory));

  std::unique_ptr<CopyOrMoveFileValidatorFactory> accept_factory(
      new TestCopyOrMoveFileValidatorFactory(VALID));
  helper.SetMediaCopyOrMoveFileValidatorFactory(std::move(accept_factory));

  helper.CopyTest(base::File::FILE_ERROR_SECURITY);
  helper.MoveTest(base::File::FILE_ERROR_SECURITY);
}

TEST(CopyOrMoveFileValidatorTest, RejectPostWrite) {
  CopyOrMoveFileValidatorTestHelper helper(GURL("http://foo"), kNoValidatorType,
                                           kWithValidatorType);
  helper.SetUp();
  std::unique_ptr<CopyOrMoveFileValidatorFactory> factory(
      new TestCopyOrMoveFileValidatorFactory(POST_WRITE_INVALID));
  helper.SetMediaCopyOrMoveFileValidatorFactory(std::move(factory));

  helper.CopyTest(base::File::FILE_ERROR_SECURITY);
  helper.MoveTest(base::File::FILE_ERROR_SECURITY);
}

}  // namespace content
