// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Tests for RasterImplementation.

#include "gpu/command_buffer/client/raster_implementation.h"

#include <GLES2/gl2.h>
#include <GLES2/gl2extchromium.h>
#include <GLES3/gl32.h>
#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <array>
#include <memory>

#include "base/bits.h"
#include "base/compiler_specific.h"
#include "base/containers/heap_array.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "cc/paint/raw_memory_transfer_cache_entry.h"
#include "cc/paint/transfer_cache_serialize_helper.h"
#include "gpu/command_buffer/client/client_test_helper.h"
#include "gpu/command_buffer/client/mock_transfer_buffer.h"
#include "gpu/command_buffer/client/query_tracker.h"
#include "gpu/command_buffer/client/raster_cmd_helper.h"
#include "gpu/command_buffer/client/ring_buffer.h"
#include "gpu/command_buffer/client/shared_memory_limits.h"
#include "gpu/command_buffer/client/transfer_buffer.h"
#include "gpu/command_buffer/common/command_buffer.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using gpu::gles2::QueryTracker;
using testing::_;
using testing::AtLeast;
using testing::AnyNumber;
using testing::DoAll;
using testing::InSequence;
using testing::Mock;
using testing::Sequence;
using testing::StrictMock;
using testing::Return;
using testing::ReturnRef;

namespace gpu {
namespace raster {

ACTION_P2(SetMemory, dst, obj) {
  UNSAFE_TODO(memcpy(dst, &obj, sizeof(obj)));
}

ACTION_P3(SetMemoryFromArray, dst, array, size) {
  UNSAFE_TODO(memcpy(dst, array, size));
}

// Used to help set the transfer buffer result to SizedResult of a single value.
template <typename T>
class SizedResultHelper {
 public:
  explicit SizedResultHelper(T result) : size_(sizeof(result)) {
    memcpy(result_, &result, sizeof(T));
  }

 private:
  uint32_t size_;
  char result_[sizeof(T)];
};

class RasterImplementationTest : public testing::Test {
 protected:
  static const uint8_t kInitialValue = 0xBD;
  static const uint32_t kNumCommandEntries = 500;
  static const uint32_t kCommandBufferSizeBytes =
      kNumCommandEntries * sizeof(CommandBufferEntry);
  static const uint32_t kTransferBufferSize = 512;

  static const GLint kMaxTextureSize = 128;
  static const GLuint kStartId = 1024;
  static const GLuint kBuffersStartId = 1;
  static const GLuint kTexturesStartId = 1;
  static const GLuint kQueriesStartId = 1;

  typedef MockTransferBuffer::ExpectedMemoryInfo ExpectedMemoryInfo;

  class TestContext {
   public:
    TestContext() : commands_(nullptr), token_(0) {}

    bool Initialize(bool lose_context_when_out_of_memory,
                    bool transfer_buffer_initialize_fail,
                    bool sync_query) {
      SharedMemoryLimits limits = SharedMemoryLimitsForTesting();
      command_buffer_ = std::make_unique<StrictMock<MockClientCommandBuffer>>();

      transfer_buffer_ = base::WrapUnique(new MockTransferBuffer(
          command_buffer_.get(), kTransferBufferSize,
          RasterImplementation::kStartingOffset,
          RasterImplementation::kAlignment, transfer_buffer_initialize_fail));

      helper_ = std::make_unique<RasterCmdHelper>(command_buffer());
      helper_->Initialize(limits.command_buffer_size);

      gpu_control_ = std::make_unique<StrictMock<MockClientGpuControl>>();
      capabilities_.max_texture_size = kMaxTextureSize;
      capabilities_.sync_query = sync_query;
      EXPECT_CALL(*gpu_control_, GetCapabilities())
          .WillOnce(ReturnRef(capabilities_));

      {
        InSequence sequence;

        gl_ = std::make_unique<RasterImplementation>(
            helper_.get(), transfer_buffer_.get(),
            lose_context_when_out_of_memory, gpu_control_.get());
      }

      // The client should be set to something non-null.
      EXPECT_CALL(*gpu_control_, SetGpuControlClient(gl_.get())).Times(1);

      if (gl_->Initialize(limits) != gpu::ContextResult::kSuccess)
        return false;

      helper_->CommandBufferHelper::Finish();
      Mock::VerifyAndClearExpectations(gl_.get());

      scoped_refptr<Buffer> ring_buffer = helper_->get_ring_buffer();
      commands_ =
          UNSAFE_TODO(static_cast<CommandBufferEntry*>(ring_buffer->memory()) +
                      command_buffer()->GetServicePutOffset());
      ClearCommands();
      EXPECT_TRUE(transfer_buffer_->InSync());

      Mock::VerifyAndClearExpectations(command_buffer());
      return true;
    }

    void TearDown() {
      Mock::VerifyAndClear(gl_.get());
      EXPECT_CALL(*command_buffer(), OnFlush()).Times(AnyNumber());
      // For command buffer.
      EXPECT_CALL(*command_buffer(), DestroyTransferBuffer(_))
          .Times(AtLeast(1));
      // The client should be unset.
      EXPECT_CALL(*gpu_control_, SetGpuControlClient(nullptr)).Times(1);
      EXPECT_CALL(*gpu_control_, CancelAllQueries()).Times(1);
      gl_.reset();
    }

    MockClientCommandBuffer* command_buffer() const {
      return command_buffer_.get();
    }

    int GetNextToken() { return ++token_; }

    void ClearCommands() {
      scoped_refptr<Buffer> ring_buffer = helper_->get_ring_buffer();
      UNSAFE_TODO(
          memset(ring_buffer->memory(), kInitialValue, ring_buffer->size()));
    }

    std::unique_ptr<MockClientCommandBuffer> command_buffer_;
    std::unique_ptr<MockClientGpuControl> gpu_control_;
    std::unique_ptr<RasterCmdHelper> helper_;
    std::unique_ptr<MockTransferBuffer> transfer_buffer_;
    std::unique_ptr<RasterImplementation> gl_;
    raw_ptr<CommandBufferEntry> commands_;
    int token_;
    Capabilities capabilities_;
  };

  RasterImplementationTest() : commands_(nullptr) {}

  void SetUp() override;
  void TearDown() override;

  bool NoCommandsWritten() {
    scoped_refptr<Buffer> ring_buffer = helper_->get_ring_buffer();
    const uint8_t* cmds = static_cast<const uint8_t*>(ring_buffer->memory());
    const uint8_t* end = UNSAFE_TODO(cmds + ring_buffer->size());
    for (; cmds < end; UNSAFE_TODO(++cmds)) {
      if (*cmds != kInitialValue) {
        return false;
      }
    }
    return true;
  }

