// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/common_decoder.h"

#include <stddef.h>
#include <stdint.h>

#include <array>
#include <limits>
#include <memory>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "gpu/command_buffer/client/client_test_helper.h"
#include "gpu/command_buffer/service/mocks.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gpu {

TEST(CommonDecoderBucket, Basic) {
  CommonDecoder::Bucket bucket;
  EXPECT_EQ(0u, bucket.size());
  EXPECT_TRUE(bucket.GetDataAsByteSpan(0, 0).empty());
}

TEST(CommonDecoderBucket, Size) {
  CommonDecoder::Bucket bucket;
  bucket.SetSize(24);
  EXPECT_EQ(24u, bucket.size());
  bucket.SetSize(12);
  EXPECT_EQ(12u, bucket.size());
}

TEST(CommonDecoderBucket, GetDataAsByteSpan) {
  CommonDecoder::Bucket bucket;

  bucket.SetSize(24);
  // In-range requests return a span of exactly the requested size.
  EXPECT_EQ(24u, bucket.GetDataAsByteSpan(0, 24).size());
  EXPECT_EQ(8u, bucket.GetDataAsByteSpan(16, 8).size());

  // A zero-sized request yields an empty span, so it is indistinguishable from
  // an out-of-range request.
  EXPECT_TRUE(bucket.GetDataAsByteSpan(0, 0).empty());
  EXPECT_TRUE(bucket.GetDataAsByteSpan(24, 0).empty());

  // Out-of-range requests return an empty span.
  EXPECT_TRUE(bucket.GetDataAsByteSpan(25, 0).empty());
  EXPECT_TRUE(bucket.GetDataAsByteSpan(0, 25).empty());
  EXPECT_TRUE(bucket.GetDataAsByteSpan(16, 9).empty());

  // Requests whose offset + size overflows size_t are rejected as well.
  constexpr size_t kMaxSize = std::numeric_limits<size_t>::max();
  EXPECT_TRUE(bucket.GetDataAsByteSpan(1, kMaxSize).empty());
  EXPECT_TRUE(bucket.GetDataAsByteSpan(kMaxSize, 1).empty());
  EXPECT_TRUE(bucket.GetDataAsByteSpan(kMaxSize, kMaxSize).empty());

  bucket.SetSize(23);
  EXPECT_TRUE(bucket.GetDataAsByteSpan(0, 24).empty());
}

TEST(CommonDecoderBucket, SetData) {
  CommonDecoder::Bucket bucket;
  static const char data[] = "testing";

  bucket.SetSize(10);
  EXPECT_TRUE(bucket.SetData(data, 0, sizeof(data)));
  EXPECT_EQ(bucket.GetDataAsByteSpan(0, sizeof(data)),
            base::as_byte_span(data));
  EXPECT_TRUE(bucket.SetData(data, 2, sizeof(data)));
  EXPECT_EQ(bucket.GetDataAsByteSpan(2, sizeof(data)),
            base::as_byte_span(data));
  EXPECT_FALSE(bucket.SetData(data, 0, sizeof(data) * 2));
  EXPECT_FALSE(bucket.SetData(data, 5, sizeof(data)));
}

class TestCommonDecoder : public CommonDecoder {
 public:
  explicit TestCommonDecoder(DecoderClient* client,
                             CommandBufferServiceBase* command_buffer_service)
      : CommonDecoder(client, command_buffer_service) {}
  error::Error DoCommand(unsigned int command,
                         unsigned int arg_count,
                         const volatile void* cmd_data) {
    return DoCommonCommand(command, arg_count, cmd_data);
  }

  CommonDecoder::Bucket* GetBucket(uint32_t id) const {
    return CommonDecoder::GetBucket(id);
  }
};

class CommonDecoderTest : public testing::Test {
 protected:
  static const size_t kBufferSize = 1024;
  static const uint32_t kInvalidShmId = UINT32_MAX;
  CommonDecoderTest() : decoder_(&client_, &command_buffer_service_) {}

  void SetUp() override {
    command_buffer_service_.CreateTransferBufferHelper(kBufferSize,
                                                       &valid_shm_id_);
  }

  void TearDown() override {}

