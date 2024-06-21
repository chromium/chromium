// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/chunked_data_pipe_upload_data_stream.h"

#include <stdint.h>

#include <limits>
#include <memory>

#include "base/containers/span.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/data_pipe_utils.h"
#include "net/base/completion_once_callback.h"
#include "net/base/io_buffer.h"
#include "net/base/test_completion_callback.h"
#include "net/log/net_log_with_source.h"
#include "services/network/public/cpp/resource_request_body.h"
#include "services/network/public/mojom/chunked_data_pipe_getter.mojom.h"
#include "services/network/test_chunked_data_pipe_getter.h"
#include "testing/gtest/include/gtest/gtest.h"

// Most tests of this class are at the URLLoader layer. These tests focus on
// things too difficult to cover with integration tests.

namespace network {

namespace {

net::CompletionOnceCallback NoCallback() {
  return base::BindOnce([](int result) {
    NOTREACHED_IN_MIGRATION()
        << "This callback should not be called. result=" << result;
  });
}

class ChunkedDataPipeUploadDataStreamTest : public testing::Test {
 public:
  ChunkedDataPipeUploadDataStreamTest() { CreateAndInitChunkedUploadStream(); }

  void CreateAndInitChunkedUploadStream() {
    CreateChunkedUploadStream();
    InitChunkedUploadStream();
  }

  void CreateChunkedUploadStream() {
    chunked_data_pipe_getter_ = std::make_unique<TestChunkedDataPipeGetter>();
    chunked_upload_stream_ = std::make_unique<ChunkedDataPipeUploadDataStream>(
        base::MakeRefCounted<network::ResourceRequestBody>(),
        chunked_data_pipe_getter_->GetDataPipeGetterRemote());
  }

  void InitChunkedUploadStream() {
    // Nothing interesting happens before Init, so always wait for it in the
    // test fixture.
    EXPECT_EQ(net::OK, chunked_upload_stream_->Init(NoCallback(),
                                                    net::NetLogWithSource()));
    get_size_callback_ = chunked_data_pipe_getter_->WaitForGetSize();
    write_pipe_ = chunked_data_pipe_getter_->WaitForStartReading();

    EXPECT_TRUE(chunked_upload_stream_->is_chunked());
    EXPECT_FALSE(chunked_upload_stream_->IsEOF());
    EXPECT_FALSE(chunked_upload_stream_->IsInMemory());
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO};
  std::unique_ptr<TestChunkedDataPipeGetter> chunked_data_pipe_getter_;
  std::unique_ptr<ChunkedDataPipeUploadDataStream> chunked_upload_stream_;