  QueryTracker::Query* GetQuery(GLuint id) {
    return gl_->query_tracker_->GetQuery(id);
  }

  QueryTracker* GetQueryTracker() { return gl_->query_tracker_.get(); }

  struct ContextInitOptions {
    bool lose_context_when_out_of_memory = false;
    bool transfer_buffer_initialize_fail = false;
    bool sync_query = true;
  };

  bool Initialize(const ContextInitOptions& init_options) {
    bool success = true;
    if (!test_context_.Initialize(init_options.lose_context_when_out_of_memory,
                                  init_options.transfer_buffer_initialize_fail,
                                  init_options.sync_query)) {
      success = false;
    }

    // Default to test context 0.
    gpu_control_ = test_context_.gpu_control_.get();
    helper_ = test_context_.helper_.get();
    transfer_buffer_ = test_context_.transfer_buffer_.get();
    gl_ = test_context_.gl_.get();
    commands_ = test_context_.commands_;
    return success;
  }

  MockClientCommandBuffer* command_buffer() const {
    return test_context_.command_buffer_.get();
  }

  int GetNextToken() { return test_context_.GetNextToken(); }

  const void* GetPut() { return helper_->GetSpace(0); }

  void ClearCommands() {
    scoped_refptr<Buffer> ring_buffer = helper_->get_ring_buffer();
    UNSAFE_TODO(
        memset(ring_buffer->memory(), kInitialValue, ring_buffer->size()));
  }

  uint32_t MaxTransferBufferSize() {
    return transfer_buffer_->MaxTransferBufferSize();
  }

  void SetMappedMemoryLimit(size_t limit) {
    gl_->mapped_memory_->set_max_allocated_bytes(limit);
  }

  ExpectedMemoryInfo GetExpectedMemory(uint32_t size) {
    return transfer_buffer_->GetExpectedMemory(size);
  }

  ExpectedMemoryInfo GetExpectedResultMemory(uint32_t size) {
    return transfer_buffer_->GetExpectedResultMemory(size);
  }

  ExpectedMemoryInfo GetExpectedMappedMemory(uint32_t size) {
    ExpectedMemoryInfo mem;

    // Temporarily allocate memory and expect that memory block to be reused.
    mem.span = gl_->mapped_memory_->Alloc(size, &mem.id, &mem.offset);
    mem.ptr = mem.span.data();
    gl_->mapped_memory_->Free(mem.ptr);

    return mem;
  }

  int CheckError() {
    ExpectedMemoryInfo result =
        GetExpectedResultMemory(sizeof(cmds::GetError::Result));
    EXPECT_CALL(*command_buffer(), OnFlush())
        .WillOnce(SetMemory(result.ptr, GLuint(GL_NO_ERROR)))
        .RetiresOnSaturation();
    return gl_->GetError();
  }

  const std::string& GetLastError() { return gl_->GetLastError(); }

  bool GetBucketContents(uint32_t bucket_id, std::vector<int8_t>* data) {
    return gl_->GetBucketContents(bucket_id, data);
  }

  bool ReadbackImagePixelsINTERNAL(const gpu::Mailbox& source_mailbox,
                                   const SkImageInfo& dst_info,
                                   GLuint dst_row_bytes,
                                   int src_x,
                                   int src_y,
                                   int plane_index,
                                   base::OnceCallback<void(bool)> readback_done,
                                   void* dst_pixels) {
    return gl_->ReadbackImagePixelsINTERNAL(
        source_mailbox, dst_info, dst_row_bytes, src_x, src_y, plane_index,
        std::move(readback_done), dst_pixels);
  }

  static SharedMemoryLimits SharedMemoryLimitsForTesting() {
    SharedMemoryLimits limits;
    limits.command_buffer_size = kCommandBufferSizeBytes;
    limits.start_transfer_buffer_size = kTransferBufferSize;
    limits.min_transfer_buffer_size = kTransferBufferSize;
    limits.max_transfer_buffer_size = kTransferBufferSize;
    limits.mapped_memory_reclaim_limit = SharedMemoryLimits::kNoLimit;
    return limits;
  }

  TestContext test_context_;

  raw_ptr<MockClientGpuControl> gpu_control_;
  raw_ptr<RasterCmdHelper> helper_;
  raw_ptr<MockTransferBuffer> transfer_buffer_;
  raw_ptr<RasterImplementation> gl_;
  raw_ptr<CommandBufferEntry> commands_;
};

void RasterImplementationTest::SetUp() {
  ContextInitOptions init_options;
  ASSERT_TRUE(Initialize(init_options));
}

void RasterImplementationTest::TearDown() {
  gl_ = nullptr;
  test_context_.TearDown();
}

class RasterImplementationManualInitTest : public RasterImplementationTest {
 protected:
  void SetUp() override {}
};

const uint8_t RasterImplementationTest::kInitialValue;
const uint32_t RasterImplementationTest::kNumCommandEntries;
const uint32_t RasterImplementationTest::kCommandBufferSizeBytes;
const uint32_t RasterImplementationTest::kTransferBufferSize;
const GLint RasterImplementationTest::kMaxTextureSize;
const GLuint RasterImplementationTest::kStartId;
const GLuint RasterImplementationTest::kBuffersStartId;
const GLuint RasterImplementationTest::kTexturesStartId;
const GLuint RasterImplementationTest::kQueriesStartId;

TEST_F(RasterImplementationTest, GetBucketContents) {
  const uint32_t kBucketId = RasterImplementation::kResultBucketId;
  const uint32_t kTestSize = MaxTransferBufferSize() + 32;

  auto buf = base::HeapArray<uint8_t>::Uninit(kTestSize);
  for (uint32_t ii = 0; ii < kTestSize; ++ii) {
    buf[ii] = ii * 3;
  }

  struct Cmds {
    cmd::GetBucketStart get_bucket_start;
    cmd::SetToken set_token1;
    cmd::GetBucketData get_bucket_data;
    cmd::SetToken set_token2;
    cmd::SetBucketSize set_bucket_size2;
  };

  ExpectedMemoryInfo mem1 = GetExpectedMemory(MaxTransferBufferSize());
  ExpectedMemoryInfo result1 = GetExpectedResultMemory(sizeof(uint32_t));
  ExpectedMemoryInfo mem2 =
      GetExpectedMemory(kTestSize - MaxTransferBufferSize());

  Cmds expected;
  expected.get_bucket_start.Init(kBucketId, result1.id, result1.offset,
                                 MaxTransferBufferSize(), mem1.id, mem1.offset);
  expected.set_token1.Init(GetNextToken());
  expected.get_bucket_data.Init(kBucketId, MaxTransferBufferSize(),
                                kTestSize - MaxTransferBufferSize(), mem2.id,
                                mem2.offset);
  expected.set_bucket_size2.Init(kBucketId, 0);
  expected.set_token2.Init(GetNextToken());

  EXPECT_CALL(*command_buffer(), OnFlush())
      .WillOnce(DoAll(
          SetMemory(result1.ptr, kTestSize),
          SetMemoryFromArray(mem1.ptr, buf.data(), MaxTransferBufferSize())))
      .WillOnce(SetMemoryFromArray(
          mem2.ptr, UNSAFE_TODO(buf.data() + MaxTransferBufferSize()),
          kTestSize - MaxTransferBufferSize()))
      .RetiresOnSaturation();

  std::vector<int8_t> data;
  GetBucketContents(kBucketId, &data);
  UNSAFE_TODO(EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected))));
  ASSERT_EQ(kTestSize, data.size());
  UNSAFE_TODO(EXPECT_EQ(0, memcmp(buf.data(), &data[0], data.size())));
}

