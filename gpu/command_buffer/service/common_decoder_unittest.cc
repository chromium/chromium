// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "gpu/command_buffer/client/client_test_helper.h"
#include "gpu/command_buffer/service/common_decoder.h"
#include "gpu/command_buffer/service/mocks.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gpu {

TEST(CommonDecoderBucket, Basic) {
  CommonDecoder::Bucket bucket;
  EXPECT_EQ(0u, bucket.size());
  EXPECT_TRUE(nullptr == bucket.GetData(0, 0));
}

TEST(CommonDecoderBucket, Size) {
  CommonDecoder::Bucket bucket;
  bucket.SetSize(24);
  EXPECT_EQ(24u, bucket.size());
  bucket.SetSize(12);
  EXPECT_EQ(12u, bucket.size());
}

TEST(CommonDecoderBucket, GetData) {
  CommonDecoder::Bucket bucket;

  bucket.SetSize(24);
  EXPECT_TRUE(nullptr != bucket.GetData(0, 0));
  EXPECT_TRUE(nullptr != bucket.GetData(24, 0));
  EXPECT_TRUE(nullptr == bucket.GetData(25, 0));
  EXPECT_TRUE(nullptr != bucket.GetData(0, 24));
  EXPECT_TRUE(nullptr == bucket.GetData(0, 25));
  bucket.SetSize(23);
  EXPECT_TRUE(nullptr == bucket.GetData(0, 24));
}

TEST(CommonDecoderBucket, SetData) {
  CommonDecoder::Bucket bucket;
  static const char data[] = "testing";

  bucket.SetSize(10);
  EXPECT_TRUE(bucket.SetData(data, 0, sizeof(data)));
  EXPECT_EQ(0, memcmp(data, bucket.GetData(0, sizeof(data)), sizeof(data)));
  EXPECT_TRUE(bucket.SetData(data, 2, sizeof(data)));
  EXPECT_EQ(0, memcmp(data, bucket.GetData(2, sizeof(data)), sizeof(data)));
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
    return reinterpret_cast<T>(static_cast<uint8_t*>(memory) + offset);
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
  EXPECT_NE(0, memcmp(bucket->GetData(0, sizeof(kData)), kData, sizeof(kData)));

  // Check we can set it.
  const uint32_t kSomeOffsetInSharedMemory = 50;
  void* memory = GetSharedMemoryAs<void*>(kSomeOffsetInSharedMemory);
  memcpy(memory, kData, sizeof(kData));
  cmd.Init(kBucketId, 0, sizeof(kData), valid_shm_id_,
           kSomeOffsetInSharedMemory);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(0, memcmp(bucket->GetData(0, sizeof(kData)), kData, sizeof(kData)));

  // Check we can set it partially.
  static const char kData2[] = "ABCEDFG";
  const uint32_t kSomeOffsetInBucket = 5;
  memcpy(memory, kData2, sizeof(kData2));
  cmd.Init(kBucketId, kSomeOffsetInBucket, sizeof(kData2), valid_shm_id_,
           kSomeOffsetInSharedMemory);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(0, memcmp(bucket->GetData(kSomeOffsetInBucket, sizeof(kData2)),
                      kData2, sizeof(kData2)));
  const char* bucket_data = bucket->GetDataAs<const char*>(0, sizeof(kData));
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
  int8_t buffer[1024];
  cmd::SetBucketDataImmediate& cmd =
      *reinterpret_cast<cmd::SetBucketDataImmediate*>(&buffer);

  static const char kData[] = "1234567890123456789";

  const uint32_t kBucketId = 123;
  const uint32_t kInvalidBucketId = 124;

  size_cmd.Init(kBucketId, sizeof(kData));
  EXPECT_EQ(error::kNoError, ExecuteCmd(size_cmd));
  CommonDecoder::Bucket* bucket = decoder_.GetBucket(kBucketId);
  // Check the data is not there.
  EXPECT_NE(0, memcmp(bucket->GetData(0, sizeof(kData)), kData, sizeof(kData)));

  // Check we can set it.
  void* memory = &buffer[0] + sizeof(cmd);
  memcpy(memory, kData, sizeof(kData));
  cmd.Init(kBucketId, 0, sizeof(kData));
  EXPECT_EQ(error::kNoError,
            ExecuteImmediateCmd(cmd, sizeof(kData)));
  EXPECT_EQ(0, memcmp(bucket->GetData(0, sizeof(kData)), kData, sizeof(kData)));

  // Check we can set it partially.
  static const char kData2[] = "ABCEDFG";
  const uint32_t kSomeOffsetInBucket = 5;
  memcpy(memory, kData2, sizeof(kData2));
  cmd.Init(kBucketId, kSomeOffsetInBucket, sizeof(kData2));
  EXPECT_EQ(error::kNoError,
            ExecuteImmediateCmd(cmd, sizeof(kData2)));
  EXPECT_EQ(0, memcmp(bucket->GetData(kSomeOffsetInBucket, sizeof(kData2)),
                      kData2, sizeof(kData2)));
  const char* bucket_data = bucket->GetDataAs<const char*>(0, sizeof(kData));
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
  memcpy(&ret, ptr, sizeof(uint32_t));
  return ret;
}

