// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/file_system/file_writer_delegate.h"

#include <stdint.h>

#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "components/services/storage/public/cpp/buckets/bucket_locator.h"
#include "net/base/io_buffer.h"
#include "net/base/request_priority.h"
#include "net/http/http_response_headers.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "storage/browser/blob/blob_data_builder.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_quota_util.h"
#include "storage/browser/file_system/sandbox_file_stream_writer.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/browser/test/async_file_test_helper.h"
#include "storage/browser/test/test_file_system_context.h"
#include "storage/common/file_system/file_system_mount_option.h"
#include "testing/platform_test.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace storage {

namespace {

const char kOrigin[] = "http://example.com";
const FileSystemType kFileSystemType = kFileSystemTypeTest;

const char kData[] = "The quick brown fox jumps over the lazy dog.\n";
const int kDataSize = std::size(kData) - 1;

class Result {
 public:
  Result()
      : status_(base::File::FILE_OK),
        bytes_written_(0),
        write_status_(FileWriterDelegate::SUCCESS_IO_PENDING) {}

  base::File::Error status() const { return status_; }
  int64_t bytes_written() const { return bytes_written_; }
  FileWriterDelegate::WriteProgressStatus write_status() const {
    return write_status_;
  }

  void Run() { loop_.Run(); }

  void DidWrite(base::File::Error status,
                int64_t bytes,
                FileWriterDelegate::WriteProgressStatus write_status) {
    write_status_ = write_status;
    if (status == base::File::FILE_OK) {
      bytes_written_ += bytes;
      if (write_status_ != FileWriterDelegate::SUCCESS_IO_PENDING) {
        DCHECK(!loop_.AnyQuitCalled());
        loop_.QuitWhenIdle();
      }
    } else {
      EXPECT_EQ(base::File::FILE_OK, status_);
      status_ = status;
      DCHECK(!loop_.AnyQuitCalled());
      loop_.QuitWhenIdle();
    }
  }

 private:
  base::RunLoop loop_;
  // For post-operation status.
  base::File::Error status_;
  int64_t bytes_written_;
  FileWriterDelegate::WriteProgressStatus write_status_;
};

}  // namespace

class FileWriterDelegateTest : public PlatformTest {
 public:
  FileWriterDelegateTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::IO) {}

 protected:
  void SetUp() override;
  void TearDown() override;

  int64_t usage() {
    return file_system_context_->GetQuotaUtil(kFileSystemType)
        ->GetBucketUsageOnFileTaskRunner(
            file_system_context_.get(),
            BucketLocator::ForDefaultBucket(
                blink::StorageKey::CreateFromStringForTesting(kOrigin)),
            kFileSystemType);
  }

  int64_t GetFileSizeOnDisk(const char* test_file_path) {
    // There might be in-flight flush/write.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::DoNothing());
    base::RunLoop().RunUntilIdle();

    FileSystemURL url = GetFileSystemURL(test_file_path);
    base::File::Info file_info;
    EXPECT_EQ(base::File::FILE_OK,
              AsyncFileTestHelper::GetMetadata(file_system_context_.get(), url,
                                               &file_info));
    return file_info.size;
  }

  FileSystemURL GetFileSystemURL(const char* file_name) const {
    return file_system_context_->CreateCrackedFileSystemURL(
        blink::StorageKey::CreateFromStringForTesting(kOrigin), kFileSystemType,
        base::FilePath().FromUTF8Unsafe(file_name));
  }

  std::unique_ptr<SandboxFileStreamWriter> CreateWriter(
      const char* test_file_path,
      int64_t offset,
      int64_t allowed_growth) {
    auto writer = std::make_unique<SandboxFileStreamWriter>(
        file_system_context_.get(), GetFileSystemURL(test_file_path), offset,
        *file_system_context_->GetUpdateObservers(kFileSystemType));
    writer->set_default_quota(allowed_growth);
    return writer;
  }

  std::unique_ptr<FileWriterDelegate> CreateWriterDelegate(
      const char* test_file_path,
      int64_t offset,
      int64_t allowed_growth) {
    auto writer = CreateWriter(test_file_path, offset, allowed_growth);
    return std::make_unique<FileWriterDelegate>(
        std::move(writer), FlushPolicy::FLUSH_ON_COMPLETION);
  }

  FileWriterDelegate::DelegateWriteCallback GetWriteCallback(Result* result) {
    return base::BindRepeating(&Result::DidWrite, base::Unretained(result));
  }

  // Creates and sets up a FileWriterDelegate for writing the given
  // |blob_content|, and creates a new FileWriterDelegate for the file.
  void PrepareForWrite(const char* test_file_path,
                       int64_t offset,
                       int64_t allowed_growth) {
    file_writer_delegate_ =
        CreateWriterDelegate(test_file_path, offset, allowed_growth);
  }

  std::unique_ptr<BlobDataHandle> CreateBlob(const std::string& contents) {
    auto builder = std::make_unique<BlobDataBuilder>("blob-uuid");
    builder->AppendData(contents);
    return blob_context_->AddFinishedBlob(std::move(builder));
  }

  // This should be alive until the very end of this instance.
  base::test::TaskEnvironment task_environment_;

  scoped_refptr<FileSystemContext> file_system_context_;
  std::unique_ptr<BlobStorageContext> blob_context_;

  std::unique_ptr<FileWriterDelegate> file_writer_delegate_;

  base::ScopedTempDir dir_;
};