  mojom::ChunkedDataPipeGetter::GetSizeCallback get_size_callback_;
  mojo::ScopedDataPipeProducerHandle write_pipe_;
};

// Test just reading through the response body once, where reads are initiated
// before data is received.
TEST_F(ChunkedDataPipeUploadDataStreamTest, ReadBeforeDataReady) {
  const std::string kData = "1234567890";

  for (int consumer_read_size : {5, 10, 20}) {
    for (int num_writes : {0, 1, 2}) {
      CreateAndInitChunkedUploadStream();

      for (int i = 0; i < num_writes; ++i) {
        std::string read_data;
        while (read_data.size() < kData.size()) {
          net::TestCompletionCallback callback;
          auto io_buffer =
              base::MakeRefCounted<net::IOBufferWithSize>(consumer_read_size);
          int result = chunked_upload_stream_->Read(
              io_buffer.get(), io_buffer->size(), callback.callback());
          if (read_data.size() == 0)
            mojo::BlockingCopyFromString(kData, write_pipe_);
          result = callback.GetResult(result);
          ASSERT_LT(0, result);
          EXPECT_LE(result, consumer_read_size);
          read_data.append(std::string(io_buffer->data(), result));
          EXPECT_FALSE(chunked_upload_stream_->IsEOF());
        }
        EXPECT_EQ(read_data, kData);
      }

      auto io_buffer =
          base::MakeRefCounted<net::IOBufferWithSize>(consumer_read_size);
      net::TestCompletionCallback callback;
      int result = chunked_upload_stream_->Read(
          io_buffer.get(), io_buffer->size(), callback.callback());
      EXPECT_EQ(net::ERR_IO_PENDING, result);

      std::move(get_size_callback_).Run(net::OK, num_writes * kData.size());
      EXPECT_EQ(net::OK, callback.GetResult(result));
      EXPECT_TRUE(chunked_upload_stream_->IsEOF());
    }
  }
}

// Test just reading through the response body once, where reads are initiated
// after data has been received.
TEST_F(ChunkedDataPipeUploadDataStreamTest, ReadAfterDataReady) {
  const std::string kData = "1234567890";

  for (int consumer_read_size : {5, 10, 20}) {
    for (int num_writes : {0, 1, 2}) {
      CreateAndInitChunkedUploadStream();

      for (int i = 0; i < num_writes; ++i) {
        mojo::BlockingCopyFromString(kData, write_pipe_);
        base::RunLoop().RunUntilIdle();

        std::string read_data;
        while (read_data.size() < kData.size()) {
          net::TestCompletionCallback callback;
          auto io_buffer =
              base::MakeRefCounted<net::IOBufferWithSize>(consumer_read_size);
          int result = chunked_upload_stream_->Read(
              io_buffer.get(), io_buffer->size(), callback.callback());
          ASSERT_LT(0, result);
          EXPECT_LE(result, consumer_read_size);
          read_data.append(std::string(io_buffer->data(), result));
          EXPECT_FALSE(chunked_upload_stream_->IsEOF());
        }
        EXPECT_EQ(read_data, kData);
      }

      std::move(get_size_callback_).Run(net::OK, num_writes * kData.size());
      base::RunLoop().RunUntilIdle();

      net::TestCompletionCallback callback;
      auto io_buffer =
          base::MakeRefCounted<net::IOBufferWithSize>(consumer_read_size);
      EXPECT_EQ(net::OK,
                chunked_upload_stream_->Read(io_buffer.get(), io_buffer->size(),
                                             callback.callback()));
      EXPECT_TRUE(chunked_upload_stream_->IsEOF());
    }
  }
}

// Test the case where the URLRequest reads through the request body multiple
// times, as can happen in the case of redirects or retries.
TEST_F(ChunkedDataPipeUploadDataStreamTest, MultipleReadThrough) {
  const std::string kData = "1234567890";

  // Send size up front - cases where the size isn't initially known are checked
  // elsewhere, and have to have size before the first read through can complete
  // successfully.
  std::move(get_size_callback_).Run(net::OK, kData.size());
  base::RunLoop().RunUntilIdle();

  // 3 is an arbitrary number greater than 1.
  for (int i = 0; i < 3; i++) {
    // Already initialized before loop runs for the first time.
    if (i != 0) {
      net::TestCompletionCallback callback;
      EXPECT_EQ(net::OK, chunked_upload_stream_->Init(callback.callback(),
                                                      net::NetLogWithSource()));

      write_pipe_ = chunked_data_pipe_getter_->WaitForStartReading();
    }

    mojo::BlockingCopyFromString(kData, write_pipe_);

    std::string read_data;
    while (read_data.size() < kData.size()) {
      net::TestCompletionCallback callback;
      auto io_buffer =
          base::MakeRefCounted<net::IOBufferWithSize>(kData.size());
      int result = chunked_upload_stream_->Read(
          io_buffer.get(), io_buffer->size(), callback.callback());
      result = callback.GetResult(result);
      ASSERT_LT(0, result);
      EXPECT_LE(static_cast<size_t>(result), kData.size());
      read_data.append(std::string(io_buffer->data(), result));
      if (read_data.size() == kData.size()) {
        EXPECT_TRUE(chunked_upload_stream_->IsEOF());
      } else {
        EXPECT_FALSE(chunked_upload_stream_->IsEOF());
      }
    }
    EXPECT_EQ(kData, read_data);

    net::TestCompletionCallback callback;
    auto io_buffer = base::MakeRefCounted<net::IOBufferWithSize>(1);
    int result =
        chunked_upload_stream_->Read(io_buffer.get(), 1, callback.callback());
    EXPECT_EQ(net::OK, callback.GetResult(result));
    EXPECT_TRUE(chunked_upload_stream_->IsEOF());

    chunked_upload_stream_->Reset();
  }
}

// Test the case where the URLRequest partially reads through the request body
// multiple times, as can happen in the case of retries. The size is known from
// the start.
TEST_F(ChunkedDataPipeUploadDataStreamTest,
       MultiplePartialReadThroughWithKnownSize) {
  const std::string kData = "1234567890";

  std::move(get_size_callback_).Run(net::OK, kData.size());
  // Wait for the stream to learn the size, to avoid any races.
  base::RunLoop().RunUntilIdle();

  // In each iteration, read through more of the body. Reset the stream after
  // each loop iteration but the last, which reads the entire body.
  for (size_t num_bytes_to_read = 0; num_bytes_to_read <= kData.size();
       num_bytes_to_read++) {
    // Already initialized before loop runs for the first time.
    if (num_bytes_to_read != 0) {
      net::TestCompletionCallback callback;
      EXPECT_EQ(net::OK, chunked_upload_stream_->Init(callback.callback(),
                                                      net::NetLogWithSource()));

      write_pipe_ = chunked_data_pipe_getter_->WaitForStartReading();
    }

    mojo::BlockingCopyFromString(kData.substr(0, num_bytes_to_read),
                                 write_pipe_);

    std::string read_data;
    while (read_data.size() < num_bytes_to_read) {
      net::TestCompletionCallback callback;
      auto io_buffer =
          base::MakeRefCounted<net::IOBufferWithSize>(kData.size());
      int result = chunked_upload_stream_->Read(
          io_buffer.get(), io_buffer->size(), callback.callback());
      result = callback.GetResult(result);
      ASSERT_LT(0, result);
      EXPECT_LE(static_cast<size_t>(result), kData.size());
      read_data.append(std::string(io_buffer->data(), result));

      if (read_data.size() == kData.size()) {
        EXPECT_TRUE(chunked_upload_stream_->IsEOF());
      } else {
        EXPECT_FALSE(chunked_upload_stream_->IsEOF());
      }
    }
    EXPECT_EQ(kData.substr(0, num_bytes_to_read), read_data);
    if (num_bytes_to_read != kData.size())
      chunked_upload_stream_->Reset();
  }

  net::TestCompletionCallback callback;
  auto io_buffer = base::MakeRefCounted<net::IOBufferWithSize>(1);
  int result =
      chunked_upload_stream_->Read(io_buffer.get(), 1, callback.callback());
  EXPECT_EQ(net::OK, callback.GetResult(result));
  EXPECT_TRUE(chunked_upload_stream_->IsEOF());
}

// Test the case where the URLRequest partially reads through the request body
// multiple times, as can happen in the case of retries. The size isn't known
// until the end.
TEST_F(ChunkedDataPipeUploadDataStreamTest,
       MultiplePartialReadThroughSizeNotKnown) {
  const std::string kData = "1234567890";

  // In each iteration, read through more of the body. Reset the stream after
  // each loop iteration but the last, which reads the entire body.
  for (size_t num_bytes_to_read = 0; num_bytes_to_read <= kData.size();
       num_bytes_to_read++) {
    // Already initialized before loop runs for the first time.
    if (num_bytes_to_read != 0) {
      net::TestCompletionCallback callback;
      EXPECT_EQ(net::OK, chunked_upload_stream_->Init(callback.callback(),
                                                      net::NetLogWithSource()));

      write_pipe_ = chunked_data_pipe_getter_->WaitForStartReading();
    }

    mojo::BlockingCopyFromString(kData.substr(0, num_bytes_to_read),
                                 write_pipe_);

    std::string read_data;
    while (read_data.size() < num_bytes_to_read) {
      net::TestCompletionCallback callback;
      auto io_buffer =
          base::MakeRefCounted<net::IOBufferWithSize>(kData.size());
      int result = chunked_upload_stream_->Read(
          io_buffer.get(), io_buffer->size(), callback.callback());
      result = callback.GetResult(result);
      ASSERT_LT(0, result);
      EXPECT_LE(static_cast<size_t>(result), kData.size());
      read_data.append(std::string(io_buffer->data(), result));
      EXPECT_FALSE(chunked_upload_stream_->IsEOF());
    }
    EXPECT_EQ(kData.substr(0, num_bytes_to_read), read_data);
    if (num_bytes_to_read != kData.size())
      chunked_upload_stream_->Reset();
  }

  std::move(get_size_callback_).Run(net::OK, kData.size());
  base::RunLoop().RunUntilIdle();

  net::TestCompletionCallback callback;
  auto io_buffer = base::MakeRefCounted<net::IOBufferWithSize>(1);
  int result =
      chunked_upload_stream_->Read(io_buffer.get(), 1, callback.callback());
  EXPECT_EQ(net::OK, callback.GetResult(result));
  EXPECT_TRUE(chunked_upload_stream_->IsEOF());
}

// Test where GetSize() is invoked before the upload is initialized.
TEST_F(ChunkedDataPipeUploadDataStreamTest, GetSizeSucceedsBeforeInit) {
  const std::string kData = "1234567890";

  chunked_data_pipe_getter_ = std::make_unique<TestChunkedDataPipeGetter>();
  chunked_upload_stream_ = std::make_unique<ChunkedDataPipeUploadDataStream>(
      base::MakeRefCounted<network::ResourceRequestBody>(),
      chunked_data_pipe_getter_->GetDataPipeGetterRemote());
  get_size_callback_ = chunked_data_pipe_getter_->WaitForGetSize();
  std::move(get_size_callback_).Run(net::OK, kData.size());
  // Wait for the ChunkedUploadStream to receive the size.
  base::RunLoop().RunUntilIdle();

  net::TestCompletionCallback callback;
  ASSERT_EQ(net::OK, chunked_upload_stream_->Init(callback.callback(),
                                                  net::NetLogWithSource()));
  write_pipe_ = chunked_data_pipe_getter_->WaitForStartReading();

  EXPECT_TRUE(chunked_upload_stream_->is_chunked());
  EXPECT_FALSE(chunked_upload_stream_->IsEOF());

  mojo::BlockingCopyFromString(kData, write_pipe_);

  std::string read_data;
  while (read_data.size() < kData.size()) {
    int read_size = kData.size() - read_data.size();
    auto io_buffer = base::MakeRefCounted<net::IOBufferWithSize>(read_size);
    int result = chunked_upload_stream_->Read(
        io_buffer.get(), io_buffer->size(), callback.callback());
    result = callback.GetResult(result);
    ASSERT_LT(0, result);
    EXPECT_LE(result, read_size);
    read_data.append(std::string(io_buffer->data(), result));
    EXPECT_EQ(read_data.size() == kData.size(),
              chunked_upload_stream_->IsEOF());
  }
}

// Test where GetSize() is only invoked after the upload is reset.
TEST_F(ChunkedDataPipeUploadDataStreamTest, GetSizeSucceedsAfterReset) {
  const std::string kData = "1234567890";

  // Read through the body once, without a size
  std::string read_data;
  mojo::BlockingCopyFromString(kData, write_pipe_);
  while (read_data.size() < kData.size()) {
    net::TestCompletionCallback callback;
    int read_size = kData.size() - read_data.size();
    auto io_buffer = base::MakeRefCounted<net::IOBufferWithSize>(read_size);
    int result = chunked_upload_stream_->Read(
        io_buffer.get(), io_buffer->size(), callback.callback());
    result = callback.GetResult(result);
    ASSERT_LT(0, result);
    EXPECT_LE(result, read_size);
    read_data.append(std::string(io_buffer->data(), result));
    EXPECT_FALSE(chunked_upload_stream_->IsEOF());
  }

  // Reset, get the size, and read through the body again.
  chunked_upload_stream_->Reset();

  std::move(get_size_callback_).Run(net::OK, kData.size());
  // Wait for the ChunkedUploadStream to receive the size.
  base::RunLoop().RunUntilIdle();

  net::TestCompletionCallback callback;
  ASSERT_EQ(net::OK, chunked_upload_stream_->Init(callback.callback(),
                                                  net::NetLogWithSource()));
  write_pipe_ = chunked_data_pipe_getter_->WaitForStartReading();

  read_data.erase();
  mojo::BlockingCopyFromString(kData, write_pipe_);
  while (read_data.size() < kData.size()) {
    int read_size = kData.size() - read_data.size();
    auto io_buffer = base::MakeRefCounted<net::IOBufferWithSize>(read_size);
    int result = chunked_upload_stream_->Read(
        io_buffer.get(), io_buffer->size(), callback.callback());
    result = callback.GetResult(result);
    ASSERT_LT(0, result);
    EXPECT_LE(result, read_size);
    read_data.append(std::string(io_buffer->data(), result));
    EXPECT_EQ(read_data.size() == kData.size(),
              chunked_upload_stream_->IsEOF());
  }
}

// Test where GetSize() is invoked with an error before the upload is
// initialized.
TEST_F(ChunkedDataPipeUploadDataStreamTest, GetSizeFailsBeforeInit) {
  const std::string kData = "1234567890";

  chunked_data_pipe_getter_ = std::make_unique<TestChunkedDataPipeGetter>();
  chunked_upload_stream_ = std::make_unique<ChunkedDataPipeUploadDataStream>(
      base::MakeRefCounted<network::ResourceRequestBody>(),
      chunked_data_pipe_getter_->GetDataPipeGetterRemote());
  get_size_callback_ = chunked_data_pipe_getter_->WaitForGetSize();
  std::move(get_size_callback_).Run(net::ERR_ACCESS_DENIED, 0);
  // Wait for the ChunkedUploadStream to receive the size.
  base::RunLoop().RunUntilIdle();

  net::TestCompletionCallback callback;
  EXPECT_EQ(net::ERR_ACCESS_DENIED,
            chunked_upload_stream_->Init(callback.callback(),
                                         net::NetLogWithSource()));
}

// Test where GetSize() is only invoked with an error after the upload is reset.
TEST_F(ChunkedDataPipeUploadDataStreamTest, GetSizeFailsAfterReset) {
  const std::string kData = "1234567890";

  // Read through the body once, without a size
  std::string read_data;
  mojo::BlockingCopyFromString(kData, write_pipe_);
  while (read_data.size() < kData.size()) {
    net::TestCompletionCallback callback;
    int read_size = kData.size() - read_data.size();
    auto io_buffer = base::MakeRefCounted<net::IOBufferWithSize>(read_size);
    int result = chunked_upload_stream_->Read(
        io_buffer.get(), io_buffer->size(), callback.callback());
    result = callback.GetResult(result);
    ASSERT_LT(0, result);
    EXPECT_LE(result, read_size);
    read_data.append(std::string(io_buffer->data(), result));
    EXPECT_FALSE(chunked_upload_stream_->IsEOF());
  }

  // Reset, get the size, and read through the body again.
  chunked_upload_stream_->Reset();

  std::move(get_size_callback_).Run(net::ERR_ACCESS_DENIED, kData.size());
  // Wait for the ChunkedUploadStream to receive the size.
  base::RunLoop().RunUntilIdle();

  net::TestCompletionCallback callback;
  ASSERT_EQ(net::ERR_ACCESS_DENIED,
            chunked_upload_stream_->Init(callback.callback(),
                                         net::NetLogWithSource()));
}

// Three variations on when the stream can be closed before a request succeeds.

// Stream is closed, then a read attempted, then the GetSizeCallback is invoked.
// The read should notice the close body pipe, but not report anything until the
// GetSizeCallback is invoked.
TEST_F(ChunkedDataPipeUploadDataStreamTest, CloseBodyPipeBeforeSuccess1) {
  write_pipe_.reset();
  base::RunLoop().RunUntilIdle();

  net::TestCompletionCallback callback;
  auto io_buffer = base::MakeRefCounted<net::IOBufferWithSize>(1);
  int result =
      chunked_upload_stream_->Read(io_buffer.get(), 1, callback.callback());
  EXPECT_EQ(result, net::ERR_IO_PENDING);
  EXPECT_FALSE(chunked_upload_stream_->IsEOF());

  std::move(get_size_callback_).Run(net::OK, 0);
  EXPECT_EQ(net::OK, callback.GetResult(result));
  EXPECT_TRUE(chunked_upload_stream_->IsEOF());
}

// A read attempt, stream is closed, then the GetSizeCallback is invoked. The
// watcher should see the close, but not report anything until the
// GetSizeCallback is invoked.
TEST_F(ChunkedDataPipeUploadDataStreamTest, CloseBodyPipeBeforeSuccess2) {
  net::TestCompletionCallback callback;
  auto io_buffer = base::MakeRefCounted<net::IOBufferWithSize>(1);
  int result =
      chunked_upload_stream_->Read(io_buffer.get(), 1, callback.callback());
  EXPECT_EQ(result, net::ERR_IO_PENDING);

  write_pipe_.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(callback.have_result());
  EXPECT_FALSE(chunked_upload_stream_->IsEOF());

  std::move(get_size_callback_).Run(net::OK, 0);
  EXPECT_EQ(net::OK, callback.GetResult(result));
  EXPECT_TRUE(chunked_upload_stream_->IsEOF());
}

// The stream is closed, the GetSizeCallback is invoked, and then a read
// attempt. The read attempt should notice the request already successfully
// completed.
TEST_F(ChunkedDataPipeUploadDataStreamTest, CloseBodyPipeBeforeSuccess3) {
  write_pipe_.reset();
  base::RunLoop().RunUntilIdle();

  std::move(get_size_callback_).Run(net::OK, 0);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(chunked_upload_stream_->IsEOF());

  net::TestCompletionCallback callback;
  auto io_buffer = base::MakeRefCounted<net::IOBufferWithSize>(1);
  int result =
      chunked_upload_stream_->Read(io_buffer.get(), 1, callback.callback());
  EXPECT_EQ(net::OK, callback.GetResult(result));
  EXPECT_TRUE(chunked_upload_stream_->IsEOF());
}

// Same cases as above, but the GetSizeCallback indicates the response was
// truncated.

TEST_F(ChunkedDataPipeUploadDataStreamTest, CloseBodyPipeBeforeTruncation1) {
  write_pipe_.reset();
  base::RunLoop().RunUntilIdle();

  net::TestCompletionCallback callback;
  auto io_buffer = base::MakeRefCounted<net::IOBufferWithSize>(1);
  int result =
      chunked_upload_stream_->Read(io_buffer.get(), 1, callback.callback());
  EXPECT_EQ(result, net::ERR_IO_PENDING);

  std::move(get_size_callback_).Run(net::OK, 1);
  EXPECT_EQ(net::ERR_FAILED, callback.GetResult(result));
}

TEST_F(ChunkedDataPipeUploadDataStreamTest, CloseBodyPipeBeforeTruncation2) {
  net::TestCompletionCallback callback;
  auto io_buffer = base::MakeRefCounted<net::IOBufferWithSize>(1);
  int result =
      chunked_upload_stream_->Read(io_buffer.get(), 1, callback.callback());
  EXPECT_EQ(result, net::ERR_IO_PENDING);

  write_pipe_.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(callback.have_result());

  std::move(get_size_callback_).Run(net::OK, 1);
  EXPECT_EQ(net::ERR_FAILED, callback.GetResult(result));
}

TEST_F(ChunkedDataPipeUploadDataStreamTest, CloseBodyPipeBeforeTruncation3) {
  write_pipe_.reset();
  base::RunLoop().RunUntilIdle();

  std::move(get_size_callback_).Run(net::OK, 1);

  net::TestCompletionCallback callback;
  auto io_buffer = base::MakeRefCounted<net::IOBufferWithSize>(1);
  int result =
      chunked_upload_stream_->Read(io_buffer.get(), 1, callback.callback());
  EXPECT_EQ(net::ERR_FAILED, callback.GetResult(result));
}

// Same cases as above, but the GetSizeCallback indicates the read failed.

TEST_F(ChunkedDataPipeUploadDataStreamTest, CloseBodyPipeBeforeFailure1) {
  write_pipe_.reset();
  base::RunLoop().RunUntilIdle();

  net::TestCompletionCallback callback;
  auto io_buffer = base::MakeRefCounted<net::IOBufferWithSize>(1);
  int result =
      chunked_upload_stream_->Read(io_buffer.get(), 1, callback.callback());
  EXPECT_EQ(result, net::ERR_IO_PENDING);

  std::move(get_size_callback_).Run(net::ERR_ACCESS_DENIED, 0);
  EXPECT_EQ(net::ERR_ACCESS_DENIED, callback.GetResult(result));
}

TEST_F(ChunkedDataPipeUploadDataStreamTest, CloseBodyPipeBeforeFailure2) {
  net::TestCompletionCallback callback;
  auto io_buffer = base::MakeRefCounted<net::IOBufferWithSize>(1);
  int result =
      chunked_upload_stream_->Read(io_buffer.get(), 1, callback.callback());
  EXPECT_EQ(result, net::ERR_IO_PENDING);

  write_pipe_.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(callback.have_result());

  std::move(get_size_callback_).Run(net::ERR_ACCESS_DENIED, 0);
  EXPECT_EQ(net::ERR_ACCESS_DENIED, callback.GetResult(result));
}

TEST_F(ChunkedDataPipeUploadDataStreamTest, CloseBodyPipeBeforeFailure3) {
  write_pipe_.reset();
  base::RunLoop().RunUntilIdle();

  std::move(get_size_callback_).Run(net::ERR_ACCESS_DENIED, 0);

  net::TestCompletionCallback callback;
  auto io_buffer = base::MakeRefCounted<net::IOBufferWithSize>(1);
  int result =
      chunked_upload_stream_->Read(io_buffer.get(), 1, callback.callback());
  EXPECT_EQ(net::ERR_ACCESS_DENIED, callback.GetResult(result));
}

// Same cases as above, but ChunkedBodyGetter pipe is destroyed without invoking
// the GetSizeCallback.

TEST_F(ChunkedDataPipeUploadDataStreamTest, CloseBodyPipeBeforeCloseGetter1) {
  write_pipe_.reset();
  base::RunLoop().RunUntilIdle();

  net::TestCompletionCallback callback;
  auto io_buffer = base::MakeRefCounted<net::IOBufferWithSize>(1);
  int result =
      chunked_upload_stream_->Read(io_buffer.get(), 1, callback.callback());
  EXPECT_EQ(result, net::ERR_IO_PENDING);

  chunked_data_pipe_getter_.reset();
  EXPECT_EQ(net::ERR_FAILED, callback.GetResult(result));
}

TEST_F(ChunkedDataPipeUploadDataStreamTest, CloseBodyPipeBeforeCloseGetter2) {
  net::TestCompletionCallback callback;
  auto io_buffer = base::MakeRefCounted<net::IOBufferWithSize>(1);
  int result =
      chunked_upload_stream_->Read(io_buffer.get(), 1, callback.callback());
  EXPECT_EQ(result, net::ERR_IO_PENDING);

  write_pipe_.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(callback.have_result());

  chunked_data_pipe_getter_.reset();
  EXPECT_EQ(net::ERR_FAILED, callback.GetResult(result));
}

TEST_F(ChunkedDataPipeUploadDataStreamTest, CloseBodyPipeBeforeCloseGetter3) {
  write_pipe_.reset();
  base::RunLoop().RunUntilIdle();

  chunked_data_pipe_getter_.reset();

  net::TestCompletionCallback callback;
  auto io_buffer = base::MakeRefCounted<net::IOBufferWithSize>(1);
  int result =
      chunked_upload_stream_->Read(io_buffer.get(), 1, callback.callback());
  EXPECT_EQ(net::ERR_FAILED, callback.GetResult(result));
}

// Test uses same order as CloseBodyPipeBeforeTruncation1, but in this test the
// GetSizeCallback indicates too many bytes were received.
TEST_F(ChunkedDataPipeUploadDataStreamTest, ExtraBytes1) {
  const std::string kData = "123";
  mojo::BlockingCopyFromString(kData, write_pipe_);

  std::string read_data;
  while (read_data.size() < kData.size()) {
    net::TestCompletionCallback callback;
    auto io_buffer = base::MakeRefCounted<net::IOBufferWithSize>(kData.size());
    int result = chunked_upload_stream_->Read(
        io_buffer.get(), io_buffer->size(), callback.callback());
    result = callback.GetResult(result);
    ASSERT_LT(0, result);
    read_data.append(std::string(io_buffer->data(), result));
    EXPECT_LE(read_data.size(), kData.size());
    EXPECT_FALSE(chunked_upload_stream_->IsEOF());
  }

  net::TestCompletionCallback callback;
  auto io_buffer = base::MakeRefCounted<net::IOBufferWithSize>(1);
  int result =
      chunked_upload_stream_->Read(io_buffer.get(), 1, callback.callback());
  EXPECT_EQ(result, net::ERR_IO_PENDING);

  std::move(get_size_callback_).Run(net::OK, kData.size() - 1);
  EXPECT_EQ(net::ERR_FAILED, callback.GetResult(result));
}

// Extra bytes are received after getting the size notification. No error should
// be reported,
TEST_F(ChunkedDataPipeUploadDataStreamTest, ExtraBytes2) {
  const std::string kData = "123";

  // Read first byte.
  mojo::BlockingCopyFromString(kData.substr(0, 1), write_pipe_);
  net::TestCompletionCallback callback;
  auto io_buffer = base::MakeRefCounted<net::IOBufferWithSize>(1);
  int result = chunked_upload_stream_->Read(io_buffer.get(), io_buffer->size(),
                                            callback.callback());
  result = callback.GetResult(result);
  ASSERT_EQ(1, result);
  EXPECT_FALSE(chunked_upload_stream_->IsEOF());

  // Start another read.
  net::TestCompletionCallback callback2;
  ASSERT_EQ(net::ERR_IO_PENDING, chunked_upload_stream_->Read(
                                     io_buffer.get(), 1, callback2.callback()));

  // Learn the size was only one byte. Read should complete, indicating the end
  // of the stream was reached.
  std::move(get_size_callback_).Run(net::OK, 1);
  EXPECT_EQ(net::OK, callback2.WaitForResult());
  EXPECT_TRUE(chunked_upload_stream_->IsEOF());

  // More data is copied to the stream unexpectedly.  It should be ignored.
  mojo::BlockingCopyFromString(kData.substr(1), write_pipe_);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(chunked_upload_stream_->IsEOF());
}

TEST_F(ChunkedDataPipeUploadDataStreamTest, ClosePipeGetterBeforeInit) {
  chunked_data_pipe_getter_ = std::make_unique<TestChunkedDataPipeGetter>();
  chunked_upload_stream_ = std::make_unique<ChunkedDataPipeUploadDataStream>(
      base::MakeRefCounted<network::ResourceRequestBody>(),
      chunked_data_pipe_getter_->GetDataPipeGetterRemote());

  // Destroy the DataPipeGetter pipe, which is the pipe used for
  // GetSizeCallback.
  chunked_data_pipe_getter_->ClosePipe();
  base::RunLoop().RunUntilIdle();

  // Init should fail in this case.
  net::TestCompletionCallback callback;
  EXPECT_EQ(net::ERR_FAILED, chunked_upload_stream_->Init(
                                 callback.callback(), net::NetLogWithSource()));
}

TEST_F(ChunkedDataPipeUploadDataStreamTest,
       ClosePipeGetterWithoutCallingGetSizeCallbackNoPendingRead) {
  // Destroy the DataPipeGetter pipe, which is the pipe used for
  // GetSizeCallback.
  chunked_data_pipe_getter_->ClosePipe();
  base::RunLoop().RunUntilIdle();

  net::TestCompletionCallback callback;
  auto io_buffer = base::MakeRefCounted<net::IOBufferWithSize>(1);
  int result =
      chunked_upload_stream_->Read(io_buffer.get(), 1, callback.callback());
  EXPECT_EQ(net::ERR_FAILED, callback.GetResult(result));
}

TEST_F(ChunkedDataPipeUploadDataStreamTest,
       ClosePipeGetterWithoutCallingGetSizeCallbackPendingRead) {
  net::TestCompletionCallback callback;
  auto io_buffer = base::MakeRefCounted<net::IOBufferWithSize>(1);
  int result =
      chunked_upload_stream_->Read(io_buffer.get(), 1, callback.callback());
  EXPECT_EQ(net::ERR_IO_PENDING, result);

  // Destroy the DataPipeGetter pipe, which is the pipe used for
  // GetSizeCallback.
  chunked_data_pipe_getter_->ClosePipe();
  EXPECT_EQ(net::ERR_FAILED, callback.GetResult(result));
}

TEST_F(ChunkedDataPipeUploadDataStreamTest,
       ClosePipeGetterAfterCallingGetSizeCallback) {
  const char kData[] = "1";
  const int kDataLen = strlen(kData);
  net::TestCompletionCallback callback;
  auto io_buffer = base::MakeRefCounted<net::IOBufferWithSize>(kDataLen);
  std::move(get_size_callback_).Run(net::OK, kDataLen);
  // Destroy the DataPipeGetter pipe, which is the pipe used for
  // GetSizeCallback.
  chunked_data_pipe_getter_->ClosePipe();
  base::RunLoop().RunUntilIdle();

  int result = chunked_upload_stream_->Read(io_buffer.get(), kDataLen,
                                            callback.callback());
  EXPECT_EQ(net::ERR_IO_PENDING, result);
  mojo::BlockingCopyFromString(kData, write_pipe_);

  // Since the pipe was closed the GetSizeCallback was invoked, reading the
  // upload body should succeed once.
  EXPECT_EQ(kDataLen, callback.GetResult(result));
  EXPECT_TRUE(chunked_upload_stream_->IsEOF());

  net::TestCompletionCallback callback2;
  EXPECT_EQ(0, chunked_upload_stream_->Read(io_buffer.get(), kDataLen,
                                            callback2.callback()));

  // But trying again will result in failure.
  chunked_upload_stream_->Reset();

  net::TestCompletionCallback callback3;
  EXPECT_EQ(net::ERR_FAILED,
            chunked_upload_stream_->Init(callback3.callback(),
                                         net::NetLogWithSource()));
}

#define EXPECT_READ(chunked_upload_stream, io_buffer, expected)       \
  {                                                                   \
    int read_result = chunked_upload_stream->Read(                    \
        io_buffer.get(), io_buffer->size(), NoCallback());            \
    EXPECT_GT(read_result, 0);                                        \
    EXPECT_EQ(std::string(io_buffer->data(), read_result), expected); \
  }

#define EXPECT_EOF(chunked_upload_stream, size)                               \
  {                                                                           \
    net::TestCompletionCallback test_callback;                                \
    auto one_byte_io_buffer = base::MakeRefCounted<net::IOBufferWithSize>(1); \
    int read_result = chunked_upload_stream->Read(                            \
        one_byte_io_buffer.get(), 1u, test_callback.callback());              \
    EXPECT_EQ(net::ERR_IO_PENDING, read_result);                              \
    std::move(get_size_callback_).Run(net::OK, size);                         \
    EXPECT_EQ(net::OK, test_callback.GetResult(read_result));                 \
    EXPECT_TRUE(chunked_upload_stream->IsEOF());                              \
  }

#define WRITE_DATA_SYNC(write_pipe, str)                       \
  {                                                            \
    std::string data(str);                                     \
    size_t actually_written_bytes = 0;                         \
    EXPECT_EQ(write_pipe->WriteData(base::as_byte_span(data),  \
                                    MOJO_WRITE_DATA_FLAG_NONE, \
                                    actually_written_bytes),   \
              MOJO_RESULT_OK);                                 \
    EXPECT_EQ(actually_written_bytes, data.size());            \
  }

TEST_F(ChunkedDataPipeUploadDataStreamTest, CacheNotUsed) {
  chunked_upload_stream_->EnableCache();

  const std::string kData = "1234567890";
  WRITE_DATA_SYNC(write_pipe_, kData);

  auto io_buffer = base::MakeRefCounted<net::IOBufferWithSize>(7);
  EXPECT_READ(chunked_upload_stream_, io_buffer, "1234567");
  EXPECT_FALSE(chunked_upload_stream_->IsEOF());

  EXPECT_READ(chunked_upload_stream_, io_buffer, "890");
  EXPECT_FALSE(chunked_upload_stream_->IsEOF());

  EXPECT_EOF(chunked_upload_stream_, kData.size());
}

TEST_F(ChunkedDataPipeUploadDataStreamTest, CacheEnableBeforeInit1) {
  CreateChunkedUploadStream();
  chunked_upload_stream_->EnableCache();
  InitChunkedUploadStream();

  auto io_buffer = base::MakeRefCounted<net::IOBufferWithSize>(7);
  int result = chunked_upload_stream_->Read(io_buffer.get(), io_buffer->size(),
                                            NoCallback());
  EXPECT_EQ(result, net::ERR_IO_PENDING);

  // Destroy the DataPipeGetter pipe, which is the pipe used for
  // GetSizeCallback.
  chunked_data_pipe_getter_->ClosePipe();
}

TEST_F(ChunkedDataPipeUploadDataStreamTest, CacheEnableBeforeInit2) {
  CreateChunkedUploadStream();
  chunked_upload_stream_->EnableCache();
  InitChunkedUploadStream();

  WRITE_DATA_SYNC(write_pipe_, "1234567890");

  auto io_buffer = base::MakeRefCounted<net::IOBufferWithSize>(7);

  EXPECT_READ(chunked_upload_stream_, io_buffer, "1234567");
  EXPECT_FALSE(chunked_upload_stream_->IsEOF());

  EXPECT_EQ(chunked_upload_stream_->Init(NoCallback(), net::NetLogWithSource()),
            net::OK);
  EXPECT_READ(chunked_upload_stream_, io_buffer, "1234567");
  EXPECT_READ(chunked_upload_stream_, io_buffer, "890");
  EXPECT_FALSE(chunked_upload_stream_->IsEOF());

  EXPECT_EOF(chunked_upload_stream_, 10);
}

TEST_F(ChunkedDataPipeUploadDataStreamTest, CacheRead) {
  chunked_upload_stream_->EnableCache();

  WRITE_DATA_SYNC(write_pipe_, "1234567890");

  auto io_buffer = base::MakeRefCounted<net::IOBufferWithSize>(7);

  EXPECT_READ(chunked_upload_stream_, io_buffer, "1234567");
  EXPECT_FALSE(chunked_upload_stream_->IsEOF());

  EXPECT_EQ(chunked_upload_stream_->Init(NoCallback(), net::NetLogWithSource()),
            net::OK);
  EXPECT_READ(chunked_upload_stream_, io_buffer, "1234567");
  EXPECT_READ(chunked_upload_stream_, io_buffer, "890");
  EXPECT_FALSE(chunked_upload_stream_->IsEOF());

  EXPECT_EOF(chunked_upload_stream_, 10);
}

TEST_F(ChunkedDataPipeUploadDataStreamTest, CacheOverWindowOnce) {
  const size_t kMaxSize = 4u;
  chunked_upload_stream_->EnableCache(kMaxSize);

  WRITE_DATA_SYNC(write_pipe_, "1234567890");

  auto io_buffer = base::MakeRefCounted<net::IOBufferWithSize>(7);
  EXPECT_READ(chunked_upload_stream_, io_buffer, "1234567");
  EXPECT_FALSE(chunked_upload_stream_->IsEOF());

  EXPECT_EQ(chunked_upload_stream_->Init(NoCallback(), net::NetLogWithSource()),
            net::OK);
  EXPECT_READ(chunked_upload_stream_, io_buffer, "1234567");

  EXPECT_READ(chunked_upload_stream_, io_buffer, "890");
  EXPECT_FALSE(chunked_upload_stream_->IsEOF());

  EXPECT_EOF(chunked_upload_stream_, 10);
}

TEST_F(ChunkedDataPipeUploadDataStreamTest, CacheOverWindowTwice) {
  const size_t kMaxSize = 4u;
  chunked_upload_stream_->EnableCache(kMaxSize);

  WRITE_DATA_SYNC(write_pipe_, "1234567890");

  auto io_buffer = base::MakeRefCounted<net::IOBufferWithSize>(7);
  EXPECT_READ(chunked_upload_stream_, io_buffer, "1234567");
  EXPECT_READ(chunked_upload_stream_, io_buffer, "890");

  int result =
      chunked_upload_stream_->Init(NoCallback(), net::NetLogWithSource());
  EXPECT_EQ(net::ERR_FAILED, result);

  // Destroy the DataPipeGetter pipe, which is the pipe used for
  // GetSizeCallback.
  chunked_data_pipe_getter_->ClosePipe();
}

TEST_F(ChunkedDataPipeUploadDataStreamTest, CacheInitBeforeRead) {
  chunked_upload_stream_->EnableCache();
  WRITE_DATA_SYNC(write_pipe_, "1234567890");
  auto io_buffer = base::MakeRefCounted<net::IOBufferWithSize>(7);

  EXPECT_EQ(chunked_upload_stream_->Init(NoCallback(), net::NetLogWithSource()),
            net::OK);
  EXPECT_READ(chunked_upload_stream_, io_buffer, "1234567");
  EXPECT_READ(chunked_upload_stream_, io_buffer, "890");
  EXPECT_FALSE(chunked_upload_stream_->IsEOF());

  EXPECT_EOF(chunked_upload_stream_, 10);
}

TEST_F(ChunkedDataPipeUploadDataStreamTest, CacheInitWhileRead) {
  chunked_upload_stream_->EnableCache();
  WRITE_DATA_SYNC(write_pipe_, "1234567890");
  auto io_buffer = base::MakeRefCounted<net::IOBufferWithSize>(7);

  EXPECT_READ(chunked_upload_stream_, io_buffer, "1234567");

  io_buffer = base::MakeRefCounted<net::IOBufferWithSize>(3);
  EXPECT_EQ(chunked_upload_stream_->Init(NoCallback(), net::NetLogWithSource()),
            net::OK);
  EXPECT_READ(chunked_upload_stream_, io_buffer, "123");
  EXPECT_READ(chunked_upload_stream_, io_buffer, "456");
  EXPECT_FALSE(chunked_upload_stream_->IsEOF());

  // Re-init in the middle of reading from the cache
  EXPECT_EQ(chunked_upload_stream_->Init(NoCallback(), net::NetLogWithSource()),
            net::OK);
  EXPECT_READ(chunked_upload_stream_, io_buffer, "123");
  EXPECT_READ(chunked_upload_stream_, io_buffer, "456");
  EXPECT_READ(chunked_upload_stream_, io_buffer, "7");
  EXPECT_FALSE(chunked_upload_stream_->IsEOF());

  // Re-init exactly after reading the last cached bit.
  EXPECT_EQ(chunked_upload_stream_->Init(NoCallback(), net::NetLogWithSource()),
            net::OK);
  EXPECT_READ(chunked_upload_stream_, io_buffer, "123");
  EXPECT_READ(chunked_upload_stream_, io_buffer, "456");
  EXPECT_READ(chunked_upload_stream_, io_buffer, "7");
  EXPECT_READ(chunked_upload_stream_, io_buffer, "890");
  EXPECT_FALSE(chunked_upload_stream_->IsEOF());

  // Reading "890" over the first cache should append "890" to the cache.
  io_buffer = base::MakeRefCounted<net::IOBufferWithSize>(7);
  EXPECT_EQ(chunked_upload_stream_->Init(NoCallback(), net::NetLogWithSource()),
            net::OK);
  EXPECT_READ(chunked_upload_stream_, io_buffer, "1234567");
  EXPECT_READ(chunked_upload_stream_, io_buffer, "890");
  EXPECT_FALSE(chunked_upload_stream_->IsEOF());
  EXPECT_EOF(chunked_upload_stream_, 10);
}

TEST_F(ChunkedDataPipeUploadDataStreamTest, CacheReadAppendDataBeforeInit) {
  chunked_upload_stream_->EnableCache();
  WRITE_DATA_SYNC(write_pipe_, "1234567890");
  auto io_buffer = base::MakeRefCounted<net::IOBufferWithSize>(7);

  EXPECT_READ(chunked_upload_stream_, io_buffer, "1234567");
  EXPECT_READ(chunked_upload_stream_, io_buffer, "890");
  EXPECT_FALSE(chunked_upload_stream_->IsEOF());

  net::TestCompletionCallback callback;
  int result = chunked_upload_stream_->Read(io_buffer.get(), io_buffer->size(),
                                            callback.callback());
  EXPECT_EQ(net::ERR_IO_PENDING, result);

  WRITE_DATA_SYNC(write_pipe_, "abc");
  EXPECT_EQ(chunked_upload_stream_->Init(NoCallback(), net::NetLogWithSource()),
            net::OK);
  // Wait mojo.
  base::RunLoop().RunUntilIdle();
  // We should not receive the second mojo data.
  EXPECT_FALSE(callback.have_result());

  EXPECT_READ(chunked_upload_stream_, io_buffer, "1234567");
  EXPECT_READ(chunked_upload_stream_, io_buffer, "890");
  EXPECT_READ(chunked_upload_stream_, io_buffer, "abc");

  EXPECT_EOF(chunked_upload_stream_, 13);
}

TEST_F(ChunkedDataPipeUploadDataStreamTest, CacheReadAppendDataAfterInit) {
  chunked_upload_stream_->EnableCache();
  WRITE_DATA_SYNC(write_pipe_, "1234567890");
  auto io_buffer = base::MakeRefCounted<net::IOBufferWithSize>(7);

  EXPECT_READ(chunked_upload_stream_, io_buffer, "1234567");
  EXPECT_READ(chunked_upload_stream_, io_buffer, "890");
  EXPECT_FALSE(chunked_upload_stream_->IsEOF());

  net::TestCompletionCallback callback;
  int result = chunked_upload_stream_->Read(io_buffer.get(), io_buffer->size(),
                                            callback.callback());
  EXPECT_EQ(net::ERR_IO_PENDING, result);
  EXPECT_EQ(chunked_upload_stream_->Init(NoCallback(), net::NetLogWithSource()),
            net::OK);
  WRITE_DATA_SYNC(write_pipe_, "abc");
  // Wait mojo.
  base::RunLoop().RunUntilIdle();
  // We should not receive the second mojo data.
  EXPECT_FALSE(callback.have_result());

  EXPECT_READ(chunked_upload_stream_, io_buffer, "1234567");
  EXPECT_READ(chunked_upload_stream_, io_buffer, "890");
  EXPECT_READ(chunked_upload_stream_, io_buffer, "abc");

  EXPECT_EOF(chunked_upload_stream_, 13);
}

TEST_F(ChunkedDataPipeUploadDataStreamTest, CacheReadAppendDataDuringRead) {
  chunked_upload_stream_->EnableCache();
  WRITE_DATA_SYNC(write_pipe_, "1234567890");
  auto io_buffer = base::MakeRefCounted<net::IOBufferWithSize>(7);

  EXPECT_READ(chunked_upload_stream_, io_buffer, "1234567");
  EXPECT_READ(chunked_upload_stream_, io_buffer, "890");
  EXPECT_FALSE(chunked_upload_stream_->IsEOF());

  net::TestCompletionCallback callback;
  int result = chunked_upload_stream_->Read(io_buffer.get(), io_buffer->size(),
                                            callback.callback());
  EXPECT_EQ(net::ERR_IO_PENDING, result);
  EXPECT_EQ(chunked_upload_stream_->Init(NoCallback(), net::NetLogWithSource()),
            net::OK);
  // We should not receive the second mojo data.
  EXPECT_FALSE(callback.have_result());
  EXPECT_READ(chunked_upload_stream_, io_buffer, "1234567");

  WRITE_DATA_SYNC(write_pipe_, "abc");
  // Wait mojo.
  base::RunLoop().RunUntilIdle();

  EXPECT_READ(chunked_upload_stream_, io_buffer, "890");
  EXPECT_READ(chunked_upload_stream_, io_buffer, "abc");

  EXPECT_EOF(chunked_upload_stream_, 13);
}

TEST_F(ChunkedDataPipeUploadDataStreamTest, ErrorAndDetach) {
  chunked_data_pipe_getter_ = std::make_unique<TestChunkedDataPipeGetter>();
  chunked_upload_stream_ = std::make_unique<ChunkedDataPipeUploadDataStream>(
      base::MakeRefCounted<network::ResourceRequestBody>(),
      chunked_data_pipe_getter_->GetDataPipeGetterRemote());
  get_size_callback_ = chunked_data_pipe_getter_->WaitForGetSize();
  std::move(get_size_callback_).Run(net::ERR_FAILED, 0);

  base::RunLoop().RunUntilIdle();
  chunked_data_pipe_getter_->ClosePipe();
  base::RunLoop().RunUntilIdle();
}

}  // namespace

}  // namespace network
