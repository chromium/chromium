// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/openscreen/decoder_buffer_reader.h"

#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "media/mojo/common/media_type_converters.h"
#include "media/mojo/common/mojo_data_pipe_read_write.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media::cast {
namespace {

constexpr uint32_t kDefaultDataPipeCapacityBytes = 512;

}  // namespace

class DecoderBufferReaderTest : public testing::Test {
 public:
  DecoderBufferReaderTest() {
    ResetDataPipes();

    serialized_data_.reserve(30);
    for (int i = 0; i < 30; i++) {
      serialized_data_.push_back(i);
    }

    // NOTE: This is INTENTIONALLY different from the above, to ensure that the
    // data() field is overwritten.
    uint8_t data[] = {42, 43, 44};
    populated_buffer_ = media::DecoderBuffer::CopyFrom(data);
  }

  ~DecoderBufferReaderTest() override = default;

 protected:
  void ResetDataPipes() {
    mojo::ScopedDataPipeProducerHandle producer_handle;
    mojo::ScopedDataPipeConsumerHandle consumer_handle;
    ASSERT_EQ(mojo::CreateDataPipe(kDefaultDataPipeCapacityBytes,
                                   producer_handle, consumer_handle),
              MOJO_RESULT_OK);

    writer_ =
        std::make_unique<media::MojoDataPipeWriter>(std::move(producer_handle));
    buffer_reader_ = std::make_unique<DecoderBufferReader>(
        base::BindRepeating(&DecoderBufferReaderTest::OnBufferReady,
                            base::Unretained(this)),
        std::move(consumer_handle));
  }

  void WriteBufferData() {
    writer_->Write(serialized_data_.data(), serialized_data_.size(),
                   base::BindOnce(&DecoderBufferReaderTest::OnWriteDone,
                                  base::Unretained(this)));
  }

  void OnWriteDone(bool succeeded) {
    ASSERT_TRUE(succeeded);
    has_write_completed_ = true;
  }

  void ProvideBuffer() {
    auto buffer = media::mojom::DecoderBuffer::From(*populated_buffer_);

    // Set the data size to that of the new data so it will be properly
    // deserialized.
    buffer->data_size = serialized_data_.size();
    buffer_reader_->ProvideBuffer(std::move(buffer));
  }

  // DecoderBufferReader::Client overrides.
  void OnBufferReady(scoped_refptr<media::DecoderBuffer> buffer) {
    EXPECT_TRUE(buffer->MatchesMetadataForTesting(*populated_buffer_));
    EXPECT_EQ(base::span(*buffer), base::span(serialized_data_));
    has_buffer_been_read_ = true;
  }

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  bool has_write_completed_ = false;
  bool has_buffer_been_read_ = false;

  std::vector<uint8_t> serialized_data_;
  scoped_refptr<media::DecoderBuffer> populated_buffer_;

  std::unique_ptr<media::MojoDataPipeWriter> writer_;
  std::unique_ptr<DecoderBufferReader> buffer_reader_;
};

TEST_F(DecoderBufferReaderTest, ProvideBufferCalledThenRead) {
  WriteBufferData();
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(has_write_completed_);
  EXPECT_FALSE(has_buffer_been_read_);

  buffer_reader_->ReadBufferAsync();
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(has_write_completed_);
  EXPECT_FALSE(has_buffer_been_read_);

  ProvideBuffer();
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(has_write_completed_);
  EXPECT_TRUE(has_buffer_been_read_);
}

TEST_F(DecoderBufferReaderTest, DataPipePopulatedThenRead) {
  ProvideBuffer();
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(has_write_completed_);
  EXPECT_FALSE(has_buffer_been_read_);

  buffer_reader_->ReadBufferAsync();
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(has_write_completed_);
  EXPECT_FALSE(has_buffer_been_read_);

  WriteBufferData();
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(has_write_completed_);
  EXPECT_TRUE(has_buffer_been_read_);
}

TEST_F(DecoderBufferReaderTest, ReadBufferAsyncCalledFirst) {
  buffer_reader_->ReadBufferAsync();
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(has_write_completed_);
  EXPECT_FALSE(has_buffer_been_read_);

  WriteBufferData();
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(has_write_completed_);
  EXPECT_FALSE(has_buffer_been_read_);

  ProvideBuffer();
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(has_write_completed_);
  EXPECT_TRUE(has_buffer_been_read_);
}

TEST_F(DecoderBufferReaderTest, AllDataPopulatedFirst) {
  WriteBufferData();
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(has_write_completed_);
  EXPECT_FALSE(has_buffer_been_read_);

  ProvideBuffer();
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(has_write_completed_);
  EXPECT_FALSE(has_buffer_been_read_);

  buffer_reader_->ReadBufferAsync();
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(has_write_completed_);
  EXPECT_TRUE(has_buffer_been_read_);
}

}  // namespace media::cast