TEST_F(RasterImplementationTest, BeginEndQueryEXT) {
  //  GL_COMMANDS_COMPLETED_CHROMIUM,
  //  GL_CURRENT_QUERY_EXT

  std::array<GLuint, 2> expected_ids = {
      1, 2};  // These must match what's actually genned.
  struct GenCmds {
    cmds::GenQueriesEXTImmediate gen;
    GLuint data[2];
  };
  GenCmds expected_gen_cmds;
  expected_gen_cmds.gen.Init(std::size(expected_ids), &expected_ids[0]);
  std::array<GLuint, std::size(expected_ids)> ids = {};
  gl_->GenQueriesEXT(std::size(expected_ids), &ids[0]);
  UNSAFE_TODO(EXPECT_EQ(
      0, memcmp(&expected_gen_cmds, commands_, sizeof(expected_gen_cmds))));
  GLuint id1 = ids[0];
  GLuint id2 = ids[1];
  ClearCommands();

  // Test BeginQueryEXT fails if id = 0.
  gl_->BeginQueryEXT(GL_COMMANDS_COMPLETED_CHROMIUM, 0);
  EXPECT_TRUE(NoCommandsWritten());
  EXPECT_EQ(GL_INVALID_OPERATION, CheckError());

  // Test BeginQueryEXT inserts command.
  struct BeginCmds {
    cmds::BeginQueryEXT begin_query;
  };
  BeginCmds expected_begin_cmds;
  const void* commands = GetPut();
  gl_->BeginQueryEXT(GL_COMMANDS_COMPLETED_CHROMIUM, id1);
  QueryTracker::Query* query = GetQuery(id1);
  ASSERT_TRUE(query != nullptr);
  expected_begin_cmds.begin_query.Init(GL_COMMANDS_COMPLETED_CHROMIUM, id1,
                                       query->shm_id(), query->shm_offset());
  UNSAFE_TODO(EXPECT_EQ(
      0, memcmp(&expected_begin_cmds, commands, sizeof(expected_begin_cmds))));
  ClearCommands();

  // Test BeginQueryEXT fails if between Begin/End.
  gl_->BeginQueryEXT(GL_COMMANDS_COMPLETED_CHROMIUM, id2);
  EXPECT_TRUE(NoCommandsWritten());
  EXPECT_EQ(GL_INVALID_OPERATION, CheckError());

  // Test EndQueryEXT sends command
  struct EndCmds {
    cmds::EndQueryEXT end_query;
  };
  commands = GetPut();
  gl_->EndQueryEXT(GL_COMMANDS_COMPLETED_CHROMIUM);
  EndCmds expected_end_cmds;
  expected_end_cmds.end_query.Init(GL_COMMANDS_COMPLETED_CHROMIUM,
                                   query->submit_count());
  UNSAFE_TODO(EXPECT_EQ(
      0, memcmp(&expected_end_cmds, commands, sizeof(expected_end_cmds))));

  // Test EndQueryEXT fails if no current query.
  ClearCommands();
  gl_->EndQueryEXT(GL_COMMANDS_COMPLETED_CHROMIUM);
  EXPECT_TRUE(NoCommandsWritten());
  EXPECT_EQ(GL_INVALID_OPERATION, CheckError());

  // Test 2nd Begin/End increments count.
  base::subtle::Atomic32 old_submit_count = query->submit_count();
  gl_->BeginQueryEXT(GL_COMMANDS_COMPLETED_CHROMIUM, id1);
  EXPECT_EQ(old_submit_count, query->submit_count());
  commands = GetPut();
  gl_->EndQueryEXT(GL_COMMANDS_COMPLETED_CHROMIUM);
  EXPECT_NE(old_submit_count, query->submit_count());
  expected_end_cmds.end_query.Init(GL_COMMANDS_COMPLETED_CHROMIUM,
                                   query->submit_count());
  UNSAFE_TODO(EXPECT_EQ(
      0, memcmp(&expected_end_cmds, commands, sizeof(expected_end_cmds))));

  // Test GetQueryObjectuivEXT fails if unused id
  GLuint available = 0xBDu;
  ClearCommands();
  gl_->GetQueryObjectuivEXT(id2, GL_QUERY_RESULT_AVAILABLE_EXT, &available);
  EXPECT_TRUE(NoCommandsWritten());
  EXPECT_EQ(0xBDu, available);
  EXPECT_EQ(GL_INVALID_OPERATION, CheckError());

  // Test GetQueryObjectuivEXT fails if bad id
  ClearCommands();
  gl_->GetQueryObjectuivEXT(4567, GL_QUERY_RESULT_AVAILABLE_EXT, &available);
  EXPECT_TRUE(NoCommandsWritten());
  EXPECT_EQ(0xBDu, available);
  EXPECT_EQ(GL_INVALID_OPERATION, CheckError());

  // Test GetQueryObjectuivEXT CheckResultsAvailable
  ClearCommands();
  gl_->GetQueryObjectuivEXT(id1, GL_QUERY_RESULT_AVAILABLE_EXT, &available);
  EXPECT_EQ(0u, available);
  available = 1u;
  gl_->GetQueryObjectuivEXT(
      id1, GL_QUERY_RESULT_AVAILABLE_NO_FLUSH_CHROMIUM_EXT, &available);
  EXPECT_EQ(0u, available);
}

