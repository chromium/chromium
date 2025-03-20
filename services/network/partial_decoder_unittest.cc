// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/partial_decoder.h"

#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "net/base/io_buffer.h"
#include "net/base/mime_sniffer.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/filter/filter_source_stream_test_util.h"
#include "services/network/pending_callback_chain.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {

using testing::_;

namespace {

static constexpr std::string_view kTestBody = "hello world";

class MockReader {
 public:
  MockReader() = default;
  ~MockReader() = default;

  base::RepeatingCallback<int(net::IOBuffer*, int)> AsReadCallback() {
    return base::BindRepeating(
        [](base::WeakPtr<MockReader> self, net::IOBuffer* dest, int dest_size) {
          CHECK(self);
          return self->Read(dest, dest_size);
        },
        weak_ptr_factory_.GetWeakPtr());
  }

  MOCK_METHOD(int, Read, (net::IOBuffer * dest, int dest_size));

 private:
  base::WeakPtrFactory<MockReader> weak_ptr_factory_{this};
};

void CheckResult(std::unique_ptr<PartialDecoder> partial_decoder,
                 std::string_view expected_decoded_data,
                 base::span<const uint8_t> expected_compressed_data,
                 std::optional<net::Error> expected_completion_status) {
  EXPECT_THAT(base::as_string_view(partial_decoder->decoded_data()),
              expected_decoded_data);

  auto result = std::move(*partial_decoder).TakeResult();
  EXPECT_EQ(result.HasRawData(), !expected_decoded_data.empty());
  std::vector<uint8_t> raw_bytes_out(expected_compressed_data.size());
  EXPECT_EQ(result.ConsumeRawData(raw_bytes_out),
            expected_compressed_data.size());
  EXPECT_THAT(raw_bytes_out, expected_compressed_data);
  EXPECT_THAT(result.completion_status(), expected_completion_status);
}

void CallOnReadRawDataCompletedAsync(
    std::unique_ptr<PartialDecoder>& partial_decoder,
    int result) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(
                     [](PartialDecoder* partial_decoder, int result) {
                       partial_decoder->OnReadRawDataCompleted(result);
                     },
                     base::Unretained(partial_decoder.get()), result));
}

}  // namespace

// Test fixture for PartialDecoder tests.
class PartialDecoderTest : public testing::Test {
 public:
  PartialDecoderTest() = default;
  ~PartialDecoderTest() override = default;

 protected:
  // Provides a task environment for the tests.
  base::test::TaskEnvironment task_environment_;
};

TEST_F(PartialDecoderTest, GzipSimpleSyncRead) {
  const auto compressed = net::CompressGzip(kTestBody);
  MockReader reader;
  EXPECT_CALL(reader, Read(_, _))
      .WillOnce([&](net::IOBuffer* dest, int dest_size) {
        CHECK_GE(dest_size, base::checked_cast<int>(compressed.size()));
        dest->first(compressed.size()).copy_from(compressed);
        return compressed.size();
      });

  auto partial_decoder = std::make_unique<PartialDecoder>(
      reader.AsReadCallback(),
      std::vector<net::SourceStreamType>({net::SourceStreamType::kGzip}),
      net::kMaxBytesToSniff);
  EXPECT_THAT(partial_decoder->ReadDecodedDataMore(
                  base::BindLambdaForTesting([](int) { FAIL(); })),
              kTestBody.size());
  EXPECT_FALSE(partial_decoder->read_in_progress());
  CheckResult(std::move(partial_decoder), kTestBody, base::span(compressed),
              /*expected_completion_status=*/std::nullopt);
}