  template <typename T>
  error::Error ExecuteCmd(const T& cmd) {
    static_assert(T::kArgFlags == cmd::kFixed,
                  "T::kArgFlags should equal cmd::kFixed");
    return decoder_.DoCommand(cmd.header.command, cmd.header.size - 1, &cmd);
  }

  template <typename T>
  error::Error ExecuteImmediateCmd(const T& cmd, size_t data_size) {
    static_assert(T::kArgFlags == cmd::kAtLeastN,
                  "T::kArgFlags should equal cmd::kAtLeastN");
    return decoder_.DoCommand(cmd.header.command, cmd.header.size - 1, &cmd);
  }

  template <typename T>
  T GetSharedMemoryAs(size_t offset) {
    void* memory =
        command_buffer_service_.GetTransferBuffer(valid_shm_id_)->memory();
    return reinterpret_cast<T>(
        UNSAFE_TODO(static_cast<uint8_t*>(memory) + offset));
  }

  FakeCommandBufferServiceBase command_buffer_service_;
  FakeDecoderClient client_;
  TestCommonDecoder decoder_;
  int32_t valid_shm_id_ = 0;
};

const size_t CommonDecoderTest::kBufferSize;
const uint32_t CommonDecoderTest::kInvalidShmId;

TEST_F(CommonDecoderTest, DoCommonCommandInvalidCommand) {
  EXPECT_EQ(error::kUnknownCommand, decoder_.DoCommand(999999, 0, nullptr));
}

TEST_F(CommonDecoderTest, HandleNoop) {
  cmd::Noop cmd;
  const uint32_t kSkipCount = 5;
  cmd.Init(kSkipCount);
  EXPECT_EQ(error::kNoError,
            ExecuteImmediateCmd(
                cmd, kSkipCount * kCommandBufferEntrySize));
  const uint32_t kSkipCount2 = 1;
  cmd.Init(kSkipCount2);
  EXPECT_EQ(error::kNoError,
            ExecuteImmediateCmd(
                cmd, kSkipCount2 * kCommandBufferEntrySize));
}

TEST_F(CommonDecoderTest, SetToken) {
  cmd::SetToken cmd;
  const int32_t kTokenId = 123;
  command_buffer_service_.SetToken(0);
  cmd.Init(kTokenId);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(kTokenId, command_buffer_service_.GetState().token);
}

