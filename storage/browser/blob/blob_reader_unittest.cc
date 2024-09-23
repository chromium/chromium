// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "storage/browser/blob/blob_reader.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/heap_array.h"
#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/numerics/safe_conversions.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_runner.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "net/base/completion_once_callback.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "storage/browser/blob/blob_data_builder.h"
#include "storage/browser/blob/blob_data_handle.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "storage/browser/file_system/file_stream_reader.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_file_util.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/browser/test/async_file_test_helper.h"
#include "storage/browser/test/fake_blob_data_handle.h"
#include "storage/browser/test/test_file_system_context.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using base::FilePath;
using net::DrainableIOBuffer;
using net::IOBuffer;

namespace storage {
namespace {

using FileCreationInfo = BlobMemoryController::FileCreationInfo;

void SaveBlobStatusAndFiles(BlobStatus* status_ptr,
                            std::vector<FileCreationInfo>* files_ptr,
                            BlobStatus status,
                            std::vector<FileCreationInfo> files) {
  *status_ptr = status;
  for (FileCreationInfo& info : files) {
    files_ptr->push_back(std::move(info));
  }
}

template <typename T>
void SetValue(T* address, T value) {
  *address = value;
}

class FakeFileStreamReader : public FileStreamReader {
 public:
  explicit FakeFileStreamReader(const std::string& contents)
      : buffer_(base::MakeRefCounted<DrainableIOBuffer>(
            base::MakeRefCounted<net::StringIOBuffer>(contents),
            contents.size())),
        net_error_(net::OK),
        size_(contents.size()) {}
  FakeFileStreamReader(const std::string& contents, uint64_t size)
      : buffer_(base::MakeRefCounted<DrainableIOBuffer>(
            base::MakeRefCounted<net::StringIOBuffer>(contents),
            contents.size())),
        net_error_(net::OK),
        size_(size) {}

  FakeFileStreamReader(const FakeFileStreamReader&) = delete;
  FakeFileStreamReader& operator=(const FakeFileStreamReader&) = delete;

  ~FakeFileStreamReader() override = default;

  void SetReturnError(int net_error) { net_error_ = net_error; }

  void SetAsyncRunner(base::SingleThreadTaskRunner* runner) {
    async_task_runner_ = runner;
  }

  int Read(net::IOBuffer* buf,
           int buf_length,
           net::CompletionOnceCallback done) override {
    DCHECK(buf);
    // When async_task_runner_ is not set, return synchronously.
    if (!async_task_runner_.get()) {
      if (net_error_ == net::OK) {
        return ReadImpl(buf, buf_length, net::CompletionOnceCallback());
      } else {
        return net_error_;
      }
    }

    // Otherwise always return asynchronously.
    if (net_error_ == net::OK) {
      async_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(base::IgnoreResult(&FakeFileStreamReader::ReadImpl),
                         base::Unretained(this), base::WrapRefCounted(buf),
                         buf_length, std::move(done)));
    } else {
      async_task_runner_->PostTask(FROM_HERE,
                                   base::BindOnce(std::move(done), net_error_));
    }
    return net::ERR_IO_PENDING;
  }

  int64_t GetLength(net::Int64CompletionOnceCallback size_callback) override {
    // When async_task_runner_ is not set, return synchronously.
    if (!async_task_runner_.get()) {
      if (net_error_ == net::OK) {
        return size_;
      } else {
        return net_error_;
      }
    }
    if (net_error_ == net::OK) {
      async_task_runner_->PostTask(
          FROM_HERE, base::BindOnce(std::move(size_callback), size_));
    } else {
      async_task_runner_->PostTask(
          FROM_HERE, base::BindOnce(std::move(size_callback),
                                    static_cast<int64_t>(net_error_)));
    }
    return net::ERR_IO_PENDING;
  }

 private:
  int ReadImpl(scoped_refptr<net::IOBuffer> buf,
               int buf_length,
               net::CompletionOnceCallback done) {
    CHECK_GE(buf_length, 0);
    int length = std::min(buf_length, buffer_->BytesRemaining());
    memcpy(buf->data(), buffer_->data(), length);
    buffer_->DidConsume(length);
    if (done.is_null()) {
      return length;
    }
    std::move(done).Run(length);
    return net::ERR_IO_PENDING;
  }

  scoped_refptr<net::DrainableIOBuffer> buffer_;
  scoped_refptr<base::SingleThreadTaskRunner> async_task_runner_;
  int net_error_;
  uint64_t size_;
};

class MockFileStreamReaderProvider
    : public BlobReader::FileStreamReaderProvider {
 public:
  ~MockFileStreamReaderProvider() override = default;

  MOCK_METHOD4(CreateForLocalFileMock,
               FileStreamReader*(base::TaskRunner* task_runner,
                                 const FilePath& file_path,
                                 int64_t initial_offset,
                                 const base::Time& expected_modification_time));
  MOCK_METHOD4(CreateFileStreamReaderMock,
               FileStreamReader*(const GURL& filesystem_url,
                                 int64_t offset,
                                 int64_t max_bytes_to_read,
                                 const base::Time& expected_modification_time));
  // Since we're returning a move-only type, we have to do some delegation for
  // gmock.
  std::unique_ptr<FileStreamReader> CreateForLocalFile(
      base::TaskRunner* task_runner,
      const base::FilePath& file_path,
      int64_t initial_offset,
      const base::Time& expected_modification_time) override {
    return base::WrapUnique(CreateForLocalFileMock(
        task_runner, file_path, initial_offset, expected_modification_time));
  }

  std::unique_ptr<FileStreamReader> CreateFileStreamReader(
      const GURL& filesystem_url,
      int64_t offset,
      int64_t max_bytes_to_read,
      const base::Time& expected_modification_time) override {
    return base::WrapUnique(CreateFileStreamReaderMock(
        filesystem_url, offset, max_bytes_to_read, expected_modification_time));
  }
};

}  // namespace

class BlobReaderTest : public ::testing::Test {
 public:
  BlobReaderTest() = default;

  BlobReaderTest(const BlobReaderTest&) = delete;
  BlobReaderTest& operator=(const BlobReaderTest&) = delete;

  ~BlobReaderTest() override = default;

  void SetUp() override {
    file_system_context_ = CreateFileSystemContextForTesting(
        /*quota_manager_proxy=*/nullptr, base::FilePath());
  }

  void TearDown() override {
    provider_ = nullptr;
    reader_.reset();
    blob_handle_.reset();
    base::RunLoop().RunUntilIdle();
    base::RunLoop().RunUntilIdle();
  }

 protected:
  void InitializeReader(std::unique_ptr<BlobDataBuilder> builder) {
    blob_handle_ =
        builder ? context_.AddFinishedBlob(std::move(builder)) : nullptr;
    provider_ = new MockFileStreamReaderProvider();
    reader_.reset(new BlobReader(blob_handle_.get()));
    reader_->SetFileStreamProviderForTesting(base::WrapUnique(provider_.get()));
  }