TEST_F(PartialDecoderTest, GzipSimpleAsyncRead) {
  const auto compressed = net::CompressGzip(kTestBody);
  MockReader reader;
  std::unique_ptr<PartialDecoder> partial_decoder;
  EXPECT_CALL(reader, Read(_, _))
      .WillOnce([&](net::IOBuffer* dest, int dest_size) {
        CHECK_GE(dest_size, base::checked_cast<int>(compressed.size()));
        dest->first(compressed.size()).copy_from(compressed);
        CallOnReadRawDataCompletedAsync(partial_decoder, compressed.size());
        return net::ERR_IO_PENDING;
      });

  partial_decoder = std::make_unique<PartialDecoder>(
      reader.AsReadCallback(),
      std::vector<net::SourceStreamType>({net::SourceStreamType::kGzip}),
      net::kMaxBytesToSniff);
  net::TestCompletionCallback callback;
  EXPECT_EQ(partial_decoder->ReadDecodedDataMore(callback.callback()),
            net::ERR_IO_PENDING);
  EXPECT_TRUE(partial_decoder->read_in_progress());
  EXPECT_THAT(callback.WaitForResult(), kTestBody.size());
  CheckResult(std::move(partial_decoder), kTestBody, base::span(compressed),
              /*expected_completion_status=*/std::nullopt);
}

TEST_F(PartialDecoderTest, GzipSyncTwoReads) {
  const auto compressed = net::CompressGzip(kTestBody);
  auto [first_chunk, second_chunk] =
      base::span(compressed).split_at(compressed.size() / 2);
  MockReader reader;
  EXPECT_CALL(reader, Read(_, _))
      .WillOnce([&](net::IOBuffer* dest, int dest_size) {
        CHECK_GE(dest_size, base::checked_cast<int>(first_chunk.size()));
        dest->first(first_chunk.size()).copy_from(first_chunk);
        return first_chunk.size();
      })
      .WillOnce([&](net::IOBuffer* dest, int dest_size) {
        CHECK_GE(dest_size, base::checked_cast<int>(first_chunk.size()));
        dest->first(second_chunk.size()).copy_from(second_chunk);
        return second_chunk.size();
      });

  auto partial_decoder = std::make_unique<PartialDecoder>(
      reader.AsReadCallback(),
      std::vector<net::SourceStreamType>({net::SourceStreamType::kGzip}),
      net::kMaxBytesToSniff);
  EXPECT_THAT(partial_decoder->ReadDecodedDataMore(
                  base::BindLambdaForTesting([](int) { FAIL(); })),
              kTestBody.size());
  EXPECT_FALSE(partial_decoder->read_in_progress());
  CheckResult(std::move(partial_decoder), kTestBody, base::span(compressed),
              /*expected_completion_status=*/std::nullopt);
}

TEST_F(PartialDecoderTest, GzipAsyncTwoReads) {
  const auto compressed = net::CompressGzip(kTestBody);
  auto [first_chunk, second_chunk] =
      base::span(compressed).split_at(compressed.size() / 2);

  MockReader reader;
  std::unique_ptr<PartialDecoder> partial_decoder;
  EXPECT_CALL(reader, Read(_, _))
      .WillOnce([&](net::IOBuffer* dest, int dest_size) {
        CHECK_GE(dest_size, base::checked_cast<int>(first_chunk.size()));
        dest->first(first_chunk.size()).copy_from(first_chunk);
        CallOnReadRawDataCompletedAsync(partial_decoder, first_chunk.size());
        return net::ERR_IO_PENDING;
      })
      .WillOnce([&](net::IOBuffer* dest, int dest_size) {
        CHECK_GE(dest_size, base::checked_cast<int>(second_chunk.size()));
        dest->first(second_chunk.size()).copy_from(second_chunk);
        CallOnReadRawDataCompletedAsync(partial_decoder, second_chunk.size());
        return net::ERR_IO_PENDING;
      });

  partial_decoder = std::make_unique<PartialDecoder>(
      reader.AsReadCallback(),
      std::vector<net::SourceStreamType>({net::SourceStreamType::kGzip}),
      net::kMaxBytesToSniff);
  net::TestCompletionCallback callback;
  EXPECT_EQ(partial_decoder->ReadDecodedDataMore(callback.callback()),
            net::ERR_IO_PENDING);
  EXPECT_TRUE(partial_decoder->read_in_progress());
  EXPECT_THAT(callback.WaitForResult(), kTestBody.size());
  CheckResult(std::move(partial_decoder), kTestBody, base::span(compressed),
              /*expected_completion_status=*/std::nullopt);
}