TEST_F(RasterImplementationManualInitTest, BadQueryTargets) {
  ContextInitOptions init_options;
  init_options.sync_query = false;
  ASSERT_TRUE(Initialize(init_options));

  GLuint id = 0;
  gl_->GenQueriesEXT(1, &id);
  ClearCommands();

  gl_->BeginQueryEXT(GL_COMMANDS_COMPLETED_CHROMIUM, id);
  EXPECT_EQ(GL_INVALID_OPERATION, CheckError());
  EXPECT_EQ(nullptr, GetQuery(id));

  gl_->BeginQueryEXT(0x123, id);
  EXPECT_EQ(GL_INVALID_ENUM, CheckError());
  EXPECT_EQ(nullptr, GetQuery(id));
}

TEST_F(RasterImplementationTest, GenUnverifiedSyncTokenCHROMIUM) {
  const CommandBufferNamespace kNamespaceId = CommandBufferNamespace::GPU_IO;
  const CommandBufferId kCommandBufferId =
      CommandBufferId::FromUnsafeValue(234u);
  const GLuint64 kFenceSync = 123u;
  SyncToken sync_token;

  EXPECT_CALL(*gpu_control_, GetNamespaceID())
      .WillRepeatedly(Return(kNamespaceId));
  EXPECT_CALL(*gpu_control_, GetCommandBufferID())
      .WillRepeatedly(Return(kCommandBufferId));

  gl_->GenUnverifiedSyncTokenCHROMIUM(nullptr);
  EXPECT_TRUE(NoCommandsWritten());
  EXPECT_EQ(GL_INVALID_VALUE, CheckError());

  const void* commands = GetPut();
  cmd::InsertFenceSync insert_fence_sync;
  insert_fence_sync.Init(kFenceSync);

  EXPECT_CALL(*gpu_control_, GenerateFenceSyncRelease())
      .WillOnce(Return(kFenceSync));
  gl_->GenUnverifiedSyncTokenCHROMIUM(sync_token.GetData());
  UNSAFE_TODO(EXPECT_EQ(
      0, memcmp(&insert_fence_sync, commands, sizeof(insert_fence_sync))));
  EXPECT_EQ(GL_NO_ERROR, CheckError());

  EXPECT_FALSE(sync_token.verified_flush());
  EXPECT_EQ(kNamespaceId, sync_token.namespace_id());
  EXPECT_EQ(kCommandBufferId, sync_token.command_buffer_id());
  EXPECT_EQ(kFenceSync, sync_token.release_count());
}

TEST_F(RasterImplementationTest, VerifySyncTokensCHROMIUM) {
  ExpectedMemoryInfo result =
      GetExpectedResultMemory(sizeof(cmds::GetError::Result));
  EXPECT_CALL(*command_buffer(), OnFlush())
      .WillRepeatedly(SetMemory(result.ptr, GLuint(GL_NO_ERROR)))
      .RetiresOnSaturation();

  const CommandBufferNamespace kNamespaceId = CommandBufferNamespace::GPU_IO;
  const CommandBufferId kCommandBufferId =
      CommandBufferId::FromUnsafeValue(234u);
  const GLuint64 kFenceSync = 123u;
  gpu::SyncToken sync_token;
  GLbyte* sync_token_datas[] = {sync_token.GetData()};

  EXPECT_CALL(*gpu_control_, GetNamespaceID())
      .WillRepeatedly(Return(kNamespaceId));
  EXPECT_CALL(*gpu_control_, GetCommandBufferID())
      .WillRepeatedly(Return(kCommandBufferId));

  EXPECT_CALL(*gpu_control_, GenerateFenceSyncRelease())
      .WillOnce(Return(kFenceSync));
  gl_->GenUnverifiedSyncTokenCHROMIUM(sync_token.GetData());
  EXPECT_TRUE(sync_token.HasData());
  EXPECT_FALSE(sync_token.verified_flush());

  ClearCommands();
  EXPECT_CALL(*gpu_control_, CanWaitUnverifiedSyncToken(sync_token))
      .WillOnce(Return(false));
  gl_->VerifySyncTokensCHROMIUM(sync_token_datas, 1);
  EXPECT_TRUE(NoCommandsWritten());
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_VALUE), gl_->GetError());
  EXPECT_FALSE(sync_token.verified_flush());

  ClearCommands();
  EXPECT_CALL(*gpu_control_, CanWaitUnverifiedSyncToken(sync_token))
      .WillOnce(Return(true));
  EXPECT_CALL(*gpu_control_, EnsureWorkVisible());
  gl_->VerifySyncTokensCHROMIUM(sync_token_datas, std::size(sync_token_datas));
  EXPECT_TRUE(NoCommandsWritten());
  EXPECT_EQ(GL_NO_ERROR, CheckError());

  EXPECT_EQ(kNamespaceId, sync_token.namespace_id());
  EXPECT_EQ(kCommandBufferId, sync_token.command_buffer_id());
  EXPECT_EQ(kFenceSync, sync_token.release_count());
  EXPECT_TRUE(sync_token.verified_flush());
}

TEST_F(RasterImplementationTest, VerifySyncTokensCHROMIUM_Sequence) {
  // To verify sync tokens, the sync tokens must all be verified after
  // CanWaitUnverifiedSyncTokens() are called. This test ensures the right
  // sequence.
  ExpectedMemoryInfo result =
      GetExpectedResultMemory(sizeof(cmds::GetError::Result));
  EXPECT_CALL(*command_buffer(), OnFlush())
      .WillRepeatedly(SetMemory(result.ptr, GLuint(GL_NO_ERROR)))
      .RetiresOnSaturation();

  const CommandBufferNamespace kNamespaceId = CommandBufferNamespace::GPU_IO;
  const CommandBufferId kCommandBufferId =
      CommandBufferId::FromUnsafeValue(234u);
  const GLuint64 kFenceSync1 = 123u;
  const GLuint64 kFenceSync2 = 234u;
  gpu::SyncToken sync_token1;
  gpu::SyncToken sync_token2;
  GLbyte* sync_token_datas[] = {sync_token1.GetData(), sync_token2.GetData()};

  EXPECT_CALL(*gpu_control_, GetNamespaceID())
      .WillRepeatedly(Return(kNamespaceId));
  EXPECT_CALL(*gpu_control_, GetCommandBufferID())
      .WillRepeatedly(Return(kCommandBufferId));

  // Generate sync token 1.
  EXPECT_CALL(*gpu_control_, GenerateFenceSyncRelease())
      .WillOnce(Return(kFenceSync1));
  gl_->GenUnverifiedSyncTokenCHROMIUM(sync_token1.GetData());
  EXPECT_TRUE(sync_token1.HasData());
  EXPECT_FALSE(sync_token1.verified_flush());

  // Generate sync token 2.
  EXPECT_CALL(*gpu_control_, GenerateFenceSyncRelease())
      .WillOnce(Return(kFenceSync2));
  gl_->GenUnverifiedSyncTokenCHROMIUM(sync_token2.GetData());
  EXPECT_TRUE(sync_token2.HasData());
  EXPECT_FALSE(sync_token2.verified_flush());

  // Ensure proper sequence of checking and validating.
  Sequence sequence;
  EXPECT_CALL(*gpu_control_, CanWaitUnverifiedSyncToken(sync_token1))
      .InSequence(sequence)
      .WillOnce(Return(true));
  EXPECT_CALL(*gpu_control_, CanWaitUnverifiedSyncToken(sync_token2))
      .InSequence(sequence)
      .WillOnce(Return(true));
  EXPECT_CALL(*gpu_control_, EnsureWorkVisible()).InSequence(sequence);
  gl_->VerifySyncTokensCHROMIUM(sync_token_datas, std::size(sync_token_datas));
  EXPECT_EQ(GL_NO_ERROR, CheckError());

  EXPECT_TRUE(sync_token1.verified_flush());
  EXPECT_TRUE(sync_token2.verified_flush());
}