  // Takes ownership of the file reader (the blob reader takes ownership).
  void ExpectLocalFileCall(const FilePath& file_path,
                           base::Time modification_time,
                           uint64_t initial_offset,
                           FakeFileStreamReader* reader) {
    EXPECT_CALL(*provider_,
                CreateForLocalFileMock(testing::_, file_path, initial_offset,
                                       modification_time))
        .WillOnce(testing::Return(reader));
  }

  // Takes ownership of the file reader (the blob reader takes ownership).
  void ExpectFileSystemCall(const GURL& filesystem_url,
                            int64_t offset,
                            int64_t max_bytes_to_read,
                            base::Time expected_modification_time,
                            FakeFileStreamReader* reader) {
    EXPECT_CALL(*provider_, CreateFileStreamReaderMock(
                                filesystem_url, offset, max_bytes_to_read,
                                expected_modification_time))
        .WillOnce(testing::Return(reader));
  }

  void CheckSizeCalculatedSynchronously(size_t expected_size, int async_size) {
    EXPECT_EQ(-1, async_size);
    EXPECT_EQ(net::OK, reader_->net_error());
    EXPECT_EQ(expected_size, reader_->total_size());
    EXPECT_TRUE(reader_->total_size_calculated());
  }

  void CheckSizeNotCalculatedYet(int async_size) {
    EXPECT_EQ(-1, async_size);
    EXPECT_EQ(net::OK, reader_->net_error());
    EXPECT_FALSE(reader_->total_size_calculated());
  }

  void CheckSizeCalculatedAsynchronously(size_t expected_size,
                                         int async_result) {
    EXPECT_EQ(net::OK, async_result);
    EXPECT_EQ(net::OK, reader_->net_error());
    EXPECT_EQ(expected_size, reader_->total_size());
    EXPECT_TRUE(reader_->total_size_calculated());
  }

  scoped_refptr<net::IOBuffer> CreateBuffer(uint64_t size) {
    return base::MakeRefCounted<net::IOBufferWithSize>(
        static_cast<size_t>(size));
  }

  bool IsReaderTotalSizeCalculated() {
    return reader_->total_size_calculated();
  }

  base::test::TaskEnvironment task_environment_;

  BlobStorageContext context_;
  std::unique_ptr<BlobDataHandle> blob_handle_;
  std::unique_ptr<BlobReader> reader_;
  raw_ptr<MockFileStreamReaderProvider> provider_ = nullptr;
  scoped_refptr<FileSystemContext> file_system_context_;
};

TEST_F(BlobReaderTest, BasicMemory) {
  auto b = std::make_unique<BlobDataBuilder>("uuid");
  const std::string kData("Hello!!!");
  const size_t kDataSize = 8ul;
  b->AppendData(kData);
  this->InitializeReader(std::move(b));

  int size_result = -1;
  EXPECT_FALSE(IsReaderTotalSizeCalculated());
  EXPECT_EQ(BlobReader::Status::DONE, reader_->CalculateSize(base::BindOnce(
                                          &SetValue<int>, &size_result)));
  EXPECT_TRUE(reader_->IsInMemory());
  CheckSizeCalculatedSynchronously(kDataSize, size_result);

  auto buffer = base::MakeRefCounted<net::IOBufferWithSize>(kDataSize);

  int bytes_read = 0;
  int async_bytes_read = 0;
  EXPECT_EQ(BlobReader::Status::DONE,
            reader_->Read(buffer.get(), kDataSize, &bytes_read,
                          base::BindOnce(&SetValue<int>, &async_bytes_read)));
  EXPECT_EQ(net::OK, reader_->net_error());
  EXPECT_EQ(kDataSize, static_cast<size_t>(bytes_read));
  EXPECT_EQ(0, async_bytes_read);
  EXPECT_EQ(0, memcmp(buffer->data(), "Hello!!!", kDataSize));
}

TEST_F(BlobReaderTest, BasicFile) {
  auto b = std::make_unique<BlobDataBuilder>("uuid");
  const FilePath kPath = FilePath::FromUTF8Unsafe("/fake/file.txt");
  const std::string kData = "FileData!!!";
  const base::Time kTime = base::Time::Now();
  b->AppendFile(kPath, 0, kData.size(), kTime);
  this->InitializeReader(std::move(b));

  // Non-async reader.
  ExpectLocalFileCall(kPath, kTime, 0, new FakeFileStreamReader(kData));

  int size_result = -1;
  EXPECT_FALSE(IsReaderTotalSizeCalculated());
  EXPECT_EQ(BlobReader::Status::DONE, reader_->CalculateSize(base::BindOnce(
                                          &SetValue<int>, &size_result)));
  EXPECT_FALSE(reader_->IsInMemory());
  CheckSizeCalculatedSynchronously(kData.size(), size_result);

  auto buffer = base::MakeRefCounted<net::IOBufferWithSize>(kData.size());

  int bytes_read = 0;
  int async_bytes_read = 0;
  EXPECT_EQ(BlobReader::Status::DONE,
            reader_->Read(buffer.get(), kData.size(), &bytes_read,
                          base::BindOnce(&SetValue<int>, &async_bytes_read)));
  EXPECT_EQ(net::OK, reader_->net_error());
  EXPECT_EQ(kData.size(), static_cast<size_t>(bytes_read));
  EXPECT_EQ(0, async_bytes_read);
  EXPECT_EQ(0, memcmp(buffer->data(), "FileData!!!", kData.size()));
}

TEST_F(BlobReaderTest, BasicFileSystem) {
  auto b = std::make_unique<BlobDataBuilder>("uuid");
  const GURL kURL("filesystem:http://example.com/temporary/test_file/here.txt");
  const std::string kData = "FileData!!!";
  const base::Time kTime = base::Time::Now();
  b->AppendFileSystemFile(
      file_system_context_->CrackURLInFirstPartyContext(kURL), 0, kData.size(),
      kTime, file_system_context_);
  this->InitializeReader(std::move(b));
  // Non-async reader.
  ExpectFileSystemCall(kURL, 0, kData.size(), kTime,
                       new FakeFileStreamReader(kData));

  int size_result = -1;
  EXPECT_FALSE(IsReaderTotalSizeCalculated());
  EXPECT_EQ(BlobReader::Status::DONE, reader_->CalculateSize(base::BindOnce(
                                          &SetValue<int>, &size_result)));
  EXPECT_FALSE(reader_->IsInMemory());

  CheckSizeCalculatedSynchronously(kData.size(), size_result);

  auto buffer = base::MakeRefCounted<net::IOBufferWithSize>(kData.size());

  int bytes_read = 0;
  int async_bytes_read = 0;
  EXPECT_EQ(BlobReader::Status::DONE,
            reader_->Read(buffer.get(), kData.size(), &bytes_read,
                          base::BindOnce(&SetValue<int>, &async_bytes_read)));
  EXPECT_EQ(net::OK, reader_->net_error());
  EXPECT_EQ(kData.size(), static_cast<size_t>(bytes_read));
  EXPECT_EQ(0, async_bytes_read);
  EXPECT_EQ(0, memcmp(buffer->data(), "FileData!!!", kData.size()));
}