void FileWriterDelegateTest::SetUp() {
  ASSERT_TRUE(dir_.CreateUniqueTempDir());

  file_system_context_ = CreateFileSystemContextForTesting(
      /*quota_manager_proxy=*/nullptr, dir_.GetPath());
  ASSERT_EQ(base::File::FILE_OK,
            AsyncFileTestHelper::CreateFile(file_system_context_.get(),
                                            GetFileSystemURL("test")));
  blob_context_ = std::make_unique<BlobStorageContext>();
}

void FileWriterDelegateTest::TearDown() {
  file_system_context_ = nullptr;
  base::RunLoop().RunUntilIdle();
}

TEST_F(FileWriterDelegateTest, WriteSuccessWithoutQuotaLimit) {
  auto blob = CreateBlob(kData);

  PrepareForWrite("test", 0, std::numeric_limits<int64_t>::max());

  Result result;
  ASSERT_EQ(0, usage());
  file_writer_delegate_->Start(blob->CreateReader(), GetWriteCallback(&result));
  result.Run();

  ASSERT_EQ(FileWriterDelegate::SUCCESS_COMPLETED, result.write_status());
  file_writer_delegate_.reset();

  ASSERT_EQ(kDataSize, usage());
  EXPECT_EQ(GetFileSizeOnDisk("test"), usage());
  EXPECT_EQ(kDataSize, result.bytes_written());
  EXPECT_EQ(base::File::FILE_OK, result.status());
}

TEST_F(FileWriterDelegateTest, WriteSuccessWithJustQuota) {
  auto blob = CreateBlob(kData);
  const int64_t kAllowedGrowth = kDataSize;
  PrepareForWrite("test", 0, kAllowedGrowth);

  Result result;
  ASSERT_EQ(0, usage());
  file_writer_delegate_->Start(blob->CreateReader(), GetWriteCallback(&result));
  result.Run();
  ASSERT_EQ(FileWriterDelegate::SUCCESS_COMPLETED, result.write_status());
  file_writer_delegate_.reset();

  ASSERT_EQ(kAllowedGrowth, usage());
  EXPECT_EQ(GetFileSizeOnDisk("test"), usage());

  EXPECT_EQ(kAllowedGrowth, result.bytes_written());
  EXPECT_EQ(base::File::FILE_OK, result.status());
}