TEST_F(RasterImplementationTest, VerifySyncTokensCHROMIUM_EmptySyncToken) {
  // To verify sync tokens, the sync tokens must all be verified after
  // CanWaitUnverifiedSyncTokens() are called. This test ensures the right
  // sequence.
  ExpectedMemoryInfo result =
      GetExpectedResultMemory(sizeof(cmds::GetError::Result));
  EXPECT_CALL(*command_buffer(), OnFlush())
      .WillRepeatedly(SetMemory(result.ptr, GLuint(GL_NO_ERROR)))
      .RetiresOnSaturation();

  gpu::SyncToken sync_token1, sync_token2;
  GLbyte* sync_token_datas[] = {sync_token1.GetData(), sync_token2.GetData()};

  // Ensure proper sequence of checking and validating.
  EXPECT_CALL(*gpu_control_, CanWaitUnverifiedSyncToken(_)).Times(0);
  EXPECT_CALL(*gpu_control_, EnsureWorkVisible()).Times(0);
  gl_->VerifySyncTokensCHROMIUM(sync_token_datas, std::size(sync_token_datas));
  EXPECT_TRUE(NoCommandsWritten());
  EXPECT_EQ(GL_NO_ERROR, CheckError());

  EXPECT_TRUE(sync_token1.verified_flush());
  EXPECT_TRUE(sync_token2.verified_flush());
}

TEST_F(RasterImplementationTest, WaitSyncTokenCHROMIUM) {
  const CommandBufferNamespace kNamespaceId = CommandBufferNamespace::GPU_IO;
  const CommandBufferId kCommandBufferId =
      CommandBufferId::FromUnsafeValue(234u);
  const GLuint64 kFenceSync = 456u;

  gpu::SyncToken sync_token;
  GLbyte* sync_token_data = sync_token.GetData();

  struct Cmds {
    cmd::InsertFenceSync insert_fence_sync;
  };
  Cmds expected;
  expected.insert_fence_sync.Init(kFenceSync);

  EXPECT_CALL(*gpu_control_, GetNamespaceID()).WillOnce(Return(kNamespaceId));
  EXPECT_CALL(*gpu_control_, GetCommandBufferID())
      .WillOnce(Return(kCommandBufferId));
  EXPECT_CALL(*gpu_control_, GenerateFenceSyncRelease())
      .WillOnce(Return(kFenceSync));
  gl_->GenUnverifiedSyncTokenCHROMIUM(sync_token_data);

  EXPECT_CALL(*gpu_control_, CanWaitUnverifiedSyncToken(sync_token))
      .WillOnce(Return(true));
  gpu::SyncToken verified_sync_token = sync_token;
  verified_sync_token.SetVerifyFlush();
  EXPECT_CALL(*gpu_control_, WaitSyncToken(verified_sync_token));
  gl_->WaitSyncTokenCHROMIUM(sync_token_data);
  UNSAFE_TODO(EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected))));
}

TEST_F(RasterImplementationTest, WaitSyncTokenCHROMIUMErrors) {
  ExpectedMemoryInfo result =
      GetExpectedResultMemory(sizeof(cmds::GetError::Result));
  EXPECT_CALL(*command_buffer(), OnFlush())
      .WillRepeatedly(SetMemory(result.ptr, GLuint(GL_NO_ERROR)))
      .RetiresOnSaturation();

  // Empty sync tokens should be produce no error and be a nop.
  ClearCommands();
  gl_->WaitSyncTokenCHROMIUM(nullptr);
  EXPECT_TRUE(NoCommandsWritten());
  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), gl_->GetError());

  // Invalid sync tokens should produce no error and be a nop.
  ClearCommands();
  gpu::SyncToken invalid_sync_token;
  gl_->WaitSyncTokenCHROMIUM(invalid_sync_token.GetConstData());
  EXPECT_TRUE(NoCommandsWritten());
  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), gl_->GetError());

  // Unverified sync token should produce INVALID_OPERATION.
  ClearCommands();
  gpu::SyncToken unverified_sync_token(CommandBufferNamespace::GPU_IO,
                                       gpu::CommandBufferId(), 0);
  EXPECT_CALL(*gpu_control_, CanWaitUnverifiedSyncToken(unverified_sync_token))
      .WillOnce(Return(false));
  gl_->WaitSyncTokenCHROMIUM(unverified_sync_token.GetConstData());
  EXPECT_TRUE(NoCommandsWritten());
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_VALUE), gl_->GetError());
}

static void CountCallback(int* count) {
  (*count)++;
}

TEST_F(RasterImplementationTest, SignalSyncToken) {
  const CommandBufferNamespace kNamespaceId = CommandBufferNamespace::GPU_IO;
  const CommandBufferId kCommandBufferId = CommandBufferId::FromUnsafeValue(1);
  const uint64_t kFenceSync = 123u;

  EXPECT_CALL(*gpu_control_, GetNamespaceID())
      .WillRepeatedly(Return(kNamespaceId));
  EXPECT_CALL(*gpu_control_, GetCommandBufferID())
      .WillRepeatedly(Return(kCommandBufferId));

  EXPECT_CALL(*gpu_control_, GenerateFenceSyncRelease())
      .WillOnce(Return(kFenceSync));
  gpu::SyncToken sync_token;
  gl_->GenUnverifiedSyncTokenCHROMIUM(sync_token.GetData());

  int signaled_count = 0;

  // Request a signal sync token, which gives a callback to the GpuControl to
  // run when the sync token is reached.
  base::OnceClosure signal_closure;
  EXPECT_CALL(*gpu_control_, DoSignalSyncToken(_, _))
      .WillOnce([&signal_closure](const SyncToken& sync_token,
                                  base::OnceClosure* callback) {
        signal_closure = std::move(*callback);
      });
  EXPECT_CALL(*gpu_control_, CanWaitUnverifiedSyncToken(sync_token))
      .WillOnce(Return(true));
  gl_->SignalSyncToken(sync_token,
                       base::BindOnce(&CountCallback, &signaled_count));
  EXPECT_EQ(0, signaled_count);

  // When GpuControl runs the callback, the original callback we gave to
  // RasterImplementation is run.
  std::move(signal_closure).Run();
  EXPECT_EQ(1, signaled_count);
}