TEST_F(PartialDecoderTest, SyncError) {
  MockReader reader;
  EXPECT_CALL(reader, Read(_, _))
      .WillOnce(
          [&](net::IOBuffer* dest, int dest_size) { return net::ERR_FAILED; });

  auto partial_decoder = std::make_unique<PartialDecoder>(
      reader.AsReadCallback(),
      std::vector<net::SourceStreamType>({net::SourceStreamType::kGzip}),
      net::kMaxBytesToSniff);
  EXPECT_THAT(partial_decoder->ReadDecodedDataMore(
                  base::BindLambdaForTesting([](int) { FAIL(); })),
              net::ERR_FAILED);
  EXPECT_FALSE(partial_decoder->read_in_progress());
  CheckResult(std::move(partial_decoder), /*expected_decoded_data=*/"",
              /*expected_compressed_data=*/{}, net::ERR_FAILED);
}

TEST_F(PartialDecoderTest, AsyncError) {
  MockReader reader;
  std::unique_ptr<PartialDecoder> partial_decoder;
  EXPECT_CALL(reader, Read(_, _))
      .WillOnce([&](net::IOBuffer* dest, int dest_size) {
        CallOnReadRawDataCompletedAsync(partial_decoder, net::ERR_FAILED);
        return net::ERR_IO_PENDING;
      });

  partial_decoder = std::make_unique<PartialDecoder>(
      reader.AsReadCallback(),
      std::vector<net::SourceStreamType>({net::SourceStreamType::kGzip}),
      net::kMaxBytesToSniff);
  net::TestCompletionCallback callback;
  EXPECT_EQ(partial_decoder->ReadDecodedDataMore(callback.callback()),
            net::ERR_IO_PENDING);
  EXPECT_TRUE(partial_decoder->read_in_progress());
  EXPECT_THAT(callback.WaitForResult(), net::ERR_FAILED);
  EXPECT_FALSE(partial_decoder->read_in_progress());
  CheckResult(std::move(partial_decoder), /*expected_decoded_data=*/"",
              /*expected_compressed_data=*/{}, net::ERR_FAILED);
}

TEST_F(PartialDecoderTest, NoDecoding) {
  MockReader reader;
  EXPECT_CALL(reader, Read(_, _))
      .WillOnce([&](net::IOBuffer* dest, int dest_size) {
        CHECK_GE(dest_size, base::checked_cast<int>(kTestBody.size()));
        dest->first(kTestBody.size())
            .copy_from(base::as_bytes(base::span(kTestBody)));
        return kTestBody.size();
      })
      .WillOnce([&](net::IOBuffer* dest, int dest_size) { return net::OK; });

  auto partial_decoder = std::make_unique<PartialDecoder>(
      reader.AsReadCallback(),
      // No decoding types specified.
      std::vector<net::SourceStreamType>({}), net::kMaxBytesToSniff);
  EXPECT_THAT(partial_decoder->ReadDecodedDataMore(
                  base::BindLambdaForTesting([](int) { FAIL(); })),
              kTestBody.size());
  EXPECT_FALSE(partial_decoder->read_in_progress());

  // Read more.
  EXPECT_THAT(partial_decoder->ReadDecodedDataMore(
                  base::BindLambdaForTesting([](int) { FAIL(); })),
              net::OK);
  CheckResult(std::move(partial_decoder), kTestBody,
              base::as_bytes(base::span(kTestBody)), net::OK);
}

TEST_F(PartialDecoderTest, GzipPartialRead) {
  const auto compressed = net::CompressGzip(kTestBody);
  // Only read half of the compressed data.
  static constexpr size_t kReadSize = kTestBody.size() / 2;

  MockReader reader;
  EXPECT_CALL(reader, Read(_, _))
      .WillOnce([&](net::IOBuffer* dest, int dest_size) {
        CHECK_GE(dest_size, base::checked_cast<int>(compressed.size()));
        dest->first(compressed.size()).copy_from(compressed);
        return compressed.size();
      });

  auto partial_decoder = std::make_unique<PartialDecoder>(
      reader.AsReadCallback(),
      std::vector<net::SourceStreamType>({net::SourceStreamType::kGzip}),
      kReadSize);
  EXPECT_THAT(partial_decoder->ReadDecodedDataMore(
                  base::BindLambdaForTesting([](int) { FAIL(); })),
              kReadSize);
  EXPECT_FALSE(partial_decoder->read_in_progress());
  CheckResult(std::move(partial_decoder), kTestBody.substr(0u, kReadSize),
              base::span(compressed),
              /*expected_completion_status=*/std::nullopt);
}