TEST_F(FileWriterDelegateTest, DISABLED_WriteFailureByQuota) {
  auto blob = CreateBlob(kData);
  const int64_t kAllowedGrowth = kDataSize - 1;
  PrepareForWrite("test", 0, kAllowedGrowth);

  Result result;
  ASSERT_EQ(0, usage());
  file_writer_delegate_->Start(blob->CreateReader(), GetWriteCallback(&result));
  result.Run();
  ASSERT_EQ(FileWriterDelegate::ERROR_WRITE_STARTED, result.write_status());
  file_writer_delegate_.reset();

  ASSERT_EQ(kAllowedGrowth, usage());
  EXPECT_EQ(GetFileSizeOnDisk("test"), usage());

  EXPECT_EQ(kAllowedGrowth, result.bytes_written());
  EXPECT_EQ(base::File::FILE_ERROR_NO_SPACE, result.status());
  ASSERT_EQ(FileWriterDelegate::ERROR_WRITE_STARTED, result.write_status());
}

TEST_F(FileWriterDelegateTest, WriteZeroBytesSuccessfullyWithZeroQuota) {
  auto blob = CreateBlob("");
  int64_t kAllowedGrowth = 0;
  PrepareForWrite("test", 0, kAllowedGrowth);

  Result result;
  ASSERT_EQ(0, usage());
  file_writer_delegate_->Start(blob->CreateReader(), GetWriteCallback(&result));
  result.Run();
  ASSERT_EQ(FileWriterDelegate::SUCCESS_COMPLETED, result.write_status());
  file_writer_delegate_.reset();

  ASSERT_EQ(kAllowedGrowth, usage());
  EXPECT_EQ(GetFileSizeOnDisk("test"), usage());

  EXPECT_EQ(kAllowedGrowth, result.bytes_written());
  EXPECT_EQ(base::File::FILE_OK, result.status());
  ASSERT_EQ(FileWriterDelegate::SUCCESS_COMPLETED, result.write_status());
}

TEST_F(FileWriterDelegateTest, WriteSuccessWithoutQuotaLimitConcurrent) {
  std::unique_ptr<FileWriterDelegate> file_writer_delegate2;

  ASSERT_EQ(base::File::FILE_OK,
            AsyncFileTestHelper::CreateFile(file_system_context_.get(),
                                            GetFileSystemURL("test2")));

  auto blob = CreateBlob(kData);

  PrepareForWrite("test", 0, std::numeric_limits<int64_t>::max());

  // Create another FileWriterDelegate for concurrent write.
  file_writer_delegate2 =
      CreateWriterDelegate("test2", 0, std::numeric_limits<int64_t>::max());

  Result result, result2;
  ASSERT_EQ(0, usage());
  file_writer_delegate_->Start(blob->CreateReader(), GetWriteCallback(&result));
  file_writer_delegate2->Start(blob->CreateReader(),
                               GetWriteCallback(&result2));
  result.Run();
  if (result.write_status() == FileWriterDelegate::SUCCESS_IO_PENDING ||
      result2.write_status() == FileWriterDelegate::SUCCESS_IO_PENDING)
    result2.Run();

  ASSERT_EQ(FileWriterDelegate::SUCCESS_COMPLETED, result.write_status());
  ASSERT_EQ(FileWriterDelegate::SUCCESS_COMPLETED, result2.write_status());
  file_writer_delegate_.reset();
  file_writer_delegate2.reset();

  ASSERT_EQ(kDataSize * 2, usage());
  EXPECT_EQ(GetFileSizeOnDisk("test") + GetFileSizeOnDisk("test2"), usage());

  EXPECT_EQ(kDataSize, result.bytes_written());
  EXPECT_EQ(base::File::FILE_OK, result.status());
  EXPECT_EQ(kDataSize, result2.bytes_written());
  EXPECT_EQ(base::File::FILE_OK, result2.status());
}