TEST_F(BlobReaderTest, BasicReadableDataHandle) {
  auto b = std::make_unique<BlobDataBuilder>("uuid");
  const std::string kData = "Test Blob Data";
  auto data_handle = base::MakeRefCounted<FakeBlobDataHandle>(kData, "");
  b->AppendReadableDataHandle(std::move(data_handle));
  this->InitializeReader(std::move(b));

  int size_result = -1;
  EXPECT_FALSE(IsReaderTotalSizeCalculated());
  EXPECT_EQ(BlobReader::Status::DONE, reader_->CalculateSize(base::BindOnce(
                                          &SetValue<int>, &size_result)));
  CheckSizeCalculatedSynchronously(kData.size(), size_result);

  EXPECT_FALSE(reader_->has_side_data());

  auto buffer = base::MakeRefCounted<net::IOBufferWithSize>(kData.size());

  int bytes_read = 0;
  int async_bytes_read = 0;
  EXPECT_EQ(BlobReader::Status::DONE,
            reader_->Read(buffer.get(), kData.size(), &bytes_read,
                          base::BindOnce(&SetValue<int>, &async_bytes_read)));
  EXPECT_EQ(net::OK, reader_->net_error());
  EXPECT_EQ(kData.size(), static_cast<size_t>(bytes_read));
  EXPECT_EQ(0, async_bytes_read);
  EXPECT_EQ(0, memcmp(buffer->data(), "Test Blob Data", kData.size()));
}

TEST_F(BlobReaderTest, ReadableDataHandleWithSideData) {
  auto b = std::make_unique<BlobDataBuilder>("uuid");
  const std::string kData = "Test Blob Data";
  const std::string kSideData = "Test side data";
  auto data_handle = base::MakeRefCounted<FakeBlobDataHandle>(kData, kSideData);
  b->AppendReadableDataHandle(std::move(data_handle));
  this->InitializeReader(std::move(b));

  int size_result = -1;
  EXPECT_FALSE(IsReaderTotalSizeCalculated());
  EXPECT_EQ(BlobReader::Status::DONE, reader_->CalculateSize(base::BindOnce(
                                          &SetValue<int>, &size_result)));
  CheckSizeCalculatedSynchronously(kData.size(), size_result);

  EXPECT_TRUE(reader_->has_side_data());
  BlobReader::Status status = BlobReader::Status::NET_ERROR;
  // As this is using a FakeBlobDataHandle, expect this to be synchronous.
  reader_->ReadSideData(base::BindOnce(&SetValue<BlobReader::Status>, &status));
  EXPECT_EQ(net::OK, reader_->net_error());
  EXPECT_EQ(BlobReader::Status::DONE, status);
  auto side_data = reader_->TakeSideData();
  EXPECT_TRUE(side_data.has_value());
  std::string result(reinterpret_cast<const char*>(side_data->data()),
                     side_data->size());
  EXPECT_EQ(kSideData, result);
}

TEST_F(BlobReaderTest, BufferLargerThanMemory) {
  auto b = std::make_unique<BlobDataBuilder>("uuid");
  const std::string kData("Hello!!!");
  const size_t kDataSize = 8ul;
  const size_t kBufferSize = 10ul;
  b->AppendData(kData);
  this->InitializeReader(std::move(b));

  int size_result = -1;
  EXPECT_FALSE(IsReaderTotalSizeCalculated());
  EXPECT_EQ(BlobReader::Status::DONE, reader_->CalculateSize(base::BindOnce(
                                          &SetValue<int>, &size_result)));
  CheckSizeCalculatedSynchronously(kData.size(), size_result);

  auto buffer = base::MakeRefCounted<net::IOBufferWithSize>(kBufferSize);

  int bytes_read = 0;
  int async_bytes_read = 0;
  EXPECT_EQ(BlobReader::Status::DONE,
            reader_->Read(buffer.get(), kBufferSize, &bytes_read,
                          base::BindOnce(&SetValue<int>, &async_bytes_read)));
  EXPECT_EQ(net::OK, reader_->net_error());
  EXPECT_EQ(kDataSize, static_cast<size_t>(bytes_read));
  EXPECT_EQ(0, async_bytes_read);
  EXPECT_EQ(0, memcmp(buffer->data(), "Hello!!!", kDataSize));
}

TEST_F(BlobReaderTest, MemoryRange) {
  auto b = std::make_unique<BlobDataBuilder>("uuid");
  const std::string kData("Hello!!!");
  const size_t kDataSize = 8ul;
  const size_t kSeekOffset = 2ul;
  const uint64_t kReadLength = 4ull;
  b->AppendData(kData);
  this->InitializeReader(std::move(b));

  int size_result = -1;
  EXPECT_FALSE(IsReaderTotalSizeCalculated());
  EXPECT_EQ(BlobReader::Status::DONE, reader_->CalculateSize(base::BindOnce(
                                          &SetValue<int>, &size_result)));
  CheckSizeCalculatedSynchronously(kData.size(), size_result);

  scoped_refptr<net::IOBuffer> buffer = CreateBuffer(kReadLength);

  reader_->SetReadRange(kSeekOffset, kReadLength);
  int bytes_read = 0;
  int async_bytes_read = 0;
  EXPECT_EQ(BlobReader::Status::DONE,
            reader_->Read(buffer.get(), kDataSize - kSeekOffset, &bytes_read,
                          base::BindOnce(&SetValue<int>, &async_bytes_read)));
  EXPECT_EQ(net::OK, reader_->net_error());
  EXPECT_EQ(kReadLength, static_cast<size_t>(bytes_read));
  EXPECT_EQ(0, async_bytes_read);
  EXPECT_EQ(0, memcmp(buffer->data(), "llo!", kReadLength));
}