TEST_F(RasterImplementationTest, SignalSyncTokenAfterContextLoss) {
  const CommandBufferNamespace kNamespaceId = CommandBufferNamespace::GPU_IO;
  const CommandBufferId kCommandBufferId = CommandBufferId::FromUnsafeValue(1);
  const uint64_t kFenceSync = 123u;

  EXPECT_CALL(*gpu_control_, GetNamespaceID()).WillOnce(Return(kNamespaceId));
  EXPECT_CALL(*gpu_control_, GetCommandBufferID())
      .WillOnce(Return(kCommandBufferId));
  EXPECT_CALL(*gpu_control_, GenerateFenceSyncRelease())
      .WillOnce(Return(kFenceSync));
  gpu::SyncToken sync_token;
  gl_->GenUnverifiedSyncTokenCHROMIUM(sync_token.GetData());

  int signaled_count = 0;

  // Request a signal sync token, which gives a callback to the GpuControl to
  // run when the sync token is reached.
  base::OnceClosure signal_closure;
  EXPECT_CALL(*gpu_control_, DoSignalSyncToken(_, _))
      .WillOnce([&signal_closure](const SyncToken& sync_token,
                                  base::OnceClosure* callback) {
        signal_closure = std::move(*callback);
      });
  EXPECT_CALL(*gpu_control_, CanWaitUnverifiedSyncToken(sync_token))
      .WillOnce(Return(true));
  gl_->SignalSyncToken(sync_token,
                       base::BindOnce(&CountCallback, &signaled_count));
  EXPECT_EQ(0, signaled_count);

  // Inform the RasterImplementation that the context is lost.
  GpuControlClient* gl_as_client = gl_;
  gl_as_client->OnGpuControlLostContext();

  // When GpuControl runs the callback, the original callback we gave to
  // RasterImplementation is *not* run, since the context is lost and we
  // have already run the lost context callback.
  std::move(signal_closure).Run();
  EXPECT_EQ(0, signaled_count);
}

TEST_F(RasterImplementationTest, ReportLoss) {
  GpuControlClient* gl_as_client = gl_;
  int lost_count = 0;
  gl_->SetLostContextCallback(base::BindOnce(&CountCallback, &lost_count));
  EXPECT_EQ(0, lost_count);

  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), gl_->GetGraphicsResetStatusKHR());
  gl_as_client->OnGpuControlLostContext();
  EXPECT_NE(static_cast<GLenum>(GL_NO_ERROR), gl_->GetGraphicsResetStatusKHR());
  // The lost context callback should be run when RasterImplementation is
  // notified of the loss.
  EXPECT_EQ(1, lost_count);
}

TEST_F(RasterImplementationTest, ReportLossReentrant) {
  GpuControlClient* gl_as_client = gl_;
  int lost_count = 0;
  gl_->SetLostContextCallback(base::BindOnce(&CountCallback, &lost_count));
  EXPECT_EQ(0, lost_count);

  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), gl_->GetGraphicsResetStatusKHR());
  gl_as_client->OnGpuControlLostContextMaybeReentrant();
  EXPECT_NE(static_cast<GLenum>(GL_NO_ERROR), gl_->GetGraphicsResetStatusKHR());
  // The lost context callback should not be run yet to avoid calling back into
  // clients re-entrantly, and having them re-enter RasterImplementation.
  EXPECT_EQ(0, lost_count);
}

TEST_F(RasterImplementationManualInitTest, FailInitOnTransferBufferFail) {
  ContextInitOptions init_options;
  init_options.transfer_buffer_initialize_fail = true;
  EXPECT_FALSE(Initialize(init_options));
}

TEST_F(RasterImplementationTest, TransferCacheSerialization) {
  gl_->set_max_inlined_entry_size_for_testing(768u);
  uint32_t buffer_size = transfer_buffer_->MaxTransferBufferSize();
  ScopedTransferBufferPtr buffer(buffer_size, helper_, transfer_buffer_);
  ASSERT_EQ(buffer.size(), buffer_size);

  base::span<uint8_t> buffer_span = buffer.as_byte_span();
  std::ranges::fill(buffer_span, 0u);
  gl_->SetRasterMappedBufferForTesting(std::move(buffer));
  auto transfer_cache = gl_->CreateTransferCacheHelperForTesting();

  std::vector<uint8_t> data(buffer_size - 16u);
  base::span<uint8_t> memory = buffer_span.subspan(8u);
  cc::ClientRawMemoryTransferCacheEntry inlined_entry(data);
  EXPECT_EQ(transfer_cache->CreateEntry(inlined_entry, memory), data.size());
  EXPECT_EQ(base::span(data), memory.first(data.size()));

  data.resize(buffer_size + 16u);
  memory = buffer_span.subspan(8u);
  cc::ClientRawMemoryTransferCacheEntry non_inlined_entry(data);
  EXPECT_EQ(transfer_cache->CreateEntry(non_inlined_entry, memory), 0u);
}

TEST_F(RasterImplementationTest, SetActiveURLCHROMIUM) {
  const uint32_t kURLBucketId = RasterImplementation::kResultBucketId;
  const std::string url = "chrome://test";
  const uint32_t kPaddedStringSize =
      transfer_buffer_->RoundToAlignment(url.size());

  gl_->SetActiveURLCHROMIUM(url.c_str());
  EXPECT_EQ(GL_NO_ERROR, CheckError());

  struct Cmds {
    cmd::SetBucketSize url_size;
    cmd::SetBucketData url_data;
    cmd::SetToken set_token;
    cmds::SetActiveURLCHROMIUM set_url_call;
    cmd::SetBucketSize url_size_end;
  };

  ExpectedMemoryInfo mem = GetExpectedMemory(kPaddedStringSize);
  UNSAFE_TODO(EXPECT_EQ(
      0, memcmp(url.c_str(), reinterpret_cast<char*>(mem.ptr), url.size())));

  Cmds expected;
  expected.url_size.Init(kURLBucketId, url.size());
  expected.url_data.Init(kURLBucketId, 0, url.size(), mem.id, mem.offset);
  expected.set_token.Init(GetNextToken());
  expected.set_url_call.Init(kURLBucketId);
  expected.url_size_end.Init(kURLBucketId, 0);
  UNSAFE_TODO(EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected))));

  // Same URL shouldn't make any commands.
  EXPECT_FALSE(NoCommandsWritten());
  ClearCommands();
  gl_->SetActiveURLCHROMIUM(url.c_str());
  EXPECT_TRUE(NoCommandsWritten());
}

