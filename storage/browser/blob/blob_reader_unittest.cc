// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/task_runner.h"
#include "base/test/scoped_task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "net/base/completion_once_callback.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/disk_cache/disk_cache.h"
#include "storage/browser/blob/blob_data_builder.h"
#include "storage/browser/blob/blob_data_handle.h"
#include "storage/browser/blob/blob_reader.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "storage/browser/fileapi/file_stream_reader.h"
#include "storage/browser/fileapi/file_system_context.h"
#include "storage/browser/fileapi/file_system_file_util.h"
#include "storage/browser/test/async_file_test_helper.h"
#include "storage/browser/test/test_file_system_context.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using base::FilePath;
using content::AsyncFileTestHelper;
using net::DrainableIOBuffer;
using net::IOBuffer;
using FileCreationInfo = storage::BlobMemoryController::FileCreationInfo;

namespace storage {
namespace {

const int kTestDiskCacheStreamIndex = 0;
const int kTestDiskCacheSideStreamIndex = 1;

void SaveBlobStatusAndFiles(BlobStatus* status_ptr,
                            std::vector<FileCreationInfo>* files_ptr,
                            BlobStatus status,
                            std::vector<FileCreationInfo> files) {
  *status_ptr = status;
  for (FileCreationInfo& info : files) {
    files_ptr->push_back(std::move(info));
  }
}

// Our disk cache tests don't need a real data handle since the tests themselves
// scope the disk cache and entries.
class EmptyDataHandle : public storage::BlobDataBuilder::DataHandle {
 private:
  ~EmptyDataHandle() override = default;
};

// A disk_cache::Entry that arbitrarily delays the completion of a read
// operation to allow testing some races without flake. This is particularly
// relevant in this unit test, which uses the always-synchronous MEMORY_CACHE.
class DelayedReadEntry : public disk_cache::Entry {
 public:
  explicit DelayedReadEntry(disk_cache::ScopedEntryPtr entry)
      : entry_(std::move(entry)) {}
  ~DelayedReadEntry() override { EXPECT_FALSE(HasPendingReadCallbacks()); }

  bool HasPendingReadCallbacks() { return !pending_read_callbacks_.empty(); }

  void RunPendingReadCallbacks() {
    std::vector<base::OnceCallback<void(void)>> callbacks;
    pending_read_callbacks_.swap(callbacks);
    for (auto& callback : callbacks)
      std::move(callback).Run();
  }

  // From disk_cache::Entry:
  void Doom() override { entry_->Doom(); }

  void Close() override { delete this; }  // Note this is required by the API.

  std::string GetKey() const override { return entry_->GetKey(); }

  base::Time GetLastUsed() const override { return entry_->GetLastUsed(); }

  base::Time GetLastModified() const override {
    return entry_->GetLastModified();
  }

  int32_t GetDataSize(int index) const override {
    return entry_->GetDataSize(index);
  }

  int ReadData(int index,
               int offset,
               IOBuffer* buf,
               int buf_len,
               CompletionOnceCallback original_callback) override {
    net::TestCompletionCallback callback;
    int rv = entry_->ReadData(index, offset, buf, buf_len, callback.callback());
    DCHECK_NE(rv, net::ERR_IO_PENDING)
        << "Test expects to use a MEMORY_CACHE instance, which is synchronous.";
    pending_read_callbacks_.push_back(
        base::BindOnce(std::move(original_callback), rv));
    return net::ERR_IO_PENDING;
  }

  int WriteData(int index,
                int offset,
                IOBuffer* buf,
                int buf_len,
                CompletionOnceCallback callback,
                bool truncate) override {
    return entry_->WriteData(index, offset, buf, buf_len, std::move(callback),
                             truncate);
  }

  int ReadSparseData(int64_t offset,
                     IOBuffer* buf,
                     int buf_len,
                     CompletionOnceCallback callback) override {
    return entry_->ReadSparseData(offset, buf, buf_len, std::move(callback));
  }

  int WriteSparseData(int64_t offset,
                      IOBuffer* buf,
                      int buf_len,
                      CompletionOnceCallback callback) override {
    return entry_->WriteSparseData(offset, buf, buf_len, std::move(callback));
  }