TEST_F(BlobReaderTest, BufferSmallerThanMemory) {
  auto b = std::make_unique<BlobDataBuilder>("uuid");
  const std::string kData("Hello!!!");
  const size_t kBufferSize = 4ul;
  b->AppendData(kData);
  this->InitializeReader(std::move(b));

  int size_result = -1;
  EXPECT_FALSE(IsReaderTotalSizeCalculated());
  EXPECT_EQ(BlobReader::Status::DONE, reader_->CalculateSize(base::BindOnce(
                                          &SetValue<int>, &size_result)));
  CheckSizeCalculatedSynchronously(kData.size(), size_result);

  auto buffer = base::MakeRefCounted<net::IOBufferWithSize>(kBufferSize);

  int bytes_read = 0;
  int async_bytes_read = 0;
  EXPECT_EQ(BlobReader::Status::DONE,
            reader_->Read(buffer.get(), kBufferSize, &bytes_read,
                          base::BindOnce(&SetValue<int>, &async_bytes_read)));
  EXPECT_EQ(net::OK, reader_->net_error());
  EXPECT_EQ(kBufferSize, static_cast<size_t>(bytes_read));
  EXPECT_EQ(0, async_bytes_read);
  EXPECT_EQ(0, memcmp(buffer->data(), "Hell", kBufferSize));

  bytes_read = 0;
  EXPECT_EQ(BlobReader::Status::DONE,
            reader_->Read(buffer.get(), kBufferSize, &bytes_read,
                          base::BindOnce(&SetValue<int>, &async_bytes_read)));
  EXPECT_EQ(net::OK, reader_->net_error());
  EXPECT_EQ(kBufferSize, static_cast<size_t>(bytes_read));
  EXPECT_EQ(0, async_bytes_read);
  EXPECT_EQ(0, memcmp(buffer->data(), "o!!!", kBufferSize));
}

TEST_F(BlobReaderTest, SegmentedBufferAndMemory) {
  auto b = std::make_unique<BlobDataBuilder>("uuid");
  const size_t kNumItems = 10;
  const size_t kItemSize = 6;
  const size_t kBufferSize = 10;
  const size_t kTotalSize = kNumItems * kItemSize;
  char current_value = 0;
  for (size_t i = 0; i < kNumItems; i++) {
    char buf[kItemSize];
    for (size_t j = 0; j < kItemSize; j++) {
      buf[j] = current_value++;
    }
    b->AppendData(std::string(buf, kItemSize));
  }
  this->InitializeReader(std::move(b));

  int size_result = -1;
  EXPECT_FALSE(IsReaderTotalSizeCalculated());
  EXPECT_EQ(BlobReader::Status::DONE, reader_->CalculateSize(base::BindOnce(
                                          &SetValue<int>, &size_result)));
  CheckSizeCalculatedSynchronously(kTotalSize, size_result);

  auto buffer = base::MakeRefCounted<net::IOBufferWithSize>(kBufferSize);

  current_value = 0;
  for (size_t i = 0; i < kTotalSize / kBufferSize; i++) {
    int bytes_read = 0;
    int async_bytes_read = 0;
    EXPECT_EQ(BlobReader::Status::DONE,
              reader_->Read(buffer.get(), kBufferSize, &bytes_read,
                            base::BindOnce(&SetValue<int>, &async_bytes_read)));
    EXPECT_EQ(net::OK, reader_->net_error());
    EXPECT_EQ(kBufferSize, static_cast<size_t>(bytes_read));
    EXPECT_EQ(0, async_bytes_read);
    for (size_t j = 0; j < kBufferSize; j++) {
      EXPECT_EQ(current_value, buffer->data()[j]);
      current_value++;
    }
  }
}

TEST_F(BlobReaderTest, FileAsync) {
  auto b = std::make_unique<BlobDataBuilder>("uuid");
  const FilePath kPath = FilePath::FromUTF8Unsafe("/fake/file.txt");
  const std::string kData = "FileData!!!";
  const base::Time kTime = base::Time::Now();
  b->AppendFile(kPath, 0, kData.size(), kTime);
  this->InitializeReader(std::move(b));

  std::unique_ptr<FakeFileStreamReader> reader(new FakeFileStreamReader(kData));
  reader->SetAsyncRunner(
      base::SingleThreadTaskRunner::GetCurrentDefault().get());

  ExpectLocalFileCall(kPath, kTime, 0, reader.release());

  int size_result = -1;
  EXPECT_FALSE(IsReaderTotalSizeCalculated());
  EXPECT_EQ(
      BlobReader::Status::IO_PENDING,
      reader_->CalculateSize(base::BindOnce(&SetValue<int>, &size_result)));
  EXPECT_FALSE(reader_->IsInMemory());
  CheckSizeNotCalculatedYet(size_result);
  base::RunLoop().RunUntilIdle();
  CheckSizeCalculatedAsynchronously(kData.size(), size_result);

  auto buffer = base::MakeRefCounted<net::IOBufferWithSize>(kData.size());

  int bytes_read = 0;
  int async_bytes_read = 0;
  EXPECT_EQ(BlobReader::Status::IO_PENDING,
            reader_->Read(buffer.get(), kData.size(), &bytes_read,
                          base::BindOnce(&SetValue<int>, &async_bytes_read)));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(net::OK, reader_->net_error());
  EXPECT_EQ(kData.size(), static_cast<size_t>(async_bytes_read));
  EXPECT_EQ(0, bytes_read);
  EXPECT_EQ(0, memcmp(buffer->data(), "FileData!!!", kData.size()));
}

TEST_F(BlobReaderTest, FileSystemAsync) {
  auto b = std::make_unique<BlobDataBuilder>("uuid");
  const GURL kURL("filesystem:http://example.com/temporary/test_file/here.txt");
  const std::string kData = "FileData!!!";
  const base::Time kTime = base::Time::Now();
  b->AppendFileSystemFile(
      file_system_context_->CrackURLInFirstPartyContext(kURL), 0, kData.size(),
      kTime, file_system_context_);
  this->InitializeReader(std::move(b));

  std::unique_ptr<FakeFileStreamReader> reader(new FakeFileStreamReader(kData));
  reader->SetAsyncRunner(
      base::SingleThreadTaskRunner::GetCurrentDefault().get());

  ExpectFileSystemCall(kURL, 0, kData.size(), kTime, reader.release());

  int size_result = -1;
  EXPECT_FALSE(IsReaderTotalSizeCalculated());
  EXPECT_EQ(
      BlobReader::Status::IO_PENDING,
      reader_->CalculateSize(base::BindOnce(&SetValue<int>, &size_result)));
  CheckSizeNotCalculatedYet(size_result);
  EXPECT_FALSE(reader_->IsInMemory());
  base::RunLoop().RunUntilIdle();
  CheckSizeCalculatedAsynchronously(kData.size(), size_result);

  auto buffer = base::MakeRefCounted<net::IOBufferWithSize>(kData.size());

  int bytes_read = 0;
  int async_bytes_read = 0;
  EXPECT_EQ(BlobReader::Status::IO_PENDING,
            reader_->Read(buffer.get(), kData.size(), &bytes_read,
                          base::BindOnce(&SetValue<int>, &async_bytes_read)));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(net::OK, reader_->net_error());
  EXPECT_EQ(kData.size(), static_cast<size_t>(async_bytes_read));
  EXPECT_EQ(0, bytes_read);
  EXPECT_EQ(0, memcmp(buffer->data(), "FileData!!!", kData.size()));
}