// https://crbug.com/543707066
TEST_F(RasterImplementationTest, ReadbackImagePixelsSyncPadding) {
  gpu::Mailbox mailbox = gpu::Mailbox::Generate();
  SkImageInfo dst_info = SkImageInfo::MakeN32Premul(2, 2);
  GLuint dst_row_bytes =
      12;  // 2 pixels * 4 bytes/pixel = 8 bytes. Row padding = 4 bytes.

  GLuint color_space_offset = base::bits::AlignUp(
      sizeof(cmds::ReadbackARGBImagePixelsINTERNALImmediate::Result),
      sizeof(uint64_t));
  GLuint pixels_offset = color_space_offset;
  GLuint dst_size = dst_info.computeByteSize(dst_row_bytes);
  GLuint total_size =
      pixels_offset +
      base::bits::AlignUp(dst_size, static_cast<GLuint>(sizeof(uint64_t)));

  ExpectedMemoryInfo mem = GetExpectedMappedMemory(total_size);

  std::vector<uint8_t> dst_pixels(dst_row_bytes * dst_info.height(), 0xAA);

  EXPECT_CALL(*command_buffer(), OnFlush())
      .WillOnce([mem, pixels_offset, dst_size]() {
        // Write 1 to readback_result (at the beginning of shm).
        auto* result = reinterpret_cast<
            cmds::ReadbackARGBImagePixelsINTERNALImmediate::Result*>(mem.ptr);
        *result = 1;

        // Write test data to the pixel portion of the shared memory.
        auto src_pixels = mem.span.subspan(pixels_offset, dst_size);
        // Fill src_pixels with distinct values, e.g. 1 to dst_size
        for (size_t i = 0; i < dst_size; ++i) {
          src_pixels[i] = static_cast<uint8_t>(i + 1);
        }
      })
      .RetiresOnSaturation();

  bool success = gl_->ReadbackImagePixels(mailbox, dst_info, dst_row_bytes,
                                          /*src_x=*/0, /*src_y=*/0,
                                          /*plane_index=*/0, dst_pixels.data());

  EXPECT_TRUE(success);

  // Expected output:
  // Row 1 (pixels: 0 to 7) copied from src_pixels (0 to 7): 1, 2, 3, 4, 5, 6,
  // 7, 8. Row 1 (padding: 8 to 11) untouched: 0xAA, 0xAA, 0xAA, 0xAA. Row 2
  // (pixels: 12 to 19) copied from src_pixels (12 to 19): 13, 14, 15, 16, 17,
  // 18, 19, 20. Row 2 (padding: 20 to 23) untouched: 0xAA, 0xAA, 0xAA, 0xAA.

  std::vector<uint8_t> expected_pixels(dst_row_bytes * dst_info.height(), 0xAA);
  for (int y = 0; y < dst_info.height(); ++y) {
    for (size_t x = 0; x < dst_info.minRowBytes(); ++x) {
      size_t dst_idx = y * dst_row_bytes + x;
      size_t src_idx = y * dst_row_bytes + x;
      expected_pixels[dst_idx] = static_cast<uint8_t>(src_idx + 1);
    }
  }

  EXPECT_EQ(dst_pixels, expected_pixels);
}

// https://crbug.com/543707066
TEST_F(RasterImplementationTest, ReadbackImagePixelsAsyncPadding) {
  gpu::Mailbox mailbox = gpu::Mailbox::Generate();
  SkImageInfo dst_info = SkImageInfo::MakeN32Premul(2, 2);
  GLuint dst_row_bytes =
      12;  // 2 pixels * 4 bytes/pixel = 8 bytes. Row padding = 4 bytes.

  GLuint color_space_offset = base::bits::AlignUp(
      sizeof(cmds::ReadbackARGBImagePixelsINTERNALImmediate::Result),
      sizeof(uint64_t));
  GLuint pixels_offset = color_space_offset;
  GLuint dst_size = dst_info.computeByteSize(dst_row_bytes);
  GLuint total_size =
      pixels_offset +
      base::bits::AlignUp(dst_size, static_cast<GLuint>(sizeof(uint64_t)));

  ExpectedMemoryInfo mem = GetExpectedMappedMemory(total_size);

  std::vector<uint8_t> dst_pixels(dst_row_bytes * dst_info.height(), 0xAA);

  base::OnceClosure signal_closure;
  EXPECT_CALL(*gpu_control_, DoSignalQuery(_, _))
      .WillOnce([&signal_closure](uint32_t query, base::OnceClosure* callback) {
        signal_closure = std::move(*callback);
      })
      .RetiresOnSaturation();

  bool callback_run = false;
  bool callback_success = false;
  base::OnceCallback<void(bool)> readback_done = base::BindOnce(
      [](bool* run, bool* success, bool val) {
        *run = true;
        *success = val;
      },
      &callback_run, &callback_success);

  bool success = ReadbackImagePixelsINTERNAL(
      mailbox, dst_info, dst_row_bytes, /*src_x=*/0, /*src_y=*/0,
      /*plane_index=*/0, std::move(readback_done), dst_pixels.data());

  EXPECT_TRUE(success);
  EXPECT_FALSE(callback_run);

  // Now simulate the GPU process completing the operation by writing to shared
  // memory.
  auto* result =
      reinterpret_cast<cmds::ReadbackARGBImagePixelsINTERNALImmediate::Result*>(
          mem.ptr);
  *result = 1;

  auto src_pixels = mem.span.subspan(pixels_offset, dst_size);
  for (size_t i = 0; i < dst_size; ++i) {
    src_pixels[i] = static_cast<uint8_t>(i + 1);
  }

  EXPECT_CALL(*command_buffer(), OnFlush()).Times(AnyNumber());

  // Run the signal query callback.
  ASSERT_TRUE(signal_closure);
  std::move(signal_closure).Run();

  EXPECT_TRUE(callback_run);
  EXPECT_TRUE(callback_success);

  // Verify pixels and padding.
  std::vector<uint8_t> expected_pixels(dst_row_bytes * dst_info.height(), 0xAA);
  for (int y = 0; y < dst_info.height(); ++y) {
    for (size_t x = 0; x < dst_info.minRowBytes(); ++x) {
      size_t dst_idx = y * dst_row_bytes + x;
      size_t src_idx = y * dst_row_bytes + x;
      expected_pixels[dst_idx] = static_cast<uint8_t>(src_idx + 1);
    }
  }

  EXPECT_EQ(dst_pixels, expected_pixels);
}

