// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/fileapi/file_writer_impl.h"

#include <limits>
#include "base/files/scoped_temp_dir.h"
#include "base/guid.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_task_environment.h"
#include "mojo/public/cpp/system/string_data_pipe_producer.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "storage/browser/blob/blob_data_builder.h"
#include "storage/browser/blob/blob_impl.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "storage/browser/fileapi/file_stream_reader.h"
#include "storage/browser/test/async_file_test_helper.h"
#include "storage/browser/test/test_file_system_context.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::AsyncFileTestHelper;
using content::CreateFileSystemContextForTesting;

namespace storage {

class FileWriterImplTest : public testing::Test {
 public:
  FileWriterImplTest()
      : scoped_task_environment_(
            base::test::ScopedTaskEnvironment::MainThreadType::IO) {}

  void SetUp() override {
    ASSERT_TRUE(dir_.CreateUniqueTempDir());
    file_system_context_ = CreateFileSystemContextForTesting(
        /*quota_manager_proxy=*/nullptr, dir_.GetPath());

    test_url_ = file_system_context_->CreateCrackedFileSystemURL(
        GURL("http://example.com"), kFileSystemTypeTest,
        base::FilePath::FromUTF8Unsafe("test"));

    ASSERT_EQ(base::File::FILE_OK, AsyncFileTestHelper::CreateFile(
                                       file_system_context_.get(), test_url_));

    blob_context_ = std::make_unique<BlobStorageContext>();

    writer_ = std::make_unique<FileWriterImpl>(
        test_url_, file_system_context_->CreateFileSystemOperationRunner(),
        blob_context_->AsWeakPtr());
  }

  blink::mojom::BlobPtr CreateBlob(const std::string& contents) {
    auto builder =
        std::make_unique<storage::BlobDataBuilder>(base::GenerateGUID());
    builder->AppendData(contents);
    auto handle = blob_context_->AddFinishedBlob(std::move(builder));
    blink::mojom::BlobPtr result;
    BlobImpl::Create(std::move(handle), MakeRequest(&result));
    return result;
  }

  mojo::ScopedDataPipeConsumerHandle CreateStream(const std::string& contents) {
    // Test with a relatively low capacity pipe to make sure it isn't all
    // written/read in one go.
    mojo::DataPipe pipe(16);
    CHECK(pipe.producer_handle.is_valid());
    auto producer = std::make_unique<mojo::StringDataPipeProducer>(
        std::move(pipe.producer_handle));
    auto* producer_raw = producer.get();
    producer_raw->Write(
        contents,
        mojo::StringDataPipeProducer::AsyncWritingMode::
            STRING_MAY_BE_INVALIDATED_BEFORE_COMPLETION,
        base::BindOnce(
            base::DoNothing::Once<std::unique_ptr<mojo::StringDataPipeProducer>,
                                  MojoResult>(),
            std::move(producer)));
    return std::move(pipe.consumer_handle);
  }

  std::string ReadFile(const FileSystemURL& url) {
    std::unique_ptr<FileStreamReader> reader =
        file_system_context_->CreateFileStreamReader(
            url, 0, std::numeric_limits<int64_t>::max(), base::Time());
    net::TestCompletionCallback callback;
    std::string result;
    while (true) {
      auto buf = base::MakeRefCounted<net::IOBufferWithSize>(4096);
      int rv = reader->Read(buf.get(), buf->size(), callback.callback());
      if (rv == net::ERR_IO_PENDING)
        rv = callback.WaitForResult();
      EXPECT_GE(rv, 0);
      if (rv < 0)
        return "(read failure)";
      if (rv == 0)
        return result;
      result.append(buf->data(), rv);
    }
  }

  base::File::Error WriteBlobSync(uint64_t position,
                                  blink::mojom::BlobPtr blob,
                                  uint64_t* bytes_written_out) {
    base::RunLoop loop;
    base::File::Error result_out;
    writer_->Write(position, std::move(blob),
                   base::BindLambdaForTesting(
                       [&](base::File::Error result, uint64_t bytes_written) {
                         result_out = result;
                         *bytes_written_out = bytes_written;
                         loop.Quit();
                       }));
    loop.Run();
    return result_out;
  }

  base::File::Error WriteStreamSync(
      uint64_t position,
      mojo::ScopedDataPipeConsumerHandle data_pipe,
      uint64_t* bytes_written_out) {
    base::RunLoop loop;
    base::File::Error result_out;
    writer_->WriteStream(
        position, std::move(data_pipe),
        base::BindLambdaForTesting(
            [&](base::File::Error result, uint64_t bytes_written) {
              result_out = result;
              *bytes_written_out = bytes_written;
              loop.Quit();
            }));
    loop.Run();
    return result_out;
  }