  int GetAvailableRange(int64_t offset,
                        int len,
                        int64_t* start,
                        CompletionOnceCallback callback) override {
    return entry_->GetAvailableRange(offset, len, start, std::move(callback));
  }

  bool CouldBeSparse() const override { return entry_->CouldBeSparse(); }

  void CancelSparseIO() override { entry_->CancelSparseIO(); }

  net::Error ReadyForSparseIO(CompletionOnceCallback callback) override {
    return entry_->ReadyForSparseIO(std::move(callback));
  }
  void SetLastUsedTimeForTest(base::Time time) override { NOTREACHED(); }

 private:
  disk_cache::ScopedEntryPtr entry_;
  std::vector<base::OnceCallback<void(void)>> pending_read_callbacks_;
};

std::unique_ptr<disk_cache::Backend> CreateInMemoryDiskCache() {
  std::unique_ptr<disk_cache::Backend> cache;
  net::TestCompletionCallback callback;
  int rv = disk_cache::CreateCacheBackend(
      net::MEMORY_CACHE, net::CACHE_BACKEND_DEFAULT, FilePath(), 0, false,
      nullptr, &cache, callback.callback());
  EXPECT_EQ(net::OK, callback.GetResult(rv));

  return cache;
}

disk_cache::ScopedEntryPtr CreateDiskCacheEntry(disk_cache::Backend* cache,
                                                const char* key,
                                                const std::string& data) {
  disk_cache::Entry* temp_entry = nullptr;
  net::TestCompletionCallback callback;
  int rv =
      cache->CreateEntry(key, net::HIGHEST, &temp_entry, callback.callback());
  if (callback.GetResult(rv) != net::OK)
    return nullptr;
  disk_cache::ScopedEntryPtr entry(temp_entry);

  scoped_refptr<net::StringIOBuffer> iobuffer =
      base::MakeRefCounted<net::StringIOBuffer>(data);
  rv = entry->WriteData(kTestDiskCacheStreamIndex, 0, iobuffer.get(),
                        iobuffer->size(), callback.callback(), false);
  EXPECT_EQ(static_cast<int>(data.size()), callback.GetResult(rv));
  return entry;
}

disk_cache::ScopedEntryPtr CreateDiskCacheEntryWithSideData(
    disk_cache::Backend* cache,
    const char* key,
    const std::string& data,
    const std::string& side_data) {
  disk_cache::ScopedEntryPtr entry = CreateDiskCacheEntry(cache, key, data);
  scoped_refptr<net::StringIOBuffer> iobuffer =
      base::MakeRefCounted<net::StringIOBuffer>(side_data);
  net::TestCompletionCallback callback;
  int rv = entry->WriteData(kTestDiskCacheSideStreamIndex, 0, iobuffer.get(),
                            iobuffer->size(), callback.callback(), false);
  EXPECT_EQ(static_cast<int>(side_data.size()), callback.GetResult(rv));
  return entry;
}

template <typename T>
void SetValue(T* address, T value) {
  *address = value;
}

class FakeFileStreamReader : public FileStreamReader {
 public:
  explicit FakeFileStreamReader(const std::string& contents)
      : buffer_(base::MakeRefCounted<DrainableIOBuffer>(
            base::MakeRefCounted<net::StringIOBuffer>(
                std::unique_ptr<std::string>(new std::string(contents))),
            contents.size())),
        net_error_(net::OK),
        size_(contents.size()) {}
  FakeFileStreamReader(const std::string& contents, uint64_t size)
      : buffer_(base::MakeRefCounted<DrainableIOBuffer>(
            base::MakeRefCounted<net::StringIOBuffer>(
                std::unique_ptr<std::string>(new std::string(contents))),
            contents.size())),
        net_error_(net::OK),
        size_(size) {}

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

  DISALLOW_COPY_AND_ASSIGN(FakeFileStreamReader);
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
  ~BlobReaderTest() override = default;