TEST_F(BlobReaderTest, ReadableDataHandleSingle) {
  auto b = std::make_unique<BlobDataBuilder>("uuid");
  const std::string kOrigData = "12345 Test Blob Data 12345";
  auto data_handle = base::MakeRefCounted<FakeBlobDataHandle>(kOrigData, "");
  b->AppendReadableDataHandle(data_handle, 6, 14);
  this->InitializeReader(std::move(b));
  const std::string kData = kOrigData.substr(6, 14);

  int size_result = -1;
  EXPECT_FALSE(IsReaderTotalSizeCalculated());
  EXPECT_EQ(BlobReader::Status::DONE, reader_->CalculateSize(base::BindOnce(
                                          &SetValue<int>, &size_result)));
  CheckSizeCalculatedSynchronously(kData.size(), size_result);

  // This test checks the optimized single mojo data item path, where the
  // data pipe passed in gets passed directly to the MojoDataItem.
  EXPECT_TRUE(reader_->IsSingleMojoDataItem());

  mojo::ScopedDataPipeProducerHandle producer;
  mojo::ScopedDataPipeConsumerHandle consumer;
  MojoResult pipe_result = mojo::CreateDataPipe(nullptr, producer, consumer);
  ASSERT_EQ(MOJO_RESULT_OK, pipe_result);

  std::optional<int> read_result;
  base::RunLoop read_loop;
  reader_->ReadSingleMojoDataItem(std::move(producer),
                                  base::BindLambdaForTesting([&](int result) {
                                    read_result = result;
                                    read_loop.Quit();
                                  }));
  read_loop.Run();

  ASSERT_TRUE(read_result.has_value());
  EXPECT_EQ(*read_result, net::OK);

  size_t num_bytes = kData.size();
  std::vector<uint8_t> buffer(num_bytes);
  MojoReadDataFlags flags = MOJO_READ_DATA_FLAG_ALL_OR_NONE;
  EXPECT_EQ(MOJO_RESULT_OK, consumer->ReadData(flags, buffer, num_bytes));

  EXPECT_EQ(base::as_string_view(base::as_byte_span(buffer)), kData);
}

// This test is the same as ReadableDataHandleSingle, but adds the
// additional wrinkle of a SetReadRange call.
TEST_F(BlobReaderTest, ReadableDataHandleSingleRange) {
  auto b = std::make_unique<BlobDataBuilder>("uuid");
  const std::string kOrigData = "12345 Test Blob Data 12345";
  auto data_handle = base::MakeRefCounted<FakeBlobDataHandle>(kOrigData, "");
  b->AppendReadableDataHandle(data_handle, 6, 14);
  this->InitializeReader(std::move(b));
  const std::string kData = kOrigData.substr(6, 14);

  int size_result = -1;
  EXPECT_FALSE(IsReaderTotalSizeCalculated());
  EXPECT_EQ(BlobReader::Status::DONE, reader_->CalculateSize(base::BindOnce(
                                          &SetValue<int>, &size_result)));
  CheckSizeCalculatedSynchronously(kData.size(), size_result);

  // This test checks the optimized single mojo data item path, where the
  // data pipe passed in gets passed directly to the MojoDataItem.
  EXPECT_TRUE(reader_->IsSingleMojoDataItem());

  uint64_t range_start = 3;
  uint64_t range_length = 6;
  reader_->SetReadRange(range_start, range_length);

  mojo::ScopedDataPipeProducerHandle producer;
  mojo::ScopedDataPipeConsumerHandle consumer;
  MojoResult pipe_result = mojo::CreateDataPipe(nullptr, producer, consumer);
  ASSERT_EQ(MOJO_RESULT_OK, pipe_result);

  std::optional<int> read_result;
  base::RunLoop read_loop;
  reader_->ReadSingleMojoDataItem(std::move(producer),
                                  base::BindLambdaForTesting([&](int result) {
                                    read_result = result;
                                    read_loop.Quit();
                                  }));
  read_loop.Run();

  ASSERT_TRUE(read_result.has_value());
  EXPECT_EQ(*read_result, net::OK);

  size_t num_bytes = range_length;
  std::vector<uint8_t> buffer(num_bytes);
  MojoReadDataFlags flags = MOJO_READ_DATA_FLAG_ALL_OR_NONE;
  EXPECT_EQ(MOJO_RESULT_OK, consumer->ReadData(flags, buffer, num_bytes));

  EXPECT_EQ(
      base::as_string_view(base::as_byte_span(buffer)).substr(0, num_bytes),
      kData.substr(range_start, range_length));
}

TEST_F(BlobReaderTest, ReadableDataHandleMultipleSlices) {
  auto b = std::make_unique<BlobDataBuilder>("uuid");
  const std::string kData1 = "Test Blob Data";
  const std::string kData2 = "Extra test blob data";

  // Create readable data handles with various slices of data.
  // It's important to test that both the first element we read and a
  // non-first element that we read handle slices, as these touch different
  // pieces of code.
  b->AppendReadableDataHandle(
      base::MakeRefCounted<FakeBlobDataHandle>(kData1, ""), 5, 4);
  b->AppendReadableDataHandle(
      base::MakeRefCounted<FakeBlobDataHandle>(kData2, ""), 6, 9);
  this->InitializeReader(std::move(b));

  std::string kData = kData1.substr(5, 4) + kData2.substr(6, 9);

  int size_result = -1;
  EXPECT_FALSE(IsReaderTotalSizeCalculated());
  EXPECT_EQ(BlobReader::Status::DONE, reader_->CalculateSize(base::BindOnce(
                                          &SetValue<int>, &size_result)));
  CheckSizeCalculatedSynchronously(kData.size(), size_result);

  // Verify this condition while we are here.
  EXPECT_FALSE(reader_->IsSingleMojoDataItem());

  auto buffer = base::MakeRefCounted<net::IOBufferWithSize>(kData.size());

  int bytes_read = 0;
  int async_bytes_read = 0;
  BlobReader::Status result =
      reader_->Read(buffer.get(), kData.size(), &bytes_read,
                    base::BindOnce(&SetValue<int>, &async_bytes_read));
  EXPECT_EQ(net::OK, reader_->net_error());
  EXPECT_EQ(result, BlobReader::Status::DONE);
  EXPECT_EQ(0, async_bytes_read);
  EXPECT_EQ(kData.size(), static_cast<size_t>(bytes_read));

  EXPECT_EQ(0, memcmp(buffer->data(), kData.data(), kData.size()));
}

