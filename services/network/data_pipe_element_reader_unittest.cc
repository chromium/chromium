// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/data_pipe_element_reader.h"

#include <stdint.h>

#include <limits>
#include <memory>

#include "base/memory/ref_counted.h"
#include "base/notimplemented.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_view_util.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/data_pipe_utils.h"
#include "net/base/io_buffer.h"
#include "net/base/test_completion_callback.h"
#include "net/test/gtest_util.h"
#include "services/network/public/mojom/data_pipe_getter.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

// Most tests of this class are at the URLLoader layer. These tests focus on
// things too difficult to cover with integration tests.

namespace network {

namespace {

class PassThroughDataPipeGetter : public mojom::DataPipeGetter {
 public:
  explicit PassThroughDataPipeGetter() = default;

  PassThroughDataPipeGetter(const PassThroughDataPipeGetter&) = delete;
  PassThroughDataPipeGetter& operator=(const PassThroughDataPipeGetter&) =
      delete;

  mojo::PendingRemote<network::mojom::DataPipeGetter>
  GetDataPipeGetterRemote() {
    EXPECT_FALSE(receiver_.is_bound());
    return receiver_.BindNewPipeAndPassRemote();
  }

  void WaitForRead(mojo::ScopedDataPipeProducerHandle* write_pipe,
                   ReadCallback* read_callback) {
    DCHECK(!run_loop_);

    if (!write_pipe_.is_valid()) {
      run_loop_ = std::make_unique<base::RunLoop>();
      run_loop_->Run();
      run_loop_.reset();
    }

    EXPECT_TRUE(write_pipe_.is_valid());
    EXPECT_TRUE(read_callback_);

    *write_pipe = std::move(write_pipe_);
    *read_callback = std::move(read_callback_);
  }

 private:
  // network::mojom::DataPipeGetter implementation:
  void Read(mojo::ScopedDataPipeProducerHandle pipe,
            ReadCallback callback) override {
    EXPECT_FALSE(write_pipe_.is_valid());
    EXPECT_FALSE(read_callback_);

    write_pipe_ = std::move(pipe);
    read_callback_ = std::move(callback);

    if (run_loop_)
      run_loop_->Quit();
  }

  void Clone(
      mojo::PendingReceiver<network::mojom::DataPipeGetter> receiver) override {
    NOTIMPLEMENTED();
  }

  std::unique_ptr<base::RunLoop> run_loop_;