// https://crbug.com/543707066
TEST_F(RasterImplementationTest, ReadbackYUVPixelsAsyncPadding) {
  gpu::Mailbox mailbox = gpu::Mailbox::Generate();
  gfx::Rect source_rect(0, 0, 4, 4);
  gfx::Rect output_rect(0, 0, 4, 4);
  int y_plane_stride = 8;
  int u_plane_stride = 4;
  int v_plane_stride = 4;

  auto y_offset = static_cast<GLuint>(base::bits::AlignUp(
      sizeof(cmds::ReadbackYUVImagePixelsINTERNALImmediate::Result),
      sizeof(uint64_t)));
  GLuint y_padded_size = output_rect.height() * y_plane_stride;

  constexpr auto kSizeofUint64 = static_cast<GLuint>(sizeof(uint64_t));
  GLuint u_offset =
      base::bits::AlignUp(y_offset + y_padded_size, kSizeofUint64);
  GLuint u_padded_size = (output_rect.height() / 2) * u_plane_stride;

  GLuint v_offset =
      base::bits::AlignUp(u_offset + u_padded_size, kSizeofUint64);
  GLuint v_padded_size = (output_rect.height() / 2) * v_plane_stride;

  size_t total_size =
      base::bits::AlignUp(v_offset + v_padded_size, kSizeofUint64);

  ExpectedMemoryInfo mem = GetExpectedMappedMemory(total_size);

  std::vector<uint8_t> y_plane_data(y_plane_stride * output_rect.height(),
                                    0xAA);
  std::vector<uint8_t> u_plane_data(u_plane_stride * (output_rect.height() / 2),
                                    0xAA);
  std::vector<uint8_t> v_plane_data(v_plane_stride * (output_rect.height() / 2),
                                    0xAA);

  base::OnceClosure signal_closure;
  EXPECT_CALL(*gpu_control_, DoSignalQuery(_, _))
      .WillOnce([&signal_closure](uint32_t query, base::OnceClosure* callback) {
        signal_closure = std::move(*callback);
      })
      .RetiresOnSaturation();

  bool release_mailbox_called = false;
  base::OnceClosure release_mailbox = base::BindOnce(
      [](bool* called) { *called = true; }, &release_mailbox_called);

  bool callback_run = false;
  bool callback_success = false;
  base::OnceCallback<void(bool)> readback_done = base::BindOnce(
      [](bool* run, bool* success, bool val) {
        *run = true;
        *success = val;
      },
      &callback_run, &callback_success);

  gl_->ReadbackYUVPixelsAsync(
      mailbox, GL_TEXTURE_2D, source_rect, output_rect,
      /*vertically_flip_texture=*/false, y_plane_stride,
      base::span(y_plane_data), u_plane_stride, base::span(u_plane_data),
      v_plane_stride, base::span(v_plane_data), std::move(release_mailbox),
      std::move(readback_done));

  EXPECT_FALSE(callback_run);

  // Simulate GPU process completion.
  auto* result =
      reinterpret_cast<cmds::ReadbackYUVImagePixelsINTERNALImmediate::Result*>(
          mem.ptr);
  *result = 1;

  // Fill in mock source data for each plane in the shared memory.
  auto y_src_pixels = mem.span.subspan(y_offset, y_padded_size);
  for (size_t i = 0; i < y_padded_size; ++i) {
    y_src_pixels[i] = static_cast<uint8_t>(i + 1);
  }

  auto u_src_pixels = mem.span.subspan(u_offset, u_padded_size);
  for (size_t i = 0; i < u_padded_size; ++i) {
    u_src_pixels[i] = static_cast<uint8_t>(i + 41);
  }

  auto v_src_pixels = mem.span.subspan(v_offset, v_padded_size);
  for (size_t i = 0; i < v_padded_size; ++i) {
    v_src_pixels[i] = static_cast<uint8_t>(i + 51);
  }

  EXPECT_CALL(*command_buffer(), OnFlush()).Times(AnyNumber());

  // Run the signal query callback.
  ASSERT_TRUE(signal_closure);
  std::move(signal_closure).Run();

  EXPECT_TRUE(callback_run);
  EXPECT_TRUE(callback_success);
  EXPECT_TRUE(release_mailbox_called);

  // Verify Y plane. Width = 4, Stride = 8, Height = 4.
  std::vector<uint8_t> expected_y(y_plane_stride * output_rect.height(), 0xAA);
  for (int y = 0; y < output_rect.height(); ++y) {
    for (int x = 0; x < output_rect.width(); ++x) {
      size_t dst_idx = y * y_plane_stride + x;
      size_t src_idx = y * y_plane_stride + x;
      expected_y[dst_idx] = static_cast<uint8_t>(src_idx + 1);
    }
  }
  EXPECT_EQ(y_plane_data, expected_y);

  // Verify U plane. Width = 2, Stride = 4, Height = 2.
  std::vector<uint8_t> expected_u(u_plane_stride * (output_rect.height() / 2),
                                  0xAA);
  for (int y = 0; y < output_rect.height() / 2; ++y) {
    for (int x = 0; x < output_rect.width() / 2; ++x) {
      size_t dst_idx = y * u_plane_stride + x;
      size_t src_idx = y * u_plane_stride + x;
      expected_u[dst_idx] = static_cast<uint8_t>(src_idx + 41);
    }
  }
  EXPECT_EQ(u_plane_data, expected_u);

  // Verify V plane. Width = 2, Stride = 4, Height = 2.
  std::vector<uint8_t> expected_v(v_plane_stride * (output_rect.height() / 2),
                                  0xAA);
  for (int y = 0; y < output_rect.height() / 2; ++y) {
    for (int x = 0; x < output_rect.width() / 2; ++x) {
      size_t dst_idx = y * v_plane_stride + x;
      size_t src_idx = y * v_plane_stride + x;
      expected_v[dst_idx] = static_cast<uint8_t>(src_idx + 51);
    }
  }
  EXPECT_EQ(v_plane_data, expected_v);
}

#include "gpu/command_buffer/client/raster_implementation_unittest_autogen.h"

}  // namespace raster
}  // namespace gpu