TEST_F(BlobReaderTest, FileRange) {
  auto b = std::make_unique<BlobDataBuilder>("uuid");
  const FilePath kPath = FilePath::FromUTF8Unsafe("/fake/file.txt");
  // We check the offset in the ExpectLocalFileCall mock.
  const std::string kRangeData = "leD";
  const std::string kData = "FileData!!!";
  const uint64_t kOffset = 2;
  const uint64_t kReadLength = 3;
  const base::Time kTime = base::Time::Now();
  b->AppendFile(kPath, 0, kData.size(), kTime);
  this->InitializeReader(std::move(b));

  std::unique_ptr<FakeFileStreamReader> reader(new FakeFileStreamReader(kData));
  reader->SetAsyncRunner(
      base::SingleThreadTaskRunner::GetCurrentDefault().get());
  ExpectLocalFileCall(kPath, kTime, 0, reader.release());

  // We create the reader again with the offset after the seek.
  reader = std::make_unique<FakeFileStreamReader>(kRangeData);
  reader->SetAsyncRunner(
      base::SingleThreadTaskRunner::GetCurrentDefault().get());
  ExpectLocalFileCall(kPath, kTime, kOffset, reader.release());

  int size_result = -1;
  EXPECT_EQ(
      BlobReader::Status::IO_PENDING,
      reader_->CalculateSize(base::BindOnce(&SetValue<int>, &size_result)));
  base::RunLoop().RunUntilIdle();

  scoped_refptr<net::IOBuffer> buffer = CreateBuffer(kReadLength);
  EXPECT_EQ(BlobReader::Status::DONE,
            reader_->SetReadRange(kOffset, kReadLength));

  int bytes_read = 0;
  int async_bytes_read = 0;
  EXPECT_EQ(BlobReader::Status::IO_PENDING,
            reader_->Read(buffer.get(), kReadLength, &bytes_read,
                          base::BindOnce(&SetValue<int>, &async_bytes_read)));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(net::OK, reader_->net_error());
  EXPECT_EQ(kReadLength, static_cast<size_t>(async_bytes_read));
  EXPECT_EQ(0, bytes_read);
  EXPECT_EQ(0, memcmp(buffer->data(), "leD", kReadLength));
}

TEST_F(BlobReaderTest, ReadableDataHandleRange) {
  auto b = std::make_unique<BlobDataBuilder>("uuid");
  const std::string kData = "Test Blob Data";
  const uint64_t kOffset = 2;
  const uint64_t kReadLength = 3;
  auto data_handle = base::MakeRefCounted<FakeBlobDataHandle>(kData, "");
  b->AppendReadableDataHandle(std::move(data_handle));
  this->InitializeReader(std::move(b));

  int size_result = -1;
  EXPECT_EQ(BlobReader::Status::DONE, reader_->CalculateSize(base::BindOnce(
                                          &SetValue<int>, &size_result)));

  scoped_refptr<net::IOBuffer> buffer = CreateBuffer(kReadLength);
  EXPECT_EQ(BlobReader::Status::DONE,
            reader_->SetReadRange(kOffset, kReadLength));

  int bytes_read = 0;
  int async_bytes_read = 0;
  EXPECT_EQ(BlobReader::Status::DONE,
            reader_->Read(buffer.get(), kReadLength, &bytes_read,
                          base::BindOnce(&SetValue<int>, &async_bytes_read)));
  EXPECT_EQ(net::OK, reader_->net_error());
  EXPECT_EQ(kReadLength, static_cast<size_t>(bytes_read));
  EXPECT_EQ(0, async_bytes_read);
  EXPECT_EQ(0, memcmp(buffer->data(), "st ", kReadLength));
}

TEST_F(BlobReaderTest, FileSomeAsyncSegmentedOffsetsUnknownSizes) {
  // This tests includes:
  // * Unknown file sizes (item length of uint64_t::max) for every other item.
  // * Offsets for every 3rd file item.
  // * Non-async reader for every 4th file item.
  auto b = std::make_unique<BlobDataBuilder>("uuid");
  const FilePath kPathBase = FilePath::FromUTF8Unsafe("/fake/file.txt");
  const base::Time kTime = base::Time::Now();
  const size_t kNumItems = 10;
  const size_t kItemSize = 6;
  const size_t kBufferSize = 10;
  const size_t kTotalSize = kNumItems * kItemSize;
  char current_value = 0;
  // Create blob and reader.
  for (size_t i = 0; i < kNumItems; i++) {
    current_value += kItemSize;
    FilePath path = kPathBase.Append(
        FilePath::FromUTF8Unsafe(base::StringPrintf("%d", current_value)));
    uint64_t offset = i % 3 == 0 ? 1 : 0;
    uint64_t size = kItemSize;
    b->AppendFile(path, offset, size, kTime);
  }
  this->InitializeReader(std::move(b));

  // Set expectations.
  current_value = 0;
  for (size_t i = 0; i < kNumItems; i++) {
    uint64_t offset = i % 3 == 0 ? 1 : 0;
    auto buf = base::HeapArray<char>::Uninit(kItemSize + offset);
    if (offset > 0) {
      memset(buf.data(), 7, offset);
    }
    for (size_t j = 0; j < kItemSize; j++) {
      buf.data()[j + offset] = current_value++;
    }
    std::unique_ptr<FakeFileStreamReader> reader(new FakeFileStreamReader(
        std::string(buf.data() + offset, kItemSize), buf.size()));
    if (i % 4 != 0) {
      reader->SetAsyncRunner(
          base::SingleThreadTaskRunner::GetCurrentDefault().get());
    }
    FilePath path = kPathBase.Append(
        FilePath::FromUTF8Unsafe(base::StringPrintf("%d", current_value)));
    ExpectLocalFileCall(path, kTime, offset, reader.release());
  }

  int size_result = -1;
  EXPECT_FALSE(IsReaderTotalSizeCalculated());
  EXPECT_EQ(
      BlobReader::Status::IO_PENDING,
      reader_->CalculateSize(base::BindOnce(&SetValue<int>, &size_result)));
  CheckSizeNotCalculatedYet(size_result);
  base::RunLoop().RunUntilIdle();
  CheckSizeCalculatedAsynchronously(kTotalSize, size_result);

  auto buffer = base::MakeRefCounted<net::IOBufferWithSize>(kBufferSize);

  current_value = 0;
  for (size_t i = 0; i < kTotalSize / kBufferSize; i++) {
    int bytes_read = 0;
    int async_bytes_read = 0;
    EXPECT_EQ(BlobReader::Status::IO_PENDING,
              reader_->Read(buffer.get(), kBufferSize, &bytes_read,
                            base::BindOnce(&SetValue<int>, &async_bytes_read)));
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(net::OK, reader_->net_error());
    EXPECT_EQ(0, bytes_read);
    EXPECT_EQ(kBufferSize, static_cast<size_t>(async_bytes_read));
    for (size_t j = 0; j < kBufferSize; j++) {
      EXPECT_EQ(current_value, buffer->data()[j]);
      current_value++;
    }
  }
}