  mojo::Receiver<network::mojom::DataPipeGetter> receiver_{this};
  mojo::ScopedDataPipeProducerHandle write_pipe_;
  ReadCallback read_callback_;
};

class DataPipeElementReaderTest : public testing::Test {
 public:
  DataPipeElementReaderTest()
      : element_reader_(nullptr, data_pipe_getter_.GetDataPipeGetterRemote()) {}

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO};
  PassThroughDataPipeGetter data_pipe_getter_;
  DataPipeElementReader element_reader_;
};

// Test that if a bad status code is passed to the ReadCallback, initialization
// fails.
TEST_F(DataPipeElementReaderTest, BadStatusCode) {
  // Returned as final net error codes, but these are invalid completion codes.
  const int test_cases[] = {net::ERR_IO_PENDING, 1};

  for (int test_case : test_cases) {
    SCOPED_TRACE(test_case);

    net::TestCompletionCallback init_callback;
    ASSERT_THAT(element_reader_.Init(init_callback.callback()),
                net::test::IsError(net::ERR_IO_PENDING));

    // Wait for DataPipeGetter::Read() to be called.
    mojo::ScopedDataPipeProducerHandle write_pipe;
    network::mojom::DataPipeGetter::ReadCallback read_pipe_callback;
    data_pipe_getter_.WaitForRead(&write_pipe, &read_pipe_callback);

    // Pass in the bad Error code, along with a size that should be ignored.
    std::move(read_pipe_callback).Run(test_case, 100);

    EXPECT_THAT(init_callback.WaitForResult(),
                net::test::IsError(net::ERR_INVALID_ARGUMENT));
  }
}

// Test the case where a caller tries to write more data than is requested. The
// extra bytes should be ignored.
TEST_F(DataPipeElementReaderTest, TooMuchWritten) {
  // Body that's written over the pipe.
  std::string body = "body+";
  // Advertised size of the body.
  size_t advertised_size = body.size() - 1;

  // The network stack calls Init.
  net::TestCompletionCallback init_callback;
  ASSERT_THAT(element_reader_.Init(init_callback.callback()),
              net::test::IsError(net::ERR_IO_PENDING));

  // Wait for DataPipeGetter::Read() to be called.
  mojo::ScopedDataPipeProducerHandle write_pipe;
  network::mojom::DataPipeGetter::ReadCallback read_pipe_callback;
  data_pipe_getter_.WaitForRead(&write_pipe, &read_pipe_callback);
  std::move(read_pipe_callback).Run(net::OK, advertised_size);

  ASSERT_THAT(init_callback.WaitForResult(), net::test::IsOk());
  ASSERT_EQ(element_reader_.GetContentLength(), advertised_size);

  // Write the full body. Even though we aren't reading anything yet, the body
  // is short enough that it should be buffered.
  mojo::BlockingCopyFromString(body, write_pipe);

  // Try to read from the body. It should typically be consumed in a single
  // read, with the next read returning net::OK / 0, but handle multiple reads
  // as well.
  std::string read_data;
  while (true) {
    EXPECT_EQ(element_reader_.BytesRemaining(),
              advertised_size - read_data.size());
    auto io_buffer = base::MakeRefCounted<net::IOBufferWithSize>(10);
    net::TestCompletionCallback read_callback;
    int bytes_read = read_callback.GetResult(element_reader_.Read(
        io_buffer.get(), io_buffer->size(), read_callback.callback()));
    if (bytes_read == net::OK) {
      break;
    }
    ASSERT_GT(bytes_read, 0);
    read_data.append(base::as_string_view(io_buffer->first(bytes_read)));
  }
  EXPECT_EQ(read_data, body.substr(0, advertised_size));
  EXPECT_EQ(element_reader_.BytesRemaining(), 0u);
}

// Test the case where a second Init() call occurs when there's a pending Init()
// call in progress. The first call should be dropped, in favor of the second
// one.
TEST_F(DataPipeElementReaderTest, InitInterruptsInit) {
  // Value deliberately outside of the range of an uint32_t, to catch any
  // accidental conversions to an int.
  const uint64_t kResponseBodySize = std::numeric_limits<uint32_t>::max();

  // The network stack calls Init.
  net::TestCompletionCallback first_init_callback;
  EXPECT_EQ(net::ERR_IO_PENDING,
            element_reader_.Init(first_init_callback.callback()));

  // Wait for DataPipeGetter::Read() to be called.
  mojo::ScopedDataPipeProducerHandle first_write_pipe;
  network::mojom::DataPipeGetter::ReadCallback first_read_pipe_callback;
  data_pipe_getter_.WaitForRead(&first_write_pipe, &first_read_pipe_callback);

  // The network stack calls Init again, interrupting the previous call.
  net::TestCompletionCallback second_init_callback;
  EXPECT_EQ(net::ERR_IO_PENDING,
            element_reader_.Init(second_init_callback.callback()));

  // Wait for DataPipeGetter::Read() to be called again.
  mojo::ScopedDataPipeProducerHandle second_write_pipe;
  network::mojom::DataPipeGetter::ReadCallback second_read_pipe_callback;
  data_pipe_getter_.WaitForRead(&second_write_pipe, &second_read_pipe_callback);

  // Sending data on the first read pipe should do nothing.
  std::move(first_read_pipe_callback)
      .Run(net::ERR_FAILED, kResponseBodySize - 1);
  // Run any pending tasks, to make sure nothing unexpected is queued.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(first_init_callback.have_result());
  EXPECT_FALSE(second_init_callback.have_result());

  // Sending data on the second pipe should result in the second init callback
  // being invoked.
  std::move(second_read_pipe_callback).Run(net::OK, kResponseBodySize);
  EXPECT_EQ(net::OK, second_init_callback.WaitForResult());
  EXPECT_FALSE(first_init_callback.have_result());

  EXPECT_EQ(kResponseBodySize, element_reader_.GetContentLength());
  EXPECT_EQ(kResponseBodySize, element_reader_.BytesRemaining());
  EXPECT_FALSE(element_reader_.IsInMemory());

  // Try to read from the body.
  auto io_buffer = base::MakeRefCounted<net::IOBufferWithSize>(10);
  net::TestCompletionCallback read_callback;
  EXPECT_EQ(net::ERR_IO_PENDING,
            element_reader_.Read(io_buffer.get(), io_buffer->size(),
                                 read_callback.callback()));

  // Writes to the first write pipe should either fail, or succeed but be
  // ignored.
  mojo::BlockingCopyFromString("foo", first_write_pipe);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(read_callback.have_result());
}

// Test the case where a second Init() call occurs when there's a pending Read()
// call in progress. The old Read() should be dropped, in favor of the new
// Init().
TEST_F(DataPipeElementReaderTest, InitInterruptsRead) {
  // Value deliberately outside of the range of an uint32_t, to catch any
  // accidental conversions to an int.
  const uint64_t kResponseBodySize = std::numeric_limits<uint32_t>::max();

  // The network stack calls Init.
  net::TestCompletionCallback first_init_callback;
  EXPECT_EQ(net::ERR_IO_PENDING,
            element_reader_.Init(first_init_callback.callback()));

  // Wait for DataPipeGetter::Read() to be called.
  mojo::ScopedDataPipeProducerHandle first_write_pipe;
  network::mojom::DataPipeGetter::ReadCallback first_read_pipe_callback;
  data_pipe_getter_.WaitForRead(&first_write_pipe, &first_read_pipe_callback);
  std::move(first_read_pipe_callback).Run(net::OK, kResponseBodySize);

  ASSERT_EQ(net::OK, first_init_callback.WaitForResult());

  auto first_io_buffer = base::MakeRefCounted<net::IOBufferWithSize>(10);
  net::TestCompletionCallback first_read_callback;
  EXPECT_EQ(net::ERR_IO_PENDING,
            element_reader_.Read(first_io_buffer.get(), first_io_buffer->size(),
                                 first_read_callback.callback()));

  // The network stack calls Init again, interrupting the previous read.
  net::TestCompletionCallback second_init_callback;
  EXPECT_EQ(net::ERR_IO_PENDING,
            element_reader_.Init(second_init_callback.callback()));

  // Wait for DataPipeGetter::Read() to be called again.
  mojo::ScopedDataPipeProducerHandle second_write_pipe;
  network::mojom::DataPipeGetter::ReadCallback second_read_pipe_callback;
  data_pipe_getter_.WaitForRead(&second_write_pipe, &second_read_pipe_callback);

  // Run any pending tasks, to make sure nothing unexpected is queued.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(first_read_callback.have_result());
  EXPECT_FALSE(second_init_callback.have_result());

  // Sending data on the second pipe should result in the second init callback
  // being invoked.
  std::move(second_read_pipe_callback).Run(net::OK, kResponseBodySize);
  EXPECT_EQ(net::OK, second_init_callback.WaitForResult());

  EXPECT_EQ(kResponseBodySize, element_reader_.GetContentLength());
  EXPECT_EQ(kResponseBodySize, element_reader_.BytesRemaining());
  EXPECT_FALSE(element_reader_.IsInMemory());

  // Try to read from the body.
  auto io_buffer = base::MakeRefCounted<net::IOBufferWithSize>(10);
  net::TestCompletionCallback second_read_callback;
  EXPECT_EQ(net::ERR_IO_PENDING,
            element_reader_.Read(io_buffer.get(), io_buffer->size(),
                                 second_read_callback.callback()));

  // Writes to the first write pipe should either fail, or succeed but be
  // ignored.
  mojo::BlockingCopyFromString("foo", first_write_pipe);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(second_read_callback.have_result());
}

}  // namespace

}  // namespace network