TEST_F(FileWriterDelegateTest, WritesWithQuotaAndOffset) {
  auto blob = CreateBlob(kData);

  // Writing kDataSize (=45) bytes data while allowed_growth is 100.
  int64_t offset = 0;
  int64_t allowed_growth = 100;
  ASSERT_LT(kDataSize, allowed_growth);
  PrepareForWrite("test", offset, allowed_growth);

  {
    Result result;
    ASSERT_EQ(0, usage());
    file_writer_delegate_->Start(blob->CreateReader(),
                                 GetWriteCallback(&result));
    result.Run();
    ASSERT_EQ(FileWriterDelegate::SUCCESS_COMPLETED, result.write_status());
    file_writer_delegate_.reset();

    ASSERT_EQ(kDataSize, usage());
    EXPECT_EQ(GetFileSizeOnDisk("test"), usage());
    EXPECT_EQ(kDataSize, result.bytes_written());
    EXPECT_EQ(base::File::FILE_OK, result.status());
  }

  // Trying to overwrite kDataSize bytes data while allowed_growth is 20.
  offset = 0;
  allowed_growth = 20;
  PrepareForWrite("test", offset, allowed_growth);

  {
    Result result;
    file_writer_delegate_->Start(blob->CreateReader(),
                                 GetWriteCallback(&result));
    result.Run();
    EXPECT_EQ(kDataSize, usage());
    EXPECT_EQ(GetFileSizeOnDisk("test"), usage());
    EXPECT_EQ(kDataSize, result.bytes_written());
    EXPECT_EQ(base::File::FILE_OK, result.status());
    ASSERT_EQ(FileWriterDelegate::SUCCESS_COMPLETED, result.write_status());
  }

  // Trying to write kDataSize bytes data from offset 25 while
  // allowed_growth is 55.
  offset = 25;
  allowed_growth = 55;
  PrepareForWrite("test", offset, allowed_growth);

  {
    Result result;
    file_writer_delegate_->Start(blob->CreateReader(),
                                 GetWriteCallback(&result));
    result.Run();
    ASSERT_EQ(FileWriterDelegate::SUCCESS_COMPLETED, result.write_status());
    file_writer_delegate_.reset();

    EXPECT_EQ(offset + kDataSize, usage());
    EXPECT_EQ(GetFileSizeOnDisk("test"), usage());
    EXPECT_EQ(kDataSize, result.bytes_written());
    EXPECT_EQ(base::File::FILE_OK, result.status());
  }

  // Trying to overwrite 45 bytes data while allowed_growth is -20.
  offset = 0;
  allowed_growth = -20;
  PrepareForWrite("test", offset, allowed_growth);
  int64_t pre_write_usage = GetFileSizeOnDisk("test");

  {
    Result result;
    file_writer_delegate_->Start(blob->CreateReader(),
                                 GetWriteCallback(&result));
    result.Run();
    ASSERT_EQ(FileWriterDelegate::SUCCESS_COMPLETED, result.write_status());
    file_writer_delegate_.reset();

    EXPECT_EQ(pre_write_usage, usage());
    EXPECT_EQ(GetFileSizeOnDisk("test"), usage());
    EXPECT_EQ(kDataSize, result.bytes_written());
    EXPECT_EQ(base::File::FILE_OK, result.status());
  }

  // Trying to overwrite 45 bytes data with offset pre_write_usage - 20,
  // while allowed_growth is 10.
  const int kOverlap = 20;
  offset = pre_write_usage - kOverlap;
  allowed_growth = 10;
  PrepareForWrite("test", offset, allowed_growth);

  {
    Result result;
    file_writer_delegate_->Start(blob->CreateReader(),
                                 GetWriteCallback(&result));
    result.Run();
    ASSERT_EQ(FileWriterDelegate::ERROR_WRITE_STARTED, result.write_status());
    file_writer_delegate_.reset();

    EXPECT_EQ(pre_write_usage + allowed_growth, usage());
    EXPECT_EQ(GetFileSizeOnDisk("test"), usage());
    EXPECT_EQ(kOverlap + allowed_growth, result.bytes_written());
    EXPECT_EQ(base::File::FILE_ERROR_NO_SPACE, result.status());
  }
}

}  // namespace storage