  base::File::Error TruncateSync(uint64_t length) {
    base::RunLoop loop;
    base::File::Error result_out;
    writer_->Truncate(length,
                      base::BindLambdaForTesting([&](base::File::Error result) {
                        result_out = result;
                        loop.Quit();
                      }));
    loop.Run();
    return result_out;
  }

  virtual bool WriteUsingBlobs() { return true; }

  base::File::Error WriteSync(uint64_t position,
                              const std::string& contents,
                              uint64_t* bytes_written_out) {
    if (WriteUsingBlobs())
      return WriteBlobSync(position, CreateBlob(contents), bytes_written_out);
    return WriteStreamSync(position, CreateStream(contents), bytes_written_out);
  }

 protected:
  base::test::ScopedTaskEnvironment scoped_task_environment_;

  base::ScopedTempDir dir_;
  scoped_refptr<FileSystemContext> file_system_context_;
  std::unique_ptr<BlobStorageContext> blob_context_;

  FileSystemURL test_url_;

  std::unique_ptr<FileWriterImpl> writer_;
};

class FileWriterImplWriteTest : public FileWriterImplTest,
                                public testing::WithParamInterface<bool> {
 public:
  bool WriteUsingBlobs() override { return GetParam(); }
};

INSTANTIATE_TEST_CASE_P(FileWriterImplTest,
                        FileWriterImplWriteTest,
                        ::testing::Bool());

TEST_F(FileWriterImplTest, WriteInvalidBlob) {
  blink::mojom::BlobPtr blob;
  MakeRequest(&blob);

  uint64_t bytes_written;
  base::File::Error result = WriteBlobSync(0, std::move(blob), &bytes_written);
  EXPECT_EQ(result, base::File::FILE_ERROR_FAILED);
  EXPECT_EQ(bytes_written, 0u);

  EXPECT_EQ("", ReadFile(test_url_));
}

TEST_P(FileWriterImplWriteTest, WriteValidEmptyString) {
  uint64_t bytes_written;
  base::File::Error result = WriteSync(0, "", &bytes_written);
  EXPECT_EQ(result, base::File::FILE_OK);
  EXPECT_EQ(bytes_written, 0u);

  EXPECT_EQ("", ReadFile(test_url_));
}

TEST_P(FileWriterImplWriteTest, WriteValidNonEmpty) {
  std::string test_data("abcdefghijklmnopqrstuvwxyz");
  uint64_t bytes_written;
  base::File::Error result = WriteSync(0, test_data, &bytes_written);
  EXPECT_EQ(result, base::File::FILE_OK);
  EXPECT_EQ(bytes_written, test_data.size());

  EXPECT_EQ(test_data, ReadFile(test_url_));
}

TEST_P(FileWriterImplWriteTest, WriteWithOffsetInFile) {
  uint64_t bytes_written;
  base::File::Error result;

  result = WriteSync(0, "1234567890", &bytes_written);
  EXPECT_EQ(result, base::File::FILE_OK);
  EXPECT_EQ(bytes_written, 10u);

  result = WriteSync(4, "abc", &bytes_written);
  EXPECT_EQ(result, base::File::FILE_OK);
  EXPECT_EQ(bytes_written, 3u);

  EXPECT_EQ("1234abc890", ReadFile(test_url_));
}

TEST_P(FileWriterImplWriteTest, WriteWithOffsetPastFile) {
  uint64_t bytes_written;
  base::File::Error result = WriteSync(4, "abc", &bytes_written);
  EXPECT_EQ(result, base::File::FILE_ERROR_FAILED);
  EXPECT_EQ(bytes_written, 0u);

  EXPECT_EQ("", ReadFile(test_url_));
}

TEST_F(FileWriterImplTest, TruncateShrink) {
  uint64_t bytes_written;
  base::File::Error result;

  result = WriteSync(0, "1234567890", &bytes_written);
  EXPECT_EQ(result, base::File::FILE_OK);
  EXPECT_EQ(bytes_written, 10u);

  result = TruncateSync(5);
  EXPECT_EQ(result, base::File::FILE_OK);

  EXPECT_EQ("12345", ReadFile(test_url_));
}

TEST_F(FileWriterImplTest, TruncateGrow) {
  uint64_t bytes_written;
  base::File::Error result;

  result = WriteSync(0, "abc", &bytes_written);
  EXPECT_EQ(result, base::File::FILE_OK);
  EXPECT_EQ(bytes_written, 3u);

  result = TruncateSync(5);
  EXPECT_EQ(result, base::File::FILE_OK);

  EXPECT_EQ(std::string("abc\0\0", 5), ReadFile(test_url_));
}

// TODO(mek): More tests, particularly for error conditions.

}  // namespace storage