  void TearDown() override {
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
    reader_->SetFileStreamProviderForTesting(base::WrapUnique(provider_));
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
    return base::MakeRefCounted<net::IOBuffer>(static_cast<size_t>(size));
  }

  bool IsReaderTotalSizeCalculated() {
    return reader_->total_size_calculated();
  }

  base::test::ScopedTaskEnvironment scoped_task_environment_;

  BlobStorageContext context_;
  std::unique_ptr<BlobDataHandle> blob_handle_;
  MockFileStreamReaderProvider* provider_ = nullptr;
  std::unique_ptr<BlobReader> reader_;

 private:
  DISALLOW_COPY_AND_ASSIGN(BlobReaderTest);
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

  scoped_refptr<net::IOBuffer> buffer =
      base::MakeRefCounted<net::IOBuffer>(kDataSize);

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

  scoped_refptr<net::IOBuffer> buffer =
      base::MakeRefCounted<net::IOBuffer>(kData.size());

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
  const GURL kURL("file://test_file/here.txt");
  const std::string kData = "FileData!!!";
  const base::Time kTime = base::Time::Now();
  b->AppendFileSystemFile(kURL, 0, kData.size(), kTime, nullptr);
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

  scoped_refptr<net::IOBuffer> buffer =
      base::MakeRefCounted<net::IOBuffer>(kData.size());

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

TEST_F(BlobReaderTest, BasicDiskCache) {
  std::unique_ptr<disk_cache::Backend> cache = CreateInMemoryDiskCache();
  ASSERT_TRUE(cache);

  auto b = std::make_unique<BlobDataBuilder>("uuid");
  const std::string kData = "Test Blob Data";
  scoped_refptr<BlobDataBuilder::DataHandle> data_handle =
      new EmptyDataHandle();
  disk_cache::ScopedEntryPtr entry =
      CreateDiskCacheEntry(cache.get(), "test entry", kData);
  b->AppendDiskCacheEntry(data_handle, entry.get(), kTestDiskCacheStreamIndex);
  this->InitializeReader(std::move(b));

  int size_result = -1;
  EXPECT_FALSE(IsReaderTotalSizeCalculated());
  EXPECT_EQ(BlobReader::Status::DONE, reader_->CalculateSize(base::BindOnce(
                                          &SetValue<int>, &size_result)));
  CheckSizeCalculatedSynchronously(kData.size(), size_result);

  EXPECT_FALSE(reader_->has_side_data());

  scoped_refptr<net::IOBuffer> buffer =
      base::MakeRefCounted<net::IOBuffer>(kData.size());

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

TEST_F(BlobReaderTest, DiskCacheWithSideData) {
  std::unique_ptr<disk_cache::Backend> cache = CreateInMemoryDiskCache();
  ASSERT_TRUE(cache);

  auto b = std::make_unique<BlobDataBuilder>("uuid");
  const std::string kData = "Test Blob Data";
  const std::string kSideData = "Test side data";
  scoped_refptr<BlobDataBuilder::DataHandle> data_handle =
      new EmptyDataHandle();
  disk_cache::ScopedEntryPtr entry = CreateDiskCacheEntryWithSideData(
      cache.get(), "test entry", kData, kSideData);
  b->AppendDiskCacheEntryWithSideData(data_handle, entry.get(),
                                      kTestDiskCacheStreamIndex,
                                      kTestDiskCacheSideStreamIndex);
  this->InitializeReader(std::move(b));

  int size_result = -1;
  EXPECT_FALSE(IsReaderTotalSizeCalculated());
  EXPECT_EQ(BlobReader::Status::DONE, reader_->CalculateSize(base::BindOnce(
                                          &SetValue<int>, &size_result)));
  CheckSizeCalculatedSynchronously(kData.size(), size_result);

  EXPECT_TRUE(reader_->has_side_data());
  BlobReader::Status status = BlobReader::Status::DONE;
  EXPECT_EQ(BlobReader::Status::DONE,
            reader_->ReadSideData(
                base::BindOnce(&SetValue<BlobReader::Status>, &status)));
  EXPECT_EQ(net::OK, reader_->net_error());
  EXPECT_TRUE(reader_->side_data());
  std::string result(reader_->side_data()->data(),
                     reader_->side_data()->size());
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

  scoped_refptr<net::IOBuffer> buffer =
      base::MakeRefCounted<net::IOBuffer>(kBufferSize);

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

  scoped_refptr<net::IOBuffer> buffer =
      base::MakeRefCounted<net::IOBuffer>(kBufferSize);

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
    b->AppendData(buf, kItemSize);
  }
  this->InitializeReader(std::move(b));

  int size_result = -1;
  EXPECT_FALSE(IsReaderTotalSizeCalculated());
  EXPECT_EQ(BlobReader::Status::DONE, reader_->CalculateSize(base::BindOnce(
                                          &SetValue<int>, &size_result)));
  CheckSizeCalculatedSynchronously(kTotalSize, size_result);

  scoped_refptr<net::IOBuffer> buffer =
      base::MakeRefCounted<net::IOBuffer>(kBufferSize);

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
  reader->SetAsyncRunner(base::ThreadTaskRunnerHandle::Get().get());

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

  scoped_refptr<net::IOBuffer> buffer =
      base::MakeRefCounted<net::IOBuffer>(kData.size());

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
  const GURL kURL("file://test_file/here.txt");
  const std::string kData = "FileData!!!";
  const base::Time kTime = base::Time::Now();
  b->AppendFileSystemFile(kURL, 0, kData.size(), kTime, nullptr);
  this->InitializeReader(std::move(b));

  std::unique_ptr<FakeFileStreamReader> reader(new FakeFileStreamReader(kData));
  reader->SetAsyncRunner(base::ThreadTaskRunnerHandle::Get().get());

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

  scoped_refptr<net::IOBuffer> buffer =
      base::MakeRefCounted<net::IOBuffer>(kData.size());

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

TEST_F(BlobReaderTest, DiskCacheAsync) {
  std::unique_ptr<disk_cache::Backend> cache = CreateInMemoryDiskCache();
  ASSERT_TRUE(cache);

  auto b = std::make_unique<BlobDataBuilder>("uuid");
  const std::string kData = "Test Blob Data";
  scoped_refptr<BlobDataBuilder::DataHandle> data_handle =
      new EmptyDataHandle();
  std::unique_ptr<DelayedReadEntry> delayed_read_entry(new DelayedReadEntry(
      CreateDiskCacheEntry(cache.get(), "test entry", kData)));
  b->AppendDiskCacheEntry(data_handle, delayed_read_entry.get(),
                          kTestDiskCacheStreamIndex);
  this->InitializeReader(std::move(b));

  int size_result = -1;
  EXPECT_FALSE(IsReaderTotalSizeCalculated());
  EXPECT_EQ(BlobReader::Status::DONE, reader_->CalculateSize(base::BindOnce(
                                          &SetValue<int>, &size_result)));
  CheckSizeCalculatedSynchronously(kData.size(), size_result);

  scoped_refptr<net::IOBuffer> buffer =
      base::MakeRefCounted<net::IOBuffer>(kData.size());

  int bytes_read = 0;
  int async_bytes_read = 0;
  EXPECT_EQ(BlobReader::Status::IO_PENDING,
            reader_->Read(buffer.get(), kData.size(), &bytes_read,
                          base::BindOnce(&SetValue<int>, &async_bytes_read)));
  EXPECT_TRUE(delayed_read_entry->HasPendingReadCallbacks());
  delayed_read_entry->RunPendingReadCallbacks();
  EXPECT_EQ(net::OK, reader_->net_error());
  EXPECT_EQ(0, bytes_read);
  EXPECT_EQ(kData.size(), static_cast<size_t>(async_bytes_read));
  EXPECT_EQ(0, memcmp(buffer->data(), "Test Blob Data", kData.size()));
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
  reader->SetAsyncRunner(base::ThreadTaskRunnerHandle::Get().get());
  ExpectLocalFileCall(kPath, kTime, 0, reader.release());

  // We create the reader again with the offset after the seek.
  reader.reset(new FakeFileStreamReader(kRangeData));
  reader->SetAsyncRunner(base::ThreadTaskRunnerHandle::Get().get());
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

TEST_F(BlobReaderTest, DiskCacheRange) {
  std::unique_ptr<disk_cache::Backend> cache = CreateInMemoryDiskCache();
  ASSERT_TRUE(cache);

  auto b = std::make_unique<BlobDataBuilder>("uuid");
  const std::string kData = "Test Blob Data";
  const uint64_t kOffset = 2;
  const uint64_t kReadLength = 3;
  scoped_refptr<BlobDataBuilder::DataHandle> data_handle =
      new EmptyDataHandle();
  disk_cache::ScopedEntryPtr entry =
      CreateDiskCacheEntry(cache.get(), "test entry", kData);
  b->AppendDiskCacheEntry(data_handle, entry.get(), kTestDiskCacheStreamIndex);
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
    std::unique_ptr<char[]> buf(new char[kItemSize + offset]);
    if (offset > 0) {
      memset(buf.get(), 7, offset);
    }
    for (size_t j = 0; j < kItemSize; j++) {
      buf.get()[j + offset] = current_value++;
    }
    std::unique_ptr<FakeFileStreamReader> reader(new FakeFileStreamReader(
        std::string(buf.get() + offset, kItemSize), kItemSize + offset));
    if (i % 4 != 0) {
      reader->SetAsyncRunner(base::ThreadTaskRunnerHandle::Get().get());
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

  scoped_refptr<net::IOBuffer> buffer =
      base::MakeRefCounted<net::IOBuffer>(kBufferSize);

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
  // Includes data, a file, and a disk cache entry.
  std::unique_ptr<disk_cache::Backend> cache = CreateInMemoryDiskCache();
  ASSERT_TRUE(cache);

  auto b = std::make_unique<BlobDataBuilder>("uuid");
  const std::string kData1("Hello ");
  const std::string kData2("there. ");
  const std::string kData3("This ");
  const std::string kData4("is multi-content.");
  const uint64_t kDataSize = 35;

  const base::Time kTime = base::Time::Now();
  const FilePath kData1Path = FilePath::FromUTF8Unsafe("/fake/file.txt");

  disk_cache::ScopedEntryPtr entry3 =
      CreateDiskCacheEntry(cache.get(), "test entry", kData3);

  b->AppendFile(kData1Path, 0, kData1.size(), kTime);
  b->AppendData(kData2);
  b->AppendDiskCacheEntry(
      scoped_refptr<BlobDataBuilder::DataHandle>(new EmptyDataHandle()),
      entry3.get(), kTestDiskCacheStreamIndex);
  b->AppendData(kData4);

  this->InitializeReader(std::move(b));

  std::unique_ptr<FakeFileStreamReader> reader(
      new FakeFileStreamReader(kData1));
  reader->SetAsyncRunner(base::ThreadTaskRunnerHandle::Get().get());
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

  scoped_refptr<net::IOBuffer> buffer =
      base::MakeRefCounted<net::IOBuffer>(kData.size());
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
  reader->SetAsyncRunner(base::ThreadTaskRunnerHandle::Get().get());
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
  reader->SetAsyncRunner(base::ThreadTaskRunnerHandle::Get().get());

  scoped_refptr<net::IOBuffer> buffer =
      base::MakeRefCounted<net::IOBuffer>(kData.size());
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
  reader_->SetFileStreamProviderForTesting(base::WrapUnique(provider_));
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
  reader_->SetFileStreamProviderForTesting(base::WrapUnique(provider_));
  int size_result = -1;
  EXPECT_EQ(
      BlobReader::Status::IO_PENDING,
      reader_->CalculateSize(base::BindOnce(&SetValue<int>, &size_result)));
  EXPECT_FALSE(reader_->IsInMemory());
  future_data.Populate(base::make_span(kData.data(), kDataSize), 0);
  context_.NotifyTransportComplete(kUuid);
  base::RunLoop().RunUntilIdle();
  CheckSizeCalculatedAsynchronously(kDataSize, size_result);
  scoped_refptr<net::IOBuffer> buffer =
      base::MakeRefCounted<net::IOBuffer>(kDataSize);

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