TEST_F(CommonDecoderTest, SetBucketSize) {
  cmd::SetBucketSize cmd;
  const uint32_t kBucketId = 123;
  const uint32_t kBucketLength1 = 1234;
  const uint32_t kBucketLength2 = 78;
  // Check the bucket does not exist.
  EXPECT_TRUE(nullptr == decoder_.GetBucket(kBucketId));
  // Check we can create one.
  cmd.Init(kBucketId, kBucketLength1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  CommonDecoder::Bucket* bucket;
  bucket = decoder_.GetBucket(kBucketId);
  EXPECT_TRUE(nullptr != bucket);
  EXPECT_EQ(kBucketLength1, bucket->size());
  // Check we can change it.
  cmd.Init(kBucketId, kBucketLength2);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  bucket = decoder_.GetBucket(kBucketId);
  EXPECT_TRUE(nullptr != bucket);
  EXPECT_EQ(kBucketLength2, bucket->size());
  // Check we can delete it.
  cmd.Init(kBucketId, 0);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  bucket = decoder_.GetBucket(kBucketId);
  EXPECT_EQ(0u, bucket->size());
}

TEST_F(CommonDecoderTest, SetBucketData) {
  cmd::SetBucketSize size_cmd;
  cmd::SetBucketData cmd;

  static const char kData[] = "1234567890123456789";

  const uint32_t kBucketId = 123;
  const uint32_t kInvalidBucketId = 124;

  size_cmd.Init(kBucketId, sizeof(kData));
  EXPECT_EQ(error::kNoError, ExecuteCmd(size_cmd));
  CommonDecoder::Bucket* bucket = decoder_.GetBucket(kBucketId);
  // Check the data is not there.
  EXPECT_NE(bucket->GetDataAsByteSpan(0, sizeof(kData)),
            base::as_byte_span(kData));

  // Check we can set it.
  const uint32_t kSomeOffsetInSharedMemory = 50;
  void* memory = GetSharedMemoryAs<void*>(kSomeOffsetInSharedMemory);
  UNSAFE_TODO(memcpy(memory, kData, sizeof(kData)));
  cmd.Init(kBucketId, 0, sizeof(kData), valid_shm_id_,
           kSomeOffsetInSharedMemory);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(bucket->GetDataAsByteSpan(0, sizeof(kData)),
            base::as_byte_span(kData));

  // Check we can set it partially.
  static const char kData2[] = "ABCEDFG";
  const uint32_t kSomeOffsetInBucket = 5;
  UNSAFE_TODO(memcpy(memory, kData2, sizeof(kData2)));
  cmd.Init(kBucketId, kSomeOffsetInBucket, sizeof(kData2), valid_shm_id_,
           kSomeOffsetInSharedMemory);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(bucket->GetDataAsByteSpan(kSomeOffsetInBucket, sizeof(kData2)),
            base::as_byte_span(kData2));
  base::span<const char> bucket_data =
      base::as_chars(bucket->GetDataAsByteSpan(0, sizeof(kData)));
  // Check that nothing was affected outside of updated area.
  EXPECT_EQ(kData[kSomeOffsetInBucket - 1],
            bucket_data[kSomeOffsetInBucket - 1]);
  EXPECT_EQ(kData[kSomeOffsetInBucket + sizeof(kData2)],
            bucket_data[kSomeOffsetInBucket + sizeof(kData2)]);

  // Check that it fails if the bucket_id is invalid
  cmd.Init(kInvalidBucketId, kSomeOffsetInBucket, sizeof(kData2), valid_shm_id_,
           kSomeOffsetInSharedMemory);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));

  // Check that it fails if the offset is out of range.
  cmd.Init(kBucketId, bucket->size(), 1, valid_shm_id_,
           kSomeOffsetInSharedMemory);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));

  // Check that it fails if the size is out of range.
  cmd.Init(kBucketId, 0, bucket->size() + 1, valid_shm_id_,
           kSomeOffsetInSharedMemory);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(CommonDecoderTest, SetBucketDataImmediate) {
  cmd::SetBucketSize size_cmd;
  std::array<int8_t, 1024> buffer;
  cmd::SetBucketDataImmediate& cmd =
      *reinterpret_cast<cmd::SetBucketDataImmediate*>(&buffer);

  static const char kData[] = "1234567890123456789";

  const uint32_t kBucketId = 123;
  const uint32_t kInvalidBucketId = 124;

  size_cmd.Init(kBucketId, sizeof(kData));
  EXPECT_EQ(error::kNoError, ExecuteCmd(size_cmd));
  CommonDecoder::Bucket* bucket = decoder_.GetBucket(kBucketId);
  // Check the data is not there.
  EXPECT_NE(bucket->GetDataAsByteSpan(0, sizeof(kData)),
            base::as_byte_span(kData));

  // Check we can set it.
  void* memory = UNSAFE_TODO(&buffer[0] + sizeof(cmd));
  UNSAFE_TODO(memcpy(memory, kData, sizeof(kData)));
  cmd.Init(kBucketId, 0, sizeof(kData));
  EXPECT_EQ(error::kNoError,
            ExecuteImmediateCmd(cmd, sizeof(kData)));
  EXPECT_EQ(bucket->GetDataAsByteSpan(0, sizeof(kData)),
            base::as_byte_span(kData));

  // Check we can set it partially.
  static const char kData2[] = "ABCEDFG";
  const uint32_t kSomeOffsetInBucket = 5;
  UNSAFE_TODO(memcpy(memory, kData2, sizeof(kData2)));
  cmd.Init(kBucketId, kSomeOffsetInBucket, sizeof(kData2));
  EXPECT_EQ(error::kNoError,
            ExecuteImmediateCmd(cmd, sizeof(kData2)));
  EXPECT_EQ(bucket->GetDataAsByteSpan(kSomeOffsetInBucket, sizeof(kData2)),
            base::as_byte_span(kData2));
  base::span<const char> bucket_data =
      base::as_chars(bucket->GetDataAsByteSpan(0, sizeof(kData)));
  // Check that nothing was affected outside of updated area.
  EXPECT_EQ(kData[kSomeOffsetInBucket - 1],
            bucket_data[kSomeOffsetInBucket - 1]);
  EXPECT_EQ(kData[kSomeOffsetInBucket + sizeof(kData2)],
            bucket_data[kSomeOffsetInBucket + sizeof(kData2)]);

  // Check that it fails if the bucket_id is invalid
  cmd.Init(kInvalidBucketId, kSomeOffsetInBucket, sizeof(kData2));
  EXPECT_NE(error::kNoError,
            ExecuteImmediateCmd(cmd, sizeof(kData2)));

  // Check that it fails if the offset is out of range.
  cmd.Init(kBucketId, bucket->size(), 1);
  EXPECT_NE(error::kNoError,
            ExecuteImmediateCmd(cmd, sizeof(kData2)));

  // Check that it fails if the size is out of range.
  size_cmd.Init(kBucketId, sizeof(kData2));
  EXPECT_EQ(error::kNoError, ExecuteCmd(size_cmd));
  cmd.Init(kBucketId, 0, bucket->size() + 1);
  EXPECT_NE(error::kNoError, ExecuteImmediateCmd(cmd, sizeof(kData)));
}