TEST_F(BlobReaderTest, MixedContent) {
  // Includes data, a file, and a data handle entry.

  auto b = std::make_unique<BlobDataBuilder>("uuid");
  const std::string kData1("Hello ");
  const std::string kData2("there. ");
  const std::string kData3("This ");
  const std::string kData4("is multi-content.");
  const uint64_t kDataSize = 35;

  const base::Time kTime = base::Time::Now();
  const FilePath kData1Path = FilePath::FromUTF8Unsafe("/fake/file.txt");

  auto data_handle = base::MakeRefCounted<FakeBlobDataHandle>(kData3, "");

  b->AppendFile(kData1Path, 0, kData1.size(), kTime);
  b->AppendData(kData2);
  b->AppendReadableDataHandle(std::move(data_handle));
  b->AppendData(kData4);

  this->InitializeReader(std::move(b));

  std::unique_ptr<FakeFileStreamReader> reader(
      new FakeFileStreamReader(kData1));
  reader->SetAsyncRunner(
      base::SingleThreadTaskRunner::GetCurrentDefault().get());
  ExpectLocalFileCall(kData1Path, kTime, 0, reader.release());

  int size_result = -1;
  EXPECT_FALSE(IsReaderTotalSizeCalculated());
  EXPECT_EQ(
      BlobReader::Status::IO_PENDING,
      reader_->CalculateSize(base::BindOnce(&SetValue<int>, &size_result)));
  CheckSizeNotCalculatedYet(size_result);
  base::RunLoop().RunUntilIdle();
  CheckSizeCalculatedAsynchronously(kDataSize, size_result);

  scoped_refptr<net::IOBuffer> buffer = CreateBuffer(kDataSize);

  int bytes_read = 0;
  int async_bytes_read = 0;
  EXPECT_EQ(BlobReader::Status::IO_PENDING,
            reader_->Read(buffer.get(), kDataSize, &bytes_read,
                          base::BindOnce(&SetValue<int>, &async_bytes_read)));
  EXPECT_EQ(0, async_bytes_read);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(net::OK, reader_->net_error());
  EXPECT_EQ(0, bytes_read);
  EXPECT_EQ(kDataSize, static_cast<size_t>(async_bytes_read));
  EXPECT_EQ(0, memcmp(buffer->data(), "Hello there. This is multi-content.",
                      kDataSize));
}

TEST_F(BlobReaderTest, StateErrors) {
  // Test common variables
  int bytes_read = -1;
  int async_bytes_read = -1;
  int size_result = -1;
  const std::string kData("Hello!!!");

  // Case: Blob handle is a nullptr.
  InitializeReader(nullptr);
  EXPECT_EQ(
      BlobReader::Status::NET_ERROR,
      reader_->CalculateSize(base::BindOnce(&SetValue<int>, &size_result)));
  EXPECT_EQ(net::ERR_FILE_NOT_FOUND, reader_->net_error());
  EXPECT_EQ(BlobReader::Status::NET_ERROR, reader_->SetReadRange(0, 10));
  EXPECT_EQ(net::ERR_FILE_NOT_FOUND, reader_->net_error());
  scoped_refptr<net::IOBuffer> buffer = CreateBuffer(10);
  EXPECT_EQ(BlobReader::Status::NET_ERROR,
            reader_->Read(buffer.get(), 10, &bytes_read,
                          base::BindOnce(&SetValue<int>, &async_bytes_read)));
  EXPECT_EQ(net::ERR_FILE_NOT_FOUND, reader_->net_error());

  // Case: Not calling CalculateSize before SetReadRange.
  auto builder1 = std::make_unique<BlobDataBuilder>("uuid1");
  builder1->AppendData(kData);
  InitializeReader(std::move(builder1));
  EXPECT_EQ(BlobReader::Status::NET_ERROR, reader_->SetReadRange(0, 10));
  EXPECT_EQ(net::ERR_UNEXPECTED, reader_->net_error());
  EXPECT_EQ(BlobReader::Status::NET_ERROR,
            reader_->Read(buffer.get(), 10, &bytes_read,
                          base::BindOnce(&SetValue<int>, &async_bytes_read)));

  // Case: Not calling CalculateSize before Read.
  auto builder2 = std::make_unique<BlobDataBuilder>("uuid2");
  builder2->AppendData(kData);
  InitializeReader(std::move(builder2));
  EXPECT_EQ(BlobReader::Status::NET_ERROR,
            reader_->Read(buffer.get(), 10, &bytes_read,
                          base::BindOnce(&SetValue<int>, &async_bytes_read)));
}

TEST_F(BlobReaderTest, FileErrorsSync) {
  int size_result = -1;
  const FilePath kPath = FilePath::FromUTF8Unsafe("/fake/file.txt");
  const std::string kData = "FileData!!!";
  const base::Time kTime = base::Time::Now();

  // Case: Error on length query.
  auto builder1 = std::make_unique<BlobDataBuilder>("uuid1");
  builder1->AppendFile(kPath, 0, kData.size(), kTime);
  this->InitializeReader(std::move(builder1));
  FakeFileStreamReader* reader = new FakeFileStreamReader(kData);
  reader->SetReturnError(net::ERR_FILE_NOT_FOUND);
  ExpectLocalFileCall(kPath, kTime, 0, reader);

  EXPECT_EQ(
      BlobReader::Status::NET_ERROR,
      reader_->CalculateSize(base::BindOnce(&SetValue<int>, &size_result)));
  EXPECT_EQ(net::ERR_FILE_NOT_FOUND, reader_->net_error());

  // Case: Error on read.
  auto builder2 = std::make_unique<BlobDataBuilder>("uuid2");
  builder2->AppendFile(kPath, 0, kData.size(), kTime);
  this->InitializeReader(std::move(builder2));
  reader = new FakeFileStreamReader(kData);
  ExpectLocalFileCall(kPath, kTime, 0, reader);
  EXPECT_EQ(BlobReader::Status::DONE, reader_->CalculateSize(base::BindOnce(
                                          &SetValue<int>, &size_result)));
  reader->SetReturnError(net::ERR_FILE_NOT_FOUND);

  auto buffer = base::MakeRefCounted<net::IOBufferWithSize>(kData.size());
  int bytes_read = 0;
  int async_bytes_read = 0;
  EXPECT_EQ(BlobReader::Status::NET_ERROR,
            reader_->Read(buffer.get(), kData.size(), &bytes_read,
                          base::BindOnce(&SetValue<int>, &async_bytes_read)));
  EXPECT_EQ(net::ERR_FILE_NOT_FOUND, reader_->net_error());
}