TEST_F(PartialDecoderTest, NoDecodingPartialRead) {
  // Use a small decoded buffer size to force partial reading.
  const size_t kDecodedBufferSize = 5;
  ASSERT_TRUE(kDecodedBufferSize < kTestBody.size())
      << "decoded buffer size should be smaller than test body size";

  MockReader reader;
  EXPECT_CALL(reader, Read(_, _))
      .WillOnce([&](net::IOBuffer* dest, int dest_size) {
        EXPECT_THAT(dest_size, kDecodedBufferSize);
        dest->first(kDecodedBufferSize)
            .copy_from(base::as_bytes(
                base::span(kTestBody).first(kDecodedBufferSize)));
        return kDecodedBufferSize;
      });

  auto partial_decoder = std::make_unique<PartialDecoder>(
      reader.AsReadCallback(),
      // No decoding types specified.
      std::vector<net::SourceStreamType>({}), kDecodedBufferSize);
  partial_decoder = std::make_unique<PartialDecoder>(
      reader.AsReadCallback(), std::vector<net::SourceStreamType>({}),
      kDecodedBufferSize);
  EXPECT_THAT(partial_decoder->ReadDecodedDataMore(
                  base::BindLambdaForTesting([](int) { FAIL(); })),
              kDecodedBufferSize);
  EXPECT_FALSE(partial_decoder->read_in_progress());
  CheckResult(std::move(partial_decoder),
              kTestBody.substr(0u, kDecodedBufferSize),
              base::as_bytes(base::span(kTestBody)).first(kDecodedBufferSize),
              /*expected_completion_status=*/std::nullopt);
}

TEST_F(PartialDecoderTest, ConsumeRawDataInChunks) {
  const auto compressed = net::CompressGzip(kTestBody);
  auto [first_chunk, second_chunk] =
      base::span(compressed).split_at(compressed.size() / 2);
  MockReader reader;
  EXPECT_CALL(reader, Read(_, _))
      .WillOnce([&](net::IOBuffer* dest, int dest_size) {
        CHECK_GE(dest_size, base::checked_cast<int>(first_chunk.size()));
        dest->first(first_chunk.size()).copy_from(first_chunk);
        return first_chunk.size();
      })
      .WillOnce([&](net::IOBuffer* dest, int dest_size) {
        CHECK_GE(dest_size, base::checked_cast<int>(first_chunk.size()));
        dest->first(second_chunk.size()).copy_from(second_chunk);
        return second_chunk.size();
      });

  auto partial_decoder = std::make_unique<PartialDecoder>(
      reader.AsReadCallback(),
      std::vector<net::SourceStreamType>({net::SourceStreamType::kGzip}),
      net::kMaxBytesToSniff);
  EXPECT_THAT(partial_decoder->ReadDecodedDataMore(
                  base::BindLambdaForTesting([](int) { FAIL(); })),
              kTestBody.size());
  auto raw_data = std::move(*partial_decoder).TakeResult();
  EXPECT_TRUE(raw_data.HasRawData());

  // Consume raw data in chunks.
  const int kChunkSize = 4;
  std::vector<uint8_t> raw_bytes_out;
  size_t total_consumed = 0;
  while (total_consumed < compressed.size()) {
    std::vector<uint8_t> chunk(kChunkSize);
    size_t consumed = raw_data.ConsumeRawData(chunk);
    EXPECT_GT(consumed, 0u);
    raw_bytes_out.insert(raw_bytes_out.end(), chunk.begin(),
                         chunk.begin() + consumed);
    total_consumed += consumed;
  }

  EXPECT_EQ(total_consumed, compressed.size());
  EXPECT_THAT(raw_bytes_out, compressed);

  // Verify that there is no more raw data.
  EXPECT_FALSE(raw_data.HasRawData());
  std::vector<uint8_t> chunk(kChunkSize);
  EXPECT_EQ(raw_data.ConsumeRawData(chunk), 0u);
}

}  // namespace network