namespace {

uint32_t LoadU32Unaligned(const void* ptr) {
  uint32_t ret;
  UNSAFE_TODO(memcpy(&ret, ptr, sizeof(uint32_t)));
  return ret;
}

void StoreU32Unaligned(uint32_t v, void* ptr) {
  UNSAFE_TODO(memcpy(ptr, &v, sizeof(uint32_t)));
}

}  // namespace

TEST_F(CommonDecoderTest, GetBucketStart) {
  cmd::SetBucketSize size_cmd;
  cmd::SetBucketData set_cmd;
  cmd::GetBucketStart cmd;

  static const char kData[] = "1234567890123456789";
  static const char zero[sizeof(kData)] = { 0, };

  const uint32_t kBucketSize = sizeof(kData);
  const uint32_t kBucketId = 123;
  const uint32_t kInvalidBucketId = 124;

  // Put data in the bucket.
  size_cmd.Init(kBucketId, sizeof(kData));
  EXPECT_EQ(error::kNoError, ExecuteCmd(size_cmd));
  const uint32_t kSomeOffsetInSharedMemory = 50;
  uint8_t* start = GetSharedMemoryAs<uint8_t*>(kSomeOffsetInSharedMemory);
  UNSAFE_TODO(memcpy(start, kData, sizeof(kData)));
  set_cmd.Init(kBucketId, 0, sizeof(kData), valid_shm_id_,
               kSomeOffsetInSharedMemory);
  EXPECT_EQ(error::kNoError, ExecuteCmd(set_cmd));

  // Check that the size is correct with no data buffer.
  void* memory = GetSharedMemoryAs<void*>(kSomeOffsetInSharedMemory);
  StoreU32Unaligned(0, memory);
  cmd.Init(kBucketId, valid_shm_id_, kSomeOffsetInSharedMemory, 0, 0, 0);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(kBucketSize, LoadU32Unaligned(memory));

  // Check that the data is copied with data buffer.
  const uint32_t kDataOffsetInSharedMemory = 54;
  uint8_t* data = GetSharedMemoryAs<uint8_t*>(kDataOffsetInSharedMemory);
  StoreU32Unaligned(0, memory);
  UNSAFE_TODO(memset(data, 0, sizeof(kData)));
  cmd.Init(kBucketId, valid_shm_id_, kSomeOffsetInSharedMemory, kBucketSize,
           valid_shm_id_, kDataOffsetInSharedMemory);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(kBucketSize, LoadU32Unaligned(memory));
  UNSAFE_TODO(EXPECT_EQ(0, memcmp(data, kData, kBucketSize)));

  // Check that we can get a piece.
  StoreU32Unaligned(0, memory);
  UNSAFE_TODO(memset(data, 0, sizeof(kData)));
  const uint32_t kPieceSize = kBucketSize / 2;
  cmd.Init(kBucketId, valid_shm_id_, kSomeOffsetInSharedMemory, kPieceSize,
           valid_shm_id_, kDataOffsetInSharedMemory);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(kBucketSize, LoadU32Unaligned(memory));
  UNSAFE_TODO(EXPECT_EQ(0, memcmp(data, kData, kPieceSize)));
  UNSAFE_TODO(EXPECT_EQ(
      0, memcmp(data + kPieceSize, zero, sizeof(kData) - kPieceSize)));

  // Check that it fails if the result_id is invalid
  cmd.Init(kInvalidBucketId, valid_shm_id_, kSomeOffsetInSharedMemory, 0, 0, 0);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));

  // Check that it fails if the data_id is invalid
  cmd.Init(kBucketId, valid_shm_id_, kSomeOffsetInSharedMemory, 1,
           CommonDecoderTest::kInvalidShmId, 0);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));

  // Check that it fails if the data_size is invalid
  cmd.Init(kBucketId, valid_shm_id_, kSomeOffsetInSharedMemory, 1, 0, 0);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
  cmd.Init(kBucketId, valid_shm_id_, kSomeOffsetInSharedMemory,
           CommonDecoderTest::kBufferSize + 1, valid_shm_id_, 0);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));

  // Check that it fails if the data_offset is invalid
  cmd.Init(kBucketId, valid_shm_id_, kSomeOffsetInSharedMemory, 0, 0, 1);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
  cmd.Init(kBucketId, valid_shm_id_, kSomeOffsetInSharedMemory,
           CommonDecoderTest::kBufferSize, valid_shm_id_, 1);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));

  // Check that it fails if the result size is not set to zero
  StoreU32Unaligned(0x1, memory);
  cmd.Init(kBucketId, valid_shm_id_, kSomeOffsetInSharedMemory, 0, 0, 0);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(CommonDecoderTest, GetBucketData) {
  cmd::SetBucketSize size_cmd;
  cmd::SetBucketData set_cmd;
  cmd::GetBucketData cmd;

  static const char kData[] = "1234567890123456789";
  static const char zero[sizeof(kData)] = { 0, };

  const uint32_t kBucketId = 123;
  const uint32_t kInvalidBucketId = 124;

  size_cmd.Init(kBucketId, sizeof(kData));
  EXPECT_EQ(error::kNoError, ExecuteCmd(size_cmd));
  const uint32_t kSomeOffsetInSharedMemory = 50;
  uint8_t* memory = GetSharedMemoryAs<uint8_t*>(kSomeOffsetInSharedMemory);
  UNSAFE_TODO(memcpy(memory, kData, sizeof(kData)));
  set_cmd.Init(kBucketId, 0, sizeof(kData), valid_shm_id_,
               kSomeOffsetInSharedMemory);
  EXPECT_EQ(error::kNoError, ExecuteCmd(set_cmd));

  // Check we can get the whole thing.
  UNSAFE_TODO(memset(memory, 0, sizeof(kData)));
  cmd.Init(kBucketId, 0, sizeof(kData), valid_shm_id_,
           kSomeOffsetInSharedMemory);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  UNSAFE_TODO(EXPECT_EQ(0, memcmp(memory, kData, sizeof(kData))));

  // Check we can get a piece.
  const uint32_t kSomeOffsetInBucket = 5;
  const uint32_t kLengthOfPiece = 6;
  const uint8_t kSentinel = 0xff;
  UNSAFE_TODO(memset(memory, 0, sizeof(kData)));
  UNSAFE_TODO(memory[-1]) = kSentinel;
  cmd.Init(kBucketId, kSomeOffsetInBucket, kLengthOfPiece, valid_shm_id_,
           kSomeOffsetInSharedMemory);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  UNSAFE_TODO(EXPECT_EQ(
      0, memcmp(memory, kData + kSomeOffsetInBucket, kLengthOfPiece)));
  UNSAFE_TODO(EXPECT_EQ(0, memcmp(memory + kLengthOfPiece, zero,
                                  sizeof(kData) - kLengthOfPiece)));
  UNSAFE_TODO(EXPECT_EQ(kSentinel, memory[-1]));

  // Check that it fails if the bucket_id is invalid
  cmd.Init(kInvalidBucketId, kSomeOffsetInBucket, sizeof(kData), valid_shm_id_,
           kSomeOffsetInSharedMemory);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));

  // Check that it fails if the offset is invalid
  cmd.Init(kBucketId, sizeof(kData) + 1, 1, valid_shm_id_,
           kSomeOffsetInSharedMemory);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));

  // Check that it fails if the size is invalid
  cmd.Init(kBucketId, 0, sizeof(kData) + 1, valid_shm_id_,
           kSomeOffsetInSharedMemory);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(CommonDecoderTest, GetAsStrings_Success) {
  CommonDecoder::Bucket bucket;

  const size_t kBucketSize = 19;
  bucket.SetSize(kBucketSize);
  size_t write_offset = 0;

  const GLint count = 2;
  bucket.SetData(&count, write_offset, sizeof(count));
  write_offset += sizeof(count);

  const std::array<GLint, 2> sizes = {2, 3};
  bucket.SetData(&sizes, write_offset, sizeof(sizes));
  write_offset += sizeof(sizes);

  const std::array<char, 3> str0 = {'a', 'b', 0};
  bucket.SetData(&str0, write_offset, sizeof(str0));
  write_offset += sizeof(str0);

  const std::array<char, 4> str1 = {'x', 'y', 'z', 0};
  bucket.SetData(&str1, write_offset, sizeof(str1));
  write_offset += sizeof(str1);

  EXPECT_EQ(write_offset, kBucketSize);

  GLsizei count_out;
  std::vector<char*> strings_out;
  std::vector<GLint> lengths_out;
  EXPECT_TRUE(bucket.GetAsStrings(&count_out, &strings_out, &lengths_out));

  EXPECT_EQ(count_out, count);
  EXPECT_EQ(lengths_out.size(), size_t(count_out));
  EXPECT_EQ(lengths_out[0], sizes[0]);
  EXPECT_EQ(lengths_out[1], sizes[1]);
  EXPECT_EQ(std::string(str0.data()), std::string(strings_out[0]));
  EXPECT_EQ(std::string(str1.data()), std::string(strings_out[1]));
}