TEST_F(BlobReaderTest, FileErrorsAsync) {
  int size_result = -1;
  const FilePath kPath = FilePath::FromUTF8Unsafe("/fake/file.txt");
  const std::string kData = "FileData!!!";
  const base::Time kTime = base::Time::Now();

  // Case: Error on length query.
  auto builder1 = std::make_unique<BlobDataBuilder>("uuid1");
  builder1->AppendFile(kPath, 0, kData.size(), kTime);
  this->InitializeReader(std::move(builder1));
  FakeFileStreamReader* reader = new FakeFileStreamReader(kData);
  reader->SetAsyncRunner(
      base::SingleThreadTaskRunner::GetCurrentDefault().get());
  reader->SetReturnError(net::ERR_FILE_NOT_FOUND);
  ExpectLocalFileCall(kPath, kTime, 0, reader);

  EXPECT_EQ(
      BlobReader::Status::IO_PENDING,
      reader_->CalculateSize(base::BindOnce(&SetValue<int>, &size_result)));
  EXPECT_EQ(net::OK, reader_->net_error());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(net::ERR_FILE_NOT_FOUND, size_result);
  EXPECT_EQ(net::ERR_FILE_NOT_FOUND, reader_->net_error());

  // Case: Error on read.
  auto builder2 = std::make_unique<BlobDataBuilder>("uuid2");
  builder2->AppendFile(kPath, 0, kData.size(), kTime);
  this->InitializeReader(std::move(builder2));
  reader = new FakeFileStreamReader(kData);
  ExpectLocalFileCall(kPath, kTime, 0, reader);
  EXPECT_EQ(BlobReader::Status::DONE, reader_->CalculateSize(base::BindOnce(
                                          &SetValue<int>, &size_result)));
  reader->SetReturnError(net::ERR_FILE_NOT_FOUND);
  reader->SetAsyncRunner(
      base::SingleThreadTaskRunner::GetCurrentDefault().get());

  auto buffer = base::MakeRefCounted<net::IOBufferWithSize>(kData.size());
  int bytes_read = 0;
  int async_bytes_read = 0;
  EXPECT_EQ(BlobReader::Status::IO_PENDING,
            reader_->Read(buffer.get(), kData.size(), &bytes_read,
                          base::BindOnce(&SetValue<int>, &async_bytes_read)));
  EXPECT_EQ(net::OK, reader_->net_error());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(net::ERR_FILE_NOT_FOUND, async_bytes_read);
  EXPECT_EQ(net::ERR_FILE_NOT_FOUND, reader_->net_error());
}

TEST_F(BlobReaderTest, RangeError) {
  const std::string kData("Hello!!!");
  const size_t kDataSize = 8ul;
  const uint64_t kReadLength = 4ull;

  // Case: offset too high.
  auto b = std::make_unique<BlobDataBuilder>("uuid1");
  b->AppendData(kData);
  this->InitializeReader(std::move(b));
  int size_result = -1;
  EXPECT_EQ(BlobReader::Status::DONE, reader_->CalculateSize(base::BindOnce(
                                          &SetValue<int>, &size_result)));
  scoped_refptr<net::IOBuffer> buffer = CreateBuffer(kDataSize);
  EXPECT_EQ(BlobReader::Status::NET_ERROR,
            reader_->SetReadRange(kDataSize + 1, kReadLength));
  EXPECT_EQ(net::ERR_REQUEST_RANGE_NOT_SATISFIABLE, reader_->net_error());

  // Case: length too long.
  auto b2 = std::make_unique<BlobDataBuilder>("uuid2");
  b2->AppendData(kData);
  this->InitializeReader(std::move(b2));
  size_result = -1;
  EXPECT_EQ(BlobReader::Status::DONE, reader_->CalculateSize(base::BindOnce(
                                          &SetValue<int>, &size_result)));
  buffer = CreateBuffer(kDataSize + 1);
  EXPECT_EQ(BlobReader::Status::NET_ERROR,
            reader_->SetReadRange(0, kDataSize + 1));
  EXPECT_EQ(net::ERR_REQUEST_RANGE_NOT_SATISFIABLE, reader_->net_error());
}

TEST_F(BlobReaderTest, HandleBeforeAsyncCancel) {
  const std::string kUuid("uuid1");
  const std::string kData("Hello!!!");
  const size_t kDataSize = 8ul;
  std::vector<FileCreationInfo> files;

  auto b = std::make_unique<BlobDataBuilder>(kUuid);
  b->AppendFutureData(kDataSize);
  BlobStatus can_populate_status =
      BlobStatus::ERR_INVALID_CONSTRUCTION_ARGUMENTS;
  blob_handle_ = context_.BuildBlob(
      std::move(b),
      base::BindOnce(&SaveBlobStatusAndFiles, &can_populate_status, &files));
  EXPECT_EQ(BlobStatus::PENDING_TRANSPORT, can_populate_status);
  provider_ = new MockFileStreamReaderProvider();
  reader_.reset(new BlobReader(blob_handle_.get()));
  reader_->SetFileStreamProviderForTesting(base::WrapUnique(provider_.get()));
  int size_result = -1;
  EXPECT_EQ(
      BlobReader::Status::IO_PENDING,
      reader_->CalculateSize(base::BindOnce(&SetValue<int>, &size_result)));
  EXPECT_FALSE(reader_->IsInMemory());
  context_.CancelBuildingBlob(kUuid,
                              BlobStatus::ERR_INVALID_CONSTRUCTION_ARGUMENTS);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(net::ERR_FAILED, size_result);
}

TEST_F(BlobReaderTest, ReadFromIncompleteBlob) {
  const std::string kUuid("uuid1");
  const std::string kData("Hello!!!");
  const size_t kDataSize = 8ul;
  std::vector<FileCreationInfo> files;

  auto b = std::make_unique<BlobDataBuilder>(kUuid);
  BlobDataBuilder::FutureData future_data = b->AppendFutureData(kDataSize);
  BlobStatus can_populate_status =
      BlobStatus::ERR_INVALID_CONSTRUCTION_ARGUMENTS;
  blob_handle_ = context_.BuildBlob(
      std::move(b),
      base::BindOnce(&SaveBlobStatusAndFiles, &can_populate_status, &files));
  EXPECT_EQ(BlobStatus::PENDING_TRANSPORT, can_populate_status);
  provider_ = new MockFileStreamReaderProvider();
  reader_.reset(new BlobReader(blob_handle_.get()));
  reader_->SetFileStreamProviderForTesting(base::WrapUnique(provider_.get()));
  int size_result = -1;
  EXPECT_EQ(
      BlobReader::Status::IO_PENDING,
      reader_->CalculateSize(base::BindOnce(&SetValue<int>, &size_result)));
  EXPECT_FALSE(reader_->IsInMemory());
  future_data.Populate(base::as_bytes(base::make_span(kData.data(), kDataSize)),
                       0);
  context_.NotifyTransportComplete(kUuid);
  base::RunLoop().RunUntilIdle();
  CheckSizeCalculatedAsynchronously(kDataSize, size_result);
  auto buffer = base::MakeRefCounted<net::IOBufferWithSize>(kDataSize);

  int bytes_read = 0;
  int async_bytes_read = 0;
  EXPECT_EQ(BlobReader::Status::DONE,
            reader_->Read(buffer.get(), kDataSize, &bytes_read,
                          base::BindOnce(&SetValue<int>, &async_bytes_read)));
  EXPECT_EQ(net::OK, reader_->net_error());
  EXPECT_EQ(kDataSize, static_cast<size_t>(bytes_read));
  EXPECT_EQ(0, async_bytes_read);
  EXPECT_EQ(kData, std::string(buffer->data(), kDataSize));
  EXPECT_EQ(net::OK, size_result);
}

}  // namespace storage
