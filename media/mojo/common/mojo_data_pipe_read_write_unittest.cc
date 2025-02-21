// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/390223051): Remove C-library calls to fix the errors.
#pragma allow_unsafe_libc_calls
#endif

#include "media/mojo/common/mojo_data_pipe_read_write.h"

#include <stdint.h>

#include <memory>

#include "base/run_loop.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "media/base/decoder_buffer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

namespace {

uint32_t kDefaultDataPipeCapacityBytes = 512;

class MojoDataPipeReadWrite {
 public:
  MojoDataPipeReadWrite(
      uint32_t data_pipe_capacity_bytes = kDefaultDataPipeCapacityBytes) {
    mojo::ScopedDataPipeProducerHandle producer_handle;
    mojo::ScopedDataPipeConsumerHandle consumer_handle;
    CHECK_EQ(mojo::CreateDataPipe(data_pipe_capacity_bytes, producer_handle,
                                  consumer_handle),
             MOJO_RESULT_OK);

    writer_ = std::make_unique<MojoDataPipeWriter>(std::move(producer_handle));
    reader_ = std::make_unique<MojoDataPipeReader>(std::move(consumer_handle));
  }

  void WriteAndRead(base::span<const uint8_t> buffer,
                    bool discard_data = false) {
    base::RunLoop run_loop;
    base::MockCallback<MojoDataPipeWriter::DoneCB> mock_write_cb;
    base::MockCallback<MojoDataPipeReader::DoneCB> mock_read_cb;
    EXPECT_TRUE(reader_->IsPipeValid());
    EXPECT_TRUE(writer_->IsPipeValid());
    EXPECT_CALL(mock_write_cb, Run(true)).Times(1);
    EXPECT_CALL(mock_read_cb, Run(true)).Times(1);

    writer_->Write(buffer, mock_write_cb.Get());
    EXPECT_TRUE(read_buffer_.empty());
    if (discard_data) {
      reader_->Read(nullptr, buffer.size(), mock_read_cb.Get());
      run_loop.RunUntilIdle();
    } else {
      read_buffer_.resize(buffer.size());
      reader_->Read(read_buffer_.data(), buffer.size(), mock_read_cb.Get());
      run_loop.RunUntilIdle();
      EXPECT_EQ(0,
                std::memcmp(buffer.data(), read_buffer_.data(), buffer.size()));
      read_buffer_.clear();
    }
  }

  std::unique_ptr<MojoDataPipeWriter> writer_;
  std::unique_ptr<MojoDataPipeReader> reader_;
  std::vector<uint8_t> read_buffer_;
};

}  // namespace

TEST(MojoDataPipeReadWriteTest, Normal) {
  base::test::SingleThreadTaskEnvironment task_environment;
  std::string kData = "hello, world";
  MojoDataPipeReadWrite pipe_read_write_;
  pipe_read_write_.WriteAndRead(base::as_byte_span(kData));
}

TEST(MojoDataPipeReadWriteTest, SequentialReading) {
  base::test::SingleThreadTaskEnvironment task_environment;
  std::string kData1 = "hello, world";
  std::string kData2 = "Bye!";
  MojoDataPipeReadWrite pipe_read_write_;
  pipe_read_write_.WriteAndRead(base::as_byte_span(kData1));
  pipe_read_write_.WriteAndRead(base::as_byte_span(kData2));
}

TEST(MojoDataPipeReadWriteTest, LongerThanCapacity) {
  base::test::SingleThreadTaskEnvironment task_environment;
  std::string kData = "hello, world, hello, world, hello, world";
  MojoDataPipeReadWrite pipe_read_write_(10);
  pipe_read_write_.WriteAndRead(base::as_byte_span(kData));
}

TEST(MojoDataPipeReadWriteTest, DiscardDataInPipe) {
  base::test::SingleThreadTaskEnvironment task_environment;
  std::string kData1 = "to be discarded";
  std::string kData2 = "hello, world, hello, world, hello, world";
  MojoDataPipeReadWrite pipe_read_write_(10);
  pipe_read_write_.WriteAndRead(base::as_byte_span(kData1), true);
  pipe_read_write_.WriteAndRead(base::as_byte_span(kData2));
}

}  // namespace media