// Regression test for https://issues.chromium.org/487755344 where negative
// GLint sizes aren't validated out.
TEST_F(CommonDecoderTest, GetAsStrings_StringsSizeNegative) {
  CommonDecoder::Bucket bucket;
  bucket.SetSize(14);

  GLint count = 2;
  bucket.SetData(&count, 0, sizeof(count));
  GLint length0 = 1;
  bucket.SetData(&length0, 4, sizeof(length0));
  GLint length1 = -1;
  bucket.SetData(&length1, 8, sizeof(length1));
  std::array<uint8_t, 2> str = {'A', 0};
  bucket.SetData(&str, 12, sizeof(str));

  GLsizei count_out;
  std::vector<char*> strings_out;
  std::vector<GLint> lengths_out;
  EXPECT_FALSE(bucket.GetAsStrings(&count_out, &strings_out, &lengths_out));
}

// Test that GetAsStrings rejects strings that are not NUL-terminated.
TEST_F(CommonDecoderTest, GetAsStrings_MissingNulTerminator) {
  CommonDecoder::Bucket bucket;

  // Layout: count=1, length0=2, followed by 3 bytes of string data where the
  // byte at the expected NUL position is not zero.
  const size_t kBucketSize = sizeof(GLint) + sizeof(GLint) + 3;
  bucket.SetSize(kBucketSize);
  size_t write_offset = 0;

  const GLint count = 1;
  bucket.SetData(&count, write_offset, sizeof(count));
  write_offset += sizeof(count);

  const GLint length0 = 2;
  bucket.SetData(&length0, write_offset, sizeof(length0));
  write_offset += sizeof(length0);

  // "abc" instead of "ab\0", so the NUL terminator is missing.
  const std::array<char, 3> str0 = {'a', 'b', 'c'};
  bucket.SetData(&str0, write_offset, sizeof(str0));

  GLsizei count_out;
  std::vector<char*> strings_out;
  std::vector<GLint> lengths_out;
  EXPECT_FALSE(bucket.GetAsStrings(&count_out, &strings_out, &lengths_out));
}

}  // namespace gpu
