// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/file_system/sandbox_file_stream_reader.h"

#include <stddef.h>
#include <stdint.h>

#include <limits>
#include <memory>
#include <string>
#include <string_view>

#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "storage/browser/file_system/file_stream_reader_test.h"
#include "storage/browser/file_system/file_stream_test_utils.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_file_util.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/browser/test/async_file_test_helper.h"
#include "storage/browser/test/mock_quota_manager_proxy.h"
#include "storage/browser/test/mock_special_storage_policy.h"
#include "storage/browser/test/test_file_system_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/origin.h"

namespace storage {

namespace {
const char kURLOrigin[] = "http://remote/";
}  // namespace

class SandboxFileStreamReaderTest : public FileStreamReaderTest {
 public:
  SandboxFileStreamReaderTest()
      : special_storage_policy_(
            base::MakeRefCounted<MockSpecialStoragePolicy>()) {}

  void SetUp() override {
    ASSERT_TRUE(dir_.CreateUniqueTempDir());

    quota_manager_ = base::MakeRefCounted<storage::MockQuotaManager>(
        /*is_incognito=*/false, dir_.GetPath(),
        base::SingleThreadTaskRunner::GetCurrentDefault(),
        special_storage_policy_);
    quota_manager_proxy_ = base::MakeRefCounted<storage::MockQuotaManagerProxy>(
        quota_manager_.get(),
        base::SingleThreadTaskRunner::GetCurrentDefault());

    file_system_context_ = CreateFileSystemContextForTesting(
        quota_manager_proxy_.get(), dir_.GetPath());

    file_system_context_->OpenFileSystem(
        blink::StorageKey::CreateFromStringForTesting(kURLOrigin),
        /*bucket=*/std::nullopt, kFileSystemTypeTemporary,
        OPEN_FILE_SYSTEM_CREATE_IF_NONEXISTENT,
        base::BindOnce([](const FileSystemURL& root_url,
                          const std::string& name, base::File::Error result) {
          ASSERT_EQ(base::File::FILE_OK, result);
        }));
    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override { base::RunLoop().RunUntilIdle(); }

  std::unique_ptr<FileStreamReader> CreateFileReader(
      const std::string& file_name,
      int64_t initial_offset,
      const base::Time& expected_modification_time) override {
    return std::make_unique<SandboxFileStreamReader>(
        file_system_context_.get(), GetFileSystemURL(file_name), initial_offset,
        expected_modification_time);
  }

  void WriteFile(const std::string& file_name,
                 std::string_view data,
                 base::Time* modification_time) override {
    FileSystemURL url = GetFileSystemURL(file_name);

    ASSERT_EQ(base::File::FILE_OK, AsyncFileTestHelper::CreateFileWithData(
                                       file_system_context_.get(), url, data));

    base::File::Info file_info;
    ASSERT_EQ(base::File::FILE_OK,
              AsyncFileTestHelper::GetMetadata(file_system_context_.get(), url,
                                               &file_info));
    if (modification_time)
      *modification_time = file_info.last_modified;
  }

  void TouchFile(const std::string& file_name, base::TimeDelta delta) override {
    FileSystemURL url = GetFileSystemURL(file_name);

    base::File::Info file_info;
    ASSERT_EQ(base::File::FILE_OK,
              AsyncFileTestHelper::GetMetadata(file_system_context_.get(), url,
                                               &file_info));
    ASSERT_EQ(base::File::FILE_OK,
              AsyncFileTestHelper::TouchFile(file_system_context_.get(), url,
                                             file_info.last_accessed,
                                             file_info.last_modified + delta));
  }

  FileSystemURL GetFileSystemURL(const std::string& file_name) {
    return file_system_context_->CreateCrackedFileSystemURL(
        blink::StorageKey::CreateFromStringForTesting(kURLOrigin),
        kFileSystemTypeTemporary, base::FilePath().AppendASCII(file_name));
  }

 protected:
  scoped_refptr<MockSpecialStoragePolicy> special_storage_policy_;

  scoped_refptr<FileSystemContext> file_system_context_;
  scoped_refptr<MockQuotaManager> quota_manager_;
  scoped_refptr<MockQuotaManagerProxy> quota_manager_proxy_;
};

INSTANTIATE_TYPED_TEST_SUITE_P(FileSystem,
                               FileStreamReaderTypedTest,
                               SandboxFileStreamReaderTest);

}  // namespace storage