void StoreU32Unaligned(uint32_t v, void* ptr) {
  memcpy(ptr, &v, sizeof(uint32_t));
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
  memcpy(start, kData, sizeof(kData));
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
  memset(data, 0, sizeof(kData));
  cmd.Init(kBucketId, valid_shm_id_, kSomeOffsetInSharedMemory, kBucketSize,
           valid_shm_id_, kDataOffsetInSharedMemory);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(kBucketSize, LoadU32Unaligned(memory));
  EXPECT_EQ(0, memcmp(data, kData, kBucketSize));

  // Check that we can get a piece.
  StoreU32Unaligned(0, memory);
  memset(data, 0, sizeof(kData));
  const uint32_t kPieceSize = kBucketSize / 2;
  cmd.Init(kBucketId, valid_shm_id_, kSomeOffsetInSharedMemory, kPieceSize,
           valid_shm_id_, kDataOffsetInSharedMemory);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(kBucketSize, LoadU32Unaligned(memory));
  EXPECT_EQ(0, memcmp(data, kData, kPieceSize));
  EXPECT_EQ(0, memcmp(data + kPieceSize, zero, sizeof(kData) - kPieceSize));

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
  memcpy(memory, kData, sizeof(kData));
  set_cmd.Init(kBucketId, 0, sizeof(kData), valid_shm_id_,
               kSomeOffsetInSharedMemory);
  EXPECT_EQ(error::kNoError, ExecuteCmd(set_cmd));

  // Check we can get the whole thing.
  memset(memory, 0, sizeof(kData));
  cmd.Init(kBucketId, 0, sizeof(kData), valid_shm_id_,
           kSomeOffsetInSharedMemory);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(0, memcmp(memory, kData, sizeof(kData)));

  // Check we can get a piece.
  const uint32_t kSomeOffsetInBucket = 5;
  const uint32_t kLengthOfPiece = 6;
  const uint8_t kSentinel = 0xff;
  memset(memory, 0, sizeof(kData));
  memory[-1] = kSentinel;
  cmd.Init(kBucketId, kSomeOffsetInBucket, kLengthOfPiece, valid_shm_id_,
           kSomeOffsetInSharedMemory);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(0, memcmp(memory, kData + kSomeOffsetInBucket, kLengthOfPiece));
  EXPECT_EQ(0, memcmp(memory + kLengthOfPiece, zero,
                      sizeof(kData) - kLengthOfPiece));
  EXPECT_EQ(kSentinel, memory[-1]);

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

}  // namespace gpu
